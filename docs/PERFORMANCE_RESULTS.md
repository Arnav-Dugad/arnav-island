# Arnav Island v0.2 measurements — 2026-09-21

Final Release executable SHA-256: `d9e80321f9db8b950423dd01a6893006dc4fa519d3252fc5c837aea7e25a7382`.

| Measurement | Observed |
|---|---:|
| Core checks | 11,682 passed |
| Dashboard/timer/settings checks | 21 passed |
| CTest suites | 3/3 passed, including real audio/media/system lifecycle |
| Native interaction regression | Passed hover on/off, leave close, navigation, timer actions, hit targets |
| 200 interrupted morphs | 15.9604 s wall time |
| Stress process CPU time | 0.390625 s |
| Stress CPU, fraction of one logical core | 2.45% |
| Stress working set | 69.49 MiB |
| Stress composition commits / surface redraws | 206 / 9 |
| Stress pending activities at finish | 0 |
| Idle duration, media provider disabled | 15.0044312 s |
| Idle CPU-time increase | 0 s |
| Idle working set | 63.71 MiB |
| Visible Overview duration, media provider disabled | 15.0115055 s |
| Visible Overview process CPU time | 0.203125 s |
| Visible Overview CPU, fraction of one logical core | 1.35% |
| Visible Overview working set | 69.96 MiB |

Raw v0.2 records and reviewed app-only screenshots are in [evidence/v0.2](evidence/v0.2/). The media provider was disabled for idle/dashboard release measurements and screenshots to exclude private playback. The stress run used normal providers. A separate earlier idle sample with media enabled recorded 0 measurable CPU seconds over 15.018 s and 69.80 MiB, but is not the final-binary acceptance sample.

These are short process-counter samples. Zero means below measurement resolution, not zero energy use. Process CPU excludes DWM/GPU work. No presented FPS, frame-time percentile, GPU utilization or battery-life claim is made. Physical high-refresh displays, mixed DPI, GPU-loss recovery and multi-day behavior remain unverified. Mathematical high-refresh tests do not establish visual smoothness.

All five views, video layout and Animation Lab were visually inspected. A real active media session also supplied a thumbnail and transport state during local inspection; private metadata/artwork were excluded from the published evidence. Empty video state wording was corrected after inspection so it no longer claims paused playback without a session.

The original v0.1 measurements below remain historical context and do not describe the redesigned dashboard.
# Historical v0.1 baseline — 2026-09-21

These results cover a short engineering-prototype run, not production acceptance.
Raw records are in [evidence](evidence/).

## Environment

Windows 11 25H2, build 26200. Intel Core i7-13650HX. Intel UHD Graphics and
NVIDIA RTX 4060 Laptop GPU present. At measurement time the OS reported an active
Intel display path at **1920×1080, 60 Hz**, despite the requested laptop target
being 1920×1200. Refresh rate was not changed. MinGW-w64 GCC 16.2, CMake Release
build (`-O3 -DNDEBUG`), statically linked compiler runtime. The app uses a hardware
D3D11 interop device by default; WARP fallback was not exercised.

## Results

| Measurement | Observed |
|---|---:|
| Core assertions | 11,682 passed |
| CTest suites | 2/2 passed (core and actual provider lifecycle) |
| Largest measured spring-curve error | 0.00198515 DIP |
| 1,000 curve generations, latest sample | 4.9709 ms |
| Real audio/media start-stop cycles | 5, completed in 1.55752 s in recorded run |
| Retarget stress inputs | 200 |
| Retarget stress elapsed | 15.6348 s |
| Process CPU time during stress | 0.09375 s |
| Average stress CPU, fraction of one logical core | approximately 0.60% |
| Stress working set | 69,115,904 bytes (65.91 MiB) |
| Stress peak working set | 71,176,192 bytes (67.88 MiB) |
| Composition commits during stress run | 205 |
| Surface redraws, including initialization | 7 |
| Queued activities at stress end | 0 |
| Settled idle sample duration | 15.0123967 s |
| Measurable CPU-time increase during idle | 0.0 s |
| Idle working set | 69,173,248 bytes (65.97 MiB) |

CPU time is the process kernel+user counter. Zero measured idle increase means
below the counter's resolution during this sample, **not proof of exactly zero
power or CPU use**. GPU/DWM work is not included in process CPU time. The stress
test retargets geometry and a volume model; it does not redraw album art, perform
networking, or continuously query hardware. Test generation at 73 ms intervals
does not measure the display frame rate. No FPS or missed-frame result is claimed.

## Visual verification

Actual native window captures were inspected. Defects discovered and fixed:

- Incorrect DirectComposition sibling insertion order hid content beneath base.
- Missing left/top clip bounds prevented consistent corner rounding.
- Surface updates now explicitly clip drawing within the compositor allocation.
- Bottom helper text required a taller expanded destination.
- Native button rendering was restyled and its corner background cleared.
- QA capture initially raced DWM; a separate delayed capture phase fixed the matte.

Reviewed captures: [island](evidence/island.png), [Animation Lab](evidence/animation-lab.png).
These are real application captures. The background matte is part of QA capture,
not the ordinary island window.

## Not measured / not passed yet

Physical 90/120/144/165/240 Hz displays, integrated-only hardware variation, frame
presentation latency/percentiles, GPU utilization, compositor stalls, battery
drain, multi-day leaks, mixed-DPI multi-monitor, hot-plug, GPU device loss,
supported media players, physical charger toggling, fullscreen games and screen
readers remain unverified. Mathematical tests sample 60/90/120/144/165/240 Hz,
which verifies time-based math only. Native computer-use inspection timed out on
app approval; app-owned captures were used instead. No user action is required
to build or run the provided prototype.

## v0.3 measured on 2026-09-22

Windows 11 on the same laptop. Process counters measured with real media disabled for reproducibility; no fabricated media playback workload was used. Units below are process CPU time and resident working set, not GPU time or measured FPS.

| Scenario | Elapsed | CPU time | One-core equivalent | Working set |
|---|---:|---:|---:|---:|
| Settled compact, 15-second sample | 15.012 s | 0.000 s | 0.00% | 64.19 MiB |
| Visible Home, 15-second sample | 15.001 s | 0.078125 s | 0.52% | 69.40 MiB |
| 200 rapid body reversals and coalesced volume events | 15.934 s | 0.984375 s | 6.18% | 64.20 MiB |

The stress run made 405 compositor commits and 211 surface updates with zero queued activities at completion. The earlier v0.3 iteration repainted the full dashboard on each reversal: 1.75 CPU seconds, 411 surface updates. Header-only redraws reduced this to 0.984375 CPU seconds and 211 updates. This is an observed iteration comparison, not an isolated laboratory benchmark.

A zero CPU delta means below process-counter resolution during this short idle interval; it does not prove zero power use. GPU activity, input-to-photon latency, dropped frames, high-refresh operation, native-glass power impact and multi-day stability were not measured. No live media stress or physical multi-monitor run was performed in this iteration.

Raw evidence: `evidence/v0.3/process-counters.json`, `retarget.json`, `tests.txt`, `ui-test.txt`, `audio-adapter.txt`. Core tests report 11,682 checks with 0.00198515 DIP maximum sampled curve error; dashboard/geometry tests report 456 checks. All three CTest suites pass.

## v0.4 measured on 2026-09-22

Final Windows 11 x64 build. Real media was disabled for reproducible process-counter sampling. CPU percentages below are fractions of one logical core, not whole-system CPU or GPU utilization.

| Scenario | Elapsed | CPU time | One-core equivalent | Working set |
|---|---:|---:|---:|---:|
| idle | 15.006 s | 0.000000 s | 0.00% | 61.82 MiB |
| dashboard | 15.013 s | 0.093750 s | 0.62% | 67.71 MiB |
| compact-timer | 15.007 s | 0.171875 s | 1.15% | 66.26 MiB |
| 200 interrupted retargets | 15.698 s | 1.187500 s | 7.56% | 64.10 MiB |

The stress run recorded 406 compositor commits, 225 surface redraws and zero queued activities at completion. The compact timer now refreshes its header/rings without redrawing the hidden dashboard. Before that change, the recorded timer sample used 0.250000 CPU seconds; this final sample used 0.171875. These short sequential observations are not a controlled performance comparison.

Zero idle CPU delta means below the process counter resolution during this sample, not zero power use. GPU usage, actual FPS/frame pacing, input latency, high-refresh displays, battery drain and multi-day stability remain unmeasured. Original synthetic cover fixtures support the separately reviewed app-only motion captures; the benchmark does not establish live-player artwork performance.

All three CTest suites pass. Core: 11,682 checks; dashboard/models: 1,244 checks. Native regression passes nine stages including app-switch policy. External foreground/fullscreen integration was inconclusive because the QA helper did not obtain foreground activation; see `evidence/v0.4/foreground-integration.json`. Raw counters and captures are in `evidence/v0.4`.

## v0.5 measured on 2026-09-23

Final build on Windows 11. Media is disabled for reproducibility. CPU is process user+kernel time; percentages are one-core equivalents, not total-system or GPU usage.

| Scenario | Elapsed | CPU time | One-core equivalent | Working set |
|---|---:|---:|---:|---:|
| idle | 15.005 s | 0.000000 s | 0.00% | 65.44 MiB |
| dashboard | 15.017 s | 0.046875 s | 0.31% | 72.24 MiB |
| 200 body retargets | 15.840 s | 1.343750 s | 8.48% | 67.57 MiB |

Stress: 405 compositor commits, 225 surface redraws, zero queued activities at completion. Repeated Shell drag-image creation: GDI handles 38 → 38 after initialization across 40 iterations. A real Shell PNG thumbnail was loaded in the provider test.

These short samples do not establish FPS, frame pacing, GPU use, battery drain or long-run stability. The retarget benchmark does not represent live scrubbing, physical headphone reconnection or external file dragging. Browser maximized visibility was recorded by the app; fullscreen entry/exit was not completed after Computer Use stopped for browser-URL confidence. Raw evidence is in `evidence/v0.5`.
## v0.6 measured on 2026-09-23

Final v0.6 executable, Windows 11, real media disabled for repeatability. These are short process-counter observations, not display presentation measurements.

| Scenario | Elapsed | CPU time | One-core equivalent | Working set |
|---|---:|---:|---:|---:|
| Idle compact | 15.004 s | 0.015625 s | 0.10% | 64.64 MiB |
| Static Live card with QA artwork | 15.001 s | 0.000000 s | below counter resolution | 67.17 MiB |
| 200 interrupted body retargets | 16.067 s | 1.218750 s | 7.59% | 67.46 MiB |

The stress run recorded 405 compositor commits, 326 surface redraws, and zero queued activities at completion. The extra content rebuilds support switching between small/large layouts; there is no constant idle animation loop. Comparisons with earlier runs are not controlled benchmarks. No GPU/power-use, high-refresh FPS, input-latency, docking, multi-day or universal-player claim follows from these samples.

Three CTest suites pass, with 11,682 core and 1,831 model/cadence checks. Twelve native interaction stages pass. Cadence math samples 60/90/120/144/165/240 Hz without changing physical time. Final screenshots use original local fixtures; private application content is excluded. Raw evidence is in `evidence/v0.6`.

## v0.7 measured on 2026-09-23

Final v0.7 code on Windows 11 (20 logical processors). Process user+kernel CPU over 15 s after a 3 s warm-up, as one-core equivalents. Not GPU, power or presented-FPS measurements.

| Scenario | CPU time | One-core equivalent | Working set |
|---|---:|---:|---:|
| Compact, media/audio workers disabled | 0.000 s | below counter resolution | 65.0 MiB |
| Compact, all providers running (session paused) | 0.016 s | 0.10% | 83.1 MiB |
| Glass Live card open (session paused) | 0.000 s | below counter resolution | 84.4 MiB |
| Compact waveform, 5 bars fed 100 synthetic frames/s | 0.172 s | 1.14% | 65.9 MiB |
| Media page waveform, 16 bars fed 100 synthetic frames/s | 0.109 s | 0.73% | 66.8 MiB |
| 200 interrupted body retargets (glass off) | 1.219 s / 16.07 s | 7.59% | 67.5 MiB |

Spectrum analysis costs 19 µs per 1024-point step (about 0.2% of one core at 100 steps/s). The waveform rows use the real bar-animation path with generated band levels so no sound had to be played on the speakers; live loopback with real audio was exercised separately (bars followed a playing YouTube session) but its CPU was not isolated. Working set rises by ~18 MiB when the mixer, loopback and brightness workers are running.

Four CTest suites pass: 11,682 core, 1,831 model, 4,502 phase (glass expressions, glides, spectrum, settings model, identity) and provider lifecycles including three start/stop cycles of the new workers. The Settings window end-to-end test passes 94 checks and the native interaction regression passes 12 stages. Raw data: `evidence/v0.7`.

## v0.14 idle measurements

Compact island, media off, measured for 30 s after a 3 s settle on the development PC (20 logical processors). Raw data: `evidence/v0.14`.

| Scenario | CPU time | One-core equivalent | Private memory |
|---|---:|---:|---:|
| Idle glance off | 0.000 s | below counter resolution | 73.5 MB |
| Idle glance on (date, CPU, GPU) | 0.094 s | 0.31% | 76.2 MB |
| v0.13, same conditions | 0.031 s | 0.10% | 62.4 MB |

The extra 11 MB over v0.13 is the soft shadow's window and compositor. Before release, a build that gave the glass and the shadow their own Direct3D devices measured 147 MB; both now share the renderer's device, and their surfaces are drawn on first use.

## v0.15 idle measurements

Compact island with the development PC's own settings (Live Island at 560 DIPs, Frosted glass, compact clock, auto-hide), nothing playing, measured for 30 s after a 6 s settle; the installed v0.14 measured the same way straight after. Raw data: `evidence/v0.15`.

| Build | CPU time | One-core equivalent | Private memory | Threads |
|---|---:|---:|---:|---:|
| v0.15 | 0.000 s | below counter resolution | 78.2 MB | 23 |
| v0.14, same settings | 0.031 s | 0.10% | 77.0 MB | 21 |

v0.15 has two more threads than v0.14, both waiting when idle. One is the brightness setter behind the Controls page's slider, which waits until a level is set. Sharing, weather, site icons and the adaptive-text wallpaper reader start no threads until they are turned on. The weather sky and the edge light are compositor animations, so they cost the app no CPU while they play; the sky plays for 60 s at a time and then rests, so an open Home page doesn't keep the compositor busy.

## v0.16 idle measurements

Same conditions as v0.15 (the development PC's own settings, compact, nothing playing, 30 s after a 6 s settle); the installed v0.15 measured straight after. Raw data: `evidence/v0.16`.

| Build | CPU time | One-core equivalent | Private memory | Threads |
|---|---:|---:|---:|---:|
| v0.16, first run | 0.047 s | 0.16% | 77.3 MB | 27 |
| v0.16, second run | 0.031 s | 0.10% | 78.8 MB | 27 |
| v0.15, same settings | 0.016 s | 0.05% | 77.8 MB | 26 |

The differences are one to three of Windows' 15.6 ms scheduler ticks over 30 seconds. The extra thread is the music library's worker, which waits until the library is first used; the player itself starts only with the first song. The halo, the bud and the liquid pill are compositor animations and cost the app nothing while they play.


## v0.17 idle measurements

The installed copy, with the development PC's own settings: compact, nothing playing, 25 s after launch, 30 s windows. v0.16.0-preview.2 was run from the same folder a few minutes earlier, so both used the same firewall rule and settings. The settings file was byte-identical afterwards.

| Build | CPU time | One-core equivalent | Private memory | Threads | UI thread |
|---|---:|---:|---:|---:|---:|
| v0.17, first window | 0.156 s | 0.52% | 81.0 MB | 28 | 125 ms |
| v0.17, second window | 0.250 s | 0.83% | 80.2 MB | 25 | 141 ms |
| v0.16, same session | 0.141 s | 0.47% | 80.4 MB | 28 | 109 ms |

Today this PC's baseline is higher for both builds than when v0.16 was first measured (0.031 s): the compact island redraws its glance each second. The two builds are within one or two of Windows' 15.6 ms scheduler ticks per second of work on the UI thread. v0.17 adds no work while idle:
- the player's crossfade timer runs only while the island's own music plays
- Up next's glide timer runs only while a row moves
- the screen-reader tree is built only when a screen reader asks for it
