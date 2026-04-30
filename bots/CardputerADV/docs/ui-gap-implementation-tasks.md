# UI Gap Implementation Tasks

Scope: concrete tasks for screens that do not yet match the approved Cardputer ADV UI direction.

Do not build, upload, flash, or run a monitor from this document. Use it as implementation handoff only.

Primary firmware file:

- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp`

Primary design artifacts:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/assistant_pulse_preview.html`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/telegram_topic_chat_overlay_variants.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_dna_helix.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-review-sheet.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v3-player-folders-sheet.png`

## Gap Table

| Priority | Area | Current Firmware | Target Design | Task | Acceptance |
|---|---|---|---|---|---|
| P1 | Chat topic overlay | `renderChatUi()` is still text-first and has no transient full-width topic overlay. Topic switching exists via `Alt/Opt` combos and Topic Deck exists separately. | `03 Compact Header Overlay`: full-width header appears over chat on topic switch/Tab, then hides. | Implement `renderChatTopicOverlay()` over `renderChatUi()`. Add transient state: visible, shownAt, lastAction, animDir, pending history debounce. | Normal chat gets max message height. `Tab` reveals header. `Alt/Opt + Left/Right` switches topic instantly. Overlay hides after `1200-1800 ms`. No network fetch inside render. |
| P1 | Topic history preview | `renderContextsUi()` shows latest 5 in a separate Topic Deck. | Chat stays primary; latest topic preview only appears as optional overlay expansion, and voice is lazy. | Reuse `gContextPreview` in chat overlay only after debounce/history task completes. Show 0-3 compact lines if space allows. | Voice rows display `VO ... lazy`; no eager audio download. Topic switching does not freeze chat. |
| P1 | Chat visual polish | `renderChatUi()` draws plain lines plus bottom input. | Premium compose/thread surfaces from HTML: bubbles, scroll rail, auto-expanding input, enter glyph. | Add compact chat draw helpers: bubble row, scroll rail, input box with 1-3 line expansion, enter glyph. Keep current text fallback for long/unknown states. | No text overlap at 240x135. Input grows only within reserved bottom area and hides older transcript rows, not controls. |
| P1 | Launcher/menu consistency | `kLauncherGroups` and `renderLauncherOverlay()` currently use grouped navigation with subitems. `current-execution-backlog.md` still says flat launcher. | One agreed UX only. Earlier user direction favored simple intuitive menu in same style; later backlog says flat, current code is grouped. | Decide and document final launcher model, then align code and docs. If grouped stays, update backlog and make controls clear. If flat stays, remove subitem behavior. | Firmware, backlog, and UX handoff describe the same launcher. No hidden entries. Controls above numeric `1-9` are explicit. |
| P2 | Focus premium variants | `renderFocusUi()` is functional but generic timer/card UI. | Focus variants from prototypes: deep ring, break breathing ball with `follow the ball`, metronome pendulum, reflect note card/parallax. | Replace drawing layer per `FocusState`, keep existing timer/settings logic. Add cheap `millis()` animations only. | `Focus`, `Break`, `Metronome`, `Reflect` are visually distinct and readable. No new blocking timers. |
| P2 | Voice Notes visual refresh | `renderVoicePlayerUi()` still uses older dense list/sidebar UI. | Use the newer premium audio player language from Library Player. | Rework `renderVoicePlayerUi()` using Library player primitives: selected note, count, compact preview, progress, minimal controls. | Saved voice notes look consistent with Library Player and are less dense. Long titles are trimmed/marquee-safe. |
| P2 | Launcher visual polish | Current launcher is usable but not fully aligned with premium menu sheets. | Premium compact menu style from `navigation_menu_variants.png` / `cardputer-v3-menu-premium.png`. | Add small glyph/orb preview, accent rail, and clearer selected row without nested card-on-card. | Looks like same system as Pulse/Library. Does not reduce navigation speed. |
| P2 | Loading FPS policy | DNA boot splash is implemented as boot-stage splash. | DNA loader can be reused as high-FPS loading/sync screen when needed. | Extract `renderBootSplash()` visual into reusable loading helper or add a second helper. Use dedicated `16-20 ms` cadence only while loading screen is active. | Boot splash remains clean. Future sync/loading screens can run best-effort `50-60 FPS` without changing global UI cadence. |
| P2 | UX review gallery | `ux/cardputer-ux-1x1.html` is reported stale in handoff. | Browser review artifact should show actual current firmware states. | Update gallery with actual Library, Folders, Topic Deck/Overlay target, DNA loading, SD-required states, current launcher. | A reviewer can open one local HTML and see current reality plus planned gaps. |

## Detailed Task Notes

### 1. Chat Topic Overlay

Current code anchors:

- Topic data/state: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:613`
- Topic fetch/history: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:2498`
- Topic switch task path: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:6304`
- Global topic key combos: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:6818`
- Current chat renderer: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:8635`

Design anchors:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/telegram_topic_chat_overlay_variants.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/docs/telegram-topic-overlay-handoff.md`

Implementation direction:

- Add overlay state, not a new `UiMode`.
- Show full-width header only on `Tab` or topic switch.
- Keep normal chat header hidden or tiny.
- Use `Alt/Opt + Left/Right` for topic switch; do not use double `Ctrl`.
- Debounce catalog/history work after topic switching stops.
- Do not call HTTP from render.

### 2. Premium Chat Surfaces

Current code anchor:

- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:8635`

Design anchors:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/chat_pulse_variants.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-chat-compose.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-chat-compose-expanded.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-chat-thread.png`

Implementation direction:

- Add bubble renderer with sender color.
- Add scroll rail only when scrollable.
- Add input composer that expands vertically up to a fixed max height.
- Older transcript rows should disappear behind input growth; text must not overlap.

### 3. Focus Premium Variants

Current code anchor:

- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:7674`

Design anchors:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/focus_pulse_variants.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-focus-deep.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-focus-break.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-focus-metro.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v2-focus-reflect.png`

Implementation direction:

- Keep existing `FocusState`, durations, metronome, and settings.
- Replace visual layer only.
- `Focus`: deep ring and large timer.
- `ShortBreak` / `LongBreak`: clean breathing ball and short text `follow the ball`.
- `Reflect`: sparse violet note card, no dense text.
- `Metronome`: pendulum/needle if metronome is enabled.

### 4. Voice Notes Visual Refresh

Current code anchor:

- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:8510`

Design/reference anchors:

- Current premium audio player: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:7779`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v3-menu-player.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v3-player-folders-sheet.png`

Implementation direction:

- Reuse Library player style for saved voice notes.
- Keep list density low: selected item + count + short neighboring context.
- Show playback remaining time and progress.
- Preserve existing playback logic.

### 5. Launcher/Menu Consistency

Current code anchors:

- Launcher groups: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:578`
- Launcher state/apply: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:3133`
- Launcher keys: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:6001`
- Launcher render: `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:7429`

Design anchors:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/navigation_menu_variants.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/output/playwright/cardputer-v3-menu-premium.png`

Implementation direction:

- First resolve whether launcher is flat or grouped.
- Current code is grouped; current backlog says flat. This must be corrected before visual polish.
- Keep the final model simple and visible on a non-touch keyboard.

### 6. Loading DNA / FPS Reuse

Current code anchor:

- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/src/main.cpp:8798`

Design anchors:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_dna_helix_preview.html`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_dna_helix.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/docs/loading-dna-helix-handoff.md`

Implementation direction:

- DNA boot splash is already close to target.
- If reused beyond boot, add dedicated loading render cadence:
  - target `16 ms`
  - fallback `20 ms`
- Do not lower the whole app into a high-FPS loop during network work.

## Implementation Order

1. Chat topic overlay.
2. Premium chat compose/thread visuals.
3. Launcher flat-vs-group consistency decision.
4. Voice Notes visual refresh.
5. Focus premium variants.
6. UX gallery update.
7. Loading DNA reuse/FPS policy if more loading states need it.
