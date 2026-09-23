#include "SessionMixer.h"
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <map>
#include <filesystem>
// Documented Core Audio meter interface; MinGW's endpointvolume.h only forward-declares it.
struct IAudioMeterInformation:public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32,float*)=0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*)=0;
};
__CRT_UUID_DECL(IAudioMeterInformation,0xc02216f6,0x8c67,0x4b5b,0x9d,0x00,0xd0,0x08,0xe7,0x3e,0x00,0x64)
namespace nexus {
namespace {
// Identifies volume changes made from the island so echoes can be recognized.
const GUID islandContext{0x5a3f7c21,0x9b1e,0x4d52,{0x8e,0x61,0x2c,0x4f,0x17,0x93,0xa0,0x6d}};
template<class T> class Callback:public T {
protected:std::atomic<ULONG> refs_{1};HANDLE event_;
public:
    explicit Callback(HANDLE e):event_(e){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(T)){*p=static_cast<T*>(this);this->AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
};
class SessionCreated final:public Callback<IAudioSessionNotification>{public:using Callback::Callback;HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl*)override{SetEvent(event_);return S_OK;}};
class SessionEvents final:public Callback<IAudioSessionEvents>{public:using Callback::Callback;
    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR,LPCGUID)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR,LPCGUID)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float,BOOL,LPCGUID)override{SetEvent(event_);return S_OK;}HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD,float*,DWORD,LPCGUID)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID,LPCGUID)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState)override{SetEvent(event_);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason)override{SetEvent(event_);return S_OK;}};
class DeviceEvents final:public Callback<IMMNotificationClient>{public:using Callback::Callback;
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow f,ERole r,LPCWSTR)override{if(f==eRender&&r==eMultimedia)SetEvent(event_);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY)override{return S_OK;}};
std::wstring description(const std::wstring& path){
    DWORD ignored=0,size=GetFileVersionInfoSizeW(path.c_str(),&ignored);if(!size)return {};std::vector<BYTE> data(size);if(!GetFileVersionInfoW(path.c_str(),0,size,data.data()))return {};
    struct Translation{WORD language,codepage;}* t=nullptr;UINT length=0;if(!VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&t),&length)||length<sizeof(Translation))return {};
    wchar_t key[64];swprintf(key,64,L"\\StringFileInfo\\%04x%04x\\FileDescription",t->language,t->codepage);wchar_t* value=nullptr;if(!VerQueryValueW(data.data(),key,reinterpret_cast<void**>(&value),&length)||!value||!length)return {};
    std::wstring result(value);while(!result.empty()&&iswspace(result.back()))result.pop_back();return result.size()<=48?result:std::wstring{};
}
}
std::wstring processName(DWORD pid,std::wstring* pathOut){
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return {};wchar_t path[MAX_PATH*2];DWORD size=MAX_PATH*2;std::wstring result;
    if(QueryFullProcessImageNameW(process,0,path,&size)){std::wstring p(path,size);if(pathOut)*pathOut=p;result=description(p);if(result.empty())result=std::filesystem::path(p).stem().wstring();}
    CloseHandle(process);return result;
}
std::shared_ptr<const Artwork> shellIcon(const std::wstring& name,int size){
    ComPtr<IShellItemImageFactory> item;if(FAILED(SHCreateItemFromParsingName(name.c_str(),nullptr,IID_PPV_ARGS(&item))))return {};
    HBITMAP bitmap=nullptr;if(FAILED(item->GetImage({size,size},SIIGBF_ICONONLY|SIIGBF_BIGGERSIZEOK,&bitmap))||!bitmap)return {};
    struct Cleanup{HBITMAP b;~Cleanup(){DeleteObject(b);}} cleanup{bitmap};
    try{
        ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        ComPtr<IWICBitmap> image;check(factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapUsePremultipliedAlpha,&image));
        ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));check(converter->Initialize(image.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        auto art=std::make_shared<Artwork>();check(converter->GetSize(&art->width,&art->height));if(!art->width||!art->height||art->width>256||art->height>256)return {};
        art->pixels.resize(size_t(art->width)*art->height*4);check(converter->CopyPixels(nullptr,art->width*4,UINT(art->pixels.size()),art->pixels.data()));
        bool any=false;for(size_t i=3;i<art->pixels.size();i+=4)if(art->pixels[i]){any=true;break;}return any?art:nullptr;
    }catch(...){return {};}
}
SessionMixer::SessionMixer(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),changed_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){if(!stop_||!changed_)throw std::runtime_error("Mixer event creation failed");worker_=std::thread([this]{run();});}
SessionMixer::~SessionMixer(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(changed_);}
void SessionMixer::run(){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;
    {
        ComPtr<IMMDeviceEnumerator> enumerator;HANDLE deviceChanged=CreateEventW(nullptr,FALSE,FALSE,nullptr);ComPtr<DeviceEvents> deviceEvents;
        if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)))){deviceEvents.Attach(new DeviceEvents(deviceChanged));enumerator->RegisterEndpointNotificationCallback(deviceEvents.Get());}
        struct Tracked {ComPtr<IAudioSessionControl2> control;ComPtr<ISimpleAudioVolume> volume;ComPtr<IAudioMeterInformation> meter;ComPtr<SessionEvents> events;DWORD pid=0;bool system=false;};
        std::map<DWORD,std::pair<std::wstring,std::shared_ptr<const Artwork>>> identities;bool stopping=false;
        while(!stopping&&enumerator){
            ComPtr<IMMDevice> device;ComPtr<IAudioSessionManager2> manager;ComPtr<SessionCreated> created;std::map<std::wstring,Tracked> tracked;
            if(FAILED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device))||FAILED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(manager.GetAddressOf())))){
                available=false;{std::lock_guard lock(mutex_);entries_.clear();}if(!pending.exchange(true))PostMessageW(window_,MixerMessage,0,0);
                HANDLE h[]={stop_,deviceChanged};if(WaitForMultipleObjects(2,h,FALSE,5000)==WAIT_OBJECT_0)stopping=true;continue;}
            created.Attach(new SessionCreated(changed_));manager->RegisterSessionNotification(created.Get());available=true;
            bool rebind=false,enumerate=true;
            while(!stopping&&!rebind){
                std::vector<Command> commands;{std::lock_guard lock(mutex_);commands.swap(commands_);}
                for(auto& c:commands)for(auto& [key,t]:tracked)if(t.pid==c.pid&&t.volume){if(c.kind==0)t.volume->SetMasterVolume(c.value,&islandContext);else t.volume->SetMute(c.value!=0,&islandContext);}
                // Re-enumerate; the first enumeration also enables creation notifications.
                ComPtr<IAudioSessionEnumerator> list;int count=0;std::map<std::wstring,Tracked> next;
                if(enumerate&&SUCCEEDED(manager->GetSessionEnumerator(&list))&&SUCCEEDED(list->GetCount(&count))){
                    for(int i=0;i<std::min(count,64);++i){ComPtr<IAudioSessionControl> control;ComPtr<IAudioSessionControl2> control2;if(FAILED(list->GetSession(i,&control))||FAILED(control.As(&control2)))continue;
                        AudioSessionState state;if(FAILED(control2->GetState(&state))||state==AudioSessionStateExpired)continue;
                        // Instance identifiers are stable; COM wrappers may differ per enumeration.
                        LPWSTR instance=nullptr;if(FAILED(control2->GetSessionInstanceIdentifier(&instance))||!instance)continue;std::wstring key(instance);CoTaskMemFree(instance);auto found=tracked.find(key);Tracked t;
                        if(found!=tracked.end())t=found->second;else{t.control=control2;control.As(&t.volume);control.As(&t.meter);control2->GetProcessId(&t.pid);t.system=control2->IsSystemSoundsSession()==S_OK;t.events.Attach(new SessionEvents(changed_));control2->RegisterAudioSessionNotification(t.events.Get());}
                        next[key]=t;}
                }
                if(enumerate){for(auto& [key,t]:tracked)if(!next.contains(key))t.control->UnregisterAudioSessionNotification(t.events.Get());tracked=std::move(next);}
                std::map<DWORD,MixerEntry> grouped;
                for(auto& [key,t]:tracked){DWORD id=t.system?0:t.pid;auto& e=grouped[id];e.pid=id;e.system=t.system;AudioSessionState state;if(SUCCEEDED(t.control->GetState(&state))&&state==AudioSessionStateActive)e.active=true;
                    if(t.volume){float v=1;BOOL m=FALSE;t.volume->GetMasterVolume(&v);t.volume->GetMute(&m);e.volume=v;e.muted=m;}
                    if(metering_.load()&&t.meter){float p=0;if(SUCCEEDED(t.meter->GetPeakValue(&p)))e.peak=std::max(e.peak,p);}}
                std::vector<MixerEntry> result;
                for(auto& [id,e]:grouped){
                    if(!e.system){auto it=identities.find(id);if(it==identities.end()){std::wstring path;auto name=processName(id,&path);it=identities.emplace(id,std::pair{name,path.empty()?nullptr:shellIcon(path)}).first;}
                        if(it->second.first.empty())continue;e.name=it->second.first;e.icon=it->second.second;}
                    else e.name=L"System sounds";
                    result.push_back(std::move(e));}
                std::stable_sort(result.begin(),result.end(),[](const MixerEntry& a,const MixerEntry& b){if(a.system!=b.system)return b.system;if(a.active!=b.active)return a.active;return _wcsicmp(a.name.c_str(),b.name.c_str())<0;});
                if(result.size()>16)result.resize(16);
                {std::lock_guard lock(mutex_);entries_=std::move(result);}if(!pending.exchange(true))PostMessageW(window_,MixerMessage,0,0);
                HANDLE h[]={stop_,changed_,deviceChanged};DWORD wait=WaitForMultipleObjects(3,h,FALSE,metering_.load()?50:INFINITE);
                if(wait==WAIT_OBJECT_0)stopping=true;else if(wait==WAIT_OBJECT_0+2)rebind=true;enumerate=wait!=WAIT_TIMEOUT;
            }
            for(auto& [key,t]:tracked)t.control->UnregisterAudioSessionNotification(t.events.Get());manager->UnregisterSessionNotification(created.Get());
        }
        if(enumerator&&deviceEvents)enumerator->UnregisterEndpointNotificationCallback(deviceEvents.Get());CloseHandle(deviceChanged);
    }
    CoUninitialize();
}
}
