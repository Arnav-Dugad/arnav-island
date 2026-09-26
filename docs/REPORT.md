# Arnav Island v0.17 — development report

## What changed

**Weather.**
- **The Town field** is a new Settings control: a text field with a caret, selection keys and paste, and up to six results below it. Typing sends a search 320 ms after the last key. The island's weather service answers on its worker (`search`, then `WeatherMessage` 2), and a result picked with Enter or a click is saved as it is (`choose`), with no second lookup. The field's row grows to fit its results and scrolls into view.
- **Names** are *Town, Region, Country* (`parseGeocodeAll`, from Open-Meteo's `admin1`), one row per name.
- **The command** *weather* alone opens the field.

**Music.**
- **Two decks.** `IslandPlayer` keeps two media engines. A crossfade starts the next song on the other deck at volume 0 and ramps both on equal-power curves. The fading deck's own end or error only empties it, so the queue moves once. A skip fades the old song out over 0.35 s.
- **Ramps** run on the island's timer only while needed: 30 ms while a volume moves, 60 ms in the last seconds of a song, 500 ms otherwise, none when nothing can crossfade.
- **Handoff fades** use the same ramps for the island's songs. For other apps they use the app's volume in Windows' mixer: lowered on a cosine, paused, then put back.
- **Up next** is `upNext` / `move` / `jump` on the queue after the song playing. The Media page's view drags rows with the pointer: a press that moves more than 5 DIPs vertically lifts the row. The rows glide toward their slots with a 75 ms time constant, redrawn at 60 Hz only while they move.
- **The next cover** is drawn in the content surface, behind the cover visual, rotated 5°, so its edge shows.

**Sharing, revision 1.**
- **Revision.** Discovery sends `port;2.1`. A 0.16 PC reads the 2, and a 0.17 PC reads the revision too. Nothing a 0.16 PC receives changes.
- **Covers** follow a music offer's text after a zero byte (a zero can't occur in the text: it is stripped).
- **Shelf** listing (`L`) and taking (`T`) are new modes. The owner lists up to 32 items and serves only an item at the listed place with the listed name, so a Shelf changed since the list is refused (*It's no longer on …'s Shelf*). A take is an ordinary transfer, so progress, Stop and the checks all apply.
- **Speed** is an eased average of bytes over quarter-second samples.

**Alerts.** Spreading is a spring from 0 to 1. The pill's offset and the bud's shape are functions of it in all four places the drop pill already lives: the DirectComposition clip curves, the glass expressions, hit tests and the window region. Leaving starts a 380 ms timer, so moving across the gap between the two doesn't close them.

**Screen readers.** A UI Automation fragment tree from the same hit targets the pointer uses. Names are composed per action, from what each shows (a song's title, a file, a PC). Switches report their state and sliders their range. Notification events are raised only while a client listens.

**Fixes.**
- **Stray logo.** The command bar showed the last alert's icon.
- **Cover over lists.** The cover sat over the Library and Up next.
- **Rows and footers.** Rows ran into their footers, and the Outputs chevron sat outside its button.
- **Sky over text.** The weather sky was drawn over the Home tile's text.
- **Contrast.** Light-theme text on glass was too faint.
- **Settings text.** Long descriptions were cut off.
- **Fill width.** The solid fill was 40 DIPs short of the canvas.
- **Command bar.** It could open without its service in test runs, and its first search waited on the radios.
- **Duplicate towns.** The same town could be listed twice.

## Verification

- **Unit suites, all passing:**
  - **Phase: 5,613 checks** (29 new). They cover:
    - v16 settings, their defaults and limits
    - the Settings items
    - the side-by-side geometry, which stays below the pill unspread, sits beside it spread, and glides with corners that always fit
    - time-left wording
    - regional town names, one row per name, and search limits
  - **Share: 165 checks** (46 new). They cover:
    - covers byte for byte, including zero bytes, and never sent to a revision 0 PC
    - revision 0 PCs not asked for their Shelf
    - a closed Shelf
    - a listed Shelf with previews and folder sizes
    - taking a file and a folder tree
    - the owner told
    - an item since removed, or at another place, refused
  - **Model: 2,061. Core: 11,682. Provider lifecycle: pass.**
- **Native UI test: 45 stages**, 3 new:
  - two alerts spreading, with the side card's hit test and the old place no longer answering
  - Up next's rows and arrows, and another PC's Shelf with its items, back button and the Nearby row's Shelf button
  - every target on the Controls page having a spoken name
- **Settings end to end:** passing. The data folder was unchanged afterwards apart from the test's own files and the event log.
- **Live checks on this PC:**
  - **Town search:** the field found Manipal (two regions) and saved Manipal, Udupi, Jubail, Indore, Jaipur and Mumbai, each with its weather.
  - **Crossfade:** two of Windows' own sounds (muted) with a 1 s crossfade. The second began 1 s before the first ended, and the queue moved exactly once. Moves in Up next were accepted and bad ones refused.
  - **Screen readers:** a UI Automation client read the pane, its expand state and its children, with names, roles, rectangles, switch states and slider values. It pressed *Stats*, collapsed and expanded the island, and heard an alert announced.
- **Captures** were drawn by the island itself, with illustrative data only:
  - side by side, and the Play here card with a cover
  - Up next mid-drag, and the peeking cover
  - another PC's Shelf, and the big-transfer ring

## Limits

- **Two PCs.** Sharing, handoff, covers and Shelf taking were verified between two independent services on one PC, not between two physical PCs. The cover and Shelf taking need 0.17 on both.
- **Crossfade** is for the island's own songs.
- **The Settings window** is not yet exposed to screen readers. The island is.
- **Side by side** needs the drop pill (top dock).
- **Unsigned preview.**

## 0.17.0-preview.2

**Glass.** Three separate failures, each making `GlassBackdrop::animate` or `layout` throw and leave the glass behind:
- **`sp` and `ss`** (the side-by-side spring and shift) weren't created with the property set. Starting a spring on a missing property fails with `E_INVALIDARG`, before the width, height and radius springs start. Fixed with one list, `glassProperties`, that creates them all. A phase test scans every expression literal for `p.<name>` and every started spring, against that list.
- **The bud's corner** was a single expression over a thousand characters long, written twice in a `Vector2`. It is now built from properties (`bt0`, `bb0`, `bw0`, then `bl`, `bt`, `bv`, `bb`, `bc`), each from a short expression.
- **`expressionNumber`** used `%.9g`, which writes exponents for tiny values (a settling spring's terms). Expressions reject them. It now writes plain decimals to about nine significant digits, and treats anything under 1e-9 as 0.
- **Why this wasn't caught.** The UI test checked that the glass was visible, never that its animations had started. It now reports the glass's last error, and the step that failed names the property and expression.

**Lyrics.** `MediaProvider` now calls `timelinePosition`: while playing, the reported position plus the time since the timeline's `LastUpdatedTime` (from the future, missing or more than six hours old: ignored). The live check used Windows' `MediaPlayer` on one of Windows' own sounds:

| Moment | True position | Reported | Report's age | Corrected |
|---:|---:|---:|---:|---:|
| 3 s | 2.91 s | 0.00 s | 2.91 s | 2.92 s |
| 6 s | 5.91 s | 0.00 s | 5.91 s | 5.91 s |
| 9 s | 8.91 s | 6.04 s | 2.87 s | 8.91 s |

**Audit.** 78 island views from its internal render, and 18 Settings pages saved by the Settings window itself (`--qa-settings-sweep`, test runs only), reviewed one by one. No screen was captured.

## 0.17.0-preview.3

**The shoulder seam.** Two things differed between the body and the shoulders:
- The DirectComposition sheen was a child of the body, so its clip cut it off at the shoulders. The light now lives in the glass: a sprite in each of the body and both shoulders, placed by `p.px`, `p.py` and faded by `p.po` on its own clock (`p.tp`).
- The left and right edge-light strips started at the body's top, which (when docked) is the shoulders' inner edge, so light ran down the join. They now start `p.r` lower while the island is docked.

Over a plain backdrop, Frosted: shoulder 31,34,41 and body 30,34,41.

**The lines.** The alert glint was a band swept by one clip rectangle whose ends were hard. It's now four bands (core and glow, each way), each drawn through six nested windows at a sixth of the brightness. The light rises and falls over the width of the windows.

**Why refraction isn't in.** Probed at run time:

| Effect over the host backdrop | Result |
|---|---|
| Colour matrix | Accepted |
| Gaussian blur | Accepted |
| Border | Accepted |
| 2D affine transform (matrix only, three or four properties, each interpolation and border mode, identity) | `E_INVALIDARG` |
| 2D affine transform after a colour matrix | `E_INVALIDARG` |
| Scale | `E_INVALIDARG` |

Composition never asked for a named property mapping, so the effect itself is refused. The code that tried it was removed; the edge light is unchanged.

**The frost.** Measured over a plain backdrop, from the island's own capture, with the settling sped up for the test (`--qa-frost`):

| Theme | Rest | Settled |
|---|---|---|
| Dark | 32, 36, 44 | 43, 45, 49 |
| Light | 125, 128, 130 | 136, 138, 141 |

A first test looked like the frost did nothing. The sped-up pace was set after the frost had started at its real pace. The test hook now restarts it.

**The beat light.** `Renderer::beat` compares the bass (50–130 Hz) with its half-second average. The level rests at 0.14 plus up to 0.34 with the average, flares within 70 ms on a hit, and falls back over 0.55 s. On glass, the rim's two beat strokes take their colour's alpha from `p.be`, a Hermite glide on its own clock (`p.tb`). The loopback analyser now also runs for the light while music plays and the island is visible. The bars' `waveform` flag is unaffected.

**Settings previews.** Rows gain `base` (their own height; controls stay centred in it) and `open`. A click kills the 450 ms dwell timer, so the Settings test, which moves and clicks at once, never opens one. The window draws at about 30 fps while a picture moves, and `settled` ignores it.

**Found in the audit.**
- **The low sun on the label.** At dawn and dusk, the low sun sat on *Sunrise* or *Sunset*. The word now moves left of it.
- **Live audio style preview.** The ring sat over the preview's title line. The line now stops short of it.
- **The Settings test** left Material on Clear, so it clicked Frosted's tint and frost while they were disabled (4 failures). It now picks the glass each of those rows belongs to first.

## 0.18.0-preview.1

**Updates.** `UpdateService` asks `api.github.com/repos/Arnav-Dugad/arnav-island/releases` (90 s after start, then every six hours; after a failure, in 30 minutes).

It picks the newest non-draft release newer than the running version, and only if both assets exist and come from `https://github.com/Arnav-Dugad/arnav-island/releases/download/` (`newestRelease`). Then it:
1. downloads the checksum and the ZIP
2. checks the SHA-256 (BCrypt)
3. unpacks with `%SystemRoot%\System32\tar.exe`
4. requires the unpacked `ArnavIsland.exe`'s ProductVersion to equal the release's version

Installing, at a quiet moment:
- The running program is renamed `ArnavIsland.old.exe` (a running image can be renamed, not overwritten) and the new one is copied in.
- The new program starts only after this process has closed its single-instance mutex (`UpdateService::launchPending` from `wWinMain`), so any version, even one that doesn't know `--after`, starts cleanly.
- If it can't start, the old program is moved back and restarted.
- The next start deletes `ArnavIsland.old.exe`.

The first end-to-end run showed why the relaunch moved. Starting the new version from inside the old one, the older build found the mutex still held and quit.

**Alerts.** `showNotice`, `showHeadphoneCard` and `showPrivacyNotice` returned early unless the island was compact or already showing an alert. So anything arriving while it was open, in Live or in the command bar was lost, including camera and microphone cards. They now go to `deferCard`: the held-card queue, marked deferred and time-stamped. `promoteCard` shows them when the island returns to compact, and drops deferred ones over 60 s old.

**Battery.** `BatteryProvider::query` now reads:
- `DefaultAlert1` and `DefaultAlert2`, `CriticalBias` and `Technology`
- the serial number, and the optional temperature, manufacture date and estimated time
- battery saver and Windows' lifetime (`GetSystemPowerStatus`)
- the number of batteries

This PC's battery reports capacities, voltage, chemistry, manufacturer, model and serial, but no temperature, date, cycles or alert levels. Its chemistry code is a vendor code (`OOI0`), which is now left out rather than shown.

**Weather on the glass.** DirectComposition visuals in the body, above the glass, below the content:
- Rain or snow is one tile drawn twice and scrolled forever (`IDCompositionAnimation::AddRepeat`).
- Drops are drawn once, three in five within the resting island's height.
- Fog is two tiles drifting sideways.

It first showed only while the island was open: the resting island redraws its header only, and the effect was updated on full redraws. It now updates on both.
