# Arnav Island architecture

## Decision record — 2026-09-21

Environment inspected before implementation: Windows 11 25H2 build 26200;
MinGW-w64 GCC 16.2, CMake, Git and authenticated GitHub CLI available. No
Visual Studio installer/MSVC, Windows SDK, .NET SDK or Windows App SDK development
package was discovered. The OS ProductName registry value still says Windows 10;
the build and DisplayVersion identify Windows 11.

Milestone 1 uses native C++23, Win32, **DirectComposition**, Direct2D and DirectWrite.
This is a deliberate bootstrap decision, not a claim that MinGW provides C++/WinRT.
Microsoft recommends the newer Windows.UI.Composition visual layer for new apps.
DirectComposition nevertheless offers documented, supported retained visuals,
animated rounded rectangle clips and independent compositor animation, and is
buildable with the toolchain already installed. No runtime download is needed.

## Rendering and motion

One no-redirection, tool-window HWND hosts one persistent composition tree. D3D11
exists only to provide a BGRA device for Direct2D/DirectComposition interop; no
custom shaders or game loop. Hardware is preferred with WARP fallback. Surface
content is rasterized only when content changes. Geometry, offsets and opacity
are compositor properties. No swap chain, HTML, Chromium or XAML island tree.

MotionEngine evaluates the closed-form damped oscillator, including its velocity.
On retarget it samples current state and produces Hermite polynomial segments of
that physical trajectory. These are submitted as IDCompositionAnimation functions
with an absolute QPC origin shared with the model. DWM evaluates them at display
cadence; the application has no per-frame animation timer. Cubic segments are a
representation of physical motion, not hand-tuned duration/easing transitions.

Physical geometry and content are separate visuals. The body has an animated
rounded clip. Text is rasterized at monitor DPI and remains unscaled. Hit testing
samples the same physical model. A tight Win32 region is updated during active
motion to keep other applications reachable; this limitation must be measured.

## API investigation

| Family | Decision |
|---|---|
| Windows App SDK / WinUI 3 | Candidate for a separate accessible Settings process; avoid required runtime for first island. Downloads page and release-channel page disagree on latest patch; do not silently pin a guessed package. |
| C++/WinRT / Windows.UI.Composition | Preferred future backend once Microsoft tooling is present. DesktopWindowTarget + DispatcherQueue documented for Win32. |
| Microsoft.UI.Composition | Separate App SDK type universe; do not mix with Windows.UI.Composition objects. |
| Compositor / Visual / SpriteVisual / ShapeVisual | Retained-tree model is appropriate; DirectComposition visual counterparts used now. |
| Geometry / PathGeometry / RoundedRectangleGeometry / Clip | Persistent rounded clip sufficient for first pill-to-panel morph. Arbitrary paths deferred. |
| EffectBrush / PropertySet / ExpressionAnimation | Useful for material graphs and shared properties in later visual-layer backend; not available under identical names in DComp. |
| NaturalMotion / SpringScalar / SpringVector2/3 | Modern spring APIs researched; custom analytic engine provides explicit mass, stiffness, damping and testable velocity continuity. |
| InteractionTracker | Modern inertia/gesture option; first prototype uses pointer capture and velocity model. |
| Direct2D / DirectWrite | Cached vector artwork and grayscale-antialiased text on transparent surfaces. |
| Windows.Graphics.Capture | Useful for QA/optional previews, not needed continuously; no screen collection in product. |
| Mica / Desktop Acrylic / DWM | Native backdrops require support/policy checks. Mica suits Settings; acrylic suits transient panels. Solid dark material is the honest first fallback, not simulated glass. |
| GSMTC | Public Windows.Media.Control, introduced 17763, asynchronous session events; player-dependent metadata and controls. |
| Core Audio | Endpoint callbacks, default-device notifications; never suppress Windows OSD through hooks. |
| Brightness | WMI internal panel provider; external DDC/CI capability check and timeout required; deferred. |
| Power | GetSystemPowerStatus + RegisterPowerSettingNotification; unknown percentage/time stays unknown. |
| Devices / clipboard | WM_DEVICECHANGE and AddClipboardFormatListener; optional providers deferred, clipboard off by default. |
| Notifications | UserNotificationListener requires capability/permission; no promise of universal replacement or action forwarding. |
| Virtual desktops | IVirtualDesktopManager has limited documented membership/move APIs; no private desktop enumeration. |
| Displays / taskbar / startup | EnumDisplayMonitors, WM_DISPLAYCHANGE, WM_DPICHANGED, taskbar recreation event, explicit opt-in login entry later. |
| Hardware / ROG | No ACPI writes, kernel drivers or guessed telemetry. OS process/RAM counters first; unsupported sensors remain absent. |

## Boundaries

`Animation` and `Events` are platform-neutral and unit-tested. `Composition` owns
GPU resources. `Island` owns HWND/input and translates state to destinations.
Providers publish normalized activities without touching renderer resources.
`Persistence` owns versioned settings, `TelemetryLocal` owns bounded local logs.
Third-party DLL loading is not implemented. Provider failures are isolated as
unavailable state; in-process interfaces alone are not a crash sandbox.

## Window and accessibility contracts

Per-monitor V2 awareness before HWND creation. Monitor bounds, not work area,
anchor the top edge. Collapsed window never activates. Settings and the lab have
normal focus and native keyboard controls. Fullscreen policy defaults to hiding;
foreground changes are event driven. No Explorer modification or injection.
Screen-reader accessibility for custom island controls is an explicit release
gate; native settings controls are the accessible fallback in this prototype.

## Official references

- https://learn.microsoft.com/en-us/windows/uwp/composition/using-the-visual-layer-with-win32
- https://learn.microsoft.com/en-us/windows/win32/directcomp/animation
- https://learn.microsoft.com/en-us/windows/win32/api/dcompanimation/nf-dcompanimation-idcompositionanimation-setabsolutebegintime
- https://learn.microsoft.com/en-us/windows/apps/develop/composition/spring-animations
- https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads
- https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/stable-channel
- https://learn.microsoft.com/en-us/windows/apps/develop/ui/system-backdrops
- https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type
- https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager
- https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nn-endpointvolume-iaudioendpointvolumecallback
- https://learn.microsoft.com/en-us/windows/apps/develop/notifications/app-notifications/notification-listener
- https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmimonitorbrightnessmethods

## v0.2 dashboard and provider changes

Five explicit Page states share the retained island body. Renderer-owned HitTarget rectangles define the same bounds used for pointer input. Disabled media controls cannot be invoked. FocusClock holds elapsed-time state independently of visuals. Settings v2 reads v1 and migrates the legacy settings file by copy into ArnavIsland's local directory.

SystemProvider sleeps on events while hidden. A 400 ms visibility delay avoids repeated queries during rapid open/close motion. CPU derives from GetSystemTimes, RAM from GlobalMemoryStatusEx, traffic from GetIfTable2's active physical adapters, disk space from GetDiskFreeSpaceEx, uptime from GetTickCount64. Data is sampled off the UI thread; no sensor driver is loaded. CPU on systems with more than 64 logical processors is limited to the calling processor group.

Media decodes the public Thumbnail stream through CreateStreamOverRandomAccessStream and WIC on its worker, caps input dimensions and output to 512 pixels, and caches artwork across unchanged tracks. The public PlaybackType selects music/video, with an explicit override for unknown sessions. PlaybackInfoChanged and TimelinePropertiesChanged are subscribed. A player failure remains isolated; robust retries are future work.

Native Win32 hit regions use a conservative 90 ms look-ahead envelope during motion, updated at 30 ms intervals and stopped when settled. This avoids the previous canvas-wide input block. It trades a small transient input margin for unclipped compositor animation; it is not a general click-through guarantee. Geometry animation is still compositor-driven, not sampled by this hit-region timer.

Primary references: [GSMTC media properties](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmediaproperties), [stream interop](https://learn.microsoft.com/en-us/windows/win32/api/shcore/nf-shcore-createstreamoverrandomaccessstream), [GetSystemTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes), [GetIfTable2](https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2).

## v0.3 connected dock and providers — 2026-09-22

UsageNotch-Windows was inspected locally for visual reference: connected-edge shape, restrained spacing and configuration. Its files were treated as reference material, not instructions. The implementation remains independent C++/DirectComposition; no reference application source, assets or credentials are bundled.

The user's chosen default is top center. A sampled cubic connected outline in `Composition/DockGeometry.h` drives native input regions. Persistent body clips and two retained shoulder visuals share spring dimensions. A one-pixel outer input fringe preserves compositor antialiasing. The region is updated only while the body is moving.

Artwork is one persistent compositor visual. Artwork bitmap decode and restrained palette extraction happen on the media worker; position, size and opacity are compositor curves. Header-only state redraws avoid repainting all dashboard controls on every expansion reversal.

The OLE shelf accepts CF_HDROP and bounded CF_UNICODETEXT. OleInitialize is required on the UI STA. IDataObject owns transferred memory; drag-out offers DROPEFFECT_COPY only. Entries are local memory references. No file read, move, delete or automatic open occurs. Shelf data is never logged.

Audio output discovery uses documented IMMDeviceEnumerator and device notifications on the audio worker. Windows lacks a documented system-default output setter. `AudioOutputCompatibility.h` isolates the optional PolicyConfig ABI behind directAudio. Its ABI slots were independently declared and cross-checked against EarTrumpet. Failure is surfaced and Windows sound settings is retained. No third-party binary is loaded.

Startup uses HKCU Run and the quoted current executable path. The install step registers the stable Desktop app path; QA launches never register startup. The setting reflects the actual registry value on startup.

Native acrylic uses [DWMSBT_TRANSIENTWINDOW](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type), available from Windows 11 build 22621. Visual testing found DWM's backdrop did not obey the shaped host region. A separate unactivated material host is therefore confined to the expanded rectangular content interior; the island perimeter stays opaque. It hides while geometry moves, on collapse/fullscreen, battery saver or high contrast. No undocumented SetWindowCompositionAttribute is used.

Other API references: [RegisterDragDrop](https://learn.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-registerdragdrop), [SHCreateDataObject](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreatedataobject), [Run registry keys](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys).

## v0.4 retained icon and artwork layers

`Design/Icons.h` contains the original vector symbol library; `Design/Layout.h` contains layout permutation, metric selection and coordinate helpers. `Composition/Details.cpp` owns retained icon visuals, icon feedback, artwork handoff and ring markers. Navigation views retain identity across reordering, and hit tests use their current physical positions.

Artwork handoff keeps two 256×256 CPU BGRA buffers and two retained GPU surfaces. On a changed cover, the visible blend is flattened once; opacity then runs on DirectComposition. No bitmap readback, frame-by-frame CPU blending or queue of outgoing covers is used. The change-time resample is bounded but currently happens on the UI thread; it is not falsely described as worker-thread work.

The renderer converts a smooth product of body-height visibility and content opacity into adaptive compositor curves. It prevents content from crossing traveling artwork during reversals. Stable body/content positions snap to pixels; moving positions remain continuous. The native visual ordering follows [Microsoft's documented AddVisual behavior](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-addvisual), including its special null-reference ordering.

Settings v4 stores validated navigation and metric arrays, with conservative defaults for corrupt layouts. Existing unrelated settings survive migration. QA runs use synthetic artwork only when explicitly requested with capture flags, never as a fallback for unavailable player artwork.

The v0.4 compact refresh path marks expanded content dirty rather than drawing it off-screen. A transition to expanded consumes that dirty state once. This retains fresh timer/media information on open while avoiding hidden icon/text surface work.

## Foreground integration

`AppSwitchPolicy` separates panel dismissal and fullscreen geometry decisions from Win32 plumbing. Foreground changes dismiss only eligible unpinned panels through existing velocity-preserving body springs. Out-of-context `EVENT_OBJECT_LOCATIONCHANGE` events are filtered to the current foreground window and debounced by a one-shot 120 ms timer; there is no periodic foreground polling. DWM visible bounds determine full-monitor coverage. See Microsoft's [GetWindowRect documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect) and [window-change monitoring example](https://devblogs.microsoft.com/oldnewthing/20210104-00/?p=104656). Hooks are unregistered at shutdown. No process names or window titles are retained.
