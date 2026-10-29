// 0.22 (revision 5): a paired phone controls the whole island. Every answer here is built on the UI thread (the network
// thread waits up to 4 s for it) and says what the island has now; anything slow (radios, dark mode, the recycle bin) is
// started on a worker and read back by the next ask. Sleeping, restarting and shutting down wait until the answer has
// gone, so the phone hears that they started.
// 0.26 (revision 9): shuffle, repeat and up next, each app's volume, the sound's levels, and the adapters to wake this PC.
// 0.27 (revision 10): this PC's temperatures, the island's clipboard history, and the PowerPoint show that runs here.
// 0.28 (revision 11): this PC as Task Manager and Settings show it: top apps, the battery's health, storage, the apps that
// start with Windows, the network, Windows Update, and the Start menu's apps to open.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Island/IslandWindow.h"
#include "Settings/SettingsModel.h"
#include "Productivity/CommandService.h"
#include "Design/Accent.h"
#include "Media/CoverCodec.h"
#include <iphlpapi.h>
#include <dxgi.h>
#include <powrprof.h>
#include <wrl/client.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <oleauto.h>
#include "Media/AppIdentity.h"
#include <dwmapi.h>
#include <netlistmgr.h>
#include <psapi.h>
#include <shellapi.h>
#include <wlanapi.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <mutex>
namespace nexus {
namespace {
std::wstring regText(HKEY root,const wchar_t* path,const wchar_t* name){
    wchar_t value[256]{};DWORD size=sizeof(value);if(RegGetValueW(root,path,name,RRF_RT_REG_SZ,nullptr,value,&size)!=ERROR_SUCCESS)return {};
    std::wstring t=value;const auto a=t.find_first_not_of(L' '),b=t.find_last_not_of(L' ');return a==std::wstring::npos?std::wstring{}:t.substr(a,b-a+1);}
// What the phone asks before running something hard to undo.
std::wstring phoneConfirm(CommandKind k){switch(k){case CommandKind::EmptyBin:return L"Empty the recycle bin on the PC? This can\u2019t be undone";case CommandKind::Sleep:return L"Put the PC to sleep?";
    case CommandKind::Restart:return L"Restart the PC? Anything unsaved there may be lost";case CommandKind::ShutDown:return L"Shut down the PC? Anything unsaved there may be lost";default:return L"Run this on the PC?";}}
bool powerPrivilege(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return false;TOKEN_PRIVILEGES p{};p.PrivilegeCount=1;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    const bool ok=LookupPrivilegeValueW(nullptr,SE_SHUTDOWN_NAME,&p.Privileges[0].Luid)&&AdjustTokenPrivileges(token,FALSE,&p,0,nullptr,nullptr)&&GetLastError()==ERROR_SUCCESS;CloseHandle(token);return ok;}
}
// What this PC is, read once: Windows' edition and version, the processor and the graphics adapter.
void IslandWindow::readPcInfo(){
    if(pcInfo_.read)return;pcInfo_.read=true;
    const wchar_t* nt=L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";std::wstring os=regText(HKEY_LOCAL_MACHINE,nt,L"ProductName");
    // Windows 11 still calls itself Windows 10 here; its build number says which it is.
    if(_wtoi(regText(HKEY_LOCAL_MACHINE,nt,L"CurrentBuildNumber").c_str())>=22000&&os.starts_with(L"Windows 10"))os.replace(8,2,L"11");
    const std::wstring version=regText(HKEY_LOCAL_MACHINE,nt,L"DisplayVersion");pcInfo_.os=os+(version.empty()?L"":L" "+version);
    pcInfo_.cpu=regText(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",L"ProcessorNameString");
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        for(UINT i=0;;++i){Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};
            if(SUCCEEDED(adapter->GetDesc1(&d))&&!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){pcInfo_.gpu=d.Description;break;}}
    const auto& p=content_.platform;pcInfo_.model=trimmed(p.manufacturer+(p.product.empty()?L"":L" "+p.product));
}
// Stats: the system provider runs while a phone keeps asking (and 12 s after), as it does while its page shows.
std::vector<uint8_t> IslandWindow::remoteStats(){
    readPcInfo();const double now=seconds();phoneStatsUntil_=now+12;if(system_)system_->setActive(true);SetTimer(window_,PhoneStatsTimer,12500,nullptr);
    const SystemSnapshot s=system_?system_->snapshot():SystemSnapshot{};PcStats st;
    st.cpu=s.cpu;st.gpu=s.gpu;st.ramUsedGiB=s.ramUsedGiB;st.ramTotalGiB=s.ramTotalGiB;st.ramPercent=s.ramPercent;st.diskUsedPercent=s.diskUsedPercent;st.diskFreeGiB=s.diskFreeGiB;st.diskTotalGiB=s.diskTotalGiB;
    st.download=s.download;st.upload=s.upload;st.uptime=s.uptime;st.logical=s.logicalProcessors;st.battery=content_.battery;st.charging=content_.charging;
    SYSTEM_POWER_STATUS power{};if(GetSystemPowerStatus(&power)&&power.BatteryLifeTime!=DWORD(-1)&&!content_.charging)st.batteryMinutes=power.BatteryLifeTime/60.;
    // The histories, oldest first: the provider fills its 40 samples from the start and then shifts them left.
    const size_t n=std::min<size_t>(s.samples,40);
    for(size_t i=0;i<n;++i){const size_t at=40-n+i;st.cpuHistory.push_back(s.cpuHistory[at]);st.gpuHistory.push_back(s.gpu>=0?s.gpuHistory[at]:-1.f);st.downloadHistory.push_back(s.downloadHistory[at]);}
    st.name=content_.shareName;st.model=pcInfo_.model;st.os=pcInfo_.os;st.cpuName=pcInfo_.cpu;st.gpuName=pcInfo_.gpu;st.cores=s.cores;
    return remoteStatsAnswer(st);
}
// 0.23: the battery in full, with the last day of its history (when history is kept).
std::vector<uint8_t> IslandWindow::remoteBattery(){
    PcBattery b;if(!battery_){b.present=false;return remoteBatteryAnswer(b);}
    battery_->setFast(true);SetTimer(window_,PhoneStatsTimer,12500,nullptr);phoneStatsUntil_=seconds()+12;
    const auto r=battery_->reading();const auto e=battery_->estimate();
    b.present=r.present;b.online=r.online;b.charging=r.charging;b.saver=r.saver;b.critical=r.critical;b.percent=r.present?r.percent:-1;
    b.minutesLeft=e.minutesRemaining(r);b.minutesToFull=e.minutesToFull(r);
    // Windows' own estimate when the battery's rate says nothing yet.
    if(b.minutesLeft<0&&!r.charging&&r.windowsSeconds>0)b.minutesLeft=r.windowsSeconds/60;
    if(!r.relative){b.designMwh=r.designMwh;b.fullMwh=r.fullMwh;b.remainingMwh=r.remainingMwh;b.rateMw=r.rateMw;}
    b.voltageMv=r.voltageMv;b.cycles=r.cycles;b.temperatureDeciK=r.temperatureDeciK;b.chemistry=r.chemistry;b.manufacturer=r.manufacturer;b.name=r.name;
    if(!testing_){const auto log=battery_->health();if(auto w=log.week()){b.healthBefore=w->first;b.health=w->second;}}
    if(b.health<0&&!r.relative&&r.designMwh>0&&r.fullMwh>0)b.health=std::min(1.,double(r.fullMwh)/double(r.designMwh));
    const auto history=battery_->history();const int64_t now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for(auto& h:history.samples)if(now-h.time<=86400)b.day.push_back({h.time,h.percent,h.charging});
    return remoteBatteryAnswer(b);
}
// Settings: the table the Settings window draws, as it is now; a change goes the Settings window's way (receiveSettings).
std::vector<uint8_t> IslandWindow::remoteSettings(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};
    const auto items=settingItems(std::max(1,GetSystemMetrics(SM_CMONITORS)));
    if(op==0){std::vector<RemoteSetting> out;
        for(auto& i:items){int control=-1;switch(i.control){case SettingControl::Toggle:control=0;break;case SettingControl::Slider:control=1;break;case SettingControl::Choice:control=2;break;
                case SettingControl::Stepper:control=3;break;case SettingControl::Swatch:control=4;break;case SettingControl::Button:control=5;break;default:break;}
            if(control<0||(control!=5&&(i.key.empty()||!i.get)))continue;
            RemoteSetting r;r.section=i.section;r.control=control;r.key=i.key;r.title=i.title;r.detail=i.detail;r.unit=i.unit;r.lo=i.lo;r.hi=i.hi;r.step=i.step;r.options=i.options;r.action=int(i.action);
            if(i.get)r.value=i.get(settings_);
            // A swatch's colours as the dark island draws them (a light island draws every swatch in its one deep accent).
            if(control==4)for(int k=i.lo;k<=i.hi;++k)r.colours.push_back(islandAccent(k,false,content_.platform.wallpaper,nullptr,false));
            out.push_back(std::move(r));}
        return remoteSettingsAnswer(settingSections(),out);}
    if(op==1){const std::string key=in.bytes(64);const int value=in.i32();if(!in.ok())return {remoteFailed};
        for(auto& i:items)if(i.key==key&&i.set&&i.get){Settings next=settings_;i.set(next,i.clamp(value));receiveSettings(next);store_.log("Info","phone_setting");return remoteValueAnswer(i.get(settings_));}
        return {remoteUnsupported};}
    if(op==2){const int action=in.u8();if(!in.ok()||action<=0||action>int(SettingAction::CheckUpdates))return {remoteFailed};
        // Buttons that act on the island (not the Animation Lab, which is a window to work in at the PC).
        if(SettingAction(action)==SettingAction::OpenLab||SettingAction(action)==SettingAction::LabPlay)return {remoteUnsupported};
        settingsAction(SettingAction(action));store_.log("Info","phone_setting_action");return {remoteOk};}
    return {remoteUnsupported};
}
// Controls: what the Controls page shows, the volume and the focus clock; the radios are read again at most every 2 s.
std::vector<uint8_t> IslandWindow::remoteControls(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};const double now=seconds();auto& c=content_.controls;
    if(op==1){const int control=in.u8(),value=in.i32();if(!in.ok())return {remoteFailed};const bool on=value!=0;
        switch(control){
        case 1:if(c.wifi<-1)return {remoteUnsupported};if(!(c.busy&1)){c.busy|=1;controlJob(1,on?1:0,1);}break;
        case 2:if(c.bluetooth<-1)return {remoteUnsupported};if(!(c.busy&2)){c.busy|=2;controlJob(2,on?1:0,2);}break;
        case 3:if(!(c.busy&8)){c.busy|=8;controlJob(4,on?1:0,8);}break;
        case 4:if(!(c.busy&4)){c.busy|=4;controlJob(3,on?0:1,4);}break;
        case 5:if(!brightness_||content_.brightness<0)return {remoteUnsupported};brightnessRequestAt_=now;content_.brightness=std::clamp(value,0,100);brightness_->set(content_.brightness);break;
        case 6:if(!audio_)return {remoteUnsupported};audio_->setVolume(std::clamp(value,0,100));break;
        case 7:if(!audio_)return {remoteUnsupported};if(audio_->muted.load()!=on)audio_->toggleMute();break;
        case 8:if(!audio_||!audio_->micAvailable)return {remoteUnsupported};if(audio_->micMuted.load()!=on)audio_->toggleMic();break;
        case 9:if(!LockWorkStation())return {remoteFailed};break;
        // After the answer: sleep, restart or shut down.
        case 10:case 11:case 12:pendingPower_=control-9;SetTimer(window_,PhonePowerTimer,900,nullptr);store_.log("Info","phone_power");break;
        case 13:std::thread([]{SHEmptyRecycleBinW(nullptr,nullptr,SHERB_NOCONFIRMATION|SHERB_NOPROGRESSUI|SHERB_NOSOUND);}).detach();break;
        case 15:case 16:content_.focus.select(control==15?FocusClock::Mode::Focus:FocusClock::Mode::Break,now);content_.focus.duration=std::clamp(value,1,600)*60.;content_.focus.toggle(now);break;
        case 17:content_.focus.toggle(now);break;
        case 18:content_.focus.reset(now);break;
        case 19:content_.focus.select(FocusClock::Mode::Stopwatch,now);content_.focus.toggle(now);break;
        default:return {remoteUnsupported};}
        store_.log("Info","phone_control");clockTimer();refresh();}
    else if(op!=0)return {remoteUnsupported};
    if(now-controlsAskedAt_>=2){controlsAskedAt_=now;controlJob(0,0,0);}
    PcControls p;p.wifi=c.wifi;p.bluetooth=c.bluetooth;p.dark=c.dark;p.busy=c.busy;p.brightness=brightness_&&brightness_->available?content_.brightness:-1;
    if(audio_){p.volume=audio_->value.load();p.muted=audio_->muted.load();p.micAvailable=audio_->micAvailable.load();p.micMuted=audio_->micMuted.load();}
    const auto& f=content_.focus;p.focusMode=int(f.mode);p.focusRunning=f.running;p.focusFinished=f.finished;p.focusDuration=f.duration;p.focusShown=f.displayed(now);
    return remoteControlsAnswer(p);
}
// The command bar, asked from the phone: its own command service (so the island's own bar is never disturbed), asked
// again while the phone types; results carry what's needed to show them and to be sure which one is run.
std::vector<uint8_t> IslandWindow::remoteCommand(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();const std::wstring text=in.text(600);if(!in.ok())return {remoteFailed};
    ensureRemoteCommands();if(!remoteCommands_)return {remoteUnsupported};
    if(text!=remoteQueryText_||op==0&&remoteQuerySeq_==0){remoteQueryText_=text;remoteQuerySeq_=remoteCommands_->query(text,workspaces_.names(),commandContext(),commandMemory_.items(),settings_.currency);}
    std::vector<CommandResult> results;std::vector<std::shared_ptr<const Artwork>> icons;const uint64_t answered=remoteCommands_->results(results,icons);
    const bool final=answered>=remoteQuerySeq_;
    if(op==0){std::vector<RemoteResult> out;if(final)for(auto& r:results){if(out.size()>=12)break;out.push_back({int(r.kind),r.confirm,r.title,r.detail,r.answer});}
        return remoteCommandAnswer(final,out);}
    if(op==1){const int index=in.u8();const std::wstring title=in.text(600);const bool confirmed=in.u8()!=0;if(!in.ok())return {remoteFailed};
        if(!final||index<0||size_t(index)>=results.size()||results[size_t(index)].title!=title)return remoteOutcome(3,L"The results changed. Try again");
        const auto [outcome,message]=runRemote(results[size_t(index)],confirmed);return remoteOutcome(outcome,message);}
    return {remoteUnsupported};
}
// One command bar result, run for the phone: what the bar does with it, without the bar (and its second Enter becomes
// the phone's own yes).
std::pair<int,std::wstring> IslandWindow::runRemote(const CommandResult& r,bool confirmed){
    const double now=seconds();auto shell=[&](const std::wstring& target){return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;};
    if((r.confirm||r.kind==CommandKind::Workspace||r.kind==CommandKind::Sleep||r.kind==CommandKind::Restart||r.kind==CommandKind::ShutDown)&&!confirmed)
        return {1,r.kind==CommandKind::Workspace?L"Open these apps on the PC?":phoneConfirm(r.kind)};
    if(r.kind!=CommandKind::None)rememberCommand(r);store_.log("Info","phone_command");
    auto& c=content_.controls;
    switch(r.kind){
    case CommandKind::OpenApp:return shell(L"shell:AppsFolder\\"+r.target)?std::pair{0,L"Opened "+r.title.substr(r.title.starts_with(L"Open ")?5:0)}:std::pair{2,std::wstring(L"Windows couldn’t open that app")};
    case CommandKind::OpenFile:case CommandKind::SearchFiles:case CommandKind::OpenSettings:return shell(r.target)?std::pair{0,std::wstring(L"Opened on the PC")}:std::pair{2,std::wstring(L"Windows couldn’t open that")};
    case CommandKind::Volume:if(audio_){audio_->setVolume(r.value);if(audio_->muted)audio_->toggleMute();}return {0,L"Volume "+std::to_wstring(r.value)+L"%"};
    case CommandKind::VolumeStep:if(audio_)audio_->setVolume(audio_->value+r.value);return {0,L"Volume changed"};
    case CommandKind::Mute:if(audio_&&!audio_->muted)audio_->toggleMute();return {0,L"Muted"};
    case CommandKind::Unmute:if(audio_&&audio_->muted)audio_->toggleMute();return {0,L"Sound on"};
    case CommandKind::MicMute:case CommandKind::MicUnmute:case CommandKind::MicToggle:{if(!audio_||!audio_->micAvailable)return {2,L"No microphone is connected"};
        const bool muted=audio_->micMuted;if(r.kind==CommandKind::MicToggle||(r.kind==CommandKind::MicMute)!=muted)audio_->toggleMic();return {0,L"Microphone changed"};}
    case CommandKind::Play:case CommandKind::Pause:{const bool want=r.kind==CommandKind::Play;if(content_.playback.canToggle&&content_.playback.playing!=want)mediaCommand(want?4:5);
        else if(want&&!content_.playback.available&&settings_.musicLibrary)shuffleLibrary();return {0,want?L"Playing":L"Paused"};}
    case CommandKind::Next:if(content_.playback.canNext)mediaCommand(3);return {0,L"Next"};
    case CommandKind::Previous:if(content_.playback.canPrevious)mediaCommand(2);return {0,L"Previous"};
    case CommandKind::PlaySong:{auto tracks=library_?library_->tracks():nullptr;if(tracks)for(size_t i=0;i<tracks->size();++i)if((*tracks)[i].path==r.target){std::vector<size_t> order(tracks->size());for(size_t k=0;k<order.size();++k)order[k]=k;playLibrary(order,i);break;}return {0,L"Playing on the PC"};}
    case CommandKind::ShuffleMusic:shuffleLibrary();return {0,L"Shuffling your music on the PC"};
    case CommandKind::ContinueOn:{for(size_t i=0;i<content_.nearby.size();++i)if(content_.nearby[i].id==std::string(r.target.begin(),r.target.end())){handoffTo(i);break;}return {0,L"Offered to "+r.title.substr(r.title.starts_with(L"Continue on ")?12:0)};}
    case CommandKind::Timer:content_.focus.select(r.target==L"break"?FocusClock::Mode::Break:FocusClock::Mode::Focus,now);content_.focus.duration=r.value;content_.focus.toggle(now);clockTimer();refresh();return {0,r.title};
    case CommandKind::Stopwatch:content_.focus.select(FocusClock::Mode::Stopwatch,now);content_.focus.toggle(now);clockTimer();refresh();return {0,L"Stopwatch started"};
    case CommandKind::StopTimer:content_.focus.reset(now);clockTimer();refresh();return {0,L"Timer stopped"};
    case CommandKind::Lock:return LockWorkStation()?std::pair{0,std::wstring(L"Locked")}:std::pair{2,std::wstring(L"Windows didn’t lock")};
    case CommandKind::Sleep:case CommandKind::Restart:case CommandKind::ShutDown:pendingPower_=r.kind==CommandKind::Sleep?1:r.kind==CommandKind::Restart?2:3;SetTimer(window_,PhonePowerTimer,900,nullptr);
        return {0,r.kind==CommandKind::Sleep?L"Going to sleep":r.kind==CommandKind::Restart?L"Restarting":L"Shutting down"};
    case CommandKind::DarkMode:if(!(c.busy&8)){c.busy|=8;controlJob(4,r.value,8);}return {0,r.value?L"Switching to dark mode":L"Switching to light mode"};
    case CommandKind::Bluetooth:if(!(c.busy&2)){c.busy|=2;controlJob(2,r.value<0?(c.bluetooth==1?0:1):r.value,2);}return {0,L"Changing Bluetooth"};
    case CommandKind::WiFi:if(!(c.busy&1)){c.busy|=1;controlJob(1,r.value<0?(c.wifi==1?0:1):r.value,1);}return {0,L"Changing Wi-Fi"};
    case CommandKind::Airplane:if(!(c.busy&4)){c.busy|=4;controlJob(3,r.value?0:1,4);}return {0,r.value?L"Turning radios off":L"Turning radios back on"};
    case CommandKind::EmptyBin:std::thread([]{SHEmptyRecycleBinW(nullptr,nullptr,SHERB_NOCONFIRMATION|SHERB_NOPROGRESSUI|SHERB_NOSOUND);}).detach();return {0,L"Emptying the recycle bin"};
    case CommandKind::Currency:copyText(r.target);return {0,L"Copied "+r.answer+L" on the PC"};
    case CommandKind::Colour:copyText(r.target);return {0,L"Copied "+r.target+L" on the PC"};
    case CommandKind::Clipboard:content_.page=Page::Shelf;content_.shelfTab=1;clipViews();content_.pinned=true;transition(IslandState::Expanded);refresh();return {0,L"Clipboard history open on the PC"};
    case CommandKind::ClearClipboard:clearClips();return {0,L"Clipboard history cleared"};
    case CommandKind::Workspace:{const Workspace* saved=workspaces_.find(r.target);if(!saved)return {2,L"That workspace no longer exists"};const auto report=openWorkspace(*saved,window_);
        return {report.failed&&!report.opened?2:0,L"Opened "+std::to_wstring(report.opened)+(report.opened==1?L" app":L" apps")+(report.running?L", "+std::to_wstring(report.running)+L" already open":L"")};}
    case CommandKind::Snip:case CommandKind::CopyText:case CommandKind::PickColour:startCapture(r.kind==CommandKind::Snip?CaptureMode::Snip:r.kind==CommandKind::CopyText?CaptureMode::Text:CaptureMode::Colour);return {0,L"On the PC: drag over what you want"};
    case CommandKind::Weather:if(!weather_)return {2,L"Weather isn’t available right now"};if(!settings_.weather){Settings next=settings_;next.weather=true;receiveSettings(next);}if(!r.target.empty()){weather_->choose(r.target);weatherAsked_=true;}return {0,L"Weather for "+r.target};
    default:return {2,L"Do that one on the PC"};
    }
}
// Audio: the outputs as the Audio page lists them; one made the default when direct switching is on (as there).
std::vector<uint8_t> IslandWindow::remoteAudio(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok()||!audio_)return {remoteUnsupported};
    if(op==1){const std::wstring id=in.text(400);if(!in.ok())return {remoteFailed};if(!settings_.directAudio)return {remoteNotAllowed};
        const auto list=audio_->devices();if(std::none_of(list.begin(),list.end(),[&](auto& d){return d.id==id;}))return {remoteFailed};
        routeRequestAt_=seconds();audio_->selectDevice(id,true);store_.log("Info","phone_output");return {remoteOk};}
    if(op!=0)return {remoteUnsupported};
    std::vector<RemoteOutput> out;for(auto& d:audio_->devices())out.push_back({d.id,d.name,d.current,d.form});return remoteAudioAnswer(out);
}
// Island: one of its pages opened on the PC (pinned, so it stays), or closed.
std::vector<uint8_t> IslandWindow::remoteIsland(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};
    if(op==1){content_.pinned=false;perform(Action::Close);return {remoteOk};}
    if(op!=0)return {remoteUnsupported};const int page=in.u8();if(!in.ok()||page<0||page>8)return {remoteFailed};
    if(page==4){openSettings();return {remoteOk};}
    static const Page pages[]={Page::Overview,Page::Media,Page::System,Page::Focus,Page::Settings,Page::Shelf,Page::Audio,Page::Control,Page::Phone};
    events_.dismiss(seconds());content_.activity.clear();content_.page=pages[page];content_.phoneView={};content_.remote.open=false;content_.pinned=true;transition(IslandState::Expanded);refresh();animate();
    store_.log("Info","phone_page");return {remoteOk};
}
// ---- 0.26 (revision 9) ----
// Playback: the island's own player (its shuffle, repeat and queue), else the session showing (as its player says).
std::vector<uint8_t> IslandWindow::remotePlayback(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};
    const MediaSnapshot p=content_.playback;if(!p.available)return {remoteUnsupported};
    const bool island=p.id==islandSessionId&&player_&&player_->active();int shuffle=p.shuffle,repeat=p.repeat;
    if(op==1||op==2){const int value=in.u8();if(!in.ok())return {remoteFailed};
        if(island){if(op==1)player_->shuffle(value!=0);else player_->repeat(value);playerArt();updateSessions();refresh();}
        else{if((op==1?shuffle:repeat)<0||!media_)return {remoteUnsupported};
            // Asked of the player on the media thread; answered as asked (the player's own word comes with its next change).
            if(op==1)shuffle=value?1:0;else repeat=std::clamp(value,0,2);media_->control(op==1?6:7,p.source,p.id,op==1?shuffle:repeat);}
        store_.log("Info","phone_playback");}
    else if(op==3){const int index=in.u16();if(!in.ok())return {remoteFailed};if(!island)return {remoteUnsupported};
        if(size_t(index)+1>=player_->size()-player_->index())return {remoteFailed};
        pauseOthers();player_->jump(player_->index()+1+size_t(index));playerArt();updateSessions();playerTick();refresh();store_.log("Info","phone_up_next");}
    else if(op!=0)return {remoteUnsupported};
    RemotePlayback r;
    if(island){r.island=r.shuffleSupported=r.repeatSupported=true;r.shuffle=player_->shuffled();r.repeat=player_->repeating();
        for(auto& t:player_->upNext(50))r.upNext.push_back({t.title,t.artist.empty()?t.album:t.artist,t.duration});}
    else{r.shuffleSupported=shuffle>=0;r.shuffle=shuffle==1;r.repeatSupported=repeat>=0;r.repeat=std::max(0,repeat);}
    return remotePlaybackAnswer(r);
}
// The meters (the analyzer, the mixer's peaks) are back to what the island needs once the last phone's ask has run out.
static UINT meterWait(double until,double now){return UINT(std::max(0.,until-now)*1000)+100;}
// Mixer: the apps as the Audio page lists them; an app's volume or mute changed as its slider would.
std::vector<uint8_t> IslandWindow::remoteMixer(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};if(!mixer_)return {remoteUnsupported};
    const bool icons=op==0&&payload.size()>=2&&payload[1]!=0;
    if(op==1||op==2){const DWORD pid=DWORD(uint32_t(in.i32()));const int value=in.u8();if(!in.ok())return {remoteFailed};
        const auto list=mixer_->entries();if(std::none_of(list.begin(),list.end(),[&](auto& e){return e.pid==pid;}))return {remoteFailed};
        if(op==1)mixer_->setVolume(pid,float(std::clamp(value,0,100))/100.f);else mixer_->setMute(pid,value!=0);store_.log("Info","phone_mixer");}
    else if(op!=0)return {remoteUnsupported};
    const double now=seconds();phoneMixerUntil_=now+3;updateProviders();SetTimer(window_,PhoneMeterTimer,meterWait(std::max(phoneMixerUntil_,phoneLevelsUntil_),now),nullptr);
    std::vector<RemoteMixerEntry> out;std::map<DWORD,std::pair<const Artwork*,std::vector<uint8_t>>> kept;
    for(auto& e:mixer_->entries()){if(out.size()>=24)break;RemoteMixerEntry r;r.pid=e.pid;r.name=e.name;r.volume=int(std::lround(e.volume*100));r.peak=int(std::lround(std::clamp(e.peak,0.f,1.f)*100));r.muted=e.muted;r.system=e.system;r.active=e.active;
        // Each icon encoded once per picture, and kept while its app is listed.
        if(e.icon){auto it=mixerIcons_.find(e.pid);if(it!=mixerIcons_.end()&&it->second.first==e.icon.get())kept[e.pid]=it->second;else kept[e.pid]={e.icon.get(),encodeCover(*e.icon,32,8*1024,.9f,true)};if(icons)r.icon=kept[e.pid].second;}
        out.push_back(std::move(r));}
    mixerIcons_=std::move(kept);return remoteMixerAnswer(out);
}
// Levels: the loopback analyzer's frame (it listens for 2 s after each ask; the phone asks about 12 times a second).
std::vector<uint8_t> IslandWindow::remoteLevels(){
    if(!analyzer_)return {remoteUnsupported};
    const double now=seconds();phoneLevelsUntil_=now+2;updateProviders();SetTimer(window_,PhoneMeterTimer,meterWait(std::max(phoneMixerUntil_,phoneLevelsUntil_),now),nullptr);
    const SpectrumFrame f=analyzer_->frame();std::vector<uint8_t> bands;for(float b:f.bands)bands.push_back(uint8_t(std::clamp(int(std::lround(b*255)),0,255)));
    return remoteLevelsAnswer(int(std::lround(std::clamp(f.level,0.f,1.f)*255)),f.resting,bands);
}
// Machine: the wired and Wi-Fi adapters that are up with an IPv4 address (its MAC, for a magic packet; its subnet, to
// broadcast it on).
std::vector<uint8_t> IslandWindow::remoteMachine(){
    ULONG size=16*1024;std::vector<uint8_t> table(size);const ULONG flags=GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER;
    ULONG r=GetAdaptersAddresses(AF_INET,flags,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(table.data()),&size);
    if(r==ERROR_BUFFER_OVERFLOW){table.resize(size);r=GetAdaptersAddresses(AF_INET,flags,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(table.data()),&size);}
    std::vector<PcAdapter> out;
    if(r==NO_ERROR)for(auto* a=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(table.data());a&&out.size()<8;a=a->Next){
        if(a->OperStatus!=IfOperStatusUp||(a->IfType!=IF_TYPE_ETHERNET_CSMACD&&a->IfType!=IF_TYPE_IEEE80211)||a->PhysicalAddressLength!=6)continue;
        for(auto* u=a->FirstUnicastAddress;u;u=u->Next){const sockaddr* sa=u->Address.lpSockaddr;if(!sa||sa->sa_family!=AF_INET)continue;
            const auto* b=reinterpret_cast<const uint8_t*>(&reinterpret_cast<const sockaddr_in*>(sa)->sin_addr);
            // A self-assigned address (169.254.x.x) has no network to wake it from.
            if(b[0]==169&&b[1]==254)continue;
            PcAdapter p;std::memcpy(p.mac,a->PhysicalAddress,6);p.name=a->FriendlyName?a->FriendlyName:L"";char ip[16];std::snprintf(ip,sizeof(ip),"%u.%u.%u.%u",b[0],b[1],b[2],b[3]);p.ipv4=ip;p.prefix=u->OnLinkPrefixLength;
            out.push_back(std::move(p));break;}}
    return remoteMachineAnswer(out);
}
// The phone's power request, once its answer has gone: 1 sleep, 2 restart, 3 shut down.
void IslandWindow::phonePower(){
    const int what=pendingPower_;pendingPower_=0;
    if(what==1){SetSuspendState(FALSE,FALSE,FALSE);return;}
    if(what==2||what==3){powerPrivilege();ExitWindowsEx(what==2?EWX_REBOOT:(EWX_SHUTDOWN|EWX_POWEROFF|EWX_HYBRID_SHUTDOWN),SHTDN_REASON_MAJOR_OTHER|SHTDN_REASON_MINOR_OTHER|SHTDN_REASON_FLAG_PLANNED);}
}
// ---- 0.27 (revision 10) ----
namespace {
const double unknownTemperature=std::numeric_limits<double>::quiet_NaN();
std::wstring wideOf(const char* s){std::wstring w;const int n=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0);if(n>1){w.resize(size_t(n-1));MultiByteToWideChar(CP_UTF8,0,s,-1,w.data(),n);}return w;}
// The ACPI thermal zones through PDH ("High Precision Temperature" in tenths of a kelvin, else "Temperature" in kelvin; no
// admin rights needed). The query is made once; zones reading nothing sensible are left out.
struct ThermalZones{PDH_HQUERY query=nullptr;PDH_HCOUNTER precise=nullptr,plain=nullptr;bool tried=false;
    ~ThermalZones(){if(query)PdhCloseQuery(query);}
    void read(std::vector<std::pair<std::wstring,double>>& out){
        if(!tried){tried=true;if(PdhOpenQueryW(nullptr,0,&query)!=ERROR_SUCCESS)query=nullptr;
            else{if(PdhAddEnglishCounterW(query,L"\\Thermal Zone Information(*)\\High Precision Temperature",0,&precise)!=ERROR_SUCCESS)precise=nullptr;
                if(PdhAddEnglishCounterW(query,L"\\Thermal Zone Information(*)\\Temperature",0,&plain)!=ERROR_SUCCESS)plain=nullptr;}}
        if(!query||PdhCollectQueryData(query)!=ERROR_SUCCESS)return;
        auto take=[&](PDH_HCOUNTER counter,double scale){if(!counter)return false;DWORD size=0,count=0;const DWORD format=PDH_FMT_DOUBLE|PDH_FMT_NOCAP100;
            if(PdhGetFormattedCounterArrayW(counter,format,&size,&count,nullptr)!=PDH_MORE_DATA||!size)return false;
            std::vector<uint8_t> buffer(size);auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());if(PdhGetFormattedCounterArrayW(counter,format,&size,&count,items)!=ERROR_SUCCESS)return false;
            bool any=false;
            for(DWORD i=0;i<count&&out.size()<24;++i){const auto& v=items[i].FmtValue;if(v.CStatus!=PDH_CSTATUS_VALID_DATA&&v.CStatus!=PDH_CSTATUS_NEW_DATA)continue;
                const double c=v.doubleValue*scale-273.15;if(!(c>-40&&c<150))continue;
                std::wstring name=items[i].szName?items[i].szName:L"";if(const auto cut=name.find_last_of(L".\\");cut!=std::wstring::npos)name=name.substr(cut+1);
                out.push_back({L"Thermal zone "+name,std::round(c*10)/10});any=true;}
            return any;};
        if(!take(precise,.1))take(plain,1);}};
// NVIDIA's own library (nvml.dll, which its driver puts in System32), loaded once when it's there.
struct Nvml{bool tried=false,ok=false;int(*count)(unsigned*)=nullptr;int(*handle)(unsigned,void**)=nullptr;int(*temperature)(void*,int,unsigned*)=nullptr;int(*name)(void*,char*,unsigned)=nullptr;
    double read(std::wstring& label){
        if(!tried){tried=true;HMODULE dll=LoadLibraryExW(L"nvml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);wchar_t files[MAX_PATH]{};
            if(!dll&&GetEnvironmentVariableW(L"ProgramW6432",files,MAX_PATH))dll=LoadLibraryExW((std::wstring(files)+L"\\NVIDIA Corporation\\NVSMI\\nvml.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
            if(dll){auto get=[&](auto& f,const char* n){f=reinterpret_cast<std::remove_reference_t<decltype(f)>>(reinterpret_cast<void*>(GetProcAddress(dll,n)));};int(*init)()=nullptr;
                get(init,"nvmlInit_v2");get(count,"nvmlDeviceGetCount_v2");get(handle,"nvmlDeviceGetHandleByIndex_v2");get(temperature,"nvmlDeviceGetTemperature");get(name,"nvmlDeviceGetName");
                ok=init&&count&&handle&&temperature&&init()==0;}}
        if(!ok)return unknownTemperature;unsigned n=0;if(count(&n)!=0)return unknownTemperature;double best=unknownTemperature;
        for(unsigned i=0;i<n&&i<8;++i){void* device=nullptr;unsigned t=0;if(handle(i,&device)!=0||temperature(device,0,&t)!=0||!t||t>150)continue;
            if(!(best>=double(t))){best=t;char text[96]{};if(name&&name(device,text,sizeof(text))==0)label=wideOf(text);}}
        return best;}};
// Any WDDM 2.4 adapter's own reading: D3DKMT's adapter performance data (KMTQAITYPE_ADAPTERPERFDATA, 62: tenths of a degree,
// and its fan), through gdi32. The structures as the WDK has them.
struct KmtOpen{LUID luid;UINT adapter;};struct KmtQuery{UINT adapter;UINT type;void* data;UINT size;};struct KmtClose{UINT adapter;};
struct KmtPerf{UINT32 physical;ULONGLONG memoryFrequency,maxMemoryFrequency,maxMemoryFrequencyOC,memoryBandwidth,pcieBandwidth;ULONG fanRpm,power,temperature;UCHAR powerStateOverride;};
double adapterTemperature(std::wstring& label,int& fan){
    HMODULE gdi=GetModuleHandleW(L"gdi32.dll");if(!gdi)return unknownTemperature;
    auto get=[&](auto& f,const char* n){f=reinterpret_cast<std::remove_reference_t<decltype(f)>>(reinterpret_cast<void*>(GetProcAddress(gdi,n)));};
    LONG(APIENTRY* open)(KmtOpen*)=nullptr;LONG(APIENTRY* query)(const KmtQuery*)=nullptr;LONG(APIENTRY* close)(const KmtClose*)=nullptr;
    get(open,"D3DKMTOpenAdapterFromLuid");get(query,"D3DKMTQueryAdapterInfo");get(close,"D3DKMTCloseAdapter");if(!open||!query||!close)return unknownTemperature;
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return unknownTemperature;double best=unknownTemperature;
    for(UINT i=0;i<8;++i){Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};
        if(FAILED(adapter->GetDesc1(&d))||(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)||d.VendorId==0x1414)continue;
        KmtOpen o{d.AdapterLuid,0};if(open(&o)<0)continue;KmtPerf perf{};const KmtQuery q{o.adapter,62,&perf,UINT(sizeof(perf))};
        if(query(&q)>=0){if(perf.temperature>0&&perf.temperature<1500&&!(best>=perf.temperature/10.)){best=perf.temperature/10.;label=d.Description;}if(perf.fanRpm>0&&perf.fanRpm<20000)fan=std::max(fan,int(perf.fanRpm));}
        const KmtClose c{o.adapter};close(&c);}
    return best;}
}
// Sensors: each thermal zone, the hottest of them as the CPU's; the GPU from NVML (NVIDIA), else its adapter's own reading,
// with its fan when it says. NaN when nothing says.
std::vector<uint8_t> IslandWindow::remoteSensors(){
    static ThermalZones zones;static Nvml nvml;PcSensors s;zones.read(s.zones);
    for(auto& [name,c]:s.zones)if(!(s.cpu>=c))s.cpu=c;
    std::wstring gpu,adapter;int fan=0;s.gpu=nvml.read(gpu);const double own=adapterTemperature(adapter,fan);if(std::isnan(s.gpu)){s.gpu=own;gpu=adapter;}
    if(!std::isnan(s.gpu))s.zones.push_back({gpu.empty()?std::wstring(L"Graphics"):gpu,s.gpu});if(fan>0)s.fans.push_back({L"Graphics fan",fan});
    return remoteSensorsAnswer(s);
}
// Clips: the island's clipboard history as the Shelf's Clipboard tab lists it (pinned first, then the newest); one copied
// here again, pinned, deleted, or brought to the phone. With "hide secrets" on, a password-like copy lists as dots and
// never leaves this PC.
std::vector<uint8_t> IslandWindow::remoteClips(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};
    auto secret=[&](const ClipEntry& e){return settings_.hideSecrets&&e.kind==ClipEntry::Kind::Text&&looksSecret(e.text);};
    auto kindOf=[](const ClipEntry& e){return e.kind==ClipEntry::Kind::Image?1:e.kind==ClipEntry::Kind::Files?2:0;};
    auto names=[](const ClipEntry& e){std::wstring t;for(auto& f:e.files){const auto cut=f.find_last_of(L"\\/");if(!t.empty())t+=L"\n";t+=cut==std::wstring::npos?f:f.substr(cut+1);}return t;};
    size_t most=40;bool thumbs=true;
    if(op==0){most=size_t(std::clamp(in.u8(),0,40));thumbs=in.u8()!=0;if(!in.ok())return {remoteFailed};}
    else if(op>=1&&op<=4){const uint64_t id=uint32_t(in.i32());if(!in.ok())return {remoteFailed};const ClipEntry* e=clips_.find(id);if(!e)return {remoteFailed};
        if(op==4){if(secret(*e))return {remoteNotAllowed};std::vector<uint8_t> image;
            if(e->kind==ClipEntry::Kind::Image&&!e->dib.empty())if(auto art=dibThumbnail(e->dib,1280)){for(auto [side,quality]:{std::pair{1280u,.8f},std::pair{960u,.7f},std::pair{640u,.6f}})if(image.empty())image=encodeCover(*art,side,180*1024,quality);}
            store_.log("Info","phone_clip_taken");
            return remoteClipAnswer(kindOf(*e),e->kind==ClipEntry::Kind::Files?names(*e):e->kind==ClipEntry::Kind::Image?std::wstring():e->text,image);}
        if(op==1){for(size_t i=0;i<content_.clips.size();++i)if(content_.clips[i].id==id){copyClip(i);break;}}
        else if(op==2){const bool on=in.u8()!=0;if(!in.ok())return {remoteFailed};if(e->pinned!=on&&!clips_.togglePin(id))return {remoteFailed};clipViews();savePinnedClips();}
        else{const bool pinned=e->pinned;clips_.remove(id);clipViews();if(pinned)savePinnedClips();}
        store_.log("Info","phone_clip");refresh();}
    else return {remoteUnsupported};
    // The list, each image's thumbnail made once per copy (and only so many, so the answer fits one frame).
    const double now=seconds();const int64_t wall=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::map<uint64_t,std::vector<uint8_t>> kept;for(auto& [id,t]:clipThumbs_)if(clips_.find(id))kept[id]=t;
    std::vector<const ClipEntry*> order;for(auto& e:clips_.entries())if(e.pinned)order.push_back(&e);for(auto& e:clips_.entries())if(!e.pinned)order.push_back(&e);
    std::vector<RemoteClip> out;size_t budget=160*1024;
    for(auto* p:order){if(out.size()>=most)break;const auto& e=*p;RemoteClip c;c.id=uint32_t(e.id);c.kind=kindOf(e);c.pinned=e.pinned;c.at=uint64_t(std::max<int64_t>(0,wall-int64_t(std::llround((now-e.time)*1000))));
        if(secret(e))c.text=L"•••••• · hidden";
        else if(e.kind==ClipEntry::Kind::Files)c.text=names(e);
        else if(e.kind==ClipEntry::Kind::Image)c.text=L"Image  ·  "+std::to_wstring(e.imageWidth)+L" × "+std::to_wstring(e.imageHeight);
        else c.text=e.text.substr(0,1000);
        if(thumbs&&e.kind==ClipEntry::Kind::Image&&e.thumbnail){auto it=kept.find(e.id);
            if(it==kept.end()){std::vector<uint8_t> t=encodeCover(*e.thumbnail,96,6*1024,.75f);if(t.empty())t=encodeCover(*e.thumbnail,64,6*1024,.6f);it=kept.emplace(e.id,std::move(t)).first;}
            if(!it->second.empty()&&it->second.size()<=budget){c.thumb=it->second;budget-=c.thumb.size();}}
        out.push_back(std::move(c));}
    clipThumbs_=std::move(kept);return remoteClipsAnswer(out);
}
// ---- Slides: the PowerPoint that runs here, by late binding (GetActiveObject: it's never started from here) ----
namespace {
struct Var{VARIANT v;Var(){VariantInit(&v);}~Var(){VariantClear(&v);}Var(const Var&)=delete;Var& operator=(const Var&)=delete;};
VARIANT number(long n){VARIANT v;VariantInit(&v);v.vt=VT_I4;v.lVal=n;return v;}
// A property or method of a late-bound object, by its name (arguments in their order).
bool invoke(IDispatch* d,const wchar_t* name,WORD flags,Var& out,std::vector<VARIANT> args={}){
    if(!d)return false;DISPID id=0;LPOLESTR n=const_cast<LPOLESTR>(name);if(FAILED(d->GetIDsOfNames(IID_NULL,&n,1,LOCALE_USER_DEFAULT,&id)))return false;
    std::reverse(args.begin(),args.end());DISPPARAMS p{args.empty()?nullptr:args.data(),nullptr,UINT(args.size()),0};
    return SUCCEEDED(d->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,flags,&p,&out.v,nullptr,nullptr));}
Microsoft::WRL::ComPtr<IDispatch> object(IDispatch* d,const wchar_t* name,std::vector<VARIANT> args={}){
    Var v;Microsoft::WRL::ComPtr<IDispatch> r;if(invoke(d,name,DISPATCH_PROPERTYGET|DISPATCH_METHOD,v,std::move(args))&&v.v.vt==VT_DISPATCH&&v.v.pdispVal)r=v.v.pdispVal;return r;}
double real(IDispatch* d,const wchar_t* name){Var v;if(!invoke(d,name,DISPATCH_PROPERTYGET,v)||FAILED(VariantChangeType(&v.v,&v.v,0,VT_R8)))return std::numeric_limits<double>::quiet_NaN();return v.v.dblVal;}
int whole(IDispatch* d,const wchar_t* name){const double v=real(d,name);return std::isfinite(v)&&std::abs(v)<1e9?int(v):0;}
std::wstring words(IDispatch* d,const wchar_t* name){Var v;if(!invoke(d,name,DISPATCH_PROPERTYGET,v)||FAILED(VariantChangeType(&v.v,&v.v,0,VT_BSTR))||!v.v.bstrVal)return {};
    std::wstring t(v.v.bstrVal,SysStringLen(v.v.bstrVal));for(auto& c:t)if(c==L'\r'||c==L'\v')c=L'\n';while(!t.empty()&&(t.back()==L'\n'||t.back()==L' '))t.pop_back();return t;}
// A shape's text (its TextFrame's TextRange), empty when it has none.
std::wstring shapeText(IDispatch* shape){auto frame=object(shape,L"TextFrame");auto range=object(frame.Get(),L"TextRange");return words(range.Get(),L"Text");}
// Slides' pictures, kept by deck, slide and size; when the show began (for one that doesn't say).
struct SlideCache{std::mutex m;std::map<std::wstring,std::vector<uint8_t>> pictures;std::wstring show;std::chrono::steady_clock::time_point since{};};
SlideCache& slideCache(){static SlideCache c;return c;}
// A slide as a JPEG `width` pixels wide (at most `limit` bytes), exported once.
std::vector<uint8_t> slidePicture(IDispatch* slide,const std::wstring& deck,int width,double aspect,size_t limit){
    const std::wstring key=deck+L"|"+std::to_wstring(whole(slide,L"SlideID"))+L"|"+std::to_wstring(whole(slide,L"SlideIndex"))+L"|"+std::to_wstring(width);auto& c=slideCache();
    {std::lock_guard lock(c.m);if(auto it=c.pictures.find(key);it!=c.pictures.end())return it->second;}
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);const std::wstring path=std::wstring(temp)+L"arnav-slide-"+std::to_wstring(GetCurrentThreadId())+L"-"+std::to_wstring(GetTickCount64())+L".jpg";
    BSTR file=SysAllocString(path.c_str()),filter=SysAllocString(L"JPG");VARIANT f,k;VariantInit(&f);VariantInit(&k);f.vt=VT_BSTR;f.bstrVal=file;k.vt=VT_BSTR;k.bstrVal=filter;
    Var done;const bool made=invoke(slide,L"Export",DISPATCH_METHOD,done,{f,k,number(width),number(std::max(1L,std::lround(width*aspect)))});SysFreeString(file);SysFreeString(filter);
    std::vector<uint8_t> bytes;if(made){std::ifstream in(std::filesystem::path(path),std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());}DeleteFileW(path.c_str());
    if(bytes.size()>limit){auto art=decodeCover(bytes,UINT(width));bytes.clear();if(art)for(auto [side,quality]:{std::pair{UINT(width),.7f},std::pair{UINT(width*3/4),.6f}})if(bytes.empty())bytes=encodeCover(*art,side,limit,quality);}
    if(!bytes.empty()){std::lock_guard lock(c.m);if(c.pictures.size()>=48)c.pictures.clear();c.pictures[key]=bytes;}return bytes;}
// What the running PowerPoint shows: its slideshow, or without one the slide its window shows (op 1 goes to `go` first).
RemoteSlides readSlides(int op,int go,bool thumbs){
    RemoteSlides r;CLSID clsid;if(FAILED(CLSIDFromProgID(L"PowerPoint.Application",&clsid)))return r;
    Microsoft::WRL::ComPtr<IUnknown> unknown;if(FAILED(GetActiveObject(clsid,nullptr,&unknown))||!unknown)return r;
    Microsoft::WRL::ComPtr<IDispatch> app;if(FAILED(unknown.As(&app)))return r;r.state=1;
    Microsoft::WRL::ComPtr<IDispatch> deck,view,slide;bool show=false;
    if(auto windows=object(app.Get(),L"SlideShowWindows");windows&&whole(windows.Get(),L"Count")>=1)
        if(auto w=object(windows.Get(),L"Item",{number(1)})){view=object(w.Get(),L"View");deck=object(w.Get(),L"Presentation");show=view&&deck;}
    if(!show){view.Reset();deck=object(app.Get(),L"ActivePresentation");if(!deck)return r;if(auto w=object(app.Get(),L"ActiveWindow"))view=object(w.Get(),L"View");}
    auto slides=object(deck.Get(),L"Slides");r.count=std::max(0,whole(slides.Get(),L"Count"));
    if(op==1&&go>=1&&go<=r.count&&view){Var done;invoke(view.Get(),L"GotoSlide",DISPATCH_METHOD,done,{number(go)});}
    if(view)slide=object(view.Get(),L"Slide");if(!slide&&r.count>0)slide=object(slides.Get(),L"Item",{number(1)});
    r.state=show?2:1;r.deck=words(deck.Get(),L"Name");if(const auto dot=r.deck.rfind(L'.');dot!=std::wstring::npos&&dot>0)r.deck.resize(dot);
    const std::wstring full=words(deck.Get(),L"FullName");auto& c=slideCache();
    {std::lock_guard lock(c.m);if(!show)c.show.clear();else if(c.show!=full){c.show=full;c.since=std::chrono::steady_clock::now();}
        if(show){const double elapsed=real(view.Get(),L"PresentationElapsedTime");r.seconds=uint32_t(std::isfinite(elapsed)&&elapsed>=0&&elapsed<1e8?elapsed:std::chrono::duration<double>(std::chrono::steady_clock::now()-c.since).count());}}
    if(!slide)return r;
    r.index=whole(slide.Get(),L"SlideIndex");
    if(auto shapes=object(slide.Get(),L"Shapes");shapes&&whole(shapes.Get(),L"HasTitle")!=0)if(auto title=object(shapes.Get(),L"Title"))r.title=shapeText(title.Get());
    if(auto notes=object(slide.Get(),L"NotesPage"))if(auto shapes=object(notes.Get(),L"Shapes"))if(auto holders=object(shapes.Get(),L"Placeholders"))if(auto body=object(holders.Get(),L"Item",{number(2)}))r.notes=shapeText(body.Get());
    if(thumbs){double aspect=.5625;if(auto setup=object(deck.Get(),L"PageSetup")){const double w=real(setup.Get(),L"SlideWidth"),h=real(setup.Get(),L"SlideHeight");if(w>0&&h>0&&h/w<4)aspect=h/w;}
        r.current=slidePicture(slide.Get(),full,640,aspect,90*1024);
        if(r.index>=1&&r.index<r.count)if(auto next=object(slides.Get(),L"Item",{number(r.index+1)}))r.next=slidePicture(next.Get(),full,480,aspect,60*1024);}
    return r;}
}
// Slides: read (and go to a slide) on a worker of its own (PowerPoint can be slow to answer, and a stuck one mustn't hold
// the island); the phone hears "failed" if it takes over 3 s.
std::vector<uint8_t> IslandWindow::remoteSlides(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};if(op!=0&&op!=1)return {remoteUnsupported};
    const bool thumbs=op==0&&payload.size()>=2&&payload[1]!=0;const int go=op==1?in.u16():0;if(!in.ok())return {remoteFailed};
    auto answer=std::make_shared<std::promise<RemoteSlides>>();auto ready=answer->get_future();
    std::thread([answer,op,go,thumbs]{const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);RemoteSlides r;try{r=readSlides(op,go,thumbs);}catch(...){}if(SUCCEEDED(com))CoUninitialize();answer->set_value(std::move(r));}).detach();
    if(ready.wait_for(std::chrono::milliseconds(3200))!=std::future_status::ready)return {remoteFailed};
    if(op==1)store_.log("Info","phone_slide");return remoteSlidesAnswer(ready.get());
}
// ---- 0.28 (revision 11) ----
// What only reads this PC is answered on the network thread that asked, never the island's: a folder's size, Windows
// Update's search and an app's icon each take their time, and none of it is the island's own. What takes longer than an
// answer may (storage, Windows Update) runs on a worker of its own, and is answered meanwhile from what it has so far. What
// each keeps lives in a store made once and never freed (a worker may outlive the window).
namespace {
using Clock=std::chrono::steady_clock;
std::wstring knownFolder(const KNOWNFOLDERID& id){PWSTR p=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(id,KF_FLAG_DONT_VERIFY,nullptr,&p))&&p)out=p;CoTaskMemFree(p);return out;}
// A program's own name for itself (its file description); empty when it has none.
std::wstring describedAs(const std::wstring& path){
    DWORD ignored=0;const DWORD size=path.empty()?0:GetFileVersionInfoSizeW(path.c_str(),&ignored);if(!size)return {};std::vector<BYTE> data(size);
    struct Translation{WORD language,codepage;}* t=nullptr;UINT n=0;wchar_t* value=nullptr;
    if(!GetFileVersionInfoW(path.c_str(),0,size,data.data())||!VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&t),&n)||n<sizeof(Translation))return {};
    wchar_t key[64];swprintf(key,64,L"\\StringFileInfo\\%04x%04x\\FileDescription",t->language,t->codepage);
    if(!VerQueryValueW(data.data(),key,reinterpret_cast<void**>(&value),&n)||!value||!n)return {};const std::wstring d=trimmed(value);return d.size()<=80?d:std::wstring();}
// Icons as a phone shows them (a PNG `side` pixels across, at most 8 KB), made once per program or app and kept (empty for
// one that has none). Made only until `until`; `later` then says one was left for another time.
struct IconStore{std::mutex m;std::map<std::wstring,std::vector<uint8_t>> made;};
IconStore& iconStore(){static auto* s=new IconStore;return *s;}
std::vector<uint8_t> iconOf(const std::wstring& name,int side,Clock::time_point until,bool* later=nullptr){
    if(name.empty())return {};auto& s=iconStore();const std::wstring key=std::to_wstring(side)+L"|"+lower(name);
    {std::lock_guard lock(s.m);if(auto it=s.made.find(key);it!=s.made.end())return it->second;}
    if(Clock::now()>until){if(later)*later=true;return {};}
    std::vector<uint8_t> png;if(auto art=shellIcon(name,side))png=encodeCover(*art,UINT(side),8*1024,.9f,true);
    std::lock_guard lock(s.m);if(s.made.size()>=800)s.made.clear();s.made[key]=png;return png;}
// ---- Top apps (24): every process's time and memory, summed by program; CPU over the time since the last ask ----
struct ProcessStore{std::mutex m;std::map<DWORD,ULONGLONG> busy;Clock::time_point at{};std::map<std::wstring,double> cpu;std::map<std::wstring,std::wstring> names;};
ProcessStore& processStore(){static auto* s=new ProcessStore;return *s;}
struct Running{DWORD pid=0,session=0;std::wstring exe,path;ULONGLONG busy=0;uint64_t memory=0;bool open=false;};
std::vector<Running> runningNow(){
    std::vector<Running> out;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return out;PROCESSENTRY32W e{};e.dwSize=sizeof(e);
    for(BOOL ok=Process32FirstW(snap,&e);ok&&out.size()<4000;ok=Process32NextW(snap,&e)){if(!e.th32ProcessID)continue;Running r;r.pid=e.th32ProcessID;r.exe=e.szExeFile;ProcessIdToSessionId(r.pid,&r.session);
        if(HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,r.pid)){r.open=true;FILETIME made,gone,kernel,user;
            if(GetProcessTimes(h,&made,&gone,&kernel,&user))r.busy=(ULONGLONG(kernel.dwHighDateTime)<<32|kernel.dwLowDateTime)+(ULONGLONG(user.dwHighDateTime)<<32|user.dwLowDateTime);
            PROCESS_MEMORY_COUNTERS m{};m.cb=sizeof(m);if(K32GetProcessMemoryInfo(h,&m,sizeof(m)))r.memory=m.WorkingSetSize;
            wchar_t path[MAX_PATH*2];DWORD n=MAX_PATH*2;if(QueryFullProcessImageNameW(h,0,path,&n))r.path.assign(path,n);CloseHandle(h);}
        out.push_back(std::move(r));}
    CloseHandle(snap);return out;}
// The processes with a window of their own on the taskbar, and its title.
std::map<DWORD,std::wstring> windowTitles(){
    std::map<DWORD,std::wstring> out;
    EnumWindows([](HWND w,LPARAM l)->BOOL{
        if(!IsWindowVisible(w)||GetWindow(w,GW_OWNER)||(GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))return TRUE;
        BOOL cloaked=FALSE;DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));wchar_t title[256]{};if(cloaked||GetWindowTextW(w,title,256)<=0)return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(w,&pid);reinterpret_cast<std::map<DWORD,std::wstring>*>(l)->emplace(pid,title);return TRUE;},reinterpret_cast<LPARAM>(&out));
    return out;}
std::vector<uint8_t> topApps(int sort,int most,bool icons){
    auto& s=processStore();std::lock_guard lock(s.m);
    // A first ask measures half a second of its own; later ones the time since the one before (a second at least, else
    // they're answered with that one's CPU, and memory as it is now).
    if(s.cpu.empty()){for(auto& r:runningNow())s.busy[r.pid]=r.busy;s.at=Clock::now();Sleep(500);}
    const auto list=runningNow();const auto now=Clock::now();const double span=std::chrono::duration<double>(now-s.at).count();const bool fresh=s.cpu.empty()?span>=.4:span>=1;
    const auto titles=windowTitles();const DWORD self=GetCurrentProcessId();const double cores=double(std::max<DWORD>(1,GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
    struct Group{RemoteProcess p;ULONGLONG busy=0;uint64_t biggest=0;std::wstring path,exe;bool windowed=false;};std::map<std::wstring,Group> groups;std::map<DWORD,ULONGLONG> busy;
    for(auto& r:list){auto& g=groups[lower(r.path.empty()?r.exe:r.path)];busy[r.pid]=r.busy;
        if(auto it=s.busy.find(r.pid);it!=s.busy.end()&&r.busy>=it->second)g.busy+=r.busy-it->second;
        g.p.memory+=r.memory;if(r.pid==self)g.p.island=true;
        // The one with a window speaks for the rest (else the biggest): its pid, its title, whether it can be ended.
        const auto w=titles.find(r.pid);const bool shown=w!=titles.end();
        if(!g.p.pid||(shown&&!g.windowed)||(!g.windowed&&!shown&&r.memory>g.biggest)){g.p.pid=r.pid;g.windowed=shown;g.biggest=r.memory;g.p.title=shown?w->second:std::wstring();g.p.locked=!r.open||r.session==0;g.path=r.path;g.exe=r.exe;}}
    if(fresh){s.cpu.clear();for(auto& [key,g]:groups)s.cpu[key]=std::clamp(double(g.busy)/(span*1e7*cores)*100,0.,100.);s.busy=std::move(busy);s.at=now;}
    std::vector<std::pair<std::wstring,Group*>> rows;
    for(auto& [key,g]:groups){g.p.window=g.windowed;const auto c=s.cpu.find(key);g.p.cpu=c==s.cpu.end()?0:c->second;rows.push_back({key,&g});}
    std::sort(rows.begin(),rows.end(),[&](auto& a,auto& b){const auto &x=a.second->p,&y=b.second->p;return sort==1||x.cpu==y.cpu?x.memory>y.memory:x.cpu>y.cpu;});
    if(rows.size()>size_t(most))rows.resize(size_t(most));
    // (Icons up to 120 KB in all, so the answer stays well inside a frame.)
    std::vector<RemoteProcess> out;const auto until=Clock::now()+std::chrono::milliseconds(1500);size_t room=120*1024;if(s.names.size()>2000)s.names.clear();
    for(auto& [key,g]:rows){auto n=s.names.find(key);
        if(n==s.names.end()){std::wstring name=describedAs(g->path);if(name.empty()){name=g->exe;if(lower(name).ends_with(L".exe"))name.resize(name.size()-4);}n=s.names.emplace(key,name).first;}
        g->p.name=n->second;if(icons){g->p.icon=iconOf(g->path,32,until);if(g->p.icon.size()>room)g->p.icon.clear();else room-=g->p.icon.size();}out.push_back(g->p);}
    return remoteProcessesAnswer(out);}
// ---- Storage (32): the drives, and the system drive's folders measured on a worker (kept ten minutes) ----
struct StorageStore{std::mutex m;PcStorage now;bool running=false,ready=false;Clock::time_point at{};};
StorageStore& storageStore(){static auto* s=new StorageStore;return *s;}
// A folder's files (their own sizes), never following a link or junction out of it; files that live only in the cloud take
// no room here (a OneDrive folder is itself a cloud reparse point, and is followed).
uint64_t folderBytes(const std::wstring& root){
    uint64_t total=0;std::vector<std::wstring> todo{root};size_t folders=0;
    while(!todo.empty()&&folders++<1'000'000){const std::wstring dir=std::move(todo.back());todo.pop_back();WIN32_FIND_DATAW f;
        HANDLE h=FindFirstFileExW((dir+L"\\*").c_str(),FindExInfoBasic,&f,FindExSearchNameMatch,nullptr,FIND_FIRST_EX_LARGE_FETCH);if(h==INVALID_HANDLE_VALUE)continue;
        do{if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!wcscmp(f.cFileName,L".")||!wcscmp(f.cFileName,L".."))continue;
                if(!(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||(f.dwReserved0&~DWORD(IO_REPARSE_TAG_CLOUD_MASK))==DWORD(IO_REPARSE_TAG_CLOUD))todo.push_back(dir+L"\\"+f.cFileName);}
            else if(!(f.dwFileAttributes&(FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS|FILE_ATTRIBUTE_OFFLINE)))total+=uint64_t(f.nFileSizeHigh)<<32|f.nFileSizeLow;}while(FindNextFileW(h,&f));
        FindClose(h);}
    return total;}
void measureStorage(){
    // At background priority (its reads too), and never a "no disk" box for an empty card reader.
    SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,nullptr);
    auto& s=storageStore();auto publish=[&](auto&& change){std::lock_guard lock(s.m);change(s.now);};
    wchar_t windows[MAX_PATH]{};GetWindowsDirectoryW(windows,MAX_PATH);const wchar_t own=wchar_t(towupper(windows[0]));
    auto drive=[&](const std::wstring& root,UINT type,PcDrive& d){ULARGE_INTEGER avail{},total{},free{};if(!GetDiskFreeSpaceExW(root.c_str(),&avail,&total,&free)||!total.QuadPart)return false;
        wchar_t label[MAX_PATH+1]{};GetVolumeInformationW(root.c_str(),label,MAX_PATH+1,nullptr,nullptr,nullptr,nullptr,0);
        d.root=root;d.label=label;d.kind=type==DRIVE_FIXED?0:type==DRIVE_REMOVABLE?1:type==DRIVE_REMOTE?2:3;d.total=total.QuadPart;d.free=std::min(free.QuadPart,total.QuadPart);d.system=towupper(root[0])==own;return true;};
    // The drives here first; a network drive can take a while to answer, so those are read last.
    std::vector<PcDrive> drives;std::vector<std::wstring> remote;const DWORD mask=GetLogicalDrives();
    for(int i=0;i<26;++i)if(mask&(1u<<i)){const std::wstring root{wchar_t(L'A'+i),L':',L'\\'};const UINT type=GetDriveTypeW(root.c_str());
        if(type==DRIVE_REMOTE)remote.push_back(root);else if(type!=DRIVE_NO_ROOT_DIR&&type!=DRIVE_UNKNOWN){PcDrive d;if(drive(root,type,d))drives.push_back(std::move(d));}}
    publish([&](PcStorage& p){p.drives=drives;});
    // The system drive's groups, each some folders on it (one inside a folder already counted isn't counted again).
    struct Group{int kind;const wchar_t* name;std::vector<std::wstring> folders;};
    const std::vector<Group> groups{{0,L"Apps",{knownFolder(FOLDERID_ProgramFiles),knownFolder(FOLDERID_ProgramFilesX86),knownFolder(FOLDERID_UserProgramFiles)}},{2,L"Documents",{knownFolder(FOLDERID_Documents)}},
        {3,L"Downloads",{knownFolder(FOLDERID_Downloads)}},{4,L"Pictures",{knownFolder(FOLDERID_Pictures)}},{5,L"Videos",{knownFolder(FOLDERID_Videos)}},{6,L"Music",{knownFolder(FOLDERID_Music)}},{7,L"Desktop",{knownFolder(FOLDERID_Desktop)}}};
    std::vector<std::wstring> counted;std::vector<PcStorageGroup> found;uint64_t sum=0;
    for(size_t i=0;i<groups.size();++i){uint64_t bytes=0;
        for(auto& f:groups[i].folders){if(f.size()<3||f[1]!=L':'||towupper(f[0])!=own)continue;const std::wstring low=lower(f)+L"\\";
            if(std::any_of(counted.begin(),counted.end(),[&](auto& c){return low.starts_with(c);}))continue;counted.push_back(low);bytes+=folderBytes(f);}
        found.push_back({groups[i].name,groups[i].kind,bytes});sum+=bytes;publish([&](PcStorage& p){p.groups=found;p.progress=int((i+1)*100/(groups.size()+1));});}
    {SHQUERYRBINFO bin{};bin.cbSize=sizeof(bin);const std::wstring root{own,L':',L'\\'};if(SUCCEEDED(SHQueryRecycleBinW(root.c_str(),&bin))){const uint64_t b=uint64_t(std::max<int64_t>(0,bin.i64Size));found.push_back({L"Recycle bin",8,b});sum+=b;}}
    // Windows and everything else (its hard links would count its files twice over, so it isn't walked): what's used, less
    // what was counted. Hence "System", not "Windows".
    for(auto& d:drives)if(d.system){const uint64_t used=d.total-d.free;found.push_back({L"System",1,used>sum?used-sum:0});}
    for(auto& r:remote){PcDrive d;if(drive(r,DRIVE_REMOTE,d))drives.push_back(std::move(d));}
    std::lock_guard lock(s.m);s.now.drives=drives;s.now.groups=found;s.now.progress=100;s.running=false;s.ready=true;s.at=Clock::now();}
std::vector<uint8_t> storageAnswer(bool again){
    auto& s=storageStore();std::unique_lock lock(s.m);
    if(!s.running&&(again||!s.ready||Clock::now()-s.at>std::chrono::minutes(10))){s.running=true;s.ready=false;s.now={};std::thread(measureStorage).detach();
        // The drives come within moments (the folders take longer: the phone asks again while it hears "measuring").
        for(int i=0;i<15&&s.running&&s.now.drives.empty();++i){lock.unlock();Sleep(20);lock.lock();}}
    PcStorage p=s.now;p.state=s.ready?1:0;if(s.ready)p.progress=100;return remoteStorageAnswer(p);}
// ---- Startup apps (33): Task Manager's list, and its own switches (StartupApproved) for the user's own entries ----
const wchar_t* const runPath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const std::wstring approvedPath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\";
// On unless its switch says off (an odd first byte: Task Manager writes 2 for on, 3 and when for off).
bool approvedOn(HKEY root,const std::wstring& key,const std::wstring& name){BYTE b[16]{};DWORD size=sizeof(b);
    return RegGetValueW(root,key.c_str(),name.c_str(),RRF_RT_REG_BINARY,nullptr,b,&size)!=ERROR_SUCCESS||!size||!(b[0]&1);}
// The program a command starts: its quoted first part, else up to ".exe" (a bare name found where Windows would find it).
std::wstring programOf(const std::wstring& command){
    wchar_t expanded[2048]{};if(!ExpandEnvironmentStringsW(command.c_str(),expanded,2048))return {};const std::wstring c=trimmed(expanded);std::wstring p;
    if(c.starts_with(L"\"")){const auto end=c.find(L'"',1);p=c.substr(1,end==std::wstring::npos?std::wstring::npos:end-1);}
    else{const auto exe=lower(c).find(L".exe");p=exe!=std::wstring::npos?c.substr(0,exe+4):c.substr(0,c.find(L' '));}
    if(!p.empty()&&p.find(L'\\')==std::wstring::npos){wchar_t found[MAX_PATH]{};if(SearchPathW(nullptr,p.c_str(),L".exe",MAX_PATH,found,nullptr))p=found;}
    return p;}
// A shortcut's program and what it's given.
bool shortcutOf(const std::wstring& lnk,std::wstring& target,std::wstring& args){
    Microsoft::WRL::ComPtr<IShellLinkW> link;Microsoft::WRL::ComPtr<IPersistFile> file;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))||FAILED(link.As(&file))||FAILED(file->Load(lnk.c_str(),STGM_READ)))return false;
    wchar_t t[MAX_PATH*2]{},a[1024]{};link->GetPath(t,MAX_PATH*2,nullptr,0);link->GetArguments(a,1024);target=t;args=a;return true;}
std::vector<RemoteStartupApp> startupApps(bool icons){
    // (Icons up to 120 KB in all, so the answer stays well inside a frame.)
    std::vector<RemoteStartupApp> out;const auto until=Clock::now()+std::chrono::milliseconds(1500);size_t room=120*1024;
    auto iconFor=[&](const std::wstring& name){auto png=iconOf(name,32,until);if(png.size()>room)png.clear();else room-=png.size();return png;};
    auto add=[&](int source,const std::wstring& key,const std::wstring& command,const std::wstring& program,bool on,const std::wstring& fallback,const std::wstring& icon){
        RemoteStartupApp a;a.id=std::to_wstring(source)+L":"+key;a.source=source;a.command=command;a.enabled=on;a.canChange=source==0||source==3;
        a.name=describedAs(program);if(a.name.empty())a.name=fallback;if(icons)a.icon=iconFor(icon.empty()?program:icon);out.push_back(std::move(a));};
    // The Run keys: the user's, this PC's, and this PC's for 32-bit programs (each with its switches beside it).
    struct Run{int source;HKEY root;REGSAM view;const wchar_t* approved;};
    for(const Run r:{Run{0,HKEY_CURRENT_USER,0,L"Run"},Run{1,HKEY_LOCAL_MACHINE,KEY_WOW64_64KEY,L"Run"},Run{2,HKEY_LOCAL_MACHINE,KEY_WOW64_32KEY,L"Run32"}}){
        HKEY k=nullptr;if(RegOpenKeyExW(r.root,runPath,0,KEY_READ|r.view,&k)!=ERROR_SUCCESS)continue;
        for(DWORD i=0;i<200;++i){wchar_t name[512];DWORD nameSize=512,type=0;std::vector<BYTE> data(8192);DWORD dataSize=DWORD(data.size());
            const LONG got=RegEnumValueW(k,i,name,&nameSize,nullptr,&type,data.data(),&dataSize);if(got==ERROR_NO_MORE_ITEMS)break;
            if(got!=ERROR_SUCCESS||(type!=REG_SZ&&type!=REG_EXPAND_SZ)||!nameSize)continue;
            std::wstring command(reinterpret_cast<const wchar_t*>(data.data()),dataSize/sizeof(wchar_t));while(!command.empty()&&!command.back())command.pop_back();if(trimmed(command).empty())continue;
            add(r.source,name,command,programOf(command),approvedOn(r.root,approvedPath+r.approved,name),name,{});}
        RegCloseKey(k);}
    // The Startup folders, the user's and everyone's (a shortcut's own icon shown).
    for(const auto& [source,folder,root]:{std::tuple{3,knownFolder(FOLDERID_Startup),HKEY_CURRENT_USER},std::tuple{4,knownFolder(FOLDERID_CommonStartup),HKEY_LOCAL_MACHINE}}){
        WIN32_FIND_DATAW f;HANDLE h=folder.empty()?INVALID_HANDLE_VALUE:FindFirstFileW((folder+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)continue;
        do{if(f.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))continue;const std::wstring file=f.cFileName,path=folder+L"\\"+file;std::wstring target,args,command=path;
            if(lower(file).ends_with(L".lnk")&&shortcutOf(path,target,args)&&!target.empty())command=L"\""+target+L"\""+(args.empty()?L"":L" "+args);else target=path;
            add(source,file,command,target,approvedOn(root,approvedPath+L"StartupFolder",file),std::filesystem::path(file).stem().wstring(),path);}while(FindNextFileW(h,&f));
        FindClose(h);}
    // Store apps' startup tasks (a package's task and its state: 2 or 4 on), named as the Start menu names them.
    HKEY k=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\SystemAppData",0,KEY_READ,&k)==ERROR_SUCCESS){
        for(DWORD i=0;i<4000&&out.size()<100;++i){wchar_t family[256];DWORD n=256;if(RegEnumKeyExW(k,i,family,&n,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)break;HKEY p=nullptr;
            if(RegOpenKeyExW(k,family,0,KEY_READ,&p)!=ERROR_SUCCESS)continue;
            for(DWORD j=0;j<32;++j){wchar_t task[256];DWORD m=256;if(RegEnumKeyExW(p,j,task,&m,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)break;DWORD state=0,size=sizeof(state);
                if(RegGetValueW(p,task,L"State",RRF_RT_REG_DWORD,nullptr,&state,&size)!=ERROR_SUCCESS)continue;const std::wstring app=packagedAppId(family);if(app.empty())continue;
                RemoteStartupApp a;a.id=L"5:"+std::wstring(family)+L"/"+task;a.source=5;a.name=packagedAppName(family);a.command=app;a.enabled=state==2||state==4;
                if(icons)a.icon=iconFor(L"shell:AppsFolder\\"+app);out.push_back(std::move(a));}
            RegCloseKey(p);}
        RegCloseKey(k);}
    return out;}
std::vector<uint8_t> startupAnswer(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};
    if(op==1){const std::wstring id=in.text(4096);const bool on=in.u8()!=0;if(!in.ok())return {remoteFailed};
        const auto list=startupApps(false);const auto it=std::find_if(list.begin(),list.end(),[&](auto& a){return a.id==id;});if(it==list.end())return {remoteFailed};if(!it->canChange)return {remoteNotAllowed};
        // As Task Manager switches it, in the user's own StartupApproved (no rights needed): 2 on; 3 off, and when.
        BYTE b[12]{};b[0]=on?2:3;if(!on){FILETIME f;GetSystemTimeAsFileTime(&f);std::memcpy(b+4,&f,8);}HKEY k=nullptr;
        if(RegCreateKeyExW(HKEY_CURRENT_USER,(approvedPath+(it->source==0?L"Run":L"StartupFolder")).c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return {remoteFailed};
        const bool done=RegSetValueExW(k,id.substr(2).c_str(),0,REG_BINARY,b,sizeof(b))==ERROR_SUCCESS;RegCloseKey(k);if(!done)return {remoteFailed};}
    else if(op!=0)return {remoteUnsupported};
    return remoteStartupAnswer(startupApps(true));}
// ---- The network (34): every adapter, and for Wi-Fi the network it's on ----
std::string addressText(const sockaddr* sa){char t[64]{};if(!sa)return {};
    if(sa->sa_family==AF_INET)inet_ntop(AF_INET,&reinterpret_cast<const sockaddr_in*>(sa)->sin_addr,t,sizeof(t));
    else if(sa->sa_family==AF_INET6)inet_ntop(AF_INET6,&reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr,t,sizeof(t));
    return t;}
struct Wifi{std::wstring ssid;int signal=255,channel=0;std::string band,security;};
std::string bandOf(ULONG kHz,int channel){if(kHz>=5925000&&kHz<=7125000)return "6 GHz";if(kHz>=4900000&&kHz<5925000)return "5 GHz";if(kHz>=2400000&&kHz<2500000)return "2.4 GHz";
    return channel>=1&&channel<=14?"2.4 GHz":channel>=32&&channel<=177?"5 GHz":"";}
std::string securityOf(int algorithm,bool secured){
    switch(algorithm){case 1:return secured?"WEP":"Open";case 2:return "WEP";case 3:return "WPA-Enterprise";case 4:return "WPA-Personal";case 6:return "WPA2-Enterprise";case 7:return "WPA2-Personal";
    case 8:case 11:return "WPA3-Enterprise";case 9:return "WPA3-Personal";case 10:return "Enhanced Open";default:return "";}}
// Each connected Wi-Fi interface's network, by its GUID. Windows 11 may keep the network's name (and the rest of it) from an
// app without location access: then only its channel, if that.
std::map<std::wstring,Wifi> wifiNow(){
    std::map<std::wstring,Wifi> out;HANDLE h=nullptr;DWORD version=0;if(WlanOpenHandle(2,nullptr,&version,&h)!=ERROR_SUCCESS)return out;PWLAN_INTERFACE_INFO_LIST list=nullptr;
    if(WlanEnumInterfaces(h,nullptr,&list)==ERROR_SUCCESS&&list)for(DWORD i=0;i<list->dwNumberOfItems;++i){const auto& it=list->InterfaceInfo[i];if(it.isState!=wlan_interface_state_connected)continue;
        Wifi w;DWORD size=0;PVOID data=nullptr;ULONG kHz=0;
        if(WlanQueryInterface(h,&it.InterfaceGuid,wlan_intf_opcode_channel_number,nullptr,&size,&data,nullptr)==ERROR_SUCCESS&&data){w.channel=int(*static_cast<ULONG*>(data));WlanFreeMemory(data);}
        data=nullptr;
        if(WlanQueryInterface(h,&it.InterfaceGuid,wlan_intf_opcode_current_connection,nullptr,&size,&data,nullptr)==ERROR_SUCCESS&&data){const auto* c=static_cast<const WLAN_CONNECTION_ATTRIBUTES*>(data);const auto& as=c->wlanAssociationAttributes;
            w.ssid=wideOf(std::string(reinterpret_cast<const char*>(as.dot11Ssid.ucSSID),std::min<ULONG>(as.dot11Ssid.uSSIDLength,32)).c_str());w.signal=int(std::min<ULONG>(as.wlanSignalQuality,100));
            w.security=securityOf(int(c->wlanSecurityAttributes.dot11AuthAlgorithm),c->wlanSecurityAttributes.bSecurityEnabled);
            // The band from the access point's own frequency (a channel number alone can't tell 6 GHz from 2.4 or 5).
            DOT11_SSID ssid=as.dot11Ssid;PWLAN_BSS_LIST bss=nullptr;
            if(WlanGetNetworkBssList(h,&it.InterfaceGuid,&ssid,dot11_BSS_type_infrastructure,c->wlanSecurityAttributes.bSecurityEnabled,nullptr,&bss)==ERROR_SUCCESS&&bss){
                for(DWORD k=0;k<bss->dwNumberOfItems;++k)if(!std::memcmp(bss->wlanBssEntries[k].dot11Bssid,as.dot11Bssid,6)){kHz=bss->wlanBssEntries[k].ulChCenterFrequency;break;}WlanFreeMemory(bss);}
            WlanFreeMemory(data);}
        w.band=bandOf(kHz,w.channel);wchar_t id[64]{};StringFromGUID2(it.InterfaceGuid,id,64);out[lower(id)]=w;}
    if(list)WlanFreeMemory(list);WlanCloseHandle(h,nullptr);return out;}
// The name of the network each adapter is on (Windows' own, which for Wi-Fi is its SSID; no location access needed).
std::map<std::wstring,std::wstring> networkNames(){
    std::map<std::wstring,std::wstring> out;Microsoft::WRL::ComPtr<INetworkListManager> manager;Microsoft::WRL::ComPtr<IEnumNetworkConnections> list;
    if(FAILED(CoCreateInstance(__uuidof(NetworkListManager),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&manager)))||FAILED(manager->GetNetworkConnections(&list))||!list)return out;
    for(int guard=0;guard<64;++guard){Microsoft::WRL::ComPtr<INetworkConnection> c;ULONG got=0;if(list->Next(1,&c,&got)!=S_OK||!got)break;GUID adapter{};Microsoft::WRL::ComPtr<INetwork> network;BSTR name=nullptr;
        if(SUCCEEDED(c->GetAdapterId(&adapter))&&SUCCEEDED(c->GetNetwork(&network))&&network&&SUCCEEDED(network->GetName(&name))&&name){wchar_t id[64]{};StringFromGUID2(adapter,id,64);out[lower(id)]=std::wstring(name,SysStringLen(name));}
        SysFreeString(name);}
    return out;}
std::vector<PcNetwork> networkNow(){
    ULONG size=32*1024;std::vector<uint8_t> table(size);const ULONG flags=GAA_FLAG_INCLUDE_GATEWAYS|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST;
    ULONG r=GetAdaptersAddresses(AF_UNSPEC,flags,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(table.data()),&size);
    if(r==ERROR_BUFFER_OVERFLOW){table.resize(size);r=GetAdaptersAddresses(AF_UNSPEC,flags,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(table.data()),&size);}
    std::vector<PcNetwork> out;if(r!=NO_ERROR)return out;
    // The adapter the default route leaves by (asked of the routing table; nothing is sent).
    DWORD route4=0,route6=0;{sockaddr_in a{};a.sin_family=AF_INET;inet_pton(AF_INET,"8.8.8.8",&a.sin_addr);if(GetBestInterfaceEx(reinterpret_cast<sockaddr*>(&a),&route4)!=NO_ERROR)route4=0;
        sockaddr_in6 b{};b.sin6_family=AF_INET6;inet_pton(AF_INET6,"2001:4860:4860::8888",&b.sin6_addr);if(GetBestInterfaceEx(reinterpret_cast<sockaddr*>(&b),&route6)!=NO_ERROR)route6=0;}
    const auto wifi=wifiNow();const auto names=networkNames();
    for(auto* a=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(table.data());a&&out.size()<16;a=a->Next){
        if(a->IfType==IF_TYPE_SOFTWARE_LOOPBACK||a->IfType==IF_TYPE_TUNNEL)continue;
        PcNetwork n;n.kind=a->IfType==IF_TYPE_ETHERNET_CSMACD?0:a->IfType==IF_TYPE_IEEE80211?1:2;n.up=a->OperStatus==IfOperStatusUp;
        // One that's down only when it's a real Wi-Fi or Ethernet adapter (not a virtual switch's or a VPN's).
        if(!n.up){if(n.kind==2)continue;MIB_IF_ROW2 row{};row.InterfaceLuid=a->Luid;if(GetIfEntry2(&row)!=NO_ERROR||!row.InterfaceAndOperStatusFlags.HardwareInterface)continue;}
        n.name=a->FriendlyName?a->FriendlyName:L"";n.description=a->Description?a->Description:L"";n.isDefault=n.up&&((route4&&a->IfIndex==route4)||(route6&&a->Ipv6IfIndex==route6));
        if(a->PhysicalAddressLength){char mac[32]{};int at=0;for(ULONG i=0;i<a->PhysicalAddressLength&&i<8;++i)at+=std::snprintf(mac+at,sizeof(mac)-size_t(at),i?":%02X":"%02X",a->PhysicalAddress[i]);n.mac=mac;}
        if(n.up)n.speed=a->ReceiveLinkSpeed!=~0ull?a->ReceiveLinkSpeed:a->TransmitLinkSpeed!=~0ull?a->TransmitLinkSpeed:0;
        for(auto* u=a->FirstUnicastAddress;u;u=u->Next){const auto* sa=u->Address.lpSockaddr;const std::string t=addressText(sa);if(t.empty())continue;
            if(sa->sa_family==AF_INET)n.ipv4.push_back(t+"/"+std::to_string(u->OnLinkPrefixLength));else n.ipv6.push_back(t);}
        for(auto* g=a->FirstGatewayAddress;g;g=g->Next){const auto* sa=g->Address.lpSockaddr;if(sa&&(n.gateway.empty()||(sa->sa_family==AF_INET&&n.gateway.find(':')!=std::string::npos)))n.gateway=addressText(sa);}
        // (Each once; Windows' placeholder servers, fec0:0:0:ffff::1-3, for an adapter that has none, aren't any.)
        for(auto* d=a->FirstDnsServerAddress;d&&n.dns.size()<8;d=d->Next)if(auto t=addressText(d->Address.lpSockaddr);!t.empty()&&!t.starts_with("fec0:0:0:ffff::")&&std::find(n.dns.begin(),n.dns.end(),t)==n.dns.end())n.dns.push_back(t);
        if(n.kind==1){const std::wstring guid=lower(wideOf(a->AdapterName));
            if(auto w=wifi.find(guid);w!=wifi.end()){n.ssid=w->second.ssid;n.signal=w->second.signal;n.channel=w->second.channel;n.band=w->second.band;n.security=w->second.security;}
            if(n.ssid.empty()&&n.up)if(auto m=names.find(guid);m!=names.end())n.ssid=m->second;}
        out.push_back(std::move(n));}
    return out;}
// ---- Windows Update (35): its own search on a worker (never an install): from what Windows already knows when only read,
// online when the phone asks to check; kept for half an hour ----
struct UpdateStore{std::mutex m;PcUpdates found;bool have=false,searching=false;Clock::time_point at{},began{};uint64_t checkedHere=0;};
UpdateStore& updateStore(){static auto* s=new UpdateStore;return *s;}
Microsoft::WRL::ComPtr<IDispatch> created(const wchar_t* progId){CLSID clsid;Microsoft::WRL::ComPtr<IDispatch> d;
    if(SUCCEEDED(CLSIDFromProgID(progId,&clsid)))CoCreateInstance(clsid,nullptr,CLSCTX_INPROC_SERVER|CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&d));return d;}
bool setProperty(IDispatch* d,const wchar_t* name,VARIANT value){
    if(!d)return false;DISPID id=0;LPOLESTR n=const_cast<LPOLESTR>(name);if(FAILED(d->GetIDsOfNames(IID_NULL,&n,1,LOCALE_USER_DEFAULT,&id)))return false;
    DISPID named=DISPID_PROPERTYPUT;DISPPARAMS p{&value,&named,1,1};return SUCCEEDED(d->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_PROPERTYPUT,&p,nullptr,nullptr,nullptr));}
bool yes(IDispatch* d,const wchar_t* name){const double v=real(d,name);return std::isfinite(v)&&v!=0;}
uint64_t unixOf(double date){return std::isfinite(date)&&date>25569?uint64_t((date-25569)*86400):0;}
bool restartPending(){HKEY k=nullptr;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired",0,KEY_READ|KEY_WOW64_64KEY,&k)!=ERROR_SUCCESS)return false;RegCloseKey(k);return true;}
// What an update is, by its classification (the same in every language): a driver, a feature update, definitions, or a
// quality one (security, critical, rollups, plain updates, service packs).
int updateKind(IDispatch* u){
    if(whole(u,L"Type")==2)return 1;
    if(auto c=object(u,L"Categories"))for(int i=0,n=whole(c.Get(),L"Count");i<n&&i<16;++i)if(auto one=object(c.Get(),L"Item",{number(i)})){const std::wstring id=lower(words(one.Get(),L"CategoryID"));
        if(id==L"e0789628-ce08-4437-be74-2495b842f43b")return 3;if(id==L"ebfc1fc5-71a4-4f7b-9aca-3b9a503104a0")return 1;
        if(id==L"3689bdc8-b205-4af4-8d4a-a63924c5e9d5"||id==L"b54e7d24-7add-428f-8b75-90a396fa584f")return 2;
        if(id==L"0fa1201d-4330-4fa8-8ae9-b877473b6441"||id==L"e6cf1350-c01b-414d-a61f-263d14d133b4"||id==L"28bc880e-0592-4cbf-8f95-c79b17911d5f"||id==L"cd5ffd1e-e932-4e3a-bf74-18bf0b1bbd83"||id==L"68c5b0a3-d1a6-4553-ae49-01d3a7827828")return 0;}
    return 4;}
void searchUpdates(bool online){
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);PcUpdates u;bool ok=false;
    try{if(auto session=created(L"Microsoft.Update.Session")){Var app;app.v.vt=VT_BSTR;app.v.bstrVal=SysAllocString(L"Arnav Island");setProperty(session.Get(),L"ClientApplicationID",app.v);
            if(auto searcher=object(session.Get(),L"CreateUpdateSearcher")){VARIANT where;VariantInit(&where);where.vt=VT_BOOL;where.boolVal=online?VARIANT_TRUE:VARIANT_FALSE;setProperty(searcher.Get(),L"Online",where);
                Var criteria;criteria.v.vt=VT_BSTR;criteria.v.bstrVal=SysAllocString(L"IsInstalled=0 and IsHidden=0");
                if(auto result=object(searcher.Get(),L"Search",{criteria.v})){ok=true;
                    if(auto list=object(result.Get(),L"Updates"))for(int i=0,n=whole(list.Get(),L"Count");i<n&&i<100;++i)if(auto one=object(list.Get(),L"Item",{number(i)})){
                        PcUpdate e;e.title=words(one.Get(),L"Title");e.kind=updateKind(one.Get());const double size=real(one.Get(),L"MaxDownloadSize");e.size=std::isfinite(size)&&size>0&&size<1e15?uint64_t(size):0;
                        e.downloaded=yes(one.Get(),L"IsDownloaded");if(auto how=object(one.Get(),L"InstallationBehavior"))e.restart=whole(how.Get(),L"RebootBehavior")!=0;u.updates.push_back(std::move(e));}}}}
        // When Windows itself last searched and installed, and whether it waits for a restart.
        if(auto au=created(L"Microsoft.Update.AutoUpdate"))if(auto results=object(au.Get(),L"Results")){u.checked=unixOf(real(results.Get(),L"LastSearchSuccessDate"));u.installed=unixOf(real(results.Get(),L"LastInstallationSuccessDate"));}
        bool restart=restartPending();if(!restart)if(auto info=created(L"Microsoft.Update.SystemInfo"))restart=yes(info.Get(),L"RebootRequired");
        u.state=restart?3:!ok?0:u.updates.empty()?1:2;}catch(...){}
    if(SUCCEEDED(com))CoUninitialize();
    auto& s=updateStore();std::lock_guard lock(s.m);s.searching=false;s.at=Clock::now();
    if(ok||!s.have){if(ok&&online)s.checkedHere=uint64_t(std::time(nullptr));s.found=std::move(u);s.have=true;}}
std::vector<uint8_t> updatesAnswer(int op){
    auto& s=updateStore();PcUpdates u;
    {std::lock_guard lock(s.m);const auto now=Clock::now();if(s.searching&&now-s.began>std::chrono::minutes(10))s.searching=false;// (one stuck that long is given up on)
        if(!s.searching&&(op==1||!s.have||now-s.at>std::chrono::minutes(30))){s.searching=true;s.began=now;std::thread([online=op==1]{searchUpdates(online);}).detach();}
        u=s.found;u.checked=std::max(u.checked,s.checkedHere);if(s.searching)u.state=4;else if(restartPending())u.state=3;}
    return remoteUpdatesAnswer(u);}
// ---- The Start menu's apps (37): the command bar's own list, by name ----
std::atomic<bool> appIconsComing{false};
std::vector<uint8_t> appsAnswer(bool icons){
    std::vector<RemoteApp> out;for(auto& a:installedApps())out.push_back({a.id,a.name,{}});
    std::sort(out.begin(),out.end(),[](auto& a,auto& b){return CompareStringEx(LOCALE_NAME_USER_DEFAULT,LINGUISTIC_IGNORECASE|SORT_DIGITSASNUMBERS,a.name.c_str(),int(a.name.size()),b.name.c_str(),int(b.name.size()),nullptr,nullptr,0)==CSTR_LESS_THAN;});
    if(icons){
        // As many as fit beside the names in an answer of 200 KB at most (180 KB of icons; a frame is 256 KB), the rest sent
        // without; those not made in time are made after it, for the next ask.
        size_t room=200*1024;for(auto& a:out){const size_t text=size_t(WideCharToMultiByte(CP_UTF8,0,a.id.c_str(),int(a.id.size()),nullptr,0,nullptr,nullptr)+WideCharToMultiByte(CP_UTF8,0,a.name.c_str(),int(a.name.size()),nullptr,0,nullptr,nullptr))+12;room=room>text?room-text:0;}
        room=std::min<size_t>(room,180*1024);
        const auto until=Clock::now()+std::chrono::milliseconds(2000);std::vector<std::wstring> rest;
        for(auto& a:out){bool later=false;const std::wstring name=L"shell:AppsFolder\\"+a.id;a.icon=iconOf(name,48,until,&later);if(later)rest.push_back(name);if(a.icon.size()>room)a.icon.clear();else room-=a.icon.size();}
        if(!rest.empty()&&!appIconsComing.exchange(true))std::thread([rest]{const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);for(auto& n:rest)iconOf(n,48,Clock::time_point::max());if(SUCCEEDED(com))CoUninitialize();appIconsComing=false;}).detach();}
    return remoteAppsAnswer(out);}
}
// On the network thread that asked (COM as it needs it there): top apps, storage, startup apps, the network, Windows
// Update and the apps list; empty for what the island's thread answers (an app opened, Windows Update's page).
std::vector<uint8_t> IslandWindow::remoteSystem(RemoteCommand command,const std::vector<uint8_t>& payload){
    const int op=payload.empty()?-1:payload[0];
    switch(command){case RemoteCommand::Processes:case RemoteCommand::Storage:case RemoteCommand::Startup:case RemoteCommand::Network:break;
        case RemoteCommand::Updates:if(op==2)return {};break;case RemoteCommand::Apps:if(op==1)return {};break;default:return {};}
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::vector<uint8_t> answer;
    try{RemoteReader in(payload);in.u8();
        switch(command){
        // Top apps, read only (ending one from the phone isn't offered).
        case RemoteCommand::Processes:{if(op==1){answer={remoteUnsupported};break;}const int sort=in.u8(),most=in.u8(),icons=in.u8();
            answer=!in.ok()?std::vector<uint8_t>{remoteFailed}:op!=0?std::vector<uint8_t>{remoteUnsupported}:topApps(sort,std::min(most,30),icons!=0);break;}
        case RemoteCommand::Storage:answer=!in.ok()?std::vector<uint8_t>{remoteFailed}:op>1?std::vector<uint8_t>{remoteUnsupported}:storageAnswer(op==1);break;
        case RemoteCommand::Startup:answer=startupAnswer(payload);break;
        case RemoteCommand::Network:answer=remoteNetworkAnswer(networkNow());break;
        case RemoteCommand::Updates:answer=!in.ok()?std::vector<uint8_t>{remoteFailed}:op>1?std::vector<uint8_t>{remoteUnsupported}:updatesAnswer(op);break;
        case RemoteCommand::Apps:{const bool icons=in.u8()!=0;answer=!in.ok()?std::vector<uint8_t>{remoteFailed}:op!=0?std::vector<uint8_t>{remoteUnsupported}:appsAnswer(icons);break;}
        default:break;}}
    catch(...){answer={remoteFailed};}
    if(SUCCEEDED(com))CoUninitialize();if(answer.empty())answer={remoteFailed};return answer;
}
// Battery health: the battery driver's own numbers, its first battery (no capacities from one that reports them only
// relative; a driver that doesn't count cycles says 0, sent as unknown).
std::vector<uint8_t> IslandWindow::remoteBatteryHealth(){
    PcBatteryHealth h;if(!battery_)return remoteBatteryHealthAnswer(h);const auto r=battery_->reading();battery_->refresh();h.present=r.present;
    if(r.present){auto mwh=[&](long long v){return r.relative||v<=0||v>0x7fffffffLL?0u:uint32_t(v);};h.designMwh=mwh(r.designMwh);h.fullMwh=mwh(r.fullMwh);h.currentMwh=mwh(r.remainingMwh);
        h.cycles=r.cycles?r.cycles:0xFFFFFFFFu;h.rateMw=r.relative?0:int32_t(std::clamp<long long>(r.rateMw,-2000000000LL,2000000000LL));h.voltageMv=r.voltageMv>0&&r.voltageMv<1000000?uint32_t(r.voltageMv):0;
        // The driver's four letters ("LION", "LiP ") as people read them.
        const std::wstring c=lower(trimmed(r.chemistry));
        h.chemistry=c.starts_with(L"lip")||c==L"lp"?L"LiP":c.starts_with(L"li")?L"Li-ion":c==L"pbac"?L"Lead acid":c==L"nimh"?L"NiMH":c==L"nicd"?L"NiCd":c==L"nizn"?L"NiZn":trimmed(r.chemistry);
        h.manufacturer=r.manufacturer;h.name=r.name;}
    return remoteBatteryHealthAnswer(h);
}
// Apps: listed off this thread; one opened as the command bar opens it (and remembered as its Open would be).
std::vector<uint8_t> IslandWindow::remoteApps(const std::vector<uint8_t>& payload){
    RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};if(op!=1)return remoteSystem(RemoteCommand::Apps,payload);
    const std::wstring id=in.text(8192);if(!in.ok()||id.empty())return {remoteFailed};const std::wstring key=lower(id);
    for(auto& a:installedApps())if(lower(a.id)==key){CommandResult r;r.kind=CommandKind::OpenApp;r.title=L"Open "+a.name;r.detail=L"App";r.target=a.id;r.appId=a.id;
        return runRemote(r,true).first==0?std::vector<uint8_t>{remoteOk,1}:std::vector<uint8_t>{remoteFailed};}
    return {remoteFailed};
}
// Windows Update: its page opened here (and what it says now); reading and checking go off this thread.
std::vector<uint8_t> IslandWindow::remoteUpdates(const std::vector<uint8_t>& payload){
    if(payload.empty()||payload[0]!=2)return remoteSystem(RemoteCommand::Updates,payload);
    if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"ms-settings:windowsupdate",nullptr,nullptr,SW_SHOWNORMAL))<=32)return {remoteFailed};
    store_.log("Info","phone_updates_page");return remoteSystem(RemoteCommand::Updates,{0});
}
}
