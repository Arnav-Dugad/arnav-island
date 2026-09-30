# Arnav Island 0.22.0-preview.1 — the whole island, in your hand

## Your phone controls everything
[Arnav Island for Android](https://github.com/Arnav-Dugad/arnav-island-android/releases/latest) 1.4 has a new **Island** tab. From it, on this PC:
- **Its numbers, live:**
  - processor, graphics and memory in rings, with a flowing graph of the processor
  - downloads and uploads, disk, battery (and time left), how long it's been on
  - tap for more: live graphs of the processor, graphics and network, the processor and graphics card by name, the PC's model and Windows version
- **The Controls page:** Wi-Fi, Bluetooth, airplane mode, dark mode, the microphone, mute, brightness and volume. A switch that's changing shows it.
- **The focus clock:** 15, 25 or 45 minutes or a break, pause and resume, reset, the stopwatch. It counts on the phone as it does here.
- **The command bar:** type what the island's bar takes (an app, a file, a setting, "timer 10", "100 usd in eur") and tap a result. Anything the island asks about first (restart, shut down, empty the recycle bin) asks on the phone.
- **Power:** lock, sleep, restart and shut down (each after asking), and emptying the recycle bin.
- **Where the sound goes,** with *Direct output switching* on. The phone offers to turn it on.
- **The island's pages:** open any of them on this PC, or close the island.
- **Every setting,** as the Settings window has it: switches, sliders, choices, steppers, colours and buttons. Turning off something that cuts the phone off asks first:
  - *Share with my PCs*
  - *My phone can control this PC*
  - *Reach my devices anywhere*
- While the phone shows the numbers, this PC keeps measuring them, and for 12 s after, as it does while the Stats page is open.
- It all needs *My phone can control this PC* (Settings › Privacy & productivity), like the remote.

## The phone's hotspot, one tap to join
- **When your phone's hotspot comes on, the island says so:** a card with its name, and **Join**. The phone's own view in Nearby has a Wi-Fi button for as long as it's on.
- **Join** makes a Wi-Fi profile for it and connects. If Wi-Fi is off, it's turned on first. The card then says how it went:
  - connected
  - can't be seen from here
  - couldn't join (the password, most likely); the profile is removed then
- **Windows 11 counts Wi-Fi names as location.** Nothing reads them until you press Join.
  - With location allowed (Windows asks the first time), the hotspot is found first, joined as it's secured, and confirmed by name.
  - Without it, it's joined as phones secure hotspots (WPA2, then WPA3) and confirmed by the Wi-Fi's own state.
- **The profile connects only when asked,** so this PC won't reach for your phone's hotspot later by itself.
- **The card stays up longer** when this PC has no internet of its own.
- **Only the phone knows the password.** You type the hotspot's name and password in the phone app once (Android doesn't tell apps). The phone sends them only to your paired PCs, sealed end to end, and only while the hotspot is on. The island keeps them in memory.

## The phone's battery, forecast
- **The phone's view in Nearby** says how long its battery lasts ("until 11 pm"), or when it's full while it charges.
- **The low-battery card** says when it runs out.
- **The forecast comes from the phone's own history,** which stays on the phone:
  - at first at the pace of the last hour or so
  - then more and more at the pace the phone usually keeps at each hour of the day, recent days counting most

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,724; share 199; provider lifecycle passing.
- **The phone against this PC's engine (seven tests), over the network and live over the internet:**
  - the whole island: its numbers, every setting and its change, the controls, the command bar (a yes asked first where it's needed), the outputs and the pages
  - the hotspot on and off
  - the direct path and the relay, as in 0.21
- **UI test** on the signed build: all 53 stages. They include the hotspot card waiting for its answer, going when the hotspot does, and its spoken name.
- **Settings end-to-end:** 216 checks, 0 failures.
- **On the Android emulator,** against a made-up PC: every control, each kind of setting, the command bar with its keyboard, the focus clock counting down, the stats sheet, the page tiles, and the emulator's own hotspot on and off reaching this PC's engine.
- **Join wasn't run against a real hotspot** in the tests (it would change this PC's Wi-Fi). Its card, its answers and the phone's side were tested.

# Arnav Island 0.21.0-preview.1 — straight there

## Faster on any network
- **A direct path.** When your phone and this PC are on different networks, they now try to reach each other directly first:
  - over IPv6, which most networks in India now have (Jio's mobile network is all IPv6)
  - through most home routers over IPv4, by UDP hole punching with free STUN servers
- **How much faster:** measured between the phone app and this PC's engine, a remote command takes 1–2 ms on a direct path, against about 340–500 ms through the relay (the brokers are in Europe).
- It's found within a few seconds of both being online, and kept alive every 15 s. If it goes quiet, everything carries on through the relay, even connections already open.
- It's still free, and sealed end to end with the pair's own key, exactly like the relay.
- **One round trip a command.** The phone keeps its remote connection open, so each command is one round trip with no new handshake. Before, every command opened a connection and a handshake: two round trips. Your phone's notifications and battery reach the island the same way.
- **The connection's quality ring.** Nearby shows a thin ring round each device:
  - whole and green on this network or directly
  - amber through the relay
  - a short red arc when it's slow

  Its row says how it's reached and its round trip. Hover the row to see how many relays carry it. The phone app shows the same ring.

## Pairing
- **Fixed: the pairing card hid itself.** After you scanned the QR code, the island shrank from the QR view to the "Pair with …?" card. The pointer was no longer over it, and the card folded away before you could press Pair.
  - A card that needs an answer now stays until it's answered or runs out: pairing, files offered, music handed over, the pairing code, a message to reply to.
  - Other alerts wait behind it.
- **Type a code on a PC too.** **Shelf › Nearby › Type a code** takes another PC's pairing code, in glass cells, so two PCs pair from anywhere.
- Pairing through the relay now takes about a second.
- Fixed: the command bar's suggestions showed under a reply you were typing.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,724; share 199; provider lifecycle passing.
- **The phone against this PC's engine, live over the internet (six tests):**
  - a direct path found within seconds of pairing through the relay; the remote in 1–2 ms a command; 3 MB each way over it
  - the path gone quiet on the PC's side: back on the relay within the minute, with the open remote connection carried over
  - through the relay on one broker (either side), a scanned code with another PC's key refused, and messages dropped on purpose both ways
  - the full relay test: files, the remote, notices, ring and music
- **Two of this PC's engines on the relay:** paired with a code in 1.0 s, and direct a second later.
- **UI test** on the signed build: all 53 stages, including:
  - the pairing card staying when the pointer leaves
  - another alert waiting behind it
  - typing a code in cells (its alphabet only)
- **Settings end-to-end:** 216 checks, 0 failures.
- **Captured and checked** in dark and light: Nearby's quality rings (here, direct over IPv6, relay, slow) and the code bar.

# Arnav Island 0.20.1-preview.1 — scan to pair, from anywhere

## Fixed: the phone couldn’t find this PC with a code
- **What went wrong:** each device kept to one public broker. When this PC was on one (HiveMQ) and the phone on another (EMQX), the phone never heard the code, however it was typed.
- **Now both stay on all three brokers at once** (HiveMQ, EMQX and Eclipse Mosquitto). Codes and hellos go out on every one, and each connection keeps to a broker both devices share.
- **Connecting is quicker** on networks whose IPv6 doesn’t reach a broker. IPv4 and IPv6 now race, and an address that doesn’t answer gets 4 s. On the test PC all three brokers connect in about 1.4 s.
- **Nothing is lost on the way.** A public broker may drop a message now and then. Before, one dropped message ended a pairing or a transfer. Now:
  - whatever isn’t acknowledged in time is sent again
  - a gap is reported at once and filled
  - a connection ends only when nothing gets through for 45 s

## Pair by QR code
- **Shelf › Nearby › Pair with a code** now shows a QR code beside the code. The code’s card has a **QR code** button that opens it.
- In [Arnav Island for Android](https://github.com/Arnav-Dugad/arnav-island-android/releases/latest) 1.2, choose **Devices › Scan the QR code**, and point the phone at the island.
- **The QR code also carries this PC’s key fingerprint:** the first 10 bytes of the SHA-256 of its public key.
  - The phone pairs only with the PC whose code it scanned.
  - It says yes by itself, so you confirm the six digits once, on the PC.
- Typing the code still works as before.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,724; share 199 (18 new: the QR encoder against a golden matrix, and the pairing link); provider lifecycle passing.
- **QR encoder:** every mask, versions 1 to 10 at level M, matches the reference encoder (Nayuki’s qrcodegen) module for module. The island’s own render, captured in dark and light, decodes to the link (zxing-cpp). The phone decodes the same matrix (ZXing).
- **The phone against this PC’s engine, live over the internet:**
  - the phone on one broker, this PC on all three
  - this PC on one broker, the phone on all three: first a scanned link with another PC’s key (refused, nothing paired), then the right one
  - every 5th message from this PC and every 4th from the phone dropped on purpose: pairing, the remote, presence and files both ways all complete
  - the full relay test: a typed code, files both ways, the remote, notices, ring and music
- **UI test** on the signed build: all 53 stages, including the pairing card's **QR code** button, the pairing view and **Stop**.
- **Settings end-to-end:** 216 checks, 0 failures.

# Arnav Island 0.20.0-preview.1 — your phone, anywhere

## Anywhere
- **Your phone and your PCs reach each other on any network:** another Wi-Fi, mobile data, anywhere. It’s free and needs no account.
  - On the same network they still talk directly.
  - Otherwise they meet through a free public relay (an MQTT broker over TLS). Everything is sealed end to end, so the relay sees only random-looking topics and encrypted bytes.
- **Pair from anywhere:** **Shelf › Nearby › Pair with a code** shows a code for ten minutes. Type it on the phone ([Arnav Island for Android](https://github.com/Arnav-Dugad/arnav-island-android/releases/latest) 1.1), then confirm the same six digits on both.
- Devices you have already paired find each other anywhere on their own. Nearby says **Online · over the internet**.
- **Settings › Privacy & productivity › Reach my devices anywhere** (on) turns it off.

## Your phone on the island
- **Reply from the island.** A message from your phone shows its actions: **Reply** opens the command bar as a reply box, where Enter sends and Esc cancels. Actions like *Mark as read* run on the phone. The card goes when the notification does.
- **Calls.** See who’s calling, and **Decline** (or answer) from here. The card stays while it rings.
- **Your phone’s view.** Click your phone in Nearby. You see:
  - its battery on a ring, with charging and temperature
  - storage, memory, network, sound, Android version and uptime
  - what plays on it

  Its buttons ring it, ask it for a photo, or send this PC’s clipboard to it.
- **Universal clipboard** (Settings › Privacy & productivity, off by default). What you copy here goes to your paired phones, and what they copy comes here. Password-like copies are marked sensitive on the phone.
- **Photos from my phone go on the Shelf** (on). A photo taken for this PC lands on the Shelf without asking.
- **Proximity:**
  - **Welcome my phone back** (on): a card with its battery when your phone comes back after two minutes away.
  - **Lock when my phone leaves** (off): locks this PC when your phone has left your network for 45 seconds and nobody has used the PC for 30.
- **Find this PC.** The phone can ring this PC: it chimes and shows *Here I am*.
- **The phone as trackpad and keyboard** (with *My phone can control this PC*): the pointer, clicks, scrolling, drags, typing and keys.
- **Lyrics on the phone.** The phone shows this island’s word-timed lyrics for what plays here.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,724 (18 new: settings v20); share 181 (9 new: pairing codes, the loopback pair, the status’s clipboard bit); provider lifecycle passing.
- **The phone against this PC’s engine**, on one network and over the internet relay:
  - pairing with a code
  - presence
  - files both ways
  - the remote with the new commands (find this PC, lyrics)
  - notification actions and replies
  - the phone’s details
  - notices that go
  - the clipboard pushed to the phone
  - a photo asked for
  - the trackpad’s frames
  - photos to the Shelf
- **Relay throughput:** about 0.84 MB/s through the public broker (2.5 MB, identical on arrival).
- **Native UI test:** 53 of 53. New stages cover your phone’s view, a notification’s actions and the reply box, the pairing code’s card, and the answers to *find this PC* and lyrics.
- **Settings end to end:** 216 of 216 (10 new: the five switches of settings v20, each both ways).

# Arnav Island 0.19.0-preview.2 — the phone’s remote, tested on the island

## Checks
- **The phone's remote.** The island's own answers to a phone are now part of the native UI test:
  - the status is laid out as the phone reads it, cover and all
  - links other than http and https are refused
  - a malformed seek is refused
  - nothing is answered while *My phone can control this PC* is off
  - an answer arrives the way the network passes it on: posted to the island's thread and waited for

  Only commands without effects are tried: nothing plays, pauses, changes the volume or locks during a test.
- **Native UI test:** 53 of 53.
- **Settings end to end:** 206 of 206.

Everything in 0.19.0-preview.1 is included: [Arnav Island for Android](https://github.com/Arnav-Dugad/arnav-island-android/releases/latest), the phone's remote, notifications, battery and **Ring**.

# Arnav Island 0.19.0-preview.1 — your phone, on the island

## Arnav Island for Android
- **A companion app for your phone.** [Download it](https://github.com/Arnav-Dugad/arnav-island-android/releases/latest) (Android 9 or later).
- **Pairing.** A phone pairs like a PC: with *Share with my PCs* on here, tap **Pair with your PC** in the app, and confirm the same six digits on both.
- **The remote.** The phone shows what plays here, with its cover. From the phone you can:
  - play and pause, skip, and seek
  - change the volume and mute
  - paste the phone’s clipboard here, or take this PC’s
  - open a link in your browser
  - lock this PC
- **Files, the Shelf and music.** Files go both ways, the phone can take from this PC’s Shelf, and music hands over as it does between your PCs.

## On the island
- **Your phone’s notifications** arrive as cards, with the app’s own icon, and the app and phone they came from under them. Calls stay up longer.
- **Your phone in Nearby** shows:
  - a phone instead of a laptop
  - its battery, and whether it is charging
  - **Ring**, which rings the phone loudly (even on silent) until it is found
- **Low battery.** A card when your phone falls to 20% and isn’t charging.
- **What your phone did here.** A card when it copies the clipboard, or opens a link. Password-like text stays hidden.

## Settings
- Settings › Privacy & productivity has two new switches, both on:
  - **My phone can control this PC**
  - **My phone’s notifications**

  Only phones you paired can do either.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,706 (3 new: settings v19); share 172 (7 new: the remote’s status, and the cover sent once, then *unchanged*); provider lifecycle passing.
- **Phone against this PC’s engine:** the Android app’s protocol engine was tested against this island’s own sharing engine on every feature:
  - pairing codes match
  - files both ways, including a folder
  - Shelf list and take
  - the remote: status and cover, media, volume, clipboard both ways, lock refused when not allowed
  - battery and notification notices
  - ring
  - music both ways, with the song’s file
- **Native UI test:** 53 of 53. The Nearby stage now includes a phone’s row with **Ring**, and a phone card.
- **Settings end to end:** 206 of 206, including the two new switches. Two checks were themselves wrong, and are fixed:
  - The accent check forgot that a song without a cover takes its app’s colour. It failed whenever such a song was playing.
  - The position check used a smaller on-screen margin than the island, which keeps its widest shape on the display.

# Arnav Island 0.18.1-preview.1 — rain that runs, lightning, signed updates

## Weather
- **Rain that reacts.** Raindrops on the glass now:
  - bead where they land
  - creep down when they're big enough
  - let go and run faster when the island moves (it opens, is dragged, or drops an alert)
  - merge when a running drop meets another, growing and speeding up

  A drop that runs off the bottom comes back small near the top.
- **Lightning.** In a storm, the glass lights up from inside now and then: a bright flicker, then a fainter one.
- **Air quality on the glass.** When the air is unhealthy for sensitive groups (US AQI over 100), a faint warm haze settles into the frost. It's stronger over 150.
- **The hourly curve.** The weather view draws a smooth line through the next eight hours' temperatures. A marker glides along it with the time.

## Battery
- **Health over time.** Stats › Battery › **Health over time** charts your battery's health, one reading a day. It marks the day it first fell below 90% and below 80%. **Last 24 hours** switches back.

## Updates
- **What's new.** After an update, the *Updated to …* card has a **What's new** button. It opens that release's notes in the island, and the wheel scrolls them.
- **Signed releases.** Every release is now signed with the island's own publishing certificate. The updater installs a new version only if:
  - its checksum matches
  - its program says it's that version
  - its signature is intact and made with that certificate

  A download that fails any of these is never installed, even if it was put on the island's GitHub page. The certificate is the island's own, not bought from a public authority, so Windows lists the signer as *Arnav Island Releases* without vouching for it. The updater checks the certificate itself.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,697 (13 new: drops running, merging and respawning, What's new, health crossings, release notes); share 165; provider lifecycle passing.
- **Native UI test:** 53 of 53 stages.
- **Settings end to end:** 202 of 202.
- **Signatures:** the updater's check accepts a program signed with the publishing certificate. It rejects the same program unsigned, and rejects it with a single byte changed.
- **On the glass, measured:**
  - lightning brightens the glass, then fades
  - AQI 170 warms dark glass from 24,25,27 to 50,43,38; AQI 120 is subtler
  - drops: 14 on the glass, some running
- **Visual audit:** the hourly curve and its marker, the health chart in dark and light, the Updated card and the What's new sheet.

# Arnav Island 0.18.0-preview.2 — the first update that installs itself

## Fixed
- **A test run could update itself again and again.** When the update test started the new version, it passed on the flag that says to pretend to be an older version, so that version updated again. The new version now always starts as what it is. Normal installs were never affected: without the test flags, the new version is already the newest.

## Checks
- **The field test.** This laptop ran 0.18.0-preview.1, installed by hand. When this release was published, the installed island found it, downloaded and checked it, installed it at a quiet moment and started it.
- **Native UI test:** 53 of 53 stages. The unit suites are unchanged from 0.18.0-preview.1 and pass.

# Arnav Island 0.18.0-preview.1 — updates itself, weather on the glass, every battery reading

## Updates itself
- **Automatic updates.** The island checks its GitHub releases a minute and a half after it starts, and then every six hours. When there's a newer version, it:
  1. downloads it
  2. checks its SHA-256 against the release's checksum file
  3. unpacks it and checks the program is that version
  4. installs it the next time the island is resting: compact, nobody using it, nothing announcing, none of its own music playing, nothing being shared, and no input for 20 seconds
  5. starts the new version, which shows *Updated to …*

  If the new version can't start, the old one goes back in its place and starts again.
- **Settings › About:** *Update automatically* (on by default) and **Check now**, which shows what the updater last did.
- **This version is the one to install by hand.** Laptops still on 0.17 need this release once; after that, each release arrives on its own.

## Weather
- **Weather on the glass.** On Frosted and Clear glass, when it's raining in your town:
  - rain runs down the island
  - drops bead on it
  - fog drifts across it
  - snow falls slowly

  Storms are heavier and drizzle lighter. It steps back while the island is open, so text stays clear. Settings › Appearance › *Weather on the glass*.
- **The weather's own view.** Click the weather tile on Home. It shows:
  - the temperature, what it feels like, and today's high and low
  - sunrise and sunset
  - the next eight hours: the sky, the temperature and the chance of rain
  - twelve readings: humidity, wind (direction and speed), gusts, UV index, air quality, pressure, visibility, dew point, cloud cover, the chance of rain, rain today, and daylight

  Readings follow your unit: Fahrenheit brings mph, miles, inches and inHg.

## Battery
- **All details.** Stats › Battery › **All details** shows everything the battery and Windows report, three to a row. The wheel pages through them.
  - health and wear
  - design capacity, full charge and what's left
  - charge or drain rate, voltage and current
  - cycles and temperature
  - chemistry, manufacturer, model, serial number and date made
  - the levels at which Windows warns, and any capacity held in reserve
  - Windows' own estimate, power mode and battery saver
  - the last seven days: hours on battery, charge used per day and charges
  - how health has changed since the island started keeping track

  Anything your battery doesn't report is left out, not shown as a dash.
- **How health is measured:** the battery's full-charge capacity divided by its design capacity, as its own firmware reports them to Windows. It's the same figure as Windows' battery report.

## Fixed
- **Alerts no longer vanish while the island is open.** A device, charging, headphone, camera, microphone or location alert that arrived while the island was open, in Live or in the command bar was thrown away. It now waits and shows as the island settles. One that waited over a minute is dropped as old news.
- **Low battery** warned again at every percent below 10. It now warns once each at 20%, 10% and 5%, and plugging in resets it. At 5% it says *Almost out of charge*.

## Settings
- **23 more rows show what they do** when you rest on them, 37 in all. New: everyday mode, auto-hide, hover to open, compact media, live waveform, media controls, swipe to skip, compact lyrics, the glance, glance rings, the level indicator, weather, magnetic buttons, animated icons, track handoff, reduce motion, Island DJ, device, charging and copy cards, privacy dots, artwork colours and weather on the glass.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,684 (28 new); share 165; provider lifecycle passing.
- **Native UI test:** 53 of 53 stages. New: an alert waits while the island is open and shows as it settles; battery details and the weather view open and close.
- **Settings end to end:** 202 of 202, with weather on the glass and automatic updates each switched both ways.
- **Updates, end to end, against the live GitHub release.** A copy of this build pretending to be 0.17.0-preview.2:
  - found 0.17.0-preview.3
  - downloaded and verified it
  - installed it in its folder, closed itself, and the new version started
  - pretending to be 0.17.0-preview.3 instead, it reported it was up to date
- **Visual audit:** the weather view in dark and light, battery details, weather on the glass (rain, fog, snow; resting and open), every Settings section and all 37 row previews.

# Arnav Island 0.17.0-preview.3 — glass that listens, frost that settles, Settings that show you

## Fixed
- **The ends of the island's curves.** Where the shoulders meet the body, the glass changed colour and texture. There were two causes:
  - The pointer's light stopped at the body.
  - The edge light ran down the join, not the edge.
  The light now runs across the shoulders, and the edge light starts below the join. Measured on Frosted glass over a plain backdrop, the shoulder and the body now match to within one step of colour.
- **Lines across the island.** For a few seconds after an alert, lines could show across the island. They came from the light that runs along the edge: it was drawn through hard-edged windows. It now rises and fades away smoothly at both ends.
- **Long titles** are tightened (down to 86% of their size) before anything is cut, so more of a song or file name fits.

## Glass
- **A glint on the rim.** On Frosted and Clear glass, the rim brightens right under the pointer and fades away along it.
- **Frost that settles.** While the island rests, Frosted glass slowly turns denser and milkier. It starts after a few seconds and takes about half a minute. It clears in a third of a second when you point at the island, open it, or an alert arrives. Turn it off in Settings › Appearance › *Frost that settles*.
- **An edge light to the beat.** While music plays, the island's rim breathes with the bass. It rests brighter when the bass is heavier, flares on each hit, and falls back over about half a second. On glass it runs round the shoulders too; on Solid it's a soft light inside the free edges. It uses the song's accent colour. Turn it off in Settings › Media & sound › *Edge light to the beat*. It is off with Reduce motion.
- **Separate tints.** Frosted and Clear each have their own tint, so changing one no longer changes the other. Settings › Appearance › *Frosted tint* and *Clear tint*.

## Settings
- **A live preview of the glass.** Settings › Appearance › *Preview* shows your island over a moving wallpaper, in your theme, material and tint. Point at it and the glint follows the pointer. Leave it, and Frosted glass frosts over. With *Edge light to the beat* on, the rim keeps a beat.
- **Rows that show what they do.** Rest the pointer on one of 14 rows for about half a second and it opens a small moving picture of the setting:
  - Theme, Frosted tint, Clear tint and Frost that settles
  - Corner radius, Soft shadow, Compact width and Dock edge
  - Light along the edge, Alerts and Two alerts at once
  - Live audio style, Artwork pulses to the beat and Edge light to the beat

  Move to another row and it closes. Clicking never opens one.

## Weather
- **Sunrise and sunset.** The Home weather tile warms around sunrise and sunset in your town:
  - lavender to peach at dawn
  - indigo to rose and orange at dusk
  - the sun low on the horizon

  The tile reads *Sunrise* or *Sunset* then. The times come with the forecast from Open-Meteo, in your town's own time zone.

## Not in this release
- **Refraction that bends the wallpaper.** Frosted glass is drawn from the blurred desktop that Windows hands to apps. Windows accepts colour and blur effects on it, but it refused every effect that moves pixels (a 2D transform and a scale, with each option tried). Clear glass shows the desktop directly, and apps can't resample it at all. Bending the wallpaper would mean copying the screen behind the island continuously, and the island doesn't do that. The rim still gathers light at the edges.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,656 (35 new); share 165; provider lifecycle passing.
- **Native UI test:** 50 of 50 stages, including Clear and Frosted glass through every shape with no glass errors.
- **Settings end to end:** 198 of 198, including both tints, the frost and the beat light, each switched both ways.
- **Glass, measured over a plain test backdrop:**
  - frost: dark 32,36,44 → 43,45,49; light 125 → 136
  - beat light: rim 43 → 64–96 with the music, back to rest without it
  - glint: rim 45 → 88 under the pointer, fading on both sides
- **Visual audit:** every Settings section and all 14 row previews, drawn by the Settings window itself.

# Arnav Island 0.17.0-preview.2 — glass fixed, lyrics on time

## Fixed
- **Frosted and Clear glass.** In 0.17.0-preview.1 the glass stopped following the island. Every time the island changed shape, the glass tried to animate a value it had never created, Windows refused, and the rest of that update was skipped. The glass is back, and the island's own test now fails if the glass ever reports an error. Two more causes of broken glass are fixed:
  - **Long expression.** The shape of a waiting alert's card was one expression too long for Windows to accept. It's now built from short steps.
  - **Scientific notation.** A spring that had nearly settled wrote numbers like `2e-05`, which Windows' expressions don't accept. The glass could freeze for a moment when that happened, and this goes back to earlier versions. Numbers are now always plain decimals.
- **Lyrics on time.** Windows reports where a player is as of the moment that player last told it. Many players, Spotify among them, tell it only now and then. The island took that report as the current position, so lyrics could run several seconds behind and jump back. It now adds the time since the report. Tested with a real Windows media session: the report was up to 5.9 s behind the music; corrected, the island is within a hundredth of a second.
- **Offer cards keep the size.** A long file name no longer pushes the size off the card. The name is shortened instead, as in *Holiday pho… · 12 files · 48.2 MB*.
- **Outputs** show a speaker for speakers and headphones for headphones, from Windows' own device type.
- **The command bar** shows the right icons for the weather, a song, shuffling and continuing on another PC. *weather* alone reads *Opens Settings › Compact › Town*.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,621 (8 new); share 165; provider lifecycle passing.
- **Native UI test:** 50 stages, 5 new. Clear and Frosted glass go through opening, the command bar, two alerts dropping and spreading, and back, with no glass errors allowed at any stage.
- **Settings end to end:** 192 of 192.
- **Visual audit:** every page, tab, card, compact mode, side dock and command state, in dark and light, drawn by the island itself (78 views). Every Settings section and page, drawn by the Settings window itself (18 views).

# Arnav Island 0.17.0-preview.1 — Up next, your other PC's Shelf, and an island you can hear

Phase 5H:
- The weather's town is chosen in Settings, and finds towns anywhere.
- Up next, with drag to reorder. The next song's cover peeks out from behind the one playing.
- Crossfades between songs. Music fades out when it moves to your other PC, and fades in there.
- The other PC's cover on *Play here*.
- Take files from your other PC's Shelf.
- A ring for big transfers that shows their speed and time left.
- Two alerts spread side by side when you point at them.
- Screen readers can read and use the island.
- Many visual fixes.

## Fixed
- **A logo in the command bar.** The search page could show the icon of the last alert, like a pair of headphones' brand logo. The command bar never shows an alert's icon now.
- **The cover on top of the Library.** With a song playing, its cover sat over the Library list. The cover now shows only on the Now playing view.
- **Rows touching the footer.** Shelf, Clipboard, Nearby and Apps rows ran into the page's footer. The chevron of the Outputs button sat outside it.
- **The weather sky over text.** The Home tile's sky was drawn over the tile's words.
- **Light theme on glass.** Text had too little contrast.
- **Settings.** Long descriptions were cut off.
- **The first command.** The command bar's first search of a session could wait on the Wi-Fi and Bluetooth state; it no longer does.
- **One town listed twice.** Two places with the same name (a town and its district) showed as two identical rows.

## Weather
- **Choose the town in Settings.** Settings › Compact › **Town**: type the town, then pick it from the list. Towns show with their region and country, so you can tell *Manipal, Karnataka, India* from *Manipal, Gandaki Pradesh, Nepal*. It works for towns anywhere: Manipal, Udupi, Jubail, Indore, Jaipur, Mumbai and so on.
- Settings then shows *Showing the weather for …*, with the temperature and the sky.
- Typing *weather* alone in the command bar opens the Town field. *weather Mumbai* still works.

## Music
- **Up next.** On the Media page, the queue button (or the next song's cover, peeking out from behind the one playing) opens **Up next**: the songs after this one.
  - Drag a row up or down to move it. The others glide aside.
  - Click a row to play it now.
  - Scroll with the wheel or the arrows.
- **Crossfade.** Songs the island plays blend into each other over 6 seconds. Change it in Settings › Media & sound › *Crossfade*; 0 turns it off. Skipping fades the old song out quickly instead of cutting it.
- **Handoff fades.** Music moving to your other PC fades out over about a second and a half before it pauses. For another app, like Spotify, the island lowers that app's volume, pauses it, and puts its volume back. On the other PC, the song rises in.
- **The cover comes along.** *Play here* on the other PC shows the song's cover, and a thin line shows how far into the song it is.

## Sharing
- **Take from your other PC's Shelf.** Shelf › Nearby › **Shelf** on a paired PC shows what's on that PC's Shelf, with previews and sizes. Click one to take a copy. It lands in Downloads and on this PC's Shelf, and the other PC tells you it was taken. Turn it off on a PC with Settings › Privacy & productivity › *My PCs can take from the Shelf*.
- **Big transfers.** A transfer of 5 GB or more shows as a ring in the compact island that fills as it goes and widens with its speed, with the speed beside it and the time left. Every transfer's row in Nearby shows its speed and time left too.
- **Compatibility.** This is revision 1 of sharing protocol 2, so 0.16 and 0.17 still share files and music with each other. The cover and taking from a Shelf need 0.17 on both PCs; the **Shelf** button shows only for PCs that have it.

## Alerts
- **Side by side.** When two alerts are out, point at them. The first slides left, and the waiting one comes up beside it as a card of the same height. Click it to bring it forward. Move away and they close up again.

## Screen readers
- **An island you can hear.** Narrator, NVDA and JAWS can now find the island as *Island*. It opens and closes like a menu, and says what it shows: *Open on Media. Playing Blue in Green by Miles Davis*. Everything on it that can be pressed has a name and a role:
  - buttons, like *Play*, *Up next*, *Take a copy of Trip itinerary.pdf*
  - switches that say whether they're on, like *Wi-Fi* and *Dark mode*
  - sliders with their values: volume, brightness, the song's position and each app's volume
- **Announcements.** Alerts are spoken as they arrive, with what you can do (*Music from another PC: Blue in Green. Play here, or not now*). The page that opens, the song that starts and volume changes are spoken too.

## Checks
- **Unit suites:** core 11,682; model 2,061; phase 5,613; share 165 (46 new).
- **Native UI test:** 45 stages, 3 new: the side-by-side alerts and their hit test, Up next and the other PC's Shelf, and spoken names.
- **Settings test:** end to end, passing.
- **Live checks:**
  - a crossfade between two of Windows' own sounds starts one second before the end, and the queue moves on once
  - a screen-reader client read the island, pressed its buttons and switches, and heard its alerts
  - the Town field found and saved all six towns above

# Arnav Island 0.16.0-preview.2 — music from the island, sharing that goes where you drop it

**preview.2** fixes a developer-test problem: when the settings test switched *Remember recent commands* off, it deleted the real command history file of whoever ran it (test runs now leave it alone). Nothing else changed since preview.1.

Phase 5G:
- Drop files and folders straight onto your other PC, or send the whole Shelf at once.
- Play songs from your Music folder in the island itself, and hand the music over to your other PC.
- Two alerts at once: the second buds off the first.
- Drag the music sideways to skip, anywhere it shows.
- The clipboard remembers after a restart.
- Weather works again.
- Sounds, Island DJ and a liquid navigation pill.

## Sharing between your own PCs
- **Drop onto a PC.** Drag files or folders over the island. It shows the Shelf on one side and your paired PCs on the other; let go over a PC to send them there.
- **Folders.** A folder arrives whole, with everything inside it, under its own name in Downloads (*Photos (2)* if you already have *Photos*).
- **The whole Shelf at once.** Each paired PC in Shelf › Nearby has **Send Shelf**. The Shelf's item count is also a stack: drag it onto a PC in the island, or out of the island to drop every file anywhere.
- **See it move.** A PC's row fills as a transfer goes, and **Stop** ends it from either PC. The compact island shows a chip with the percentage.
- **Clearer answers.** Offers say how many files there are and how big they are. The other PC is told when you stop sending. If Downloads doesn't have room, you're told before anything is written.
- **Update both PCs.** This version's sharing (protocol 2) needs 0.16 on both PCs. A PC still on 0.15 shows *Needs the latest Arnav Island* instead of failing silently.

## Music
- **Play from the island.** Media › **Library** lists the songs in your Music folder (MP3, M4A, AAC, FLAC, WAV, WMA, Ogg and Opus) with their covers. Pick one, or press **Shuffle all**. With nothing playing, the Media page offers **Shuffle my music**; in the command bar, type *play* and a song, or *shuffle*.
- **A real player.** Songs the island plays appear in Windows' own media controls. The keyboard's media keys control them (checked), and Windows' media flyout shows them. Starting a song pauses whatever else was playing.
- **Continue on my other PC.** With sharing on, the Media page's **Continue on** button (or *continue on* in the command bar) offers what's playing to a paired PC. The other PC shows *Play here*. It plays the song from where you were:
  - in a player that already has it
  - from its own Music folder
  - from the song's file, sent along when the island was playing it
  - by opening the same app, for Spotify and Store apps
  
  This PC then pauses.
- **Drag to skip, everywhere.** A sideways drag over the music now skips tracks in the compact island, the Live Island, Home and the Media page. A chip on the leading side grows as you drag and snaps when letting go would skip. Before, the Live Island only switched between players on a drag, and with *Open on hover* the island had usually opened into it by the time you dragged. Players now switch with the dots under the cover.
- **Island DJ.** Near the end of a track, a halo breathes behind the compact ring in the colours of what plays next. The island's own queue knows the next song; for other apps it uses the colours of what plays now. A new track blooms in its cover's colours as it starts.

## Alerts
- **Two alerts at once.** When a second alert arrives while one shows, it grows out of the first pill's foot as a bud, then lets go and settles just below it. It takes the pill's place when the first ends; click it to bring it forward. Up to four can wait, and a pairing code, file offer or music offer is never lost.
- **Sounds.** A faint two-note chime plays with the light along the edge, and a soft click as chips move in Settings › Compact. They're made by the app itself (no sound files), quiet on purpose, and silent while something plays full screen.

## Clipboard
- **Remembers after a restart.** The clipboard history is saved on this PC, encrypted for your Windows account, and comes back when the island starts. Images are kept as PNG. Copies that look like passwords or keys are kept only if you pin them. Turn it off in Settings › Privacy & productivity › *Remember the clipboard after restarts* (the saved copy is deleted).

## Weather
- **Fixed.** *Open-Meteo couldn't be reached* appeared on every lookup: Open-Meteo answers in a compressed form Windows' web client couldn't unpack. The island now asks again without compression when that happens. If the network really is down, it keeps your town and tries again every two minutes. If you tried a town on 0.15, type *weather* and the town once more.

## Motion
- **A liquid navigation pill.** The highlight under the navigation stretches toward the page you pick, its leading end first, then draws its tail in, keeping its corners round.

## Settings (version 15)
- **New and on:**
  - remember the clipboard after restarts
  - sounds
  - continue on my other PC (only with sharing on)
  - Island DJ
  - two alerts at once
  - music library
- Nothing new goes online. The library reads your Music folder on this PC only, and handing music over goes only to your own paired PCs.

## Limits
- **Handoff** plays the same song on the other PC when a player there has it, when your library there has it, when the island was playing it from a file, or when the app itself resumes it. Otherwise it opens the same app and presses play, and moves to where you were once that app shows the same song; whether the app picks up the same song is up to the app. Songs in a browser tab can't follow, because browsers don't expose the page.
- **Sharing and handoff** were tested end to end between two independent services on one PC (loopback), not between two physical PCs.
- **The music library** reads one folder: your Music folder, up to 8,000 songs, 12 folders deep. The list is kept in memory.
- **Island DJ** knows the next song only in the island's own queue.
- **Two alerts at once** needs the drop pill (top dock).
- **Not on the lock screen** as an island (Windows' secure desktop).
