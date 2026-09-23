#pragma once
#include "Common/Win32.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include "Interaction/DashboardModel.h"
#include "Media/AppIdentity.h"
namespace nexus {
constexpr UINT MediaMessage=WM_APP+11;
struct MediaSnapshot {
    bool available=false,playing=false,canToggle=false,canPrevious=false,canNext=false,canSeek=false;
    std::wstring title=L"Nothing playing",artist=L"Play something to get started.",source,appName;
    MediaKind kind=MediaKind::Unknown;std::shared_ptr<const Artwork> artwork,appIcon,serviceIcon;std::string service;bool browser=false,current=false;
    double position=0,duration=0,sampledAt=0,seekMin=0,seekMax=0;uint64_t revision=0;
    // Unique per Windows session while it exists. Browser tabs share one app ID,
    // so selection and commands use this instead of `source`.
    uint64_t id=0;
};
// Every Windows media session, not only the one Windows marks as current, so
// simultaneous players can be browsed. Commands address a session by its id.
class MediaProvider {
    HWND window_;HANDLE stop_;std::shared_ptr<void> changed_;std::thread worker_;
    std::mutex mutex_;std::vector<MediaSnapshot> sessions_;struct Command{int action;std::wstring source;uint64_t id;};std::vector<Command> commands_;double requestedSeek_=-1;std::wstring seekSource_,seekTitle_;uint64_t seekId_=0;
    void run();void publish(std::vector<MediaSnapshot>);
public:
    explicit MediaProvider(HWND);
    ~MediaProvider();
    std::vector<MediaSnapshot> sessions(){std::lock_guard lock(mutex_);return sessions_;}
    MediaSnapshot snapshot(){std::lock_guard lock(mutex_);for(auto& s:sessions_)if(s.current)return s;return sessions_.empty()?MediaSnapshot{}:sessions_.front();}
    void seek(double seconds,const MediaSnapshot& expected){{std::lock_guard lock(mutex_);requestedSeek_=seconds;seekSource_=expected.source;seekTitle_=expected.title;seekId_=expected.id;}SetEvent(changed_.get());}
    void control(int action,const std::wstring& source={},uint64_t id=0){{std::lock_guard lock(mutex_);commands_.push_back({action,source,id});}SetEvent(changed_.get());}
};
}
