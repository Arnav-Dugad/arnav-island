#pragma once
#include "Mirror/MirrorHost.h"
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace nexus::mirror {
// 0.24: a phone's screen in a floating window of its own: rounded, with a shadow, kept to the phone's shape as it's
// resized, dragged by its top, and on top of other windows while pinned. The window lives on a thread of its own, which
// also draws it (the session thread only hands over each decoded frame), so dragging never waits for the network and a
// resize never waits on another thread. A click is a tap on the phone,
// a drag a swipe, the wheel a scroll, the right button Back, the middle one Home, and typing goes to the phone (with the
// phone app's control turned on there).
class MirrorWindow : public FrameSink {
public:
    explicit MirrorWindow(const std::atomic<bool>* islandStop = nullptr) : islandStop_(islandStop) {}
    ~MirrorWindow() override { end(); }
    ID3D11Device* device() override;
    bool begin(int width, int height, const std::wstring& name) override;
    void frame(ID3D11Texture2D* nv12, UINT slice, int width, int height) override;
    void end() override;
    // Set when the window's close button (or Esc held) ends it.
    std::atomic<bool> closed{false};
    // Frames drawn (tests), and the window.
    std::atomic<int> presented{0};
    HWND window() const { return hwnd_; }
private:
    const std::atomic<bool>* islandStop_ = nullptr;
    ComPtr<ID3D11Device> device_; ComPtr<ID3D11DeviceContext> context_; ComPtr<IDXGISwapChain1> swap_;
    ComPtr<ID2D1Factory1> d2d_; ComPtr<ID2D1Device> d2dDevice_; ComPtr<ID2D1DeviceContext> d2dContext_;
    VideoScaler scaler_; HWND hwnd_ = nullptr; std::thread ui_; std::wstring name_;
    std::atomic<int> clientW_{0}, clientH_{0}, made_{0}; std::atomic<bool> hover_{false}, pinned_{true}, pending_{false};
    std::atomic<int> phoneW_{0}, phoneH_{0};
    int swapW_ = 0, swapH_ = 0; ComPtr<ID3D11Texture2D> lastFrame_; UINT lastSlice_ = 0; int lastW_ = 0, lastH_ = 0;
    // The picture's place in the window (for mapping clicks), updated as it's drawn.
    std::mutex placeLock_; RECT picture_{};
    bool dragging_ = false;
    void run(int width, int height);
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(HWND, UINT, WPARAM, LPARAM);
    void touch(int action, int x, int y);
    void present();
};
}
