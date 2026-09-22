#pragma once
#include "Common/Win32.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
namespace nexus {
constexpr UINT AudioMessage=WM_APP+10;
struct AudioDevice {std::wstring id,name;bool current=false;};
class AudioProvider {
    HWND window_=nullptr;HANDLE changed_=nullptr,stop_=nullptr;std::thread worker_;
    std::atomic<int> requested_{-1};std::atomic<bool> muteRequest_{false};
    std::mutex deviceMutex_;std::vector<AudioDevice> devices_;std::wstring requestedDevice_;bool switchEnabled_=false;
    void run();
public:
    std::atomic<int> value{0};std::atomic<bool> muted{false},available{false},notificationPending{false};
    std::atomic<HRESULT> switchResult{S_OK};
    std::vector<AudioDevice> devices(){std::lock_guard lock(deviceMutex_);return devices_;}
    void selectDevice(std::wstring id,bool enabled){{std::lock_guard lock(deviceMutex_);requestedDevice_=std::move(id);switchEnabled_=enabled;}SetEvent(changed_);}
    explicit AudioProvider(HWND);
    ~AudioProvider();
    void toggleMute(){muteRequest_=true;SetEvent(changed_);}
    void setVolume(int percent){requested_=std::clamp(percent,0,100);SetEvent(changed_);}
};
}
