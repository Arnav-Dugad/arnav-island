#pragma once
#include "Common/Win32.h"
#include "Audio/Spectrum.h"
#include <atomic>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT SpectrumMessage=WM_APP+23;
struct SpectrumFrame {std::array<float,Spectrum::bandCount> bands{};float level=0;bool resting=true;};
// Analyzes what Windows is playing through WASAPI shared-mode loopback on a
// worker thread. Audio is never recorded, stored or transmitted: each buffer is
// folded into the spectrum and discarded. Capture runs only while requested.
class LoopbackAnalyzer {
    HWND window_;HANDLE stop_,wake_;std::thread worker_;std::atomic<bool> active_{false};std::mutex mutex_;SpectrumFrame frame_;
    void run();
public:
    std::atomic<bool> available{false},pending{false};
    explicit LoopbackAnalyzer(HWND);~LoopbackAnalyzer();
    void setActive(bool active){if(active_.exchange(active)!=active)SetEvent(wake_);}
    bool active()const{return active_.load();}
    SpectrumFrame frame(){std::lock_guard lock(mutex_);return frame_;}
};
}
