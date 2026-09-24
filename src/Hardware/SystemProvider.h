#pragma once
#include "Common/Win32.h"
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT SystemMessage=WM_APP+12;
struct SystemSnapshot {
    double cpu=-1,ramPercent=0,ramUsedGiB=0,ramTotalGiB=0,download=0,upload=0;
    double diskUsedPercent=-1,diskFreeGiB=0,diskTotalGiB=0;
    bool networkAvailable=false;uint64_t uptime=0;unsigned logicalProcessors=0;
    std::array<float,40> cpuHistory{},downloadHistory{};unsigned samples=0;
    // GPU busy percentage (-1 when the counters are unavailable) and its history.
    double gpu=-1;std::array<float,40> gpuHistory{};
};
class SystemProvider {
    HWND window_;HANDLE stop_,wake_;std::thread worker_;std::atomic<bool> active_{false};std::mutex mutex_;SystemSnapshot current_;
    void run();
public:
    explicit SystemProvider(HWND);~SystemProvider();
    void setActive(bool active){if(active_.exchange(active)!=active)SetEvent(wake_);}
    SystemSnapshot snapshot(){std::lock_guard lock(mutex_);return current_;}
};
}
