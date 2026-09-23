# Arnav Island v0.8 — development report

## What changed

**Edge reveal.** The island is hidden until the pointer touches the screen edge where it lives. The whole visual tree sits under a stage visual that slides past the docked edge on a spring and fades. The glass layer follows the same spring through its compositor expression, and the window's input region follows it too: while tucked, clicks reach the windows underneath. A 33 ms pointer check runs only while the feature is enabled. Open pages, a pinned island, drags, drops and seeking keep the island out.

**Real logos.** 88 Simple Icons marks (CC0) are embedded as SVG path data and turned into Direct2D geometry by a small path reader (all SVG commands, including arcs). 44 service rules identify web services from browser window titles, but only with confirmation: the title has to contain the playing title, or match Spotify's "title • artist" form, or be the only brand in view. Device makers come from name keywords (matched at word starts) and Bluetooth vendor IDs.

**Phase 3, devices and power.**
- Bluetooth: paired devices from SetupAPI properties (connected, battery, class of device), refreshed on HCI connect/disconnect and device-node events. Connection cards show the maker logo, type and battery. The Devices tab lists devices, and audio devices get one-shot connect/disconnect through the Bluetooth audio driver's KS property.
- Battery: the battery IOCTLs supply capacities, rate, voltage and cycles. The tab shows health, a 24-hour graph from local history, and labelled estimates. A charging card appears on AC changes.
- Power mode from Windows' notification. ROG model and Armoury Crate detection, read-only.

**Motion.** Card morph (372 × 92), logo pop spring, one-shot energy arc, sliding stats tab pill, and page changes that slide in the direction of the chosen navigation item.

## Verification

- **Native interaction regression (`--ui-test`): 17/17 stages pass.** New stages drive the real window. The pointer 6 px below the edge must not reveal the island and the edge pixel must. After the reveal the window region must be visible, and after tucking it must be empty.
- **Settings end-to-end (`--settings-test`): 104/104**, including the five new preferences. Passed on four consecutive runs after the fixes below (earlier runs failed intermittently; see below).
- Unit suites: 11,682 core, 1,831 model and **4,784 phase** checks. New checks cover all 88 brand paths parsing and staying inside their 24 × 24 view box, service detection (positive, negative and ambiguous titles), device classification (including the TAS2400 regression), connection-change diffing, the edge-reveal state machine and edge band, and battery estimates and history round trips.
- Providers on this laptop: battery design 90.0 Wh, full charge 75.9 Wh (health 84%), cycles unsupported. 10 paired Bluetooth devices, 6 with battery, 6 audio. ASUS ROG Strix G614JV with Armoury Crate. Five start/stop cycles.
- A real run on the desktop (not test mode) captured the island hidden at start, sliding in when the pointer touched the top edge, open on hover, and tucked away again (`evidence/v0.8/auto-hide-sequence.png`).
- Idle while tucked away: 0.31% of one core over 30 s, 62 MB private memory (`evidence/v0.8/idle-hidden.json`).

Device screenshots use `--qa-sample` devices. Captures showing real device names or the desktop were used only for local checks.

## Bugs found and fixed

- **The island could disappear after revealing.** The window region was only updated when the size and drag springs settled, not the new slide spring. The region froze at the tucked pose, so the revealed island was invisible and unclickable. It now waits for the slide spring too; the new test stages cover it.
- **Settings window could stall.** Its loop rendered only while animating. The last frame could land a hair before the springs settled, so the resting frame was never drawn. Input sent from another thread was handled inside `GetMessage` without the loop waking. Both are fixed; the Settings test also dropped from about 5 minutes to 3.
- The precision-seek test sampled the island's position once while it was still expanding; it now samples per pointer event.
- "TAS2400" showed a Samsung logo because it contains "s24". Names are now matched at word starts.
- Plugged in but not charging was shown as "Charging". It now reads "Plugged in" with a battery-care note.
- Vendor discovery (a Start menu scan) ran on the UI thread and delayed start-up by up to two seconds. It now runs on a worker.
- Turning off charge history now deletes the history file.

## Limits

- No Bluetooth codec, latency or signal strength: Windows has no public API for them.
- Device battery appears only when the device reports it to Windows. Connect/Disconnect is for audio devices only.
- A device card shows when the device connects; it is not yet tied to Windows switching the default output.
- Web services are recognised only from the active window title of each browser window; background tabs are not.
- This battery reports cycle count 0 (shown as unavailable) and rate 0 while holding charge, so time-to-full appears only while it is actually charging.
- No firmware, fan, GPU-mode or profile control, by design.
- Unsigned preview. Custom Settings controls still lack UI Automation.

## Next

Phase 4: opt-in clipboard shelf, microphone-in-use indicator, local commands and workspace presets. See DELIVERY_PHASES.md.

## Publication and installation — 2026-09-23

Published [v0.8.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.8.0-preview.1) from source commit `bcc4b7a`; public documentation commit `dd0b03f`. The anonymously downloaded ZIP and its executable match the tested build. The downloaded executable replaced the copy in `Desktop\Arnav Island\app`; the sign-in entry was already pointing there and was left as is. Existing preferences, display memory and charge history were kept; test files were removed. The app was relaunched through Explorer and started tucked away. Hashes are in `evidence/v0.8/public-verification.json`.
