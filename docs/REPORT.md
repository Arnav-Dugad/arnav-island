# Arnav Island v0.17 — development report

## What changed

**Weather.**
- **The Town field** is a new Settings control: a text field with a caret, selection keys and paste, and up to six results below it. Typing sends a search 320 ms after the last key. The island's weather service answers on its worker (`search`, then `WeatherMessage` 2), and a result picked with Enter or a click is saved as it is (`choose`), with no second lookup. The field's row grows to fit its results and scrolls into view.
- **Names** are *Town, Region, Country* (`parseGeocodeAll`, from Open-Meteo's `admin1`), one row per name.
- **The command** *weather* alone opens the field.

**Music.**
- **Two decks.** `IslandPlayer` keeps two media engines. A crossfade starts the next song on the other deck at volume 0 and ramps both on equal-power curves. The fading deck's own end or error only empties it, so the queue moves once. A skip fades the old song out over 0.35 s.
- **Ramps** run on the island's timer only while needed: 30 ms while a volume moves, 60 ms in the last seconds of a song, 500 ms otherwise, none when nothing can crossfade.
- **Handoff fades** use the same ramps for the island's songs. For other apps they use the app's volume in Windows' mixer: lowered on a cosine, paused, then put back.
- **Up next** is `upNext` / `move` / `jump` on the queue after the song playing. The Media page's view drags rows with the pointer: a press that moves more than 5 DIPs vertically lifts the row. The rows glide toward their slots with a 75 ms time constant, redrawn at 60 Hz only while they move.
- **The next cover** is drawn in the content surface, behind the cover visual, rotated 5°, so its edge shows.

**Sharing, revision 1.**
- **Revision.** Discovery sends `port;2.1`. A 0.16 PC reads the 2, and a 0.17 PC reads the revision too. Nothing a 0.16 PC receives changes.
- **Covers** follow a music offer's text after a zero byte (a zero can't occur in the text: it is stripped).
- **Shelf** listing (`L`) and taking (`T`) are new modes. The owner lists up to 32 items and serves only an item at the listed place with the listed name, so a Shelf changed since the list is refused (*It's no longer on …'s Shelf*). A take is an ordinary transfer, so progress, Stop and the checks all apply.
- **Speed** is an eased average of bytes over quarter-second samples.

**Alerts.** Spreading is a spring from 0 to 1. The pill's offset and the bud's shape are functions of it in all four places the drop pill already lives: the DirectComposition clip curves, the glass expressions, hit tests and the window region. Leaving starts a 380 ms timer, so moving across the gap between the two doesn't close them.

**Screen readers.** A UI Automation fragment tree from the same hit targets the pointer uses. Names are composed per action, from what each shows (a song's title, a file, a PC). Switches report their state and sliders their range. Notification events are raised only while a client listens.

**Fixes.**
- **Stray logo.** The command bar showed the last alert's icon.
- **Cover over lists.** The cover sat over the Library and Up next.
- **Rows and footers.** Rows ran into their footers, and the Outputs chevron sat outside its button.
- **Sky over text.** The weather sky was drawn over the Home tile's text.
- **Contrast.** Light-theme text on glass was too faint.
- **Settings text.** Long descriptions were cut off.
- **Fill width.** The solid fill was 40 DIPs short of the canvas.
- **Command bar.** It could open without its service in test runs, and its first search waited on the radios.
- **Duplicate towns.** The same town could be listed twice.

## Verification

- **Unit suites, all passing:**
  - **Phase: 5,613 checks** (29 new). They cover:
    - v16 settings, their defaults and limits
    - the Settings items
    - the side-by-side geometry, which stays below the pill unspread, sits beside it spread, and glides with corners that always fit
    - time-left wording
    - regional town names, one row per name, and search limits
  - **Share: 165 checks** (46 new). They cover:
    - covers byte for byte, including zero bytes, and never sent to a revision 0 PC
    - revision 0 PCs not asked for their Shelf
    - a closed Shelf
    - a listed Shelf with previews and folder sizes
    - taking a file and a folder tree
    - the owner told
    - an item since removed, or at another place, refused
  - **Model: 2,061. Core: 11,682. Provider lifecycle: pass.**
- **Native UI test: 45 stages**, 3 new:
  - two alerts spreading, with the side card's hit test and the old place no longer answering
  - Up next's rows and arrows, and another PC's Shelf with its items, back button and the Nearby row's Shelf button
  - every target on the Controls page having a spoken name
- **Settings end to end:** passing. The data folder was unchanged afterwards apart from the test's own files and the event log.
- **Live checks on this PC:**
  - **Town search:** the field found Manipal (two regions) and saved Manipal, Udupi, Jubail, Indore, Jaipur and Mumbai, each with its weather.
  - **Crossfade:** two of Windows' own sounds (muted) with a 1 s crossfade. The second began 1 s before the first ended, and the queue moved exactly once. Moves in Up next were accepted and bad ones refused.
  - **Screen readers:** a UI Automation client read the pane, its expand state and its children, with names, roles, rectangles, switch states and slider values. It pressed *Stats*, collapsed and expanded the island, and heard an alert announced.
- **Captures** were drawn by the island itself, with illustrative data only:
  - side by side, and the Play here card with a cover
  - Up next mid-drag, and the peeking cover
  - another PC's Shelf, and the big-transfer ring

## Limits

- **Two PCs.** Sharing, handoff, covers and Shelf taking were verified between two independent services on one PC, not between two physical PCs. The cover and Shelf taking need 0.17 on both.
- **Crossfade** is for the island's own songs.
- **The Settings window** is not yet exposed to screen readers. The island is.
- **Side by side** needs the drop pill (top dock).
- **Unsigned preview.**

## 0.17.0-preview.2

**Glass.** Three separate failures, each making `GlassBackdrop::animate` or `layout` throw and leave the glass behind:
- **`sp` and `ss`** (the side-by-side spring and shift) weren't created with the property set. Starting a spring on a missing property fails with `E_INVALIDARG`, before the width, height and radius springs start. Fixed with one list, `glassProperties`, that creates them all. A phase test scans every expression literal for `p.<name>` and every started spring, against that list.
- **The bud's corner** was a single expression over a thousand characters long, written twice in a `Vector2`. It is now built from properties (`bt0`, `bb0`, `bw0`, then `bl`, `bt`, `bv`, `bb`, `bc`), each from a short expression.
- **`expressionNumber`** used `%.9g`, which writes exponents for tiny values (a settling spring's terms). Expressions reject them. It now writes plain decimals to about nine significant digits, and treats anything under 1e-9 as 0.
- **Why this wasn't caught.** The UI test checked that the glass was visible, never that its animations had started. It now reports the glass's last error, and the step that failed names the property and expression.

**Lyrics.** `MediaProvider` now calls `timelinePosition`: while playing, the reported position plus the time since the timeline's `LastUpdatedTime` (from the future, missing or more than six hours old: ignored). The live check used Windows' `MediaPlayer` on one of Windows' own sounds:

| Moment | True position | Reported | Report's age | Corrected |
|---:|---:|---:|---:|---:|
| 3 s | 2.91 s | 0.00 s | 2.91 s | 2.92 s |
| 6 s | 5.91 s | 0.00 s | 5.91 s | 5.91 s |
| 9 s | 8.91 s | 6.04 s | 2.87 s | 8.91 s |

**Audit.** 78 island views from its internal render, and 18 Settings pages saved by the Settings window itself (`--qa-settings-sweep`, test runs only), reviewed one by one. No screen was captured.

## 0.17.0-preview.3

**The shoulder seam.** Two things differed between the body and the shoulders:
- The DirectComposition sheen was a child of the body, so its clip cut it off at the shoulders. The light now lives in the glass: a sprite in each of the body and both shoulders, placed by `p.px`, `p.py` and faded by `p.po` on its own clock (`p.tp`).
- The left and right edge-light strips started at the body's top, which (when docked) is the shoulders' inner edge, so light ran down the join. They now start `p.r` lower while the island is docked.

Over a plain backdrop, Frosted: shoulder 31,34,41 and body 30,34,41.

**The lines.** The alert glint was a band swept by one clip rectangle whose ends were hard. It's now four bands (core and glow, each way), each drawn through six nested windows at a sixth of the brightness. The light rises and falls over the width of the windows.

**Why refraction isn't in.** Probed at run time:

| Effect over the host backdrop | Result |
|---|---|
| Colour matrix | Accepted |
| Gaussian blur | Accepted |
| Border | Accepted |
| 2D affine transform (matrix only, three or four properties, each interpolation and border mode, identity) | `E_INVALIDARG` |
| 2D affine transform after a colour matrix | `E_INVALIDARG` |
| Scale | `E_INVALIDARG` |

Composition never asked for a named property mapping, so the effect itself is refused. The code that tried it was removed; the edge light is unchanged.

**The frost.** Measured over a plain backdrop, from the island's own capture, with the settling sped up for the test (`--qa-frost`):

| Theme | Rest | Settled |
|---|---|---|
| Dark | 32, 36, 44 | 43, 45, 49 |
| Light | 125, 128, 130 | 136, 138, 141 |

A first test looked like the frost did nothing. The sped-up pace was set after the frost had started at its real pace. The test hook now restarts it.

**The beat light.** `Renderer::beat` compares the bass (50–130 Hz) with its half-second average. The level rests at 0.14 plus up to 0.34 with the average, flares within 70 ms on a hit, and falls back over 0.55 s. On glass, the rim's two beat strokes take their colour's alpha from `p.be`, a Hermite glide on its own clock (`p.tb`). The loopback analyser now also runs for the light while music plays and the island is visible. The bars' `waveform` flag is unaffected.

**Settings previews.** Rows gain `base` (their own height; controls stay centred in it) and `open`. A click kills the 450 ms dwell timer, so the Settings test, which moves and clicks at once, never opens one. The window draws at about 30 fps while a picture moves, and `settled` ignores it.

**Found in the audit.**
- **The low sun on the label.** At dawn and dusk, the low sun sat on *Sunrise* or *Sunset*. The word now moves left of it.
- **Live audio style preview.** The ring sat over the preview's title line. The line now stops short of it.
- **The Settings test** left Material on Clear, so it clicked Frosted's tint and frost while they were disabled (4 failures). It now picks the glass each of those rows belongs to first.

## 0.18.0-preview.1

**Updates.** `UpdateService` asks `api.github.com/repos/Arnav-Dugad/arnav-island/releases` (90 s after start, then every six hours; after a failure, in 30 minutes).

It picks the newest non-draft release newer than the running version, and only if both assets exist and come from `https://github.com/Arnav-Dugad/arnav-island/releases/download/` (`newestRelease`). Then it:
1. downloads the checksum and the ZIP
2. checks the SHA-256 (BCrypt)
3. unpacks with `%SystemRoot%\System32\tar.exe`
4. requires the unpacked `ArnavIsland.exe`'s ProductVersion to equal the release's version

Installing, at a quiet moment:
- The running program is renamed `ArnavIsland.old.exe` (a running image can be renamed, not overwritten) and the new one is copied in.
- The new program starts only after this process has closed its single-instance mutex (`UpdateService::launchPending` from `wWinMain`), so any version, even one that doesn't know `--after`, starts cleanly.
- If it can't start, the old program is moved back and restarted.
- The next start deletes `ArnavIsland.old.exe`.

The first end-to-end run showed why the relaunch moved. Starting the new version from inside the old one, the older build found the mutex still held and quit.

**Alerts.** `showNotice`, `showHeadphoneCard` and `showPrivacyNotice` returned early unless the island was compact or already showing an alert. So anything arriving while it was open, in Live or in the command bar was lost, including camera and microphone cards. They now go to `deferCard`: the held-card queue, marked deferred and time-stamped. `promoteCard` shows them when the island returns to compact, and drops deferred ones over 60 s old.

**Battery.** `BatteryProvider::query` now reads:
- `DefaultAlert1` and `DefaultAlert2`, `CriticalBias` and `Technology`
- the serial number, and the optional temperature, manufacture date and estimated time
- battery saver and Windows' lifetime (`GetSystemPowerStatus`)
- the number of batteries

This PC's battery reports capacities, voltage, chemistry, manufacturer, model and serial, but no temperature, date, cycles or alert levels. Its chemistry code is a vendor code (`OOI0`), which is now left out rather than shown.

**Weather on the glass.** DirectComposition visuals in the body, above the glass, below the content:
- Rain or snow is one tile drawn twice and scrolled forever (`IDCompositionAnimation::AddRepeat`).
- Drops are drawn once, three in five within the resting island's height.
- Fog is two tiles drifting sideways.

It first showed only while the island was open: the resting island redraws its header only, and the effect was updated on full redraws. It now updates on both.

## 0.18.1-preview.1

**Signing.** A code-signing certificate, `CN=Arnav Island Releases, O=Arnav Dugad` (RSA 3072, SHA-256, to 2036), is kept in the publisher's `Cert:\CurrentUser\My`. It is exportable, so it can be backed up with `Export-PfxCertificate` and a password of the publisher's choosing.

`scripts/package.ps1` signs `ArnavIsland.exe` with it (`Set-AuthenticodeSignature`, SHA-256, DigiCert timestamp). It refuses to package if the certificate is missing, or if the signature doesn't verify as that certificate's.

`signerSha256` runs `WinVerifyTrust` with no revocation checks and no network. It accepts success or `CERT_E_UNTRUSTEDROOT`: the signature is intact, and the root is simply one Windows doesn't know. It then hashes the signing certificate from the provider's chain and compares it with the SHA-256 pinned in `UpdateService.h`. A tampered file returns `TRUST_E_BAD_DIGEST` and yields no signer.

| File | Signer | Accepted |
|---|---|---|
| The build, unsigned | none | no |
| The build, signed with the publishing certificate | d4cbca03… | yes |
| The same, one byte changed | none | no |

**Drops.** `GlassDrops` steps each drop:
- Drops under 1.5 DIP cling.
- Larger ones run at 7 DIP/s per DIP of radius over 1.5.
- A shake (from any change of the island's springs' targets, decaying over 0.9 s) quadruples the speed and lets clinging drops go.
- A running drop merges with any drop within 0.9 of their combined radius (√ of the summed squares, at most 5.2 DIP).

Each drop is a DirectComposition visual gliding linearly between steps. A merged drop slides into its taker as it fades.

**The now marker.** The curve is Catmull-Rom through the hours, drawn as Béziers. For the span now running, the marker's y is that span's Hermite cubic rewritten in seconds, so DirectComposition moves it along the curve exactly. Its x is linear in time.

**What's new.** The updater keeps the release body with the download, and writes `whats-new.md` next to the settings when it installs. The new version reads it once (`whatsNewLines`: headings, points, Markdown removed, the Checks section left out), then deletes it.

**Found in the audit.**
- The Updated card showed a Bluetooth icon: the card-icon layer had no case for it.
- Home's volume bar showed through the What's new sheet, as it had through the weather view.
- The health chart's 80% label collided with the "Below 80%" marker.
- The first haze was stronger than "faint".

## 0.19.0-preview.1

**Revision 2.** PCs announce `2.2`; phones announce `2.2;phone`. Three modes are added, each open only to paired devices:

| Mode | Frames |
|---|---|
| **R**, the remote | Request `[0x20, command, payload]` gets reply `[0x21, status, payload]`. Status is ok, not allowed, unsupported or failed. |
| **N**, notices | `[0x30, 1, battery, charging]`, or `[0x30, 2, urgent, text, icon]`. The text is `app\ntitle\ntext\nphone`; the icon is a PNG of at most 24 KB. Answered `[0x31]`. |
| **F**, find my phone | `[0x40]`, answered `[0x41]`. |

The status payload holds, in order:
1. flags: available, playing, previous, next, toggle, muted, charging, seek, battery present
2. position and duration (f64)
3. volume, battery and CPU (CPU 255 means unknown)
4. the cover, which is one of: none; its SHA-256, length and JPEG; or *unchanged*, when it matches the hash the phone sent
5. five lines: title, artist, app, PC name, weather

**On the island's thread.** `ShareService` calls the island's remote handler on a network thread. The handler posts `RemoteMessage` with a heap-held call carrying a promise. The UI thread answers from what the island shows, and the network thread waits up to 4 s. The island encodes a cover once per picture: 320 px, or 200 px when that would be over 96 KB. CPU is busy time over all time between two asks (`GetSystemTimes`).

**Phones remembered.** Paired phones are listed in `share-phones.nexus`, apart from the peers file, so older versions still read that file as before.

**The phone app's engine** is plain Kotlin, and `tests/SharePeer.cpp` (`share_peer`) exposes this island's `ShareService` to it over loopback. The app's `InteropTest` drives it through every mode:
- pairing (the codes match)
- files and folders both ways
- the Shelf
- the remote: status, cover once, then *unchanged*
- notices
- ring
- music both ways, with the song's file

The ECDH secret derivation (CNG's `KDF_HASH` over Z) and Java's `ECDH` with SHA-256 of Z agree.

## 0.20.0-preview.1

**Revision 3.** PCs announce `2.3`, phones `2.3;phone`. What's new:

| Part | Frames |
|---|---|
| Remote commands | Ring this PC (10). Lyrics (11), answered `[ok, state, key, count, lines]`; each line holds its time, text, and each word's time and UTF-16 start. |
| **N** notices | Type 2 adds the key and up to three actions (flags, then the title). Type 3 is the phone's details, `name\tvalue` lines. Type 4 says a notification is gone. |
| **A**, an action | `[0x70, key, index, reply]`, answered `[0x71, status]` (done, gone or failed). |
| **C**, the clipboard | `[0x50, sensitive, text]`, answered `[0x51, status]`. |
| **K**, the camera | `[0x42]`, answered `[0x43, status]`. |
| **I**, input | A stream of move, button, scroll, text and key frames, until the phone stops or two minutes pass. |

A few smaller changes:
- An offer's flags byte adds *to the Shelf* (bit 1), sent only to revision 3.
- The status adds bit 9: the universal clipboard is on.

**The relay** (`ShareRelay`) carries the same protocol between networks. Each side connects to public MQTT brokers over WebSocket and TLS (WinHTTP on Windows, a TLS socket on Android), with MQTT 3.1.1, QoS 0 and a clean session. Since 0.20.1 it stays on all three at once: broker.hivemq.com, broker.emqx.io and test.mosquitto.org.
- Hellos and pairing codes go out on every broker.
- A device counts as here when it was heard on any broker in the last 150 s.
- A tunnel keeps to one broker: the one the other device was heard on most lately. A code's tunnel opens on all, and keeps to whichever the other device answers on.
- A broker that drops ends only the tunnels on it.
- Before 0.20.1 each side kept to the first broker that answered. Two devices on different brokers never met, which is why a phone couldn't find a PC by its code.

A paired pair's secret S is SHA-256 of `arnav-relay-v1` and the pair's ECDH agreement. Topics are `arnavisland/r1/` followed by 40 hex characters of SHA-256 over:
- `inbox`, S and the device's id, for a device's inbox
- `host` or `guest` and the code's hash, for pairing codes

Every message is AES-256-GCM with a fresh nonce and the topic as associated data. It carries one of:
- **hello:** presence, revision, phone, name
- **open**, **data**, **ack** and **close:** a tunnel

A tunnel is joined to a local loopback socket pair, so the ordinary handshake, pairing check and per-connection keys run through it unchanged. Data moves in 48 KB chunks, 64 in flight, acknowledged every 24; that measured 0.84 MB/s through a public broker. Hellos go out once a minute, and a device silent for 150 s counts as gone.

**Retransmission (0.20.1).** A broker at QoS 0 may drop a message. In testing, EMQX dropped three small messages in a row in the middle of a transfer, and the tunnel used to end at the first gap. Now:
- The sender keeps each message until it is acknowledged. The end of each burst asks for an acknowledgement.
- A message not acknowledged within 1.5 s is sent again, eight at a time. The wait doubles up to 8 s, and resets on progress. An OPEN is sent again too, until the other side answers.
- The receiver keeps messages that come early, up to two windows. It reports the gap at once with the sequence it is missing (at most every 300 ms), and a duplicate gets its acknowledgement again.
- Everything written is acknowledged before a tunnel's close is sent.
- A tunnel ends after 45 s without progress, or a gap open for 30 s.
- A tunnel id that just ended is remembered for 2 minutes, so a late OPEN starts nothing.

Older versions still work with this: their acknowledgements and duplicates behave as before. A test switch drops every Nth data message on purpose, and the phone's interop tests use it in both directions.

**Pairing codes** are 8 characters from `23456789ABCDEFGHJKMNPQRSTUVWXYZ`, valid for 10 minutes. The code's hash names the rendezvous topics and keys, and the usual six-digit confirmation follows over the tunnel. Someone who only watches the broker never has the code.

**Pairing QR codes (0.20.1).** Nearby also shows the code as a QR code (`QrCode.cpp`: byte mode, level M, versions 1 to 10, the standard mask penalty). It holds the link `arnavisland://pair/<code>?k=<fingerprint>`, where the fingerprint is the first 10 bytes of the SHA-256 of this PC's public key, in hex. The link is 50 bytes, so the code is version 4 (33 x 33). The phone checks the fingerprint against the key its handshake received:
- Another key: the phone refuses, and nothing is paired.
- The same key: the phone says yes by itself, and the PC still confirms the six digits.

Someone who sees the QR code and races the PC to answer it can't pass the fingerprint check.

**The direct path (0.21).** Beside the brokers, each side keeps a UDP socket per family, and its hellos carry its candidates:
- global IPv6 addresses (2000::/3)
- LAN IPv4 addresses
- the public addresses STUN binding requests return (stun.l.google.com, stun.cloudflare.com)

Addresses are looked at again every 45 s, and at once after a network change.

On hearing new candidates, both sides send sealed probes (nonce and send time) to every candidate every 200 ms for 8 s. Each side's outgoing probes open its own NAT and firewall for the other's (hole punching). A probe is answered to the address it came from. An address the other side reached us from is probed too (peer-reflexive), which gets through one "hard" NAT.
- The first answered path is kept, and a later one only if it's 30% faster.
- It's kept alive every 15 s and dropped after 35 s without a sealed datagram.
- With no path, it's tried again every minute.
- A probe over a broker measures the relay's round trip, for the quality ring.

Datagrams carry `A1 01`, the recipient's inbox id (20 bytes), then the message sealed exactly as on a broker, so the rest of the relay treats the path as one more broker (`directPath`). A tunnel on it:
- carries at most 1,100 bytes a message
- paces by a congestion window (slow start from 64, a message per round trip after, halved on loss, 16 to 1,024)
- resends after three round trips (150 ms to 2 s), doubling to 4 s
- reports gaps at the path's round trip (40 to 300 ms)

When the path goes quiet, its tunnels move to the broker the other side was heard on most lately, and the other side's follow them there (a tunnel on the direct path accepts that broker). Nothing that was acknowledged is sent again.

This machine's IPv4 NAT is endpoint-dependent and its router offers no UPnP or NAT-PMP, but its IPv6 has no NAT and STUN answers there in 25 to 50 ms. Two hard IPv4 NATs can't punch through; the relay carries on.

**Kept connections (revision 4).** A phone's remote and notices connections serve requests until closed or idle for a minute. The phone keeps one per device and mode, replaces it after 45 s idle or when a faster path appears, and tries a fresh one once if the kept one fails. A command is then one round trip.

Measured between the phone's engine and share_peer, per remote command:

| Path | Round trip |
|---|---|
| direct (loopback) | 1 to 2 ms |
| relay, phone on one broker | 337 ms |
| relay, PC on one broker | 501 ms |

Before 0.21, each command was two round trips plus a handshake.

## 0.22.0-preview.1

**Revision 5.** Announced as `2.5`. The remote gains six commands. Answers start `[ok, 1]`; numbers are little-endian; strings are a u32 length and UTF-8.

| Command | Request | Answer |
|---|---|---|
| 12 stats | — | 10 f64 (CPU, GPU, RAM used, total and %, disk used %, free and total, down, up), u64 uptime, u16 threads, i8 battery, u8 charging, f64 minutes left, n and n samples each of CPU, GPU (255 unknown) and download, then name, model, Windows, processor, graphics card |
| 13 settings | `[0]` all; `[1, key, i32]` change; `[2, action]` a button | sections, then each setting: section, control, key, title, detail, range and step, value, action, unit, options, colours; a change answers the value kept |
| 14 controls | `[0]`; `[1, control, i32]` | Wi-Fi, Bluetooth, dark (1, 0, -1 unknown, -2 none), brightness, volume, flags (muted, microphone there, muted, focus running, finished), focus mode, duration, shown, busy |
| 15 command | `[0, text]`; `[1, text, index, title, yes]` | final, results (kind, asks first, title, detail, answer); or outcome (done, asks first, failed, stale) and a message |
| 16 audio | `[0]`; `[1, id]` | outputs (id, name, current, form); not allowed while *Direct output switching* is off |
| 17 island | `[0, page]`; `[1]` | opens a page, or closes the island |

Sleep, restart and shut down wait 0.9 s, so the answer reaches the phone first.

**Notices** gain `[0x30, 5, on, name, password]`, the phone's hotspot (name at most 32 bytes, password at most 64).

## 0.23.0-preview.1

**Revision 6.** Announced as `2.6`.
- **Command 18, battery:** see `remoteBatteryAnswer` in ShareService.h.
- **Stats** end with each core's load: u8 n, then n bytes of percent (255 unknown).
- **Mode Q:** this PC asks a paired phone, on a connection it keeps open. Each request is `[0x80, command, payload]`, answered `[0x81, status, payload]`.
  - **Command 1 (readings)** answers `[u32 n, lines]` and `[u32 n, cover JPEG]`.
  - **Command 2 (the focus clock)** sends `[mode, running, finished, f64 shown, f64 duration, name]`.
  - **Keeping it open:** a connection idle for 40 s is reopened (the phone closes one after a minute). A reused one gets 6 s to answer, then a fresh one is tried.
