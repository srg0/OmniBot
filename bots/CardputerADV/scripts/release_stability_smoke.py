#!/usr/bin/env python3
"""Static wiring checks for the low-risk 0.2.142 stability iteration."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    constants = read("src/main_parts/001_main.cpp.inc")
    boot = read("src/main_parts/040_main.cpp.inc")
    voice = read("src/main_parts/038_main.cpp.inc") + read("src/main_parts/039_main.cpp.inc") + read("src/main_parts/049_main.cpp.inc")
    pulse = read("src/main_parts/059_main.cpp.inc")

    require("kMaxRecordSeconds = 30.0f" in constants,
            "ordinary voice must have a bounded short-turn cap")
    require("kRealtimeInputSampleRate = 24000" in constants and
            "kRealtimeModelSampleRate = 24000" in constants,
            "Realtime PCM clock changed unexpectedly")
    require("shouldStopOrdinaryRecording" in voice,
            "ordinary voice does not use the wrap-safe stop policy")
    require("record stop ms=" in voice and "roundtrip ms=" in voice,
            "ordinary voice latency marks are incomplete")

    require("gPendingOtaCheckAction = true" in boot and "boot remote check queued" in boot,
            "boot OTA check is still only cosmetic")
    require("otaUpdateAvailable" in pulse and 'd.print("UP")' in pulse,
            "Pulse OTA availability badge is not wired")

    print("release_stability_smoke: PASS")


if __name__ == "__main__":
    main()
