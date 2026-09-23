# Arnav Island 0.8.0-preview.1 — Hidden until you reach for it

Phase 3 of the delivery plan (devices and power), plus an edge-reveal mode, real brand logos and new motion.

## Hidden until the pointer reaches the edge
- **On by default.** The island tucks away above the top edge (or past the right edge when docked there). Touch the screen edge where it lives, within a band a little wider than the island, and it slides in on a spring. Hover it to open as usual.
- It tucks away again after your collapse delay once the pointer leaves and nothing is holding it open. An open page, a pinned island, a drag, a drop or an active seek all keep it out.
- Tucked away, the island ignores clicks, so the windows under it get them.
- **Show alerts while hidden** (off by default) lets device, charging and volume cards slide in on their own.
- Settings → General → *Hide until the pointer reaches the edge* turns it off.

## Real logos
- **88 brand marks** from Simple Icons (CC0), drawn as vector paths at any scale: music and video services, social apps, browsers, players, game stores and hardware makers.
- **Web services are identified from the browser's window title**, but only when the title confirms it: YouTube, YouTube Music, Spotify Web, SoundCloud, Twitch, Netflix, Prime Video, Disney+, Hotstar, JioSaavn, Gaana, Apple Music, Deezer, Tidal, Vimeo, Crunchyroll, Max, Plex, Kick, Audible, Bandcamp and more (44 rules). If the title is ambiguous, the browser's own icon is shown.
- Desktop apps keep their real Windows icons. Apps with a known mark (for example Spotify, VLC, Discord, Steam) use it when Windows has no icon.
- The compact island shows the logo when a session has no artwork.

## Devices
- **Connection cards.** When a paired Bluetooth device connects or disconnects, the island grows into a card with the device's maker logo (Samsung, Sony, Apple, Bose, JBL, boAt, Sennheiser and others), its type (earbuds, headphones, speaker, controller, keyboard, mouse, phone, watch), and its battery when the device reports one.
- **Stats → Devices** lists paired devices with connection state and battery. Audio devices get **Connect/Disconnect** buttons that ask the Windows Bluetooth audio driver to connect or disconnect. Scroll the list with the wheel.
- Connection changes come from Bluetooth radio and device events, with a slow once-a-minute check as a backstop. Paired is never shown as connected.

## Battery and power
- **Charging card.** Plugging in or unplugging shows charge level, a one-shot energy sweep around the ring (skipped with reduced motion), and the charge rate and time to full when the battery driver reports them. It says so when the laptop is plugged in but holding its charge (battery care).
- **Stats → Battery:** health (full-charge vs design capacity), full-charge energy, cycle count when supported, a 24-hour charge graph, time to full or remaining (labelled estimates), design capacity and voltage.
- **Charge history** is kept for 7 days on this device only. Turning it off erases the history file.
- **Power mode** (for example Best power efficiency, Balanced or Best performance) from Windows' own power-mode notification.

## ROG and vendor tools
- The laptop model is read from the firmware tables Windows already exposes. On ASUS machines with Armoury Crate installed, Stats → System and Settings → Devices & power get an **Armoury Crate** button that opens the installed app. Nothing is sent to firmware, ACPI or vendor drivers.

## Motion
- Edge reveal: a spring slide with a fade; the glass blur and click region follow the same spring.
- Cards: the island morphs into a 372 × 92 card, the logo pops in on a spring and an energy arc sweeps once around it.
- Stats tabs: a sliding pill between System, Battery and Devices.
- Changing pages slides the content in the direction of the navigation item you picked.

## Settings
- New section **Devices & power**: device cards, charging card, charge history, Bluetooth settings, power settings and Armoury Crate (only on machines that have it).
- The window now always draws its final resting frame, and it responds to input sent from other threads straight away instead of waiting for the next mouse move.

## Fixes
- Word-start matching for device names: "TAS2400" no longer picks up a Samsung logo because it contains "s24".
- Vendor discovery moved off the UI thread, removing a start-up pause of up to two seconds.

Windows 11 x64, unsigned preview. Not covered: Bluetooth codec (Windows has no public API for it), firmware or GPU-mode control, clipboard shelf, privacy indicators, commands and workspace presets (Phase 4). Device battery appears only for devices that report it to Windows. Logos are trademarks of their owners and are used only to identify the app or device.

---

Previous release: [0.7.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.7.0-preview.1): glass material, separate Settings window, multi-session media, live waveform, per-app mixer, volume and brightness indicator.
