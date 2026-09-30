#pragma once
#include "Common/Win32.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>

namespace nexus::mirror {
using Microsoft::WRL::ComPtr;
// 0.24: one monitor's desktop, as the GPU composes it (DXGI desktop duplication), with the pointer drawn in: each new
// frame into a texture of our own (BGRA, the monitor's size), on a D3D11 device of the GPU that drives that monitor (the
// encoder uses the same device, so frames never leave the GPU). Windows takes the duplication away when the desktop
// changes (the lock screen, an administrator's prompt): the capture then says Lost, and is opened again.
class ScreenCapture {
public:
    enum class Result { Frame, Timeout, Lost };
    // The monitor that holds the point (the primary one for (0,0)).
    bool open(POINT on = {0, 0});
    void close();
    bool opened() const { return duplication_ != nullptr; }
    ID3D11Device* device() const { return device_.Get(); }
    // Waits up to `ms` for the desktop (or the pointer) to change; on a Frame, `frame` holds it, pointer and all.
    Result next(int ms, ComPtr<ID3D11Texture2D>& frame);
    // The monitor's place on the virtual desktop, in pixels.
    RECT area() const { return area_; }
    int width() const { return area_.right - area_.left; }
    int height() const { return area_.bottom - area_.top; }
    // How the monitor is turned (the duplicated image is in its unturned orientation).
    DXGI_MODE_ROTATION rotation() const { return rotation_; }
private:
    ComPtr<ID3D11Device> device_; ComPtr<ID3D11DeviceContext> context_; ComPtr<IDXGIOutputDuplication> duplication_;
    // The desktop as last duplicated (without the pointer), and the frame handed out (with it).
    ComPtr<ID3D11Texture2D> desktop_, frame_; RECT area_{}; DXGI_MODE_ROTATION rotation_ = DXGI_MODE_ROTATION_IDENTITY; bool fresh_ = false;
    // The pointer: its shape as Windows gives it, where it is and whether it shows.
    std::vector<uint8_t> shape_; DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo_{}; POINT pointerAt_{}; bool pointerShown_ = false;
    ComPtr<ID3D11Texture2D> patch_; UINT patchWidth_ = 0, patchHeight_ = 0; std::vector<uint8_t> blend_;
    void readPointer(const DXGI_OUTDUPL_FRAME_INFO& info);
    void drawPointer();
};
}
