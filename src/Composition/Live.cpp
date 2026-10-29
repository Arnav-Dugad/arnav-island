#include "Composition/Renderer.h"
#include <chrono>
#include <cwctype>

// 0.27: a phone's live activity on the island, the way a Live Activity sits in the Dynamic Island: compact while it runs (its
// app's icon, what it says, the ETA in its colour and a hairline of its progress along the foot), and a card when opened,
// when it changes in a way worth seeing, and once more when it finishes.
namespace nexus {
namespace {
using Live=ContentSnapshot::Live;
int64_t unixMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
// The card draws a tracking line when the phone says how far along it is (or that it can't), else the app's own picture.
bool tracking(const Live& l){return l.done||l.a.has(LiveActivity::HasProgress)||l.a.has(LiveActivity::Indeterminate)||!l.a.segments.empty();}
float snapshotHeight(const Live& l){if(!l.snapshot||!l.snapshot->width)return 0;return std::clamp(332.f*float(l.snapshot->height)/float(l.snapshot->width),44.f,112.f);}
// The line under the track: the short status, when it says something the title and text don't.
std::wstring footLine(const Live& l){if(l.done)return {};const auto& s=l.a.status;return s.empty()||s==l.a.title||s==l.a.text?std::wstring():s;}
// The tracking line's ends in the card (content DIPs): it leaves room for the start and end icons.
float trackStart(const Live& l){return l.start?32.f:5.f;}float trackEnd(const Live& l){return l.end?300.f:327.f;}
constexpr float trackTop=62,trackMid=16;
UINT32 rgb(uint32_t argb,UINT32 fallback){return (argb>>24)?argb&0xffffff:fallback;}
// A picture clipped to a rounded box.
template<class Draw> void rounded(ID2D1RenderTarget* rt,ID2D1Factory* f,D2D1_RECT_F r,float radius,Draw draw){ComPtr<ID2D1RoundedRectangleGeometry> g;if(FAILED(f->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r,radius,radius),&g)))return;
    ComPtr<ID2D1Layer> layer;if(FAILED(rt->CreateLayer(&layer)))return;rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),g.Get()),layer.Get());draw();rt->PopLayer();}
}
float liveCardHeight(const Live& l){
    if(tracking(l))return footLine(l).empty()?122.f:142.f;
    if(l.snapshot)return 16+56+10+snapshotHeight(l)+16;
    return 92;
}
UINT32 liveAccent(const Live& l,UINT32 fallback,bool light){
    UINT32 c=(l.a.accent>>24)?l.a.accent&0xffffff:0;if(!c&&l.icon)c=l.icon->accent;if(!c)return fallback;
    double h,s,v;rgbToHsl(((c>>16)&255)/255.,((c>>8)&255)/255.,(c&255)/255.,h,s,v);if(s<.14)return fallback;
    return hslColor(h,std::max(s,.5),light?std::clamp(v,.34,.5):std::clamp(v,.52,.68));
}
std::wstring liveTitle(const Live& l){
    if(l.done){const auto said=lowered(l.a.title+L" "+l.a.text+L" "+l.a.app);
        if(l.a.has(LiveActivity::Navigation)||said.find(L"ride")!=std::wstring::npos||said.find(L"driver")!=std::wstring::npos||said.find(L"trip")!=std::wstring::npos||said.find(L"cab")!=std::wstring::npos)return L"You\u2019ve arrived";
        if(l.a.has(LiveActivity::Delivery))return L"Delivered";return L"Done";}
    return l.a.title.empty()?(l.a.text.empty()?l.a.app:l.a.text):l.a.title;
}
// The card's leading picture: the large icon (a restaurant, a rider) with the app's icon as a badge, else the app's icon;
// a finished one wears a green check.
void Renderer::activityIcon(ID2D1RenderTarget* rt,const ContentSnapshot& s,UINT32 raised,UINT32 accent){
    const auto* l=s.liveNow();if(!l)return;ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised,.95f),&b);
    const auto& main=l->large?l->large:l->icon;
    if(main){rt->FillRoundedRectangle(D2D1::RoundedRect({4,4,52,52},14,14),b.Get());rounded(rt,d2d_.Get(),{6,6,50,50},12,[&]{drawPreviewFilled(rt,*main,6,6,44,44);});}
    else{rt->FillEllipse(D2D1::Ellipse({28,28},24,24),b.Get());drawIcon(rt,d2d_.Get(),Icon::Phone,16,16,24,accent);}
    auto badge=[&](auto paint){b->SetColor(D2D1::ColorF(raised));rt->FillEllipse(D2D1::Ellipse({45,45},10.5f,10.5f),b.Get());paint();};
    if(l->done)badge([&]{b->SetColor(D2D1::ColorF(0x30d158));rt->FillEllipse(D2D1::Ellipse({45,45},9,9),b.Get());drawIcon(rt,d2d_.Get(),Icon::Check,38.5f,38.5f,13,0xffffff);});
    else if(l->large&&l->icon)badge([&]{ComPtr<ID2D1EllipseGeometry> g;if(FAILED(d2d_->CreateEllipseGeometry(D2D1::Ellipse({45,45},8.5f,8.5f),&g)))return;ComPtr<ID2D1Layer> layer;rt->CreateLayer(&layer);
        rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),g.Get()),layer.Get());drawPreviewFilled(rt,*l->icon,36.5f,36.5f,17,17);rt->PopLayer();});
}
// The compact island while an activity is live: its icon on the left, what it says, the ETA on the right in its colour (rolling),
// how many there are when more than one, and a hairline of its progress along the foot.
void Renderer::activityCompact(ID2D1RenderTarget* rt,const ContentSnapshot& s,UINT32 ink,UINT32 muted,UINT32 track,UINT32 accent,UINT32 raised){
    const auto& l=*s.liveNow();const float w=s.liveWidth();const UINT32 tint=liveAccent(l,accent,s.light);ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(tint),&b);
    if(l.icon)rounded(rt,d2d_.Get(),{10,6,32,28},7,[&]{drawPreviewFilled(rt,*l.icon,10,6,22,22);});
    else{b->SetColor(D2D1::ColorF(raised));rt->FillEllipse(D2D1::Ellipse({21,17},11,11),b.Get());drawIcon(rt,d2d_.Get(),Icon::Phone,14,10,14,tint);}
    float right=w-13;
    if(l.done){b->SetColor(D2D1::ColorF(0x30d158));rt->FillEllipse(D2D1::Ellipse({right-8,17},8,8),b.Get());drawIcon(rt,d2d_.Get(),Icon::Check,right-14,11,12,0xffffff);right-=24;}
    else{const auto eta=liveEta(l.a,unixMs());const double f=l.a.fraction();
        if(!eta.empty()){headerSpots_.push_back({13,eta,right,0,11.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD,tint,34,true});right-=measure(eta,11.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD)+10;}
        else if(f>=0){const auto p=std::to_wstring(int(f*100+.5))+L"%";text(rt,p,right-60,0,60,11.5f,tint,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_TRAILING,34);right-=measure(p,11.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD)+10;}}
    // More than one: a small count, cycled by a click on the card or the wheel.
    if(s.lives.size()>1){const auto n=std::to_wstring(s.lives.size());const float pw=std::max(16.f,measure(n,9,DWRITE_FONT_WEIGHT_SEMI_BOLD)+9);b->SetColor(D2D1::ColorF(ink,s.light?.1f:.14f));
        rt->FillRoundedRectangle(D2D1::RoundedRect({right-pw,10,right,24},7,7),b.Get());text(rt,n,right-pw,10,pw,9,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER,14);right-=pw+7;}
    const std::wstring title=liveTitle(l);text(rt,title,40,0,std::max(12.f,right-40),11.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);
    // The hairline: its segments' colours up to where it is (the activity's colour when it has none), the rest a faint track.
    const double f=l.done?1.:l.a.fraction();
    if(f>=0||l.a.has(LiveActivity::Indeterminate)){const float x0=18,x1=w-18,y=32.25f;b->SetColor(D2D1::ColorF(track,s.light?.55f:.6f));rt->FillRoundedRectangle(D2D1::RoundedRect({x0,y-.75f,x1,y+.75f},.75f,.75f),b.Get());
        if(f>0){const auto ends=l.a.segmentEnds();double from=0;const float span=x1-x0;
            auto paint=[&](double a,double z,UINT32 c){if(z<=a)return;b->SetColor(D2D1::ColorF(c));rt->FillRoundedRectangle(D2D1::RoundedRect({x0+float(a)*span,y-.9f,x0+float(z)*span,y+.9f},.9f,.9f),b.Get());};
            if(ends.empty()||l.done)paint(0,f,l.done?0x30d158:tint);else for(size_t i=0;i<ends.size()&&from<f;++i){paint(from,std::min(f,ends[i]),rgb(l.a.segments[i].argb,tint));from=ends[i];}
            b->SetColor(D2D1::ColorF(l.done?0x30d158:tint,.35f));rt->FillEllipse(D2D1::Ellipse({x0+float(f)*span,y},3.2f,2.2f),b.Get());}}
    compactTargets.push_back({Action::LiveOpen,0,0,w,34});
    // A different activity (or its new title) eases in like any new label.
    const std::wstring key=L"\x1flive"+std::wstring(l.a.key.begin(),l.a.key.end())+title;
    if(key!=headerLabel_){if(restCompact_&&!s.expanded&&!s.reducedMotion&&!headerLabel_.empty()){headerEntrance_=seconds();auto a=entrance(headerEntrance_,.35f);headerEffect_->SetOpacity(a.Get());}headerLabel_=key;}
}
// The card (content coordinates; the leading picture is the card's icon layer): title, text, where it's from, the ETA large on the
// right; under them the tracking line (retained layers, updateActivity) and its status, or the app's own picture.
void Renderer::activityCard(ID2D1RenderTarget* rt,const ContentSnapshot& s,UINT32 ink,UINT32 muted,UINT32 track,UINT32 accent,UINT32 raised,float raisedAlpha){
    const auto& l=*s.liveNow();const UINT32 tint=liveAccent(l,accent,s.light);ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(tint),&b);(void)track;
    float etaW=0;
    if(l.done){b->SetColor(D2D1::ColorF(0x30d158));rt->FillEllipse(D2D1::Ellipse({315,20},15,15),b.Get());drawIcon(rt,d2d_.Get(),Icon::Check,305,10,20,0xffffff);etaW=40;}
    else{const auto eta=liveEta(l.a,unixMs());const double f=l.a.fraction();
        if(!eta.empty()){std::wstring big=eta,unit;const bool now=eta==L"Now";
            if(eta.size()>4&&eta.ends_with(L" min")){big=eta.substr(0,eta.size()-4);unit=L"min";}else if(eta.size()>2&&eta.ends_with(L" h")){big=eta.substr(0,eta.size()-2);unit=L"hr";}else if(!now&&l.a.has(LiveActivity::CountsDown))unit=L"left";
            const float size=now?20.f:big.size()>5?20.f:25.f;const auto weight=now?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_LIGHT;
            contentSpots_.push_back({14,big,332,now?6.f:0.f,size,weight,now?tint:ink,0,true});if(!unit.empty())text(rt,unit,232,31,100,9.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);
            etaW=std::max(measure(big,size,weight),unit.empty()?0.f:measure(unit,9.5f))+4;}
        else if(f>=0){const auto p=std::to_wstring(int(f*100+.5))+L"%";text(rt,p,232,0,100,22,ink,DWRITE_FONT_WEIGHT_LIGHT,DWRITE_TEXT_ALIGNMENT_TRAILING,32);etaW=measure(p,22,DWRITE_FONT_WEIGHT_LIGHT)+4;}}
    const float room=std::max(80.f,332-70-etaW-10);
    const std::wstring title=liveTitle(l),second=l.done?(l.a.title.empty()?l.a.text:l.a.title):(l.a.text.empty()?l.a.status:l.a.text);
    text(rt,title,70,3,room,13.5f,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,second,70,22,room,11,muted);
    // Where it's from (the app and the phone), and which of several it is (the count cycles them).
    const std::wstring from=l.a.app+(l.phone.empty()?L"":L"  \u00b7  "+l.phone);drawIcon(rt,d2d_.Get(),Icon::Phone,69,40,12,tint,.9f);
    float fromW=std::min(measure(from,9)+2,room-(s.lives.size()>1?62.f:16.f));text(rt,from,85,40,fromW,9,muted);
    if(s.lives.size()>1){const std::wstring n=std::to_wstring(std::clamp(s.liveShown,0,int(s.lives.size())-1)+1)+L" of "+std::to_wstring(s.lives.size());const float pw=measure(n,9,DWRITE_FONT_WEIGHT_SEMI_BOLD)+14,px=85+fromW+8;
        b->SetColor(D2D1::ColorF(raised,raisedAlpha));rt->FillRoundedRectangle(D2D1::RoundedRect({px,38,px+pw,53},7.5f,7.5f),b.Get());text(rt,n,px,38,pw,9,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER,15);
        targets.push_back({Action::LiveNext,px-4,34,pw+8,23});}
    if(tracking(l)){const auto foot=footLine(l);if(!foot.empty())text(rt,foot,trackStart(l)-2,trackTop+34,trackEnd(l)-trackStart(l)+4,10.5f,muted);}
    else if(l.snapshot){const float h=snapshotHeight(l);b->SetColor(D2D1::ColorF(raised,raisedAlpha));rt->FillRoundedRectangle(D2D1::RoundedRect({0,66,332,66+h},12,12),b.Get());
        rounded(rt,d2d_.Get(),{0,66,332,66+h},12,[&]{drawPreviewFilled(rt,*l.snapshot,0,66,332,h);});}
}
// The tracking line's layers: drawn again only when what it shows changes; the fill, the head's glow and the tracker glide on one
// spring to each new progress (from the start the first time a card shows it).
void Renderer::updateActivity(const ContentSnapshot& s,UINT32 ink,UINT32 track,UINT32 accent,UINT32 raised){
    const double now=seconds();const float k=scale_;
    if(!actLayer_){for(auto* p:{std::addressof(actLayer_),std::addressof(actBase_),std::addressof(actFill_),std::addressof(actHead_),std::addressof(actTracker_),std::addressof(actShimmerHost_),std::addressof(actShimmer_)})check(device_->CreateVisual(p->GetAddressOf()));
        check(device_->CreateEffectGroup(&actEffect_));actLayer_->SetEffect(actEffect_.Get());actEffect_->SetOpacity(0.f);check(device_->CreateEffectGroup(&actShimmerEffect_));actShimmerHost_->SetEffect(actShimmerEffect_.Get());
        check(device_->CreateRectangleClip(&actFillClip_));actFillClip_->SetLeft(0.f);actFillClip_->SetTop(0.f);actFillClip_->SetBottom(32*k);actFill_->SetClip(actFillClip_.Get());
        check(device_->CreateRectangleClip(&actShimmerClip_));actShimmerHost_->SetClip(actShimmerClip_.Get());check(device_->CreateTranslateTransform(&actBob_));actTracker_->SetTransform(actBob_.Get());
        for(auto* v:{actBase_.Get(),actShimmerHost_.Get(),actFill_.Get(),actHead_.Get(),actTracker_.Get()})check(actLayer_->AddVisual(v,TRUE,nullptr));check(actShimmerHost_->AddVisual(actShimmer_.Get(),FALSE,nullptr));
        check(content_->AddVisual(actLayer_.Get(),TRUE,nullptr));}
    const auto* lp=s.liveNow();const bool on=s.card&&s.notice.kind==25&&lp&&tracking(*lp);
    if(!on){actEffect_->SetOpacity(0.f);if(actMoving_){actMoving_=false;actBob_->SetOffsetY(0.f);actShimmer_->SetOffsetX(-200*k);}actKey_.clear();return;}
    const auto& l=*lp;const UINT32 tint=liveAccent(l,accent,s.light),green=0x30d158;const float x0=trackStart(l),x1=trackEnd(l),span=x1-x0;const bool indeterminate=!l.done&&l.a.has(LiveActivity::Indeterminate);
    const double target=l.done?1.:std::max(0.,l.a.fraction());
    actLayer_->SetOffsetX(0.f);actLayer_->SetOffsetY(std::round(trackTop*k));
    // What the layers show (not where the progress is): redrawn only when this changes.
    std::string drawn=l.a.key+'|'+std::to_string(tint)+'|'+std::to_string(int(s.light))+'|'+std::to_string(int(l.done))+'|'+std::to_string(int(indeterminate))+'|'+std::to_string(raised)+'|'+std::to_string(reinterpret_cast<uintptr_t>(l.tracker.get()))+std::to_string(reinterpret_cast<uintptr_t>(l.start.get()))+std::to_string(reinterpret_cast<uintptr_t>(l.end.get()));
    for(auto& g:l.a.segments)drawn+='s'+std::to_string(g.length)+':'+std::to_string(g.argb);for(auto& p:l.a.points)drawn+='p'+std::to_string(p.position)+':'+std::to_string(p.argb);
    if(drawn!=actDrawn_){actDrawn_=drawn;
        const auto ends=l.a.segmentEnds();auto colourOf=[&](size_t i){return l.done?green:i<l.a.segments.size()?rgb(l.a.segments[i].argb,tint):tint;};
        // Each segment a rounded bar with a hairline gap before the next; lit, it carries a soft glow.
        auto bars=[&](ID2D1RenderTarget* rt,float alpha,bool glow){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(tint),&b);const size_t n=std::max<size_t>(1,ends.size());double from=0;
            for(size_t i=0;i<n;++i){const double to=ends.empty()?1.:ends[i];const float a=x0+float(from)*span+(i?1.5f:0),z=x0+float(to)*span-(i+1<n?1.5f:0);from=to;if(z<=a)continue;const UINT32 c=colourOf(i);
                if(glow)for(int g=3;g>=1;--g){b->SetColor(D2D1::ColorF(c,.07f));const float e=float(g)*2.2f;rt->FillRoundedRectangle(D2D1::RoundedRect({a-e*.4f,trackMid-3-e,z+e*.4f,trackMid+3+e},3+e,3+e),b.Get());}
                b->SetColor(D2D1::ColorF(c,alpha));rt->FillRoundedRectangle(D2D1::RoundedRect({a,trackMid-3,z,trackMid+3},3,3),b.Get());}
            for(auto& p:l.a.points){const float x=x0+float(p.position)/1000.f*span;const UINT32 c=l.done?green:rgb(p.argb,tint);b->SetColor(D2D1::ColorF(raised,glow?1.f:.9f));rt->FillEllipse(D2D1::Ellipse({x,trackMid},4.6f,4.6f),b.Get());
                b->SetColor(D2D1::ColorF(c,glow?1.f:.55f));rt->FillEllipse(D2D1::Ellipse({x,trackMid},3.2f,3.2f),b.Get());if(glow){b->SetColor(D2D1::ColorF(0xffffff,.9f));rt->FillEllipse(D2D1::Ellipse({x,trackMid},1.2f,1.2f),b.Get());}}};
        surface(actBaseSurface_,332,32,[&](auto* rt){bars(rt,s.light?.3f:.24f,false);ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(raised),&b);
            // The ends: where it started (the restaurant, the pickup) and where it's going.
            auto end=[&](const std::shared_ptr<const Artwork>& art,float cx,bool reached){if(!art)return;b->SetColor(D2D1::ColorF(raised));rt->FillEllipse(D2D1::Ellipse({cx,trackMid},10.5f,10.5f),b.Get());
                b->SetColor(D2D1::ColorF(reached?green:ink,reached?.9f:.16f));rt->DrawEllipse(D2D1::Ellipse({cx,trackMid},10.5f,10.5f),b.Get(),reached&&cx>100?2.f:1.2f);ComPtr<ID2D1EllipseGeometry> g;d2d_->CreateEllipseGeometry(D2D1::Ellipse({cx,trackMid},8.f,8.f),&g);
                ComPtr<ID2D1Layer> layer;rt->CreateLayer(&layer);rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),g.Get()),layer.Get());drawPreview(rt,*art,cx-7,trackMid-7,14,14);rt->PopLayer();};
            end(l.start,10.5f,true);end(l.end,321.5f,l.done);});actBase_->SetContent(actBaseSurface_.Get());
        surface(actFillSurface_,332,32,[&](auto* rt){bars(rt,1,true);});actFill_->SetContent(actFillSurface_.Get());
        surface(actHeadSurface_,40,32,[&](auto* rt){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(l.done?green:tint,.62f)},{.55f,D2D1::ColorF(l.done?green:tint,.18f)},{1,D2D1::ColorF(l.done?green:tint,0.f)}};ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,3,&c));
            ComPtr<ID2D1RadialGradientBrush> g;check(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({20,16},{0,0},19,13),c.Get(),&g));rt->FillRectangle({0,0,40,32},g.Get());});
        // The tracker (the rider, the car): on a plate ringed in its colour, so the line passes behind it.
        surface(actTrackerSurface_,32,32,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(0,s.light?.14f:.32f),&b);rt->FillEllipse(D2D1::Ellipse({16,17},13.5f,13.5f),b.Get());
            b->SetColor(D2D1::ColorF(raised));rt->FillEllipse(D2D1::Ellipse({16,16},13,13),b.Get());b->SetColor(D2D1::ColorF(l.done?green:tint));rt->DrawEllipse(D2D1::Ellipse({16,16},12.2f,12.2f),b.Get(),1.6f);
            if(l.done)drawIcon(rt,d2d_.Get(),Icon::Check,8.5f,8.5f,15,green);
            else if(l.tracker){ComPtr<ID2D1EllipseGeometry> g;d2d_->CreateEllipseGeometry(D2D1::Ellipse({16,16},10.5f,10.5f),&g);ComPtr<ID2D1Layer> layer;rt->CreateLayer(&layer);rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),g.Get()),layer.Get());drawPreview(rt,*l.tracker,6.5f,6.5f,19,19);rt->PopLayer();}
            else{rt->FillEllipse(D2D1::Ellipse({16,16},5.5f,5.5f),b.Get());b->SetColor(D2D1::ColorF(0xffffff));rt->FillEllipse(D2D1::Ellipse({16,16},2.f,2.f),b.Get());}});
        surface(actShimmerSurface_,90,32,[&](auto* rt){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(tint,0.f)},{.5f,D2D1::ColorF(tint,.85f)},{1,D2D1::ColorF(tint,0.f)}};ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,3,&c));
            ComPtr<ID2D1LinearGradientBrush> g;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{90,0}),c.Get(),&g));rt->FillRoundedRectangle(D2D1::RoundedRect({0,trackMid-3,90,trackMid+3},3,3),g.Get());});actShimmer_->SetContent(actShimmerSurface_.Get());}
    // The progress spring: a card that just opened on this activity sweeps in from the start; later updates glide.
    if(l.a.key!=actKey_){actKey_=l.a.key;if(s.reducedMotion)actProgress_.reset(target,now);else{actProgress_.reset(0,now);actProgress_.retarget(target,now,{1,64,14});}}
    else if(std::abs(actProgress_.target()-target)>1e-4){if(s.reducedMotion)actProgress_.reset(target,now);else actProgress_.retarget(target,now,{1,52,13.5});}
    auto clipRight=animation(actProgress_,now,span*k,x0*k);actFillClip_->SetRight(clipRight.Get());actFill_->SetOffsetX(0.f);
    const bool head=!indeterminate&&target>0&&!l.done;auto hx=animation(actProgress_,now,span*k,(x0-20)*k);actHead_->SetOffsetX(hx.Get());actHead_->SetContent(head?actHeadSurface_.Get():nullptr);
    auto tx=animation(actProgress_,now,span*k,(x0-16)*k);actTracker_->SetOffsetX(tx.Get());actTracker_->SetContent(indeterminate||(l.done&&l.end)?nullptr:actTrackerSurface_.Get());
    // Alive: the tracker bobs a little and a track that can't say how far shimmers along (compositor loops, none in reduced motion).
    const bool moving=!s.reducedMotion&&!l.done;
    if(moving){ComPtr<IDCompositionAnimation> bob;check(device_->CreateAnimation(&bob));check(bob->SetAbsoluteBeginTime(ticks(now)));check(bob->AddSinusoidal(0,0,1.25f*k,.62f,0));actBob_->SetOffsetY(bob.Get());}else actBob_->SetOffsetY(0.f);
    actShimmerClip_->SetLeft(x0*k);actShimmerClip_->SetRight(x1*k);actShimmerClip_->SetTop(0.f);actShimmerClip_->SetBottom(32*k);actShimmerEffect_->SetOpacity(indeterminate?(s.light?.55f:.7f):0.f);
    if(indeterminate&&moving){const double period=1.7;ComPtr<IDCompositionAnimation> run;check(device_->CreateAnimation(&run));check(run->SetAbsoluteBeginTime(ticks(now)));
        check(run->AddCubic(0,(x0-90)*k,float((span+90)*k/period),0,0));check(run->AddRepeat(period,period));actShimmer_->SetOffsetX(run.Get());}
    else actShimmer_->SetOffsetX(std::round((x0+span/2-45)*k));
    actMoving_=moving;actEffect_->SetOpacity(1.f);(void)ink;(void)track;
}
}
