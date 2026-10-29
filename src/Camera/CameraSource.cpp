// 0.27 (revision 10): "Arnav Island Phone Camera", the media source behind the island's virtual camera (MFCreateVirtualCamera,
// Windows 11). A small in-process COM server: its class gives an activation object, which makes the source: one live video
// stream, NV12 at 1280 x 720, 30 frames a second. Frames come from the island over a named pipe (CameraPipe.h): this DLL runs
// in Windows' Frame Server (another account, another session), so a pipe whose access the island grants is the way across.
// Until the island's first frame (and whenever it stops sending) the camera shows a dark frame.
#include "Camera/CameraPipe.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <new>

namespace {
using namespace nexus::camera;
// Not in MinGW's headers: the device stream attributes the Frame Server reads, IMFMediaStream2, and KS's capture pin.
const GUID streamCategory{0x2939e7b8, 0xa62e, 0x4579, {0xb6, 0x74, 0xd4, 0x07, 0x3d, 0xfa, 0xbb, 0xba}};
const GUID streamId{0x11bd5120, 0xd124, 0x446b, {0x88, 0xe6, 0x17, 0x06, 0x02, 0x57, 0xff, 0xf9}};
const GUID frameServerShared{0x1cb378e9, 0xb279, 0x41d4, {0xaf, 0x97, 0x34, 0xa2, 0x43, 0xe6, 0x83, 0x20}};
const GUID frameSourceTypes{0x17145fd1, 0x1b2b, 0x423c, {0x80, 0x01, 0x2b, 0x68, 0x33, 0xed, 0x35, 0x88}};
const GUID pinVideoCapture{0xfb6c4281, 0x0353, 0x11d1, {0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba}};
const IID iidStream2{0xc5bc37d6, 0x75c7, 0x46a1, {0xa1, 0x32, 0x81, 0xb5, 0xf7, 0x23, 0xc2, 0x0f}};
enum StreamState { stateStopped = 0, statePaused = 1, stateRunning = 2 };
struct IStream2 : IMFMediaStream { virtual HRESULT STDMETHODCALLTYPE SetStreamState(int) = 0; virtual HRESULT STDMETHODCALLTYPE GetStreamState(int*) = 0; };
// User mode's IKsControl (ksproxy.h's, HRESULTs; ks.h's is the driver one): the camera's controls.
const IID iidKsControl{0x28f54685, 0x06fd, 0x11d2, {0xb2, 0x7a, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96}};
struct IKsControl : IUnknown { virtual HRESULT STDMETHODCALLTYPE KsProperty(void*, ULONG, void*, ULONG, ULONG*) = 0; virtual HRESULT STDMETHODCALLTYPE KsMethod(void*, ULONG, void*, ULONG, ULONG*) = 0; virtual HRESULT STDMETHODCALLTYPE KsEvent(void*, ULONG, void*, ULONG, ULONG*) = 0; };
std::atomic<long> objects{0}, locks{0}; HMODULE self = nullptr;

// The latest frame from the island (a reader thread keeps it while the stream runs).
class Frames {
public:
    void start() { std::lock_guard g(lock_); if (running_) return; running_ = true; stop_ = false; worker_ = std::thread([this] { run(); }); }
    void stop() { { std::lock_guard g(lock_); if (!running_) return; running_ = false; stop_ = true; if (pipe_ != INVALID_HANDLE_VALUE) CancelIoEx(pipe_, nullptr); } if (worker_.joinable()) worker_.join(); }
    // The frame to send now (dark until the island has sent one, or after it has been quiet a second).
    void fill(BYTE* nv12) {
        std::lock_guard g(lock_);
        if (!latest_.empty() && GetTickCount64() - at_ < 1500) { memcpy(nv12, latest_.data(), frameBytes); return; }
        memset(nv12, 24, size_t(frameWidth) * frameHeight); memset(nv12 + size_t(frameWidth) * frameHeight, 128, size_t(frameWidth) * frameHeight / 2);
    }
private:
    void run() {
        std::vector<BYTE> buffer(frameBytes);
        while (!stop_) {
            HANDLE p = CreateFileW(pipeName, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (p == INVALID_HANDLE_VALUE) { for (int i = 0; i < 10 && !stop_; ++i) Sleep(50); continue; }
            { std::lock_guard g(lock_); pipe_ = p; }
            auto read = [&](void* to, DWORD n) { BYTE* d = static_cast<BYTE*>(to); while (n && !stop_) { DWORD got = 0; if (!ReadFile(p, d, n, &got, nullptr) || !got) return false; d += got; n -= got; } return n == 0; };
            for (FrameHeader h{}; !stop_ && read(&h, sizeof(h)) && h.magic == frameMagic && h.bytes == frameBytes && read(buffer.data(), frameBytes);) { std::lock_guard g(lock_); latest_.swap(buffer); buffer.resize(frameBytes); at_ = GetTickCount64(); }
            { std::lock_guard g(lock_); pipe_ = INVALID_HANDLE_VALUE; } CloseHandle(p);
        }
    }
    std::mutex lock_; std::thread worker_; std::atomic<bool> stop_{false}; bool running_ = false; HANDLE pipe_ = INVALID_HANDLE_VALUE; std::vector<BYTE> latest_; ULONGLONG at_ = 0;
};

class Source;
class Stream final : public IStream2, public IKsControl {
public:
    Stream(Source* source, IMFStreamDescriptor* descriptor) : source_(source), descriptor_(descriptor) { ++objects; descriptor_->AddRef(); MFCreateEventQueue(&queue_); }
    ~Stream() { if (queue_) queue_->Release(); descriptor_->Release(); --objects; }
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IMFMediaEventGenerator || iid == IID_IMFMediaStream || iid == iidStream2) *out = static_cast<IStream2*>(this);
        else if (iid == iidKsControl) *out = static_cast<IKsControl*>(this); else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ULONG(++refs_); }
    STDMETHODIMP_(ULONG) Release() override { const long n = --refs_; if (!n) delete this; return ULONG(n); }
    STDMETHODIMP GetEvent(DWORD f, IMFMediaEvent** e) override { IMFMediaEventQueue* q; { std::lock_guard g(lock_); if (!queue_) return MF_E_SHUTDOWN; q = queue_; q->AddRef(); } const HRESULT hr = q->GetEvent(f, e); q->Release(); return hr; }
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* c, IUnknown* s) override { std::lock_guard g(lock_); return queue_ ? queue_->BeginGetEvent(c, s) : MF_E_SHUTDOWN; }
    STDMETHODIMP EndGetEvent(IMFAsyncResult* r, IMFMediaEvent** e) override { std::lock_guard g(lock_); return queue_ ? queue_->EndGetEvent(r, e) : MF_E_SHUTDOWN; }
    STDMETHODIMP QueueEvent(MediaEventType t, REFGUID x, HRESULT s, const PROPVARIANT* v) override { std::lock_guard g(lock_); return queue_ ? queue_->QueueEventParamVar(t, x, s, v) : MF_E_SHUTDOWN; }
    STDMETHODIMP GetMediaSource(IMFMediaSource** out) override;
    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** out) override { if (!out) return E_POINTER; *out = descriptor_; descriptor_->AddRef(); return S_OK; }
    // A sample of the latest frame for each request, stamped with the system clock (the Frame Server's time base).
    STDMETHODIMP RequestSample(IUnknown* token) override {
        std::lock_guard g(lock_); if (!queue_) return MF_E_SHUTDOWN; if (state_ != stateRunning) return MF_E_INVALIDREQUEST;
        IMFMediaBuffer* buffer = nullptr; IMFSample* sample = nullptr; BYTE* data = nullptr; HRESULT hr = MFCreateMemoryBuffer(frameBytes, &buffer);
        if (SUCCEEDED(hr)) hr = buffer->Lock(&data, nullptr, nullptr);
        if (SUCCEEDED(hr)) { frames_.fill(data); buffer->Unlock(); buffer->SetCurrentLength(frameBytes); hr = MFCreateSample(&sample); }
        if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer);
        if (SUCCEEDED(hr)) { const LONGLONG now = MFGetSystemTime(); sample->SetSampleTime(now); sample->SetSampleDuration(10000000 / frameRate); sample->SetUINT32(MFSampleExtension_CleanPoint, 1); if (token) sample->SetUnknown(MFSampleExtension_Token, token);
            hr = queue_->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample); }
        if (sample) sample->Release(); if (buffer) buffer->Release(); return hr;
    }
    STDMETHODIMP SetStreamState(int s) override { std::lock_guard g(lock_); if (s == state_) return S_OK; state_ = s; if (s == stateRunning) frames_.start(); else frames_.stop(); return S_OK; }
    STDMETHODIMP GetStreamState(int* s) override { if (!s) return E_POINTER; std::lock_guard g(lock_); *s = state_; return S_OK; }
    // No camera controls (exposure, focus...): every KS request is a set this source doesn't have.
    STDMETHODIMP KsProperty(void*, ULONG, void*, ULONG, ULONG*) override { return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND); }
    STDMETHODIMP KsMethod(void*, ULONG, void*, ULONG, ULONG*) override { return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND); }
    STDMETHODIMP KsEvent(void*, ULONG, void*, ULONG, ULONG*) override { return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND); }
    HRESULT start() { { std::lock_guard g(lock_); if (!queue_) return MF_E_SHUTDOWN; state_ = stateRunning; } frames_.start(); PROPVARIANT v; PropVariantInit(&v); return QueueEvent(MEStreamStarted, GUID_NULL, S_OK, &v); }
    HRESULT stop() { { std::lock_guard g(lock_); if (!queue_) return MF_E_SHUTDOWN; state_ = stateStopped; } frames_.stop(); PROPVARIANT v; PropVariantInit(&v); return QueueEvent(MEStreamStopped, GUID_NULL, S_OK, &v); }
    void shutdown() { frames_.stop(); std::lock_guard g(lock_); if (queue_) { queue_->Shutdown(); queue_->Release(); queue_ = nullptr; } source_ = nullptr; }
private:
    std::atomic<long> refs_{1}; std::mutex lock_; Source* source_; IMFStreamDescriptor* descriptor_; IMFMediaEventQueue* queue_ = nullptr; int state_ = stateStopped; Frames frames_;
};

class Source final : public IMFMediaSourceEx, public IMFGetService, public IKsControl {
public:
    Source() { ++objects; }
    ~Source() { if (stream_) stream_->Release(); if (descriptor_) descriptor_->Release(); if (attributes_) attributes_->Release(); if (queue_) queue_->Release(); --objects; }
    HRESULT initialize() {
        HRESULT hr = MFCreateEventQueue(&queue_); if (SUCCEEDED(hr)) hr = MFCreateAttributes(&attributes_, 4);
        IMFMediaType* type = nullptr; IMFStreamDescriptor* sd = nullptr; IMFMediaTypeHandler* handler = nullptr; IMFAttributes* sa = nullptr;
        if (SUCCEEDED(hr)) hr = MFCreateMediaType(&type);
        if (SUCCEEDED(hr)) { type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12); MFSetAttributeSize(type, MF_MT_FRAME_SIZE, frameWidth, frameHeight); MFSetAttributeRatio(type, MF_MT_FRAME_RATE, frameRate, 1);
            MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1); type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1); type->SetUINT32(MF_MT_DEFAULT_STRIDE, frameWidth);
            type->SetUINT32(MF_MT_SAMPLE_SIZE, frameBytes); type->SetUINT32(MF_MT_AVG_BITRATE, frameBytes * 8 * frameRate); hr = MFCreateStreamDescriptor(0, 1, &type, &sd); }
        if (SUCCEEDED(hr)) hr = sd->GetMediaTypeHandler(&handler);
        if (SUCCEEDED(hr)) hr = handler->SetCurrentMediaType(type);
        // What the Frame Server reads of the stream: a colour capture pin, shared between apps.
        if (SUCCEEDED(hr)) hr = sd->QueryInterface(IID_PPV_ARGS(&sa));
        if (SUCCEEDED(hr)) { sa->SetGUID(streamCategory, pinVideoCapture); sa->SetUINT32(streamId, 0); sa->SetUINT32(frameServerShared, 1); sa->SetUINT32(frameSourceTypes, 1); }
        if (SUCCEEDED(hr)) hr = MFCreatePresentationDescriptor(1, &sd, &descriptor_);
        if (SUCCEEDED(hr)) hr = descriptor_->SelectStream(0);
        if (SUCCEEDED(hr)) { stream_ = new (std::nothrow) Stream(this, sd); if (!stream_) hr = E_OUTOFMEMORY; }
        if (sa) sa->Release(); if (handler) handler->Release(); if (sd) sd->Release(); if (type) type->Release(); return hr;
    }
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_IMFMediaEventGenerator || iid == IID_IMFMediaSource || iid == IID_IMFMediaSourceEx) *out = static_cast<IMFMediaSourceEx*>(this);
        else if (iid == IID_IMFGetService) *out = static_cast<IMFGetService*>(this); else if (iid == iidKsControl) *out = static_cast<IKsControl*>(this); else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ULONG(++refs_); }
    STDMETHODIMP_(ULONG) Release() override { const long n = --refs_; if (!n) delete this; return ULONG(n); }
    STDMETHODIMP GetEvent(DWORD f, IMFMediaEvent** e) override { IMFMediaEventQueue* q; { std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; q = queue_; q->AddRef(); } const HRESULT hr = q->GetEvent(f, e); q->Release(); return hr; }
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* c, IUnknown* s) override { std::lock_guard g(lock_); return shut_ ? MF_E_SHUTDOWN : queue_->BeginGetEvent(c, s); }
    STDMETHODIMP EndGetEvent(IMFAsyncResult* r, IMFMediaEvent** e) override { std::lock_guard g(lock_); return shut_ ? MF_E_SHUTDOWN : queue_->EndGetEvent(r, e); }
    STDMETHODIMP QueueEvent(MediaEventType t, REFGUID x, HRESULT s, const PROPVARIANT* v) override { std::lock_guard g(lock_); return shut_ ? MF_E_SHUTDOWN : queue_->QueueEventParamVar(t, x, s, v); }
    STDMETHODIMP GetCharacteristics(DWORD* c) override { if (!c) return E_POINTER; std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; *c = MFMEDIASOURCE_IS_LIVE; return S_OK; }
    STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** out) override { if (!out) return E_POINTER; std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; return descriptor_->Clone(out); }
    STDMETHODIMP Start(IMFPresentationDescriptor* pd, const GUID* format, const PROPVARIANT*) override {
        if (!pd) return E_INVALIDARG; if (format && *format != GUID_NULL) return MF_E_UNSUPPORTED_TIME_FORMAT;
        { std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; }
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UNKNOWN; v.punkVal = static_cast<IStream2*>(stream_);
        HRESULT hr = QueueEvent(started_ ? MEUpdatedStream : MENewStream, GUID_NULL, S_OK, &v); started_ = true;
        if (SUCCEEDED(hr)) hr = stream_->start();
        PROPVARIANT t; PropVariantInit(&t); t.vt = VT_I8; t.hVal.QuadPart = MFGetSystemTime(); if (SUCCEEDED(hr)) hr = QueueEvent(MESourceStarted, GUID_NULL, S_OK, &t);
        return hr;
    }
    STDMETHODIMP Stop() override { { std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; } HRESULT hr = stream_->stop(); PROPVARIANT v; PropVariantInit(&v); if (SUCCEEDED(hr)) hr = QueueEvent(MESourceStopped, GUID_NULL, S_OK, &v); return hr; }
    STDMETHODIMP Pause() override { return MF_E_INVALID_STATE_TRANSITION; }
    STDMETHODIMP Shutdown() override { std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; shut_ = true; if (stream_) stream_->shutdown(); queue_->Shutdown(); return S_OK; }
    STDMETHODIMP GetSourceAttributes(IMFAttributes** out) override { if (!out) return E_POINTER; std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN; *out = attributes_; attributes_->AddRef(); return S_OK; }
    STDMETHODIMP GetStreamAttributes(DWORD id, IMFAttributes** out) override {
        if (!out) return E_POINTER; if (id != 0) return MF_E_INVALIDSTREAMNUMBER; std::lock_guard g(lock_); if (shut_) return MF_E_SHUTDOWN;
        IMFStreamDescriptor* sd = nullptr; BOOL selected = FALSE; HRESULT hr = descriptor_->GetStreamDescriptorByIndex(0, &selected, &sd); if (FAILED(hr)) return hr;
        hr = sd->QueryInterface(IID_PPV_ARGS(out)); sd->Release(); return hr;
    }
    STDMETHODIMP SetD3DManager(IUnknown*) override { return S_OK; }
    STDMETHODIMP GetService(REFGUID, REFIID, LPVOID* out) override { if (out) *out = nullptr; return MF_E_UNSUPPORTED_SERVICE; }
    STDMETHODIMP KsProperty(void*, ULONG, void*, ULONG, ULONG*) override { return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND); }
    STDMETHODIMP KsMethod(void*, ULONG, void*, ULONG, ULONG*) override { return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND); }
    STDMETHODIMP KsEvent(void*, ULONG, void*, ULONG, ULONG*) override { return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND); }
    Stream* stream_ = nullptr;
private:
    std::atomic<long> refs_{1}; std::mutex lock_; IMFMediaEventQueue* queue_ = nullptr; IMFAttributes* attributes_ = nullptr; IMFPresentationDescriptor* descriptor_ = nullptr; bool shut_ = false, started_ = false;
};
STDMETHODIMP Stream::GetMediaSource(IMFMediaSource** out) { if (!out) return E_POINTER; std::lock_guard g(lock_); if (!source_) return MF_E_SHUTDOWN; *out = static_cast<IMFMediaSourceEx*>(source_); (*out)->AddRef(); return S_OK; }

// What the class makes: an activation object (its attributes kept for whoever asks), which makes the source.
class Activate final : public IMFActivate {
public:
    Activate() { ++objects; MFCreateAttributes(&a_, 4); }
    ~Activate() { if (source_) source_->Release(); if (a_) a_->Release(); --objects; }
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override { if (!out) return E_POINTER; *out = nullptr; if (iid == IID_IUnknown || iid == IID_IMFAttributes || iid == IID_IMFActivate) { *out = static_cast<IMFActivate*>(this); AddRef(); return S_OK; } return E_NOINTERFACE; }
    STDMETHODIMP_(ULONG) AddRef() override { return ULONG(++refs_); }
    STDMETHODIMP_(ULONG) Release() override { const long n = --refs_; if (!n) delete this; return ULONG(n); }
    STDMETHODIMP ActivateObject(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr; std::lock_guard g(lock_);
        if (!source_) { auto* s = new (std::nothrow) Source(); if (!s) return E_OUTOFMEMORY; const HRESULT hr = s->initialize(); if (FAILED(hr)) { s->Release(); return hr; } source_ = s; }
        return source_->QueryInterface(iid, out);
    }
    STDMETHODIMP ShutdownObject() override { std::lock_guard g(lock_); if (source_) { source_->Shutdown(); source_->Release(); source_ = nullptr; } return S_OK; }
    STDMETHODIMP DetachObject() override { std::lock_guard g(lock_); if (source_) { source_->Release(); source_ = nullptr; } return S_OK; }
    // IMFAttributes, kept by the attribute store it wraps.
    STDMETHODIMP GetItem(REFGUID k, PROPVARIANT* v) override { return a_->GetItem(k, v); } STDMETHODIMP GetItemType(REFGUID k, MF_ATTRIBUTE_TYPE* t) override { return a_->GetItemType(k, t); }
    STDMETHODIMP CompareItem(REFGUID k, REFPROPVARIANT v, BOOL* r) override { return a_->CompareItem(k, v, r); } STDMETHODIMP Compare(IMFAttributes* t, MF_ATTRIBUTES_MATCH_TYPE m, BOOL* r) override { return a_->Compare(t, m, r); }
    STDMETHODIMP GetUINT32(REFGUID k, UINT32* v) override { return a_->GetUINT32(k, v); } STDMETHODIMP GetUINT64(REFGUID k, UINT64* v) override { return a_->GetUINT64(k, v); }
    STDMETHODIMP GetDouble(REFGUID k, double* v) override { return a_->GetDouble(k, v); } STDMETHODIMP GetGUID(REFGUID k, GUID* v) override { return a_->GetGUID(k, v); }
    STDMETHODIMP GetStringLength(REFGUID k, UINT32* n) override { return a_->GetStringLength(k, n); } STDMETHODIMP GetString(REFGUID k, LPWSTR v, UINT32 n, UINT32* l) override { return a_->GetString(k, v, n, l); }
    STDMETHODIMP GetAllocatedString(REFGUID k, LPWSTR* v, UINT32* l) override { return a_->GetAllocatedString(k, v, l); } STDMETHODIMP GetBlobSize(REFGUID k, UINT32* n) override { return a_->GetBlobSize(k, n); }
    STDMETHODIMP GetBlob(REFGUID k, UINT8* b, UINT32 n, UINT32* l) override { return a_->GetBlob(k, b, n, l); } STDMETHODIMP GetAllocatedBlob(REFGUID k, UINT8** b, UINT32* n) override { return a_->GetAllocatedBlob(k, b, n); }
    STDMETHODIMP GetUnknown(REFGUID k, REFIID i, LPVOID* v) override { return a_->GetUnknown(k, i, v); } STDMETHODIMP SetItem(REFGUID k, REFPROPVARIANT v) override { return a_->SetItem(k, v); }
    STDMETHODIMP DeleteItem(REFGUID k) override { return a_->DeleteItem(k); } STDMETHODIMP DeleteAllItems() override { return a_->DeleteAllItems(); }
    STDMETHODIMP SetUINT32(REFGUID k, UINT32 v) override { return a_->SetUINT32(k, v); } STDMETHODIMP SetUINT64(REFGUID k, UINT64 v) override { return a_->SetUINT64(k, v); }
    STDMETHODIMP SetDouble(REFGUID k, double v) override { return a_->SetDouble(k, v); } STDMETHODIMP SetGUID(REFGUID k, REFGUID v) override { return a_->SetGUID(k, v); }
    STDMETHODIMP SetString(REFGUID k, LPCWSTR v) override { return a_->SetString(k, v); } STDMETHODIMP SetBlob(REFGUID k, const UINT8* b, UINT32 n) override { return a_->SetBlob(k, b, n); }
    STDMETHODIMP SetUnknown(REFGUID k, IUnknown* v) override { return a_->SetUnknown(k, v); } STDMETHODIMP LockStore() override { return a_->LockStore(); } STDMETHODIMP UnlockStore() override { return a_->UnlockStore(); }
    STDMETHODIMP GetCount(UINT32* n) override { return a_->GetCount(n); } STDMETHODIMP GetItemByIndex(UINT32 i, GUID* k, PROPVARIANT* v) override { return a_->GetItemByIndex(i, k, v); }
    STDMETHODIMP CopyAllItems(IMFAttributes* d) override { return a_->CopyAllItems(d); }
private:
    std::atomic<long> refs_{1}; std::mutex lock_; IMFAttributes* a_ = nullptr; Source* source_ = nullptr;
};

class Factory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override { if (!out) return E_POINTER; if (iid == IID_IUnknown || iid == IID_IClassFactory) { *out = this; return S_OK; } *out = nullptr; return E_NOINTERFACE; }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; } STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr; if (outer) return CLASS_E_NOAGGREGATION;
        auto* a = new (std::nothrow) Activate(); if (!a) return E_OUTOFMEMORY; const HRESULT hr = a->QueryInterface(iid, out); a->Release(); return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override { if (lock) ++locks; else --locks; return S_OK; }
} factory;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD why, LPVOID) { if (why == DLL_PROCESS_ATTACH) { self = instance; DisableThreadLibraryCalls(instance); } return TRUE; }
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID clsid, REFIID iid, void** out) {
    if (!out) return E_POINTER; *out = nullptr; if (clsid != sourceClsid) return CLASS_E_CLASSNOTAVAILABLE; return factory.QueryInterface(iid, out);
}
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE DllCanUnloadNow() { return objects == 0 && locks == 0 ? S_OK : S_FALSE; }
