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
