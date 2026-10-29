// 0.22 (revision 5): a paired phone controls the whole island. Every answer here is built on the UI thread (the network
// thread waits up to 4 s for it) and says what the island has now; anything slow (radios, dark mode, the recycle bin) is
// started on a worker and read back by the next ask. Sleeping, restarting and shutting down wait until the answer has
// gone, so the phone hears that they started.
// 0.26 (revision 9): shuffle, repeat and up next, each app's volume, the sound's levels, and the adapters to wake this PC.
// 0.27 (revision 10): this PC's temperatures, the island's clipboard history, and the PowerPoint show that runs here.
#include <winsock2.h>
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
}
