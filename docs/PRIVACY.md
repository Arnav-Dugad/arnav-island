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

## v0.12 capture, Shelf and clipboard

- **Captures happen only when you ask** (a shortcut, a Shelf button or a command). The frozen screen image stays in memory while the overlay is open and is discarded; only what you choose is kept: a snip as a PNG in Pictures › Screenshots and on the clipboard, recognised text on the clipboard, or a colour value.
- **Text recognition runs on this PC** with Windows' built-in engine. No image or text is sent anywhere.
- **Shelf actions** write new files (conversions, archives) only when you press the action, next to the original or in Documents. Originals are never changed or deleted; Remove only takes an item off the Shelf.
- **Pinned Shelf** (off by default) keeps file paths and dropped text in `shelf.nexus` on this PC. Turning it off deletes the file.
- **Pinned copies** are saved in `clips-pinned.nexus`, encrypted with Windows DPAPI for your account, so other accounts and copies of the file elsewhere can't read them. Images are never saved. Settings' Clear, or turning clipboard history off, deletes the file.
- **The clipboard picker** sends a Ctrl+V keystroke to the app you were in, only when you press Enter in it.
- **crash.txt**: if the island crashes, it records the error code and code offsets inside the program. No memory contents, file names, text or device data.
- Public screenshots use sample files in a public folder, illustrative clips and a painted backdrop for the overlay.

## v0.13 command bar

- **File results** come from the Windows Search index already on this PC, queried locally; nothing is sent anywhere. If the index is off, the island scans your user folder briefly, also locally.
- **Remembered commands** (on by default) are kept in `commands.nexus` in the island's folder: what you ran, the text you typed for it, when and how often, pins, and the paths of files you opened from the bar. Turning *Remember recent commands* off deletes the file.
- **Currency conversion** (off by default) downloads the European Central Bank's public daily rates file, at most twice a day. The request contains nothing about you or what you converted; like any web request, it reveals your IP address to the ECB. The file is cached as `rates.xml`.
- **System actions** (theme, radios, recycle bin, sleep, restart, shut down, lock) run only when you choose them, and the ones that are hard to undo ask for a second Enter first.
- Public screenshots use the showcase track, sample lyric lines and sample files in the Public folder; test runs never search your own files.

## v0.14 awareness

- **The screen-capture dot** reads Windows' own capability consent records on this PC (the same place Windows keeps camera and microphone use). Nothing is captured or sent; only the app's name is shown.
- **GPU use** is read from Windows' performance counters on this PC, once a second and only while it's shown.
- **The beat pulse** uses the same loopback analysis as the waveform: each audio buffer is folded into band levels and discarded, and nothing is recorded, stored or sent. It runs only while a playing cover is shown.
- **The idle glance** shows the date and your own CPU and GPU use; nothing is stored.

## v0.15 weather, sharing and the week

- **Weather** (off by default) goes online only after you choose a town (the `weather` command) or turn it on. The town you type is sent once to Open-Meteo's geocoding service to find it; afterwards only its coordinates, rounded to two decimals (about a kilometre), are sent to Open-Meteo's forecast service every 30 minutes while weather is on. Like any web request, this reveals your IP address to Open-Meteo. The place is kept in `weather.nexus`; turning weather off deletes it.
- **Site icons** (off by default) fetch `/favicon.ico` from the site a copied link points to, which tells that site your IP address and that its link was copied. Nothing else is sent, and icons are kept in memory only.
- **Sharing with your PCs** (off by default):
  - While on, the island announces this PC's name, a random device id and a port on your local network every 3 seconds (UDP broadcast). It listens for your other PCs on TCP port 47820.
  - Pairing exchanges public keys, and both people confirm the same six-digit code.
  - Files go only to and from paired PCs, only after the receiving person accepts, and they are encrypted end to end (AES-256-GCM under a key agreed by ECDH P-256, with fresh nonces from both PCs for each connection). A PC that isn't paired, or whose key doesn't match its pairing, is refused.
  - This PC's private key is kept in `share-identity.nexus`, encrypted with Windows DPAPI for your account. Paired PCs' public keys and names are kept in `share-peers.nexus`.
  - Received files are saved in Downloads. Nothing goes through any server.
- **Battery health** is read once a day from Windows' battery driver and kept in `battery-health.nexus` on this PC: the day, full-charge and design capacity, and cycle count. The weekly card is worked out from it and from the charge history already kept locally.
- **Adaptive text** reads your wallpaper file and the positions of open windows, on this PC only, to decide each letter's colour. Nothing is stored or sent. Public screenshots use an illustrative backdrop, never the real wallpaper.
- **Brightness and the Controls page** change Wi-Fi, Bluetooth, airplane mode, dark mode, brightness and the microphone only when you press them.
- **Fullscreen peek** watches the pointer at the screen edge only while a fullscreen app has hidden the island and something is playing.
- Public screenshots use illustrative PCs ("Studio PC", "Travel laptop"), a sample town and an illustrative backdrop; test runs never start sharing, weather or site icons.

## v0.16 clipboard, music and sharing

- **The clipboard history is kept across restarts** (on by default while clipboard history itself is on; clipboard history stays off until you turn it on). It is saved in `clips-history.nexus`, encrypted with Windows DPAPI for your account, and read back when the island starts:
  - text, links and file lists as they were copied, and images as PNG
  - which app they came from, and when
  - copies that look like passwords, one-time codes or keys are not written unless you pin them
  - copies from password managers and copies marked private are never read at all, as before
  - turning off *Remember the clipboard after restarts* or clipboard history, or clearing it, deletes the file
- **The music library** reads the songs in your Music folder (their tags and covers) on this PC when you open Media › Library, shuffle or type *play* and a song. The list and covers are kept in memory only and never sent. Test runs read only a folder they are given (Windows' own sounds), never the Music folder.
- **The island's player** registers with Windows' media controls for its window, so Windows shows the song's title, artist and cover in its own media controls, as it does for any player.
- **Continue on my other PC** (only with sharing on) sends, to a paired PC you choose, the song's title, artist, album, the app playing it and where it is, encrypted like shared files. When the island itself plays the song from a file and the other PC asks for it, that file is sent too (it's kept in `Handoff` in the island's folder on the other PC). Nothing is sent until you press **Continue on**; the other PC plays nothing until someone there presses **Play here**. To play the song there, the island may open the same app (Spotify by its link, a Store app by its id).
- **Sharing** (protocol 2) now also carries folders, keeping their tree under a new folder in Downloads; received paths are cleaned (no `..`, drive letters, reserved names or unsafe characters; at most 24 levels). Discovery announces the protocol number with the port.
- **Sounds** are generated in memory and played through Windows' default output; nothing is recorded.
- Public screenshots use Windows' own sounds as the library, illustrative PCs and a made-up transfer.

## v0.17 town, Shelf, covers and screen readers

- **The Town field** (Settings › Compact › Town) sends what you type there to Open-Meteo's geocoding service, a moment after you stop typing, to list matching towns. Typing a town turns weather on (it's off until then). The town you pick is kept in `weather.nexus` on this PC, as before; after that only its coordinates are sent, for the forecast. Test runs look towns up only when asked to, and keep their place in a file of their own in the temp folder.
- **A song's cover** now goes with *Continue on*: a small JPEG (192 pixels) of the cover already on screen, encrypted like everything shared. It goes only to the paired PC you chose, and is shown on its *Play here* card and not kept.
- **Your Shelf, seen from your other PCs** (on while sharing is on; turn it off with *My PCs can take from the Shelf*). A paired PC can ask for the list of files and folders on this Shelf: their names, sizes and a small preview (72 pixels). It can ask for a copy of one of them, which is sent like a file you send. Only what is on the Shelf at that moment, at the place and with the name that PC saw, can be taken. Text on the Shelf is never listed. This PC shows a card when something is taken. PCs that aren't paired get nothing.
- **Screen readers** read the island through Windows' UI Automation, on this PC: the names on its buttons (song titles, file names, your PCs' names) and its alerts, as they would for any app. Nothing is sent anywhere. Test runs log only that an announcement was made, never what it said.
- **Crossfades and fades** change only the island's own volume. For another app's music moving to your other PC, that app's volume in the Windows mixer goes down and is put back after it pauses.
- Public screenshots show Windows' own sounds, illustrative PCs and files, and a made-up transfer. They're drawn by the island itself, never taken from the screen.
