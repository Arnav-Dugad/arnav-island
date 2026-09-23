#pragma once
#include "Common/Win32.h"
#include "Interaction/DashboardModel.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace nexus {
constexpr UINT MixerMessage=WM_APP+24;
struct MixerEntry {DWORD pid=0;std::wstring name;std::shared_ptr<const Artwork> icon;float volume=1,peak=0;bool muted=false,system=false,active=false;};
// Per-application volume through the documented audio session API on the
// default output. Entries group every session of a process; system sounds are
// kept separate. Peak meters are sampled only while the mixer is visible.
class SessionMixer {
    HWND window_;HANDLE stop_,changed_;std::thread worker_;std::mutex mutex_;std::vector<MixerEntry> entries_;
    struct Command {DWORD pid;int kind;float value;};std::vector<Command> commands_;std::atomic<bool> metering_{false};
    void run();
public:
    std::atomic<bool> available{false},pending{false};
    explicit SessionMixer(HWND);~SessionMixer();
    std::vector<MixerEntry> entries(){std::lock_guard lock(mutex_);return entries_;}
    void setVolume(DWORD pid,float value){{std::lock_guard lock(mutex_);commands_.push_back({pid,0,std::clamp(value,0.f,1.f)});for(auto& e:entries_)if(e.pid==pid)e.volume=std::clamp(value,0.f,1.f);}SetEvent(changed_);}
    void setMute(DWORD pid,bool muted){{std::lock_guard lock(mutex_);commands_.push_back({pid,1,muted?1.f:0.f});for(auto& e:entries_)if(e.pid==pid)e.muted=muted;}SetEvent(changed_);}
    void setMetering(bool on){if(metering_.exchange(on)!=on)SetEvent(changed_);}
};
std::wstring processName(DWORD pid,std::wstring* path=nullptr);
std::shared_ptr<const Artwork> shellIcon(const std::wstring& parsingName,int size=40);
}
