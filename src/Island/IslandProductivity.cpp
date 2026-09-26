#include "Island/IslandWindow.h"
#include "App/Version.h"
#include <shlobj.h>
#include <powrprof.h>
#include <ctime>
#include <fstream>
#include <thread>
namespace nexus {
namespace {
constexpr int HotkeyId=0x4e49;
constexpr UINT_PTR ClipboardRetryTimer=31,CommandCloseTimer=32;
constexpr UINT WorkspaceMessage=WM_APP+32,CommandJobMessage=WM_APP+36;
// A system action finished on a worker: radios, the recycle bin or the theme.
struct CommandJob {CommandKind kind=CommandKind::None;int which=0,value=0,result=0;};
int64_t unixTime(){return int64_t(std::time(nullptr));}
// Restart and shut down need the shutdown privilege, which every signed-in user holds but must switch on.
bool shutdownPrivilege(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&token))return false;TOKEN_PRIVILEGES p{};p.PrivilegeCount=1;p.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    bool ok=LookupPrivilegeValueW(nullptr,SE_SHUTDOWN_NAME,&p.Privileges[0].Luid)&&AdjustTokenPrivileges(token,FALSE,&p,0,nullptr,nullptr)&&GetLastError()==ERROR_SUCCESS;CloseHandle(token);return ok;}
const wchar_t* confirmText(CommandKind k){switch(k){case CommandKind::EmptyBin:return L"Press Enter again to delete them for good";case CommandKind::Sleep:return L"Press Enter again to put the PC to sleep";
    case CommandKind::Restart:return L"Press Enter again to restart  \u00b7  save your work first";case CommandKind::ShutDown:return L"Press Enter again to shut down  \u00b7  save your work first";case CommandKind::Lock:return L"Press Enter again to lock the PC";default:return L"Press Enter again to go ahead";}}
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
    if(settings_.clipboardHistory&&!testing_){clipboard_.start(window_);if(!pinsLoaded_){pinsLoaded_=true;loadPinnedClips();}
        // Remembering across restarts: read the saved history back once; turned off, the saved copy is removed.
        if(settings_.clipboardKeep)loadClipHistory();else if(clipHistoryReady_){clipHistoryReady_=false;clipSignature_=0;saveClipHistory(true);}}
    // Turning history off forgets everything, pinned copies included.
    else{clipboard_.stop();pinsLoaded_=false;if(!clips_.entries().empty()||!content_.clips.empty()){clips_.forget();clipViews();}savePinnedClips();clipHistoryReady_=false;clipSignature_=0;saveClipHistory(true);}
    if(settings_.pinnedShelf!=pinnedShelfWas_){pinnedShelfWas_=settings_.pinnedShelf;if(settings_.pinnedShelf&&content_.shelf.empty())loadShelfFile();shelfChanged();}
    syncCaptureHotkeys();
    const bool privacy=(settings_.privacyDots||settings_.privacyCards)&&!testing_;
    if(privacy&&!privacy_)privacy_=std::make_unique<PrivacyProvider>(window_);
    else if(!privacy&&privacy_){privacy_.reset();privacyUses_.clear();content_.privacy.clear();}
    if(!settings_.privacyDots)content_.privacy.clear();else content_.privacy=privacyUses_;
    ensureCommands();
    // Command history follows its setting; turning it off forgets it (the file too, never in a test run: a test
    // toggling the setting must not touch the file of the person running it).
    if(settings_.commandHistory){if(!commandMemoryLoaded_)loadCommandMemory();}
    else if(commandMemoryLoaded_||!commandMemory_.items().empty()){commandMemory_.forget();commandMemoryLoaded_=false;if(!testing_){std::error_code ignored;std::filesystem::remove(store_.directory/L"commands.nexus",ignored);}}
    syncHotkey();syncWeather();syncUpdates();
    // Site icons run only while that setting (and rich rows) is on; turning it off forgets them.
    const bool icons=settings_.siteIcons&&settings_.richClips&&!testing_;if(icons&&!siteIcons_)siteIcons_=std::make_unique<SiteIcons>(window_);else if(!icons&&siteIcons_){siteIcons_.reset();clipViews();}
}
// 0.18: updates run while Update automatically is on (never in a test run, unless --qa-update asks, with --qa-version=
// the version to pretend to be). A manual check starts it once even when the setting is off.
void IslandWindow::syncUpdates(bool checkNow){
    qaUpdate_=testing_&&launchArgsQa_;const bool want=checkNow||(!testing_&&settings_.autoUpdate)||qaUpdate_;
    if(want&&!update_){AppVersion v=parseVersion(toUtf8(qaVersion_.empty()?std::wstring(appVersion):qaVersion_));if(!v.valid)return;
        if(!testing_||qaUpdate_)UpdateService::cleanUp();update_=std::make_unique<UpdateService>(window_,store_.directory/L"update",v,checkNow?0.:qaUpdate_?3.:90.);}
    else if(!want&&update_&&!checkNow){update_.reset();KillTimer(window_,UpdateTimer);pushSettingsContext();}
    else if(checkNow&&update_)update_->checkNow();
    // Started by an update: say so, once the island has settled.
    if(!updatedFrom_.empty()&&!updateShown_){updateShown_=true;SetTimer(window_,UpdatedTimer,2500,nullptr);}
}
// Quiet: resting compact, nobody at the island, nothing announcing, no music of the island's own, nothing being
// shared, Settings closed, and no input for 20 seconds.
bool IslandWindow::quietForUpdate(){
    if(state_!=IslandState::Compact||content_.pinned||interaction_!=InteractionState::Rest||events_.active()||!heldCards_.empty()||content_.command.active)return false;
    if(player_&&player_->playing())return false;if(settingsWindow_&&settingsWindow_->open())return false;
    if(!content_.transfers.empty())return false;
    LASTINPUTINFO last{sizeof(last)};if(!qaUpdate_&&GetLastInputInfo(&last)&&GetTickCount()-last.dwTime<20000)return false;
    return true;
}
void IslandWindow::installUpdate(){
    if(!update_||update_->state()!=UpdateService::State::Ready){KillTimer(window_,UpdateTimer);return;}if(!quietForUpdate())return;
    KillTimer(window_,UpdateTimer);store_.log("Info","update_installing");
    // Everything is saved first; the new version waits for this one to close.
    if(!testing_)store_.save(settings_,settingsFile_);
    if(update_->install(launchArgs_)){store_.log("Info","update_started");PostMessageW(window_,WM_CLOSE,0,0);}else pushSettingsContext();
}
void IslandWindow::showUpdated(){
    if(!renderer_)return;content_.notice={};content_.notice.kind=18;content_.notice.app=std::wstring(L"Updated to ")+appVersion;content_.notice.detail=L"From "+updatedFrom_+L". What\u2019s new is on GitHub";
    {const Activity a{ActivityKind::Notification,"update",55,18,2.4,6};if(deferCard(a)||holdCard(a))return;events_.publish(a,seconds());}
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","update_card_shown");
}
// Weather runs only while it is on; turning it off forgets the place too.
// A test run uses it only with --qa-weather-live, and then keeps its place in a file of its own in the temp folder.
void IslandWindow::syncWeather(){
    const bool want=settings_.weather&&(!testing_||qaWeatherLive_);const auto file=testing_?std::filesystem::temp_directory_path()/L"arnav-island-qa-weather.nexus":store_.directory/L"weather.nexus";
    if(want&&!weather_)weather_=std::make_unique<WeatherService>(window_,file);
    else if(!want&&weather_){weather_.reset();content_.weather={};std::error_code ignored;std::filesystem::remove(file,ignored);if(renderer_)refresh();}
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
    // Some apps write the clipboard twice for one copy; the second update is the same copy.
    const bool repeat=!clips_.entries().empty()&&clips_.entries().front().sameContent(e)&&seconds()-clips_.entries().front().time<1.5;
    auto label=copiedLabel(e);if(!clips_.add(std::move(e),seconds()))return;if(repeat){clipViews();return;}
    content_.clipOffset=0;clipViews();store_.log("Info","clipboard_kept");
    if(settings_.clipboardConfirm&&!(settings_.autoHide&&autoHide_.hidden&&!settings_.alertsReveal)){copyLabel_=label;events_.publish({ActivityKind::Clipboard,"clipboard",25,0,.6,2.2},seconds());presentActivity();}
    else refresh();
}
void IslandWindow::clipViews(){
    const double now=seconds();content_.clips.clear();content_.clipsPaused=clips_.paused;
    // Pinned copies first, then the newest.
    std::vector<const ClipEntry*> order;for(auto& e:clips_.entries())if(e.pinned)order.push_back(&e);for(auto& e:clips_.entries())if(!e.pinned)order.push_back(&e);
    for(auto* p:order){const auto& e=*p;ContentSnapshot::Clip c;c.id=e.id;c.kind=int(e.kind);c.icon=e.sourceIcon;c.thumbnail=e.thumbnail;c.pinned=e.pinned;c.secret=e.kind==ClipEntry::Kind::Text&&looksSecret(e.text);
        switch(e.kind){
        case ClipEntry::Kind::Image:c.preview=L"Image  ·  "+std::to_wstring(e.imageWidth)+L" × "+std::to_wstring(e.imageHeight);break;
        case ClipEntry::Kind::Files:{auto& f=e.files.front();auto slash=f.find_last_of(L"\\/");c.preview=slash==std::wstring::npos?f:f.substr(slash+1);if(e.files.size()>1)c.preview+=L" and "+std::to_wstring(e.files.size()-1)+L" more";break;}
        default:c.preview=clipPreview(e.text,90);
            // Rich rows: a colour code, code, or a link's site.
            if(settings_.richClips&&e.kind==ClipEntry::Kind::Text&&!c.secret){if(e.text.size()<=40)if(auto colour=parseColourCode(e.text)){c.hasColour=true;c.colour=*colour;c.preview=colourHex(*colour)+L"  \u00b7  "+colourDetail(*colour).substr(0,colourDetail(*colour).find(L"  "));}
                if(!c.hasColour&&looksLikeCode(e.text)){c.code=true;c.preview=firstLine(e.text).substr(0,160);}}
            if(settings_.richClips&&e.kind==ClipEntry::Kind::Link){c.host=linkHost(e.text);c.path=linkPath(e.text);if(settings_.siteIcons&&siteIcons_&&!c.host.empty())c.favicon=siteIcons_->get(c.host);}}
        c.meta=(e.source.empty()?std::wstring(L"Copied"):e.source)+L"  ·  "+ageText(now-e.time);content_.clips.push_back(std::move(c));}
    content_.clipOffset=std::clamp(content_.clipOffset,0,std::max(0,int(content_.clips.size())-4));
    clipsChanged();
}
void IslandWindow::clearClips(){clips_.clear();clipViews();store_.log("Info","clipboard_cleared");refresh();}
// `index` is a row of the Shelf list (pinned first), matched to its copy by id.
void IslandWindow::copyClip(size_t index){
    if(index>=content_.clips.size())return;const ClipEntry* found=clips_.find(content_.clips[index].id);if(!found)return;ClipEntry e=*found;
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
    if(!renderer_)return;
    if(settings_.autoHide&&autoHide_.hidden&&!settings_.alertsReveal)return;
    content_.notice={u.capability==Capability::Camera?5:u.capability==Capability::Microphone?6:u.capability==Capability::ScreenCapture?12:7,{},u.app,u.icon};
    {const Activity a{ActivityKind::Notification,"privacy",70,double(content_.notice.kind),2.4,3.4};if(deferCard(a)||holdCard(a))return;events_.publish(a,seconds());}
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","privacy_card_shown");
}
// ---- Command bar -----------------------------------------------------------
// The command bar's service (apps, files, answers on its own thread). Test runs search only the Public folder, where the
// sample files live, never the user's own files.
void IslandWindow::ensureCommands(){if(!commands_){wchar_t pub[MAX_PATH]{};GetEnvironmentVariableW(L"PUBLIC",pub,MAX_PATH);commands_=std::make_unique<CommandService>(window_,testing_&&*pub?std::wstring(pub):userFolder(),store_.directory);}}
void IslandWindow::openCommand(){
    if(content_.command.active){SetForegroundWindow(window_);return;}ensureCommands();
    commandReturn_=GetForegroundWindow();if(commandReturn_==window_)commandReturn_=nullptr;
    content_.command=ContentSnapshot::Command{};content_.command.active=true;content_.notice.kind=0;content_.pinned=false;KillTimer(window_,CommandCloseTimer);
    // The island takes keyboard focus only while the command bar is open.
    SetWindowLongPtrW(window_,GWL_EXSTYLE,GetWindowLongPtrW(window_,GWL_EXSTYLE)&~WS_EX_NOACTIVATE);
    motion_.commandHeight=commandIslandHeight(3);transition(IslandState::Command);commandQuery(true);
    SetForegroundWindow(window_);SetFocus(window_);store_.log("Info","command_bar_opened");
}
void IslandWindow::closeCommand(bool restoreFocus){
    if(!content_.command.active)return;KillTimer(window_,CommandCloseTimer);content_.command=ContentSnapshot::Command{};content_.hovered=Action::None;
    SetWindowLongPtrW(window_,GWL_EXSTYLE,GetWindowLongPtrW(window_,GWL_EXSTYLE)|WS_EX_NOACTIVATE);
    transition(IslandState::Compact);feedback(Action::None);
    HWND back=commandReturn_;commandReturn_=nullptr;if(restoreFocus&&back&&IsWindow(back))SetForegroundWindow(back);
}
void IslandWindow::commandQuery(bool refreshState){if(content_.command.clips){clipResults();return;}if(commands_)commands_->query(content_.command.text,workspaces_.names(),commandContext(),commandMemory_.items(),settings_.currency,refreshState);}
// What the island is doing, so an empty bar can suggest the obvious next step.
CommandContext IslandWindow::commandContext(){
    CommandContext c;const auto& p=content_.playback;c.media=p.available&&p.canToggle;c.playing=c.media&&p.playing;c.track=p.title.empty()?L"":p.title+(p.artist.empty()?L"":L"  \u00b7  "+p.artist);
    c.muted=audio_&&audio_->muted.load();c.micMuted=audio_&&audio_->micAvailable.load()&&audio_->micMuted.load();c.timer=content_.focus.running;SYSTEMTIME t{};GetLocalTime(&t);c.hour=t.wHour;return c;
}
void IslandWindow::loadCommandMemory(){
    commandMemoryLoaded_=true;if(testing_)return;std::ifstream in(store_.directory/L"commands.nexus",std::ios::binary);if(in)commandMemory_=CommandMemory::read(in);
}
void IslandWindow::saveCommandMemory(){
    if(testing_||!settings_.commandHistory)return;auto temp=store_.directory/L"commands.nexus.tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);commandMemory_.write(out);}
    MoveFileExW(temp.c_str(),(store_.directory/L"commands.nexus").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
void IslandWindow::rememberCommand(const CommandResult& r){if(!settings_.commandHistory)return;commandMemory_.record(r,content_.command.text.empty()?r.phrase:content_.command.text,unixTime());saveCommandMemory();}
// Ctrl+Enter on a file: its folder opens with the file selected.
void IslandWindow::revealResult(size_t index){
    auto& c=content_.command;if(index>=c.results.size()||c.results[index].kind!=CommandKind::OpenFile){commandShake();return;}const auto r=c.results[index];
    if(auto* pidl=ILCreateFromPathW(r.target.c_str())){const HRESULT hr=SHOpenFolderAndSelectItems(pidl,0,nullptr,0);ILFree(pidl);if(SUCCEEDED(hr)){rememberCommand(r);store_.log("Info","command_reveal");closeCommand(false);return;}}
    commandStatus(L"Windows could not show that file",true);
}
void IslandWindow::commandResults(){
    if(!commands_||!content_.command.active||content_.command.clips)return;std::vector<CommandResult> results;std::vector<std::shared_ptr<const Artwork>> icons;
    auto seq=commands_->results(results,icons);if(seq<commandSeq_)return;commandSeq_=seq;
    // Phase 5G: "play" and a song finds it in your Music folder; "shuffle" plays it all; "continue on" offers the music to a paired PC.
    {const std::wstring typed=lowered(trimmed(content_.command.text));std::vector<CommandResult> extra;
        if(settings_.musicLibrary&&library_){
            if(typed.starts_with(L"play ")&&typed.size()>5){library_->scan();if(auto tracks=library_->tracks())for(size_t i:searchLibrary(*tracks,typed.substr(5),4)){const auto& t=(*tracks)[i];CommandResult r;r.kind=CommandKind::PlaySong;r.title=L"Play "+t.title;r.detail=(t.artist.empty()?std::wstring():t.artist+L"  \u00b7  ")+L"From your Music folder";r.target=t.path;extra.push_back(std::move(r));}}
            if(typed==L"shuffle"||typed==L"shuffle music"||typed==L"shuffle my music"||typed==L"play my music"||typed==L"play music"){CommandResult r;r.kind=CommandKind::ShuffleMusic;r.title=L"Shuffle my music";r.detail=L"Songs from your Music folder, played by the island";extra.push_back(std::move(r));}}
        if(settings_.sharing&&settings_.handoff&&content_.playback.available&&(typed==L"handoff"||typed.starts_with(L"continue on")||typed.starts_with(L"play on"))){
            const std::wstring who=typed.starts_with(L"continue on")?trimmed(typed.substr(11)):typed.starts_with(L"play on")?trimmed(typed.substr(7)):L"";
            for(auto& p:content_.nearby)if(p.paired&&p.online&&p.version>=shareProtocol&&(who.empty()||lowered(p.name).find(who)!=std::wstring::npos)){CommandResult r;r.kind=CommandKind::ContinueOn;r.title=L"Continue on "+p.name;r.detail=content_.playback.title+L"  \u00b7  plays there from where it is";r.target=std::wstring(p.id.begin(),p.id.end());extra.push_back(std::move(r));}}
        if(!extra.empty()){icons.insert(icons.begin(),extra.size(),nullptr);results.insert(results.begin(),std::make_move_iterator(extra.begin()),std::make_move_iterator(extra.end()));}}
    auto& c=content_.command;bool same=results.size()==c.results.size();for(size_t i=0;same&&i<results.size();++i)same=results[i].title==c.results[i].title;
    c.results=std::move(results);c.icons=std::move(icons);if(!same){c.selected=0;if(c.armed){c.armed=false;c.status.clear();}}c.selected=std::clamp(c.selected,0,std::max(0,int(std::min<size_t>(5,c.results.size()))-1));
    // The bar grows and shrinks with its results, on the body spring.
    const double height=c.results.empty()?commandIslandHeight(3):commandIslandHeightAt(commandRows(c.results,5).footer);if(std::abs(motion_.commandHeight-height)>.5){motion_.commandHeight=height;animate();}
    refresh();commandSelect(c.selected);
}
void IslandWindow::commandSelect(int index){
    auto& c=content_.command;int rows=int(std::min<size_t>(5,c.results.size()));if(rows==0){content_.hovered=Action::None;feedback(Action::None);return;}
    const int next=std::clamp(index,0,rows-1);if(next!=c.selected&&c.armed){c.armed=false;c.status.clear();refresh();}c.selected=next;content_.hovered=Action(int(Action::CommandResultBase)+c.selected);feedback(content_.hovered);
}
// A short horizontal shake: nothing to run.
void IslandWindow::commandShake(){if(motion_.reduced)return;double now=seconds();motion_.dragX.reset(0,now,-420);motion_.dragX.retarget(0,now,{1,900,16});animate();}
void IslandWindow::commandChar(wchar_t ch){
    auto& c=content_.command;if(!c.active||ch<0x20||ch==0x7f||c.text.size()>=160)return;
    c.text.insert(c.caret,1,ch);++c.caret;c.armed=false;c.status.clear();
    // "clip " switches to searching the clipboard history.
    if(!c.clips&&c.text==L"clip "){c.clips=true;c.paste=true;c.text.clear();c.caret=0;c.selected=0;}
    commandQuery();refresh();
}
// Returns true when the key was used by the command bar.
bool IslandWindow::commandKey(WPARAM key){
    auto& c=content_.command;if(!c.active)return false;const bool ctrl=GetKeyState(VK_CONTROL)&0x8000;
    auto wordLeft=[&](size_t at){while(at>0&&c.text[at-1]==L' ')--at;while(at>0&&c.text[at-1]!=L' ')--at;return at;};
    auto wordRight=[&](size_t at){while(at<c.text.size()&&c.text[at]==L' ')++at;while(at<c.text.size()&&c.text[at]!=L' ')++at;return at;};
    bool edited=false;
    switch(key){
    case VK_ESCAPE:closeCommand();return true;
    // Space is typed (it arrives as WM_CHAR); the island's own "Space activates the highlight" must not run a result.
    case VK_SPACE:return true;
    case VK_RETURN:if(ctrl&&!c.clips){revealResult(size_t(c.selected));return true;}runCommand(size_t(c.selected));return true;
    // Tab takes the ghost completion (the rest of an app, command or file name).
    case VK_TAB:{if(c.clips||c.results.empty())return true;const auto ghost=ghostSuffix(c.text,c.results[0].completion);if(ghost.empty()||c.caret!=c.text.size()){commandShake();return true;}
        c.text=c.results[0].completion.substr(0,160);c.caret=c.text.size();edited=true;break;}
    case 'C':{if(!ctrl)return false;if(size_t(c.selected)<c.results.size()){const auto& r=c.results[size_t(c.selected)];
        if(r.kind==CommandKind::OpenFile){copyText(r.target);commandStatus(L"Path copied",false,false);}else if(r.kind==CommandKind::Currency){copyText(r.target);commandStatus(L"Copied "+r.answer,false,false);}}return true;}
    case 'P':{if(!ctrl||c.clips)return ctrl;if(size_t(c.selected)>=c.results.size())return true;const auto r=c.results[size_t(c.selected)];
        if(!settings_.commandHistory){commandStatus(L"Turn on \u201cRemember recent commands\u201d in Settings to pin",true,false);return true;}
        const int state=commandMemory_.togglePin(r,c.text.empty()?r.phrase:c.text,unixTime());
        if(state<0){commandStatus(CommandMemory::memorable(r.kind)?L"Up to six commands can be pinned":L"This row can\u2019t be pinned",true,false);return true;}
        saveCommandMemory();commandStatus(state?L"Pinned to the empty bar":L"Unpinned",false,false);if(c.text.empty())commandQuery();return true;}
    case VK_UP:commandSelect(c.selected-1);return true;
    case VK_DOWN:commandSelect(c.selected+1);return true;
    case VK_LEFT:c.caret=ctrl?wordLeft(c.caret):(c.caret?c.caret-1:0);break;
    case VK_RIGHT:c.caret=ctrl?wordRight(c.caret):std::min(c.text.size(),c.caret+1);break;
    case VK_HOME:c.caret=0;break;
    case VK_END:c.caret=c.text.size();break;
    case VK_BACK:if(c.clips&&c.text.empty()){c.clips=false;c.paste=false;c.selected=0;commandQuery();refresh();return true;}if(c.caret){size_t from=ctrl?wordLeft(c.caret):c.caret-1;c.text.erase(from,c.caret-from);c.caret=from;edited=true;}break;
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
    auto done=[&]{store_.log("Info","command_run");closeCommand(r.kind!=CommandKind::OpenApp&&r.kind!=CommandKind::SearchFiles&&r.kind!=CommandKind::OpenSettings&&r.kind!=CommandKind::OpenFile);};
    // Anything hard to undo asks for a second Enter first (workspaces word their own).
    if(r.confirm&&r.kind!=CommandKind::Workspace&&!c.armed){c.armed=true;c.error=false;c.status=confirmText(r.kind);refresh();return;}
    if(r.kind!=CommandKind::None)rememberCommand(r);
    auto background=[&](int which,int value,const std::wstring& working){commandStatus(working,false,false);auto job=new CommandJob{r.kind,which,value,0};HWND self=window_;
        std::thread([job,self]{if(job->kind==CommandKind::EmptyBin)job->result=SUCCEEDED(SHEmptyRecycleBinW(nullptr,nullptr,SHERB_NOCONFIRMATION|SHERB_NOPROGRESSUI|SHERB_NOSOUND))?1:-1;
            else if(job->kind==CommandKind::DarkMode)job->result=setDarkMode(job->value==1)?1:-1;
            else{if(job->value<0){const int now=radioOn(job->which);job->value=now==1?0:1;}job->result=setRadios(job->which,job->value==1);}
            if(!PostMessageW(self,CommandJobMessage,0,reinterpret_cast<LPARAM>(job)))delete job;}).detach();};
    switch(r.kind){
    case CommandKind::DarkMode:background(0,r.value,r.value?L"Switching to dark mode\u2026":L"Switching to light mode\u2026");return;
    case CommandKind::Bluetooth:background(2,r.value,L"Changing Bluetooth\u2026");return;
    case CommandKind::WiFi:background(1,r.value,L"Changing Wi-Fi\u2026");return;
    case CommandKind::Airplane:background(3,r.value?0:1,r.value?L"Turning radios off\u2026":L"Turning radios back on\u2026");return;
    case CommandKind::EmptyBin:background(0,0,L"Emptying the recycle bin\u2026");return;
    case CommandKind::Sleep:closeCommand(false);store_.log("Info","command_sleep");SetSuspendState(FALSE,FALSE,FALSE);return;
    case CommandKind::Restart:case CommandKind::ShutDown:{const bool restart=r.kind==CommandKind::Restart;shutdownPrivilege();
        if(ExitWindowsEx(restart?EWX_REBOOT:(EWX_SHUTDOWN|EWX_POWEROFF|EWX_HYBRID_SHUTDOWN),SHTDN_REASON_MAJOR_OTHER|SHTDN_REASON_MINOR_OTHER|SHTDN_REASON_FLAG_PLANNED)){store_.log("Info",restart?"command_restart":"command_shutdown");closeCommand(false);}
        else commandStatus(restart?L"Windows did not restart":L"Windows did not shut down",true);return;}
    case CommandKind::Currency:copyText(r.target);commandStatus(L"Copied "+r.answer);return;
    case CommandKind::Colour:copyText(r.target);commandStatus(L"Copied "+r.target);return;
    // Choosing a place is the consent to fetch weather for it.
    case CommandKind::Weather:{if(r.target.empty()){openSettings(4);done();return;}if(!settings_.weather){Settings next=settings_;next.weather=true;receiveSettings(next);}if(!weather_){commandStatus(L"Weather is not available in test runs",true);return;}weather_->choose(r.target);weatherAsked_=true;commandStatus(L"Finding "+r.target+L"\u2026",false,false);return;}
    case CommandKind::OpenFile:if(shell(r.target))done();else commandStatus(L"Windows could not open that file",true);return;
    case CommandKind::None:commandShake();return;
    case CommandKind::Volume:if(audio_){audio_->setVolume(r.value);if(audio_->muted)audio_->toggleMute();}done();return;
    case CommandKind::VolumeStep:if(audio_)audio_->setVolume(audio_->value+r.value);done();return;
    case CommandKind::Mute:if(audio_&&!audio_->muted)audio_->toggleMute();done();return;
    case CommandKind::Unmute:if(audio_&&audio_->muted)audio_->toggleMute();done();return;
    case CommandKind::ClipPaste:pasteClip(index,(GetKeyState(VK_SHIFT)&0x8000)!=0);return;
    case CommandKind::Snip:case CommandKind::CopyText:case CommandKind::PickColour:store_.log("Info","command_run");startCapture(r.kind==CommandKind::Snip?CaptureMode::Snip:r.kind==CommandKind::CopyText?CaptureMode::Text:CaptureMode::Colour);return;
    case CommandKind::MicMute:case CommandKind::MicUnmute:case CommandKind::MicToggle:{if(!audio_||!audio_->micAvailable){commandStatus(L"No microphone is connected",true,false);return;}
        const bool muted=audio_->micMuted;if(r.kind==CommandKind::MicToggle||(r.kind==CommandKind::MicMute)!=muted)audio_->toggleMic();done();return;}
    case CommandKind::Play:case CommandKind::Pause:{bool want=r.kind==CommandKind::Play;if(content_.playback.canToggle&&content_.playback.playing!=want)mediaCommand(want?4:5);
        // Nothing to resume: "play" shuffles your music instead.
        else if(want&&!content_.playback.available&&settings_.musicLibrary)shuffleLibrary();done();return;}
    case CommandKind::Next:if(content_.playback.canNext)mediaCommand(3);done();return;
    case CommandKind::Previous:if(content_.playback.canPrevious)mediaCommand(2);done();return;
    // Phase 5G: a song from the library (the list plays on from it), shuffling it, and continuing the music on a PC.
    case CommandKind::PlaySong:{auto tracks=library_?library_->tracks():nullptr;if(tracks)for(size_t i=0;i<tracks->size();++i)if((*tracks)[i].path==r.target){std::vector<size_t> order(tracks->size());for(size_t k=0;k<order.size();++k)order[k]=k;playLibrary(order,i);break;}done();return;}
    case CommandKind::ShuffleMusic:shuffleLibrary();done();return;
    case CommandKind::ContinueOn:{for(size_t i=0;i<content_.nearby.size();++i)if(content_.nearby[i].id==std::string(r.target.begin(),r.target.end())){handoffTo(i);break;}closeCommand(false);return;}
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
void IslandWindow::commandJobDone(LPARAM l){
    std::unique_ptr<CommandJob> job(reinterpret_cast<CommandJob*>(l));if(!content_.command.active)return;
    if(job->result==-2){commandStatus(job->which==2?L"No Bluetooth radio was found":job->which==1?L"No Wi-Fi radio was found":L"No radios were found",true);return;}
    if(job->result<0){commandStatus(job->kind==CommandKind::EmptyBin?L"The recycle bin could not be emptied":job->kind==CommandKind::DarkMode?L"Windows did not change the mode":L"Windows did not allow that change",true);return;}
    const bool on=job->value==1;std::wstring text;
    switch(job->kind){case CommandKind::EmptyBin:text=L"The recycle bin is empty";break;case CommandKind::DarkMode:text=on?L"Dark mode is on":L"Light mode is on";break;
        case CommandKind::Bluetooth:text=on?L"Bluetooth is on":L"Bluetooth is off";break;case CommandKind::WiFi:text=on?L"Wi-Fi is on":L"Wi-Fi is off";break;
        default:text=on?L"Wi-Fi and Bluetooth are back on":L"Wi-Fi and Bluetooth are off";break;}
    commandStatus(text);store_.log("Info","command_system_action");
}
// Handles the Phase 4 window messages; returns true when one was handled.
bool IslandWindow::productivityMessage(UINT m,WPARAM w,LPARAM l,LRESULT& result){
    switch(m){
    case WM_CLIPBOARDUPDATE:onClipboard();result=0;return true;
    case PrivacyMessage:updatePrivacy();result=0;return true;
    // wParam 1: exchange rates arrived, so the open query is asked again.
    case CommandMessage:if(w==1){if(content_.command.active&&!content_.command.clips)commandQuery();}else commandResults();result=0;return true;
    case CommandJobMessage:commandJobDone(l);result=0;return true;
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
