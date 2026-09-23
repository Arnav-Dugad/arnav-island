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
