#include "IslandWindow.h"
#include <windowsx.h>
namespace nexus {
// Chooses which Windows media session the island shows. Following the system's
// current session is the default; a swipe or tap pins a choice until Windows
// marks a different session current or the chosen player disappears.
void IslandWindow::updateSessions(){
    auto list=media_?media_->sessions():std::vector<MediaSnapshot>{};int current=-1;
    // Phase 5G: the island's own player joins Windows' sessions, first; its copy through Windows' media controls is left out.
    if(player_&&player_->active()){const auto* t=player_->track();
        std::erase_if(list,[&](const MediaSnapshot& m){std::wstring source=m.source;for(auto& c:source)c=wchar_t(std::towlower(c));return source.find(L"arnavisland")!=std::wstring::npos||(t&&m.title==t->title&&(t->artist.empty()||m.artist==t->artist));});
        list.insert(list.begin(),player_->snapshot(seconds()));}
    for(size_t i=0;i<list.size();++i)if(list[i].current)current=int(i);
    // Sessions are told apart by id: every tab of one browser shares an app ID.
    const uint64_t system=current>=0?list[current].id:0;
    if(settings_.followSession&&system&&system!=followedId_)selectedId_=system;followedId_=system;
    int index=-1;for(size_t i=0;i<list.size()&&index<0;++i)if(list[i].id==selectedId_&&list[i].available)index=int(i);
    for(size_t i=0;i<list.size()&&index<0;++i)if(list[i].source==selectedSource_&&list[i].available)index=int(i);
    if(index<0)index=current>=0?current:0;
    MediaSnapshot s=list.empty()?MediaSnapshot{}:list[index];if(!list.empty()){selectedSource_=list[index].source;selectedId_=list[index].id;}
    if(content_.scrub.active&&(s.id!=content_.playback.id||s.title!=content_.playback.title||!s.canSeek))endScrub(false);
    content_.playback=s;content_.sessions=std::move(list);content_.session=content_.sessions.empty()?0:index;clockTimer();syncLyrics();
    bool changed=s.title!=content_.media;content_.media=s.title;content_.artist=s.artist;
    // Island DJ: the colour of the island's next song, once its cover is read.
    content_.djAccent=0;if(s.id==islandSessionId&&player_&&player_->upcoming()&&library_)if(auto art=library_->artwork(player_->upcoming()->path))content_.djAccent=art->accent;
    if(s.available&&changed){events_.publish({ActivityKind::Media,"media",20,0,.8,4},seconds());presentActivity();}else{refresh();animate();}
    if(s.available!=mediaReachable_){mediaReachable_=s.available;store_.log("Info",s.available?"media_session_connected":s.title!=L"Media access unavailable"?"media_manager_connected_no_session":"media_provider_unavailable");}
}
void IslandWindow::switchSession(int delta,bool absolute){
    int n=int(content_.sessions.size());if(n<1)return;int index=absolute?std::clamp(delta,0,n-1):((content_.session+delta)%n+n)%n;if(index==content_.session&&!absolute)return;
    int direction=absolute?(index>content_.session?1:-1):delta;if(content_.scrub.active)endScrub(false);
    selectedSource_=content_.sessions[index].source;selectedId_=content_.sessions[index].id;content_.session=index;content_.playback=content_.sessions[index];content_.media=content_.playback.title;content_.artist=content_.playback.artist;syncLyrics();
    double now=seconds();if(!motion_.reduced){motion_.swipe.reset(direction*26.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}
    store_.log("Info","media_session_selected");clockTimer();refresh();animate();
}
// Loopback analysis and mixer metering run only while something shows them.
void IslandWindow::updateProviders(){
    bool visible=IsWindowVisible(window_)!=FALSE;
    bool bars=settings_.waveform&&visible&&content_.playback.playing&&content_.hud==0&&(state_==IslandState::Compact?settings_.edge==0&&settings_.compactMedia:(content_.live||content_.page==Page::Media));
    // The waveform timeline needs the same real audio while the Media page shows it.
    bars=bars||(settings_.waveTimeline&&visible&&content_.playback.playing&&state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Media);
    // So does the beat pulse of the cover on the Home page.
    bars=bars||(settings_.artPulse&&!motion_.reduced&&visible&&content_.playback.playing&&content_.playback.artwork&&state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Overview);
    if(analyzer_)analyzer_->setActive(bars);bool delivering=bars&&analyzer_&&analyzer_->available.load();if(!analyzer_&&testing_)delivering=content_.waveform;
    if(delivering!=content_.waveform){content_.waveform=delivering;if(renderer_)refresh();}
    if(mixer_)mixer_->setMetering(visible&&state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Audio&&content_.audioTab==0);
}
// The pointer has moved onto the island body, clear of the edge rows that reveal it.
bool IslandWindow::pointerOffEdge(){
    POINT c{};GetCursorPos(&c);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);const double band=10*dpi_/96;
    return settings_.edge==1?mi.rcMonitor.right-1-c.x>band:settings_.edge==2?c.x-mi.rcMonitor.left>band:c.y-mi.rcMonitor.top>band;
}
void IslandWindow::autoHideTick(){
    if(!renderer_||!IsWindowVisible(window_))return;double now=seconds(),s=dpi_/96;POINT c{};GetCursorPos(&c);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);RECT w{};GetWindowRect(window_,&w);
    // Resting geometry, not the animated pose, so the trigger never drifts.
    const double bw=motion_.width.target(),bh=motion_.height.target();auto origin=bodyAt(bw,bh,motion_.drop.target());
    const double left=w.left+origin.x*s,top=w.top+origin.y*s,right=left+bw*s,bottom=top+bh*s;
    const double center=settings_.edge?(top+bottom)/2:(left+right)/2,half=(settings_.edge?bh:bw)*s/2+56*s;
    bool atEdge=atIslandEdge(c.x,c.y,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right,mi.rcMonitor.bottom,settings_.edge,center,half);
    bool over=settings_.edge==1?(c.x>=left-8*s&&c.y>=top-8*s&&c.y<=bottom+8*s):settings_.edge==2?(c.x<=right+8*s&&c.y>=top-8*s&&c.y<=bottom+8*s):(c.x>=left-8*s&&c.x<=right+8*s&&c.y>=mi.rcMonitor.top&&c.y<=bottom+8*s);
    bool engaged=state_!=IslandState::Compact||content_.pinned||interaction_!=InteractionState::Rest||content_.dropHover||content_.scrub.active||(settings_.alertsReveal&&events_.active().has_value()&&events_.active()->kind!=ActivityKind::Media);
    bool before=autoHide_.hidden,hidden=autoHide_.update(settings_.autoHide,engaged,atEdge,over,now,settings_.collapseDelay/1000.);
    if(hidden==before)return;
    if(hidden){KillTimer(window_,7);KillTimer(window_,19);content_.hovered=Action::None;}
    if(motion_.reduced)motion_.slide.reset(hidden?1:0,now);else motion_.slide.retarget(hidden?1:0,now,hidden?SpringSpec{1,300,34}:SpringSpec{.9,420,26});
    // The idle glance's readings stop while the island is tucked away.
    animate();clockTimer();store_.log("Info",hidden?"island_tucked":"island_revealed_at_edge");
}
// Connection and charging cards: a short Notification-state island.
void IslandWindow::showNotice(int kind,const BluetoothDevice& device){
    if(!renderer_||(state_!=IslandState::Compact&&state_!=IslandState::Notification))return;
    if(settings_.autoHide&&autoHide_.hidden&&!settings_.alertsReveal)return;
    content_.notice={kind,device};{const Activity a{kind>=3?ActivityKind::Power:ActivityKind::Device,"notice",60,double(kind),2.4,3.2};if(holdCard(a))return;events_.publish(a,seconds());}
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info",kind>=3?"power_card_shown":"device_card_shown");
}
// The glint along the edge, and (Phase 5G, with sounds on and nothing full screen) its faint chime.
void IslandWindow::alertSplash(){if(settings_.sounds&&!fullscreenHidden_&&renderer_)playSound(Sound::Chime);if(settings_.edgeSplash&&renderer_)renderer_->splash(float(motion_.width.target()),float(motion_.height.target()),float(motion_.radius.target()),!settings_.floating()&&motion_.drop.target()<=0,motion_.reduced?0:.34,motion_.reduced);}
// ---- Phase 5G: two alerts at once ------------------------------------------------------------------
// With the drop pill (top dock), an alert that arrives while another shows waits below it as a bud; the island
// never replaces an alert someone may need to answer. The same alert updating (same kind and subject) still
// updates in place. At most four wait; each takes the pill's place when the one before ends, is answered or
// is clicked past (clicking the bud swaps them, keeping an unanswered pairing, offer or music card waiting).
bool IslandWindow::holdCard(const Activity& a){
    const auto& n=content_.notice;const auto& s=shownNotice_;
    const bool stacking=settings_.stackAlerts&&settings_.notifyStyle==1&&settings_.edge==0&&!settings_.floating()&&state_==IslandState::Notification&&content_.card&&events_.active().has_value()&&s.kind!=0;
    const bool same=n.kind==s.kind&&n.app==s.app&&n.device.name==s.device.name;
    if(!stacking||same){shownNotice_=content_.notice;return false;}
    if(heldCards_.size()>=4)heldCards_.pop_back();
    heldCards_.push_back({content_.notice,a});content_.notice=shownNotice_;
    if(settings_.sounds&&!fullscreenHidden_)playSound(Sound::Chime);
    syncBud();refresh();store_.log("Info","alert_held");return true;
}
// The next waiting alert takes the pill's place (swap: the one showing waits again if it still needs an answer).
bool IslandWindow::promoteCard(bool swap){
    if(heldCards_.empty())return false;const double now=seconds();
    if(swap&&events_.active()){const int k=content_.notice.kind;if(k==14||k==15||k==17)heldCards_.push_back({content_.notice,*events_.active()});}
    auto next=std::move(heldCards_.front());heldCards_.pop_front();
    content_.notice=next.notice;shownNotice_=next.notice;events_.dismiss(now);events_.publish(next.activity,now);
    // The bud rises into the pill as its alert arrives there; another one buds again a moment later.
    if(motion_.reduced)motion_.bud.reset(0,now);else motion_.bud.retarget(0,now,SpringSpec{1,320,36});
    content_.bud={};transition(IslandState::Notification);presentActivity();alertSplash();
    if(!heldCards_.empty())SetTimer(window_,63,460,nullptr);
    store_.log("Info","alert_promoted");return true;
}
// The bud shows while an alert waits (and the pill is out); it buds with a little spring.
void IslandWindow::syncBud(){
    const bool show=!heldCards_.empty()&&state_==IslandState::Notification&&settings_.notifyStyle==1&&settings_.edge==0&&!settings_.floating();const double now=seconds();
    if(show){const auto& n=heldCards_.front().notice;content_.bud={n.kind,budTitle(n),int(heldCards_.size())-1};}else{content_.bud={};spreadAlerts(false);}
    const double target=show?1:0;if(motion_.bud.target()!=target){if(motion_.reduced)motion_.bud.reset(target,now);else motion_.bud.retarget(target,now,show?SpringSpec{1,170,15}:SpringSpec{1,320,36});animate();}
}
// Whether a point in the canvas (DIPs) is on the bud (once it has let go of the pill).
bool IslandWindow::budAt(double x,double y)const{
    const double t=seconds();if(settings_.edge!=0||!dropped(t)||content_.bud.kind==0)return false;const double b=motion_.bud.sample(t).position;if(b<.6)return false;
    const auto origin=bodyAt(motion_.width.sample(t).position,motion_.height.sample(t).position,motion_.drop.sample(t).position);
    const auto bud=budNow(t,origin,motion_.width.sample(t).position,motion_.height.sample(t).position);
    return x>=bud.left&&x<=bud.right&&y>=bud.top&&y<=bud.bottom;
}
// Phase 5H: while the pointer is on the island (or its bud), the two alerts spread side by side; the pill makes room.
void IslandWindow::spreadAlerts(bool on){
    on=on&&content_.bud.kind!=0&&state_==IslandState::Notification&&settings_.edge==0&&!settings_.floating();const double now=seconds(),target=on?1:0;
    if(on)KillTimer(window_,SpreadTimer);if(motion_.spread.target()==target)return;
    if(on)motion_.spreadShift=std::max(0.,std::min((motion_.budWidth+budGap)/2,(Renderer::canvasWidth-motion_.width.target())/2-4));
    if(motion_.reduced)motion_.spread.reset(target,now);else motion_.spread.retarget(target,now,on?SpringSpec{1,230,24}:SpringSpec{1,300,32});
    animate();
}
std::wstring IslandWindow::budTitle(const ContentSnapshot::Notice& n){
    switch(n.kind){
    case 1:return n.device.name+L" connected";case 2:return n.device.name+L" disconnected";case 3:return L"Charging";case 4:return L"On battery";
    case 5:return (n.app.empty()?std::wstring(L"An app"):n.app)+L" \u00b7 camera";case 6:return (n.app.empty()?std::wstring(L"An app"):n.app)+L" \u00b7 microphone";case 7:return (n.app.empty()?std::wstring(L"An app"):n.app)+L" \u00b7 location";
    case 8:return L"Sound moved to "+n.device.name;case 9:return L"Colour picked";case 10:return L"Text copied";case 11:return L"Snip on the Shelf";case 12:return (n.app.empty()?std::wstring(L"An app"):n.app)+L" \u00b7 screen";
    case 13:return L"Your battery this week";case 14:return L"Pair with "+n.app+L"?";case 15:return n.app+L" is sending";case 17:return L"Continue "+n.app;default:return n.app;}
}
void IslandWindow::updateBattery(){
    auto reading=battery_->reading();auto estimate=battery_->estimate();content_.power=reading;content_.toFull=estimate.minutesToFull(reading);content_.remaining=estimate.minutesRemaining(reading);
    auto history=battery_->history();content_.history.clear();const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for(auto& sample:history.samples)if(now-sample.time<=86400){content_.history.push_back(float(1-double(now-sample.time)/86400.));content_.history.push_back(sample.percent/100.f);}
    // The health trend, and once a week (from 9 o'clock, with at least three days of history) its card.
    if(!testing_){auto log=battery_->health();if(auto w=log.week()){content_.healthBefore=w->first;content_.healthNow=w->second;}else{content_.healthBefore=-1;content_.healthNow=log.days.empty()?-1:BatteryHealthLog::health(log.days.back());}
        if(settings_.batteryWeekly&&settings_.batteryHistory){if(log.lastCard==0)battery_->markWeeklyCard(now);
            else if(now-log.lastCard>=7*86400){SYSTEMTIME t{};GetLocalTime(&t);auto week=summarizeWeek(history,now);if(t.wHour>=9&&week.days>=3&&state_==IslandState::Compact&&!(settings_.autoHide&&autoHide_.hidden)){content_.week=week;battery_->markWeeklyCard(now);showNotice(13);}}}}
    if((state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==1)||(content_.card&&(content_.notice.kind==3||content_.notice.kind==4||content_.notice.kind==13)))refresh();
}
// Volume and brightness changes grow the resting island into a level indicator.
void IslandWindow::levelIndicator(){
    int hud=0;if(settings_.hud&&settings_.edge==0&&state_==IslandState::Compact&&events_.active()){auto kind=events_.active()->kind;hud=kind==ActivityKind::Volume?1:kind==ActivityKind::Brightness&&content_.brightness>=0?2:kind==ActivityKind::AppVolume&&content_.appVolume>=0?3:0;}
    double base=settings_.uiMode==0?72:settings_.compactWidth,now=seconds();
    double level=hud==2?content_.brightness/100.:hud==3?content_.appVolume/100.:content_.muted?0:content_.volume/100.;
    if(hud){if(!content_.hud||motion_.reduced)motion_.level.reset(level,now);else motion_.level.retarget(level,now,{1,520,40});}
    if(hud!=content_.hud){content_.hud=hud;motion_.compactWidth=hud?std::max(base,236.):base;updateProviders();}
}
void IslandWindow::setMixerAt(LPARAM point){
    size_t index=int(pressedAction_)-int(Action::MixerSliderBase);if(!mixer_||index>=content_.mixer.size())return;double now=seconds();
    auto origin=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);
    float value=float(std::clamp((GET_X_LPARAM(point)*96/dpi_-origin.x-motion_.dragX.sample(now).position-20-156)/174.,0.,1.));
    auto& e=content_.mixer[index];if(std::abs(e.volume-value)<.004f)return;e.volume=value;if(e.muted&&value>0){e.muted=false;mixer_->setMute(e.pid,false);}mixer_->setVolume(e.pid,value);refresh();
}
// Synced lyrics for the current track, once the setting is on. QA runs never go online.
void IslandWindow::syncLyrics(){
    const auto& p=content_.playback;
    if(!settings_.lyrics||!p.available||!(p.duration>=20)||p.title.empty()){lyricsKey_.clear();content_.lyrics=nullptr;content_.lyricsState=0;content_.lyricLine=-1;KillTimer(window_,38);return;}
    if(testing_){tickLyrics(false);return;}
    if(!lyrics_)lyrics_=std::make_unique<LyricsService>(window_,store_.directory/L"lyrics");
    const LyricsService::Track track{p.title,p.artist,p.duration,p.browser};const auto key=LyricsService::key(track);
    if(key!=lyricsKey_){lyricsKey_=key;content_.lyricLine=-1;}
    lyrics_->request(track);// repeats are ignored; a failed lookup is retried after a minute
    auto r=lyrics_->get(key);content_.lyrics=r.state==LyricsService::State::Found?r.lines:nullptr;content_.lyricsState=int(r.state);
    tickLyrics(false);
}
// Moves to the line being sung and wakes up again when the next one starts.
void IslandWindow::tickLyrics(bool redraw){
    KillTimer(window_,38);const auto& p=content_.playback;int line=-1;
    if(content_.lyrics&&!content_.lyrics->empty()){const double position=p.position+(p.playing?std::max(0.,seconds()-p.sampledAt):0.);line=lyricIndex(*content_.lyrics,position);
        if(p.playing){const double next=nextLyricIn(*content_.lyrics,position);if(next>=0)SetTimer(window_,38,UINT(std::clamp(next*1000+20,30.,60000.)),nullptr);}}
    if(line!=content_.lyricLine){content_.lyricLine=line;if(redraw&&renderer_)refresh();}
}
void IslandWindow::clearLyrics(){
    if(lyrics_)lyrics_->clear();else{std::error_code ignored;std::filesystem::remove_all(store_.directory/L"lyrics",ignored);}
    lyricsKey_.clear();content_.lyrics=nullptr;content_.lyricsState=0;content_.lyricLine=-1;store_.log("Info","lyrics_cleared");syncLyrics();refresh();
}
// Skips within the session's seekable range and shows which way it went on the artwork.
void IslandWindow::seekBy(double delta){
    auto& p=content_.playback;if(!p.canSeek||!(p.duration>0)||!renderer_)return;const double now=seconds();
    const double position=p.position+(p.playing?std::max(0.,now-p.sampledAt):0.),hi=p.seekMax>p.seekMin?p.seekMax:p.duration;
    const double target=skipTarget(position,delta,p.seekMin,hi);mediaSeek(target);p.position=target;p.sampledAt=now;
    if(state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Media){const bool video=settings_.mediaLayout==2||(settings_.mediaLayout==0&&p.kind==MediaKind::Video);const float top=video?30.f:44.f,size=video?148.f:100.f;
        renderer_->skipFeedback(delta>0,20+size*(delta>0?.75f:.25f),38+top+size/2,motion_.reduced);}
    tickLyrics(false);refresh();store_.log("Info","media_skip");
}
// A tapped lyric line: playback jumps to where that line begins.
void IslandWindow::seekLyric(int line){
    auto& p=content_.playback;if(!p.canSeek||!(p.duration>0)||!content_.lyrics||line<0||size_t(line)>=content_.lyrics->size())return;
    const double hi=p.seekMax>p.seekMin?p.seekMax:p.duration,target=std::clamp((*content_.lyrics)[size_t(line)].time,p.seekMin,hi);
    mediaSeek(target);p.position=target;p.sampledAt=seconds();tickLyrics(false);refresh();store_.log("Info","lyrics_seek");
}
// The wheel over the compact island's logo changes the volume of the app that is playing.
void IslandWindow::appVolumeWheel(int delta){
    std::vector<MixerName> names;for(auto& e:content_.mixer)names.push_back({e.name,e.active,e.system});
    const int i=mixerIndexFor(content_.playback.appName,names);
    if(i>=0){auto& e=content_.mixer[size_t(i)];const float v=std::clamp(e.volume+.04f*float(delta)/120.f,0.f,1.f);
        if(mixer_){if(e.muted&&v>0){e.muted=false;mixer_->setMute(e.pid,false);}mixer_->setVolume(e.pid,v);}e.volume=v;content_.appVolume=int(std::lround(v*100));content_.appVolumeName=e.name;}
    else{content_.appVolume=-1;content_.appVolumeName.clear();}
    events_.publish({ActivityKind::AppVolume,"app-volume",40,double(content_.appVolume),.5,2},seconds());presentActivity();
}
// A short card when the sound moves to headphones on its own; false when it cannot show now.
bool IslandWindow::showHeadphoneCard(const AudioDevice& output,const std::wstring& fromId,const std::wstring& fromName){
    if(!renderer_||(state_!=IslandState::Compact&&state_!=IslandState::Notification))return false;
    if(settings_.autoHide&&autoHide_.hidden&&!settings_.alertsReveal)return false;
    BluetoothDevice device;device.name=outputDisplayName(output.name);
    // The paired Bluetooth device behind this output, for its logo and battery.
    for(auto& d:content_.devices)if(d.name.size()>=3&&routeFold(output.name).find(routeFold(d.name))!=std::wstring::npos){device=d;break;}
    if(device.brand.empty())device.brand=std::string(deviceBrand(device.name));if(device.kind==DeviceKind::Other)device.kind=deviceKind(0,device.name);if(device.kind==DeviceKind::Other)device.kind=DeviceKind::Headphones;
    const bool back=settings_.directAudio&&std::any_of(content_.outputs.begin(),content_.outputs.end(),[&](auto& d){return d.id==fromId;});switchBackId_=back?fromId:std::wstring{};
    content_.notice={8,device,outputDisplayName(fromName),nullptr,back};
    {const Activity a{ActivityKind::Device,"headphones",60,8,2.4,6};if(holdCard(a))return true;events_.publish(a,seconds());}transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","headphone_card_shown");return true;
}
}
