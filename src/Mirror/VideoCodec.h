#pragma once
#include "Common/Win32.h"
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <strmif.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::mirror {
using Microsoft::WRL::ComPtr;
// Media Foundation, started once for the process (the first codec to open starts it).
bool startMedia();

// 0.24: frames turned from one size and format into another on the GPU (D3D11's video processor): the desktop's BGRA
// into the encoder's NV12 at its size, or a decoded NV12 frame into a window's BGRA back buffer. The picture keeps its
// shape, black round it where the shapes differ.
class VideoScaler {
public:
    bool convert(ID3D11Device* device, ID3D11Texture2D* in, UINT inSlice, ID3D11Texture2D* out, DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_IDENTITY, bool yuvIn = false);
    void reset() { enumerator_.Reset(); processor_.Reset(); inputs_.clear(); outputs_.clear(); }
private:
    ComPtr<ID3D11VideoDevice> video_; ComPtr<ID3D11VideoContext> context_; ComPtr<ID3D11VideoProcessorEnumerator> enumerator_; ComPtr<ID3D11VideoProcessor> processor_;
    UINT inW_ = 0, inH_ = 0, outW_ = 0, outH_ = 0; ID3D11Device* device_ = nullptr;
    struct InView { ID3D11Texture2D* texture; UINT slice; ComPtr<ID3D11VideoProcessorInputView> view; };
    struct OutView { ID3D11Texture2D* texture; ComPtr<ID3D11VideoProcessorOutputView> view; };
    std::vector<InView> inputs_; std::vector<OutView> outputs_;
};

// 0.24: H.264 from D3D11 textures: the GPU's own encoder where it has one for this device's GPU (else Windows' software
// one), set for the least delay (no B-frames, low-latency mode, constant bit rate, a key frame every two seconds or when
// asked). Output is Annex B (start codes), each key frame with its SPS and PPS in front.
class VideoEncoder {
public:
    struct Frame { std::vector<uint8_t> data; bool key = false; int64_t pts = 0; };
    ~VideoEncoder() { close(); }
    bool open(ID3D11Device* device, int width, int height, int fps, int bitrate);
    void close();
    bool opened() const { return mft_ != nullptr; }
    // Scales and converts the frame, and hands it to the encoder; whatever it has finished comes back in `out`.
    bool encode(ID3D11Texture2D* bgra, DXGI_MODE_ROTATION rotation, int64_t pts100ns, std::vector<Frame>& out);
    void keyframe() { wantKey_ = true; }
    void bitrate(int bps);
    int width() const { return width_; }
    int height() const { return height_; }
    bool hardware() const { return hardware_; }
    std::wstring name() const { return name_; }
private:
    ComPtr<ID3D11Device> device_; ComPtr<ID3D11DeviceContext> context_; ComPtr<IMFDXGIDeviceManager> manager_; UINT token_ = 0;
    ComPtr<IMFTransform> mft_; ComPtr<IMFMediaEventGenerator> events_; ComPtr<ICodecAPI> codec_;
    bool async_ = false, hardware_ = false, provides_ = false, wantKey_ = true; int needInput_ = 0; UINT32 outSize_ = 0;
    int width_ = 0, height_ = 0, fps_ = 30, bitrate_ = 0; std::wstring name_;
    VideoScaler scaler_; std::vector<ComPtr<ID3D11Texture2D>> ring_; size_t next_ = 0; ComPtr<ID3D11Texture2D> staging_;
    std::vector<uint8_t> header_;
    bool configure(IMFTransform* mft, bool hardware);
    bool pull(std::vector<Frame>& out);
    bool pump(std::vector<Frame>& out, int waitMs, bool untilInput);
};

// 0.24: H.264 decoded on the GPU (DXVA through Windows' decoder) into NV12 textures; each finished frame is handed to
// `show` with its texture and array slice.
class VideoDecoder {
public:
    ~VideoDecoder() { close(); }
    bool open(ID3D11Device* device);
    void close();
    bool opened() const { return mft_ != nullptr; }
    // One access unit (Annex B). Returns false when the decoder broke (it's reopened on the next key frame).
    template<class F> bool decode(const uint8_t* data, size_t size, int64_t pts100ns, F show) {
        if (!input(data, size, pts100ns)) return false;
        ComPtr<ID3D11Texture2D> texture; UINT slice = 0; int64_t pts = 0;
        while (output(texture, slice, pts)) show(texture.Get(), slice, width_, height_);
        return !broken_;
    }
    int width() const { return width_; }
    int height() const { return height_; }
private:
    ComPtr<ID3D11Device> device_; ComPtr<IMFDXGIDeviceManager> manager_; UINT token_ = 0; ComPtr<IMFTransform> mft_;
    int width_ = 0, height_ = 0; bool broken_ = false, dxva_ = false; ComPtr<ID3D11Texture2D> upload_;
    bool input(const uint8_t* data, size_t size, int64_t pts);
    bool output(ComPtr<ID3D11Texture2D>& texture, UINT& slice, int64_t& pts);
    bool newOutputType();
};

// NAL units in an Annex B stream: each unit's type and where it starts (after its start code) and ends.
struct Nal { int type; size_t start, end; };
std::vector<Nal> nalUnits(const uint8_t* data, size_t size);
}
