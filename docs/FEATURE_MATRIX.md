# Feature status — v0.8 preview

| Area | Implemented | Limits |
|---|---|---|
| Design | Small top-center dock with curved shoulders; right-edge option; dark/light/system themes | Visual quality assessed from actual captures, not certified against another product |
| Motion | Analytical springs, velocity-preserving reversals, size/corner morph, art position/scale, hover highlight, press highlight, charging pulse, page offset | Interactive vector icons lift/scale; labels stay stable. Two-surface handoff preserves the visible composite during interruption |
| Media | GSMTC thumbnail/metadata/transport/timeline; separate compact, Home, Music and Video destinations | All services not tested; only OS-exposed artwork; Auto cannot reliably classify every browser |
| Shelf | File paths and Unicode text; copy-only OLE drop/drag-out; 32 entries with scroll, Shell thumbnails/file icons and drag images | In-memory; installed document handlers required; no virtual file/bitmap data, pre-entry attraction or persistence |
| Audio | Endpoint events, volume/mute, output names, optional direct switching | Isolated undocumented setter; current-output reselection tested, physical headphones/speakers switching not exercised |
| Power | Real percentage/AC state, event-driven charging pulse, low-battery priority; v0.8 battery driver readings (see below) | Estimates are labelled; no firmware access |
| Compact | Media title/art, timer, volume activity, battery | One priority activity at a time; no waveform simulation |
| Glass | v0.7: whole-island Frosted/Clear glass from the Windows.UI.Composition host backdrop brush, tint, sheen and rim; shape follows the body springs in the compositor | Blur needs Windows Transparency effects; off, battery saver or high contrast give tinted, unblurred glass. Glass always floats (no concave shoulders) |
| Statistics | CPU/history, memory, physical network throughput, disk free, uptime, thread count | Adaptive 1 Hz while visible; no GPU/fan/temperature readings |
| Glance rings | Battery/timer/both/off; compositor marker rotation; larger Focus ring | Arcs update on state snapshots, not per-frame radial geometry |
| Design system | 35 original vector symbols, centred controls, physical-pixel settled offsets, labelled navigation | Complete accessibility and text scaling remain unfinished |
| Focus | Timer/break/stopwatch; pause/reset and completion activity | No persistence through restart, calendar or reminders |
| Preferences | Ten groups, reorderable navigation, chosen Home metrics, v5 settings migration, independent layout reset, launch-at-sign-in | Startup uses current exe path; no settings import/export UI, profile import/export or full geometry editor |
| Native behavior | Per-monitor DPI v2, input region, selected monitor, fullscreen hide, configurable app-switch collapse and protected drags, decorated/maximized browser exclusion, no taskbar button | Mixed-DPI/hot-plug/high-refresh hardware acceptance unfinished |
| Diagnostics | Animation Lab, basic HUD, app-only captures, process-counter benchmark | No fabricated FPS/GPU metrics |
| Accessibility | Keyboard controls, OS reduced motion, opaque fallback under high contrast | Full screen-reader tree, text scaling and complete high-contrast palette unfinished |
| Safety | No injection, drivers, cloud, Explorer patching or arbitrary plugins | Provider recovery and GPU device-loss restoration unfinished |

Clipboard history, external notifications, camera/microphone indicators and external download monitoring are not implemented. Brightness (v0.7) and read-only ROG discovery (v0.8) are covered in the tables below. Empty or unavailable sensor values stay unavailable.

| Feature | Implemented | Limits |
|---|---|---|
| Precision seeking | Spring-expanded timeline, fine pointer gain, one seek on release, cancel | Requires advertised Windows session capabilities and seek bounds |
| Audio handoff | Confirmed default endpoint activity, pulse and selected icon spring | No invented Bluetooth codec, latency or connection status |

| Feature | Implemented | Limits |
|---|---|---|
| v0.6 modes | Mini Pill, Live Island hover card, Command Center; blank-surface click does not open/pin | Live card is intentionally a smaller control subset; large workspace remains available |
| Compact customization | Width 160–560 DIP, optional media/volume/timer/battery/clock; Mini fixed at 72 DIP | Details fit available space; no hidden telemetry polling for extra statistics |
| Display memory | Stable device-path preference and bounded local geometry profiles | Physical docking/refresh/mixed-DPI acceptance remains incomplete |
| Shelf peek | Spring-scale/fade larger cached thumbnail on hover | File icons remain icons when no document thumbnail is available |
| Artwork atmosphere | Continuous spring retargeting of retained radial color layers | Subtle light only; no real audio visualization in this phase |

See DELIVERY_PHASES.md for the remaining request; planned providers are not shipped features.

| v0.7 feature | Implemented | Limits |
|---|---|---|
| Settings window | Separate thread and window; all persisted preferences; live application; autosave; keyboard navigation; Mica when allowed | Custom-drawn controls have no UI Automation tree yet; screen-reader support remains unfinished |
| Media sessions | All GSMTC sessions (up to 8), per-session events, selection by app ID, swipe/drag/touchpad/tap, follow-current option | Players that do not publish a Windows media session cannot appear; one browser can expose one session for several tabs |
| App logos | Icon and name from `shell:AppsFolder\<AUMID>` or the running executable; YouTube/YouTube Music marks when the browser window title confirms both the title and service | Other web services show the browser; an inactive tab's service cannot be confirmed |
| Live waveform | WASAPI shared loopback on a worker, 1024-point FFT, 24 log bands, compositor-interpolated bars in compact, Live and Media | Protected/exclusive-mode audio can appear silent; runs only while a session is playing and bars are visible |
| Per-app mixer | IAudioSessionManager2 sessions grouped by process, ISimpleAudioVolume volume/mute, live IAudioMeterInformation peaks, real process icons | Default output only; apps that route to another device are not listed |
| Level indicator | Volume (endpoint callback) and brightness (WmiMonitorBrightnessEvent) grow the compact island into a bar | Brightness works on panels that expose the WMI class (internal laptop panels); external monitors report unavailable |
| Intent-aware hover | Fast pointer sweeps restart the hover delay | Threshold is fixed at 700 DIP/s |

| v0.8 feature | Implemented | Limits |
|---|---|---|
| Edge reveal | On by default. Tucks the island past its docked edge; reveals only when the pointer is on the edge pixels within the island's band (±56 DIP); tucks after the collapse delay; open pages, pin, drags, drops and seeking keep it out; hidden island has an empty input region. Glass and region follow the slide spring | Checked every 33 ms while enabled (a cursor position read, no hooks). Alerts while hidden are off by default |
| Brand marks | 88 Simple Icons 16.32.0 paths (CC0) parsed into Direct2D geometry; white plate for dark marks | Trademarks of their owners; used only to identify an app, service or device maker |
| Web service identity | 44 rules matched against visible browser window titles, requiring the playing title or a Spotify-style "title • artist" match; brand-only titles accepted only when unique | A background tab, a renamed window or a title without the service name falls back to the browser icon |
| Bluetooth cards | SetupAPI device properties for connection, battery and class of device; HCI connect/disconnect and device-node events; 60 s backstop check; maker from name keywords and vendor ID; type from class of device and name | Battery only when the device reports it to Windows. No codec, latency or signal strength (no public API). Names are matched at word starts |
| Audio connect | One-shot connect/disconnect through the Bluetooth audio driver's KS property on the device's endpoint | Audio devices only; availability depends on the driver. Keyboards, mice and controllers are listed without buttons |
| Battery tab | IOCTL_BATTERY_QUERY_* capacities, rate, voltage, cycles; health = full-charge/design; 24-hour graph from local history; estimates from the reported rate | Cycle count 0 is shown as unavailable. Relative-unit batteries hide energy values. This laptop reports rate 0 while holding charge |
| Charge history | 7 days at 5-minute spacing in `battery-history.nexus`, atomic writes | On by default; turning it off deletes the file |
| Charging card | AC change shows level, rate, time to full or battery-care state, one-shot energy sweep | No endless decorative loop; with reduced motion the sweep is skipped |
| Power mode | PowerRegisterForEffectivePowerModeNotifications | Display only; the island never changes the power mode |
| ROG | Manufacturer/model from the BIOS registry values; Armoury Crate detected in the Start menu app list and opened by its app ID | Read-only. No ACPI, WMI writes, GPU-mode, fan or profile control |
