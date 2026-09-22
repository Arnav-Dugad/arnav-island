# Feature status — v0.3 preview

| Area | Implemented | Limits |
|---|---|---|
| Design | Small top-center dock with curved shoulders; right-edge option; dark/light/system themes | Visual quality assessed from actual captures, not certified against another product |
| Motion | Analytical springs, velocity-preserving reversals, size/corner morph, art position/scale, hover highlight, press highlight, charging pulse, page offset | Magnetic highlight moves; button labels remain stable. Track artwork itself changes on new metadata, without a two-art crossfade |
| Media | GSMTC thumbnail/metadata/transport/timeline; separate compact, Home, Music and Video destinations | All services not tested; only OS-exposed artwork; Auto cannot reliably classify every browser |
| Shelf | File paths and Unicode text; copy-only OLE drop/drag-out; 32 entries with scroll | In-memory; filename previews; no virtual file/bitmap data, pre-entry attraction or persistence |
| Audio | Endpoint events, volume/mute, output names, optional direct switching | Isolated undocumented setter; current-output reselection tested, physical headphones/speakers switching not exercised |
| Power | Real percentage/AC state, event-driven charging pulse, low-battery priority | No invented runtime estimates or firmware access |
| Compact | Media title/art, timer, volume activity, battery | One priority activity at a time; no waveform simulation |
| Glass | Documented DWM transient backdrop inside expanded panel; opaque exterior and fallback | Intentionally excludes rounded perimeter; effect is subtle and stops during body motion |
| Statistics | CPU/history, memory, physical network throughput, disk free, uptime, thread count | Adaptive 1 Hz while visible; no GPU/fan/temperature readings |
| Focus | Timer/break/stopwatch; pause/reset and completion activity | No persistence through restart, calendar or reminders |
| Preferences | Five groups, 25 controls, versioned atomic local settings, reset, launch-at-sign-in | Startup uses current exe path; no settings import/export UI, per-display profile library or full geometry editor |
| Native behavior | Per-monitor DPI v2, input region, selected monitor, fullscreen hide, no taskbar button | Mixed-DPI/hot-plug/high-refresh hardware acceptance unfinished |
| Diagnostics | Animation Lab, basic HUD, app-only captures, process-counter benchmark | No fabricated FPS/GPU metrics |
| Accessibility | Keyboard controls, OS reduced motion, opaque fallback under high contrast | Full screen-reader tree, text scaling and complete high-contrast palette unfinished |
| Safety | No injection, drivers, cloud, Explorer patching or arbitrary plugins | Provider recovery and GPU device-loss restoration unfinished |

Brightness, ROG actions, clipboard history, external notifications, camera/microphone indicators and external download monitoring are not implemented. Empty or unavailable sensor values stay unavailable.
