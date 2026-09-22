#include "Renderer.h"
namespace nexus {
Action Renderer::hit(float x,float y)const {
    if(y>=38+navY&&y<38+navY+44){double now=seconds();for(size_t i=0;i<7;++i){auto& icon=icons_[i];if(!icon.used)continue;double left=icon.x.sample(now).position+16-icon.drawnSize/2-14;if(x>=left&&x<left+47)return icon.action;}return Action::None;}
    for(auto& target:targets)if(target.y<navY&&target.contains(x-20,y-38))return target.action;return Action::None;
}
void Renderer::icon(Action action,Icon glyph,float x,float y,float size,UINT32 color,int stableSlot){
    if(drawingContent_){iconRequests_.push_back({action,glyph,x,y,size,color,stableSlot});return;}
    size_t slot=stableSlot>=0?size_t(stableSlot):iconCursor_++;if(slot>=icons_.size())return;auto& item=icons_[slot];double now=seconds();bool initial=item.key<0||item.action!=action;item.used=true;item.action=action;int key=(int(glyph)<<24)|int(color);
    if(item.key!=key||item.drawnSize!=size){surface(item.surface,32,32,[&](auto* rt){drawIcon(rt,d2d_.Get(),glyph,16-size/2,16-size/2,size,color);});item.visual->SetContent(item.surface.Get());item.key=key;item.drawnSize=size;}
    float px=std::round((20+x+size/2-16)*scale_)/scale_,py=std::round((38+y+size/2-16)*scale_)/scale_;item.baseY=py;double dy=py+(iconMotion_&&action!=Action::None&&hoverAction_==action?-1.25:0);
    auto aim=[&](Spring& spring,double target){if(initial||!iconMotion_)spring.reset(target,now);else if(std::abs(spring.target()-target)>.01)spring.retarget(target,now,MotionTokens::iconPosition);};aim(item.x,px);aim(item.y,dy);
    if(initial)item.zoom.reset(1,now);item.effect->SetOpacity(1.f);
    auto ax=animation(item.x,now,scale_),ay=animation(item.y,now,scale_),zoom=animation(item.zoom,now);item.visual->SetOffsetX(ax.Get());item.visual->SetOffsetY(ay.Get());item.scale->SetScaleX(zoom.Get());item.scale->SetScaleY(zoom.Get());
}
void Renderer::iconFeedback(Action action,bool pressed,bool enabled){hoverAction_=action;pressing_=pressed;double now=seconds();bool changed=false;for(auto& item:icons_){if(!item.used)continue;bool hover=enabled&&action!=Action::None&&item.action==action;double scale=hover?(pressed?.88:1.09):1,y=item.baseY+(hover&&!pressed?-1.25:0);if(std::abs(item.zoom.target()-scale)<.001&&std::abs(item.y.target()-y)<.001)continue;changed=true;if(enabled){item.zoom.retarget(scale,now,MotionTokens::icon);item.y.retarget(y,now,MotionTokens::icon);}else{item.zoom.reset(1,now);item.y.reset(item.baseY,now);}auto z=animation(item.zoom,now),dy=animation(item.y,now,scale_);item.scale->SetScaleX(z.Get());item.scale->SetScaleY(z.Get());item.visual->SetOffsetY(dy.Get());}if(changed)commit();}
void Renderer::updateArtwork(const ContentSnapshot& s,UINT32 background){
    if(artwork_==s.playback.artwork)return;bool first=!artwork_;artwork_=s.playback.artwork;double now=seconds();handoff_.change(artwork_.get(),background,now,!first&&s.settings.trackHandoff&&!s.reducedMotion);
    auto draw=[&](ComPtr<IDCompositionSurface>& target,const std::vector<uint8_t>& pixels){surface(target,256,256,[&](auto* rt){ComPtr<ID2D1Bitmap> bitmap;auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));check(rt->CreateBitmap({256,256},pixels.data(),256*4,properties,&bitmap));rt->DrawBitmap(bitmap.Get(),{0,0,256,256});});};
    draw(artFromSurface_,handoff_.from);draw(artToSurface_,handoff_.to);artFrom_->SetContent(artFromSurface_.Get());artTo_->SetContent(artToSurface_.Get());auto fade=animation(handoff_.mix,now);incomingEffect_->SetOpacity(fade.Get());
}
void Renderer::updateRings(const ContentSnapshot& s,UINT32 accent,UINT32 muted,UINT32 track){
    bool battery=(s.settings.glanceRings&1)&&s.battery>=0,timer=(s.settings.glanceRings&2);ringsEnabled_=battery||timer;ringCount_=int(battery)+int(timer);ringsEffect_->SetOpacity(ringsEnabled_?1.f:0.f);double now=seconds();bool running=s.focus.running||s.focus.held>0||s.focus.finished;
    double bp=std::clamp(s.battery/100.,0.,1.),tp=s.focus.mode==FocusClock::Mode::Stopwatch?std::fmod(s.focus.elapsed(now),60.)/60.:normalizedProgress(s.focus.displayed(now),s.focus.duration);
    int flags=int(battery)|(int(timer)<<1)|(int(running)<<2)|(int(s.charging)<<3);if(ringBattery_==bp&&ringTimer_==tp&&ringFlags_==flags&&ringColor_==accent&&ringTrack_==track)return;ringBattery_=bp;ringTimer_=tp;ringFlags_=flags;ringColor_=accent;ringTrack_=track;
    surface(ringsSurface_,56,34,[&](auto* rt){if(battery){drawRing(rt,d2d_.Get(),timer?12.f:36.f,17,8.5f,1.8f,bp,accent,track);drawIcon(rt,d2d_.Get(),s.charging?Icon::Power:Icon::Battery,timer?7.f:31.f,12,10,accent);}if(timer){drawRing(rt,d2d_.Get(),36,17,8.5f,1.8f,running?tp:0,accent,track);drawIcon(rt,d2d_.Get(),Icon::Focus,31,12,10,running?accent:muted);}});rings_->SetContent(ringsSurface_.Get());
    surface(dotSurface_,32,32,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillEllipse(D2D1::Ellipse({16,7.5f},1.6f,1.6f),b.Get());});batteryDot_->SetOffsetX((timer?-4.f:20.f)*scale_);batteryDot_->SetContent(dotSurface_.Get());timerDot_->SetContent(dotSurface_.Get());batteryDotEffect_->SetOpacity(battery?1.f:0.f);timerDotEffect_->SetOpacity(timer&&running?1.f:0.f);
    double degrees=tp*360;if(s.focus.mode==FocusClock::Mode::Stopwatch){double base=std::floor(timerAngle.target()/360)*360;degrees+=base;if(degrees<timerAngle.target()-180)degrees+=360;}
    auto aim=[&](Spring& spring,double target){if(s.reducedMotion)spring.reset(target,now);else if(std::abs(spring.target()-target)>.001)spring.retarget(target,now,MotionTokens::ring);};aim(batteryAngle,bp*360);aim(timerAngle,degrees);auto ba=animation(batteryAngle,now),ta=animation(timerAngle,now);batteryRotation_->SetAngle(ba.Get());timerRotation_->SetAngle(ta.Get());
}
}
