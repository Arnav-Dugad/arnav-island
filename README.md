# Arnav Island

A clean, native Windows 11 island for media, useful system statistics and focus.

**[Download the Windows x64 preview](https://github.com/Arnav-Dugad/arnav-island/releases)**

![Arnav Island Overview](images/overview.png)

Extract the ZIP and run **ArnavIsland.exe**. Hover to open, leave to close, or pin it. Settings control hover delay, motion and fullscreen behavior. No account, cloud backend, subscription, paid API or installer is required.

Five views:
- **Overview:** clock/date, current media, battery/power, CPU, memory, network and volume/mute.
- **Media:** player-provided thumbnails and controls, timeline, separate music/video layouts with a manual override.
- **System:** CPU history, RAM, disk space, upload/download, uptime and links to Windows settings.
- **Focus:** 25-minute focus timer, 5-minute break and stopwatch.
- **Settings:** hover preferences, five spring presets, reduced motion and fullscreen hiding.

The app is native C++23 with Win32, DirectComposition, Direct2D and DirectWrite. Geometry is animated by the Windows compositor. Monitoring sleeps when the relevant panels are closed. Private media stays in memory and is never uploaded or logged.

**This is an unsigned preview.** Complete screen-reader/high-contrast support, mixed-DPI and high-refresh acceptance, device-loss recovery and multi-day reliability remain unfinished. Media integrations depend on what the player exposes to Windows; universal support for Netflix, JioHotstar, Prime Video, Apple TV or every browser is not claimed. The screenshots intentionally show an empty media state for privacy.

Read [Quick Start](QUICK_START.md), [Release notes](RELEASE_NOTES.md), and [Future ideas](FUTURE_IDEAS.md). This repository hosts portable downloads, release notes and the public roadmap.

