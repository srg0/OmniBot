# Cardputer ADV UX Preview

Open:

```bash
open /Users/s1z0v/kd-projects/adv_cardputer/worktrees/omnibot-adv-cardputer/bots/CardputerADV/ux/cardputer-ux-1x1.html
```

Purpose:

- review firmware screens at exact `240x135` coordinates;
- switch between current screens;
- see the exact current location as `App / Screen` in the top breadcrumb;
- use the left navigation map grouped by app, with active app and active screen highlighted;
- use the `Prev / Current / Next` strip under the device for local navigation;
- use on-screen controls and keyboard hotkeys for full navigation;
- review the Porkchop-inspired text launcher/settings aesthetic before firmware porting;
- click a point to produce references like `#chat-face@x120,y64`;
- store local browser comments and export them as JSON.

Main hotkeys:

- `Tab` / `Shift+Tab`: next/previous screen inside current app.
- `Double Tab`: open the launcher.
- `Alt+Down` / `Alt+Up`: next/previous app group.
- `Ctrl+L`: open launcher screen.
- `L`: open loading screen.
- `D`: open logs/debug screen.
- `1-9` / `0`: jump to screen by gallery order.
- `Arrows`: create/move selected comment coordinate, `Shift+Arrows` moves by 10 px.
- `+` / `-`: change preview scale.
- `C`: copy current screen/coordinate link.
- `M`: focus comment box, `Ctrl+Enter`: save comment, `Esc`: leave comment box.

This is a static review artifact only. It does not drive the firmware build.

Current UX decisions:

- Voice Player stays as-is.
- Assistant Pulse is the only primary home/watch screen and replaces the previous watchface set.
- Companion now has two eye themes for review: Organic Eyes and primitive Glyph Eyes.
- Typing from Assistant Pulse opens chat/composer flow.
- Launcher and Settings use a text-menu/pixel-cursor style adapted from the M5PORKCHOP visual language.
