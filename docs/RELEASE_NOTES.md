# Arnav Island 0.17.0-preview.2 — glass fixed, lyrics on time

## Fixed
- **Frosted and Clear glass.** In 0.17.0-preview.1 the glass stopped following the island. Every time the island changed shape, the glass tried to animate a value it had never created, Windows refused, and the rest of that update was skipped. The glass is back, and the island's own test now fails if the glass ever reports an error. Two more causes of broken glass are fixed:
  - **Long expression.** The shape of a waiting alert's card was one expression too long for Windows to accept. It's now built from short steps.
  - **Scientific notation.** A spring that had nearly settled wrote numbers like `2e-05`, which Windows' expressions don't accept. The glass could freeze for a moment when that happened, and this goes back to earlier versions. Numbers are now always plain decimals.
- **Lyrics on time.** Windows reports where a player is as of the moment that player last told it. Many players, Spotify among them, tell it only now and then. The island took that report as the current position, so lyrics could run several seconds behind and jump back. It now adds the time since the report. Tested with a real Windows media session: the report was up to 5.9 s behind the music; corrected, the island is within a hundredth of a second.
- **Offer cards keep the size.** A long file name no longer pushes the size off the card. The name is shortened instead, as in *Holiday pho… · 12 files · 48.2 MB*.
- **Outputs** show a speaker for speakers and headphones for headphones, from Windows' own device type.
- **The command bar** shows the right icons for the weather, a song, shuffling and continuing on another PC. *weather* alone reads *Opens Settings › Compact › Town*.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,621 (8 new); share 165; provider lifecycle passing.
- **Native UI test:** 50 stages, 5 new. Clear and Frosted glass go through opening, the command bar, two alerts dropping and spreading, and back, with no glass errors allowed at any stage.
- **Settings end to end:** 192 of 192.
- **Visual audit:** every page, tab, card, compact mode, side dock and command state, in dark and light, drawn by the island itself (78 views). Every Settings section and page, drawn by the Settings window itself (18 views).

# Arnav Island 0.17.0-preview.1 — Up next, your other PC's Shelf, and an island you can hear

Phase 5H:
- The weather's town is chosen in Settings, and finds towns anywhere.
- Up next, with drag to reorder. The next song's cover peeks out from behind the one playing.
- Crossfades between songs. Music fades out when it moves to your other PC, and fades in there.
- The other PC's cover on *Play here*.
- Take files from your other PC's Shelf.
- A ring for big transfers that shows their speed and time left.
- Two alerts spread side by side when you point at them.
- Screen readers can read and use the island.
- Many visual fixes.

## Fixed
- **A logo in the command bar.** The search page could show the icon of the last alert, like a pair of headphones' brand logo. The command bar never shows an alert's icon now.
- **The cover on top of the Library.** With a song playing, its cover sat over the Library list. The cover now shows only on the Now playing view.
- **Rows touching the footer.** Shelf, Clipboard, Nearby and Apps rows ran into the page's footer. The chevron of the Outputs button sat outside it.
- **The weather sky over text.** The Home tile's sky was drawn over the tile's words.
- **Light theme on glass.** Text had too little contrast.
- **Settings.** Long descriptions were cut off.
- **The first command.** The command bar's first search of a session could wait on the Wi-Fi and Bluetooth state; it no longer does.
- **One town listed twice.** Two places with the same name (a town and its district) showed as two identical rows.

## Weather
- **Choose the town in Settings.** Settings › Compact › **Town**: type the town, then pick it from the list. Towns show with their region and country, so you can tell *Manipal, Karnataka, India* from *Manipal, Gandaki Pradesh, Nepal*. It works for towns anywhere: Manipal, Udupi, Jubail, Indore, Jaipur, Mumbai and so on.
- Settings then shows *Showing the weather for …*, with the temperature and the sky.
- Typing *weather* alone in the command bar opens the Town field. *weather Mumbai* still works.

## Music
- **Up next.** On the Media page, the queue button (or the next song's cover, peeking out from behind the one playing) opens **Up next**: the songs after this one.
  - Drag a row up or down to move it. The others glide aside.
  - Click a row to play it now.
  - Scroll with the wheel or the arrows.
- **Crossfade.** Songs the island plays blend into each other over 6 seconds. Change it in Settings › Media & sound › *Crossfade*; 0 turns it off. Skipping fades the old song out quickly instead of cutting it.
- **Handoff fades.** Music moving to your other PC fades out over about a second and a half before it pauses. For another app, like Spotify, the island lowers that app's volume, pauses it, and puts its volume back. On the other PC, the song rises in.
- **The cover comes along.** *Play here* on the other PC shows the song's cover, and a thin line shows how far into the song it is.

## Sharing
- **Take from your other PC's Shelf.** Shelf › Nearby › **Shelf** on a paired PC shows what's on that PC's Shelf, with previews and sizes. Click one to take a copy. It lands in Downloads and on this PC's Shelf, and the other PC tells you it was taken. Turn it off on a PC with Settings › Privacy & productivity › *My PCs can take from the Shelf*.
- **Big transfers.** A transfer of 5 GB or more shows as a ring in the compact island that fills as it goes and widens with its speed, with the speed beside it and the time left. Every transfer's row in Nearby shows its speed and time left too.
- **Compatibility.** This is revision 1 of sharing protocol 2, so 0.16 and 0.17 still share files and music with each other. The cover and taking from a Shelf need 0.17 on both PCs; the **Shelf** button shows only for PCs that have it.

## Alerts
- **Side by side.** When two alerts are out, point at them. The first slides left, and the waiting one comes up beside it as a card of the same height. Click it to bring it forward. Move away and they close up again.

## Screen readers
- **An island you can hear.** Narrator, NVDA and JAWS can now find the island as *Island*. It opens and closes like a menu, and says what it shows: *Open on Media. Playing Blue in Green by Miles Davis*. Everything on it that can be pressed has a name and a role:
  - buttons, like *Play*, *Up next*, *Take a copy of Trip itinerary.pdf*
  - switches that say whether they're on, like *Wi-Fi* and *Dark mode*
  - sliders with their values: volume, brightness, the song's position and each app's volume
- **Announcements.** Alerts are spoken as they arrive, with what you can do (*Music from another PC: Blue in Green. Play here, or not now*). The page that opens, the song that starts and volume changes are spoken too.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,613; share 165 (46 new).
- **Native UI test:** 45 stages, 3 new: the side-by-side alerts and their hit test, Up next and the other PC's Shelf, and spoken names.
- **Settings test:** end to end, passing.
- **Live checks:**
  - a crossfade between two of Windows' own sounds starts one second before the end, and the queue moves on once
  - a screen-reader client read the island, pressed its buttons and switches, and heard its alerts
  - the Town field found and saved all six towns above

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
