#include "Renderer.h"
namespace nexus {
namespace {
// Geometric service marks drawn at runtime to identify a browser tab's service.
void drawService(ID2D1RenderTarget* rt,ID2D1Factory* factory,MediaService service,float x,float y,float size){
    ComPtr<ID2D1SolidColorBrush> red,white;rt->CreateSolidColorBrush(D2D1::ColorF(0xff0033),&red);rt->CreateSolidColorBrush(D2D1::ColorF(0xffffff),&white);
    auto triangle=[&](float cx,float cy,float r){ComPtr<ID2D1PathGeometry> g;factory->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> sink;g->Open(&sink);sink->BeginFigure({cx-r*.55f,cy-r*.75f},D2D1_FIGURE_BEGIN_FILLED);sink->AddLine({cx+r*.8f,cy});sink->AddLine({cx-r*.55f,cy+r*.75f});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();rt->FillGeometry(g.Get(),white.Get());};
    if(service==MediaService::YouTube){float h=size*.7f;rt->FillRoundedRectangle(D2D1::RoundedRect({x,y+(size-h)/2,x+size,y+(size+h)/2},size*.2f,size*.2f),red.Get());triangle(x+size/2,y+size/2,size*.2f);}
    else{rt->FillEllipse(D2D1::Ellipse({x+size/2,y+size/2},size/2,size/2),red.Get());rt->DrawEllipse(D2D1::Ellipse({x+size/2,y+size/2},size*.29f,size*.29f),white.Get(),size*.07f);triangle(x+size/2,y+size/2,size*.16f);}
}
ComPtr<IDCompositionAnimation> glideAnimation(IDCompositionDevice* device,const Glide& g,double now,float factor,float bias){
    ComPtr<IDCompositionAnimation> a;check(device->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(g.t0)));auto c=g.segment();
    if(g.span>0){check(a->AddCubic(0,float(c.p)*factor+bias,float(c.v)*factor,float(c.quadratic)*factor,float(c.cubic)*factor));check(a->End(g.span,float(g.p1)*factor+bias));}
    else{check(a->AddCubic(0,float(g.p1)*factor+bias,0,0,0));check(a->End(0,float(g.p1)*factor+bias));}
    (void)now;return a;
}
}
void Renderer::updateSpectrumLayout(const ContentSnapshot& s,UINT32 accent){
    int mode=-1;const bool playing=s.settings.waveform&&s.playback.playing&&s.waveform&&s.hud==0;
    if(playing){if(!s.expanded){if(edge_==0&&s.settings.compactMedia)mode=s.settings.uiMode==0?1:0;}else if(s.live)mode=2;else if(s.page==Page::Media)mode=3;}
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
void Renderer::spectrum(const SpectrumFrame& frame){
    if(barMode_<0||barCount_<=0)return;double now=seconds();const float lo=.15f,hi=barMode_==3?1.f:barMode_==1?.62f:.78f;
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
    const auto& p=s.playback;bool visible=s.settings.appIcons&&s.expanded&&p.available&&(s.live||s.page==Page::Media||s.page==Page::Overview)&&(p.appIcon||p.service!=MediaService::None);
    if(visible&&(badgeIcon_!=p.appIcon||badgeService_!=int(p.service)||badgeLight_!=s.light)){badgeIcon_=p.appIcon;badgeService_=int(p.service);badgeLight_=s.light;
        surface(badgeSurface_,22,22,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(bg),&b);rt->FillEllipse(D2D1::Ellipse({11,11},11,11),b.Get());
            if(p.service!=MediaService::None)drawService(rt,d2d_.Get(),p.service,4,4,14);else if(p.appIcon)drawPreview(rt,*p.appIcon,4,4,14,14);});badge_->SetContent(badgeSurface_.Get());}
    double now=seconds(),target=visible?1:0;if(std::abs(badgeOpacity_.target()-target)>.001){if(s.reducedMotion)badgeOpacity_.reset(target,now);else badgeOpacity_.retarget(target,now,MotionTokens::artworkOpacity);auto o=animation(badgeOpacity_,now);badgeEffect_->SetOpacity(o.Get());}
}
}
