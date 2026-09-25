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
