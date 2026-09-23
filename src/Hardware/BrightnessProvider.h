#pragma once
#include "Common/Win32.h"
#include <atomic>
#include <thread>
namespace nexus {
constexpr UINT BrightnessMessage=WM_APP+25;
// Internal-panel brightness through the documented WMI monitor classes. External
// monitors and desktops without the class simply report unavailable.
class BrightnessProvider {
    HWND window_;HANDLE stop_;std::thread worker_;void run();
public:
    std::atomic<int> value{-1};std::atomic<bool> available{false};
    explicit BrightnessProvider(HWND);~BrightnessProvider();
};
}
