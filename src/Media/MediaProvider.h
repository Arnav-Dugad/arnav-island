#pragma once
#include "Common/Win32.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include "Interaction/DashboardModel.h"
namespace nexus {
constexpr UINT MediaMessage=WM_APP+11;
struct MediaSnapshot {
    bool available=false,playing=false,canToggle=false,canPrevious=false,canNext=false;
    std::wstring title=L"Nothing playing",artist=L"Play something to get started.",source;
    MediaKind kind=MediaKind::Unknown;std::shared_ptr<const Artwork> artwork;
    double position=0,duration=0,sampledAt=0;uint64_t revision=0;
};
class MediaProvider {
    HWND window_;HANDLE stop_;std::shared_ptr<void> changed_;std::thread worker_;
    std::mutex mutex_;MediaSnapshot latest_;std::atomic<int> command_{0};
    void run();void publish(MediaSnapshot);
public:
    explicit MediaProvider(HWND);
    ~MediaProvider();
    MediaSnapshot snapshot(){std::lock_guard lock(mutex_);return latest_;}
    void control(int action){command_=action;SetEvent(changed_.get());}
};
}
