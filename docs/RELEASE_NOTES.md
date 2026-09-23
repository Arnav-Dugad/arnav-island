# Arnav Island 0.9.0-preview.1 — Type it, copy it, see who's listening

Phase 4 of the delivery plan (productivity and privacy), plus fixes to media sessions, the edge reveal, sharper rendering and many more real logos.

## Command bar
- **Alt+Shift+Space** opens a command bar on the island from anywhere. You can also use the search button on Home. Type, see exactly what will happen, press Enter.
- It understands:
  - volume ("volume 30", "mute")
  - playback ("next", "pause")
  - timers ("focus 25", "timer 10 min", "stopwatch")
  - installed apps ("open spotify", or just "edge"; initials like "vsc" work)
  - file search ("find budget pdfs from last month", which opens File Explorer search in your user folder with date and type filters)
  - Windows Settings pages ("bluetooth settings")
  - workspaces, the clipboard history, and "lock".
- Nothing you type is ever run as a shell command. Every result is one of a fixed set of actions.
- The bar grows and shrinks with its results. The caret glides and blinks in the compositor, and the highlight follows your arrow keys or pointer. Esc returns you to where you were.
- The shortcut can be changed or turned off in Settings → Privacy & productivity (Off, Alt+Shift+Space, Ctrl+Alt+Space, Win+Alt+Space). If another app already owns it, Settings says so. For example, Claude's desktop app uses Ctrl+Alt+Space.

## Workspaces
- "save workspace study" remembers the apps you have open. Later, "study" (or "workspace study") lists them, and a second Enter opens the ones that aren't running. Nothing is ever closed.
- Up to 8 workspaces of up to 12 apps each, stored only on this PC. You can remove them in Settings.

## Clipboard history
- **Off until you turn it on** (Shelf → Clipboard, or Settings). It keeps your last 24 copies in memory only: text, links, images and files. Click one to put it back on the clipboard.
- Copies that apps mark as private (password managers, one-time codes) are never kept. Neither is anything from KeePass, 1Password, Bitwarden and similar apps. Pause and Clear are one click away. Turning history off forgets everything.
- The island briefly shows "Copied link", "Copied image" and so on. You can turn that off.

## Privacy indicators
- Small dots at the end of the island show when an app is using the **camera** (green), **microphone** (orange) or **location** (blue). The source is Windows' own privacy records, checked against apps that are actually running.
- Open pages show which app it is. A card appears when an app starts using the camera or microphone.

## Media fixes
- **Two browser tabs no longer show up as the same thing.** Every tab of a browser shares one app ID, and the island used that ID to pick sessions and send play/pause. It now tracks each session separately, so switching between a YouTube tab and a JioHotstar tab shows each one, and play/pause reaches the right tab.
- **Sites are identified from every open tab**, not just the visible window title. Each tab can label only one session. A background tab is recognised, and one YouTube tab never labels a second session.
- A session without artwork shows its service or app logo in the artwork square.

## Edge reveal
- Touching the screen edge now shows the **compact island only**. It opens when you move onto it and rest, as before.
- The "new media" notice no longer counts as an alert that keeps the island out while hidden.

## Sharper, smoother
- **Bigger, flowing shoulder curves.** The island now sweeps out of the screen edge. The curves are drawn at the exact pixel size of every shape, and all rounded corners are antialiased. The previous curves were a scaled bitmap and the corners were hard-edged.
- **Sharper text:** grid-fitted, higher-contrast rendering and Segoe UI Variable's optical sizes (Small for captions).
- The hover highlight keeps true rounded corners at any size.
- New content eases in when you change page, tab or session, and tabs slide toward the one you picked. The compact label eases in when it changes.
- Fixed: the device card's level ring was clipped; glance rings floated alone above expanded pages; the Home volume percentage sat above its slider.

## Logos
- **180 brand marks** (from 88), including Xbox, AULA, Philips, PowerA, Marshall, Jabra, Logitech, Nintendo, Microsoft, JioHotstar, Gmail, F1, Gemini, OpenAI, Claude, GitHub, Cloudflare, Prime Video, Hulu, Minecraft, Riot, Ubisoft and many more.
- Devices are recognised by name, Bluetooth company ID or USB vendor ID. For example: AULA keyboards, Xbox and PowerA controllers, Philips speakers (including model-only names such as "TAS2400"), Logitech mice and keyboards, and Nintendo Pro Controllers.

Windows 11 x64, unsigned preview. Settings move to version 8; existing preferences are kept. The command bar has no text-selection or IME support yet.

---

Previous release: [0.8.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.8.0-preview.1): edge reveal, Bluetooth and battery cards, 88 logos.
