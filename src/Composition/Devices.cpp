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
void Renderer::updateCard(const ContentSnapshot& s,UINT32 accent,UINT32 raised,UINT32 ink){
    bool shown=s.card&&s.notice.kind;double now=seconds();cardIconEffect_->SetOpacity(shown?1.f:0.f);
    // Energy sweep sits on the ring of whichever surface shows power.
    if(shown){energy_->SetOffsetX(std::round((20+28-36)*scale_));energy_->SetOffsetY(std::round((16+28-36)*scale_));}
    else if(s.expanded&&!s.live&&s.page==Page::System&&s.statsTab==1){energy_->SetOffsetX(std::round((20+58-36)*scale_));energy_->SetOffsetY(std::round((38+100-36)*scale_));}
    if(!shown){cardKey_=-1;return;}
    int key=s.notice.kind*1000+int(std::hash<std::wstring>{}(s.notice.device.name)%997);
    if(key!=cardKey_){cardKey_=key;
        surface(cardIconSurface_,56,56,[&](auto* rt){if(s.notice.kind>=3){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(s.notice.kind==3?0x3fcf7a:raised,s.notice.kind==3?1.f:.9f),&b);rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),s.notice.kind==3?Icon::Bolt:Icon::Battery,14,14,28,s.notice.kind==3?0x0b1f12:ink);}
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
}
