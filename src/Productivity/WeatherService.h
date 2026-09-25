#pragma once
#include "Common/Win32.h"
#include "Productivity/Weather.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT WeatherMessage=WM_APP+41;
// Phase 5F: the weather at one chosen place, from Open-Meteo, on a worker. It runs only while
// weather is turned on; it asks every 30 minutes, and sends only the place's coordinates (or,
// once, the town typed to find them). The place is kept in weather.nexus on this PC.
class WeatherService {
    HWND window_;std::filesystem::path file_;HANDLE stop_,wake_;std::thread worker_;std::mutex mutex_;
    std::optional<WeatherPlace> place_;std::optional<WeatherNow> now_;std::wstring pending_,status_;void run();
public:
    WeatherService(HWND window,std::filesystem::path file);~WeatherService();
    // Finds the town, remembers it and asks for its weather.
    void choose(std::wstring town){{std::lock_guard lock(mutex_);pending_=std::move(town);}SetEvent(wake_);}
    void refresh(){SetEvent(wake_);}
    std::optional<WeatherPlace> place(){std::lock_guard lock(mutex_);return place_;}
    std::optional<WeatherNow> now(){std::lock_guard lock(mutex_);return now_;}
    // What the last lookup said went wrong ("" when it worked).
    std::wstring status(){std::lock_guard lock(mutex_);return status_;}
};
}
