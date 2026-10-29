#include "Island/IslandWindow.h"
#include "Media/CoverCodec.h"
#include <chrono>
#include <functional>

// 0.27 (revision 10): a phone's live activities (notice 7) on the island. The compact island shows the one that matters while
// they run; the card opens on a hover or a click, by itself for a few seconds when one changes in a way worth seeing (what it
// says, a segment or a point passed), and once more when one reaches its end ("Delivered", a check) before the island
// shrinks back. Several: the newest change shows, a count says how many, the wheel or a click goes through them.
namespace nexus {
namespace {
int64_t unixMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
// The Settings model's "Live activities from my phone" (phoneLive): on until the setting exists.
template<class S> bool phoneLiveOn(const S& s){if constexpr(requires{s.phoneLive;})return bool(s.phoneLive);else return true;}
// Worth opening the card for: what it says changed, or it went past a segment's end or a point.
bool meaningful(const LiveActivity& was,const LiveActivity& now){
    if((now.status!=was.status&&!now.status.empty())||(now.title!=was.title&&!now.title.empty()))return true;
    const double a=was.fraction(),b=now.fraction();if(a<0||b<0||b<=a)return false;
    for(double e:now.segmentEnds())if(e<.9999&&a<e&&b>=e)return true;for(auto& p:now.points){const double x=p.position/1000.;if(a<x&&b>=x)return true;}
    return false;
}
}
bool IslandWindow::liveAllowed()const{return settings_.phoneNotices&&phoneLiveOn(settings_);}
void IslandWindow::phoneLiveNotice(const std::string& peer,const std::wstring& phone,const std::vector<uint8_t>& notice){
    if(!renderer_)return;LiveActivity a;if(!parseLive(notice,a)){store_.log("Warning","phone_live_malformed");return;}
    auto& lives=content_.lives;if(!liveAllowed()){while(!lives.empty())liveRemove(lives.size()-1);return;}
    const double now=seconds();const bool card=state_==IslandState::Notification&&content_.notice.kind==25;
    auto it=std::find_if(lives.begin(),lives.end(),[&](auto& l){return l.peer==peer&&l.a.key==a.key;});
    if(a.op==2){if(it==lives.end())return;const size_t i=size_t(it-lives.begin());
        // It reached its end: a moment of "Delivered" with a check, then the island shrinks back. Otherwise it simply goes.
        if(!it->done&&it->a.fraction()>=.98){it->done=it->ended=true;it->endedAt=it->changedAt=now;it->a.progress=1000;if(now-livePickedAt_>30||!card)content_.liveShown=int(i);
            if(content_.liveShown==int(i)&&(state_==IslandState::Compact||card))liveShow(5.5,true);else if(state_==IslandState::Compact){refresh();animate();}store_.log("Info","phone_live_done");}
        else liveRemove(i);
        return;}
    const bool added=it==lives.end();
    if(added){if(lives.size()>=8)liveRemove(0);ContentSnapshot::Live fresh;fresh.peer=peer;fresh.at=now;lives.push_back(std::move(fresh));it=lives.end()-1;}
    auto& l=*it;const LiveActivity was=l.a;l.phone=phone;
    // Each picture is decoded once, when it changes.
    auto picture=[&](const std::vector<uint8_t>& before,const std::vector<uint8_t>& after,std::shared_ptr<const Artwork>& art,UINT side){if(added||before!=after)art=after.empty()?nullptr:decodeCover(after,side);};
    picture(was.appIcon,a.appIcon,l.icon,64);picture(was.trackerIcon,a.trackerIcon,l.tracker,64);picture(was.startIcon,a.startIcon,l.start,48);picture(was.endIcon,a.endIcon,l.end,48);picture(was.largeIcon,a.largeIcon,l.large,112);picture(was.snapshot,a.snapshot,l.snapshot,480);
    const bool change=added||meaningful(was,a);l.a=std::move(a);l.done=l.ended=false;if(change)l.changedAt=now;
    const int index=int(it-lives.begin());if(change&&(now-livePickedAt_>30||added))content_.liveShown=index;const bool shown=content_.liveShown==index;
    if(card){if(shown){liveNotice();if(change)events_.publish({ActivityKind::Notification,"live",60,25.,2.4,5},now);}refresh();animate();}
    else if(change&&shown&&state_==IslandState::Compact&&!content_.command.active&&!fullscreenHidden_)liveShow(5,added);
    else if(state_==IslandState::Compact){refresh();animate();}
    if(added)store_.log("Info","phone_live_shown");
}
// The card's notice: what the alert machinery and screen readers see of it (the renderer draws the activity itself).
void IslandWindow::liveNotice(){
    const auto* l=content_.liveNow();if(!l)return;auto& n=content_.notice;n={};n.kind=25;n.phone=true;n.peer=l->peer;n.key=l->a.key;n.app=liveTitle(*l);
    n.detail=l->done?l->a.title:(l->a.text.empty()?l->a.status:l->a.text);n.source=l->a.app+(l->phone.empty()?L"":L"  \u00b7  "+l->phone);n.icon=l->large?l->large:l->icon;n.progress=l->done?1.:l->a.fraction();
}
void IslandWindow::liveShow(double duration,bool glint){
    if(!renderer_||!content_.liveNow())return;liveNotice();content_.pinned=false;
    const Activity a{ActivityKind::Notification,"live",60,25.,std::min(2.4,duration),duration};if(holdCard(a))return;events_.publish(a,seconds());
    transition(IslandState::Notification);presentActivity();if(glint)alertSplash();
}
// Before every move of the springs: the compact island wide enough for the activity (not while a level shows), the card as
// tall as the activity needs, and the clock running while any is live.
void IslandWindow::liveLayout(){
    content_.settings=settings_;const double base=settings_.uiMode==0?72:settings_.compactWidth;
    if(content_.liveCompact()&&!content_.hud){motion_.compactWidth=content_.liveWidth();liveWidened_=true;}else if(liveWidened_){liveWidened_=false;if(!content_.hud)motion_.compactWidth=base;}
    const auto* l=content_.liveNow();motion_.cardHeight=state_==IslandState::Notification&&content_.notice.kind==25&&l?liveCardHeight(*l):92;
    const bool timer=!content_.lives.empty();if(timer!=liveTimer_){liveTimer_=timer;if(timer)SetTimer(window_,LiveTimer,1000,nullptr);else KillTimer(window_,LiveTimer);}
}
void IslandWindow::liveRemove(size_t i){
    auto& lives=content_.lives;if(i>=lives.size())return;const bool card=state_==IslandState::Notification&&content_.notice.kind==25,mine=card&&content_.notice.key==lives[i].a.key&&content_.notice.peer==lives[i].peer;
    lives.erase(lives.begin()+std::ptrdiff_t(i));if(content_.liveShown>int(i))--content_.liveShown;content_.liveShown=std::clamp(content_.liveShown,0,std::max(0,int(lives.size())-1));
    if(mine){if(lives.empty()){events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);}else liveNotice();}
    if(state_==IslandState::Compact||card)refresh();animate();
}
void IslandWindow::livePick(int delta){
    const int n=int(content_.lives.size());if(n<2)return;content_.liveShown=((content_.liveShown+delta)%n+n)%n;livePickedAt_=seconds();
    if(state_==IslandState::Notification&&content_.notice.kind==25){liveNotice();events_.publish({ActivityKind::Notification,"live",60,25.,2.4,4},seconds());}
    refresh();animate();
}
// Once a second while any is live: gone with its phone (or the settings), a finish shown long enough, the ETA moving on.
void IslandWindow::liveTick(){
    const double now=seconds();auto& lives=content_.lives;const bool card=state_==IslandState::Notification&&content_.notice.kind==25;
    for(size_t i=lives.size();i-->0;){const auto& l=lives[i];const bool qa=testing_&&l.peer.rfind("qa",0)==0;
        const bool here=qa||std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==l.peer&&p.online;});
        const bool held=card&&content_.notice.key==l.a.key&&interaction_!=InteractionState::Rest;
        if(!liveAllowed()||!here||(l.done&&now-l.endedAt>5.6&&!held))liveRemove(i);}
    if(const auto* l=content_.liveNow()){const auto eta=liveEta(l->a,unixMs());if(eta!=liveEtaShown_){liveEtaShown_=eta;if(state_==IslandState::Compact||card)refresh();}}
    liveLayout();
}
bool IslandWindow::liveAction(Action a){
    if(a==Action::LiveOpen){if(content_.liveNow())liveShow(4);return true;}
    if(a==Action::LiveNext){livePick(1);return true;}
    return false;
}
bool IslandWindow::liveMessage(UINT m,WPARAM w,LPARAM l){
    (void)l;if(m==WM_TIMER&&w==LiveTimer){liveTick();return true;}
    if(content_.lives.empty())return false;const bool card=state_==IslandState::Notification&&content_.notice.kind==25&&content_.liveNow();
    // Resting on the compact island opens the activity's card (the Command Center is a click on the card away).
    if(m==WM_TIMER&&w==7&&state_==IslandState::Compact&&content_.liveCompact()&&settings_.hoverOpen&&interaction_==InteractionState::Hover){KillTimer(window_,7);if(settings_.autoHide&&!pointerOffEdge()){edgeHold_=true;return true;}liveShow(3);return true;}
    // Left resting, the card closes like any (its activity stays in the compact island).
    if(m==WM_TIMER&&w==8&&card&&interaction_==InteractionState::Rest&&!content_.pinned&&seconds()-cardShownAt_>=2.4){KillTimer(window_,8);events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);return true;}
    // The wheel or a two-finger swipe: the next or the previous of several (sideways on the compact island too, not a track skip).
    if((m==WM_MOUSEWHEEL&&card)||(m==WM_MOUSEHWHEEL&&(card||(state_==IslandState::Compact&&content_.liveCompact())))){if(content_.lives.size()<2)return m==WM_MOUSEHWHEEL;
        const double now=seconds();if(now-livePickedAt_>.45){liveWheel_+=GET_WHEEL_DELTA_WPARAM(w)*(m==WM_MOUSEHWHEEL?1:-1);if(std::abs(liveWheel_)>=120){livePick(liveWheel_>0?1:-1);liveWheel_=0;}}return true;}
    // A click on the card itself (not one of its buttons): the next of several; with one, the Command Center.
    if(m==WM_LBUTTONUP&&card&&interaction_==InteractionState::Pressed&&pressedAction_==Action::None){if(content_.lives.size()>1)livePick(1);else{content_.page=Page::Overview;PostMessageW(window_,WM_APP+50,2,0);}}
    return false;
}
// --qa-activity[=compact|delivered|several|snapshot]: a made-up delivery from a made-up app on a made-up phone, through the real
// notice path (encoded as the phone sends it), with pictures painted here. Default: its card, held open.
namespace {
struct LiveWriter{std::vector<uint8_t> b;void u8(uint8_t v){b.push_back(v);}void u16(uint16_t v){u8(uint8_t(v));u8(uint8_t(v>>8));}void u32(uint32_t v){for(int k=0;k<4;++k)u8(uint8_t(v>>(8*k)));}void u64(uint64_t v){u32(uint32_t(v));u32(uint32_t(v>>32));}
    void blob(const std::vector<uint8_t>& v){u32(uint32_t(v.size()));b.insert(b.end(),v.begin(),v.end());}void text(const std::wstring& t){blob(utf8(t));}
    static std::vector<uint8_t> utf8(const std::wstring& t){const int n=WideCharToMultiByte(CP_UTF8,0,t.c_str(),int(t.size()),nullptr,0,nullptr,nullptr);std::vector<uint8_t> v(size_t(std::max(0,n)));if(n>0)WideCharToMultiByte(CP_UTF8,0,t.c_str(),int(t.size()),reinterpret_cast<char*>(v.data()),n,nullptr,nullptr);return v;}};
std::vector<uint8_t> encodeLive(const LiveActivity& a){LiveWriter w;w.u8(0x30);w.u8(7);w.u8(uint8_t(a.op));w.blob({a.key.begin(),a.key.end()});if(a.op==2)return w.b;
    w.text(a.app);w.text(a.package);w.text(a.title);w.text(a.text);w.text(a.status);w.u8(a.flags);w.u16(a.progress);w.u8(uint8_t(a.segments.size()));for(auto& s:a.segments){w.u16(s.length);w.u32(s.argb);}
    w.u8(uint8_t(a.points.size()));for(auto& p:a.points){w.u16(p.position);w.u32(p.argb);}w.u64(a.when);w.u32(a.accent);for(auto* v:{&a.appIcon,&a.trackerIcon,&a.startIcon,&a.endIcon,&a.largeIcon,&a.snapshot})w.blob(*v);return w.b;}
// A picture painted with Direct2D, as PNG bytes (the phone's pictures arrive as PNG or JPEG).
std::vector<uint8_t> painted(UINT w,UINT h,const std::function<void(ID2D1RenderTarget*,ID2D1Factory*)>& draw){
    ComPtr<IWICImagingFactory> wic;ComPtr<IWICBitmap> bitmap;ComPtr<ID2D1Factory> f;ComPtr<ID2D1RenderTarget> rt;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)))||FAILED(wic->CreateBitmap(w,h,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap))||FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,f.GetAddressOf())))return {};
    if(FAILED(f->CreateWicBitmapRenderTarget(bitmap.Get(),D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED)),&rt)))return {};
    rt->BeginDraw();rt->Clear(D2D1::ColorF(0,0));draw(rt.Get(),f.Get());if(FAILED(rt->EndDraw()))return {};
    Artwork art;art.width=w;art.height=h;art.pixels.resize(size_t(w)*h*4);if(FAILED(bitmap->CopyPixels(nullptr,w*4,UINT(art.pixels.size()),art.pixels.data())))return {};
    return encodeCover(art,std::max(w,h),1u<<20,.9f,true);
}
}
void IslandWindow::qaActivity(const std::wstring& cmd){
    settings_.phoneNotices=true;content_.settings=settings_;const std::string peer="qa-phone";const std::wstring phone=L"Pixel 9";
    auto brush=[](ID2D1RenderTarget* rt,UINT32 c,float a=1){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(c,a),&b);return b;};
    LiveActivity a;a.op=1;a.key="qa-delivery";a.app=L"Tiffin";a.package=L"com.example.tiffin";a.title=L"Order picked up";a.text=L"Ravi is riding over with 2 items from Masala Corner";a.status=L"Arriving in 12 mins";
    a.flags=LiveActivity::Ongoing|LiveActivity::Promoted|LiveActivity::HasProgress|LiveActivity::CountsDown|LiveActivity::Delivery;a.progress=620;a.accent=0xffff7a2f;
    a.segments={{240,0xffff9f45},{260,0xffffc145},{500,0xff34c77b}};a.points={{240,0xffff9f45},{500,0xffffc145}};a.when=uint64_t(unixMs()+12*60*1000+20000);
    // The app: an orange tile with a bowl and its steam.
    a.appIcon=painted(64,64,[&](ID2D1RenderTarget* rt,ID2D1Factory* f){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xffb347)},{1,D2D1::ColorF(0xff6a1f)}};ComPtr<ID2D1GradientStopCollection> c;rt->CreateGradientStopCollection(stops,2,&c);
        ComPtr<ID2D1LinearGradientBrush> g;rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{64,64}),c.Get(),&g);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,64,64},15,15),g.Get());
        ComPtr<ID2D1PathGeometry> bowl;f->CreatePathGeometry(&bowl);ComPtr<ID2D1GeometrySink> s;bowl->Open(&s);s->BeginFigure({14,32},D2D1_FIGURE_BEGIN_FILLED);s->AddArc({{50,32},{18,16},0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL});s->EndFigure(D2D1_FIGURE_END_CLOSED);s->Close();
        rt->FillGeometry(bowl.Get(),brush(rt,0xffffff).Get());rt->FillRoundedRectangle(D2D1::RoundedRect({11,29,53,33},2,2),brush(rt,0xffffff).Get());
        for(float x:{24.f,32.f,40.f})rt->DrawLine({x,13},{x,23},brush(rt,0xffffff,.85f).Get(),3);});
    // The restaurant: a warm plate of food, seen from above.
    a.largeIcon=painted(112,112,[&](ID2D1RenderTarget* rt,ID2D1Factory*){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0x8a3b12)},{1,D2D1::ColorF(0x2a1208)}};ComPtr<ID2D1GradientStopCollection> c;rt->CreateGradientStopCollection(stops,2,&c);
        ComPtr<ID2D1RadialGradientBrush> g;rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({50,46},{0,0},80,80),c.Get(),&g);rt->FillRectangle({0,0,112,112},g.Get());
        rt->FillEllipse(D2D1::Ellipse({56,58},38,38),brush(rt,0xf4efe6).Get());rt->FillEllipse(D2D1::Ellipse({56,58},29,29),brush(rt,0xe8a33b).Get());rt->FillEllipse(D2D1::Ellipse({50,52},14,12),brush(rt,0xd4651f).Get());
        for(auto [x,y]:{std::pair{66.f,50.f},std::pair{62.f,68.f},std::pair{44.f,66.f},std::pair{70.f,62.f}})rt->FillEllipse(D2D1::Ellipse({x,y},3.6f,3.6f),brush(rt,0x4caf50).Get());});
    // The rider: a scooter in the app's colour.
    a.trackerIcon=painted(64,64,[&](ID2D1RenderTarget* rt,ID2D1Factory* f){auto o=brush(rt,0xff7a2f),w=brush(rt,0xffffff);rt->DrawEllipse(D2D1::Ellipse({17,44},8,8),w.Get(),4.5f);rt->DrawEllipse(D2D1::Ellipse({48,44},8,8),w.Get(),4.5f);
        ComPtr<ID2D1PathGeometry> body;f->CreatePathGeometry(&body);ComPtr<ID2D1GeometrySink> s;body->Open(&s);s->BeginFigure({12,38},D2D1_FIGURE_BEGIN_FILLED);for(auto p:{D2D1_POINT_2F{20,28},D2D1_POINT_2F{38,28},D2D1_POINT_2F{42,18},D2D1_POINT_2F{50,18},D2D1_POINT_2F{54,38},D2D1_POINT_2F{40,40},D2D1_POINT_2F{30,36}})s->AddLine(p);s->EndFigure(D2D1_FIGURE_END_CLOSED);s->Close();
        rt->FillGeometry(body.Get(),o.Get());rt->FillRoundedRectangle(D2D1::RoundedRect({18,12,34,28},3,3),o.Get());rt->FillRoundedRectangle(D2D1::RoundedRect({21,15,31,25},2,2),w.Get());});
    auto disc=[&](UINT32 colour,Icon glyph){return painted(48,48,[&,colour,glyph](ID2D1RenderTarget* rt,ID2D1Factory* f){rt->FillEllipse(D2D1::Ellipse({24,24},24,24),brush(rt,colour).Get());drawIcon(rt,f,glyph,10,10,28,0xffffff);});};
    a.startIcon=disc(0xff9f45,Icon::Archive);a.endIcon=disc(0x3a8dde,Icon::Home);
    const bool compact=cmd.find(L"--qa-activity=compact")!=std::wstring::npos,delivered=cmd.find(L"--qa-activity=delivered")!=std::wstring::npos,several=cmd.find(L"--qa-activity=several")!=std::wstring::npos,snapshot=cmd.find(L"--qa-activity=snapshot")!=std::wstring::npos;
    if(snapshot){a.flags=LiveActivity::Ongoing|LiveActivity::Delivery;a.segments.clear();a.points.clear();a.trackerIcon.clear();a.startIcon.clear();a.endIcon.clear();a.status.clear();
        // The app's own view, drawn as a picture: a little map with the route and the rider on it.
        a.snapshot=painted(640,200,[&](ID2D1RenderTarget* rt,ID2D1Factory* f){rt->FillRectangle({0,0,640,200},brush(rt,0x1d2a33).Get());for(int k=0;k<9;++k){rt->DrawLine({float(k*80-40),0},{float(k*80+60),200},brush(rt,0x2c3d49).Get(),7);rt->DrawLine({0,float(k*30)},{640,float(k*30+40)},brush(rt,0x26353f).Get(),4);}
            ComPtr<ID2D1PathGeometry> route;f->CreatePathGeometry(&route);ComPtr<ID2D1GeometrySink> s;route->Open(&s);s->BeginFigure({70,150},D2D1_FIGURE_BEGIN_HOLLOW);s->AddBezier({{180,40},{330,190},{420,90}});s->AddBezier({{480,30},{540,70},{580,52}});s->EndFigure(D2D1_FIGURE_END_OPEN);s->Close();
            rt->DrawGeometry(route.Get(),brush(rt,0xff7a2f,.35f).Get(),14);rt->DrawGeometry(route.Get(),brush(rt,0xff9f45).Get(),5);rt->FillEllipse(D2D1::Ellipse({70,150},11,11),brush(rt,0xff9f45).Get());
            rt->FillEllipse(D2D1::Ellipse({580,52},13,13),brush(rt,0x3a8dde).Get());rt->FillEllipse(D2D1::Ellipse({420,90},15,15),brush(rt,0xffffff).Get());rt->FillEllipse(D2D1::Ellipse({420,90},9,9),brush(rt,0xff7a2f).Get());});}
    phoneLiveNotice(peer,phone,encodeLive(a));
    if(several){LiveActivity t;t.op=1;t.key="qa-timer";t.app=L"Clock";t.package=L"com.example.clock";t.title=L"Pasta";t.text=L"Timer  \u00b7  10 min";t.flags=LiveActivity::Ongoing|LiveActivity::CountsDown;t.when=uint64_t(unixMs()+4*60*1000+59000);t.accent=0xff5ac8fa;
        t.appIcon=painted(64,64,[&](ID2D1RenderTarget* rt,ID2D1Factory* f){rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,64,64},15,15),brush(rt,0x1e88e5).Get());drawIcon(rt,f,Icon::Clock,12,12,40,0xffffff);});
        phoneLiveNotice(peer,phone,encodeLive(t));content_.liveShown=0;livePickedAt_=seconds();}
    if(delivered){a.progress=1000;a.title=L"Order delivered";a.status=L"Enjoy your meal";phoneLiveNotice(peer,phone,encodeLive(a));LiveActivity end;end.op=2;end.key=a.key;phoneLiveNotice(peer,phone,encodeLive(end));return;}
    if(compact){events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);refresh();animate();return;}
    liveShow(600);
}
}
