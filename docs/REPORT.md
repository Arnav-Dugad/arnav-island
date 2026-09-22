# Arnav Island v0.3 development report

## Result

The new default is a smaller top-center island with the reference's curved edge connection. Right-edge docking remains optional. The project continues to use native C++23/Win32/DirectComposition; the UsageNotch folder was read as a visual reference, without copying its code or assets.

The compact body is 196 × 34 DIPs and the expanded dashboard is 420 × 300 DIPs, with configurable scale. Home, Media, Stats, Focus, Shelf, Audio and Preferences use restrained spacing and small controls. Preferences now contains five groups of controls. The installed app has a Desktop-root shortcut and configurable per-user startup.

The persistent artwork visual moves between compact, Home and Media positions with velocity-preserving spring curves. Palette extraction runs on the media worker. Volume, charging, timer and track activity appear while compact. Hover opening/closing, light/dark/system themes, monitor, edge, width, corner, scale, motion and accent controls are saved locally.

The shelf accepts files and Unicode text through OLE, holds up to 32 entries, and supports copy-only drag-out. Clear forgets entries and never touches originals. File rows currently show names, not decoded image/file thumbnails. The island responds on entry; pre-entry drag attraction and shell-image absorption are future work.

Audio outputs come from real endpoint enumeration. Optional switching uses an isolated compatibility adapter because Windows does not expose a documented system-default setter. Reselecting the currently selected endpoint succeeded on this laptop. Switching between physical headphones and speakers was not tested. Users can disable compatibility switching and open Windows sound settings.

Native DWM acrylic required a correction after real UI inspection: its backdrop did not respect the curved window region and spilled outside it. The final material host is confined to the panel interior, with an opaque outer silhouette. Another correction expanded the native input mask by one pixel to avoid cutting away antialiased edge pixels. Material is disabled during body motion, in battery saver/high contrast or when explicitly turned off.

## Validation and evidence

The final build passes CTest's core physics/orchestrator, provider lifecycle/OLE and dashboard/settings/geometry suites. Native interaction regression covers hover, collapse, disabled hover, page navigation, timers, hit targets, scale/theme/right-edge changes and shelf drop/clear. Audio compatibility reselection returned S_OK. Evidence is under `docs/evidence/v0.3`.

Actual native captures cover compact, Home, light theme, right edge, shelf, audio, settings, media artwork, video layout and 110% application scale. Captures disable real media; the artwork study is explicitly labelled original local QA artwork. No private media artwork, local settings or logs are included.

Performance measurements are in PERFORMANCE_RESULTS.md. They are process counters, not a measured FPS, GPU utilization or perceptual-quality certification. No high-refresh hardware or multi-day test was performed. This is a preview, not a claim that the entire original fifty-part specification is complete.

## Remaining limitations

Full accessibility/UI Automation, text scaling, per-display profiles, import/export, provider retries/device-loss restoration, thumbnail previews for shelved files, true shell-image absorption, track crossfades and broader hardware/player validation remain unfinished. No private streaming-service thumbnails are scraped or captured. Windows media metadata determines availability. No unsafe ROG/ACPI code, kernel drivers, analytics, cloud services or paid dependency has been introduced.

Public distribution remains separate from private source history. Publication and installation verification are appended after the actual upload and local installation.

## Publication and local installation verified

- Public release: https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.3.0-preview.1
- Source implementation checkpoint: `80f499e`, pushed to the private development repository, tagged `v0.3.0-preview.1`.
- The public ZIP was downloaded without credentials and SHA-256 matched: `3806342d4a9c358af32af5922b5b0aec0e876d5ede9dd7f6de5b1138cdba3935`.
- The installed executable matches the tested build: `a0da094aba596c103ce8bb30068ed92c113a77c79184efd3f901656fc1ca5fa1`.
- Installed at Desktop/Arnav Island/app; Desktop-root `Arnav Island.lnk` points to this copy. It was verified running and responsive.
- Per-user HKCU Run points to the quoted installed executable with `--startup`; this was read back and verified. An actual reboot/sign-in cycle was not performed.
- The obsolete Desktop/Nexus Island folder was clean, its Git history was preserved, and its only ignored files were the known app and shortcuts. It was sent to the Recycle Bin. UsageNotch was retained.
- Verification records: `evidence/v0.3/public-verification.json` and `cleanup.json`.
