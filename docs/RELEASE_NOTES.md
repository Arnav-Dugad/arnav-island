# Arnav Island 0.11.0-preview.1 — Now Playing Pro

Phase 5B: lyrics, artwork colours, better seeking, a headphone card and audio controls.

## Synced lyrics (off until you turn them on)
- Turn on **Settings → Media & sound → Synced lyrics**. The line being sung appears:
  - in the compact island, in the accent colour
  - on the Live card
  - on the Media page, which shows the previous, current and next lines. The current line glows softly on dark islands, and each new line rises into place.
- Lyrics come from **LRCLIB**, a free lyrics library that needs no account or key. **Only the song title and artist are sent.** The island picks the version whose length matches your track, so a live or extended cut never scrolls out of step.
- Each song is looked up once. Its lyrics, or the fact that none were found, are saved on your PC. Settings → Media & sound → Saved lyrics → **Clear** removes them.
- The lyrics button next to the page title switches the Media page between lyrics and the usual title and artist.
- Titles from YouTube and other sites, such as "Artist - Song (Official Video)", are cleaned up before searching. Songs with no synced lyrics simply show the normal layout.

## Colours from the artwork
- The island reads a small palette from the cover: a main colour, a second colour and the picture's overall tone.
- The played part of the timeline and the waveform run from the main colour to the second. The background glow and the glass tint take the overall tone.
- **The light island now uses the artwork's colour too**, as a deep shade, instead of always the same green.

## Smarter seeking
- **Hover the timeline** to see the time under the pointer, and the lyric sung there when lyrics are on.
- **Detents:** while dragging, the playhead snaps to even time marks (every 10 s on a normal song) and to the start of lyric lines, with a small tick. Pull away from the bar for fine control, as before.
- **Double-click the artwork** on the Media page to skip 10 s: left half back, right half forward. Keep clicking to go further. With the pointer on the timeline, ← and → also skip 10 s.
- Windows doesn't tell apps about chapters, so there are no chapter detents. Lyric lines take their place.

## Headphone card
- When **Windows moves your sound to headphones** (for example, earbuds connecting), a card shows where the sound went and where it came from, with a **Switch back** button.
- The card doesn't appear when you pick the output on the island yourself. Turn it off in Settings → Devices & power → *Headphone switch card*.

## Audio controls
- **Per-app volume from the compact island:** scroll over the playing app's logo or artwork to change that app's volume, without touching the system volume. The level bar shows the app's own icon.
- **Microphone mute:** the Audio page has a microphone button, which turns red when muted. It mutes your default microphone for calls and recording alike. The command bar understands "mute mic", "unmute mic" and "mic". The island briefly says "Microphone off" or "Microphone on" when the state changes, including changes made elsewhere in Windows.

## Fixed
- In Settings on the light theme, the Wallpaper accent showed as orange. It now uses a deep shade of the wallpaper colour.

Windows 11 x64, unsigned preview. Settings move to version 10; existing preferences are kept.

---

Previous release: [0.10.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.10.0-preview.1): glass materials, the Animation Lab in Settings and the waveform timeline.
