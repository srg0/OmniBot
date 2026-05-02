# Cardputer ADV Current Execution Backlog

Last consolidated: 2026-05-01.

This is the active source of truth for the current Cardputer ADV/OpenClaw iteration.
When chat context becomes noisy, continue from this file first.

## Current Baseline

- Firmware tree: `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV`.
- Firmware code baseline is commit `b38771e Cardputer ADV 0.2.26 harden direct OTA fallback`; this backlog file may move independently.
- Source version: `0.2.27-dev` in `platformio.ini` and fallback `APP_VERSION`.
- Live OTA manifest on `https://bridge.ai.k-digital.pro` serves `0.2.27-dev`.
- Bridge repo has recent Cardputer commits for voice route, device actions, topics, and firmware request logging.
- Bridge topic API returns real Telegram titles/emojis and bounded recent histories.
- SD `/Volumes/CARDSD` is mounted and usable.
- Pomodoro WAV package is staged on SD:
  - `/pomodoro/audio/{ru,en,es}/{start,break,reflection}.wav`
  - compatibility aliases `/pomodoro/audio/focus_<lang>.wav`, `break_<lang>.wav`, `reflection_<lang>.wav`
  - `/pomodoro/audio/index.json`
- All staged Pomodoro WAV files were validated as complete RIFF/WAVE files.

## Non-Negotiable Rules

- One firmware/upload owner at a time. Do not run concurrent `pio` builds/uploads against the same `.pio/build`.
- Use `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build|upload`.
- Do not run bare `pio run` / `pio run -t upload` unless Wi-Fi, hub host, hub port, device id, and token env vars are already exported.
- Never print Wi-Fi passwords, device tokens, OpenAI/OpenRouter keys, Telegram tokens, or OpenClaw root tokens.
- Keep the current autonomous bridge path; do not reintroduce the local laptop hub as a normal dependency.
- SD is required for media, long audio, OTA staging, and recovery bundles. Text chat/basic assistant must still degrade gracefully without SD.
- Do not revive rejected scope:
  - no M5PORKCHOP settings/menu port;
  - no wake word / always-listening mode;
  - no broad MP3-player backend until WAV/PCM path is fully stable.

## P0 - Baseline Verification

Goal: prove that the current source, bridge manifest, SD assets, and physical device are all describing the same product.

- [done] Source version and live OTA manifest are both `0.2.27-dev`.
- [done] SD Pomodoro multilingual WAV assets are staged and validated.
- [done] Clean firmware build passes through the wrapper: RAM 28.3%, app flash 76.9% of the 3MB OTA slot.
- [done] Live OTA manifest download and binary SHA verified without exposing the device token.
- [next] On hardware, confirm boot version, Wi-Fi, bridge connection, SD mount, and Library scan.
- [next] Capture the exact current device version before any OTA or cable flash.

Exit criteria:

- Build passes.
- Manifest binary SHA matches.
- Device screen/serial confirms the expected version and bridge connectivity.

## P0 - OTA Proof And Recovery

Goal: stop guessing why OTA sometimes reboots without changing the app.

Current decision:

- Keep dual-slot in-app OTA as the short-term production path.
- Keep M5Launcher/SD-managed rollback as the medium-term recovery architecture.
- Do not replace the working OpenClaw app with Launcher until boot-confirm/rollback is cable-tested.

Tasks:

- [done] Source uses `partitions_8mb_dual_ota.csv` with two 3MB app slots.
- [done] OTA requires SD for staged download and has direct-stream fallback.
- [done] OTA apply logs partition/boot/update errors.
- [done] Published a no-op `0.2.27-dev` proof build; live manifest binary download and SHA smoke passed.
- [next] Trigger OTA on device and collect logs immediately after reboot.
- [next] If OTA still returns to old version, inspect running partition, next boot partition, otadata, and `esp_ota_mark_app_valid_cancel_rollback` behavior.
- [planned] Finish Launcher-managed SD recovery flow per `/Users/s1z0v/kd-projects/adv_cardputer/docs/launcher-boot-ota-architecture.md`.

Exit criteria:

- A real OTA jump changes the visible boot version.
- Failure mode is logged with a concrete ESP-IDF error/partition state.
- Previous-good app remains recoverable through cable or Launcher SD bundle.

## P0 - Voice Reliability And Latency

Goal: make short voice stable and define the path for long voice without RAM clipping or HTTP timeout failures.

Current stable path:

- Manual voice capture.
- Upload to bridge.
- STT/agent/TTS on bridge/OpenClaw side.
- WAV/PCM playback on device.

Tasks:

- [done] Short voice path was manually reported working.
- [done] Device actions can be emitted by the agent and executed by firmware whitelist.
- [done] WAV playback parses RIFF chunks and progress is based on played bytes.
- [next] Verify double Ctrl start/stop and G0 hold on current hardware build.
- [next] Verify assistant voice replies autoplay fully and saved notes replay fully.
- [next] Add/verify Pomodoro language cue selection in firmware: `audioLanguage` should choose RU/EN/ES cue aliases instead of hardcoded RU paths.
- [planned] Replace long synchronous voice turn with async/chunked transport:
  - create turn/session;
  - upload numbered audio chunks;
  - commit;
  - show pending UI;
  - receive transcript/reply/audio through polling or WS;
  - stream/play saved WAV from SD.
- [planned] Add fast command lane for app/device actions so commands such as "start Pomodoro" do not wait for a slow full OpenClaw turn.

Exit criteria:

- Short voice roundtrip works repeatedly.
- Long recording can be captured to SD without reboot.
- Network failure does not lose the whole recording.
- User sees clear state: recording, saving, uploading, waiting, downloading, playing, failed.

## P1 - Topic Workspace

Goal: Pulse and Chat must be two views of one selected OpenClaw/Telegram topic workspace.

Current state:

- Bridge topic API returns current/default topic, real titles, emojis, and recent histories.
- Firmware sends selected context in JSON and `X-Conversation-Key`.
- `Alt/Opt + Left/Right` switches topics.
- Topic overlay exists on Pulse/Chat and hides internal ids.

Tasks:

- [next] Verify on hardware that topic title and emoji render, not raw ids or squares.
- [next] Verify topic history loads into the chat view after switching.
- [next] Verify text/voice turns execute in the selected topic, not a random/default topic.
- [next] Keep topic deck only as fallback/debug; normal UX is in-chat overlay.
- [next] Debounce history fetch after rapid topic switches and never fetch inside render.
- [planned] Implement clean "create new topic" UX later, after switching/history is stable.

Exit criteria:

- The selected topic name/emoji is visible on Pulse and Chat.
- Sending from Pulse and Full Chat uses the same selected workspace.
- Recent messages match the selected Telegram topic.

## P1 - UI / Input / Navigation

Goal: keep the device fast and readable on the real 240x135 display.

Accepted navigation model:

- `Up/Down`: main launcher group.
- `Left/Right` or `Tab`: screen inside current group.
- `Enter`: open.
- `Esc`: close overlay/back to clear default state.
- Double `Ctrl`: voice record toggle.
- `Alt/Opt + Left/Right`: topic switch.
- `Option`: Pulse / Full Chat view toggle.

Tasks:

- [next] Hardware QA for Tab/arrows/WASD/Enter/Esc/R/Option/Ctrl.
- [next] Polish launcher visuals without adding another navigation depth.
- [next] Fix any remaining text overlap in logs/menu/player/focus.
- [planned] Premium chat bubbles/input/scroll rail.
- [planned] Voice Notes visual refresh using Library Player primitives.
- [planned] Focus visual variants, keeping existing timer/settings logic.

Exit criteria:

- No screen has footer/header/text overlap.
- The user can identify where they are and how to go back.
- Input appears immediately and local echo is shown before assistant reply.

## P1 - Pomodoro / Focus Audio

Goal: make the staged RU/EN/ES Pomodoro voice assets actually usable from Focus.

Tasks:

- [done] Stage complete WAV assets on SD.
- [next] Patch `playFocusCue()` to resolve cue path by `gDeviceSettings.audioLanguage`.
- [next] Preserve RU fallback if selected language file is missing.
- [next] Surface missing-audio hint on Focus screen if SD/audio asset is absent.
- [next] Verify start/break/reflection cues on hardware.

Exit criteria:

- Changing audio language changes Pomodoro cue language.
- Missing SD or missing cue gives a readable warning, not silence.

## P2 - Pet, Notes, Assets, Demo

- [next] Hardware QA pet screen: hotkeys, persistence, accelerometer movement, autonomous state changes.
- [next] Notes app QA: create, open, edit, save to SD, long note scroll.
- [next] Assets screen QA: download/sync progress, SD-required state, no flicker.
- [next] Battery/status screen QA.
- [planned] Demo video checklist after P0/P1 acceptance:
  - boot/version;
  - Wi-Fi/bridge;
  - topic switch;
  - English/Russian text;
  - voice request/reply;
  - Pomodoro cue;
  - audio library replay;
  - debug logs.

## Reference Documents

- `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV/docs/current-gap-audit-2026-05-01.md`
- `/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV/docs/ui-gap-implementation-tasks.md`
- `/Users/s1z0v/kd-projects/adv_cardputer/docs/launcher-boot-ota-architecture.md`
- `/Users/s1z0v/kd-projects/adv_cardputer/docs/cardputer-production-finish-plan.md`
- `/Users/s1z0v/kd-projects/adv_cardputer/docs/cardputer-bridge-audio-asset-sync-handoff.md`

## Verification Commands

Build:

```bash
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build
```

Upload:

```bash
/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh upload
```

Download mode:

```text
Hold G0 -> press Reset -> release G0 -> upload.
```
