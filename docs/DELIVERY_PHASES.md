# Researched delivery phases — 2026-09-23

This is the implementation plan for the latest request, not a list of shipped features. Phase 1 is the current release boundary. Later phases require separate implementation and acceptance; no background work is scheduled automatically.

## Phase 1 — everyday scale and continuity (v0.6)

Mini Pill (72 × 34 DIP), Live Island (configurable 160–560 × 34 DIP at rest; 360 × 154 DIP hover card), and Command Center (420 × 334 DIP on hover). Clicking unused island space must never expand or pin it. Hovering Command Center inside the small card opens the larger workspace; settings and shelf controls remain directly accessible. No product name is drawn on the island. Ten settings groups expose compact details and width adjustment.

Shelf hover preview uses the existing bounded background thumbnail cache. Artwork atmosphere uses three retained radial surfaces with critically damped channel opacities: retargeting keeps current position and velocity, never a per-frame CPU color redraw. Local display profiles bind offsets, width, scale and dock edge to Windows display device paths, not monitor enumeration order. Profiles stay local.

Verification: settings migration, profile roundtrip and malformed data, Mini/Live geometry, no body-click expansion, supported refresh cadence math, native captures, and process counters. Physical mixed-DPI docking and actual 120/144/165/240 Hz presentation require hardware acceptance.

## Phase 2 — audio and media (delivered in v0.7)

- Multi-session carousel: enumerate [GSMTC GetSessions](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager.getsessions?view=winrt-26100), keep selection by session identity and support horizontal pointer gestures/keyboard navigation. OS media sessions are distinct from audio streams; not every audible application exposes metadata or controls.
- Per-app mixer: [IAudioSessionManager2](https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nn-audiopolicy-iaudiosessionmanager2), session-created/state events and ISimpleAudioVolume. Group only when process identity is trustworthy; handle session exit and device changes.
- Real equalizer: event-driven [WASAPI loopback](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording) on a worker, bounded spectral analysis, compositor interpolation. Opt-in; no recording or transmission. Suspend when hidden/inactive; protected or exclusive audio may be unavailable. Never replace missing samples with a fake looping waveform.
- Media logos: use application-provided icons where identity is reliable; review official branding/redistribution terms before bundling each service's real logo. A browser session alone does not establish Spotify/YouTube/Netflix identity. Keep a neutral browser icon for ambiguity. Metadata/thumbnail/control coverage requires real Spotify, Apple Music, YouTube, VLC and browser testing.
- Volume HUD already complements Windows. Refine the compact level line; do not suppress system UI using hooks or patches.

## Phase 3 — devices, brightness, power (delivered in v0.8)

- Bluetooth device events and reconnect: supported Windows device enumeration and [Bluetooth APIs](https://learn.microsoft.com/en-us/windows/win32/bluetooth/about-bluetooth). Battery via supported device properties or advertised [GATT services](https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-client). Do not equate paired with connected. Codec/battery/reconnect are capability-dependent; do not invent universal codec access or firmware control.
- Headphone arrival: correlate device connection and confirmed default endpoint before showing a brief card; no unsolicited output switching. Existing v0.5 route feedback is smaller in scope.
- Brightness: subscribe to [WmiMonitorBrightnessEvent](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmimonitorbrightnessevent); validate on the internal panel. External-monitor control is separate and optional.
- Battery: read [BATTERY_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/power/battery-information-str) with documented battery IOCTLs. Capacity wear is an estimate based on supplied design/full capacities; zero cycle count can mean unsupported. Rate units can be relative rather than mW. Show charging rate, runtime and completion only when the specific source and units support them. Completion extrapolation under changing charge rates must be labelled as an estimate or omitted. Charge history is opt-in local bounded storage.
- Charging design: brief directional energy stroke into the real percentage ring; reduced-motion fade; no endless decorative loop.
- ROG: read-only discovery first, link to installed vendor controls. No stable universal ASUS profile/GPU-mode control contract has been established here. Do not implement ACPI writes, reverse-engineered firmware commands or driver requirements. Any provider must be optional and explicitly feature-detected.

## Phase 4 — productivity and privacy (delivered in v0.9)

- Clipboard shelf: opt-in AddClipboardFormatListener, bounded in-memory text/image/file/link snapshots, exclusions, pause/clear, no logs of content and no uploads. Never indiscriminately persist copied secrets. Animate only after a successful supported read.
- Microphone: enumerate documented capture audio-session activity and process identity where possible, clearly scoped to that signal. Camera and location require further documented capability research. [AppCapability.AccessChanged](https://learn.microsoft.com/en-us/uwp/api/windows.security.authorization.appcapabilityaccess.appcapability.accesschanged?view=winrt-26100) is a permission-status event, not a global hardware-use event. No broad app-use claim or registry scraping fallback.
- Local commands: deterministic allowlisted grammar for volume, known app launches, and local Windows Search queries. Show parsed intent before launch/search. No arbitrary shell execution or cloud model; date/file filters need tests and transparent search scope.
- Workspace presets: explicitly configured app/URL sets, safe launch targets, debounce/deduplicate and optional confirmation. No automatic browser navigation or application closing while merely switching presets.

## Refresh-rate strategy

Retained DirectComposition animations use continuous absolute-time curves sampled by Windows, with no 60 Hz animation timer. The 30 ms transient HWND-region update is input-region maintenance, not visual frame rendering. Do not advertise it as input synchronization at 240 Hz. The [compositor clock](https://learn.microsoft.com/en-us/windows/win32/directcomp/compositor-clock/compositor-clock) supports monitor-aware frame statistics and dynamic-rate work; investigate it when adding genuinely frame-produced waveform content. Mixed-monitor cadence is an OS/driver/display behavior requiring presentation measurements. Cadence unit tests establish numerical accuracy, not measured FPS.

## Status after v0.7 — 2026-09-23

Phase 2 shipped: multi-session carousel, per-app mixer, real loopback spectrum, app-provided logos (plus confirmed YouTube/YouTube Music marks), and the compact level indicator. From Phase 3, the brightness event provider and brightness indicator shipped early; Bluetooth cards, headphone arrival, battery health and the charging redesign remain. Phase 4 (clipboard, privacy indicators, commands, workspaces) has not started.

Also delivered outside the phase plan at the user's request: a separate live-applying Settings window, and a Frosted/Clear glass material built on the host backdrop brush.

Next recommended phase: Phase 3 devices and power — Bluetooth connection cards with battery where devices expose it, headphone arrival correlated with the confirmed default endpoint, and a battery-health page limited to readings the battery driver actually supplies.

## Status after v0.8 — 2026-09-23

Phase 3 shipped: Bluetooth connection cards (SetupAPI connection/battery/class properties, HCI and device-node events), a Devices tab with one-shot audio connect/disconnect, the battery-health tab from battery IOCTLs with labelled estimates, local charge history, the charging card with a one-shot energy sweep, power-mode display, and read-only ROG discovery that opens Armoury Crate.

Deviations from the plan above, each deliberate:
- Charge history is on by default rather than opt-in, so the 24-hour graph is useful from day one. It stays on this device and turning it off deletes the file.
- Headphone arrival shows when the Bluetooth device connects. It is not yet correlated with the default audio endpoint changing; the existing output-switch feedback still covers that.
- Service logos are bundled (Simple Icons, CC0) instead of read from each service. They identify, and never imply endorsement.

Also delivered at the user's request: edge reveal (hidden until the pointer touches the island's screen edge), 88 brand marks with window-title service detection, and new motion (card morph, logo pop, tab pill, directional page slide).

Next recommended phase: Phase 4, productivity and privacy. Start with the opt-in clipboard shelf and microphone-in-use indicator, both scoped to documented signals as described above.

## Status after v0.9 — 2026-09-23

Phase 4 shipped:
- **Clipboard shelf:** opt-in, AddClipboardFormatListener, 24 entries or 48 MB in memory, private-copy flags and password managers excluded, pause and clear, no logging of content, nothing persisted.
- **Privacy indicators:** camera, microphone and location dots, band and cards.
- **Command bar:** a fixed grammar, with the parsed intent shown before anything runs and no shell execution. File search is limited to the user folder, with date and type filters verified against Windows Search.
- **Workspaces:** explicit save of open apps, a second Enter to launch, and nothing ever closed.

Deviations from the plan above, each deliberate:
- Privacy indicators read Windows' capability access records (the `CapabilityAccessManager\ConsentStore` data behind Settings' "Recent activity") rather than only microphone audio sessions. This covers the camera and location as well. Each record is confirmed against running processes, so a crashed app's open record is ignored. It is read-only and nothing is stored.
- Workspaces hold apps, not browser URLs. The island never reads tab addresses.
- The shortcut defaults to Alt+Shift+Space, because Ctrl+Alt+Space is commonly taken (Claude's desktop app uses it).

Also delivered at the user's request:
- Per-session media identity, which fixes duplicate browser sessions, and joint site assignment from every tab title.
- Compact-only edge reveal.
- Wider pixel-exact shoulders and antialiased corners.
- Sharper text.
- 180 brand marks.
- Content entrance motion.

Next recommended work: UI Automation for the island and Settings controls (screen readers); IME and text selection in the command bar; and correlating headphone cards with the default audio endpoint.

## Status after v0.10 — Phase 5A, motion and materials — 2026-09-23

Shipped:
- **Materials:** Frosted and Clear glass reworked so they differ, with or without Windows transparency; a text halo on unblurred glass; a pointer-following light.
- **Liquid morph**, **staggered row entrances** and **icon swap pops**.
- **Refresh-rate proof:** the Animation Lab, now inside Settings → Motion, plays real transitions and reports frames counted with `DCompositionGetFrameId`, next to the island monitor's refresh rate.
- **Accent from the wallpaper** as a fifth swatch; the artwork accent already existed.
- **Waveform timeline** on the Media page, learned from loopback levels.
- Removed "Your day at a glance" from Home.

Not done in 5A, each deliberately:
- **Odometer numbers.** Digits rolling on change need per-digit surfaces and clipping in several text layouts; deferred rather than faked with a fade.
- **Shared-element transitions.** The artwork already moves and scales between compact, Live and Media on retained springs; a true shared-element morph between arbitrary elements is not implemented.
- The DWM timing counters were found not to track composition on this Windows build (they advance by one per query), so they are no longer used for any number shown.

Next recommended work: the rest of Phase 5, then UI Automation for screen readers and IME in the command bar.

## Status after v0.11 — Phase 5B, Now Playing Pro — 2026-09-24

Shipped: synced lyrics (opt-in, LRCLIB, title and artist only, cached locally), artwork palette (gradients, glow, light-island colour), smarter seeking (hover bubble, detents, double-click and arrow-key skips), the headphone switch card with *Switch back*, per-app volume from the compact island, and microphone mute.

Deviations from the plan, each deliberate:
- **No chapter detents.** Windows media sessions don't expose chapters to other apps. Detents sit on even time marks and lyric line starts instead.
- **Lyrics lookups send only the title and artist**, even though LRCLIB's exact-match endpoint also wants the album and length. The island uses the search endpoint and compares lengths on this PC.
- **Microphone mute covers both default microphone roles**, communications and console, so calls and recorders agree.

Next: Phase 5C, capture and Shelf superpowers.

## Status after v0.12 — Phase 5C, capture and Shelf — 2026-09-24

Shipped: snip to Shelf, copy text from anything (on-device recognition), colour picker, Shelf quick actions (open, open with, show in folder, copy path, copy text, PNG/JPG conversion, half size, zip), the opt-in pinned Shelf, and clipboard v2 (search, pins, picker with paste, hidden secrets).

Deviations from the plan, each deliberate:
- **No WebP conversion.** Windows has a WebP decoder but no encoder; images convert to PNG or JPG.
- **The clipboard picker is Alt+Shift+V, in the island.** Ctrl+Shift+V is already "paste without formatting" in browsers, Office and terminals, so taking it globally would break those apps. The picker opens in the command bar rather than at the text cursor.
- **"Paste as plain text"** is how history always pastes: copies are kept as plain text. Shift+Enter in the picker copies without pasting.
- **Pinned copies survive restarts** (encrypted with the Windows account key), unlike the rest of the history, which stays memory-only.

Also delivered: a polish pass (Home date, mixer button, Shelf chevrons), a local crash report, and two fixes found by testing — a release-only crash class in hand-declared Windows interfaces (fixed for text recognition and for Bluetooth audio connect) and drag-and-drop registration hidden by a comment during development.

Next: Phase 5D, command bar v2.

## Status after v0.13 — Phase 5D, command bar v2 — 2026-09-24

Shipped, as selected: instant file results, system actions with live state and confirmations, recent/pinned/suggested commands, currency conversion (opt-in), matched-letter highlighting, ghost completion and rolling answer digits; plus attached glass with vibrancy, a lyrics redesign and an alignment pass.

Deviations from the plan, each deliberate:
- **Not in this release, as chosen:** the calculator and units, emoji search, editing and input methods, and smarter workspaces. They remain candidates for a later release.
- **Airplane mode** turns every radio off (or back on) through Windows.Devices.Radios; Windows has no public API for its own airplane-mode switch.
- **Answer count-up** applies to currency answers, the one answer type in this release.
- **Karaoke fill** follows line timing (LRCLIB has no word timing).

Next: Phase 5E, live widgets and awareness.

## Status after v0.14 — Phase 5E, glass, awareness and motion — 2026-09-24

Shipped, as asked: a screen-capture dot, rebuilt Frosted and Clear glass (researched, bench-tested, then built), live GPU use, a left dock edge, the Space fix in the command bar, rolling numbers for volume, battery and timers, the compact lyric morph, the beat pulse, the liquid morph and the idle glance; plus, from the suggestions, the tilting highlight, the soft shadow, wallpaper tint, group headers, colour swatches and typo suggestions.

Deviations, each deliberate:
- **"Glass edges that bend the background like a real lens"** became light gathered along the edges. Windows gives apps the backdrop only as a brush that can't be offset or warped (a shifted copy renders black), so refraction isn't possible from an app.
- **Screen capture** is detected through Windows' capture consent records; apps that capture by other means aren't seen.
- **"Live widgets"**, the earlier name for this phase, is partly covered by the idle glance; weather and calendar widgets remain candidates.

Found and fixed along the way: 0.13's vibrancy effect never ran (now it does), and an 85 MB memory regression from extra GPU devices was caught before release.

## Status after v0.15 — Phase 5F, Now Playing, awareness, sharing and motion — 2026-09-25

Shipped, as asked:
- **Now Playing:** compact media controls, swipe to skip, fullscreen peek, the spectrum ring around the compact cover, and the Command Center's lyric line in the compact island and the Live Island, lit word by word when the lyrics carry word timing.
- **Awareness:** the weather glance with an animated sky, which app is using the camera with a one-click route to its privacy page, battery health trends with a weekly card, and rich clipboard rows.
- **Sharing:** between your own PCs on the local network.
- **The island:** the Controls page, the compact chips editor, per-app accent colours, the drop pill, light along the edge on alerts, and adaptive per-letter text on Clear glass.
- **Motion:** rolling numbers that blur and stretch, the lean with a rubber band, morphing and celebrating icons, and a shadow that deepens as the island grows.
- **Checks:** an alignment pass over the islands.

Deviations, each deliberate:
- **Now Playing on the lock screen** isn't possible: Windows draws the lock screen on a secure desktop apps can't draw on. The fullscreen peek covers the other half of the request.
- **Word-by-word lyrics** light up only when the lyrics carry word tags; LRCLIB's are mostly line-timed.
- **Sharing** was verified over loopback between two independent services (pairing, refusals, byte-exact transfers, persistence), not yet between two physical PCs.
- **Adaptive text** works from the wallpaper, so it steps aside when a window is behind the island; reading the real screen behind the island would mean hiding the island from screenshots.
- **The drop pill** needs the top dock; side docks and floating islands keep growing.

Found and fixed along the way:
- The island's own surfaces can't draw while another is open (DirectComposition draws one surface at a time), which made the first weather sky fail at startup.
- The auto-hide stages of the UI test left the island slid away for the stages after them.
- On see-through glass, the stub and the pill overlapped for a few frames.
- The shoulders slid over a pill that hadn't yet left the edge.
- "Thunderstorm" was cut off on the Home tile.

## Status after v0.16 — Phase 5G, sharing, music and alerts — 2026-09-25

Shipped, as asked:
- **Sharing that goes where you drop it:** drop zones for paired PCs, folders, the whole Shelf at once (Send Shelf and the stack gesture), progress and Stop.
- **Weather:** fixed ("Open-Meteo couldn't be reached").
- **Handoff:** Continue on my other PC.
- **Two cards at once:** the second buds off the first.
- **Sounds:** a chime with the edge light, a click in the chips editor.
- **Island DJ:** the ring glows toward the next track.
- **Liquid morphs:** the navigation pill stretches like a droplet.
- **Drag to change media:** fixed and extended everywhere the music shows.
- **Start songs from the island:** the island's own player and music library.
- **Clipboard:** kept across restarts.
- **Checks:** unit, sharing, UI (seven new stages) and settings suites.

Deviations, each deliberate:
- **Handoff** can't make an app play a song it doesn't have; it uses whichever of four routes works on the other PC, and says so when none does.
- **Island DJ** can't know what another app plays next; it uses the current cover's colours there.
- **Swiping** now skips tracks everywhere; switching players moved to the dots under the cover.

Found and fixed along the way:
- WinHTTP's decompression failed on Open-Meteo's answers (a 200 whose body couldn't be read), so weather never worked.
- With *Open on hover*, the island was in the Live Island before a drag could start, and drags there only switched sessions.
- On Windows, a blocking socket call doesn't wake when another thread shuts the socket, so stopping a transfer could hang; transfers now wait in fifth-of-a-second slices.
- The UI test's fullscreen stage could be undone by real foreground changes on the desktop.
- This toolchain's `windows.foundation.h` defines `IReference<BYTE>` twice (`boolean` is `BYTE`); the player's file skips the duplicate.


## Status after v0.17 — Phase 5H, Up next, Shelves and screen readers — 2026-09-25

Shipped, as asked:
- **UI, alignment and animation fixes:** the command bar's stray logo, the cover over the Library, rows touching footers, the Outputs chevron, the sky over the Home tile's text, light-theme contrast, cut-off Settings text, a duplicate town.
- **Weather:** the town is chosen in Settings, with regions; checked with Manipal, Udupi, Jubail, Indore, Jaipur and Mumbai.
- **The other PC's cover** on *Play here*.
- **Up next** with drag to reorder, and the next cover peeking out.
- **Taking from the other PC's Shelf.**
- **Crossfade**, and fades when music moves between PCs.
- **A ring for 5 GB+ transfers** with speed and time left.
- **Two alerts side by side** on hover.
- **A spoken island** through UI Automation.
- **Checks:** unit, sharing (46 new checks), UI (three new stages) and settings suites.

Deviations, each deliberate:
- **Crossfade** covers the island's own songs; another app's crossfade is that app's setting.
- **Screen readers** get the island itself; the Settings window, which draws its own controls, is next.
- **Compatibility:** the cover and taking from a Shelf are revision 1 of protocol 2, so 0.16 PCs keep sharing with 0.17 and are simply not asked for them.

Found and fixed along the way:
- The solid island's fill was 640 DIPs wide on a 680-DIP canvas, so the side-by-side card lost its right edge.
- In test runs the command bar could open without its search service.
- The first search waited on Windows' radio state; it's now read when the service starts.
- A test script's clean-up stopped every copy of the island, including the installed one; it now stops only test copies.

## Status after v0.17.0-preview.2 — glass, lyrics timing, full UI audit — 2026-09-26

Shipped, as asked:
- **Frosted and Clear glass:** fixed. There were three causes:
  - properties missing from the property set
  - an expression too long to start
  - numbers written with exponents
- **Lyrics timing:** positions now move on from when the player last reported them; checked against a real session.
- **Every UI surface and alignment:** audited from the island's own renders and the Settings window's own frames. Fixed:
  - offer card details
  - output icons
  - command icons and hints

Guards added:
- A phase test reads the glass source and fails if any `p.<name>` it reads or animates isn't created with the property set.
- A phase test fails if any expression number has an exponent.
- The UI test fails if the glass reports any error, and runs Clear and Frosted through every shape.

## Status after v0.17.0-preview.3 — glass that listens, frost, Settings previews — 2026-09-26

Shipped, as asked:
- **The colour change at the ends of the curves:** fixed. The pointer's light moved into the glass (body and both shoulders), and the side edge light now starts below the shoulder join.
- **Lines across the island for a few seconds:** fixed. The alert's edge light was drawn through hard-edged windows; it now uses six nested windows, each a sixth as bright, so it rises and falls smoothly.
- **A glint that follows the pointer across the rim:** a radial-gradient stroke on the body's rim, centred under the pointer.
- **A frost that thickens as the island rests:** a paler, milkier, softer material fades in over 24 s after 6 s at rest, and clears in 0.35 s.
- **Separate tints for Clear and Frosted:** settings v17 (`clearTint`).
- **A live preview of the glass in Settings:** Appearance › Preview.
- **Rows that open with small animated previews:** 14 rows, after a 450 ms rest.
- **Dynamic type:** titles shrink to 86% before being cut.
- **A sunrise and sunset sky:** Open-Meteo's daily sunrise and sunset, in the town's own time zone.
- **A breathing edge light to the beat:** glass rim and shoulders, or soft strips on Solid, driven by the bass.

Not shipped:
- **Refraction that bends the wallpaper.** Windows' composition accepts colour and blur effects on the host backdrop, but refused every geometric effect (2D affine transform with each interpolation and border mode, identity included, and scale) with `E_INVALIDARG`. Clear glass has no backdrop to resample. The alternative, copying the screen behind the island continuously, isn't something the island does.

## Status after v0.18.0-preview.1 — updates, weather on the glass, battery details — 2026-09-26

Shipped, as asked:
- **Weather in the glass:** rain streaks, beaded drops, fog and snow inside the island's outline, above the glass and under the text; dimmed while open; still with reduced motion.
- **Battery health explained, and every reading:** Stats › Battery › All details (up to 27 readings, paged by the wheel).
- **Many weather stats:** the weather view from the Home tile: now, eight hours and twelve readings, plus air quality.
- **More Settings previews:** 23 more, 37 in all.
- **The alerts:** alerts that arrived while the island was open were lost; they now wait. Low battery no longer repeats every percent.
- **Automatic updates** from the GitHub releases on every PC running 0.18 or later.

Verified:
- The update path end to end against the live 0.17.0-preview.3 release, from a scratch folder.
- The UI test (53 stages) and the Settings test.
- Unit tests for versions, release choice, checksums, weather parsing and units, and battery helpers.

Limits:
- **Unsigned.** Updates are checked by SHA-256 and by the program's own version, both from this repository's releases over HTTPS. A signed build would add a publisher check.
- **First install.** Laptops on 0.17 or earlier need 0.18 installed once.
