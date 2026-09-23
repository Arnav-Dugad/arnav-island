# Arnav Island 0.5.0-preview.1

## Browser visibility fix

Maximized or decorated windows no longer count as fullscreen merely because their visible bounds reach all monitor edges. This prevents a normal browser from hiding the island, including monitor-sized windows and auto-hidden-taskbar layouts. Normal foreground changes also restore the island's topmost position without activating it. Borderless, non-maximized, monitor-covering windows still follow Hide in fullscreen.

## Interaction refinements

- **Precision scrubbing:** the media timeline grows with a spring on hover/drag. Pull more than 35 DIPs away for reduced sensitivity, or 70 for fine control. Release sends one seek request; Escape/capture loss cancels. The provider verifies current source/title and supported seek bounds. Unsupported players remain read-only. Playback progression runs as a compositor animation between snapshots.
- **Shelf previews:** Windows Shell thumbnails or associated file icons load on a background worker, bounded to 32 entries and 128-pixel requests. Cloud placeholders marked offline/recall are skipped. Document page previews depend on installed Windows thumbnail handlers.
- **Drag absorption:** the native Shell drag image remains available over the island, then a preview tile springs toward the shelf row. When the thumbnail is not yet ready, a file glyph is used. Dragging out uses the cached thumbnail as a Shell drag image; original files remain untouched and operations remain copy-only.
- **Confirmed audio handoff:** a brief compact output activity and restrained pulse appear only after the default endpoint changes. Startup enumeration and repeated volume events stay quiet. The selected output icon receives a small spring response.

## Boundaries

Windows 11 x64, unsigned preview. No cloud, subscriptions or drivers. Media seeking depends on Windows-exposed capabilities; no service-specific scraping or private video frame capture. Shell thumbnail handlers can be slow; isolation from faulty third-party handlers and full accessibility/high-refresh/multi-monitor/long-duration acceptance remain unfinished.
