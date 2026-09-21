# Arnav Island v0.2 development report

## Delivered

The application is now named **Arnav Island**. A native five-view dashboard replaces the original single-panel prototype: Overview, Media, System, Focus and Settings. The visual treatment uses restrained monochrome surfaces, clearer type hierarchy, consistent rounded controls and more breathing room. The original compositor spring engine remains responsible for the persistent body, size and corner morphs.

Hover opening is on by default with a configurable delay. Leaving closes the island after 650 ms; Pin keeps it open. Controls now expose real actions: media transport, volume/mute, page selection, timers and Windows settings links. Tab/Shift+Tab and Enter/Space are supported after activation. This is not yet a complete accessible surface.

Media uses Windows GSMTC metadata and WIC thumbnail decoding on the provider worker. Music and video have different layouts. Playback type drives Auto, with a manual override for ambiguous browser sessions. The timeline uses reported position/duration and extrapolates only while playing. Unsupported transport actions are disabled. A real active media session supplied artwork and playback state during this run; its private title and thumbnail were not included in the release evidence. Named streaming services have not all been individually tested.

The System provider reports OS CPU, RAM, network traffic, disk space, logical processor count and uptime. It samples once per second only while Overview/System is visible, after a 400 ms activation delay. Disk space refreshes less often. A first stress run revealed repeated wakeups during fast reversals; delaying monitoring reduced observed process CPU time from 1.35938 s to 0.28125 s for approximately 16 seconds of stress. The first sample also included a QA capture, so this is a practical iteration comparison, not a controlled scientific benchmark.

Focus has a 25-minute timer, 5-minute break and stopwatch with pause/reset. Countdown state uses elapsed time rather than counting frames. Settings v2 migrates old preferences by copy, saves atomically, and retains the legacy files. All operation remains local.

## Validation

- Native Release build completed with installed GCC/MinGW; no new paid dependency or runtime.
- Core spring/orchestration tests, real audio/media/system provider lifecycle tests, and new dashboard/timer/settings tests pass.
- Native interaction regression exercises hover open, leave close, disabled hover, navigation, timer actions and hit targets. The test moves the cursor and restores it; external pointer movement can interfere.
- Actual native Overview, Media empty state, System, Focus and Settings captures inspected. Release images explicitly disable the media provider so private playback is excluded.
- Actual active-session thumbnail rendering inspected locally. No unsupported per-service compatibility claim is made.
- Repeatable retarget stress and idle/dashboard process counters recorded in PERFORMANCE_RESULTS.md. No unmeasured FPS, GPU-use or power claim.

## Release scope and remaining work

This is a **public preview**, not completed Milestone 1 or an Apple-quality certification. It is substantially more useful and readable than v0.1, but shared-element artwork motion, deeply animated microinteractions, native acrylic, a complete UI Automation tree, high-contrast/text-scale acceptance, high-refresh capture and long-running resilience still need work.

The window now uses a small moving conservative hit envelope instead of covering the entire canvas during motion. Updating that Win32 region requires a 30 ms timer only while moving; compositor animation itself is independent of that timer. A small area beyond the silhouette can still intercept input during motion. GPU device loss still closes the app instead of rebuilding the device. Hardware temperatures, invasive OEM controls, file shelf, clipboard, external notifications and arbitrary plugins remain absent.

The distribution is a portable, unsigned Windows x64 ZIP with license and notices. Source, safe screenshots, measured evidence and this report are intended for the requested GitHub release. Runtime logs, local settings, private media captures and unrelated Desktop files are excluded. See QUICK_START.md for use and FUTURE_IDEAS.md for the next design candidates.
