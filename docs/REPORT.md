# Arnav Island v0.6 — phase-one development report

The normal experience is now smaller. Mini Pill rests at 72 × 34 DIP. Live Island opens a 360 × 154 DIP hover card; Command Center retains the full 420 × 334 DIP workspace. Hover is the normal expansion action. Blank-space clicks no longer toggle or pin the surface, and the island no longer draws its product name.

Compact width extends to 560 DIP. New settings select the mode and compact volume/timer/clock/media/battery details. Details fit the selected width; Mini intentionally stays minimal. Shelf peek uses cached Shell artwork with compositor opacity/scale springs. Artwork atmosphere blends three retained radial color layers with velocity-preserving springs, respecting reduced motion. Display memory stores stable monitor identity, offsets, width, scale and edge locally and falls back to primary when the preferred display is absent.

## Scope and research

This is phase 1 of the user's expanded request. DELIVERY_PHASES.md records the API research and remaining phases: multi-session media, real loopback visualization, per-app mixer, branding review, Bluetooth, brightness, health data, clipboard, privacy and local commands/workspaces. Those providers are not simulated or claimed as shipped. No arbitrary firmware access, service scraping, or audio/clipboard capture has been enabled.

## Verification and defects corrected

- Three CTest suites pass: 11,682 core checks and 1,831 dashboard/model/cadence checks, plus real provider lifecycle coverage.
- Twelve native interaction stages pass, including no surface-click expansion, small/large mode transitions, Command Center hover timer routing, settings, seeking, and prior shelf/nav regressions.
- Native screenshots exposed a clipped Live title and a shelf overlay behind the content layer. Both were corrected and re-captured. A hover timer ID originally collided with fullscreen debounce; the IDs are now separate and the routing is covered by native regression.
- Display-profile parsing/roundtrip, malformed records, stable-identity lookup and missing-display behavior are covered by models. Physical docking/mixed-DPI acceptance is still required.
- Spring curves are checked at 60/90/120/144/165/240 Hz against the analytical trajectory. This proves numerical behavior, not achieved display FPS. Actual high-refresh presentation, waveform rendering and latency remain unmeasured.

Final reviewed captures, process counters and publication/installation checks are recorded in evidence/v0.6. This is an unsigned preview, not a zero-defect or mass-deployment certification.

## Publication and installation — 2026-09-23

Published [v0.6.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.6.0-preview.1) from source commit `4eee895`; public distribution commit `d2485eb`. The anonymous ZIP download and unpacked executable match the tested local hashes. The installed Desktop executable reports v0.6.0-preview.1 and is running/responding. The Desktop shortcut has no expansion arguments.

The settings file hash and sign-in registry value are unchanged; startup remains enabled. The new local display-profile store contains one profile and a valid Windows display identity. Monitor identities remain local and are excluded from evidence. Recent startup log entries are informational; the installed app connected to a real media session. This confirms provider connection, not universal service/action coverage.

Exact hashes and verification flags are in `evidence/v0.6/public-verification.json`. Remaining phases and acceptance limits are explicit in DELIVERY_PHASES.md.
