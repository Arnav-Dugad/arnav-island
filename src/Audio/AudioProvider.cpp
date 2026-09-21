#include <cmath>
#include "AudioProvider.h"
#include <algorithm>
namespace nexus {
class VolumeCallback final:public IAudioEndpointVolumeCallback {
    std::atomic<ULONG> refs_{1};AudioProvider& owner_;HWND window_;
public:
    VolumeCallback(AudioProvider& p,HWND w):owner_(p),window_(w){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override {if(!p)return E_POINTER;*p=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IAudioEndpointVolumeCallback)){*p=static_cast<IAudioEndpointVolumeCallback*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA n)override{owner_.value=int(std::lround(n->fMasterVolume*100));owner_.muted=n->bMuted!=FALSE;if(!owner_.notificationPending.exchange(true))PostMessageW(window_,AudioMessage,1,0);return S_OK;}
};
class DeviceCallback final:public IMMNotificationClient {
    std::atomic<ULONG> refs_{1};HANDLE event_;
public:
    explicit DeviceCallback(HANDLE e):event_(e){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IMMNotificationClient)){*p=static_cast<IMMNotificationClient*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow f,ERole r,LPCWSTR)override{if(f==eRender&&r==eMultimedia)SetEvent(event_);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)override{SetEvent(event_);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY)override{return S_OK;}
};
AudioProvider::AudioProvider(HWND w):window_(w){changed_=CreateEventW(nullptr,FALSE,FALSE,nullptr);stop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!changed_||!stop_)throw std::runtime_error("Audio event creation failed");worker_=std::thread([this]{run();});}
AudioProvider::~AudioProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(changed_);CloseHandle(stop_);}
void AudioProvider::run(){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;
    {
    ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IAudioEndpointVolume> endpoint;
    ComPtr<VolumeCallback> volume;volume.Attach(new VolumeCallback(*this,window_));
    ComPtr<DeviceCallback> devices;devices.Attach(new DeviceCallback(changed_));
    try{
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));
        check(enumerator->RegisterEndpointNotificationCallback(devices.Get()));
        bool done=false;
        while(!done){
            int requested=requested_.exchange(-1);bool mute=muteRequest_.exchange(false);if(mute&&endpoint){BOOL current=FALSE;if(SUCCEEDED(endpoint->GetMute(&current)))endpoint->SetMute(!current,nullptr);}
            if(requested>=0&&endpoint) endpoint->SetMasterVolumeLevelScalar(requested/100.f,nullptr);
            else {
                if(endpoint){endpoint->UnregisterControlChangeNotify(volume.Get());endpoint.Reset();}
                available=false;ComPtr<IMMDevice> device;
                if(SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device))&&SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(endpoint.GetAddressOf())))){
                    float v=0;BOOL m=FALSE;endpoint->GetMasterVolumeLevelScalar(&v);endpoint->GetMute(&m);value=int(std::lround(v*100));muted=m;available=true;check(endpoint->RegisterControlChangeNotify(volume.Get()));
                }
                PostMessageW(window_,AudioMessage,0,0);
            }
            HANDLE events[]={stop_,changed_};done=WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0;
        }
    }catch(...){available=false;PostMessageW(window_,AudioMessage,0,0);}
    if(endpoint)endpoint->UnregisterControlChangeNotify(volume.Get());
    if(enumerator)enumerator->UnregisterEndpointNotificationCallback(devices.Get());
    }
    CoUninitialize();
}
}
