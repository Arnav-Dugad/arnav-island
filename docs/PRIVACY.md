# Privacy

Core operation is local. No account, API key, analytics, backend, network client,
automatic upload or paid service is implemented. Windows media metadata stays in
memory. Audio monitoring reads volume/mute, not microphone samples. There is no
loopback audio recording, screen recording, clipboard history, notification
observation, file content collection or sensor driver in normal operation.

Local data lives under `%LOCALAPPDATA%/NexusIsland`:

- `settings.nexus`: versioned preferences; temporary file then atomic replacement.
- `events.log` and `events.previous.log`: bounded structured event diagnostics.
  No media titles, notification bodies, clipboard text, file contents or device
  identifiers are logged. Levels currently emitted: Info, Warning, Error.
  Debug and Trace output are not yet implemented.
- `benchmark.json`: only created by the explicit developer benchmark.
- QA PNGs: only created with `--capture`, never automatically in ordinary use.

Open/clear logs from the tray menu. Settings reset is explicit. QA screenshots
must be visually reviewed before sharing. Initial development captures contained
adjacent desktop content and are excluded from the repository; subsequent
capture mode uses an app-owned neutral matte. GitHub publishing includes source,
documentation and reviewed project artifacts, not the user's local runtime data.

Future clipboard, notifications, calendar and plugins require separate opt-in
designs. No global camera/microphone indicator is shown without trustworthy OS
evidence. No third-party native plugin execution is enabled.
