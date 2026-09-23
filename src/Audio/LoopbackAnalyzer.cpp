#include "LoopbackAnalyzer.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <vector>
namespace nexus {
namespace {
class DefaultChanged final:public IMMNotificationClient {
    std::atomic<ULONG> refs_{1};HANDLE event_;
public:
    explicit DefaultChanged(HANDLE e):event_(e){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IMMNotificationClient)){*p=static_cast<IMMNotificationClient*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow f,ERole r,LPCWSTR)override{if(f==eRender&&r==eConsole)SetEvent(event_);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY)override{return S_OK;}
};
// KSDATAFORMAT_SUBTYPE_IEEE_FLOAT without depending on ksuser.lib.
constexpr GUID floatSubtype{0x00000003,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
struct Format {bool floating=false;int bits=0,channels=0,rate=48000,block=0;};
Format describe(const WAVEFORMATEX* f){
    Format r;r.channels=f->nChannels;r.rate=int(f->nSamplesPerSec);r.bits=f->wBitsPerSample;r.block=f->nBlockAlign;r.floating=f->wFormatTag==WAVE_FORMAT_IEEE_FLOAT;
    if(f->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&f->cbSize>=22){auto x=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(f);r.floating=IsEqualGUID(x->SubFormat,floatSubtype);}
    return r;
}
}
LoopbackAnalyzer::LoopbackAnalyzer(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){if(!stop_||!wake_)throw std::runtime_error("Spectrum event creation failed");worker_=std::thread([this]{run();});}
LoopbackAnalyzer::~LoopbackAnalyzer(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
void LoopbackAnalyzer::run(){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;
    {
        ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<DefaultChanged> notify;HANDLE deviceChanged=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)))){notify.Attach(new DefaultChanged(deviceChanged));enumerator->RegisterEndpointNotificationCallback(notify.Get());}
        Spectrum spectrum;std::vector<float> mono;bool stopping=false;
        while(!stopping&&enumerator){
            if(!active_.load()){HANDLE h[]={stop_,wake_};if(WaitForMultipleObjects(2,h,FALSE,INFINITE)==WAIT_OBJECT_0)break;continue;}
            ComPtr<IMMDevice> device;ComPtr<IAudioClient> client;ComPtr<IAudioCaptureClient> capture;WAVEFORMATEX* mix=nullptr;Format format;
            HRESULT hr=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device);
            if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf()));
            if(SUCCEEDED(hr))hr=client->GetMixFormat(&mix);
            if(SUCCEEDED(hr)){format=describe(mix);hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,mix,nullptr);}
            if(SUCCEEDED(hr))hr=client->GetService(IID_PPV_ARGS(&capture));
            if(SUCCEEDED(hr))hr=client->Start();
            if(mix)CoTaskMemFree(mix);
            bool supported=SUCCEEDED(hr)&&format.channels>0&&(format.floating?format.bits==32:(format.bits==16||format.bits==24||format.bits==32));available=supported;
            if(!supported){HANDLE h[]={stop_,wake_,deviceChanged};if(WaitForMultipleObjects(3,h,FALSE,2000)==WAIT_OBJECT_0)stopping=true;continue;}
            double last=seconds(),lastPost=0,lastPacket=seconds();bool wasResting=true;
            while(active_.load()){
                HANDLE h[]={stop_,deviceChanged,wake_};DWORD wait=WaitForMultipleObjects(3,h,FALSE,10);
                if(wait==WAIT_OBJECT_0){stopping=true;break;}if(wait==WAIT_OBJECT_0+1)break;
                UINT32 packet=0;bool received=false;
                while(SUCCEEDED(hr=capture->GetNextPacketSize(&packet))&&packet){
                    BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;if(FAILED(hr=capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr)))break;
                    mono.resize(frames);
                    if(flags&AUDCLNT_BUFFERFLAGS_SILENT)std::fill(mono.begin(),mono.end(),0.f);
                    else for(UINT32 f=0;f<frames;++f){float sum=0;const BYTE* p=data+size_t(f)*format.block;for(int c=0;c<format.channels;++c){float v;
                        if(format.floating)v=reinterpret_cast<const float*>(p)[c];
                        else if(format.bits==16)v=reinterpret_cast<const int16_t*>(p)[c]/32768.f;
                        else if(format.bits==24){const BYTE* s=p+c*3;int32_t x=(int32_t(s[0])<<8)|(int32_t(s[1])<<16)|(int32_t(s[2])<<24);v=float(x)/2147483648.f;}
                        else v=reinterpret_cast<const int32_t*>(p)[c]/2147483648.f;sum+=v;}mono[f]=sum/format.channels;}
                    spectrum.push(mono.data(),int(frames));capture->ReleaseBuffer(frames);received=true;
                }
                if(hr==AUDCLNT_E_DEVICE_INVALIDATED||FAILED(hr))break;
                double now=seconds(),dt=std::clamp(now-last,.001,.1);last=now;
                // The engine delivers nothing while idle; after a real gap, let the spectrum decay.
                if(received)lastPacket=now;else if(now-lastPacket>.06)spectrum.pushSilence(int(dt*format.rate));
                spectrum.analyze(float(format.rate),float(dt));bool resting=spectrum.resting();
                {std::lock_guard lock(mutex_);frame_.bands=spectrum.bands;frame_.level=spectrum.level;frame_.resting=resting;}
                // Post at most ~120 times a second; the compositor interpolates between frames.
                if((!resting||!wasResting)&&now-lastPost>=.008&&!pending.exchange(true)){PostMessageW(window_,SpectrumMessage,0,0);lastPost=now;}
                wasResting=resting;
            }
            client->Stop();
            if(!active_.load()){spectrum=Spectrum{};std::lock_guard lock(mutex_);frame_=SpectrumFrame{};}
        }
        if(enumerator&&notify)enumerator->UnregisterEndpointNotificationCallback(notify.Get());CloseHandle(deviceChanged);
    }
    CoUninitialize();
}
}
