# Loading Spheres Screen Handoff

## Scope

Design/prototype handoff for a premium loading screen on M5Stack Cardputer ADV.

Do not build, upload, flash, or run monitor from this handoff. Another agent may be working with the device.

## Artifacts

Animated HTML/canvas preview:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_spheres_preview.html`

Static keyframes:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_spheres_keyframes.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_spheres_keyframes.svg`
- generator: `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/generate_loading_spheres_sheet.js`

## Visual Direction

Use this as the boot/loading/sync visual:

- Dark panel with subtle border, same premium ADV palette.
- Three intersecting moving spheres:
  - cyan primary
  - violet secondary
  - green/teal tertiary
- Thin cyan/violet orbit rings around the cluster.
- Small top-left `OPENCLAW` chip.
- Small top-right progress percentage.
- Thin progress bar above the sphere cluster.
- Large center/bottom state label: `LOADING`, `LINKING`, `SYNC`, `READY`.
- Small muted subtitle: `sync context` or another short loading reason.

Keep labels short. Do not add explanatory text.

## Animation

HTML preview uses smooth alpha/compositing. Firmware should approximate this procedurally.

Recommended firmware animation:

- Time source: `millis()`.
- Refresh: use existing render cadence, no blocking delay.
- Sphere positions:
  - `x = baseX + sin(t + phase) * dx`
  - `y = baseY + cos(t * rate + phase) * dy`
  - `r = baseR + sin(t * 1.2 + phase) * 2`
- Orbit rings:
  - 1 cyan ring pulsing radius around `46..55`
  - 1 violet ring pulsing radius around `53..59`
- Progress shimmer:
  - short cyan segment sliding across a dark 156 px progress rail.

## Firmware Porting Notes

Use compact drawing primitives only:

- `fillRoundRect`
- `drawRoundRect`
- `fillCircle`
- `drawCircle`
- `drawArc`
- `drawFastHLine`
- `setTextColor`
- existing RGB565 helper

RGB565 has no real alpha. Approximate sphere depth with:

- darker filled circle base
- bright outer circle
- 1-2 highlight arcs
- smaller darker/green/cyan overlap circles if needed

Do not port CSS blur, SVG gradients, or runtime images.

## Suggested API

Add a small rendering helper rather than a new heavy UI layer:

```cpp
void renderLoadingSpheresUi(const String& stateLabel,
                            const String& detail,
                            uint8_t progress,
                            uint32_t nowMs);
```

Potential call sites:

- Wi-Fi connect
- hub connect
- context/topic sync
- asset sync
- OTA preparation
- SD/index loading

## Acceptance Criteria

- Screen reads clearly at native `240 x 135`.
- No text overlap at `LOADING`, `LINKING`, `SYNC`, `READY`.
- Animation is time-based and never blocks input/network.
- No bitmap assets required.
- Loading state can be reused by multiple firmware flows.
