# Arnav Island v0.3.0-preview.1

Smaller, quieter, connected to the display edge.

## Changes
- Top-center curved shoulders; optional right-edge docking.
- Compact body reduced to 196 × 34 DIPs. Expanded panel is 420 × 300 DIPs.
- Seven focused views with restrained typography and small controls.
- One retained artwork visual glides between compact, Home and Media destinations. Album-derived accents are computed off the UI thread.
- Copy-only OLE file/text shelf with drag-out, deduplication, scroll and clear. Nothing moves or deletes originals.
- Event-driven audio output enumeration and optional direct switching. Windows sound settings remain available.
- Charging pulse, compact volume/media/timer information, subtle magnetic hover/press highlight and spring content offset.
- Dark/light/system themes; scale, width, corner, edge, monitor, offsets, hover/close delays, motion presets, reduced motion, accents, compact activity and glass controls.
- Per-user sign-in startup, configurable from Preferences → Behavior.
- Native Windows acrylic is confined to the expanded content interior. Opaque fallback is used when disabled, unavailable, in high contrast or battery saver.

## Compatibility and scope
Windows 11 x64. Media artwork depends on what the player exposes through Windows media sessions: protected services and browsers can omit it. No browser scraping, video capture or DRM bypass is used.

Direct output switching is an isolated, optional undocumented Windows compatibility interface; it can be disabled in Activities preferences. A failed switch does not claim success, and the Windows sound-settings button remains available. Communications routing is not deliberately changed.

Shelf entries live in memory and disappear on exit; file rows preview names, not file contents. Image files can be held as file references; bitmap and virtual cloud-file drag formats are not supported. The island reacts on drag entry, not before the pointer reaches its region.

The preview is unsigned. Full UI Automation/text scaling, high-refresh hardware validation, mixed-DPI hot-plug, long-duration reliability and device-loss recovery remain unfinished. No measured smoothness guarantee or universal player compatibility is claimed.
