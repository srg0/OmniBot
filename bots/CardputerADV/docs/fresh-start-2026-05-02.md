# Cardputer ADV Fresh Start

Date: 2026-05-02.

Use this file as the first context document when continuing the Cardputer/OpenClaw work in a new chat.

## Goal

Build a closed loop where Sergey can dictate a firmware request on the Cardputer, the request reaches OpenClaw/Codex, Codex changes firmware/bridge code, publishes a new OTA build to OpenClaw, and the Cardputer downloads, installs, and verifies it.

Target loop:

1. User speaks or types a request on Cardputer.
2. Firmware sends the turn to the bridge with current topic/workspace context.
3. Bridge routes the request to OpenClaw/Codex without breaking the normal OpenClaw route.
4. Codex produces a small reviewed patch, builds firmware, publishes OTA, and reports candidate version.
5. Cardputer installs OTA through the clean boot updater.
6. User verifies on hardware and continues the next iteration.

## Repositories And Paths

- Firmware repo: `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware`
- Firmware target: `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV`
- Bridge repo: `/Users/s1z0v/kd-projects/Openclaw/cardputer-bridge`
- Local provisioning env: `/Users/s1z0v/kd-projects/adv_cardputer/refs/ClawPuter/.env`
- Preferred build wrapper: `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh`
- Live bridge: `https://bridge.ai.k-digital.pro`
- Live host: `cnt-openclaw`

Never print or commit Wi-Fi passwords, device tokens, OpenAI/OpenRouter keys, Telegram tokens, or OpenClaw root credentials.

## Current Baseline

- Firmware baseline commit: `29bb7d5 Stabilize Cardputer OTA clean boot baseline`
- Firmware tag: `cardputer-ota-baseline-0.2.53`
- Firmware version on baseline: `0.2.53-dev`
- OTA manifest currently serves `0.2.53-dev`.
- Published OTA binary was created before the baseline commit and has manifest `build_git_sha` ending in `dirty`; the code state is now captured by commit `29bb7d5`.
- Firmware branch status at baseline time: `main...fork/main [ahead 2, behind 5]`. Do not push blindly until upstream synchronization is intentionally handled.
- Bridge branch status at baseline time: `codex/cardputer-bridge-deploy...origin/codex/cardputer-bridge-deploy [ahead 3]`.
- Bridge repo has unrelated dirty host snapshot files under `../hosts/...`; do not commit those unless explicitly doing host-state work.

## Firmware Architecture

- `src/main.cpp` is intentionally a thin include index.
- Firmware logic lives in `src/main_parts/*.cpp.inc`.
- This is a single translation unit split, not a real `.cpp` multi-file refactor.
- The split preserves anonymous namespace linkage, shared `g*` state, initialization order, and behavior.
- Do not move fragments into separate `.cpp` files without first untangling global state.

Primary guardrail document:

- `bots/CardputerADV/docs/ota-clean-boot-baseline.md`
- `bots/CardputerADV/docs/cardputer-ops-recipes.md`

## OTA Architecture

OTA is the current stable baseline.

Runtime apply flow:

1. Normal runtime does not download and flash firmware directly.
2. User selects OTA apply.
3. Firmware writes an NVS boot-apply request and restarts.
4. Clean boot updater starts before WebSocket, audio, keyboard, topics, pet, asset scans, and other heap-heavy systems.
5. Clean boot updater refreshes manifest, downloads firmware to SD, verifies size/SHA, flashes inactive OTA slot, switches boot partition, and reboots.

Validated invariants:

- SD staging path: `/pomodoro/firmware.tmp.bin`.
- Firmware SHA-256 must match manifest before flash.
- Flash uses inactive OTA slot.
- Flash uses `OTA_WITH_SEQUENTIAL_WRITES`.
- SD SPI is `8MHz`; do not restore `25MHz` without hardware retest.
- Clean updater heap was about `156KB`.
- Serial validation for `0.2.53-dev` included `sd stage download ok`, `staged flash 2390k/2390k`, and boot into `version=0.2.53-dev`.

Known residual:

- Full runtime can still log TLS allocation failures during non-critical boot-time asset/manifest fetch. OTA apply is isolated from that path.

## Build, Bump, Publish

Detailed operational recipes are in `bots/CardputerADV/docs/cardputer-ops-recipes.md`.

Build:

```bash
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build
```

Upload by cable:

```bash
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh upload
```

Manual version bump locations:

- `bots/CardputerADV/platformio.ini`: `-D APP_VERSION=\"0.2.xx-dev\"`
- `bots/CardputerADV/src/main_parts/001_main.cpp.inc`: fallback `#define APP_VERSION "0.2.xx-dev"`

Publish OTA from a built binary:

```bash
cd /Users/s1z0v/kd-projects/Openclaw/cardputer-bridge
npm run publish:firmware -- \
  --config config.s1z0v.local.json \
  --bin /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV/.pio/build/cardputer_adv/firmware.bin \
  --version <version> \
  --git "$(git -C /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware rev-parse --short HEAD)"
```

Smoke OTA manifest and binary checksum without printing tokens:

```bash
TOKEN="$(python3 - <<'PY'
import json
with open('/Users/s1z0v/kd-projects/Openclaw/cardputer-bridge/config.s1z0v.local.json') as f:
    print(json.load(f)['devices'][0]['device_token'])
PY
)"
BRIDGE=https://bridge.ai.k-digital.pro
curl -fsS -H "Authorization: Bearer $TOKEN" "$BRIDGE/api/cardputer/firmware/manifest" | jq .
FW_URL="$(curl -fsS -H "Authorization: Bearer $TOKEN" "$BRIDGE/api/cardputer/firmware/manifest" | jq -r '.firmware_url')"
FW_SHA="$(curl -fsS -H "Authorization: Bearer $TOKEN" "$BRIDGE/api/cardputer/firmware/manifest" | jq -r '.sha256')"
curl -fL -H "Authorization: Bearer $TOKEN" "$FW_URL" -o /tmp/cardputer-fw-smoke.bin
test "$(shasum -a 256 /tmp/cardputer-fw-smoke.bin | awk '{print $1}')" = "$FW_SHA"
```

## Voice Architecture

There are two voice paths.

Standard push-to-talk path:

- Firmware records PCM/WAV-like raw audio.
- Firmware sends it to bridge endpoint `/api/device-audio-turn-raw`.
- Bridge transcribes, routes the text turn into OpenClaw/current topic, generates/returns reply text and optional audio.
- This is the main practical voice UX today.

Realtime path:

- Firmware has a `Ctrl+R` realtime mode.
- Firmware connects to `wss://bridge.ai.k-digital.pro/api/cardputer/v1/realtime`.
- Bridge proxies to OpenAI Realtime model through `openRealtimeModelSocket()`.
- Firmware sends `audio.append` chunks and `audio.commit`.
- Bridge returns transcript, text deltas, audio deltas, and `device_actions`.
- Firmware stores reply audio to `/voice/__realtime_reply.pcm` and plays it as raw PCM.

Realtime status:

- Implemented in code on both sides.
- Not yet promoted to the stable main UX.
- Needs hardware QA for connect, recording, audio playback, topic persistence, and action execution.
- Check on device with `Ctrl+R`. Expected statuses: `Realtime connecting`, `Realtime ready`, `Realtime listening`, `Realtime thinking`, then audio/text response.
- If it fails, collect Cardputer logs and bridge journal lines containing `[realtime]`.

Current voice regression:

- User reports `voice transport failed` / `connection failed`.
- Latest bridge debug run showed no recent `voice_turns`, meaning at least some failures may happen before the bridge logs an accepted upload.
- First next diagnostic step: serial monitor during one standard voice attempt and bridge journal tail at the same time.
- Focus on device HTTP/TLS/connect path to `/api/device-audio-turn-raw`, Wi-Fi reconnect state, host/port config, and heap before upload.

Bridge debug command:

```bash
python3 /Users/s1z0v/.codex/skills/openclaw-cardputer-bridge-debug/scripts/cardputer_bridge_tail.py --hours 2 --history 5 --journal
```

## Topics And Workspace

Desired model:

- Chat, pulse, voice, and topic browsing are one workspace.
- The selected Telegram/OpenClaw topic is global context, not a separate app.
- Full chat is a view inside the current topic.
- Pulse is another view of the same topic.
- Topic switch should work from pulse and chat.

Current bridge evidence:

- Bridge returns topic titles and emoji, for example `Bridge 🌉`, `Cardputer 💻`, `ADV Cardputer 💾`.
- Topic list is present through bridge debug.

Open issues:

- On-device topic titles/emoji display has been inconsistent.
- Topic history sometimes failed with `history http -1`.
- Ensure firmware renders topic title/emoji from bridge fields, not internal `to:root-main`/numeric ids.
- Ensure text/voice turns are sent to the selected topic and replies appear in the same topic.

## Device Actions

Bridge supports natural-language device action markers and strips them from visible text/TTS.

Examples:

- `focus.start` for Pomodoro.
- `ui.open` for screens.
- `audio.play` / `voice.play`.
- `settings.*`.
- `topics.*`.
- `pet.*`.

Current status:

- User observed some actions working after voice response.
- Action reliability still needs verification after the current voice transport regression is fixed.
- Next test: say “запусти Pomodoro на 25 минут” and verify both voice reply and actual UI/focus state change.

## UI Status

Stable or acceptable:

- OTA clean boot updater.
- Main pulse screen baseline.
- Basic chat/text sending.
- SD-backed audio/library groundwork.

Still needs polish:

- Main menu/navigation should be simplified and made mode-consistent.
- Topic overlay should match premium Cardputer UI and work from pulse.
- Voice player progress/equalizer should reflect real playback.
- Logs/menu typography still needs line-height/clipping audit.
- Pet revive shortcut and help overlay need final verification.
- Focus/Pomodoro screen needs final fit/UX pass.

Rejected or paused scope:

- M5PORKCHOP settings import is canceled.
- Wake word is canceled.
- 1x1 HTML review flow is no longer a focus.

## Closed-Loop Firmware Agent Plan

This is not fully implemented yet. Required architecture:

1. Voice/text request arrives in a dedicated Cardputer development topic.
2. Bridge classifies whether the request is a normal assistant chat, device action, or firmware-development request.
3. Firmware-development requests are routed to a Codex/OpenClaw job with a strict allowlist:
   - allowed repos: firmware and bridge only;
   - no secret printing;
   - no destructive git commands;
   - build required before publish;
   - OTA publish only after successful build;
   - commit and version bump required for release candidates.
4. Codex produces patch, build log, and OTA manifest update.
5. Bridge sends Cardputer a compact “version ready” message with version, notes, and install action.
6. User applies OTA on device.
7. Device uploads boot/OTA result logs back to bridge.

Acceptance criteria for the closed loop:

- A spoken request can create a firmware patch.
- Patch is committed with a meaningful message.
- New version is published to OTA manifest.
- Cardputer sees the version, applies it, and reboots into it.
- Logs confirm success or automatically preserve enough failure evidence for rollback/debug.

## MVP Iteration Plan

Principle: fix one narrow path per iteration, keep OTA clean-boot invariants stable, and make every hardware test reproducible from cable/serial before expanding UX.

Iteration 0: repo and live-state baseline.

- Verify firmware, bridge, and `Homio/openclaw-config` are clean.
- Record exact firmware SHA, version, bridge SHA, live bridge health, OTA manifest version/SHA, and current serial port.
- Run bridge debug once and preserve the voice/realtime evidence window.
- Stop if repo state is dirty in an unexplained way.

Acceptance:

- A single baseline note has commit IDs, manifest, serial port, and bridge health.

Iteration 1: observe the current standard voice failure without changing OTA.

- Start serial monitor and bridge journal tail at the same time.
- Trigger exactly one standard voice attempt.
- Classify failure as device pre-connect, TLS/connect, upload body, response header/body, bridge STT/OpenClaw, TTS/audio download, or device action execution.
- Use firmware logs around `VOICE`, `HTTP`, `WIFI`, heap, and the bridge `[voice-raw]` lines.

Acceptance:

- The failure has one primary owner: firmware network/TLS, bridge endpoint, upstream STT/OpenClaw/TTS, or UI/playback.
- If no `[voice-raw] uploaded` exists, focus stays on device-side connect/upload path.

Iteration 2: add a serial diagnostic harness if the current firmware cannot be driven from cable.

- Add newline-delimited serial commands without printing secrets.
- Minimum commands: `diag`, `voice.start`, `voice.stop`, `rt.start`, `rt.stop`, `rt.commit`, `ota.fetch`, `ota.apply`, `logs.send`, `reboot`, `topic.list`, `topic.current`.
- Add stable `DBG ...` output lines for machine parsing: version, heap, Wi-Fi status, hub, topic, last HTTP diag, last voice diag, realtime state, OTA manifest, and boot slot.
- Keep serial commands as wrappers around existing firmware functions, not a parallel business-logic path.

Acceptance:

- A host script can open serial, run `diag`, trigger a voice/realtime/OTA operation, and collect a bounded log without touching the keyboard.

Iteration 3: standard voice repair.

- Patch the smallest proven cause from Iteration 1.
- Preferred order of suspects: Wi-Fi reconnect freshness, TLS heap/handshake, raw upload chunking/timeout, bridge auth/path, response wait timeout, TTS follow-up.
- Build with the wrapper and flash by cable.
- Run three standard voice attempts through serial/keyboard with bridge tail.

Acceptance:

- Each run reaches `[voice-raw] uploaded`, returns transcript/reply, and either plays audio or reports a text-only fallback explicitly.
- The selected topic receives the transcript and assistant reply.

Iteration 4: realtime voice repair.

- First run bridge-side `scripts/smoke-realtime.mjs` to separate bridge/OpenAI issues from firmware issues.
- Then test device `Ctrl+R`/serial realtime: connect, ready, record, commit, transcript, text, audio, device actions.
- Add missing realtime diagnostics only if logs do not isolate the failure.

Acceptance:

- Realtime reaches `realtime.ready`, sends audio/text, gets `response.done`, and either plays reply PCM or logs a precise audio failure.

Iteration 5: cable-controlled regression loop.

- Create or update a host-side loop script that can build, cable flash, monitor boot, run `diag`, run standard voice smoke, run realtime smoke, and save artifacts.
- Artifacts should include serial log, bridge journal slice, manifest snapshot, git diff/status, and pass/fail JSON.
- Do not auto-commit from this loop yet.

Acceptance:

- Codex can run one full hardware smoke cycle without asking the user to press keys unless the board must enter download mode.

Iteration 6: OTA-controlled regression loop.

- Build and publish a small version bump candidate.
- Use serial command or existing UI path to fetch manifest and request OTA apply.
- Verify clean boot updater logs: high heap, SD stage, SHA/size match, inactive slot flash, reboot into new version, bridge health confirmation.
- Preserve the previous version as rollback context.

Acceptance:

- Codex can publish a candidate, device can install it, and serial logs prove the booted version.

Iteration 7: firmware request route MVP.

- Pick one dedicated Cardputer development topic.
- Bridge classifies firmware-development intent separately from normal assistant chat and device actions.
- The first MVP can create a local Codex task/manual handoff; fully automatic patching can come after guardrails pass.
- Device receives a compact “candidate ready” message with version, summary, and install prompt/action.

Acceptance:

- A typed or spoken request such as “add calculator stub” reaches the development topic, creates a firmware work item, and returns an actionable status to the device.

Iteration 8: commit/publish discipline.

- Enforce version bump before OTA publish.
- Require clean build before publish.
- Require commit before non-dirty release candidate unless explicitly marked dirty.
- Keep bridge and firmware commits separate.

Acceptance:

- A release candidate can be traced from device version -> manifest SHA -> firmware commit -> build log.

Iteration 9: closed-loop demo.

- Use a tiny safe feature, for example a calculator placeholder screen or diagnostic command.
- Request it from Cardputer voice/text.
- Patch, build, commit, publish OTA, install, reboot, and verify on device.

Acceptance:

- One end-to-end feature request completes without manual repo commands from the user.

## Open Questions For Sergey

1. Which topic should be the canonical Cardputer development topic for firmware-building requests?
2. For MVP, should firmware-development requests require explicit confirmation before Codex edits code, or is confirmation only required before OTA publish/install?
3. Should the device auto-check OTA every 10 seconds, every minute, or only while the OTA/development screen is open?
4. Should OTA install be fully automatic for dev builds, or always ask on the device before rebooting?
5. What is the preferred spoken confirmation phrase: Russian-only, English-only, or both?
6. Should standard voice remain the primary UX and realtime stay as an optional live mode until it is proven stable?
7. Is it acceptable to add a serial command harness to production dev firmware, or should it be compiled behind a `DEV_SERIAL_COMMANDS` flag?
8. Should Codex be allowed to deploy bridge code to `cnt-openclaw` automatically after tests, or only prepare a commit and ask first?
9. What minimum evidence should every iteration save: serial log only, or serial log plus bridge journal plus manifest plus screenshot/photo when relevant?
10. For the first closed-loop demo feature, should we use a calculator stub, a diagnostic screen, or a voice command/action because it exercises the full route?

## Realtime VAD Baseline 2026-05-02

Baseline evidence:

- Firmware branch: `codex/cardputer-ota-baseline-0.2.53-clean`.
- Firmware commit: `d484c6c Stabilize Cardputer voice realtime baseline`.
- Live serial log: `/Users/s1z0v/kd-projects/adv_cardputer/tmp/runtime-monitor/realtime-live-vad-20260502-142203.log`.
- One-touch realtime path reached `realtime.ready`, started microphone capture, received `speech.stopped`, stopped local recording through VAD, received transcript, received `response.done`, and played a correct assistant answer.
- User confirmed the system answered correctly.

Working facts:

- `Ctrl+R` now means enter hands-free realtime session, not manual commit.
- Bridge realtime is configured with server VAD and `create_response: true`.
- Firmware now treats `speech.stopped` as the automatic turn boundary and releases the microphone before response playback.
- `rt.smoke` still passes with `audio=1`.
- `rt.smoke.audio` verifies the `audio.append` path without `protocol_error invalid json`.

Realtime cable smoke after flashing `d484c6c`:

- 5 serial-driven iterations were run from `/Users/s1z0v/kd-projects/adv_cardputer/scripts/openclaw_device_realtime_smoke.py`.
- Pre-fix result: all transports returned ok, but one longer text response logged `[RT] audio decode oom chars=4096`, matching the live symptom where audio could cut off.
- Fix: realtime `audio.delta` decoding no longer mallocs one contiguous decoded buffer per WebSocket frame. It streams base64 decode through a small stack buffer and writes PCM incrementally to `/voice/__realtime_reply.pcm`.
- Post-fix result: 5/5 serial realtime smokes passed with CA-verified WSS: 3 text-response runs returned `audio=1`, and 2 synthetic audio runs verified `audio.append`/`audio.commit` without protocol errors or decode OOM.
- Follow-up live monitor caught repeated `SSL - Memory allocation failed` on manual `Ctrl+R` after the hub WebSocket had been running. Fix: realtime now pauses the normal hub WebSocket before opening its own WSS/TLS connection and resumes it when realtime closes or connect fails.
- Post-fix cable smoke reproduced the low-contiguous-heap condition (`largest=22516`) and still reached `realtime.ready`; the old TLS allocation failure did not repeat in three serial realtime starts.

Known remaining issue:

- Long realtime replies no longer hit the known contiguous-heap audio decode OOM in serial smoke, but the live hands-free second-turn path still needs manual QA after this fix.
- The likely next fixes are shorter realtime replies, stronger playback-to-listening cooldown, and echo/feedback prevention if live QA still shows cut-off audio or dropped second turns.
- Standard voice remains the safer fallback path while realtime is hardened.

## Next Session First Steps

1. Start from commit `29bb7d5` / tag `cardputer-ota-baseline-0.2.53`.
2. Do not change OTA while debugging voice unless the bug is proven to be OTA-related.
3. Reproduce standard voice once with serial monitor and bridge tail running.
4. Fix `voice transport failed` before adding new UI/features.
5. After voice is stable, test `Ctrl+R` realtime path and decide whether to keep it as a separate voice mode or fold it into the main pulse UX.
6. Then implement the closed-loop firmware request route.
