#include "Common/Capture.h"
#include "IslandWindow.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <psapi.h>
#include <sstream>
#include <iomanip>
#include <array>
#include <fstream>

namespace nexus {
static IslandWindow* foregroundOwner=nullptr;
static constexpr UINT TrayMessage=WM_APP+1,FullscreenMessage=WM_APP+2;
static constexpr UINT_PTR SettleTimer=1,ActivityTimer=2,HudTimer=3,ScenarioTimer=4;
static HFONT uiFont(int height,int weight=FW_NORMAL){return CreateFontW(height,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");}
static BOOL CALLBACK collectMonitor(HMONITOR m,HDC,LPRECT,LPARAM p){reinterpret_cast<std::vector<HMONITOR>*>(p)->push_back(m);return TRUE;}

int IslandWindow::run(HINSTANCE instance,const std::wstring& cmd){
    instance_=instance;settings_=store_.load();motion_.body=preset(MotionPreset(settings_.preset));
    BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);motion_.reduced=settings_.reduceMotion||!animations;
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=L"NexusIsland.Surface";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);check(RegisterClassW(&wc)?S_OK:HRESULT_FROM_WIN32(GetLastError()));
    window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP|WS_EX_NOACTIVATE,wc.lpszClassName,L"Nexus Island",WS_POPUP,0,0,760,480,nullptr,nullptr,instance,this);
    if(!window_)throw std::runtime_error("Island window creation failed");
    position();renderer_=std::make_unique<Renderer>();renderer_->initialize(window_,dpi_);power(false);renderer_->redraw(content_);
    audio_=std::make_unique<AudioProvider>(window_);media_=std::make_unique<MediaProvider>(window_);
    for(auto id:{&GUID_ACDC_POWER_SOURCE,&GUID_BATTERY_PERCENTAGE_REMAINING}){
        auto registration=RegisterPowerSettingNotification(window_,id,DEVICE_NOTIFY_WINDOW_HANDLE);if(registration)powerNotifications_.push_back(registration);
    }
    tray_.cbSize=sizeof(tray_);tray_.hWnd=window_;tray_.uID=1;tray_.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray_.uCallbackMessage=TrayMessage;tray_.hIcon=LoadIcon(nullptr,IDI_APPLICATION);wcscpy_s(tray_.szTip,L"Nexus Island — right-click for controls");Shell_NotifyIconW(NIM_ADD,&tray_);
    foregroundOwner=this;foregroundHook_=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    ShowWindow(window_,SW_SHOWNOACTIVATE);animate();
    if(cmd.find(L"--expanded")!=std::wstring::npos)transition(IslandState::Expanded);
    if(cmd.find(L"--lab")!=std::wstring::npos)openLab();
    if(cmd.find(L"--benchmark")!=std::wstring::npos){benchmark_=true;benchmarkStart_=seconds();FILETIME c,e;GetProcessTimes(GetCurrentProcess(),&c,&e,&initialKernel_,&initialUser_);SetTimer(window_,ScenarioTimer,73,nullptr);}
    if(cmd.find(L"--capture")!=std::wstring::npos)SetTimer(window_,5,2800,nullptr); store_.log("Info","application_started_directcomposition");
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(lab_&&IsDialogMessageW(lab_,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}
    store_.log("Info","application_stopped");return int(msg.wParam);
}
IslandWindow::~IslandWindow(){
    foregroundOwner=nullptr;if(foregroundHook_)UnhookWinEvent(foregroundHook_);
    media_.reset();audio_.reset();for(auto h:powerNotifications_)UnregisterPowerSettingNotification(h);
    if(tray_.hWnd)Shell_NotifyIconW(NIM_DELETE,&tray_);
    renderer_.reset();if(lab_&&IsWindow(lab_))DestroyWindow(lab_);if(window_&&IsWindow(window_))DestroyWindow(window_);
}
void IslandWindow::position(){
    std::vector<HMONITOR> monitors;EnumDisplayMonitors(nullptr,nullptr,collectMonitor,reinterpret_cast<LPARAM>(&monitors));
    HMONITOR selected=MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY);
    if(settings_.monitor>0&&size_t(settings_.monitor)<=monitors.size())selected=monitors[settings_.monitor-1];
    MONITORINFO info{sizeof(info)};GetMonitorInfoW(selected,&info);
    dpi_=float(GetDpiForWindow(window_));float s=dpi_/96;
    int width=int(Renderer::canvasWidth*s),height=int(Renderer::canvasHeight*s);
    int x=(info.rcMonitor.left+info.rcMonitor.right-width)/2+int(settings_.horizontalOffset*s);
    x=std::clamp<int>(x,info.rcMonitor.left-width/2,info.rcMonitor.right-width/2);
    SetWindowPos(window_,HWND_TOPMOST,x,info.rcMonitor.top+int(settings_.verticalOffset*s),width,height,SWP_NOACTIVATE);
}
void IslandWindow::updateRegion(bool envelope){
    if(!window_)return;float s=dpi_/96;auto now=seconds();
    double width=motion_.width.sample(now).position,height=motion_.height.sample(now).position,radius=motion_.radius.sample(now).position;
    double x=(Renderer::canvasWidth-width)/2+motion_.dragX.sample(now).position,y=motion_.dragY.sample(now).position;
    HRGN region;
    if(envelope) region=CreateRectRgn(0,0,int(Renderer::canvasWidth*s),int(Renderer::canvasHeight*s));
    else region=CreateRoundRectRgn(int(x*s),int(y*s),int((x+width)*s)+1,int((y+height)*s)+1,int(radius*2*s),int(radius*2*s));
    if(!SetWindowRgn(window_,region,FALSE))DeleteObject(region);
}
void IslandWindow::animate(){
    double now=seconds();motion_.target(state_,now,interaction_==InteractionState::Hover,interaction_==InteractionState::Pressed);
    renderer_->animate(motion_,now);lastMotion_=now;updateRegion(true);SetTimer(window_,SettleTimer,1500,nullptr);
}
void IslandWindow::transition(IslandState s){state_=s;animate();if(lab_)InvalidateRect(lab_,nullptr,FALSE);}
void IslandWindow::power(bool notify){
    SYSTEM_POWER_STATUS p{};if(GetSystemPowerStatus(&p)){
        content_.battery=p.BatteryLifePercent<=100?p.BatteryLifePercent:-1;
        if(p.BatteryFlag&128)content_.battery=-1;
        content_.charging=p.ACLineStatus==1;
        if(notify){events_.publish({ActivityKind::Power,"power",content_.battery>=0&&content_.battery<10?100:30,double(content_.battery),.8,3},seconds());
            content_.headline=content_.charging?L"A little energy. A fresh start.":L"Ready to go with you.";
            content_.detail=content_.charging?L"Power connected. Settle in.":L"Running on battery power.";
            renderer_->redraw(content_,debug_);transition(IslandState::Expanded);SetTimer(window_,ActivityTimer,500,nullptr);
        }
    }
}
void CALLBACK IslandWindow::foregroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD){if(foregroundOwner)PostMessageW(foregroundOwner->window_,FullscreenMessage,0,0);}
void IslandWindow::fullscreen(){
    if(!settings_.hideFullscreen||benchmark_){ShowWindow(window_,SW_SHOWNOACTIVATE);return;}
    HWND fg=GetForegroundWindow();if(!fg||fg==window_||fg==lab_)return;
    wchar_t cls[128]{};GetClassNameW(fg,cls,128);if(wcscmp(cls,L"Progman")==0||wcscmp(cls,L"WorkerW")==0){ShowWindow(window_,SW_SHOWNOACTIVATE);return;}
    RECT r{};GetWindowRect(fg,&r);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(fg,MONITOR_DEFAULTTONEAREST),&mi);
    bool full=r.left<=mi.rcMonitor.left&&r.top<=mi.rcMonitor.top&&r.right>=mi.rcMonitor.right&&r.bottom>=mi.rcMonitor.bottom;
    bool same=MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST)==MonitorFromWindow(fg,MONITOR_DEFAULTTONEAREST);
    ShowWindow(window_,full&&same?SW_HIDE:SW_SHOWNOACTIVATE);
}
LRESULT CALLBACK IslandWindow::procedure(HWND h,UINT m,WPARAM w,LPARAM l){
    auto self=reinterpret_cast<IslandWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<IslandWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self)try{return self->message(m,w,l);}catch(const std::exception&){self->store_.log("Error","window_operation_failed");if(self->renderer_)PostMessageW(h,WM_CLOSE,0,0);}
    return DefWindowProcW(h,m,w,l);
}
LRESULT IslandWindow::message(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(window_,&ps);EndPaint(window_,&ps);return 0;}
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_NCHITTEST:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window_,&p);double t=seconds(),s=dpi_/96;
        double width=motion_.width.sample(t).position,height=motion_.height.sample(t).position;
        double x=p.x/s-(Renderer::canvasWidth-width)/2-motion_.dragX.sample(t).position,y=p.y/s-motion_.dragY.sample(t).position;
        return x>=0&&x<=width&&y>=0&&y<=height?HTCLIENT:HTTRANSPARENT;}
    case WM_MOUSEMOVE:{
        if(interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging){
            POINT p{};GetCursorPos(&p);double dx=(p.x-down_.x)*96/dpi_,dy=(p.y-down_.y)*96/dpi_;
            if(std::abs(dx)+std::abs(dy)>4){interaction_=InteractionState::Dragging;double now=seconds();double dt=now-dragTime_;
                if(dt>.002)dragVelocity_=std::clamp((p.y-lastPointer_.y)*96/dpi_/dt,-1600.,1600.);
                motion_.dragX.reset(rubberBand(dx,100),now);motion_.dragY.reset(rubberBand(std::max(0.,dy),100),now);
                renderer_->animate(motion_,now);dragTime_=now;lastPointer_=p;}
        }else if(interaction_==InteractionState::Rest){interaction_=InteractionState::Hover;TRACKMOUSEEVENT e{sizeof(e),TME_LEAVE,window_,0};TrackMouseEvent(&e);animate();}return 0;}
    case WM_MOUSELEAVE:if(interaction_==InteractionState::Hover){interaction_=InteractionState::Rest;animate();}return 0;
    case WM_LBUTTONDOWN:interaction_=InteractionState::Pressed;GetCursorPos(&down_);lastPointer_=down_;dragTime_=seconds();SetCapture(window_);animate();return 0;
    case WM_LBUTTONUP:{bool dragged=interaction_==InteractionState::Dragging;ReleaseCapture();interaction_=InteractionState::Hover;
        if(dragged){double now=seconds();auto pos=motion_.dragY.sample(now).position;motion_.dragY.reset(pos,now,dragVelocity_*.3);motion_.dragY.retarget(0,now,motion_.body);motion_.dragX.retarget(0,now,motion_.body);animate();}
        else transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);return 0;}
    case WM_CAPTURECHANGED:if(interaction_==InteractionState::Pressed||interaction_==InteractionState::Dragging){interaction_=InteractionState::Rest;motion_.dragX.retarget(0,seconds(),motion_.body);motion_.dragY.retarget(0,seconds(),motion_.body);animate();}return 0;
    case WM_RBUTTONUP:showMenu();return 0;
    case WM_MOUSEWHEEL:if(audio_&&audio_->available)audio_->setVolume(audio_->value+(GET_WHEEL_DELTA_WPARAM(w)>0?2:-2));return 0;
    case MediaMessage:if(media_){auto s=media_->snapshot();content_.media=s.title;content_.artist=s.artist;renderer_->redraw(content_,debug_);store_.log("Info",s.available?"media_session_connected":"media_no_active_session_or_unavailable");}return 0; case AudioMessage:if(audio_){content_.volume=audio_->value;content_.muted=audio_->muted;motion_.volume.retarget(content_.muted?0:content_.volume/100.,seconds(),{1,550,42});
        if(w){events_.publish({ActivityKind::Volume,"volume",40,double(content_.volume),.5,2},seconds());content_.headline=content_.muted?L"A moment of quiet.":L"Sound, just where you want it.";content_.detail=audio_->available?L"System output volume":L"Audio output unavailable";renderer_->redraw(content_,debug_);transition(IslandState::Expanded);SetTimer(window_,ActivityTimer,500,nullptr);}
        else{renderer_->redraw(content_,debug_);renderer_->animate(motion_,seconds());}}return 0;
    case WM_POWERBROADCAST:if(w==PBT_POWERSETTINGCHANGE)power(true);return TRUE;
    case FullscreenMessage:fullscreen();return 0;
    case WM_DISPLAYCHANGE:position();animate();return 0;
    case WM_DPICHANGED:if(renderer_){dpi_=float(HIWORD(w));renderer_=std::make_unique<Renderer>();renderer_->initialize(window_,dpi_);position();renderer_->redraw(content_,debug_);animate();}return 0;
    case WM_SETTINGCHANGE:{BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);motion_.reduced=settings_.reduceMotion||!enabled;return 0;}
    case TrayMessage:if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU)showMenu();else if(l==WM_LBUTTONDBLCLK)openLab(true);return 0;
    case WM_TIMER:
        if(w==5){KillTimer(window_,5);captureWindow(window_,store_.directory/L"island-capture.png");if(lab_)captureWindow(lab_,store_.directory/L"lab-capture.png");} if(w==SettleTimer){updateRegion(false);KillTimer(window_,SettleTimer);}
        if(w==ActivityTimer&&events_.tick(seconds())){if(!events_.active()){KillTimer(window_,ActivityTimer);if(interaction_==InteractionState::Rest)transition(IslandState::Compact);}}
        if(w==HudTimer)updateHud();
        if(w==ScenarioTimer){
            ++scenarioStep_;double now=seconds();
            events_.publish({ActivityKind::Volume,"benchmark-volume",40,double(scenarioStep_%101),.1,.3},now);
            motion_.volume.retarget((scenarioStep_%101)/100.,now,{1,550,42});
            transition(scenarioStep_%2?IslandState::Expanded:IslandState::Compact);
            if(scenarioStep_>=200)finishBenchmark();
        }return 0;
    case WM_CLOSE:DestroyWindow(window_);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    static UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");if(m==taskbarCreated)Shell_NotifyIconW(NIM_ADD,&tray_);
    return DefWindowProcW(window_,m,w,l);
}
void IslandWindow::showMenu(){
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Expand / collapse");AppendMenuW(menu,MF_STRING,2,L"Animation Lab");AppendMenuW(menu,MF_STRING,3,L"Settings");
    AppendMenuW(menu,MF_STRING|(hud_?MF_CHECKED:0),4,L"Performance HUD");AppendMenuW(menu,MF_STRING|(debug_?MF_CHECKED:0),5,L"Visual bounds");
    AppendMenuW(menu,MF_STRING,6,L"Open local logs");AppendMenuW(menu,MF_STRING,7,L"Clear local logs");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,9,L"Exit Nexus Island");
    POINT p;GetCursorPos(&p);SetForegroundWindow(window_);int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,window_,nullptr);DestroyMenu(menu);PostMessageW(window_,WM_NULL,0,0);
    switch(command){case 1:transition(state_==IslandState::Compact?IslandState::Expanded:IslandState::Compact);break;case 2:openLab();break;case 3:openLab(true);break;
    case 4:hud_=!hud_;if(hud_){openLab();SetTimer(window_,HudTimer,1000,nullptr);updateHud();}else{KillTimer(window_,HudTimer);if(lab_)SetWindowTextW(lab_,L"Nexus Island / Animation Lab");}break;
    case 5:debug_=!debug_;renderer_->redraw(content_,debug_);break;
    case 6:ShellExecuteW(nullptr,L"open",store_.directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case 7:store_.clear();break;case 9:PostMessageW(window_,WM_CLOSE,0,0);break;}
}
void IslandWindow::openLab(bool settings){
    if(lab_){ShowWindow(lab_,SW_SHOW);SetForegroundWindow(lab_);return;}
    labMode_=!settings;
    WNDCLASSW wc{};wc.hInstance=instance_;wc.lpfnWndProc=labProcedure;wc.lpszClassName=L"NexusIsland.Lab";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    lab_=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,settings?L"Nexus Island / Settings":L"Nexus Island / Animation Lab",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,180,220,730,650,nullptr,nullptr,instance_,this);
    BOOL dark=TRUE;DwmSetWindowAttribute(lab_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    auto control=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int id,int x,int y,int width,int height){HWND h=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,width,height,lab_,reinterpret_cast<HMENU>(INT_PTR(id)),instance_,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;};
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
    label(32,25,L"N E X U S   /   S T U D I O",13,RGB(143,219,197),FW_SEMIBOLD);
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
    case 123:store_.save(settings_);store_.log("Info","settings_saved");SetWindowTextW(lab_,L"Nexus Island / Preferences saved");break;
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
    std::wostringstream title;title<<L"Nexus HUD | DWM "<<refresh<<L" Hz (not app FPS) | RAM "<<p.WorkingSetSize/1048576<<L" MB | commits "<<renderer_->commits<<L" | queue "<<events_.depth()<<L" | state "<<int(state_);
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


