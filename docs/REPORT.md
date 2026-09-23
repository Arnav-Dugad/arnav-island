# Arnav Island v0.10 — development report

## What changed

**Materials.**
- **Frosted vs Clear glass.** With Windows transparency off, the host backdrop brush falls back to an opaque fill, so both glass materials had looked like Solid. Now Frosted uses the blur when it's available and otherwise a translucent frost with a denser tint. Clear never uses the backdrop at all: it has a light tint, plus a stronger rim, sheen and depth gradient.
- **Readability.** Text on unblurred glass gets a faint four-offset halo (0.6 px, 26–30% opacity).
- **Pointer light.** A 260-DIP radial light follows the pointer on spring-driven compositor offsets and opacity. It is drawn once per tone and has no per-frame CPU work.

**Animation Lab in Settings.** The separate lab window and HUD were removed, and Settings → Motion now holds:
- **Preset or Custom spring.** The sliders show the chosen preset's values, and moving one creates a Custom spring.
- **Live preview.** A mini island plus the analytical step response, settle time and overshoot.
- **Slow motion.** Stiffness is divided by k² and damping by k, so the curve keeps its shape. It is never saved.
- **Try-it buttons.** They run real transitions on the island.
- **Every old entry point** (tray, `--lab`, the desktop shortcut, a second instance) opens this page.

**Waveform timeline.**
- **Data.** Each track keeps 64 buckets of mean loopback level, normalised to the loudest bucket heard, with a minimum scale so near-silence isn't inflated. Unheard buckets are drawn as dots.
- **Drawing.** Bars ease with compositor scale animations. The played part is clipped by a linear animation that tracks playback, the playhead is a capsule, and the bars swell while seeking.
- **Memory.** The last 32 tracks are kept, in memory only. Learning happens only while the setting is on.

**Motion.**
- **Liquid morph.** The dimension that grows leads on a spring 1.3× stiffer, and the other follows on one 0.8× as stiff.
- **Staggered rows.** Six 48-DIP bands of the content surface fade and rise in, 28 ms apart. All band animations start together and hold their first value, so no band flashes before its turn.
- **Icon swap pops.**

**Wallpaper accent.** The wallpaper is decoded at 48 × 48 with WIC, averaged with saturation weighting, and turned into a pastel. Near-grey pictures stay neutral. It updates on `SPI_SETDESKWALLPAPER`.

**Removed.** "Your day at a glance" from Home.

## Verification

- **Unit suites:** all four pass:
  - 11,682 core checks
  - 1,839 model checks
  - **5,178 phase checks**
  - provider lifecycle
- **New checks:**
  - learned waveform: unknown vs heard, bad samples, bucket mapping, normalisation, quiet tracks, clamping, track keys, and least-recently-used eviction
  - lab springs: presets unchanged, the custom spring, and slow motion keeping the curve's shape
  - sliders following the preset, and a slider making the spring Custom
  - wallpaper accent: grey stays neutral, hue is kept, always legible
  - accent swatches
  - settings v9: migration, round trip, slow motion never saved, and bounds
- **Native UI regression:** 17/17 stages. **Settings end-to-end:** 123/123, including every new Motion control and *Waveform timeline*.
- **Real desktop:**
  - The Lab's Expand and Interrupt moved the real island, and the readout showed "59 fps while it moved" on this 60 Hz panel.
  - Pressing Collapse while already collapsed reported "It didn't move".
  - The wallpaper accent was computed from this PC's wallpaper.
- **Captures:** glass (light, dark, pattern backdrop), the waveform (light, dark, glass), the row cascade (checked slowed ×10, then restored), and the pointer light on each material.
- **Idle while tucked away:** 0.00 s of CPU over 30 s, 60.5 MB private memory (`evidence/v0.10/idle-hidden.json`).

Screenshots in `evidence/v0.10` use the synthetic showcase session over the app's matte or colour pattern.

## Bugs found while testing

- **Fabricated fps figure.** The first lab readout used `DWM_TIMING_INFO.cFrame`. A probe showed that it, `cRefresh` and `cDXRefresh` each advance by exactly one per query on this Windows build, whether anything moves or not. The readout said "6 fps". It now counts `DCompositionGetFrameId` (completed frames) from the click until the shape rests within half a DIP. A probe confirmed these IDs rise at the display rate while the island moves and slow down when it's idle.
- **Row flash.** Staggered rows flashed at full opacity before their own start time. The fix is described under Motion above.
- **Tinted grey wallpaper.** The pastel conversion clamped saturation up to 0.28, which turned a grey wallpaper pink. Low saturation now stays neutral.
- **Learning while off.** The waveform learned levels even with the setting off. It no longer does.

## Limits

- Frame counts are system-wide composition. Another app animating at the same time is counted too.
- A new track's waveform fills in as it plays. Protected audio can read as silence.
- Odometer digits and shared-element morphs are not implemented (see DELIVERY_PHASES.md).
- Unsigned preview. No UI Automation tree for screen readers yet.
