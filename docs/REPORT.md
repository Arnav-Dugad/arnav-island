# Arnav Island v0.5 development report

The primary correction is a fullscreen false positive: screen-covering geometry alone did not distinguish maximized browser windows from borderless fullscreen. The policy now excludes decorated and maximized windows and restores the topmost island without stealing focus. The maximized browser observation and remaining acceptance gaps are recorded below.

The release adds spring-expanded precision seeking, background Shell thumbnails/file icons, native drag images with a compositor absorption tile, and audio-route confirmation feedback. The renderer keeps geometry animation on DirectComposition. Scrub input changes preview state immediately and sends one supported seek request on release. Current media identity is checked before applying it.

## Defects found during testing

- Windows Shell parsing rejected a mixed-separator QA path; extraction now uses native separators.
- The initial seeking hint overlapped large video artwork; it is positioned in the metadata column below the controls.
- Drag-image bitmap lifetime needed explicit cleanup. The regression warms one-time Shell resources before comparing repeated handle usage; it does not confuse initialization with sustained leakage.
- Audio feedback originally animated every output row; it now targets the confirmed row and the Audio navigation icon.

## Validation

Core, dashboard and real provider lifecycle suites are run for the final build. Native regression covers previous hover/navigation/shelf/layout behavior and precision-seek cancellation. Thumbnail extraction and 40 repeated drag-image/data-object operations are checked with GDI resource counters. Original QA artwork is used in shareable captures. No private browser screenshots are included in the repository or release.

Final measurements are recorded in PERFORMANCE_RESULTS.md; build and publication checksums are in evidence/v0.5. No universal-player, zero-defect, high-refresh or multi-day claim is made.

## v0.5 acceptance observations

- All three CTest suites pass. Dashboard/model checks: 1,256. Core math retains 11,682 checks.
- Native interaction regression passes ten stages, including fine seeking, cancellation and timeline hit targets.
- Shell thumbnail extraction succeeds for the local PNG fixture. After Shell initialization, 40 drag-image/data roundtrips hold GDI resources at 38 → 38. This checks repeated resource allocation; it is not a successful external drag-and-drop session test.
- Reviewed actual native seek and shelf captures in `evidence/v0.5` show the moved seek hint and the loaded file thumbnail/icon.
- With the browser activated, the app-owned audit reports `hidden=0`, `decorated=1`, `maximized=1`, `shell=0`. It records no app title, URL or window content.
- Computer Use ended because browser URL detection could not enforce its policy confidently. No further browser inputs were issued. Actual fullscreen entry/exit, external app-to-app drag sessions and physical headphone route changes remain unverified. Native gesture/model/provider tests are reported separately from those acceptance gaps.

## Publication and laptop update — 2026-09-23

- Private source implementation commit: `f812e69`; public distribution commit: `1821cc7`. Both repositories have the `v0.5.0-preview.1` tag.
- Published the [public v0.5 preview release](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.5.0-preview.1) with the Windows x64 archive and SHA-256 file.
- Downloaded the public asset anonymously and verified the archive and unpacked executable against the tested local build before installation.
- Updated the existing Desktop app and source clone. The installed app reports `0.5.0-preview.1`, is running and responding, and matches the tested executable SHA-256. The Desktop shortcut points to this installation.
- Settings file hash and startup registry value are unchanged; launch at login remains enabled. Recent startup log entries are informational with no error in the inspected startup tail; there was no active media session at this check.
- Exact hashes and installation checks are in `evidence/v0.5/public-verification.json`. This remains an unsigned preview with the acceptance gaps above.
