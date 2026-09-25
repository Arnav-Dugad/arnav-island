#include "Renderer.h"
namespace nexus {
namespace {
ComPtr<IDCompositionAnimation> glideAnimation(IDCompositionDevice* device,const Glide& g,double now,float factor,float bias){
    ComPtr<IDCompositionAnimation> a;check(device->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(g.t0)));auto c=g.segment();
    if(g.span>0){check(a->AddCubic(0,float(c.p)*factor+bias,float(c.v)*factor,float(c.quadratic)*factor,float(c.cubic)*factor));check(a->End(g.span,float(g.p1)*factor+bias));}
    else{check(a->AddCubic(0,float(g.p1)*factor+bias,0,0,0));check(a->End(0,float(g.p1)*factor+bias));}
    (void)now;return a;
}
}
void Renderer::updateSpectrumLayout(const ContentSnapshot& s,UINT32 accent){
    int mode=-1;const bool playing=s.settings.waveform&&s.playback.playing&&s.waveform&&s.hud==0;
    if(playing){if(!s.expanded){if(edge_==0&&s.settings.compactMedia)mode=s.settings.uiMode==0?1:s.settings.waveformStyle==1?-1:0;}else if(s.card)mode=-1;else if(s.live)mode=2;else if(s.page==Page::Media&&!lyricsPanel(s))mode=3;}// lyric lines take the bars' place
    if(barColor_!=accent||!spectrumSurface_){barColor_=accent;surface(spectrumSurface_,3,20,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,3,20},1.5f,1.5f),b.Get());});}
    int count=mode==3?16:mode==1?4:mode>=0?5:barCount_;float pitch=mode==3?6.f:mode==1?5.f:5.4f;float span=(count-1)*pitch+3;
    float inset=mode==1?12:mode==2?22:(ringsEnabled_&&!s.expanded?(ringCount_==2?66.f:42.f):14.f);
    double now=seconds();
    if(mode!=barMode_||count!=barCount_){
        for(size_t i=0;i<bars_.size();++i){auto& bar=bars_[i];bool used=int(i)<count&&mode>=0;bar.visual->SetContent(used?spectrumSurface_.Get():nullptr);bar.visual->SetOffsetX(std::round(i*pitch*scale_));bar.visual->SetOffsetY(0.f);bar.glide.to(.15,now,0);auto a=glideAnimation(device_.Get(),bar.glide,now,1,0);bar.scale->SetScaleY(a.Get());}
        barMode_=mode;barCount_=count;
    }else if(mode>=0)for(int i=0;i<count;++i)bars_[i].visual->SetContent(spectrumSurface_.Get());
    barInset_=inset+span;
    float top=mode==3?136.f:mode==2?21.f:7.f;spectrum_->SetOffsetY(std::round(top*scale_));if(mode==3)spectrum_->SetOffsetX(std::round((20+380-span)*scale_));
    double target=mode>=0?1:0;if(std::abs(spectrumOpacity_.target()-target)>.001){if(s.reducedMotion)spectrumOpacity_.reset(target,now);else spectrumOpacity_.retarget(target,now,MotionTokens::artworkOpacity);auto o=animation(spectrumOpacity_,now);spectrumEffect_->SetOpacity(o.Get());}
}
// The spectrum ring around the compact cover (or app logo), and the cover turning round while it shows.
void Renderer::updateRing(const ContentSnapshot& s,UINT32 accent){
    constexpr float r0=12.5f,tick=4;
    if(!ring_){check(device_->CreateVisual(&ring_));check(device_->CreateEffectGroup(&ringEffect_));ring_->SetEffect(ringEffect_.Get());ringEffect_->SetOpacity(0.f);check(body_->AddVisual(ring_.Get(),TRUE,artFrame_.Get()));
        ring_->SetOffsetX(std::round(23*scale_));ring_->SetOffsetY(std::round(17*scale_));
        for(size_t k=0;k<ringTicks_.size();++k){auto& t=ringTicks_[k];check(device_->CreateVisual(&t.visual));check(device_->CreateScaleTransform(&t.scale));check(device_->CreateRotateTransform(&t.rotate));
            t.scale->SetCenterX(1*scale_);t.scale->SetCenterY(tick*scale_);t.rotate->SetCenterX(1*scale_);t.rotate->SetCenterY((r0+tick)*scale_);t.rotate->SetAngle(float(k)*15.f);
            IDCompositionTransform* chain[]={t.scale.Get(),t.rotate.Get()};ComPtr<IDCompositionTransform> group;check(device_->CreateTransformGroup(chain,2,&group));t.visual->SetTransform(group.Get());
            t.visual->SetOffsetX(-1*scale_);t.visual->SetOffsetY(-(r0+tick)*scale_);t.scale->SetScaleY(.22f);check(ring_->AddVisual(t.visual.Get(),FALSE,nullptr));}}
    const bool mark=bool(s.playback.artwork)||(s.settings.appIcons&&s.playback.available);
    const bool on=!s.expanded&&edge_==0&&s.settings.uiMode!=0&&s.settings.compactMedia&&s.settings.waveformStyle==1&&s.settings.waveform&&s.playback.playing&&s.waveform&&s.hud==0&&mark;
    if(spectrumRingColor_!=accent||!ringSurface_){spectrumRingColor_=accent;surface(ringSurface_,2,4,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,2,4},1,1),b.Get());});for(auto& t:ringTicks_)t.visual->SetContent(ringOn_?ringSurface_.Get():nullptr);}
    const double now=seconds();
    // Off, the ticks hold no picture at all, so nothing of the ring can linger on screen.
    if(on!=ringOn_){ringOn_=on;for(auto& t:ringTicks_)t.visual->SetContent(on?ringSurface_.Get():nullptr);if(!on)for(auto& t:ringTicks_){t.glide.to(.22,now,0);auto a=glideAnimation(device_.Get(),t.glide,now,1,0);t.scale->SetScaleY(a.Get());}}
    // The cover is round inside the ring (its clip radius is in the 256-DIP cover's own units).
    const float round=on&&s.playback.artwork?128.f:32.f;
    if(round!=artRound_){const float from=artRound_<0?round:artRound_;artRound_=round;auto* c=artClip_.Get();
        if(s.reducedMotion||from==round){const float r=round*scale_;c->SetTopLeftRadiusX(r);c->SetTopLeftRadiusY(r);c->SetTopRightRadiusX(r);c->SetTopRightRadiusY(r);c->SetBottomLeftRadiusX(r);c->SetBottomLeftRadiusY(r);c->SetBottomRightRadiusX(r);c->SetBottomRightRadiusY(r);}
        else{auto a=ease(now,from*scale_,round*scale_,.32);c->SetTopLeftRadiusX(a.Get());c->SetTopLeftRadiusY(a.Get());c->SetTopRightRadiusX(a.Get());c->SetTopRightRadiusY(a.Get());c->SetBottomLeftRadiusX(a.Get());c->SetBottomLeftRadiusY(a.Get());c->SetBottomRightRadiusX(a.Get());c->SetBottomRightRadiusY(a.Get());}}
}
void Renderer::trackSkip(int direction,bool reduced){
    if(reduced)return;const double now=seconds();kick_.reset(direction>0?-16:16,now);kick_.retarget(0,now,{1,380,24});auto a=animation(kick_,now,scale_);headerKick_->SetOffsetX(a.Get());commit();
}
void Renderer::waveform(const std::array<float,64>& heights,bool reduced){
    if(!wave_)return;const double now=seconds();bool changed=false;
    for(size_t i=0;i<waveBars_.size();++i){auto& b=waveBars_[i];const float h=heights[i];
        // Unheard stretches are short dots; heard ones rise with their loudness.
        const float target=h<0?.14f:.24f+.76f*std::clamp(h,0.f,1.f);if(std::abs(target-b.target)<.004f)continue;b.target=target;changed=true;
        b.glide.to(target,now,reduced?0:.16);auto a=glideAnimation(device_.Get(),b.glide,now,1,0);b.baseScale->SetScaleY(a.Get());b.fillScale->SetScaleY(a.Get());}
    if(changed)commit();
}
// The cover swells with the bass: the lowest bands (50 to 130 Hz, kick drums and bass lines)
// against their own average over the last half second, so a steady bass leaves it at rest
// and only a hit swells it, by up to 4%.
void Renderer::beat(const SpectrumFrame& f){
    const double now=seconds(),dt=std::clamp(now-beatAt_,0.,.2);beatAt_=now;float bass=0;for(int b=0;b<4;++b)bass=std::max(bass,f.bands[size_t(b)]);
    bassAverage_+=float((bass-bassAverage_)*(1-std::exp(-dt/.45)));if(!artPulseOn_)return;
    const float hit=f.resting?0.f:std::clamp((bass-bassAverage_)*4.f,0.f,1.f);
    artBeat_.to(1+.04*hit,now,.07);auto a=glideAnimation(device_.Get(),artBeat_,now,1,0);artPulse_->SetScaleX(a.Get());artPulse_->SetScaleY(a.Get());
}
// Off (paused, no cover, reduced motion): the cover settles back to its size.
void Renderer::setArtPulse(bool on){
    if(on==artPulseOn_)return;artPulseOn_=on;if(on)return;const double now=seconds();artBeat_.to(1,now,.12);auto a=glideAnimation(device_.Get(),artBeat_,now,1,0);artPulse_->SetScaleX(a.Get());artPulse_->SetScaleY(a.Get());
}
void Renderer::spectrum(const SpectrumFrame& frame){
    beat(frame);
    // The ring: tick k (clockwise from the top) shows band 2j, where j is its distance from the top,
    // so the bass lifts the crown and the treble the bottom, the same on both sides.
    if(ringOn_){const double now=seconds();for(size_t k=0;k<ringTicks_.size();++k){auto& t=ringTicks_[k];const size_t j=k<=12?k:24-k;const float v=frame.resting?0.f:std::pow(std::clamp(frame.bands[std::min<size_t>(Spectrum::bandCount-1,j*2)],0.f,1.f),.85f);
        t.glide.to(.22+.78*v,now,.06);auto a=glideAnimation(device_.Get(),t.glide,now,1,0);t.scale->SetScaleY(a.Get());}}
    if(barMode_<0||barCount_<=0){if(artPulseOn_||ringOn_)commit();return;}double now=seconds();const float lo=.15f,hi=barMode_==3?1.f:barMode_==1?.62f:.78f;
    for(int i=0;i<barCount_;++i){int a=i*Spectrum::bandCount/barCount_,b=std::max(a+1,(i+1)*Spectrum::bandCount/barCount_);float v=0;for(int k=a;k<b;++k)v=std::max(v,frame.bands[k]);
        if(barMode_!=3)v=std::pow(v,.8f);float target=frame.resting?lo:lo+(hi-lo)*v;auto& bar=bars_[i];bar.glide.to(target,now,.055);auto anim=glideAnimation(device_.Get(),bar.glide,now,1,0);bar.scale->SetScaleY(anim.Get());}
    commit();
}
void Renderer::meters(const std::vector<MixerEntry>& entries,int offset,bool visible){
    double now=seconds();int rows=visible?std::min(4,std::max(0,int(entries.size())-offset)):0;
    for(int i=0;i<4;++i){auto& m=meters_[i];bool used=i<rows;m.visual->SetContent(used?meterSurface_.Get():nullptr);if(!used)continue;auto& e=entries[offset+i];double peak=e.muted?0:std::clamp(double(e.peak)*e.volume,0.,1.);m.glide.to(std::max(.002,peak),now,.06);auto a=glideAnimation(device_.Get(),m.glide,now,1,0);m.scale->SetScaleX(a.Get());}
    meterEffect_->SetOpacity(rows?1.f:0.f);meterRows_=rows;commit();
}
void Renderer::updateHud(const ContentSnapshot& s,UINT32 accent,UINT32 track){
    bool visible=s.hud!=0&&!s.expanded&&edge_==0;double now=seconds();
    if(!hudTrackSurface_||meterColor_!=accent){meterColor_=accent;surface(hudTrackSurface_,150,4,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(track),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,150,4},2,2),b.Get());});
        surface(hudFillSurface_,150,4,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(accent),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,150,4},2,2),b.Get());});hudTrack_->SetContent(hudTrackSurface_.Get());hudFill_->SetContent(hudFillSurface_.Get());
        surface(meterSurface_,174,2,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(accent,.8f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,174,2},1,1),b.Get());});}
    double target=visible?1:0;if(std::abs(hudOpacity_.target()-target)>.001){if(s.reducedMotion)hudOpacity_.reset(target,now);else hudOpacity_.retarget(target,now,visible?MotionTokens::artworkOpacity:SpringSpec{1,700,60});auto o=animation(hudOpacity_,now);hudEffect_->SetOpacity(o.Get());}
}
void Renderer::updateBadge(const ContentSnapshot& s,UINT32 bg){
    const auto& p=s.playback;// Without artwork the logo fills the artwork square instead, so no corner badge.
    bool visible=s.settings.appIcons&&s.expanded&&!s.card&&p.available&&p.artwork&&(s.live||s.page==Page::Media||s.page==Page::Overview);
    if(visible&&(badgeIcon_!=p.appIcon||badgeService_!=int(std::hash<std::string>{}(p.service)&0x7fffffff)||badgeLight_!=s.light)){badgeIcon_=p.appIcon;badgeService_=int(std::hash<std::string>{}(p.service)&0x7fffffff);badgeLight_=s.light;
        surface(badgeSurface_,22,22,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(bg),&b);rt->FillEllipse(D2D1::Ellipse({11,11},11,11),b.Get());
            identity(rt,p,4,4,14,0x3a3d45);});badge_->SetContent(badgeSurface_.Get());}
    double now=seconds(),target=visible?1:0;if(std::abs(badgeOpacity_.target()-target)>.001){if(s.reducedMotion)badgeOpacity_.reset(target,now);else badgeOpacity_.retarget(target,now,MotionTokens::artworkOpacity);auto o=animation(badgeOpacity_,now);badgeEffect_->SetOpacity(o.Get());}
}
}
