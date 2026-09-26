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
    if(playing){if(!s.expanded){if(edge_==0&&s.settings.compactMedia)mode=s.settings.uiMode==0?1:s.settings.waveformStyle==1?-1:0;}else if(s.card)mode=-1;else if(s.live)mode=2;else if(s.mediaPage()&&!lyricsPanel(s))mode=3;}// lyric lines take the bars' place
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
    // Phase 5G, Island DJ: the new cover's colours bloom behind the ring as a track starts; through a track's last ten
    // seconds a halo breathes in the colours of what plays next (the island's own queue knows; otherwise what plays now).
    {if(!djBloom_.visual)for(auto* h:{&djBloom_,&djEnd_}){check(device_->CreateVisual(&h->visual));check(device_->CreateEffectGroup(&h->effect));check(device_->CreateScaleTransform(&h->scale));
            h->scale->SetCenterX(28*scale_);h->scale->SetCenterY(28*scale_);h->visual->SetTransform(h->scale.Get());h->visual->SetEffect(h->effect.Get());h->effect->SetOpacity(0.f);
            h->visual->SetOffsetX(-28*scale_);h->visual->SetOffsetY(-28*scale_);check(ring_->AddVisual(h->visual.Get(),TRUE,nullptr));}
        const bool dj=on&&s.settings.islandDj&&!s.reducedMotion;const auto& p=s.playback;
        if(p.title!=djTitle_){const bool bloom=!djTitle_.empty();djTitle_=p.title;
            if(bloom&&dj){halo(djBloom_,accent);auto o=ease(now,.9f,0.f,1.3);djBloom_.effect->SetOpacity(o.Get());auto k=ease(now,.8f,1.32f,1.3);djBloom_.scale->SetScaleX(k.Get());djBloom_.scale->SetScaleY(k.Get());}}
        if(!dj)djBloom_.effect->SetOpacity(0.f);
        const UINT32 next=s.djAccent?s.djAccent:accent;const double remaining=p.duration>0?p.duration-(p.position+(p.playing?now-p.sampledAt:0)):-1;
        wchar_t key[96];swprintf(key,96,L"%d|%d|%.1f|%06x",int(dj),int(p.playing),p.playing&&remaining>=0?now+remaining:-1.,unsigned(next));
        if(key!=djKey_){djKey_=key;
            if(!dj||!p.playing||p.duration<30||remaining<3)djEnd_.effect->SetOpacity(0.f);
            else{halo(djEnd_,next);djEnd_.scale->SetScaleX(1.f);djEnd_.scale->SetScaleY(1.f);
                // Held dark until ten seconds are left, then in and out every 1.8 s until the track ends.
                const double t0=std::max(.05,remaining-10),h=.9;ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now)));
                auto smooth=[&](double at,double p0,double p1){const double d=p1-p0;check(a->AddCubic(at,float(p0),0,float(3*d/(h*h)),float(-2*d/(h*h*h))));};
                check(a->AddCubic(0,0,0,0,0));smooth(t0,0,.85);smooth(t0+h,.85,.25);smooth(t0+2*h,.25,.85);check(a->AddRepeat(t0+3*h,2*h));check(a->End(std::max(t0+3*h+.1,remaining+.2),0));
                djEnd_.effect->SetOpacity(a.Get());}}}
    // The cover is round inside the ring (its clip radius is in the 256-DIP cover's own units).
    const float round=on&&s.playback.artwork?128.f:32.f;
    if(round!=artRound_){const float from=artRound_<0?round:artRound_;artRound_=round;auto* c=artClip_.Get();
        if(s.reducedMotion||from==round){const float r=round*scale_;c->SetTopLeftRadiusX(r);c->SetTopLeftRadiusY(r);c->SetTopRightRadiusX(r);c->SetTopRightRadiusY(r);c->SetBottomLeftRadiusX(r);c->SetBottomLeftRadiusY(r);c->SetBottomRightRadiusX(r);c->SetBottomRightRadiusY(r);}
        else{auto a=ease(now,from*scale_,round*scale_,.32);c->SetTopLeftRadiusX(a.Get());c->SetTopLeftRadiusY(a.Get());c->SetTopRightRadiusX(a.Get());c->SetTopRightRadiusY(a.Get());c->SetBottomLeftRadiusX(a.Get());c->SetBottomLeftRadiusY(a.Get());c->SetBottomRightRadiusX(a.Get());c->SetBottomRightRadiusY(a.Get());}}
}
// A soft halo around the ring, in one colour: clear inside the ticks, brightest just outside them, fading out.
void Renderer::halo(Halo& h,UINT32 colour){
    if(h.surface&&h.colour==colour)return;h.colour=colour;
    surface(h.surface,56,56,[&](ID2D1RenderTarget* rt){rt->Clear(D2D1::ColorF(0,0));
        D2D1_GRADIENT_STOP stops[]={{0.f,D2D1::ColorF(colour,0.f)},{.44f,D2D1::ColorF(colour,0.f)},{.58f,D2D1::ColorF(colour,.5f)},{.74f,D2D1::ColorF(colour,.2f)},{1.f,D2D1::ColorF(colour,0.f)}};
        ComPtr<ID2D1GradientStopCollection> collection;check(rt->CreateGradientStopCollection(stops,5,&collection));
        ComPtr<ID2D1RadialGradientBrush> brush;check(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({28,28},{0,0},28,28),collection.Get(),&brush));
        rt->FillEllipse(D2D1::Ellipse({28,28},28,28),brush.Get());});
    h.visual->SetContent(h.surface.Get());
}
void Renderer::trackSkip(int direction,bool reduced){
    // After a drag the header springs home from where the drag left it; otherwise it kicks the way the swipe went.
    if(reduced){swipeOffset_=0;headerKick_->SetOffsetX(0.f);commit();return;}
    const double now=seconds();kick_.reset(swipeOffset_!=0?swipeOffset_:direction>0?-16:16,now);swipeOffset_=0;kick_.retarget(0,now,{1,380,24});auto a=animation(kick_,now,scale_);headerKick_->SetOffsetX(a.Get());commit();
}
void Renderer::swipeFollow(float dx,float bodyWidth,float bodyHeight,bool header,bool reduced){
    if(!swipeHint_){check(device_->CreateVisual(&swipeHint_));check(device_->CreateEffectGroup(&swipeEffect_));check(swipeHint_->SetEffect(swipeEffect_.Get()));
        check(device_->CreateScaleTransform(&swipeScale_));swipeScale_->SetCenterX(15*scale_);swipeScale_->SetCenterY(15*scale_);check(swipeHint_->SetTransform(swipeScale_.Get()));
        swipeEffect_->SetOpacity(0.f);check(body_->AddVisual(swipeHint_.Get(),TRUE,nullptr));}
    // The chip: a soft disc with the arrow in the accent (redrawn when the colours change).
    const UINT32 key=swipeInk_^(swipeAccent_*2654435761u);
    if(key!=swipeDrawn_||!swipeNext_){swipeDrawn_=key;
        for(auto [surface,glyph]:{std::pair{std::addressof(swipeNext_),Icon::Next},std::pair{std::addressof(swipePrevious_),Icon::Previous}})
            this->surface(*surface,30,30,[&](ID2D1RenderTarget* rt){rt->Clear(D2D1::ColorF(0,0));ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(swipeInk_,.16f),&b));
                rt->FillEllipse(D2D1::Ellipse({15,15},14.5f,14.5f),b.Get());b->SetColor(D2D1::ColorF(swipeInk_,.22f));rt->DrawEllipse(D2D1::Ellipse({15,15},14,14),b.Get(),1);
                drawIcon(rt,d2d_.Get(),glyph,7,7,16,swipeAccent_?swipeAccent_:swipeInk_);});}
    // Dragging left brings the next track (the chip on the right), dragging right the previous one.
    const int side=dx<0?1:dx>0?-1:0;const float progress=std::min(1.f,std::abs(dx)/44.f);
    if(side&&side!=swipeSide_){swipeSide_=side;check(swipeHint_->SetContent((side>0?swipeNext_:swipePrevious_).Get()));}
    swipeX_=side>0?bodyWidth-38+(1-progress)*8:8-(1-progress)*8;swipeY_=std::round(bodyHeight/2-15);
    check(swipeHint_->SetOffsetX(std::round(swipeX_*scale_)));check(swipeHint_->SetOffsetY(swipeY_*scale_));
    const bool armed=progress>=1;const double now=seconds();swipeProgress_=progress;
    swipeEffect_->SetOpacity(std::pow(progress,1.4f));
    if(armed!=swipeArmed_&&!reduced){auto a=ease(now,armed?.92f:1.12f,armed?1.12f:.92f,.16);swipeScale_->SetScaleX(a.Get());swipeScale_->SetScaleY(a.Get());}
    else if(!armed){const float k=.62f+.3f*progress;swipeScale_->SetScaleX(k);swipeScale_->SetScaleY(k);}
    swipeArmed_=armed;
    // The header follows the finger, with resistance (the Live Island and pages slide their content instead).
    swipeOffset_=header?std::clamp(dx*.42f,-26.f,26.f):0;headerKick_->SetOffsetX(swipeOffset_*scale_);commit();
}
void Renderer::swipeEnd(bool skipped,bool reduced){
    if(!swipeHint_)return;const double now=seconds();
    // Skipping sends the chip off with a last swell; otherwise it shrinks back as it fades.
    if(reduced)swipeEffect_->SetOpacity(0.f);
    else{auto fade=ease(now,std::pow(swipeProgress_,1.4f),0.f,skipped?.3:.2);swipeEffect_->SetOpacity(fade.Get());
        auto size=ease(now,swipeArmed_?1.12f:.8f,skipped?1.4f:.6f,skipped?.3:.2);swipeScale_->SetScaleX(size.Get());swipeScale_->SetScaleY(size.Get());}
    swipeArmed_=false;swipeSide_=0;swipeProgress_=0;
    if(!skipped&&swipeOffset_!=0){if(reduced)headerKick_->SetOffsetX(0.f);else{kick_.reset(swipeOffset_,now);kick_.retarget(0,now,{1,380,26});auto a=animation(kick_,now,scale_);headerKick_->SetOffsetX(a.Get());}swipeOffset_=0;}
    commit();
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
    bassAverage_+=float((bass-bassAverage_)*(1-std::exp(-dt/.45)));
    const float hit=f.resting?0.f:std::clamp((bass-bassAverage_)*4.f,0.f,1.f);
    if(beatEdgeOn_)edgeBeat(f.resting?-1.f:hit,now);
    if(!artPulseOn_)return;
    artBeat_.to(1+.04*hit,now,.07);auto a=glideAnimation(device_.Get(),artBeat_,now,1,0);artPulse_->SetScaleX(a.Get());artPulse_->SetScaleY(a.Get());
}
// The edge light rests brighter the more weight the bass carries (its half-second average), so it breathes with the
// music; each hit flares it within 70 ms and it falls back over about half a second. hit < 0: silence, it settles low.
void Renderer::edgeBeat(float hit,double now){
    const double rest=hit<0?.08:.14+.34*std::clamp(double(bassAverage_)*1.4,0.,1.),current=edgeBeat_.sample(now).position;
    if(hit>.12f){const double peak=std::min(1.,rest+.2+.62*double(hit));if(peak>current+.05){edgeBeat_.to(peak,now,.07);edgeAttack_=now+.07;edgeLight(now);return;}}
    if(now>=edgeAttack_&&std::abs(edgeBeat_.p1-rest)>.03){edgeBeat_.to(rest,now,.55);edgeLight(now);}
}
// Hands the level's glide, from where it is now, to whichever material shows it.
void Renderer::edgeLight(double now){
    if(beatMaterial_==1){const auto c=edgeBeat_.sample(now);glass_.beat(c.position,c.velocity,edgeBeat_.p1,std::max(0.,edgeBeat_.t0+edgeBeat_.span-now),beatColor_);}
    else if(beatLightEffect_){auto a=glideAnimation(device_.Get(),edgeBeat_,now,1,0);beatLightEffect_->SetOpacity(a.Get());}
}
// On while music plays (and the setting allows): the light wakes with the next beat. Off: it fades out.
void Renderer::setBeatEdge(bool on,UINT32 color,bool glass,bool reduced){
    const double now=seconds();const int material=glass?1:0;
    // Moving between glass and solid: the one left behind goes dark at once.
    if(beatMaterial_!=material){if(beatMaterial_==1)glass_.beat(0,0,0,0,beatColor_);else if(beatMaterial_==0)beatLightEffect_->SetOpacity(0.f);beatMaterial_=material;if(beatEdgeOn_)edgeLight(now);}
    if(!glass){
        // The solid strips: the accent at the edge, gone a third of the way in; only on edges away from the screen.
        const bool free[4]={!(attached_&&edge_==2),!(attached_&&edge_==1),!(attached_&&edge_==0),true};const int key=int(color&0xffffff)^(edge_<<24)^(int(attached_)<<27);
        if(key!=beatStripKey_){if(beatSurfaceColor_!=color||!beatSurfaces_[0]){beatSurfaceColor_=color;for(int k=0;k<4;++k){const bool vertical=k<2;const int w=vertical?int(beatBand):int(canvasWidth),h=vertical?int(canvasHeight):int(beatBand);
                    surface(beatSurfaces_[size_t(k)],w,h,[&](auto* rt){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(color,.62f)},{.35f,D2D1::ColorF(color,.2f)},{1,D2D1::ColorF(color,0.f)}};ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,3,&c));
                        const D2D1_POINT_2F from=k==0?D2D1::Point2F(0,0):k==1?D2D1::Point2F(beatBand,0):k==2?D2D1::Point2F(0,0):D2D1::Point2F(0,beatBand),to=k==0?D2D1::Point2F(beatBand,0):k==1?D2D1::Point2F(0,0):k==2?D2D1::Point2F(0,beatBand):D2D1::Point2F(0,0);
                        ComPtr<ID2D1LinearGradientBrush> b;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(from,to),c.Get(),&b));rt->FillRectangle({0,0,float(w),float(h)},b.Get());});}}
            beatStripKey_=key;for(size_t k=0;k<4;++k)beatStrips_[k]->SetContent(free[k]?beatSurfaces_[k].Get():nullptr);}}
    if(color!=beatColor_){beatColor_=color;if(beatEdgeOn_&&glass)edgeLight(now);}
    if(on==beatEdgeOn_)return;beatEdgeOn_=on;
    if(!on){edgeBeat_.to(0,now,reduced?.01:.6);edgeLight(now);}
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
    if(barMode_<0||barCount_<=0){if(artPulseOn_||ringOn_||beatEdgeOn_)commit();return;}double now=seconds();const float lo=.15f,hi=barMode_==3?1.f:barMode_==1?.62f:.78f;
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
    bool visible=s.settings.appIcons&&s.expanded&&!s.card&&p.available&&p.artwork&&(s.live||s.mediaPage()||s.page==Page::Overview);
    if(visible&&(badgeIcon_!=p.appIcon||badgeService_!=int(std::hash<std::string>{}(p.service)&0x7fffffff)||badgeLight_!=s.light)){badgeIcon_=p.appIcon;badgeService_=int(std::hash<std::string>{}(p.service)&0x7fffffff);badgeLight_=s.light;
        surface(badgeSurface_,22,22,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(bg),&b);rt->FillEllipse(D2D1::Ellipse({11,11},11,11),b.Get());
            identity(rt,p,4,4,14,0x3a3d45);});badge_->SetContent(badgeSurface_.Get());}
    double now=seconds(),target=visible?1:0;if(std::abs(badgeOpacity_.target()-target)>.001){if(s.reducedMotion)badgeOpacity_.reset(target,now);else badgeOpacity_.retarget(target,now,MotionTokens::artworkOpacity);auto o=animation(badgeOpacity_,now);badgeEffect_->SetOpacity(o.Get());}
}
}
