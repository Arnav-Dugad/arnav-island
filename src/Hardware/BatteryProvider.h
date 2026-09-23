#pragma once
#include "Common/Win32.h"
#include "Hardware/BatteryModel.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT BatteryMessage=WM_APP+26;
// Battery driver readings (IOCTL_BATTERY_QUERY_*) on a worker: capacities,
// rate, voltage, cycles and names. Samples every 5 s while shown, otherwise
// every minute, and appends the optional local history every 5 minutes.
class BatteryProvider {
    HWND window_;HANDLE stop_,wake_;std::thread worker_;std::mutex mutex_;BatteryReading reading_;BatteryEstimate estimate_;BatteryHistory history_;
    std::atomic<bool> fast_{false},keepHistory_{true};std::filesystem::path historyFile_;void run();
public:
    BatteryProvider(HWND,std::filesystem::path historyFile,bool keepHistory);~BatteryProvider();
    void setFast(bool on){if(fast_.exchange(on)!=on&&on)SetEvent(wake_);}
    // Turning history off also erases what was kept.
    void setHistory(bool on){if(keepHistory_.exchange(on)!=on&&!on)SetEvent(wake_);}
    void refresh(){SetEvent(wake_);}
    BatteryReading reading(){std::lock_guard lock(mutex_);return reading_;}
    BatteryEstimate estimate(){std::lock_guard lock(mutex_);return estimate_;}
    BatteryHistory history(){std::lock_guard lock(mutex_);return history_;}
    static BatteryReading query();
};
}
