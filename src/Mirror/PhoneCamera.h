#pragma once
#include "Mirror/MirrorHost.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nexus::mirror {
// 0.27 (revision 10): a phone's camera as a real Windows camera, "Arnav Island Phone Camera" (Windows 11, build 22000 on):
// MFCreateVirtualCamera with the island's own media source (ArnavIslandCamera.dll beside the island, registered for this user
// and copied where Windows' Frame Server can read it). Decoded frames are fitted into the camera's 1280 x 720 (NV12, black
// bars), at most 30 a second, and handed to the source over a named pipe only the Frame Server's account and this user can
// open. The camera is there while the phone sends and goes when it stops (it's made for this session only).
class PhoneCamera : public CameraOutlet {
public:
    ~PhoneCamera() override { close(); }
    bool open(const std::wstring& phone, std::wstring& why) override;
    void frame(ID3D11Texture2D* nv12, UINT slice, int width, int height) override;
    void close() override;
    // The island tells itself how a camera went (wParam 1 on, 0 off, 2 couldn't; lParam the phone's name or why, a
    // std::wstring owned from then on).
    static constexpr UINT message = WM_APP + 67;
    // Whether this Windows can have one at all (Windows 11's virtual cameras).
    static bool supported();
    // Windows' camera service (the Frame Server, Local Service in session 0) reads classes machine-wide only, so a camera
    // registered for one user (HKCU) is refused at Start (0x80070003). This sets it up once for the PC (administrators only:
    // ArnavIsland.exe --set-up-camera): the source copied to Program Files and its class registered in HKLM, used from then on.
    static bool setUp(std::wstring& why);
private:
    void serve();
    struct Camera; Camera* camera_ = nullptr;
    std::thread server_; std::atomic<bool> stop_{false}, connected_{false};
    std::mutex lock_; std::condition_variable fresh_; std::vector<uint8_t> frame_; uint64_t serial_ = 0; double lastAt_ = 0;
    ComPtr<ID3D11Texture2D> staging_; std::wstring phone_;
};
}
