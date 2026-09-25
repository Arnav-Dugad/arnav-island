# Arnav Island v0.15 — development report

## What changed

**Notifications.**
- **Drop pill.** A new `drop` spring runs from 0 (merged) to 1 (dropped).
  - While it is out, the body is offset by `drop × 42` DIPs as a floating pill. A stub of the compact island (its width, at most 196 DIPs) stays docked with the shoulders.
  - The body's outline starts above the screen edge and comes down twice as fast as the body (`top = min(2·dp − E, dp)`), so its corners round out as it leaves instead of popping.
  - The shoulders and the stub move from the body's width to the stub's only after the pill has left the edge (`(dp − E/2) / 12`). The stub reaches just one DIP into the pill, so see-through glass never shows the two overlapping.
  - The same expressions drive four renderers: DirectComposition (body offset, animated clips and wing positions through a new `curveOf` helper), the Windows.UI.Composition glass (a fourth part with its own rounded geometry, layers and rim), the shadow window (a second nine-grid sprite for the stub) and the input region (two outlines; the gap between them is click-through).
  - Pointer mapping goes through one `bodyAt(w, h, drop)` everywhere.
- **Edge light.** The island's outline (the free edges only when docked) is stroked into three surfaces: a core line, a dimmer line and a glow. Six clipped bands, narrowest brightest, sweep out from the middle in 0.8 s under one fade. It plays on device, power, privacy, capture, headphone and sharing cards.
- **Privacy card.** A **Settings** button opens the capability's `ms-settings:privacy-*` page, and clicking the compact dots shows the card again.

**Now Playing.**
- **Compact controls.** Previous, play/pause and next are hit in the header's own coordinates, and hover-to-open waits while the pointer is over them, so pressing a control never opens the island.
- **Swipe to skip.** `WM_MOUSEHWHEEL` accumulates (with a reset window), or a sideways release past 44 DIPs; the title kicks in the direction of the swipe.
- **Fullscreen peek.** While a fullscreen app has hidden the island and something plays, the screen edge above it shows the island (a 120 ms poll that runs only then). It hides 0.8 s after the pointer leaves.
- **Spectrum ring.** 24 ticks around a round cover, mirrored from the loopback bands.
- **Word timing.** Enhanced-LRC `<mm:ss.xx>` tags become `LyricWord`s, kept only when in order and shifted with repeated lines and the offset. `lyricFill` turns a line into (time, characters lit) keyframes. The compact island and the Live Island now use the Command Center's two-slot morphing line.
- **App accents.** With no artwork, the accent comes from the playing app's icon, cached per icon.

**Awareness.**
- **Weather.** Open-Meteo geocoding (once per town) and forecast (every 30 minutes) over WinHTTP, parsed by pure, tested functions. The place is stored in `weather.nexus`.
- **Animated sky.** The Home tile's sky is DirectComposition animations looped with `AddRepeat`: rays turning, twinkling stars, drifting clouds, falling streaks and flakes, fog bands and a double storm flash. It runs for 60 s each time the tile appears, and every loop ends exactly where it is, so nothing jumps when it rests.
- **Battery health.** `BatteryHealthLog` stores one full-charge/design reading a day. `summarizeWeek` counts charges, use per day and hours on battery from the existing 7-day history. The card shows from 9 am after three days of history, a week after the last one.
- **Rich clipboard rows.** `linkHost` / `linkPath`, a colour swatch, `looksLikeCode` and `codeSpans` (keywords, strings, numbers, comments, punctuation) drawn in Cascadia Mono or Consolas. Site icons are opt-in and decoded from `/favicon.ico` through WIC.

**Controls page.**
- Six tiles and two sliders. Radios, dark mode and the microphone run on a worker, and their live states are polled every 2 s while the page shows.
- Brightness goes through `WmiSetBrightness` on a setter thread, and the level HUD is suppressed for the island's own changes.

**Sharing between your own PCs** (`ShareService`, Winsock + CNG).
- **Discovery.** UDP broadcast announces an id, a port and the computer's name every 3 s. A PC unheard for 12 s is offline.
- **Handshake.** The initiator sends its id, its P-256 public key and a *commitment* (SHA-256) to a random nonce. The responder answers with its id, key and nonce, and only then does the initiator reveal its nonce. So neither side can steer the pairing code.
  - Session key: `SHA-256("arnav-share-v1" ‖ ECDH ‖ nonces ‖ ids)`.
  - Pairing code: `SHA-256("arnav-pair-v1" ‖ keys ‖ nonces) mod 10⁶`.
- **Pairing.** Both people confirm the code. Their answers are exchanged sealed, and the peer's key is stored only when both said yes.
- **Transfers.**
  - Sent only to a paired PC whose key matches, and only after the receiving person accepts.
  - AES-256-GCM frames with a direction byte and counter as the nonce, 256 KB chunks, and an end frame carrying the size and SHA-256.
  - Received as `.arnavpart`, verified, then renamed to a free name in Downloads.
- **Names and storage.** Received names are made safe (no folders, reserved names or unsafe characters; at most 120 characters). The private key rests under DPAPI.

**The island.**
- **Chips editor.** A Settings control with drag and arrow keys. The order is stored as `chip0..chip6`.
- **Controls in the navigation.** The Controls page joins saved navigation orders after Stats (settings v14).
- **Adaptive text.**
  - The monitor's wallpaper is placed as Windows places it (`IDesktopWallpaper`: fill, fit, stretch, centre, tile) into a 480-wide luminance map on a worker.
  - A 2-DIP grid under the compact island is darkened by the Clear scrim's own alpha.
  - `text()` sets a per-run brush on a DirectWrite layout, with the halo inverted. Icons pick their ink at their centre, and rolling digits choose between two strips per column.
  - It applies only while no other window overlaps the island (checked on foreground and location changes and every 1.5 s).

**Motion.**
- **Digit blur.** A second strip draws each figure seven times along the roll at 20% alpha. On jumps of 2.5 figures or more, its opacity, the sharp strip's opacity and a 10% vertical stretch follow the roll's speed.
- **Morphing icons.**
  - `drawMorph` blends play ↔ pause quads, the speaker's waves ↔ its cross, and the microphone's slash. `drawIconAnimated` gives each icon a short celebration.
  - Both are flipbooks: a strip of frames in one surface, its offset stepped by the compositor.
- **Lean.**
  - Dragged against the top edge, the root gets a shear (0.06° per DIP) and a stretch.
  - The glass gets the same as a `TransformMatrix` expression, and the input region is transformed to match.
  - The transform is attached only while the island leans.
- **Rising shadow.** The shadow's margin, drop and opacity grow with the island's height.

## Bugs found while testing

- **The first weather sky crashed the app at startup** (`0x88980801`): DirectComposition lets only one surface draw at a time, and the sky's surfaces were drawn while the content surface was still open. They are now drawn after it closes.
- **Two-row lyrics filled both rows at once.** Each row's fill was interpolated only between the line's own keyframes, so the second row spread across the whole line. Rows now get a keyframe at every character edge the fill crosses, and the compact line got the same. It was found by capturing the line at five moments.
- **A one-pixel sliver at the content's right edge** appeared intermittently on the Media page. It came in about half of the runs and was traced by hiding layers one at a time. The content surface is now four DIPs wider and taller than the bands that show it, so an edge sample reads its own clear pixels.
- **Clear glass showed the stub's outline through the pill** for a few frames. The stub now stops one DIP into the pill.
- **The shoulders slid over a pill that hadn't left the edge yet.** They now move only after the pill detaches.
- **"Thunderstorm" was cut off on the Home tile** (now "Storm" there). **The sun covered the end of the place name** (now smaller and in the corner). **The received-file card cut off its detail** (now the file name only).
- **Test harness:** the auto-hide stages of the UI test left the island slid away for the stages after them (the new stages now start from a shown island). The settings test outgrew a five-minute limit (it takes 317 s).

## Verification

- **Unit suites: 5 of 5 pass.**
  - **Phase: 5,534 checks** (102 new since 0.14), covering:
    - word-timed lyrics (parsing, repeats, offsets and fill keyframes)
    - link hosts and paths, code detection and colouring
    - weather parsing, the place file, WMO codes and units, the weather command
    - the health log and the week
    - chips, the Controls migration and v13 → v14 settings
    - the adaptive grid and the drop distance
  - **Share: 49 checks** (a new suite). Two independent services with their own identities, over loopback:
    - refusal before pairing
    - a declined pairing, then matching codes
    - a declined file, and two byte-exact 1 MB transfers (the second gets a free name)
    - identity and pairing kept across a restart
    - refusal after the other PC forgets this one
    - safe file names
  - **Model: 2,061 checks. Core: 11,682 checks. Provider lifecycle: pass.**
- **Native UI test: 35 stages, 7 new.** Nothing reaches the real player, Wi-Fi or Bluetooth.
  - The compact controls are hit where they are drawn (checked through `hit()`, no click).
  - The Controls page lays out its tiles and sliders.
  - The drop pill's input region covers the stub and the pill but not the gap.
  - The Nearby tab offers Pair and Forget and chooses a target.
  - The fullscreen peek shows and hides.
- **Settings end-to-end: 176 of 176.** The user's settings file was byte-identical afterwards.
- **Captures, on the island's own matte:**
  - the drop and the return caught mid-motion on Frosted, Clear and Solid
  - the edge light mid-sweep
  - digits mid-blur
  - all eight skies
  - adaptive text over an illustrative backdrop
  - the sharing tab and cards
  - the sequential lyric fill
- **Live:** a borderless fullscreen window hid the island as it should. Nothing was playing on the PC, so the peek itself was verified by the UI test's made-up track.
- **Idle (compact, the PC's own settings, 30 s):**
  - v0.15: 0.000 s CPU, 78.2 MB private
  - v0.14 under the same conditions: 0.031 s, 77.0 MB
  - raw data: `evidence/v0.15/idle-compact*.json`

Screenshots in `evidence/v0.15` use the island's own matte, the showcase track, sample clips, illustrative PCs ("Studio PC", "Travel laptop") and a sample town.

## Limits

- **Lock screen:** apps can't draw on Windows' secure desktop.
- **Sharing** has been verified over loopback, not between two physical PCs. Discovery is UDP broadcast on one network segment, and Windows may ask about the firewall the first time.
- **Word-by-word lyrics** need lyrics with word tags; LRCLIB's are mostly line-timed.
- **Adaptive text** works from the wallpaper, so it switches off when a window is behind the island. A span wallpaper is treated as fill.
- **The drop pill** and **the lean** are top-dock only.
- **Unsigned preview.** There is no UI Automation tree for screen readers yet.
