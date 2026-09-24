# Arnav Island v0.12 — development report

## What changed

**Capture (snip, text, colour).**
- **The overlay.** A full-screen Direct2D window shows a frozen copy of every monitor:
  - The island is excluded from the copy (`WDA_EXCLUDEFROMCAPTURE`, held only for the instant of the copy).
  - The dim fades in over 160 ms.
  - Snip and text accept a dragged region, or a click on the window or screen under the pointer (visible, uncloaked top-level windows in z-order, then monitors).
  - A size label follows the selection, and accent corners mark it.
  - Colour mode shows an 11 × 11 loupe.
  - Esc is a hotkey only while the overlay is open, so it cancels even when Windows keeps the keyboard elsewhere.
- **Snips** are encoded to PNG through WIC on a worker thread. They're saved to *Pictures › Screenshots* with a unique name, put on the clipboard as a DIB, and added to the Shelf with a thumbnail.
- **Text** goes through `Windows.Media.Ocr` in the profile languages, on a worker thread:
  - Small regions are enlarged up to 3× (to about 1600 px) for accuracy.
  - Lines are joined with real line breaks.
  - The hand-written WinRT interfaces were read from the Windows metadata.

**Shelf.**
- **Item view.** Clicking a row opens it: preview, type, size, dimensions and folder, plus eight actions.
- **Conversions.** To PNG, to JPG (flattened onto white at quality 0.92) and half size run on WIC and are saved beside the original, or in Documents if that folder is read-only.
- **ZIP.** The island has its own writer:
  - deflate with LZ77 hash chains and fixed Huffman codes
  - blocks that wouldn't shrink are stored instead
  - CRC-32, UTF-8 names and DOS timestamps
  - archives are refused above 4 GB (no ZIP64), and names can't leave the archive
- **Pinned Shelf (opt-in).** It saves only links and dropped text, and on load skips files that no longer exist.

**Clipboard v2.**
- **Pins.** Up to 12. They are never evicted, and the newest copy is never the one evicted. They're saved as UTF-8 through DPAPI (`CryptProtectData`, current user).
- **Search.** Every word must match the text, file names or source.
- **Picker.** It lives in the command bar: Alt+Shift+V, or typing "clip ". Enter pastes: the copy goes on the clipboard, focus returns to the app, then Ctrl+V is sent with any held Shift or Alt released.
- **Secret detection.** One-time codes, mixed-class passwords and known key prefixes are masked until pointed at.

**Polish.**
- Home shows today's date in the user's format.
- The mixer button's arrow sits inside the button.
- Shelf rows carry a chevron.
- New icons: snip, eyedropper, folder, copy, archive, resize, convert, bin, open.

## Bugs found while testing

- **A release-only crash in text recognition.** The hand-declared WinRT interfaces were in an anonymous namespace. GCC saw that nothing in the file implemented them and, at -O2 and above, treated calls through them as unreachable. The crash jumped to the image base. It only happened in the release build, and was reproduced in isolation at -O3 on either thread. Moving the interfaces to a named namespace fixed it.
- **The same hazard in shipped code.** A scan found it in the Bluetooth audio connection interface (v0.8), which has been fixed the same way.
- **A comment swallowed a line of code.** A `//` comment appended to a dense line also hid the island's drag-and-drop registration that followed it on the same line. The UI test's drop stage caught it. The comment was fixed, and a scan of every source file found no other instance.
- **Snips failed to save.** The PNG encoder still held the file when it was renamed from `.part`, so the rename failed. The encoder is now released first.
- **Overlays cancelled themselves.** An overlay could cancel itself on deactivation when started by a click, because it never had the keyboard. It no longer cancels on deactivation; Esc and right-click still cancel.
- **A repeated cancel after closing.** Closing the overlay deactivated it and posted a second, "cancelled" result. There's now one answer per overlay.
- **A crash reporter.** It was added while chasing these bugs and stays in the app: `crash.txt` records the exception and code offsets only.

## Verification

- **Unit suites:** all four pass, including **5,290 phase checks**. The new checks cover:
  - deflate round trips through an independent inflater: text shrinks at least 3×, random data isn't expanded, empty input, chunked streaming, maximum-length matches and the sliding window
  - CRC-32
  - ZIP central directory, local headers, UTF-8 names and contents
  - DOS dates and entry-name safety
  - drag rectangles, window picking, colour text, snip names, OCR scaling and text tidying
  - clipboard pins, limits, restore order, search, secret detection (including ordinary text that must stay visible) and pin files
  - Shelf save and load
- **Native UI regression:** now **19 stages**. The new stage opens a Shelf item, goes back, removes an item, searches the clipboard, types a filter and pins from the list. **Settings end-to-end:** 135/135.
- **Real desktop:** a test window with known text and colour.
  - Copy text read "Quarterly review: launch on Thursday" exactly.
  - The picker copied **#3A7BD5** exactly.
  - A snip saved a 401 × 251 PNG and put the image on the clipboard. The test snips were deleted afterwards.
  - Outside the selection, the dimmed screen measured about 57% of its original brightness.
- **OCR:** a rendered two-line sample was read exactly in 47 ms (en-US).
- **ZIP:** checked with three readers:
  - .NET `ZipFile` read the listing, names and dates.
  - `Expand-Archive` extracted it; the text file's SHA-256 matched and it compressed from 1.24 MB to 10 KB.
  - File Explorer's own ZIP reader (Shell) read the names exactly (é is U+00E9, and the Devanagari folder entry is intact).
- **DPAPI:** a pin file round trip restored both pins, and the ciphertext doesn't contain the text.
- **Idle while tucked away:** 0.00 s CPU over 30 s, 60.6 MB private memory (`evidence/v0.12/idle-hidden.json`).

Screenshots in `evidence/v0.12` use sample files in a public folder, illustrative clips, and the capture overlay over a painted backdrop, never the real screen.

## Limits

- Converting to WebP isn't offered: Windows includes a WebP reader but no writer.
- The ZIP writer uses fixed Huffman codes. It shrinks text well, but less than dynamic-Huffman tools, and archives stay under 4 GB.
- The clipboard picker opens in the island, not at the text cursor.
- Copies are kept as plain text, so pasting from history never carries formatting.
- Unsigned preview. No UI Automation tree for screen readers yet.
