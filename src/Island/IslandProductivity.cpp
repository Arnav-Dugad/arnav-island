#include "Island/IslandWindow.h"
#include <shlobj.h>
namespace nexus {
namespace {
constexpr int HotkeyId=0x4e49;
constexpr UINT_PTR ClipboardRetryTimer=31,CommandCloseTimer=32;
constexpr UINT WorkspaceMessage=WM_APP+32;
// Result of a workspace save or open, finished on a worker thread.
struct WorkspaceDone {bool save=false;std::wstring name;std::vector<WorkspaceApp> apps;LaunchReport report;};
std::wstring userFolder(){PWSTR path=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile,0,nullptr,&path))&&path)out=path;CoTaskMemFree(path);return out;}
std::wstring copiedLabel(const ClipEntry& e){
    switch(e.kind){case ClipEntry::Kind::Link:return L"Copied link";case ClipEntry::Kind::Image:return L"Copied image";
        case ClipEntry::Kind::Files:return e.files.size()==1?L"Copied 1 file":L"Copied "+std::to_wstring(e.files.size())+L" files";default:return L"Copied text";}
}
}
// Providers follow preferences: clipboard listening only when history is on,
// the privacy watcher only when a privacy feature is on, the shortcut on demand.
void IslandWindow::syncProductivity(){
    if(settings_.clipboardHistory&&!testing_)clipboard_.start(window_);
    else{clipboard_.stop();if(!clips_.entries().empty()||!content_.clips.empty()){clips_.clear();clipViews();}}
    const bool privacy=(settings_.privacyDots||settings_.privacyCards)&&!testing_;
    if(privacy&&!privacy_)privacy_=std::make_unique<PrivacyProvider>(window_);
    else if(!privacy&&privacy_){privacy_.reset();privacyUses_.clear();content_.privacy.clear();}
    if(!settings_.privacyDots)content_.privacy.clear();else content_.privacy=privacyUses_;
    if(!commands_)commands_=std::make_unique<CommandService>(window_,userFolder());
    syncHotkey();
}
void IslandWindow::syncHotkey(){
    const int want=testing_?0:settings_.commandShortcut;if(want==hotkeyChoice_)return;
    if(hotkey_){UnregisterHotKey(window_,HotkeyId);hotkey_=false;}hotkeyChoice_=want;if(!want)return;
    const UINT modifiers[]={0,MOD_ALT|MOD_SHIFT,MOD_CONTROL|MOD_ALT,MOD_WIN|MOD_ALT};
    hotkey_=RegisterHotKey(window_,HotkeyId,modifiers[want]|MOD_NOREPEAT,VK_SPACE)!=FALSE;
    // Another app owns it (Claude's Quick Entry uses Ctrl+Alt+Space): Settings says so.
    if(!hotkey_)store_.log("Warning","command_shortcut_taken");
    if(settingsWindow_&&settingsWindow_->open())settingsWindow_->update(settings_,settingsContext(),settingsSequence_);
}
// ---- Clipboard -------------------------------------------------------------
void IslandWindow::onClipboard(){
    if(!settings_.clipboardHistory||clips_.paused)return;
    ClipEntry e;auto read=clipboard_.capture(e);
    if(read==ClipboardWatcher::Read::Busy){if(clipRetries_++<5)SetTimer(window_,ClipboardRetryTimer,80,nullptr);return;}
    clipRetries_=0;if(read!=ClipboardWatcher::Read::Captured)return;
    if(e.kind==ClipEntry::Kind::Image){e.thumbnail=dibThumbnail(e.dib,96,&e.imageWidth,&e.imageHeight);if(!e.thumbnail)return;}
    if(!e.sourcePath.empty()){auto it=clipIcons_.find(e.sourcePath);if(it==clipIcons_.end()){if(clipIcons_.size()>64)clipIcons_.clear();it=clipIcons_.emplace(e.sourcePath,shellIcon(e.sourcePath,32)).first;}e.sourceIcon=it->second;}
    auto label=copiedLabel(e);if(!clips_.add(std::move(e),seconds()))return;
    content_.clipOffset=0;clipViews();store_.log("Info","clipboard_kept");
    if(settings_.clipboardConfirm&&!(settings_.autoHide&&autoHide_.hidden&&!settings_.alertsReveal)){copyLabel_=label;events_.publish({ActivityKind::Clipboard,"clipboard",25,0,.6,2.2},seconds());presentActivity();}
    else refresh();
}
void IslandWindow::clipViews(){
    const double now=seconds();content_.clips.clear();content_.clipsPaused=clips_.paused;
    for(auto& e:clips_.entries()){ContentSnapshot::Clip c;c.id=e.id;c.kind=int(e.kind);c.icon=e.sourceIcon;c.thumbnail=e.thumbnail;
        switch(e.kind){
        case ClipEntry::Kind::Image:c.preview=L"Image  ·  "+std::to_wstring(e.imageWidth)+L" × "+std::to_wstring(e.imageHeight);break;
        case ClipEntry::Kind::Files:{auto& f=e.files.front();auto slash=f.find_last_of(L"\\/");c.preview=slash==std::wstring::npos?f:f.substr(slash+1);if(e.files.size()>1)c.preview+=L" and "+std::to_wstring(e.files.size()-1)+L" more";break;}
        default:c.preview=clipPreview(e.text,90);}
        c.meta=(e.source.empty()?std::wstring(L"Copied"):e.source)+L"  ·  "+ageText(now-e.time);content_.clips.push_back(std::move(c));}
    content_.clipOffset=std::clamp(content_.clipOffset,0,std::max(0,int(content_.clips.size())-4));
}
void IslandWindow::clearClips(){clips_.clear();clipViews();store_.log("Info","clipboard_cleared");refresh();}
void IslandWindow::copyClip(size_t index){
    if(index>=clips_.entries().size())return;ClipEntry e=clips_.entries()[index];
    if(!clipboard_.copy(e)){content_.clipStatus=L"The clipboard is busy; try again";content_.clipStatusUntil=seconds()+2.5;refresh();return;}
    clips_.add(std::move(e),seconds());content_.clipOffset=0;clipViews();content_.clipStatus=L"Copied again";content_.clipStatusUntil=seconds()+2.5;
    motion_.pulse.reset(motion_.reduced?.12:.5,seconds());motion_.pulse.retarget(0,seconds(),{1,70,16});refresh();animate();
}
// ---- Privacy ---------------------------------------------------------------
void IslandWindow::updatePrivacy(){
    if(!privacy_)return;auto now=privacy_->uses();
    // A card for each camera or microphone that newly starts; location changes only the dots.
    if(settings_.privacyCards)for(auto& u:now)if(u.capability!=Capability::Location&&std::find(privacyUses_.begin(),privacyUses_.end(),u)==privacyUses_.end()){showPrivacyNotice(u);break;}
    bool changed=now.size()!=privacyUses_.size();privacyUses_=std::move(now);content_.privacy=settings_.privacyDots?privacyUses_:std::vector<PrivacyUse>{};
    if(changed)store_.log("Info",privacyUses_.empty()?"privacy_indicators_cleared":"privacy_indicators_shown");
    refresh();animate();
}
void IslandWindow::showPrivacyNotice(const PrivacyUse& u){
    if(!renderer_||(state_!=IslandState::Compact&&state_!=IslandState::Notification))return;
    if(settings_.autoHide&&autoHide_.hidden&&!settings_.alertsReveal)return;
    content_.notice={u.capability==Capability::Camera?5:u.capability==Capability::Microphone?6:7,{},u.app,u.icon};
    events_.publish({ActivityKind::Notification,"privacy",70,double(content_.notice.kind),2.4,3.4},seconds());
    transition(IslandState::Notification);presentActivity();store_.log("Info","privacy_card_shown");
}
// ---- Command bar -----------------------------------------------------------
void IslandWindow::openCommand(){
    if(content_.command.active){SetForegroundWindow(window_);return;}
    commandReturn_=GetForegroundWindow();if(commandReturn_==window_)commandReturn_=nullptr;
    content_.command=ContentSnapshot::Command{};content_.command.active=true;content_.notice.kind=0;content_.pinned=false;KillTimer(window_,CommandCloseTimer);
    // The island takes keyboard focus only while the command bar is open.
    SetWindowLongPtrW(window_,GWL_EXSTYLE,GetWindowLongPtrW(window_,GWL_EXSTYLE)&~WS_EX_NOACTIVATE);
    motion_.commandHeight=commandIslandHeight(3);transition(IslandState::Command);commandQuery();
    SetForegroundWindow(window_);SetFocus(window_);store_.log("Info","command_bar_opened");
}
void IslandWindow::closeCommand(bool restoreFocus){
    if(!content_.command.active)return;KillTimer(window_,CommandCloseTimer);content_.command=ContentSnapshot::Command{};content_.hovered=Action::None;
    SetWindowLongPtrW(window_,GWL_EXSTYLE,GetWindowLongPtrW(window_,GWL_EXSTYLE)|WS_EX_NOACTIVATE);
    transition(IslandState::Compact);feedback(Action::None);
    HWND back=commandReturn_;commandReturn_=nullptr;if(restoreFocus&&back&&IsWindow(back))SetForegroundWindow(back);
}
void IslandWindow::commandQuery(){if(commands_)commands_->query(content_.command.text,workspaces_.names());}
void IslandWindow::commandResults(){
    if(!commands_||!content_.command.active)return;std::vector<CommandResult> results;std::vector<std::shared_ptr<const Artwork>> icons;
    auto seq=commands_->results(results,icons);if(seq<commandSeq_)return;commandSeq_=seq;
    auto& c=content_.command;bool same=results.size()==c.results.size();for(size_t i=0;same&&i<results.size();++i)same=results[i].title==c.results[i].title;
    c.results=std::move(results);c.icons=std::move(icons);if(!same){c.selected=0;c.armed=false;}c.selected=std::clamp(c.selected,0,std::max(0,int(std::min<size_t>(3,c.results.size()))-1));
    // The bar grows and shrinks with its results, on the body spring.
    const int rows=c.results.empty()?3:int(std::min<size_t>(3,c.results.size()));const double height=commandIslandHeight(rows);if(std::abs(motion_.commandHeight-height)>.5){motion_.commandHeight=height;animate();}
    refresh();commandSelect(c.selected);
}
void IslandWindow::commandSelect(int index){
    auto& c=content_.command;int rows=int(std::min<size_t>(3,c.results.size()));if(rows==0){content_.hovered=Action::None;feedback(Action::None);return;}
    c.selected=std::clamp(index,0,rows-1);content_.hovered=Action(int(Action::CommandResultBase)+c.selected);feedback(content_.hovered);
}
// A short horizontal shake: nothing to run.
void IslandWindow::commandShake(){if(motion_.reduced)return;double now=seconds();motion_.dragX.reset(0,now,-420);motion_.dragX.retarget(0,now,{1,900,16});animate();}
void IslandWindow::commandChar(wchar_t ch){
    auto& c=content_.command;if(!c.active||ch<0x20||ch==0x7f||c.text.size()>=160)return;
    c.text.insert(c.caret,1,ch);++c.caret;c.armed=false;c.status.clear();commandQuery();refresh();
}
// Returns true when the key was used by the command bar.
bool IslandWindow::commandKey(WPARAM key){
    auto& c=content_.command;if(!c.active)return false;const bool ctrl=GetKeyState(VK_CONTROL)&0x8000;
    auto wordLeft=[&](size_t at){while(at>0&&c.text[at-1]==L' ')--at;while(at>0&&c.text[at-1]!=L' ')--at;return at;};
    auto wordRight=[&](size_t at){while(at<c.text.size()&&c.text[at]==L' ')++at;while(at<c.text.size()&&c.text[at]!=L' ')++at;return at;};
    bool edited=false;
    switch(key){
    case VK_ESCAPE:closeCommand();return true;
    case VK_RETURN:runCommand(size_t(c.selected));return true;
    case VK_UP:commandSelect(c.selected-1);return true;
    case VK_DOWN:commandSelect(c.selected+1);return true;
    case VK_LEFT:c.caret=ctrl?wordLeft(c.caret):(c.caret?c.caret-1:0);break;
    case VK_RIGHT:c.caret=ctrl?wordRight(c.caret):std::min(c.text.size(),c.caret+1);break;
    case VK_HOME:c.caret=0;break;
    case VK_END:c.caret=c.text.size();break;
    case VK_BACK:if(c.caret){size_t from=ctrl?wordLeft(c.caret):c.caret-1;c.text.erase(from,c.caret-from);c.caret=from;edited=true;}break;
    case VK_DELETE:if(c.caret<c.text.size()){size_t to=ctrl?wordRight(c.caret):c.caret+1;c.text.erase(c.caret,to-c.caret);edited=true;}break;
    case 'V':if(!ctrl)return false;{std::wstring pasted;if(OpenClipboard(window_)){if(HANDLE h=GetClipboardData(CF_UNICODETEXT))if(auto* t=static_cast<const wchar_t*>(GlobalLock(h))){pasted=t;GlobalUnlock(h);}CloseClipboard();}
        auto end=pasted.find_first_of(L"\r\n");if(end!=std::wstring::npos)pasted.resize(end);for(auto& ch:pasted)if(ch==L'\t')ch=L' ';pasted=pasted.substr(0,160-std::min<size_t>(160,c.text.size()));
        c.text.insert(c.caret,pasted);c.caret+=pasted.size();edited=true;}break;
    case 'A':if(!ctrl)return false;c.caret=c.text.size();break;
    default:return false;
    }
    if(edited){c.armed=false;c.status.clear();commandQuery();}
    refresh();return true;
}
void IslandWindow::commandStatus(std::wstring text,bool error,bool close){
    auto& c=content_.command;c.status=std::move(text);c.error=error;c.armed=false;refresh();
    if(error)commandShake();if(close)SetTimer(window_,CommandCloseTimer,error?2200:1300,nullptr);
}
void IslandWindow::saveWorkspaces(){
    if(testing_)return;auto temp=store_.directory/L"workspaces.nexus.tmp";{std::ofstream out(temp,std::ios::binary);workspaces_.write(out);}
    MoveFileExW(temp.c_str(),(store_.directory/L"workspaces.nexus").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
void IslandWindow::runCommand(size_t index){
    auto& c=content_.command;if(index>=c.results.size()){commandShake();return;}const CommandResult r=c.results[index];const double now=seconds();
    auto shell=[&](const std::wstring& target){return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;};
    auto done=[&]{store_.log("Info","command_run");closeCommand(r.kind!=CommandKind::OpenApp&&r.kind!=CommandKind::SearchFiles&&r.kind!=CommandKind::OpenSettings);};
    switch(r.kind){
    case CommandKind::None:commandShake();return;
    case CommandKind::Volume:if(audio_){audio_->setVolume(r.value);if(audio_->muted)audio_->toggleMute();}done();return;
    case CommandKind::VolumeStep:if(audio_)audio_->setVolume(audio_->value+r.value);done();return;
    case CommandKind::Mute:if(audio_&&!audio_->muted)audio_->toggleMute();done();return;
    case CommandKind::Unmute:if(audio_&&audio_->muted)audio_->toggleMute();done();return;
    case CommandKind::Play:case CommandKind::Pause:{bool want=r.kind==CommandKind::Play;if(media_&&content_.playback.canToggle&&content_.playback.playing!=want)media_->control(1,content_.playback.source,content_.playback.id);done();return;}
    case CommandKind::Next:if(media_&&content_.playback.canNext)media_->control(3,content_.playback.source,content_.playback.id);done();return;
    case CommandKind::Previous:if(media_&&content_.playback.canPrevious)media_->control(2,content_.playback.source,content_.playback.id);done();return;
    case CommandKind::Timer:content_.focus.select(r.target==L"break"?FocusClock::Mode::Break:FocusClock::Mode::Focus,now);content_.focus.duration=r.value;content_.focus.toggle(now);clockTimer();done();return;
    case CommandKind::Stopwatch:content_.focus.select(FocusClock::Mode::Stopwatch,now);content_.focus.toggle(now);clockTimer();done();return;
    case CommandKind::StopTimer:content_.focus.reset(now);clockTimer();done();return;
    case CommandKind::OpenApp:if(shell(L"shell:AppsFolder\\"+r.target))done();else commandStatus(L"Windows could not open that app",true);return;
    case CommandKind::SearchFiles:case CommandKind::OpenSettings:if(shell(r.target))done();else commandStatus(L"Windows could not open that",true);return;
    case CommandKind::Lock:closeCommand(false);LockWorkStation();return;
    case CommandKind::Clipboard:closeCommand(false);content_.page=Page::Shelf;content_.shelfTab=1;clipViews();content_.pinned=true;transition(IslandState::Expanded);refresh();return;
    case CommandKind::ClearClipboard:clearClips();commandStatus(L"Clipboard history cleared");return;
    case CommandKind::DeleteWorkspace:if(workspaces_.remove(r.target)){saveWorkspaces();commandStatus(L"Removed “"+r.target+L"”");}else commandStatus(L"That workspace no longer exists",true);return;
    case CommandKind::SaveWorkspace:case CommandKind::Workspace:{
        const Workspace* saved=r.kind==CommandKind::Workspace?workspaces_.find(r.target):nullptr;
        if(r.kind==CommandKind::Workspace&&!saved){commandStatus(L"That workspace no longer exists",true);return;}
        // Opening several apps asks for a second Enter, after listing them.
        if(r.kind==CommandKind::Workspace&&!c.armed){c.armed=true;c.status=L"Press Enter again to open "+appSummary(saved->apps);c.error=false;refresh();return;}
        commandStatus(r.kind==CommandKind::SaveWorkspace?L"Saving the apps you have open…":L"Opening "+appSummary(saved->apps)+L"…",false,false);
        auto job=new WorkspaceDone;job->save=r.kind==CommandKind::SaveWorkspace;job->name=r.target;Workspace copy=saved?*saved:Workspace{};HWND self=window_;
        std::thread([job,copy,self]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(job->save)job->apps=openApps(self);else job->report=openWorkspace(copy,self);CoUninitialize();
            if(!PostMessageW(self,WorkspaceMessage,0,reinterpret_cast<LPARAM>(job)))delete job;}).detach();
        return;}
    }
}
// Handles the Phase 4 window messages; returns true when one was handled.
bool IslandWindow::productivityMessage(UINT m,WPARAM w,LPARAM l,LRESULT& result){
    switch(m){
    case WM_CLIPBOARDUPDATE:onClipboard();result=0;return true;
    case PrivacyMessage:updatePrivacy();result=0;return true;
    case CommandMessage:commandResults();result=0;return true;
    case WM_HOTKEY:if(int(w)==HotkeyId){if(content_.command.active)closeCommand();else openCommand();}result=0;return true;
    case WM_CHAR:if(content_.command.active){commandChar(wchar_t(w));result=0;return true;}return false;
    // Clicking elsewhere closes the command bar (captures run without focus, so not in tests).
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE&&content_.command.active&&!testing_){closeCommand(false);}return false;
    case WM_TIMER:
        if(w==ClipboardRetryTimer){KillTimer(window_,ClipboardRetryTimer);onClipboard();result=0;return true;}
        if(w==CommandCloseTimer){KillTimer(window_,CommandCloseTimer);closeCommand();result=0;return true;}
        return false;
    case WorkspaceMessage:{std::unique_ptr<WorkspaceDone> job(reinterpret_cast<WorkspaceDone*>(l));result=0;
        if(job->save){if(job->apps.empty()){commandStatus(L"No open apps to save",true);return true;}
            if(!workspaces_.save({job->name,job->apps})){commandStatus(L"Up to 8 workspaces can be saved; remove one first",true);return true;}
            saveWorkspaces();commandStatus(L"Saved "+appSummary(job->apps)+L" as “"+job->name+L"”");store_.log("Info","workspace_saved");}
        else{auto& rep=job->report;std::wstring text=rep.opened?L"Opened "+std::to_wstring(rep.opened)+(rep.opened==1?L" app":L" apps"):L"Nothing to open";
            if(rep.running)text+=L"  ·  "+std::to_wstring(rep.running)+L" already running";if(rep.failed)text+=L"  ·  "+std::to_wstring(rep.failed)+L" not found";
            commandStatus(text,rep.failed>0&&rep.opened==0);store_.log("Info","workspace_opened");}
        return true;}
    default:return false;
    }
}
}
