#pragma once
#include "Common/Win32.h"
#include <atomic>
#include <thread>
namespace nexus {
constexpr UINT BrightnessMessage=WM_APP+25;
// Internal-panel brightness through the documented WMI monitor classes. External
// monitors and desktops without the class simply report unavailable.
class BrightnessProvider {
    HWND window_;HANDLE stop_,wake_;std::thread worker_,setter_;void run();void setLoop();
public:
    std::atomic<int> value{-1},wanted{-1};std::atomic<bool> available{false};
    explicit BrightnessProvider(HWND);~BrightnessProvider();
    // Phase 5F: sets the panel's brightness (0-100) through WmiSetBrightness on a worker; while a
    // slider moves only the latest request is applied.
    void set(int percent){wanted=percent<0?0:percent>100?100:percent;SetEvent(wake_);}
};
}
