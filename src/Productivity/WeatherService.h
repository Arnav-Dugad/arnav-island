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
    // Phase 5G: a town search (Settings' Town field) and a place chosen from its matches.
    std::wstring search_,searched_;std::vector<WeatherPlace> matches_;bool searchFailed_=false;std::optional<WeatherPlace> chosen_;
public:
    WeatherService(HWND window,std::filesystem::path file);~WeatherService();
    // Finds the town, remembers it and asks for its weather.
    void choose(std::wstring town){{std::lock_guard lock(mutex_);pending_=std::move(town);}SetEvent(wake_);}
    void refresh(){SetEvent(wake_);}
    // Looks for towns matching `query` (posts WeatherMessage with wParam 2 when the matches are in).
    void search(std::wstring query){{std::lock_guard lock(mutex_);search_=std::move(query);}SetEvent(wake_);}
    // The last search's matches, what it looked for, and whether Open-Meteo couldn't be reached.
    std::vector<WeatherPlace> matches(){std::lock_guard lock(mutex_);return matches_;}std::wstring searched(){std::lock_guard lock(mutex_);return searched_;}bool searchFailed(){std::lock_guard lock(mutex_);return searchFailed_;}
    // Makes `place` the weather's place (kept in weather.nexus) and fetches its weather.
    void choose(const WeatherPlace& place){{std::lock_guard lock(mutex_);chosen_=place;}SetEvent(wake_);}
    std::optional<WeatherPlace> place(){std::lock_guard lock(mutex_);return place_;}
    std::optional<WeatherNow> now(){std::lock_guard lock(mutex_);return now_;}
    // What the last lookup said went wrong ("" when it worked).
    std::wstring status(){std::lock_guard lock(mutex_);return status_;}
};
}
