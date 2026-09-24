#include "IslandWindow.h"
#include <windowsx.h>
namespace nexus {
// Chooses which Windows media session the island shows. Following the system's
// current session is the default; a swipe or tap pins a choice until Windows
// marks a different session current or the chosen player disappears.
void IslandWindow::updateSessions(){
    auto list=media_->sessions();int current=-1;
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
    const double bw=motion_.width.target(),bh=motion_.height.target();auto origin=bodyOrigin(bw,bh,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);
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
    content_.notice={kind,device};events_.publish({kind>=3?ActivityKind::Power:ActivityKind::Device,"notice",60,double(kind),2.4,3.2},seconds());
    transition(IslandState::Notification);presentActivity();store_.log("Info",kind>=3?"power_card_shown":"device_card_shown");
}
void IslandWindow::updateBattery(){
    auto reading=battery_->reading();auto estimate=battery_->estimate();content_.power=reading;content_.toFull=estimate.minutesToFull(reading);content_.remaining=estimate.minutesRemaining(reading);
    auto history=battery_->history();content_.history.clear();const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for(auto& sample:history.samples)if(now-sample.time<=86400){content_.history.push_back(float(1-double(now-sample.time)/86400.));content_.history.push_back(sample.percent/100.f);}
    if((state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==1)||(content_.card&&(content_.notice.kind==3||content_.notice.kind==4)))refresh();
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
    auto origin=bodyOrigin(motion_.width.sample(now).position,motion_.height.sample(now).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);
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
    const double target=skipTarget(position,delta,p.seekMin,hi);if(media_)media_->seek(target,p);p.position=target;p.sampledAt=now;
    if(state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Media){const bool video=settings_.mediaLayout==2||(settings_.mediaLayout==0&&p.kind==MediaKind::Video);const float top=video?30.f:44.f,size=video?148.f:100.f;
        renderer_->skipFeedback(delta>0,20+size*(delta>0?.75f:.25f),38+top+size/2,motion_.reduced);}
    tickLyrics(false);refresh();store_.log("Info","media_skip");
}
// A tapped lyric line: playback jumps to where that line begins.
void IslandWindow::seekLyric(int line){
    auto& p=content_.playback;if(!p.canSeek||!(p.duration>0)||!content_.lyrics||line<0||size_t(line)>=content_.lyrics->size())return;
    const double hi=p.seekMax>p.seekMin?p.seekMax:p.duration,target=std::clamp((*content_.lyrics)[size_t(line)].time,p.seekMin,hi);
    if(media_)media_->seek(target,p);p.position=target;p.sampledAt=seconds();tickLyrics(false);refresh();store_.log("Info","lyrics_seek");
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
    events_.publish({ActivityKind::Device,"headphones",60,8,2.4,6},seconds());transition(IslandState::Notification);presentActivity();store_.log("Info","headphone_card_shown");return true;
}
}
