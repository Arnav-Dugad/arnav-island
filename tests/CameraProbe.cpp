// 0.27.1: "Arnav Island Phone Camera" end to end, as an app sees it. Run on a Windows 11 PC where it has been set up once
// (ArnavIsland.exe --set-up-camera, as administrator), with the island closed. A test picture (a grey ramp, left to right) is
// fed to the camera as a phone's decoded frames would be; the camera is then found among the PC's cameras, opened with
// Media Foundation's source reader as any app opens one, and a frame read back from it. Exit 0 when the frame arrives with
// the ramp in it, 2 when the camera isn't set up for this PC.
#include "Mirror/PhoneCamera.h"
#include "Camera/CameraPipe.h"
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

int wmain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED); MFStartup(MF_VERSION);
    if (!nexus::mirror::PhoneCamera::supported()) { std::puts("UNSUPPORTED (needs Windows 11)"); return 3; }
    wchar_t path[MAX_PATH]{}; DWORD n = sizeof(path);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\CLSID\\{7D3C1A52-4B8E-4F61-9A27-5E0B6C21D94F}\\InprocServer32", nullptr, RRF_RT_REG_SZ, nullptr, path, &n) != ERROR_SUCCESS) { std::puts("NOT SET UP"); return 2; }

    // The test picture, 640 x 360 NV12: luma rising left to right, chroma neutral.
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context))) { std::puts("NO D3D11"); return 1; }
    const int w = 640, h = 360; std::vector<uint8_t> nv12(size_t(w) * h * 3 / 2, 128);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) nv12[size_t(y) * w + x] = uint8_t(16 + x * 219 / (w - 1));
    D3D11_TEXTURE2D_DESC d{}; d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_NV12; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{nv12.data(), UINT(w), 0}; ComPtr<ID3D11Texture2D> picture;
    if (FAILED(device->CreateTexture2D(&d, &init, &picture))) { std::puts("NO NV12 TEXTURE"); return 1; }

    nexus::mirror::PhoneCamera camera; std::wstring why;
    if (!camera.open(L"Probe", why)) { std::printf("OPEN FAILED: %ls\n", why.c_str()); return 1; }
    std::puts("camera open");
    std::atomic<bool> stop{false};
    std::thread feed([&] { while (!stop) { camera.frame(picture.Get(), 0, w, h); std::this_thread::sleep_for(std::chrono::milliseconds(33)); } });
    int result = 1;
    // First the pipe itself, as the camera's source reads it: a frame with the ramp in it.
    if (GetEnvironmentVariableW(L"PROBE_PIPE", nullptr, 0)) {
        HANDLE p = INVALID_HANDLE_VALUE; for (int i = 0; i < 40 && p == INVALID_HANDLE_VALUE; ++i) { p = CreateFileW(nexus::camera::pipeName, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr); if (p == INVALID_HANDLE_VALUE) { std::printf("pipe open error %lu\n", GetLastError()); Sleep(100); } }
        if (p != INVALID_HANDLE_VALUE) {
            nexus::camera::FrameHeader fh{}; DWORD got = 0; std::vector<uint8_t> f(nexus::camera::frameBytes); BOOL ok = ReadFile(p, &fh, sizeof(fh), &got, nullptr);
            std::printf("pipe header ok=%d got=%lu magic=%08x bytes=%u\n", ok, got, fh.magic, fh.bytes);
            DWORD total = 0; while (ok && total < f.size()) { ok = ReadFile(p, f.data() + total, DWORD(f.size() - total), &got, nullptr); total += got; if (!got) break; }
            const uint8_t* row = f.data() + size_t(nexus::camera::frameHeight / 2) * nexus::camera::frameWidth;
            std::printf("pipe frame %lu bytes, luma left %d right %d\n", total, row[200], row[1080]); CloseHandle(p);
        }
    }
    {
        ComPtr<IMFAttributes> want; MFCreateAttributes(&want, 1); want->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        IMFActivate** devices = nullptr; UINT32 count = 0; ComPtr<IMFMediaSource> source;
        // PROBE_INPROC: the camera's own source loaded here, beside the island's copy, not through Windows' camera service.
        if (GetEnvironmentVariableW(L"PROBE_INPROC", nullptr, 0)) {
            wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH); std::wstring dll(exe); dll = dll.substr(0, dll.find_last_of(L'\\') + 1) + L"ArnavIslandCamera.dll";
            using GetClass = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**); HMODULE m = LoadLibraryW(dll.c_str());
            auto get = m ? reinterpret_cast<GetClass>(reinterpret_cast<void*>(GetProcAddress(m, "DllGetClassObject"))) : nullptr; ComPtr<IClassFactory> factory;
            ComPtr<IMFActivate> activate;
            if (get && SUCCEEDED(get(nexus::camera::sourceClsid, IID_PPV_ARGS(&factory))) && SUCCEEDED(factory->CreateInstance(nullptr, IID_PPV_ARGS(&activate)))) activate->ActivateObject(IID_PPV_ARGS(&source));
            std::printf("in-process source: %s\n", source ? "made" : "not made");
        }
        for (int tries = 0; tries < 20 && !source; ++tries) {
            if (SUCCEEDED(MFEnumDeviceSources(want.Get(), &devices, &count))) {
                for (UINT32 i = 0; i < count; ++i) {
                    wchar_t* name = nullptr; UINT32 len = 0;
                    if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len))) {
                        if (tries == 0) std::printf("  camera: %ls\n", name);
                        if (!source && wcsncmp(name, L"Arnav Island Phone Camera", 25) == 0) { HRESULT hr = devices[i]->ActivateObject(IID_PPV_ARGS(&source)); if (FAILED(hr)) std::printf("ACTIVATE FAILED 0x%08lx\n", static_cast<unsigned long>(hr)); }
                        CoTaskMemFree(name);
                    }
                    devices[i]->Release();
                }
                CoTaskMemFree(devices); devices = nullptr;
            }
            if (!source) std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        if (!source) std::puts("NOT LISTED among the PC's cameras");
        else {
            ComPtr<IMFAttributes> options; MFCreateAttributes(&options, 1); options->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            ComPtr<IMFSourceReader> reader; HRESULT hr = MFCreateSourceReaderFromMediaSource(source.Get(), options.Get(), &reader);
            if (SUCCEEDED(hr)) { ComPtr<IMFMediaType> nv; MFCreateMediaType(&nv); nv->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); nv->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12); reader->SetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, nv.Get()); }
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15); int frames = 0;
            while (SUCCEEDED(hr) && std::chrono::steady_clock::now() < until && result != 0) {
                DWORD stream = 0, flags = 0; LONGLONG at = 0; ComPtr<IMFSample> sample;
                hr = reader->ReadSample(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &stream, &flags, &at, &sample);
                if (FAILED(hr)) { std::printf("READ FAILED 0x%08lx\n", static_cast<unsigned long>(hr)); break; }
                if (!sample) continue;
                ComPtr<IMFMediaType> type; reader->GetCurrentMediaType(DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &type); UINT32 fw = 0, fh = 0; MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &fw, &fh);
                ComPtr<IMFMediaBuffer> buffer; sample->ConvertToContiguousBuffer(&buffer); BYTE* p = nullptr; DWORD length = 0;
                if (buffer && SUCCEEDED(buffer->Lock(&p, nullptr, &length)) && fw && fh && length >= fw * fh) {
                    // The ramp, fitted into the camera's frame: dark towards the left, bright towards the right of the middle row.
                    const BYTE* row = p + size_t(fh / 2) * fw; const int left = row[fw / 8 + 8], right = row[fw - fw / 8 - 8];
                    if (++frames % 30 == 1 || right - left > 100) std::printf("frame %d: %ux%u, luma left %d right %d\n", frames, fw, fh, left, right);
                    if (right - left > 100) result = 0;
                    buffer->Unlock();
                }
            }
            source->Shutdown();
        }
    }
    stop = true; feed.join(); camera.close();
    std::puts(result == 0 ? "PASS" : "FAIL");
    MFShutdown(); CoUninitialize(); return result;
}
