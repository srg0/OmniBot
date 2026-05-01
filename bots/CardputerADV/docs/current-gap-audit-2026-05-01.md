# Cardputer ADV Gap Audit 2026-05-01

Baseline checked:

- Firmware source commit: `94dc8cf Cardputer ADV 0.2.21 harden OTA apply`.
- Version in source: `0.2.21-dev`.
- Build command: `/Users/s1z0v/kd-projects/adv_cardputer/scripts/cardputer_adv_pio.sh build`.
- Build result: success, RAM `28.3%`, app flash `76.6%` of the 3MB OTA slot.
- Bridge health: `https://bridge.ai.k-digital.pro/healthz` returns `ok: true`.

## Done In Current Firmware / Bridge

- Autonomous remote bridge path is the default; no local laptop hub is required for normal text/voice turns.
- Dual OTA partition layout is present on the device and in source.
- OTA apply was hardened in `0.2.21-dev` with direct ESP-IDF OTA APIs, boot-slot diagnostics, SD-required policy, SHA/size checks, and visible version on boot/update screens.
- Pulse is the main home/watch screen and can switch to chat; Option toggles Pulse / full chat.
- Topics are a shared workspace for Pulse and Chat, not separate per-screen state.
- Topic switching uses `Alt/Opt + Left/Right`; double Ctrl remains reserved for voice.
- Topic overlay exists on Pulse and Chat and hides internal ids/thread/root labels.
- Text turns include `X-Conversation-Key`, `X-Client-Msg-Id`, and `X-Turn-Id`.
- Text sends local echo immediately and shows pending/thinking state instead of waiting silently.
- Bridge topic API is deployed: recent topics, current/default topic, topic select, bounded history.
- Bridge sends real topic context to OpenClaw in the upstream prompt, including current topic, available topics, and recent topic overview.
- Bridge strips markdown for device-visible text/TTS, instead of adding a heavy markdown renderer in firmware.
- Bridge extracts and sanitizes `[[cardputer_actions:...]]`; firmware executes only whitelisted device actions.
- Keyboard path uses TCA8418 on ADV; serial confirms that path on hardware.
- Russian display/input support is present; layout toggle exists.
- Notes mini app exists and can use SD/LittleFS for text note files.
- Audio Library scans `/audio` on SD and managed `/pomodoro/audio`.
- Voice notes and assistant reply audio use WAV/PCM as the stable path.
- Playback streams WAV from file and parses RIFF chunks instead of assuming a fixed 44-byte header.
- Playback progress is based on actual bytes played, not a fake timer.
- A lightweight real spectrum analyzer exists for PCM playback chunks.
- Pet/Tamagotchi runtime is present: stats, persistence, procedural renderer, hotkeys, and device actions.
- Battery, logs, settings, assets, firmware OTA, library, folders, voice notes, focus, chat, face, hero, and Pulse screens exist.
- SD-required banners exist for SD-backed media/OTA surfaces.
- Boot splash/loading screen with version and stage logs exists.

## Partially Done / Needs Hardware Acceptance

- OTA should now install correctly from `0.2.21-dev`, but the fix still needs one real OTA jump to a newer version, for example `0.2.22-dev`, to prove slot switching.
- Short voice roundtrip works according to the last manual check, but `0.2.21-dev` still needs final on-device QA for double Ctrl, G0 hold, full response playback, and saved voice note replay.
- Topic names/emojis improved, but real Telegram title/emoji coverage depends on what OpenClaw/session metadata exposes and on the SD emoji pack. Unknown sessions still need safe fallbacks.
- Topic history loads through bridge, but switching/fetch debounce and read-state UX are still weaker than the target design.
- The topic overlay exists, but the separate `Contexts` / topic deck screen still remains in firmware as a fallback; the intended final UX is topic overlay inside Pulse/Chat.
- Audio asset manifest sync exists, but the actual `/audio/take15-day-*.wav` visibility and playback should be verified on the physical SD card.
- Device actions exist, but the command catalog/user-facing help is basic; natural-language action reliability depends on the upstream agent emitting the marker.
- Notes app exists, but editing UX is minimal; it is not yet a polished markdown editor/viewer.
- Focus mode is functional, but not fully ported to the premium focus variants from design references.
- Voice player is improved, but saved voice notes UI and long-title marquee still need final polish on hardware.
- Launcher is grouped and buildable, but visual polish/navigation hints are not final.
- Loading DNA/transfer screens exist in pieces, but not every long network operation uses the same polished progress animation yet.
- HTML `ux/cardputer-ux-1x1.html` is behind the actual firmware state and cannot be treated as 1:1 review truth yet.

## Not Done / Lost Or Deliberately Deferred

- True 5-10 minute voice is not implemented end-to-end. Firmware records to file/chunks internally, but the network transport is still a single HTTP upload to the bridge, not a resumable chunk-session protocol.
- Realtime streaming voice is not implemented. Current flow is record -> upload file -> bridge/OpenClaw -> optional reply WAV/TTS -> playback.
- The bridge still has default caps around 4MB upload and 120s voice duration unless production config overrides them; this is not enough for reliable 5-10 minute voice.
- Async turn protocol is not implemented: the desired `POST turn -> turn_id -> pending UI -> poll/WS result -> download audio` remains a planned reliability upgrade.
- MP3 player repo was not embedded as a full MP3 backend. The accepted stable media path is WAV/PCM; MP3 remains separate/future.
- Wake word / "Hey Pixel" is not implemented. Voice remains manual via double Ctrl / G0 / hotkeys.
- Full OpenClaw bridge v1 topic contract is only partially used by firmware. The bridge has `/api/cardputer/v1/topics/...`, but core turns still go through compatibility endpoints `/api/device-text-turn` and `/api/device-audio-turn-raw`.
- Secure token rotation/revocation and token hashing in bridge state are not implemented; existing protection is bearer/device token gating and not committing secrets.
- TLS certificate pinning on the device is not implemented.
- Full M5PORKCHOP-style settings/menu theme is not fully ported.
- Premium chat compose/thread/reply screens are not fully ported.
- Premium Focus variants `deep/break/metro/reflect` are not fully ported.
- Full browser design gallery update is not done.
- Demo video capture/checklist is not done.
- M5Launcher / SD firmware bundle work remains external to this firmware. There is no custom in-firmware multiboot manager.
- Automatic background firmware update with reboot is not enabled as a finished product path; current OTA remains a user-visible firmware screen/action with safety checks.
- Topic creation UX is implemented as an `Alt+Left` arm/repeat flow, not the originally discussed hold-release "create if released on left edge" interaction.

## Highest-Value Next Work

1. Publish `0.2.22-dev` with no unrelated changes and test OTA from `0.2.21-dev` to prove slot switching.
2. Replace voice turn transport with async/chunked sessions on bridge and firmware; this unlocks long recordings and avoids HTTP timeout/body failures.
3. Finish topic UX as one workspace: remove normal reliance on the separate topic deck, debounce history fetch, improve title/emoji fallbacks, and verify OpenClaw actually executes in the selected session.
4. Finish the visual pass on Chat, Launcher, Voice Notes, Focus, Settings, and update the 1x1 HTML review artifact to match firmware.
5. Run hardware acceptance matrix: text, RU input, topic switch, topic history, voice record, voice reply, saved playback, `/audio` library, OTA, SD-missing banners, notes, pet persistence.

