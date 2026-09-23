# Arnav Island 0.9 — type it, copy it, see who's listening

A native Windows 11 island with physical spring motion, real system information and no cloud.

[Download the Windows x64 preview](https://github.com/Arnav-Dugad/arnav-island/releases/tag/v0.9.0-preview.1)

![Command bar opening Microsoft Edge](docs/evidence/v0.9/command-edge.png)

- **Command bar**: Alt+Shift+Space, then "volume 30", "focus 25", "open edge", "find budget pdfs from last month" or "bluetooth settings". You see what will happen before you press Enter; nothing typed runs as a shell command.
- **Workspaces**: save the apps you have open under a name, and reopen them later with a second Enter.
- **Clipboard history** (opt-in): your last 24 copies on the Shelf, in memory only. Private copies and password managers are skipped.
- **Privacy dots**: green for camera, orange for microphone, blue for location, with the app's name when you open the island.
- **Fixed**: two browser tabs no longer appear as one media session; touching the screen edge shows the compact island only.
- **Sharper and smoother**: wider pixel-exact shoulder curves, antialiased corners, crisper text, content that eases in, and 180 real brand marks including Xbox, AULA, Philips and PowerA.
- Carried forward: edge reveal, Bluetooth and battery cards, glass material, live Settings window, multi-session media, waveform, per-app mixer, focus timer, file shelf.

![Clipboard history on the Shelf](docs/evidence/v0.9/clipboard.png)

No account, subscription, browser engine, cloud, driver or administrator access is required. Extract the ZIP and run ArnavIsland.exe.

[Release notes](docs/RELEASE_NOTES.md) · [Quick start](docs/QUICK_START.md) · [Report](docs/REPORT.md) · [Feature limits](docs/FEATURE_MATRIX.md) · [Delivery phases](docs/DELIVERY_PHASES.md) · [Design system](docs/DESIGN_SYSTEM.md) · [Performance](docs/PERFORMANCE_RESULTS.md) · [Privacy](docs/PRIVACY.md)

Build with MinGW-w64 GCC 16 and CMake (`scripts/build.ps1 -Test`). Backend: Win32, DirectComposition, Direct2D/DirectWrite, plus Windows.UI.Composition for the glass backdrop. Unavailable values are shown as unavailable, never invented.

![Camera in use card](docs/evidence/v0.9/privacy-card.png)
