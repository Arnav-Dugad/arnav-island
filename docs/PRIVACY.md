# Privacy

Core operation is local. No account, API key, analytics, backend, network client,
automatic upload or paid service is implemented. Windows media metadata stays in
memory. Audio monitoring reads volume/mute, not microphone samples. There is no
loopback audio recording, screen recording, clipboard history, notification
observation, file content collection or sensor driver in normal operation.

Local data lives under `%LOCALAPPDATA%/ArnavIsland`:

- `settings.nexus`: versioned preferences; temporary file then atomic replacement.
- `events.log` and `events.previous.log`: bounded structured event diagnostics.
  No media titles, notification bodies, clipboard text, file contents or device
  identifiers are logged. Levels currently emitted: Info, Warning, Error.
  Debug and Trace output are not yet implemented.
- `benchmark.json`: only created by the explicit developer benchmark.
- QA PNGs: only created with `--capture`, never automatically in ordinary use.

Open/clear logs from the tray menu. Settings reset is explicit. QA screenshots
must be visually reviewed before sharing. Initial development captures contained
adjacent desktop content and are excluded from the repository; subsequent
capture mode uses an app-owned neutral matte. GitHub publishing includes source,
documentation and reviewed project artifacts, not the user's local runtime data.

Future clipboard, notifications, calendar and plugins require separate opt-in
designs. No global camera/microphone indicator is shown without trustworthy OS
evidence. No third-party native plugin execution is enabled.

v0.2: artwork is decoded in memory, never logged or stored in normal use. System monitoring reads aggregate OS counters, not network packet contents. Version 1 settings are copied from the old NexusIsland directory only if new settings do not exist. Legacy files are retained. `--capture-safe` disables the media provider for that QA run; release screenshots use it. `--ui-test` moves and restores the cursor and runs only when explicitly requested. No normal startup input injection occurs.

## v0.3 additions
The file shelf keeps only file-path/text references in memory. It never copies file contents to a cache, uploads data or deletes originals. Clear and quit discard references. Text dragged into the shelf is explicitly supplied by the user; clipboard history is not monitored.

Audio switching is local and opt-out. Startup writes only this application's HKCU Run value. Settings and local log files remain excluded from source and release archives. Release screenshots use `--capture-safe`, which disables real media sessions. The artwork study screenshot is explicitly labelled synthetic local QA artwork; it is not an active music service.

## v0.4 local data

Personal layout adds only navigation IDs, metric IDs and visual preferences to local settings. Artwork handoff buffers are in memory and discarded with the renderer. No media images, titles, new analytics or network requests are persisted by these features. Synthetic motion-study images are explicit developer diagnostics, excluded from ordinary operation.

Foreground integration observes only transient window handles and visible bounds to apply local fullscreen and dismissal policies. No foreground application history, executable paths, titles or content is saved or transmitted.

Shelf thumbnail extraction reads only user-dropped file references, locally through Windows Shell. Previews live in memory and are not logged or uploaded. Windows may maintain its own thumbnail cache. Offline/recall placeholders are skipped. Media seeking is sent only to the current matching Windows session. Developer visibility audit records booleans (hidden, decorated, maximized, shell), not titles, URLs or application history.
## v0.6 display preferences

`displays.nexus` stores monitor device identities and geometry locally. It is not uploaded or included in release archives. File peek and artwork atmosphere reuse existing in-memory visual data. The phase plan does not enable clipboard monitoring or audio capture; both remain future opt-in providers.

## v0.7 audio, media and settings

- **Live waveform:** WASAPI loopback reads the output mix on a worker thread only while bars are visible and a session is playing. Each buffer is reduced to 24 band levels in memory and discarded. Nothing is recorded, written to disk, logged or transmitted. Loopback does not access microphones.
- **Mixer:** reads application audio sessions, process IDs, executable paths (to show the app's own name and icon) and peak levels. None of this is stored or logged. Volume/mute changes are applied only when you move a control.
- **Media identity:** app names/icons come from Windows for the session's app ID. To confirm a YouTube tab, the island reads visible browser window titles locally and compares them with the playing title. Titles are not stored, logged or sent anywhere; only a YouTube/YouTube Music/none flag is kept in memory.
- **Brightness:** reads the current panel brightness from Windows' WMI monitor classes. It never changes brightness.
- **Settings window:** preferences are written to `settings.nexus` only. QA runs of `--settings-test` use a separate `settings-qa.nexus` and never touch sign-in startup.
- Release screenshots are produced with synthetic sessions (`--qa-showcase`) built from stock Windows app icons, over an app-owned matte or colour pattern; no personal desktop content is published.

## v0.8 devices, battery and logos

- **Edge reveal** reads only the pointer position (GetCursorPos) about 30 times a second while enabled. No hooks, no input recording.
- **Web service logos:** visible browser window titles are compared locally with the playing title. Only the matched service name (for example `youtube`) is kept in memory. Titles are never stored, logged or sent.
- **Bluetooth:** device names, connection state, battery and class of device are read from Windows' device properties to draw cards and the Devices tab. They are kept in memory only. Logs record that a device card was shown, never the device name or address. Connect/Disconnect is sent only when you press the button.
- **Battery:** readings come from the battery driver. `battery-history.nexus` stores time, percentage and charging state every 5 minutes for 7 days. It never leaves the device. Turning off *Keep charge history* deletes it.
- **Platform:** manufacturer and model come from the firmware values Windows keeps in the registry; Armoury Crate is detected from the Start menu app list. Nothing is written.
- Public screenshots of device cards use `--qa-sample`, which replaces real paired devices with illustrative ones.

## v0.9 clipboard, privacy indicators, commands and workspaces

- **Clipboard history** is off until you turn it on. When on, the island reads the clipboard only when it changes and keeps up to 24 copies in memory. It never writes them to disk, never logs their content and never sends them anywhere. Copies marked private by their app (the `ExcludeClipboardContentFromMonitorProcessing` and `CanIncludeInClipboardHistory` flags Windows' own history respects) and copies from password managers are skipped. Turning history off, clearing it, or quitting forgets everything.
- **Privacy indicators** read Windows' capability access records under `HKCU\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore` for the camera, microphone and location. They check which apps are running to confirm current use. Nothing is written, and logs record only that indicators changed, never which app.
- **Browser tabs:** to tell media sites apart, the island reads tab titles from browser windows through UI Automation (the tab strip only, never page content). Titles stay in memory and are compared with the playing title.
- **Command bar:** what you type stays in memory and is parsed locally. File searches open File Explorer's own search in your user folder. Nothing is sent anywhere.
- **Workspaces** store app names and their Start menu IDs or program paths in `workspaces.nexus`, on this PC only. Settings → Privacy & productivity removes them.
- Public screenshots of these features use illustrative content (`--qa-clipboard`, `--qa-privacy`, `--qa-command`).

## v0.10 waveform, lab and wallpaper

- **Waveform timeline:** when it is on and a session plays, the loopback analyzer's overall level (while the analyzer runs for the compact bars or the Media page) is averaged into 64 buckets per track. Only these 64 numbers are kept, for at most 32 tracks, in memory; they are keyed by title, artist and length and forgotten when the island quits. No audio is recorded and nothing is written to disk or logged.
- **Wallpaper accent:** the wallpaper file named by Windows is decoded locally at 48 × 48 to compute one colour. The picture and its path are not stored, logged or sent.
- **Animation Lab:** reads the display refresh rate, Windows' compositor frame counter and the app's own memory use. Slow motion is never saved.
- Public screenshots use the synthetic showcase session over an app-owned matte or colour pattern (`--qa-showcase`, `--qa-pattern`).

## v0.11 lyrics, palette and audio

- **Synced lyrics are the island's first feature that goes online, and they are off until you turn them on.** When on, the island sends the playing song's title and artist (cleaned of words like "Official Video") to `lrclib.net` over HTTPS, and nothing else: no account, key, cookie, album, length or identifier. The request carries an `ArnavIsland/<version>` user agent. Answers are chosen on this PC by comparing song lengths. They are kept in `%LOCALAPPDATA%\ArnavIsland\lyrics`, one small file per song named by a hash, at most 400, and Settings → Media & sound → Saved lyrics → Clear deletes them. Lookups are not logged.
- **Artwork palette:** computed in memory from the cover Windows already provides; nothing is stored.
- **Per-app volume and microphone mute** use the same Windows audio APIs as the mixer. The island changes the microphone's mute only when you ask it to, and only reads it otherwise.
- **Headphone card:** reads output names and form factors from Windows; nothing is stored.
- Public screenshots use the synthetic showcase session with placeholder lyric lines written for testing, and illustrative devices (`--qa-lyrics`, `--qa-headphones --qa-sample`).
