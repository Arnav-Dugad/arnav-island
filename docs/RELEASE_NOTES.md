# Arnav Island 0.14.0-preview.1 — New glass, a screen-capture dot, live GPU, a left dock

Phase 5E:
- Frosted and Clear glass are rebuilt as a real material.
- The island tells you when an app is capturing your screen, and shows live GPU use.
- It can dock on the left.
- Numbers, lyrics and the artwork move with the music.

## Glass, rebuilt
- **A real material.** Frosted glass now shapes the blurred backdrop the way Apple's materials do. It boosts the colour behind it, then remaps its brightness into a calm band, so text stays readable over any wallpaper and the glass still glows with what's behind it.
- **Light along the edges.** Where the glass meets open space, a brighter band of the material catches the light, like the thick edge of a pane.
- **A highlight that moves.** The specular rim and inner glow tilt as the island stretches and moves, like light on a sheet of glass.
- **Fine grain.** A faint, even texture separates frosted glass from a flat tint and hides colour banding.
- **A soft shadow.** A gentle shadow under the island lifts it off the desktop, on Solid and glass alike. It never catches clicks. You can turn it off in Settings › Appearance.
- **Tinted to your wallpaper.** The glass leans lighter or darker to match your wallpaper.
- **Clear glass is easier to read.** It has a slightly deeper tint and a stronger text halo.
- **An honest fix:** version 0.13 said Frosted glass boosted colour, but Windows rejected that effect, so it never ran. This release fixes that and applies the material on the GPU.

## Awareness
- **A purple dot when an app is capturing your screen.** It sits with the green camera, orange microphone and blue location dots, and a card names the app. It covers apps that capture through Windows' screen-capture API, such as browsers sharing your screen and the Snipping Tool's recorder.
- **Live GPU use:**
  - on the Stats page, with a history line
  - as a Home statistic you can choose
  - in the idle glance
- **An idle glance.** When nothing else is showing, the compact island shows today's date, plus CPU and GPU use where there's room. You can turn it off in Settings › Compact › Glance when idle.

## Motion
- **Numbers roll.** Volume, battery, the focus timer, the Home statistics and the level indicator roll digit by digit, the short way round (9 → 0 is one step, as on a counter).
- **Lyrics morph in the compact island.** Each new line rises into place as the last one lifts away.
- **The artwork pulses to the beat.** The cover swells gently with the bass of what's playing (Settings › Media & sound).
- **Liquid shape changes.** Corners swell a little as the island grows or shrinks, so it flows between shapes like a drop of liquid.

## Command bar
- **Space works.** Pressing Space between words used to run the highlighted result. It now types a space.
- **Group headers.** When results mix kinds, small headers separate them: Apps, Files, Answer, Actions.
- **Colour codes.** Type #3A7BD5, #39f or rgb(58, 123, 213) to see a swatch with its RGB and HSL values. Enter copies it.
- **Did you mean…?** A misspelt app or command ("spotfy", "bluetoth") offers the closest match.

## Dock on the left
Settings › Island › Dock edge now has **Left**, with the same shoulders, auto-hide and edge reveal as the right.

## Settings (version 13)
New switches, all on by default:
- **Soft shadow** (Appearance)
- **Glance when idle** (Compact)
- **Artwork pulses to the beat** (Media & sound)

## Limits
- **No true lens bending.** Windows gives apps the blurred backdrop only as a brush that can't be offset or warped (a shifted copy renders black), so the edges gather light instead of refracting the background.
- **Screen capture detection** relies on Windows' capture consent records, so it misses apps that capture by other means, such as older desktop-duplication recorders.
- **Real blur** still needs Windows' Transparency effects (Settings › Personalization › Colors).
- **GPU use** comes from Windows' own GPU performance counters (the ones Task Manager uses). It is the busiest engine's load, summed across apps.
