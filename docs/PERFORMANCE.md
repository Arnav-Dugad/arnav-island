# Performance engineering

## Current design

DirectComposition owns animation sampling. No application render loop, polling
of audio/media/battery state, per-frame disk writes or repeated geometry rasterizing.
D3D11 is only an interop device; there are no application shaders. Graphics objects
and surfaces persist. Content changes allocate temporary text/brush resources;
these are not allocated for every compositor frame. Further caching is planned.

Audio and GSMTC have dedicated COM workers. Audio events carry atomics and a
coalesced posted notification; media workers publish short snapshots. Filesystem
settings/log writes run on one bounded background queue. Power and foreground
changes arrive as OS events. HUD counters update only when enabled. Activity
expiry uses a temporary 500 ms timer. A settle timer tightens the input region
after motion; it does not drive rendering. All these timers stop when idle.

## Instrumentation

Right-click the island or tray icon > Performance HUD opens the lab and adds
DWM refresh cadence, process working set, composition commit count, queue depth,
and island state to its title. DWM refresh is explicitly **not app FPS**.
Per-app presented FPS, frame-time percentiles, GPU cost, UI-stall percentiles and
active-animation counts are not yet instrumented. Do not infer them from commits.

## Repeatable runs

1. `scripts/build.ps1 -Test`: release build, physical/state/settings tests and
   real provider start/stop lifecycle tests. Provider tests never change volume.
2. Close the running prototype, then `build/NexusIsland.exe --benchmark`.
   The app executes 200 size reversals with coalesced synthetic volume activities
   at a 73 ms input cadence; this cadence is a stress input rate, not frame rate.
   It exits and writes `%LOCALAPPDATA%/NexusIsland/benchmark.json`.
3. `scripts/measure-idle.ps1`: launches or measures the app after settling,
   collecting process CPU time and working set over a timed idle interval.
4. `--lab --expanded --capture`: explicit app QA screenshots. A neutral temporary
   matte shields personal desktop content during island capture. Lab uses
   PrintWindow. Capture is never enabled in normal usage.

The remaining matrix must cover real volume-key storms, supported media players,
power attachment, file drag after implementation, notification storms after
implementation, dashboard, mixed-DPI multi-monitor, suspend/resume, GPU device
loss, 60/90/120/144/165/240 Hz physical displays and multi-day soak. Preserve raw
measurements and tool/build context. Do not claim unmeasured battery impact.
