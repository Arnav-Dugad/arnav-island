# Performance policy

Arnav Island uses compositor animation for size, corners and offsets. It does not render a frame from an application 60 Hz loop. A short 30 ms Win32 region timer runs only during physical movement for input bounds; it is not the animation clock.

Idle workers wait on events. CPU/RAM/network collection activates after Overview/System remains visible for 400 ms and samples at 1 second intervals. Disk space is refreshed about every 30 active samples. Media timeline repainting runs once per second only for a visible, playing media view. Focus uses a 1 second timer while running, including when collapsed. GPU use (v0.14) comes from Windows' PDH performance counters, sampled with the rest of the system readings and only while the Stats page, Home or the idle glance shows it; the app uses no monitoring driver and reads no GPU sensors.

Thumbnail decoding happens on the media worker and is capped to 512 pixels on its longest edge. Input compressed stream size, dimensions and pixel count are bounded. Surface rasterization occurs for changed content and button hover states, not each body-animation frame. D2D surface updates can still allocate render targets/bitmaps; richer retained content caching is future optimization work.

Developer scenarios:

```powershell
.\build\ArnavIsland.exe --benchmark
.\scripts\measure-idle.ps1
.\build\ArnavIsland.exe --ui-test --capture-safe
.\build\ArnavIsland.exe --expanded --lab
```

The HUD reports process memory, DWM refresh rate, compositor commit count and activity queue depth. It does not report true presented application FPS or GPU load. A refresh-rate reading must never be relabeled FPS. See PERFORMANCE_RESULTS.md for actual measured evidence and untested conditions.

Required future acceptance: high-refresh frame pacing, actual presentation latency, UI stalls under heavy load, GPU-loss recovery, WARP, integrated-only laptops, mixed DPI, multi-day leaks and battery drain. No claim of Apple-level motion quality is made from compilation or numerical tests alone.

v0.3 avoids full dashboard redraw on pure expand/collapse reversals. The DWM material host only exists after an expanded panel requests glass; it hides during movement. No steady animation timer runs for settled hover highlights, artwork, or charging. Motion is uploaded to DirectComposition as time curves. See PERFORMANCE_RESULTS.md for measured process counters and explicit unmeasured metrics.

## v0.4 rendering policy

Icon assets are cached per retained slot; pointer feedback changes compositor transforms. Artwork work is bounded to cover changes. Full dashboard redraws are skipped while compact; timer ticks update the compact header/rings and mark the hidden content dirty. Opening performs one fresh content render. No background frame callback or continuous decorative loop was added.

The measurement loop separately records settled idle, visible Home, compact active timer and 200 rapid geometry retargets. All are process counters, with actual media disabled for reproducibility. The compact timer optimization was made after observing avoidable hidden content repaint cost.
