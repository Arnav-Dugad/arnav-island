#include "Mirror/PhoneCamera.h"
#include "Camera/CameraPipe.h"
#include <mfapi.h>
#include <mfidl.h>
#include <sddl.h>
#include <shellapi.h>
#include <aclapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace nexus::mirror {
// Windows 11's virtual camera (mfvirtualcamera.h, not in MinGW's headers), from mfsensorgroup.dll at run time. Declared
// outside the anonymous namespace on purpose: there GCC sees no class deriving from it and turns each virtual call on it
// into a jump to nowhere (0.27's crash whenever a phone's camera started).
struct IMFCameraSyncObject;
struct IVirtualCamera : IMFAttributes {
    virtual HRESULT STDMETHODCALLTYPE AddDeviceSourceInfo(LPCWSTR) = 0; virtual HRESULT STDMETHODCALLTYPE AddProperty(const void*, ULONG, const BYTE*, ULONG) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddRegistryEntry(LPCWSTR, LPCWSTR, DWORD, const BYTE*, ULONG) = 0; virtual HRESULT STDMETHODCALLTYPE Start(IMFAsyncCallback*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0; virtual HRESULT STDMETHODCALLTYPE Remove() = 0; virtual HRESULT STDMETHODCALLTYPE GetMediaSource(IMFMediaSource**) = 0;
    virtual HRESULT STDMETHODCALLTYPE SendCameraProperty(REFGUID, ULONG, ULONG, void*, ULONG, void*, ULONG, ULONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSyncEvent(REFGUID, ULONG, ULONG, HANDLE, IMFCameraSyncObject**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSyncSemaphore(REFGUID, ULONG, ULONG, HANDLE, LONG, IMFCameraSyncObject**) = 0; virtual HRESULT STDMETHODCALLTYPE Shutdown() = 0;
};
namespace {
using namespace nexus::camera;
// type 0 a software camera source; lifetime 0 this session; access 0 this user.
using CreateCamera = HRESULT(WINAPI*)(int, int, int, LPCWSTR, LPCWSTR, const GUID*, ULONG, IVirtualCamera**);
CreateCamera creator() { static const auto f = [] { HMODULE m = LoadLibraryExW(L"mfsensorgroup.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32); return m ? reinterpret_cast<CreateCamera>(reinterpret_cast<void*>(GetProcAddress(m, "MFCreateVirtualCamera"))) : nullptr; }(); return f; }
DWORD windowsBuild() { using Get = LONG(WINAPI*)(OSVERSIONINFOW*); OSVERSIONINFOW v{sizeof(v)}; auto get = reinterpret_cast<Get>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"))); return get && get(&v) == 0 ? v.dwBuildNumber : 0; }
std::wstring userSid() {
    HANDLE token = nullptr; std::wstring s; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return s;
    DWORD n = 0; GetTokenInformation(token, TokenUser, nullptr, 0, &n); std::vector<BYTE> b(n); LPWSTR text = nullptr;
    if (n && GetTokenInformation(token, TokenUser, b.data(), n, &n) && ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid, &text)) { s = text; LocalFree(text); }
    CloseHandle(token); return s;
}
void tell(WPARAM what, const std::wstring& text) { if (HWND island = FindWindowW(L"ArnavIsland.Surface", nullptr)) { auto* p = new std::wstring(text); if (!PostMessageW(island, PhoneCamera::message, what, LPARAM(p))) delete p; } }
std::wstring hex(HRESULT hr) { wchar_t b[16]; swprintf(b, 16, L"0x%08lX", static_cast<unsigned long>(hr)); return b; }
// The source's class, set up once by an administrator for the PC (HKLM): Windows' camera service reads no other.
const std::wstring classKey = std::wstring(L"Software\\Classes\\CLSID\\") + sourceClsidText;
bool registerSource(const std::wstring& path, HKEY root = HKEY_CURRENT_USER) {
    const std::wstring& key = classKey; HKEY k = nullptr, s = nullptr; bool ok = false;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        const wchar_t* name = L"Arnav Island Phone Camera"; RegSetValueExW(k, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(name), DWORD((wcslen(name) + 1) * 2));
        if (RegCreateKeyExW(k, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &s, nullptr) == ERROR_SUCCESS) {
            ok = RegSetValueExW(s, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_str()), DWORD((path.size() + 1) * 2)) == ERROR_SUCCESS &&
                 RegSetValueExW(s, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Both"), 10) == ERROR_SUCCESS; RegCloseKey(s); }
        RegCloseKey(k); }
    return ok;
}
// The PC-wide source, when it has been set up (and is still there).
std::wstring machineSource() {
    wchar_t path[MAX_PATH]{}; DWORD n = sizeof(path); if (RegGetValueW(HKEY_LOCAL_MACHINE, (classKey + L"\\InprocServer32").c_str(), nullptr, RRF_RT_REG_SZ, nullptr, path, &n) != ERROR_SUCCESS) return {};
    std::error_code e; return std::filesystem::exists(path, e) ? std::wstring(path) : std::wstring();
}
std::wstring beside() { wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH); return (std::filesystem::path(exe).parent_path() / L"ArnavIslandCamera.dll").wstring(); }
// Whether two files hold the same bytes (true when the first isn't there: nothing to bring up to date).
bool sameFile(const std::wstring& a, const std::wstring& b) {
    std::ifstream x(std::filesystem::path(a), std::ios::binary), y(std::filesystem::path(b), std::ios::binary); if (!x || !y) return !x;
    return std::equal(std::istreambuf_iterator<char>(x), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(y), std::istreambuf_iterator<char>());
}
std::atomic<bool> askedSetUp{false};
void askSetUp() {
    if (askedSetUp.exchange(true)) return;
    std::thread([] {
        wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        SHELLEXECUTEINFOW e{}; e.cbSize = sizeof(e); e.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI; e.lpVerb = L"runas"; e.lpFile = exe; e.lpParameters = L"--set-up-camera --quiet"; e.nShow = SW_HIDE;
        if (!ShellExecuteExW(&e) || !e.hProcess) { tell(4, L"Not set up: allow Arnav Island on this PC next time"); return; }
        WaitForSingleObject(e.hProcess, 5 * 60 * 1000); DWORD code = 1; GetExitCodeProcess(e.hProcess, &code); CloseHandle(e.hProcess);
        tell(code == 0 && !machineSource().empty() ? 3 : 4, code == 0 ? L"Start it again from your phone to use it in any app" : L"Couldn\u2019t set it up");
    }).detach();
}
double clockNow() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}
struct PhoneCamera::Camera { IVirtualCamera* camera = nullptr; };
bool PhoneCamera::supported() { return windowsBuild() >= 22000 && creator() != nullptr; }

bool PhoneCamera::setUp(std::wstring& why) {
    namespace fs = std::filesystem; wchar_t exe[MAX_PATH]{}, programs[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH); std::error_code e;
    const fs::path from = fs::path(exe).parent_path() / L"ArnavIslandCamera.dll"; if (!fs::exists(from, e)) { why = L"ArnavIslandCamera.dll isn’t beside the island"; return false; }
    if (!GetEnvironmentVariableW(L"ProgramFiles", programs, MAX_PATH)) { why = L"There’s no Program Files folder"; return false; }
    const fs::path dir = fs::path(programs) / L"Arnav Island", to = dir / L"ArnavIslandCamera.dll"; fs::create_directories(dir, e);
    if (!CopyFileW(from.c_str(), to.c_str(), FALSE)) { why = L"Couldn’t copy the camera into Program Files (run it as administrator)"; return false; }
    if (!registerSource(to.wstring(), HKEY_LOCAL_MACHINE)) { why = L"Couldn’t register the camera for this PC (run it as administrator)"; return false; }
    why = L"Arnav Island Phone Camera is set up for this PC"; return true;
}

bool PhoneCamera::open(const std::wstring& phone, std::wstring& why) {
    close(); phone_ = phone;
    if (!supported()) { why = L"A camera other apps can pick needs Windows 11"; tell(2, why); return false; }
    // Windows' camera service sees only a camera set up for the PC. Not set up yet: it is asked for (once) and the camera
    // shows in its window this time. Set up with an older camera part than the island's: brought up to date (asked once),
    // the one there used meanwhile.
    const std::wstring machine = machineSource();
    if (machine.empty()) { askSetUp(); why = L"Setting up \u201cArnav Island Phone Camera\u201d: allow it on this PC"; tell(2, why); return false; }
    if (!sameFile(beside(), machine)) askSetUp();
    // 0.27 registered it for this user too (pointing into %PUBLIC%): gone, so this user's apps see the PC's one.
    RegDeleteTreeW(HKEY_CURRENT_USER, classKey.c_str());
    CoInitializeEx(nullptr, COINIT_MULTITHREADED); MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    // Only a camera Windows made is touched (a failed call may leave anything in its out pointer).
    IVirtualCamera* camera = nullptr; HRESULT hr = creator()(0, 0, 0, L"Arnav Island Phone Camera", sourceClsidText, nullptr, 0, &camera);
    if (FAILED(hr)) camera = nullptr; else if (!camera) hr = E_POINTER;
    if (camera) { hr = camera->Start(nullptr); if (FAILED(hr)) { camera->Shutdown(); camera->Release(); camera = nullptr; } }
    if (!camera) { why = L"Windows didn\u2019t take the camera (" + hex(hr) + L")"; tell(2, why); return false; }
    camera_ = new Camera{camera}; stop_ = false; server_ = std::thread([this] { serve(); }); tell(1, phone); return true;
}

void PhoneCamera::close() {
    if (!camera_) return; stop_ = true; fresh_.notify_all(); if (server_.joinable()) server_.join();
    camera_->camera->Remove(); camera_->camera->Shutdown(); camera_->camera->Release(); delete camera_; camera_ = nullptr; staging_.Reset(); tell(0, phone_);
}

// The pipe the source reads: one reader at a time (the Frame Server), each new frame written whole; a reader that goes is
// waited for again.
void PhoneCamera::serve() {
    const std::wstring sid = userSid(); const std::wstring sddl = L"D:P(A;;GA;;;SY)" + (sid.empty() ? std::wstring() : L"(A;;GA;;;" + sid + L")") + L"(A;;GR;;;LS)";
    PSECURITY_DESCRIPTOR sd = nullptr; ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr); SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr); std::vector<uint8_t> out;
    while (!stop_) {
        HANDLE pipe = CreateNamedPipeW(pipeName, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 2, frameBytes + 64, 0, 0, &sa);
        if (pipe == INVALID_HANDLE_VALUE) { Sleep(250); continue; }
        OVERLAPPED o{}; o.hEvent = event; ResetEvent(event); bool joined = ConnectNamedPipe(pipe, &o) || GetLastError() == ERROR_PIPE_CONNECTED;
        while (!joined && !stop_ && GetLastError() == ERROR_IO_PENDING) { if (WaitForSingleObject(event, 200) == WAIT_OBJECT_0) { DWORD n = 0; joined = GetOverlappedResult(pipe, &o, &n, FALSE); break; } }
        if (!joined) { CancelIoEx(pipe, &o); CloseHandle(pipe); continue; }
        connected_ = true; uint64_t sent = 0;
        while (!stop_) {
            { std::unique_lock held(lock_); fresh_.wait_for(held, std::chrono::milliseconds(250), [&] { return stop_ || serial_ != sent; }); if (stop_) break; if (serial_ == sent) continue; out = frame_; sent = serial_; }
            FrameHeader h; out.insert(out.begin(), reinterpret_cast<const uint8_t*>(&h), reinterpret_cast<const uint8_t*>(&h) + sizeof(h));
            ResetEvent(event); OVERLAPPED w{}; w.hEvent = event; DWORD n = 0; bool ok = WriteFile(pipe, out.data(), DWORD(out.size()), nullptr, &w) || GetLastError() == ERROR_IO_PENDING;
            while (ok && !stop_) { if (WaitForSingleObject(event, 200) == WAIT_OBJECT_0) { ok = GetOverlappedResult(pipe, &w, &n, FALSE) && n == out.size(); break; } }
            if (stop_) CancelIoEx(pipe, &w); if (!ok) break;
        }
        connected_ = false; DisconnectNamedPipe(pipe); CloseHandle(pipe);
    }
    CloseHandle(event); LocalFree(sd);
}

// A decoded frame, fitted into the camera's size (black bars) while something watches the camera, at most 30 a second.
void PhoneCamera::frame(ID3D11Texture2D* nv12, UINT slice, int width, int height) {
    if (!camera_ || !connected_ || !nv12 || width < 2 || height < 2) return; const double now = clockNow(); if (now - lastAt_ < 1.0 / frameRate - .004) return; lastAt_ = now;
    ComPtr<ID3D11Device> device; nv12->GetDevice(device.GetAddressOf()); ComPtr<ID3D11DeviceContext> context; device->GetImmediateContext(context.GetAddressOf());
    D3D11_TEXTURE2D_DESC d{}; nv12->GetDesc(&d); D3D11_TEXTURE2D_DESC sd{};
    if (staging_) staging_->GetDesc(&sd);
    if (!staging_ || sd.Width != d.Width || sd.Height != d.Height) { staging_.Reset(); D3D11_TEXTURE2D_DESC s = d; s.ArraySize = 1; s.MipLevels = 1; s.BindFlags = 0; s.MiscFlags = 0; s.Usage = D3D11_USAGE_STAGING; s.CPUAccessFlags = D3D11_CPU_ACCESS_READ; if (FAILED(device->CreateTexture2D(&s, nullptr, staging_.GetAddressOf()))) return; }
    context->CopySubresourceRegion(staging_.Get(), 0, 0, 0, 0, nv12, D3D11CalcSubresource(0, slice, d.MipLevels), nullptr);
    D3D11_MAPPED_SUBRESOURCE m{}; if (FAILED(context->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &m))) return;
    const int w = std::min<int>(width, int(d.Width)) & ~1, h = std::min<int>(height, int(d.Height)) & ~1; const uint8_t* srcY = static_cast<const uint8_t*>(m.pData); const uint8_t* srcUV = srcY + size_t(m.RowPitch) * d.Height;
    // Fitted, centred, on even pixels (for the chroma); luma sampled bilinearly, chroma at the nearest pixel.
    const double k = std::min(double(frameWidth) / w, double(frameHeight) / h); const int dw = std::max(2, int(w * k) & ~1), dh = std::max(2, int(h * k) & ~1), ox = (int(frameWidth) - dw) / 2 & ~1, oy = (int(frameHeight) - dh) / 2 & ~1;
    std::vector<uint8_t> f(frameBytes); std::memset(f.data(), 16, size_t(frameWidth) * frameHeight); std::memset(f.data() + size_t(frameWidth) * frameHeight, 128, size_t(frameWidth) * frameHeight / 2);
    std::vector<int> x0(static_cast<size_t>(dw)), fx(static_cast<size_t>(dw)); for (int x = 0; x < dw; ++x) { const double s = std::clamp((x + .5) / k - .5, 0., double(w - 1)); x0[size_t(x)] = std::min(int(s), w - 2); fx[size_t(x)] = int((s - x0[size_t(x)]) * 256); }
    for (int y = 0; y < dh; ++y) { const double s = std::clamp((y + .5) / k - .5, 0., double(h - 1)); const int y0 = std::min(int(s), h - 2), fy = int((s - y0) * 256);
        const uint8_t* a = srcY + size_t(y0) * m.RowPitch; const uint8_t* b = a + m.RowPitch; uint8_t* o = &f[size_t(oy + y) * frameWidth + size_t(ox)];
        for (int x = 0; x < dw; ++x) { const int i = x0[size_t(x)], t = fx[size_t(x)]; const int top = a[i] * (256 - t) + a[i + 1] * t, bottom = b[i] * (256 - t) + b[i + 1] * t; o[x] = uint8_t((top * (256 - fy) + bottom * fy) >> 16); } }
    uint8_t* uv = f.data() + size_t(frameWidth) * frameHeight;
    for (int y = 0; y < dh / 2; ++y) { const int sy = std::min(h / 2 - 1, int((y + .5) * 2 / k) / 2); const uint8_t* r = srcUV + size_t(sy) * m.RowPitch; uint8_t* o = uv + size_t(oy / 2 + y) * frameWidth + size_t(ox);
        for (int x = 0; x < dw / 2; ++x) { const int sx = std::min(w / 2 - 1, int((x + .5) * 2 / k) / 2); o[2 * x] = r[2 * sx]; o[2 * x + 1] = r[2 * sx + 1]; } }
    context->Unmap(staging_.Get(), 0);
    { std::lock_guard held(lock_); frame_.swap(f); ++serial_; } fresh_.notify_all();
}
}
