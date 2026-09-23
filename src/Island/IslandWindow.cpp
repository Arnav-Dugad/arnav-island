#include "Common/Capture.h"
#include "IslandWindow.h"
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
    instance_=instance;visibilityAudit_=cmd.find(L"--visibility-audit")!=std::wstring::npos;motionStudy_=cmd.find(L"--motion-study")!=std::wstring::npos;settingsTest_=cmd.find(L"--settings-test")!=std::wstring::npos;testing_=settingsTest_||cmd.find(L"--capture")!=std::wstring::npos||cmd.find(L"--benchmark")!=std::wstring::npos||cmd.find(L"--ui-test")!=std::wstring::npos;if(testing_)settingsFile_=L"settings-qa.nexus";settings_=settingsTest_?Settings{}:store_.load();if(!testing_){std::ifstream profiles(store_.directory/L"displays.nexus");if(profiles)displays_=DisplayProfiles::read(profiles);}if(cmd.find(L"--ui-test")!=std::wstring::npos){settings_=Settings{};settings_.uiMode=2;}settings_.startAtLogin=startup::enabled();if(cmd.find(L"--enable-startup")!=std::wstring::npos&&!testing_){settings_.startAtLogin=startup::apply(true);store_.save(settings_,settingsFile_);}motion_.body=preset(MotionPreset(settings_.preset));
    BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);motion_.reduced=settings_.reduceMotion||!animations;
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=L"ArnavIsland.Surface";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));check(RegisterClassW(&wc)?S_OK:HRESULT_FROM_WIN32(GetLastError()));
    window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP|WS_EX_NOACTIVATE,wc.lpszClassName,L"Arnav Island",WS_POPUP,0,0,760,480,nullptr,nullptr,instance,this);
    if(!window_)throw std::runtime_error("Island window creation failed");
    position();renderer_=std::make_unique<Renderer>();renderer_->initialize(window_,dpi_);power(false);applySettings();if(!testing_){displays_.remember(currentDisplay_,settings_);saveDisplays();}
    audio_=std::make_unique<AudioProvider>(window_);if(cmd.find(L"--capture-safe")==std::wstring::npos)media_=std::make_unique<MediaProvider>(window_);system_=std::make_unique<SystemProvider>(window_);if(cmd.find(L"--capture-safe")==std::wstring::npos){analyzer_=std::make_unique<LoopbackAnalyzer>(window_);mixer_=std::make_unique<SessionMixer>(window_);brightness_=std::make_unique<BrightnessProvider>(window_);if(!(testing_&&cmd.find(L"--qa-sample")!=std::wstring::npos))bluetooth_=std::make_unique<BluetoothProvider>(window_);powerMode_=std::make_unique<PowerModeWatcher>(window_);}battery_=std::make_unique<BatteryProvider>(window_,testing_?std::filesystem::path{}:store_.directory/L"battery-history.nexus",settings_.batteryHistory);if(!testing_){std::ifstream saved(store_.directory/L"workspaces.nexus",std::ios::binary);if(saved)workspaces_=WorkspaceStore::read(saved);}if(cmd.find(L"--capture-safe")==std::wstring::npos)syncProductivity();platformThread_=std::thread([w=window_]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);auto info=new PlatformInfo(platformInfo());if(!PostMessageW(w,PlatformMessage,0,reinterpret_cast<LPARAM>(info)))delete info;CoUninitialize();});content_.settings=settings_;
    for(auto id:{&GUID_ACDC_POWER_SOURCE,&GUID_BATTERY_PERCENTAGE_REMAINING}){
        auto registration=RegisterPowerSettingNotification(window_,id,DEVICE_NOTIFY_WINDOW_HANDLE);if(registration)powerNotifications_.push_back(registration);
    }
    tray_.cbSize=sizeof(tray_);tray_.hWnd=window_;tray_.uID=1;tray_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray_.uCallbackMessage=TrayMessage;tray_.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));wcscpy_s(tray_.szTip,L"Arnav Island — right-click for controls");Shell_NotifyIconW(NIM_ADD,&tray_);
    previews_=std::make_unique<ShelfPreviews>(window_);dropTarget_.Attach(new ShelfDropTarget([this](bool hover){content_.dropHover=hover;if(hover){KillTimer(window_,8);content_.page=Page::Shelf;transition(IslandState::Expanded);}refresh();},[this](std::vector<ShelfItem> items){size_t first=content_.shelf.size();auto preview=items.empty()?nullptr:previews_->get(items.front().value);for(auto& item:items){if(content_.shelf.size()>=32)break;bool exists=std::any_of(content_.shelf.begin(),content_.shelf.end(),[&](auto& existing){return existing.kind==item.kind&&existing.value==item.value;});if(!exists)content_.shelf.push_back(std::move(item));}content_.pinned=false;motion_.pulse.reset(.45,seconds());motion_.pulse.retarget(0,seconds(),{1,95,22});requestPreviews();refresh();animate();if(content_.shelf.size()>first){POINT p{};GetCursorPos(&p);ScreenToClient(window_,&p);auto origin=bodyOrigin(motion_.width.sample(seconds()).position,motion_.height.sample(seconds()).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);content_.shelfOffset=int(first>3?first-3:0);renderer_->absorb(preview,float(p.x*96/dpi_-origin.x),float(p.y*96/dpi_-origin.y),42,94+float(first-content_.shelfOffset)*36,motion_.reduced);refresh();}}));dropTarget_->attach(window_,[this](const std::vector<ShelfItem>& items){requestPreviews(items);});check(RegisterDragDrop(window_,dropTarget_.Get()));
    foregroundOwner=this;foregroundHook_=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    ShowWindow(window_,SW_SHOWNOACTIVATE);animate();
    locationHook_=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);if(!testing_)fullscreen();
    if(cmd.find(L"--expanded")!=std::wstring::npos){content_.pinned=true;transition(IslandState::Expanded);refresh();}
    for(auto pair:{std::pair{L"--media",Page::Media},std::pair{L"--system",Page::System},std::pair{L"--focus",Page::Focus},std::pair{L"--shelf",Page::Shelf},std::pair{L"--audio",Page::Audio}})if(cmd.find(pair.first)!=std::wstring::npos){content_.page=pair.second;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(cmd.find(L"--video-layout")!=std::wstring::npos){settings_.mediaLayout=2;refresh();}
    if(cmd.find(L"--music-layout")!=std::wstring::npos){settings_.mediaLayout=1;refresh();}
        if(cmd.find(L"--scale110")!=std::wstring::npos&&testing_){settings_.scale=110;applySettings(true);}
    if(testing_&&cmd.find(L"--qa-dark")!=std::wstring::npos){settings_.theme=0;applySettings();}
    if(cmd.find(L"--light")!=std::wstring::npos){settings_.theme=1;applySettings();}
    if(cmd.find(L"--right")!=std::wstring::npos){settings_.edge=1;applySettings(true);}
    if(testing_&&cmd.find(L"--qa-pattern")!=std::wstring::npos)showcasePattern_=true;
    if(testing_&&cmd.find(L"--qa-showcase")!=std::wstring::npos){
        auto art=std::make_shared<Artwork>();art->width=art->height=256;art->pixels.resize(256*256*4);for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){size_t i=(y*256+x)*4;art->pixels[i]=BYTE(150+x/3);art->pixels[i+1]=BYTE(70+y/2);art->pixels[i+2]=BYTE(230-x/3);art->pixels[i+3]=255;}art->accent=0xd9b8f2;
        auto player=resolveApp(L"Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"),browser=resolveApp(L"MSEdge"),calculator=resolveApp(L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App");
        MediaSnapshot first;first.available=first.playing=first.canToggle=first.canNext=first.canPrevious=first.canSeek=true;first.title=L"Artwork study";first.artist=L"Original local QA artwork";first.appName=player.name.empty()?L"Media Player":player.name;first.appIcon=player.icon;first.artwork=art;first.duration=first.seekMax=214;first.position=83;first.sampledAt=seconds();first.current=true;first.kind=MediaKind::Music;first.source=L"qa.player";
        MediaSnapshot second=first;second.title=L"Browser playback study";second.artist=L"Synthetic QA session";second.service="youtube";second.browser=true;second.appName=L"Microsoft Edge";second.appIcon=browser.icon;second.playing=false;second.current=false;second.source=L"qa.browser";
        first.id=1;second.id=2;content_.sessions={first,second};content_.session=0;content_.playback=first;content_.media=first.title;selectedSource_=first.source;selectedId_=first.id;
        content_.mixer={MixerEntry{1,first.appName,player.icon,.72f,.0f,false,false,true},MixerEntry{2,L"Microsoft Edge",browser.icon,.45f,0,false,false,true},MixerEntry{3,calculator.name.empty()?L"Calculator":calculator.name,calculator.icon,1,0,true,false,false},MixerEntry{0,L"System sounds",nullptr,.8f,0,false,true,false}};
        media_.reset();mixer_.reset();analyzer_.reset();content_.waveform=true;SetTimer(window_,22,10,nullptr);refresh();animate();}
    if(cmd.find(L"--qa-art")!=std::wstring::npos&&testing_){auto art=std::make_shared<Artwork>();art->width=256;art->height=256;art->pixels.resize(256*256*4);for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){size_t i=(y*256+x)*4;art->pixels[i]=BYTE(110+x/2);art->pixels[i+1]=BYTE(55+y/2);art->pixels[i+2]=BYTE(180-x/3);art->pixels[i+3]=255;}content_.playback.artwork=art;content_.playback.title=L"Artwork study";content_.playback.artist=L"Original local QA artwork";content_.playback.available=true;refresh();animate();}
    for(auto pair:{std::pair{L"--layout",6},std::pair{L"--details",4},std::pair{L"--multitasking",0},std::pair{L"--qa-modes",1},std::pair{L"--qa-appearance",2}})if(testing_&&cmd.find(pair.first)!=std::wstring::npos)openSettings(pair.second); if(cmd.find(L"--qa-timer")!=std::wstring::npos&&testing_){content_.focus.held=450;content_.focus.toggle(seconds());clockTimer();refresh();}
    if(testing_&&cmd.find(L"--qa-seek")!=std::wstring::npos){content_.page=Page::Media;content_.playback.available=true;content_.playback.canSeek=true;content_.playback.duration=content_.playback.seekMax=240;content_.playback.position=75;content_.playback.sampledAt=seconds();content_.scrub.begin(220,380,0,240);content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(testing_&&cmd.find(L"--qa-previews")!=std::wstring::npos){content_.page=Page::Shelf;auto path=(std::filesystem::current_path()/L"docs/evidence/v0.4/artwork.png").wstring();content_.shelf.push_back({ShelfItem::Kind::File,path,L"Original QA artwork.png"});content_.shelf.push_back({ShelfItem::Kind::File,(std::filesystem::current_path()/L"docs/QUICK_START.md").wstring(),L"Quick start.md"});requestPreviews();content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(cmd.find(L"--qa-handoff")!=std::wstring::npos&&testing_)SetTimer(window_,15,180,nullptr);
    if(cmd.find(L"--lab")!=std::wstring::npos)openLab();
    if(cmd.find(L"--benchmark")!=std::wstring::npos){benchmark_=true;benchmarkStart_=seconds();FILETIME c,e;GetProcessTimes(GetCurrentProcess(),&c,&e,&initialKernel_,&initialUser_);SetTimer(window_,ScenarioTimer,73,nullptr);}
    if(cmd.find(L"--ui-test")!=std::wstring::npos){settings_.hoverOpen=true;settings_.hoverDelay=100;SetTimer(window_,13,200,nullptr);}
    if(testing_&&cmd.find(L"--qa-live")!=std::wstring::npos){settings_.uiMode=1;content_.pinned=true;transition(IslandState::LiveActivity);refresh();}
    if(testing_&&cmd.find(L"--qa-mini")!=std::wstring::npos){settings_.uiMode=0;applySettings();transition(IslandState::Compact);}
    if(testing_&&cmd.find(L"--qa-wide")!=std::wstring::npos){settings_.uiMode=1;settings_.compactWidth=560;settings_.compactClock=true;applySettings();transition(IslandState::Compact);}
    if(testing_&&cmd.find(L"--qa-peek")!=std::wstring::npos){content_.hovered=Action::ShelfItemBase;refresh();}
    if(cmd.find(L"--settings")!=std::wstring::npos&&!settingsTest_)openSettings();
    if(testing_&&cmd.find(L"--qa-glass")!=std::wstring::npos){settings_.material=cmd.find(L"--qa-clear")!=std::wstring::npos?2:1;applySettings(false,true);}
    if(settingsTest_){settingsTestPhase_=0;SetTimer(window_,21,400,nullptr);}
    // Synthetic 100 Hz band levels through the real bar-animation path; measures rendering cost without playing audio.
    if(testing_&&cmd.find(L"--qa-spectrum")!=std::wstring::npos){content_.playback.available=content_.playback.playing=true;content_.playback.title=L"Spectrum load study";content_.waveform=true;analyzer_.reset();refresh();animate();SetTimer(window_,22,10,nullptr);}
    // Real device and battery data through the real card and tab paths.
    // --qa-sample swaps in illustrative devices so public screenshots never show real device names.
    const bool sample=cmd.find(L"--qa-sample")!=std::wstring::npos;
    auto qaDevices=[sample]{if(!sample)return BluetoothProvider::enumerate();auto make=[](const wchar_t* name,bool connected,int battery,DeviceKind kind,const char* brand){BluetoothDevice d;d.name=name;d.connected=connected;d.audio=kind==DeviceKind::Headphones||kind==DeviceKind::Earbuds||kind==DeviceKind::Speaker;d.battery=battery;d.kind=kind;d.brand=brand;return d;};
        return std::vector<BluetoothDevice>{make(L"Galaxy Buds3 Pro",true,82,DeviceKind::Earbuds,"samsung"),make(L"WH-1000XM5",true,64,DeviceKind::Headphones,"sony"),make(L"DualSense Wireless Controller",false,41,DeviceKind::Gamepad,"playstation"),make(L"JBL Flip 6",false,-1,DeviceKind::Speaker,"jbl")};};
    // Phase 4 QA states, with illustrative content only.
    if(testing_&&cmd.find(L"--qa-clipboard")!=std::wstring::npos){settings_.clipboardHistory=true;content_.settings=settings_;double now=seconds();auto notepad=shellIcon(L"C:\\Windows\\System32\\notepad.exe",32);
        auto add=[&](ClipEntry e,double age){e.sourceIcon=notepad;clips_.add(std::move(e),now-age);};
        {ClipEntry e;e.kind=ClipEntry::Kind::Files;e.files={L"C:\\Users\\Public\\Documents\\Quarterly review.pdf",L"C:\\Users\\Public\\Documents\\Budget.xlsx"};e.source=L"File Explorer";add(std::move(e),5400);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Image;e.thumbnail=content_.sessions.empty()?nullptr:content_.sessions.front().artwork;e.imageWidth=1920;e.imageHeight=1080;e.source=L"Snipping Tool";if(!e.thumbnail){auto art=std::make_shared<Artwork>();art->width=art->height=64;art->pixels.resize(64*64*4);for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto* p=&art->pixels[size_t(y*64+x)*4];p[0]=uint8_t(200-x);p[1]=uint8_t(120+y);p[2]=uint8_t(230-y);p[3]=255;}e.thumbnail=art;}add(std::move(e),1800);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Link;e.text=L"https://github.com/Arnav-Dugad/arnav-island/releases";e.source=L"Microsoft Edge";add(std::move(e),240);}
        {ClipEntry e;e.kind=ClipEntry::Kind::Text;e.text=L"Agenda for Thursday: launch review, owners and follow-ups";e.source=L"Notepad";add(std::move(e),20);}
        clipViews();content_.page=Page::Shelf;content_.shelfTab=1;content_.pinned=true;transition(IslandState::Expanded);refresh();}
    if(testing_&&cmd.find(L"--qa-privacy")!=std::wstring::npos){PrivacyUse camera{Capability::Camera,L"Microsoft Teams",L"MSTeams_8wekyb3d8bbwe",true,1,installedAppIcon(L"Microsoft Teams")};PrivacyUse mic{Capability::Microphone,L"Microsoft Teams",L"MSTeams_8wekyb3d8bbwe",true,1,camera.icon};
        privacyUses_={camera,mic};content_.privacy=privacyUses_;
        if(cmd.find(L"--qa-privacy-card")!=std::wstring::npos)showPrivacyNotice(camera);else{refresh();animate();}}
    if(auto at=cmd.find(L"--qa-command");testing_&&at!=std::wstring::npos){std::wstring text;auto eq=cmd.find(L'=',at);if(eq!=std::wstring::npos&&eq<cmd.find(L' ',at)){auto end=cmd.find(L" --",eq);text=cmd.substr(eq+1,end==std::wstring::npos?std::wstring::npos:end-eq-1);for(auto& c:text)if(c==L'_')c=L' ';}
        if(!commands_)commands_=std::make_unique<CommandService>(window_,L"C:\\Users\\Public");openCommand();content_.command.text=text;content_.command.caret=text.size();commandQuery();refresh();}
    if(testing_&&cmd.find(L"--qa-device-card")!=std::wstring::npos){auto devices=qaDevices();auto it=std::find_if(devices.begin(),devices.end(),[](auto& d){return d.battery>=0&&!d.brand.empty();});if(it==devices.end()&&!devices.empty())it=devices.begin();if(it!=devices.end())showNotice(1,*it);}
    if(testing_&&cmd.find(L"--qa-power-card")!=std::wstring::npos){if(battery_)updateBattery();showNotice(content_.charging?3:4);}
    for(auto pair:{std::pair{L"--qa-battery",1},std::pair{L"--qa-devices",2}})if(testing_&&cmd.find(pair.first)!=std::wstring::npos){content_.page=Page::System;content_.statsTab=pair.second;content_.pinned=true;if(bluetooth_||sample)content_.devices=qaDevices();if(battery_){battery_->setFast(true);updateBattery();}transition(IslandState::Expanded);refresh();}
    if(testing_&&cmd.find(L"--qa-hud")!=std::wstring::npos){content_.volume=audio_?audio_->value.load():40;events_.publish({ActivityKind::Volume,"volume",40,double(content_.volume),.5,30},seconds());presentActivity();}
    if(testing_&&cmd.find(L"--qa-brightness")!=std::wstring::npos){content_.brightness=64;events_.publish({ActivityKind::Brightness,"brightness",40,64,.5,30},seconds());presentActivity();}
    if(cmd.find(L"--capture")!=std::wstring::npos&&!benchmark_&&cmd.find(L"--ui-test")==std::wstring::npos)SetTimer(window_,5,cmd.find(L"--qa-command")!=std::wstring::npos?4200:2800,nullptr); store_.log("Info","application_started_directcomposition");
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(lab_&&IsDialogMessageW(lab_,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}
    store_.log("Info","application_stopped");return int(msg.wParam);
}
IslandWindow::~IslandWindow(){
    if(qaMatte_)DestroyWindow(qaMatte_);if(qaBrush_)DeleteObject(qaBrush_);
    foregroundOwner=nullptr;if(foregroundHook_)UnhookWinEvent(foregroundHook_);if(locationHook_)UnhookWinEvent(locationHook_);
    if(hotkey_)UnregisterHotKey(window_,0x4e49);clipboard_.stop();commands_.reset();privacy_.reset();
    if(platformThread_.joinable())platformThread_.join();
    previews_.reset();powerMode_.reset();bluetooth_.reset();battery_.reset();brightness_.reset();mixer_.reset();analyzer_.reset();system_.reset();media_.reset();audio_.reset();for(auto h:powerNotifications_)UnregisterPowerSettingNotification(h);
    if(tray_.hWnd)Shell_NotifyIconW(NIM_DELETE,&tray_);
    renderer_.reset();if(lab_&&IsWindow(lab_))DestroyWindow(lab_);if(window_&&IsWindow(window_))DestroyWindow(window_);
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
    if(settings_.edge)x=info.rcMonitor.right-width-int(settings_.gap()*s);int y=settings_.edge?(info.rcMonitor.top+info.rcMonitor.bottom-height)/2+int(settings_.verticalOffset*s):info.rcMonitor.top+int((settings_.verticalOffset+settings_.gap())*s);SetWindowPos(window_,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);positioning_=false;dpi_=float(GetDpiForWindow(window_))*settings_.scale/100.f;
}
void IslandWindow::updateRegion(bool envelope){
    if(!window_)return;double scale=dpi_/96,now=seconds();HRGN region=CreateRectRgn(0,0,0,0);
    for(double dt:{0.,.03,.06,.09}){if(!envelope&&dt>0)break;double t=now+dt,w=motion_.width.sample(t).position,h=motion_.height.sample(t).position;auto origin=bodyOrigin(w,h,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);origin.x+=motion_.dragX.sample(t).position;origin.y+=motion_.dragY.sample(t).position;{double slide=motion_.slide.sample(t).position;if(settings_.edge)origin.x+=slide*74;else origin.y-=slide*44;}auto outline=dockOutline(w,h,motion_.radius.sample(t).position,settings_.edge,!settings_.floating());std::vector<POINT> points;points.reserve(outline.size());for(auto p:outline)points.push_back({LONG(std::lround((p.x+origin.x)*scale)),LONG(std::lround((p.y+origin.y)*scale))});HRGN pose=CreatePolygonRgn(points.data(),int(points.size()),WINDING);CombineRgn(region,region,pose,RGN_OR);DeleteObject(pose);}
    HRGN outline=CreateRectRgn(0,0,0,0);CombineRgn(outline,region,region,RGN_COPY);for(auto offset:std::array<POINT,8>{{{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}}){HRGN fringe=CreateRectRgn(0,0,0,0);CombineRgn(fringe,outline,outline,RGN_COPY);OffsetRgn(fringe,offset.x,offset.y);CombineRgn(region,region,fringe,RGN_OR);DeleteObject(fringe);}DeleteObject(outline);if(!SetWindowRgn(window_,region,FALSE))DeleteObject(region);
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
    bool changed=state_!=s;state_=s;if(s!=IslandState::Compact&&content_.hud){content_.hud=0;motion_.compactWidth=settings_.uiMode==0?72:settings_.compactWidth;}content_.expanded=s!=IslandState::Compact;content_.live=s==IslandState::LiveActivity||s==IslandState::Notification||s==IslandState::Command;content_.card=s==IslandState::Notification||s==IslandState::Command;if(changed){content_.settings=settings_;if(content_.expanded)refresh();else renderer_->redraw(content_,debug_,true);}clockTimer();animate();if(lab_)InvalidateRect(lab_,nullptr,FALSE);}
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
    auto kind=events_.active()->kind;content_.activity=kind==ActivityKind::Volume?(content_.muted?L"Muted":L"Volume "+std::to_wstring(content_.volume)+L"%"):kind==ActivityKind::Power?(content_.charging?L"Power connected":L"On battery"):kind==ActivityKind::Timer?L"Session complete":kind==ActivityKind::Device?L"Output · "+routeName_:kind==ActivityKind::Brightness?L"Brightness "+std::to_wstring(content_.brightness)+L"%":kind==ActivityKind::Clipboard?copyLabel_:L"";levelIndicator();refresh();animate();SetTimer(window_,ActivityTimer,500,nullptr);
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
    if(benchmark_)return;
    HWND fg=GetForegroundWindow();if(!fg||fg==window_||fg==lab_)return;
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
    if(hide){KillTimer(window_,7);KillTimer(window_,8);interaction_=InteractionState::Rest;content_.hovered=Action::None;feedback(Action::None);}
    if(bool(IsWindowVisible(window_))==hide){ShowWindow(window_,hide?SW_HIDE:SW_SHOWNOACTIVATE);if(!hide)SetTimer(window_,SettleTimer,30,nullptr);}if(!hide)SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    clockTimer();
}
LRESULT CALLBACK IslandWindow::procedure(HWND h,UINT m,WPARAM w,LPARAM l){
    auto self=reinterpret_cast<IslandWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<IslandWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self)try{return self->message(m,w,l);}catch(const std::exception&){self->store_.log("Error","window_operation_failed");if(self->renderer_)PostMessageW(h,WM_CLOSE,0,0);}
    return DefWindowProcW(h,m,w,l);
}
LRESULT IslandWindow::message(UINT m,WPARAM w,LPARAM l){
    {LRESULT handled=0;if(productivityMessage(m,w,l,handled))return handled;}
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_APP+50:if(w==1)openLab();else if(w==2)transition(IslandState::Expanded);else if(w==3){settings_.startAtLogin=startup::apply(true);store_.save(settings_,settingsFile_);refresh();}return 0;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(window_,&ps);EndPaint(window_,&ps);return 0;}
    case WM_MOUSEACTIVATE:return state_==IslandState::Compact?MA_NOACTIVATE:MA_ACTIVATE;
    case WM_KEYDOWN:if(content_.command.active&&commandKey(w))return 0;if(content_.hovered==Action::VolumeSlider&&audio_&&(w==VK_LEFT||w==VK_RIGHT||w==VK_HOME||w==VK_END)){audio_->setVolume(w==VK_HOME?0:w==VK_END?100:audio_->value+(w==VK_RIGHT?2:-2));return 0;}if(w==VK_ESCAPE){if(content_.scrub.active){endScrub(false);ReleaseCapture();pressedAction_=Action::None;return 0;}perform(Action::Close);return 0;}if(w==VK_TAB){std::vector<Action> enabled;for(auto& t:renderer_->targets)if(t.enabled)enabled.push_back(t.action);auto it=std::find(enabled.begin(),enabled.end(),content_.hovered);int index=it==enabled.end()?-1:int(it-enabled.begin());if(!enabled.empty()){index=(index+((GetKeyState(VK_SHIFT)&0x8000)?int(enabled.size())-1:1))%int(enabled.size());content_.hovered=enabled[index];feedback(content_.hovered);}return 0;}if(w==VK_RETURN||w==VK_SPACE){perform(content_.hovered);return 0;}break;
    case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window_,&p);double t=seconds(),s=dpi_/96;
        double width=motion_.width.sample(t).position,height=motion_.height.sample(t).position;
        auto origin=bodyOrigin(width,height,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);double slide=motion_.slide.sample(t).position;double x=p.x/s-origin.x-motion_.dragX.sample(t).position-(settings_.edge?slide*74:0),y=p.y/s-origin.y-motion_.dragY.sample(t).position+(settings_.edge?0:slide*44);
        return x>=0&&x<=width&&y>=0&&y<=height?HTCLIENT:HTTRANSPARENT;}
    case WM_MOUSEMOVE:{
        KillTimer(window_,8);
        if(interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging){
            if(pressedAction_==Action::Seek){scrubAt(l);return 0;}if(pressedAction_==Action::VolumeSlider){setVolumeAt(l);return 0;}if(inRange(pressedAction_,Action::MixerSliderBase,Action::MixerMuteBase)){setMixerAt(l);return 0;}if(pressedAction_!=Action::None){if(inRange(pressedAction_,Action::ShelfItemBase,Action::MixerSliderBase)){POINT pointer{};GetCursorPos(&pointer);if(std::abs(pointer.x-down_.x)+std::abs(pointer.y-down_.y)>6){size_t index=int(pressedAction_)-int(Action::ShelfItemBase);pressedAction_=Action::None;interaction_=InteractionState::Rest;ReleaseCapture();dragShelf(index);}}return 0;}
            POINT p{};GetCursorPos(&p);double dx=(p.x-down_.x)*96/dpi_,dy=(p.y-down_.y)*96/dpi_;
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
            auto target=hit(l);
            // In the command bar the highlight follows the pointer across results and stays on the selection otherwise.
            if(content_.command.active){if(inRange(target,Action::CommandResultBase,Action::ActionEnd)){int row=int(target)-int(Action::CommandResultBase);if(row!=content_.command.selected){content_.command.selected=row;refresh();}}else target=Action(int(Action::CommandResultBase)+content_.command.selected);}
            if(target!=content_.hovered){bool detail=target==Action::Seek||content_.hovered==Action::Seek||content_.page==Page::Shelf;content_.hovered=target;if(detail)refresh();KillTimer(window_,19);if(content_.live&&target==Action::Overview)SetTimer(window_,19,420,nullptr);}feedback(target,GET_X_LPARAM(l),GET_Y_LPARAM(l));
        }return 0;}
    case WM_MOUSELEAVE:KillTimer(window_,7);edgeHold_=false;KillTimer(window_,19);if(interaction_==InteractionState::Hover){interaction_=InteractionState::Rest;content_.hovered=Action::None;feedback(Action::None);refresh();animate();if(!content_.pinned)SetTimer(window_,8,settings_.collapseDelay,nullptr);}return 0;
    case WM_LBUTTONDOWN:pressedAction_=hit(l);if(pressedAction_==Action::Seek)scrubAt(l,true);if(pressedAction_==Action::VolumeSlider)setVolumeAt(l);if(inRange(pressedAction_,Action::MixerSliderBase,Action::MixerMuteBase))setMixerAt(l);interaction_=InteractionState::Pressed;GetCursorPos(&down_);lastPointer_=down_;dragTime_=seconds();SetCapture(window_);if(pressedAction_==Action::None)animate();else feedback(pressedAction_,GET_X_LPARAM(l),GET_Y_LPARAM(l),true);return 0;
    case WM_LBUTTONUP:{if(pressedAction_==Action::Seek){scrubAt(l);endScrub(true);}bool dragged=interaction_==InteractionState::Dragging;interaction_=InteractionState::Hover;ReleaseCapture();
        if(dragged){POINT up{};GetCursorPos(&up);double dx=(up.x-down_.x)*96/dpi_,dy=(up.y-down_.y)*96/dpi_;
            if(state_!=IslandState::Compact&&content_.sessions.size()>1&&(content_.live||content_.page==Page::Media||content_.page==Page::Overview)&&std::abs(dx)>44&&std::abs(dx)>std::abs(dy))switchSession(dx<0?1:-1);}
        if(dragged){double now=seconds();auto pos=motion_.dragY.sample(now).position;motion_.dragY.reset(pos,now,dragVelocity_*.3);motion_.dragY.retarget(0,now,motion_.body);motion_.dragX.retarget(0,now,motion_.body);animate();}
        else if(pressedAction_!=Action::None){if(pressedAction_==hit(l))perform(pressedAction_);}
        else{animate();}
        pressedAction_=Action::None;feedback(hit(l),GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;}
    case WM_CAPTURECHANGED:if(content_.scrub.active)endScrub(false);if(interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging){interaction_=InteractionState::Rest;motion_.dragX.retarget(0,seconds(),motion_.body);motion_.dragY.retarget(0,seconds(),motion_.body);animate();}return 0;
    case WM_RBUTTONUP:showMenu();return 0;
    case WM_MOUSEHWHEEL:if(state_!=IslandState::Compact&&content_.sessions.size()>1&&(content_.live||content_.page==Page::Media||content_.page==Page::Overview)){double now=seconds();swipeAccumulator_+=GET_WHEEL_DELTA_WPARAM(w);if(std::abs(swipeAccumulator_)>=120&&now-swipeTime_>.3){switchSession(swipeAccumulator_>0?1:-1);swipeAccumulator_=0;swipeTime_=now;}}return 0;
    case WM_MOUSEWHEEL:if(state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==2){content_.deviceOffset=std::clamp(content_.deviceOffset+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,int(content_.devices.size())-4));refresh();return 0;}if(state_==IslandState::Expanded&&content_.page==Page::Shelf&&content_.shelfTab==1){content_.clipOffset=std::clamp(content_.clipOffset+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,int(content_.clips.size())-4));refresh();return 0;}if(state_!=IslandState::Compact&&(content_.page==Page::Shelf||content_.page==Page::Audio)){bool mixer=content_.page==Page::Audio&&content_.audioTab==0;auto& offset=content_.page==Page::Shelf?content_.shelfOffset:mixer?content_.mixerOffset:content_.audioOffset;int count=int(content_.page==Page::Shelf?content_.shelf.size():mixer?content_.mixer.size():content_.outputs.size());offset=std::clamp(offset+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,count-4));refresh();return 0;}if(audio_&&audio_->available){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window_,&p);if(settings_.wheelVolume||hit(MAKELPARAM(p.x,p.y))==Action::VolumeSlider)audio_->setVolume(audio_->value+(GET_WHEEL_DELTA_WPARAM(w)>0?2:-2));}return 0;
    case MediaMessage:if(media_)updateSessions();return 0;
    case SpectrumMessage:if(analyzer_){analyzer_->pending=false;bool delivering=analyzer_->active()&&analyzer_->available.load();if(delivering!=content_.waveform){content_.waveform=delivering;refresh();}if(delivering)renderer_->spectrum(analyzer_->frame());}return 0;
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
    case BrightnessMessage:if(brightness_){content_.brightness=brightness_->value.load();if(w){events_.publish({ActivityKind::Brightness,"brightness",40,double(content_.brightness),.5,2},seconds());presentActivity();}}return 0; case AudioMessage:if(audio_){audio_->notificationPending=false;content_.outputs=audio_->devices();std::wstring current;for(auto& d:content_.outputs)if(d.current){current=d.id;routeName_=d.name;}bool routeChanged=route_.observe(current);if(FAILED(audio_->switchResult.load()))content_.feedback=L"Switch unavailable · open Windows sound settings";else content_.feedback=L"";content_.volume=audio_->value;content_.muted=audio_->muted;motion_.volume.retarget(content_.muted?0:content_.volume/100.,seconds(),{1,550,42});
        if(w){events_.publish({ActivityKind::Volume,"volume",40,double(content_.volume),.5,2},seconds());presentActivity();}
        else{refresh();renderer_->animate(motion_,seconds());}if(routeChanged){events_.publish({ActivityKind::Device,"audio-route",45,0,.8,3},seconds());motion_.pulse.reset(motion_.reduced?0:.18,seconds());motion_.pulse.retarget(0,seconds(),{1,70,18});presentActivity();auto selected=std::find_if(content_.outputs.begin(),content_.outputs.end(),[](auto& d){return d.current;});renderer_->routeConfirmed(motion_.reduced,selected==content_.outputs.end()?Action::None:Action(int(Action::DeviceBase)+int(selected-content_.outputs.begin())));}}return 0;
    case ShelfPreviewMessage:if(previews_){for(auto& item:content_.shelf)if(item.kind==ShelfItem::Kind::File)item.preview=previews_->get(item.value);refresh();}return 0; case SystemMessage:if(system_){content_.system=system_->snapshot();if(state_!=IslandState::Compact&&IsWindowVisible(window_))refresh();}return 0;
    case WM_POWERBROADCAST:if(w==PBT_POWERSETTINGCHANGE)power(true);return TRUE;
    case SettingsChangedMessage:{std::unique_ptr<Settings> incoming(reinterpret_cast<Settings*>(l));settingsSequence_=unsigned(w);if(incoming)receiveSettings(*incoming);return 0;}
    case SettingsActionMessage:settingsAction(SettingAction(w));return 0;
    case FullscreenMessage:if(w){SetTimer(window_,17,120,nullptr);}else{DWORD pid=0;auto fg=GetForegroundWindow();if(fg)GetWindowThreadProcessId(fg,&pid);if(!testing_&&pid&&pid!=GetCurrentProcessId())yieldToApp();fullscreen();}return 0;
    case WM_DISPLAYCHANGE:if(renderer_)applySettings(true);return 0;
    case WM_DPICHANGED:if(positioning_)return 0;if(renderer_)applySettings(true);return 0;
    case WM_SETTINGCHANGE:if(renderer_)applySettings();return 0;
    case TrayMessage:if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU)showMenu();else if(l==WM_LBUTTONDBLCLK)openSettings();return 0;
    case WM_TIMER: if(w==17){KillTimer(window_,17);fullscreen();return 0;}
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
            case 9:{perform(Action::Media);content_.playback.canSeek=true;content_.playback.duration=content_.playback.seekMax=240;content_.playback.seekMin=0;refresh();auto point=[&](float x,float y){auto origin=bodyOrigin(motion_.width.sample(seconds()).position,motion_.height.sample(seconds()).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);return MAKELPARAM(int((origin.x+x)*dpi_/96),int((origin.y+y)*dpi_/96));};scrubAt(point(210,229),true);pass=content_.scrub.active&&std::abs(content_.scrub.value-120)<1;auto initial=content_.scrub.value;scrubAt(point(248,310));pass=pass&&content_.scrub.value>initial&&content_.scrub.value<initial+4;endScrub(false);pass=pass&&!content_.scrub.active&&renderer_->hit(210,228)==Action::Seek;perform(Action::Close);SetTimer(window_,13,100,nullptr);break;}
            case 10:{settings_.edge=0;settings_.uiMode=0;applySettings(true);transition(IslandState::Compact);SendMessageW(window_,WM_LBUTTONDOWN,0,MAKELPARAM(int(Renderer::canvasWidth/2),18));SendMessageW(window_,WM_LBUTTONUP,0,MAKELPARAM(int(Renderer::canvasWidth/2),18));pass=state_==IslandState::Compact&&!content_.pinned&&motion_.width.target()<=76;transition(IslandState::LiveActivity);pass=pass&&content_.live&&motion_.height.target()==154&&renderer_->hit(30,120)==Action::Overview;content_.hovered=Action::Overview;interaction_=InteractionState::Hover;SendMessageW(window_,WM_TIMER,19,0);pass=pass&&state_==IslandState::Expanded&&!content_.live;pass=pass&&!content_.live;settings_.compactWidth=560;applySettings();perform(Action::Close);pass=pass&&state_==IslandState::Compact;SetTimer(window_,13,100,nullptr);break;}
            case 11:{settings_.uiMode=1;applySettings();pass=motion_.width.target()>=560;perform(Action::WidthDown);pass=pass&&settings_.compactWidth==520;perform(Action::CompactVolumeToggle);perform(Action::CompactClockToggle);pass=pass&&!settings_.compactVolume&&settings_.compactClock;perform(Action::ShelfPeekToggle);pass=pass&&!settings_.shelfPeek;SetTimer(window_,13,100,nullptr);break;}
            case 12:{settings_.autoHide=true;settings_.collapseDelay=300;content_.pinned=false;interaction_=InteractionState::Rest;transition(IslandState::Compact);autoHide_=AutoHide{};
                MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);SetCursorPos((mi.rcMonitor.left+mi.rcMonitor.right)/2,(mi.rcMonitor.top+mi.rcMonitor.bottom)/2);autoHideTick();pass=!autoHide_.hidden;SetTimer(window_,13,500,nullptr);break;}
            case 13:{autoHideTick();pass=autoHide_.hidden&&motion_.slide.target()==1;RECT w{};GetWindowRect(window_,&w);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);
                SetCursorPos((w.left+w.right)/2,mi.rcMonitor.top+6);autoHideTick();pass=pass&&autoHide_.hidden;
                SetCursorPos((w.left+w.right)/2,mi.rcMonitor.top);autoHideTick();pass=pass&&!autoHide_.hidden&&motion_.slide.target()==0;SetTimer(window_,13,900,nullptr);break;}
            case 14:{RECT box{};pass=GetWindowRgnBox(window_,&box)!=NULLREGION&&box.bottom-box.top>int(20*dpi_/96);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);
                SetCursorPos((mi.rcMonitor.left+mi.rcMonitor.right)/2,(mi.rcMonitor.top+mi.rcMonitor.bottom)/2);autoHideTick();SetTimer(window_,13,1400,nullptr);break;}
            case 15:autoHideTick();SetTimer(window_,13,settings_.collapseDelay+500,nullptr);return 0;
            case 16:{autoHideTick();SendMessageW(window_,WM_TIMER,SettleTimer,0);RECT box{};int kind=GetWindowRgnBox(window_,&box);pass=autoHide_.hidden&&(kind==NULLREGION||box.bottom<=0||box.top>=box.bottom);settings_.autoHide=false;break;}
            }
            if(!pass||scenarioStep_==17){SetCursorPos(qaCursor_.x,qaCursor_.y);auto result=pass?"PASS native hover open, leave close, disabled hover, navigation, timer actions, hit targets, edge/scale/theme, OLE drop and shelf clear, navigation reorder, metric choices, detail toggles, app-switch collapse, pin and drag protection, precision seeking and cancel, hover-only surface, mini/live modes, wide compact settings, auto-hide tucks away (click-through region) and reveals only at its edge (visible region)":"FAIL native interaction regression";store_.submit([dir=store_.directory,result,step=scenarioStep_,state=int(state_),interaction=int(interaction_),width=motion_.width.sample(seconds()).position]{std::ofstream(dir/L"ui-test.txt")<<"stage "<<step<<" state "<<state<<" interaction "<<interaction<<" width "<<width<<": "<<result<<'\n';});PostMessageW(window_,WM_CLOSE,0,0);}return 0;
        }
        if(w==19){KillTimer(window_,19);if(content_.live&&content_.hovered==Action::Overview&&interaction_==InteractionState::Hover)perform(Action::Overview);}
        if(w==7){KillTimer(window_,7);if(settings_.autoHide&&!pointerOffEdge()){edgeHold_=true;return 0;}if(settings_.hoverOpen&&interaction_==InteractionState::Hover&&state_==IslandState::Compact)transition(settings_.uiMode==2?IslandState::Expanded:IslandState::LiveActivity);}
        if(w==8){KillTimer(window_,8);if(interaction_==InteractionState::Rest&&!content_.pinned)transition(IslandState::Compact);}
        if(w==9){if(content_.focus.tick(seconds())){content_.page=Page::Focus;events_.publish({ActivityKind::Timer,"timer",70,0,2,8},seconds());presentActivity();}if(IsWindowVisible(window_))refresh();clockTimer();}
        if(w==5){
            KillTimer(window_,5);
            WNDCLASSW matteClass{};matteClass.lpfnWndProc=[](HWND h,UINT m,WPARAM w,LPARAM l)->LRESULT{if(m==WM_PAINT&&GetPropW(h,L"pattern")){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);const COLORREF colors[]={RGB(255,94,98),RGB(255,184,76),RGB(84,214,160),RGB(76,146,255),RGB(170,110,255),RGB(245,245,245),RGB(28,32,40)};int band=std::max<LONG>(1,r.right/14);for(int i=0;i*band<r.right;++i){RECT b{i*band,0,(i+1)*band,r.bottom};HBRUSH brush=CreateSolidBrush(colors[i%7]);FillRect(dc,&b,brush);DeleteObject(brush);}EndPaint(h,&ps);return 0;}return DefWindowProcW(h,m,w,l);};matteClass.hInstance=instance_;matteClass.hbrBackground=CreateSolidBrush(RGB(34,40,50));matteClass.lpszClassName=L"NexusIsland.QAMatte";RegisterClassW(&matteClass);
            RECT r{};GetWindowRect(window_,&r);HWND matte=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,matteClass.lpszClassName,L"Nexus QA matte",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance_,nullptr);
            qaMatte_=matte;qaBrush_=matteClass.hbrBackground;if(showcasePattern_){SetPropW(matte,L"pattern",HANDLE(1));InvalidateRect(matte,nullptr,TRUE);}
            ShowWindow(matte,SW_SHOWNOACTIVATE);SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);UpdateWindow(matte);SetTimer(window_,6,250,nullptr);
        }
        if(w==6){
            KillTimer(window_,6);DwmFlush();captureWindow(window_,store_.directory/L"island-capture.png");
            if(motionStudy_){SetTimer(window_,16,80,nullptr);return 0;}
            DestroyWindow(qaMatte_);qaMatte_=nullptr;UnregisterClassW(L"NexusIsland.QAMatte",instance_);DeleteObject(qaBrush_);qaBrush_=nullptr;
            if(lab_)captureWindow(lab_,store_.directory/L"lab-capture.png",true);
        }
        if(w==SettleTimer){double t=seconds();if(motion_.width.settled(t)&&motion_.height.settled(t)&&motion_.dragX.settled(t)&&motion_.dragY.settled(t)&&motion_.slide.settled(t)){updateRegion(false);renderer_->animate(motion_,t);KillTimer(window_,SettleTimer);}else updateRegion(true);}
        if(w==ActivityTimer&&events_.tick(seconds())){if(!events_.active()){KillTimer(window_,ActivityTimer);content_.activity.clear();levelIndicator();refresh();if(interaction_==InteractionState::Rest&&!content_.pinned)transition(IslandState::Compact);}else presentActivity();}
        if(w==HudTimer)updateHud();
        if(w==ScenarioTimer){
            ++scenarioStep_;double now=seconds();
            events_.publish({ActivityKind::Volume,"benchmark-volume",40,double(scenarioStep_%101),.1,.3},now);
            motion_.volume.retarget((scenarioStep_%101)/100.,now,{1,550,42});
            transition(scenarioStep_%2?IslandState::Expanded:IslandState::Compact);
            if(scenarioStep_>=200)finishBenchmark();
        }return 0;
    case WM_CLOSE:DestroyWindow(window_);return 0;
    case WM_DESTROY:settingsWindow_.reset();RevokeDragDrop(window_);PostQuitMessage(0);return 0;
    }
    static UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");if(m==taskbarCreated)Shell_NotifyIconW(NIM_ADD,&tray_);
    return DefWindowProcW(window_,m,w,l);
}
void IslandWindow::showMenu(){
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Expand / collapse");AppendMenuW(menu,MF_STRING,2,L"Animation Lab");AppendMenuW(menu,MF_STRING,3,L"Settings");
    AppendMenuW(menu,MF_STRING|(hud_?MF_CHECKED:0),4,L"Performance HUD");AppendMenuW(menu,MF_STRING|(debug_?MF_CHECKED:0),5,L"Visual bounds");
    AppendMenuW(menu,MF_STRING,6,L"Open local logs");AppendMenuW(menu,MF_STRING,7,L"Clear local logs");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,9,L"Exit Arnav Island");
    POINT p;GetCursorPos(&p);SetForegroundWindow(window_);int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,window_,nullptr);DestroyMenu(menu);PostMessageW(window_,WM_NULL,0,0);
    switch(command){case 1:transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);break;case 2:openLab();break;case 3:perform(Action::Settings);break;
    case 4:hud_=!hud_;if(hud_){openLab();SetTimer(window_,HudTimer,1000,nullptr);updateHud();}else{KillTimer(window_,HudTimer);if(lab_)SetWindowTextW(lab_,L"Arnav Island / Animation Lab");}break;
    case 5:debug_=!debug_;renderer_->redraw(content_,debug_);break;
    case 6:ShellExecuteW(nullptr,L"open",store_.directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case 7:store_.clear();break;case 9:PostMessageW(window_,WM_CLOSE,0,0);break;}
}
void IslandWindow::openLab(bool settings){
    if(lab_){ShowWindow(lab_,SW_SHOW);SetForegroundWindow(lab_);return;}
    labMode_=!settings;
    store_.log("Info",settings?"settings_opened":"animation_lab_opened");
    WNDCLASSW wc{};wc.hInstance=instance_;wc.lpfnWndProc=labProcedure;wc.lpszClassName=L"ArnavIsland.Lab";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));RegisterClassW(&wc);
    lab_=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,settings?L"Arnav Island / Settings":L"Arnav Island / Animation Lab",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,180,220,730,650,nullptr,nullptr,instance_,this);
    BOOL dark=TRUE;DwmSetWindowAttribute(lab_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    auto control=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int id,int x,int y,int width,int height){if(wcscmp(cls,L"BUTTON")==0&&style==BS_PUSHBUTTON)style=BS_OWNERDRAW;HWND h=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,width,height,lab_,reinterpret_cast<HMENU>(INT_PTR(id)),instance_,nullptr);if(style==BS_AUTOCHECKBOX)SetWindowTheme(h,L"",L"");SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;};
    auto combo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,100,32,173,270,200);
    for(auto name:{L"Balanced",L"Fluid",L"Playful",L"Snappy",L"Calm"})SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));SendMessageW(combo,CB_SETCURSEL,settings_.preset,0);
    control(L"BUTTON",L"Expand",BS_PUSHBUTTON,101,32,232,142,38);control(L"BUTTON",L"Collapse",BS_PUSHBUTTON,102,184,232,142,38);control(L"BUTTON",L"Interrupt / reverse",BS_PUSHBUTTON,103,336,232,174,38);control(L"BUTTON",L"Media shape",BS_PUSHBUTTON,104,520,232,146,38);
    auto stiffness=control(TRACKBAR_CLASSW,L"",TBS_HORZ,110,32,325,290,30);SendMessageW(stiffness,TBM_SETRANGE,TRUE,MAKELPARAM(100,1000));SendMessageW(stiffness,TBM_SETPOS,TRUE,int(motion_.body.stiffness));
    auto damping=control(TRACKBAR_CLASSW,L"",TBS_HORZ,111,366,325,300,30);SendMessageW(damping,TBM_SETRANGE,TRUE,MAKELPARAM(15,90));SendMessageW(damping,TBM_SETPOS,TRUE,int(motion_.body.damping));
    auto mass=control(TRACKBAR_CLASSW,L"",TBS_HORZ,112,32,403,290,30);SendMessageW(mass,TBM_SETRANGE,TRUE,MAKELPARAM(5,30));SendMessageW(mass,TBM_SETPOS,TRUE,int(motion_.body.mass*10));
    control(L"BUTTON",L"Impulse",BS_PUSHBUTTON,113,366,400,142,34);control(L"BUTTON",L"Stall UI 300 ms",BS_PUSHBUTTON,114,518,400,148,34);
    control(L"BUTTON",L"Reduce motion",BS_AUTOCHECKBOX,120,32,472,160,26);SendDlgItemMessageW(lab_,120,BM_SETCHECK,settings_.reduceMotion,0);
    control(L"BUTTON",L"Hide in fullscreen",BS_AUTOCHECKBOX,121,214,472,175,26);SendDlgItemMessageW(lab_,121,BM_SETCHECK,settings_.hideFullscreen,0);
    control(L"BUTTON",L"Reset defaults",BS_PUSHBUTTON,122,32,540,150,36);control(L"BUTTON",L"Save preferences",BS_PUSHBUTTON,123,506,540,160,36);
    ShowWindow(lab_,SW_SHOW);SetForegroundWindow(lab_);
}
void IslandWindow::drawLab(HWND h){
    PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HBRUSH bg=CreateSolidBrush(RGB(14,18,25));FillRect(dc,&r,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);
    auto label=[&](int x,int y,const std::wstring& text,int size,COLORREF color,int weight=FW_NORMAL){HFONT f=uiFont(size,weight);auto old=SelectObject(dc,f);SetTextColor(dc,color);TextOutW(dc,x,y,text.c_str(),int(text.size()));SelectObject(dc,old);DeleteObject(f);};
    label(32,25,L"A R N A V   /   S T U D I O",13,RGB(143,219,197),FW_SEMIBOLD);
    label(30,58,labMode_?L"Make motion feel inevitable.":L"A space that feels like yours.",30,RGB(240,244,251),FW_SEMIBOLD);
    label(32,103,L"Retarget freely. One object, always in motion.",16,RGB(145,159,180));
    label(32,149,L"MOTION CHARACTER",12,RGB(177,190,207),FW_SEMIBOLD);
    label(32,299,L"STIFFNESS  /  "+std::to_wstring(int(motion_.body.stiffness)),13,RGB(177,190,207));
    label(366,299,L"DAMPING  /  "+std::to_wstring(int(motion_.body.damping)),13,RGB(177,190,207));
    label(32,378,L"MASS  /  "+std::to_wstring(motion_.body.mass).substr(0,4),13,RGB(177,190,207));
    label(366,378,L"CONTINUITY TESTS",12,RGB(177,190,207),FW_SEMIBOLD);
    EndPaint(h,&ps);
}
void IslandWindow::labCommand(int id){
    switch(id){
    case 100:settings_.preset=int(SendDlgItemMessageW(lab_,100,CB_GETCURSEL,0,0));motion_.body=preset(MotionPreset(settings_.preset));
        SendDlgItemMessageW(lab_,110,TBM_SETPOS,TRUE,int(motion_.body.stiffness));SendDlgItemMessageW(lab_,111,TBM_SETPOS,TRUE,int(motion_.body.damping));SendDlgItemMessageW(lab_,112,TBM_SETPOS,TRUE,int(motion_.body.mass*10));animate();break;
    case 101:transition(IslandState::Expanded);break;case 102:transition(IslandState::Compact);break;case 103:transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);break;
    case 104:transition(IslandState::Media);break;
    case 113:{double now=seconds();motion_.dragY.reset(motion_.dragY.sample(now).position,now,350);motion_.dragY.retarget(0,now,motion_.body);animate();break;}
    case 114:transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);Sleep(300);break;
    case 120:settings_.reduceMotion=SendDlgItemMessageW(lab_,120,BM_GETCHECK,0,0)==BST_CHECKED;motion_.reduced=settings_.reduceMotion;animate();break;
    case 121:settings_.hideFullscreen=SendDlgItemMessageW(lab_,121,BM_GETCHECK,0,0)==BST_CHECKED;fullscreen();break;
    case 122:settings_={};motion_.body=preset(MotionPreset::Balanced);motion_.reduced=false;SendDlgItemMessageW(lab_,100,CB_SETCURSEL,0,0);SendDlgItemMessageW(lab_,120,BM_SETCHECK,FALSE,0);SendDlgItemMessageW(lab_,121,BM_SETCHECK,TRUE,0);labCommand(100);break;
    case 123:store_.save(settings_,settingsFile_);store_.log("Info","settings_saved");SetWindowTextW(lab_,L"Arnav Island / Preferences saved");break;
    }
    InvalidateRect(lab_,nullptr,FALSE);
}
LRESULT CALLBACK IslandWindow::labProcedure(HWND h,UINT m,WPARAM w,LPARAM l){
    auto self=reinterpret_cast<IslandWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<IslandWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(h,m,w,l);
    try{
    if(m==WM_PAINT){self->drawLab(h);return 0;}
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN){static HBRUSH background=CreateSolidBrush(RGB(14,18,25));SetTextColor(reinterpret_cast<HDC>(w),RGB(211,221,234));SetBkColor(reinterpret_cast<HDC>(w),RGB(14,18,25));return reinterpret_cast<LRESULT>(background);}
    if(m==WM_DRAWITEM){auto d=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(d->CtlType==ODT_BUTTON){
        HBRUSH background=CreateSolidBrush(RGB(14,18,25));FillRect(d->hDC,&d->rcItem,background);DeleteObject(background);
        bool pressed=d->itemState&ODS_SELECTED,accent=d->CtlID==123;
        COLORREF color=accent?(pressed?RGB(107,177,160):RGB(145,219,197)):(pressed?RGB(42,53,67):RGB(29,37,49));
        HBRUSH brush=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,accent?color:RGB(51,63,79));auto oldBrush=SelectObject(d->hDC,brush),oldPen=SelectObject(d->hDC,pen);
        RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,10,10);SelectObject(d->hDC,oldBrush);SelectObject(d->hDC,oldPen);DeleteObject(brush);DeleteObject(pen);
        wchar_t label[128]{};GetWindowTextW(d->hwndItem,label,128);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,accent?RGB(10,27,23):RGB(229,235,244));HFONT font=uiFont(14,FW_MEDIUM);auto oldFont=SelectObject(d->hDC,font);DrawTextW(d->hDC,label,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(d->hDC,oldFont);DeleteObject(font);
        if(d->itemState&ODS_FOCUS){RECT focus=d->rcItem;InflateRect(&focus,-4,-4);DrawFocusRect(d->hDC,&focus);}return TRUE;
    }}
    if(m==WM_COMMAND&&(HIWORD(w)==BN_CLICKED||HIWORD(w)==CBN_SELCHANGE)){self->labCommand(LOWORD(w));return 0;}
    if(m==WM_HSCROLL){self->motion_.body.stiffness=SendDlgItemMessageW(h,110,TBM_GETPOS,0,0);self->motion_.body.damping=SendDlgItemMessageW(h,111,TBM_GETPOS,0,0);self->motion_.body.mass=SendDlgItemMessageW(h,112,TBM_GETPOS,0,0)/10.;self->animate();InvalidateRect(h,nullptr,FALSE);return 0;}
    if(m==WM_CLOSE){DestroyWindow(h);return 0;}
    if(m==WM_DESTROY){self->lab_=nullptr;return 0;}
    if(m==WM_KEYDOWN&&w==VK_ESCAPE){DestroyWindow(h);return 0;}
    }catch(...){self->store_.log("Error","lab_operation_failed");}
    return DefWindowProcW(h,m,w,l);
}
void IslandWindow::updateHud(){
    if(!lab_)return;PROCESS_MEMORY_COUNTERS p{sizeof(p)};GetProcessMemoryInfo(GetCurrentProcess(),&p,sizeof(p));
    DWM_TIMING_INFO timing{sizeof(timing)};HRESULT timingResult=DwmGetCompositionTimingInfo(nullptr,&timing);
    double refresh=SUCCEEDED(timingResult)&&timing.rateRefresh.uiDenominator?double(timing.rateRefresh.uiNumerator)/timing.rateRefresh.uiDenominator:0;
    std::wostringstream title;title<<L"Arnav HUD | DWM "<<refresh<<L" Hz (not app FPS) | RAM "<<p.WorkingSetSize/1048576<<L" MB | commits "<<renderer_->commits<<L" | queue "<<events_.depth()<<L" | state "<<int(state_);
    SetWindowTextW(lab_,title.str().c_str());
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
void IslandWindow::refresh(){content_.settings=settings_;content_.reducedMotion=motion_.reduced;bool compact=state_==IslandState::Compact;renderer_->redraw(content_,debug_,compact);contentDirty_=compact;}
void IslandWindow::clockTimer(){bool visible=state_!=IslandState::Compact&&IsWindowVisible(window_);bool request=visible&&!content_.live&&(content_.page==Page::Overview||content_.page==Page::System);if(request!=systemRequested_){systemRequested_=request;if(request)SetTimer(window_,10,400,nullptr);else{KillTimer(window_,10);if(system_)system_->setActive(false);}}if(content_.focus.running||(visible&&(content_.live||content_.page==Page::Media)&&content_.playback.playing))SetTimer(window_,9,1000,nullptr);else KillTimer(window_,9);if(settings_.compactClock&&IsWindowVisible(window_))SetTimer(window_,18,60000,nullptr);else KillTimer(window_,18);updateProviders();
    if(battery_)battery_->setFast((state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==1)||(content_.card&&content_.notice.kind>=3));
    bool hideActive=settings_.autoHide&&IsWindowVisible(window_)&&!testing_;if(hideActive!=autoHideTimer_){autoHideTimer_=hideActive;if(hideActive)SetTimer(window_,23,33,nullptr);else{KillTimer(window_,23);if(autoHide_.hidden||motion_.slide.target()!=0){autoHide_.hidden=false;if(motion_.reduced)motion_.slide.reset(0,seconds());else motion_.slide.retarget(0,seconds(),{.9,420,26});if(renderer_)animate();}}}}
Action IslandWindow::hit(LPARAM l){if(state_==IslandState::Compact)return Action::None;double now=seconds();auto origin=bodyOrigin(motion_.width.sample(now).position,motion_.height.sample(now).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);return renderer_->hit(float(GET_X_LPARAM(l)*96/dpi_-origin.x-motion_.dragX.sample(now).position),float(GET_Y_LPARAM(l)*96/dpi_-origin.y-motion_.dragY.sample(now).position-motion_.contentShift.sample(now).position));}
void IslandWindow::applySettings(bool rebuild,bool reposition){
    motion_.body=preset(MotionPreset(settings_.preset));motion_.edge=settings_.edge;motion_.compactWidth=settings_.uiMode==0?72:settings_.compactWidth;motion_.corner=settings_.corner;BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);motion_.reduced=settings_.reduceMotion||!enabled;
    DWORD light=0,size=sizeof(light);if(settings_.theme==2)RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);content_.light=settings_.theme==1||(settings_.theme==2&&light);content_.expanded=state_!=IslandState::Compact;
    content_.blur=GlassBackdrop::effectsEnabled();if(rebuild){renderer_.reset();position();renderer_=std::make_unique<Renderer>();renderer_->initialize(window_,dpi_);}else if(reposition)position();motion_.edge=settings_.edge;motion_.compactWidth=settings_.uiMode==0?72:settings_.compactWidth;refresh();animate();
    if(settingsWindow_&&settingsWindow_->open())settingsWindow_->update(settings_,settingsContext(),settingsSequence_);
}
void IslandWindow::feedback(Action action,float px,float py,bool press){
    renderer_->iconFeedback(action,press,settings_.animatedIcons&&!motion_.reduced);
    double now=seconds();auto it=std::find_if(renderer_->targets.begin(),renderer_->targets.end(),[&](auto& target){return target.action==action&&target.enabled;});
    bool visible=it!=renderer_->targets.end()&&state_!=IslandState::Compact,changed=false;
    auto aim=[&](Spring& spring,double value){if(std::abs(spring.target()-value)>.25){changed=true;if(motion_.reduced)spring.reset(value,now);else spring.retarget(value,now,{.65,520,33});}};
    aim(motion_.hoverOpacity,visible?1:0);if(visible){auto origin=bodyOrigin(motion_.width.sample(now).position,motion_.height.sample(now).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);double dx=0,dy=0;if(settings_.magnetic&&!motion_.reduced&&px){dx=std::clamp((px*96/dpi_-origin.x-20-it->x-it->width/2)*.1,-2.,2.);dy=std::clamp((py*96/dpi_-origin.y-(content_.card?16:content_.live?20:38)-it->y-it->height/2)*.1,-2.,2.);}aim(motion_.hoverX,20+it->x+dx+(press?1.5:0));aim(motion_.hoverY,(content_.card?16:content_.live?20:38)+it->y+dy+(press?1:0));aim(motion_.hoverW,it->width-(press?3:0));aim(motion_.hoverH,it->height-(press?2:0));}if(changed)renderer_->animate(motion_,now);
}
void IslandWindow::dragShelf(size_t index){if(index>=content_.shelf.size())return;auto item=content_.shelf[index];auto data=shelfData(item);shelfDragImage(data.Get(),item.preview);ComPtr<ShelfDragSource> source;source.Attach(new ShelfDragSource);DWORD effect=0;DoDragDrop(data.Get(),source.Get(),DROPEFFECT_COPY,&effect);content_.dropHover=false;refresh();}
void IslandWindow::setVolumeAt(LPARAM point){if(!audio_||!audio_->available)return;double now=seconds();auto origin=bodyOrigin(motion_.width.sample(now).position,motion_.height.sample(now).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);int target=volumeAt(GET_X_LPARAM(point)*96/dpi_,origin.x+motion_.dragX.sample(now).position+58,262);if(target!=audio_->value)audio_->setVolume(target);}
void IslandWindow::perform(Action a){
    double now=seconds();bool save=false,rebuild=false;
    if(a==Action::Settings){openSettings();return;}
    if(a>=Action::Overview&&a<=Action::Focus){if(Page next=Page(int(a)-int(Action::Overview));content_.page!=next&&state_==IslandState::Expanded&&!motion_.reduced){auto slot=[&](Page p){return int(std::find(settings_.navigation.begin(),settings_.navigation.end(),int(p))-settings_.navigation.begin());};motion_.swipe.reset(slot(next)>slot(content_.page)?22.:-22.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.page=Page(int(a)-int(Action::Overview));transition(IslandState::Expanded);}
        if(a==Action::Shelf||a==Action::Audio){content_.page=a==Action::Shelf?Page::Shelf:Page::Audio;transition(IslandState::Expanded);}
    if(inRange(a,Action::MixerSliderBase,Action::MixerMuteBase))return;
    if(inRange(a,Action::MixerMuteBase,Action::SessionBase)){size_t i=int(a)-int(Action::MixerMuteBase);if(mixer_&&i<content_.mixer.size()){auto& e=content_.mixer[i];e.muted=!e.muted;mixer_->setMute(e.pid,e.muted);}refresh();return;}
    if(inRange(a,Action::ClipBase,Action::CommandResultBase)){copyClip(size_t(content_.clipOffset+int(a)-int(Action::ClipBase)));return;}
    if(inRange(a,Action::CommandResultBase,Action::ActionEnd)){runCommand(size_t(int(a)-int(Action::CommandResultBase)));return;}
    if(inRange(a,Action::SessionBase,Action::DeviceConnectBase)){switchSession(int(a)-int(Action::SessionBase),true);return;}
    if(inRange(a,Action::DeviceConnectBase,Action::DeviceConnectEnd)){size_t i=int(a)-int(Action::DeviceConnectBase);if(bluetooth_&&i<content_.devices.size()){auto& d=content_.devices[i];bluetooth_->request(d.name,!d.connected);deviceRequest_=true;content_.deviceFeedback=(d.connected?L"Disconnecting ":L"Connecting ")+d.name+L"…";}refresh();return;}
    if(int(a)>=int(Action::DeviceBase)&&int(a)<int(Action::ShelfItemBase)){size_t index=int(a)-int(Action::DeviceBase);if(audio_&&index<content_.outputs.size()){if(settings_.directAudio){audio_->selectDevice(content_.outputs[index].id,true);content_.feedback=L"Switching output…";}else perform(Action::SoundSettings);}refresh();return;}
    switch(a){
        case Action::LayoutSlot:content_.layoutSlot=(content_.layoutSlot+1)%7;break;
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
    case Action::Edge:settings_.edge=1-settings_.edge;motion_.dragX.reset(0,now);motion_.dragY.reset(0,now);save=rebuild=true;break;
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
    case Action::ShelfClear:content_.shelf.clear();content_.shelfOffset=0;requestPreviews();break;
    case Action::ShelfFiles:content_.shelfTab=0;break;
    case Action::ShelfClipboard:content_.shelfTab=1;clipViews();break;
    case Action::ClipboardEnable:{Settings next=settings_;next.clipboardHistory=true;receiveSettings(next);content_.clipStatus=L"Clipboard history is on";content_.clipStatusUntil=seconds()+2.5;break;}
    case Action::ClipboardPause:clips_.paused=!clips_.paused;clipViews();content_.clipStatus=clips_.paused?L"Paused — new copies are not kept":L"Keeping new copies again";content_.clipStatusUntil=seconds()+2.5;break;
    case Action::ClipboardClear:clearClips();content_.clipStatus=L"Cleared";content_.clipStatusUntil=seconds()+2.5;break;
    case Action::CommandOpen:openCommand();return;
    case Action::Pin:content_.pinned=!content_.pinned;break;
    case Action::Close:content_.pinned=false;transition(IslandState::Compact);break;
    case Action::Play:if(media_&&content_.playback.canToggle)media_->control(1,content_.playback.source,content_.playback.id);break;
    case Action::Previous:if(media_&&content_.playback.canPrevious)media_->control(2,content_.playback.source,content_.playback.id);break;
    case Action::Next:if(media_&&content_.playback.canNext)media_->control(3,content_.playback.source,content_.playback.id);break;
    case Action::AudioApps:content_.audioTab=0;break;case Action::AudioOutputs:content_.audioTab=1;break;
    case Action::StatsSystem:case Action::StatsBattery:case Action::StatsDevices:content_.statsTab=int(a)-int(Action::StatsSystem);if(a==Action::StatsBattery&&battery_){battery_->refresh();if(content_.charging)renderer_->energize(motion_.reduced,true);}break;
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
    case Action::MotionPreset:settings_.preset=(settings_.preset+1)%5;motion_.body=preset(MotionPreset(settings_.preset));save=true;animate();break;
    case Action::Lab:openLab();break;
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
void IslandWindow::requestPreviews(const std::vector<ShelfItem>& incoming){if(!previews_)return;std::vector<std::wstring> paths;for(auto& item:content_.shelf)if(item.kind==ShelfItem::Kind::File)paths.push_back(item.value);for(auto& item:incoming)if(item.kind==ShelfItem::Kind::File&&std::find(paths.begin(),paths.end(),item.value)==paths.end())paths.push_back(item.value);previews_->request(std::move(paths));}
void IslandWindow::scrubAt(LPARAM point,bool begin){if(!content_.playback.canSeek||content_.page!=Page::Media)return;double now=seconds();auto origin=bodyOrigin(motion_.width.sample(now).position,motion_.height.sample(now).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);double x=GET_X_LPARAM(point)*96/dpi_-origin.x-20,y=GET_Y_LPARAM(point)*96/dpi_-origin.y-229;auto& s=content_.scrub;if(begin)s.begin(x,380,0,content_.playback.duration);else s.move(x,y,380,0,content_.playback.duration);s.value=std::clamp(s.value,content_.playback.seekMin,content_.playback.seekMax);refresh();}
void IslandWindow::endScrub(bool commit){if(!content_.scrub.active)return;double value=content_.scrub.value;content_.scrub.active=false;if(commit&&media_)media_->seek(value,content_.playback);refresh();}
}
