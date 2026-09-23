#pragma once
#include "Common/Win32.h"
#include <string>
namespace nexus {
constexpr UINT PowerModeMessage=WM_APP+28,PlatformMessage=WM_APP+29;
// Read-only hardware identity from the firmware tables Windows exposes in the
// registry, plus whether ASUS Armoury Crate is installed. No vendor drivers,
// ACPI calls or firmware commands are used.
struct PlatformInfo {std::wstring manufacturer,product,armoury;bool asus=false,rog=false;};
PlatformInfo platformInfo();
const wchar_t* powerModeName(int mode);
// Windows' effective power mode (documented notification API, Windows 10 1809+).
class PowerModeWatcher {
    void* registration_=nullptr;
public:
    explicit PowerModeWatcher(HWND);~PowerModeWatcher();
    PowerModeWatcher(const PowerModeWatcher&)=delete;PowerModeWatcher& operator=(const PowerModeWatcher&)=delete;
};
}
