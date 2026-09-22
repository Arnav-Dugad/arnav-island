# Quick start

1. Extract the ZIP into a folder you intend to keep. Run **ArnavIsland.exe**.
2. Hover over the top-center island to open it. Click the dot to pin; × or Escape closes it. Right-click opens the tray menu.
3. Home shows media, CPU, memory, battery and volume. Media uses Windows media sessions; Stats provides real OS counters; Focus includes 25-minute focus, 5-minute break and stopwatch.
4. Drag a file or text onto the island to open Shelf. Drag a shelf row back out. Transfers are copy-only. Scroll to reach additional rows; Clear forgets references. Quit clears the shelf.
5. Audio lists active Windows outputs. Choose one to switch, or use Windows sound settings. Direct switching is optional compatibility behavior, configurable under Preferences → Activities.
6. The **⋯** view is Preferences. Click the group selector to cycle Behavior, Appearance, Motion, Activities and Advanced. Changes save locally.
7. Enable or disable **Start at sign-in** in Behavior. This registers the current executable path for your Windows account. If you move the app, toggle it off/on from the new location. The developer-installed copy already has this enabled.
8. Tab/Shift+Tab select controls after activation; Enter/Space activate; Escape collapses. Animation Lab is available from the right-click menu or Advanced preferences.

Settings: `%LOCALAPPDATA%\ArnavIsland\settings.nexus`. Logs: same folder, `events.log`. No user media text, file paths or clipboard content are logged. The legacy Nexus settings are migrated by copy.

Artwork appears only when Windows media metadata provides it. Use Media's Auto/Music/Video selector for ambiguous browser sessions. Play/previous/next are disabled when the player does not advertise the capability. The app does not suppress the normal Windows volume flyout.

Remove sign-in startup before deleting the portable folder. No service, driver or Explorer modification is installed.

## v0.4 preferences and controls

Navigation now uses labelled vector icons. Open **Settings → Personal layout** (cycle the group selector) to choose an item and move it left/right. The three Home statistic rows cycle among CPU, memory, battery, download, upload, disk free and uptime, skipping duplicates.

**Fine details** controls Animated icons, Track handoff and Glance rings. Reset layout restores navigation and Home statistics without changing other preferences. Drag the Home volume line to set a level; when keyboard focus is on it, Left/Right adjust by two points and Home/End select 0/100. The normal mouse wheel volume control remains available.

Artwork blends only when supplied by Windows media metadata. The ring arcs use real state updates; their markers animate through the compositor. Existing startup and other preferences are preserved when installing this update at the same path.

## Working alongside other apps

Preferences → Multitasking controls collapse on app switching and optional scroll-anywhere volume. Unpinned panels collapse without resetting your selected page or timer; pinned panels and active drags remain open. By default, scroll over the volume slider to adjust volume. Shelf and output lists scroll normally.
