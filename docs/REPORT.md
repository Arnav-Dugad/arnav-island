# Arnav Island v0.4 development report

## Delivered

The interface has been reworked around original vector icons, consistent control alignment, physical icon feedback, labelled navigation and a clear title/task/statistics hierarchy. The compact body remains 196 × 34 DIPs; the expanded panel is 420 × 334 DIPs. The expanded height makes space for legible navigation labels rather than squeezing icons into the old button row.

Track handoff now uses two retained artwork surfaces. If another track arrives before the blend finishes, the current visible composite becomes the outgoing image. The new blend begins without reverting to an earlier cover. A shared parent still moves the artwork between compact, Home and Media destinations. Buffer storage is bounded.

Personal layouts are implemented: all seven navigation items can be reordered, three distinct Home statistics can be chosen, and Reset layout is separate from resetting preferences. Settings v4 migrates v1–v3 and validates layout permutations. Hit testing follows moving navigation icons rather than their eventual positions.

Glance rings show real battery and focus-clock progress, with compositor-driven marker rotation. They can show battery, timer, both or neither. The Focus surface also has a larger ring. Interactive navigation/media/audio/shelf icons have physical hover and press responses; static information symbols do not animate continuously. The new volume bar supports pointer dragging and keyboard adjustment.

## Defects found and corrected during native inspection

A rapid expansion reversal allowed artwork to cross fading labels. Expanded text and headers are now gated by the actual body geometry; captures show the cover traveling through an uncluttered surface. Fractional content offsets also kept text slightly blurred after settling. Stable offsets now snap to physical pixels while moving transforms remain continuous.

The previous media log labelled an empty but connected media session manager as unavailable. This diagnostic classification is corrected. Disabled primary playback controls now use a visibly muted surface.

## Validation

CTest covers existing physics/event orchestration and provider lifecycle, plus new interrupted artwork composites, buffer bounds, layout validation/migration, unique Home metrics, ring ranges, slider mapping and geometry-gated visibility. Native regression covers hover/collapse, navigation, timers, shelf, scale/theme/edge changes, layout reorder, moving hit targets, chosen statistics and detail toggles.

Native UI captures and a ten-frame app-only motion study are stored in `docs/evidence/v0.4`. The study exercises rapid cover changes, press response and collapse/expand interruption. Original synthetic QA artwork is explicitly labelled; no private playback image is published. Captures are sparse observations, not a video frame-pacing benchmark.

Measured process-counter results are recorded in PERFORMANCE_RESULTS.md. No FPS, GPU-use, latency, high-refresh or multi-day result is invented.

## Release limits

This remains a preview. A standard suitable for hundreds of millions of installations needs considerably broader hardware, accessibility, reliability and deployment validation than one laptop can establish. Complete UI Automation/text scaling, high-contrast acceptance, device-loss recovery, mixed-DPI/hot-plug/high-refresh tests, automatic update/signing infrastructure and long soak testing remain unfinished.

The existing file shelf, audio-output compatibility boundary, privacy policy and local-only operation remain in place. File content thumbnails, per-service media guarantees and invasive hardware integrations are not claimed. Publication and local installation verification will be appended after completion.
## v0.4 multitasking refinement

Unpinned panels settle back into the compact island when another application takes the foreground. The current page, timer and shelf remain intact. Pinning a panel keeps it open; active pointer and file-drop gestures are protected. Preferences → Multitasking can disable this behavior.

Volume scrolling now targets the volume slider by default, preventing accidental changes when scrolling over navigation or statistics. The previous anywhere-on-island behavior is available as an explicit preference. Shelf and output lists keep their own scrolling.

Fullscreen detection observes foreground changes and debounced foreground-window geometry events, so entering fullscreen in the same player is detected. It uses DWM visible frame bounds rather than invisible resize borders. All monitoring is local and no app titles, paths or content are collected. Windows that do not expose valid bounds are not guessed to be fullscreen.

The final multitasking checks pass all 16 dismissal-policy combinations plus native collapse/pin/drop checks. An external-window fullscreen attempt was inconclusive because the QA helper did not obtain foreground activation; it is not recorded as a successful end-to-end player/fullscreen test. Geometry and lifecycle checks pass; real game/player transitions remain an acceptance task.
