# Feature status — v0.2 preview

Implemented does not mean validated on every Windows configuration.

| Area | Available now | Limits |
|---|---|---|
| Identity and UI | Arnav Island; five native dashboard views | Not a finished WinUI settings app |
| Body motion | Analytic springs, preserved velocity, independent DirectComposition curves, shape morph, rubber-band drag | Shared artwork motion and content choreography pending |
| Hover | Configurable opening delay, leave-to-close, pin | Pointer regression passed; touch not supported |
| Overview | Time/date, real CPU/RAM, power/battery, media, network, volume | Larger customization system pending |
| Media | OS thumbnail, title/artist, playing state, transport capability checks, timeline; music/video layouts | Player must expose SMTC; browser classification can require manual override; all named services not tested |
| Audio | Endpoint events, volume buttons/scroll, mute toggle | Output-device name and switcher pending |
| Power | Battery percentage, AC connection, low-battery priority | Rich charging motion and saver policy pending |
| System | CPU/history, RAM, physical adapter traffic, disk free/total, uptime, logical processor count | No GPU temperature, fan, OEM firmware or driver access; >64-core groups not covered |
| Focus | Focus/break timer and stopwatch, pause/reset, completion activity | No persistence through app exit, calendar or notes |
| Quick controls | Windows sound/display/network/Bluetooth settings links | Links are not in-island toggles |
| Settings | Hover toggle/delay, five motion presets, reduced motion, fullscreen hide, automatic local save | No complete geometry editor or import/export UI |
| Animation Lab | Tuning, interruption, impulses, deliberate UI stall | Not all requested animation channels implemented |
| Diagnostics | Local structured logs, basic optional HUD, benchmark, app-only capture | No claimed measured FPS/GPU utilization |
| DPI/display | Per-monitor-v2, selected monitor setting, display-change repositioning | Mixed-DPI, hot-plug, every-monitor and high refresh unvalidated |
| Fullscreen | Documented foreground/bounds heuristic; monitoring sleeps when hidden | Games/movies matrix unvalidated |
| Privacy | Local-only; no account, upload, clipboard, microphone samples or arbitrary plugin loading | Explicit QA capture can include current media unless --capture-safe is used |
| Accessibility | Native lab keyboard controls; island Tab/Enter/Escape; OS reduced-motion setting | Full UIA, text scaling and high contrast remain release gates |
| Resilience | Provider boundaries, caught media/audio errors, no Windows injection or patching | Provider retry and GPU device-loss recovery pending |
| Distribution | Portable Windows x64 preview, static compiler runtime | Unsigned; no automatic updater or startup registration |

A conservative moving window region can still intercept clicks just outside the visual edge during motion. The whole canvas is no longer used as the temporary region. It is not a complete cross-process click-through solution.

File shelf, clipboard, notification observation, brightness, ROG actions, temperature/fan sensors, camera/microphone indicators, downloads and external plugins are deferred. No placeholder sensor values or fake provider successes are presented. LibreHardwareMonitor and G-Helper remain research references only; no code, driver or firmware interface from either is bundled.
