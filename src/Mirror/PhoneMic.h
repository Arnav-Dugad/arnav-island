#pragma once
#include "Mirror/MirrorHost.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nexus::mirror {
// 0.26: a phone's microphone, played on this PC (WASAPI, shared mode) into VB-Audio's virtual cable when it's installed (its
// "CABLE Input"), so any app can take "CABLE Output" as a microphone; otherwise into the default output. The sound is
// turned into the device's own format (its rate by straight-line interpolation, its channels), held about 80 ms against
// the network's jitter, and anything more than about 300 ms behind is dropped. It lives on a thread of its own.
class PhoneMicrophone : public AudioSink {
public:
    ~PhoneMicrophone() override { end(); }
    bool begin(int rate, int channels, const std::wstring& name, std::wstring& device) override;
    void samples(const int16_t* pcm, size_t frames, uint64_t time) override;
    void end() override;
    // Whether it plays into the virtual cable (known once begin has returned true).
    bool cable() const { return cable_; }
private:
    void run();
    int rate_ = 48000, channels_ = 1, outRate_ = 48000, outChannels_ = 2; bool cable_ = false; std::wstring device_;
    std::thread worker_; std::atomic<bool> stop_{false}; std::atomic<int> ready_{0};
    // What waits to be played, interleaved in the device's channels, at its rate; and the resampler's place.
    std::mutex lock_; std::deque<float> fifo_; double pos_ = 0; std::vector<float> prev_;
};
}
