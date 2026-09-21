# Measured performance — 2026-09-21

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
