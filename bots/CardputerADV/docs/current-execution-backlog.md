# Cardputer ADV Current Execution Backlog

Last consolidated: 2026-05-01.

Purpose: single active task list for the current Cardputer ADV firmware iteration. This file is the execution tracker when chat interruptions happen.

## P0 - Stabilize Current Firmware Base

- [handoff] Use exactly one firmware/upload owner at a time. Do not run concurrent `pio upload` or direct `pio run` from another agent against the same `.pio/build`.
- [handoff] Upload through `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh upload` so Wi-Fi and bridge env are sourced; avoid bare `pio run -t upload` with empty `DEFAULT_WIFI_SSID/PASSWORD`.
- [done] Merge visual handoff changes already present in `main.cpp`: Assistant Pulse, Library Player, Audio Folders, grouped launcher.
- [resolved] Keep launcher grouped with shallow subitems, not a long flat list: `Up/Down` chooses the main group, `Left/Right` chooses the screen inside that group, `Enter` opens.
- [done] Restore missing launcher entries after merge: `Assets` and `Logs`.
- [handoff done] Keep early `OpenClaw Loading...` splash with boot stages: Display, Audio, Storage, Input, Config, Wi-Fi.
- [handoff done] Keep display brightness explicitly set with `M5Cardputer.Display.setBrightness(180)`.
- [handoff done] Keep boot-time LittleFS -> SD voice migration disabled until hardware boot is stable.
- [handoff done] Keep full `/emoji` SD scan disabled at boot; use lazy exact-file lookup for emoji assets.
- [done] Build passed after merge with PlatformIO `pio run -j1`.
- [done] Source whitespace validation passed with `git diff --check`.
- [done] Final firmware build passed after Topic Deck changes: RAM 28.3%, app flash 73.8% of 3MB OTA slot.
- [done] Cable flash completed successfully after partition layout change. Bootloader, partition table, otadata, and app image were written and hash-verified.
- [resolved] Latest wrapper flash completed successfully through `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh upload`: app wrote to 100%, hash verified, hard reset sent.
- [pending hardware] Verify on-device boot screen, Wi-Fi provisioning defaults/NVS, bridge connection, text chat, voice record/playback, SD mount, and launcher navigation.

## P0 - OTA / Partition / SD Policy

- [done] Add dual-OTA partition table: `partitions_8mb_dual_ota.csv`.
- [done] Switch `platformio.ini` to the dual-OTA partition table.
- [done] Keep `nvs` offset stable at `0x9000` so saved settings can survive if flash is not erased.
- [done] Use 3MB OTA slots: `app0` at `0x10000`, `app1` at `0x310000`.
- [done] Keep internal `spiffs` as smaller fallback storage at `0x610000`, size `0x1f0000`.
- [done] Make OTA apply SD-required. If SD is missing, show an on-screen error instead of staging firmware in LittleFS.
- [done] Add numeric `Update.getError()` logging for OTA begin/write/end failures.
- [done] Cable-flashed this layout once. OTA cannot fix a single-slot partition table by itself, but this device should now be on dual-OTA layout.
- [pending hardware] Publish/test next OTA version only after the device is running the dual-OTA layout.

## P0 - SD Required User Feedback

- [done] OTA explicitly blocks without SD and shows `SD card required for OTA` / `Insert SD card`.
- [done] Add consistent SD-required banners for SD-backed screens where user expects media to exist: `Library`, `Folders`, `Voice Notes`, `Assets`, `Firmware OTA`.
- [done] Keep text chat and basic assistant screen usable without SD.
- [done] Avoid silently falling back to internal LittleFS for large voice/audio/OTA workflows unless it is explicitly a recovery path.
- [next] Add a smaller emoji-missing hint in chat if SD is present but `/emoji` pack is missing.

## P1 - Voice / Audio Reliability

## P0 - Reliability Pass 2026-05-01

- [done] Reviewed `/Users/s1z0v/Downloads/cardputer_adv_codex_handoff_2026_05_01_0715_39874ba9_5396_419d.md`; treat OTA apply, voice turn, WAV playback, assistant audio lifecycle, and action visibility as the active reliability block before new UX work.
- [done] Version target bumped to `0.2.16-dev` for the next clean source build; do not repack older binaries as a fake source release.
- [done] OTA apply path now logs running/boot/next partitions, rejects missing/too-small inactive OTA slot, requires `Update.isFinished()`, uses strict `Update.end(false)`, and verifies that boot partition changed to the expected inactive slot before reboot.
- [done] OTA screen action label changed from ambiguous `R load` to `R check`; full historical firmware revert remains a separate real feature, not a fake button.
- [done] Voice/text turns now include `X-Client-Msg-Id` and `X-Turn-Id`; user-visible errors distinguish transport failure, HTTP status, and assistant timeout instead of collapsing into `voice upload body failed` / `Hub request failed`.
- [done] Bridge default turn wait is aligned below firmware HTTPClient's 16-bit timeout cap: bridge waits 60s, firmware waits 65s. Longer 5-10 minute interactions require the planned async `POST turn -> turn_id -> poll/WS result` protocol, not one long synchronous HTTP request.
- [done] WAV playback now parses RIFF chunks (`fmt `, `data`) instead of assuming a fixed 44-byte header, records exact decoder errors, and Library progress is driven by real playback bytes.
- [done] Non-slash help aliases (`help`, `commands`, `actions`, `команды`, `помощь`) show the available action groups and examples without reintroducing slash-command UX.
- [next] Hardware QA after flashing: OTA fetch/apply must show partition logs and boot into `0.2.16-dev`; Library must audibly play `/audio/take15-day-01.wav` and a bridge `ai_answers` WAV; voice turn must show exact `504/413/422/transport` if it fails.
- [next] Implement the async turn protocol for truly long voice and slow assistant replies: upload audio to SD/cache, send turn metadata, return immediately with `turn_id`, show pending UI, poll/WS for transcript/reply/audio, then download/play saved WAV.

- [current baseline] Voice recording and response worked in previous checks, but long voice/audio and playback truncation still need hardware validation after this merge.
- [done] Audio library scan now supports direct SD user library at `/audio` in addition to managed `/pomodoro/audio`.
- [flashed] Verify `/audio/take15-day-01.wav` ... `/audio/take15-day-15.wav` appear in Library/Folders after `R scan`.
- [next] Verify double Ctrl voice start/stop and `G0` hold behavior after launcher changes.
- [next] Verify saved own voice notes and assistant voice replies are written to SD and appear in Voice Notes player.
- [next] Verify incoming voice playback is full length, not 2-5 seconds truncated.
- [next] Keep WAV/PCM as primary stable format; MP3 player integration remains secondary.

## P1 - Launcher / Menu UX

- [resolved] Use grouped direct navigation, not a deep nested launcher: `Assistant`, `Chat`, `Audio`, `Focus`, `OpenClaw`, `System`.
- [done] Make controls deterministic: `Up/Down` moves main group, `Left/Right` or `Tab` moves sub-screen, `Enter` opens.
- [done] Remove `Topics` as a sibling of `Full Chat`; topic is now a chat context, not a separate chat mode.
- [next] Improve launcher visual polish without adding another navigation depth.
- [next] Keep numeric keys as shortcuts to the first 1-6 main groups only; sub-screen selection stays horizontal.
- [next] Verify `Tab`, arrows, WASD, `Enter`, and `Esc` on hardware.

## P1 - OpenClaw Pet / Tamagotchi

- [accepted] Handoff captured in `/Users/s1z0v/kd-projects/adv_cardputer/docs/openclaw-pet-tamagotchi-handoff.md`.
- [accepted] Patch export `/Users/s1z0v/kd-projects/adv_cardputer/tmp/openclaw-pet-tamagotchi.patch` is an older full diff from `0.2.11-dev` to `0.2.13-dev`; do not apply it wholesale over current `0.2.14-dev`, because it would collide with Notes and focus geometry fixes.
- [done] Current firmware already contains the pet runtime, NVS persistence, stats/mood/stage/activity, offline catch-up, autonomous hunt/explore/rest, procedural OpenClaw pet renderer, pet hotkeys, `/pet`/`/tama`/`/petreset`, and `pet.*` device actions.
- [next] Hardware QA pet screen: verify launcher entry, care hotkeys `F/P/C/S/M/D/H/E/R`, Enter/Space auto-care, accelerometer movement, persistence across reboot, and autonomous state changes over time.

## P1 - Assistant Pulse / Animation Quality

- [done] Assistant Pulse is the main home/watch screen.
- [next] Improve circular geometry: thicker arcs, darker underlay, bright top arc, manual round caps.
- [next] Fix Think animation feeling frozen: use segmented moving arcs and raise render cadence for `gThinking`.
- [next] Do not change display driver unless hardware evidence shows a driver-level issue. Current issue is renderer geometry/cadence/blocking, not ST7789 backend.

## P2 - Premium Visual Ports

- [handoff] Detailed gap tasks with source/design/code links are in `docs/ui-gap-implementation-tasks.md`.
- [todo] Premium Chat surfaces: compose, reply-ready, thread/bubbles, scroll rail.
- [todo] Premium Focus variants: deep focus, break breathing ball, metro pendulum, reflect/note card.
- [todo] Voice Notes visual refresh using the new Library Player primitives.
- [done] Resolve launcher model mismatch: grouped launcher is the current accepted model; do not revert to flat unless explicitly requested.
- [todo] Finish menu visual polish from HTML prototype only after launcher model is agreed.
- [todo] Update `ux/cardputer-ux-1x1.html` to match actual firmware screens: Library Player, Folders, SD-required states, current launcher, OTA state.

## P2 - Bridge / OpenClaw Integration

- [baseline] Device talks to OpenClaw bridge, not local laptop hub, for autonomous Wi-Fi use.
- [done] Bridge Topic API updated and deployed: `/api/cardputer/v1/topics` includes `current/default`, `/topics/{topic_key}/select` persists selected current topic, and `conversation_key` exact matches update bridge current topic.
- [done] Firmware text-turn now sends selected context both in JSON and `X-Conversation-Key`; voice raw upload already used `X-Conversation-Key`.
- [done] Firmware Topic Deck added on `Topics` launcher item: R syncs remote topics, arrows/Ctrl+arrows switch, Enter selects and loads last 5 messages lazily.
- [planned] Replace full Topic Deck as the normal chat UI with the `03 Compact Header Overlay` design from `telegram-topic-overlay-handoff.md`: header is hidden during normal chat, appears only on `Tab` or topic switch, then collapses after ~1200-1800 ms.
- [done] Switch topics with `Alt+Left/Right`, not `Ctrl+Left/Right`, so double Ctrl remains dedicated to voice start/stop and accidental topic switching is avoided.
- [done] Topic overlay appears on Pulse and Chat as a transient overlay; topic id/thread/root-main internals are hidden from the user.
- [done] Pulse `type to chat` starts typing into the Pulse input pill; `Enter` sends non-empty text and opens Full Chat only when input is empty.
- [planned] Topic switching must be local and instant; Telegram topic catalog/history fetch only after debounce.
- [planned] History preview stays bounded to the latest small slice; voice messages in topic preview remain lazy placeholders only, no eager media download.
- [planned] Keep chat message area primary; topic header is an overlay/compact state indicator, not a permanent row consuming display height.
- [next] After cable flash, verify bridge health, manifest fetch, text turn, voice turn, and runtime logs upload.
- [next] Keep all access token handling in build env / provisioning config. Do not commit secrets.

## Verification Commands

Build:

```bash
cd /Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV
/Users/s1z0v/.platformio/penv/bin/pio run -j1
```

If `.pio/build/cardputer_adv/*.d` or `.sconsign*.tmp` disappears during build, another PlatformIO/SCons process is touching the same build directory. Stop/wait for that process or build from an isolated copy excluding `.pio`.

Expected binary:

```text
/Users/s1z0v/kd-projects/Openclaw/cardputer-firmware/bots/CardputerADV/.pio/build/cardputer_adv/firmware.bin
```

Download mode for cable flash:

```text
Hold G0 -> press Reset -> release G0 -> upload.
```
