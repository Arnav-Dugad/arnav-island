# Nexus Island architecture

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
