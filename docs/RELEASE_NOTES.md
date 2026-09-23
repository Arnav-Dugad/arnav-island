# Arnav Island 0.7.0-preview.1 — Glass, sound and a real Settings window

Phase 2 of the delivery plan (audio and media), plus a separate Settings window and a true glass material.

## Glass
- **Material: Solid, Frosted glass or Clear glass.** Glass blurs whatever is behind the island using Windows' own host backdrop, with a tint, a soft top sheen and a light-catching rim. The blur morphs with the island at the display's refresh rate: its rounded shape is evaluated by the compositor from the same spring equations as the island body.
- Glass islands float 8 px from the screen edge, like a physical island. **Glass tint** controls contrast.
- Blur requires Windows **Settings → Personalization → Colors → Transparency effects**. When that is off (or in battery saver/high contrast) the glass stays tinted but not blurred, matching how Windows treats its own materials. The Settings window links straight to that page.

## Settings window
- Preferences moved out of the island into their own window: General, Island, Appearance, Motion, Compact, Media & sound, Home & navigation and About.
- **Every change applies instantly** — drag a slider and the island moves, resizes or restyles while you drag. Changes save automatically.
- Animated toggles, sliding segmented selectors, sliders, steppers and accent swatches, rendered on a vsync-paced swap chain so motion follows the monitor's refresh rate (60–240 Hz). Mica backdrop when Windows allows it. Keyboard: Tab, arrows, Space/Enter.
- Destructive actions (reset, clear logs) ask for a second click.
- Open it from the island's gear, the Settings item in the navigation bar, the tray menu, or by double-clicking the tray icon.

## Sound and media
- **Every media session, not just one.** When several players are active (for example Spotify and a browser), swipe horizontally on the Live card or Media page — drag, or two-finger swipe on a touchpad — or tap the app chips/dots. "Follow the active player" keeps the island on whatever Windows marks as current.
- **Real app logos.** The badge on the artwork is the playing app's own icon, read from Windows (Start menu entry or running executable) — nothing is bundled. Browser tabs playing YouTube or YouTube Music show those services' marks when the browser's window title confirms it; otherwise the browser's logo is shown.
- **Live waveform from real system audio.** Bars in the compact island, Live card and Media page follow the actual output through WASAPI loopback and an FFT. Silence rests; nothing is simulated. Audio is analyzed in memory and discarded.
- **Per-app volume mixer.** Audio → Apps lists every application using the default output with its real icon, a draggable volume slider, mute, and a live peak meter.
- **Volume and brightness indicator.** Changing volume or display brightness grows the resting island into a compact level bar. Brightness comes from the documented WMI monitor brightness events (internal panels).

## Everywhere
- Intent-aware hover: a pointer sweeping past no longer opens the island; resting on it does.
- Session changes slide the content in the swipe direction; the artwork cross-fades.
- Fixed: Codex's v0.6 "installed with startup enabled" had only happened inside Codex's own sandbox. This release is installed and registered for sign-in on the real Windows account.

Windows 11 x64, unsigned preview. Not yet covered: Bluetooth device cards, battery-health dashboard, clipboard shelf, privacy indicators, commands and workspace presets (later phases in DELIVERY_PHASES.md). Service marks are only YouTube/YouTube Music; other web services show the browser. Protected (DRM) audio can appear silent to loopback analysis.
