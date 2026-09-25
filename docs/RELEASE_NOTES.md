# Arnav Island 0.15.0-preview.1 — A pill that drops, a sky that moves, your PCs within reach

Phase 5F:
- Notifications drop out of the island as their own glass pill.
- A weather glance plays the sky.
- Files go between your own PCs.
- Control the music from the compact island, and control Wi-Fi, Bluetooth and brightness from a new Controls page.
- Lyrics light up word by word, numbers blur on big jumps, icons morph, and the island leans when you drag it.

## Notifications
- **A pill that drops.** A card no longer grows the island. It drops out below as its own glass pill, settles with a small bounce, and rises back in when it's done. The island stays docked above it, at compact size, and the space between them is click-through. Choose *Grow the island* under Settings › Appearance › Alerts to get the old style back. Drop needs the top dock; on side docks and a floating island, cards still grow the island.
- **Light runs along the edge.** When an alert arrives, a glint sweeps out along the island's edge from the middle and fades (Settings › Appearance › *Light along the edge*).
- **Which app is using the camera.** The camera, microphone, screen-capture and location card now has a **Settings** button that opens that permission's page in Windows Settings. Clicking the coloured dots in the compact island brings the card back.

## Now Playing
- **Control the music from the compact island.** Previous, play/pause and next sit at the end of the compact island (Settings › Compact).
- **Swipe to skip.** Swipe sideways on a touchpad over the compact island, or drag it sideways and let go, to skip a track. The title kicks in the direction you swiped.
- **Over fullscreen apps.** While the island is hidden for a fullscreen game or video, touch the top edge of the screen above it to bring back the compact island, with its controls and swipe. It hides again 0.8 s after you move away.
- **A spectrum ring.** The compact artwork turns round and a ring of 24 ticks pulses around it with the music. You can go back to bars in Settings › Compact › Live audio style.
- **Lyrics everywhere, word by word.** The compact island and the Live Island use the Command Center's lyric line: it lights up as it's sung and morphs to the next line. When the lyrics carry word timing, each word lights as it's sung.
- **Accent colours from the app.** With no artwork, the island takes its accent from the playing app's icon (Settings › Appearance › App colours).

## Weather
- **A weather glance.** Type *weather* and a town in the command bar (for example, *weather Paris*). The temperature then shows on Home and in the compact island's idle glance.
- **A sky that moves.** On Home the weather tile plays the conditions for a minute each time it appears, then comes to rest: sun rays turning, stars twinkling, clouds drifting, rain streaks, snow, fog and storm flashes.
- **Off until you ask.** Weather is off until you type a town or turn it on in Settings. It then sends the town once (to find it), and afterwards only its rounded coordinates every 30 minutes, to Open-Meteo.

## Controls page
- **A new page.** Tiles for Wi-Fi, Bluetooth, airplane mode, dark mode, the island's focus timer and your microphone, plus volume and brightness sliders. Tiles show their real state and light up with an animated icon when switched on.

## Sharing between your own PCs
- **Send a Shelf file to your other PC.** Turn on *Share with my PCs* (Settings › Privacy & productivity) on both PCs, on the same network.
  - Open Shelf › **Nearby** and press **Pair** next to the other PC.
  - Both PCs show the same six-digit code. Check that it matches and press **Pair** on both.
  - Then open a file on the Shelf and press the **Send** button. The other PC asks you to **Accept**, and the file lands in Downloads.
- **Private by design.**
  - Files go only between paired PCs, and only after the receiving PC accepts.
  - Files and their names are encrypted end to end (AES-256-GCM, with keys agreed by ECDH P-256). Only the PC's name and that sharing is on are announced on the network.
  - **Forget** unpairs a PC.
- **Firewall.** The first time you turn it on, Windows may ask to let the island through the firewall. Allow it on private networks only.

## Battery
- **Health trends.** The island records your battery's full-charge capacity once a day, on this PC only.
- **A weekly summary card.** Once a week, in the morning, a card shows:
  - your battery's health, and how much it changed since last week
  - how many times you charged
  - how much of the battery a typical day uses

## Clipboard
- **Rich previews.**
  - A link shows its site and path (and the site's icon, if you turn on *Site icons*).
  - A colour code shows a swatch.
  - Code shows in a monospaced font with syntax colouring.

## Compact island
- **Arrange the chips.** Drag the chips (clock, volume, battery, timer, CPU, GPU, weather) into your own order in Settings › Compact. The arrow keys work too.
- **Adaptive text on Clear glass.** On Clear glass, each letter turns dark over a bright part of your wallpaper and light over a dark one. This works only when no window is behind the island. When one is, the island keeps its usual colours.

## Motion and icons
- **Numbers stretch and blur on big jumps.** A digit that rolls a long way smears along its roll and stretches a little, then lands sharp.
- **Icons morph.** Play turns into pause, and the speaker's waves turn into a cross, instead of swapping. Navigation and control icons play a short animation when chosen or switched on.
- **The island leans.** Drag it against the top of the screen and it leans the way you pull and stretches a little, then springs back. The glass leans with it.
- **A shadow that rises.** As the island grows, its shadow drops further and deepens, as if it were lifting off the desktop.

## Settings (version 14)
- **New and on by default:**
  - compact media controls
  - swipe to skip
  - Now Playing over fullscreen apps
  - accent from the app's icon
  - the weekly battery card
  - rich clipboard rows
  - light along the edge
  - adaptive text
  - the drop pill
  - the spectrum ring
- **New and off by default:**
  - weather
  - site icons
  - sharing with your PCs
- **Controls** joins the page order after Stats.
- The chip order is saved.

## Limits
- **Not on the lock screen.** Windows draws the lock screen on a secure desktop that apps can't draw on, so Now Playing there stays Windows' own.
- **Word-by-word lyrics** need lyrics that carry word timing. Most lyrics on LRCLIB are timed by line, and those fill smoothly across the line instead.
- **Sharing** was tested end to end between two PCs simulated on this machine (over loopback), not yet between two physical PCs. Discovery uses UDP broadcast, so both PCs must be on the same network segment, and some guest or public Wi-Fi networks block it.
- **Adaptive text** reads your wallpaper, not what's actually behind the island, so it switches off whenever a window is behind the island. With a span wallpaper across several monitors, it treats the picture as *Fill*.
- **Drop pill** is for the top dock only.
- **Real blur** still needs Windows' Transparency effects.
