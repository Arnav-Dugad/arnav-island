# Arnav Island 0.12.0-preview.1 — Capture and Shelf superpowers

Phase 5C: capture the screen, read text from it, pick colours, do more with Shelf files, and a smarter clipboard.

## Snip, copy text, pick a colour
- **Alt+Shift+S: snip.** The screen freezes and dims, with the island left out of the picture. Drag a region, or click a window or a whole screen. The snip is saved as a PNG in *Pictures › Screenshots*, added to the Shelf and copied, and a card shows it with an *Open Shelf* button.
- **Alt+Shift+T: copy text from anything.** Drag over text on screen; Windows' own on-device text recognition reads it and the text is copied, keeping its line breaks. Nothing leaves your PC.
- **Alt+Shift+C: colour picker.** A magnifier shows the pixels around the pointer. Click to copy the colour as HEX, or hold Shift for `rgb(...)`. The arrow keys nudge one pixel.
- A size label follows your selection, and the corners carry your accent colour. Esc or right-click cancels.
- The same tools are buttons at the bottom of the Shelf, and command-bar words: "snip", "copy text" and "pick colour".

## Shelf quick actions
- **Click a Shelf item** to open it: a preview, its type, size and dimensions, and actions:
  - **Open**, **Open with**, **In folder**, **Copy path**
  - for images: **Copy text** (on-device recognition), **To PNG / To JPG** and **Half size**, saved next to the original
  - **Zip** a file or folder, or zip the whole Shelf from its footer. The archive is compressed, keeps Unicode names, and opens in File Explorer and other ZIP tools.
  - **Remove**
- Converted and zipped files join the Shelf automatically.

## Keep the Shelf (off until you turn it on)
- Settings → Privacy & productivity → *Keep the Shelf after restarts*. It remembers **links** to your Shelf files and any dropped text, never copies of the files. Files that have been deleted are skipped.

## Clipboard v2
- **Search your copies:** the search button on the Shelf's Clipboard tab, or type "clip " in the command bar. Every word you type must match.
- **Alt+Shift+V: the clipboard picker.** It lists your last copies in the command bar. Type to filter; **Enter pastes** into the app you were in, and **Shift+Enter** only copies. Copies are kept as plain text, so pasting from history never brings formatting.
- **Pin** a copy with the pin on its row. Pinned copies stay at the top and are never pushed out. Up to 12 are kept, and they survive restarts, **encrypted for your Windows account**.
- **Passwords and codes stay hidden:** copies that look like a password, one-time code or API key show as dots until you point at them.
- The Shelf's *Clear* keeps pins; Settings' *Clear clipboard history*, or turning history off, removes everything.

## Polish
- Home shows today's date in your own date format where the old heading was.
- The *Windows volume mixer* button's arrow now sits inside the button.
- Shelf rows show a chevron, and the Shelf's empty state mentions snipping.

## Fixed
- **Bluetooth audio Connect/Disconnect:** the interface behind the buttons was declared in a way that let the compiler's optimiser treat calls through it as unreachable in release builds. It is now declared safely, the same way as the new text recognition, which had the same problem during development and crashed until fixed.

## New in the background
- If the island ever crashes, it writes `crash.txt` next to its logs: the error code and code offsets only, no content, file names or personal data.

Windows 11 x64, unsigned preview. Settings move to version 11; existing preferences are kept.

---

Previous release: [0.11.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.11.0-preview.1): synced lyrics, artwork colours, smarter seeking, the headphone card and audio controls.
