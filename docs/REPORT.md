# Arnav Island v0.9 — development report

## What changed

**Phase 4, productivity and privacy.**
- **Command bar.** A fixed, testable grammar, parsed on a worker thread along with the Start-menu app list and icons. Intent is shown before Enter, and nothing is run as a shell command. File search builds a `search-ms:` query in Windows Advanced Query Syntax, limited to the user folder. The date filters were verified against Windows Search: on this PC "this week" and "thisweek" both match 156 files, and today ⊂ this week ⊂ this month.
- **Workspaces.** Open apps are captured by their installed app ID where Windows has one, otherwise by program path. Opening needs a second Enter and skips apps already running.
- **Clipboard history.** Opt-in and memory-only, honouring the private-copy flags Windows' own history uses.
- **Privacy indicators.** Read from Windows' capability access records and confirmed against running processes, then shown as compact dots, an expanded-page band and cards.

**Fixes from your report.**
- **Duplicate media sessions.** Every Edge tab shares the app ID `MSEdge`, and the island used that ID to select sessions, so session 2 always resolved back to session 1. A play command also went to every Edge session. Sessions now carry their own ID for selection, play/pause and seeking.
- **Site identity.** Sites were matched against visible window titles only. They are now assigned jointly from every tab title, read through UI Automation from the tab strip alone (about 40–130 ms, only when the set of sessions changes). Each tab can label one session.
- **Compact-only edge reveal.** The pointer on the edge rows never opens the island; resting on the island does. Separately, with *Show alerts while hidden* on, the start-up "new media" notice had counted as an alert and kept the island out. It no longer does.

**Rendering.**
- **Shoulders.** They were a 32 px bitmap scaled by radius/32, which is blurry at the compact radius of 17. They are now twice as wide as deep and drawn at the exact pixel size for each target radius, so they show 1:1 at rest. The body and shoulders snap to whole pixels at rest with a 1 px overlap.
- **Corners.** DirectComposition clips were hard-edged, visibly stair-stepped at 8× zoom. The whole tree now uses soft borders.
- **Text.** Grid-fitted, higher-contrast grayscale rendering with Segoe UI Variable's optical sizes.
- **Hover highlight.** It was a stretched bitmap and is now a rounded clip.
- **Canvas.** Widened to 680 px so the widest compact island keeps its shoulders.

**Motion.**
- The command bar morphs to its row count.
- The caret glides and blinks in the compositor.
- New page, tab, session and result content eases in (220 ms ease-out, compositor-timed); navigation and controls stay steady.
- Tabs slide toward the one chosen, and the compact label eases in when it changes.

**Logos.** 180 marks:
- Simple Icons 16.32.0.
- Simple Icons 9.21.0 for marks withdrawn later.
- Public-domain Commons files traced with potrace.
- AULA traced from its site.

On your devices, 9 of 10 paired Bluetooth devices now get a maker mark, up from 5.

## Verification

- **Unit suites:** 11,682 core, 1,834 model and **5,093 phase** checks. New checks cover:
  - the command grammar: every action, malformed timers, app initials, uninstallers never offered, percent-encoded search text, and shell-like input never becoming a command
  - file-query filters
  - clipboard history dedupe and the count and memory bounds
  - workspace store limits, Unicode round trips and malformed files
  - privacy record decoding and staleness
  - site assignment for your two reported cases
  - device brands (TAS2400 is Philips; "Patriot" is not Riot)
  - settings v8 migration.
- **Provider tests on this PC:**
  - 108 consent records read.
  - Open apps captured.
  - Browser tabs read in 42 ms.
  - The command worker resolved Edge with its icon.
  - A clipboard round trip: another app's copy captured, copied back, own copy ignored, original restored.
- **Native UI regression:** 17/17 stages. **Settings end-to-end:** 114/114, including the new section.
- **Real desktop:**
  - The shortcut opened the bar with keyboard focus, typing reached it, and Esc returned focus to the previous window.
  - Edge reveal showed compact only at the edge, opened on the island and tucked away.
  - Live clipboard capture worked, and a repeated copy counted once.
- **Idle while tucked away:** 0.05% of one core over 30 s, 64 MB private memory (`evidence/v0.9/idle-hidden.json`).

Screenshots in `evidence/v0.9` use illustrative clipboard, privacy and device content. Captures with your desktop, titles or devices were used only for local checks.

## Bugs found while testing

- The caret surface was drawn inside the content surface's draw call. DirectComposition refused (`0x88980801`) and the app exited; the caret now updates afterwards. Start-up errors are also written to `last-error.txt`.
- The expanded privacy band was added beneath the body's opaque fill (z-order).
- Activity and hover timers could collapse the open command bar. `transition` now refuses to leave the command state except through the bar's own close path.
- Ctrl+Alt+Space is taken on this PC (Claude's desktop app), so the default is Alt+Shift+Space, with a choice and a conflict notice in Settings.

## Limits

- Command bar: no text selection or IME. Apps must be in the Start menu.
- Privacy dots cover apps Windows' capability tracking sees; some drivers and virtual cameras bypass it.
- Clipboard history keeps plain text for rich text and ends when the island quits.
- Workspaces do not restore browser tabs or window positions.
- Text on the transparent island is grayscale-antialiased.
- Unsigned preview. No UI Automation tree for screen readers yet.
