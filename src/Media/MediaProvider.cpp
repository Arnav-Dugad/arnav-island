#include "MediaProvider.h"
#include "MediaAbi.h"
#include <roapi.h>
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
}
MediaProvider::MediaProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),changed_(signal()){if(!stop_)throw std::runtime_error("Media stop event allocation failed");worker_=std::thread([this]{run();});}
MediaProvider::~MediaProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);}
void MediaProvider::publish(MediaSnapshot s){{std::lock_guard lock(mutex_);latest_=std::move(s);}PostMessageW(window_,MediaMessage,0,0);}
void MediaProvider::run(){
    if(FAILED(RoInitialize(RO_INIT_MULTITHREADED)))return;
    {
    ComPtr<Manager> manager;ComPtr<Session> session;INT64 managerToken=0,mediaToken=0;bool managerSubscribed=false,mediaSubscribed=false;
    ComPtr<Changed> managerChanged,mediaChanged;
    try{
        HSTRING name=nullptr;const wchar_t* runtime=L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";check(WindowsCreateString(runtime,UINT32(wcslen(runtime)),&name));
        ComPtr<Statics> factory;HRESULT hr=RoGetActivationFactory(name,guid(L"{2050c4ee-11a0-57de-aed7-c97c70338245}"),reinterpret_cast<void**>(factory.GetAddressOf()));WindowsDeleteString(name);check(hr);
        ComPtr<Async> request;check(factory->RequestAsync(&request));manager=awaitResult<Manager>(request.Get(),L"{10f0074e-923d-5510-8f4a-dde37754ca0e}",stop_);
        managerChanged.Attach(new Changed(L"{228bd0ed-1fa2-5e9b-a6ec-42566173103b}",changed_));check(manager->add_CurrentSessionChanged(managerChanged.Get(),&managerToken));managerSubscribed=true;
        mediaChanged.Attach(new Changed(L"{0f2ce2b7-afa7-5ed0-8cb6-8c40cf9b3a5f}",changed_));
        for(;;){
            ComPtr<Session> next;check(manager->GetCurrentSession(&next));
            if(next.Get()!=session.Get()){
                if(session&&mediaSubscribed)session->remove_MediaPropertiesChanged(mediaToken);mediaSubscribed=false;session=next;
                if(session){check(session->add_MediaPropertiesChanged(mediaChanged.Get(),&mediaToken));mediaSubscribed=true;}
            }
            MediaSnapshot snapshot;
            if(session){
                int command=command_.exchange(0);ComPtr<IInspectable> action;
                if(command==1)session->TryTogglePlayPauseAsync(&action);else if(command==2)session->TrySkipPreviousAsync(&action);else if(command==3)session->TrySkipNextAsync(&action);
                ComPtr<Async> props;check(session->TryGetMediaPropertiesAsync(&props));auto properties=awaitResult<Properties>(props.Get(),L"{84593a3d-951a-55b6-8353-5205e577797b}",stop_);
                HSTRING title=nullptr,artist=nullptr;check(properties->get_Title(&title));snapshot.title=consume(title);check(properties->get_Artist(&artist));snapshot.artist=consume(artist);snapshot.available=true;
                if(snapshot.title.empty())snapshot.title=L"Untitled media";
            }
            publish(std::move(snapshot));HANDLE events[]={stop_,changed_.get()};if(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0)break;
        }
    }catch(...){if(WaitForSingleObject(stop_,0)!=WAIT_OBJECT_0)publish({false,L"Media access unavailable",L"This provider could not connect to Windows media sessions"});}
    if(session&&mediaSubscribed)session->remove_MediaPropertiesChanged(mediaToken);
    if(manager&&managerSubscribed)manager->remove_CurrentSessionChanged(managerToken);
    }
    RoUninitialize();
}
}
