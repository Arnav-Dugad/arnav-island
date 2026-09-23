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
