# Clean Loading Spheres Handoff

## Scope

Design-only handoff. Do not build, upload, flash, or run a monitor from this task.

This is the preferred loading animation direction after review: cleaner and less dense than the earlier aurora version.

## HTML Example

Use this as the live reference:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_clean_spheres_preview.html`

Static keyframes:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_clean_spheres.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_clean_spheres.svg`
- generator: `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/generate_loading_clean_spheres_sheet.js`

## Visual Decision

Use the clean version, not the dense aurora one.

Composition:

- dark embedded panel with subtle border
- two main intersecting spheres:
  - mint/pearl sphere on the left
  - warm gold sphere on the right
- one small central blue-white lens overlap
- two very thin orbit rings
- tiny `ADV` chip top-left
- small progress percent top-right
- thin progress shimmer above the spheres
- short state label: `LOAD`, `LINK`, `SYNC`, `READY`

Avoid:

- four large spheres
- many bright glints
- heavy background waves
- persistent dense multicolor overlap
- long labels

## Max FPS Strategy

Target max sustainable animation FPS without starving keyboard/network.

Current firmware constants observed:

- `kRenderIntervalMs = 20`, about `50 FPS`
- `kBusyRenderIntervalMs = 60`, about `16 FPS`

For this loading screen, do not use the slow busy interval. Use a dedicated loading cadence:

- preferred target: `16 ms` frame interval, best-effort `60 FPS`
- acceptable fallback: `20 ms`, about `50 FPS`
- adaptive floor: if render + `pushSprite` consistently takes too long, step down to `20 ms`

Do not uncap the main loop blindly. Keep input and network responsive.

Suggested firmware constants:

```cpp
constexpr uint32_t kLoadingRenderIntervalMs = 16;
constexpr uint32_t kLoadingRenderFallbackMs = 20;
```

Optional adaptive rule:

```cpp
uint32_t frameStart = millis();
renderLoadingSpheresUi(label, detail, progress, frameStart);
uint32_t frameCost = millis() - frameStart;
uint32_t nextInterval = frameCost > 14 ? kLoadingRenderFallbackMs : kLoadingRenderIntervalMs;
```

## Porting Shape

Keep it procedural and cheap:

- `fillRoundRect` for panel/chip/progress
- `fillCircle` for 2 main spheres + center lens
- `drawCircle` for orbit rings
- `drawArc` for 1-2 highlights per sphere
- no PNG/GIF/keyframes in firmware
- no extra full-screen framebuffer beyond the existing canvas
- no blocking `delay()`

Approximate HTML gradients in RGB565 with layered circles:

1. dark sphere base
2. bright colored circle
3. smaller pale highlight circle/arc
4. center lens circle with pale outline

## Acceptance Criteria

- Cleaner than aurora version at native `240 x 135`.
- Looks good without HTML blur/alpha fidelity.
- Runs best-effort `50-60 FPS` during short loading/sync states.
- Does not reduce chat/input/network responsiveness.
- No firmware flashing is part of this handoff.
