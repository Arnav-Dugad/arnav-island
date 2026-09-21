#pragma once
#include "Common/Win32.h"
#include <atomic>
#include <thread>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
namespace nexus {
constexpr UINT AudioMessage=WM_APP+10;
class AudioProvider {
    HWND window_=nullptr;HANDLE changed_=nullptr,stop_=nullptr;std::thread worker_;
    std::atomic<int> requested_{-1};
    void run();
public:
    std::atomic<int> value{0};std::atomic<bool> muted{false},available{false};
    explicit AudioProvider(HWND);
    ~AudioProvider();
    void setVolume(int percent){requested_=std::clamp(percent,0,100);SetEvent(changed_);}
};
}
