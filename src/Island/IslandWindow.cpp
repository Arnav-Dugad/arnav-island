#include "Common/Capture.h"
#include "IslandWindow.h"
#include "Design/Wallpaper.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <psapi.h>
#include <shellscalingapi.h>
#include <sstream>
#include <iomanip>
#include <array>
#include <fstream>

namespace nexus {
static IslandWindow* foregroundOwner=nullptr;
static constexpr UINT TrayMessage=WM_APP+1,FullscreenMessage=WM_APP+2;
static constexpr UINT_PTR SettleTimer=1,ActivityTimer=2,HudTimer=3,ScenarioTimer=4;
static HFONT uiFont(int height,int weight=FW_NORMAL){return CreateFontW(height,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");}
static std::string monitorIdentity(HMONITOR monitor){
    MONITORINFOEXW info{};info.cbSize=sizeof(info);if(!GetMonitorInfoW(monitor,&info))return {};
    UINT32 pathCount=0,modeCount=0;std::wstring id;
    for(int attempt=0;attempt<3;++attempt){if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathCount,&modeCount)!=ERROR_SUCCESS)break;std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);auto result=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathCount,paths.data(),&modeCount,modes.data(),nullptr);if(result==ERROR_INSUFFICIENT_BUFFER)continue;if(result!=ERROR_SUCCESS)break;for(UINT32 i=0;i<pathCount;++i){auto& p=paths[i];DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),p.sourceInfo.adapterId,p.sourceInfo.id};if(DisplayConfigGetDeviceInfo(&source.header)!=ERROR_SUCCESS||wcscmp(source.viewGdiDeviceName,info.szDevice))continue;DISPLAYCONFIG_TARGET_DEVICE_NAME target{};target.header={DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME,sizeof(target),p.targetInfo.adapterId,p.targetInfo.id};if(DisplayConfigGetDeviceInfo(&target.header)==ERROR_SUCCESS)id=target.monitorDevicePath;break;}break;}
    if(id.empty())return {};int length=WideCharToMultiByte(CP_UTF8,0,id.data(),int(id.size()),nullptr,0,nullptr,nullptr);std::string result(length,'\0');WideCharToMultiByte(CP_UTF8,0,id.data(),int(id.size()),result.data(),length,nullptr,nullptr);return result;
}
static BOOL CALLBACK collectMonitor(HMONITOR m,HDC,LPRECT,LPARAM p){reinterpret_cast<std::vector<HMONITOR>*>(p)->push_back(m);return TRUE;}

int IslandWindow::run(HINSTANCE instance,const std::wstring& cmd){
    instance_=instance;visibilityAudit_=cmd.find(L"--visibility-audit")!=std::wstring::npos;motionStudy_=cmd.find(L"--motion-study")!=std::wstring::npos;settingsTest_=cmd.find(L"--settings-test")!=std::wstring::npos;testing_=settingsTest_||cmd.find(L"--capture")!=std::wstring::npos||cmd.find(L"--benchmark")!=std::wstring::npos||cmd.find(L"--ui-test")!=std::wstring::npos;if(testing_)settingsFile_=L"settings-qa.nexus";soundsMuted()=testing_;settings_=settingsTest_?Settings{}:store_.load();if(!testing_){std::ifstream profiles(store_.directory/L"displays.nexus");if(profiles)displays_=DisplayProfiles::read(profiles);}if(cmd.find(L"--ui-test")!=std::wstring::npos){settings_=Settings{};settings_.uiMode=2;}settings_.startAtLogin=startup::enabled();if(cmd.find(L"--enable-startup")!=std::wstring::npos&&!testing_){settings_.startAtLogin=startup::apply(true);store_.save(settings_,settingsFile_);}motion_.body=preset(MotionPreset(settings_.preset));
    BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);motion_.reduced=settings_.reduceMotion||!animations;
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=L"ArnavIsland.Surface";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));check(RegisterClassW(&wc)?S_OK:HRESULT_FROM_WIN32(GetLastError()));
    window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP|WS_EX_NOACTIVATE,wc.lpszClassName,L"Arnav Island",WS_POPUP,0,0,760,480,nullptr,nullptr,instance,this);
    if(!window_)throw std::runtime_error("Island window creation failed");
    // The shadow window sits just under the island and never takes input (layered and transparent to the mouse).
    {WNDCLASSW sc{};sc.hInstance=instance;sc.lpfnWndProc=DefWindowProcW;sc.lpszClassName=L"ArnavIsland.Shadow";RegisterClassW(&sc);
        shadow_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED,sc.lpszClassName,L"",WS_POPUP,0,0,760,480,nullptr,nullptr,instance,nullptr);}
    position();renderer_=std::make_unique<Renderer>();renderer_->initialize(window_,dpi_,shadow_);clipboard_.attach(window_);power(false);applySettings();if(!testing_){displays_.remember(currentDisplay_,settings_);saveDisplays();}
    audio_=std::make_unique<AudioProvider>(window_);if(cmd.find(L"--capture-safe")==std::wstring::npos)media_=std::make_unique<MediaProvider>(window_);system_=std::make_unique<SystemProvider>(window_);if(cmd.find(L"--capture-safe")==std::wstring::npos){analyzer_=std::make_unique<LoopbackAnalyzer>(window_);mixer_=std::make_unique<SessionMixer>(window_);brightness_=std::make_unique<BrightnessProvider>(window_);if(!(testing_&&cmd.find(L"--qa-sample")!=std::wstring::npos))bluetooth_=std::make_unique<BluetoothProvider>(window_);powerMode_=std::make_unique<PowerModeWatcher>(window_);}battery_=std::make_unique<BatteryProvider>(window_,testing_?std::filesystem::path{}:store_.directory/L"battery-history.nexus",settings_.batteryHistory);if(!testing_){std::ifstream saved(store_.directory/L"workspaces.nexus",std::ios::binary);if(saved)workspaces_=WorkspaceStore::read(saved);}if(cmd.find(L"--capture-safe")==std::wstring::npos)syncProductivity();platformThread_=std::thread([w=window_]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);auto info=new PlatformInfo(platformInfo());info->wallpaper=wallpaperAccent();if(!PostMessageW(w,PlatformMessage,0,reinterpret_cast<LPARAM>(info)))delete info;CoUninitialize();});content_.settings=settings_;
    for(auto id:{&GUID_ACDC_POWER_SOURCE,&GUID_BATTERY_PERCENTAGE_REMAINING}){
        auto registration=RegisterPowerSettingNotification(window_,id,DEVICE_NOTIFY_WINDOW_HANDLE);if(registration)powerNotifications_.push_back(registration);
    }
    tray_.cbSize=sizeof(tray_);tray_.hWnd=window_;tray_.uID=1;tray_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray_.uCallbackMessage=TrayMessage;tray_.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));wcscpy_s(tray_.szTip,L"Arnav Island — right-click for controls");Shell_NotifyIconW(NIM_ADD,&tray_);
    previews_=std::make_unique<ShelfPreviews>(window_);if(!content_.shelf.empty())requestPreviews();/* a pinned Shelf loaded above */dropTarget_.Attach(new ShelfDropTarget([this](bool hover){content_.dropHover=hover;if(hover){KillTimer(window_,8);content_.page=Page::Shelf;transition(IslandState::Expanded);}refresh();},[this](std::vector<ShelfItem> items){size_t first=content_.shelf.size();auto preview=items.empty()?nullptr:previews_->get(items.front().value);for(auto& item:items){if(content_.shelf.size()>=32)break;bool exists=std::any_of(content_.shelf.begin(),content_.shelf.end(),[&](auto& existing){return existing.kind==item.kind&&existing.value==item.value;});if(!exists)content_.shelf.push_back(std::move(item));}content_.pinned=false;motion_.pulse.reset(.45,seconds());motion_.pulse.retarget(0,seconds(),{1,95,22});requestPreviews();refresh();animate();if(content_.shelf.size()>first){POINT p{};GetCursorPos(&p);ScreenToClient(window_,&p);auto origin=bodyAt(motion_.width.sample(seconds()).position,motion_.height.sample(seconds()).position,motion_.drop.sample(seconds()).position);content_.shelfOffset=int(first>3?first-3:0);renderer_->absorb(preview,float(p.x*96/dpi_-origin.x),float(p.y*96/dpi_-origin.y),42,94+float(first-content_.shelfOffset)*36,motion_.reduced);refresh();}}));dropTarget_->attach(window_,[this](const std::vector<ShelfItem>& items){requestPreviews(items);});
    // Phase 5G: with PCs paired, a drag lights the zone under it and a drop on a PC sends there.
    dropTarget_->route([this](POINT at){const Action zone=dropZoneAt(at);if(zone!=content_.dropZone){content_.dropZone=zone;refresh();}},[this](const std::vector<ShelfItem>& items,POINT at){return dropOnPeer(items,at);});
    check(RegisterDragDrop(window_,dropTarget_.Get()));
    foregroundOwner=this;foregroundHook_=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    ShowWindow(window_,SW_SHOWNOACTIVATE);syncShadow();animate();
    locationHook_=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);if(!testing_)fullscreen();
    if(cmd.find(L"--expanded")!=std::wstring::npos){content_.pinned=true;transition(IslandState::Expanded);refresh();}
    for(auto pair:{std::pair{L"--media",Page::Media},std::pair{L"--system",Page::System},std::pair{L"--focus",Page::Focus},std::pair{L"--shelf",Page::Shelf},std::pair{L"--audio",Page::Audio}})if(cmd.find(pair.first)!=std::wstring::npos){content_.page=pair.second;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(cmd.find(L"--controls")!=std::wstring::npos){content_.pinned=true;perform(Action::Control);}
    // QA: the island held mid-drag, leaning (the springs sit at the pulled position).
    if(testing_&&cmd.find(L"--qa-lean")!=std::wstring::npos){motion_.dragX.reset(80,seconds());motion_.dragY.reset(30,seconds());animate();}
    // QA: the weekly battery card with illustrative numbers.
    if(testing_&&cmd.find(L"--qa-weekly")!=std::wstring::npos){content_.week={6,17.6,21.5,7};content_.healthNow=.843;content_.healthBefore=.846;showNotice(13);}
    // QA: illustrative weather (no network): --qa-weather=<WMO code>.
    if(auto at=cmd.find(L"--qa-weather");testing_&&at!=std::wstring::npos){int code=3;auto eq=cmd.find(L'=',at);if(eq!=std::wstring::npos&&eq<cmd.find(L' ',at))code=_wtoi(cmd.c_str()+eq+1);settings_.weather=true;settings_.homeMetrics={0,7,8};content_.weather={true,14.4,code,cmd.find(L"--qa-night")==std::wstring::npos,L"Sample town"};refresh();}
    if(cmd.find(L"--video-layout")!=std::wstring::npos){settings_.mediaLayout=2;refresh();}
    if(cmd.find(L"--music-layout")!=std::wstring::npos){settings_.mediaLayout=1;refresh();}
        if(cmd.find(L"--scale110")!=std::wstring::npos&&testing_){settings_.scale=110;applySettings(true);}
    if(testing_&&cmd.find(L"--qa-dark")!=std::wstring::npos){settings_.theme=0;applySettings();}
    if(cmd.find(L"--light")!=std::wstring::npos){settings_.theme=1;applySettings();}
    if(cmd.find(L"--right")!=std::wstring::npos){settings_.edge=1;applySettings(true);}
    if(testing_&&cmd.find(L"--qa-pattern")!=std::wstring::npos)showcasePattern_=true;
    if(testing_&&cmd.find(L"--qa-showcase")!=std::wstring::npos){
        auto art=std::make_shared<Artwork>();art->width=art->height=256;art->pixels.resize(256*256*4);for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){size_t i=(y*256+x)*4;art->pixels[i]=BYTE(150+x/3);art->pixels[i+1]=BYTE(70+y/2);art->pixels[i+2]=BYTE(230-x/3);art->pixels[i+3]=255;}paintArtwork(*art);
        auto player=resolveApp(L"Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"),browser=resolveApp(L"MSEdge"),calculator=resolveApp(L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App");
        MediaSnapshot first;first.available=first.playing=first.canToggle=first.canNext=first.canPrevious=first.canSeek=true;first.title=L"Artwork study";first.artist=L"Original local QA artwork";first.appName=player.name.empty()?L"Media Player":player.name;first.appIcon=player.icon;first.artwork=art;first.duration=first.seekMax=214;first.position=83;first.sampledAt=seconds();first.current=true;first.kind=MediaKind::Music;first.source=L"qa.player";
        MediaSnapshot second=first;second.title=L"Browser playback study";second.artist=L"Synthetic QA session";second.service="youtube";second.browser=true;second.appName=L"Microsoft Edge";second.appIcon=browser.icon;second.playing=false;second.current=false;second.source=L"qa.browser";
        first.id=1;second.id=2;content_.sessions={first,second};content_.session=0;content_.playback=first;content_.media=first.title;selectedSource_=first.source;selectedId_=first.id;
        content_.mixer={MixerEntry{1,first.appName,player.icon,.72f,.0f,false,false,true},MixerEntry{2,L"Microsoft Edge",browser.icon,.45f,0,false,false,true},MixerEntry{3,calculator.name.empty()?L"Calculator":calculator.name,calculator.icon,1,0,true,false,false},MixerEntry{0,L"System sounds",nullptr,.8f,0,false,true,false}};
        media_.reset();mixer_.reset();analyzer_.reset();content_.waveform=true;SetTimer(window_,22,10,nullptr);refresh();animate();}
    if(cmd.find(L"--qa-art")!=std::wstring::npos&&testing_){auto art=std::make_shared<Artwork>();art->width=256;art->height=256;art->pixels.resize(256*256*4);for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){size_t i=(y*256+x)*4;art->pixels[i]=BYTE(110+x/2);art->pixels[i+1]=BYTE(55+y/2);art->pixels[i+2]=BYTE(180-x/3);art->pixels[i+3]=255;}paintArtwork(*art);content_.playback.artwork=art;content_.playback.title=L"Artwork study";content_.playback.artist=L"Original local QA artwork";content_.playback.available=true;refresh();animate();}
    for(auto pair:{std::pair{L"--layout",6},std::pair{L"--details",4},std::pair{L"--multitasking",0},std::pair{L"--qa-modes",1},std::pair{L"--qa-appearance",2}})if(testing_&&cmd.find(pair.first)!=std::wstring::npos)openSettings(pair.second); if(cmd.find(L"--qa-timer")!=std::wstring::npos&&testing_){content_.focus.held=450;content_.focus.toggle(seconds());clockTimer();refresh();}
    if(testing_&&cmd.find(L"--qa-seek")!=std::wstring::npos){content_.page=Page::Media;content_.playback.available=true;content_.playback.canSeek=true;content_.playback.duration=content_.playback.seekMax=240;content_.playback.position=75;content_.playback.sampledAt=seconds();content_.scrub.begin(220,380,0,240);content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(testing_&&cmd.find(L"--qa-previews")!=std::wstring::npos){content_.page=Page::Shelf;auto path=(std::filesystem::current_path()/L"docs/evidence/v0.4/artwork.png").wstring();content_.shelf.push_back({ShelfItem::Kind::File,path,L"Original QA artwork.png"});content_.shelf.push_back({ShelfItem::Kind::File,(std::filesystem::current_path()/L"docs/QUICK_START.md").wstring(),L"Quick start.md"});requestPreviews();content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(cmd.find(L"--qa-handoff")!=std::wstring::npos&&testing_)SetTimer(window_,15,180,nullptr);
    if(cmd.find(L"--lab")!=std::wstring::npos)openSettings(3);
    if(cmd.find(L"--benchmark")!=std::wstring::npos){benchmark_=true;benchmarkStart_=seconds();FILETIME c,e;GetProcessTimes(GetCurrentProcess(),&c,&e,&initialKernel_,&initialUser_);SetTimer(window_,ScenarioTimer,73,nullptr);}
    if(cmd.find(L"--ui-test")!=std::wstring::npos){settings_.hoverOpen=true;settings_.hoverDelay=100;SetTimer(window_,13,200,nullptr);}
    if(testing_&&cmd.find(L"--qa-live")!=std::wstring::npos){settings_.uiMode=1;content_.pinned=true;transition(IslandState::LiveActivity);refresh();}
    if(testing_&&cmd.find(L"--qa-mini")!=std::wstring::npos){settings_.uiMode=0;applySettings();transition(IslandState::Compact);}
    if(testing_&&cmd.find(L"--qa-wide")!=std::wstring::npos){settings_.uiMode=1;settings_.compactWidth=560;settings_.compactClock=true;applySettings();transition(IslandState::Compact);}
    if(testing_&&cmd.find(L"--qa-peek")!=std::wstring::npos){content_.hovered=Action::ShelfItemBase;refresh();}
    if(cmd.find(L"--settings")!=std::wstring::npos&&!settingsTest_){auto at=cmd.find(L"--settings-section=");openSettings(at==std::wstring::npos?-1:_wtoi(cmd.c_str()+at+19));}
    if(testing_&&cmd.find(L"--qa-glass")!=std::wstring::npos){settings_.material=cmd.find(L"--qa-clear")!=std::wstring::npos?2:1;applySettings(false,true);}
    // --qa-adaptive: Clear glass with adaptive text over an illustrative backdrop (bright left half, dark right half).
    if(testing_&&cmd.find(L"--qa-adaptive")!=std::wstring::npos){qaBackdrop_=true;settings_.material=2;settings_.adaptiveText=true;applySettings(false,true);}
    // --qa-nearby: the Shelf's Nearby tab with illustrative PCs (no network); --qa-share-card=<14|15|16>: a sharing card.
    if(testing_&&cmd.find(L"--qa-nearby")!=std::wstring::npos){settings_.sharing=true;content_.settings=settings_;content_.shareName=L"Desk PC";content_.nearby={{"a1",L"Studio PC",true,true},{"b2",L"Travel laptop",false,true},{"c3",L"Living room PC",true,false}};content_.nearbyTarget=shareTarget_="a1";
        content_.page=Page::Shelf;content_.shelfTab=2;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(auto at=cmd.find(L"--qa-share-card=");testing_&&at!=std::wstring::npos){const int kind=_wtoi(cmd.c_str()+at+16);
        if(kind==17)shareCard(17,L"Blue in Green",L"Miles Davis  ·  from Studio PC",{},60);else if(kind==14)shareCard(14,L"Studio PC",L"482 913",{},60);else if(kind==15)shareCard(15,L"Studio PC",L"Holiday photos.zip  \u00b7  48.2 MB",{},60);else shareCard(16,L"Received from Studio PC",L"Holiday photos.zip",L"C:\\Users\\Public\\Downloads\\Holiday photos.zip",60);}
    if(testing_&&cmd.find(L"--qa-solid")!=std::wstring::npos){settings_.material=0;applySettings(false,true);}
    // --qa-library=<folder>: the library reads only that folder (a test run never reads the Music folder).
    if(auto at=cmd.find(L"--qa-library=");testing_&&at!=std::wstring::npos){std::wstring folder=cmd.substr(at+13);if(!folder.empty()&&folder[0]==L'"'){folder.erase(0,1);folder=folder.substr(0,folder.find(L'"'));}else folder=folder.substr(0,folder.find(L' '));qaLibrary_=folder;syncLibrary();
        // --qa-play: the first song plays (muted) on the Media page; --qa-library-view: the list shows.
        qaPlay_=cmd.find(L"--qa-play")!=std::wstring::npos;qaLibraryView_=cmd.find(L"--qa-library-view")!=std::wstring::npos;if(library_&&(qaPlay_||qaLibraryView_))library_->scan();}
    if(testing_&&cmd.find(L"--qa-edge")!=std::wstring::npos){settings_.edge=1;applySettings(false,true);}
    if(testing_&&cmd.find(L"--qa-left")!=std::wstring::npos){settings_.edge=2;applySettings(false,true);}
    if(testing_&&cmd.find(L"--qa-currency")!=std::wstring::npos)settings_.currency=true;
    if(testing_&&cmd.find(L"--qa-answer-cycle")!=std::wstring::npos)SetTimer(window_,44,1500,nullptr);
    if(settingsTest_){settingsTestPhase_=0;SetTimer(window_,21,400,nullptr);}
    // Synthetic 100 Hz band levels through the real bar-animation path; measures rendering cost without playing audio.
    // --qa-dj (with --qa-art --qa-spectrum): a track in its last seconds, the next one's colour known, so the ring's halo breathes.
    if(testing_&&cmd.find(L"--qa-dj")!=std::wstring::npos){settings_.uiMode=1;settings_.compactWidth=420;settings_.waveformStyle=1;settings_.waveform=true;settings_.islandDj=true;content_.settings=settings_;applySettings(false,true);
        auto& p=content_.playback;p.duration=214;p.position=208;p.sampledAt=seconds();p.canSeek=p.canToggle=p.canNext=p.canPrevious=true;content_.djAccent=0xff6fa3;}
    if(testing_&&cmd.find(L"--qa-spectrum")!=std::wstring::npos){content_.playback.available=content_.playback.playing=true;content_.playback.title=L"Spectrum load study";content_.waveform=true;analyzer_.reset();refresh();animate();SetTimer(window_,22,10,nullptr);}
    // Real device and battery data through the real card and tab paths.
    // --qa-sample swaps in illustrative devices so public screenshots never show real device names.
    const bool sample=cmd.find(L"--qa-sample")!=std::wstring::npos;
    auto qaDevices=[sample]{if(!sample)return BluetoothProvider::enumerate();auto make=[](const wchar_t* name,bool connected,int battery,DeviceKind kind,const char* brand){BluetoothDevice d;d.name=name;d.connected=connected;d.audio=kind==DeviceKind::Headphones||kind==DeviceKind::Earbuds||kind==DeviceKind::Speaker;d.battery=battery;d.kind=kind;d.brand=brand;return d;};
        return std::vector<BluetoothDevice>{make(L"Galaxy Buds3 Pro",true,82,DeviceKind::Earbuds,"samsung"),make(L"WH-1000XM5",true,64,DeviceKind::Headphones,"sony"),make(L"DualSense Wireless Controller",false,41,DeviceKind::Gamepad,"playstation"),make(L"JBL Flip 6",false,-1,DeviceKind::Speaker,"jbl")};};
    // QA: the pointer light placed over the island, for captures.
    if(testing_&&cmd.find(L"--qa-sheen")!=std::wstring::npos){refresh();renderer_->pointer(250,70,true,true);}
    // Phase 4 QA states, with illustrative content only.
    // QA: the synthetic showcase track "heard" up to its playhead (deterministic envelope), for waveform captures.
    if(testing_&&cmd.find(L"--qa-wave")!=std::wstring::npos&&content_.playback.duration>0){auto& p=content_.playback;auto& w=waves_.track(WaveformLibrary::key(p.title,p.artist,p.duration));
        for(double t=0;t<p.position;t+=.05){double beat=.42+.3*std::sin(t*.8)*std::sin(t*.8)+.18*std::sin(t*3.1)+.1*std::sin(t*11.0);w.hear(t,p.duration,float(std::clamp(beat*(t<6?t/6:1),0.,1.)));}pushWaveform();}
    // Phase 5B QA: placeholder lyric lines (written for testing, not a real song), the seek
    // bubble, a headphone switch, the app-volume indicator and a muted microphone.
    if(testing_&&cmd.find(L"--qa-lyrics")!=std::wstring::npos){settings_.lyrics=true;content_.settings=settings_;
        content_.lyrics=std::make_shared<std::vector<LyricLine>>(parseLrc(L"[01:10.00]Light moves along the glass\n[01:16.50]Every colour finds its place\n[01:22.00]Hold the moment, let it settle into place before it goes\n[01:27.50]Soft and slow, a steady pace\n[01:33.00]\n[01:38.00]Sample lines for testing only"));
        content_.lyricsState=int(LyricsService::State::Found);tickLyrics(false);refresh();animate();}
    if(testing_&&cmd.find(L"--qa-bubble")!=std::wstring::npos){content_.seekHover=250;refresh();}
    if(testing_&&cmd.find(L"--qa-headphones")!=std::wstring::npos){AudioDevice speakers{L"qa-speakers",L"Speakers (Realtek(R) Audio)",false,FormSpeakers},phones{L"qa-phones",L"Headphones (WH-1000XM5)",true,FormHeadphones};content_.outputs={speakers,phones};settings_.directAudio=true;if(cmd.find(L"--qa-sample")!=std::wstring::npos)content_.devices=qaDevices();showHeadphoneCard(phones,speakers.id,speakers.name);}
    if(testing_&&cmd.find(L"--qa-appvolume")!=std::wstring::npos){content_.appVolume=45;content_.appVolumeName=content_.playback.appName;events_.publish({ActivityKind::AppVolume,"app-volume",40,45,.5,30},seconds());presentActivity();}
    if(testing_&&cmd.find(L"--qa-mic")!=std::wstring::npos){qaMicMuted_=true;content_.micAvailable=true;content_.micMuted=true;refresh();}
    if(testing_&&cmd.find(L"--qa-clipboard")!=std::wstring::npos){settings_.clipboardHistory=true;content_.settings=settings_;double now=seconds();auto notepad=shellIcon(L"C:\\Windows\\System32\\notepad.exe",32);
        auto add=[&](ClipEntry e,double age){e.sourceIcon=notepad;clips_.add(std::move(e),now-age);};
        {ClipEntry e;e.kind=ClipEntry::Kind::Files;e.files={L"C:\\Users\\Public\\Documents\\Quarterly review.pdf",L"C:\\Users\\Public\\Documents\\Budget.xlsx"};e.source=L"File Explorer";add(std::move(e),5400);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Image;e.thumbnail=content_.sessions.empty()?nullptr:content_.sessions.front().artwork;e.imageWidth=1920;e.imageHeight=1080;e.source=L"Snipping Tool";if(!e.thumbnail){auto art=std::make_shared<Artwork>();art->width=art->height=64;art->pixels.resize(64*64*4);for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto* p=&art->pixels[size_t(y*64+x)*4];p[0]=uint8_t(200-x);p[1]=uint8_t(120+y);p[2]=uint8_t(230-y);p[3]=255;}e.thumbnail=art;}add(std::move(e),1800);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Link;e.text=L"https://github.com/Arnav-Dugad/arnav-island/releases";e.source=L"Microsoft Edge";add(std::move(e),240);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Text;e.text=L"Agenda for Thursday: launch review, owners and follow-ups";e.source=L"Notepad";add(std::move(e),20);}
        // 5F: a colour code and a snippet of code, for the rich rows.
        {ClipEntry e;e.kind=ClipEntry::Kind::Text;e.text=L"#3A7BD5";e.source=L"Notepad";add(std::move(e),12);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Text;e.text=L"const island = new Island({ glass: true, radius: 22 });\n    island.open(\"media\");";e.source=L"Notepad";add(std::move(e),6);}
        // 5C: a pinned copy, a password-like one (masked), and the picker.
        if(cmd.find(L"--qa-pins")!=std::wstring::npos){ClipEntry secret;secret.text=L"Tr0ub4dor&3xQ";secret.source=L"Notepad";add(std::move(secret),60);clips_.togglePin(clips_.entries().back().id);}
        clipViews();content_.page=Page::Shelf;content_.shelfTab=1;content_.pinned=true;transition(IslandState::Expanded);refresh();
        if(cmd.find(L"--qa-picker")!=std::wstring::npos){content_.pinned=false;clipSearch(true);}}
    // 5C QA: sample Shelf files (the project's own evidence images), result cards and the capture overlay over a painted backdrop.
    if(testing_&&cmd.find(L"--qa-shelf")!=std::wstring::npos){const auto dir=std::filesystem::current_path()/L"docs"/L"evidence";wchar_t pub[MAX_PATH]{};GetEnvironmentVariableW(L"PUBLIC",pub,MAX_PATH);
        const auto samples=std::filesystem::path(pub)/L"Documents"/L"Arnav Island samples";std::error_code ec;std::filesystem::create_directories(samples,ec);
        for(auto [rel,name]:{std::pair{L"v0.11\\lyrics-dark.png",L"Album notes.png"},std::pair{L"v0.10\\waveform-light.png",L"Waveform study.png"},std::pair{L"v0.11\\headphone-card.png",L"Headphone card.png"}}){auto p=(samples/name).wstring();std::filesystem::copy_file(dir/rel,p,std::filesystem::copy_options::overwrite_existing,ec);if(std::filesystem::exists(p))content_.shelf.push_back({ShelfItem::Kind::File,p,name});}
        content_.shelf.push_back({ShelfItem::Kind::Text,L"Meeting notes: launch review on Thursday",L"Meeting notes: launch review on Thursday"});requestPreviews();
        content_.page=Page::Shelf;content_.shelfTab=0;content_.pinned=true;transition(IslandState::Expanded);if(cmd.find(L"--qa-shelf-detail")!=std::wstring::npos)openShelfItem(0);refresh();}
    // QA (Phase 5G): a transfer on its way to the first illustrative PC, the Nearby tab, and a drag held over the island's first PC.
    if(testing_&&cmd.find(L"--qa-transfer")!=std::wstring::npos)content_.transfers.push_back({7,"a1",L"Holiday photos",L"Studio PC",27ull<<20,64ull<<20,12,true});
    if(testing_&&cmd.find(L"--qa-nearby-tab")!=std::wstring::npos){content_.page=Page::Shelf;content_.shelfTab=2;content_.shelfDetail=-1;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    // --qa-handoff-picker (with --qa-nearby --qa-art): the Media page offering two paired PCs to continue on.
    if(testing_&&cmd.find(L"--qa-handoff-picker")!=std::wstring::npos){settings_.handoff=true;content_.settings=settings_;content_.nearby={{"a1",L"Studio PC",true,true},{"c3",L"Living room PC",true,true}};
        auto& p=content_.playback;p.available=p.playing=p.canToggle=p.canNext=p.canPrevious=true;p.duration=337;p.position=96;p.sampledAt=seconds();p.canSeek=true;content_.page=Page::Media;content_.handoffPicking=true;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    // --qa-swipe (with --qa-art): a sideways drag held past the point where letting go would skip, on the compact island.
    if(testing_&&cmd.find(L"--qa-swipe")!=std::wstring::npos){auto& p=content_.playback;p.available=p.playing=p.canToggle=p.canNext=p.canPrevious=true;settings_.swipeSkip=true;refresh();SetTimer(window_,68,1500,nullptr);}
    // --qa-two-cards: an offer, then (0.4 s later) a second alert, which waits below the first as a bud.
    if(testing_&&cmd.find(L"--qa-two-cards")!=std::wstring::npos){settings_.notifyStyle=1;settings_.stackAlerts=true;shareCard(15,L"Studio PC",L"Holiday photos  ·  12 files  ·  48.2 MB",{},60);SetTimer(window_,67,400,nullptr);}
    // --qa-nav-move=<ms>: Home opens, then the Shelf is chosen after <ms> (to catch the liquid pill on its way).
    if(auto at=cmd.find(L"--qa-nav-move=");testing_&&at!=std::wstring::npos){content_.page=Page::Overview;content_.pinned=true;transition(IslandState::Expanded);refresh();SetTimer(window_,66,UINT(std::clamp(_wtoi(cmd.c_str()+at+14),100,20000)),nullptr);}
    if(testing_&&cmd.find(L"--qa-drop")!=std::wstring::npos){content_.page=Page::Shelf;content_.shelfDetail=-1;content_.dropHover=true;content_.dropZone=Action::NearbyBase;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(auto at=cmd.find(L"--qa-card=");testing_&&at!=std::wstring::npos){const auto kind=cmd.substr(at+10,4);
        if(kind==L"colo")captureCard(9,L"#3A7BD5",L"Copied  \u00b7  rgb(58, 123, 213)",{},0x3a7bd5);
        else if(kind==L"text")captureCard(10,L"Copied 12 words",L"Quarterly review: launch on Thursday");
        else if(kind==L"snip")captureCard(11,L"Snip saved to the Shelf",L"1280 \u00d7 720  \u00b7  also copied",content_.playback.artwork);}
    if(auto at=cmd.find(L"--qa-overlay=");testing_&&at!=std::wstring::npos){const auto mode=cmd.substr(at+13,4);qaBackdrop();
        qaOverlay_=mode==L"text"?int(CaptureMode::Text):mode==L"colo"?int(CaptureMode::Colour):int(CaptureMode::Snip);SetTimer(window_,42,500,nullptr);}
    if(testing_&&cmd.find(L"--qa-privacy")!=std::wstring::npos){PrivacyUse camera{Capability::Camera,L"Microsoft Teams",L"MSTeams_8wekyb3d8bbwe",true,1,installedAppIcon(L"Microsoft Teams")};PrivacyUse mic{Capability::Microphone,L"Microsoft Teams",L"MSTeams_8wekyb3d8bbwe",true,1,camera.icon};
        // Screen capture by an illustrative app (no real app name or icon).
        PrivacyUse screen{Capability::ScreenCapture,L"Screen recorder",L"C:#Program Files#Recorder#recorder.exe",false,1,nullptr};
        privacyUses_={camera,mic};if(cmd.find(L"--qa-screen")!=std::wstring::npos)privacyUses_.push_back(screen);content_.privacy=privacyUses_;
        // --qa-card-later: the card arrives 1.2 s after start, so a timed capture can catch it moving.
        if(cmd.find(L"--qa-card-later")!=std::wstring::npos){qaLater_=privacyUses_;privacyUses_.clear();content_.privacy.clear();refresh();animate();SetTimer(window_,49,1200,nullptr);}
        else if(cmd.find(L"--qa-privacy-card")!=std::wstring::npos)showPrivacyNotice(cmd.find(L"--qa-screen")!=std::wstring::npos?screen:camera);else{refresh();animate();}}
    if(auto at=cmd.find(L"--qa-command");testing_&&at!=std::wstring::npos){std::wstring text;auto eq=cmd.find(L'=',at);if(eq!=std::wstring::npos&&eq<cmd.find(L' ',at)){auto end=cmd.find(L" --",eq);text=cmd.substr(eq+1,end==std::wstring::npos?std::wstring::npos:end-eq-1);for(auto& c:text)if(c==L'_')c=L' ';}
        if(!commands_)commands_=std::make_unique<CommandService>(window_,[]{wchar_t pub[MAX_PATH]{};GetEnvironmentVariableW(L"PUBLIC",pub,MAX_PATH);return *pub?std::wstring(pub):std::wstring(L"C:\\Users\\Public");}(),store_.directory);openCommand();content_.command.text=text;content_.command.caret=text.size();commandQuery();refresh();}
    if(testing_&&cmd.find(L"--qa-device-card")!=std::wstring::npos){auto devices=qaDevices();auto it=std::find_if(devices.begin(),devices.end(),[](auto& d){return d.battery>=0&&!d.brand.empty();});if(it==devices.end()&&!devices.empty())it=devices.begin();if(it!=devices.end())showNotice(1,*it);}
    if(testing_&&cmd.find(L"--qa-power-card")!=std::wstring::npos){if(battery_)updateBattery();showNotice(content_.charging?3:4);}
    for(auto pair:{std::pair{L"--qa-battery",1},std::pair{L"--qa-devices",2}})if(testing_&&cmd.find(pair.first)!=std::wstring::npos){content_.page=Page::System;content_.statsTab=pair.second;content_.pinned=true;if(bluetooth_||sample)content_.devices=qaDevices();if(battery_){battery_->setFast(true);updateBattery();}transition(IslandState::Expanded);refresh();}
    if(testing_&&cmd.find(L"--qa-hud")!=std::wstring::npos){content_.volume=audio_?audio_->value.load():40;events_.publish({ActivityKind::Volume,"volume",40,double(content_.volume),.5,30},seconds());presentActivity();}
    if(testing_&&cmd.find(L"--qa-brightness")!=std::wstring::npos){content_.brightness=64;events_.publish({ActivityKind::Brightness,"brightness",40,64,.5,30},seconds());presentActivity();}
    if(cmd.find(L"--capture")!=std::wstring::npos&&!benchmark_&&cmd.find(L"--ui-test")==std::wstring::npos&&cmd.find(L"--qa-overlay")==std::wstring::npos){UINT after=cmd.find(L"--qa-command")!=std::wstring::npos?4200:2800;if(const auto k=cmd.find(L"--capture-after=");testing_&&k!=std::wstring::npos)after=UINT(std::clamp(_wtoi(cmd.c_str()+k+16),100,20000));SetTimer(window_,5,after,nullptr);}if(testing_&&cmd.find(L"--qa-entrance")!=std::wstring::npos)SetTimer(window_,34,2700,nullptr); store_.log("Info","application_started_directcomposition");
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    store_.log("Info","application_stopped");return int(msg.wParam);
}
IslandWindow::~IslandWindow(){
    if(qaMatte_)DestroyWindow(qaMatte_);if(qaBrush_)DeleteObject(qaBrush_);
    foregroundOwner=nullptr;if(foregroundHook_)UnhookWinEvent(foregroundHook_);if(locationHook_)UnhookWinEvent(locationHook_);
    if(hotkey_)UnregisterHotKey(window_,0x4e49);clipboard_.stop();commands_.reset();privacy_.reset();
    if(platformThread_.joinable())platformThread_.join();
    previews_.reset();powerMode_.reset();bluetooth_.reset();battery_.reset();brightness_.reset();mixer_.reset();analyzer_.reset();system_.reset();media_.reset();audio_.reset();for(auto h:powerNotifications_)UnregisterPowerSettingNotification(h);
    if(tray_.hWnd)Shell_NotifyIconW(NIM_DELETE,&tray_);
    renderer_.reset();if(window_&&IsWindow(window_))DestroyWindow(window_);
}
void IslandWindow::position(){
    positioning_=true;
    std::vector<HMONITOR> monitors;EnumDisplayMonitors(nullptr,nullptr,collectMonitor,reinterpret_cast<LPARAM>(&monitors));
    HMONITOR selected=MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY);
    if(settings_.monitor>0&&size_t(settings_.monitor)<=monitors.size())selected=monitors[settings_.monitor-1];
    if(!selectDisplay_&&!displays_.preferred.empty()){selected=MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY);for(auto monitor:monitors)if(monitorIdentity(monitor)==displays_.preferred){selected=monitor;break;}}
    auto identity=monitorIdentity(selected);if(displays_.preferred.empty())displays_.preferred=identity;if(selectDisplay_){displays_.preferred=identity;selectDisplay_=false;}
    if(identity!=currentDisplay_){displays_.restore(identity,settings_);currentDisplay_=identity;}
    MONITORINFO info{sizeof(info)};GetMonitorInfoW(selected,&info);
    UINT monitorDpiX=96,monitorDpiY=96;if(FAILED(GetDpiForMonitor(selected,MDT_EFFECTIVE_DPI,&monitorDpiX,&monitorDpiY)))monitorDpiX=GetDpiForWindow(window_);dpi_=float(monitorDpiX)*settings_.scale/100.f;float s=dpi_/96;
    int width=int(Renderer::canvasWidth*s),height=int(Renderer::canvasHeight*s);
    int x=(info.rcMonitor.left+info.rcMonitor.right-width)/2+int(settings_.horizontalOffset*s);
    // Widest body plus its shoulders (twice the radius each side) must stay on the monitor.
    int footprint=int(std::max(440+2*64,settings_.compactWidth+2*34)*s);if(footprint<=info.rcMonitor.right-info.rcMonitor.left)x=std::clamp<int>(x,info.rcMonitor.left-(width-footprint)/2,info.rcMonitor.right-(width+footprint)/2);else x=(info.rcMonitor.left+info.rcMonitor.right-width)/2;
    if(settings_.edge==1)x=info.rcMonitor.right-width;else if(settings_.edge==2)x=info.rcMonitor.left;int y=settings_.edge?(info.rcMonitor.top+info.rcMonitor.bottom-height)/2+int(settings_.verticalOffset*s):info.rcMonitor.top+int(settings_.verticalOffset*s);SetWindowPos(window_,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);syncShadow();positioning_=false;dpi_=float(GetDpiForWindow(window_))*settings_.scale/100.f;
}
// The shadow window keeps the island's rectangle, sits directly beneath it and shows and hides with it.
void IslandWindow::syncShadow(){
    if(!shadow_||!window_)return;RECT r{};GetWindowRect(window_,&r);const bool visible=IsWindowVisible(window_);
    SetWindowPos(shadow_,window_,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
}
void IslandWindow::updateRegion(bool envelope){
    if(!window_)return;double scale=dpi_/96,now=seconds();HRGN region=CreateRectRgn(0,0,0,0);
    for(double dt:{0.,.03,.06,.09}){if(!envelope&&dt>0)break;double t=now+dt,w=motion_.width.sample(t).position,h=motion_.height.sample(t).position,r=motion_.radius.sample(t).position,drop=motion_.drop.sample(t).position;const bool out=dropped(t);
        const double dragX=motion_.dragX.sample(t).position,dragY=motion_.dragY.sample(t).position,slide=motion_.slide.sample(t).position;
        // The lean (top dock): shear about the top edge and stretch about the top centre, before the drag offset.
        const double lean=settings_.edge==0&&!motion_.reduced?std::tan(dragX*leanDegreesPerDip*3.14159265358979/180):0,sy=settings_.edge==0&&!motion_.reduced?1+dragY*stretchPerDip:1,sx=settings_.edge==0&&!motion_.reduced?1-dragY*narrowPerDip:1,cx=Renderer::canvasWidth/2;
        auto add=[&](const std::vector<PointD>& outline,PointD at){std::vector<POINT> points;points.reserve(outline.size());
            for(auto p:outline){double x=p.x+at.x,y=p.y+at.y;x+=lean*y;x=cx+(x-cx)*sx;y*=sy;points.push_back({LONG(std::lround((x+dragX)*scale)),LONG(std::lround((y+dragY)*scale))});}
            HRGN pose=CreatePolygonRgn(points.data(),int(points.size()),WINDING);CombineRgn(region,region,pose,RGN_OR);DeleteObject(pose);};
        auto origin=bodyAt(w,h,drop);if(settings_.edge)origin.x+=slide*74;else origin.y-=slide*44;
        // A dropped pill is a floating shape of its own; the docked stub above it keeps the shoulders.
        add(dockOutline(w,h,r,settings_.edge,!settings_.floating()&&!out),origin);
        // The bud of a waiting alert, below the pill.
        if(out){const auto bud=budShape(motion_.bud.sample(t).position,origin.y+h,Renderer::canvasWidth/2,motion_.budWidth,motion_.budHeight);if(bud.bottom-bud.top>2)add(dockOutline(bud.right-bud.left,bud.bottom-bud.top,bud.radius,0,false),{bud.left,bud.top});}
        if(out){const double corner=std::min({r,w/2,h/2}),dp=drop*dropDistance,tall=std::clamp(std::max(std::min(2*dp-corner-2,dp),0.)+1,1.,motion_.stubHeight),span=w+(motion_.stubWidth-w)*std::clamp((drop*dropDistance-(corner+2)/2)/12,0.,1.);add(dockOutline(span,tall,std::min(motion_.stubRadius,tall/2),0,true),{(Renderer::canvasWidth-span)/2,-slide*44});}}
    HRGN outline=CreateRectRgn(0,0,0,0);CombineRgn(outline,region,region,RGN_COPY);for(auto offset:std::array<POINT,8>{{{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}}){HRGN fringe=CreateRectRgn(0,0,0,0);CombineRgn(fringe,outline,outline,RGN_COPY);OffsetRgn(fringe,offset.x,offset.y);CombineRgn(region,region,fringe,RGN_OR);DeleteObject(fringe);}DeleteObject(outline);
    // The shadow shows everywhere except under the island itself, so it never darkens see-through glass.
    if(shadow_){RECT r{};GetClientRect(window_,&r);HRGN around=CreateRectRgn(0,0,r.right,r.bottom);CombineRgn(around,around,region,RGN_DIFF);if(!SetWindowRgn(shadow_,around,FALSE))DeleteObject(around);}
    if(!SetWindowRgn(window_,region,FALSE))DeleteObject(region);
}
void IslandWindow::animate(){
    double now=seconds();bool expanded=state_!=IslandState::Compact;motion_.live=content_.live;motion_.card=content_.card;motion_.target(state_,now,interaction_==InteractionState::Hover,interaction_==InteractionState::Pressed);
    bool artwork=content_.playback.artwork&&!content_.card&&(expanded?(content_.live||content_.page==Page::Media||content_.page==Page::Overview):settings_.compactMedia&&!content_.hud);
    auto move=[&](Spring& spring,double target,SpringSpec spec){if(spring.target()!=target){if(motion_.reduced)spring.reset(target,now);else spring.retarget(target,now,spec);}};
    move(motion_.artX,expanded?20:settings_.edge?21:12,MotionTokens::artwork);move(motion_.artY,content_.live?20:expanded?(content_.page==Page::Media?(settings_.mediaLayout==2||(settings_.mediaLayout==0&&content_.playback.kind==MediaKind::Video)?68:82):81):settings_.edge?18:6,MotionTokens::artwork);move(motion_.artSize,content_.live?52:expanded?(content_.page==Page::Media?(settings_.mediaLayout==2||(settings_.mediaLayout==0&&content_.playback.kind==MediaKind::Video)?148:100):64):22,MotionTokens::artwork);move(motion_.artOpacity,artwork?1:0,MotionTokens::artworkOpacity);
    renderer_->animate(motion_,now);lastMotion_=now;updateRegion(true);SetTimer(window_,SettleTimer,30,nullptr);
}
void IslandWindow::transition(IslandState s){
    // The command bar closes only through closeCommand (Esc, run, click away), never by an activity or hover timing out.
    if(content_.command.active&&s!=IslandState::Command)return;
    // Phase 5G: an alert waiting as a bud takes the pill's place rather than the island closing.
    if(s==IslandState::Compact&&!heldCards_.empty()&&promoteCard())return;
    bool changed=state_!=s;state_=s;
    // Leaving the pill, the bud goes back in (its alert still waits).
    if(changed&&s!=IslandState::Notification&&motion_.bud.target()!=0){content_.bud={};const double t=seconds();if(motion_.reduced)motion_.bud.reset(0,t);else motion_.bud.retarget(0,t,SpringSpec{1,320,36});}if(s!=IslandState::Compact&&content_.hud){content_.hud=0;motion_.compactWidth=settings_.uiMode==0?72:settings_.compactWidth;}content_.expanded=s!=IslandState::Compact;content_.live=s==IslandState::LiveActivity||s==IslandState::Notification||s==IslandState::Command;content_.card=s==IslandState::Notification||s==IslandState::Command;
    // A notification drops out of the docked island as its own pill (a little bounce as it settles), and rises back into it after.
    {const bool drop=s==IslandState::Notification&&settings_.notifyStyle==1&&settings_.edge==0&&!settings_.floating();const double now=seconds(),target=drop?1:0;
        if(drop&&motion_.drop.target()<=0&&std::abs(motion_.drop.sample(now).position)<1e-3)motion_.stubWidth=std::min(motion_.compactWidth,196.);
        if(motion_.drop.target()!=target){if(motion_.reduced)motion_.drop.reset(target,now);else motion_.drop.retarget(target,now,drop?SpringSpec{1,180,16}:SpringSpec{1,260,2*std::sqrt(260.)});}}
    if(changed){content_.settings=settings_;if(content_.expanded)refresh();else renderer_->redraw(content_,debug_,true);}clockTimer();animate();}
void IslandWindow::power(bool notify){
    SYSTEM_POWER_STATUS p{};if(GetSystemPowerStatus(&p)){
        const int previousBattery=content_.battery;const bool previouslyCharging=content_.charging;
        content_.battery=p.BatteryLifePercent<=100?p.BatteryLifePercent:-1;
        if(p.BatteryFlag&128)content_.battery=-1;
        content_.charging=p.ACLineStatus==1;if(notify&&!previouslyCharging&&content_.charging){motion_.pulse.reset(motion_.reduced?.15:.8,seconds());motion_.pulse.retarget(0,seconds(),{1,50,18});}
        if(notify&&(previousBattery!=content_.battery||previouslyCharging!=content_.charging)){
            if(previouslyCharging!=content_.charging&&settings_.powerCards){if(battery_)battery_->refresh();showNotice(content_.charging?3:4);}
            else if(previouslyCharging!=content_.charging||(content_.battery>=0&&content_.battery<10)){
                events_.publish({ActivityKind::Power,"power",content_.battery>=0&&content_.battery<10?100:30,double(content_.battery),.8,3},seconds());presentActivity();
            }else refresh();
        }
    }
}
void IslandWindow::presentActivity(){
    if(!events_.active())return;
    switch(events_.active()->kind){
    case ActivityKind::Volume:content_.headline=content_.muted?L"A moment of quiet.":L"Sound, just where you want it.";content_.detail=L"System output volume";break;
    case ActivityKind::Brightness:content_.headline=L"Light, just right.";content_.detail=L"Display brightness";break;
    case ActivityKind::Power:content_.headline=content_.battery>=0&&content_.battery<10?L"Time to connect your charger.":content_.charging?L"A little energy. A fresh start.":L"Ready to go with you.";content_.detail=content_.charging?L"Power connected. Settle in.":L"Running on battery power.";break;
    case ActivityKind::Media:content_.headline=L"A new rhythm.";content_.detail=L"Now in your Windows media session";break;
    default:break;
    }
    auto kind=events_.active()->kind;content_.activity=kind==ActivityKind::Volume?(content_.muted?L"Muted":L"Volume "+std::to_wstring(content_.volume)+L"%"):kind==ActivityKind::Power?(content_.charging?L"Power connected":L"On battery"):kind==ActivityKind::Timer?L"Session complete":kind==ActivityKind::Device?L"Output · "+routeName_:kind==ActivityKind::Brightness?L"Brightness "+std::to_wstring(content_.brightness)+L"%":kind==ActivityKind::Clipboard?copyLabel_:kind==ActivityKind::AppVolume?(content_.appVolume>=0?content_.appVolumeName+L" "+std::to_wstring(content_.appVolume)+L"%":std::wstring(L"No volume control for this app")):kind==ActivityKind::Microphone?std::wstring(content_.micMuted?L"Microphone off":L"Microphone on"):L"";levelIndicator();refresh();animate();SetTimer(window_,ActivityTimer,500,nullptr);
}
void CALLBACK IslandWindow::foregroundEvent(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG child,DWORD,DWORD){
    if(!foregroundOwner)return;
    if(event==EVENT_OBJECT_LOCATIONCHANGE&&(object!=OBJID_WINDOW||child!=CHILDID_SELF||hwnd!=GetForegroundWindow()))return;
    PostMessageW(foregroundOwner->window_,FullscreenMessage,event==EVENT_OBJECT_LOCATIONCHANGE,0);
}
void IslandWindow::yieldToApp(){
    if(!AppSwitchPolicy::collapse(settings_.collapseOnAppSwitch,content_.pinned,
        interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging,content_.dropHover))return;
    KillTimer(window_,7);KillTimer(window_,8);interaction_=InteractionState::Rest;content_.hovered=Action::None;
    feedback(Action::None);if(state_!=IslandState::Compact)transition(IslandState::Compact);
}
void IslandWindow::fullscreen(){
    if(benchmark_||qaFullscreen_)return;
    HWND fg=GetForegroundWindow();if(!fg||fg==window_)return;
    DWORD pid=0;GetWindowThreadProcessId(fg,&pid);if(pid==GetCurrentProcessId())return;
    wchar_t cls[128]{};GetClassNameW(fg,cls,128);
    bool shell=wcscmp(cls,L"Progman")==0||wcscmp(cls,L"WorkerW")==0||wcscmp(cls,L"Shell_TrayWnd")==0;
    bool hide=false;
    if(settings_.hideFullscreen&&!shell&&IsWindowVisible(fg)&&!IsIconic(fg)){
        RECT r{};MONITORINFO mi{sizeof(mi)};auto monitor=MonitorFromWindow(fg,MONITOR_DEFAULTTONEAREST);
        if(SUCCEEDED(DwmGetWindowAttribute(fg,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))&&GetMonitorInfoW(monitor,&mi))
            hide=monitor==MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST)&&AppSwitchPolicy::fullscreen(r.left,r.top,r.right,r.bottom,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right,mi.rcMonitor.bottom,(GetWindowLongPtrW(fg,GWL_STYLE)&WS_CAPTION)!=0,IsZoomed(fg));
    }
    if(visibilityAudit_){bool decorated=(GetWindowLongPtrW(fg,GWL_STYLE)&WS_CAPTION)!=0,zoomed=IsZoomed(fg);store_.submit([dir=store_.directory,hide,decorated,zoomed,shell]{std::ofstream f(dir/L"visibility-audit.json");f<<"{\"hidden\":"<<hide<<",\"decorated\":"<<decorated<<",\"maximized\":"<<zoomed<<",\"shell\":"<<shell<<"}\n";});}
    fullscreenHidden_=hide;if(hide&&peeking_)hide=false;
    if(hide){KillTimer(window_,7);KillTimer(window_,8);interaction_=InteractionState::Rest;content_.hovered=Action::None;feedback(Action::None);}
    if(bool(IsWindowVisible(window_))==hide){ShowWindow(window_,hide?SW_HIDE:SW_SHOWNOACTIVATE);if(!hide)SetTimer(window_,SettleTimer,30,nullptr);}if(!hide)SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);syncShadow();
    clockTimer();
}
LRESULT CALLBACK IslandWindow::procedure(HWND h,UINT m,WPARAM w,LPARAM l){
    auto self=reinterpret_cast<IslandWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<IslandWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self)try{return self->message(m,w,l);}catch(const std::exception&){self->store_.log("Error","window_operation_failed");if(self->renderer_)PostMessageW(h,WM_CLOSE,0,0);}
    return DefWindowProcW(h,m,w,l);
}
LRESULT IslandWindow::message(UINT m,WPARAM w,LPARAM l){
    if(musicMessage(m,w,l))return 0;
    {LRESULT handled=0;if(clipMessage(m,w,l,handled)||captureMessage(m,w,l,handled)||productivityMessage(m,w,l,handled))return handled;}
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_APP+50:if(w==1)openSettings(3);else if(w==2)transition(IslandState::Expanded);else if(w==3){settings_.startAtLogin=startup::apply(true);store_.save(settings_,settingsFile_);refresh();}return 0;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(window_,&ps);EndPaint(window_,&ps);return 0;}
    case WM_MOUSEACTIVATE:return state_==IslandState::Compact?MA_NOACTIVATE:MA_ACTIVATE;
    case WM_KEYDOWN:if(content_.command.active&&commandKey(w))return 0;if(content_.hovered==Action::VolumeSlider&&audio_&&(w==VK_LEFT||w==VK_RIGHT||w==VK_HOME||w==VK_END)){audio_->setVolume(w==VK_HOME?0:w==VK_END?100:audio_->value+(w==VK_RIGHT?2:-2));return 0;}if(content_.hovered==Action::Seek&&!content_.scrub.active&&(w==VK_LEFT||w==VK_RIGHT)){seekBy(w==VK_RIGHT?10:-10);return 0;}if(w==VK_ESCAPE){if(content_.scrub.active){endScrub(false);ReleaseCapture();pressedAction_=Action::None;return 0;}perform(Action::Close);return 0;}if(w==VK_TAB){std::vector<Action> enabled;for(auto& t:renderer_->targets)if(t.enabled)enabled.push_back(t.action);auto it=std::find(enabled.begin(),enabled.end(),content_.hovered);int index=it==enabled.end()?-1:int(it-enabled.begin());if(!enabled.empty()){index=(index+((GetKeyState(VK_SHIFT)&0x8000)?int(enabled.size())-1:1))%int(enabled.size());content_.hovered=enabled[index];feedback(content_.hovered);}return 0;}if(w==VK_RETURN||w==VK_SPACE){perform(content_.hovered);return 0;}break;
    case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window_,&p);double t=seconds(),s=dpi_/96;
        double width=motion_.width.sample(t).position,height=motion_.height.sample(t).position;
        auto origin=bodyAt(width,height,motion_.drop.sample(t).position);double slide=motion_.slide.sample(t).position;double x=p.x/s-origin.x-motion_.dragX.sample(t).position-(settings_.edge?slide*74:0),y=p.y/s-origin.y-motion_.dragY.sample(t).position+(settings_.edge?0:slide*44);
        if(x>=0&&x<=width&&y>=0&&y<=height)return HTCLIENT;
        // The bud of a waiting alert takes clicks too.
        if(budAt(p.x/s,p.y/s))return HTCLIENT;return HTTRANSPARENT;}
    case WM_MOUSEMOVE:{
        KillTimer(window_,8);
        if(interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging){
            if(pressedAction_==Action::Seek){scrubAt(l);return 0;}if(pressedAction_==Action::VolumeSlider){setVolumeAt(l);return 0;}if(pressedAction_==Action::ControlBrightness){setBrightnessAt(l);return 0;}if(inRange(pressedAction_,Action::MixerSliderBase,Action::MixerMuteBase)){setMixerAt(l);return 0;}if(pressedAction_!=Action::None){if(inRange(pressedAction_,Action::ShelfItemBase,Action::MixerSliderBase)){POINT pointer{};GetCursorPos(&pointer);if(std::abs(pointer.x-down_.x)+std::abs(pointer.y-down_.y)>6){size_t index=int(pressedAction_)-int(Action::ShelfItemBase);pressedAction_=Action::None;interaction_=InteractionState::Rest;ReleaseCapture();dragShelf(index);}return 0;}
                // A press on a button that then travels sideways over the music becomes a swipe (the button is not pressed).
                POINT pointer{};GetCursorPos(&pointer);const double sx=(pointer.x-down_.x)*96/dpi_,sy=(pointer.y-down_.y)*96/dpi_;
                if(!mediaSwipe()||std::abs(sx)<=10||std::abs(sx)<=std::abs(sy)*1.3)return 0;
                // The Shelf's stack, dragged: every file at once.
                if(pressedAction_==Action::ShelfStack){if(std::abs(sx)+std::abs(sy)>6){pressedAction_=Action::None;interaction_=InteractionState::Rest;ReleaseCapture();dragStack();}return 0;}
                const Action was=pressedAction_;pressedAction_=Action::None;renderer_->iconFeedback(was,false,settings_.animatedIcons&&!motion_.reduced);interaction_=InteractionState::Dragging;swipeAxis_=1;}
            POINT p{};GetCursorPos(&p);double dx=(p.x-down_.x)*96/dpi_,dy=(p.y-down_.y)*96/dpi_;
            // Sideways over the music: the drag skips tracks. The island barely moves; what plays follows the drag.
            if(interaction_!=InteractionState::Dragging&&std::abs(dx)+std::abs(dy)>4)swipeAxis_=mediaSwipe()&&std::abs(dx)>std::abs(dy)*1.2?1:2;
            if(swipeAxis_==1&&std::abs(dx)+std::abs(dy)>4){interaction_=InteractionState::Dragging;KillTimer(window_,7);const double now=seconds();
                motion_.dragX.reset(rubberBand(dx*.18,24),now);motion_.dragY.reset(0,now);const bool header=state_==IslandState::Compact;
                if(!header)motion_.swipe.reset(motion_.reduced?0:std::clamp(dx*.35,-30.,30.),now);
                renderer_->animate(motion_,now);renderer_->swipeFollow(float(dx),float(motion_.width.sample(now).position),float(motion_.height.sample(now).position),header,motion_.reduced);
                dragTime_=now;lastPointer_=p;return 0;}
            if(std::abs(dx)+std::abs(dy)>4){interaction_=InteractionState::Dragging;double now=seconds();double dt=now-dragTime_;
                if(dt>.002)dragVelocity_=std::clamp((p.y-lastPointer_.y)*96/dpi_/dt,-1600.,1600.);
                motion_.dragX.reset(rubberBand(dx,100),now);motion_.dragY.reset(rubberBand(std::max(0.,dy),100),now);
                renderer_->animate(motion_,now);dragTime_=now;lastPointer_=p;}
        }else{
            if(interaction_==InteractionState::Rest){interaction_=InteractionState::Hover;TRACKMOUSEEVENT e{sizeof(e),TME_LEAVE,window_,0};TrackMouseEvent(&e);animate();GetCursorPos(&hoverSample_);hoverSampleTime_=seconds();if(settings_.hoverOpen&&state_==IslandState::Compact)SetTimer(window_,7,settings_.hoverDelay,nullptr);}
            else if(edgeHold_&&pointerOffEdge()){edgeHold_=false;if(settings_.hoverOpen&&state_==IslandState::Compact&&interaction_==InteractionState::Hover){GetCursorPos(&hoverSample_);hoverSampleTime_=seconds();SetTimer(window_,7,settings_.hoverDelay,nullptr);}}
            else if(settings_.hoverOpen&&state_==IslandState::Compact&&interaction_==InteractionState::Hover){
                // Intent: a pointer sweeping past keeps restarting the delay; only a resting pointer opens.
                POINT p{};GetCursorPos(&p);double now=seconds(),dt=now-hoverSampleTime_;double speed=dt>0?std::hypot(double(p.x-hoverSample_.x),double(p.y-hoverSample_.y))*96/dpi_/dt:0;
                if(dt>.004&&speed>700)SetTimer(window_,7,settings_.hoverDelay,nullptr);hoverSample_=p;hoverSampleTime_=now;}
            {double now=seconds();auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);
                renderer_->pointer(float(GET_X_LPARAM(l)*96/dpi_-origin.x-motion_.dragX.sample(now).position),float(GET_Y_LPARAM(l)*96/dpi_-origin.y-motion_.dragY.sample(now).position),true,motion_.reduced);}
            auto target=hit(l);
            // In the command bar the highlight follows the pointer across results and stays on the selection otherwise.
            if(content_.command.active){if(inRange(target,Action::CommandResultBase,Action::CommandResultEnd)){int row=int(target)-int(Action::CommandResultBase);if(row!=content_.command.selected){content_.command.selected=row;refresh();}}else target=Action(int(Action::CommandResultBase)+content_.command.selected);}
            {float hover=-1;if(target==Action::Seek){double now=seconds();auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);hover=float(std::clamp(GET_X_LPARAM(l)*96/dpi_-origin.x-motion_.dragX.sample(now).position-20,0.,380.));}
                if(hover!=content_.seekHover){content_.seekHover=hover;renderer_->seekPreview(content_);}}
            if(target!=content_.hovered){const Action was=content_.hovered;bool detail=target==Action::Seek||content_.hovered==Action::Seek||content_.page==Page::Shelf||state_==IslandState::Compact;content_.hovered=target;if(detail)refresh();
                // Leaving the compact controls for the rest of the island starts the hover delay again.
                if(state_==IslandState::Compact&&settings_.hoverOpen&&interaction_==InteractionState::Hover&&target==Action::None&&was!=Action::None){GetCursorPos(&hoverSample_);hoverSampleTime_=seconds();SetTimer(window_,7,settings_.hoverDelay,nullptr);}KillTimer(window_,19);if(content_.live&&target==Action::Overview)SetTimer(window_,19,420,nullptr);}feedback(target,GET_X_LPARAM(l),GET_Y_LPARAM(l));
        }return 0;}
    case WM_MOUSELEAVE:KillTimer(window_,7);edgeHold_=false;if(content_.seekHover>=0){content_.seekHover=-1;renderer_->seekPreview(content_);}renderer_->pointer(0,0,false,motion_.reduced);KillTimer(window_,19);if(interaction_==InteractionState::Hover){interaction_=InteractionState::Rest;content_.hovered=Action::None;feedback(Action::None);refresh();animate();if(!content_.pinned)SetTimer(window_,8,settings_.collapseDelay,nullptr);}return 0;
    case WM_LBUTTONDOWN:pressedAction_=hit(l);
        if(pressedAction_==Action::SkipBack||pressedAction_==Action::SkipForward){const double t=seconds();POINT c{};GetCursorPos(&c);
            const bool again=skipClickAction_==pressedAction_&&t-skipClickTime_<=GetDoubleClickTime()/1000.&&std::abs(c.x-skipClickPoint_.x)<=GetSystemMetrics(SM_CXDOUBLECLK)&&std::abs(c.y-skipClickPoint_.y)<=GetSystemMetrics(SM_CYDOUBLECLK);
            if(again)seekBy(pressedAction_==Action::SkipForward?10:-10);skipClickAction_=pressedAction_;skipClickTime_=t;skipClickPoint_=c;}
        if(pressedAction_==Action::Seek)scrubAt(l,true);if(pressedAction_==Action::VolumeSlider)setVolumeAt(l);if(pressedAction_==Action::ControlBrightness)setBrightnessAt(l);if(inRange(pressedAction_,Action::MixerSliderBase,Action::MixerMuteBase))setMixerAt(l);interaction_=InteractionState::Pressed;GetCursorPos(&down_);lastPointer_=down_;dragTime_=seconds();SetCapture(window_);if(pressedAction_==Action::None)animate();else feedback(pressedAction_,GET_X_LPARAM(l),GET_Y_LPARAM(l),true);return 0;
    case WM_LBUTTONUP:{if(pressedAction_==Action::Seek){scrubAt(l);endScrub(true);}bool dragged=interaction_==InteractionState::Dragging;interaction_=InteractionState::Hover;const int axis=swipeAxis_;swipeAxis_=0;ReleaseCapture();
        if(dragged&&axis==1){POINT up{};GetCursorPos(&up);const double dx=(up.x-down_.x)*96/dpi_;endSwipe(std::abs(dx)>=44?(dx<0?1:-1):0);pressedAction_=Action::None;feedback(hit(l),GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;}
        if(dragged){double now=seconds();auto pos=motion_.dragY.sample(now).position;motion_.dragY.reset(pos,now,dragVelocity_*.3);motion_.dragY.retarget(0,now,motion_.body);motion_.dragX.retarget(0,now,motion_.body);animate();}
        else if(pressedAction_!=Action::None){if(pressedAction_==hit(l))perform(pressedAction_);}
        else{animate();}
        pressedAction_=Action::None;feedback(hit(l),GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;}
    case WM_CAPTURECHANGED:if(content_.scrub.active)endScrub(false);if(swipeAxis_==1&&interaction_==InteractionState::Dragging){swipeAxis_=0;interaction_=InteractionState::Rest;endSwipe(0);return 0;}swipeAxis_=0;if(interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging){interaction_=InteractionState::Rest;motion_.dragX.retarget(0,seconds(),motion_.body);motion_.dragY.retarget(0,seconds(),motion_.body);animate();}return 0;
    case WM_RBUTTONUP:showMenu();return 0;
    // A two-finger sideways swipe over the music skips a track, like a drag (sessions switch with the dots under the cover).
    case WM_MOUSEHWHEEL:if(mediaSwipe()){double now=seconds();if(now-hwheelAt_>.4)swipeAccumulator_=0;hwheelAt_=now;swipeAccumulator_+=GET_WHEEL_DELTA_WPARAM(w);
            if(std::abs(swipeAccumulator_)>=120&&now-swipeTime_>.45){skipTrack(swipeAccumulator_>0?1:-1);swipeAccumulator_=0;swipeTime_=now;}}return 0;
    case WM_MOUSEWHEEL:
        // The library's list scrolls a song at a time.
        if(state_!=IslandState::Compact&&content_.library&&content_.page==Page::Media){const int step=GET_WHEEL_DELTA_WPARAM(w)>0?-1:1;const int before=content_.libraryOffset;content_.libraryOffset+=step;libraryRows();if(content_.libraryOffset!=before)refresh();return 0;}
        if(state_==IslandState::Compact&&settings_.edge==0&&settings_.uiMode!=0&&settings_.compactMedia&&content_.playback.available&&(content_.playback.artwork||settings_.appIcons)){
            double now=seconds();auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window_,&p);
            const double x=p.x*96/dpi_-origin.x-motion_.dragX.sample(now).position,logo=content_.hud==3?std::max(236.,double(settings_.compactWidth))/2-109:14;
            if(x>=logo-10&&x<=logo+26){appVolumeWheel(GET_WHEEL_DELTA_WPARAM(w));return 0;}}
        if(state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==2){content_.deviceOffset=std::clamp(content_.deviceOffset+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,int(content_.devices.size())-4));refresh();return 0;}if(state_==IslandState::Expanded&&content_.page==Page::Shelf&&content_.shelfTab==1){content_.clipOffset=std::clamp(content_.clipOffset+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,int(content_.clips.size())-4));refresh();return 0;}if(state_!=IslandState::Compact&&(content_.page==Page::Shelf||content_.page==Page::Audio)){bool mixer=content_.page==Page::Audio&&content_.audioTab==0;auto& offset=content_.page==Page::Shelf?content_.shelfOffset:mixer?content_.mixerOffset:content_.audioOffset;int count=int(content_.page==Page::Shelf?content_.shelf.size():mixer?content_.mixer.size():content_.outputs.size());offset=std::clamp(offset+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,count-4));refresh();return 0;}if(audio_&&audio_->available){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window_,&p);if(settings_.wheelVolume||hit(MAKELPARAM(p.x,p.y))==Action::VolumeSlider)audio_->setVolume(audio_->value+(GET_WHEEL_DELTA_WPARAM(w)>0?2:-2));}return 0;
    case MediaMessage:if(media_)updateSessions();return 0;
    case LyricsMessage:syncLyrics();refresh();return 0;
    case SpectrumMessage:if(analyzer_){analyzer_->pending=false;bool delivering=analyzer_->active()&&analyzer_->available.load();if(delivering!=content_.waveform){content_.waveform=delivering;refresh();}if(delivering){auto frame=analyzer_->frame();renderer_->spectrum(frame);
        // The waveform timeline learns the track's loudness from what is actually heard.
        auto& p=content_.playback;if(settings_.waveTimeline&&p.playing&&p.duration>0&&!content_.scrub.active){double position=p.position+std::max(0.,seconds()-p.sampledAt);if(waves_.track(WaveformLibrary::key(p.title,p.artist,p.duration)).hear(position,p.duration,frame.level)>=0)pushWaveform(frame.level);}}}return 0;
    case MixerMessage:if(mixer_){mixer_->pending=false;auto entries=mixer_->entries();bool layout=entries.size()!=content_.mixer.size();for(size_t i=0;!layout&&i<entries.size();++i){auto& a=entries[i];auto& b=content_.mixer[i];layout=a.pid!=b.pid||a.name!=b.name||a.muted!=b.muted||a.active!=b.active||std::abs(a.volume-b.volume)>.004f||a.icon!=b.icon;}
        bool dragging=inRange(pressedAction_,Action::MixerSliderBase,Action::MixerMuteBase);content_.mixer=std::move(entries);content_.mixerOffset=std::clamp(content_.mixerOffset,0,std::max(0,int(content_.mixer.size())-4));
        bool shown=state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Audio&&content_.audioTab==0;if(layout&&!dragging&&shown)refresh();renderer_->meters(content_.mixer,content_.mixerOffset,shown);}return 0;
    case BatteryMessage:if(battery_)updateBattery();return 0;
    case BluetoothMessage:if(bluetooth_){content_.devices=bluetooth_->devices();auto events=bluetooth_->takeEvents();content_.deviceOffset=std::clamp(content_.deviceOffset,0,std::max(0,int(content_.devices.size())-4));
        if(deviceRequest_){deviceRequest_=false;content_.deviceFeedback=FAILED(HRESULT(bluetooth_->commandResult.load()))?L"Windows could not reach that device":L"";}
        if(!events.empty()&&settings_.deviceCards)showNotice(events.back().connected?1:2,events.back().device);
        if(state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==2)refresh();}return 0;
    case PlatformMessage:{std::unique_ptr<PlatformInfo> info(reinterpret_cast<PlatformInfo*>(l));content_.platform=*info;if(settingsWindow_&&settingsWindow_->open())settingsWindow_->update(settings_,settingsContext(),settingsSequence_);refresh();return 0;}
    case PowerModeMessage:content_.powerMode=int(w);if(state_==IslandState::Expanded&&content_.page==Page::System)refresh();return 0;
    case BrightnessMessage:if(brightness_){content_.brightness=brightness_->value.load();if(w&&seconds()-brightnessRequestAt_<1.5){refresh();return 0;}if(w){events_.publish({ActivityKind::Brightness,"brightness",40,double(content_.brightness),.5,2},seconds());presentActivity();}}return 0; case AudioMessage:if(audio_&&w==2){const bool was=content_.micMuted;content_.micMuted=audio_->micMuted;content_.micAvailable=audio_->micAvailable;if(was!=content_.micMuted){events_.publish({ActivityKind::Microphone,"microphone",45,0,.5,2},seconds());presentActivity();}return 0;}
        if(audio_){audio_->notificationPending=false;const bool micWas=content_.micMuted;content_.micMuted=audio_->micMuted||qaMicMuted_;content_.micAvailable=audio_->micAvailable||qaMicMuted_;
        if(micKnown_&&content_.micAvailable&&micWas!=content_.micMuted){events_.publish({ActivityKind::Microphone,"microphone",45,0,.5,2},seconds());presentActivity();}micKnown_=true;content_.outputs=audio_->devices();const std::wstring fromId=route_.current,fromName=lastRouteName_;std::wstring current;for(auto& d:content_.outputs)if(d.current){current=d.id;routeName_=d.name;}bool routeChanged=route_.observe(current);lastRouteName_=routeName_;if(FAILED(audio_->switchResult.load()))content_.feedback=L"Switch unavailable · open Windows sound settings";else content_.feedback=L"";content_.volume=audio_->value;content_.muted=audio_->muted;motion_.volume.retarget(content_.muted?0:content_.volume/100.,seconds(),{1,550,42});
        if(w){events_.publish({ActivityKind::Volume,"volume",40,double(content_.volume),.5,2},seconds());presentActivity();}
        else{refresh();renderer_->animate(motion_,seconds());}if(routeChanged){const auto out=std::find_if(content_.outputs.begin(),content_.outputs.end(),[](auto& d){return d.current;});
            // Windows moved the sound to headphones (not the island itself): a card that offers the way back.
            const bool headphones=settings_.headphoneCards&&out!=content_.outputs.end()&&isHeadphoneOutput(out->name,out->form)&&seconds()-routeRequestAt_>4&&!fromId.empty()&&showHeadphoneCard(*out,fromId,fromName);
            if(!headphones)events_.publish({ActivityKind::Device,"audio-route",45,0,.8,3},seconds());motion_.pulse.reset(motion_.reduced?0:.18,seconds());motion_.pulse.retarget(0,seconds(),{1,70,18});presentActivity();auto selected=std::find_if(content_.outputs.begin(),content_.outputs.end(),[](auto& d){return d.current;});renderer_->routeConfirmed(motion_.reduced,selected==content_.outputs.end()?Action::None:Action(int(Action::DeviceBase)+int(selected-content_.outputs.begin())));}}return 0;
    case ShelfPreviewMessage:if(previews_){for(auto& item:content_.shelf)if(item.kind==ShelfItem::Kind::File)item.preview=previews_->get(item.value);refresh();}return 0; case SystemMessage:if(system_){content_.system=system_->snapshot();
        // Open: every reading. Compact (the idle glance): only when a shown percentage changes.
        const long glance=content_.system.cpu<0?-1:std::lround(content_.system.cpu)*1000+(content_.system.gpu<0?999:std::lround(content_.system.gpu));
        if(IsWindowVisible(window_)&&(state_!=IslandState::Compact||(systemRequested_&&glance!=glanceShown_))){glanceShown_=glance;refresh();}}return 0;
    case WM_POWERBROADCAST:if(w==PBT_POWERSETTINGCHANGE)power(true);return TRUE;
    case SettingsChangedMessage:{std::unique_ptr<Settings> incoming(reinterpret_cast<Settings*>(l));settingsSequence_=unsigned(w);if(incoming)receiveSettings(*incoming);return 0;}
    case SettingsActionMessage:settingsAction(SettingAction(w),int(l));return 0;
    case ShareMessage:shareEvents();return 0;
    case WallpaperLumaMessage:{std::unique_ptr<std::shared_ptr<WallpaperLuma>> map(reinterpret_cast<std::shared_ptr<WallpaperLuma>*>(l));wallLoading_=false;if(map&&*map)wallLuma_=*map;adaptBackdrop();return 0;}
    case FullscreenMessage:adaptBackdrop();if(w){SetTimer(window_,17,120,nullptr);}else{DWORD pid=0;auto fg=GetForegroundWindow();if(fg)GetWindowThreadProcessId(fg,&pid);if(!testing_&&pid&&pid!=GetCurrentProcessId())yieldToApp();fullscreen();}return 0;
    case WM_DISPLAYCHANGE:if(renderer_)applySettings(true);return 0;
    case WM_DPICHANGED:if(positioning_)return 0;if(renderer_)applySettings(true);return 0;
    case WM_SETTINGCHANGE:if(renderer_)applySettings();
        if(w==SPI_SETDESKWALLPAPER){wallLuma_.reset();loadWallpaperLuma();}
        if(w==SPI_SETDESKWALLPAPER)std::thread([window=window_]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);auto info=new PlatformInfo(platformInfo());info->wallpaper=wallpaperAccent();if(!PostMessageW(window,PlatformMessage,0,reinterpret_cast<LPARAM>(info)))delete info;CoUninitialize();}).detach();
        return 0;
    case TrayMessage:if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU)showMenu();else if(l==WM_LBUTTONDBLCLK)openSettings();return 0;
    case SiteIconMessage:clipViews();if(state_!=IslandState::Compact&&content_.page==Page::Shelf&&content_.shelfTab==1)refresh();return 0;
    case WeatherMessage:if(weather_){auto now=weather_->now();auto place=weather_->place();if(now&&place)content_.weather={true,now->temperature,now->code,now->day,place->name};
        if(weatherAsked_&&content_.command.active){weatherAsked_=false;if(w)commandStatus(weather_->status(),true);else if(now&&place)commandStatus(place->name+L"  \u00b7  "+temperatureText(now->temperature,settings_.weatherUnit)+L"  "+skyName(skyOf(now->code)));}
        refresh();}return 0;
    case ControlStateMessage:{auto& c=content_.controls;auto part=[&](int shift){return int((w>>shift)&15)-2;};c.wifi=part(0);c.bluetooth=part(4);c.dark=part(8);c.busy&=~int(l);
        if(state_!=IslandState::Compact&&content_.page==Page::Control)refresh();return 0;}
    case WM_TIMER: if(w==17){KillTimer(window_,17);fullscreen();return 0;}if(w==66&&testing_){KillTimer(window_,66);perform(Action::Shelf);return 0;}if(w==63){KillTimer(window_,63);syncBud();return 0;}if(w==68&&testing_){KillTimer(window_,68);const double t=seconds();renderer_->swipeFollow(-52,float(motion_.width.sample(t).position),float(motion_.height.sample(t).position),true,false);return 0;}if(w==67&&testing_){KillTimer(window_,67);shareCard(16,L"Received from Travel laptop",L"Trip notes.pdf",{},8);return 0;}
        if(w==47){controlJob(0,0);return 0;}
        if(w==46){peekTick();return 0;}
        // QA: alternate two currency answers so the rolling digits can be filmed.
        if(w==52){adaptBackdrop();return 0;}
        if(w==53){wallLuma_.reset();loadWallpaperLuma();return 0;}
        if(w==49){KillTimer(window_,49);if(!qaLater_.empty()){privacyUses_=qaLater_;content_.privacy=privacyUses_;showPrivacyNotice(privacyUses_.front());}return 0;}
        if(w==44){auto& c=content_.command;if(c.active){c.text=c.text==L"250 eur to jpy"?L"987 eur to jpy":L"250 eur to jpy";c.caret=c.text.size();commandQuery();refresh();}return 0;}
                if(w==16){
            DwmFlush();captureWindow(window_,store_.directory/(L"motion-"+std::to_wstring(motionStudyStep_)+L".png"));
            if(motionStudyStep_==0||motionStudyStep_==2){auto old=content_.playback.artwork;if(old){auto art=std::make_shared<Artwork>(*old);for(size_t i=0;i<art->pixels.size();i+=4){std::swap(art->pixels[i],art->pixels[i+2]);art->pixels[i+1]=BYTE((art->pixels[i+1]+77)%256);}content_.playback.artwork=art;refresh();animate();}}
            if(motionStudyStep_==4)feedback(Action::Play,0,0,true);
            if(motionStudyStep_==5){feedback(Action::Play);transition(IslandState::Compact);}
            if(motionStudyStep_==6)transition(IslandState::Expanded);
            if(motionStudyStep_==8)SetTimer(window_,16,900,nullptr);
            if(++motionStudyStep_>=10){KillTimer(window_,16);DestroyWindow(qaMatte_);qaMatte_=nullptr;UnregisterClassW(L"NexusIsland.QAMatte",instance_);DeleteObject(qaBrush_);qaBrush_=nullptr;PostMessageW(window_,WM_CLOSE,0,0);}return 0;
        }
        if(w==15){auto previous=content_.playback.artwork;if(previous){auto art=std::make_shared<Artwork>(*previous);for(size_t i=0;i<art->pixels.size();i+=4){std::swap(art->pixels[i],art->pixels[i+2]);art->pixels[i+1]=BYTE((art->pixels[i+1]+45)%256);}content_.playback.artwork=art;refresh();animate();}if(++handoffStep_>=12){KillTimer(window_,15);store_.log("Info","qa_rapid_artwork_handoff_completed");}}
        if(w==18){refresh();}if(w==20){KillTimer(window_,20);store_.save(settings_,settingsFile_);if(!testing_)saveDisplays();}if(w==21){settingsTestStep();return 0;}if(w==23){autoHideTick();return 0;}if(w==22){SpectrumFrame f;f.resting=false;double t=seconds();for(int i=0;i<Spectrum::bandCount;++i)f.bands[i]=float(.5+.45*std::sin(t*7+i*.7));renderer_->spectrum(f);return 0;}if(w==10){KillTimer(window_,10);if(system_&&systemRequested_)system_->setActive(true);}
        if(w==13){
            KillTimer(window_,13);bool pass=true;
            switch(scenarioStep_++){
            case 0:{GetCursorPos(&qaCursor_);POINT p{int(Renderer::canvasWidth/2*dpi_/96),int(18*dpi_/96)};ClientToScreen(window_,&p);SetCursorPos(p.x,p.y);}SendMessageW(window_,WM_MOUSEMOVE,0,MAKELPARAM(int(Renderer::canvasWidth/2*dpi_/96),int(18*dpi_/96)));SetTimer(window_,13,600,nullptr);return 0;
            case 1:pass=state_==IslandState::Expanded&&motion_.width.sample(seconds()).position>400;SetCursorPos(qaCursor_.x,qaCursor_.y+100);SendMessageW(window_,WM_MOUSELEAVE,0,0);SetTimer(window_,13,900,nullptr);break;
            case 2:pass=state_==IslandState::Compact;settings_.hoverOpen=false;{POINT p{int(Renderer::canvasWidth/2*dpi_/96),int(18*dpi_/96)};ClientToScreen(window_,&p);SetCursorPos(p.x,p.y);}SendMessageW(window_,WM_MOUSEMOVE,0,MAKELPARAM(int(Renderer::canvasWidth/2*dpi_/96),int(18*dpi_/96)));SetTimer(window_,13,500,nullptr);break;
            case 3:pass=state_==IslandState::Compact;perform(Action::Focus);perform(Action::Timer5);perform(Action::TimerToggle);pass=pass&&content_.page==Page::Focus&&content_.focus.running;perform(Action::TimerReset);perform(Action::System);pass=pass&&content_.page==Page::System;perform(Action::Settings);pass=pass&&settingsWindow_&&settingsWindow_->open();settingsWindow_.reset();perform(Action::Close);pass=pass&&state_==IslandState::Compact;SetTimer(window_,13,100,nullptr);break;
            case 4:perform(Action::Edge);perform(Action::Scale);pass=settings_.edge==1&&settings_.scale==110;perform(Action::Theme);perform(Action::GlassToggle);perform(Action::Shelf);SetTimer(window_,13,200,nullptr);break;
            case 5:{auto data=shelfData({ShelfItem::Kind::Text,L"QA note",L"QA note"});DWORD effect=DROPEFFECT_COPY;dropTarget_->DragEnter(data.Get(),MK_LBUTTON,{0,0},&effect);dropTarget_->Drop(data.Get(),0,{0,0},&effect);pass=effect==DROPEFFECT_COPY&&content_.shelf.size()==1&&content_.shelf.front().value==L"QA note";perform(Action::ShelfClear);pass=pass&&content_.shelf.empty();perform(Action::Close);SetTimer(window_,13,100,nullptr);break;}
            case 6:{perform(Action::Overview);content_.layoutSlot=0;auto first=settings_.navigation[0];perform(Action::LayoutRight);pass=content_.layoutSlot==1&&settings_.navigation[1]==first&&validNavigation(settings_.navigation);perform(Action::MetricOne);pass=pass&&settings_.homeMetrics[0]!=settings_.homeMetrics[1]&&settings_.homeMetrics[0]!=settings_.homeMetrics[2];SetTimer(window_,13,650,nullptr);break;}
            case 7:pass=renderer_->hit(20+float(navStep)+14,38+navY+12)==Action::Overview;perform(Action::LayoutReset);pass=pass&&settings_.navigation==defaultNavigation&&settings_.homeMetrics==defaultMetrics;perform(Action::Rings);perform(Action::IconsToggle);perform(Action::HandoffToggle);pass=pass&&settings_.glanceRings==0&&!settings_.animatedIcons&&!settings_.trackHandoff;perform(Action::Close);SetTimer(window_,13,100,nullptr);break;
            case 8:content_.pinned=false;transition(IslandState::Expanded);yieldToApp();pass=state_==IslandState::Compact;content_.pinned=true;transition(IslandState::Expanded);yieldToApp();pass=pass&&state_==IslandState::Expanded;content_.pinned=false;settings_.collapseOnAppSwitch=false;yieldToApp();pass=pass&&state_==IslandState::Expanded;settings_.collapseOnAppSwitch=true;content_.dropHover=true;yieldToApp();pass=pass&&state_==IslandState::Expanded;content_.dropHover=false;yieldToApp();pass=pass&&state_==IslandState::Compact;SetTimer(window_,13,100,nullptr);break;
            case 9:{perform(Action::Media);content_.playback.canSeek=true;content_.playback.duration=content_.playback.seekMax=240;content_.playback.seekMin=0;refresh();auto point=[&](float x,float y){auto origin=bodyAt(motion_.width.sample(seconds()).position,motion_.height.sample(seconds()).position,motion_.drop.sample(seconds()).position);return MAKELPARAM(int((origin.x+x)*dpi_/96),int((origin.y+y)*dpi_/96));};scrubAt(point(210,229),true);pass=content_.scrub.active&&std::abs(content_.scrub.value-120)<1;auto initial=content_.scrub.value;scrubAt(point(248,310));pass=pass&&content_.scrub.value>initial&&content_.scrub.value<initial+4;endScrub(false);pass=pass&&!content_.scrub.active&&renderer_->hit(210,228)==Action::Seek;perform(Action::Close);SetTimer(window_,13,100,nullptr);break;}
            case 10:{settings_.edge=0;settings_.uiMode=0;applySettings(true);transition(IslandState::Compact);SendMessageW(window_,WM_LBUTTONDOWN,0,MAKELPARAM(int(Renderer::canvasWidth/2),18));SendMessageW(window_,WM_LBUTTONUP,0,MAKELPARAM(int(Renderer::canvasWidth/2),18));pass=state_==IslandState::Compact&&!content_.pinned&&motion_.width.target()<=76;transition(IslandState::LiveActivity);pass=pass&&content_.live&&motion_.height.target()==154&&renderer_->hit(30,120)==Action::Overview;content_.hovered=Action::Overview;interaction_=InteractionState::Hover;SendMessageW(window_,WM_TIMER,19,0);pass=pass&&state_==IslandState::Expanded&&!content_.live;pass=pass&&!content_.live;settings_.compactWidth=560;applySettings();perform(Action::Close);pass=pass&&state_==IslandState::Compact;SetTimer(window_,13,100,nullptr);break;}
            case 11:{settings_.uiMode=1;applySettings();pass=motion_.width.target()>=560;perform(Action::WidthDown);pass=pass&&settings_.compactWidth==520;perform(Action::CompactVolumeToggle);perform(Action::CompactClockToggle);pass=pass&&!settings_.compactVolume&&settings_.compactClock;perform(Action::ShelfPeekToggle);pass=pass&&!settings_.shelfPeek;SetTimer(window_,13,100,nullptr);break;}
            case 12:{settings_.autoHide=true;settings_.collapseDelay=300;content_.pinned=false;interaction_=InteractionState::Rest;transition(IslandState::Compact);autoHide_=AutoHide{};
                MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);SetCursorPos((mi.rcMonitor.left+mi.rcMonitor.right)/2,(mi.rcMonitor.top+mi.rcMonitor.bottom)/2);autoHideTick();pass=!autoHide_.hidden;SetTimer(window_,13,500,nullptr);break;}
            case 13:{autoHideTick();pass=autoHide_.hidden&&motion_.slide.target()==1;RECT w{};GetWindowRect(window_,&w);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);
                SetCursorPos((w.left+w.right)/2,mi.rcMonitor.top+6);autoHideTick();pass=pass&&autoHide_.hidden;
                SetCursorPos((w.left+w.right)/2,mi.rcMonitor.top);autoHideTick();pass=pass&&!autoHide_.hidden&&motion_.slide.target()==0;SetTimer(window_,13,900,nullptr);break;}
            case 14:{RECT box{};pass=GetWindowRgnBox(window_,&box)!=NULLREGION&&box.bottom-box.top>int(20*dpi_/96);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);
                // The reveal at the top pixel is also a hover over the attached island, which opens it; the pointer
                // has now left, so the island settles back to rest before the away timer is measured.
                SetCursorPos((mi.rcMonitor.left+mi.rcMonitor.right)/2,(mi.rcMonitor.top+mi.rcMonitor.bottom)/2);KillTimer(window_,7);interaction_=InteractionState::Rest;content_.pinned=false;transition(IslandState::Compact);autoHideTick();SetTimer(window_,13,1400,nullptr);break;}
            case 15:autoHideTick();SetTimer(window_,13,settings_.collapseDelay+500,nullptr);return 0;
            case 16:{autoHideTick();SendMessageW(window_,WM_TIMER,SettleTimer,0);RECT box{};int kind=GetWindowRgnBox(window_,&box);pass=autoHide_.hidden&&(kind==NULLREGION||box.bottom<=0||box.top>=box.bottom);settings_.autoHide=false;SetTimer(window_,13,100,nullptr);break;}
            // Phase 5B: double-click skips on the artwork halves, arrow keys on the timeline, seek detents.
            // A made-up track id and title, so the real player never receives these seeks.
            case 17:{perform(Action::Media);auto& p=content_.playback;p.available=true;p.canSeek=true;p.playing=false;p.id=0xffffffffffffull;p.title=L"UI test track";p.source=L"qa.test";p.kind=MediaKind::Music;settings_.mediaLayout=0;p.duration=p.seekMax=240;p.seekMin=0;p.position=100;p.sampledAt=seconds();refresh();
                auto point=[&](float x,float y){auto origin=bodyAt(motion_.width.sample(seconds()).position,motion_.height.sample(seconds()).position,motion_.drop.sample(seconds()).position);return MAKELPARAM(int((origin.x+x)*dpi_/96),int((origin.y+y)*dpi_/96));};
                auto twice=[&](float x,float y){skipClickAction_=Action::None;for(int i=0;i<2;++i){SendMessageW(window_,WM_LBUTTONDOWN,0,point(x,y));SendMessageW(window_,WM_LBUTTONUP,0,point(x,y));}};
                twice(95,132);pass=std::abs(p.position-110)<.01;twice(45,132);pass=pass&&std::abs(p.position-100)<.01;
                content_.hovered=Action::Seek;SendMessageW(window_,WM_KEYDOWN,VK_RIGHT,0);pass=pass&&std::abs(p.position-110)<.01;content_.hovered=Action::None;
                scrubAt(point(20+95+2.5f,229),true);pass=pass&&content_.scrub.active&&content_.scrub.value==60;scrubAt(point(20+95+30,229));pass=pass&&content_.scrub.value>62;endScrub(false);
                p.position=4;p.sampledAt=seconds();twice(45,132);pass=pass&&p.position==0;perform(Action::Close);SetTimer(window_,13,100,nullptr);break;}
            // Phase 5C: the Shelf item view (open, back, remove) and clipboard search with a pin.
            case 18:{perform(Action::Shelf);content_.shelfTab=0;content_.shelf={{ShelfItem::Kind::Text,L"UI test note",L"UI test note"},{ShelfItem::Kind::Text,L"Second note",L"Second note"}};refresh();
                perform(Action::ShelfItemBase);pass=content_.shelfDetail==0;perform(Action::ShelfBack);pass=pass&&content_.shelfDetail==-1;
                perform(Action(int(Action::ShelfItemBase)+1));perform(Action::ShelfRemove);pass=pass&&content_.shelf.size()==1&&content_.shelf[0].value==L"UI test note"&&content_.shelfDetail==-1;
                const bool history=settings_.clipboardHistory;settings_.clipboardHistory=true;clips_.forget();{ClipEntry a;a.text=L"alpha one";clips_.add(a,seconds());ClipEntry b;b.text=L"beta two";clips_.add(b,seconds());}
                clipSearch(false);pass=pass&&content_.command.active&&content_.command.clips&&content_.command.results.size()==2;for(wchar_t ch:std::wstring(L"beta"))commandChar(ch);pass=pass&&content_.command.results.size()==1&&content_.command.results[0].title==L"beta two";
                closeCommand(false);clipViews();perform(Action::ShelfClipboard);perform(Action::ClipPinBase);pass=pass&&clips_.pinned()==1&&content_.clips.front().pinned;
                clips_.forget();clipViews();settings_.clipboardHistory=history;content_.shelf.clear();perform(Action::Close);SetTimer(window_,13,100,nullptr);break;}
            // Phase 5D: glass is attached with shoulders (its input region includes them); a lyric line is a seek target.
            case 19:settings_.material=1;settings_.verticalOffset=0;applySettings(false,true);transition(IslandState::Compact);SetTimer(window_,13,900,nullptr);break;
            // The shoulders make the region's top row wider than its middle by nearly four corner radii, whatever the pose.
            case 20:{RECT box{};GetWindowRgnBox(window_,&box);const double r=motion_.radius.sample(seconds()).position,scale=dpi_/96;
                HRGN shape=CreateRectRgn(0,0,0,0);GetWindowRgn(window_,shape);auto span=[&](int y){int n=0;for(int x=box.left;x<box.right;++x)n+=PtInRegion(shape,x,y)?1:0;return n;};
                const int topRow=span(box.top+2),middleRow=span((box.top+box.bottom)/2);DeleteObject(shape);
                pass=!settings_.floating()&&(!renderer_->glassAvailable()||renderer_->glassStyle().visible)&&topRow>middleRow+int(2*r*scale);
                perform(Action::Media);auto& p=content_.playback;p.available=true;p.canSeek=true;p.playing=false;p.id=0xfffffffffffeull;p.title=L"UI test song";p.artist=L"QA";p.source=L"qa.test";p.kind=MediaKind::Music;settings_.mediaLayout=0;p.duration=p.seekMax=240;p.seekMin=0;p.position=72;p.sampledAt=seconds();
                const bool lyricsWere=settings_.lyrics;settings_.lyrics=true;content_.settings=settings_;content_.lyricsView=true;content_.lyrics=std::make_shared<std::vector<LyricLine>>(parseLrc(L"[01:10.00]First line\n[01:16.50]Second line\n[01:22.00]Third line"));content_.lyricsState=int(LyricsService::State::Found);tickLyrics(false);refresh();
                pass=pass&&content_.lyricLine==0&&std::any_of(renderer_->targets.begin(),renderer_->targets.end(),[](auto& t){return t.action==Action(int(Action::LyricLineBase)+3);});
                perform(Action(int(Action::LyricLineBase)+3));pass=pass&&std::abs(p.position-76.5)<.01&&content_.lyricLine==1;
                settings_.lyrics=lyricsWere;content_.settings=settings_;content_.lyrics=nullptr;content_.lyricLine=-1;perform(Action::Close);SetTimer(window_,13,100,nullptr);break;}
            // Command bar v2: a ghost completion Tab accepts, a live system row, and a second Enter before locking.
            // Enter is pressed exactly once, so the PC is never actually locked.
            case 21:openCommand();for(wchar_t ch:std::wstring(L"dark mo"))commandChar(ch);SetTimer(window_,13,700,nullptr);break;
            case 22:pass=content_.command.active&&!content_.command.results.empty()&&ghostSuffix(content_.command.text,content_.command.results[0].completion)==L"de";commandKey(VK_TAB);SetTimer(window_,13,700,nullptr);break;
            case 23:{auto& c=content_.command;pass=c.text==L"dark mode"&&!c.results.empty()&&c.results[0].kind==CommandKind::DarkMode&&(c.results[0].value==0||c.results[0].value==1);
                c.text.clear();c.caret=0;for(wchar_t ch:std::wstring(L"lock"))commandChar(ch);SetTimer(window_,13,700,nullptr);break;}
            case 24:{auto& c=content_.command;pass=!c.results.empty()&&c.results[0].kind==CommandKind::Lock&&c.results[0].confirm;
                if(pass){commandKey(VK_RETURN);pass=c.armed&&c.status.starts_with(L"Press Enter again");}closeCommand(false);SetTimer(window_,13,500,nullptr);break;}
            // Space between words: with results showing and the top row highlighted (as after the arrow keys or the
            // pointer), a real key-down and character, as the keyboard sends them, add a space and run nothing.
            // The row is a harmless timer: were it run, the bar would close and this stage fail.
            case 25:openCommand();for(wchar_t ch:std::wstring(L"timer 10"))commandChar(ch);SetTimer(window_,13,700,nullptr);break;
            case 26:{auto& c=content_.command;pass=c.active&&!c.results.empty()&&c.results[0].kind==CommandKind::Timer;content_.hovered=Action(int(Action::CommandResultBase));
                SendMessageW(window_,WM_KEYDOWN,VK_SPACE,0);SendMessageW(window_,WM_CHAR,L' ',0);SendMessageW(window_,WM_KEYUP,VK_SPACE,0);for(wchar_t ch:std::wstring(L"min"))commandChar(ch);SetTimer(window_,13,700,nullptr);break;}
            case 27:{auto& c=content_.command;pass=c.active&&c.text==L"timer 10 min"&&c.caret==12&&!c.armed;closeCommand(false);SetTimer(window_,13,500,nullptr);break;}
            // Phase 5F: the compact media controls are hit where they are drawn. Only the mapping is checked
            // (hit(), no click), with a made-up track, so the real player is never paused or skipped.
            // (The auto-hide stages leave the island slid away; these start from it shown.)
            case 28:{settings_.autoHide=false;autoHide_=AutoHide{};motion_.slide.reset(0,seconds());settings_.edge=0;settings_.uiMode=1;settings_.compactWidth=560;settings_.compactControls=true;settings_.compactMedia=true;applySettings(false,true);
                auto& p=content_.playback;p.available=true;p.playing=true;p.canToggle=p.canNext=p.canPrevious=true;p.id=0xfffffffffffcull;p.title=L"UI test song";p.source=L"qa.test";p.kind=MediaKind::Music;content_.activity.clear();transition(IslandState::Compact);refresh();SetTimer(window_,13,900,nullptr);break;}
            case 29:{auto at=[&](Action a)->LPARAM{for(auto& t:renderer_->compactTargets)if(t.action==a){const double now=seconds(),w=motion_.width.sample(now).position;auto o=bodyAt(w,motion_.height.sample(now).position,0);
                    return MAKELPARAM(int((o.x+(w-motion_.compactWidth)/2+t.x+t.width/2)*dpi_/96),int((o.y+t.y+t.height/2)*dpi_/96));}return MAKELPARAM(-1000,-1000);};
                pass=state_==IslandState::Compact&&hit(at(Action::Previous))==Action::Previous&&hit(at(Action::Play))==Action::Play&&hit(at(Action::Next))==Action::Next;
                // The Controls page lays out its six tiles and both sliders (nothing is toggled).
                perform(Action::Control);pass=pass&&content_.page==Page::Control;auto has=[&](Action a){return std::any_of(renderer_->targets.begin(),renderer_->targets.end(),[&](auto& t){return t.action==a;});};
                for(Action a:{Action::ControlWifi,Action::ControlBluetooth,Action::ControlAirplane,Action::ControlDark,Action::ControlFocus,Action::ControlMic,Action::VolumeSlider,Action::ControlBrightness})pass=pass&&has(a);
                content_.playback=MediaSnapshot{};perform(Action::Close);SetTimer(window_,13,500,nullptr);break;}
            // A notification drops out as its own pill: the input region covers the docked stub and the pill, never the gap between them.
            case 30:{settings_.notifyStyle=1;settings_.edgeSplash=true;applySettings();content_.pinned=false;shareCard(16,L"UI test card",L"Nothing was shared",{},4);
                pass=state_==IslandState::Notification&&motion_.drop.target()==1&&motion_.stubWidth<=196;SetTimer(window_,13,1200,nullptr);break;}
            case 31:{SendMessageW(window_,WM_TIMER,SettleTimer,0);HRGN shape=CreateRectRgn(0,0,0,0);GetWindowRgn(window_,shape);const double s=dpi_/96,cx=Renderer::canvasWidth/2;
                auto in=[&](double x,double y){return PtInRegion(shape,int(x*s),int(y*s))!=FALSE;};
                const double gapY=motion_.stubHeight+(dropDistance-motion_.stubHeight)/2,beside=motion_.stubWidth/2+40;
                pass=in(cx,motion_.stubHeight/2)&&in(cx,dropDistance+motion_.height.target()/2)&&!in(cx-beside,gapY)&&!in(cx+beside,gapY)&&!in(cx-beside,4);DeleteObject(shape);
                events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);pass=pass&&motion_.drop.target()==0;SetTimer(window_,13,500,nullptr);break;}
            // The Shelf's Nearby tab (sharing on, illustrative PCs, no network in a test run): Pair and Forget targets, choosing where Send goes.
            case 32:{const bool was=settings_.sharing;settings_.sharing=true;content_.settings=settings_;content_.nearby={{"a1",L"Test PC one",true,true},{"b2",L"Test PC two",false,true}};shareTarget_.clear();content_.nearbyTarget.clear();
                perform(Action::Shelf);perform(Action::ShelfNearby);auto has=[&](Action a){return std::any_of(renderer_->targets.begin(),renderer_->targets.end(),[&](auto& t){return t.action==a;});};
                pass=content_.shelfTab==2&&has(Action::NearbyBase)&&has(Action(int(Action::NearbyBase)+1))&&has(Action::NearbyForgetBase)&&!has(Action(int(Action::NearbyForgetBase)+1));
                perform(Action::NearbyBase);pass=pass&&shareTarget_=="a1"&&content_.nearbyTarget=="a1";
                settings_.sharing=was;content_.settings=settings_;content_.nearby.clear();shareTarget_.clear();content_.nearbyTarget.clear();content_.shelfTab=0;perform(Action::Close);SetTimer(window_,13,300,nullptr);break;}
            // Now Playing over a fullscreen app (as if one had hidden the island, with a made-up track): touching the
            // screen's top edge above the island shows it; 0.8 s after the pointer leaves, it hides again.
            case 33:{transition(IslandState::Compact);auto& p=content_.playback;p.available=true;p.playing=true;p.id=0xfffffffffffbull;p.title=L"UI test song";p.source=L"qa.test";
                // The stage stands in for a fullscreen app, so the real foreground (another app moving meanwhile) is not asked.
                qaFullscreen_=true;fullscreenHidden_=true;ShowWindow(window_,SW_HIDE);RECT w{};GetWindowRect(window_,&w);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);
                SetCursorPos((w.left+w.right)/2,mi.rcMonitor.top);peekTick();pass=peeking_&&IsWindowVisible(window_);
                SetCursorPos((mi.rcMonitor.left+mi.rcMonitor.right)/2,(mi.rcMonitor.top+mi.rcMonitor.bottom)/2);KillTimer(window_,7);interaction_=InteractionState::Rest;syncPeek();SetTimer(window_,13,1500,nullptr);break;}
            case 34:{interaction_=InteractionState::Rest;peekTick();pass=!peeking_&&!IsWindowVisible(window_);qaFullscreen_=false;
                fullscreenHidden_=false;peeking_=false;syncPeek();ShowWindow(window_,SW_SHOWNOACTIVATE);content_.playback=MediaSnapshot{};refresh();SetTimer(window_,13,400,nullptr);break;}
            // Phase 5G: a real sideways drag across the Live Island skips a track (it only switched sessions before);
            // a short one springs back without skipping. The made-up track never reaches a real player.
            case 35:{settings_.swipeSkip=true;auto& p=content_.playback;p.available=true;p.playing=true;p.canToggle=p.canNext=p.canPrevious=true;p.id=0xfffffffffffaull;p.title=L"UI test song";p.source=L"qa.test";
                content_.activity.clear();transition(IslandState::LiveActivity);refresh();SetTimer(window_,13,800,nullptr);break;}
            case 36:{pass=state_==IslandState::LiveActivity&&mediaSwipe();const unsigned before=skips_;
                const double now=seconds(),w=motion_.width.sample(now).position,h=motion_.height.sample(now).position,s=dpi_/96;auto o=bodyAt(w,h,0);RECT wr{};GetWindowRect(window_,&wr);
                const int cx=int((o.x+w*.6)*s),cy=int((o.y+h*.3)*s);
                auto drag=[&](double dips){SetCursorPos(wr.left+cx,wr.top+cy);SendMessageW(window_,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(cx,cy));
                    for(int k=1;k<=4;++k){const int x=cx-int(k*dips/4*s);SetCursorPos(wr.left+x,wr.top+cy);SendMessageW(window_,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,cy));}
                    SendMessageW(window_,WM_LBUTTONUP,0,MAKELPARAM(cx-int(dips*s),cy));};
                drag(80);pass=pass&&skips_==before+1&&lastSkip_==1;drag(-80);pass=pass&&skips_==before+2&&lastSkip_==-1;drag(20);pass=pass&&skips_==before+2;
                SetCursorPos(qaCursor_.x,qaCursor_.y);content_.playback=MediaSnapshot{};transition(IslandState::Compact);SetTimer(window_,13,500,nullptr);break;}
            // Dragging files over the island with a PC paired shows where they can go; the zone under the pointer is found.
            case 37:{settings_.sharing=true;content_.settings=settings_;content_.nearby={{"a1",L"Test PC one",true,true},{"b2",L"Test PC two",false,true}};
                content_.page=Page::Shelf;content_.shelfTab=0;content_.shelfDetail=-1;content_.dropHover=true;content_.pinned=true;transition(IslandState::Expanded);refresh();SetTimer(window_,13,900,nullptr);break;}
            case 38:{auto target=[&](Action a){return std::find_if(renderer_->targets.begin(),renderer_->targets.end(),[&](auto& t){return t.action==a&&t.enabled;});};
                pass=target(Action::DropShelf)!=renderer_->targets.end()&&target(Action::NearbyBase)!=renderer_->targets.end()&&target(Action(int(Action::NearbyBase)+1))==renderer_->targets.end();
                if(pass){const auto t=*target(Action::NearbyBase);const double now=seconds(),w=motion_.width.sample(now).position,h=motion_.height.sample(now).position,s=dpi_/96;auto o=bodyAt(w,h,0);RECT wr{};GetWindowRect(window_,&wr);
                    const POINT at{LONG(wr.left+(o.x+20+t.x+t.width/2)*s),LONG(wr.top+(o.y+38+t.y+t.height/2)*s)};pass=dropZoneAt(at)==Action::NearbyBase;}
                content_.dropHover=false;content_.dropZone=Action::None;settings_.sharing=false;content_.settings=settings_;content_.nearby.clear();content_.pinned=false;perform(Action::Close);SetTimer(window_,13,500,nullptr);break;}
            // Two alerts at once: the second waits below the first as a bud (the same alert again updates in place), is clickable
            // where it is drawn, and takes the pill's place when the first ends.
            case 39:{settings_.notifyStyle=1;settings_.stackAlerts=true;settings_.edge=0;heldCards_.clear();transition(IslandState::Compact);
                shareCard(16,L"UI test first",L"One",{},30);shareCard(16,L"UI test second",L"Two",{},30);
                pass=heldCards_.size()==1&&content_.notice.app==L"UI test first"&&content_.bud.kind==16&&content_.bud.title==L"UI test second"&&motion_.bud.target()==1;
                shareCard(16,L"UI test first",L"One",{},30);pass=pass&&heldCards_.size()==1;SetTimer(window_,13,1200,nullptr);break;}
            case 40:{const double t=seconds(),s=dpi_/96;auto o=bodyAt(motion_.width.sample(t).position,motion_.height.sample(t).position,motion_.drop.sample(t).position);
                const auto bud=budShape(motion_.bud.sample(t).position,o.y+motion_.height.sample(t).position,Renderer::canvasWidth/2,motion_.budWidth,motion_.budHeight);
                pass=hit(MAKELPARAM(int(Renderer::canvasWidth/2*s),int((bud.top+bud.bottom)/2*s)))==Action::BudPromote;
                events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);
                pass=pass&&content_.notice.app==L"UI test second"&&heldCards_.empty()&&state_==IslandState::Notification;
                events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);pass=pass&&state_==IslandState::Compact;SetTimer(window_,13,500,nullptr);break;}
            // The Media page offers the library and continuing on a paired PC while music plays, and the library with nothing playing.
            case 41:{settings_.musicLibrary=true;settings_.sharing=true;settings_.handoff=true;content_.settings=settings_;content_.nearby={{"a1",L"Test PC one",true,true}};
                auto& p=content_.playback;p.available=true;p.playing=true;p.canToggle=p.canNext=p.canPrevious=true;p.id=0xfffffffffff9ull;p.title=L"UI test song";p.source=L"qa.test";
                content_.library=false;content_.page=Page::Media;content_.pinned=true;transition(IslandState::Expanded);refresh();
                auto has=[&](Action a){return std::any_of(renderer_->targets.begin(),renderer_->targets.end(),[&](auto& t){return t.action==a;});};
                pass=has(Action::LibraryOpen)&&has(Action::HandoffOpen);content_.playback=MediaSnapshot{};refresh();pass=pass&&has(Action::LibraryShuffle)&&has(Action::LibraryOpen)&&!has(Action::HandoffOpen);
                settings_.sharing=false;content_.settings=settings_;content_.nearby.clear();content_.pinned=false;perform(Action::Close);break;}
            }
            if(!pass||scenarioStep_==42){SetCursorPos(qaCursor_.x,qaCursor_.y);auto result=pass?"PASS native hover open, leave close, disabled hover, navigation, timer actions, hit targets, edge/scale/theme, OLE drop and shelf clear, navigation reorder, metric choices, detail toggles, app-switch collapse, pin and drag protection, precision seeking and cancel, hover-only surface, mini/live modes, wide compact settings, auto-hide tucks away (click-through region) and reveals only at its edge (visible region), double-click and arrow-key skips and seek detents, Shelf item view and clipboard search with pins, attached glass with shoulders, lyric tap-to-seek, ghost completion with Tab, live system rows, a second Enter before locking and Space between words, compact media controls, Controls page, drop pill region, Nearby tab, fullscreen peek, Live Island drag skips, drop zones for paired PCs, two alerts at once, Library and Continue on":"FAIL native interaction regression";store_.submit([dir=store_.directory,result,step=scenarioStep_,state=int(state_),interaction=int(interaction_),width=motion_.width.sample(seconds()).position]{std::ofstream(dir/L"ui-test.txt")<<"stage "<<step<<" state "<<state<<" interaction "<<interaction<<" width "<<width<<": "<<result<<'\n';});PostMessageW(window_,WM_CLOSE,0,0);}return 0;
        }
        if(w==34){KillTimer(window_,34);perform(Action::Media);}// QA: page change 100 ms before the capture
        if(w==19){KillTimer(window_,19);if(content_.live&&content_.hovered==Action::Overview&&interaction_==InteractionState::Hover)perform(Action::Overview);}
        // Over the compact controls the island stays compact, so they can be clicked.
        if(w==7){KillTimer(window_,7);if(state_==IslandState::Compact&&content_.hovered!=Action::None)return 0;if(settings_.autoHide&&!pointerOffEdge()){edgeHold_=true;return 0;}if(settings_.hoverOpen&&interaction_==InteractionState::Hover&&state_==IslandState::Compact)transition(settings_.uiMode==2?IslandState::Expanded:IslandState::LiveActivity);}
        if(w==8){KillTimer(window_,8);if(interaction_==InteractionState::Rest&&!content_.pinned)transition(IslandState::Compact);}
        if(w==9){if(content_.focus.tick(seconds())){content_.page=Page::Focus;events_.publish({ActivityKind::Timer,"timer",70,0,2,8},seconds());presentActivity();}if(IsWindowVisible(window_))refresh();clockTimer();}
        if(w==5){
            KillTimer(window_,5);
            WNDCLASSW matteClass{};matteClass.lpfnWndProc=[](HWND h,UINT m,WPARAM w,LPARAM l)->LRESULT{if(m==WM_PAINT&&GetPropW(h,L"pattern")){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);const COLORREF colors[]={RGB(255,94,98),RGB(255,184,76),RGB(84,214,160),RGB(76,146,255),RGB(170,110,255),RGB(245,245,245),RGB(28,32,40)};int band=std::max<LONG>(1,r.right/14);for(int i=0;i*band<r.right;++i){RECT b{i*band,0,(i+1)*band,r.bottom};HBRUSH brush=CreateSolidBrush(colors[i%7]);FillRect(dc,&b,brush);DeleteObject(brush);}EndPaint(h,&ps);return 0;}return DefWindowProcW(h,m,w,l);};matteClass.hInstance=instance_;matteClass.hbrBackground=CreateSolidBrush(RGB(34,40,50));matteClass.lpszClassName=L"NexusIsland.QAMatte";RegisterClassW(&matteClass);
            RECT r{};GetWindowRect(window_,&r);HWND matte=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,matteClass.lpszClassName,L"Nexus QA matte",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance_,nullptr);
            qaMatte_=matte;qaBrush_=matteClass.hbrBackground;if(showcasePattern_){SetPropW(matte,L"pattern",HANDLE(1));InvalidateRect(matte,nullptr,TRUE);}
            ShowWindow(matte,SW_SHOWNOACTIVATE);SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);syncShadow();UpdateWindow(matte);SetTimer(window_,6,250,nullptr);
        }
        if(w==6){
            KillTimer(window_,6);DwmFlush();captureWindow(window_,store_.directory/L"island-capture.png");
            if(motionStudy_){SetTimer(window_,16,80,nullptr);return 0;}
            DestroyWindow(qaMatte_);qaMatte_=nullptr;UnregisterClassW(L"NexusIsland.QAMatte",instance_);DeleteObject(qaBrush_);qaBrush_=nullptr;
        }
        if(w==SettleTimer){double t=seconds();if(motion_.width.settled(t)&&motion_.height.settled(t)&&motion_.drop.settled(t)&&motion_.bud.settled(t)&&motion_.dragX.settled(t)&&motion_.dragY.settled(t)&&motion_.slide.settled(t)){updateRegion(false);renderer_->animate(motion_,t);KillTimer(window_,SettleTimer);}else updateRegion(true);}
        if(w==ActivityTimer&&events_.tick(seconds())){if(!events_.active()&&!heldCards_.empty()&&state_==IslandState::Notification)promoteCard();else if(!events_.active()){KillTimer(window_,ActivityTimer);content_.activity.clear();levelIndicator();refresh();if(interaction_==InteractionState::Rest&&!content_.pinned)transition(IslandState::Compact);}else presentActivity();}
        if(w==HudTimer){if(settingsWindow_&&settingsWindow_->open())settingsWindow_->update(settings_,settingsContext(),settingsSequence_);else KillTimer(window_,HudTimer);}
        if(w==36){KillTimer(window_,36);transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);}// Lab: interrupt reversal
        if(w==37)labMeasure();
        if(w==38)tickLyrics();
        if(w==ScenarioTimer){
            ++scenarioStep_;double now=seconds();
            events_.publish({ActivityKind::Volume,"benchmark-volume",40,double(scenarioStep_%101),.1,.3},now);
            motion_.volume.retarget((scenarioStep_%101)/100.,now,{1,550,42});
            transition(scenarioStep_%2?IslandState::Expanded:IslandState::Compact);
            if(scenarioStep_>=200)finishBenchmark();
        }return 0;
    case WM_CLOSE:DestroyWindow(window_);return 0;
    case WM_DESTROY:if(clipHistoryReady_&&settings_.clipboardHistory&&settings_.clipboardKeep&&!testing_)saveClipHistory(true);settingsWindow_.reset();RevokeDragDrop(window_);if(shadow_){DestroyWindow(shadow_);shadow_=nullptr;}PostQuitMessage(0);return 0;
    }
    static UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");if(m==taskbarCreated)Shell_NotifyIconW(NIM_ADD,&tray_);
    return DefWindowProcW(window_,m,w,l);
}
void IslandWindow::showMenu(){
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Expand / collapse");AppendMenuW(menu,MF_STRING,2,L"Animation Lab");AppendMenuW(menu,MF_STRING,3,L"Settings");
   AppendMenuW(menu,MF_STRING|(debug_?MF_CHECKED:0),5,L"Visual bounds");
    AppendMenuW(menu,MF_STRING,6,L"Open local logs");AppendMenuW(menu,MF_STRING,7,L"Clear local logs");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,9,L"Exit Arnav Island");
    POINT p;GetCursorPos(&p);SetForegroundWindow(window_);int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,window_,nullptr);DestroyMenu(menu);PostMessageW(window_,WM_NULL,0,0);
    switch(command){case 1:transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);break;case 2:openSettings(3);break;case 3:perform(Action::Settings);break;
    case 5:debug_=!debug_;renderer_->redraw(content_,debug_);break;
    case 6:ShellExecuteW(nullptr,L"open",store_.directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case 7:store_.clear();break;case 9:PostMessageW(window_,WM_CLOSE,0,0);break;}
}
void IslandWindow::finishBenchmark(){
    KillTimer(window_,ScenarioTimer);FILETIME c,e,k,u;GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);
    auto value=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
    double elapsed=seconds()-benchmarkStart_,cpu=(value(k)+value(u)-value(initialKernel_)-value(initialUser_))/1e7;
    PROCESS_MEMORY_COUNTERS p{sizeof(p)};GetProcessMemoryInfo(GetCurrentProcess(),&p,sizeof(p));
    auto directory=store_.directory;unsigned commits=renderer_->commits,redraws=renderer_->redraws;size_t depth=events_.depth();
    store_.submit([=]{std::ofstream f(directory/L"benchmark.json");f<<"{\n  \"scenario\": \"200 rapid retargets and coalesced volume activities\",\n  \"elapsed_seconds\": "<<elapsed<<",\n  \"cpu_seconds\": "<<cpu<<",\n  \"working_set_bytes\": "<<p.WorkingSetSize<<",\n  \"peak_working_set_bytes\": "<<p.PeakWorkingSetSize<<",\n  \"composition_commits\": "<<commits<<",\n  \"surface_redraws\": "<<redraws<<",\n  \"queued_activities\": "<<depth<<"\n}\n";});
    store_.log("Info","benchmark_completed");PostMessageW(window_,WM_CLOSE,0,0);
}
}

namespace nexus {
void IslandWindow::saveDisplays(){auto profiles=displays_;store_.submit([profiles,dir=store_.directory]{auto path=dir/L"displays.tmp";{std::ofstream f(path);profiles.write(f);f.flush();if(!f)throw std::runtime_error("Display profiles write failed");}if(!MoveFileExW(path.c_str(),(dir/L"displays.nexus").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Display profiles replace failed");});}
void IslandWindow::pushWaveform(float live){
    auto& p=content_.playback;if(!renderer_||state_==IslandState::Compact||content_.live||content_.page!=Page::Media||!(p.duration>0)||!settings_.waveTimeline)return;
    std::array<float,TrackWaveform::bars> heights;heights.fill(-1.f);
    if(auto* w=waves_.find(WaveformLibrary::key(p.title,p.artist,p.duration))){heights=w->heights();
        // The bar under the playhead moves with the music.
        if(live>=0&&p.playing){int i=TrackWaveform::bucket(p.position+std::max(0.,seconds()-p.sampledAt),p.duration);heights[size_t(i)]=std::max(heights[size_t(i)],std::clamp(live,0.f,1.f));}}
    renderer_->waveform(heights,motion_.reduced);
}
void IslandWindow::refresh(){content_.settings=settings_;content_.reducedMotion=motion_.reduced;bool compact=state_==IslandState::Compact;{const double now=seconds();const bool still=motion_.height.settled(now)&&motion_.width.settled(now);renderer_->setRest(!compact&&still&&motion_.reveal.settled(now),compact&&still);}renderer_->redraw(content_,debug_,compact);contentDirty_=compact;if(!compact)pushWaveform();}
void IslandWindow::clockTimer(){bool visible=state_!=IslandState::Compact&&IsWindowVisible(window_);
    // The idle glance in the compact island shows CPU and GPU, so it needs the system provider too.
    const bool glance=state_==IslandState::Compact&&IsWindowVisible(window_)&&!autoHide_.hidden&&settings_.compactGlance&&settings_.uiMode!=0&&settings_.edge==0&&!(settings_.compactMedia&&content_.playback.available)&&!content_.focus.running;
    bool request=(visible&&!content_.live&&(content_.page==Page::Overview||content_.page==Page::System))||glance;if(request!=systemRequested_){systemRequested_=request;if(request)SetTimer(window_,10,400,nullptr);else{KillTimer(window_,10);if(system_)system_->setActive(false);}}if(content_.focus.running||(visible&&(content_.live||content_.page==Page::Media)&&content_.playback.playing))SetTimer(window_,9,1000,nullptr);else KillTimer(window_,9);if(settings_.compactClock&&IsWindowVisible(window_))SetTimer(window_,18,60000,nullptr);else KillTimer(window_,18);updateProviders();
    syncPeek();
    // The Controls page re-reads its switches every two seconds while it shows.
    if(visible&&!content_.live&&content_.page==Page::Control)SetTimer(window_,47,2000,nullptr);else KillTimer(window_,47);
    if(battery_)battery_->setFast((state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==1)||(content_.card&&(content_.notice.kind==3||content_.notice.kind==4)));
    bool hideActive=settings_.autoHide&&IsWindowVisible(window_)&&!testing_;if(hideActive!=autoHideTimer_){autoHideTimer_=hideActive;if(hideActive)SetTimer(window_,23,33,nullptr);else{KillTimer(window_,23);if(autoHide_.hidden||motion_.slide.target()!=0){autoHide_.hidden=false;if(motion_.reduced)motion_.slide.reset(0,seconds());else motion_.slide.retarget(0,seconds(),{.9,420,26});if(renderer_)animate();}}}}
Action IslandWindow::hit(LPARAM l){double now=seconds();const double width=motion_.width.sample(now).position;auto origin=bodyAt(width,motion_.height.sample(now).position,motion_.drop.sample(now).position);
    const float x=float(GET_X_LPARAM(l)*96/dpi_-origin.x-motion_.dragX.sample(now).position),y=float(GET_Y_LPARAM(l)*96/dpi_-origin.y-motion_.dragY.sample(now).position);
    if(budAt(GET_X_LPARAM(l)*96/dpi_,GET_Y_LPARAM(l)*96/dpi_))return Action::BudPromote;
    // Compact: only the media controls, in the header's own coordinates (it is centred in the body).
    if(state_==IslandState::Compact){if(settings_.edge!=0||settings_.uiMode==0)return Action::None;return renderer_->compactHit(x-float((width-motion_.compactWidth)/2),y);}
    return renderer_->hit(x,y-float(motion_.contentShift.sample(now).position));}
// Whether a sideways gesture here skips tracks: over the music in the compact island, the Live Island, Home and the Media page.
bool IslandWindow::mediaSwipe()const{
    const auto& p=content_.playback;if(!settings_.swipeSkip||!p.available||!(p.canNext||p.canPrevious)||content_.command.active||content_.card)return false;
    return state_==IslandState::Compact||content_.live||(state_==IslandState::Expanded&&(content_.page==Page::Media||content_.page==Page::Overview));
}
// A sideways drag let go: direction 1 the next track, -1 the previous, 0 none (it springs back).
void IslandWindow::endSwipe(int direction){
    const double now=seconds();renderer_->swipeEnd(direction!=0,motion_.reduced);
    if(direction)skipTrack(direction);
    else if(state_!=IslandState::Compact)motion_.swipe.retarget(0,now,MotionTokens::content);
    motion_.dragX.retarget(0,now,motion_.body);motion_.dragY.retarget(0,now,motion_.body);animate();
}
// A sideways swipe on the music: the next or previous track. The compact header kicks the way it went; open, the content slides in from there.
void IslandWindow::skipTrack(int direction){
    ++skips_;lastSkip_=direction;auto& p=content_.playback;renderer_->trackSkip(direction,motion_.reduced);
    if(state_!=IslandState::Compact&&!motion_.reduced){const double now=seconds();motion_.swipe.reset(direction>0?26:-26,now);motion_.swipe.retarget(0,now,MotionTokens::content);animate();}
    if(direction>0?p.canNext:p.canPrevious){mediaCommand(direction>0?3:2);store_.log("Info","compact_track_swipe");}
}
// Now Playing over fullscreen apps: while the island is hidden for one, touching its edge shows the
// compact island (with its controls and swipes); it hides again 0.8 s after the pointer leaves.
void IslandWindow::syncPeek(){
    const bool watch=(fullscreenHidden_&&settings_.fullscreenPeek&&content_.playback.available&&!testing_)||peeking_;
    if(watch!=peekTimer_){peekTimer_=watch;if(watch)SetTimer(window_,46,120,nullptr);else KillTimer(window_,46);}
}
void IslandWindow::peekTick(){
    POINT c{};GetCursorPos(&c);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);RECT w{};GetWindowRect(window_,&w);const double s=dpi_/96,now=seconds();
    const double bw=motion_.width.target(),bh=motion_.height.target();auto origin=bodyAt(bw,bh,motion_.drop.target());
    const double left=w.left+origin.x*s,top=w.top+origin.y*s,right=left+bw*s,bottom=top+bh*s,center=settings_.edge?(top+bottom)/2:(left+right)/2,half=(settings_.edge?bh:bw)*s/2+40*s;
    if(!peeking_){
        if(fullscreenHidden_&&content_.playback.available&&atIslandEdge(c.x,c.y,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right,mi.rcMonitor.bottom,settings_.edge,center,half)){
            peeking_=true;peekIdle_=0;ShowWindow(window_,SW_SHOWNOACTIVATE);SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);syncShadow();clockTimer();refresh();animate();store_.log("Info","fullscreen_peek");}
        return;}
    if(!fullscreenHidden_){peeking_=false;syncPeek();return;}
    const bool over=WindowFromPoint(c)==window_;
    if(over||state_!=IslandState::Compact||interaction_!=InteractionState::Rest){peekIdle_=0;return;}
    if(peekIdle_==0){peekIdle_=now;return;}
    if(now-peekIdle_>.8){peeking_=false;ShowWindow(window_,SW_HIDE);syncShadow();clockTimer();}
}
// Adaptive text: the wallpaper under the island's monitor, decoded on a worker (again when it changes, and every ten minutes for slideshows).
void IslandWindow::loadWallpaperLuma(){
    if(!window_||wallLoading_||!settings_.adaptiveText||settings_.material!=2||qaBackdrop_)return;
    MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTOPRIMARY),&info))return;
    wallLoading_=true;std::thread([window=window_,monitor=info.rcMonitor]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);auto map=new std::shared_ptr<WallpaperLuma>(wallpaperLuma(monitor));
        if(!PostMessageW(window,WallpaperLumaMessage,0,reinterpret_cast<LPARAM>(map)))delete map;CoUninitialize();}).detach();
}
// Whether any other window overlaps the compact island (then what is behind it is not the wallpaper).
bool IslandWindow::backdropCovered()const{
    RECT r{};GetWindowRect(window_,&r);const double s=dpi_/96,w=motion_.compactWidth;
    struct Search{RECT island;HWND self,shadow;bool hit=false;} search{{LONG(r.left+(Renderer::canvasWidth-w)/2*s),r.top,LONG(r.left+(Renderer::canvasWidth+w)/2*s),LONG(r.top+34*s)},window_,shadow_};
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& c=*reinterpret_cast<Search*>(p);if(h==c.self||h==c.shadow||!IsWindowVisible(h)||IsIconic(h))return TRUE;
        if(GetWindowLongW(h,GWL_EXSTYLE)&WS_EX_TRANSPARENT)return TRUE;BOOL cloaked=FALSE;DwmGetWindowAttribute(h,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)return TRUE;
        wchar_t name[32]{};GetClassNameW(h,name,32);if(!wcscmp(name,L"Progman")||!wcscmp(name,L"WorkerW"))return TRUE;
        RECT box{};if(FAILED(DwmGetWindowAttribute(h,DWMWA_EXTENDED_FRAME_BOUNDS,&box,sizeof(box))))GetWindowRect(h,&box);RECT overlap{};if(IntersectRect(&overlap,&box,&c.island)){c.hit=true;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));
    return search.hit;
}
// The grid of what shows through Clear glass under the compact island (2-DIP cells, the top 40 DIPs of the canvas):
// the wallpaper's luminance, dimmed by the glass's scrim as the glass dims it. Null when it does not apply.
void IslandWindow::adaptBackdrop(){
    if(!renderer_||!window_)return;
    const bool wanted=settings_.adaptiveText&&settings_.material==2&&settings_.edge==0&&renderer_->glassAvailable();
    if(wanted)SetTimer(window_,52,1500,nullptr);else KillTimer(window_,52);
    std::shared_ptr<LumaGrid> grid;
    if(wanted&&(qaBackdrop_||(wallLuma_&&!backdropCovered()))){
        grid=std::make_shared<LumaGrid>();grid->cols=int(Renderer::canvasWidth/grid->cell);grid->rows=20;grid->luma.resize(size_t(grid->cols)*size_t(grid->rows));
        const bool light=content_.light;const double a=std::clamp((light?.32:.46)*(.6+settings_.glassTint/100.*1.1),.08,.62),scrim=light?246:12;
        RECT r{};GetWindowRect(window_,&r);const double s=dpi_/96;
        for(int y=0;y<grid->rows;++y)for(int x=0;x<grid->cols;++x){double L=0;const double cx=(x+.5)*grid->cell,cy=(y+.5)*grid->cell;
            // Captures use an illustrative backdrop (bright on the left, dark on the right), never the real wallpaper.
            if(qaBackdrop_)L=cx<Renderer::canvasWidth/2?235:25;
            else{const auto& m=*wallLuma_;const double mw=m.monitor.right-m.monitor.left,mh=m.monitor.bottom-m.monitor.top,sx=r.left+cx*s-m.monitor.left,sy=r.top+cy*s-m.monitor.top;
                L=m.luma[size_t(std::clamp(int(sy/mh*m.h),0,m.h-1))*size_t(m.w)+size_t(std::clamp(int(sx/mw*m.w),0,m.w-1))];}
            grid->luma[size_t(y)*size_t(grid->cols)+size_t(x)]=uint8_t(std::lround(L*(1-a)+scrim*a));}}
    const auto& old=content_.adapt;const bool same=(!old&&!grid)||(old&&grid&&old->luma==grid->luma);if(same)return;
    content_.adapt=grid;if(state_==IslandState::Compact)renderer_->redraw(content_,debug_,true);
}
void IslandWindow::applySettings(bool rebuild,bool reposition){
    motion_.body=bodySpring(settings_);motion_.edge=settings_.edge;motion_.compactWidth=settings_.uiMode==0?72:settings_.compactWidth;motion_.corner=settings_.corner;BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);motion_.reduced=settings_.reduceMotion||!enabled;
    DWORD light=0,size=sizeof(light);if(settings_.theme==2)RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);content_.light=settings_.theme==1||(settings_.theme==2&&light);content_.expanded=state_!=IslandState::Compact;
    content_.blur=GlassBackdrop::effectsEnabled();if(rebuild){renderer_.reset();position();renderer_=std::make_unique<Renderer>();renderer_->initialize(window_,dpi_,shadow_);}else if(reposition)position();syncLyrics();motion_.edge=settings_.edge;motion_.compactWidth=settings_.uiMode==0?72:settings_.compactWidth;refresh();animate();
    if(settingsWindow_&&settingsWindow_->open())settingsWindow_->update(settings_,settingsContext(),settingsSequence_);
    // Adaptive text follows the settings: the map loads when first needed; slideshows are re-read every ten minutes.
    syncSharing();syncLibrary();
    if(settings_.adaptiveText&&settings_.material==2){if(!wallLuma_)loadWallpaperLuma();SetTimer(window_,53,600000,nullptr);}else{KillTimer(window_,53);wallLuma_.reset();}adaptBackdrop();
}
void IslandWindow::feedback(Action action,float px,float py,bool press){
    renderer_->iconFeedback(action,press,settings_.animatedIcons&&!motion_.reduced);
    double now=seconds();auto it=std::find_if(renderer_->targets.begin(),renderer_->targets.end(),[&](auto& target){return target.action==action&&target.enabled;});
    bool visible=it!=renderer_->targets.end()&&state_!=IslandState::Compact&&action!=Action::SkipBack&&action!=Action::SkipForward,changed=false;
    auto aim=[&](Spring& spring,double value){if(std::abs(spring.target()-value)>.25){changed=true;if(motion_.reduced)spring.reset(value,now);else spring.retarget(value,now,{.65,520,33});}};
    aim(motion_.hoverOpacity,visible?1:0);if(visible){auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);double dx=0,dy=0;if(settings_.magnetic&&!motion_.reduced&&px){dx=std::clamp((px*96/dpi_-origin.x-20-it->x-it->width/2)*.1,-2.,2.);dy=std::clamp((py*96/dpi_-origin.y-(content_.card?16:content_.live?20:38)-it->y-it->height/2)*.1,-2.,2.);}aim(motion_.hoverX,20+it->x+dx+(press?1.5:0));aim(motion_.hoverY,(content_.card?16:content_.live?20:38)+it->y+dy+(press?1:0));aim(motion_.hoverW,it->width-(press?3:0));aim(motion_.hoverH,it->height-(press?2:0));}if(changed)renderer_->animate(motion_,now);
}
void IslandWindow::dragShelf(size_t index){if(index>=content_.shelf.size())return;auto item=content_.shelf[index];auto data=shelfData(item);shelfDragImage(data.Get(),item.preview);ComPtr<ShelfDragSource> source;source.Attach(new ShelfDragSource);DWORD effect=0;DoDragDrop(data.Get(),source.Get(),DROPEFFECT_COPY,&effect);content_.dropHover=false;refresh();}
// Phase 5F, the Controls page. A job sets a switch (0 none, 1 Wi-Fi, 2 Bluetooth, 3 all radios,
// 4 dark mode) on a worker, then reads every switch back and posts them; `busy` bits clear then.
void IslandWindow::controlJob(int which,int target,int busy){
    if(which==0&&controlQuery_->exchange(true))return;HWND w=window_;auto flag=controlQuery_;
    std::thread([w,which,target,busy,flag]{if(which>=1&&which<=3)setRadios(which,target!=0);else if(which==4)setDarkMode(target!=0);
        const int wifi=radioOn(1),bluetooth=radioOn(2),dark=darkModeNow();auto pack=[](int v,int shift){return WPARAM(std::clamp(v+2,0,15))<<shift;};
        if(which==0)flag->store(false);PostMessageW(w,ControlStateMessage,pack(wifi,0)|pack(bluetooth,4)|pack(dark,8),LPARAM(busy));}).detach();
}
void IslandWindow::toggleControl(Action a){
    auto& c=content_.controls;const double now=seconds();
    switch(a){
    case Action::ControlWifi:if(c.wifi>=0&&!(c.busy&1)){c.busy|=1;controlJob(1,c.wifi==1?0:1,1);}break;
    case Action::ControlBluetooth:if(c.bluetooth>=0&&!(c.busy&2)){c.busy|=2;controlJob(2,c.bluetooth==1?0:1,2);}break;
    // Airplane: every radio off, or back on when they all are.
    case Action::ControlAirplane:if(!(c.busy&4)&&(c.wifi>=0||c.bluetooth>=0)){c.busy|=4;controlJob(3,c.wifi==0&&c.bluetooth==0?1:0,4);}break;
    case Action::ControlDark:if(c.dark>=0&&!(c.busy&8)){c.busy|=8;controlJob(4,c.dark==1?0:1,8);}break;
    case Action::ControlFocus:if(!content_.focus.running&&(content_.focus.mode!=FocusClock::Mode::Focus||content_.focus.finished))content_.focus.select(FocusClock::Mode::Focus,now);content_.focus.toggle(now);break;
    case Action::ControlMic:if(audio_&&audio_->micAvailable)audio_->toggleMic();break;
    default:break;
    }
    store_.log("Info","control_switch");clockTimer();refresh();
}
void IslandWindow::setBrightnessAt(LPARAM point){if(!brightness_||content_.brightness<0)return;double now=seconds();auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);const int target=volumeAt(GET_X_LPARAM(point)*96/dpi_,origin.x+motion_.dragX.sample(now).position+58,262);if(target!=content_.brightness){brightnessRequestAt_=now;brightness_->set(target);content_.brightness=target;refresh();}}
void IslandWindow::setVolumeAt(LPARAM point){if(!audio_||!audio_->available)return;double now=seconds();auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);int target=volumeAt(GET_X_LPARAM(point)*96/dpi_,origin.x+motion_.dragX.sample(now).position+58,262);if(target!=audio_->value)audio_->setVolume(target);}
void IslandWindow::perform(Action a){
    double now=seconds();bool save=false,rebuild=false;
    if(a==Action::Settings){openSettings();return;}
    if(shareAction(a)||libraryAction(a))return;
    if(a==Action::BudPromote){promoteCard(true);return;}
    if(a>=Action::Overview&&a<=Action::Focus){if(Page next=Page(int(a)-int(Action::Overview));content_.page!=next&&state_==IslandState::Expanded&&!motion_.reduced){auto slot=[&](Page p){return int(std::find(settings_.navigation.begin(),settings_.navigation.end(),int(p))-settings_.navigation.begin());};motion_.swipe.reset(slot(next)>slot(content_.page)?22.:-22.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.page=Page(int(a)-int(Action::Overview));transition(IslandState::Expanded);}
        if(a==Action::Shelf||a==Action::Audio||a==Action::Control){content_.page=a==Action::Shelf?Page::Shelf:a==Action::Audio?Page::Audio:Page::Control;transition(IslandState::Expanded);if(a==Action::Control)controlJob(0,0);}
        if(a>=Action::ControlWifi&&a<=Action::ControlMic){toggleControl(a);return;}
    // The privacy dots bring back the card for the most important use; its button opens that permission's page.
    if(a==Action::PrivacyShow){auto uses=privacyUses_;std::stable_sort(uses.begin(),uses.end(),[](auto& x,auto& y){auto rank=[](Capability c){return int(std::find(std::begin(capabilityOrder),std::end(capabilityOrder),c)-std::begin(capabilityOrder));};return rank(x.capability)<rank(y.capability);});
        if(!uses.empty())showPrivacyNotice(uses.front());return;}
    if(a==Action::PrivacySettings){const int k=content_.notice.kind;const wchar_t* page=k==5?L"ms-settings:privacy-webcam":k==6?L"ms-settings:privacy-microphone":k==7?L"ms-settings:privacy-location":k==12?L"ms-settings:privacy-graphicscaptureprogrammatic":L"ms-settings:privacy";
        ShellExecuteW(nullptr,L"open",page,nullptr,nullptr,SW_SHOWNORMAL);store_.log("Info","privacy_settings_opened");return;}
    if(inRange(a,Action::MixerSliderBase,Action::MixerMuteBase))return;
    if(inRange(a,Action::MixerMuteBase,Action::SessionBase)){size_t i=int(a)-int(Action::MixerMuteBase);if(mixer_&&i<content_.mixer.size()){auto& e=content_.mixer[i];e.muted=!e.muted;mixer_->setMute(e.pid,e.muted);}refresh();return;}
    if(inRange(a,Action::ClipBase,Action::CommandResultBase)){copyClip(size_t(content_.clipOffset+int(a)-int(Action::ClipBase)));return;}
    if(inRange(a,Action::ClipPinBase,Action::ClipPinEnd)){const size_t row=size_t(content_.clipOffset+int(a)-int(Action::ClipPinBase));if(row<content_.clips.size()){if(!clips_.togglePin(content_.clips[row].id)){content_.clipStatus=L"Up to 12 copies can be pinned";content_.clipStatusUntil=seconds()+2.5;}clipViews();savePinnedClips();}refresh();return;}
    if(inRange(a,Action::ShelfItemBase,Action::MixerSliderBase)){openShelfItem(int(a)-int(Action::ShelfItemBase));return;}
    if(a>=Action::ShelfBack&&a<=Action::ShelfZip){shelfAction(a);return;}
    if(a==Action::CaptureSnip||a==Action::CaptureText||a==Action::CaptureColour){startCapture(a==Action::CaptureSnip?CaptureMode::Snip:a==Action::CaptureText?CaptureMode::Text:CaptureMode::Colour);return;}
    if(a==Action::ClipSearch){clipSearch(false);return;}
    if(inRange(a,Action::CommandResultBase,Action::CommandResultEnd)){runCommand(size_t(int(a)-int(Action::CommandResultBase)));return;}
    if(inRange(a,Action::LyricLineBase,Action::LyricLineEnd)){seekLyric(content_.lyricLine+int(a)-int(Action::LyricLineBase)-2);return;}
    if(inRange(a,Action::SessionBase,Action::DeviceConnectBase)){switchSession(int(a)-int(Action::SessionBase),true);return;}
    if(inRange(a,Action::DeviceConnectBase,Action::DeviceConnectEnd)){size_t i=int(a)-int(Action::DeviceConnectBase);if(bluetooth_&&i<content_.devices.size()){auto& d=content_.devices[i];bluetooth_->request(d.name,!d.connected);deviceRequest_=true;content_.deviceFeedback=(d.connected?L"Disconnecting ":L"Connecting ")+d.name+L"…";}refresh();return;}
    if(int(a)>=int(Action::DeviceBase)&&int(a)<int(Action::ShelfItemBase)){size_t index=int(a)-int(Action::DeviceBase);if(audio_&&index<content_.outputs.size()){if(settings_.directAudio){routeRequestAt_=seconds();audio_->selectDevice(content_.outputs[index].id,true);content_.feedback=L"Switching output…";}else perform(Action::SoundSettings);}refresh();return;}
    switch(a){
        case Action::LayoutSlot:content_.layoutSlot=(content_.layoutSlot+1)%pageCount;break;
    case Action::LayoutLeft:moveNavigation(settings_.navigation,content_.layoutSlot,-1);save=true;break;
    case Action::LayoutRight:moveNavigation(settings_.navigation,content_.layoutSlot,1);save=true;break;
    case Action::MetricOne:case Action::MetricTwo:case Action::MetricThree:cycleMetric(settings_.homeMetrics,int(a)-int(Action::MetricOne));save=true;break;
    case Action::Rings:settings_.glanceRings=(settings_.glanceRings+1)%4;save=true;break;
    case Action::IconsToggle:settings_.animatedIcons=!settings_.animatedIcons;save=true;renderer_->iconFeedback(Action::None,false,false);break;
    case Action::AppSwitchToggle:settings_.collapseOnAppSwitch=!settings_.collapseOnAppSwitch;save=true;break; case Action::WheelVolumeToggle:settings_.wheelVolume=!settings_.wheelVolume;save=true;break; case Action::HandoffToggle:settings_.trackHandoff=!settings_.trackHandoff;save=true;break;
    case Action::LayoutReset:settings_.navigation=defaultNavigation;settings_.homeMetrics=defaultMetrics;content_.layoutSlot=0;save=true;break;
    case Action::SettingsNext:content_.settingsPage=(content_.settingsPage+1)%10;break;
    case Action::StartupToggle:if(!testing_){bool desired=!settings_.startAtLogin;if(startup::apply(desired)){settings_.startAtLogin=desired;save=true;}else content_.feedback=L"Windows could not update sign-in settings";}break;
    case Action::Theme:settings_.theme=(settings_.theme+1)%3;save=true;break;
    case Action::Scale:settings_.scale=settings_.scale>=120?80:settings_.scale+10;save=rebuild=true;break;
    case Action::Edge:settings_.edge=(settings_.edge+1)%3;motion_.dragX.reset(0,now);motion_.dragY.reset(0,now);save=rebuild=true;break;
    case Action::CompactWidth:settings_.compactWidth=std::min(560,settings_.compactWidth+40);save=true;break;
    case Action::WidthDown:settings_.compactWidth=std::max(160,settings_.compactWidth-40);save=true;break;
    case Action::UiMode:settings_.uiMode=(settings_.uiMode+1)%3;save=true;break;
    case Action::CompactVolumeToggle:settings_.compactVolume=!settings_.compactVolume;save=true;break;
    case Action::CompactTimerToggle:settings_.compactTimer=!settings_.compactTimer;save=true;break;
    case Action::CompactClockToggle:settings_.compactClock=!settings_.compactClock;save=true;break;
    case Action::ShelfPeekToggle:settings_.shelfPeek=!settings_.shelfPeek;save=true;break;
    case Action::HorizontalOffset:settings_.horizontalOffset=settings_.horizontalOffset>=240?-240:settings_.horizontalOffset+40;save=rebuild=true;break;
    case Action::Corner:settings_.corner=settings_.corner>=28?14:settings_.corner+2;save=true;break;
    case Action::CollapseDelay:settings_.collapseDelay=settings_.collapseDelay>=1500?300:settings_.collapseDelay+150;save=true;break;
    case Action::GlassToggle:settings_.material=(settings_.material+1)%3;save=rebuild=true;break;
    case Action::AccentsToggle:settings_.albumAccents=!settings_.albumAccents;save=true;break;
    case Action::MagneticToggle:settings_.magnetic=!settings_.magnetic;save=true;break;
    case Action::CompactMediaToggle:settings_.compactMedia=!settings_.compactMedia;save=true;break;
    case Action::CompactBatteryToggle:settings_.compactBattery=!settings_.compactBattery;save=true;break;
    case Action::Accent:settings_.accent=(settings_.accent+1)%4;save=true;break;
    case Action::AudioCompatibility:settings_.directAudio=!settings_.directAudio;save=true;break;
    case Action::Offset:settings_.verticalOffset=settings_.verticalOffset>=24?0:settings_.verticalOffset+4;save=rebuild=true;break;
    case Action::Monitor:{std::vector<HMONITOR> monitors;EnumDisplayMonitors(nullptr,nullptr,collectMonitor,reinterpret_cast<LPARAM>(&monitors));displays_.remember(currentDisplay_,settings_);settings_.monitor=(settings_.monitor+1)%(int(monitors.size())+1);selectDisplay_=true;save=rebuild=true;break;}
    case Action::SettingsReset:{bool startup=settings_.startAtLogin;settings_=Settings{};settings_.startAtLogin=startup;save=rebuild=true;break;}
    case Action::ShelfClear:content_.shelf.clear();content_.shelfOffset=0;content_.shelfDetail=-1;requestPreviews();break;
    case Action::ShelfFiles:case Action::ShelfClipboard:{content_.shelfDetail=-1;int tab=a==Action::ShelfFiles?0:1;if(tab!=content_.shelfTab&&!motion_.reduced){motion_.swipe.reset(tab>content_.shelfTab?14.:-14.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.shelfTab=tab;if(tab==1)clipViews();break;}
    case Action::ClipboardEnable:{Settings next=settings_;next.clipboardHistory=true;receiveSettings(next);content_.clipStatus=L"Clipboard history is on";content_.clipStatusUntil=seconds()+2.5;break;}
    case Action::ClipboardPause:clips_.paused=!clips_.paused;clipViews();content_.clipStatus=clips_.paused?L"Paused — new copies are not kept":L"Keeping new copies again";content_.clipStatusUntil=seconds()+2.5;break;
    case Action::ClipboardClear:clearClips();content_.clipStatus=L"Cleared";content_.clipStatusUntil=seconds()+2.5;break;
    case Action::CommandOpen:openCommand();return;
    case Action::LyricsToggle:content_.lyricsView=!content_.lyricsView;break;
    case Action::SkipBack:case Action::SkipForward:break;// a double-click skips (WM_LBUTTONDOWN)
    case Action::SwitchBack:if(audio_&&!switchBackId_.empty()){routeRequestAt_=now;audio_->selectDevice(switchBackId_,true);switchBackId_.clear();content_.notice.switchBack=false;events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);store_.log("Info","headphone_switch_back");}break;
    case Action::MicMute:if(audio_&&audio_->micAvailable)audio_->toggleMic();break;
    case Action::Pin:content_.pinned=!content_.pinned;break;
    case Action::Close:content_.pinned=false;transition(IslandState::Compact);break;
    case Action::Play:if(content_.playback.canToggle)mediaCommand(1);break;
    case Action::Previous:if(content_.playback.canPrevious)mediaCommand(2);break;
    case Action::Next:if(content_.playback.canNext)mediaCommand(3);break;
    case Action::AudioApps:case Action::AudioOutputs:{int tab=a==Action::AudioApps?0:1;if(tab!=content_.audioTab&&!motion_.reduced){motion_.swipe.reset(tab>content_.audioTab?14.:-14.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.audioTab=tab;break;}
    case Action::StatsSystem:case Action::StatsBattery:case Action::StatsDevices:if(int tab=int(a)-int(Action::StatsSystem);tab!=content_.statsTab&&!motion_.reduced){motion_.swipe.reset(tab>content_.statsTab?14.:-14.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.statsTab=int(a)-int(Action::StatsSystem);if(a==Action::StatsBattery&&battery_){battery_->refresh();if(content_.charging)renderer_->energize(motion_.reduced,true);}break;
    case Action::Armoury:if(!content_.platform.armoury.empty())ShellExecuteW(nullptr,L"open",(L"shell:AppsFolder\\"+content_.platform.armoury).c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case Action::PowerSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:powersleep",nullptr,nullptr,SW_SHOWNORMAL);break;
    case Action::MixerSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:apps-volume",nullptr,nullptr,SW_SHOWNORMAL);break;
    case Action::MediaMode:settings_.mediaLayout=(settings_.mediaLayout+1)%3;save=true;break;
    case Action::VolumeDown:if(audio_)audio_->setVolume(audio_->value-5);break;
    case Action::VolumeUp:if(audio_)audio_->setVolume(audio_->value+5);break;
    case Action::Mute:if(audio_)audio_->toggleMute();break;
    case Action::Timer25:content_.focus.select(FocusClock::Mode::Focus,now);break;
    case Action::Timer5:content_.focus.select(FocusClock::Mode::Break,now);break;
    case Action::Stopwatch:content_.focus.select(FocusClock::Mode::Stopwatch,now);break;
    case Action::TimerToggle:content_.focus.toggle(now);break;
    case Action::TimerReset:content_.focus.reset(now);break;
    case Action::HoverToggle:settings_.hoverOpen=!settings_.hoverOpen;save=true;break;
    case Action::HoverDelay:settings_.hoverDelay=settings_.hoverDelay>=600?100:settings_.hoverDelay+100;save=true;break;
    case Action::FullscreenToggle:settings_.hideFullscreen=!settings_.hideFullscreen;save=true;fullscreen();break;
    case Action::ReducedToggle:{settings_.reduceMotion=!settings_.reduceMotion;BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);motion_.reduced=settings_.reduceMotion||!enabled;save=true;animate();break;}
    case Action::MotionPreset:settings_.preset=(settings_.preset+1)%5;motion_.body=bodySpring(settings_);save=true;animate();break;
    case Action::Lab:openSettings(3);break;
    case Action::DisplaySettings:ShellExecuteW(nullptr,L"open",L"ms-settings:display",nullptr,nullptr,SW_SHOWNORMAL);break;
    case Action::NetworkSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:network-status",nullptr,nullptr,SW_SHOWNORMAL);break;
    case Action::BluetoothSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:bluetooth",nullptr,nullptr,SW_SHOWNORMAL);break;
    case Action::SoundSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:sound",nullptr,nullptr,SW_SHOWNORMAL);break;
    default:break;
    }
    if(save){if(a!=Action::Monitor)displays_.remember(currentDisplay_,settings_);applySettings(rebuild);if(!testing_){store_.save(settings_,settingsFile_);saveDisplays();}}clockTimer();refresh();animate();
}
}
namespace nexus {
void IslandWindow::requestPreviews(const std::vector<ShelfItem>& incoming){if(incoming.empty())shelfChanged();if(content_.shelfDetail>=int(content_.shelf.size()))content_.shelfDetail=-1;if(!previews_)return;std::vector<std::wstring> paths;for(auto& item:content_.shelf)if(item.kind==ShelfItem::Kind::File)paths.push_back(item.value);for(auto& item:incoming)if(item.kind==ShelfItem::Kind::File&&std::find(paths.begin(),paths.end(),item.value)==paths.end())paths.push_back(item.value);previews_->request(std::move(paths));}
void IslandWindow::scrubAt(LPARAM point,bool begin){if(!content_.playback.canSeek||content_.page!=Page::Media)return;double now=seconds();auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);double x=GET_X_LPARAM(point)*96/dpi_-origin.x-20,y=GET_Y_LPARAM(point)*96/dpi_-origin.y-229;auto& s=content_.scrub;if(begin)s.begin(x,380,0,content_.playback.duration);else s.move(x,y,380,0,content_.playback.duration);
    // Detents at even time marks and lyric lines; 4 DIPs of pointer travel either side, at the current gain.
    std::vector<double> lines;if(settings_.lyrics&&content_.lyrics)for(auto& line:*content_.lyrics)lines.push_back(line.time);
    bool snapped=false;double v=snapToDetent(s.raw,seekDetents(content_.playback.duration,380,lines),4.*content_.playback.duration/380.*s.precision,&snapped);
    if(snapped&&v!=lastDetent_)++content_.detentPulse;lastDetent_=snapped?v:-1;
    s.value=std::clamp(v,content_.playback.seekMin,content_.playback.seekMax);refresh();}
void IslandWindow::endScrub(bool commit){if(!content_.scrub.active)return;double value=content_.scrub.value;content_.scrub.active=false;if(commit)mediaSeek(value);refresh();}
}
