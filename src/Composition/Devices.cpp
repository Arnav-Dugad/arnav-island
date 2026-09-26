#include "Renderer.h"
namespace nexus {
void Renderer::updateBud(const ContentSnapshot& s,UINT32 ink,UINT32 muted,UINT32 accent,UINT32 raised){
    if(!s.bud.kind)return;const std::wstring key=std::to_wstring(s.bud.kind)+L"|"+s.bud.title+L"|"+std::to_wstring(s.bud.more)+L"|"+std::to_wstring(ink)+L"|"+std::to_wstring(accent);if(key==budKey_&&budLabelSurface_)return;budKey_=key;
    const int k=s.bud.kind;const Icon glyph=k==1||k==2?Icon::Bluetooth:k==3?Icon::Bolt:k==4?Icon::Battery:k==5?Icon::Camera:k==6?Icon::Microphone:k==7?Icon::Location:k==8?Icon::Earbuds:k==9?Icon::Eyedropper:k==10?Icon::Text:k==11||k==12?Icon::Snip:k==13?Icon::Heart:k==14?Icon::Link:k==15?Icon::Download:k==17?Icon::Handoff:Icon::Laptop;
    surface(budLabelSurface_,236,36,[&](ID2D1RenderTarget* rt){rt->Clear(D2D1::ColorF(0,0));ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(raised,.9f),&b));
        rt->FillEllipse(D2D1::Ellipse({22,18},12,12),b.Get());drawIcon(rt,d2d_.Get(),glyph,14,10,16,accent);
        const float room=s.bud.more>0?150.f:180.f;text(rt,s.bud.title,42,0,room,11,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,36);
        if(s.bud.more>0)text(rt,L"+"+std::to_wstring(s.bud.more),196,0,30,10,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_TRAILING,36);});
    budLabel_->SetContent(budLabelSurface_.Get());
}
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
    // Only on an alert card: the command bar is a card state too, and must never show the last alert's icon (a device's logo, its ring).
    bool shown=s.card&&!s.command.active&&s.notice.kind;double now=seconds();cardIconEffect_->SetOpacity(shown?1.f:0.f);
    // Capture cards (9-11) have no level ring.
    if(!shown||(s.notice.kind>=9&&s.notice.kind!=12&&s.notice.kind!=13)){cardRing_->SetContent(nullptr);cardRingKey_=-1;}
    else{const bool privacy=(s.notice.kind>=5&&s.notice.kind<=7)||s.notice.kind==12,power=s.notice.kind==3||s.notice.kind==4,weekly=s.notice.kind==13;const int percent=privacy?100:power?s.battery:weekly?(s.healthNow>=0?int(std::lround(s.healthNow*100)):-1):s.notice.device.battery;const UINT32 color=privacy?(s.notice.kind==5?0x30d158:s.notice.kind==6?0xff9f0a:s.notice.kind==12?0xbf5af2:0x0a84ff):s.notice.kind==3?0x5fd98a:accent;
        int ringKey=int((s.notice.kind*101+percent+1)^int(color%100003)^int(track%9973));
        if(ringKey!=cardRingKey_){cardRingKey_=ringKey;surface(cardRingSurface_,72,72,[&](auto* rt){drawRing(rt,d2d_.Get(),36,36,31,3,percent>=0?percent/100.:0,color,track);});cardRing_->SetContent(cardRingSurface_.Get());}}
    // Energy sweep sits on the ring of whichever surface shows power.
    if(shown){energy_->SetOffsetX(std::round((20+28-36)*scale_));energy_->SetOffsetY(std::round((16+28-36)*scale_));}
    else if(s.expanded&&!s.live&&s.page==Page::System&&s.statsTab==1){energy_->SetOffsetX(std::round((20+58-36)*scale_));energy_->SetOffsetY(std::round((38+100-36)*scale_));}
    if(!shown){cardKey_=-1;return;}
    // The icon is part of the key: a cover that arrives after the card (read from this PC's library) is drawn too.
    int key=s.notice.kind*1000+int(std::hash<std::wstring>{}(s.notice.device.name+s.notice.app+s.notice.detail)%997)+int((reinterpret_cast<uintptr_t>(s.notice.icon.get())>>4)%2000)*20000;
    if(key!=cardKey_){cardKey_=key;
        surface(cardIconSurface_,56,56,[&](auto* rt){
            if(s.notice.kind==9){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(s.notice.colour),&b);rt->FillEllipse(D2D1::Ellipse({28,28},23,23),b.Get());b->SetColor(D2D1::ColorF(ink,.22f));rt->DrawEllipse(D2D1::Ellipse({28,28},23.5f,23.5f),b.Get(),1.5f);return;}
            if(s.notice.kind==10){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),Icon::Text,16,16,24,accent);return;}
            // 0.18: the island has updated itself.
            if(s.notice.kind==18){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),Icon::Spark,16,16,24,accent);return;}
            if(s.notice.kind==11){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({4,4,52,52},12,12),b.Get());if(s.notice.icon)drawPreview(rt,*s.notice.icon,6,6,44,44);else drawIcon(rt,d2d_.Get(),Icon::Snip,16,16,24,accent);return;}
            if((s.notice.kind>=5&&s.notice.kind<=7)||s.notice.kind==12){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());
                if(s.notice.icon)drawPreview(rt,*s.notice.icon,12,12,32,32);else drawIcon(rt,d2d_.Get(),s.notice.kind==5?Icon::Camera:s.notice.kind==6?Icon::Microphone:s.notice.kind==12?Icon::Snip:Icon::Location,16,16,24,ink);}
            else if(s.notice.kind>=14&&s.notice.kind<=17){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());
                // Music from another PC shows its cover when this PC has the song, else a note.
                if(s.notice.kind==17&&s.notice.icon){ComPtr<ID2D1RoundedRectangleGeometry> g;d2d_->CreateRoundedRectangleGeometry(D2D1::RoundedRect({4,4,52,52},12,12),&g);ComPtr<ID2D1Layer> layer;rt->CreateLayer(&layer);rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),g.Get()),layer.Get());drawPreview(rt,*s.notice.icon,4,4,48,48);rt->PopLayer();}
                else drawIcon(rt,d2d_.Get(),s.notice.kind==14?Icon::Link:s.notice.kind==15?Icon::Download:s.notice.kind==17?Icon::Handoff:Icon::Laptop,16,16,24,accent);}
            else if(s.notice.kind==13){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),Icon::Heart,16,16,24,accent);}
            else if(s.notice.kind==3||s.notice.kind==4){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(s.notice.kind==3?0x3fcf7a:raised,s.notice.kind==3?1.f:.9f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),s.notice.kind==3?Icon::Bolt:Icon::Battery,14,14,28,s.notice.kind==3?0x0b1f12:ink);}
            else deviceBadge(rt,s.notice.device,4,4,48,raised,ink);});cardIcon_->SetContent(cardIconSurface_.Get());
        if(s.reducedMotion)cardPop_.reset(1,now);else{cardPop_.reset(.55,now);cardPop_.retarget(1,now,{.8,420,20});}
        auto z=animation(cardPop_,now);cardIconScale_->SetScaleX(z.Get());cardIconScale_->SetScaleY(z.Get());
        energize(s.reducedMotion,s.notice.kind==3);
    }
    (void)accent;
}
// Light running along the island's edge when an alert arrives. The outline (the free edges only, when the island is
// attached to the screen) is stroked twice (a crisp core line with a little glow, and a wide soft glow). Each runs as
// a soft glint from the middle of the edge opposite the screen outward both ways: six nested windows around a moving
// centre, each a sixth as bright, so the light rises and falls away smoothly at both ends; it fades in under a second.
// Frost settles only while nothing is happening: the island compact, the pointer away, and no alert in the last four seconds.
void Renderer::updateFrost(){
    const double now=seconds();const bool rest=frostAllowed_&&!expanded_&&!pointerInside_&&now>=frostHold_;if(rest==frostOn_)return;frostOn_=rest;glass_.frost(rest,now,qaFrostPace);
}
void Renderer::splash(float w,float h,float r,bool attached,double delay,bool reduced){
    frostHold_=seconds()+4;updateFrost();
    if(reduced||w<20||h<20)return;
    if(!splash_){check(device_->CreateVisual(&splash_));check(device_->CreateEffectGroup(&splashEffect_));splash_->SetEffect(splashEffect_.Get());splashEffect_->SetOpacity(0.f);splash_->SetBorderMode(DCOMPOSITION_BORDER_MODE_HARD);check(body_->AddVisual(splash_.Get(),FALSE,nullptr));
        for(size_t k=0;k<splashBands_.size();++k){check(device_->CreateVisual(&splashBands_[k]));check(device_->CreateRectangleClip(&splashClips_[k]));splashBands_[k]->SetClip(splashClips_[k].Get());
            check(device_->CreateEffectGroup(&splashBandEffects_[k]));splashBands_[k]->SetEffect(splashBandEffects_[k].Get());splashBandEffects_[k]->SetOpacity(1.f/splashSteps);check(splash_->AddVisual(splashBands_[k].Get(),FALSE,nullptr));}}
    const int edge=edge_;r=std::min({r,w/2,h/2});
    // The stroke sits just inside the body, where its clip keeps it.
    const float i=.9f;ComPtr<ID2D1PathGeometry> path;check(d2d_->CreatePathGeometry(&path));{ComPtr<ID2D1GeometrySink> sink;check(path->Open(&sink));const D2D1_SIZE_F arc{r-i,r-i};
        if(!attached){sink->BeginFigure({r,i},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddLine({w-r,i});sink->AddArc({{w-i,r},arc,0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({w-i,h-r});sink->AddArc({{w-r,h-i},arc,0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
            sink->AddLine({r,h-i});sink->AddArc({{i,h-r},arc,0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({i,r});sink->AddArc({{r,i},arc,0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->EndFigure(D2D1_FIGURE_END_CLOSED);}
        else if(edge==0){sink->BeginFigure({i,0},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddLine({i,h-r});sink->AddArc({{r,h-i},arc,0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({w-r,h-i});sink->AddArc({{w-i,h-r},arc,0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({w-i,0});sink->EndFigure(D2D1_FIGURE_END_OPEN);}
        else if(edge==1){sink->BeginFigure({w,i},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddLine({r,i});sink->AddArc({{i,r},arc,0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({i,h-r});sink->AddArc({{r,h-i},arc,0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({w,h-i});sink->EndFigure(D2D1_FIGURE_END_OPEN);}
        else{sink->BeginFigure({0,i},D2D1_FIGURE_BEGIN_HOLLOW);sink->AddLine({w-r,i});sink->AddArc({{w-i,r},arc,0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({w-i,h-r});sink->AddArc({{w-r,h-i},arc,0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});sink->AddLine({0,h-i});sink->EndFigure(D2D1_FIGURE_END_OPEN);}
        check(sink->Close());}
    ComPtr<ID2D1StrokeStyle> round;check(d2d_->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND),nullptr,0,&round));
    // Tier 0: the core line with a little glow; tier 1: the wide soft glow.
    for(size_t k=0;k<2;++k){splashSurfaces_[k].Reset();surface(splashSurfaces_[k],int(std::ceil(w)),int(std::ceil(h)),[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(0xffffff,k?.14f:.24f),&b));
        rt->DrawGeometry(path.Get(),b.Get(),k?6.f:4.f,round.Get());b->SetColor(D2D1::ColorF(0xffffff,k?.30f:.95f));rt->DrawGeometry(path.Get(),b.Get(),1.4f,round.Get());});}
    // Sweeps: along x for the top dock (and a floating island), along y for side docks.
    const bool vertical=attached&&edge!=0;const float length=vertical?h:w,centre=length/2,halves[2]{44,120},travel=centre+halves[1];const double start=seconds()+delay,duration=.8;
    auto sweep=[&](float from,float to){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(start)));const double d=duration;const float span=(to-from)*scale_;
        check(a->AddCubic(0,from*scale_,0,float(3*span/(d*d)),float(-2*span/(d*d*d))));check(a->End(d,to*scale_));return a;};
    // Band (tier, direction, step): the window centre +- half, half growing with the step, its centre running from the middle outward.
    for(int tier=0;tier<2;++tier)for(int dir=0;dir<2;++dir)for(int step=0;step<splashSteps;++step){const size_t k=size_t((tier*2+dir)*splashSteps+step);auto* c=splashClips_[k].Get();splashBands_[k]->SetContent(splashSurfaces_[size_t(tier)].Get());
        const float half=halves[tier]*float(step+1)/splashSteps,sign=dir?1.f:-1.f;auto lo=sweep(centre-half,centre+sign*travel-half),hi=sweep(centre+half,centre+sign*travel+half);
        if(vertical){c->SetLeft(0.f);c->SetRight(std::ceil(w*scale_));c->SetTop(lo.Get());c->SetBottom(hi.Get());}
        else{c->SetTop(0.f);c->SetBottom(std::ceil(h*scale_));c->SetLeft(lo.Get());c->SetRight(hi.Get());}}
    // Up quickly, a moment bright, then away.
    ComPtr<IDCompositionAnimation> fade;check(device_->CreateAnimation(&fade));check(fade->SetAbsoluteBeginTime(ticks(start)));
    check(fade->AddCubic(0,0,10,0,0));check(fade->AddCubic(.1,1,0,0,0));check(fade->AddCubic(.45,1,-float(1/.4),0,0));check(fade->End(.85,0));splashEffect_->SetOpacity(fade.Get());commit();
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
    std::vector<Capability> caps;for(auto c:capabilityOrder)if(std::any_of(s.privacy.begin(),s.privacy.end(),[&](auto& u){return u.capability==c;}))caps.push_back(c);
    std::wstring what;for(auto c:caps){if(!what.empty())what+=L" and ";what+=capabilityName(c);}
    std::wstring line=what+L"  \u00b7  "+s.privacy.front().app;{std::vector<std::wstring> apps;for(auto& u:s.privacy)if(std::find(apps.begin(),apps.end(),u.key)==apps.end())apps.push_back(u.key);if(apps.size()>1)line+=L" +"+std::to_wstring(apps.size()-1);}
    std::wstring key=line+std::to_wstring(ink)+std::to_wstring(raised);if(key==privacyKey_)return;privacyKey_=key;
    const float textWidth=std::min(300.f,measure(line,10.5f,DWRITE_FONT_WEIGHT_MEDIUM)),pill=textWidth+22+float(caps.size())*9,x=(380-pill)/2;
    surface(privacySurface_,380,24,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.9f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({x,0,x+pill,24},12,12),b.Get());
        float dx=x+11;for(auto c:caps){b->SetColor(D2D1::ColorF(capabilityColour(c)));rt->FillEllipse(D2D1::Ellipse({dx+3,12},3.2f,3.2f),b.Get());dx+=9;}
        text(rt,line,dx+3,0,textWidth+2,10.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,24);(void)muted;});
    privacyBand_->SetContent(privacySurface_.Get());
}
// Rows cascade in: each band of the content rises 6 DIPs and brightens, 28 ms after
// the one above it, on compositor time.
void Renderer::bandCascade(double start,size_t k,ComPtr<IDCompositionAnimation>& fade,ComPtr<IDCompositionAnimation>& rise){
    // Every band's animation begins now and holds its starting value until its turn,
    // so no row shows early and then blinks.
    auto build=[&](double delay,float from,float to,double duration){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(start)));
        const float span=to-from;const double d=duration;if(delay>0)check(a->AddCubic(0,from,0,0,0));
        check(a->AddCubic(delay,from,float(3*span/d),float(-3*span/(d*d)),float(span/(d*d*d))));check(a->End(delay+d,to));return a;};
    const double delay=double(k)*.028;fade=build(delay,.15f,1.f,.22);rise=build(delay,6*scale_,0.f,.26);
}
void Renderer::cascade(double start){
    cascadeStart_=start;
    for(size_t k=0;k<bands_.size();++k){ComPtr<IDCompositionAnimation> fade,rise;bandCascade(start,k,fade,rise);bands_[k].effect->SetOpacity(fade.Get());bands_[k].visual->SetOffsetY(rise.Get());}
    // Rolling numbers on the content cascade with the band they sit in.
    for(int id:{0,2,3,4,5}){auto& o=odometers_[size_t(id)];if(!o.root||o.shown.empty())continue;ComPtr<IDCompositionAnimation> fade,rise;bandCascade(start,size_t(std::clamp(int(o.top/48),0,int(bands_.size())-1)),fade,rise);o.effect->SetOpacity(fade.Get());o.root->SetOffsetY(rise.Get());}
    commit();
}
// A soft light that follows the pointer: brightest on glass, a whisper on dark solid,
// none on light solid (it would read as a smudge).
void Renderer::pointer(float x,float y,bool inside,bool reduced){
    if(inside!=pointerInside_){pointerInside_=inside;updateFrost();}
    if(!sheen_||sheenStrength_<=0){if(sheenEffect_)sheenEffect_->SetOpacity(0.f);if(glass_.available()&&material_!=0){const double t=seconds();Spring none{0};glass_.sheen(none,none,none,t,0);}return;}
    const double now=seconds();const double target=inside?1:0;
    auto aim=[&](Spring& spring,double value,SpringSpec spec,bool jump){if(reduced||jump)spring.reset(value,now);else if(std::abs(spring.target()-value)>.2)spring.retarget(value,now,spec);};
    const bool appearing=inside&&sheenOpacity_.sample(now).position<.02;
    aim(sheenX_,x-130,{1,260,32},appearing);aim(sheenY_,y-130,{1,260,32},appearing);
    if(std::abs(sheenOpacity_.target()-target)>.01){if(reduced)sheenOpacity_.reset(target,now);else sheenOpacity_.retarget(target,now,{1,120,22});}
    // Glass carries the light itself, so it runs on across the shoulders; the DirectComposition layer stays dark.
    if(material_!=0&&glass_.available()){glass_.sheen(sheenX_,sheenY_,sheenOpacity_,now,sheenStrength_);sheenEffect_->SetOpacity(0.f);commit();return;}
    auto ox=animation(sheenX_,now,scale_),oy=animation(sheenY_,now,scale_),o=animation(sheenOpacity_,now,sheenStrength_);
    sheen_->SetOffsetX(ox.Get());sheen_->SetOffsetY(oy.Get());sheenEffect_->SetOpacity(o.Get());commit();
}
}
