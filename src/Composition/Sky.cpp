#include "Renderer.h"
#include <ctime>
#include <random>
namespace nexus {
// Phase 5F: the weather tile's sky. A small layer over the Home weather statistic plays the
// current conditions: sun rays turning, stars twinkling, clouds drifting, rain streaks, snow,
// fog bands and storm flashes. Everything is a DirectComposition animation that loops with
// AddRepeat, so no frame is drawn by the app; it plays for a minute each time the tile appears
// (the particles fade out at the end), then rests, so an open Home page never keeps the GPU busy.
namespace {
constexpr float tileW=120,tileH=68;
constexpr double skyRun=60;
}
void Renderer::skyHide(){
    if(!sky_||!skyShown_)return;
    skyEffect_->SetOpacity(0.f);for(auto& p:skyParts_){p.visual->SetContent(nullptr);}skyShown_=false;skyKey_.clear();
}
// 0.18: the town's weather on the glass. Rain (heavier in a storm, lighter as drizzle) falls as streaks and beads as
// drops; fog drifts; snow falls slowly. Only on glass, only with the setting, and dimmer while the island is open so the
// text stays clear. With reduced motion nothing falls or drifts: the drops and fog stay still.
void Renderer::updateWeatherGlass(const ContentSnapshot& s){
    if(!weatherFx_)return;
    const Sky sky=s.weather.valid?skyOf(s.weather.code):Sky::Clear;const bool glass=material_!=0,wet=sky==Sky::Rain||sky==Sky::Drizzle||sky==Sky::Storm,cold=sky==Sky::Snow,foggy=sky==Sky::Fog;
    const bool on=glass&&s.settings.weatherGlass&&s.settings.weather&&(wet||cold||foggy);
    const std::string key=on?std::to_string(int(sky))+'|'+std::to_string(int(s.light))+'|'+std::to_string(int(s.reducedMotion)):std::string();
    // Open, the weather steps back behind the text.
    weatherFxEffect_->SetOpacity(on?(s.expanded?.55f:1.f):0.f);
    if(key==weatherFxKey_)return;weatherFxKey_=key;
    for(auto& t:fxTiles_)t->SetContent(nullptr);dropsFx_->SetContent(nullptr);rainFx_->SetOffsetY(0.f);rainFx_->SetOffsetX(0.f);fogFx_->SetOffsetX(0.f);
    if(!on)return;
    const float W=canvasWidth,H=canvasHeight,px=scale_;const UINT32 ink=s.light?0x3a4658:0xffffff;const float strength=sky==Sky::Storm?1.4f:sky==Sky::Drizzle?.55f:1.f;
    std::mt19937 random(0xa11ce5u);std::uniform_real_distribution<float> u(0.f,1.f);
    if(wet||cold){
        // One tile of streaks (or flakes), drawn twice, one above the other, and scrolled down by its own height forever.
        surface(rainTile_,int(W),int(H),[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(ink,.2f),&b));
            const int n=cold?170:int(300*strength);
            for(int k=0;k<n;++k){const float x=u(random)*W,y=u(random)*H;
                if(cold){const float r=1.1f+u(random)*1.7f;b->SetOpacity((s.light?.55f:.7f)*(.55f+u(random)*.45f));rt->FillEllipse(D2D1::Ellipse({x,y},r,r),b.Get());}
                else{const float len=8+u(random)*14*strength;b->SetOpacity((s.light?.30f:.26f)*(.55f+u(random)*.6f)*std::min(1.f,strength));rt->DrawLine({x,y},{x-len*.18f,y+len},b.Get(),.9f+u(random)*.5f);
                    // The tile wraps: a streak running off its bottom continues at its top.
                    if(y+len>H)rt->DrawLine({x,y-H},{x-len*.18f,y-H+len},b.Get(),.9f);}}});
        for(int k=0;k<2;++k){fxTiles_[size_t(k)]->SetContent(rainTile_.Get());fxTiles_[size_t(k)]->SetOffsetY(std::round(float(k-1)*H*px));}
        if(!s.reducedMotion){const double period=cold?7.:sky==Sky::Drizzle?1.1:sky==Sky::Storm?.5:.75;const double t0=seconds();
            auto loop=[&](float from,float to){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(t0)));check(a->AddCubic(0,from,float((to-from)/period),0,0));check(a->AddRepeat(period,period));return a;};
            auto down=loop(0,H*px);rainFx_->SetOffsetY(down.Get());if(!cold){auto side=loop(0,-H*.18f*px);rainFx_->SetOffsetX(side.Get());}}}
    if(wet){
        // Drops beaded on the pane (three in five within the resting island's height): a soft body with a bright crescent where the light catches it.
        surface(dropsTile_,int(W),int(canvasHeight),[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(ink,.1f),&b));const int n=int(46*strength);
            for(int k=0;k<n;++k){const float x=u(random)*W,y=k%5<3?6+u(random)*26:u(random)*canvasHeight,r=.9f+u(random)*u(random)*3.2f;
                b->SetColor(D2D1::ColorF(s.light?0x1c2433:0x000000,s.light?.10f:.22f));rt->FillEllipse(D2D1::Ellipse({x+.4f,y+.6f},r,r*1.1f),b.Get());
                b->SetColor(D2D1::ColorF(0xffffff,s.light?.45f:.28f));rt->DrawEllipse(D2D1::Ellipse({x,y},r,r*1.1f),b.Get(),.6f);
                b->SetColor(D2D1::ColorF(0xffffff,s.light?.75f:.55f));rt->FillEllipse(D2D1::Ellipse({x-r*.35f,y-r*.4f},r*.28f,r*.28f),b.Get());}});
        dropsFx_->SetContent(dropsTile_.Get());}
    if(foggy||(sky==Sky::Drizzle)){
        // Fog: soft banks of mist, two tiles side by side drifting slowly across.
        surface(fogTile_,int(W),int(canvasHeight),[&](auto* rt){const int n=foggy?16:7;
            for(int k=0;k<n;++k){const float x=u(random)*W,y=u(random)*canvasHeight*.5f,r=40+u(random)*90;D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(s.light?0xffffff:0xc9d2de,(foggy?(s.light?.34f:.16f):(s.light?.18f:.08f)))},{1,D2D1::ColorF(s.light?0xffffff:0xc9d2de,0.f)}};
                ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,2,&c));ComPtr<ID2D1RadialGradientBrush> g;check(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({x,y},{0,0},r*1.6f,r),c.Get(),&g));
                // Drawn again one tile to the left and right, so the tiles meet without a seam.
                for(float shift:{-W,0.f,W}){g->SetCenter({x+shift,y});rt->FillRectangle({x+shift-r*1.7f,y-r,x+shift+r*1.7f,y+r},g.Get());}}});
        for(int k=2;k<4;++k){fxTiles_[size_t(k)]->SetContent(fogTile_.Get());fxTiles_[size_t(k)]->SetOffsetX(std::round(float(k-2)*W*px));}
        if(!s.reducedMotion){const double period=foggy?60.:90.;ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(seconds())));check(a->AddCubic(0,0,float(-W*px/period),0,0));check(a->AddRepeat(period,period));fogFx_->SetOffsetX(a.Get());}}
}
void Renderer::skyScene(const ContentSnapshot& s,float x){
    // 0.17.0-preview.3: around sunrise and sunset at the chosen town the sky warms, and the sun sits low.
    const SunPhase phase=sunPhase(int64_t(std::time(nullptr)),s.weather.sunrise,s.weather.sunset,s.weather.day);
    const Sky sky=skyOf(s.weather.code);const bool day=phase==SunPhase::Day||phase==SunPhase::Dawn||phase==SunPhase::Dusk,low=phase==SunPhase::Dawn||phase==SunPhase::Dusk;
    const std::wstring key=std::to_wstring(int(sky))+L"|"+std::to_wstring(int(phase))+L"|"+std::to_wstring(int(day))+L"|"+std::to_wstring(int(x))+L"|"+std::to_wstring(int(s.light))+L"|"+std::to_wstring(material_)+L"|"+std::to_wstring(skyTile_)+L"|"+std::to_wstring(int(skyTileAlpha_*1000));
    if(skyShown_&&key==skyKey_)return;
    if(!sky_){
        check(device_->CreateVisual(&sky_));check(device_->CreateEffectGroup(&skyEffect_));sky_->SetEffect(skyEffect_.Get());check(device_->CreateRectangleClip(&skyClip_));
        const float r=13*scale_;auto* c=skyClip_.Get();c->SetLeft(0.f);c->SetTop(0.f);c->SetRight(std::round(tileW*scale_));c->SetBottom(std::round(tileH*scale_));
        c->SetTopLeftRadiusX(r);c->SetTopLeftRadiusY(r);c->SetTopRightRadiusX(r);c->SetTopRightRadiusY(r);c->SetBottomLeftRadiusX(r);c->SetBottomLeftRadiusY(r);c->SetBottomRightRadiusX(r);c->SetBottomRightRadiusY(r);
        sky_->SetClip(skyClip_.Get());check(content_->AddVisual(sky_.Get(),TRUE,nullptr));
        check(device_->CreateVisual(&skyBase_));check(sky_->AddVisual(skyBase_.Get(),FALSE,nullptr));
        for(auto& p:skyParts_){check(device_->CreateVisual(&p.visual));check(device_->CreateEffectGroup(&p.effect));p.visual->SetEffect(p.effect.Get());check(sky_->AddVisual(p.visual.Get(),FALSE,nullptr));}
        check(device_->CreateRotateTransform(&skyRays_));skyRays_->SetCenterX(32*scale_);skyRays_->SetCenterY(32*scale_);
    }
    skyKey_=key;skyShown_=true;
    surface(skyBaseSurface_,int(tileW),int(tileH),[&](auto* rt){rt->Clear(D2D1::ColorF(skyTile_,skyTileAlpha_));});skyBase_->SetContent(skyBaseSurface_.Get());
    // Colours: light glass and light solid take darker marks; dark takes pale ones. All stay faint enough for the text over them.
    const bool light=s.light;const UINT32 ink=light?0x1c2230:0xffffff,rain=light?0x3b7fc4:0x9fd0ff,sun=0xffc45a;
    const float cloudAlpha=light?.07f:.10f;
    for(auto* made:{std::addressof(skySun_),std::addressof(skyStreak_),std::addressof(skyFlake_),std::addressof(skyCloud_),std::addressof(skyFog_),std::addressof(skyStar_),std::addressof(skyFlash_)})made->Reset();
    surface(skySun_,64,64,[&](auto* rt){
        D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(sun,light?.45f:.38f)},{.45f,D2D1::ColorF(sun,.12f)},{1,D2D1::ColorF(sun,0.f)}};ComPtr<ID2D1GradientStopCollection> collection;check(rt->CreateGradientStopCollection(stops,3,&collection));
        ComPtr<ID2D1RadialGradientBrush> glow;check(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({32,32},{0,0},26,26),collection.Get(),&glow));rt->FillEllipse(D2D1::Ellipse({32,32},26,26),glow.Get());
        ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(sun,light?.75f:.6f),&b));ComPtr<ID2D1StrokeStyle> round;check(d2d_->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND),nullptr,0,&round));
        for(int k=0;k<12;++k){const float a=float(k)*3.14159265f/6,inner=k%2?11.5f:10.5f,outer=k%2?16.f:19.f;rt->DrawLine({32+inner*std::cos(a),32+inner*std::sin(a)},{32+outer*std::cos(a),32+outer*std::sin(a)},b.Get(),1.5f,round.Get());}
        b->SetColor(D2D1::ColorF(sun,light?.9f:.8f));rt->FillEllipse(D2D1::Ellipse({32,32},6.5f,6.5f),b.Get());});
    surface(skyStreak_,8,14,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(rain,light?.6f:.55f),&b));ComPtr<ID2D1StrokeStyle> round;check(d2d_->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND),nullptr,0,&round));rt->DrawLine({6,1.5f},{2.5f,12.5f},b.Get(),1.3f,round.Get());});
    surface(skyFlake_,6,6,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(light?0x7d93ad:0xffffff,light?.7f:.75f),&b));rt->FillEllipse(D2D1::Ellipse({3,3},2.2f,2.2f),b.Get());});
    surface(skyCloud_,48,22,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(ink,cloudAlpha),&b));
        // One silhouette (three puffs and a base), so overlaps never darken.
        ComPtr<ID2D1EllipseGeometry> e[3];check(d2d_->CreateEllipseGeometry(D2D1::Ellipse({14,14},9,7),&e[0]));check(d2d_->CreateEllipseGeometry(D2D1::Ellipse({25,10},11,9),&e[1]));check(d2d_->CreateEllipseGeometry(D2D1::Ellipse({36,14},9,7),&e[2]));
        ComPtr<ID2D1RoundedRectangleGeometry> base;check(d2d_->CreateRoundedRectangleGeometry(D2D1::RoundedRect({6,12,44,21},4.5f,4.5f),&base));
        ID2D1Geometry* parts[]={e[0].Get(),e[1].Get(),e[2].Get(),base.Get()};ComPtr<ID2D1GeometryGroup> group;check(d2d_->CreateGeometryGroup(D2D1_FILL_MODE_WINDING,parts,4,&group));rt->FillGeometry(group.Get(),b.Get());});
    surface(skyFog_,150,10,[&](auto* rt){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(ink,0.f)},{.5f,D2D1::ColorF(ink,light?.09f:.11f)},{1,D2D1::ColorF(ink,0.f)}};ComPtr<ID2D1GradientStopCollection> collection;check(rt->CreateGradientStopCollection(stops,3,&collection));
        ComPtr<ID2D1LinearGradientBrush> b;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,5},{150,5}),collection.Get(),&b));rt->FillRoundedRectangle(D2D1::RoundedRect({0,2,150,8},3,3),b.Get());});
    surface(skyStar_,5,5,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(light?0x55657a:0xffffff,.9f),&b));rt->FillEllipse(D2D1::Ellipse({2.5f,2.5f},1.3f,1.3f),b.Get());});
    surface(skyFlash_,int(tileW),int(tileH),[&](auto* rt){rt->Clear(D2D1::ColorF(light?0xffffff:0xdfe8ff,light?.35f:.2f));});
    // Dawn: lavender above, peach at the horizon; dusk: indigo above, a warm rose-orange glow low down.
    skyGlow_.Reset();if(low)surface(skyGlow_,int(tileW),int(tileH),[&](auto* rt){const bool dawn=phase==SunPhase::Dawn;const float a=light?.30f:.36f;
        D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(dawn?0x8f8ad6:0x4c4f9e,a*.35f)},{.55f,D2D1::ColorF(dawn?0xf3a6b8:0xe8708a,a*.7f)},{1,D2D1::ColorF(dawn?0xffc08a:0xff9a52,a)}};ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,3,&c));
        ComPtr<ID2D1LinearGradientBrush> b;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{0,tileH}),c.Get(),&b));rt->FillRectangle({0,0,tileW,tileH},b.Get());});
    const double now=seconds();
    // A linear run from `from` to `to` every `period` seconds, started `phase` seconds ago, until the run ends; it then holds
    // where it is (so nothing jumps when the scene comes to rest).
    auto loop=[&](double phase,double from,double to,double period){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now-phase)));const double end=phase+skyRun;
        check(a->AddCubic(0,float(from),float((to-from)/period),0,0));check(a->AddRepeat(period,period));check(a->End(end,float(from+(to-from)*std::fmod(end/period,1.))));return a;};
    // There and back (fog drifting, stars twinkling), holding where it is at the end.
    auto sway=[&](double phase,double a0,double a1,double half){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now-phase)));const float slope=float((a1-a0)/half);const double end=phase+skyRun,m=std::fmod(end,2*half);
        check(a->AddCubic(0,float(a0),slope,0,0));check(a->AddCubic(half,float(a1),-slope,0,0));check(a->AddRepeat(2*half,2*half));check(a->End(end,float(m<half?a0+(a1-a0)*m/half:a1-(a1-a0)*(m-half)/half)));return a;};
    // Particles fade out over the run's last second.
    auto fadeOut=[&](){ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now)));check(a->AddCubic(0,1,0,0,0));check(a->AddCubic(skyRun-1,1,-1,0,0));check(a->End(skyRun,0));return a;};
    for(auto& p:skyParts_){p.visual->SetContent(nullptr);p.visual->SetTransform(static_cast<IDCompositionTransform*>(nullptr));p.effect->SetOpacity(1.f);p.visual->SetOffsetX(0.f);p.visual->SetOffsetY(0.f);}
    size_t next=0;auto part=[&]()->SkyPart&{return skyParts_[std::min(next++,skyParts_.size()-1)];};
    const float px=scale_;
    // The warm light sits beneath everything else in the scene.
    if(low){auto& p=part();p.visual->SetContent(skyGlow_.Get());}
    auto sunAt=[&](float cx,float cy,double speed){auto& p=part();p.visual->SetContent(skySun_.Get());p.visual->SetOffsetX(std::round((cx-32)*px));p.visual->SetOffsetY(std::round((cy-32)*px));p.visual->SetTransform(skyRays_.Get());
        auto turn=loop(0,0,360,360/speed);skyRays_->SetAngle(turn.Get());};
    auto cloud=[&](float y,double period,double phase){auto& p=part();p.visual->SetContent(skyCloud_.Get());p.visual->SetOffsetY(std::round(y*px));auto drift=loop(phase,-50*px,(tileW+4)*px,period);p.visual->SetOffsetX(drift.Get());};
    auto streaks=[&](int count,double period,bool snow){
        for(int k=0;k<count;++k){auto& p=part();p.visual->SetContent(snow?skyFlake_.Get():skyStreak_.Get());const double phase=period*std::fmod(k*.618034,1.);const float x0=6+float(k)*(tileW-12)/float(count)+(k%2?4.f:0.f);
            auto fall=loop(phase,-16*px,(tileH+4)*px,period*(1+.13*(k%3)));p.visual->SetOffsetY(fall.Get());
            auto side=loop(phase,(x0+(snow?-3:3))*px,(x0+(snow?5:-3))*px,period*(1+.13*(k%3)));p.visual->SetOffsetX(side.Get());auto fade=fadeOut();p.effect->SetOpacity(fade.Get());}};
    switch(sky){
    case Sky::Clear:
        // The sun sits in the corner, above the place name.
        if(low)sunAt(tileW-10,tileH-4,8);else if(day)sunAt(tileW-6,3,12);
        else{const float spots[][2]={{tileW-22,10},{tileW-40,22},{tileW-12,30},{tileW-58,8},{tileW-30,44}};for(int k=0;k<5;++k){auto& p=part();p.visual->SetContent(skyStar_.Get());p.visual->SetOffsetX(std::round(spots[k][0]*px));p.visual->SetOffsetY(std::round(spots[k][1]*px));auto twinkle=sway(k*.37,.25,1,.9+k*.23);p.effect->SetOpacity(twinkle.Get());}}
        break;
    case Sky::PartlyCloudy:if(low)sunAt(tileW-10,tileH-4,7);else if(day)sunAt(tileW-7,4,9);cloud(18,34,8);cloud(38,48,30);break;
    case Sky::Cloudy:cloud(8,30,4);cloud(26,44,26);cloud(42,38,15);break;
    case Sky::Fog:for(int k=0;k<3;++k){auto& p=part();p.visual->SetContent(skyFog_.Get());p.visual->SetOffsetY(std::round((14+k*18)*px));auto drift=sway(k*1.7,-34*px,4*px,5.5+k*1.3);p.visual->SetOffsetX(drift.Get());}break;
    case Sky::Drizzle:cloud(4,40,10);streaks(7,1.25,false);break;
    case Sky::Rain:cloud(4,36,10);streaks(11,.72,false);break;
    case Sky::Snow:streaks(10,4.6,true);break;
    default:{// Storm: heavy rain and, every few seconds, a double flash.
        cloud(2,30,6);streaks(10,.6,false);auto& p=part();p.visual->SetContent(skyFlash_.Get());
        ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now)));
        check(a->AddCubic(0,0,0,0,0));check(a->AddCubic(2.6,1,-10,0,0));check(a->AddCubic(2.7,0,0,0,0));check(a->AddCubic(2.85,.65,-6.5,0,0));check(a->AddCubic(2.95,0,0,0,0));check(a->AddRepeat(7,7));check(a->End(skyRun,0));p.effect->SetOpacity(a.Get());break;}
    }
    sky_->SetOffsetX(std::round(x*scale_));sky_->SetOffsetY(std::round(126*scale_));skyEffect_->SetOpacity(1.f);
}
}
