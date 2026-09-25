#include "IslandWindow.h"
#include "Media/CoverCodec.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <filesystem>
// Phase 5G: music the island plays itself, from your Music folder (MusicLibrary, IslandPlayer),
// and music handed between your own PCs: what plays goes to a paired PC, which plays it where it
// can (a player that has the song, the song in its own library, the song's file sent along, or the
// same app opened), from where it was.
namespace nexus {
namespace {
constexpr UINT_PTR HandoffWaitTimer=61,PlayerIdleTimer=62,PlayerFadeTimer=81,AppFadeTimer=82;
std::wstring musicFolder(){PWSTR p=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Music,0,nullptr,&p))&&p)out=p;CoTaskMemFree(p);return out;}
std::wstring lowerName(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
// Browsers can't reopen a tab that played elsewhere.
bool browserApp(const std::wstring& app){const auto a=lowerName(app);for(auto b:{L"chrome",L"msedge",L"firefox",L"opera",L"brave",L"vivaldi",L"arc"})if(a.find(b)!=std::wstring::npos)return true;return false;}
}
// The library exists while its setting is on (a test run reads only the folder it is given with --qa-library).
void IslandWindow::syncLibrary(){
    const bool want=settings_.musicLibrary&&(!testing_||!qaLibrary_.empty());
    if(want&&!library_)library_=std::make_unique<MusicLibrary>(window_,testing_?std::vector<std::wstring>{qaLibrary_}:std::vector<std::wstring>{musicFolder()});
    else if(!want&&library_){library_.reset();content_.library=false;content_.libraryTracks.reset();content_.libraryArt.clear();}
    // Turned off, the island's own song stops too.
    if(!settings_.musicLibrary&&player_&&player_->active()){player_->stop();updateSessions();}
    if(player_){player_->crossfade(settings_.crossfade);playerTick();}
}
bool IslandWindow::ensurePlayer(){if(!player_){player_=std::make_unique<IslandPlayer>(window_);player_->crossfade(settings_.crossfade);}return player_->ready();}
// Phase 5H: the player's volume ramps (crossfades, fades) and its watch for where a crossfade begins run on a timer, only while needed.
void IslandWindow::playerTick(){
    bool changed=false;const int ms=player_?player_->tick(changed):0;if(changed){playerArt();updateSessions();}
    if(ms!=playerTickMs_){playerTickMs_=ms;if(ms)SetTimer(window_,PlayerFadeTimer,UINT(ms),nullptr);else KillTimer(window_,PlayerFadeTimer);}
}
// Music leaving for another PC sinks away before it pauses: the island's own song by its volume, another app's by its
// volume in the mixer (put back once it has paused). An app the mixer doesn't know just pauses.
void IslandWindow::fadeOutForHandoff(){
    const auto& p=content_.playback;if(!p.playing)return;
    if(p.id==islandSessionId&&player_){player_->fadeOut(1.4);playerTick();return;}
    std::vector<MixerName> names;for(auto& e:content_.mixer)names.push_back({e.name,e.active,e.system});const int i=mixer_&&!motion_.reduced?mixerIndexFor(p.appName,names):-1;
    if(i<0||content_.mixer[size_t(i)].muted){mediaCommand(5);return;}
    const auto& e=content_.mixer[size_t(i)];appFade_={e.pid,e.volume,seconds(),1.4,p.source,p.id,false};SetTimer(window_,AppFadeTimer,33,nullptr);
}
void IslandWindow::appFadeTick(){
    auto& f=appFade_;if(!mixer_||!f.pid){KillTimer(window_,AppFadeTimer);f={};return;}const double p=(seconds()-f.began)/f.length;
    if(p<1){mixer_->setVolume(f.pid,float(f.from*std::cos(std::clamp(p,0.,1.)*1.5707963)));return;}
    // Silent: pause that session (if it still plays), then put its volume back a moment later.
    if(!f.paused){f.paused=true;mixer_->setVolume(f.pid,0);if(media_)for(auto& s:content_.sessions)if(s.id==f.session&&s.source==f.source&&s.playing){media_->control(1,s.source,s.id);break;}return;}
    if(p>1.5){mixer_->setVolume(f.pid,f.from);KillTimer(window_,AppFadeTimer);f={};}
}
// ---- Up next ----------------------------------------------------------------------------------
// The songs after the one playing, the covers of the rows on screen, and the next one's (peeking out on the Media page).
void IslandWindow::upNextRows(){
    content_.upNextTracks=player_&&player_->active()?player_->upNext(200):std::vector<LibraryTrack>{};const int n=int(content_.upNextTracks.size());
    content_.upNextOffset=std::clamp(content_.upNextOffset,0,std::max(0,n-5));content_.upNextArt.clear();
    if(library_)for(int i=content_.upNextOffset;i<std::min(n,content_.upNextOffset+5);++i)content_.upNextArt.push_back(library_->artwork(content_.upNextTracks[size_t(i)].path));
    const auto* next=player_&&player_->active()?player_->upcoming():nullptr;content_.nextArt=next&&library_?library_->artwork(next->path):nullptr;
}
bool IslandWindow::upNextAction(Action a){
    const double now=seconds();auto slide=[&](double from){if(!motion_.reduced){motion_.swipe.reset(from,now);motion_.swipe.retarget(0,now,MotionTokens::content);}};
    switch(a){
    case Action::UpNextOpen:if(!player_||!player_->active())return true;content_.upNext=true;content_.library=false;content_.handoffPicking=false;content_.upNextOffset=0;content_.page=Page::Media;
        if(state_!=IslandState::Expanded)transition(IslandState::Expanded);slide(22);upNextRows();refresh();animate();store_.log("Info","up_next_opened");return true;
    case Action::UpNextBack:content_.upNext=false;content_.queueDrag={};slide(-22);refresh();animate();return true;
    case Action::UpNextUp:content_.upNextOffset-=5;upNextRows();refresh();return true;
    case Action::UpNextDown:content_.upNextOffset+=5;upNextRows();refresh();return true;
    default:break;}
    // A row clicked: that song plays now (those before it stay behind in the queue).
    if(inRange(a,Action::UpNextItemBase,Action::UpNextItemEnd)){const size_t i=size_t(content_.upNextOffset+int(a)-int(Action::UpNextItemBase));
        if(player_&&player_->active()&&i<content_.upNextTracks.size()){pauseOthers();player_->jump(player_->index()+1+i);playerArt();updateSessions();playerTick();upNextRows();refresh();store_.log("Info","up_next_jumped");}return true;}
    return false;
}
// A row let go at content y: it moves to the place it was dropped on (among the rows on screen).
void IslandWindow::dropQueueRow(){
    auto& d=content_.queueDrag;if(!d.active){d={};return;}const int n=int(content_.upNextTracks.size()),first=content_.upNextOffset,shown=std::max(0,std::min(5,n-first));
    const int to=std::clamp(int(std::floor((d.y-d.grab-34+17)/34))+first,first,std::max(first,first+shown-1)),from=d.from;d={};
    if(player_&&from!=to&&from>=0&&to>=0&&player_->move(player_->index()+1+size_t(from),player_->index()+1+size_t(to))){playerArt();updateSessions();store_.log("Info","up_next_reordered");}
    upNextRows();refresh();SetTimer(window_,QueueGlideTimer,16,nullptr);
}
// The rows on screen: their songs, their covers (read as they are wanted) and the one playing.
void IslandWindow::libraryRows(){
    content_.libraryTracks=library_?library_->tracks():nullptr;content_.libraryScanning=library_&&library_->scanning();
    const size_t n=content_.libraryTracks?content_.libraryTracks->size():0;content_.libraryOffset=std::clamp(content_.libraryOffset,0,std::max(0,int(n)-5));
    content_.libraryArt.clear();for(size_t i=size_t(content_.libraryOffset);i<std::min(n,size_t(content_.libraryOffset)+5);++i)content_.libraryArt.push_back(library_->artwork((*content_.libraryTracks)[i].path));
    content_.libraryPlaying=player_&&player_->track()?player_->track()->path:L"";
}
// The cover of the island's song, once the library has read it.
void IslandWindow::playerArt(){
    if(!player_||!player_->track()||!library_){content_.nextArt=nullptr;return;}auto art=library_->artwork(player_->track()->path);if(art&&art!=player_->artwork())player_->artwork(art);
    // Island DJ: the next song's cover is read ahead, so the ring can glow in its colours (and it peeks out on the Media page).
    const auto* next=player_->upcoming();content_.nextArt=next?library_->artwork(next->path):nullptr;
    if(content_.upNext)upNextRows();
}
// Anything else playing pauses, so two players never play over each other.
void IslandWindow::pauseOthers(){if(!media_)return;for(auto& s:content_.sessions)if(s.playing&&s.id!=islandSessionId)media_->control(1,s.source,s.id);}
void IslandWindow::playLibrary(const std::vector<size_t>& order,size_t first,double start,bool play){
    auto tracks=library_?library_->tracks():nullptr;if(!tracks||tracks->empty()||order.empty())return;
    if(!ensurePlayer()){commandStatus(L"This PC couldn't start playback",true);return;}
    std::vector<LibraryTrack> queue;queue.reserve(order.size());for(size_t i:order)if(i<tracks->size())queue.push_back((*tracks)[i]);if(queue.empty())return;
    if(play)pauseOthers();player_->play(std::move(queue),std::min(first,order.size()-1),start,play);selectedId_=islandSessionId;content_.upNext=false;
    // Back to what plays, so the song is seen arriving.
    const bool swap=content_.library&&content_.page==Page::Media&&state_!=IslandState::Compact;content_.library=false;
    if(swap&&!motion_.reduced){const double now=seconds();motion_.swipe.reset(-18,now);motion_.swipe.retarget(0,now,MotionTokens::content);}
    playerArt();updateSessions();playerTick();animate();store_.log("Info","island_player_started");
}
void IslandWindow::shuffleLibrary(){
    if(!library_)syncLibrary();if(!library_)return;
    if(!library_->scanned()){library_->scan();shufflePending_=true;return;}
    auto tracks=library_->tracks();if(!tracks||tracks->empty()){shufflePending_=false;if(state_!=IslandState::Compact){content_.library=true;content_.page=Page::Media;libraryRows();refresh();}return;}
    shufflePending_=false;playLibrary(shuffledOrder(tracks->size(),uint32_t(GetTickCount64())),0);
}
bool IslandWindow::libraryAction(Action a){
    const double now=seconds();
    auto slide=[&](double from){if(!motion_.reduced){motion_.swipe.reset(from,now);motion_.swipe.retarget(0,now,MotionTokens::content);}};
    switch(a){
    case Action::LibraryOpen:syncLibrary();if(!library_)return true;library_->scan();content_.library=true;content_.handoffPicking=false;content_.page=Page::Media;if(state_!=IslandState::Expanded)transition(IslandState::Expanded);slide(22);libraryRows();refresh();animate();return true;
    case Action::LibraryBack:content_.library=false;slide(-22);refresh();animate();return true;
    case Action::LibraryShuffle:shuffleLibrary();return true;
    case Action::LibraryUp:content_.libraryOffset-=5;libraryRows();refresh();return true;
    case Action::LibraryDown:content_.libraryOffset+=5;libraryRows();refresh();return true;
    // Continue on: straight to the one paired PC that is here, or a choice of them.
    case Action::HandoffOpen:{std::vector<size_t> ready;for(size_t i=0;i<content_.nearby.size()&&i<3;++i)if(content_.nearby[i].paired&&content_.nearby[i].online&&content_.nearby[i].version>=shareProtocol)ready.push_back(i);
        if(ready.size()==1)handoffTo(ready[0]);else if(!ready.empty()){content_.handoffPicking=!content_.handoffPicking;refresh();}return true;}
    case Action::HandoffPlay:handoffAccept();return true;
    case Action::HandoffDecline:if(share_&&handoffOffer_)share_->answerHandoff(handoffOffer_,0);handoffOffer_=0;events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);return true;
    default:break;}
    if(inRange(a,Action::HandoffPeerBase,Action::HandoffPeerEnd)){handoffTo(size_t(int(a)-int(Action::HandoffPeerBase)));return true;}
    if(inRange(a,Action::LibraryItemBase,Action::LibraryItemEnd)){const size_t i=size_t(content_.libraryOffset+int(a)-int(Action::LibraryItemBase));
        auto tracks=library_?library_->tracks():nullptr;if(!tracks||i>=tracks->size())return true;
        // The list plays on from the song chosen.
        std::vector<size_t> order(tracks->size());for(size_t k=0;k<order.size();++k)order[k]=k;playLibrary(order,i);return true;}
    return false;
}
// The island's own session or a Windows one: 1 play/pause, 2 previous, 3 next, 4 play, 5 pause.
void IslandWindow::mediaCommand(int action){
    const auto& p=content_.playback;
    if(p.id==islandSessionId&&player_){switch(action){case 1:player_->toggle();break;case 2:player_->previous();break;case 3:player_->next();break;case 4:player_->resume();break;case 5:player_->pause();break;default:break;}if(action!=5&&action!=2&&player_->playing())pauseOthers();playerArt();updateSessions();playerTick();return;}
    if(!media_)return;if(action==4||action==5){if(p.playing==(action==4))return;action=1;}media_->control(action,p.source,p.id);
}
void IslandWindow::mediaSeek(double target){const auto& p=content_.playback;if(p.id==islandSessionId&&player_){player_->seek(target);updateSessions();return;}if(media_)media_->seek(target,p);}
bool IslandWindow::musicMessage(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case PlayerMessage:if(player_&&player_->handle(w,l)){playerArt();updateSessions();playerTick();
            // A paused song gives its session up after 20 minutes.
            if(player_->active()&&!player_->playing())SetTimer(window_,PlayerIdleTimer,20*60*1000,nullptr);else KillTimer(window_,PlayerIdleTimer);}return true;
    case LibraryMessage:{if(w==0&&shufflePending_)shuffleLibrary();
        if(w==0&&testing_&&(qaPlay_||qaLibraryView_)){content_.pinned=true;if(qaLibraryView_)libraryAction(Action::LibraryOpen);
            if(qaPlay_){auto tracks=library_->tracks();if(tracks&&!tracks->empty()&&ensurePlayer()){player_->mute(true);std::vector<size_t> order(tracks->size());for(size_t k=0;k<order.size();++k)order[k]=k;playLibrary(order,0);content_.page=Page::Media;transition(IslandState::Expanded);
                // --qa-upnext: Up next opens; --qa-queue-drag: its second row is held partway down, as if being dragged.
                if(qaUpNext_){upNextAction(Action::UpNextOpen);if(qaQueueDrag_){content_.queueDrag={true,1,34+2.6f*34+12,12};SetTimer(window_,QueueGlideTimer,16,nullptr);refresh();}}}}
            qaPlay_=qaLibraryView_=false;}if(w==0&&content_.command.active&&!content_.command.clips)commandQuery();const auto before=player_?player_->artwork():nullptr;const auto nextBefore=content_.nextArt;const UINT32 dj=content_.djAccent;playerArt();
        // A cover read: the song's own, or the next one's colour for the ring (Island DJ).
        if(player_&&player_->active()&&(player_->artwork()!=before||(w==1&&player_->upcoming()&&library_->artwork(player_->upcoming()->path)&&library_->artwork(player_->upcoming()->path)->accent!=dj)))updateSessions();
        libraryRows();if((content_.library||content_.upNext||content_.nextArt!=nextBefore)&&content_.page==Page::Media&&state_!=IslandState::Compact)refresh();
        // A song offered from another PC shows its cover once it is found here.
        if(state_==IslandState::Notification&&content_.notice.kind==17&&!content_.notice.icon&&handoffMatch_>=0){auto tracks=library_?library_->tracks():nullptr;if(tracks&&size_t(handoffMatch_)<tracks->size())if(auto art=library_->artwork((*tracks)[size_t(handoffMatch_)].path)){content_.notice.icon=art;refresh();}}
        return true;}
    case WM_TIMER:
        if(w==PlayerIdleTimer){KillTimer(window_,PlayerIdleTimer);if(player_&&!player_->playing()){player_->stop();updateSessions();}return true;}
        if(w==HandoffWaitTimer){handoffWait();return true;}
        if(w==PlayerFadeTimer){playerTick();return true;}
        if(w==AppFadeTimer){appFadeTick();return true;}
        if(w==QueueGlideTimer){if(!content_.queueDrag.active&&!(renderer_&&renderer_->queueGliding()))KillTimer(window_,QueueGlideTimer);refresh();return true;}
        return false;
    }
    return false;
}
// ---- Handoff ------------------------------------------------------------------------------------
// What plays, offered to a paired PC. For a song the island plays from a file, the file can follow.
void IslandWindow::handoffTo(size_t index){
    if(!share_||index>=content_.nearby.size())return;const auto& peer=content_.nearby[index];const auto& p=content_.playback;if(!p.available||!peer.paired)return;
    ShareHandoff music;music.title=p.title;music.artist=p.artist;music.app=p.source;music.duration=p.duration;music.playing=p.playing;
    music.position=std::clamp(p.position+(p.playing?std::max(0.,seconds()-p.sampledAt):0.),0.,p.duration>0?p.duration:1e9);
    std::wstring file;if(p.id==islandSessionId&&player_&&player_->track()){file=player_->track()->path;music.album=player_->track()->album;}
    // Phase 5H: the cover goes along (a 192-pixel JPEG), so the other PC's card shows it.
    if(p.artwork)music.cover=encodeCover(*p.artwork,192,shareCoverLimit);
    share_->handoff(peer.id,music,file);content_.handoffPicking=false;store_.log("Info","handoff_offered");
    shareCard(16,L"Offered to "+peer.name,p.title,{},3);
}
void IslandWindow::handoffEvent(const ShareEvent& e){
    using K=ShareEvent::Kind;
    if(e.kind==K::Handoff){
        // Only with the setting on (and the island's player able to play what might come).
        if(!settings_.handoff){share_->answerHandoff(e.transfer,0);return;}
        handoffOffer_=e.transfer;handoffMusic_=e.handoff;handoffFrom_=e.name;handoffAt_=seconds();handoffMatch_=-1;
        if(library_&&library_->scanned())if(auto tracks=library_->tracks())handoffMatch_=matchTrack(*tracks,e.handoff.title,e.handoff.artist,e.handoff.fileName,e.handoff.fileSize);
        shareCard(17,e.handoff.title,(e.handoff.artist.empty()?L"":e.handoff.artist+L"  ·  ")+L"from "+e.name,{},60);
        // The cover the other PC sent (Phase 5H), else this PC's copy of the song's, and where in the song it was.
        if(auto cover=decodeCover(e.handoff.cover))content_.notice.icon=cover;
        else if(handoffMatch_>=0){auto tracks=library_->tracks();content_.notice.icon=library_->artwork((*tracks)[size_t(handoffMatch_)].path);}
        content_.notice.progress=e.handoff.duration>0?std::clamp(e.handoff.position/e.handoff.duration,0.,1.):-1;refresh();
        return;}
    if(e.kind==K::HandoffAnswered){
        if(e.code==0){shareCard(16,e.name+L" said not now",e.handoff.title,{},3.5);return;}
        // The music moves: it stops here (when this is still the song playing).
        const auto& p=content_.playback;if(p.playing&&p.title==e.handoff.title)fadeOutForHandoff();
        shareCard(16,L"Playing on "+e.name,e.handoff.title,{},3.5);store_.log("Info","handoff_accepted");return;}
    if(e.kind==K::HandoffFile){
        if(!ensurePlayer()||!settings_.musicLibrary)return;LibraryTrack t=MusicLibrary::read(e.detail);if(t.title.empty()||t.title==titleFromFileName(e.detail)){if(!e.handoff.title.empty())t.title=e.handoff.title;}if(t.artist.empty())t.artist=e.handoff.artist;
        pauseOthers();player_->play({t},0,handoffStart_,true);player_->fadeIn(1.6);selectedId_=islandSessionId;if(library_)library_->artwork(t.path);playerArt();updateSessions();playerTick();store_.log("Info","handoff_song_received");}
}
// Opens the app the music came from (Spotify by its link, a Store app by its id, others by their Start menu entry).
bool IslandWindow::launchApp(const std::wstring& app){
    if(app.empty()||browserApp(app))return false;const auto a=lowerName(app);
    if(a.find(L"spotify")!=std::wstring::npos)return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"spotify:",nullptr,nullptr,SW_SHOWMINNOACTIVE))>32;
    if(app.find(L'!')!=std::wstring::npos){ComPtr<IApplicationActivationManager> manager;DWORD pid=0;
        return SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&manager)))&&SUCCEEDED(manager->ActivateApplication(app.c_str(),nullptr,AO_NONE,&pid));}
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",(L"shell:AppsFolder\\"+app).c_str(),nullptr,nullptr,SW_SHOWMINNOACTIVE))>32;
}
// Play here: the first way that works (a player with the song, the library, the song's file, the same app).
void IslandWindow::handoffAccept(){
    if(!share_||!handoffOffer_)return;const auto m=handoffMusic_;const uint32_t offer=handoffOffer_;handoffOffer_=0;
    const double start=std::max(0.,m.position+(m.playing?seconds()-handoffAt_:0.));handoffStart_=start;int code=0;
    const auto same=[&](const MediaSnapshot& s){return s.available&&s.id!=islandSessionId&&foldName(s.title)==foldName(m.title);};
    if(auto it=std::find_if(content_.sessions.begin(),content_.sessions.end(),same);it!=content_.sessions.end()){
        code=1;selectedId_=it->id;if(media_){media_->seek(start,*it);if(!it->playing)media_->control(1,it->source,it->id);}}
    else if(handoffMatch_>=0&&library_&&library_->tracks()&&size_t(handoffMatch_)<library_->tracks()->size()){
        code=1;std::vector<size_t> order(library_->tracks()->size());for(size_t k=0;k<order.size();++k)order[k]=k;playLibrary(order,size_t(handoffMatch_),start,true);if(player_){player_->fadeIn(1.6);playerTick();}}
    else if(m.fileSize>0&&settings_.musicLibrary&&ensurePlayer())code=2;
    else if(launchApp(m.app)){code=1;handoffWait_={m.app,m.title,start,seconds(),seconds()+20,false};SetTimer(window_,HandoffWaitTimer,500,nullptr);}
    share_->answerHandoff(offer,code);
    events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);
    if(!code)shareCard(16,L"Couldn't find it on this PC",m.title+(browserApp(m.app)?L"  ·  open it in your browser here":L""),{},4.5);
    else store_.log("Info","handoff_played");
}
// After opening the app: once its session shows the song, it jumps to where the other PC was (and plays).
void IslandWindow::handoffWait(){
    auto& w=handoffWait_;const double now=seconds();const auto app=lowerName(w.app);
    for(auto& s:content_.sessions){if(s.id==islandSessionId||lowerName(s.source).find(app)==std::wstring::npos)continue;
        if(foldName(s.title)==foldName(w.title)){if(media_){media_->seek(w.position+(now-w.accepted),s);if(!s.playing)media_->control(1,s.source,s.id);}selectedId_=s.id;KillTimer(window_,HandoffWaitTimer);return;}
        // Another song is cued: play it once (apps that sync, like Spotify, move on to the right one).
        if(!s.playing&&!w.nudged&&media_){media_->control(1,s.source,s.id);w.nudged=true;}}
    if(now>w.until)KillTimer(window_,HandoffWaitTimer);
}
}
