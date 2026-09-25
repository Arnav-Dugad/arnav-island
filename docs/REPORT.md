# Arnav Island v0.16 — development report

## What changed

**Sharing (protocol 2).**
- **Batches and folders.** A transfer is sealed frames:
  - an offer (file count, total size, a title)
  - per file, a header (size and relative path), 256 KB data frames and an end frame with its size and SHA-256
  - a batch end, which the receiver acknowledges once every file is saved
- **Saving.** Each file is written as `.arnavpart`, checked, then renamed. Each top-level folder takes a free name in Downloads, and every received path goes through `safeSharePath`: no `..` or empty parts, reserved names or unsafe characters, and at most 24 levels.
- **Stopping.** On Windows a blocking `recv` doesn't wake when another thread shuts the socket down, so every wait now uses `select` in 200 ms slices and checks the transfer's stop flag. While a person decides, the receiver watches for the sender closing (`MSG_PEEK`), so a withdrawn offer's card goes away.
- **Versions.** Discovery sends `port;2`: 0.15 still reads the port, and 0.16 learns the other PC's protocol. A 0.15 PC is shown as needing the update, and a mismatched handshake answers 2 ("outdated") instead of closing.
- **Drop zones.** `ShelfDropTarget` gained `route(over, send)`. While a drag is over the island, the Shelf page's zones replace its tabs and rows, and the zone under the pointer is found with the renderer's own hit targets.
- **The stack.** A drag from the Shelf's count is one `CF_HDROP` with every file.
- **Progress.** Events are limited to one per percent and 100 ms. The window redraws for them at most five times a second.

**Music.**
- **The player** is `IMFMediaEngine` (audio only) plus `ISystemMediaTransportControls` for the island's window, through the toolchain's own `windows.media.h`. That header defines `IReference<boolean>` and `IReference<BYTE>`, which are the same type here, so the player's file skips the first by its guard.
- **How it joins the island.** Engine events and media keys come back as `PlayerMessage`. `updateSessions` puts the island's session first and drops the copy Windows reports for it (source `ArnavIsland.exe`). Controls and seeks go through `mediaCommand` and `mediaSeek`, which address either the island's player or Windows' sessions.
- **The library** is scanned on a worker the first time it's wanted: the shell property store for title, artist, album, track and length, and shell thumbnails for covers (thumbnail only, so there are no generic icons). Only the rows on screen, the song playing and the next song have their covers read, and at most 64 are kept.
- **Handoff** is its own sharing mode. The receiving PC tries four routes in turn:
  1. a Windows session with the same title (seek, then play)
  2. its library (`matchTrack`: title and artist, else file name and size)
  3. the song's file, when the sender played it from one (answer 2 streams it)
  4. launching the app: `spotify:`, `IApplicationActivationManager` for packaged ids, or `shell:AppsFolder`. It then waits up to 20 s for that app's session to show the song, seeks, and presses play once.
  
  The sender pauses when the other PC accepts.
- **Drag to skip.** `mediaSwipe()` decides where a sideways gesture skips: over the music in the compact island, the Live Island, Home and the Media page. A press that starts on a button and travels 10 DIPs sideways becomes a swipe. `swipeFollow` moves the compact header with the drag and grows the chip; `swipeEnd` sends it off or springs it back. Touchpad sideways scrolling skips too.
- **Island DJ.** Two halos sit behind the ring. The bloom is two eases, 1.3 s. The end glow's opacity animation is held at zero until 10 s before the end, then repeats a 1.8 s cycle with `AddRepeat`. Nothing is redrawn while it plays.

**Two alerts at once.**
- **Holding.** `holdCard` runs at the five places a card appears. When a card is already showing and the new one is a different alert (not the same kind and subject), the new notice and its activity are queued, and the showing notice is restored.
- **Promoting.** `promoteCard` brings the next card forward from the activity timer, from any transition to compact, or from a click on the bud. Clicking the bud keeps an unanswered pairing, offer or music card waiting.
- **The bud's shape** comes from `budShape(b, pillFoot, centre, width, height)`. It grows down as a capsule touching the pill (b < 0.55), then lets go and settles 8 DIPs below, springing at ζ ≈ 0.58. The same shape drives:
  - a clipped DirectComposition visual (the fill on Solid, the label on every material)
  - a fifth glass part with its own rim (expressions over `b`, `bw` and `bh`)
  - a shadow sprite
  - the input region and the hit test

**The clipboard across restarts.**
- **What's saved.** `saveHistory` and `loadHistory` handle a small binary format: kinds, pins, wall-clock times, sources, text, file lists and image PNGs. It's DPAPI-sealed into `clips-history.nexus`.
- **Images** get their PNG once, on a worker, when they're copied (`dibToPng`). On load they're decoded back to a bottom-up 32-bit DIB and a thumbnail.
- **Timing.** Saves are debounced (1.5 s) and change-detected, flushed on `WM_ENDSESSION` and at exit, and never happen before the saved history has been read back. That way a quick copy at start-up can't overwrite the file with a shorter history.

**Weather.** `httpsGet` makes one attempt with WinHTTP's decompression. If the server answered 200 but the body can't be read (as with Open-Meteo), it tries once more without asking for compression. The weather service keeps an unreachable town and retries every 2 minutes.

**Motion and sound.**
- **The liquid pill.** Two edge springs: the leading end is stiffer ({.7, 560, 34}), the trailing one softer ({.9, 300, 30}). Two caps are clipped from the pill's surface, and a 2-DIP middle is scaled by a `curveOf` of the gap between them.
- **Sounds** are 16-bit WAVs synthesised in memory: two bell partials for the chime (-18 dBFS peak) and a filtered tick for the click (-20 dBFS). They're played with `PlaySound` and muted in test runs.

## Bugs found while testing

- **Weather never worked in 0.15.** The unit tests covered parsing, not the network. A probe showed WinHTTP returning 200 and then failing `WinHttpQueryDataAvailable` with `E_ABORT`, but only with decompression on.
- **Drag to skip** only worked in the compact island, and with *Open on hover* the island had become the Live Island before a drag began. In the Live Island a drag only switched players.
- **Stopping a transfer hung** until the other PC answered (a blocking `recv` doesn't wake on `shutdown`). Also, a receiver that stopped before answering reported nothing.
- **A file named `.mp3`** got the title ".mp3".
- **The first drop layout** drew under the tabs and the tab highlight. The stack's thumbnails overlapped its count, and a Nearby row's progress bar crowded its status.
- **The UI test's fullscreen stage** could be undone by real foreground changes elsewhere on the desktop.
- **Island DJ's next colour** waited for an unrelated redraw after the next cover loaded.
- **The settings test deleted the tester's own command history** (since v0.13). Switching *Remember recent commands* off removes `commands.nexus`, and that wasn't guarded in test runs. It is now (preview.2), checked with a placeholder file that survives a full settings test.

## Verification

- **Unit suites, all passing:**
  - **Phase: 5,584 checks**, 32 new: the clipboard history format (secrets, images without a PNG yet, ages, cut-off files, restore order), the library's naming, sorting, search, matching and shuffles, the bud's shape, the sounds, and settings v15.
  - **Share: 119 checks** (70 new): batches and folder trees byte for byte, free folder names, stopping from either side (and the other PC told), an outdated PC, handoff declined, accepted, and accepted with the song's file, and received paths cleaned.
  - **Model: 2,061. Core: 11,682. Provider lifecycle: pass.**
- **Native UI test: 42 stages**, 7 new:
  - a real sideways drag across the Live Island skips forward and back, and a short one doesn't
  - drop zones, and the zone under a point
  - two alerts: holding, updating in place, the bud's hit test, and promotion
  - the Media page's Library and Continue on buttons
- **Settings end-to-end: 188 of 188.** The user's settings file was byte-identical afterwards.
- **Live checks on this PC:**
  - the weather service end to end: a sample town found, the forecast parsed, the place saved
  - the island's player playing Windows' own sounds (muted), seen by Windows as a media session with its title, state and length
  - the keyboard's play/pause key pausing it
- **Captures, on the island's own matte:**
  - the player and the library (Windows' sounds)
  - drop zones, a transfer with Stop, and the stack
  - two alerts on Frosted, Clear and Solid
  - the music card and the Continue on picker
  - the liquid pill mid-move, Island DJ's halo and the skip chip
- **Idle:** 0.031–0.047 s CPU in 30 s, against 0.016 s for 0.15 today (one to three scheduler ticks), and 77–79 MB private.

## Limits

- **Sharing and handoff** were verified between two independent services on one PC, not between two physical PCs. Both PCs need 0.16.
- **Handoff** can't make an app play a song it doesn't have. Browser tabs can't follow.
- **Island DJ** knows the next song only in the island's own queue.
- **Two alerts at once** needs the drop pill (top dock).
- **The music library** reads the Music folder only (8,000 songs, 12 levels).
- **Unsigned preview.** There's no UI Automation tree yet.
