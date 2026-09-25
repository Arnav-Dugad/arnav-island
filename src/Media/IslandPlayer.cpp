#include "Media/IslandPlayer.h"
#include <initguid.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <shlwapi.h>
#include <shcore.h>
#include <wincodec.h>
#include <roapi.h>
#include <winstring.h>
// In this toolchain's headers boolean and BYTE are one type, so IReference<boolean> would be defined twice; it is not used.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.media.h>
#include <windows.storage.streams.h>
#include <systemmediatransportcontrolsinterop.h>
#include <cmath>
// Phase 5G: the island's own player. Media Foundation's media engine plays one song at a time from
// the queue; Windows' media controls for the island window carry its title, artist, cover and
// timeline, and bring back the media keys and the flyout's buttons.
namespace nexus {
namespace player {
namespace media=ABI::Windows::Media;
namespace foundation=ABI::Windows::Foundation;
struct HString{HSTRING h=nullptr;explicit HString(const std::wstring& s){WindowsCreateString(s.c_str(),UINT32(s.size()),&h);}~HString(){if(h)WindowsDeleteString(h);}HString(const HString&)=delete;HString& operator=(const HString&)=delete;};
// The engine's events, posted to the window (wParam 1, lParam the event).
struct EngineEvents final:IMFMediaEngineNotify{
    LONG refs=1;HWND window;explicit EngineEvents(HWND w):window(w){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;if(iid==IID_IUnknown||iid==__uuidof(IMFMediaEngineNotify)){*p=static_cast<IMFMediaEngineNotify*>(this);AddRef();return S_OK;}*p=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release()override{const LONG n=InterlockedDecrement(&refs);if(!n)delete this;return ULONG(n);}
    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event,DWORD_PTR,DWORD)override{
        // Time updates come several times a second; the island samples the position itself.
        if(event!=MF_MEDIA_ENGINE_EVENT_TIMEUPDATE&&event!=MF_MEDIA_ENGINE_EVENT_PROGRESS)PostMessageW(window,PlayerMessage,1,LPARAM(event));return S_OK;}
};
// A media key or flyout button (wParam 2, lParam the button).
struct Buttons final:foundation::ITypedEventHandler<media::SystemMediaTransportControls*,media::SystemMediaTransportControlsButtonPressedEventArgs*>{
    LONG refs=1;HWND window;explicit Buttons(HWND w):window(w){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;
        if(iid==IID_IUnknown||iid==IID_IAgileObject||iid==IID___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CSystemMediaTransportControlsButtonPressedEventArgs){*p=static_cast<IUnknown*>(this);AddRef();return S_OK;}*p=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release()override{const LONG n=InterlockedDecrement(&refs);if(!n)delete this;return ULONG(n);}
    HRESULT STDMETHODCALLTYPE Invoke(media::ISystemMediaTransportControls*,media::ISystemMediaTransportControlsButtonPressedEventArgs* args)override{
        media::SystemMediaTransportControlsButton button{};if(args&&SUCCEEDED(args->get_Button(&button)))PostMessageW(window,PlayerMessage,2,LPARAM(button));return S_OK;}
};
// A seek from the flyout (wParam 3, lParam the position in milliseconds).
struct Seeks final:foundation::ITypedEventHandler<media::SystemMediaTransportControls*,media::PlaybackPositionChangeRequestedEventArgs*>{
    LONG refs=1;HWND window;explicit Seeks(HWND w):window(w){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;
        if(iid==IID_IUnknown||iid==IID_IAgileObject||iid==IID___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs){*p=static_cast<IUnknown*>(this);AddRef();return S_OK;}*p=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release()override{const LONG n=InterlockedDecrement(&refs);if(!n)delete this;return ULONG(n);}
    HRESULT STDMETHODCALLTYPE Invoke(media::ISystemMediaTransportControls*,media::IPlaybackPositionChangeRequestedEventArgs* args)override{
        foundation::TimeSpan at{};if(args&&SUCCEEDED(args->get_RequestedPlaybackPosition(&at)))PostMessageW(window,PlayerMessage,3,LPARAM(std::max<INT64>(0,at.Duration/10000)));return S_OK;}
};
// A cover as PNG, for Windows' media controls.
ComPtr<IStream> png(const Artwork& art){
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return nullptr;
    ComPtr<IWICBitmap> bitmap;if(FAILED(factory->CreateBitmapFromMemory(art.width,art.height,GUID_WICPixelFormat32bppPBGRA,art.width*4,UINT(art.pixels.size()),const_cast<BYTE*>(art.pixels.data()),&bitmap)))return nullptr;
    ComPtr<IWICFormatConverter> converter;if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(bitmap.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    ComPtr<IStream> out;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&out)))return nullptr;
    ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||FAILED(encoder->Initialize(out.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&frame,&options))||FAILED(frame->Initialize(options.Get()))||
       FAILED(frame->SetSize(art.width,art.height))||FAILED(frame->SetPixelFormat(&format))||FAILED(frame->WriteSource(converter.Get(),nullptr))||FAILED(frame->Commit())||FAILED(encoder->Commit()))return nullptr;
    LARGE_INTEGER zero{};out->Seek(zero,STREAM_SEEK_SET,nullptr);return out;
}
}
struct IslandPlayer::Impl{
    HWND window;bool started=false;ComPtr<IMFMediaEngine> engine;
    ComPtr<ABI::Windows::Media::ISystemMediaTransportControls> smtc;EventRegistrationToken buttonsToken{},seeksToken{};bool buttonsOn=false,seeksOn=false;
    std::vector<LibraryTrack> queue;size_t at=0;double pendingStart=-1;bool wantPlay=false,loaded=false;std::shared_ptr<const Artwork> art;uint64_t revision=0;
    explicit Impl(HWND w):window(w){
        started=SUCCEEDED(MFStartup(MF_VERSION,MFSTARTUP_LITE));if(!started)return;
        ComPtr<IMFMediaEngineClassFactory> factory;ComPtr<IMFAttributes> attributes;ComPtr<player::EngineEvents> events;events.Attach(new player::EngineEvents(window));
        if(FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(MFCreateAttributes(&attributes,1))||
           FAILED(attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK,events.Get()))||FAILED(factory->CreateInstance(MF_MEDIA_ENGINE_AUDIOONLY,attributes.Get(),&engine))){engine.Reset();return;}
        engine->SetAutoPlay(FALSE);
        // Windows' media controls for this window (optional: without them only the island controls it).
        ComPtr<ISystemMediaTransportControlsInterop> interop;player::HString name(L"Windows.Media.SystemMediaTransportControls");
        if(SUCCEEDED(RoGetActivationFactory(name.h,IID_ISystemMediaTransportControlsInterop,reinterpret_cast<void**>(interop.GetAddressOf())))&&
           SUCCEEDED(interop->GetForWindow(window,IID___x_ABI_CWindows_CMedia_CISystemMediaTransportControls,reinterpret_cast<void**>(smtc.GetAddressOf())))&&smtc){
            ComPtr<player::Buttons> buttons;buttons.Attach(new player::Buttons(window));buttonsOn=SUCCEEDED(smtc->add_ButtonPressed(buttons.Get(),&buttonsToken));
            ComPtr<ABI::Windows::Media::ISystemMediaTransportControls2> smtc2;
            if(SUCCEEDED(smtc.As(&smtc2))){ComPtr<player::Seeks> seeks;seeks.Attach(new player::Seeks(window));seeksOn=SUCCEEDED(smtc2->add_PlaybackPositionChangeRequested(seeks.Get(),&seeksToken));}
            smtc->put_IsEnabled(FALSE);}
    }
    ~Impl(){
        if(smtc){if(buttonsOn)smtc->remove_ButtonPressed(buttonsToken);ComPtr<ABI::Windows::Media::ISystemMediaTransportControls2> smtc2;if(seeksOn&&SUCCEEDED(smtc.As(&smtc2)))smtc2->remove_PlaybackPositionChangeRequested(seeksToken);
            if(auto updater=displayUpdater())updater->ClearAll();smtc->put_IsEnabled(FALSE);smtc.Reset();}
        if(engine){engine->Shutdown();engine.Reset();}if(started)MFShutdown();
    }
    ComPtr<ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater> displayUpdater(){ComPtr<ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater> u;if(smtc)smtc->get_DisplayUpdater(&u);return u;}
    double position()const{if(!engine||!loaded)return std::max(0.,pendingStart);const double t=engine->GetCurrentTime();return std::isfinite(t)?std::max(0.,t):0;}
    double duration()const{double d=engine&&loaded?engine->GetDuration():NAN;if(!std::isfinite(d)||d<=0)d=at<queue.size()?queue[at].duration:0;return d;}
    bool playing()const{return engine&&!queue.empty()&&(loaded?!engine->IsPaused()&&!engine->IsEnded():wantPlay);}
    void load(double start,bool play){
        if(!engine||at>=queue.size())return;loaded=false;pendingStart=start;wantPlay=play;art.reset();++revision;
        // A file URL, so names with # or % play too.
        wchar_t url[2084];DWORD size=2084;if(FAILED(UrlCreateFromPathW(queue[at].path.c_str(),url,&size,0)))return;
        BSTR source=SysAllocString(url);engine->SetSource(source);SysFreeString(source);engine->Load();
        updateDisplay();updateStatus();
    }
    // What Windows shows: the song, its cover, whether it plays, and where it is.
    void updateDisplay(){
        auto updater=displayUpdater();if(!updater)return;
        if(at>=queue.size()){updater->ClearAll();updater->Update();smtc->put_IsEnabled(FALSE);return;}
        smtc->put_IsEnabled(TRUE);smtc->put_IsPlayEnabled(TRUE);smtc->put_IsPauseEnabled(TRUE);smtc->put_IsStopEnabled(TRUE);smtc->put_IsPreviousEnabled(TRUE);smtc->put_IsNextEnabled(at+1<queue.size());
        updater->put_Type(ABI::Windows::Media::MediaPlaybackType_Music);
        ComPtr<ABI::Windows::Media::IMusicDisplayProperties> music;
        if(SUCCEEDED(updater->get_MusicProperties(&music))&&music){const auto& t=queue[at];player::HString title(t.title),artist(t.artist),album(t.album);music->put_Title(title.h);music->put_Artist(artist.h);music->put_AlbumArtist(artist.h);}
        ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStreamReference> thumbnail;
        if(art&&art->width&&art->height)if(auto stream=player::png(*art)){ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> random;ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics> statics;
            player::HString name(L"Windows.Storage.Streams.RandomAccessStreamReference");
            if(SUCCEEDED(CreateRandomAccessStreamOverStream(stream.Get(),BSOS_DEFAULT,IID___x_ABI_CWindows_CStorage_CStreams_CIRandomAccessStream,reinterpret_cast<void**>(random.GetAddressOf())))&&
               SUCCEEDED(RoGetActivationFactory(name.h,IID___x_ABI_CWindows_CStorage_CStreams_CIRandomAccessStreamReferenceStatics,reinterpret_cast<void**>(statics.GetAddressOf()))))statics->CreateFromStream(random.Get(),&thumbnail);}
        updater->put_Thumbnail(thumbnail.Get());updater->Update();
    }
    void updateStatus(){
        if(!smtc||at>=queue.size())return;smtc->put_PlaybackStatus(playing()?ABI::Windows::Media::MediaPlaybackStatus_Playing:ABI::Windows::Media::MediaPlaybackStatus_Paused);
        ComPtr<ABI::Windows::Media::ISystemMediaTransportControls2> smtc2;if(FAILED(smtc.As(&smtc2)))return;
        ComPtr<IInspectable> made;player::HString name(L"Windows.Media.SystemMediaTransportControlsTimelineProperties");if(FAILED(RoActivateInstance(name.h,&made)))return;
        ComPtr<ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties> timeline;if(FAILED(made.As(&timeline)))return;
        auto span=[](double s){ABI::Windows::Foundation::TimeSpan t{};t.Duration=INT64(std::llround(std::max(0.,s)*1e7));return t;};const double d=duration();
        timeline->put_StartTime(span(0));timeline->put_EndTime(span(d));timeline->put_MinSeekTime(span(0));timeline->put_MaxSeekTime(span(d));timeline->put_Position(span(std::min(position(),d>0?d:position())));
        smtc2->UpdateTimelineProperties(timeline.Get());
    }
};
IslandPlayer::IslandPlayer(HWND window):impl_(std::make_unique<Impl>(window)){}
IslandPlayer::~IslandPlayer()=default;
bool IslandPlayer::ready()const{return bool(impl_->engine);}
void IslandPlayer::play(std::vector<LibraryTrack> queue,size_t index,double start,bool playing){if(queue.empty()||!impl_->engine)return;impl_->queue=std::move(queue);impl_->at=std::min(index,impl_->queue.size()-1);impl_->load(start,playing);}
void IslandPlayer::toggle(){if(playing())pause();else resume();}
void IslandPlayer::resume(){auto& i=*impl_;if(!i.engine||i.queue.empty())return;if(!i.loaded){i.wantPlay=true;return;}if(i.engine->IsEnded())i.engine->SetCurrentTime(0);i.engine->Play();++i.revision;i.updateStatus();}
void IslandPlayer::pause(){auto& i=*impl_;if(!i.engine||i.queue.empty())return;if(!i.loaded){i.wantPlay=false;return;}i.engine->Pause();++i.revision;i.updateStatus();}
void IslandPlayer::next(){auto& i=*impl_;if(i.at+1>=i.queue.size())return;++i.at;i.load(0,true);}
// Back to the start of the song, or to the one before when it has barely begun.
void IslandPlayer::previous(){auto& i=*impl_;if(i.queue.empty())return;if(i.position()>3||i.at==0){seek(0);if(!playing())resume();return;}--i.at;i.load(0,true);}
void IslandPlayer::seek(double s){auto& i=*impl_;if(!i.engine||i.queue.empty())return;const double d=i.duration();s=std::clamp(s,0.,d>0?std::max(0.,d-.25):s);if(!i.loaded){i.pendingStart=s;return;}i.engine->SetCurrentTime(s);++i.revision;i.updateStatus();}
void IslandPlayer::stop(){auto& i=*impl_;if(i.engine&&!i.queue.empty()){i.engine->Pause();BSTR none=SysAllocString(L"");i.engine->SetSource(none);SysFreeString(none);}i.queue.clear();i.at=0;i.loaded=false;i.art.reset();++i.revision;i.updateDisplay();}
bool IslandPlayer::active()const{return !impl_->queue.empty();}
void IslandPlayer::mute(bool muted){if(impl_->engine)impl_->engine->SetMuted(muted?TRUE:FALSE);}
bool IslandPlayer::playing()const{return impl_->playing();}
const LibraryTrack* IslandPlayer::track()const{return impl_->at<impl_->queue.size()?&impl_->queue[impl_->at]:nullptr;}
const LibraryTrack* IslandPlayer::upcoming()const{return impl_->at+1<impl_->queue.size()?&impl_->queue[impl_->at+1]:nullptr;}
size_t IslandPlayer::index()const{return impl_->at;}
size_t IslandPlayer::size()const{return impl_->queue.size();}
double IslandPlayer::position()const{return impl_->position();}
double IslandPlayer::duration()const{return impl_->duration();}
void IslandPlayer::artwork(std::shared_ptr<const Artwork> art){if(art==impl_->art)return;impl_->art=std::move(art);++impl_->revision;impl_->updateDisplay();}
std::shared_ptr<const Artwork> IslandPlayer::artwork()const{return impl_->art;}
MediaSnapshot IslandPlayer::snapshot(double now)const{
    MediaSnapshot m;const auto* t=track();if(!t)return m;const auto& i=*impl_;
    m.available=true;m.playing=playing();m.canToggle=true;m.canPrevious=true;m.canNext=i.at+1<i.queue.size();m.duration=duration();m.canSeek=m.duration>0;
    m.title=t->title;m.artist=!t->artist.empty()?t->artist:!t->album.empty()?t->album:L"Unknown artist";m.source=islandSource;m.appName=L"Your music";m.kind=MediaKind::Music;m.artwork=i.art;
    m.position=std::min(position(),m.duration>0?m.duration:position());m.sampledAt=now;m.seekMin=0;m.seekMax=m.duration;m.id=islandSessionId;m.current=m.playing;m.revision=i.revision;
    return m;
}
bool IslandPlayer::handle(WPARAM kind,LPARAM value){
    auto& i=*impl_;if(!i.engine)return false;
    if(kind==2){using B=ABI::Windows::Media::SystemMediaTransportControlsButton;switch(B(value)){
        case ABI::Windows::Media::SystemMediaTransportControlsButton_Play:resume();break;case ABI::Windows::Media::SystemMediaTransportControlsButton_Pause:case ABI::Windows::Media::SystemMediaTransportControlsButton_Stop:pause();break;
        case ABI::Windows::Media::SystemMediaTransportControlsButton_Next:next();break;case ABI::Windows::Media::SystemMediaTransportControlsButton_Previous:previous();break;default:return false;}return true;}
    if(kind==3){seek(double(value)/1000);return true;}
    if(kind!=1)return false;
    switch(DWORD(value)){
    case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:i.loaded=true;if(i.pendingStart>0)i.engine->SetCurrentTime(i.pendingStart);i.pendingStart=-1;if(i.wantPlay)i.engine->Play();i.updateStatus();break;
    case MF_MEDIA_ENGINE_EVENT_PLAYING:case MF_MEDIA_ENGINE_EVENT_PAUSE:case MF_MEDIA_ENGINE_EVENT_SEEKED:case MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE:i.updateStatus();break;
    // The queue plays on; at its end the last song rests, paused, at its start.
    case MF_MEDIA_ENGINE_EVENT_ENDED:if(i.at+1<i.queue.size()){++i.at;i.load(0,true);}else{i.engine->Pause();i.engine->SetCurrentTime(0);i.updateStatus();}break;
    // A song that can't be played is passed over.
    case MF_MEDIA_ENGINE_EVENT_ERROR:if(i.at+1<i.queue.size()){++i.at;i.load(0,true);}else{stop();}break;
    default:return false;}
    ++i.revision;return true;
}
}
