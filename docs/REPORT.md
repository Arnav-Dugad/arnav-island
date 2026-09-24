# Arnav Island v0.11 — development report

## What changed

**Synced lyrics (opt-in).**
- **Lookup.** A worker thread queries LRCLIB's search endpoint over WinHTTP with the song title and artist only. Among the results it keeps the synced version whose length is within 3 s of the track (10 s at most). A cut that doesn't match is never used, because its lines would drift.
- **Messy titles.** Browser titles like "Artist - Song (Official Video)", "- Topic" and VEVO channels, "feat." credits and "- Remastered" suffixes are cleaned before the search. If the full artist credit finds nothing, the first-named artist is tried once.
- **Cache.** Answers, including "none", are cached one file per song and written atomically. The folder is capped at 400 songs, and "none" expires after 14 days. Network failures aren't cached and are retried after a minute.
- **Quitting** cancels a request in flight by closing its WinHTTP handle.
- **Timing.** The island moves to the next line with a timer set for exactly when it starts, not by polling. The compact label and Live card show the current line, and the Media page shows three lines on their own compositor layer.

**Artwork palette.**
- A hue histogram of the cover, weighted by saturation squared times brightness, gives:
  - a main colour, lightened for the dark island
  - a second hue at least 45° away (or a deeper shade of the main colour)
  - the overall tone
  - a deep shade for light islands
- Grey covers, and colour specks under about 1.2% of the weighted picture, stay neutral.
- The timeline fill and the waveform bars run from the main colour to the second (the waveform uses 8 shared surfaces). The glow and glass tint take the overall tone. The previous accent was the plain average colour, which was often muddy.

**Seeking.**
- **Detents** are even time marks (10 s, or wider on long tracks so they stay 12 DIPs apart) plus lyric line starts. Snapping takes 4 DIPs of pointer travel, scaled by the fine-control gain, so fine control still reaches every second.
- **The scrub keeps a raw pointer value** separate from the snapped value, so the playhead leaves a detent as soon as the pointer does.
- **Hover bubble.** It shows the time and the lyric at that point.
- **Skips.** Double-clicking a half of the artwork skips 10 s, and further quick clicks chain. The arrow keys work on the timeline.

**Headphone card.**
- **Trigger.** The default output changes to an endpoint whose Windows form factor is headphones, headset or handset.
- **Exceptions.** No card appears within 4 s of a switch made from the island, or while the island is open. In those cases the old "Output · name" note is used.
- **Content.** It shows the paired Bluetooth device's logo and battery, and *Switch back* returns to the previous output.

**Audio.**
- **App volume.** The mouse wheel over the compact logo changes the mixer session of the playing app, matched by name, and the level bar shows that app's icon.
- **Microphone mute.** It covers both default capture roles, with a change callback. The Audio page button, the command words and the on-island note all follow it.

**Fixes.**
- A 5A bug: in Settings on the light theme, the Wallpaper accent fell through to the Peach colour, which showed as orange.
- Card kinds are now explicit ranges, where they had been "3 and up" and "5 and up", so a new card kind can't land in the power or privacy drawing paths.

## Verification

- **Unit suites:**
  - 11,682 core checks
  - 1,839 model checks
  - **5,245 phase checks**
  - provider lifecycle
- **New unit checks:**
  - UTF-8 in both directions, including invalid bytes
  - JSON: escapes, surrogate pairs, nesting limit, malformed input
  - LRC parsing: offset on every line, multiple time tags, garbage
  - line timing
  - ten title-cleaning cases, including "Video Games" and "Live Forever", which must survive
  - choosing a result by length
  - the cache file round trip, expiry and corruption
  - the palette: red, two-hue, grey and speck covers, and transparent art
  - detents, snapping, the raw scrub value and skip clamps
  - headphone detection and output names
  - app-to-mixer matching
  - the mic commands
  - settings v10
- **Native UI regression:** now **18 stages**. The new stage double-clicks each artwork half, presses → on the timeline, checks the snap near the 60 s mark and checks the clamp at 0. It uses a made-up track, so no real player is ever seeked. **Settings end-to-end:** 129/129.
- **Real network:** three LRCLIB lookups.
  - A clean player title and a YouTube-style title both found the same 47-line synced lyrics in 0.2–0.7 s.
  - A nonexistent song was recorded as missing.
  - A second run answered all three from the cache in about 30 ms.
- **Real audio:**
  - The island's microphone button muted both default microphone roles, and Windows reported muted for each. The button turned red. A second click restored both.
  - Output form factors read correctly on this PC (speakers 1, microphones 4).
- **Idle while tucked away:** 0.00 s CPU over 30 s, 60.2 MB private memory (`evidence/v0.11/idle-hidden.json`). The lyrics worker doesn't exist until lyrics are turned on.

**Not exercised on real hardware in this session:**
- A real headphone switch: no headphones were connected. The card was checked with a synthetic switch.
- App volume with a live player: nothing was playing. The name matching is unit-tested and the indicator was captured.

Screenshots in `evidence/v0.11` use the synthetic showcase session with placeholder lyric lines written for testing, and illustrative devices.

## Limits

- Lyrics exist only for songs LRCLIB has synced, and they follow the position Windows reports.
- There are no chapter detents: Windows media sessions don't expose chapters.
- A browser's volume covers all of its tabs.
- Unsigned preview. No UI Automation tree for screen readers yet.
