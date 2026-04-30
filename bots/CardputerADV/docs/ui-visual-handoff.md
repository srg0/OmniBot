# Cardputer ADV UI Visual Handoff

Scope: current ADV Cardputer visual work around Assistant Pulse, chat, focus, launcher/menu, audio player, and meditation folders.

Primary sources:

- Prototype: `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/assistant_pulse_preview.html`
- Screenshots: `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/`
- Firmware: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp`
- Design skill: `/Users/s1z0v/.codex/skills/cardputer-adv-screen-design`

## Current Changes

Firmware file changed:

- `bots/CardputerADV/src/main.cpp`

New local skill created:

- `/Users/s1z0v/.codex/skills/cardputer-adv-screen-design/SKILL.md`
- `/Users/s1z0v/.codex/skills/cardputer-adv-screen-design/references/style-guide.md`
- `/Users/s1z0v/.codex/skills/cardputer-adv-screen-design/references/firmware-porting.md`
- `/Users/s1z0v/.codex/skills/cardputer-adv-screen-design/agents/openai.yaml`

Prototype artifacts updated outside the firmware repo:

- `tmp/watchface-concepts/assistant_pulse_preview.html`
- `output/playwright/cardputer-v3-menu-player.png`
- `output/playwright/cardputer-v3-menu-folders.png`
- `output/playwright/cardputer-v3-menu-premium.png`
- `output/playwright/cardputer-v3-player-folders-sheet.png`

## Firmware Implementation Summary

[DONE] Assistant Pulse home/watch screen

- Runtime states: ready, recording, thinking, playback through `AssistantPulseState`.
- Firmware renderer: `renderAssistantPulseUi()`.
- Visuals: dark 240x135 surface, cyan/violet/red/amber/green state palette, animated arcs, waveform, big time/record timer, premium pill button.
- Mapped as primary `UiMode::Home`; `UiMode::Clock` currently reuses it.

[DONE] Audio library player for meditation/audio assets

- Firmware renderer: `renderLibraryUi()`.
- Uses `/pomodoro/audio` WAV assets.
- Shows current folder, selected track, animated media orb, waveform/progress, previous/play/next controls, and a compact two-row nearby list.
- Keys: `,` / `;` previous track, `.` / `/` next track, `Enter` / space play/stop, `F` folder picker, `R` rescan.

[DONE] Meditation folder picker

- New mode: `UiMode::AudioFolders`.
- Firmware renderer: `renderAudioFoldersUi()`.
- Folder model: `AudioFolder`, `gAudioFolders`, `gAudioFolderFilter`.
- Folders are inferred from first-level subdirectories under `/pomodoro/audio`. Empty folders do not appear until they contain WAV assets.
- Always inserts `All audio`.
- Keys: `,` / `;` previous folder, `.` / `/` next folder, `Enter` / space open, `R` rescan.

[DONE] Launcher navigation rework

- Launcher groups changed from 7 grouped entries to 13 direct rows:
  `Pulse`, `Chat`, `Chat Eyes`, `Big Eyes`, `Klo Hero`, `Voice Notes`, `Focus`, `Library`, `Folders`, `Contexts`, `Battery`, `Settings`, `Firmware`.
- `Tab` opens launcher instead of silently cycling app screens.
- Launcher overlay is now a compact 5-row list with selected row, page indicator, and bottom key hint.

[PARTIAL] Focus UI

- Firmware already has functional focus state, cycles, progress, metronome setting, help overlay, and state colors.
- It does not yet match the premium HTML variants:
  focus big orb/timer, deep ring, break breathing ball with `follow the ball`, metro pendulum, reflect parallax/NOTE card.

[PARTIAL] Menu visual system

- Firmware has a usable launcher overlay and direct menu entries.
- The richer HTML `premium` menu with left orbital visual, group detail card, bottom group indicators, plus `hub`, `quick`, and `settings` compositions is not fully ported.

[TODO] Chat visual system

- Firmware chat is functional but still text-first: transcript lines, optional face panel, input tail, scroll indicators.
- HTML premium chat states are not ported:
  pulse chat, multiline compose field with auto-expanding input, reply-ready voice surface, and bubble/thread layout with scroll rail.

[TODO] Voice Notes player visual refresh

- `renderVoicePlayerUi()` still uses the older dense voice-note list aesthetic.
- The new premium player visual was ported to `renderLibraryUi()` for meditation/audio assets, not to saved voice-note playback.

[TODO] Firmware UX review artifact

- `bots/CardputerADV/ux/cardputer-ux-1x1.html` still reflects an older review gallery.
- It does not yet include the new Library Player and Audio Folders screens from the latest prototype.

## Not Yet Ported From HTML Prototype

Prototype states in `assistant_pulse_preview.html`:

- Voice: `ready`, `record`, `think`, `play` -> [DONE] in `renderAssistantPulseUi()`.
- Chat: `pulse`, `compose`, `reply`, `thread` -> [TODO] premium visual port.
- Focus: `focus`, `deep`, `break`, `metro`, `reflect` -> [PARTIAL] runtime exists, premium visuals mostly not ported.
- Menu: `premium`, `player`, `folders`, `hub`, `launcher`, `quick`, `settings` -> [PARTIAL] player/folders and basic launcher ported; other menu variants not.

## Verification State

Already verified before this handoff:

- PlatformIO build passed from `bots/CardputerADV`.
- Build memory at that point: RAM about 28.2%, flash about 62.8%.
- Upload was attempted on `/dev/cu.usbmodem2101`, but failed because the board was not in download mode: `Wrong boot mode detected (0x27)`.

Additional merge verification:

- Launcher is now a direct flat list and includes all firmware screens: Pulse, Chat, Chat Eyes, Big Eyes, Klo Hero, Voice Notes, Focus, Library, Folders, Assets, Contexts, Battery, Settings, Firmware, Logs.
- OTA partition layout was switched to dual-slot via `partitions_8mb_dual_ota.csv`.
- OTA storage policy is SD-first and SD-required for firmware apply. If SD is missing, firmware shows `SD card required for OTA` / `Insert SD card` instead of trying to stage the firmware in internal LittleFS.
- OTA errors now log numeric `Update.getError()` for `begin`, short write, and `end` failures.
- PlatformIO build passed after merge with RAM about 28.2% and flash about 73.3% of a 3MB app slot.
- A later upload succeeded after manual download mode.
- Important provisioning fix: the first successful upload was built without `DEFAULT_WIFI_SSID`, `DEFAULT_WIFI_PASSWORD`, `DEFAULT_HUB_HOST`, and `DEFAULT_HUB_PORT`, so the device entered BLE setup when NVS credentials were empty.
- Rebuild/reflash was repeated with compile-time defaults mapped from the local env. After that, serial no longer indicated BLE setup; the next blocker was hub endpoint reachability (`connection refused` / TCP timeout).

Required before flashing:

1. Hold `G0`.
2. Press `Reset`.
3. Release `G0`.
4. Run upload again.

Before building a self-provisioning image, export the expected PlatformIO variables:

```bash
export DEFAULT_WIFI_SSID=...
export DEFAULT_WIFI_PASSWORD=...
export DEFAULT_HUB_HOST=...
export DEFAULT_HUB_PORT=...
export DEFAULT_DEVICE_TOKEN=...
```

Do not print real credential values in logs or handoff docs.

## Implementation Plan

1. Port premium Chat surfaces.
   - Add small chat-specific render helpers: bubble, scroll rail, compose field, enter glyph.
   - Keep one `renderChatUi()` entry but branch between compose/thread/reply visual states using existing `gInputBuffer`, `gAssistantPendingVisible`, `gChatScrollOffset`, and recent lines.
   - Preserve current text fallback for long/unknown chats.

2. Port premium Focus variants.
   - Keep existing `FocusState` and settings logic.
   - Replace only the drawing layer:
     `Focus` -> orb/timer,
     `ShortBreak` / `LongBreak` -> breathing ball,
     `Focus + metronome` or help state -> pendulum visual,
     `Reflect` -> violet reflect/NOTE card.
   - Avoid adding new blocking timers; derive animation from `millis()`.

3. Upgrade Voice Notes player.
   - Reuse the new Library player primitives.
   - Keep the voice-note list but reduce density to selected note, count, ticker/preview, progress, and minimal controls.
   - Do not mix voice notes with meditation folders unless explicitly requested.

4. Finish menu visual polish.
   - Decide whether firmware launcher should stay direct-list or return to 5 app groups.
   - If direct-list stays, add small orb/glyph preview and group accents without increasing navigation complexity.
   - Add access to entries 10-13 through clear keyboard hints because numeric direct keys only cover 1-9.

5. Update `ux/cardputer-ux-1x1.html`.
   - Add Library Player and Audio Folders screens.
   - Mark current firmware reality, not only aspirational HTML concepts.
   - Use it as the stable browser review artifact inside the firmware repo.

6. Hardware verification.
   - Build with PlatformIO.
   - Flash only after manual download mode if auto-reset fails.
   - Check on-device text clipping for Chat compose, Folder labels, long audio titles, and Focus help.

## SD Audio Layout

Put meditation WAV files on SD or filesystem under:

```text
/pomodoro/audio/<folder-name>/<track-name>.wav
```

Examples:

```text
/pomodoro/audio/meditations/morning_breath.wav
/pomodoro/audio/sleep/body_scan.wav
/pomodoro/audio/focus/deep_25.wav
```

The folder picker uses `<folder-name>` as the category. Files directly inside `/pomodoro/audio` appear under `Root`.
