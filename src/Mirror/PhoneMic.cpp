#include "Mirror/PhoneMic.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace nexus::mirror {
namespace {
// PKEY_Device_FriendlyName, declared here so no extra GUID library is needed.
constexpr PROPERTYKEY friendlyNameKey{{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
// KSDATAFORMAT_SUBTYPE_IEEE_FLOAT without depending on ksuser.lib.
constexpr GUID floatSubtype{0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
std::wstring nameOf(IMMDevice* d) {
    ComPtr<IPropertyStore> props; std::wstring n;
    if (SUCCEEDED(d->OpenPropertyStore(STGM_READ, &props))) { PROPVARIANT v; PropVariantInit(&v); if (SUCCEEDED(props->GetValue(friendlyNameKey, &v)) && v.vt == VT_LPWSTR && v.pwszVal) n = v.pwszVal; PropVariantClear(&v); }
    return n;
}
}

bool PhoneMicrophone::begin(int rate, int channels, const std::wstring&, std::wstring& device) {
    end(); rate_ = std::clamp(rate, 8000, 96000); channels_ = std::clamp(channels, 1, 2); stop_ = false; ready_ = 0; cable_ = false; device_.clear();
    { std::lock_guard held(lock_); fifo_.clear(); pos_ = 0; prev_.clear(); }
    worker_ = std::thread([this] { run(); if (ready_ == 0) ready_ = -1; });
    // run() opens the device, then says so (or that it couldn't).
    for (int i = 0; i < 300 && ready_ == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (ready_ != 1) { end(); return false; }
    device = device_; return true;
}

void PhoneMicrophone::end() { stop_ = true; if (worker_.joinable()) worker_.join(); }

// Into the device's channels (mono to both front ones; stereo to them as it is, or folded for a mono device) and its rate.
void PhoneMicrophone::samples(const int16_t* pcm, size_t frames, uint64_t) {
    if (ready_ != 1 || !frames) return;
    const size_t oc = size_t(outChannels_); const double step = double(rate_) / outRate_;
    std::lock_guard held(lock_);
    if (prev_.size() != oc) prev_.assign(oc, 0.f);
    // The frame before this block, then the block, in the device's channels.
    std::vector<float> src((frames + 1) * oc, 0.f); std::copy(prev_.begin(), prev_.end(), src.begin());
    for (size_t i = 0; i < frames; ++i) {
        const int16_t* s = pcm + i * size_t(channels_); float* d = &src[(i + 1) * oc];
        const float l = s[0] / 32768.f, r = channels_ == 2 ? s[1] / 32768.f : l;
        if (oc == 1) d[0] = (l + r) * .5f; else { d[0] = l; d[1] = r; }
    }
    // Straight-line interpolation between neighbouring frames.
    while (pos_ < double(frames)) {
        const size_t k = size_t(pos_); const float t = float(pos_ - double(k));
        for (size_t c = 0; c < oc; ++c) fifo_.push_back(src[k * oc + c] * (1 - t) + src[(k + 1) * oc + c] * t);
        pos_ += step;
    }
    pos_ -= double(frames); std::copy(src.end() - long(oc), src.end(), prev_.begin());
    // More than about 300 ms behind: back to the jitter buffer's 80 ms.
    const size_t most = size_t(outRate_ * .3) * oc, keep = size_t(outRate_ * .08) * oc;
    if (fifo_.size() > most) fifo_.erase(fifo_.begin(), fifo_.begin() + long(fifo_.size() - keep));
}

void PhoneMicrophone::run() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return;
    {
        ComPtr<IMMDeviceEnumerator> enumerator; ComPtr<IMMDevice> device; ComPtr<IAudioClient> client; ComPtr<IAudioRenderClient> render; WAVEFORMATEX* mix = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
            // VB-Audio's virtual cable when it's there ("CABLE Input (VB-Audio Virtual Cable)"), else the default output.
            ComPtr<IMMDeviceCollection> all; UINT count = 0;
            if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &all)) && SUCCEEDED(all->GetCount(&count)))
                for (UINT i = 0; i < count && !device; ++i) { ComPtr<IMMDevice> d; if (SUCCEEDED(all->Item(i, &d)) && d) { const std::wstring n = nameOf(d.Get()); if (n.find(L"CABLE Input") != std::wstring::npos) { device = d; device_ = n; cable_ = true; } } }
            if (!device && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) device_ = nameOf(device.Get());
        }
        bool floating = false; int bits = 0; UINT32 bufferFrames = 0;
        HRESULT hr = device ? device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.GetAddressOf())) : E_FAIL;
        if (SUCCEEDED(hr)) hr = client->GetMixFormat(&mix);
        if (SUCCEEDED(hr)) {
            outRate_ = int(mix->nSamplesPerSec); outChannels_ = std::max<int>(1, mix->nChannels); bits = mix->wBitsPerSample;
            floating = mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->cbSize >= 22 && IsEqualGUID(reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix)->SubFormat, floatSubtype));
            hr = (floating && bits == 32) || (!floating && (bits == 16 || bits == 32)) ? client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 2000000, 0, mix, nullptr) : E_FAIL;
        }
        if (SUCCEEDED(hr)) hr = client->GetBufferSize(&bufferFrames);
        if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&render));
        if (SUCCEEDED(hr)) hr = client->Start();
        if (mix) CoTaskMemFree(mix);
        if (SUCCEEDED(hr) && outRate_ > 0) {
            ready_ = 1;
            // About 40 ms kept in the device; playing starts (and starts again after running dry) once 80 ms have come.
            const UINT32 target = std::min(bufferFrames, UINT32(outRate_ / 25)); const size_t oc = size_t(outChannels_), jitter = size_t(outRate_ * .08) * oc;
            bool waiting = true; std::vector<float> out;
            while (!stop_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                UINT32 padding = 0; if (FAILED(client->GetCurrentPadding(&padding))) break;
                if (padding >= target) continue;
                const UINT32 want = target - padding; BYTE* data = nullptr; if (FAILED(render->GetBuffer(want, &data))) break;
                out.assign(size_t(want) * oc, 0.f);
                { std::lock_guard held(lock_); if (waiting && fifo_.size() >= jitter) waiting = false;
                  if (!waiting) { const size_t take = std::min(out.size(), fifo_.size()); std::copy(fifo_.begin(), fifo_.begin() + long(take), out.begin()); fifo_.erase(fifo_.begin(), fifo_.begin() + long(take)); if (take < out.size()) waiting = true; } }
                if (floating) std::memcpy(data, out.data(), out.size() * sizeof(float));
                else if (bits == 16) { auto* d = reinterpret_cast<int16_t*>(data); for (size_t i = 0; i < out.size(); ++i) d[i] = int16_t(std::lround(std::clamp(out[i], -1.f, 1.f) * 32767)); }
                else { auto* d = reinterpret_cast<int32_t*>(data); for (size_t i = 0; i < out.size(); ++i) d[i] = int32_t(std::llround(double(std::clamp(out[i], -1.f, 1.f)) * 2147483647.0)); }
                render->ReleaseBuffer(want, 0);
            }
            client->Stop();
        }
    }
    CoUninitialize();
}
}
