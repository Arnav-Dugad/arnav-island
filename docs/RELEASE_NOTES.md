# Arnav Island 0.13.0-preview.1 — Command bar v2, glass that meets the screen, new lyrics

Phase 5D:
- The command bar finds files and switches system settings.
- Frosted and Clear glass take the island's full shape.
- Lyrics are redesigned.

## Glass that meets the screen
- **The full shape.** Frosted and Clear glass now attach to the top of the screen with the same concave shoulders as Solid, as one continuous piece of glass. On the right edge, the shoulders run above and below. A vertical offset in Settings still floats the island.
- **Vibrancy.** Frosted glass boosts the colour of the blurred desktop behind it, as Apple's materials do. The GPU applies it.
- **A finished edge.** A fine specular rim and a soft inner glow follow the whole outline, curves included, so the pane looks thick.
- **Real blur needs Windows' Transparency effects**, under Settings › Personalization › Colors. When they're off, Frosted uses a dense translucent frost instead.

## Lyrics, redesigned
- **A proper lyrics view.** It sits between the title and the controls:
  - the line being sung is large and bold, and the lines around it are dimmed
  - each new line springs up into place while the old one fades away
- **A fill that follows the song.** It sweeps across the sung line row by row and is always complete before the next line begins.
- **Instrumental breaks.** Three dots light up one by one across the gap.
- **Tap a line to jump there.**
- **Easier to read on see-through glass.**

## Command bar v2
- **Your files, as you type.** Results come from the Windows Search index.
  - Say the type, date and folder in plain words: "budget pdfs from last week in downloads".
  - **Enter** opens a file, **Ctrl+Enter** shows it in its folder, and **Ctrl+C** copies its path.
  - Files you open often rise to the top.
  - If the index is off, the bar does a quick scan of your user folder instead.
- **System switches that know the current state:**
  - dark or light mode
  - Bluetooth and Wi-Fi
  - airplane mode, which turns every radio off or back on
  - empty the recycle bin (the row shows how many items and how much space)
  - sleep, restart, shut down and lock

  Each row says what will actually happen, for example "Turn Bluetooth off" while it's on. Anything hard to undo asks for a second Enter.
- **The empty bar is useful:**
  - your pinned commands (**Ctrl+P** pins one)
  - suggestions such as *Pause* while music plays, *Unmute* when you're muted, or dark mode in the evening
  - your recent commands
- **Currency conversion (off until you turn it on).**
  - Type "100 usd to inr", "$50 in €" or just "20 pounds".
  - Rates are the European Central Bank's daily reference rates, fetched at most twice a day.
  - Enter copies the amount, and the digits roll into place.
- **Typing aids:**
  - The letters that matched are highlighted.
  - A faint completion follows the cursor, and **Tab** accepts it.
  - While you type a command's name, the bar already shows what it will do.
- **Context keys.** The keys at the bottom change with the selected row.

## Polish
- Page titles now sit on the same line as the header buttons.
- Home: the title, artist and play button are centred on the artwork, and the volume track, its fill and its icons share one line.
- Media: the time labels are centred with the mode button.
- New icons: moon, Wi-Fi, plane and currency exchange.

## Settings (version 12)
- **Remember recent commands** is on. The file stays on this PC, and turning the setting off deletes it.
- **Currency conversion** is off.

Existing preferences are kept.

Windows 11 x64, unsigned preview.

---

Previous release: [0.12.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.12.0-preview.1): snip, copy text and colour picker, Shelf quick actions, and clipboard v2.
