# Feature status — v0.17 preview

| Area | Implemented | Limits |
|---|---|---|
| Design | Small top-center dock with curved shoulders; right-edge option; dark/light/system themes | Visual quality assessed from actual captures, not certified against another product |
| Motion | Analytical springs, velocity-preserving reversals, size/corner morph, art position/scale, hover highlight, press highlight, charging pulse, page offset | Interactive vector icons lift/scale; labels stay stable. Two-surface handoff preserves the visible composite during interruption |
| Media | GSMTC thumbnail/metadata/transport/timeline; separate compact, Home, Music and Video destinations | All services not tested; only OS-exposed artwork; Auto cannot reliably classify every browser |
| Shelf | File paths and Unicode text; copy-only OLE drop/drag-out; 32 entries with scroll, Shell thumbnails/file icons and drag images | In-memory; installed document handlers required; no virtual file/bitmap data, pre-entry attraction or persistence |
| Audio | Endpoint events, volume/mute, output names, optional direct switching | Isolated undocumented setter; current-output reselection tested, physical headphones/speakers switching not exercised |
| Power | Real percentage/AC state, event-driven charging pulse, low-battery priority; v0.8 battery driver readings (see below) | Estimates are labelled; no firmware access |
| Compact | Media title/art, timer, volume activity, battery | One priority activity at a time; no waveform simulation |
| Glass | v0.7: whole-island Frosted/Clear glass from the Windows.UI.Composition host backdrop brush, tint, sheen and rim; shape follows the body springs in the compositor | Blur needs Windows Transparency effects; off, battery saver or high contrast give tinted, unblurred glass. Glass always floats (no concave shoulders) |
| Statistics | CPU/history, memory, physical network throughput, disk free, uptime, thread count | Adaptive 1 Hz while visible; no GPU/fan/temperature readings |
| Glance rings | Battery/timer/both/off; compositor marker rotation; larger Focus ring | Arcs update on state snapshots, not per-frame radial geometry |
| Design system | 35 original vector symbols, centred controls, physical-pixel settled offsets, labelled navigation | Complete accessibility and text scaling remain unfinished |
| Focus | Timer/break/stopwatch; pause/reset and completion activity | No persistence through restart, calendar or reminders |
| Preferences | Ten groups, reorderable navigation, chosen Home metrics, v5 settings migration, independent layout reset, launch-at-sign-in | Startup uses current exe path; no settings import/export UI, profile import/export or full geometry editor |
| Native behavior | Per-monitor DPI v2, input region, selected monitor, fullscreen hide, configurable app-switch collapse and protected drags, decorated/maximized browser exclusion, no taskbar button | Mixed-DPI/hot-plug/high-refresh hardware acceptance unfinished |
| Diagnostics | Animation Lab in Settings → Motion with compositor frame counts, app-only captures, process-counter benchmark | No fabricated FPS/GPU metrics; frame counts are system-wide composition, not per-window presentation |
| Accessibility | Keyboard controls, OS reduced motion, opaque fallback under high contrast | Full screen-reader tree, text scaling and complete high-contrast palette unfinished |
| Safety | No injection, drivers, cloud, Explorer patching or arbitrary plugins | Provider recovery and GPU device-loss restoration unfinished |

External notifications and download monitoring are not implemented. Clipboard history and camera/microphone/location indicators arrived in v0.9 (below). Brightness (v0.7) and read-only ROG discovery (v0.8) are covered in the tables below. Empty or unavailable sensor values stay unavailable.

| Feature | Implemented | Limits |
|---|---|---|
| Precision seeking | Spring-expanded timeline, fine pointer gain, one seek on release, cancel | Requires advertised Windows session capabilities and seek bounds |
| Audio handoff | Confirmed default endpoint activity, pulse and selected icon spring | No invented Bluetooth codec, latency or connection status |

| Feature | Implemented | Limits |
|---|---|---|
| v0.6 modes | Mini Pill, Live Island hover card, Command Center; blank-surface click does not open/pin | Live card is intentionally a smaller control subset; large workspace remains available |
| Compact customization | Width 160–560 DIP, optional media/volume/timer/battery/clock; Mini fixed at 72 DIP | Details fit available space; no hidden telemetry polling for extra statistics |
| Display memory | Stable device-path preference and bounded local geometry profiles | Physical docking/refresh/mixed-DPI acceptance remains incomplete |
| Shelf peek | Spring-scale/fade larger cached thumbnail on hover | File icons remain icons when no document thumbnail is available |
| Artwork atmosphere | Continuous spring retargeting of retained radial color layers | Subtle light only; no real audio visualization in this phase |

See DELIVERY_PHASES.md for the remaining request; planned providers are not shipped features.

| v0.7 feature | Implemented | Limits |
|---|---|---|
| Settings window | Separate thread and window; all persisted preferences; live application; autosave; keyboard navigation; Mica when allowed | Custom-drawn controls have no UI Automation tree yet; screen-reader support remains unfinished |
| Media sessions | All GSMTC sessions (up to 8), per-session events, selection by app ID, swipe/drag/touchpad/tap, follow-current option | Players that do not publish a Windows media session cannot appear; one browser can expose one session for several tabs |
| App logos | Icon and name from `shell:AppsFolder\<AUMID>` or the running executable; YouTube/YouTube Music marks when the browser window title confirms both the title and service | Other web services show the browser; an inactive tab's service cannot be confirmed |
| Live waveform | WASAPI shared loopback on a worker, 1024-point FFT, 24 log bands, compositor-interpolated bars in compact, Live and Media | Protected/exclusive-mode audio can appear silent; runs only while a session is playing and bars are visible |
| Per-app mixer | IAudioSessionManager2 sessions grouped by process, ISimpleAudioVolume volume/mute, live IAudioMeterInformation peaks, real process icons | Default output only; apps that route to another device are not listed |
| Level indicator | Volume (endpoint callback) and brightness (WmiMonitorBrightnessEvent) grow the compact island into a bar | Brightness works on panels that expose the WMI class (internal laptop panels); external monitors report unavailable |
| Intent-aware hover | Fast pointer sweeps restart the hover delay | Threshold is fixed at 700 DIP/s |

| v0.8 feature | Implemented | Limits |
|---|---|---|
| Edge reveal | On by default. Tucks the island past its docked edge; reveals only when the pointer is on the edge pixels within the island's band (±56 DIP); tucks after the collapse delay; open pages, pin, drags, drops and seeking keep it out; hidden island has an empty input region. Glass and region follow the slide spring | Checked every 33 ms while enabled (a cursor position read, no hooks). Alerts while hidden are off by default |
| Brand marks | 88 Simple Icons 16.32.0 paths (CC0) parsed into Direct2D geometry; white plate for dark marks | Trademarks of their owners; used only to identify an app, service or device maker |
| Web service identity | 44 rules matched against visible browser window titles, requiring the playing title or a Spotify-style "title • artist" match; brand-only titles accepted only when unique | A background tab, a renamed window or a title without the service name falls back to the browser icon |
| Bluetooth cards | SetupAPI device properties for connection, battery and class of device; HCI connect/disconnect and device-node events; 60 s backstop check; maker from name keywords and vendor ID; type from class of device and name | Battery only when the device reports it to Windows. No codec, latency or signal strength (no public API). Names are matched at word starts |
| Audio connect | One-shot connect/disconnect through the Bluetooth audio driver's KS property on the device's endpoint | Audio devices only; availability depends on the driver. Keyboards, mice and controllers are listed without buttons |
| Battery tab | IOCTL_BATTERY_QUERY_* capacities, rate, voltage, cycles; health = full-charge/design; 24-hour graph from local history; estimates from the reported rate | Cycle count 0 is shown as unavailable. Relative-unit batteries hide energy values. This laptop reports rate 0 while holding charge |
| Charge history | 7 days at 5-minute spacing in `battery-history.nexus`, atomic writes | On by default; turning it off deletes the file |
| Charging card | AC change shows level, rate, time to full or battery-care state, one-shot energy sweep | No endless decorative loop; with reduced motion the sweep is skipped |
| Power mode | PowerRegisterForEffectivePowerModeNotifications | Display only; the island never changes the power mode |
| ROG | Manufacturer/model from the BIOS registry values; Armoury Crate detected in the Start menu app list and opened by its app ID | Read-only. No ACPI, WMI writes, GPU-mode, fan or profile control |

| v0.9 feature | Implemented | Limits |
|---|---|---|
| Command bar | Alt+Shift+Space (configurable) or Home's search button; fixed grammar for volume, playback, timers, installed apps (name, prefix, initials), file search with date/type filters, 34 Windows Settings pages, workspaces, clipboard and lock; intent shown before Enter; parsing and app icons on a worker | No text selection or IME composition; apps must be in the Start menu; file search covers the user folder |
| Workspaces | Save open apps by name (installed app ID, or program path for classic apps); open the ones not running after a second Enter; 8 × 12, local file | No browser tabs or window positions; apps that are running are not moved or closed |
| Clipboard history | Opt-in; text, links, images (DIB), files; 24 entries / 48 MB in memory; private flags and password managers skipped; click to copy back; pause, clear; duplicate updates collapsed | Nothing survives quitting; rich text is kept as plain text; image formats without a DIB are skipped |
| Privacy indicators | Camera, microphone and location use from Windows' capability access records, confirmed against running processes; compact dots, expanded band, cards for camera and microphone | Apps that bypass Windows' capability tracking (some drivers, virtual cameras) cannot be seen; location use can be brief |
| Media identity | Sessions keyed per session, not per app ID; sites assigned jointly from all tab titles (UI Automation, tab strip only) | A site whose tab title has neither the playing title nor its name stays unidentified |
| Rendering | Shoulders drawn 1:1 at each radius, pixel-snapped at rest; soft (antialiased) clip borders; grid-fitted high-contrast text with optical sizes; rounded-clip hover highlight; content entrances | Text on transparent layers is grayscale-antialiased (ClearType needs an opaque target) |
| Brand marks | 180 marks; device makers by name, Bluetooth company ID and USB vendor ID | Wordmark logos read small in round tiles |

| v0.10 feature | Implemented | Limits |
|---|---|---|
| Glass materials | Frosted: host backdrop blur, or a translucent frost when Windows transparency is off. Clear: never blurred, light tint, stronger rim and sheen, depth gradient. Faint text halo on unblurred glass | Clear glass over very bright, busy backgrounds trades contrast for transparency; Glass tint adds contrast |
| Pointer light | 260-DIP radial light on spring-driven offsets and opacity; glass and dark solid only | Off with Reduce motion and on the light solid island |
| Animation Lab | Preset or Custom spring, live preview with settle time and overshoot, slow motion (not saved), Expand/Collapse/Interrupt/Card on the real island, refresh rate from the island's monitor, composed frames per second, memory | Frames are counted system-wide; another app animating at the same time is counted too |
| Waveform timeline | 64 bars of mean loopback level per stretch of the track, normalised to the loudest stretch heard; unheard stretches drawn as dots; played part in the accent; capsule playhead; seek swell; last 32 tracks remembered in memory | Only what has actually played is known, so a new track fills in as it plays; protected audio can read as silence; needs a session with a known duration |
| Motion | Liquid morph, staggered row entrances, icon swap pops | Odometer digits and shared-element artwork morphs are not implemented |
| Wallpaper accent | 48 × 48 WIC decode of the wallpaper, saturation-weighted, softened to a legible pastel; refreshed on wallpaper change | Slideshows update when Windows reports the change; solid-colour backgrounds fall back to Mint |

| v0.11 feature | Implemented | Limits |
|---|---|---|
| Synced lyrics | Opt-in. LRCLIB `/api/search` with title and artist only (WinHTTP, 5–6 s timeouts, cancelled on quit); result chosen by track length (within 3 s, or 10 s); titles cleaned of "Official Video", "feat.", remaster suffixes and "Artist - Title" channel formats; one file per song in `%LOCALAPPDATA%\ArnavIsland\lyrics` (400 songs at most; "not found" retried after 14 days); compact label, Live card and a three-line Media panel with a rising line change | Only songs LRCLIB has synced lyrics for; timing follows the position Windows reports, so a player that reports rarely can drift until the next update; no plain (unsynced) lyrics view |
| Artwork palette | Main colour, second colour, overall tone and a deep shade per cover (hue histogram weighted by saturation and brightness); timeline and waveform gradients, glow and glass tint; light islands use the deep shade | Grey and black-and-white covers stay neutral; a speck of colour under about 1% of the picture is ignored |
| Seeking | Hover bubble with time and lyric; detents at 10–1800 s marks spaced at least 12 DIPs apart and at lyric lines (4 DIPs of pointer travel, scaled by the fine-control gain); double-click artwork halves for ±10 s (repeatable); ← and → on the timeline | Windows media sessions do not expose chapters, so there are no chapter detents; skips need a session that allows seeking |
| Headphone card | Default output change to an endpoint whose Windows form factor is headphones, headset or handset (name used only when none is reported); paired Bluetooth device's logo and battery when the names match; *Switch back* to the previous output | Not shown for switches made from the island (4 s window) or while the island is open; *Switch back* needs direct output switching |
| App volume | Wheel over the compact logo or artwork changes the mixer session whose app name matches the playing session (exact, then contained name); level bar with the app's icon | A browser's volume covers all of its tabs; apps whose audio session name differs from their media identity are not matched |
| Microphone | Mute for the default communications and console capture endpoints together; Audio page button, command words, and an on-island note on any change | Apps that mute inside themselves (a call's own mute button) are not reflected |

| v0.12 feature | Implemented | Limits |
|---|---|---|
| Capture overlay | Frozen view of all monitors (island excluded with `WDA_EXCLUDEFROMCAPTURE`), dim, drag or click-a-window/screen, size label, colour loupe, Esc as a temporary hotkey, right-click cancel | Protected content (DRM video) appears black, as in any screenshot; windows are picked by their frame bounds |
| Snip | PNG in Pictures › Screenshots, DIB on the clipboard, Shelf item with thumbnail, card with Open Shelf | Saved at the screen's physical resolution |
| Copy text | Windows.Media.Ocr in the profile languages, on a worker; small regions enlarged up to 3x; line breaks kept | Only languages Windows has recognition packs for (here en-US, en-GB, ar-SA); handwriting and stylised text read poorly |
| Colour picker | 11 x 11 loupe, HEX (Shift: RGB), one-pixel arrow nudges | Reads the frozen screenshot, so colours are as displayed |
| Shelf item actions | Open, Open with, In folder, Copy path; images: Copy text, To PNG / To JPG, Half size; Zip; Remove | No WebP output (Windows ships no WebP encoder); outputs go beside the original, or to Documents |
| ZIP | Built-in deflate (LZ77 + fixed Huffman, stored fallback), CRC-32, UTF-8 names, DOS dates, folders recursed | No ZIP64 (4 GB), fixed Huffman compresses less than dynamic; symlinked folders are followed as normal folders |
| Pinned Shelf | Opt-in `shelf.nexus`: file paths and dropped text, missing files skipped, 32 items | Files moved while the island is closed drop off the Shelf |
| Clipboard v2 | Search (every word), up to 12 pins never evicted and saved with DPAPI, secret masking, Alt+Shift+V picker that pastes into the previous app | Picker appears in the island, not at the text cursor; kept copies are plain text |

| v0.13 feature | Implemented | Limits |
|---|---|---|
| Attached glass | Frosted and Clear use the Solid outline: body plus two concave shoulders clipped by composition path geometry, whole-pixel joins, edge docking with shoulders above and below | Real blur needs Windows Transparency effects; without them Frosted is a dense translucent frost |
| Glass material | Colour-matrix saturation over the host backdrop (1.75 dark, 1.65 light), specular rim and two-step inner glow along the whole outline | Region-clipped window, so no outer drop shadow |
| Lyrics scroller | Spring scroll, growing sung line, fading ghost, compositor-timed fill per row, three-dot gaps, tap to seek, brighter neighbours on see-through glass | LRCLIB is line-timed, so the fill runs at a singing pace, not per word |
| File results | Windows Search index (OLE DB), words/type/date/folder filters, noise excluded, frecency ranking, Enter / Ctrl+Enter / Ctrl+C, thumbnails and icons | Only indexed locations; fallback scan covers the user folder for 0.4 s |
| System actions | Dark/light mode, Bluetooth, Wi-Fi, all radios (airplane), recycle bin with count and size, sleep, restart, shut down, lock; live state; second Enter for the last five | No public API for Windows' own airplane-mode switch, night light or do not disturb |
| Empty bar | Pinned (Ctrl+P, up to 6), suggested (media, mute, microphone, timer, dark mode by time of day) and recent commands | Kept in `commands.nexus` on this PC |
| Currency | Opt-in; ECB daily rates cached 12 h; strict grammar; rolling digits; Enter copies | About 30 currencies (those the ECB publishes); rates are the previous working day's |
| Typing aids | Matched letters highlighted, ghost completion with Tab, a command preview while its name is typed, per-row footer keys | — |

| v0.14 feature | Implemented | Limits |
|---|---|---|
| Glass material | Colour matrix over the host backdrop (saturation 1.8 dark / 1.55 light, luminosity gain and offset leaning with the wallpaper), edge light on free edges, 7% grain, rim and glow that tilt with motion (±26°) | Real blur needs Windows Transparency effects; the backdrop can't be displaced, so there is no true lens refraction |
| Soft shadow | Pre-blurred nine-grid in a click-through window under the island, Solid and glass, all edges | Shadow strength is fixed per theme and material |
| Screen capture dot | Consent-store `graphicsCaptureProgrammatic` / `graphicsCaptureWithoutBorder` records; purple dot and a card | Only apps using Windows' capture API; desktop duplication and GDI capture are not reported |
| GPU use | PDH `GPU Engine(*)\Utilization Percentage`, busiest engine summed over processes; Stats with history, Home statistic, idle glance | Sampled once a second only while shown; no temperature, clock or memory |
| Idle glance | Date, CPU and GPU in the compact island when nothing else is showing and there is room | Not in Mini Pill or on side docks; 0.31% of one core while shown |
| Rolling numbers | Odometer columns for the currency answer, level indicator, focus clock, Home statistics, compact volume, battery and timer | Digits roll; letters and symbols change in place |
| Compact lyric morph | Two layers: the new line rises in as the old one lifts away | Line-timed, like the lyrics themselves |
| Beat pulse | Bass (50-130 Hz) against its half-second average; up to 4% | Needs the loopback analyzer, which runs only while the cover is shown and playing |
| Command bar | Space types; group headers; colour swatches (hex, short hex, rgb()); "Did you mean" for apps and commands | Typos are guessed only when nothing matched, and only for four letters or more |
| Left dock | Mirror of the right dock: shoulders, auto-hide band, input region, reveal | — |

| v0.15 feature | Implemented | Limits |
|---|---|---|
| Drop pill | The body drops 42 DIPs as a floating pill while a docked stub (the compact width, at most 196 DIPs) keeps the shoulders; the outline peels from the edge; shoulders move in only after the pill detaches; DirectComposition, glass, shadow and input region all follow | Top dock only; side docks and floating islands grow as before |
| Edge light | Three tiers (core, dim, glow) of the island's outline sweep out from the middle of the free edge in 0.8 s on alerts | Plays on device, power, privacy, capture, headphone and sharing cards |
| Now Playing | Compact previous/play/next, swipe to skip (touchpad horizontal scroll or a sideways drag), fullscreen peek at the screen edge | The lock screen is off-limits to apps (a secure desktop) |
| Spectrum ring | 24 ticks around a round compact cover, from the loopback bands | Needs the loopback analyzer (runs only while playing and shown) |
| Word-timed lyrics | Enhanced-LRC `<mm:ss.xx>` word tags light words in the compact island, the Live Island and the Command Center | LRCLIB's lyrics are mostly line-timed; those sweep at a singing pace |
| Weather | Opt-in; Open-Meteo geocoding once, forecast every 30 min; Home tile with an animated sky (sun, stars, clouds, rain, snow, fog, storm) for 60 s at a time; glance chip | Needs the network; one place |
| Controls page | Wi-Fi, Bluetooth, airplane, dark mode, focus timer, microphone, volume and brightness, with live states | Brightness only on displays that expose it through WMI (built-in panels) |
| Sharing | UDP discovery, SAS pairing (six digits, committed nonce), static ECDH P-256 + AES-256-GCM transfers, accept prompt, Downloads, 16 GB limit; 49 loopback checks | Tested over loopback, not between two physical PCs; one network segment; Windows may ask about the firewall |
| Battery week | Daily full-charge capacity in `battery-health.nexus`; weekly card (health change, charges, use per day) from 9 am after 3 days of history | Needs a battery driver that reports capacity in mWh |
| Rich clipboard | Link host and path, optional site icon, colour swatch, code in Cascadia Mono / Consolas with token colours | Site icons fetch `/favicon.ico` from the site (opt-in) |
| Chips editor | Drag or arrow keys in Settings › Compact; seven chips, saved as an order | When space is short, chips on the right win |
| Adaptive text | Wallpaper luminance (fill/fit/stretch/centre/tile) through the Clear scrim; letters, icons and rolling digits flip per glyph | Clear glass, top dock, only when no window overlaps the island; span wallpapers treated as fill |
| Motion | Digit blur and stretch on jumps of 2.5+ figures, icon morphs (play/pause, volume/mute, microphone) and celebrations, lean with rubber band while dragged, shadow that deepens as the island grows | Lean on the top dock only |

| v0.16 feature | Implemented | Limits |
|---|---|---|
| Drop onto a PC | While files are dragged over the island with a PC paired, the Shelf page shows a Shelf zone and a zone per paired PC (up to four); the zone under the pointer lights; a drop on a PC sends there | Text dropped on a PC goes to the Shelf (only files and folders are sent) |
| Folders and batches | Protocol 2: one transfer carries any number of files and folder trees (up to 20,000 files, 16 GB a file, 1 TB in all); each file checked by SHA-256; received as `.arnavpart` and renamed; free names for top-level folders | Both PCs need 0.16; links and junctions inside folders are not followed |
| Transfers | Progress on the PC's Nearby row and a compact chip; Stop from either PC (waits wake within 0.2 s); the offer card goes away when the sender stops; space checked before accepting | One network segment |
| Shelf stack | Send Shelf on each paired row; the item count drags as a stack of every Shelf file (onto a PC in the island, or out to any app) | Files only |
| Island player | Media Foundation media engine, one song at a time from a queue; Windows media transport controls for the window (title, artist, cover, timeline, buttons, seeking); media keys; other players pause when it starts; a paused song gives its session up after 20 minutes | Plays what Windows can decode (Ogg/Opus need the Web Media Extensions) |
| Music library | Music folder scanned on first use (tags via the shell's property store, covers via its thumbnails), sorted by artist, album and track; five rows with covers; search in the command bar (*play* ...); *shuffle* | Music folder only; up to 8,000 songs, 12 levels; in memory |
| Continue on | Title, artist, album, app and position to a paired PC; there: a player with the song, the library, the song's file (island songs), or the same app opened (then it moves to the position once the app shows the song); this PC pauses | Browser tabs can't follow; other apps resume whatever they resume |
| Drag to skip | Sideways drags over the music skip in the compact island, the Live Island, Home and the Media page; a chip grows on the leading side and snaps past 44 DIPs; the header follows the finger; touchpad sideways swipes skip too | Players switch with the dots under the cover (no longer by swiping) |
| Two alerts at once | A second alert waits as a bud: it grows from the pill's foot, lets go 8 DIPs below and settles with a spring; up to four wait; clicking brings it forward; an unanswered pairing, offer or music card is never lost; the same alert updating replaces in place | Drop pill (top dock) only; in DirectComposition, the glass (a fifth part with its own rim), the shadow and the input region |
| Clipboard across restarts | `clips-history.nexus` under DPAPI; images as PNG made on a worker; saved 1.5 s after a change and at sign-out or shutdown, only after the saved history has been read back | Password-like copies only when pinned |
| Sounds | A two-note chime (-18 dBFS) with alerts, a click (-20 dBFS) in the chips editor; generated in memory | Quiet while an app is full screen; off in test runs |
| Island DJ | Halo behind the compact ring: a bloom in the new cover's colours on each track, a breathing glow in the next song's colours through a track's last 10 s | The next song is known only in the island's own queue |
| Liquid navigation pill | Two edge springs (leading quicker) with caps and a stretched middle | — |
| Weather | WinHTTP decompression failure detected and retried uncompressed; the town is kept and retried every 2 minutes when offline | — |


| v0.17 feature | Implemented | Limits |
|---|---|---|
| Town field | Settings › Compact › Town: a text field that searches Open-Meteo 320 ms after typing stops; up to six towns named *Town, Region, Country*, one row per name; arrow keys, Enter, click; the chosen place saved and its weather shown in Settings | Needs the internet (Open-Meteo) |
| Up next | The island's queue after the song playing (up to 200 listed), five rows with covers; drag to reorder (the others glide aside), click to play now, wheel and arrows to scroll; the next cover peeks from behind the current one | The island's own songs only |
| Crossfade | Two media engines; equal-power curves (sine in, cosine out) over 0–12 s (6 by default); a 0.35 s fade on skips | Songs shorter than twice the crossfade plus 4 s don't crossfade |
| Handoff fades | The island's song fades out over 1.4 s, then pauses; another app's mixer volume is lowered, the app paused, and its volume put back; on the other PC the song rises over 1.6 s | Apps without a mixer session pause straight away |
| The cover on Play here | A 192-pixel JPEG of the cover goes with the offer (revision 1); the card shows it and how far into the song it is | Both PCs need 0.17 |
| Another PC's Shelf | Nearby › Shelf lists a paired PC's Shelf (up to 32 items, previews up to 6 KB each); a click takes a copy into Downloads and onto this Shelf; the other PC confirms; off with *My PCs can take from the Shelf* | Files and folders only; both PCs need 0.17 |
| Big transfers | 5 GB or more: a ring in the compact island filling with progress, its stroke from 1.6 to 4.4 DIPs with speed (full at 110 MB/s), the speed beside it and the time left in the label; speed and time left on Nearby rows | Speed is smoothed over quarter-second samples |
| Side by side | Pointing at two alerts: the pill moves left and the waiting alert becomes a card as tall as it, 8 DIPs to its right (spring), clickable there; closes up 0.38 s after the pointer leaves | Drop pill (top dock) |
| Screen readers | UI Automation: the island is a pane (expand and collapse) whose children are its current targets in reading order, with names; Invoke for buttons, Toggle for switches, RangeValue for sliders; alerts, pages, songs and levels announced | The Settings window is not yet exposed |

| v0.17.0-preview.3 feature | Implemented | Limits |
|---|---|---|
| Rim glint | A radial-gradient stroke (78 DIP radius) on the body's rim under the pointer, with the pointer's light | Glass only; the shoulders' curves keep their own rim |
| Settling frost | Frosted: a paler (half saturation), milkier, 10 px softer material over the glass; in over 24 s after 6 s at rest, out in 0.35 s on hover, opening or an alert; *Frost that settles* | Needs Windows transparency effects |
| Beat edge light | Bass against its half-second average; a rim stroke on glass (and shoulders), soft strips inside the free edges on Solid, in the accent; *Edge light to the beat* | Music playing and the island visible; off with Reduce motion |
| Clear tint | `clearTint` (v17), separate from Frosted's `glassTint` | — |
| Glass preview | Settings › Appearance › Preview: the island over a moving wallpaper in the chosen theme, material and tint, with glint, frost and beat | A drawing of the look, not the Windows glass itself |
| Row previews | 14 rows open an animated picture after 450 ms at rest; close on moving to another row | Pointer only |
| Dynamic type | Text of 9 pt or more that is wider than its space shrinks, by up to 14%, before it is cut | — |
| Sunrise and sunset sky | Open-Meteo daily sunrise and sunset (local time and offset); dawn 45 min before to 35 after sunrise, dusk 40 before to 35 after sunset; warm gradient, a low sun, *Sunrise*/*Sunset* label | Home weather tile |
| Refraction | Not shipped: Windows refuses geometric effects on the host backdrop | — |
