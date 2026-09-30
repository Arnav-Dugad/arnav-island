#include "Mirror/ScreenCapture.h"
#include <algorithm>
#include <cstring>

namespace nexus::mirror {
bool ScreenCapture::open(POINT on) {
    close();
    ComPtr<IDXGIFactory1> factory; if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf())))) return false;
    // The adapter and output that show the point.
    const HMONITOR wanted = MonitorFromPoint(on, MONITOR_DEFAULTTOPRIMARY);
    ComPtr<IDXGIAdapter1> adapter; ComPtr<IDXGIOutput> output;
    for (UINT a = 0; !output && factory->EnumAdapters1(a, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++a) {
        ComPtr<IDXGIOutput> o;
        for (UINT k = 0; adapter->EnumOutputs(k, o.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++k) {
            DXGI_OUTPUT_DESC d{}; if (SUCCEEDED(o->GetDesc(&d)) && d.Monitor == wanted) { output = o; break; }
        }
    }
    if (!output) return false;
    DXGI_OUTPUT_DESC desc{}; output->GetDesc(&desc); area_ = desc.DesktopCoordinates; rotation_ = desc.Rotation;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
    if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
            levels, UINT(std::size(levels)), D3D11_SDK_VERSION, device_.GetAddressOf(), nullptr, context_.GetAddressOf()))) return false;
    // The encoder may use the device from its own thread too.
    ComPtr<ID3D10Multithread> threads; if (SUCCEEDED(device_.As(&threads))) threads->SetMultithreadProtected(TRUE);
    ComPtr<IDXGIOutput1> output1; if (FAILED(output.As(&output1)) || FAILED(output1->DuplicateOutput(device_.Get(), duplication_.GetAddressOf()))) { close(); return false; }
    DXGI_OUTDUPL_DESC dd{}; duplication_->GetDesc(&dd);
    D3D11_TEXTURE2D_DESC t{}; t.Width = dd.ModeDesc.Width; t.Height = dd.ModeDesc.Height; t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&t, nullptr, desktop_.GetAddressOf())) || FAILED(device_->CreateTexture2D(&t, nullptr, frame_.GetAddressOf()))) { close(); return false; }
    fresh_ = false; return true;
}
void ScreenCapture::close() { duplication_.Reset(); desktop_.Reset(); frame_.Reset(); patch_.Reset(); patchWidth_ = patchHeight_ = 0; context_.Reset(); device_.Reset(); }

ScreenCapture::Result ScreenCapture::next(int ms, ComPtr<ID3D11Texture2D>& frame) {
    if (!duplication_) return Result::Lost;
    DXGI_OUTDUPL_FRAME_INFO info{}; ComPtr<IDXGIResource> resource;
    const HRESULT hr = duplication_->AcquireNextFrame(UINT(std::max(0, ms)), &info, resource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return Result::Timeout;
    if (FAILED(hr)) { duplication_.Reset(); return Result::Lost; }
    bool changed = false;
    if (info.LastPresentTime.QuadPart != 0 || !fresh_) {
        ComPtr<ID3D11Texture2D> image; if (SUCCEEDED(resource.As(&image))) { context_->CopyResource(desktop_.Get(), image.Get()); fresh_ = true; changed = true; }
    }
    const POINT before = pointerAt_; const bool shownBefore = pointerShown_;
    readPointer(info);
    duplication_->ReleaseFrame();
    const bool moved = pointerShown_ != shownBefore || (pointerShown_ && (before.x != pointerAt_.x || before.y != pointerAt_.y)) || info.PointerShapeBufferSize > 0;
    if (!changed && !moved) return Result::Timeout;
    context_->CopyResource(frame_.Get(), desktop_.Get());
    drawPointer();
    frame = frame_; return Result::Frame;
}

void ScreenCapture::readPointer(const DXGI_OUTDUPL_FRAME_INFO& info) {
    if (info.LastMouseUpdateTime.QuadPart != 0) { pointerShown_ = info.PointerPosition.Visible != 0; pointerAt_ = info.PointerPosition.Position; }
    if (info.PointerShapeBufferSize == 0) return;
    shape_.resize(info.PointerShapeBufferSize); UINT got = 0;
    if (FAILED(duplication_->GetFramePointerShape(UINT(shape_.size()), shape_.data(), &got, &shapeInfo_))) { shape_.clear(); shapeInfo_ = {}; }
}

// The pointer, blended into the frame on the CPU (a pointer is small): its patch of the frame is read, mixed and put back.
void ScreenCapture::drawPointer() {
    if (!pointerShown_ || shape_.empty() || shapeInfo_.Width == 0) return;
    const bool mono = shapeInfo_.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;
    const int w = int(shapeInfo_.Width), h = int(mono ? shapeInfo_.Height / 2 : shapeInfo_.Height);
    D3D11_TEXTURE2D_DESC fd{}; frame_->GetDesc(&fd);
    const int left = std::max(0, int(pointerAt_.x)), top = std::max(0, int(pointerAt_.y));
    const int right = std::min(int(fd.Width), int(pointerAt_.x) + w), bottom = std::min(int(fd.Height), int(pointerAt_.y) + h);
    if (right <= left || bottom <= top) return;
    const UINT pw = UINT(right - left), ph = UINT(bottom - top);
    if (!patch_ || patchWidth_ < pw || patchHeight_ < ph) {
        D3D11_TEXTURE2D_DESC t{}; t.Width = std::max<UINT>(pw, 64); t.Height = std::max<UINT>(ph, 64); t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_STAGING; t.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        patch_.Reset(); if (FAILED(device_->CreateTexture2D(&t, nullptr, patch_.GetAddressOf()))) return; patchWidth_ = t.Width; patchHeight_ = t.Height;
    }
    D3D11_BOX box{UINT(left), UINT(top), 0, UINT(right), UINT(bottom), 1};
    context_->CopySubresourceRegion(patch_.Get(), 0, 0, 0, 0, frame_.Get(), 0, &box);
    D3D11_MAPPED_SUBRESOURCE m{}; if (FAILED(context_->Map(patch_.Get(), 0, D3D11_MAP_READ, 0, &m))) return;
    blend_.resize(size_t(pw) * ph * 4);
    for (UINT y = 0; y < ph; ++y) std::memcpy(&blend_[size_t(y) * pw * 4], static_cast<const uint8_t*>(m.pData) + size_t(y) * m.RowPitch, size_t(pw) * 4);
    context_->Unmap(patch_.Get(), 0);
    // Where the patch starts within the pointer's own image.
    const int ox = left - int(pointerAt_.x), oy = top - int(pointerAt_.y); const UINT pitch = shapeInfo_.Pitch;
    for (UINT y = 0; y < ph; ++y) for (UINT x = 0; x < pw; ++x) {
        uint8_t* d = &blend_[(size_t(y) * pw + x) * 4]; const int sx = ox + int(x), sy = oy + int(y);
        if (mono) {
            // Two masks, one bit a pixel: AND keeps the screen, XOR inverts it.
            const size_t bit = size_t(sx); const uint8_t m8 = uint8_t(0x80 >> (bit % 8));
            const bool andBit = (shape_[size_t(sy) * pitch + bit / 8] & m8) != 0, xorBit = (shape_[size_t(sy + h) * pitch + bit / 8] & m8) != 0;
            if (!andBit) { const uint8_t v = xorBit ? 0xFF : 0x00; d[0] = d[1] = d[2] = v; }
            else if (xorBit) { d[0] = uint8_t(~d[0]); d[1] = uint8_t(~d[1]); d[2] = uint8_t(~d[2]); }
        } else {
            const uint8_t* s = &shape_[size_t(sy) * pitch + size_t(sx) * 4];
            if (shapeInfo_.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) {
                const unsigned a = s[3]; for (int c = 0; c < 3; ++c) d[c] = uint8_t((s[c] * a + d[c] * (255 - a) + 127) / 255);
            } else {
                // Masked colour: where the mask is clear the colour replaces the screen; where it's set, it inverts it.
                if (s[3] == 0) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; } else { d[0] ^= s[0]; d[1] ^= s[1]; d[2] ^= s[2]; }
            }
        }
        d[3] = 0xFF;
    }
    context_->UpdateSubresource(frame_.Get(), 0, &box, blend_.data(), pw * 4, 0);
}
}
