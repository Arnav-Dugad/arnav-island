#include "IslandWindow.h"
#include "App/Version.h"
#include <shellapi.h>
#include <sstream>
#include <fstream>
#include <shellscalingapi.h>
namespace nexus {
namespace {
BOOL CALLBACK countMonitor(HMONITOR,HDC,LPRECT,LPARAM p){++*reinterpret_cast<int*>(p);return TRUE;}
std::string narrow(const std::wstring& w){std::string s;for(wchar_t c:w)s+=c<128?char(c):'?';return s;}
}
SettingsContext IslandWindow::settingsContext(){
    SettingsContext c;int monitors=0;EnumDisplayMonitors(nullptr,nullptr,countMonitor,reinterpret_cast<LPARAM>(&monitors));c.monitors=std::max(1,monitors);
    c.blur=GlassBackdrop::effectsEnabled();c.armoury=!content_.platform.armoury.empty();c.glassAvailable=renderer_&&renderer_->glassAvailable();c.version=appVersion;return c;
}
void IslandWindow::openSettings(int section){
    if(!settingsWindow_)settingsWindow_=std::make_unique<SettingsWindow>(window_);
    settingsWindow_->show(settings_,settingsContext(),section);store_.log("Info","settings_window_opened");
}
void IslandWindow::scheduleSave(){SetTimer(window_,20,350,nullptr);}
// Applies a complete preference set from the Settings window. Only the work a
// change needs is done: repositioning for offsets, rebuilding for DPI/edge/display.
void IslandWindow::receiveSettings(Settings next){
    Settings previous=settings_;double now=seconds();
    if(next.startAtLogin!=previous.startAtLogin){if(testing_)startupRequest_=next.startAtLogin;else if(!startup::apply(next.startAtLogin)){next.startAtLogin=previous.startAtLogin;content_.feedback=L"Windows could not update sign-in settings";}}
    const bool monitor=next.monitor!=previous.monitor;if(monitor){displays_.remember(currentDisplay_,previous);selectDisplay_=true;}
    const bool rebuild=monitor||next.scale!=previous.scale||next.edge!=previous.edge;
    const bool reposition=next.verticalOffset!=previous.verticalOffset||next.horizontalOffset!=previous.horizontalOffset||next.glassy()!=previous.glassy()||next.compactWidth!=previous.compactWidth;
    settings_=next;if(next.edge!=previous.edge){motion_.dragX.reset(0,now);motion_.dragY.reset(0,now);}
    if(!monitor)displays_.remember(currentDisplay_,settings_);
    applySettings(rebuild,reposition);if(battery_)battery_->setHistory(settings_.batteryHistory);if(next.hideFullscreen!=previous.hideFullscreen)fullscreen();clockTimer();scheduleSave();
}
void IslandWindow::settingsAction(SettingAction action){
    switch(action){
    case SettingAction::OpenLab:openLab();break;
    case SettingAction::ResetAll:{Settings next;next.startAtLogin=settings_.startAtLogin;receiveSettings(next);store_.log("Info","settings_reset");break;}
    case SettingAction::ResetLayout:{Settings next=settings_;next.navigation=defaultNavigation;next.homeMetrics=defaultMetrics;content_.layoutSlot=0;receiveSettings(next);break;}
    case SettingAction::OpenLogs:ShellExecuteW(nullptr,L"open",store_.directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case SettingAction::ClearLogs:store_.clear();break;
    case SettingAction::DisplaySettings:ShellExecuteW(nullptr,L"open",L"ms-settings:display",nullptr,nullptr,SW_SHOWNORMAL);break;
    case SettingAction::SoundSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:sound",nullptr,nullptr,SW_SHOWNORMAL);break;
    case SettingAction::BluetoothSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:bluetooth",nullptr,nullptr,SW_SHOWNORMAL);break;
    case SettingAction::PowerSettings:ShellExecuteW(nullptr,L"open",L"ms-settings:powersleep",nullptr,nullptr,SW_SHOWNORMAL);break;
    case SettingAction::OpenArmoury:if(!content_.platform.armoury.empty())ShellExecuteW(nullptr,L"open",(L"shell:AppsFolder\\"+content_.platform.armoury).c_str(),nullptr,nullptr,SW_SHOWNORMAL);break;
    case SettingAction::TransparencySettings:ShellExecuteW(nullptr,L"open",L"ms-settings:personalization-colors",nullptr,nullptr,SW_SHOWNORMAL);break;
    default:break;
    }
}
// Checks that a stored preference actually changed island behavior, beyond
// being saved. Returns an empty string on success.
std::string IslandWindow::verifySetting(const SettingItem& item){
    const auto& k=item.key;const double s=dpi_/96;RECT r{};GetWindowRect(window_,&r);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);
    auto fail=[](std::string why){return why;};
    if(k=="uiMode"||k=="compactWidth"){double expected=settings_.uiMode==0?72:settings_.compactWidth;if(motion_.compactWidth!=expected)return fail("compact width not applied");}
    if(k=="edge"){if(motion_.edge!=settings_.edge)return fail("motion edge not applied");if(settings_.edge?std::abs(r.right-(mi.rcMonitor.right-int(settings_.gap()*s)))>1:std::abs(r.top-(mi.rcMonitor.top+int((settings_.verticalOffset+settings_.gap())*s)))>1)return fail("window not docked to edge");}
    if(k=="verticalOffset"&&settings_.edge==0&&std::abs(r.top-(mi.rcMonitor.top+int((settings_.verticalOffset+settings_.gap())*s)))>1)return fail("vertical position not applied");
    if(k=="horizontalOffset"&&settings_.edge==0){int center=(r.left+r.right)/2-(mi.rcMonitor.left+mi.rcMonitor.right)/2;int expected=int(settings_.horizontalOffset*s);int limit=((mi.rcMonitor.right-mi.rcMonitor.left)-int((std::max(420,settings_.compactWidth)+56)*s))/2;if(std::abs(center-std::clamp(expected,-limit,limit))>2)return fail("horizontal position not applied");}
    if(k=="scale"){UINT dx=96,dy=96;GetDpiForMonitor(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),MDT_EFFECTIVE_DPI,&dx,&dy);if(std::abs(dpi_-dx*settings_.scale/100.f)>.5f)return fail("scale not applied to DPI");}
    if(k=="corner"&&motion_.corner!=settings_.corner)return fail("corner radius not applied");
    if(k=="theme"){DWORD light=0,size=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);bool expected=settings_.theme==1||(settings_.theme==2&&light);if(content_.light!=expected)return fail("theme not applied");}
    if(k=="material"||k=="glassTint"){auto style=renderer_->glassStyle();if(renderer_->glassAvailable()&&style.visible!=settings_.glassy())return fail("glass visibility not applied");if(settings_.glassy()&&std::abs(style.tint-settings_.glassTint/100.f)>.001f)return fail("glass tint not applied");if(k=="material"&&settings_.edge==0&&std::abs(r.top-(mi.rcMonitor.top+int((settings_.verticalOffset+settings_.gap())*s)))>1)return fail("glass float gap not applied");}
    if(k=="preset"){auto p=preset(MotionPreset(settings_.preset));if(motion_.body.stiffness!=p.stiffness||motion_.body.damping!=p.damping||motion_.body.mass!=p.mass)return fail("motion preset not applied");}
    if(k=="reduceMotion"){BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);if(motion_.reduced!=(settings_.reduceMotion||!animations))return fail("reduced motion not applied");}
    if(k=="hoverOpen"){
        // Drive the real hover path: pointer arrives, the hover timer fires.
        transition(IslandState::Compact);interaction_=InteractionState::Hover;SendMessageW(window_,WM_TIMER,7,0);bool opened=state_!=IslandState::Compact;
        interaction_=InteractionState::Rest;content_.pinned=false;transition(IslandState::Compact);if(opened!=settings_.hoverOpen)return fail("hover opening does not follow preference");}
    if(k=="startAtLogin"&&startupRequest_!=int(settings_.startAtLogin))return fail("sign-in startup request not issued");
    if(k=="accent"||k=="albumAccents"){UINT32 accents[]={0xa4deca,0xa6cafa,0xccb8f1,0xefc7a6};UINT32 expected=content_.light?0x487467:settings_.albumAccents&&content_.playback.artwork?content_.playback.artwork->accent:accents[settings_.accent];if(renderer_->accentColor()!=expected)return fail("accent color not applied");}
    if(k=="autoHide"&&(settings_.autoHide!=autoHideTimer_&&!testing_))return fail("edge polling does not follow preference");
    if(k=="autoHide"&&!settings_.autoHide&&motion_.slide.target()!=0)return fail("island stays hidden after auto-hide was turned off");
    if(k=="monitor"&&MonitorFromWindow(window_,MONITOR_DEFAULTTONULL)==nullptr)return fail("island left every display");
    if(content_.settings!=settings_)return fail("renderer did not receive preferences");
    return {};
}
void IslandWindow::settingsTestStep(){
    auto items=settingItems(settingsContext().monitors);HWND settings=settingsWindow_?settingsWindow_->handle():nullptr;
    auto log=[&](bool pass,const std::string& line){settingsTestLog_.push_back((pass?"PASS ":"FAIL ")+line);if(!pass)++settingsTestFailures_;};
    auto click=[&](RECT r,float fx=.5f){int x=r.left+int(std::lround((r.right-r.left)*fx)),y=(r.top+r.bottom)/2;SendMessageW(settings,WM_MOUSEMOVE,0,MAKELPARAM(x,y));SendMessageW(settings,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(settings,WM_LBUTTONUP,0,MAKELPARAM(x,y));};
    auto probe=[&](int index)->std::optional<SettingsWindow::Probe>{for(auto& p:settingsWindow_->probes())if(p.kind==SettingsWindow::Probe::Kind::Control&&p.index==index)return p;return std::nullopt;};
    auto testable=[&](const SettingItem& i){return i.control!=SettingControl::Button||i.action==SettingAction::ResetLayout||i.action==SettingAction::OpenLab||i.action==SettingAction::ResetAll;};
    auto finish=[&]{
        KillTimer(window_,21);settingsWindow_.reset();
        std::ostringstream out;out<<"Settings window end-to-end test: "<<(settingsTestFailures_?"FAIL":"PASS")<<" ("<<settingsTestLog_.size()<<" checks, "<<settingsTestFailures_<<" failures)\n";for(auto& l:settingsTestLog_)out<<l<<'\n';
        store_.submit([dir=store_.directory,text=out.str()]{std::ofstream(dir/L"settings-test.txt")<<text;});PostMessageW(window_,WM_CLOSE,0,0);};
    if(++settingsTestWait_>400){{std::string where;RECT client{};if(settings)GetClientRect(settings,&client);if(auto p=probe(settingsTestItem_))where=" rect "+std::to_string(p->rect.top)+".."+std::to_string(p->rect.bottom)+" client "+std::to_string(client.bottom)+" settled "+std::to_string(settingsWindow_->settled());log(false,"test timed out in phase "+std::to_string(settingsTestPhase_)+" item "+std::to_string(settingsTestItem_)+where);}finish();return;}
    switch(settingsTestPhase_){
    case 0:if(!settingsWindow_)openSettings(0);if(settingsWindow_->open()&&!settingsWindow_->probes().empty()){settingsTestItem_=-1;settingsTestPhase_=1;}return;
    case 1:{int next=settingsTestItem_+1;while(next<int(items.size())&&!testable(items[next]))++next;settingsTestItem_=next;settingsTestWait_=0;
        if(next>=int(items.size())){settingsTestPhase_=20;SetTimer(window_,21,700,nullptr);return;}settingsTestPhase_=2;settingsTestValue_=0;return;}
    case 2:{auto& item=items[settingsTestItem_];if(settingsWindow_->section()!=item.section){for(auto& p:settingsWindow_->probes())if(p.kind==SettingsWindow::Probe::Kind::Section&&p.index==item.section)click(p.rect);return;}if(!settingsWindow_->settled())return;settingsTestPhase_=3;return;}
    case 3:{auto p=probe(settingsTestItem_);if(!p){log(false,items[settingsTestItem_].key+" has no control");settingsTestPhase_=1;return;}
        if(!p->visible){if(!settingsWindow_->settled())return;RECT client{};GetClientRect(settings,&client);SendMessageW(settings,WM_MOUSEWHEEL,MAKEWPARAM(0,p->rect.top<client.bottom/2?120:-120),0);return;}
        if(!settingsWindow_->settled())return;settingsTestPhase_=4;return;}
    case 4:case 6:{
        // Act through the real pointer path, then wait for the island to apply it.
        auto& item=items[settingsTestItem_];auto found=probe(settingsTestItem_);if(!found){settingsTestPhase_=3;return;}auto p=*found;int v=item.get?item.get(settings_):0,expected=v;bool second=settingsTestPhase_==6;settingsTestBefore_=v;
        switch(item.control){
        case SettingControl::Toggle:expected=!v;click(p.parts[0]);break;
        case SettingControl::Choice:{int n=int(p.parts.size());expected=item.key=="material"?(second?2:1):second?settingsTestValue_:(v+1)%n;if(!second)settingsTestValue_=v;click(p.parts[expected]);break;}
        case SettingControl::Swatch:{int n=int(p.parts.size());expected=second?settingsTestValue_:(v+1)%n;if(!second)settingsTestValue_=v;click(p.parts[expected]);break;}
        case SettingControl::Stepper:{int n=item.hi-item.lo+1;expected=item.lo+((v-item.lo+(second?-1:1))%n+n)%n;click(p.parts[second?0:1]);break;}
        case SettingControl::Slider:{float f=second?.9f:.1f;int x=p.rect.left+int(std::lround((p.rect.right-p.rect.left)*f));float fraction=float(x-p.rect.left)/float(p.rect.right-p.rect.left);expected=std::clamp(item.lo+int(std::lround(fraction*(item.hi-item.lo)/item.step))*item.step,item.lo,item.hi);click(p.rect,f);break;}
        case SettingControl::Order:{int slot=item.key.back()-'0';int neighbor=slot<6?slot+1:slot-1;expected=settings_.navigation[neighbor];click(p.parts[slot<6?1:0]);break;}
        case SettingControl::Button:
            if(item.action==SettingAction::ResetLayout){Settings changed=settings_;int from=0;moveNavigation(changed.navigation,from,1);assignMetric(changed.homeMetrics,0,5);receiveSettings(changed);}
            click(p.rect);if(item.action==SettingAction::ResetAll)click(p.rect);break;
        default:break;
        }
        settingsTestExpected_=expected;settingsTestBefore_=v;settingsTestWait_=0;settingsTestPhase_=settingsTestPhase_+1;return;}
    case 5:case 7:{
        auto& item=items[settingsTestItem_];int expected=settingsTestExpected_;bool reached=false;std::string what;
        if(item.control==SettingControl::Button){
            if(item.action==SettingAction::ResetLayout){reached=settings_.navigation==defaultNavigation&&settings_.homeMetrics==defaultMetrics;what="navigation and statistics restored";}
            else if(item.action==SettingAction::OpenLab){reached=lab_!=nullptr;what="Animation Lab opened";if(reached){DestroyWindow(lab_);}}
            else if(item.action==SettingAction::ResetAll){Settings defaults;defaults.startAtLogin=settings_.startAtLogin;reached=settings_==defaults;what="all preferences reset (sign-in kept)";}
        }else{int actual=item.get(settings_);int tolerance=item.control==SettingControl::Slider?item.step:0;reached=std::abs(actual-expected)<=tolerance&&(item.control!=SettingControl::Slider||actual!=settingsTestBefore_);what=std::to_string(settingsTestBefore_)+" -> "+std::to_string(actual);}
        if(!reached&&settingsTestWait_<30)return;
        std::string derived=reached?verifySetting(item):"island never applied the change";
        std::string name=item.key.empty()?narrow(item.title):item.key;log(reached&&derived.empty(),name+": "+what+(derived.empty()?"":" ("+derived+")"));
        bool twoMoves=settingsTestPhase_==5&&(item.control==SettingControl::Toggle||item.control==SettingControl::Choice||item.control==SettingControl::Swatch||item.control==SettingControl::Stepper||item.control==SettingControl::Slider);
        settingsTestWait_=0;settingsTestPhase_=twoMoves?30:1;return;}
    case 30:{if(!settingsWindow_->settled())return;settingsTestPhase_=6;return;}
    case 20:{Settings saved=store_.load(settingsFile_);log(saved==settings_,"persistence: settings-qa.nexus matches the live island");
        // Keyboard path: focus the first section, Tab past every section to its first control, toggle with Space.
        bool before=settings_.startAtLogin;for(auto& p:settingsWindow_->probes())if(p.kind==SettingsWindow::Probe::Kind::Section&&p.index==0)click(p.rect);
        for(size_t i=0;i<settingSections().size();++i)SendMessageW(settings,WM_KEYDOWN,VK_TAB,0);SendMessageW(settings,WM_KEYDOWN,VK_SPACE,0);settingsTestValue_=before;settingsTestPhase_=21;settingsTestWait_=0;SetTimer(window_,21,90,nullptr);return;}
    case 21:{bool changed=settings_.startAtLogin!=bool(settingsTestValue_);if(!changed&&settingsTestWait_<30)return;log(changed,"keyboard: Tab focus and Space toggle reach the island");finish();return;}
    }
}
}
