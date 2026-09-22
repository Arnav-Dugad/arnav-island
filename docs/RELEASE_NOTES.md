# Arnav Island v0.4.0-preview.1

A substantial visual and interaction redesign.

## New
- Replaced character-based interface icons with an original 35-symbol vector library. Consistent stroke weight, centered control labels, aligned cards and a compact labelled navigation bar.
- Interactive icon scale/lift springs and a moving selected-navigation indicator. Motion is interruptible and respects reduced motion and the Animated icons preference.
- Track artwork blends through two retained compositor surfaces. A new track arriving during a blend starts from the visible composite, avoiding a flash back to the old cover. Artwork still glides between compact/Home/Media positions.
- Personal layout controls: choose a navigation item, move it left/right, choose three distinct Home metrics, and reset the layout independently.
- Battery/timer glance rings with native animated endpoint markers. Battery, Timer, Both and Off options; Focus also has a larger progress ring.
- True pointer-drag volume slider with Left/Right/Home/End keyboard control when focused.
- Eight preference groups and settings format v4 with migration from v1–v3.
- Expanded text visibility follows the body's physical geometry, preventing overlap with traveling artwork during fast reversals. Settled text positions snap to physical pixels at fractional scale.
- Corrected the diagnostic label for a connected media manager without an active session.

## Use
Open Preferences → Personal layout to reorder navigation and select Home statistics. Preferences → Fine details controls animated icons, handoff and glance rings. The compact body remains 196 × 34 DIPs by default; the expanded panel is 420 × 334 DIPs with room for labelled controls.

## Scope
Windows 11 x64. Media artwork still depends on OS-exposed player metadata; no service scraping or video capture is used. Ring arcs update with actual battery/timer snapshots, while endpoint markers animate independently on the compositor. No artificial perpetual icon or ring animation runs at idle.

The file shelf holds in-memory path/text references with filename previews and copy-only drag-out. Audio default switching remains an optional isolated compatibility interface with a Windows sound-settings fallback. Existing startup and appearance preferences are preserved on upgrade.

This is an unsigned preview, not a certification for deployment to hundreds of millions of machines. Full UI Automation, text-scale/high-contrast coverage, device-loss recovery, mixed-monitor/high-refresh validation and extended soak testing remain release gates.

## Multitasking polish
- Configurable spring collapse when switching to another app, preserving page, timer and shelf state. Pinning and active drags are respected.
- Foreground size-change events detect entering fullscreen without an app switch. DWM visible bounds avoid treating invisible resize borders as fullscreen content.
- Slider-only volume scrolling by default; an optional preference restores scrolling anywhere on the island.
