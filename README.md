# Arnav Island

A native Windows 11 desktop island with continuous spring motion and a quieter home for everyday controls.

![Arnav Island overview](docs/evidence/v0.2/expanded.png)

**v0.2 is a downloadable preview, not a production-quality guarantee.** This release replaces the original Nexus Island prototype's single panel with five useful views. No account, paid service, browser engine, cloud infrastructure, driver, or administrator access is needed.

## Download and run

Get the Windows x64 ZIP from [Releases](https://github.com/Arnav-Dugad/arnav-island/releases). Extract it and run `ArnavIsland.exe`. The executable is currently unsigned. Windows may show its normal reputation prompt. ARM64 and other configurations have not been validated.

Hover over the pill to open it. Leave to close it, or press **Pin** to keep it open. Hover opening and its delay are configurable in Settings. Click the pill to open or close it. Right-click the island or tray icon for the Animation Lab, diagnostics, or Exit.

## What's inside

- **Overview:** local time/date, battery and power status, current media artwork, CPU and memory, network traffic, volume and mute.
- **Media:** different music and video layouts, artwork, title/artist, playback controls and a timeline when supplied by the player. Choose Auto, Music, or Video layout.
- **System:** CPU history, used/total RAM, network upload/download, disk space, processor count and uptime. Direct links open Windows sound, display, network and Bluetooth settings.
- **Focus:** 25-minute focus timer, 5-minute break, stopwatch, pause and reset.
- **Settings:** hover behavior, five spring presets, reduced motion and fullscreen hiding. Preferences save locally.

Thumbnails and controls use Windows' public media-session API. YouTube, streaming services and music applications work to the extent that their player/browser exposes this information. We do not claim universal Netflix, JioHotstar, Prime Video or Apple TV compatibility. Unknown browser content is not guessed; the layout override is available for that case. No scraping or streaming-service sign-in is used.

## Build and verify

```powershell
.\scripts\build.ps1 -Test
.\build\ArnavIsland.exe --lab --expanded
.\build\ArnavIsland.exe --ui-test --capture-safe
.\build\ArnavIsland.exe --benchmark
.\scripts\package.ps1
```

Validated here with MinGW-w64 GCC 16.2 and CMake on Windows 11. C++23, Win32, DirectComposition, Direct2D and DirectWrite; compiler runtimes are statically linked. MSVC support is intended but untested on this machine. There is no per-frame application rendering loop.

[Report](docs/REPORT.md) · [Feature status](docs/FEATURE_MATRIX.md) · [Architecture](docs/ARCHITECTURE.md) · [Motion](docs/ANIMATION_SYSTEM.md) · [Measured performance](docs/PERFORMANCE_RESULTS.md) · [Privacy](docs/PRIVACY.md) · [Future ideas](docs/FUTURE_IDEAS.md)
