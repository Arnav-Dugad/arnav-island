# Visual and interaction system — 0.4

## Intent
The island is a persistent object. Its interior needs hierarchy and readable alignment before decoration. The compact silhouette stays small; the expanded surface has a clear title, one principal task area, supporting information and one navigation row.

## Geometry and typography
- 20 DIP panel margins and a 380 DIP content width.
- Home uses three 120 DIP statistic cards with 10 DIP gaps.
- Navigation uses seven equal slots with visible labels, a retained selection indicator and moving icon hit regions.
- Text uses Segoe UI Variable Text with explicit weight and centered rectangle alignment for controls. Titles are 18 DIPs, primary values 23 DIPs, normal control text 11.5 DIPs and navigation captions 9.5 DIPs.
- Surfaces render at effective DPI. Stable body/content offsets snap to physical pixels. Text itself is not scaled during island size changes; temporary animated transforms remain subpixel for smooth movement.

## Original vector icons
`src/Design/Icons.h` defines 35 symbols on a 24-unit optical grid, with rounded 1.65-unit strokes and deliberate filled playback forms. No emoji, symbol-font dependence or copied Apple assets are used for interface icons. Decorative statistics symbols remain quiet; interactive icons use retained compositor visuals.

Hover changes icon scale to 1.09 and raises it 1.25 DIPs. Press compresses to 0.88. Springs recover from their current state, including when input reverses. Motion tokens live in `MotionEngine.h`. Disabled actions keep muted treatment. Labels remain untransformed.

## Artwork continuity
Two retained 256-DIP surfaces carry the outgoing composite and the incoming cover. On a new track, a bounded 256×256 CPU snapshot combines the currently visible frames and resamples the new cover. This work happens once per cover change, never once per animation frame. DirectComposition animates incoming opacity; the common parent retains geometry/scale motion. Rapid replacement stores two bounded buffers rather than a growing queue of covers.

The body geometry gates expanded content and header visibility. During a fast reversal the cover travels through a clear surface, rather than across fading labels. The opacity projection is converted to the same adaptive compositor curves as the springs. No UI-thread frame loop is used.

## Rings and information
Battery progress comes from the OS; focus progress comes from elapsed time. Arcs update on meaningful snapshots (once per second for a running timer). Their endpoint markers use native rotation springs. The large Focus ring and numeric countdown use the same clock state. No timer progress is invented when the timer is idle.

## Personal layouts
Settings v4 stores a validated permutation of all seven pages and three distinct metric IDs. Reordering cannot remove Settings or another page. Invalid permutations restore only the layout default rather than invalidating unrelated preferences. Navigation hit testing follows the current physical icon positions during a reorder. Personal layout has its own Reset action.

## Evidence and limits
Actual native captures include dark/light, 110% application scale, right edge, compact rings, focus, settings and rapid handoff/reversal frames. These are visual checks, not high-refresh frame-pacing measurements. Full accessibility and broader display/hardware validation remain unfinished.

## Multitasking

The eighth preference group controls app-switch collapse and volume-wheel scope. Collapsing preserves physical position/velocity through the existing MotionEngine, as well as current page and activity state. Pinning overrides app-switch dismissal. These behaviors do not identify or track applications.

## v0.7 materials and the Settings window

**Glass.** Frosted: dark tint 0x0b0c10 at ~44% (light 0xf6f7f9 at ~54%) over the blurred backdrop; Clear roughly halves the tint. The tint borrows 12% of the artwork accent in dark mode so the island picks up the music without turning colorful. A white sheen fades from the top edge to 46% height; a 1 DIP inner rim runs from 30% white at the top to 7% at the bottom, like light catching an edge. Fills inside glass become translucent white (7.5% dark, 50% light) and hairlines 12%/10%, so cards read as layers of the same material. Glass floats with an 8 DIP gap and uses fully rounded corners.

**Settings window.** 980×700 DIP, 236 DIP sidebar, 20 DIP card padding, 64 DIP rows (52 for ordering rows). Titles use Segoe UI Variable Display 28; row titles 14 medium; details 12. Controls: 44×22 toggles whose knob springs across and stretches on press; 220 DIP sliders with a thumb that grows on hover and tightens while dragging; segmented selectors whose pill slides and resizes on a spring; 28 DIP accent swatches with a travelling ring; steppers with 32 DIP round buttons. The sidebar indicator travels between sections and stretches with its velocity. Section content fades and rises 14 DIP on change.
