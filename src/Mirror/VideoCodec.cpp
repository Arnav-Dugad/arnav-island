#include "Mirror/VideoCodec.h"
#include <strmif.h>
#include <codecapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

namespace nexus::mirror {
namespace {
// Not in MinGW's import libraries: ICodecAPI's IID and Windows' H.264 decoder's CLSID.
const IID codecApi = {0x901db4c7, 0x31ce, 0x41a2, {0x85, 0xdc, 0x8f, 0xa0, 0xbf, 0x41, 0xb8, 0xda}};
const CLSID h264Decoder = {0x62ce7e72, 0x4c71, 0x4d20, {0xb1, 0x5d, 0x45, 0x28, 0x31, 0xa8, 0x7d, 0x9d}};
}
bool startMedia() {
    static std::once_flag once; static bool ok = false;
    std::call_once(once, [] { ok = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)); });
    return ok;
}

// ---- scaling and colour ----
bool VideoScaler::convert(ID3D11Device* device, ID3D11Texture2D* in, UINT inSlice, ID3D11Texture2D* out, DXGI_MODE_ROTATION rotation, bool yuvIn) {
    if (!device || !in || !out) return false;
    D3D11_TEXTURE2D_DESC id{}, od{}; in->GetDesc(&id); out->GetDesc(&od);
    const bool turned = rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270;
    if (device != device_ || !processor_ || id.Width != inW_ || id.Height != inH_ || od.Width != outW_ || od.Height != outH_) {
        reset(); device_ = device; inW_ = id.Width; inH_ = id.Height; outW_ = od.Width; outH_ = od.Height;
        ComPtr<ID3D11DeviceContext> ctx; device->GetImmediateContext(ctx.GetAddressOf());
        if (FAILED(device->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void**>(video_.ReleaseAndGetAddressOf()))) ||
            FAILED(ctx->QueryInterface(__uuidof(ID3D11VideoContext), reinterpret_cast<void**>(context_.ReleaseAndGetAddressOf())))) return false;
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd{}; cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE; cd.InputFrameRate = {60, 1}; cd.InputWidth = inW_; cd.InputHeight = inH_;
        cd.OutputFrameRate = {60, 1}; cd.OutputWidth = outW_; cd.OutputHeight = outH_; cd.Usage = D3D11_VIDEO_USAGE_OPTIMAL_SPEED;
        if (FAILED(video_->CreateVideoProcessorEnumerator(&cd, enumerator_.GetAddressOf())) || FAILED(video_->CreateVideoProcessor(enumerator_.Get(), 0, processor_.GetAddressOf()))) { reset(); return false; }
        // RGB is full range; the encoder's NV12 (and the decoder's) is BT.709, 16-235.
        D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb{}; rgb.RGB_Range = 0; D3D11_VIDEO_PROCESSOR_COLOR_SPACE yuv{}; yuv.YCbCr_Matrix = 1; yuv.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
        context_->VideoProcessorSetStreamColorSpace(processor_.Get(), 0, yuvIn ? &yuv : &rgb);
        context_->VideoProcessorSetOutputColorSpace(processor_.Get(), yuvIn ? &rgb : &yuv);
        D3D11_VIDEO_COLOR black{}; if (yuvIn) { black.RGBA = {0, 0, 0, 1}; } else { black.YCbCr = {16.f / 255, 128.f / 255, 128.f / 255, 1}; }
        context_->VideoProcessorSetOutputBackgroundColor(processor_.Get(), yuvIn ? FALSE : TRUE, &black);
        context_->VideoProcessorSetStreamFrameFormat(processor_.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        context_->VideoProcessorSetStreamAutoProcessingMode(processor_.Get(), 0, FALSE);
    }
    auto inView = std::find_if(inputs_.begin(), inputs_.end(), [&](auto& v) { return v.texture == in && v.slice == inSlice; });
    if (inView == inputs_.end()) {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC d{}; d.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = 0; d.Texture2D.ArraySlice = inSlice;
        ComPtr<ID3D11VideoProcessorInputView> v; if (FAILED(video_->CreateVideoProcessorInputView(in, enumerator_.Get(), &d, v.GetAddressOf()))) return false;
        if (inputs_.size() > 32) inputs_.erase(inputs_.begin());
        inputs_.push_back({in, inSlice, v}); inView = inputs_.end() - 1;
    }
    auto outView = std::find_if(outputs_.begin(), outputs_.end(), [&](auto& v) { return v.texture == out; });
    if (outView == outputs_.end()) {
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC d{}; d.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11VideoProcessorOutputView> v; if (FAILED(video_->CreateVideoProcessorOutputView(out, enumerator_.Get(), &d, v.GetAddressOf()))) return false;
        if (outputs_.size() > 16) outputs_.erase(outputs_.begin());
        outputs_.push_back({out, v}); outView = outputs_.end() - 1;
    }
    // The picture keeps its shape: fitted into the output, centred.
    const double sw = turned ? inH_ : inW_, sh = turned ? inW_ : inH_, scale = std::min(outW_ / sw, outH_ / sh);
    const LONG dw = LONG(sw * scale) & ~1L, dh = LONG(sh * scale) & ~1L, dx = (LONG(outW_) - dw) / 2 & ~1L, dy = (LONG(outH_) - dh) / 2 & ~1L;
    RECT src{0, 0, LONG(inW_), LONG(inH_)}, dst{dx, dy, dx + dw, dy + dh}, all{0, 0, LONG(outW_), LONG(outH_)};
    context_->VideoProcessorSetStreamSourceRect(processor_.Get(), 0, TRUE, &src);
    context_->VideoProcessorSetStreamDestRect(processor_.Get(), 0, TRUE, &dst);
    context_->VideoProcessorSetOutputTargetRect(processor_.Get(), TRUE, &all);
    D3D11_VIDEO_PROCESSOR_ROTATION r = D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY;
    if (rotation == DXGI_MODE_ROTATION_ROTATE90) r = D3D11_VIDEO_PROCESSOR_ROTATION_90; else if (rotation == DXGI_MODE_ROTATION_ROTATE180) r = D3D11_VIDEO_PROCESSOR_ROTATION_180; else if (rotation == DXGI_MODE_ROTATION_ROTATE270) r = D3D11_VIDEO_PROCESSOR_ROTATION_270;
    context_->VideoProcessorSetStreamRotation(processor_.Get(), 0, r != D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY, r);
    D3D11_VIDEO_PROCESSOR_STREAM stream{}; stream.Enable = TRUE; stream.pInputSurface = inView->view.Get();
    return SUCCEEDED(context_->VideoProcessorBlt(processor_.Get(), outView->view.Get(), 0, 1, &stream));
}

// ---- NAL units ----
std::vector<Nal> nalUnits(const uint8_t* d, size_t n) {
    std::vector<Nal> out; size_t i = 0; auto code = [&](size_t at) -> size_t { if (at + 3 <= n && d[at] == 0 && d[at + 1] == 0 && d[at + 2] == 1) return 3; if (at + 4 <= n && d[at] == 0 && d[at + 1] == 0 && d[at + 2] == 0 && d[at + 3] == 1) return 4; return 0; };
    while (i < n && !code(i)) ++i;
    while (i < n) {
        const size_t start = i + code(i); size_t j = start; while (j < n && !code(j)) ++j;
        if (start < j) out.push_back({d[start] & 0x1F, start, j});
        i = j;
    }
    return out;
}

// ---- encoding ----
namespace {
void setCodec(ICodecAPI* c, const GUID& api, UINT32 v) { if (!c) return; VARIANT x; VariantInit(&x); x.vt = VT_UI4; x.ulVal = v; c->SetValue(&api, &x); }
void setBool(ICodecAPI* c, const GUID& api, bool v) { if (!c) return; VARIANT x; VariantInit(&x); x.vt = VT_BOOL; x.boolVal = v ? VARIANT_TRUE : VARIANT_FALSE; c->SetValue(&api, &x); }
}

bool VideoEncoder::configure(IMFTransform* mft, bool hardware) {
    ComPtr<IMFAttributes> attrs;
    if (SUCCEEDED(mft->GetAttributes(attrs.GetAddressOf())) && attrs) {
        UINT32 async = 0; attrs->GetUINT32(MF_TRANSFORM_ASYNC, &async); async_ = async != 0;
        if (async_ && FAILED(attrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE))) return false;
        attrs->SetUINT32(MF_LOW_LATENCY, TRUE);
    } else async_ = false;
    // A GPU's encoder takes textures from this device (another GPU's refuses it).
    if (hardware && FAILED(mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(manager_.Get())))) return false;
    ComPtr<ICodecAPI> codec; mft->QueryInterface(codecApi, reinterpret_cast<void**>(codec.GetAddressOf()));
    setBool(codec.Get(), CODECAPI_AVLowLatencyMode, true);
    setCodec(codec.Get(), CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_CBR);
    setCodec(codec.Get(), CODECAPI_AVEncCommonMeanBitRate, UINT32(bitrate_));
    setCodec(codec.Get(), CODECAPI_AVEncMPVGOPSize, UINT32(fps_ * 2));
    setCodec(codec.Get(), CODECAPI_AVEncMPVDefaultBPictureCount, 0);
    ComPtr<IMFMediaType> out; if (FAILED(MFCreateMediaType(out.GetAddressOf()))) return false;
    out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264); out->SetUINT32(MF_MT_AVG_BITRATE, UINT32(bitrate_));
    MFSetAttributeSize(out.Get(), MF_MT_FRAME_SIZE, UINT32(width_), UINT32(height_)); MFSetAttributeRatio(out.Get(), MF_MT_FRAME_RATE, UINT32(fps_), 1);
    MFSetAttributeRatio(out.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1); out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    out->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
    if (FAILED(mft->SetOutputType(0, out.Get(), 0))) { out->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main); if (FAILED(mft->SetOutputType(0, out.Get(), 0))) return false; }
    ComPtr<IMFMediaType> in; bool set = false;
    for (DWORD i = 0; !set && SUCCEEDED(mft->GetInputAvailableType(0, i, in.ReleaseAndGetAddressOf())); ++i) {
        GUID sub{}; in->GetGUID(MF_MT_SUBTYPE, &sub); if (sub != MFVideoFormat_NV12) continue;
        MFSetAttributeSize(in.Get(), MF_MT_FRAME_SIZE, UINT32(width_), UINT32(height_)); MFSetAttributeRatio(in.Get(), MF_MT_FRAME_RATE, UINT32(fps_), 1);
        set = SUCCEEDED(mft->SetInputType(0, in.Get(), 0));
    }
    if (!set) return false;
    MFT_OUTPUT_STREAM_INFO info{}; if (SUCCEEDED(mft->GetOutputStreamInfo(0, &info))) { provides_ = (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0; outSize_ = std::max<UINT32>(info.cbSize, UINT32(width_ * height_)); }
    mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0); mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0); mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    if (async_ && FAILED(mft->QueryInterface(__uuidof(IMFMediaEventGenerator), reinterpret_cast<void**>(events_.ReleaseAndGetAddressOf())))) return false;
    codec_ = codec; return true;
}

bool VideoEncoder::open(ID3D11Device* device, int width, int height, int fps, int bitrate) {
    close(); if (!device || !startMedia()) return false;
    device_ = device; device_->GetImmediateContext(context_.GetAddressOf());
    width_ = std::max(16, width & ~1); height_ = std::max(16, height & ~1); fps_ = std::clamp(fps, 5, 120); bitrate_ = std::clamp(bitrate, 100'000, 80'000'000);
    if (FAILED(MFCreateDXGIDeviceManager(&token_, manager_.GetAddressOf())) || FAILED(manager_->ResetDevice(device_.Get(), token_))) return false;
    MFT_REGISTER_TYPE_INFO in{MFMediaType_Video, MFVideoFormat_NV12}, out{MFMediaType_Video, MFVideoFormat_H264};
    for (int pass = 0; pass < 2 && !mft_; ++pass) {
        const bool hw = pass == 0; IMFActivate** list = nullptr; UINT32 count = 0;
        if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, (hw ? MFT_ENUM_FLAG_HARDWARE : MFT_ENUM_FLAG_SYNCMFT) | MFT_ENUM_FLAG_SORTANDFILTER, &in, &out, &list, &count))) continue;
        for (UINT32 i = 0; i < count; ++i) {
            if (!mft_) {
                ComPtr<IMFTransform> t;
                if (SUCCEEDED(list[i]->ActivateObject(__uuidof(IMFTransform), reinterpret_cast<void**>(t.GetAddressOf()))) && configure(t.Get(), hw)) {
                    mft_ = t; hardware_ = hw; wchar_t* n = nullptr; UINT32 len = 0; if (SUCCEEDED(list[i]->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &n, &len))) { name_ = n; CoTaskMemFree(n); }
                } else { list[i]->ShutdownObject(); events_.Reset(); codec_.Reset(); }
            }
            list[i]->Release();
        }
        CoTaskMemFree(list);
    }
    if (!mft_) { close(); return false; }
    // The NV12 frames it encodes (several, as a GPU encoder may still read one while the next is made).
    D3D11_TEXTURE2D_DESC t{}; t.Width = UINT(width_); t.Height = UINT(height_); t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_NV12; t.SampleDesc.Count = 1;
    t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = D3D11_BIND_RENDER_TARGET | (hardware_ ? D3D11_BIND_VIDEO_ENCODER : 0);
    for (int k = 0; k < 6; ++k) {
        ComPtr<ID3D11Texture2D> tex; if (FAILED(device_->CreateTexture2D(&t, nullptr, tex.GetAddressOf()))) { t.BindFlags = D3D11_BIND_RENDER_TARGET; if (FAILED(device_->CreateTexture2D(&t, nullptr, tex.GetAddressOf()))) { close(); return false; } }
        ring_.push_back(tex);
    }
    if (!hardware_) { t.Usage = D3D11_USAGE_STAGING; t.BindFlags = 0; t.CPUAccessFlags = D3D11_CPU_ACCESS_READ; if (FAILED(device_->CreateTexture2D(&t, nullptr, staging_.GetAddressOf()))) { close(); return false; } }
    wantKey_ = true; needInput_ = 0; return true;
}
void VideoEncoder::close() {
    if (mft_) { mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0); mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0); }
    events_.Reset(); codec_.Reset(); mft_.Reset(); ring_.clear(); staging_.Reset(); scaler_.reset(); manager_.Reset(); context_.Reset(); device_.Reset(); header_.clear(); needInput_ = 0;
}
void VideoEncoder::bitrate(int bps) { bitrate_ = std::clamp(bps, 100'000, 80'000'000); setCodec(codec_.Get(), CODECAPI_AVEncCommonMeanBitRate, UINT32(bitrate_)); }

bool VideoEncoder::pull(std::vector<Frame>& out) {
    MFT_OUTPUT_DATA_BUFFER db{}; db.dwStreamID = 0; ComPtr<IMFSample> mine;
    if (!provides_) { ComPtr<IMFMediaBuffer> buffer; if (FAILED(MFCreateSample(mine.GetAddressOf())) || FAILED(MFCreateMemoryBuffer(outSize_, buffer.GetAddressOf()))) return false; mine->AddBuffer(buffer.Get()); db.pSample = mine.Get(); }
    DWORD status = 0; const HRESULT hr = mft_->ProcessOutput(0, 1, &db, &status);
    if (db.pEvents) db.pEvents->Release();
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        ComPtr<IMFMediaType> type; if (SUCCEEDED(mft_->GetOutputAvailableType(0, 0, type.GetAddressOf()))) mft_->SetOutputType(0, type.Get(), 0);
        if (provides_ && db.pSample) db.pSample->Release(); return true;
    }
    if (FAILED(hr)) { if (provides_ && db.pSample) db.pSample->Release(); return false; }
    IMFSample* sample = db.pSample; if (!sample) return false;
    ComPtr<IMFMediaBuffer> buffer; BYTE* p = nullptr; DWORD n = 0;
    if (SUCCEEDED(sample->ConvertToContiguousBuffer(buffer.GetAddressOf())) && SUCCEEDED(buffer->Lock(&p, nullptr, &n))) {
        Frame f; f.data.assign(p, p + n); buffer->Unlock();
        UINT32 clean = 0; sample->GetUINT32(MFSampleExtension_CleanPoint, &clean); LONGLONG t = 0; sample->GetSampleTime(&t); f.pts = t;
        // Keeps the SPS and PPS, and puts them before any key frame that comes without them.
        const auto units = nalUnits(f.data.data(), f.data.size()); bool idr = false, hasHeader = false;
        for (auto& u : units) { if (u.type == 5) idr = true; if (u.type == 7) hasHeader = true; }
        if (hasHeader) { header_.clear(); for (auto& u : units) if (u.type == 7 || u.type == 8) { static const uint8_t sc[] = {0, 0, 0, 1}; header_.insert(header_.end(), sc, sc + 4); header_.insert(header_.end(), f.data.begin() + long(u.start), f.data.begin() + long(u.end)); } }
        f.key = idr || clean != 0;
        if (f.key && !hasHeader && !header_.empty()) f.data.insert(f.data.begin(), header_.begin(), header_.end());
        if (!f.data.empty()) out.push_back(std::move(f));
    }
    if (provides_) sample->Release();
    return true;
}

// The asynchronous (GPU) encoder's events: it asks for input and says when output is ready. Waits up to `waitMs` for
// what's wanted: room for input, or (after input) the frame it makes.
bool VideoEncoder::pump(std::vector<Frame>& out, int waitMs, bool untilInput) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs); const size_t had = out.size();
    for (;;) {
        ComPtr<IMFMediaEvent> e; const HRESULT hr = events_->GetEvent(MF_EVENT_FLAG_NO_WAIT, e.GetAddressOf());
        if (hr == MF_E_NO_EVENTS_AVAILABLE) {
            if ((untilInput && needInput_ > 0) || (!untilInput && out.size() > had) || std::chrono::steady_clock::now() >= end) return untilInput ? needInput_ > 0 : true;
            std::this_thread::sleep_for(std::chrono::microseconds(500)); continue;
        }
        if (FAILED(hr)) return false;
        MediaEventType type = MEUnknown; e->GetType(&type);
        if (type == METransformNeedInput) ++needInput_;
        else if (type == METransformHaveOutput) { if (!pull(out)) return false; }
    }
}

bool VideoEncoder::encode(ID3D11Texture2D* bgra, DXGI_MODE_ROTATION rotation, int64_t pts, std::vector<Frame>& out) {
    if (!mft_) return false;
    auto& nv12 = ring_[next_++ % ring_.size()];
    if (!scaler_.convert(device_.Get(), bgra, 0, nv12.Get(), rotation)) return false;
    ComPtr<IMFSample> sample; ComPtr<IMFMediaBuffer> buffer;
    if (hardware_) { if (FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), nv12.Get(), 0, FALSE, buffer.GetAddressOf()))) return false; }
    else {
        // Windows' software encoder takes memory: the frame comes back from the GPU.
        context_->CopyResource(staging_.Get(), nv12.Get()); D3D11_MAPPED_SUBRESOURCE m{}; if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &m))) return false;
        const DWORD size = DWORD(width_) * DWORD(height_) * 3 / 2; BYTE* p = nullptr;
        if (FAILED(MFCreateMemoryBuffer(size, buffer.GetAddressOf())) || FAILED(buffer->Lock(&p, nullptr, nullptr))) { context_->Unmap(staging_.Get(), 0); return false; }
        const auto* src = static_cast<const uint8_t*>(m.pData);
        for (int y = 0; y < height_; ++y) std::memcpy(p + size_t(y) * width_, src + size_t(y) * m.RowPitch, size_t(width_));
        for (int y = 0; y < height_ / 2; ++y) std::memcpy(p + size_t(width_) * height_ + size_t(y) * width_, src + size_t(m.RowPitch) * height_ + size_t(y) * m.RowPitch, size_t(width_));
        buffer->Unlock(); buffer->SetCurrentLength(size); context_->Unmap(staging_.Get(), 0);
    }
    if (FAILED(MFCreateSample(sample.GetAddressOf())) || FAILED(sample->AddBuffer(buffer.Get()))) return false;
    sample->SetSampleTime(pts); sample->SetSampleDuration(10'000'000 / fps_);
    if (wantKey_) { setCodec(codec_.Get(), CODECAPI_AVEncVideoForceKeyFrame, 1); wantKey_ = false; }
    if (async_) {
        if (!pump(out, 250, true)) return false;
        if (FAILED(mft_->ProcessInput(0, sample.Get(), 0))) return false; --needInput_;
        // The frame usually follows within a few milliseconds: waited for, so it goes at once.
        return pump(out, 20, false);
    }
    HRESULT hr = mft_->ProcessInput(0, sample.Get(), 0);
    if (hr == MF_E_NOTACCEPTING) { while (pull(out)) {} hr = mft_->ProcessInput(0, sample.Get(), 0); }
    if (FAILED(hr)) return false;
    for (int k = 0; k < 8; ++k) { const size_t had = out.size(); MFT_OUTPUT_STREAM_INFO i{}; (void)i; if (!pull(out) || out.size() == had) break; }
    return true;
}

// ---- decoding ----
bool VideoDecoder::open(ID3D11Device* device) {
    close(); if (!startMedia()) return false; device_ = device;
    if (FAILED(CoCreateInstance(h264Decoder, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMFTransform), reinterpret_cast<void**>(mft_.GetAddressOf())))) return false;
    ComPtr<IMFAttributes> attrs; mft_->GetAttributes(attrs.GetAddressOf());
    UINT32 aware = 0; if (device_ && attrs && SUCCEEDED(attrs->GetUINT32(MF_SA_D3D11_AWARE, &aware)) && aware && SUCCEEDED(MFCreateDXGIDeviceManager(&token_, manager_.GetAddressOf())) &&
        SUCCEEDED(manager_->ResetDevice(device_.Get(), token_))) dxva_ = SUCCEEDED(mft_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(manager_.Get())));
    if (attrs) attrs->SetUINT32(MF_LOW_LATENCY, TRUE);
    ComPtr<ICodecAPI> codec; if (SUCCEEDED(mft_->QueryInterface(codecApi, reinterpret_cast<void**>(codec.GetAddressOf())))) setBool(codec.Get(), CODECAPI_AVLowLatencyMode, true);
    ComPtr<IMFMediaType> in; MFCreateMediaType(in.GetAddressOf()); in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(mft_->SetInputType(0, in.Get(), 0)) || !newOutputType()) { close(); return false; }
    mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0); mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    broken_ = false; return true;
}
void VideoDecoder::close() { if (mft_) mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0); mft_.Reset(); manager_.Reset(); upload_.Reset(); device_.Reset(); width_ = height_ = 0; dxva_ = false; }
bool VideoDecoder::newOutputType() {
    ComPtr<IMFMediaType> type;
    for (DWORD i = 0; SUCCEEDED(mft_->GetOutputAvailableType(0, i, type.ReleaseAndGetAddressOf())); ++i) {
        GUID sub{}; type->GetGUID(MF_MT_SUBTYPE, &sub); if (sub != MFVideoFormat_NV12) continue;
        if (FAILED(mft_->SetOutputType(0, type.Get(), 0))) return false;
        UINT32 w = 0, h = 0; if (SUCCEEDED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &w, &h))) { width_ = int(w); height_ = int(h); }
        return true;
    }
    return false;
}
bool VideoDecoder::input(const uint8_t* data, size_t size, int64_t pts) {
    if (!mft_ || !size) return false;
    ComPtr<IMFMediaBuffer> buffer; BYTE* p = nullptr; if (FAILED(MFCreateMemoryBuffer(DWORD(size), buffer.GetAddressOf())) || FAILED(buffer->Lock(&p, nullptr, nullptr))) return false;
    std::memcpy(p, data, size); buffer->Unlock(); buffer->SetCurrentLength(DWORD(size));
    ComPtr<IMFSample> sample; MFCreateSample(sample.GetAddressOf()); sample->AddBuffer(buffer.Get()); sample->SetSampleTime(pts);
    HRESULT hr = mft_->ProcessInput(0, sample.Get(), 0);
    if (hr == MF_E_NOTACCEPTING) { ComPtr<ID3D11Texture2D> t; UINT s; int64_t q; while (output(t, s, q)) {} hr = mft_->ProcessInput(0, sample.Get(), 0); }
    if (FAILED(hr)) { broken_ = true; return false; }
    return true;
}
bool VideoDecoder::output(ComPtr<ID3D11Texture2D>& texture, UINT& slice, int64_t& pts) {
    MFT_OUTPUT_STREAM_INFO info{}; if (FAILED(mft_->GetOutputStreamInfo(0, &info))) return false;
    MFT_OUTPUT_DATA_BUFFER db{}; ComPtr<IMFSample> mine;
    const bool provides = (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;
    if (!provides) { ComPtr<IMFMediaBuffer> b; if (FAILED(MFCreateSample(mine.GetAddressOf())) || FAILED(MFCreateMemoryBuffer(std::max<DWORD>(info.cbSize, DWORD(width_ * height_ * 3 / 2)), b.GetAddressOf()))) return false; mine->AddBuffer(b.Get()); db.pSample = mine.Get(); }
    DWORD status = 0; const HRESULT hr = mft_->ProcessOutput(0, 1, &db, &status);
    if (db.pEvents) db.pEvents->Release();
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) { if (provides && db.pSample) db.pSample->Release(); return newOutputType() && output(texture, slice, pts); }
    if (FAILED(hr) || !db.pSample) { if (provides && db.pSample) db.pSample->Release(); return false; }
    IMFSample* sample = db.pSample; LONGLONG t = 0; sample->GetSampleTime(&t); pts = t;
    ComPtr<IMFMediaBuffer> buffer; bool ok = false;
    if (SUCCEEDED(sample->GetBufferByIndex(0, buffer.GetAddressOf()))) {
        ComPtr<IMFDXGIBuffer> dxgi;
        if (SUCCEEDED(buffer.As(&dxgi)) && SUCCEEDED(dxgi->GetResource(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(texture.ReleaseAndGetAddressOf())))) { dxgi->GetSubresourceIndex(&slice); ok = true; }
        else if (device_) {
            // Software decoding: the frame in memory goes up to a texture of ours.
            BYTE* p = nullptr; DWORD n = 0;
            if (SUCCEEDED(buffer->Lock(&p, nullptr, &n)) && n >= DWORD(width_ * height_ * 3 / 2)) {
                D3D11_TEXTURE2D_DESC d{}; if (upload_) upload_->GetDesc(&d);
                if (!upload_ || int(d.Width) != width_ || int(d.Height) != height_) {
                    D3D11_TEXTURE2D_DESC u{}; u.Width = UINT(width_); u.Height = UINT(height_); u.MipLevels = 1; u.ArraySize = 1; u.Format = DXGI_FORMAT_NV12; u.SampleDesc.Count = 1; u.Usage = D3D11_USAGE_DEFAULT; u.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    upload_.Reset(); device_->CreateTexture2D(&u, nullptr, upload_.GetAddressOf());
                }
                if (upload_) { ComPtr<ID3D11DeviceContext> ctx; device_->GetImmediateContext(ctx.GetAddressOf()); ctx->UpdateSubresource(upload_.Get(), 0, nullptr, p, UINT(width_), 0); texture = upload_; slice = 0; ok = true; }
                buffer->Unlock();
            }
        }
    }
    if (provides) sample->Release();
    return ok;
}
}
