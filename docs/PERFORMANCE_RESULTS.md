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
