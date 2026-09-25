# Quick start

1. Extract the ZIP into a folder you intend to keep. Run **ArnavIsland.exe**.
2. Hover over the top-center island to open it. Blank-space clicks do not expand or pin it; × or Escape closes it. Right-click opens the tray menu.
3. Home shows media, CPU, memory, battery and volume. Media uses Windows media sessions; Stats provides real OS counters; Focus includes 25-minute focus, 5-minute break and stopwatch.
4. Drag a file or text onto the island to open Shelf. Drag a shelf row back out. Transfers are copy-only. Scroll to reach additional rows; Clear forgets references. Quit clears the shelf.
5. Audio lists active Windows outputs. Choose one to switch, or use Windows sound settings. Direct switching is optional compatibility behavior, configurable under Settings → Media & sound.
6. **Settings** opens in its own window (gear icon, the Settings navigation item, the tray menu, or double-click the tray icon). Every change applies to the island immediately and saves automatically.
7. Enable or disable **Start at sign-in** in Settings → General. This registers the current executable path for your Windows account. If you move the app, toggle it off/on from the new location. The developer-installed copy already has this enabled.
8. Tab/Shift+Tab select controls after activation; Enter/Space activate; Escape collapses. The Animation Lab is in Settings → Motion (the tray menu's Animation Lab item opens it).

Settings: `%LOCALAPPDATA%\ArnavIsland\settings.nexus`. Logs: same folder, `events.log`. No user media text, file paths or clipboard content are logged. The legacy Nexus settings are migrated by copy.

Artwork appears only when Windows media metadata provides it. Use Media's Auto/Music/Video selector for ambiguous browser sessions. Play/previous/next are disabled when the player does not advertise the capability. The app does not suppress the normal Windows volume flyout.

Remove sign-in startup before deleting the portable folder. No service, driver or Explorer modification is installed.

## v0.4 preferences and controls

Navigation now uses labelled vector icons. Open **Settings → Personal layout** (cycle the group selector) to choose an item and move it left/right. The three Home statistic rows cycle among CPU, memory, battery, download, upload, disk free and uptime, skipping duplicates.

**Fine details** controls Animated icons, Track handoff and Glance rings. Reset layout restores navigation and Home statistics without changing other preferences. Drag the Home volume line to set a level; when keyboard focus is on it, Left/Right adjust by two points and Home/End select 0/100. The normal mouse wheel volume control remains available.

Artwork blends only when supplied by Windows media metadata. The ring arcs use real state updates; their markers animate through the compositor. Existing startup and other preferences are preserved when installing this update at the same path.

## Working alongside other apps

Preferences → Multitasking controls collapse on app switching and optional scroll-anywhere volume. Panels collapse without resetting your selected page or timer; active drags are protected. By default, scroll over the volume slider to adjust volume. Shelf and output lists scroll normally.

## v0.5 interactions

The seek bar expands on hover or drag. Pull vertically away from it to reduce seeking sensitivity (35 DIPs: 35%; 70 DIPs: 12%). Release to request the position; Escape cancels. Seeking appears only when the current Windows media session advertises support and valid seek bounds.

Shelf previews arrive asynchronously. Windows provides image/document thumbnails when supported, otherwise a file icon. Dragging out carries the cached preview and copies references safely; clearing the shelf does not delete originals. Cloud placeholders are not hydrated for thumbnails.

Opening a normal maximized browser keeps the island visible. Hide in fullscreen applies to borderless monitor-covering windows. Turning that setting off keeps the island visible for fullscreen too.

## v0.6 everyday modes

In **Settings → Island modes**, select **Mini Pill**, **Live Island** or **Command Center**. Mini rests at 72 × 34 DIPs. Live rests at your selected compact width and opens a 360 × 154 DIP hover card. Command Center opens the full 420 × 334 DIP workspace on hover. Hover over the card's **Command Center** control for 420 ms to open the workspace; the gear opens Settings. Leaving closes the island after your chosen delay. Turning Open on hover off leaves the tray menu as the explicit access route.

Increase compact width up to **560 DIPs** (more than twice the previous limit); use the narrower control to step down without wrapping. Compact details can show media, volume, battery, timer and clock. Items are fitted to available width; Mini intentionally shows less. Glance rings share the same battery/timer visibility preferences. Hide all identifying branding on the island is now the default and only rendering behavior.

Hover a cached Shelf thumbnail for a larger preview. It fades and scales gently, and does not alter the file. Document previews depend on installed Windows Shell handlers. Album accent enables a restrained radial atmosphere whose color channels blend continuously between artwork updates. Reduce motion makes these changes immediate.

Display selection, horizontal/vertical offset, width, scale and dock edge are remembered by display identity in `%LOCALAPPDATA%\ArnavIsland\displays.nexus`. Disconnected preferred displays fall back to primary; reconnect restores the preferred display. The active monitor's placement is saved with preference changes. Hardware docking across mixed-DPI monitors still needs acceptance testing.

Larger requested integrations are tracked honestly in DELIVERY_PHASES.md in the source repository; they are not represented by simulated controls in this release.

## v0.7 glass, sound and Settings window

**Settings window.** Sections: General, Island, Appearance, Motion, Compact, Media & sound, Home & navigation and About. Drag sliders to watch the island move and resize live. Tab moves focus, arrows adjust sliders and selectors, Space/Enter toggles. Reset and Clear logs need a second click within four seconds.

**Glass.** Settings → Appearance → Material: Solid, Frosted glass or Clear glass. Glass floats 8 px from the edge; Edge offset overrides the gap. Glass tint trades transparency for contrast. For real blur, turn on Windows **Transparency effects** (the Appearance page has a button that opens it). With it off, glass is tinted but not blurred.

**Several players.** When more than one app is playing, the Live card shows page dots under the artwork and the Media page shows app chips beside the title. Swipe horizontally (drag on empty space, or two-finger swipe on a touchpad) or tap a dot/chip. Media & sound → Follow the active player keeps the island on the session Windows marks as current.

**App logos.** The small badge on the artwork is the playing app's own Windows icon. A browser tab is labelled YouTube or YouTube Music only when the browser window's title contains the playing title and the service name. Turn badges off in Media & sound → App logos.

**Live waveform.** Compact → Live waveform shows bars that follow the real output audio. They rest when nothing is audible. Protected (DRM) playback may analyze as silence.

**Mixer.** Open Audio → Apps. Drag a slider to set an app's volume, tap the speaker to mute; the thin line under each slider is its live level. Outputs lists output devices as before. Windows volume mixer opens the system page.

**Volume and brightness indicator.** Changing volume or laptop brightness grows the resting island into a level bar for about two seconds. Turn it off in Compact → Volume and brightness indicator. The Windows volume flyout is not suppressed.

## v0.8 edge reveal, devices and battery

**Edge reveal.** The island starts tucked away. Push the pointer to the top edge of the screen above where the island sits (or the right edge if you docked it there) and it slides in; hover to open it. Move away and it tucks back after your collapse delay. Turn it off in Settings → General → *Hide until the pointer reaches the edge*. *Show alerts while hidden* lets device, charging and volume cards appear without reaching for it.

**Device cards.** Connect earbuds, headphones, a controller or a keyboard and the island shows a card with the maker's logo and battery. Open **Stats → Devices** to see every paired device; audio devices have Connect/Disconnect. Scroll the list with the wheel.

**Battery.** **Stats → Battery** shows health, full-charge energy, cycles (when the battery reports them), a 24-hour graph and time estimates. Plugging in shows a charging card. Settings → Devices & power controls the cards and the charge history.

**ROG laptops.** With Armoury Crate installed, Stats → System and Settings → Devices & power have an Armoury Crate button.

## v0.9 command bar, clipboard, privacy and workspaces

**Command bar.** Press **Alt+Shift+Space** (or the search button on Home). Try "volume 30", "focus 25", "open edge", "find notes pdfs from this week", "bluetooth settings" or "lock". The first row shows what Enter will do; ↑ and ↓ choose another; Esc closes. The shortcut can be changed in Settings → Privacy & productivity.

**Workspaces.** Open the apps you use together and type "save workspace study". Later, type "study" and press Enter twice to open the ones that aren't running.

**Clipboard history.** Shelf → Clipboard → Turn on. Your last 24 copies appear there; click one to copy it again. Pause and Clear are at the bottom. Nothing is kept on disk.

**Privacy dots.** A green dot means the camera is in use, orange the microphone, blue location. Open the island to see which app.

## v0.10 glass, lab and waveform

**Glass.** Settings → Appearance → Material. *Frosted glass* blurs what is behind the island when Windows **Transparency effects** are on, and becomes a soft frost when they are off. *Clear glass* never blurs; the desktop shows through a light tint. Move the pointer over the island to see the light follow it.

**Animation Lab.** Settings → Motion. Pick a character, or drag Stiffness, Damping or Weight to make a Custom spring; the preview replays with each change (click it to replay). *Slow motion* slows the real island to ½× or ¼× until you set it back or restart. *Try it on the island* plays Expand, Collapse, Interrupt or a Card; the line under the preview then shows the frames Windows composed while it moved.

**Waveform timeline.** Play something and open the Media page. The timeline fills in with the song's loudness as you listen. Drag it to seek as before. Settings → Media & sound → *Waveform timeline* switches back to a plain line.

**Wallpaper accent.** Settings → Appearance → Accent, last swatch.

## v0.11 Now Playing Pro

**Synced lyrics.** Settings → Media & sound → *Synced lyrics*. Play a song: the line being sung shows in the compact island and on the Live card, and the Media page shows lyrics in place of the title. The lyrics button beside the page title switches back to the title. *Saved lyrics → Clear* removes what is kept on your PC.

**Seeking.** Hover the timeline to see the time (and lyric) under the pointer. Drag to seek: the playhead clicks into even time marks and lyric lines; pull away from the bar for fine control. Double-click the left or right half of the artwork to skip 10 s back or forward; keep clicking to go further. With the pointer on the timeline, ← and → skip 10 s.

**App volume.** With something playing, scroll over the app logo or artwork at the left of the compact island.

**Microphone.** Audio page → the microphone button (red when muted), or type "mute mic", "unmute mic" or "mic" in the command bar.

**Headphones.** When Windows switches to headphones, a card offers *Switch back*. Settings → Devices & power → *Headphone switch card* turns it off.

## v0.12 capture and Shelf

**Snip** (Alt+Shift+S), **copy text** (Alt+Shift+T) and **pick a colour** (Alt+Shift+C). Drag a region, or click a window or screen; Esc or right-click cancels. In colour mode, click to copy HEX, hold Shift for RGB, and use the arrow keys for one-pixel moves. The same tools are the three buttons at the bottom of the Shelf, and the command-bar words "snip", "copy text" and "pick colour". Snips are saved in Pictures › Screenshots.

**Shelf actions.** Click a Shelf item for Open, Open with, In folder, Copy path, and for images Copy text, To PNG/To JPG and Half size. Zip turns a file or folder into an archive beside it; the Zip button in the Shelf footer zips everything on the Shelf. Settings → Privacy & productivity → *Keep the Shelf after restarts* remembers the Shelf.

**Clipboard.** Alt+Shift+V opens your copies in the command bar: type to filter, Enter pastes into the app you were in, Shift+Enter only copies. On the Shelf's Clipboard tab, the pin keeps a copy at the top (and across restarts) and the search button finds older ones. Copies that look like passwords or codes stay dotted until you point at them. The shortcuts can be turned off in Settings → Privacy & productivity.

## v0.13 command bar, glass and lyrics

**Find files.** Open the command bar (Alt+Shift+Space) and type part of a file's name; matches from the Windows Search index appear under the apps. Add a type, a date and a folder in plain words: "budget pdfs from last week in downloads", "screenshots in pictures", "notes from yesterday". **Enter** opens, **Ctrl+Enter** shows the file in its folder, **Ctrl+C** copies its path. "find ..." lists only files, with a row to see everything in File Explorer.

**Switch things.** "dark mode", "light mode", "bluetooth", "wifi off", "airplane mode", "empty recycle bin", "sleep", "restart", "shut down", "lock". Each row says what it will do right now; the last five ask for a second Enter.

**The empty bar** shows pinned commands, a few suggestions and your recent commands. **Ctrl+P** pins or unpins the selected row (up to six). Settings → Privacy & productivity → *Remember recent commands* turns this off and forgets it.

**Complete with Tab.** A faint completion follows what you type; Tab accepts it. Matched letters are highlighted in every row.

**Currency** (Settings → Privacy & productivity → *Currency conversion*): "100 usd to inr", "$50 in €", "20 pounds" (into your own currency). Enter copies the amount.

**Glass.** Frosted and Clear now meet the screen with shoulders, like Solid. For real blur, turn on Windows Settings → Personalization → Colors → *Transparency effects*.

**Lyrics.** On the Media page with lyrics on, the sung line fills as it is sung; tap any line to jump there.

## v0.14 glass, awareness and motion

**Try the new glass.** Settings › Appearance › Material: *Frosted glass* or *Clear glass*. Frosted needs Windows' Transparency effects (Settings › Personalization › Colors); the island's Appearance page has a button that opens them. *Soft shadow* on the same page turns the shadow under the island on or off.

**Screen capture dot.** When an app captures your screen through Windows (sharing a screen in a browser, the Snipping Tool's recorder), a purple dot joins the privacy dots and a card names the app.

**GPU.** The Stats page shows GPU use with a history line. On Home, choose GPU for any of the three statistics (Settings › Home & navigation).

**Idle glance.** With nothing playing and no timer running, the compact island shows the date, and CPU and GPU where there is room. Settings › Compact › *Glance when idle* turns it off.

**Dock on the left.** Settings › Island › Dock edge › Left.

**Command bar.** Space between words works as you'd expect. Type a colour code (#3A7BD5, #39f, rgb(58, 123, 213)) to see it; Enter copies the hex. A misspelt app or command offers the closest match.

**The artwork pulses to the beat** while music plays (Settings › Media & sound › *Artwork pulses to the beat*).

## v0.15 Now Playing, weather, sharing and the Controls page

**Music from the compact island.** Previous, play/pause and next sit at the end of the compact island while something plays. Swipe sideways with two fingers on a touchpad (or drag the island sideways and let go) to skip. Over a fullscreen game or video, touch the top edge of the screen to bring the island back for a moment.

**Weather.** Open the command bar and type *weather* and your town (*weather Lisbon*), then Enter. The temperature shows in the idle glance, and on Home when you choose Weather as a statistic (Settings › Home & navigation). The Home tile plays the sky for a minute each time it appears.

**Controls page.** Open it from the navigation bar. Wi-Fi, Bluetooth, airplane mode, dark mode, the focus timer and the microphone are tiles; volume and brightness are sliders.

**Share a file with your other PC.**
1. On both PCs: Settings › Privacy & productivity › *Share with my PCs*. Allow the island through the firewall on private networks if Windows asks.
2. On one PC: Shelf › Nearby › **Pair** next to the other PC. Both PCs show a six-digit code. If it's the same, press **Pair** on both.
3. Drop a file on the Shelf, open it, and press **Send** (the paper plane). The other PC shows the file; press **Accept** and it's saved in Downloads. **Show** opens it in its folder.
Click a paired PC in Nearby to make it where Send goes. **Forget** unpairs it.

**Alerts.** Cards drop out of the island as a pill (Settings › Appearance › Alerts: *Grow the island* brings back the old style). A glint runs along the edge when one arrives (*Light along the edge*). On a privacy card, **Settings** opens that permission's page in Windows.

**Arrange the compact island.** Settings › Compact › *Arrange the compact island*: drag the chips, or select one and use the arrow keys. *Live audio style* switches between the spectrum ring and bars.

**Adaptive text.** With Settings › Appearance › Material › *Clear glass*, letters turn dark over bright parts of your wallpaper and light over dark ones (Settings › Appearance › *Text that adapts to the wallpaper*; the wallpaper is read on this PC only).

**Battery week.** Once a week, in the morning, a card summarises your battery's health, charges and daily use (Settings › Devices & power › *Weekly battery summary*).

**Clipboard.** Links show their site and path, colour codes a swatch and code is highlighted. *Site icons for links* (off by default) fetches each link's icon from its site.

## v0.16 drop to share, music from the island, two alerts at once

**Send by dropping.** With a PC paired (Shelf › Nearby), drag files or folders over the island: the Shelf is on the left, your PCs on the right. Let go over a PC to send them. **Send Shelf** on a PC's row sends every file on the Shelf; or drag the Shelf's item count (the little stack) onto a PC. A row fills as a transfer goes; **Stop** ends it. Both PCs need 0.16.

**Play your music.** Media › the list button (left of *Auto*) opens **Library**: your Music folder's songs. Click one, or **Shuffle all**. With nothing playing, the Media page offers **Shuffle my music**. In the command bar: *play* and a song, or *shuffle*. The keyboard's media keys work too.

**Continue on your other PC.** With sharing on, press the button right of *Auto* on the Media page (or type *continue on*). Pick the PC; there, press **Play here**.

**Skip by dragging.** Drag the music sideways, in the compact island, the Live Island, Home or the Media page, and let go when the chip snaps.

**Two alerts.** A second alert waits below the first as a small pill. Click it to see it now.

**Settings.** Settings › Appearance: *Two alerts at once*, *Sounds*. Settings › Media & sound: *Island DJ*, *Music library*, *Continue on my other PC*. Settings › Privacy & productivity: *Remember the clipboard after restarts*.

**Weather.** If you set a town on 0.15, type *weather* and the town once more.

