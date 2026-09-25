# Arnav Island 0.16.0-preview.2 — music from the island, sharing that goes where you drop it

**preview.2** fixes a developer-test problem: when the settings test switched *Remember recent commands* off, it deleted the real command history file of whoever ran it (test runs now leave it alone). Nothing else changed since preview.1.

Phase 5G:
- Drop files and folders straight onto your other PC, or send the whole Shelf at once.
- Play songs from your Music folder in the island itself, and hand the music over to your other PC.
- Two alerts at once: the second buds off the first.
- Drag the music sideways to skip, anywhere it shows.
- The clipboard remembers after a restart.
- Weather works again.
- Sounds, Island DJ and a liquid navigation pill.

## Sharing between your own PCs
- **Drop onto a PC.** Drag files or folders over the island. It shows the Shelf on one side and your paired PCs on the other; let go over a PC to send them there.
- **Folders.** A folder arrives whole, with everything inside it, under its own name in Downloads (*Photos (2)* if you already have *Photos*).
- **The whole Shelf at once.** Each paired PC in Shelf › Nearby has **Send Shelf**. The Shelf's item count is also a stack: drag it onto a PC in the island, or out of the island to drop every file anywhere.
- **See it move.** A PC's row fills as a transfer goes, and **Stop** ends it from either PC. The compact island shows a chip with the percentage.
- **Clearer answers.** Offers say how many files there are and how big they are. The other PC is told when you stop sending. If Downloads doesn't have room, you're told before anything is written.
- **Update both PCs.** This version's sharing (protocol 2) needs 0.16 on both PCs. A PC still on 0.15 shows *Needs the latest Arnav Island* instead of failing silently.

## Music
- **Play from the island.** Media › **Library** lists the songs in your Music folder (MP3, M4A, AAC, FLAC, WAV, WMA, Ogg and Opus) with their covers. Pick one, or press **Shuffle all**. With nothing playing, the Media page offers **Shuffle my music**; in the command bar, type *play* and a song, or *shuffle*.
- **A real player.** Songs the island plays appear in Windows' own media controls. The keyboard's media keys control them (checked), and Windows' media flyout shows them. Starting a song pauses whatever else was playing.
- **Continue on my other PC.** With sharing on, the Media page's **Continue on** button (or *continue on* in the command bar) offers what's playing to a paired PC. The other PC shows *Play here*. It plays the song from where you were:
  - in a player that already has it
  - from its own Music folder
  - from the song's file, sent along when the island was playing it
  - by opening the same app, for Spotify and Store apps
  
  This PC then pauses.
- **Drag to skip, everywhere.** A sideways drag over the music now skips tracks in the compact island, the Live Island, Home and the Media page. A chip on the leading side grows as you drag and snaps when letting go would skip. Before, the Live Island only switched between players on a drag, and with *Open on hover* the island had usually opened into it by the time you dragged. Players now switch with the dots under the cover.
- **Island DJ.** Near the end of a track, a halo breathes behind the compact ring in the colours of what plays next. The island's own queue knows the next song; for other apps it uses the colours of what plays now. A new track blooms in its cover's colours as it starts.

## Alerts
- **Two alerts at once.** When a second alert arrives while one shows, it grows out of the first pill's foot as a bud, then lets go and settles just below it. It takes the pill's place when the first ends; click it to bring it forward. Up to four can wait, and a pairing code, file offer or music offer is never lost.
- **Sounds.** A faint two-note chime plays with the light along the edge, and a soft click as chips move in Settings › Compact. They're made by the app itself (no sound files), quiet on purpose, and silent while something plays full screen.

## Clipboard
- **Remembers after a restart.** The clipboard history is saved on this PC, encrypted for your Windows account, and comes back when the island starts. Images are kept as PNG. Copies that look like passwords or keys are kept only if you pin them. Turn it off in Settings › Privacy & productivity › *Remember the clipboard after restarts* (the saved copy is deleted).

## Weather
- **Fixed.** *Open-Meteo couldn't be reached* appeared on every lookup: Open-Meteo answers in a compressed form Windows' web client couldn't unpack. The island now asks again without compression when that happens. If the network really is down, it keeps your town and tries again every two minutes. If you tried a town on 0.15, type *weather* and the town once more.

## Motion
- **A liquid navigation pill.** The highlight under the navigation stretches toward the page you pick, its leading end first, then draws its tail in, keeping its corners round.

## Settings (version 15)
- **New and on:**
  - remember the clipboard after restarts
  - sounds
  - continue on my other PC (only with sharing on)
  - Island DJ
  - two alerts at once
  - music library
- Nothing new goes online. The library reads your Music folder on this PC only, and handing music over goes only to your own paired PCs.

## Limits
- **Handoff** plays the same song on the other PC when a player there has it, when your library there has it, when the island was playing it from a file, or when the app itself resumes it. Otherwise it opens the same app and presses play, and moves to where you were once that app shows the same song; whether the app picks up the same song is up to the app. Songs in a browser tab can't follow, because browsers don't expose the page.
- **Sharing and handoff** were tested end to end between two independent services on one PC (loopback), not between two physical PCs.
- **The music library** reads one folder: your Music folder, up to 8,000 songs, 12 folders deep. The list is kept in memory.
- **Island DJ** knows the next song only in the island's own queue.
- **Two alerts at once** needs the drop pill (top dock).
- **Not on the lock screen** as an island (Windows' secure desktop).
