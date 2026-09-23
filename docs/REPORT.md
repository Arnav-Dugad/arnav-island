# Arnav Island v0.7 — development report

## What changed

**Settings became a window.** All 48 persisted preferences live in a separate Settings window (eight sections) running on its own UI thread. Edits reach the island immediately: sliders reposition and resize it while you drag, materials and themes restyle it, display/edge/scale changes rebuild only what they must. Preferences save 350 ms after the last change. The window's animations run on a vsync-paced swap chain, so they follow the monitor's refresh rate. The in-island settings pages were removed; the gear and the Settings navigation item open the window.

**Glass.** Frosted and Clear glass blur what is behind the island through Windows' host backdrop brush, with tint, sheen and rim. The blur layer is a Windows.UI.Composition tree under the existing DirectComposition tree on the same window. Its rounded shape is driven by compositor expressions that evaluate the same spring equations as the body, so it morphs in step at display cadence without any per-frame work in the app.

**Phase 2 (audio and media).** Every Windows media session is available with swipe/drag/touchpad/tap selection; the playing app's own icon badges the artwork; YouTube and YouTube Music are identified only when the browser's window title confirms them; the waveform follows real loopback audio; Audio → Apps is a per-app mixer with live meters; volume and brightness changes grow the resting island into a level bar. Hover opening became intent-aware.

## Verification

- **Settings end-to-end test (`--settings-test`): 94/94 checks pass.** It clicks every control in the real Settings window through its pointer path, waits for the island, and verifies behavior, not just stored values: compact width, window position for edge/offsets/glass gap, DPI for scale, theme, glass visibility and tint, motion preset, reduced motion, the real hover-open path, accent colour, sign-in request, navigation order, unique statistics, Animation Lab, reset layout, reset all, file persistence, and keyboard Tab/Space.
- The test found and fixed a real bug on its first run: releasing mouse capture cleared the pressed control before the click was evaluated, so clicks would never have registered.
- Unit suites: 11,682 core; 1,831 model; **4,502 new** checks: generated glass expression text is parsed and evaluated and matches `Spring::sample` in all damping regimes; Hermite glides keep position and velocity continuous; 110 Hz/1 kHz/6 kHz tones land in their bands, silence rests, noise stays bounded; every persisted key is reachable from the Settings model and round-trips; v5 files migrate.
- Provider lifecycle on this laptop: mixer found 3 sources, loopback capture available, panel brightness read as 100%, AppsFolder resolved a real icon; three start/stop cycles.
- Native interaction regression: 12/12 stages.
- Real-device checks on this laptop: two Edge media sessions were listed with Edge's own icon, the YouTube tab was identified, the waveform followed the playing video, the mixer listed Edge and System sounds with real icons, and the brightness indicator showed the real panel value.
- Glass: with Transparency effects temporarily switched on (then restored to off), captures show real blur of the content behind the island. A prototype measured the glass edge within 2 px of the DirectComposition body at ~3.5 px/ms.

Screenshots in `evidence/v0.7` use synthetic sessions and an app-owned backdrop; captures showing your desktop were used only for local checks and are not published.

## Found along the way

Codex's v0.6 report said the app was installed with sign-in startup enabled. On the real account, `HKCU\...\Run` had no Arnav Island entry and `%LOCALAPPDATA%\ArnavIsland` did not exist: Codex ran inside its own packaged-app sandbox, which virtualized both. v0.7 is installed to `Desktop\Arnav Island\app`, registered for sign-in on the real account, and running.

Your Windows **Transparency effects** setting is off, so glass currently shows as tinted rather than blurred. Turning it on (Settings → Appearance has a button) enables the blur.

## Limits

- Custom Settings controls are keyboard-accessible but do not yet expose UI Automation for screen readers.
- Only YouTube and YouTube Music get service marks; other web services show the browser's logo. A background tab cannot be identified.
- Loopback cannot analyze protected or exclusive-mode audio; such playback rests.
- The mixer covers the default output device only.
- Brightness needs a panel exposing the WMI brightness class (typical for laptop screens).
- Presented FPS at 120–240 Hz, GPU/power use and long-run stability were not measured.
- Unsigned preview.

## Next

Phase 3: Bluetooth device cards, headphone arrival, battery health and the charging redesign. See DELIVERY_PHASES.md.

## Publication and installation — 2026-09-23

Published [v0.7.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.7.0-preview.1) from source commit `d61662e`; public documentation commit `ba35f4c`. The anonymously downloaded ZIP and its executable match the tested build. The downloaded executable is installed at `Desktop\Arnav Island\app`, registered in the real `HKCU\...\Run` key (checked outside any sandbox), launched through Explorer, and running and responding. QA files from testing were removed from `%LOCALAPPDATA%\ArnavIsland`, so the installed app starts from default preferences. Hashes are in `evidence/v0.7/public-verification.json`.
