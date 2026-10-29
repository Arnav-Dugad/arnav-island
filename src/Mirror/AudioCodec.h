#pragma once
#include "Common/Win32.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nexus::mirror {
// ---- Revision 10 (0.27): the phone's microphone, compressed ----
// Kind 4's request gains u8 codec after u8 channels: 0 PCM s16le, 1 AAC-LC (ADTS, one frame a packet), 2 Opus (one 20 ms
// packet a frame). The reply's fps byte says which this PC took: Opus when Media Foundation has an Opus decoder, else AAC
// (Windows' own decoder), else PCM, never more than the phone offered. Packets are decoded here, before the jitter buffer.
constexpr int micPcm = 0, micAac = 1, micOpus = 2;
// WAVE_FORMAT_OPUS as a Media Foundation subtype (not in MinGW's headers).
inline const GUID& opusSubtype() { static const GUID g{0x0000704F, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}}; return g; }
// Media Foundation, started once for the process (the video path does the same).
inline bool micMedia() { static const bool ok = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)); return ok; }
// The first audio decoder Media Foundation has for this subtype (null when none).
inline ComPtr<IMFTransform> micDecoderFor(const GUID& subtype) {
    ComPtr<IMFTransform> t; if (!micMedia()) return t;
    MFT_REGISTER_TYPE_INFO in{MFMediaType_Audio, subtype}; IMFActivate** list = nullptr; UINT32 count = 0;
    if (FAILED(MFTEnumEx(MFT_CATEGORY_AUDIO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER, &in, nullptr, &list, &count))) return t;
    for (UINT32 i = 0; i < count; ++i) { if (!t) list[i]->ActivateObject(IID_PPV_ARGS(&t)); list[i]->Release(); }
    CoTaskMemFree(list); return t;
}
// Whether this PC can decode the codec (PCM always). Asked once per codec.
inline bool micCodecAvailable(int codec) {
    if (codec == micPcm) return true; if (codec != micAac && codec != micOpus) return false;
    static int known[3] = {1, -1, -1}; if (known[codec] < 0) known[codec] = micDecoderFor(codec == micAac ? MFAudioFormat_AAC : opusSubtype()) ? 1 : 0;
    return known[codec] == 1;
}
// The codec this PC takes when the phone offers up to `offered`: the best it has, no better than offered.
inline int acceptMicCodec(int offered) { for (int c = std::clamp(offered, 0, 2); c > 0; --c) if (micCodecAvailable(c)) return c; return micPcm; }
inline const wchar_t* micCodecName(int codec) { return codec == micOpus ? L"Opus" : codec == micAac ? L"AAC" : L"PCM"; }

// Packets in, 16-bit interleaved PCM out (at the rate and channels asked for; the decoder's own are turned into those).
class MicDecoder {
public:
    bool open(int codec, int rate, int channels) {
        mft_.Reset(); codec_ = codec; rate_ = rate; channels_ = channels; if (codec == micPcm) return true;
        mft_ = micDecoderFor(codec == micAac ? MFAudioFormat_AAC : opusSubtype()); if (!mft_) return false;
        ComPtr<IMFMediaType> in; if (FAILED(MFCreateMediaType(&in))) return false;
        in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio); in->SetGUID(MF_MT_SUBTYPE, codec == micAac ? MFAudioFormat_AAC : opusSubtype());
        in->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, UINT32(codec == micOpus ? 48000 : rate)); in->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, UINT32(channels));
        if (codec == micAac) {
            // ADTS (payload type 1), as HEAACWAVEINFO's tail, then the AudioSpecificConfig: AAC-LC, the rate's index, the channels.
            static const int rates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
            int index = 3; for (int i = 0; i < 12; ++i) if (rates[i] == rate) index = i;
            const uint8_t info[14] = {1, 0, 0xFE, 0, 0, 0, 0, 0, 0, 0, 0, 0, uint8_t((2 << 3) | (index >> 1)), uint8_t(((index & 1) << 7) | (channels << 3))};
            in->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1); in->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16); in->SetBlob(MF_MT_USER_DATA, info, sizeof(info));
        }
        if (FAILED(mft_->SetInputType(0, in.Get(), 0))) { mft_.Reset(); return false; }
        // 16-bit PCM when it offers it, else float.
        ComPtr<IMFMediaType> chosen;
        for (DWORD i = 0; ; ++i) { ComPtr<IMFMediaType> o; if (FAILED(mft_->GetOutputAvailableType(0, i, &o))) break; GUID s{}; o->GetGUID(MF_MT_SUBTYPE, &s); UINT32 bits = 0; o->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
            if (s == MFAudioFormat_PCM && bits == 16) { chosen = o; break; } if (!chosen && s == MFAudioFormat_Float) chosen = o; }
        if (!chosen || FAILED(mft_->SetOutputType(0, chosen.Get(), 0))) { mft_.Reset(); return false; }
        GUID s{}; chosen->GetGUID(MF_MT_SUBTYPE, &s); floating_ = s == MFAudioFormat_Float; UINT32 r = 0, c = 0; chosen->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &r); chosen->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &c);
        outRate_ = int(r ? r : 48000); outChannels_ = int(c ? c : 1); pos_ = 0; prev_.assign(size_t(channels_), 0);
        MFT_OUTPUT_STREAM_INFO info{}; provides_ = SUCCEEDED(mft_->GetOutputStreamInfo(0, &info)) && (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)); outSize_ = std::max<DWORD>(info.cbSize, 32768);
        mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0); mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        return true;
    }
    int codec() const { return codec_; }
    // One packet (PCM: the samples themselves); what came out is appended to pcm (interleaved, `channels` wide). False when the
    // decoder refused it (the caller opens it again).
    bool decode(const uint8_t* data, size_t size, uint64_t time, std::vector<int16_t>& pcm) {
        if (codec_ == micPcm) { const size_t n = size / 2; const size_t at = pcm.size(); pcm.resize(at + n); std::memcpy(pcm.data() + at, data, n * 2); return true; }
        if (!mft_ || !size) return false;
        ComPtr<IMFMediaBuffer> buffer; ComPtr<IMFSample> sample; BYTE* p = nullptr;
        if (FAILED(MFCreateMemoryBuffer(DWORD(size), &buffer)) || FAILED(buffer->Lock(&p, nullptr, nullptr))) return false;
        std::memcpy(p, data, size); buffer->Unlock(); buffer->SetCurrentLength(DWORD(size));
        if (FAILED(MFCreateSample(&sample)) || FAILED(sample->AddBuffer(buffer.Get()))) return false; sample->SetSampleTime(LONGLONG(time));
        HRESULT hr = mft_->ProcessInput(0, sample.Get(), 0);
        if (hr == MF_E_NOTACCEPTING) { drain(pcm); hr = mft_->ProcessInput(0, sample.Get(), 0); }
        if (FAILED(hr)) return false;
        drain(pcm); return true;
    }
private:
    void drain(std::vector<int16_t>& pcm) {
        for (int guard = 0; guard < 16; ++guard) {
            MFT_OUTPUT_DATA_BUFFER db{}; ComPtr<IMFSample> out; ComPtr<IMFMediaBuffer> ob;
            if (!provides_) { if (FAILED(MFCreateSample(&out)) || FAILED(MFCreateMemoryBuffer(outSize_, &ob)) || FAILED(out->AddBuffer(ob.Get()))) return; db.pSample = out.Get(); }
            DWORD status = 0; const HRESULT hr = mft_->ProcessOutput(0, 1, &db, &status);
            if (provides_ && db.pSample) out.Attach(db.pSample); if (db.pEvents) db.pEvents->Release();
            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT || FAILED(hr) || !out) return;
            ComPtr<IMFMediaBuffer> contiguous; if (FAILED(out->ConvertToContiguousBuffer(&contiguous))) return;
            BYTE* p = nullptr; DWORD length = 0; if (FAILED(contiguous->Lock(&p, nullptr, &length))) return;
            const size_t width = floating_ ? 4 : 2, frames = length / (width * size_t(outChannels_)); emit(p, frames, pcm); contiguous->Unlock();
        }
    }
    // The decoder's frames in the asked channels and rate (straight-line interpolation, as the sound device's conversion does).
    void emit(const BYTE* p, size_t frames, std::vector<int16_t>& pcm) {
        auto at = [&](size_t i, int c) { const int k = std::min(c, outChannels_ - 1); return floating_ ? reinterpret_cast<const float*>(p)[i * size_t(outChannels_) + size_t(k)] : reinterpret_cast<const int16_t*>(p)[i * size_t(outChannels_) + size_t(k)] / 32768.f; };
        const size_t oc = size_t(channels_); std::vector<float> src((frames + 1) * oc); std::copy(prev_.begin(), prev_.end(), src.begin());
        for (size_t i = 0; i < frames; ++i) for (size_t c = 0; c < oc; ++c) src[(i + 1) * oc + c] = oc == 1 && outChannels_ == 2 ? (at(i, 0) + at(i, 1)) * .5f : at(i, int(c));
        const double step = double(outRate_) / double(rate_);
        while (pos_ < double(frames)) { const size_t k = size_t(pos_); const float t = float(pos_ - double(k));
            for (size_t c = 0; c < oc; ++c) pcm.push_back(int16_t(std::lround(std::clamp(src[k * oc + c] * (1 - t) + src[(k + 1) * oc + c] * t, -1.f, 1.f) * 32767))); pos_ += step; }
        pos_ -= double(frames); if (frames) std::copy(src.end() - long(oc), src.end(), prev_.begin());
    }
    ComPtr<IMFTransform> mft_; int codec_ = micPcm, rate_ = 48000, channels_ = 1, outRate_ = 48000, outChannels_ = 1; bool floating_ = false, provides_ = false; DWORD outSize_ = 32768;
    double pos_ = 0; std::vector<float> prev_;
};
}
