#pragma once
#include "Common/Win32.h"
#include "Media/Lyrics.h"
#include <atomic>
#include <filesystem>
#include <list>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT LyricsMessage=WM_APP+33;
// Synced lyrics for the playing track, from LRCLIB (lrclib.net: free, no account or key).
// Only the song title and artist are sent. Answers, including "none", are kept in
// %LOCALAPPDATA%\ArnavIsland\lyrics so a song is looked up once. Runs only when the
// island asks, on its own thread; the newest request replaces an unstarted one.
class LyricsService {
public:
    enum class State {Unknown,Loading,Found,Missing,Instrumental,Offline};
    struct Result {State state=State::Unknown;std::shared_ptr<const std::vector<LyricLine>> lines;};
    struct Track {std::wstring title,artist;double duration=0;bool browser=false;};
    static std::wstring key(const Track& t){return t.title+L"\n"+t.artist+L"\n"+std::to_wstring(int(std::lround(t.duration)));}
    LyricsService(HWND window,std::filesystem::path directory);
    ~LyricsService();
    void request(const Track& track);
    Result get(const std::wstring& key);
    // Forgets everything: memory and the files on disk.
    void clear();
private:
    HWND window_;std::filesystem::path directory_;HANDLE wake_,stop_;std::thread worker_;std::mutex mutex_;
    std::optional<Track> pending_;std::list<std::pair<std::wstring,Result>> results_;std::atomic<double> offlineUntil_{0};std::atomic<void*> active_{nullptr};
    void run();void store(const std::wstring& key,Result r);Result lookup(const Track& t);
    std::optional<std::string> fetch(const std::string& path,bool& reachable);
};
}
