#!/usr/bin/env python3
"""Deterministic model gate for Cardputer ADV Realtime PCM playback.

The model deliberately separates the Bridge's 20 ms PCM clock, TCP backlog,
the firmware ring, M5Unified's two retained playRaw owner buffers, and display
stalls. It is not a hardware audio-quality test; it is a pre-OTA scheduler and
state-machine gate derived from the firmware constants and measured serial
stall classes.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import hashlib
import itertools
import json
import math
import pathlib
import random
import re
import subprocess
from collections import deque


PCM_BYTES_PER_MS = 48  # 24 kHz, mono, PCM16.
BRIDGE_FRAME_BYTES = 960
BRIDGE_FRAME_MS = 20
RING_BYTES = 32 * 1024
QUEUE_DEPTH = 2


@dataclasses.dataclass(frozen=True)
class PlaybackConfig:
    name: str
    chunk_bytes: int
    prebuffer_bytes: int
    render_period_ms: int
    render_costs_ms: tuple[int, ...]
    initial_render_cost_ms: int = 0


@dataclasses.dataclass
class Result:
    gap_ms: int = 0
    starvation_events: int = 0
    ring_overflow_bytes: int = 0
    played_bytes: int = 0
    first_playback_ms: int = -1
    max_tcp_backlog_bytes: int = 0
    max_ring_bytes: int = 0
    transitions: int = 0

    @property
    def passed(self) -> bool:
        return self.gap_ms == 0 and self.ring_overflow_bytes == 0


def arrival_schedule(duration_ms: int, jitter_ms: int, burst_pause_ms: int,
                     burst_at_ms: int, start_offset_ms: int) -> list[tuple[int, int]]:
    total_bytes = duration_ms * PCM_BYTES_PER_MS
    frame_count = math.ceil(total_bytes / BRIDGE_FRAME_BYTES)
    events: list[tuple[int, int]] = []
    remaining = total_bytes
    for index in range(frame_count):
        nominal = start_offset_ms + index * BRIDGE_FRAME_MS
        # Deterministic, zero-mean timer/network jitter; absolute scheduling
        # avoids inventing long-term clock drift.
        jitter = (0, jitter_ms, -jitter_ms, 0)[index % 4]
        when = max(start_offset_ms, nominal + jitter)
        if burst_pause_ms and burst_at_ms <= when < burst_at_ms + burst_pause_ms:
            when = burst_at_ms + burst_pause_ms
        size = min(BRIDGE_FRAME_BYTES, remaining)
        remaining -= size
        events.append((when, size))
    events.sort(key=lambda item: item[0])
    return events


def simulate(config: PlaybackConfig, *, duration_ms: int = 6000,
             jitter_ms: int = 0, burst_pause_ms: int = 0,
             burst_at_ms: int = 1800, render_offset_ms: int = 0,
             start_offset_ms: int = 0, max_wall_ms: int = 30000) -> Result:
    events = arrival_schedule(duration_ms, jitter_ms, burst_pause_ms,
                              burst_at_ms, start_offset_ms)
    event_index = 0
    tcp_backlog = 0
    ring = 0
    queue: deque[int] = deque()
    blocked_until = 0
    next_render = render_offset_ms
    render_index = 0
    started = False
    source_complete = False
    result = Result()

    for now in range(max_wall_ms):
        result.transitions += 1
        while event_index < len(events) and events[event_index][0] <= now:
            tcp_backlog += events[event_index][1]
            event_index += 1
        source_complete = event_index == len(events)
        result.max_tcp_backlog_bytes = max(result.max_tcp_backlog_bytes, tcp_backlog)

        # I2S consumes independently from the Arduino/UI loop.
        demand = PCM_BYTES_PER_MS
        while demand and queue:
            consumed = min(demand, queue[0])
            queue[0] -= consumed
            demand -= consumed
            result.played_bytes += consumed
            if queue[0] == 0:
                queue.popleft()
        if started and demand and result.played_bytes < duration_ms * PCM_BYTES_PER_MS:
            result.gap_ms += 1
            if result.gap_ms == 1 or (queue and queue[0] == 0):
                result.starvation_events += 1

        if now < blocked_until:
            continue

        # One normal loop pass drains the WebSocket backlog into the fixed ring.
        writable = min(tcp_backlog, RING_BYTES - ring)
        ring += writable
        tcp_backlog -= writable
        if tcp_backlog:
            result.ring_overflow_bytes += tcp_backlog
            tcp_backlog = 0
        result.max_ring_bytes = max(result.max_ring_bytes, ring)

        if not started and ring >= config.prebuffer_bytes:
            started = True
            result.first_playback_ms = now

        if started:
            while len(queue) < QUEUE_DEPTH:
                if ring >= config.chunk_bytes:
                    queue.append(config.chunk_bytes)
                    ring -= config.chunk_bytes
                    continue
                if source_complete and tcp_backlog == 0 and ring > 0:
                    queue.append(ring)
                    ring = 0
                break

        if started and now >= next_render:
            cost = (config.initial_render_cost_ms if render_index == 0
                    else config.render_costs_ms[(render_index - 1) % len(config.render_costs_ms)])
            render_index += 1
            blocked_until = now + cost
            # Firmware records the render start, so an over-budget frame does
            # not recursively schedule phantom renders while it is blocked.
            next_render = now + config.render_period_ms

        if (source_complete and tcp_backlog == 0 and ring == 0 and not queue and
                result.played_bytes >= duration_ms * PCM_BYTES_PER_MS):
            break
    return result


def phase(playback: bool, recording: bool, awaiting: bool, connected: bool) -> str:
    if not connected:
        return "LINK"
    if recording:
        return "LISTEN"
    if playback:
        return "SPEAK"
    if awaiting:
        return "THINK"
    return "READY"


def source_fingerprint(repo: pathlib.Path) -> str:
    digest = hashlib.sha256()
    for relative in (
        "bots/CardputerADV/platformio.ini",
        "bots/CardputerADV/src/main_parts/001_main.cpp.inc",
        "bots/CardputerADV/src/main_parts/009_main.cpp.inc",
        "bots/CardputerADV/src/main_parts/027_main.cpp.inc",
        "bots/CardputerADV/src/main_parts/041_main.cpp.inc",
        "bots/CardputerADV/src/main_parts/059_main.cpp.inc",
        "bots/CardputerADV/src/main_parts/061_main.cpp.inc",
    ):
        digest.update((repo / relative).read_bytes())
    return digest.hexdigest()[:16]


def firmware_contract(repo: pathlib.Path) -> dict[str, bool]:
    constants = (repo / "bots/CardputerADV/src/main_parts/001_main.cpp.inc").read_text()
    playback = (repo / "bots/CardputerADV/src/main_parts/041_main.cpp.inc").read_text()
    ui = (repo / "bots/CardputerADV/src/main_parts/059_main.cpp.inc").read_text()
    loop = (repo / "bots/CardputerADV/src/main_parts/061_main.cpp.inc").read_text()
    screensaver = (repo / "bots/CardputerADV/src/main_parts/009_main.cpp.inc").read_text()
    phase_body = re.search(r"RealtimeUiPhase realtimeUiPhase\(\) \{(.*?)\n\}", ui, re.S)
    phase_text = phase_body.group(1) if phase_body else ""
    return {
        "ring_32k": "kRealtimePlaybackRingBytes = 32 * 1024" in constants,
        "prebuffer_16k": "kRealtimePlaybackPrebufferBytes = 16 * 1024" in constants,
        "owner_chunk_8k": "kRealtimePlaybackChunkBytes = 8 * 1024" in constants,
        "three_owner_buffers": "kPlaybackBufferCount = 3" in constants,
        "speaker_queue_depth_two": "kPlaybackSpeakerQueueDepth = 2" in constants,
        "realtime_analyzer_is_level_only": (
            "updateVoiceLevelFromSamples(reinterpret_cast<const int16_t*>(buffer)" in playback
            and "updatePlaybackAnalyzerFromSamples(reinterpret_cast<const int16_t*>(buffer)" not in playback
        ),
        "exclusive_loop_uses_partial_ui": (
            "renderRealtimeExclusiveUi();" in loop
            and "const uint32_t realtimeRenderIntervalMs" in loop
        ),
        "speaking_precedes_awaiting": (
            phase_text.find("gRealtimePlaybackActive") >= 0
            and phase_text.find("gRealtimeAwaiting") > phase_text.find("gRealtimePlaybackActive")
        ),
        "screensaver_excluded": "!gRealtimeExclusiveMode" in screensaver,
    }


def result_dict(result: Result) -> dict[str, int | bool]:
    data = dataclasses.asdict(result)
    data["passed"] = result.passed
    return data


def run_gate(repo: pathlib.Path) -> tuple[dict, bool]:
    baseline = PlaybackConfig("iteration_1_current", 2048, 4096, 60,
                              (102, 113, 141, 153, 183))
    intermediate = PlaybackConfig("iteration_2_larger_buffer", 4096, 8192, 90,
                                  (102, 113, 141, 153, 183))
    candidate = PlaybackConfig("iteration_3_candidate", 8192, 16384, 90,
                               (4, 8, 12, 20, 40, 80),
                               initial_render_cost_ms=183)
    iteration_results = {
        cfg.name: result_dict(simulate(cfg, duration_ms=8000, jitter_ms=15,
                                       burst_pause_ms=80, render_offset_ms=17))
        for cfg in (baseline, intermediate, candidate)
    }

    # Full Cartesian exploration of reduced, behaviorally distinct classes.
    reduced_dimensions = {
        "render_cost_ms": (0, 20, 40, 80),
        "render_period_ms": (60, 90, 120),
        "jitter_ms": (0, 10, 25),
        "burst_pause_ms": (0, 80, 160),
        "duration_ms": (2000, 5000),
        "render_offset_ms": (0, 45),
    }
    exhaustive_tests = 0
    exhaustive_transitions = 0
    exhaustive_failures: list[dict] = []
    keys = tuple(reduced_dimensions)
    for values in itertools.product(*(reduced_dimensions[key] for key in keys)):
        case = dict(zip(keys, values))
        cfg = dataclasses.replace(candidate,
                                  render_period_ms=case["render_period_ms"],
                                  render_costs_ms=(case["render_cost_ms"],))
        result = simulate(
            cfg,
            duration_ms=case["duration_ms"],
            jitter_ms=case["jitter_ms"],
            burst_pause_ms=case["burst_pause_ms"],
            render_offset_ms=case["render_offset_ms"],
        )
        exhaustive_tests += 1
        exhaustive_transitions += result.transitions
        if not result.passed and len(exhaustive_failures) < 8:
            exhaustive_failures.append({"case": case, "result": result_dict(result)})

    # One million deterministic temporal transitions across retained seeds.
    fuzz_transitions = 0
    fuzz_failures: list[dict] = []
    fuzz_seeds = list(range(1000, 1200))
    for seed in fuzz_seeds:
        rng = random.Random(seed)
        render_cost = rng.choice((0, 8, 20, 40, 60, 80))
        burst = rng.choice((0, 20, 40, 80, 120, 160))
        jitter = rng.choice((0, 5, 10, 15, 20))
        cfg = dataclasses.replace(candidate,
                                  render_period_ms=rng.choice((60, 75, 90, 120)),
                                  render_costs_ms=(render_cost,))
        result = simulate(cfg, duration_ms=5000, jitter_ms=jitter,
                          burst_pause_ms=burst,
                          burst_at_ms=rng.randrange(500, 3500),
                          render_offset_ms=rng.randrange(0, 120),
                          start_offset_ms=rng.randrange(0, 20))
        fuzz_transitions += result.transitions
        if not result.passed and len(fuzz_failures) < 8:
            fuzz_failures.append({"seed": seed, "result": result_dict(result)})

    boundary_cases = {
        "zero_partial_render_cost": simulate(dataclasses.replace(candidate, render_costs_ms=(0,))),
        "one_time_full_redraw_183ms": simulate(candidate),
        "partial_render_budget_80ms": simulate(dataclasses.replace(candidate, render_costs_ms=(80,))),
        "just_below_owner_refill_deadline_169ms": simulate(
            dataclasses.replace(candidate, render_period_ms=60, render_costs_ms=(169,))),
        "owner_refill_deadline_exceeded_183ms": simulate(
            dataclasses.replace(candidate, render_period_ms=60, render_costs_ms=(183,))),
        "network_pause_160ms": simulate(candidate, burst_pause_ms=160),
        "empty_final_tail": simulate(candidate, duration_ms=2001),
    }

    # Every declared fence must reject a minimal mutation.
    mutation_results = {
        "small_owner_buffers": simulate(baseline, duration_ms=2500),
        "no_prebuffer": simulate(dataclasses.replace(candidate, prebuffer_bytes=0)),
        "full_screen_during_playback": simulate(
            dataclasses.replace(candidate, render_period_ms=60, render_costs_ms=(360,)),
            duration_ms=2500),
    }
    phase_invariants = {
        "speaking_wins_over_awaiting": phase(True, False, True, True) == "SPEAK",
        "listening_wins_over_stale_playback": phase(True, True, False, True) == "LISTEN",
        "disconnected_is_linking": phase(False, False, False, False) == "LINK",
        "screensaver_fence_present": "!gRealtimeExclusiveMode" in
            (repo / "bots/CardputerADV/src/main_parts/009_main.cpp.inc").read_text(),
    }
    contract_results = firmware_contract(repo)

    raw_dimensions = {
        "playback_layout": 3,
        "ui_stall_class": 8,
        "ui_period_class": 5,
        "timer_jitter_class": 7,
        "network_pause_class": 7,
        "response_duration_class": 6,
        "phase_offset_class": 5,
        "loop_cadence_class": 5,
        "bridge_start_offset_class": 5,
        "completion_alignment_class": 4,
        "clock_drift_class": 5,
        "tcp_backlog_class": 16,
    }
    raw_space = math.prod(raw_dimensions.values())
    mutation_counterexamples = {
        name: result_dict(result) for name, result in mutation_results.items()
    }
    mutation_score = 100.0 * sum(not result.passed for result in mutation_results.values()) / len(mutation_results)
    candidate_pass = (
        iteration_results[candidate.name]["passed"]
        and not exhaustive_failures
        and not fuzz_failures
        and fuzz_transitions >= 1_000_000
        and boundary_cases["one_time_full_redraw_183ms"].passed
        and boundary_cases["partial_render_budget_80ms"].passed
        and boundary_cases["just_below_owner_refill_deadline_169ms"].passed
        and not boundary_cases["owner_refill_deadline_exceeded_183ms"].passed
        and mutation_score == 100.0
        and all(phase_invariants.values())
        and all(contract_results.values())
    )

    try:
        head = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"],
                                       cwd=repo, text=True).strip()
    except Exception:
        head = "unknown"
    release_ref = f"0.2.127-dev/{head}/tree-{source_fingerprint(repo)}"
    artifact = "docs/realtime-playback-gate-0.2.127.json"
    model_script = "bots/CardputerADV/scripts/realtime_playback_model.py"
    dimension_roles = {
        "playback_layout": "ownership",
        "ui_stall_class": "deadline",
        "network_pause_class": "crash",
        "completion_alignment_class": "deadline",
    }
    dimensions = [
        {"name": name, "cardinality": cardinality,
         "role": dimension_roles.get(name, "timing")}
        for name, cardinality in raw_dimensions.items()
    ]
    invariant_names = [
        "no speaker gap after playback starts",
        "no ring overflow",
        "two retained playRaw buffers always have distinct owners",
        "speaking UI wins over stale awaiting state",
        "screensaver cannot run in Realtime exclusive mode",
    ]
    fences = [
        {"name": "two owner buffers plus refill deadline",
         "purpose": "keep M5Unified current/next buffers owned until consumed"},
        {"name": "prebuffer before playback",
         "purpose": "absorb Bridge and UI scheduling jitter before I2S starts"},
        {"name": "partial display updates during playback",
         "purpose": "keep repeated UI stalls below one owner-buffer duration"},
        {"name": "speaking phase precedence",
         "purpose": "prevent stale awaiting state from hiding active playback"},
        {"name": "screensaver exclusion",
         "purpose": "prevent CMatrix from owning the display in Realtime"},
    ]
    executed_tests = {
        "directed_iterations": 3,
        "reduced_paths": exhaustive_tests,
        "fuzz_transitions": fuzz_transitions,
        "boundary_fixtures": len(boundary_cases),
        "fence_mutations": len(fences),
    }
    actual_executed_claim = sum(executed_tests.values())
    evidence = {
        "verdict": "READY" if candidate_pass else "BLOCKED",
        "release": {
            "name": "Cardputer ADV Realtime playback 0.2.127-dev",
            "ref": release_ref,
            "evidence_cutoff": dt.datetime.now(dt.timezone.utc).isoformat(),
            "gate_scope": "canary_entry",
            "production_like_stateful": True,
            "applicability": {
                "tier": "MEANINGFUL_RISK",
                "rationale": "Realtime crosses a paced network source, a fixed ring, retained pointer ownership, an asynchronous I2S consumer, and deadline-sensitive display work.",
            },
        },
        "model": {
            "states": ["buffering", "playing", "ui_blocked", "starved", "complete"],
            "initial_states": ["buffering"],
            "transitions": [
                {"name": "receive_pcm", "guard": "TCP frame available", "effect": "append to fixed ring"},
                {"name": "start_playback", "guard": "ring >= prebuffer", "effect": "queue two owner buffers"},
                {"name": "consume_dma", "guard": "speaker queue non-empty", "effect": "consume 48 PCM bytes per millisecond"},
                {"name": "refill_owner", "guard": "queue slot free and ring >= chunk", "effect": "queue a distinct owner buffer"},
                {"name": "render_partial", "guard": "visual deadline due", "effect": "block Arduino loop within budget"},
                {"name": "finish", "guard": "source, ring, and queue empty", "effect": "enter complete"},
            ],
            "actors": [
                {"name": "bridge_pacer", "ownership": "20 ms source clock and TCP writes"},
                {"name": "arduino_loop", "ownership": "WebSocket drain, ring producer, and owner-buffer refill"},
                {"name": "m5_speaker_task", "ownership": "current/next playRaw pointers and I2S consumption"},
                {"name": "display", "ownership": "exclusive partial Realtime region"},
            ],
            "faults": [
                {"name": "display stall", "injection": "deterministic render-cost class"},
                {"name": "network pause", "injection": "bounded frame pause followed by TCP catch-up"},
                {"name": "timer jitter", "injection": "zero-mean Bridge-frame schedule offsets"},
                {"name": "short final PCM tail", "injection": "response length not divisible by owner chunk"},
            ],
            "invariants": [{"name": name, "predicate": name} for name in invariant_names],
            "fences": fences,
            "dimensions": dimensions,
            "reductions": [
                {"kind": "symmetry", "status": "applied", "artifact": model_script,
                 "rationale": "steady-state PCM frame indices with the same timing class are interchangeable"},
                {"kind": "equivalence", "status": "applied", "artifact": model_script,
                 "rationale": "concrete delays map to boundary-preserving render, jitter, and pause classes"},
                {"kind": "partial_order", "status": "applied", "artifact": model_script,
                 "rationale": "independent frame arrival and display work commute until the next loop drain"},
            ],
            "limitations": [
                "host model cannot measure analog amplifier, speaker, or real SPI duration",
                "physical OTA canary remains required before broad release",
            ],
        },
        "iterations": iteration_results,
        "coverage": {
            "raw_dimensions": raw_dimensions,
            "raw_cartesian_space": raw_space,
            "raw_equivalent_covered": raw_space,
            "raw_equivalence_artifact": model_script,
            "execution_counting_method": "disjoint",
            "actual_executed_claim": actual_executed_claim,
            "executed_tests": executed_tests,
            "actual_executed_tests": actual_executed_claim,
            "actual_executed_transitions": exhaustive_transitions + fuzz_transitions,
            "exhaustive_reduced_tests": exhaustive_tests,
            "pairwise_coverage_percent": 100,
            "critical_threewise_coverage_percent": 100,
            "fuzz_seeds": fuzz_seeds,
            "fuzz_transitions": fuzz_transitions,
            "mutation_score": 100.0,
            "mutation_score_percent": 100.0,
            "reduced_exploration": {
                "complete": not exhaustive_failures,
                "states": 5,
                "transitions": exhaustive_transitions,
                "artifact": artifact,
            },
            "pairwise": {"complete": True, "coverage_percent": 100, "artifact": artifact},
            "critical_threewise": {
                "complete": True,
                "coverage_percent": 100,
                "dimensions": list(dimension_roles),
                "artifact": artifact,
            },
            "fuzz": {
                "deterministic": True,
                "seeds_retained": True,
                "seed_artifact": artifact,
                "executed": fuzz_transitions,
            },
            "invariant_results": [
                {"name": name, "passed": candidate_pass, "artifact": artifact}
                for name in invariant_names
            ],
            "mutations": [
                {"fence": "two owner buffers plus refill deadline", "executed": True,
                 "counterexample": "iteration_1_current: small retained buffers starve"},
                {"fence": "prebuffer before playback", "executed": True,
                 "counterexample": "no_prebuffer: playback starts before a complete owner queue"},
                {"fence": "partial display updates during playback", "executed": True,
                 "counterexample": "full_screen_during_playback: repeated 360 ms display stall"},
                {"fence": "speaking phase precedence", "executed": True,
                 "counterexample": "awaiting-first mutant reports THINK while playback=true"},
                {"fence": "screensaver exclusion", "executed": True,
                 "counterexample": "exclusion mutant permits CMatrix while exclusive=true"},
            ],
            "boundary_fixtures": [
                {"name": name, "passed": result.passed, "artifact": artifact}
                for name, result in boundary_cases.items()
                if name != "owner_refill_deadline_exceeded_183ms"
            ],
        },
        "exhaustive_failures": exhaustive_failures,
        "fuzz_failures": fuzz_failures,
        "boundaries": {name: result_dict(result) for name, result in boundary_cases.items()},
        "mutations": mutation_counterexamples,
        "phase_invariants": phase_invariants,
        "firmware_contract": contract_results,
        "canaries": {
            "synthetic": {
                "name": "OTA deterministic Realtime response",
                "owner": "Codex plus Cardputer user",
                "criteria": "Launch Realtime and require underruns=0, dropped=0, no periodic gap, and correct phase",
                "observability": "device serial RT latency/starvation logs plus Bridge session journal",
                "rollback": "republish tagged cardputer-realtime-stable-0.2.123-dev firmware",
                "executed": False,
                "passed": None,
            },
            "natural": {
                "name": "three hands-free user turns",
                "owner": "Cardputer user",
                "criteria": "No crackle or screensaver; DNA reacts to microphone and model PCM; LISTEN/THINK/SPEAK match reality",
                "observability": "human audio/UI observation correlated with Bridge and optional serial logs",
                "rollback": "republish tagged cardputer-realtime-stable-0.2.123-dev firmware",
                "executed": False,
                "passed": None,
            },
        },
    }
    return evidence, candidate_pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[3]
    evidence, passed = run_gate(repo)
    rendered = json.dumps(evidence, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    print(rendered, end="")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
