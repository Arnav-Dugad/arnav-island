# Quick start

1. Extract the ZIP into a folder you intend to keep. Run **ArnavIsland.exe**.
2. Hover over the top-center island to open it. Blank-space clicks do not expand or pin it; × or Escape closes it. Right-click opens the tray menu.
3. Home shows media, CPU, memory, battery and volume. Media uses Windows media sessions; Stats provides real OS counters; Focus includes 25-minute focus, 5-minute break and stopwatch.
4. Drag a file or text onto the island to open Shelf. Drag a shelf row back out. Transfers are copy-only. Scroll to reach additional rows; Clear forgets references. Quit clears the shelf.
5. Audio lists active Windows outputs. Choose one to switch, or use Windows sound settings. Direct switching is optional compatibility behavior, configurable under Settings → Media & sound.
6. **Settings** opens in its own window (gear icon, the Settings navigation item, the tray menu, or double-click the tray icon). Every change applies to the island immediately and saves automatically.
7. Enable or disable **Start at sign-in** in Settings → General. This registers the current executable path for your Windows account. If you move the app, toggle it off/on from the new location. The developer-installed copy already has this enabled.
8. Tab/Shift+Tab select controls after activation; Enter/Space activate; Escape collapses. Animation Lab is available from the right-click menu or Advanced preferences.

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
