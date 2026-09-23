#include "IslandWindow.h"
#include <windowsx.h>
namespace nexus {
// Chooses which Windows media session the island shows. Following the system's
// current session is the default; a swipe or tap pins a choice until Windows
// marks a different session current or the chosen player disappears.
void IslandWindow::updateSessions(){
    auto list=media_->sessions();int current=-1;
    for(size_t i=0;i<list.size();++i)if(list[i].current)current=int(i);
    std::wstring system=current>=0?list[current].source:std::wstring{};
    if(settings_.followSession&&!system.empty()&&system!=followedSource_)selectedSource_=system;followedSource_=system;
    int index=-1;for(size_t i=0;i<list.size();++i)if(list[i].source==selectedSource_&&list[i].available)index=int(i);
    if(index<0)index=current>=0?current:0;
    MediaSnapshot s=list.empty()?MediaSnapshot{}:list[index];if(!list.empty())selectedSource_=list[index].source;
    if(content_.scrub.active&&(s.source!=content_.playback.source||s.title!=content_.playback.title||!s.canSeek))endScrub(false);
    content_.playback=s;content_.sessions=std::move(list);content_.session=content_.sessions.empty()?0:index;clockTimer();
    bool changed=s.title!=content_.media;content_.media=s.title;content_.artist=s.artist;
    if(s.available&&changed){events_.publish({ActivityKind::Media,"media",20,0,.8,4},seconds());presentActivity();}else{refresh();animate();}
    if(s.available!=mediaReachable_){mediaReachable_=s.available;store_.log("Info",s.available?"media_session_connected":s.title!=L"Media access unavailable"?"media_manager_connected_no_session":"media_provider_unavailable");}
}
void IslandWindow::switchSession(int delta,bool absolute){
    int n=int(content_.sessions.size());if(n<1)return;int index=absolute?std::clamp(delta,0,n-1):((content_.session+delta)%n+n)%n;if(index==content_.session&&!absolute)return;
    int direction=absolute?(index>content_.session?1:-1):delta;if(content_.scrub.active)endScrub(false);
    selectedSource_=content_.sessions[index].source;content_.session=index;content_.playback=content_.sessions[index];content_.media=content_.playback.title;content_.artist=content_.playback.artist;
    double now=seconds();if(!motion_.reduced){motion_.swipe.reset(direction*26.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}
    store_.log("Info","media_session_selected");clockTimer();refresh();animate();
}
// Loopback analysis and mixer metering run only while something shows them.
void IslandWindow::updateProviders(){
    bool visible=IsWindowVisible(window_)!=FALSE;
    bool bars=settings_.waveform&&visible&&content_.playback.playing&&content_.hud==0&&(state_==IslandState::Compact?settings_.edge==0&&settings_.compactMedia:(content_.live||content_.page==Page::Media));
    if(analyzer_)analyzer_->setActive(bars);bool delivering=bars&&analyzer_&&analyzer_->available.load();if(!analyzer_&&testing_)delivering=content_.waveform;
    if(delivering!=content_.waveform){content_.waveform=delivering;if(renderer_)refresh();}
    if(mixer_)mixer_->setMetering(visible&&state_!=IslandState::Compact&&!content_.live&&content_.page==Page::Audio&&content_.audioTab==0);
}
void IslandWindow::autoHideTick(){
    if(!renderer_||!IsWindowVisible(window_))return;double now=seconds(),s=dpi_/96;POINT c{};GetCursorPos(&c);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&mi);RECT w{};GetWindowRect(window_,&w);
    // Resting geometry, not the animated pose, so the trigger never drifts.
    const double bw=motion_.width.target(),bh=motion_.height.target();auto origin=bodyOrigin(bw,bh,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);
    const double left=w.left+origin.x*s,top=w.top+origin.y*s,right=left+bw*s,bottom=top+bh*s;
    const double center=settings_.edge?(top+bottom)/2:(left+right)/2,half=(settings_.edge?bh:bw)*s/2+56*s;
    bool atEdge=atIslandEdge(c.x,c.y,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right,mi.rcMonitor.bottom,settings_.edge,center,half);
    bool over=settings_.edge?(c.x>=left-8*s&&c.y>=top-8*s&&c.y<=bottom+8*s):(c.x>=left-8*s&&c.x<=right+8*s&&c.y>=mi.rcMonitor.top&&c.y<=bottom+8*s);
    bool engaged=state_!=IslandState::Compact||content_.pinned||interaction_!=InteractionState::Rest||content_.dropHover||content_.scrub.active||(settings_.alertsReveal&&events_.active().has_value());
    bool before=autoHide_.hidden,hidden=autoHide_.update(settings_.autoHide,engaged,atEdge,over,now,settings_.collapseDelay/1000.);
    if(hidden==before)return;
    if(hidden){KillTimer(window_,7);KillTimer(window_,19);content_.hovered=Action::None;}
    if(motion_.reduced)motion_.slide.reset(hidden?1:0,now);else motion_.slide.retarget(hidden?1:0,now,hidden?SpringSpec{1,300,34}:SpringSpec{.9,420,26});
    animate();store_.log("Info",hidden?"island_tucked":"island_revealed_at_edge");
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
    if((state_==IslandState::Expanded&&content_.page==Page::System&&content_.statsTab==1)||(content_.card&&content_.notice.kind>=3))refresh();
}
// Volume and brightness changes grow the resting island into a level indicator.
void IslandWindow::levelIndicator(){
    int hud=0;if(settings_.hud&&settings_.edge==0&&state_==IslandState::Compact&&events_.active()){auto kind=events_.active()->kind;hud=kind==ActivityKind::Volume?1:kind==ActivityKind::Brightness&&content_.brightness>=0?2:0;}
    double base=settings_.uiMode==0?72:settings_.compactWidth,now=seconds();
    double level=hud==2?content_.brightness/100.:content_.muted?0:content_.volume/100.;
    if(hud){if(!content_.hud||motion_.reduced)motion_.level.reset(level,now);else motion_.level.retarget(level,now,{1,520,40});}
    if(hud!=content_.hud){content_.hud=hud;motion_.compactWidth=hud?std::max(base,236.):base;updateProviders();}
}
void IslandWindow::setMixerAt(LPARAM point){
    size_t index=int(pressedAction_)-int(Action::MixerSliderBase);if(!mixer_||index>=content_.mixer.size())return;double now=seconds();
    auto origin=bodyOrigin(motion_.width.sample(now).position,motion_.height.sample(now).position,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);
    float value=float(std::clamp((GET_X_LPARAM(point)*96/dpi_-origin.x-motion_.dragX.sample(now).position-20-156)/174.,0.,1.));
    auto& e=content_.mixer[index];if(std::abs(e.volume-value)<.004f)return;e.volume=value;if(e.muted&&value>0){e.muted=false;mixer_->setMute(e.pid,false);}mixer_->setVolume(e.pid,value);refresh();
}
}
