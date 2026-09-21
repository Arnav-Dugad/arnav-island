# v0.1.0 — Native motion prototype

First runnable engineering prototype, not a production release or completed
Milestone 1. Windows 11 x64; no installer or administrator privileges required.

Contains a DirectComposition island, velocity-preserving springs, geometry
morphing, pointer feedback, real system volume/battery, GSMTC metadata foundation,
Animation Lab, Settings skeleton and local diagnostics. No cloud or paid APIs.

Extract the archive, then open `app/NexusIsland.exe`. Right-click the island or
tray icon for Animation Lab, Settings, diagnostics and Exit. Scroll over the
island changes system volume. Only one instance runs at a time.

Validation: release build, two test suites, 11,682 core checks, five real provider
start/stop cycles, 200-interruption stress run and reviewed native screenshots.
See `docs/PERFORMANCE_RESULTS.md` for measurements and their limits.

Known release gates include transient click-through routing, full accessibility,
mixed-DPI Settings layout, device-loss recovery, active-player testing, artwork
and complete frame/GPU instrumentation. See `docs/FEATURE_MATRIX.md` and
`docs/REPORT.md`. No high-refresh or multi-day reliability claim is made.
