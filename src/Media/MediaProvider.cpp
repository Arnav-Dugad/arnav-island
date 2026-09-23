#include "MediaProvider.h"
#include "MediaAbi.h"
#include <roapi.h>
#include <shcore.h>
#include <wincodec.h>
namespace nexus {
using namespace mediaabi;
namespace {
std::shared_ptr<void> signal(){HANDLE h=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!h)throw std::runtime_error("Media event allocation failed");return {h,[](void* p){CloseHandle(p);}};}
class CallbackBase {
protected:
    std::atomic<ULONG> refs_{1};GUID iid_;std::shared_ptr<void> event_;
    CallbackBase(const wchar_t* id,std::shared_ptr<void> e):iid_(guid(id)),event_(std::move(e)){}
    HRESULT query(REFIID id,void** out,IUnknown* self){if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IAgileObject||id==iid_){*out=self;self->AddRef();return S_OK;}return E_NOINTERFACE;}
};
class Completion final:public IUnknown,public CallbackBase {
public:
    Completion(const wchar_t* id,std::shared_ptr<void> e):CallbackBase(id,std::move(e)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{return query(id,p,this);}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs_;if(!n)delete this;return n;}
    virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable*,AsyncStatus){SetEvent(event_.get());return S_OK;}
};
class Changed final:public IUnknown,public CallbackBase {
public:
    Changed(const wchar_t* id,std::shared_ptr<void> e):CallbackBase(id,std::move(e)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{return query(id,p,this);}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs_;if(!n)delete this;return n;}
    virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable*,IInspectable*){SetEvent(event_.get());return S_OK;}
};
template<class T> ComPtr<T> awaitResult(Async* operation,const wchar_t* completionIid,HANDLE stop){
    auto ready=signal();ComPtr<Completion> callback;callback.Attach(new Completion(completionIid,ready));check(operation->put_Completed(callback.Get()));
    HANDLE events[]={stop,ready.get()};DWORD result=WaitForMultipleObjects(2,events,FALSE,5000);
    if(result!=WAIT_OBJECT_0+1){ComPtr<IAsyncInfo> info;if(SUCCEEDED(operation->QueryInterface(IID_PPV_ARGS(&info))))info->Cancel();throw std::runtime_error("Media operation cancelled or timed out");}
    ComPtr<T> value;check(operation->GetResults(reinterpret_cast<IInspectable**>(value.GetAddressOf())));return value;
}
std::wstring consume(HSTRING h){UINT32 length=0;auto p=WindowsGetStringRawBuffer(h,&length);std::wstring s(p,length);WindowsDeleteString(h);return s;}
std::shared_ptr<const Artwork> readArtwork(Properties* properties,HANDLE stop){
    ComPtr<StreamReference> reference;if(FAILED(properties->get_Thumbnail(&reference))||!reference)return {};
    ComPtr<Async> operation;check(reference->OpenReadAsync(&operation));auto random=awaitResult<IInspectable>(operation.Get(),L"{3dddecf4-1d39-58e8-83b1-dbed541c7f35}",stop);
    ComPtr<IStream> stream;check(CreateStreamOverRandomAccessStream(random.Get(),IID_PPV_ARGS(&stream)));
    STATSTG stat{};if(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME))&&stat.cbSize.QuadPart>16*1024*1024)return {};
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));UINT w=0,h=0;check(frame->GetSize(&w,&h));if(!w||!h||w>8192||h>8192||uint64_t(w)*h>16777216)return {};
    double scale=std::min(1.,512./std::max(w,h));w=std::max(1u,UINT(w*scale));h=std::max(1u,UINT(h*scale));
    ComPtr<IWICBitmapScaler> scaler;check(factory->CreateBitmapScaler(&scaler));check(scaler->Initialize(frame.Get(),w,h,WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));check(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    auto art=std::make_shared<Artwork>();art->width=w;art->height=h;art->pixels.resize(size_t(w)*h*4);check(converter->CopyPixels(nullptr,w*4,UINT(art->pixels.size()),art->pixels.data()));uint64_t red=0,green=0,blue=0,count=0;for(size_t i=0;i+3<art->pixels.size();i+=64){if(art->pixels[i+3]<200)continue;red+=art->pixels[i+2];green+=art->pixels[i+1];blue+=art->pixels[i];++count;}if(count){auto tone=[&](uint64_t v){return uint32_t(150+v/count*80/255);};art->accent=(tone(red)<<16)|(tone(green)<<8)|tone(blue);}return art;
}
}
MediaProvider::MediaProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),changed_(signal()){if(!stop_)throw std::runtime_error("Media stop event allocation failed");worker_=std::thread([this]{run();});}
MediaProvider::~MediaProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);}
void MediaProvider::publish(MediaSnapshot s){{std::lock_guard lock(mutex_);latest_=std::move(s);}PostMessageW(window_,MediaMessage,0,0);}
void MediaProvider::run(){
    if(FAILED(RoInitialize(RO_INIT_MULTITHREADED)))return;
    {
    ComPtr<Manager> manager;ComPtr<Session> session;INT64 managerToken=0,mediaToken=0,playToken=0,timeToken=0;bool managerSubscribed=false,mediaSubscribed=false,playSubscribed=false,timeSubscribed=false;
    ComPtr<Changed> managerChanged,mediaChanged,playChanged,timeChanged;MediaSnapshot previous;uint64_t revision=0;
    try{
        HSTRING name=nullptr;const wchar_t* runtime=L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";check(WindowsCreateString(runtime,UINT32(wcslen(runtime)),&name));
        ComPtr<Statics> factory;HRESULT hr=RoGetActivationFactory(name,guid(L"{2050c4ee-11a0-57de-aed7-c97c70338245}"),reinterpret_cast<void**>(factory.GetAddressOf()));WindowsDeleteString(name);check(hr);
        ComPtr<Async> request;check(factory->RequestAsync(&request));manager=awaitResult<Manager>(request.Get(),L"{10f0074e-923d-5510-8f4a-dde37754ca0e}",stop_);
        managerChanged.Attach(new Changed(L"{228bd0ed-1fa2-5e9b-a6ec-42566173103b}",changed_));check(manager->add_CurrentSessionChanged(managerChanged.Get(),&managerToken));managerSubscribed=true;
        mediaChanged.Attach(new Changed(L"{0f2ce2b7-afa7-5ed0-8cb6-8c40cf9b3a5f}",changed_));
        playChanged.Attach(new Changed(L"{2bdf1426-d41f-5896-897f-efc0b0fa7392}",changed_));
        timeChanged.Attach(new Changed(L"{e8bf62af-fac1-5fff-9053-0bf191ae777e}",changed_));
        for(;;){
            ComPtr<Session> next;check(manager->GetCurrentSession(&next));
            if(next.Get()!=session.Get()){
                if(session&&mediaSubscribed)session->remove_MediaPropertiesChanged(mediaToken);
                if(session&&playSubscribed)session->remove_PlaybackInfoChanged(playToken);
                if(session&&timeSubscribed)session->remove_TimelinePropertiesChanged(timeToken);
                mediaSubscribed=playSubscribed=timeSubscribed=false;session=next;previous={};
                if(session){check(session->add_MediaPropertiesChanged(mediaChanged.Get(),&mediaToken));mediaSubscribed=true;playSubscribed=SUCCEEDED(session->add_PlaybackInfoChanged(playChanged.Get(),&playToken));timeSubscribed=SUCCEEDED(session->add_TimelinePropertiesChanged(timeChanged.Get(),&timeToken));}
            }
            MediaSnapshot snapshot;
            if(session){
                int command=command_.exchange(0);ComPtr<IInspectable> action;
                if(command==1)session->TryTogglePlayPauseAsync(&action);else if(command==2)session->TrySkipPreviousAsync(&action);else if(command==3)session->TrySkipNextAsync(&action);
                ComPtr<Async> props;check(session->TryGetMediaPropertiesAsync(&props));auto properties=awaitResult<Properties>(props.Get(),L"{84593a3d-951a-55b6-8353-5205e577797b}",stop_);
                HSTRING title=nullptr,artist=nullptr;check(properties->get_Title(&title));snapshot.title=consume(title);check(properties->get_Artist(&artist));snapshot.artist=consume(artist);snapshot.available=true;
                if(snapshot.title.empty())snapshot.title=L"Untitled media";
                HSTRING source=nullptr;if(SUCCEEDED(session->get_SourceAppUserModelId(&source)))snapshot.source=consume(source);
                ComPtr<EnumReference> type;INT32 kind=0;if(SUCCEEDED(properties->get_PlaybackType(&type))&&type)type->get_Value(&kind);snapshot.kind=classifyMedia(kind,snapshot.source);
                if(previous.artwork&&snapshot.title==previous.title&&snapshot.artist==previous.artist&&snapshot.source==previous.source)snapshot.artwork=previous.artwork;
                else try{snapshot.artwork=readArtwork(properties.Get(),stop_);}catch(...){/* An absent or corrupt thumbnail never disables transport. */}
                ComPtr<PlaybackInfo> playback;if(SUCCEEDED(session->GetPlaybackInfo(reinterpret_cast<IInspectable**>(playback.GetAddressOf())))&&playback){INT32 status=0;playback->get_Status(&status);snapshot.playing=status==4;ComPtr<Controls> controls;if(SUCCEEDED(playback->get_Controls(&controls))&&controls){BYTE enabled=0;controls->Toggle(&enabled);snapshot.canToggle=enabled;controls->Previous(&enabled);snapshot.canPrevious=enabled;controls->Next(&enabled);snapshot.canNext=enabled;enabled=0;if(SUCCEEDED(controls->Position(&enabled)))snapshot.canSeek=enabled;}}
                ComPtr<Timeline> timeline;if(SUCCEEDED(session->GetTimelineProperties(reinterpret_cast<IInspectable**>(timeline.GetAddressOf())))&&timeline){INT64 start=0,end=0,position=0;timeline->get_Start(&start);timeline->get_End(&end);timeline->get_Position(&position);snapshot.duration=std::max(0.,double(end-start)/1e7);snapshot.position=std::clamp(double(position-start)/1e7,0.,snapshot.duration);INT64 lo=0,hi=0;timeline->get_MinSeek(&lo);timeline->get_MaxSeek(&hi);snapshot.seekMin=std::clamp(double(lo-start)/1e7,0.,snapshot.duration);snapshot.seekMax=std::clamp(double(hi-start)/1e7,0.,snapshot.duration);snapshot.canSeek=snapshot.canSeek&&snapshot.seekMax>snapshot.seekMin;double requested=-1;std::wstring source,title;{std::lock_guard lock(mutex_);requested=requestedSeek_;requestedSeek_=-1;source=seekSource_;title=seekTitle_;}if(requested>=0&&snapshot.canSeek&&source==snapshot.source&&title==snapshot.title){ComPtr<IInspectable> operation;session->TryChangePlaybackPositionAsync(start+INT64(std::clamp(requested,snapshot.seekMin,snapshot.seekMax)*1e7),&operation);}}
                snapshot.sampledAt=seconds();snapshot.revision=++revision;
            }
            previous=snapshot;publish(std::move(snapshot));HANDLE events[]={stop_,changed_.get()};if(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0)break;
        }
    }catch(...){if(WaitForSingleObject(stop_,0)!=WAIT_OBJECT_0){MediaSnapshot s;s.title=L"Media access unavailable";s.artist=L"Windows media sessions could not be reached";publish(std::move(s));}}
    if(session&&mediaSubscribed)session->remove_MediaPropertiesChanged(mediaToken);
    if(session&&playSubscribed)session->remove_PlaybackInfoChanged(playToken);
    if(session&&timeSubscribed)session->remove_TimelinePropertiesChanged(timeToken);
    if(manager&&managerSubscribed)manager->remove_CurrentSessionChanged(managerToken);
    }
    RoUninitialize();
}
}
