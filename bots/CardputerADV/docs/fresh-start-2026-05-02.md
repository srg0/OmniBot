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

## Next Session First Steps

1. Start from commit `29bb7d5` / tag `cardputer-ota-baseline-0.2.53`.
2. Do not change OTA while debugging voice unless the bug is proven to be OTA-related.
3. Reproduce standard voice once with serial monitor and bridge tail running.
4. Fix `voice transport failed` before adding new UI/features.
5. After voice is stable, test `Ctrl+R` realtime path and decide whether to keep it as a separate voice mode or fold it into the main pulse UX.
6. Then implement the closed-loop firmware request route.
