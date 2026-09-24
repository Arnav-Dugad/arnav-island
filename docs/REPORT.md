# Arnav Island v0.14 — development report

## What changed

**The glass material.**
- **Research first.** Apple's materials are not only blur. They add vibrancy (saturation), remap the backdrop's luminosity into a narrow band, add a fine grain, light the edges and cast a soft shadow. Each was prototyped on a test bench: a generated, photo-like backdrop shown full screen, with the island grabbed live over it.
- **The material matrix.** One Direct2D colour-matrix effect runs over the host backdrop brush:
  - saturation 1.8 (dark) or 1.55 (light)
  - a luminosity gain of 0.34 (dark) or 0.74 (light), less as the tint rises
  - an offset that leans with the wallpaper's brightness
- **Edge light.** Strips along the free edges (14 px) run the same backdrop through a brighter, more saturated matrix. Mask brushes with linear gradients fade them to nothing inside.
- **Grain.** A 7% two-tone noise texture, drawn once into a composition drawing surface with Direct2D.
- **Moving highlight.** The rim and glow gradients rotate about their centres. The angle is an expression of the drag and of how far the width and height still are from their targets, clamped to ±26°, so it tilts while the island moves and levels when it settles.
- **Soft shadow.** A separate window sits under the island: layered, transparent and without a redirection bitmap, so it never takes a click. It holds one sprite, a rounded rectangle blurred with Direct2D's Gaussian blur and laid out as a nine-grid, and follows the island's size and shape.
- **Clear glass.** The dark tint base went from 0.40 to 0.46, and the text halo from 0.34 to 0.48 (0.40 in light mode).

**Awareness.**
- **Screen capture.** The capability consent store's `graphicsCaptureProgrammatic` and `graphicsCaptureWithoutBorder` records are read alongside camera, microphone and location. An active capture gives a purple dot (the order is camera, microphone, screen, location) and a card (kind 12).
- **GPU.** A PDH query reads `GPU Engine(*)\Utilization Percentage`:
  - collected only while the Stats page, Home or the idle glance shows it
  - each engine's load is summed across processes (instances are keyed from `luid_`), and the busiest engine is the GPU's load
  - `gpuBusy` is a pure function with tests

**Motion.**
- **Odometers.** The currency answer's rolling digits became a general odometer:
  - Where a number is drawn, its place is recorded as a *spot*, and the odometer draws it in retained columns instead.
  - There are twelve spots: the answer, the level indicator, the focus clock, three Home statistics, the compact volume, battery and timer chips, the compact timer label, and the glance's CPU and GPU.
  - `digitRoll` re-bases a column into the middle turn of its 30-digit strip and picks the nearest copy of the new digit, so a countdown rolls one step and a spring's overshoot never leaves the strip.
  - Cells are whole pixels, so digits at rest sit on the pixel grid. Numbers that appear during a content cascade join their band's fade and rise.
- **Compact lyrics.** Two layers inside a clip the size of the label. A new line rises 9 DIPs and fades in while the old one lifts and fades, on compositor-timed ease-out curves.
- **Beat pulse.** The bass (the 50–130 Hz bands) is compared with its own half-second average, so only hits swell the cover, by up to 4%.
  - The scale about the cover's centre is composed before its size scale in a transform group.
  - The loopback analyzer now also runs while the Home page shows a playing cover.
- **Liquid morph.** On opening or closing, the corner radius gets a velocity kick on a softer spring, so the outline rounds out mid-morph.

**Command bar.**
- **Space.** Arrow keys and the pointer make the selected row the island's hovered action, and the island's generic "Space activates the hovered action" then ran it. The command bar now consumes Space's key-down, and the character arrives as usual.
- **Rows.** `commandRows` lays out the rows with an 18-DIP header before each group when the shown results span several groups. The bar's height follows the layout.
- **Colours.** `#rgb`, `#rrggbb`, `#rrggbbaa`, `colour …` and `rgb(r, g, b)` are parsed. The row draws a swatch and shows RGB and HSL, and Enter copies the hex.
- **Typos.** When nothing matches, the closest app (whole name or first word) and the closest command phrase are offered, by optimal-string-alignment distance: none under four letters, one edit up to six, two beyond.

**Left dock.** Edge 2 mirrors the right dock throughout: outline, body origin, shoulders, window position, auto-hide band, input region and edge reveal.

## Bugs found while testing

- **v0.13's vibrancy never ran.** Composition requires every Direct2D effect property to be supplied, and 0.13 set only the matrix, so Windows rejected the effect (`E_INVALIDARG`) and the plain blur showed instead. Its report said the effect was working, which was wrong. Now all three properties are set, and an effect factory that is still compiling is accepted.
- **Composition drop shadows render black on a desktop window target.** Every `LayerVisual` + `DropShadow` combination tried rendered as a black box. The shadow is now a pre-blurred nine-grid sprite.
- **The shadow window caught clicks.** A hit test over the shadow returned the shadow window. It is now layered and transparent without `SetLayeredWindowAttributes`, and the same hit test passes through to the app beneath.
- **An 85 MB memory regression, caught before release.** The glass and shadow layers each created their own Direct3D device for their drawn surfaces, and the grain was drawn even with the Solid material. Both now share the renderer's device and draw their surfaces on first use. Private memory at idle fell from 147 MB to 73.5 MB (v0.13 measures 62.4 MB under the same conditions).
- **The compact header dimmed every second while a timer ran.** The label was the ticking time, and each change replayed the header's entrance. A running timer's label now rolls as an odometer, and the entrance plays only for a real change of label. This bug predates 0.14.
- **The idle glance would have sampled while tucked away.** Auto-hide now re-evaluates the providers when the island tucks or reveals.
- **The first Space test passed without the fix.** It didn't hover a row, so nothing could be run. The stage now types a query with a harmless timer row, highlights that row, and sends real `WM_KEYDOWN`/`WM_CHAR`/`WM_KEYUP` messages. With the fix removed, it fails at that stage.

## Verification

- **Unit suites:** 4 of 4 pass.
  - **Phase: 5,432 checks** (26 new): rolling-digit targets over every position and digit, GPU engine sums, colour parsing and HSL, edit distance, typo suggestions, row headers, screen-capture order, v12 → v13 settings and the left edge band.
  - **Model: 2,061 checks** (222 new): the left dock's outline mirrors the right one point for point.
- **Native UI test:** 28 stages. The three new ones type a space between words while a result is highlighted.
- **Settings end-to-end:** 145 of 145, including the three new switches (it steps the dock edge between Top and Right; Left was checked by the unit suites and on the bench).
- **Test bench:**
  - Frosted and Clear, dark and light, on generated backdrops
  - the left dock
  - the soft shadow under Solid and glass
  - the idle glance
  - rolling digits caught mid-roll
  - a lyric line caught mid-morph
  - the cover's edge moving with a synthetic beat
- **Idle (compact, media off, 30 s):**
  - glance off: 0.000 s CPU, 73.5 MB private
  - glance on: 0.094 s CPU (0.31% of one core), 76.2 MB
  - raw data: `evidence/v0.14/idle-compact-glance-*.json`

Screenshots in `evidence/v0.14` use a painted backdrop, the showcase track and a sample file in the Public folder.

## Limits

- **No displacement lensing.** The host backdrop brush can't be offset, scaled or warped: a `Transform2D` over it renders black. The edges gather light instead of bending the background.
- **Screen capture** is known only for apps that go through Windows' capture API and its consent records. Desktop-duplication or GDI capture is invisible to it.
- **Real blur** still needs Windows' Transparency effects.
- **Compact lyrics** are line-timed, as before.
- **Unsigned preview.** There is no UI Automation tree for screen readers yet.
