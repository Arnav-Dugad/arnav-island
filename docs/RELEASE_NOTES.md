# Arnav Island 0.10.0-preview.1 — Glass, a lab and a waveform

The first part of Phase 5 (motion and materials), plus the fixes you asked for.

## Frosted and Clear glass are different now
- **Frosted glass** blurs what's behind the island when Windows transparency effects are on. When they're off, it becomes a soft, translucent frost. It used to fall back to a flat opaque fill that looked the same as Solid.
- **Clear glass never blurs.** It lets the desktop show through with only a light tint, and has a brighter rim and sheen, so the two materials are easy to tell apart.
- Text on glass gets a faint halo, so it stays readable over busy or bright backgrounds.
- A soft light follows your pointer across the island. It's strongest on glass, very faint on the dark solid island, and off on the light solid island and with Reduce motion.

## The Animation Lab moved into Settings
Settings → Motion now contains the whole lab:
- **Live preview:** a small island opens and closes with your spring, next to its curve. It shows how long the motion takes to settle and how much it overshoots. Click it to replay.
- **Motion character:** Balanced, Fluid, Playful, Snappy, Calm, or **Custom**. The Stiffness, Damping and Weight sliders show the chosen preset's values. Moving any slider makes it Custom, and the island uses the change immediately.
- **Slow motion (1×, ½×, ¼×)** slows the island down so you can study it. It's never saved, so the island is back to normal speed next time.
- **Try it on the island:** Expand, Collapse, Interrupt (reverses mid-flight) and Card. The readout then shows your display's refresh rate, the frames Windows actually composed while the island moved (for example "59 fps" on a 60 Hz screen), and the app's memory. If the island was already in that state, it says so instead of showing a number.
- The separate Animation Lab window and its HUD are gone. The tray menu, `--lab` and the old shortcut all open this page.

## A waveform timeline for media
- On the Media page, the timeline is a waveform of the track you're listening to. It is learned from the audio actually playing, so it is never a made-up shape.
- Stretches you haven't heard yet are quiet dots, and they fill in as the song plays. Heard bars rise in as they arrive.
- The bars you've passed are lit in the accent colour. A capsule playhead moves smoothly with playback, and the waveform swells while you drag to seek.
- The island remembers the waveforms of your last 32 tracks while it runs, in memory only, so a replay starts complete.
- Turn it off in Settings → Media & sound → *Waveform timeline* to get the plain line back.

## Motion
- **Liquid morph:** when the island opens, the width leads and the height follows on a softer spring, and the reverse when it closes. The shape flows instead of scaling uniformly.
- **Staggered rows:** when a page, tab or session changes, its rows rise and fade in one after another, 28 ms apart, timed by the compositor.
- **Icon pops:** when a button's icon changes in place, such as play to pause or mute to volume, the new icon springs in.

## Colour
- **Wallpaper accent:** the fifth accent swatch takes a soft colour from your desktop wallpaper, and updates when you change the wallpaper. Grey or black-and-white wallpapers give a neutral accent.

## Fixed and removed
- "Your day at a glance" no longer appears on the Home page.
- Settings → Appearance explains what each material does and what happens when Windows transparency is off.

Windows 11 x64, unsigned preview. Settings move to version 9; existing preferences are kept.

---

Previous release: [0.9.0-preview.1](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.9.0-preview.1): command bar, clipboard history, privacy dots and workspaces.
