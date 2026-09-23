#include "Renderer.h"
namespace nexus {
namespace {
Icon kindIcon(DeviceKind k){switch(k){case DeviceKind::Headphones:return Icon::Audio;case DeviceKind::Earbuds:return Icon::Earbuds;case DeviceKind::Speaker:return Icon::Speaker;case DeviceKind::Phone:return Icon::Phone;case DeviceKind::Keyboard:return Icon::Keyboard;case DeviceKind::Mouse:return Icon::Mouse;case DeviceKind::Gamepad:return Icon::Gamepad;case DeviceKind::Watch:return Icon::Watch;case DeviceKind::Computer:return Icon::Stats;case DeviceKind::Audio:return Icon::Volume;default:return Icon::Bluetooth;}}
}
// The best real identity for a media session: the website's mark, an installed
// app of that service, a brand monogram, the player's own icon, then a mark
// matched to the player's name.
void Renderer::identity(ID2D1RenderTarget* rt,const MediaSnapshot& p,float x,float y,float size,UINT32 plate){
    if(!p.service.empty()){
        if(auto* mark=findBrand(p.service)){brands_.tile(rt,d2d_.Get(),*mark,x,y,size);return;}
        if(p.serviceIcon){drawPreview(rt,*p.serviceIcon,x,y,size,size);return;}
        if(auto* rule=findService(p.service);rule&&rule->monogram){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(rule->color),&b);rt->FillEllipse(D2D1::Ellipse({x+size/2,y+size/2},size/2,size/2),b.Get());text(rt,std::wstring(1,rule->monogram),x,y,size,size*.5f,0xffffff,DWRITE_FONT_WEIGHT_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER,size);return;}
    }
    if(p.appIcon){drawPreview(rt,*p.appIcon,x,y,size,size);return;}
    if(auto* mark=findBrand(appBrand(p.appName+L" "+p.source))){brands_.tile(rt,d2d_.Get(),*mark,x,y,size);return;}
    ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(plate),&b);rt->FillEllipse(D2D1::Ellipse({x+size/2,y+size/2},size/2,size/2),b.Get());drawIcon(rt,d2d_.Get(),Icon::Music,x+size*.2f,y+size*.2f,size*.6f,0xffffff);
}
void Renderer::deviceBadge(ID2D1RenderTarget* rt,const BluetoothDevice& d,float x,float y,float size,UINT32 plate,UINT32 ink){
    if(auto* mark=findBrand(d.brand)){brands_.tile(rt,d2d_.Get(),*mark,x,y,size);return;}
    ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(plate,plate==0xffffff?.08f:1.f),&b);rt->FillEllipse(D2D1::Ellipse({x+size/2,y+size/2},size/2,size/2),b.Get());
    drawIcon(rt,d2d_.Get(),kindIcon(d.kind),x+size*.22f,y+size*.22f,size*.56f,ink);
}
// A retained pill slides between segmented tabs on a spring.
void Renderer::updateTabs(const ContentSnapshot& s,float x,float y,int count,int selected,bool visible,UINT32 fill){
    double now=seconds();if(!tabSurface_||tabColor_!=fill){tabColor_=fill;surface(tabSurface_,82,26,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(fill),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,82,26},9,9),b.Get());});tabPill_->SetContent(tabSurface_.Get());}
    int key=visible?int(s.page)*10+count:-1;double target=x+std::clamp(selected,0,std::max(0,count-1))*88.;
    if(key!=tabKey_||s.reducedMotion){tabX_.reset(target,now);tabKey_=key;}else if(std::abs(tabX_.target()-target)>.01)tabX_.retarget(target,now,MotionTokens::navigation);
    auto ax=animation(tabX_,now,scale_,20*scale_);tabPill_->SetOffsetX(ax.Get());tabPill_->SetOffsetY(std::round((38+y)*scale_));
    double opacity=visible?1:0;if(std::abs(tabOpacity_.target()-opacity)>.001){if(s.reducedMotion)tabOpacity_.reset(opacity,now);else tabOpacity_.retarget(opacity,now,MotionTokens::artworkOpacity);}auto o=animation(tabOpacity_,now);tabEffect_->SetOpacity(o.Get());
    (void)count;
}
void Renderer::updateCard(const ContentSnapshot& s,UINT32 track,UINT32 accent,UINT32 raised,UINT32 ink){
    bool shown=s.card&&s.notice.kind;double now=seconds();cardIconEffect_->SetOpacity(shown?1.f:0.f);
    if(!shown){cardRing_->SetContent(nullptr);cardRingKey_=-1;}
    else{const bool privacy=s.notice.kind>=5,power=s.notice.kind>=3&&!privacy;const int percent=privacy?100:power?s.battery:s.notice.device.battery;const UINT32 color=privacy?(s.notice.kind==5?0x30d158:s.notice.kind==6?0xff9f0a:0x0a84ff):s.notice.kind==3?0x5fd98a:accent;
        int ringKey=int((s.notice.kind*101+percent+1)^int(color%100003)^int(track%9973));
        if(ringKey!=cardRingKey_){cardRingKey_=ringKey;surface(cardRingSurface_,72,72,[&](auto* rt){drawRing(rt,d2d_.Get(),36,36,31,3,percent>=0?percent/100.:0,color,track);});cardRing_->SetContent(cardRingSurface_.Get());}}
    // Energy sweep sits on the ring of whichever surface shows power.
    if(shown){energy_->SetOffsetX(std::round((20+28-36)*scale_));energy_->SetOffsetY(std::round((16+28-36)*scale_));}
    else if(s.expanded&&!s.live&&s.page==Page::System&&s.statsTab==1){energy_->SetOffsetX(std::round((20+58-36)*scale_));energy_->SetOffsetY(std::round((38+100-36)*scale_));}
    if(!shown){cardKey_=-1;return;}
    int key=s.notice.kind*1000+int(std::hash<std::wstring>{}(s.notice.device.name+s.notice.app)%997);
    if(key!=cardKey_){cardKey_=key;
        surface(cardIconSurface_,56,56,[&](auto* rt){if(s.notice.kind>=5){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());
                if(s.notice.icon)drawPreview(rt,*s.notice.icon,12,12,32,32);else drawIcon(rt,d2d_.Get(),s.notice.kind==5?Icon::Camera:s.notice.kind==6?Icon::Microphone:Icon::Location,16,16,24,ink);}
            else if(s.notice.kind>=3){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(s.notice.kind==3?0x3fcf7a:raised,s.notice.kind==3?1.f:.9f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),s.notice.kind==3?Icon::Bolt:Icon::Battery,14,14,28,s.notice.kind==3?0x0b1f12:ink);}
            else deviceBadge(rt,s.notice.device,4,4,48,raised,ink);});cardIcon_->SetContent(cardIconSurface_.Get());
        if(s.reducedMotion)cardPop_.reset(1,now);else{cardPop_.reset(.55,now);cardPop_.retarget(1,now,{.8,420,20});}
        auto z=animation(cardPop_,now);cardIconScale_->SetScaleX(z.Get());cardIconScale_->SetScaleY(z.Get());
        energize(s.reducedMotion,s.notice.kind==3);
    }
    (void)accent;
}
// One sweep of light around the ring: a charger connecting, or a device arriving.
void Renderer::energize(bool reduced,bool charging){
    if(reduced)return;double now=seconds();
    surface(energySurface_,72,72,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(charging?0x7dffb0:0xffffff),&b);ComPtr<ID2D1StrokeStyle> round;auto props=D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND);d2d_->CreateStrokeStyle(props,nullptr,0,&round);
        for(int i=0;i<18;++i){float a0=-1.5708f+i*.075f,a1=a0+.08f;b->SetOpacity(float(i+1)/18.f);rt->DrawLine({36+31*std::cos(a0),36+31*std::sin(a0)},{36+31*std::cos(a1),36+31*std::sin(a1)},b.Get(),3.4f,round.Get());}});
    energy_->SetContent(energySurface_.Get());energySpin_.reset(0,now);energySpin_.retarget(450,now,{1,26,10});energyGlow_.reset(1,now);energyGlow_.retarget(0,now,{1,9,6});
    auto spin=animation(energySpin_,now),glow=animation(energyGlow_,now);energyRotation_->SetAngle(spin.Get());energyEffect_->SetOpacity(glow.Get());commit();
}
float Renderer::measure(const std::wstring& value,float size,DWRITE_FONT_WEIGHT weight){
    if(value.empty())return 0;ComPtr<IDWriteTextFormat> f;ComPtr<IDWriteTextLayout> l;
    if(FAILED(write_->CreateTextFormat(fontFamily(size),nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f))||FAILED(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),4096,64,&l)))return float(value.size())*size*.5f;
    DWRITE_TEXT_METRICS m{};l->GetMetrics(&m);return m.widthIncludingTrailingWhitespace;
}
// The command bar's caret: glides to its position and blinks in the compositor.
void Renderer::updateCaret(const ContentSnapshot& s,float x,UINT32 accent){
    const bool shown=s.card&&s.command.active&&s.command.status.empty();double now=seconds();
    if(!shown){caretEffect_->SetOpacity(0.f);caret_->SetContent(nullptr);return;}
    if(caretColor_!=accent||!caretSurface_){caretColor_=accent;surface(caretSurface_,2,20,[&](auto* rt){rt->Clear(D2D1::ColorF(accent));});}
    caret_->SetContent(caretSurface_.Get());caret_->SetOffsetY(std::round(11*scale_));
    if(s.reducedMotion||std::abs(caretX_.target()-x)>60)caretX_.reset(x,now);else if(std::abs(caretX_.target()-x)>.01)caretX_.retarget(x,now,{1,1400,70});
    auto cx=animation(caretX_,now,scale_);caret_->SetOffsetX(cx.Get());
    // Solid while typing, then a soft blink: on 0.55 s, fade, off, fade back, repeat.
    ComPtr<IDCompositionAnimation> blink;check(device_->CreateAnimation(&blink));check(blink->SetAbsoluteBeginTime(ticks(now)));
    if(s.reducedMotion){blink->AddCubic(0,1,0,0,0);blink->End(1,1);}
    else{blink->AddCubic(0,1,0,0,0);blink->AddCubic(.55,1,-10,0,0);blink->AddCubic(.65,0,0,0,0);blink->AddCubic(1.0,0,10,0,0);blink->AddCubic(1.1,1,0,0,0);blink->AddRepeat(1.1,1.1);}
    caretEffect_->SetOpacity(blink.Get());
}
// On expanded pages, the band above the content names what is using the camera,
// microphone or location.
void Renderer::updatePrivacyBand(const ContentSnapshot& s,UINT32 ink,UINT32 muted,UINT32 raised){
    privacyVisible_=s.expanded&&!s.live&&!s.privacy.empty();if(!privacyVisible_){privacyEffect_->SetOpacity(0.f);return;}
    std::vector<Capability> caps;for(auto c:{Capability::Camera,Capability::Microphone,Capability::Location})if(std::any_of(s.privacy.begin(),s.privacy.end(),[&](auto& u){return u.capability==c;}))caps.push_back(c);
    std::wstring what;for(auto c:caps){if(!what.empty())what+=L" and ";what+=capabilityName(c);}
    std::wstring line=what+L"  \u00b7  "+s.privacy.front().app;{std::vector<std::wstring> apps;for(auto& u:s.privacy)if(std::find(apps.begin(),apps.end(),u.key)==apps.end())apps.push_back(u.key);if(apps.size()>1)line+=L" +"+std::to_wstring(apps.size()-1);}
    std::wstring key=line+std::to_wstring(ink)+std::to_wstring(raised);if(key==privacyKey_)return;privacyKey_=key;
    const float textWidth=std::min(300.f,measure(line,10.5f,DWRITE_FONT_WEIGHT_MEDIUM)),pill=textWidth+22+float(caps.size())*9,x=(380-pill)/2;
    surface(privacySurface_,380,24,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.9f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({x,0,x+pill,24},12,12),b.Get());
        float dx=x+11;for(auto c:caps){b->SetColor(D2D1::ColorF(c==Capability::Camera?0x30d158:c==Capability::Microphone?0xff9f0a:0x0a84ff));rt->FillEllipse(D2D1::Ellipse({dx+3,12},3.2f,3.2f),b.Get());dx+=9;}
        text(rt,line,dx+3,0,textWidth+2,10.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,24);(void)muted;});
    privacyBand_->SetContent(privacySurface_.Get());
}
}
