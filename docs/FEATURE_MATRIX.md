# Feature status

This is an engineering prototype of Milestone 1, **not a completed Milestone 1
or production release**. Implemented is not synonymous with fully validated.

| Area | Current state | Remaining verification / work |
|---|---|---|
| Native island | Running Win32 + DirectComposition; no taskbar/Alt-Tab surface; tray controls | Shell lifecycle, mixed displays, release QA |
| Motion | Analytic springs, preserved velocity, compositor curves, size/radius morph | Actual frame timing, shared elements, content choreography |
| Interaction | Hover, press, click toggle, rubber-band drag/release, scroll volume | Touch, accessibility tree, transient input routing |
| Geometry states | All named destinations modeled; compact/expanded main flow | Per-state content layout; most other states are lab geometry only |
| Volume | Real endpoint callbacks, coalescing, mute state, animated bar, scroll writes | Friendly device name, switcher, rapid physical-key QA |
| Media | Public GSMTC manager/session events, title and artist, worker lifecycle | Active-player test; artwork, timeline, visible transport controls, palette |
| Battery | Real percentage/AC events; low-battery priority; unknown stays unknown | Saver policy, physical charger tests, richer energy animation |
| Animation Lab | Native controls, five presets, tuning, reversal, impulse, deliberate stall | More channels, graphs, polished accessible controls |
| Settings | Native skeleton, versioned local settings, presets, reset/save | Complete sections, import/export UI, all geometry controls, tuning persistence |
| Diagnostics | Structured local log, rotation, open/clear, optional basic HUD, benchmark | Rich FPS/stall/GPU instrumentation, copy diagnostics |
| DPI | PMv2 declaration, island surface recreation on DPI change | Fractional DPI, settings layout and multi-monitor validation |
| Displays | Primary/selected numeric monitor setting, display-change repositioning | Stable monitor identifiers, profiles, follow-active and all displays |
| Fullscreen | Foreground event + monitor bounds heuristic, hide option | Movies/games, borderless false positives, importance/minimal policy |
| Material | Dark gradient base, physical one-pixel rim, antialiased clip | Native acrylic/backdrop implementation, soft shadow, GlassMaterial integration |
| Providers | Separate audio/media workers, power events, normalized orchestration | Universal provider contract, retry/backoff, out-of-process isolation |
| File shelf | Deferred | OLE drag/drop, safe references, previews, drag-out |
| Clipboard | Not implemented; nothing observed/stored | Explicit opt-in privacy design and exclusions |
| Notifications | Researched; not observed | Manifest capability, user consent, supported action semantics |
| Brightness | Researched WMI path | Internal panel capability detection; optional external DDC/CI |
| Hardware | Process memory in HUD only | OS utilization providers; no temperatures/fans shown |
| ASUS ROG | Research boundary only | No safe supported OEM data contract integrated |
| Gaming | Generic fullscreen hide only | Session tracking, supported utilization; no injection |
| Privacy indicators | Unavailable | No unsupported global mic/camera usage claims |
| Downloads | Activity enum foundation only | Application-owned downloads and reliable progress source |
| Productivity | Deferred | Timer, stopwatch, Pomodoro, notes, calendar |
| Quick controls | Scroll system volume only | Supported brightness/audio/network controls |
| Themes | Dark prototype | System/high contrast/light, readable adaptive accents |
| Accessibility | Native lab keyboard controls, OS animation setting | UIA island provider, text scaling, complete high contrast |
| Startup | Manual quiet launch | Opt-in login registration |
| Plugins | No third-party code loading | Secure broker/schema model |
| Crash recovery | Caught provider errors; app fails independently | Device-loss recovery, provider restart, multi-day resilience |

## Known release blockers

- During motion the island temporarily uses a larger Win32 input region so the
  compositor cannot be cropped by stale window geometry. HTTRANSPARENT is not a
  general cross-process input-forwarding contract. Click-through outside the
  moving silhouette needs a dedicated input-routing design and testing.
- GPU device loss currently closes the prototype instead of rebuilding the
  graphics device; this is safe for Windows but not production-grade resilience.
- Native Settings is a skeleton. It is not a finished WinUI 3 settings application.
- Custom island controls do not yet expose a complete UI Automation provider.
- No active player was available for the live media integration acceptance test.

## Safe future integrations

LibreHardwareMonitor is MPL-2.0 and includes hardware access paths that may need
privileges/drivers. It is researched, **not bundled or invoked**. Review each sensor
backend and its dependencies before considering an optional read-only provider.
G-Helper uses ASUS ACPI/WMI through the ASUS System Control Interface and carries
GPL-3.0 licensing. It is researched, **not copied or invoked**. Do not probe unknown
ACPI endpoints, alter ASUS services, or port firmware writes into this project.

Sources: [LibreHardwareMonitor license](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/master/LICENSE),
[G-Helper license](https://github.com/seerge/g-helper/blob/main/LICENSE),
[G-Helper FAQ](https://github.com/seerge/g-helper/wiki/FAQ).
