#include "Renderer.h"
namespace nexus {
Action Renderer::hit(float x,float y)const {
    if(live_)y+=18;
    if(y>=38+navY&&y<38+navY+44){double now=seconds();for(size_t i=0;i<7;++i){auto& icon=icons_[i];if(!icon.used)continue;double left=icon.x.sample(now).position+16-icon.drawnSize/2-14;if(x>=left&&x<left+47)return icon.action;}return Action::None;}
    for(auto& target:targets)if(target.y<navY&&target.contains(x-20,y-38))return target.action;return Action::None;
}
void Renderer::icon(Action action,Icon glyph,float x,float y,float size,UINT32 color,int stableSlot){
    if(drawingContent_){iconRequests_.push_back({action,glyph,x,y,size,color,stableSlot});return;}
    size_t slot=stableSlot>=0?size_t(stableSlot):iconCursor_++;if(slot>=icons_.size())return;auto& item=icons_[slot];double now=seconds();bool initial=item.key<0||item.action!=action;item.used=true;item.action=action;int key=(int(glyph)<<24)|int(color);
    // A new glyph in the same place (play to pause, mute to volume) pops in on a spring.
    const bool swapped=!initial&&item.key>=0&&(item.key>>24)!=int(glyph)&&iconMotion_;
    if(item.key!=key||item.drawnSize!=size){surface(item.surface,32,32,[&](auto* rt){drawIcon(rt,d2d_.Get(),glyph,16-size/2,16-size/2,size,color);});item.visual->SetContent(item.surface.Get());item.key=key;item.drawnSize=size;}
    if(swapped){item.zoom.reset(.55,now);item.zoom.retarget(1,now,{1,560,22});}
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
    // Glance rings belong to the compact island; expanded pages show battery and timer themselves.
    bool enabled=!s.live&&!s.expanded&&s.settings.uiMode!=0&&s.settings.compactWidth>=260;bool battery=enabled&&s.settings.compactBattery&&(s.settings.glanceRings&1)&&s.battery>=0,timer=enabled&&s.settings.compactTimer&&(s.settings.glanceRings&2);ringsEnabled_=battery||timer;ringCount_=int(battery)+int(timer);ringsEffect_->SetOpacity(ringsEnabled_?1.f:0.f);double now=seconds();bool running=s.focus.running||s.focus.held>0||s.focus.finished;
    double bp=std::clamp(s.battery/100.,0.,1.),tp=s.focus.mode==FocusClock::Mode::Stopwatch?std::fmod(s.focus.elapsed(now),60.)/60.:normalizedProgress(s.focus.displayed(now),s.focus.duration);
    int flags=int(battery)|(int(timer)<<1)|(int(running)<<2)|(int(s.charging)<<3);if(ringBattery_==bp&&ringTimer_==tp&&ringFlags_==flags&&ringColor_==accent&&ringTrack_==track)return;ringBattery_=bp;ringTimer_=tp;ringFlags_=flags;ringColor_=accent;ringTrack_=track;
    surface(ringsSurface_,56,34,[&](auto* rt){if(battery){drawRing(rt,d2d_.Get(),timer?12.f:36.f,17,8.5f,1.8f,bp,accent,track);drawIcon(rt,d2d_.Get(),s.charging?Icon::Power:Icon::Battery,timer?7.f:31.f,12,10,accent);}if(timer){drawRing(rt,d2d_.Get(),36,17,8.5f,1.8f,running?tp:0,accent,track);drawIcon(rt,d2d_.Get(),Icon::Focus,31,12,10,running?accent:muted);}});rings_->SetContent(ringsSurface_.Get());
    surface(dotSurface_,32,32,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillEllipse(D2D1::Ellipse({16,7.5f},1.6f,1.6f),b.Get());});batteryDot_->SetOffsetX((timer?-4.f:20.f)*scale_);batteryDot_->SetContent(dotSurface_.Get());timerDot_->SetContent(dotSurface_.Get());batteryDotEffect_->SetOpacity(battery?1.f:0.f);timerDotEffect_->SetOpacity(timer&&running?1.f:0.f);
    double degrees=tp*360;if(s.focus.mode==FocusClock::Mode::Stopwatch){double base=std::floor(timerAngle.target()/360)*360;degrees+=base;if(degrees<timerAngle.target()-180)degrees+=360;}
    auto aim=[&](Spring& spring,double target){if(s.reducedMotion)spring.reset(target,now);else if(std::abs(spring.target()-target)>.001)spring.retarget(target,now,MotionTokens::ring);};aim(batteryAngle,bp*360);aim(timerAngle,degrees);auto ba=animation(batteryAngle,now),ta=animation(timerAngle,now);batteryRotation_->SetAngle(ba.Get());timerRotation_->SetAngle(ta.Get());
}
}
namespace nexus {
void Renderer::drawPreview(ID2D1RenderTarget* rt,const Artwork& art,float x,float y,float w,float h){if(!art.width||!art.height||art.pixels.size()<size_t(art.width)*art.height*4)return;ComPtr<ID2D1Bitmap> b;auto p=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));if(FAILED(rt->CreateBitmap({art.width,art.height},art.pixels.data(),art.width*4,p,&b)))return;float factor=std::min(w/art.width,h/art.height),dw=art.width*factor,dh=art.height*factor;rt->DrawBitmap(b.Get(),{x+(w-dw)/2,y+(h-dh)/2,x+(w+dw)/2,y+(h+dh)/2},1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);}
void Renderer::ensureWave(){
    if(wave_)return;
    for(auto* v:{std::addressof(wave_),std::addressof(waveBase_),std::addressof(waveFill_)})check(device_->CreateVisual(v->GetAddressOf()));
    check(timeline_->AddVisual(wave_.Get(),FALSE,nullptr));check(wave_->AddVisual(waveBase_.Get(),FALSE,nullptr));check(wave_->AddVisual(waveFill_.Get(),FALSE,nullptr));
    // The playhead stays above the bars.
    check(timeline_->RemoveVisual(seekThumb_.Get()));check(timeline_->AddVisual(seekThumb_.Get(),FALSE,nullptr));
    check(device_->CreateEffectGroup(&waveEffect_));wave_->SetEffect(waveEffect_.Get());check(device_->CreateScaleTransform(&waveScale_));wave_->SetTransform(waveScale_.Get());
    check(device_->CreateRectangleClip(&waveClip_));waveClip_->SetLeft(0.f);waveClip_->SetTop(-16*scale_);waveClip_->SetBottom(16*scale_);waveClip_->SetRight(0.f);waveFill_->SetClip(waveClip_.Get());
    const float pitch=(380-3)/63.f;
    for(size_t i=0;i<waveBars_.size();++i){auto& b=waveBars_[i];for(auto* v:{std::addressof(b.base),std::addressof(b.fill)})check(device_->CreateVisual(v->GetAddressOf()));
        check(device_->CreateScaleTransform(&b.baseScale));check(device_->CreateScaleTransform(&b.fillScale));
        for(auto [v,t]:{std::pair{b.base.Get(),b.baseScale.Get()},std::pair{b.fill.Get(),b.fillScale.Get()}}){t->SetCenterY(10*scale_);v->SetTransform(t);v->SetOffsetX(std::round(float(i)*pitch*scale_));v->SetOffsetY(std::round(-10*scale_));t->SetScaleY(.14f);}
        check(waveBase_->AddVisual(b.base.Get(),FALSE,nullptr));check(waveFill_->AddVisual(b.fill.Get(),FALSE,nullptr));}
}
void Renderer::updateTimeline(const ContentSnapshot& s,UINT32 accent,UINT32 track,UINT32 accent2){
    bool visible=s.expanded&&!s.live&&s.page==Page::Media&&s.playback.duration>0;timelineEffect_->SetOpacity(visible?1.f:0.f);if(!visible)return;
    const bool wave=s.settings.waveTimeline;ensureWave();const bool second=waveSecond_!=accent2;
    if(seekColor_!=accent||second||!seekTrackSurface_||seekStyle_!=int(wave)){seekColor_=accent;seekStyle_=int(wave);
        auto solid=[&](auto& surface_,UINT32 color){surface(surface_,380,4,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(color),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,380,4},2,2),b.Get());});};
        solid(seekTrackSurface_,track);
        // The played part runs from the accent to the artwork's second colour.
        surface(seekFillSurface_,380,4,[&](auto* rt){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(accent)},{1,D2D1::ColorF(accent2)}};ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,2,&c));ComPtr<ID2D1LinearGradientBrush> b;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{380,0}),c.Get(),&b));rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,380,4},2,2),b.Get());});
        // Waveform mode: a slim capsule playhead; line mode: the familiar dot.
        seekThumbSurface_.Reset();if(wave)surface(seekThumbSurface_,4,28,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,4,28},2,2),b.Get());});
        else surface(seekThumbSurface_,16,16,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillEllipse({{8,8},6,6},b.Get());});
        seekThumb_->SetContent(seekThumbSurface_.Get());seekThumbScale_->SetCenterX((wave?2:8)*scale_);seekThumbScale_->SetCenterY((wave?14:8)*scale_);}
    if(waveColors_[0]!=track||waveColors_[1]!=accent||second){waveColors_[0]=track;waveColors_[1]=accent;
        auto bar=[&](auto& target,UINT32 color){surface(target,3,20,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(color),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,3,20},1.5f,1.5f),b.Get());});};
        bar(waveBaseSurface_,track);for(size_t k=0;k<waveFillSurfaces_.size();++k)bar(waveFillSurfaces_[k],mixColor(accent,accent2,double(k)/double(waveFillSurfaces_.size()-1)));
        for(size_t i=0;i<waveBars_.size();++i){auto& b=waveBars_[i];b.base->SetContent(waveBaseSurface_.Get());b.fill->SetContent(waveFillSurfaces_[i*waveFillSurfaces_.size()/waveBars_.size()].Get());}}
    waveSecond_=accent2;
    seekTrack_->SetContent(wave?nullptr:seekTrackSurface_.Get());seekFill_->SetContent(wave?nullptr:seekFillSurface_.Get());waveEffect_->SetOpacity(wave?1.f:0.f);
    double now=seconds();bool emphasis=s.scrub.active||s.hovered==Action::Seek;double target=emphasis?2.5:1;
    if(seekEmphasis_.target()!=target){if(s.reducedMotion)seekEmphasis_.reset(target,now);else seekEmphasis_.retarget(target,now,MotionTokens::icon);}
    // Landing on a detent gives the playhead and track a small tick.
    if(s.detentPulse!=detentSeen_){detentSeen_=s.detentPulse;if(!s.reducedMotion&&s.scrub.active){seekEmphasis_.reset(target+.55,now);seekEmphasis_.retarget(target,now,{1,700,26});}}
    double position=s.scrub.active?s.scrub.value:s.playback.position+(s.playback.playing?std::max(0.,now-s.playback.sampledAt):0.);float fraction=float(normalizedProgress(position,s.playback.duration));
    timeline_->SetOffsetX(20*scale_);timeline_->SetOffsetY(227*scale_);auto grow=animation(seekEmphasis_,now);seekTrackScale_->SetScaleY(grow.Get());seekFillScale_->SetScaleY(grow.Get());
    // The waveform swells a little on hover and while scrubbing.
    auto swell=animation(seekEmphasis_,now,.2f,.8f);waveScale_->SetScaleY(swell.Get());
    const float thumbBias=wave?-2*scale_:-8*scale_;seekFillScale_->SetScaleX(std::max(.0001f,fraction));seekThumb_->SetOffsetX(fraction*380*scale_+thumbBias);seekThumb_->SetOffsetY((wave?-14:-6)*scale_);waveClip_->SetRight(fraction*380*scale_);
    if(s.playback.playing&&!s.scrub.active&&position<s.playback.duration){auto linear=[&](float factor,float bias){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));a->SetAbsoluteBeginTime(ticks(now));a->AddCubic(0,fraction*factor+bias,factor/float(s.playback.duration),0,0);a->End(s.playback.duration-position,factor+bias);return a;};
        auto fill=linear(1,0),marker=linear(380*scale_,thumbBias),reveal=linear(380*scale_,0);seekFillScale_->SetScaleX(fill.Get());seekThumb_->SetOffsetX(marker.Get());waveClip_->SetRight(reveal.Get());}
    auto thumb=wave?animation(seekEmphasis_,now,.12f,.88f):animation(seekEmphasis_,now,.3f,.3f);seekThumbScale_->SetScaleX(wave?1.f:0.f);if(!wave)seekThumbScale_->SetScaleX(thumb.Get());seekThumbScale_->SetScaleY(thumb.Get());
}
void Renderer::absorb(const std::shared_ptr<const Artwork>& preview,float x,float y,float tx,float ty,bool reduced){
    if(reduced)return;surface(dropSurface_,64,64,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(0x20252d,.96f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,64,64},14,14),b.Get());if(preview)drawPreview(rt,*preview,6,6,52,52);else drawIcon(rt,d2d_.Get(),Icon::File,17,17,30,0xb8dfd1);});dropGhost_->SetContent(dropSurface_.Get());double now=seconds();Spring sx{x-32},sy{y-32},zoom{1},opacity{1};sx.reset(x-32,now);sy.reset(y-32,now);zoom.reset(1,now);opacity.reset(1,now);sx.retarget(tx-12,now,MotionTokens::drop);sy.retarget(ty-12,now,MotionTokens::drop);zoom.retarget(.375,now,MotionTokens::drop);opacity.retarget(0,now,MotionTokens::dropFade);auto ax=animation(sx,now,scale_),ay=animation(sy,now,scale_),z=animation(zoom,now),fade=animation(opacity,now);dropGhost_->SetOffsetX(ax.Get());dropGhost_->SetOffsetY(ay.Get());dropScale_->SetScaleX(z.Get());dropScale_->SetScaleY(z.Get());dropEffect_->SetOpacity(fade.Get());commit();
}
void Renderer::routeConfirmed(bool reduced,Action selected){if(reduced)return;double now=seconds();for(auto& i:icons_)if(i.used&&(i.action==Action::Audio||i.action==selected)){i.zoom.reset(1.18,now);i.zoom.retarget(1,now,MotionTokens::icon);auto z=animation(i.zoom,now);i.scale->SetScaleX(z.Get());i.scale->SetScaleY(z.Get());}commit();}
}

namespace nexus {
void Renderer::updateAtmosphere(const ContentSnapshot& s){
    // The glow takes the picture's overall tone (more saturated than the accent, so a little stronger).
    bool enabled=s.settings.albumAccents&&bool(s.playback.artwork);const bool ambient=enabled&&s.playback.artwork->ambient;uint32_t color=enabled?(ambient?s.playback.artwork->ambient:s.playback.artwork->accent):0;double now=seconds();
    for(int i=0;i<3;++i){double target=enabled?double((color>>((2-i)*8))&255)/255.*(s.light?.09:.13)*(ambient?1.25:1):0;auto& spring=atmosphereColor_[i];if(std::abs(spring.target()-target)>.0001){if(s.reducedMotion)spring.reset(target,now);else spring.retarget(target,now,MotionTokens::atmosphere);auto a=animation(spring,now);atmosphereEffect_[i]->SetOpacity(a.Get());}}
}
void Renderer::updatePeek(const ContentSnapshot& s){
    int index=int(s.hovered)-int(Action::ShelfItemBase);bool visible=s.expanded&&!s.live&&s.page==Page::Shelf&&s.settings.shelfPeek&&index>=0&&size_t(index)<s.shelf.size()&&bool(s.shelf[index].preview);
    double now=seconds();if(visible&&peekArtwork_!=s.shelf[index].preview){peekArtwork_=s.shelf[index].preview;surface(peekSurface_,146,132,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(s.light?0xffffff:0x20252d,.99f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,146,132},15,15),b.Get());drawPreview(rt,*peekArtwork_,9,9,128,114);b->SetColor(D2D1::ColorF(s.light?0x20252d:0xffffff,.14f));rt->DrawRoundedRectangle(D2D1::RoundedRect({.5f,.5f,145.5f,131.5f},15,15),b.Get(),1/scale_);});peek_->SetContent(peekSurface_.Get());}
    if(peekOpacity_.target()!=(visible?1:0)){if(s.reducedMotion){peekOpacity_.reset(visible?1:0,now);peekZoom_.reset(1,now);}else{peekOpacity_.retarget(visible?1:0,now,MotionTokens::peek);peekZoom_.retarget(visible?1:.88,now,MotionTokens::peek);}auto opacity=animation(peekOpacity_,now),zoom=animation(peekZoom_,now);peekEffect_->SetOpacity(opacity.Get());peekScale_->SetScaleX(zoom.Get());peekScale_->SetScaleY(zoom.Get());}
}
}
