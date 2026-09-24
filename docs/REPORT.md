# Arnav Island v0.13 — development report

## What changed

**Glass in the island's own shape.**
- **One piece of glass.** The Windows.UI.Composition layer is split into:
  - a body whose screen-side corners run past the edge
  - two concave shoulders, clipped by real path geometry: a Direct2D figure handed to composition through the documented `IGeometrySource2DInterop`
- **No seams.** Every part carries its own copy of the backdrop, tint, depth and sheen layers, all sized to one shared frame, so gradients continue across the joins. All edges are computed with `Round()` inside the compositor expressions, so the joins sit on whole pixels in every frame. See-through glass never shows a seam.
- **Motion.** Shoulders are built at the target radius and scaled about their anchor while the radius springs, exactly like the Solid wings.
- **Vibrancy.** Direct2D's colour-matrix effect is described to composition through `IGraphicsEffectD2D1Interop`, so saturation (1.75 dark, 1.65 light) runs on the GPU over the host backdrop brush. If an older Windows rejects the effect, the plain blur remains.
- **Edge.** A specular rim and a two-step inner glow stroke the body geometry and the shoulder curves:
  - absolute-mapped gradients start at each shoulder's join
  - an inset clip hands the body's rim over to the curve at exactly the shoulder's depth
- **Settings.** Glass no longer floats by default, and `Settings::gap()` is gone.

**Lyrics scroller.**
- **Layers.**
  - Neighbouring lines share one layer.
  - The sung line has its own layer.
  - The line it replaces is a fading "ghost" (its lit surface, swapped rather than redrawn).
- **One spring for a line change.** It carries the scroll, the new line's growth from 13 to 17 px and the ghost's shrink and fade.
- **The fill.** A lit copy of the line is revealed by per-row rectangle clips animated in the compositor. The fill moves at about 14 characters a second and finishes before the next timestamp, so the CPU does nothing between lines.
- **Gaps.** Instrumental gaps (and the intro) show three dots whose opacities ramp across the gap.
- **Tapping.** Visible lines are hit targets that seek to the line's start.

**Command bar v2.**
- **Files.** A Windows Search OLE DB session (`Search.CollatorDSO`) runs on the command worker.
  - Typed filters become SQL: full-text prefixes for words, `LIKE` with escaping for anything else, extension, `System.Kind`, UTC date bounds and a known-folder scope.
  - Build output (`.pyc`, `node_modules`, `.git` …) is excluded.
  - Results are ranked by name match, how often and how recently you opened the file (halving weekly), and file age.
  - If the index can't be reached, a bounded 0.4 s scan of the user folder takes over, and the index is retried after a minute.
  - Command rows appear first; file rows follow when found.
- **System actions.**
  - Radios use `Windows.Devices.Radios` from a short-lived multithreaded apartment. On this PC an unpackaged app is *Allowed* to use them.
  - The theme writes the two Personalize values and broadcasts `ImmersiveColorSet`.
  - The recycle bin uses `SHQueryRecycleBin` / `SHEmptyRecycleBin`.
  - Sleep uses `SetSuspendState`.
  - Restart and shut down use `ExitWindowsEx`, with the shutdown privilege switched on first. They use hybrid shutdown, as the Start menu does.
  - Rows are built from the live state, and anything hard to undo is armed by the first Enter.
- **Command memory.** Kept in `commands.nexus` on this PC: up to 40 entries and 6 pins. Toggles re-read their current state when shown again.
- **Currency.**
  - Queries must match a strict grammar, so ordinary words are never taken as currency.
  - Conversion goes through the ECB's per-euro rates, with the daily XML cached for 12 hours.
  - The answer's digits are columns over a strip of tabular figures. They spin into place (leftmost first) and roll only when they change.
- **Typing aids.** Matched letters are drawn with DirectWrite drawing effects. The ghost completion comes from the top row, and Tab accepts it. A command's name that isn't finished yet previews that command.

**Alignment audit.** Each page was captured and measured at 2× and 3×. Five fixes:
- header titles centred on the header buttons (they sat 3.5 px low)
- Home's title/artist block and play button centred on the artwork (4 px and 3 px)
- the volume track, accent fill and icons on one line (1 px)
- Media's time labels centred with the mode button (3 px)

## Bugs found while testing

- **Search results disappeared for some folders.** `System.ItemPathDisplay` returns display names ("Public Documents"), so the existence check rejected real files. The query now reads `System.ItemUrl`, which carries the true path. A probe proved the SQL was right, and counters found the step that dropped the rows.
- **A comment swallowed code again.** An end-of-line `//` comment hid the odometer's column offset and clip calls. The source scan from v0.12, now run after every edit batch, caught it before any testing.
- **Uneven answer digits.** Proportional figures left a gap after the comma, so the columns now use tabular figures and the answer's own glyph positions.
- **Test runs could search real files.** QA runs are now confined to the Public folder, so no personal file can appear in a capture. The captures taken before the fix were deleted and never left this PC.
- **The auto-hide UI stage assumed floating glass.** Attached glass sits under the top pixel, so the reveal there also opens the island, which is the same behaviour as Solid. The stage now lets the island settle before timing the tuck.

## Verification

- **Unit suites:** 4 of 4 pass, including **5,393 phase checks** (103 new). The new checks cover:
  - ECB XML, the currency grammar (including 10 ordinary phrases that must not convert), conversion, rounding and grouping
  - system rows for every live state
  - matched-letter runs and ghost completion
  - file specs, date ranges (week, month and year edges, leap years), SQL escaping of quotes and wildcards, item URLs, scan matching and ranking
  - the command memory: order, pins, limits and the file round trip
  - v11 → v12 settings
- **Native UI test:** now **25 stages**. New stages:
  - attached glass with shoulders (the input region's top row is wider than its middle by nearly four radii)
  - lyric tap-to-seek on a made-up track
  - ghost completion with Tab
  - a live dark-mode row
  - "lock" arming on the first Enter (the test never presses a second)
- **Settings end-to-end:** 139 of 139.
- **Real PC:**
  - index queries answered in 22–100 ms
  - radio state read, and set-to-current-state returned Allowed
  - the ECB feed converted live (rate of 23 Sep)
  - the recycle-bin row showed the true item count and size
  - with Transparency effects switched on briefly, the blurred and saturated glass was captured, then the setting was restored
- **Idle, tucked away:** 0.00 s CPU over 30 s, 60.5 MB private memory (`evidence/v0.13/idle-hidden.json`).

Screenshots in `evidence/v0.13` use a painted backdrop, the showcase track, sample lyric lines and sample files in the Public folder.

## Limits

- Real blur needs Windows' Transparency effects; without them Frosted is a translucent frost.
- LRCLIB lyrics are timed per line, so the fill moves at a singing pace, not word by word.
- Windows has no public switch for airplane mode itself: the command turns every radio off or on. There is no documented API for night light or do not disturb.
- Folders outside the Windows Search index are found only by the fallback scan, which covers the user folder.
- Unsigned preview. No UI Automation tree for screen readers yet.
