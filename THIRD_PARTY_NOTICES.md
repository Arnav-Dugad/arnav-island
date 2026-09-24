# Third-party notices

Windows system DLLs are supplied by Windows and are not redistributed here.
The project is built with MinGW-w64 and GCC; their runtime libraries retain their
own licenses, including the GCC Runtime Library Exception where applicable.
The project's MIT license does not replace third-party licenses.

The isolated public WinRT ABI declarations in `src/Media/MediaAbi.h` were checked
against Microsoft's windows-rs Windows.Media.Control metadata projection:
https://github.com/microsoft/windows-rs/tree/master/crates/libs/windows/src/Windows/Media/Control
Microsoft windows-rs is available under MIT or Apache-2.0. Attribution: Copyright
Microsoft Corporation. The project uses the MIT option for adapted declarations.

The Windows.UI.Composition declarations in `src/Composition/WinCompAbi.h` were
written from the method order and interface IDs in Windows' own installed
metadata (Windows.UI.winmd) and contain no third-party code.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions: The above copyright notice and this
permission notice shall be included in all copies or substantial portions of
the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

G-Helper and LibreHardwareMonitor were researched only; neither is a dependency.

The optional audio compatibility ABI slot order was cross-checked against EarTrumpet's public IPolicyConfig declaration:
https://github.com/File-New-Project/EarTrumpet/blob/master/EarTrumpet/Interop/MMDeviceAPI/IPolicyConfig.cs
The minimal slot-only interface here is independently authored; no EarTrumpet implementation or binary is included. EarTrumpet has its own license (including exclusions); it is not described as an unrestricted MIT dependency. UsageNotch was a visual reference only and is not bundled.

Brand marks in `src/Design/Brands.h` are SVG paths from Simple Icons 16.32.0
(https://github.com/simple-icons/simple-icons), released under CC0 1.0. The
marks themselves are trademarks of their respective owners. They are used
only to identify the app, service or device maker that Windows reports, and
their use implies no endorsement. Simple Icons' own disclaimer applies.

Brand marks withdrawn from later Simple Icons releases (Xbox, Microsoft, Windows 11,
OpenAI, Logitech, Nintendo, Minecraft, Prime Video, Amazon, Hulu, LinkedIn, Slack,
Skype, Microsoft Edge, Teams and Outlook) come from Simple Icons 9.21.0, also CC0 1.0.

Philips (shield), PowerA, Marshall, Jabra and JioHotstar marks were traced into
paths from files marked public domain on Wikimedia Commons ("Philips Shield blue.svg",
"PowerA Logo.svg", "Marshall logo.svg", "Jabra logo.svg", "JioHotstar 2025.png").
The AULA mark was traced from the logo on the brand's website (aulastar.com). All
remain trademarks of their owners and are shown only to identify a device, app
or service; no endorsement is implied.

The command bar's file search uses Windows Advanced Query Syntax through File
Explorer's search-ms: protocol; no search engine or index is bundled.

Currency conversion (off by default) uses the euro foreign exchange reference
rates published by the European Central Bank (https://www.ecb.europa.eu/stats/
policy_and_exchange_rates/euro_reference_exchange_rates/). The rates are shown
as published; the ECB reference rates are for information purposes only.

The Windows.Devices.Radios, OLE DB and Windows Search interfaces are the
declarations shipped with MinGW-w64 and Windows; the composition path and
effect interop interfaces in `src/Composition/WinCompAbi.h` were written from
Windows' installed metadata (IIDs read by reflection) and Microsoft's public
documentation, and contain no third-party code.

GPU use is read through Windows' Performance Data Helper (PDH) API, part of
Windows. The v0.14 material, grain and shadow are drawn with Direct2D and
Windows.UI.Composition; no third-party code or assets were added.
