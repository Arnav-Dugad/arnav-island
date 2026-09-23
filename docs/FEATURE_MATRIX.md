# Feature status — v0.6 preview

| Area | Implemented | Limits |
|---|---|---|
| Design | Small top-center dock with curved shoulders; right-edge option; dark/light/system themes | Visual quality assessed from actual captures, not certified against another product |
| Motion | Analytical springs, velocity-preserving reversals, size/corner morph, art position/scale, hover highlight, press highlight, charging pulse, page offset | Interactive vector icons lift/scale; labels stay stable. Two-surface handoff preserves the visible composite during interruption |
| Media | GSMTC thumbnail/metadata/transport/timeline; separate compact, Home, Music and Video destinations | All services not tested; only OS-exposed artwork; Auto cannot reliably classify every browser |
| Shelf | File paths and Unicode text; copy-only OLE drop/drag-out; 32 entries with scroll, Shell thumbnails/file icons and drag images | In-memory; installed document handlers required; no virtual file/bitmap data, pre-entry attraction or persistence |
| Audio | Endpoint events, volume/mute, output names, optional direct switching | Isolated undocumented setter; current-output reselection tested, physical headphones/speakers switching not exercised |
| Power | Real percentage/AC state, event-driven charging pulse, low-battery priority | No invented runtime estimates or firmware access |
| Compact | Media title/art, timer, volume activity, battery | One priority activity at a time; no waveform simulation |
| Glass | Documented DWM transient backdrop inside expanded panel; opaque exterior and fallback | Intentionally excludes rounded perimeter; effect is subtle and stops during body motion |
| Statistics | CPU/history, memory, physical network throughput, disk free, uptime, thread count | Adaptive 1 Hz while visible; no GPU/fan/temperature readings |
| Glance rings | Battery/timer/both/off; compositor marker rotation; larger Focus ring | Arcs update on state snapshots, not per-frame radial geometry |
| Design system | 35 original vector symbols, centred controls, physical-pixel settled offsets, labelled navigation | Complete accessibility and text scaling remain unfinished |
| Focus | Timer/break/stopwatch; pause/reset and completion activity | No persistence through restart, calendar or reminders |
| Preferences | Ten groups, reorderable navigation, chosen Home metrics, v5 settings migration, independent layout reset, launch-at-sign-in | Startup uses current exe path; no settings import/export UI, profile import/export or full geometry editor |
| Native behavior | Per-monitor DPI v2, input region, selected monitor, fullscreen hide, configurable app-switch collapse and protected drags, decorated/maximized browser exclusion, no taskbar button | Mixed-DPI/hot-plug/high-refresh hardware acceptance unfinished |
| Diagnostics | Animation Lab, basic HUD, app-only captures, process-counter benchmark | No fabricated FPS/GPU metrics |
| Accessibility | Keyboard controls, OS reduced motion, opaque fallback under high contrast | Full screen-reader tree, text scaling and complete high-contrast palette unfinished |
| Safety | No injection, drivers, cloud, Explorer patching or arbitrary plugins | Provider recovery and GPU device-loss restoration unfinished |

Brightness, ROG actions, clipboard history, external notifications, camera/microphone indicators and external download monitoring are not implemented. Empty or unavailable sensor values stay unavailable.

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
