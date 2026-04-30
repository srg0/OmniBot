# DNA Helix Loading Handoff

## Scope

Design-only handoff. Do not build, upload, flash, or run a monitor.

This concept is an original procedural DNA/molecule-style loading screen for the `240 x 135` Cardputer ADV display. It is inspired by double-helix visual references, but does not copy or embed any external asset.

## Artifacts

Live HTML/canvas preview:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_dna_helix_preview.html`

Static keyframes:

- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_dna_helix.png`
- `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/loading_dna_helix.svg`
- generator: `/Users/s1z0v/kd-projects/adv_cardputer/tmp/watchface-concepts/generate_loading_dna_helix_sheet.js`

## Visual Direction

Use a clean horizontal double helix:

- 12 base-pair columns across the center.
- Mint strand and gold strand.
- Front/back depth simulated by node size and opacity.
- Base-pair rungs alternate bright/dim depending on rotation phase.
- One thin teal orbital ellipse around the helix.
- Top-left `ADV` chip.
- Top-right small progress percent.
- Thin progress shimmer.
- Large lower label: `DNA`, `LINK`, `SYNC`, `READY`.
- Small subtitle: `molecular context`.

Avoid dense 3D realism. The screen should read like a premium embedded loading state, not a scientific poster.

## Animation

Use one phase value from `millis()`:

```cpp
float phase = nowMs * 0.0021f;
```

For each base-pair index `i`:

```cpp
float theta = phase + i * 0.72f;
float s = sinf(theta);
float c = cosf(theta);
int16_t x = startX + i * step;
int16_t yA = axisY + s * amp;
int16_t yB = axisY - s * amp;
bool strandAFront = c > 0;
```

Draw order:

1. dim rungs
2. dim back strand nodes/segments
3. bright front strand segments
4. bright front nodes
5. UI text/progress

## Max FPS

This is lighter than the sphere versions and should be safe at high FPS.

Suggested cadence:

- target: `16 ms` best-effort, about `60 FPS`
- fallback: `20 ms`, about `50 FPS`

Do not use the existing slow busy interval for this loading screen. Also do not uncap the whole app loop; only render this loading UI at the faster interval while it is active.

## Firmware Cost

Expected firmware cost is low:

- about 12 base pairs
- about 24 nodes
- about 11 strand segments per strand
- about 12 rungs
- one ellipse/orbit approximation, or skip if too expensive

No bitmap, GIF, Lottie, SVG runtime, or keyframe assets.

## Porting Notes

Use:

- `fillRoundRect`
- `drawRoundRect`
- `fillCircle`
- `drawLine`
- `drawEllipse` if available, otherwise approximate with a few arcs/skip
- existing RGB565 colors
- `millis()` based animation

On RGB565 hardware, skip real alpha. Use explicit dim colors:

- mint front: bright mint
- mint back: dark teal
- gold front: warm gold
- gold back: muted amber/brown
- rung front: pale grey-green
- rung back: dark green-grey

## References Checked

- Canvas DNA helix examples show the usual approach: draw a double helix with time-based sine/cosine depth.
- Wikimedia Commons DNA SVG was used only as a general double-helix visual reference, not copied.

## Acceptance Criteria

- Looks clean at native `240 x 135`.
- Does not crowd text or progress.
- Animation can run at best-effort `50-60 FPS`.
- No network/file IO inside render.
- No firmware flashing as part of this handoff.
