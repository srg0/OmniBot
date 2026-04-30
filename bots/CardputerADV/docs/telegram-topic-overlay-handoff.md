# Telegram Topic Overlay Handoff

## Scope

Design-only handoff for the next firmware agent. Do not build, upload, flash, or run a monitor as part of this handoff. Another agent may be working with the device.

Target screen is M5Stack Cardputer ADV, `240 x 135` TFT, physical keyboard only.

## User Decision

The separate topic picker screen is not the desired UX.

Use the `03 Compact Header Overlay` direction from:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/telegram_topic_chat_overlay_variants.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/telegram_topic_chat_overlay_variants.svg`
- generator: `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/generate_topic_chat_overlay_variants.js`

The topic header must appear directly over the chat, full width, then hide after a short timeout so the chat gets its vertical space back.

## Desired UX

Default chat state:

- No persistent full header.
- Show at most a tiny topic chip if it does not cost a chat row, for example `ADV` / `BUILD` in the existing top/status area.
- Chat transcript and input should keep maximum available height.

When the user presses `Tab` in chat:

- Reveal the full-width topic overlay header.
- Keep current topic selected.
- Do not fetch network data just for revealing the header.
- Hide again after roughly `1200-1800 ms` of no topic action.

When the user switches topic:

- Reveal the same full-width overlay immediately.
- Animate the topic title horizontally or with a short cyan pulse.
- Update local `gSelectedContext` immediately.
- Defer Telegram/history fetching until the user stops switching for roughly `350-600 ms`.
- Hide overlay after sync starts or after a short idle timeout.

When the user confirms/sends in a topic:

- Save selected context with the existing preference path.
- Compose/send should use `currentConversationKey()`.

## Visual Direction

Use variant `03 Compact Header Overlay` as the base:

- Full-width header height: `34-36 px`.
- Header background: deep near-black, not bright blue.
- Border/accent: cyan for active topic.
- Title: large short topic name, e.g. `BUILD`, `ADV`, `RUN`, `BODY`.
- Secondary label: small topic group/context, e.g. `ADV FW`.
- Right hint only while visible: `CTRL ->`, `CTRL <-`, or a compact page count like `2/8`.
- Underline: `2 px` cyan line at bottom of overlay.

Hide state:

- Header collapses to either no header or a tiny chip.
- Do not keep a permanent 36 px header because it wastes about two chat rows on a 135 px screen.

Overlay preview modes:

- Normal reveal: header only.
- Optional expanded reveal after stopping on topic: show up to the last 5 cached items if there is room.
- Voice messages must be placeholders, for example `VO 0:18 lazy`; do not auto-load audio during topic switching.

## Existing Firmware Anchors

Current implementation already has context data:

- `gContexts`
- `gSelectedContext`
- `currentConversationKey()`
- `loadConversationContexts()`
- `saveCurrentContext()`
- `UiMode::Contexts`

Relevant file:

- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp`

Default topics already exist in firmware:

- `Adv Cardputer` / `ADV`
- `Marathon` / `RUN`
- `Body` / `BODY`

Reuse these instead of adding a parallel topic model.

## Input Notes

Preferred user-facing interaction:

- `Tab`: reveal topic overlay while in chat.
- `Ctrl + Left/Right`: switch topic, if reliable on the current keyboard path.
- `Enter` / `Space`: confirm selected topic only when overlay is visible.

Fallback if `Ctrl + arrow` is unreliable or conflicts:

- `Alt/Opt + Left/Right`
- or existing context-style `,` / `.` while overlay is visible.

Important: `Tab`, `Ctrl+L`, `G0`, `Ctrl+V`, double `Ctrl`, and debug combos already have meanings. If `Tab` is intercepted in chat, keep launcher/app navigation reachable via the existing combo path, or only intercept `Tab` when the active mode is `ChatFull` / `ChatFace`.

## State Machine

Add a small transient overlay state, not a heavy new screen:

- `topicOverlayVisible`
- `topicOverlayShownAtMs`
- `topicOverlayLastActionMs`
- `topicOverlayExpanded`
- `pendingTopicSync`
- `pendingTopicSyncAtMs`
- `topicSwitchAnimStartMs`
- `topicSwitchDirection`

Suggested behavior:

1. `showTopicOverlay(reason)`
   - sets visible true
   - sets timestamps
   - no network call

2. `cycleTopic(delta)`
   - updates `gSelectedContext`
   - starts animation
   - marks `pendingTopicSync`
   - schedules sync for `now + 350..600 ms`

3. idle tick
   - if pending sync time elapsed, fetch cached/latest topic metadata/messages
   - if no action for `1200..1800 ms`, hide overlay

4. render
   - draw chat normally
   - draw overlay over chat if visible
   - if hidden, draw only tiny chip or nothing

## Network And Freeze Avoidance

Do not block render or input on Telegram history.

Rules:

- Topic switching is local-first.
- Network requests happen only after debounce.
- Fetch a bounded summary: last 5 messages, trimmed to fit.
- Apply byte/character limits per message.
- Voice/audio messages are metadata-only until opened.
- If network is slow, overlay remains responsive and shows `cached` / `sync...`.
- No synchronous loops in draw/render functions.

This is the main protection against the previous "everything hangs" failure mode.

## Porting Shape

Keep the drawing procedural:

- `fillRoundRect`
- `drawRoundRect`
- `drawFastHLine`
- `fillCircle`
- `drawArc`
- existing `trimPreview` / text fitting helpers
- existing RGB565 helpers

Avoid:

- large images
- SVG/runtime assets
- blocking animation delays
- fetching history from inside render
- persistent large header in chat

## Acceptance Criteria

- Chat default view has maximum transcript space.
- Pressing `Tab` shows the full-width topic overlay without network delay.
- Switching topics feels instant and animates locally.
- Overlay hides automatically after timeout.
- Current context is reflected in outgoing `conversation_key`.
- Latest messages are bounded and lazy-loaded after switching stops.
- Voice items are placeholders until explicitly opened.
