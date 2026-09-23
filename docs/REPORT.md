# Arnav Island v0.5 development report

The primary correction is a fullscreen false positive: screen-covering geometry alone did not distinguish maximized browser windows from borderless fullscreen. The policy now excludes decorated and maximized windows and restores the topmost island without stealing focus. The exact browser/user environment still requires the native acceptance results recorded below.

The release adds spring-expanded precision seeking, background Shell thumbnails/file icons, native drag images with a compositor absorption tile, and audio-route confirmation feedback. The renderer keeps geometry animation on DirectComposition. Scrub input changes preview state immediately and sends one supported seek request on release. Current media identity is checked before applying it.

## Defects found during testing

- Windows Shell parsing rejected a mixed-separator QA path; extraction now uses native separators.
- The initial seeking hint overlapped large video artwork; it is positioned in the metadata column below the controls.
- Drag-image bitmap lifetime needed explicit cleanup. The regression warms one-time Shell resources before comparing repeated handle usage; it does not confuse initialization with sustained leakage.
- Audio feedback originally animated every output row; it now targets the confirmed row and the Audio navigation icon.

## Validation

Core, dashboard and real provider lifecycle suites are run for the final build. Native regression covers previous hover/navigation/shelf/layout behavior and precision-seek cancellation. Thumbnail extraction and 40 repeated drag-image/data-object operations are checked with GDI resource counters. Original QA artwork is used in shareable captures. No private browser screenshots are included in the repository or release.

Final measurements, browser observations, executable checksums, installation and publication evidence are appended after verification. No universal-player, zero-defect, high-refresh or multi-day claim is made.

## v0.5 acceptance observations

- All three CTest suites pass. Dashboard/model checks: 1,256. Core math retains 11,682 checks.
- Native interaction regression passes ten stages, including fine seeking, cancellation and timeline hit targets.
- Shell thumbnail extraction succeeds for the local PNG fixture. After Shell initialization, 40 drag-image/data roundtrips hold GDI resources at 38 → 38. This checks repeated resource allocation; it is not a successful external drag-and-drop session test.
- Reviewed actual native seek and shelf captures in `evidence/v0.5` show the moved seek hint and the loaded file thumbnail/icon.
- With the browser activated, the app-owned audit reports `hidden=0`, `decorated=1`, `maximized=1`, `shell=0`. It records no app title, URL or window content.
- Computer Use ended because browser URL detection could not enforce its policy confidently. No further browser inputs were issued. Actual fullscreen entry/exit, external app-to-app drag sessions and physical headphone route changes remain unverified. Native gesture/model/provider tests are reported separately from those acceptance gaps.
