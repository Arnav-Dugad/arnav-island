#pragma once
#include "Common/Win32.h"
#include "Media/Library.h"
#include "Interaction/DashboardModel.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT LibraryMessage=WM_APP+72;
// Phase 5G: the songs in your Music folder, found and read (title, artist, album, length) on a worker
// the first time they are wanted, and kept in memory only. Album art is read on demand for the
// rows on screen and the song playing (at most 64 kept). Posts LibraryMessage when the list or a
// picture is ready (wParam 0 list, 1 art).
class MusicLibrary {
public:
    // roots: the folders searched (the Music folder; tests pass their own).
    MusicLibrary(HWND notify,std::vector<std::wstring> roots);
    ~MusicLibrary();
    MusicLibrary(const MusicLibrary&)=delete;MusicLibrary& operator=(const MusicLibrary&)=delete;
    // Starts a scan if none has run (or again when asked).
    void scan(bool again=false);
    bool scanning()const;bool scanned()const;
    std::shared_ptr<const std::vector<LibraryTrack>> tracks()const;
    // The song's cover if read, else null (and it is read soon).
    std::shared_ptr<const Artwork> artwork(const std::wstring& path);
    // Reads one song's details now (a handed-off file); call off the UI thread or for one file only.
    static LibraryTrack read(const std::wstring& path);
private:
    void run();
    HWND notify_;std::vector<std::wstring> roots_;std::thread worker_;mutable std::mutex mutex_;std::condition_variable wake_;
    bool stopping_=false,scanWanted_=false,scanning_=false,scanned_=false;std::shared_ptr<const std::vector<LibraryTrack>> tracks_;
    std::deque<std::wstring> wanted_;std::map<std::wstring,std::shared_ptr<const Artwork>> art_;std::deque<std::wstring> artOrder_;
};
// The cover of an audio file (its embedded picture, or the folder's), at most 256 pixels, with its colours.
std::shared_ptr<const Artwork> audioArtwork(const std::wstring& path);
}
