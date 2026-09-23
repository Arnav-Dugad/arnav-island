#include "Renderer.h"
#include <dxgi1_2.h>
#include "Composition/DockGeometry.h"

namespace nexus {
void Renderer::initialize(HWND hwnd,float dpi) {
    dpi_=dpi;scale_=dpi/96;
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr);
    if(FAILED(hr)){software=true;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr));}
    check(d3d_.As(&dxgi_));check(DCompositionCreateDevice(dxgi_.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(device_.GetAddressOf())));
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,d2d_.GetAddressOf()));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write_.GetAddressOf())));textParams_=sharpTextParams(write_.Get());
    check(device_->CreateTargetForHwnd(hwnd,TRUE,&target_));glass_.initialize(hwnd,scale_,canvasWidth,canvasHeight);
    for(auto* p:{std::addressof(root_),std::addressof(body_),std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_),std::addressof(art_),std::addressof(wingLeft_),std::addressof(wingRight_),std::addressof(pulseVisual_),std::addressof(hoverVisual_)})check(device_->CreateVisual(p->GetAddressOf()));
    check(root_->AddVisual(wingLeft_.Get(),FALSE,nullptr));check(root_->AddVisual(wingRight_.Get(),FALSE,nullptr));check(device_->CreateVisual(&stage_));// Soft borders antialias every rounded clip and bitmap edge below (body corners, artwork, pills).
    check(stage_->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT));check(stage_->AddVisual(root_.Get(),FALSE,nullptr));check(target_->SetRoot(stage_.Get()));check(device_->CreateEffectGroup(&stageEffect_));check(stage_->SetEffect(stageEffect_.Get()));check(root_->AddVisual(body_.Get(),FALSE,nullptr));
    for(auto* p:{std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_),std::addressof(art_),std::addressof(pulseVisual_),std::addressof(hoverVisual_)})check(body_->AddVisual(p->Get(),FALSE,nullptr));
    check(device_->CreateRectangleClip(&clip_));check(clip_->SetLeft(0.f));check(clip_->SetTop(0.f));check(body_->SetClip(clip_.Get()));
    check(device_->CreateRectangleClip(&innerClip_));check(innerClip_->SetLeft(0.f));check(innerClip_->SetTop(0.f));check(inner_->SetClip(innerClip_.Get()));
    check(inner_->SetOffsetX(1));check(inner_->SetOffsetY(1));
    check(content_->SetOffsetX(std::round(20*scale_)));check(content_->SetOffsetY(std::round(38*scale_)));
    check(bar_->SetOffsetX(58*scale_));check(bar_->SetOffsetY(253*scale_));
    check(device_->CreateRectangleClip(&barClip_));check(barClip_->SetLeft(0.f));check(barClip_->SetTop(0.f));check(bar_->SetClip(barClip_.Get()));
    check(barClip_->SetBottom(2*scale_));
    check(device_->CreateEffectGroup(&contentEffect_));
    check(content_->SetEffect(contentEffect_.Get()));check(device_->CreateEffectGroup(&barEffect_));check(bar_->SetEffect(barEffect_.Get()));
    check(device_->CreateRectangleClip(&artClip_));check(artClip_->SetLeft(0.f));check(artClip_->SetTop(0.f));check(artClip_->SetRight(256*scale_));check(artClip_->SetBottom(256*scale_));
    check(artClip_->SetTopLeftRadiusX(32.f*scale_));check(artClip_->SetTopLeftRadiusY(32.f*scale_));check(artClip_->SetTopRightRadiusX(32.f*scale_));check(artClip_->SetTopRightRadiusY(32.f*scale_));check(artClip_->SetBottomLeftRadiusX(32.f*scale_));check(artClip_->SetBottomLeftRadiusY(32.f*scale_));check(artClip_->SetBottomRightRadiusX(32.f*scale_));check(artClip_->SetBottomRightRadiusY(32.f*scale_));check(art_->SetClip(artClip_.Get()));
    for(auto pair:{std::pair{std::addressof(headerEffect_),header_.Get()},std::pair{std::addressof(artEffect_),art_.Get()},std::pair{std::addressof(pulseEffect_),pulseVisual_.Get()},std::pair{std::addressof(hoverEffect_),hoverVisual_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));check(pair.second->SetEffect(pair.first->Get()));}
    for(auto pair:{std::pair{std::addressof(artScale_),art_.Get()},std::pair{std::addressof(leftScale_),wingLeft_.Get()},std::pair{std::addressof(rightScale_),wingRight_.Get()},std::pair{std::addressof(hoverScale_),hoverVisual_.Get()}}){check(device_->CreateScaleTransform(pair.first->GetAddressOf()));check(pair.second->SetTransform(pair.first->Get()));}
    for(auto* p:{std::addressof(iconLayer_),std::addressof(artFrom_),std::addressof(artTo_),std::addressof(nav_),std::addressof(rings_),std::addressof(batteryDot_),std::addressof(timerDot_)})check(device_->CreateVisual(p->GetAddressOf()));
    check(art_->AddVisual(artFrom_.Get(),FALSE,nullptr));check(art_->AddVisual(artTo_.Get(),FALSE,nullptr));
    check(body_->AddVisual(nav_.Get(),FALSE,nullptr));check(body_->AddVisual(iconLayer_.Get(),FALSE,nullptr));check(body_->AddVisual(rings_.Get(),FALSE,nullptr));check(rings_->AddVisual(batteryDot_.Get(),FALSE,nullptr));check(rings_->AddVisual(timerDot_.Get(),FALSE,nullptr));
    for(auto pair:{std::pair{std::addressof(iconEffect_),iconLayer_.Get()},std::pair{std::addressof(incomingEffect_),artTo_.Get()},std::pair{std::addressof(navEffect_),nav_.Get()},std::pair{std::addressof(ringsEffect_),rings_.Get()},std::pair{std::addressof(batteryDotEffect_),batteryDot_.Get()},std::pair{std::addressof(timerDotEffect_),timerDot_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));check(pair.second->SetEffect(pair.first->Get()));}
    for(auto pair:{std::pair{std::addressof(batteryRotation_),batteryDot_.Get()},std::pair{std::addressof(timerRotation_),timerDot_.Get()}}){check(device_->CreateRotateTransform(pair.first->GetAddressOf()));pair.first->Get()->SetCenterX(16*scale_);pair.first->Get()->SetCenterY(16*scale_);check(pair.second->SetTransform(pair.first->Get()));}
    batteryDot_->SetOffsetY(1*scale_);timerDot_->SetOffsetY(1*scale_);batteryDot_->SetOffsetX(-4*scale_);timerDot_->SetOffsetX(20*scale_);
    for(auto& i:icons_){check(device_->CreateVisual(&i.visual));check(device_->CreateScaleTransform(&i.scale));i.scale->SetCenterX(16*scale_);i.scale->SetCenterY(16*scale_);i.visual->SetTransform(i.scale.Get());check(device_->CreateEffectGroup(&i.effect));i.visual->SetEffect(i.effect.Get());iconLayer_->AddVisual(i.visual.Get(),FALSE,nullptr);}
    for(auto* v:{std::addressof(timeline_),std::addressof(seekTrack_),std::addressof(seekFill_),std::addressof(seekThumb_),std::addressof(dropGhost_)})check(device_->CreateVisual(v->GetAddressOf()));
    iconLayer_->AddVisual(timeline_.Get(),FALSE,nullptr);body_->AddVisual(dropGhost_.Get(),FALSE,nullptr);
    for(auto* v:{seekTrack_.Get(),seekFill_.Get(),seekThumb_.Get()})timeline_->AddVisual(v,FALSE,nullptr);
    for(auto pair:{std::pair{std::addressof(seekTrackScale_),seekTrack_.Get()},std::pair{std::addressof(seekFillScale_),seekFill_.Get()},std::pair{std::addressof(seekThumbScale_),seekThumb_.Get()},std::pair{std::addressof(dropScale_),dropGhost_.Get()}}){check(device_->CreateScaleTransform(pair.first->GetAddressOf()));pair.second->SetTransform(pair.first->Get());}
    check(device_->CreateEffectGroup(&timelineEffect_));timeline_->SetEffect(timelineEffect_.Get());check(device_->CreateEffectGroup(&dropEffect_));dropGhost_->SetEffect(dropEffect_.Get());dropEffect_->SetOpacity(0.f);
    seekTrackScale_->SetCenterY(2*scale_);seekFillScale_->SetCenterY(2*scale_);seekThumbScale_->SetCenterX(8*scale_);seekThumbScale_->SetCenterY(8*scale_);
    for(int i=0;i<3;++i){check(device_->CreateVisual(&atmosphere_[i]));check(device_->CreateEffectGroup(&atmosphereEffect_[i]));atmosphere_[i]->SetEffect(atmosphereEffect_[i].Get());body_->AddVisual(atmosphere_[i].Get(),FALSE,header_.Get());
        surface(atmosphereSurface_[i],640,500,[&](auto* rt){const UINT32 colors[]={0xff8175,0x82efb4,0x9baaff};D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(colors[i],.65f)},{1,D2D1::ColorF(colors[i],0.f)}};ComPtr<ID2D1GradientStopCollection> collection;check(rt->CreateGradientStopCollection(stops,2,&collection));ComPtr<ID2D1RadialGradientBrush> brush;check(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({120,50},{0,0},340,210),collection.Get(),&brush));rt->FillRectangle({0,0,640,500},brush.Get());});atmosphere_[i]->SetContent(atmosphereSurface_[i].Get());atmosphereEffect_[i]->SetOpacity(0.f);}
    check(device_->CreateVisual(&peek_));check(device_->CreateScaleTransform(&peekScale_));check(device_->CreateEffectGroup(&peekEffect_));peek_->SetTransform(peekScale_.Get());peek_->SetEffect(peekEffect_.Get());body_->AddVisual(peek_.Get(),FALSE,nullptr);peekEffect_->SetOpacity(0.f);peek_->SetOffsetX(234*scale_);peek_->SetOffsetY(82*scale_);peekScale_->SetCenterX(146*scale_);peekScale_->SetCenterY(60*scale_);
    for(auto* v:{std::addressof(spectrum_),std::addressof(meterLayer_),std::addressof(hudTrack_),std::addressof(hudFill_),std::addressof(artFrame_),std::addressof(badge_)})check(device_->CreateVisual(v->GetAddressOf()));
    body_->AddVisual(spectrum_.Get(),FALSE,nullptr);body_->AddVisual(meterLayer_.Get(),FALSE,nullptr);body_->AddVisual(hudTrack_.Get(),FALSE,nullptr);body_->AddVisual(hudFill_.Get(),FALSE,nullptr);body_->AddVisual(artFrame_.Get(),TRUE,art_.Get());artFrame_->AddVisual(badge_.Get(),FALSE,nullptr);
    for(auto pair:{std::pair{std::addressof(spectrumEffect_),spectrum_.Get()},std::pair{std::addressof(meterEffect_),meterLayer_.Get()},std::pair{std::addressof(badgeEffect_),badge_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));pair.second->SetEffect(pair.first->Get());pair.first->Get()->SetOpacity(0.f);}
    check(device_->CreateEffectGroup(&hudEffect_));hudTrack_->SetEffect(hudEffect_.Get());hudFill_->SetEffect(hudEffect_.Get());hudEffect_->SetOpacity(0.f);
    check(device_->CreateRectangleClip(&hudClip_));hudClip_->SetLeft(0.f);hudClip_->SetTop(0.f);hudClip_->SetBottom(4*scale_);hudFill_->SetClip(hudClip_.Get());for(auto* v:{hudTrack_.Get(),hudFill_.Get()})v->SetOffsetY(std::round(15*scale_));
    for(auto& bar:bars_){check(device_->CreateVisual(&bar.visual));check(device_->CreateScaleTransform(&bar.scale));bar.scale->SetCenterY(10*scale_);bar.visual->SetTransform(bar.scale.Get());spectrum_->AddVisual(bar.visual.Get(),FALSE,nullptr);bar.glide.p1=.15;}
    meterLayer_->SetOffsetX(std::round((20+156)*scale_));for(size_t i=0;i<meters_.size();++i){auto& m=meters_[i];check(device_->CreateVisual(&m.visual));check(device_->CreateScaleTransform(&m.scale));m.visual->SetTransform(m.scale.Get());m.visual->SetOffsetY(std::round((38+62+i*36+24)*scale_));meterLayer_->AddVisual(m.visual.Get(),FALSE,nullptr);}
    for(auto* v:{std::addressof(tabPill_),std::addressof(cardIcon_),std::addressof(energy_),std::addressof(cardRing_)})check(device_->CreateVisual(v->GetAddressOf()));
    // The card's level ring is wider than the content surface's origin allows, so it is its own layer that fades and moves with the content.
    check(content_->AddVisual(cardRing_.Get(),FALSE,nullptr));
    for(auto* v:{std::addressof(caret_),std::addressof(privacyBand_)})check(device_->CreateVisual(v->GetAddressOf()));
    for(auto pair:{std::pair{std::addressof(caretEffect_),caret_.Get()},std::pair{std::addressof(privacyEffect_),privacyBand_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));pair.second->SetEffect(pair.first->Get());pair.first->Get()->SetOpacity(0.f);}
    check(content_->AddVisual(caret_.Get(),FALSE,nullptr));check(body_->AddVisual(privacyBand_.Get(),FALSE,nullptr));privacyBand_->SetOffsetX(std::round(20*scale_));privacyBand_->SetOffsetY(std::round(7*scale_));cardRing_->SetOffsetX(std::round(-8*scale_));cardRing_->SetOffsetY(std::round(-8*scale_));
    body_->AddVisual(tabPill_.Get(),FALSE,content_.Get());body_->AddVisual(cardIcon_.Get(),TRUE,content_.Get());body_->AddVisual(energy_.Get(),TRUE,cardIcon_.Get());
    for(auto pair:{std::pair{std::addressof(tabEffect_),tabPill_.Get()},std::pair{std::addressof(cardIconEffect_),cardIcon_.Get()},std::pair{std::addressof(energyEffect_),energy_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));pair.second->SetEffect(pair.first->Get());pair.first->Get()->SetOpacity(0.f);}
    check(device_->CreateScaleTransform(&cardIconScale_));cardIconScale_->SetCenterX(28*scale_);cardIconScale_->SetCenterY(28*scale_);cardIcon_->SetTransform(cardIconScale_.Get());cardIcon_->SetOffsetX(std::round(20*scale_));cardIcon_->SetOffsetY(std::round(16*scale_));
    check(device_->CreateRotateTransform(&energyRotation_));energyRotation_->SetCenterX(36*scale_);energyRotation_->SetCenterY(36*scale_);energy_->SetTransform(energyRotation_.Get());
    surface(pulseSurface_,640,500,[](auto* rt){rt->Clear(D2D1::ColorF(0x80e8ba,.22f));});check(pulseVisual_->SetContent(pulseSurface_.Get()));
    // The hover highlight is a plain layer shaped by an animated rounded clip, so its corners stay true at any size.
    check(device_->CreateRectangleClip(&hoverClip_));hoverClip_->SetLeft(0.f);hoverClip_->SetTop(0.f);{const float r=10*scale_;auto* c=hoverClip_.Get();c->SetTopLeftRadiusX(r);c->SetTopLeftRadiusY(r);c->SetTopRightRadiusX(r);c->SetTopRightRadiusY(r);c->SetBottomLeftRadiusX(r);c->SetBottomLeftRadiusY(r);c->SetBottomRightRadiusX(r);c->SetBottomRightRadiusY(r);}
    check(hoverVisual_->SetClip(hoverClip_.Get()));
}
void Renderer::surface(ComPtr<IDCompositionSurface>& s,int w,int h,std::function<void(ID2D1RenderTarget*)> draw) {
    if(!s)check(device_->CreateSurface(UINT(std::ceil(w*scale_)),UINT(std::ceil(h*scale_)),DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_ALPHA_MODE_PREMULTIPLIED,&s));
    ComPtr<IDXGISurface> dx;POINT offset{};
    check(s->BeginDraw(nullptr,__uuidof(IDXGISurface),reinterpret_cast<void**>(dx.GetAddressOf()),&offset));
    try {
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),dpi_,dpi_);
        ComPtr<ID2D1RenderTarget> rt;check(d2d_->CreateDxgiSurfaceRenderTarget(dx.Get(),&properties,&rt));
        rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Translation(offset.x/scale_,offset.y/scale_));
        rt->PushAxisAlignedClip(D2D1::RectF(0,0,float(w),float(h)),D2D1_ANTIALIAS_MODE_ALIASED);rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);if(textParams_)rt->SetTextRenderingParams(textParams_.Get());rt->Clear(D2D1::ColorF(0,0));draw(rt.Get());rt->PopAxisAlignedClip();check(rt->EndDraw());
    }catch(...){s->EndDraw();throw;}
    check(s->EndDraw());++redraws;
}
void Renderer::pixelSurface(ComPtr<IDCompositionSurface>& s,int w,int h,std::function<void(ID2D1RenderTarget*)> draw){
    s.Reset();check(device_->CreateSurface(UINT(std::max(1,w)),UINT(std::max(1,h)),DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_ALPHA_MODE_PREMULTIPLIED,&s));
    ComPtr<IDXGISurface> dx;POINT offset{};check(s->BeginDraw(nullptr,__uuidof(IDXGISurface),reinterpret_cast<void**>(dx.GetAddressOf()),&offset));
    try{auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        ComPtr<ID2D1RenderTarget> rt;check(d2d_->CreateDxgiSurfaceRenderTarget(dx.Get(),&properties,&rt));rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Translation(float(offset.x),float(offset.y)));
        rt->PushAxisAlignedClip(D2D1::RectF(0,0,float(w),float(h)),D2D1_ANTIALIAS_MODE_ALIASED);rt->Clear(D2D1::ColorF(0,0));draw(rt.Get());rt->PopAxisAlignedClip();check(rt->EndDraw());
    }catch(...){s->EndDraw();throw;}
    check(s->EndDraw());++redraws;
}
// Shoulders are drawn at the target radius in physical pixels, so at rest they
// show 1:1; while the radius animates, a scale transform bridges the gap.
void Renderer::wings(float radius){
    wingRadius_=radius;const auto shape=shoulderShape(radius);
    wingAlong_=std::max(1,int(std::lround(shape.width*scale_)));wingDepth_=std::max(1,int(std::lround(shape.depth*scale_)));
    const float along=float(wingAlong_),depth=float(wingDepth_);
    auto draw=[&](ComPtr<IDCompositionSurface>& target,bool right){
        // Local frame: x along the edge (0 at the outer end), y away from the edge.
        auto p=[&](float x,float y){if(right)x=along+1-x;return edge_?D2D1::Point2F(depth-y,x):D2D1::Point2F(x,y);};
        pixelSurface(target,edge_?wingDepth_:wingAlong_+1,edge_?wingAlong_+1:wingDepth_,[&](ID2D1RenderTarget* rt){
            ComPtr<ID2D1PathGeometry> path;check(d2d_->CreatePathGeometry(&path));ComPtr<ID2D1GeometrySink> sink;check(path->Open(&sink));
            sink->BeginFigure(p(0,0),D2D1_FIGURE_BEGIN_FILLED);sink->AddLine(p(along+1,0));sink->AddLine(p(along+1,depth));sink->AddLine(p(along,depth));
            sink->AddBezier({p(along,depth*float(shoulderSideControl)),p(along*float(shoulderEdgeControl),0),p(0,0)});sink->EndFigure(D2D1_FIGURE_END_CLOSED);check(sink->Close());
            ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(baseColor_),&b);rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);rt->FillGeometry(path.Get(),b.Get());});};
    draw(leftSurface_,false);draw(rightSurface_,true);
    wingLeft_->SetContent(attached_&&material_==0?leftSurface_.Get():nullptr);wingRight_->SetContent(attached_&&material_==0?rightSurface_.Get():nullptr);
}
void Renderer::text(ID2D1RenderTarget* rt,const std::wstring& value,float x,float y,float w,float size,UINT32 color,DWRITE_FONT_WEIGHT weight,DWRITE_TEXT_ALIGNMENT alignment,float height) {
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(size),nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);f->SetTextAlignment(alignment);if(height>0)f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(color),&b));
    rt->DrawText(value.c_str(),UINT32(value.size()),f.Get(),D2D1::RectF(std::round(x*scale_)/scale_,std::round(y*scale_)/scale_,std::round((x+w)*scale_)/scale_,std::round((y+(height>0?height:size*1.6f))*scale_)/scale_),b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
// Ease-out cubic from `from` to 1 over 0.22 s, beginning at `start`.
ComPtr<IDCompositionAnimation> Renderer::entrance(double start,float from){
    constexpr double d=.22;const float span=1-from;ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(start)));
    check(a->AddCubic(0,from,float(3*span/d),float(-3*span/(d*d)),float(span/(d*d*d))));check(a->End(d,1.f));return a;
}
void Renderer::redraw(const ContentSnapshot& s,bool debug,bool headerOnly) {
    if(!headerOnly){
        // A different page, tab, session or result set on an open, resting island.
        std::string key=std::to_string(int(s.page))+'.'+std::to_string(s.statsTab)+'.'+std::to_string(s.audioTab)+'.'+std::to_string(s.shelfTab)+'.'+std::to_string(s.session)+'.'+std::to_string(int(s.live))+std::to_string(int(s.card))+std::to_string(int(s.command.active))+'.'+std::to_string(s.command.results.size());
        if(key!=contentKey_){const double now=seconds();if(restExpanded_&&s.expanded&&!s.reducedMotion&&!contentKey_.empty()){contentEntrance_=now;auto a=entrance(now,.5f);contentEffect_->SetOpacity(a.Get());}contentKey_=key;}
    }
    // Glass keeps text colors but lets fills breathe; solid surfaces stay opaque.
    const bool glass=s.settings.glassy()&&glass_.available();
    const UINT32 bg=s.light?0xf7f7f9:0x090a0c,ink=s.light?0x202329:0xf1f3f7,muted=s.light?0x656b75:(glass?0xa3aab6:0x8e96a4),raised=glass?0xffffff:s.light?0xeceef2:0x14171d,line=glass?(s.light?0x000000:0xffffff):s.light?0xdde1e7:0x242a33;
    const float raisedAlpha=glass?(s.light?.5f:.075f):1.f,lineAlpha=glass?(s.light?.1f:.12f):1.f;const UINT32 solidRaised=glass?(s.light?0xe6e8ec:0x1b1d23):raised,track=glass?(s.light?0xc5c9d0:0x3a3e46):line;
    const UINT32 accents[]={0xa4deca,0xa6cafa,0xccb8f1,0xefc7a6};UINT32 accent=s.settings.albumAccents&&s.playback.artwork?s.playback.artwork->accent:accents[s.settings.accent];if(s.light)accent=0x487467;
    bool attached=!s.settings.floating();
    GlassStyle style;style.visible=glass;style.light=s.light;style.blur=s.blur;style.material=s.settings.material;style.tint=s.settings.glassTint/100.f;style.accent=s.settings.albumAccents&&s.playback.artwork?s.playback.artwork->accent:0;glass_.style(style);expanded_=s.expanded;live_=s.live;edge_=s.settings.edge;attached_=attached;iconMotion_=s.settings.animatedIcons&&!s.reducedMotion;
    if(!headerOnly){targets.clear();iconCursor_=7;for(auto& i:icons_)i.used=false;}
    if(baseColor_!=bg||accentColor_!=accent||material_!=int(glass)||cachedEdge_!=edge_){
        baseColor_=bg;accentColor_=accent;material_=int(glass);cachedEdge_=edge_;
        surface(baseSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));});body_->SetContent(glass?nullptr:baseSurface_.Get());
        surface(innerSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));});inner_->SetContent(glass?nullptr:innerSurface_.Get());
        wings(wingRadius_>0?wingRadius_:17);
        {const UINT32 tone=s.light?0x1c2230:0xffffff;if(hoverColor_!=tone){hoverColor_=tone;surface(hoverSurface_,440,360,[&](auto* rt){rt->Clear(D2D1::ColorF(tone,s.light?.065f:.10f));});hoverVisual_->SetContent(hoverSurface_.Get());}}
        surface(barSurface_,380,2,[&](auto* rt){rt->Clear(D2D1::ColorF(accent));});bar_->SetContent(barSurface_.Get());
        surface(navSurface_,47,44,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink,s.light?.055f:.055f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,47,44},12,12),b.Get());});nav_->SetContent(navSurface_.Get());
    }
    wingLeft_->SetContent(attached&&!glass?leftSurface_.Get():nullptr);wingRight_->SetContent(attached&&!glass?rightSurface_.Get():nullptr);barEffect_->SetOpacity(s.page==Page::Overview&&s.expanded&&!s.live?1.f:0.f);
    updateAtmosphere(s);updatePeek(s);updateArtwork(s,solidRaised);updateTimeline(s,accent,track);updateRings(s,accent,muted,track);updateSpectrumLayout(s,accent);updateCard(s,track,accent,solidRaised,ink);updatePrivacyBand(s,ink,muted,solidRaised);{const bool panel=s.expanded&&!s.live;const bool stats=panel&&s.page==Page::System,audio=panel&&s.page==Page::Audio,shelf=panel&&s.page==Page::Shelf;updateTabs(s,0,stats?-2.f:30.f,stats?3:2,stats?s.statsTab:shelf?s.shelfTab:s.audioTab,stats||audio||shelf,s.light?0x262c34:0xe8ecf2);}updateHud(s,accent,track);updateBadge(s,glass?(s.light?0xf1f2f4:0x15161a):bg);
    surface(headerSurface_,600,150,[&](auto* rt){
        if(s.expanded)return;
        if(edge_){text(rt,s.battery>=0?std::to_wstring(s.battery)+L"%":L"—",0,82,64,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER);text(rt,s.charging?L"Charging":L"Battery",0,105,64,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_CENTER);return;}
        bool mini=s.settings.uiMode==0;float width=mini?72.f:float(s.settings.compactWidth);
        if(s.hud){
            // Level indicator: icon, compositor-driven bar (hudTrack_/hudFill_) and value.
            float x=std::max(236.f,width)/2-109;bool brightness=s.hud==2;int level=brightness?s.brightness:(s.muted?0:s.volume);
            drawIcon(rt,d2d_.Get(),brightness?Icon::Brightness:level==0?Icon::Muted:Icon::Volume,x,8,18,ink);text(rt,level>=0?std::to_wstring(level):L"—",x+186,0,32,11.5f,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_TRAILING,34);return;}
        const bool bars=s.settings.waveform&&s.playback.playing&&s.waveform&&s.settings.compactMedia;
        // Privacy dots: green camera, orange microphone, blue location (in that order, once each).
        std::vector<UINT32> dots;for(auto c:{Capability::Camera,Capability::Microphone,Capability::Location})if(std::any_of(s.privacy.begin(),s.privacy.end(),[&](auto& u){return u.capability==c;}))dots.push_back(c==Capability::Camera?0x30d158:c==Capability::Microphone?0xff9f0a:0x0a84ff);
        auto drawDots=[&](float right){ComPtr<ID2D1SolidColorBrush> d;rt->CreateSolidColorBrush(D2D1::ColorF(0),&d);float x=right-float(dots.size())*9+4;for(auto color:dots){d->SetColor(D2D1::ColorF(color));rt->FillEllipse(D2D1::Ellipse({x+3,17},3.2f,3.2f),d.Get());x+=9;}};
        if(mini){if(!s.playback.artwork||!s.settings.compactMedia){drawIcon(rt,d2d_.Get(),s.focus.running?Icon::Focus:s.charging?Icon::Power:Icon::Music,14,10,14,muted);}if(!dots.empty()&&s.activity.empty()){drawDots(66);return;}if(!bars)text(rt,s.activity.empty()?L"···":s.activity.starts_with(L"Volume")?std::to_wstring(s.volume):s.activity==L"Muted"?L"—":L"•",42,0,26,10,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER,34);return;}
        bool hasArt=bool(s.playback.artwork)&&s.settings.compactMedia,logo=!hasArt&&s.settings.compactMedia&&s.settings.appIcons&&s.playback.available;if(logo)identity(rt,s.playback,13,7,20,solidRaised);float start=hasArt||logo?42.f:14.f,end=width-(ringsEnabled_?(ringCount_==2?64:40):10)-(bars?32:0);if(!dots.empty()){drawDots(end);end-=float(dots.size())*9+8;}
        auto chip=[&](Icon glyph,const std::wstring& value,float span){if(end-start<span+78)return;end-=span;drawIcon(rt,d2d_.Get(),glyph,end+3,11,12,muted);text(rt,value,end+19,0,span-21,10,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);end-=7;};
        if(s.settings.compactClock){SYSTEMTIME t{};GetLocalTime(&t);wchar_t value[12];swprintf(value,12,L"%02u:%02u",t.wHour,t.wMinute);chip(Icon::Clock,value,58);}
        if(s.settings.compactVolume)chip(s.muted?Icon::Muted:Icon::Volume,std::to_wstring(s.volume),48);
        if(s.settings.compactBattery&&s.battery>=0&&!ringsEnabled_)chip(s.charging?Icon::Power:Icon::Battery,std::to_wstring(s.battery)+L"%",56);
        if(s.settings.compactTimer&&s.focus.running)chip(Icon::Focus,clockText(std::ceil(s.focus.displayed(seconds()))),64);
        std::wstring label=!s.activity.empty()?s.activity:s.settings.compactMedia&&s.playback.available?s.playback.title:s.settings.compactTimer&&s.focus.running?clockText(std::ceil(s.focus.displayed(seconds()))):L"Ready";
        text(rt,label,start,0,std::max(12.f,end-start),11.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);
        if(label!=headerLabel_){if(restCompact_&&!s.expanded&&!s.reducedMotion&&!headerLabel_.empty()){headerEntrance_=seconds();auto a=entrance(headerEntrance_,.35f);headerEffect_->SetOpacity(a.Get());}headerLabel_=label;}
    });header_->SetContent(headerSurface_.Get());if(headerOnly){commit();return;}
    drawingContent_=true;iconRequests_.clear();caretTarget_=42;
    surface(contentSurface_,380,284,[&](auto* rt){
        const bool logoArt=s.settings.appIcons&&s.playback.available&&(!s.playback.service.empty()||s.playback.appIcon);
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink),&b);
        auto box=[&](float x,float y,float w,float h,UINT32 color,float radius=12){b->SetColor(D2D1::ColorF(color,color==raised?raisedAlpha:1.f));rt->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),b.Get());};
        auto hairline=[&](float x,float y,float w){b->SetColor(D2D1::ColorF(line,lineAlpha));rt->DrawLine({x,y},{x+w,y},b.Get(),1/scale_);};
        auto label=[&](const std::wstring& value,float x,float y,float w,float h,float size,UINT32 color,DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_MEDIUM){text(rt,value,x,y,w,size,color,weight,DWRITE_TEXT_ALIGNMENT_CENTER,h);};
        auto button=[&](Action a,const std::wstring& value,float x,float y,float w,float h,bool selected=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});box(x,y,w,h,selected?(s.light?0x262c34:0xe8ecf2):raised,9);label(value,x+6,y,w-12,h,11.5f,!enabled?muted:selected?(s.light?0xf8f8fa:0x171b22):ink);};
        auto iconButton=[&](Action a,Icon glyph,float x,float y,float w=32,float h=32,bool primary=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});if(primary)box(x,y,w,h,!enabled?raised:s.light?0x252b33:0xe8ecf2,h/2);icon(a,glyph,x+(w-18)/2,y+(h-18)/2,18,!enabled?muted:primary?(s.light?0xffffff:0x161b22):ink);};
        auto value=[](double v,int precision=0){if(v<0)return std::wstring(L"—");wchar_t buf[48];swprintf(buf,48,precision?L"%.1f":L"%.0f",v);return std::wstring(buf);};
        if(s.card&&s.command.active){
            const auto& c=s.command;box(0,0,380,42,raised,14);drawIcon(rt,d2d_.Get(),Icon::Search,14,12,18,c.text.empty()?muted:ink);
            // The typed line scrolls left when it is wider than the field, keeping the caret visible.
            const float field=320;float width=measure(c.text,14),before=measure(c.text.substr(0,std::min(c.caret,c.text.size())),14),shift=std::max(0.f,std::min(width-field,before-field+8));
            if(c.text.empty())text(rt,L"Type a command, app or search",46,0,field,14,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,42);
            else{rt->PushAxisAlignedClip(D2D1::RectF(42,0,42+field+4,42),D2D1_ANTIALIAS_MODE_ALIASED);text(rt,c.text,42-shift,0,width+40,14,ink,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,42);rt->PopAxisAlignedClip();}
            caretTarget_=42+before-shift;
            auto glyph=[](CommandKind k){switch(k){case CommandKind::Volume:case CommandKind::VolumeStep:case CommandKind::Unmute:return Icon::Volume;case CommandKind::Mute:return Icon::Muted;case CommandKind::Play:return Icon::Play;case CommandKind::Pause:return Icon::Pause;
                case CommandKind::Next:return Icon::Next;case CommandKind::Previous:return Icon::Previous;case CommandKind::Timer:return Icon::Focus;case CommandKind::Stopwatch:return Icon::Clock;case CommandKind::StopTimer:return Icon::Reset;case CommandKind::OpenApp:return Icon::Apps;
                case CommandKind::SearchFiles:return Icon::Search;case CommandKind::OpenSettings:return Icon::Settings;case CommandKind::Workspace:case CommandKind::SaveWorkspace:case CommandKind::DeleteWorkspace:return Icon::Workspace;
                case CommandKind::Clipboard:case CommandKind::ClearClipboard:return Icon::Clipboard;case CommandKind::Lock:return Icon::Lock;default:return Icon::Info;}};
            if(c.results.empty()){
                // Nothing typed yet: a few things to try.
                const std::pair<Icon,const wchar_t*> hints[]={{Icon::Volume,L"volume 40  \u00b7  mute  \u00b7  next"},{Icon::Focus,L"focus 25  \u00b7  timer 10 min  \u00b7  stopwatch"},{Icon::Search,L"open spotify  \u00b7  find budget pdfs from last month"}};
                for(int i=0;i<3;++i){float y=52+i*40.f;drawIcon(rt,d2d_.Get(),hints[i].first,14,y+10,16,muted);text(rt,hints[i].second,46,y,330,11,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,38);}
            }
            for(int i=0;i<int(std::min<size_t>(3,c.results.size()));++i){const auto& r=c.results[i];float y=52+i*40.f;Action a=Action(int(Action::CommandResultBase)+i);targets.push_back({a,0,y,380,38,r.kind!=CommandKind::None});
                const auto* icon=size_t(i)<c.icons.size()?c.icons[i].get():nullptr;
                if(icon)drawPreview(rt,*icon,7,y+5,28,28);else{box(6,y+4,30,30,raised,15);drawIcon(rt,d2d_.Get(),glyph(r.kind),13,y+11,16,r.kind==CommandKind::None?muted:ink);}
                const bool keycap=i==c.selected&&r.kind!=CommandKind::None;const wchar_t* cap=c.armed?L"Enter again":L"Enter";const float capWidth=keycap?measure(cap,9.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD)+14:0;const float room=(keycap?372-capWidth-8:374)-46;
                text(rt,r.title,46,y+2,room,12.5f,r.kind==CommandKind::None?muted:ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,r.detail,46,y+21,room,10,muted);
                if(keycap){box(372-capWidth,y+10,capWidth,19,s.light?0xffffff:0x2c323c,6);label(cap,372-capWidth,y+10,capWidth,19,9.5f,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);}}
            const float footer=commandFooterY(c.results.empty()?3:int(std::min<size_t>(3,c.results.size())));
            if(!c.status.empty()){drawIcon(rt,d2d_.Get(),c.error?Icon::Info:c.armed?Icon::Workspace:Icon::Check,2,footer+6,14,c.error?0xe5484d:accent);text(rt,c.status,22,footer,356,10.5f,c.error?0xe5484d:ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,26);}
            else{float x=0;for(auto [cap,what]:{std::pair{L"Enter",L"Run"},std::pair{L"\u2191 \u2193",L"Choose"},std::pair{L"Esc",L"Close"}}){float w=measure(cap,9,DWRITE_FONT_WEIGHT_SEMI_BOLD)+12;box(x,footer+4,w,18,raised,5);label(cap,x,footer+4,w,18,9,muted,DWRITE_FONT_WEIGHT_SEMI_BOLD);x+=w+6;float t=measure(what,10)+2;text(rt,what,x,footer,t,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,26);x+=t+16;}}
            return;
        }
        if(s.card&&s.notice.kind>=5){
            // Privacy card: the app's own icon in a ring of the capability's colour.
            const auto& n=s.notice;const wchar_t* what=n.kind==5?L"Camera in use":n.kind==6?L"Microphone in use":L"Location in use";
            text(rt,what,72,6,196,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.app.empty()?std::wstring(L"An app"):n.app,72,30,196,11,muted);
            const UINT32 color=n.kind==5?0x30d158:n.kind==6?0xff9f0a:0x0a84ff;box(292,10,40,40,raised,20);drawIcon(rt,d2d_.Get(),n.kind==5?Icon::Camera:n.kind==6?Icon::Microphone:Icon::Location,302,20,20,color);
            return;
        }
        if(s.card&&s.notice.kind){
            auto& n=s.notice;const bool power=n.kind>=3;const int percent=power?s.battery:n.device.battery;
            const bool holding=n.kind==3&&s.power.present&&!s.power.charging;std::wstring title=power?(n.kind==3?(holding?L"Plugged in":L"Charging"):L"On battery"):n.device.name,detail;
            if(n.kind==3){wchar_t w[32]{};if(!s.power.relative&&s.power.rateMw>0)swprintf(w,32,L"%.1f W",s.power.rateMw/1000.);detail=holding?L"Not charging right now":w[0]?std::wstring(w)+(s.toFull>=0?L"  ·  full in about "+durationText(s.toFull):L""):L"Power connected";}
            else if(n.kind==4)detail=s.remaining>=0?L"About "+durationText(s.remaining)+L" left":L"Running on battery power";
            else{const wchar_t* kinds[]={L"Device",L"Headphones",L"Earbuds",L"Speaker",L"Phone",L"Computer",L"Keyboard",L"Mouse",L"Controller",L"Watch",L"Audio"};detail=std::wstring(n.kind==1?L"Connected":L"Disconnected")+L"  ·  "+kinds[int(n.device.kind)];}
            text(rt,title,72,6,percent>=0?186.f:256.f,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,detail,72,30,percent>=0?186.f:256.f,11,muted);
            if(percent>=0){text(rt,std::to_wstring(percent)+L"%",256,2,76,24,ink,DWRITE_FONT_WEIGHT_LIGHT,DWRITE_TEXT_ALIGNMENT_TRAILING,32);text(rt,power?L"battery":L"device battery",236,32,96,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
            return;
        }
        if(s.live){
            const bool bars=s.settings.waveform&&s.playback.playing&&s.waveform;
            text(rt,s.playback.available?s.playback.title:s.focus.running?L"Focus in progress":L"A little space for now",68,0,bars?216.f:250.f,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
            text(rt,s.playback.available?s.playback.artist:s.focus.running?clockText(std::ceil(s.focus.displayed(seconds()))):L"Sound · focus · things within reach",68,23,250,10.5f,muted);
            if(!s.playback.artwork){box(0,0,52,52,raised,14);if(logoArt)identity(rt,s.playback,10,10,32,solidRaised);else drawIcon(rt,d2d_.Get(),s.focus.running?Icon::Focus:Icon::Music,15,15,22,accent);}
            iconButton(Action::Previous,Icon::Previous,66,44,36,32,false,s.playback.canPrevious);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,116,40,40,40,true,s.playback.canToggle);iconButton(Action::Next,Icon::Next,170,44,36,32,false,s.playback.canNext);iconButton(Action::Mute,s.muted?Icon::Muted:Icon::Volume,228,44,32,32);text(rt,std::to_wstring(s.volume)+L"%",266,52,54,11,muted);
            button(Action::Overview,L"Command Center",0,88,150,24);button(Action::Shelf,L"Shelf",160,88,73,24);iconButton(Action::Settings,Icon::Settings,284,86,36,28);
            if(s.sessions.size()>1){int n=int(std::min<size_t>(6,s.sessions.size()));float total=12+(n-1)*9.f,x=26-total/2;
                for(int i=0;i<n;++i){bool selected=i==s.session;float w=selected?12.f:5.f;b->SetColor(D2D1::ColorF(selected?ink:muted,selected?1.f:.55f));rt->FillRoundedRectangle(D2D1::RoundedRect({x,58,x+w,63},2.5f,2.5f),b.Get());targets.push_back({Action(int(Action::SessionBase)+i),x-2,54,w+4,13});x+=w+4;}}
            return;
        }
        const wchar_t* titles[]={L"Your day, at a glance",L"Now playing",L"System overview",L"Make room for focus",L"Make it yours",L"Within reach",L"Sound, your way"};
        int chips=s.page==Page::Media?int(std::min<size_t>(5,s.sessions.size())):0;if(chips<2)chips=0;
        auto tabs=[&](std::initializer_list<std::pair<Action,const wchar_t*>> items,int selected,float y){int i=0;for(auto& [a,name]:items){float x=i*88.f;targets.push_back({a,x,y,82,26});label(name,x,y,82,26,11.5f,i==selected?(s.light?0xf8f8fa:0x171b22):muted,i==selected?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_MEDIUM);++i;}};
        if(s.page==Page::System)tabs({{Action::StatsSystem,L"System"},{Action::StatsBattery,L"Battery"},{Action::StatsDevices,L"Devices"}},s.statsTab,-2);
        else text(rt,titles[int(s.page)],0,0,chips?340.f-chips*28:302.f,18,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        iconButton(Action::Close,Icon::Close,352,-4,28,28);if(s.page==Page::Overview)iconButton(Action::CommandOpen,Icon::Search,318,-4,28,28);
        for(int i=0;i<chips;++i){auto& session=s.sessions[i];float x=344-float(chips-i)*28;bool selected=i==s.session;box(x,-2,24,24,raised,12);
            identity(rt,session,x+3,1,18,solidRaised);
            if(selected){b->SetColor(D2D1::ColorF(accent));rt->DrawEllipse(D2D1::Ellipse({x+12,10},12.5f,12.5f),b.Get(),1.6f);}
            targets.push_back({Action(int(Action::SessionBase)+i),x,-2,24,24});}
        if(s.page==Page::Overview){
            if(!s.playback.artwork){box(0,43,64,64,raised,15);if(logoArt)identity(rt,s.playback,12,55,40,solidRaised);else drawIcon(rt,d2d_.Get(),Icon::Music,19,62,26,muted);}
            text(rt,s.playback.available?s.playback.title:L"A quieter place for everything",80,48,252,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.available?s.playback.artist:L"Play something. Find your rhythm.",80,74,252,11,muted);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,340,58,40,40,true,s.playback.canToggle);targets.push_back({Action::Media,0,38,328,74});
            const Icon glyphs[]={Icon::Processor,Icon::Memory,Icon::Battery,Icon::Download,Icon::Upload,Icon::Disk,Icon::Clock};const wchar_t* names[]={L"CPU",L"Memory",L"Battery",L"Download",L"Upload",L"Disk free",L"Uptime"};
            for(int i=0;i<3;++i){int metric=s.settings.homeMetrics[i];float x=i*130.f;box(x,126,120,68,raised,13);drawIcon(rt,d2d_.Get(),glyphs[metric],x+12,137,14,muted);text(rt,names[metric],x+33,136,77,10,muted);std::wstring number;switch(metric){case 0:number=value(s.system.cpu)+ (s.system.cpu>=0?L"%":L"");break;case 1:number=s.system.ramTotalGiB?value(s.system.ramPercent)+L"%":L"—";break;case 2:number=s.battery>=0?std::to_wstring(s.battery)+L"%":L"—";break;case 3:number=s.system.networkAvailable?rateText(s.system.download):L"—";break;case 4:number=s.system.networkAvailable?rateText(s.system.upload):L"—";break;case 5:number=s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—";break;default:number=clockText(double(s.system.uptime));break;}text(rt,number,x+12,156,100,metric>=3?18:23,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
            iconButton(Action::Mute,s.muted?Icon::Muted:Icon::Volume,0,200,28,28);b->SetColor(D2D1::ColorF(line,lineAlpha));rt->DrawLine({38,215},{300,215},b.Get(),2);text(rt,s.muted?L"Muted":std::to_wstring(s.volume)+L"%",306,207,36,10.5f,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,16);iconButton(Action::Audio,Icon::Audio,346,200,34,28);targets.push_back({Action::VolumeSlider,38,201,262,26});
        }else if(s.page==Page::Media){
            bool video=s.settings.mediaLayout==2||(s.settings.mediaLayout==0&&s.playback.kind==MediaKind::Video);float tx=video?166.f:118.f,tw=380-tx;
            if(!s.playback.artwork){box(0,video?30:44,video?148:100,video?148:100,raised,18);if(logoArt)identity(rt,s.playback,video?42:22,video?72:66,video?64:56,solidRaised);else drawIcon(rt,d2d_.Get(),Icon::Music,video?56:34,video?85:77,32,muted);}
            text(rt,s.playback.title,tx,45,tw,16,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist,tx,74,tw,11,muted);{auto* rule=findService(s.playback.service);std::wstring app=rule?std::wstring(rule->name):s.playback.appName;if(app.empty())app=video?L"Video":L"Music";
                if(s.sessions.size()>1)app+=L"  ·  "+std::to_wstring(s.session+1)+L" of "+std::to_wstring(s.sessions.size());text(rt,app,tx,100,std::max(40.f,tw-(s.settings.waveform&&s.playback.playing&&s.waveform?104.f:0.f)),9.5f,muted);}
            iconButton(Action::Previous,Icon::Previous,tx,132,36,36,false,s.playback.canPrevious);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,tx+54,124,52,52,true,s.playback.canToggle);iconButton(Action::Next,Icon::Next,tx+124,132,36,36,false,s.playback.canNext);
            double position=std::clamp(s.playback.position+(s.playback.playing?std::max(0.,seconds()-s.playback.sampledAt):0.),0.,s.playback.duration);if(s.playback.canSeek)targets.push_back({Action::Seek,0,178,380,25});if(s.scrub.active){position=s.scrub.value;text(rt,L"Pull away for finer control",tx,176,tw,8.5f,muted);}text(rt,s.playback.duration>0?clockText(position):L"No timeline provided",0,200,160,10,muted);button(Action::MediaMode,s.settings.mediaLayout==0?L"Auto":s.settings.mediaLayout==1?L"Music":L"Video",167,199,54,22);text(rt,s.playback.duration>0?clockText(s.playback.duration):L"",300,200,80,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);
        }else if(s.page==Page::System&&s.statsTab==1){
            const auto& p=s.power;const int percent=s.battery;const bool charging=p.charging||(s.charging&&!p.present);
            drawRing(rt,d2d_.Get(),58,100,40,4,percent>=0?percent/100.:0,charging?0x5fd98a:accent,track);
            if(charging)drawIcon(rt,d2d_.Get(),Icon::Bolt,51,68,14,0x5fd98a);text(rt,percent>=0?std::to_wstring(percent)+L"%":L"—",18,86,80,20,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER,28);
            wchar_t rate[32]{};if(p.present&&!p.relative&&p.rateMw!=0)swprintf(rate,32,L"  ·  %.1f W",std::abs(p.rateMw)/1000.);
            text(rt,std::wstring(charging?L"Charging":s.charging?L"Plugged in":L"On battery")+rate,122,44,258,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
            text(rt,charging&&s.toFull>=0?L"Full in about "+durationText(s.toFull):!s.charging&&s.remaining>=0?L"About "+durationText(s.remaining)+L" left":s.charging&&percent>=99?L"Fully charged":s.charging&&!charging&&p.present?L"Charging paused, likely battery care":L"Time estimate appears once the rate settles",122,68,258,11,muted);
            auto stat=[&](Icon glyph,const wchar_t* name,const std::wstring& number,float x){drawIcon(rt,d2d_.Get(),glyph,x,98,12,muted);text(rt,name,x+17,96,70,9,muted);text(rt,number,x,112,84,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);};
            wchar_t full[32]{};if(p.fullMwh>0&&!p.relative)swprintf(full,32,L"%.1f Wh",p.fullMwh/1000.);double health=p.health();
            stat(Icon::Heart,L"HEALTH",health>=0?std::to_wstring(int(std::lround(health*100)))+L"%":L"—",122);stat(Icon::Battery,L"FULL CHARGE",full[0]?full:L"—",208);stat(Icon::Reset,L"CYCLES",p.cycles?std::to_wstring(p.cycles):L"—",294);
            text(rt,L"Last 24 hours",0,146,200,9.5f,muted);
            if(s.history.size()>=8){ComPtr<ID2D1PathGeometry> path;d2d_->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;path->Open(&sink);auto point=[&](size_t i){return D2D1::Point2F(s.history[i]*380,196-s.history[i+1]*32);};
                sink->BeginFigure({s.history[0]*380,196},D2D1_FIGURE_BEGIN_FILLED);for(size_t i=0;i+1<s.history.size();i+=2)sink->AddLine(point(i));sink->AddLine({s.history[s.history.size()-2]*380,196});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();
                b->SetColor(D2D1::ColorF(accent,.14f));rt->FillGeometry(path.Get(),b.Get());b->SetColor(D2D1::ColorF(accent));for(size_t i=2;i+1<s.history.size();i+=2)rt->DrawLine(point(i-2),point(i),b.Get(),1.6f);}
            else text(rt,L"History fills in as the day goes on",0,166,380,10,muted);
            hairline(0,197,380);button(Action::PowerSettings,L"Power & battery",0,203,150,25);
            {wchar_t about[64]{};if(p.designMwh>0&&!p.relative)swprintf(about,64,p.voltageMv>0?L"Design %.1f Wh  ·  %.1f V":L"Design %.1f Wh",p.designMwh/1000.,p.voltageMv/1000.);text(rt,about,160,210,220,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
        }else if(s.page==Page::System&&s.statsTab==2){
            if(s.devices.empty())label(L"No Bluetooth devices are paired",0,90,380,40,12,muted,DWRITE_FONT_WEIGHT_NORMAL);
            for(int i=s.deviceOffset;i<std::min(s.deviceOffset+4,int(s.devices.size()));++i){float y=32+float(i-s.deviceOffset)*42;auto& d=s.devices[i];box(0,y,380,38,raised,11);
                deviceBadge(rt,d,6,y+5,28,s.light?0xe4e6ea:0x2a2d34,ink);text(rt,d.name,44,y+3,d.audio?200.f:250.f,12,d.connected?ink:muted,DWRITE_FONT_WEIGHT_SEMI_BOLD);
                text(rt,std::wstring(d.connected?L"Connected":L"Not connected")+(d.battery>=0?L"  ·  "+std::to_wstring(d.battery)+L"%":L""),44,y+20,200,9.5f,muted);
                if(d.battery>=0){float bx=d.audio?254.f:334.f;box(bx,y+15,36,9,track,4.5f);box(bx+1.5f,y+16.5f,33*std::clamp(d.battery/100.f,.04f,1.f),6,d.battery<=20?0xf06a6a:(d.connected?accent:muted),3);}
                if(d.audio)button(Action(int(Action::DeviceConnectBase)+i),d.connected?L"Disconnect":L"Connect",298,y+7,78,24);}
            hairline(0,197,380);button(Action::BluetoothSettings,L"Bluetooth settings",0,203,150,25);
            {int connected=int(std::count_if(s.devices.begin(),s.devices.end(),[](auto& d){return d.connected;}));text(rt,s.deviceFeedback.empty()?std::to_wstring(s.devices.size())+L" paired  ·  "+std::to_wstring(connected)+L" connected":s.deviceFeedback,160,210,220,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
        }else if(s.page==Page::System){
            auto stat=[&](Icon glyph,const wchar_t* name,const std::wstring& number,float x,float y,float w){drawIcon(rt,d2d_.Get(),glyph,x,y,14,muted);text(rt,name,x+22,y-1,w-22,10,muted);text(rt,number,x,y+20,w,22,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);};
            stat(Icon::Processor,L"PROCESSOR",s.system.cpu<0?L"—":value(s.system.cpu)+L"%",0,42,180);stat(Icon::Memory,L"MEMORY / GB",s.system.ramTotalGiB?value(s.system.ramUsedGiB,1)+L" / "+value(s.system.ramTotalGiB,1):L"—",208,42,172);
            b->SetColor(D2D1::ColorF(accent));for(unsigned i=1;i<s.system.samples;++i){unsigned base=40-s.system.samples;rt->DrawLine({float((i-1)*380./39),125-s.system.cpuHistory[base+i-1]*.25f},{float(i*380./39),125-s.system.cpuHistory[base+i]*.25f},b.Get(),1.5f);}hairline(0,132,380);
            stat(Icon::Download,L"DOWN",s.system.networkAvailable?rateText(s.system.download):L"—",0,147,120);stat(Icon::Upload,L"UP",s.system.networkAvailable?rateText(s.system.upload):L"—",132,147,120);stat(Icon::Disk,L"FREE",s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—",264,147,116);{std::wstring line=s.platform.product.empty()?std::to_wstring(s.system.logicalProcessors)+L" threads  ·  up "+clockText(double(s.system.uptime)):s.platform.product+(s.powerMode>=0?L"  ·  "+std::wstring(powerModeName(s.powerMode)):L"");const bool armoury=!s.platform.armoury.empty();text(rt,line,0,208,armoury?262.f:380.f,10,muted);if(armoury)button(Action::Armoury,L"Armoury Crate",270,201,110,25);}
        }else if(s.page==Page::Focus){
            button(Action::Timer25,L"Focus",0,36,120,28,s.focus.mode==FocusClock::Mode::Focus);button(Action::Timer5,L"Break",130,36,120,28,s.focus.mode==FocusClock::Mode::Break);button(Action::Stopwatch,L"Stopwatch",260,36,120,28,s.focus.mode==FocusClock::Mode::Stopwatch);
            double progress=s.focus.mode==FocusClock::Mode::Stopwatch?std::fmod(s.focus.elapsed(seconds()),60.)/60.:normalizedProgress(s.focus.displayed(seconds()),s.focus.duration);drawRing(rt,d2d_.Get(),63,130,38,3,progress,accent,line);drawIcon(rt,d2d_.Get(),Icon::Focus,50,117,26,accent);
            text(rt,clockText(std::ceil(s.focus.displayed(seconds()))),124,91,252,42,ink,DWRITE_FONT_WEIGHT_LIGHT);text(rt,s.focus.finished?L"A moment well spent":s.focus.running?L"One thing at a time":L"A little space to begin",127,144,245,11,muted);iconButton(Action::TimerToggle,s.focus.running?Icon::Pause:Icon::Play,130,180,42,42,true);iconButton(Action::TimerReset,Icon::Reset,190,183,36,36);text(rt,s.focus.running?L"Pause session":L"Start session",238,193,142,10,muted);
        }else if(s.page==Page::Shelf){
            tabs({{Action::ShelfFiles,L"Files"},{Action::ShelfClipboard,L"Clipboard"}},s.shelfTab,30);
            const bool status=!s.clipStatus.empty()&&seconds()<s.clipStatusUntil;
            if(s.shelfTab==0){
                if(s.shelf.empty()){box(0,62,380,130,raised,18);icon(Action::None,Icon::Shelf,172,76,36,accent);label(s.dropHover?L"Release to keep it close":L"A place to keep things close",20,118,340,28,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);label(L"Drop files or text. Drag them back out.",20,148,340,22,11,muted);}else for(size_t i=s.shelfOffset;i<std::min(size_t(s.shelfOffset+4),s.shelf.size());++i){float y=62+float(i-s.shelfOffset)*36;auto& item=s.shelf[i];Action a=Action(int(Action::ShelfItemBase)+int(i));targets.push_back({a,0,y,380,32});box(0,y,380,32,raised,9);if(item.preview)drawPreview(rt,*item.preview,6,y+3,32,26);else icon(a,item.kind==ShelfItem::Kind::File?Icon::File:Icon::Text,14,y+8,16,muted);text(rt,item.label,46,y+7,326,11,ink);}
                text(rt,std::to_wstring(s.shelf.size())+L" items  \u00b7  Copy only",0,208,260,10,muted);button(Action::ShelfClear,L"Clear",316,201,64,25,false,!s.shelf.empty());
            }else if(!s.settings.clipboardHistory){
                box(0,62,380,130,raised,18);drawIcon(rt,d2d_.Get(),Icon::Clipboard,176,74,28,accent);label(L"Keep what you copy close",20,106,340,24,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
                label(L"Your last 24 copies, in memory only. Private copies are skipped.",20,130,340,20,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);button(Action::ClipboardEnable,L"Turn on",150,156,80,26,true);
                text(rt,L"Off  \u00b7  Nothing is read until you turn it on",0,208,380,10,muted);
            }else{
                if(s.clips.empty()){box(0,62,380,130,raised,18);drawIcon(rt,d2d_.Get(),Icon::Clipboard,176,80,28,muted);label(s.clipsPaused?L"Paused":L"Copy something to keep it here",20,116,340,24,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);label(L"Click a copy to put it back on the clipboard",20,142,340,20,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);}
                for(int i=s.clipOffset;i<std::min(s.clipOffset+4,int(s.clips.size()));++i){float y=62+float(i-s.clipOffset)*36;auto& c=s.clips[size_t(i)];Action a=Action(int(Action::ClipBase)+(i-s.clipOffset));targets.push_back({a,0,y,380,32});box(0,y,380,32,raised,9);
                    if(c.thumbnail)drawPreview(rt,*c.thumbnail,6,y+3,32,26);else drawIcon(rt,d2d_.Get(),c.kind==int(ClipEntry::Kind::Link)?Icon::Link:c.kind==int(ClipEntry::Kind::Files)?Icon::File:Icon::Text,14,y+8,16,muted);
                    text(rt,c.preview,46,y+2,c.icon?300.f:326.f,11,ink);text(rt,c.meta,46,y+17,300,9,muted);if(c.icon)drawPreview(rt,*c.icon,352,y+8,16,16);}
                text(rt,status?s.clipStatus:s.clipsPaused?std::wstring(L"Paused  \u00b7  new copies are not kept"):std::to_wstring(s.clips.size())+(s.clips.size()==1?L" copy":L" copies")+L"  \u00b7  memory only",0,208,236,10,status?ink:muted);
                button(Action::ClipboardPause,s.clipsPaused?L"Resume":L"Pause",244,201,64,25);button(Action::ClipboardClear,L"Clear",316,201,64,25,false,!s.clips.empty());
            }
        }else if(s.page==Page::Audio){
            const bool apps=s.audioTab==0;tabs({{Action::AudioApps,L"Apps"},{Action::AudioOutputs,L"Outputs"}},s.audioTab,30);
            if(apps){
                text(rt,s.mixer.empty()?L"":std::to_wstring(s.mixer.size())+(s.mixer.size()==1?L" source":L" sources"),180,37,200,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);
                if(s.mixer.empty())label(L"No apps are using audio right now",0,96,380,40,12,muted,DWRITE_FONT_WEIGHT_NORMAL);
                for(int i=s.mixerOffset;i<std::min(s.mixerOffset+4,int(s.mixer.size()));++i){float y=62+float(i-s.mixerOffset)*36;auto& e=s.mixer[i];
                    if(e.icon)drawPreview(rt,*e.icon,2,y+4,22,22);else drawIcon(rt,d2d_.Get(),e.system?Icon::Settings:Icon::Apps,5,y+7,16,muted);
                    text(rt,e.name,34,y+1,114,11,e.active?ink:muted,DWRITE_FONT_WEIGHT_MEDIUM);text(rt,e.muted?L"Muted":std::to_wstring(int(std::lround(e.volume*100)))+L"%",34,y+17,114,9,muted);
                    float x=156,w=174,cy=y+15,v=std::clamp(e.volume,0.f,1.f);ComPtr<ID2D1StrokeStyle> round;auto props=D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND);d2d_->CreateStrokeStyle(props,nullptr,0,&round);
                    b->SetColor(D2D1::ColorF(track));rt->DrawLine({x,cy},{x+w,cy},b.Get(),3,round.Get());b->SetColor(D2D1::ColorF(accent,e.muted?.4f:1.f));if(v>0)rt->DrawLine({x,cy},{x+w*v,cy},b.Get(),3,round.Get());
                    b->SetColor(D2D1::ColorF(ink));rt->FillEllipse(D2D1::Ellipse({x+w*v,cy},5.5f,5.5f),b.Get());
                    targets.push_back({Action(int(Action::MixerSliderBase)+i),150,y,186,30});iconButton(Action(int(Action::MixerMuteBase)+i),e.muted?Icon::Muted:Icon::Volume,344,y,32,30);}
                button(Action::MixerSettings,L"Windows volume mixer",0,203,201,25);icon(Action::MixerSettings,Icon::ArrowRight,208,208,14,muted);
            }else{
            text(rt,s.feedback.empty()?L"Choose where your sound goes":s.feedback,176,37,204,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);for(size_t i=s.audioOffset;i<std::min(size_t(s.audioOffset+4),s.outputs.size());++i){float y=62+float(i-s.audioOffset)*34;auto& output=s.outputs[i];Action a=Action(int(Action::DeviceBase)+int(i));targets.push_back({a,0,y,380,30});box(0,y,380,30,raised,9);icon(a,Icon::Audio,10,y+7,16,output.current?accent:muted);text(rt,output.name,37,y+7,302,11,ink);if(output.current)drawIcon(rt,d2d_.Get(),Icon::Check,353,y+7,16,accent);}
            button(Action::SoundSettings,L"Windows sound settings",0,203,201,25);icon(Action::SoundSettings,Icon::ArrowRight,208,208,14,muted);text(rt,s.settings.directAudio?L"Direct switching":L"System picker",236,210,144,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
        }
        hairline(0,229,380);
        const wchar_t* labels[]={L"Home",L"Media",L"Stats",L"Focus",L"Settings",L"Shelf",L"Audio"};const Action actions[]={Action::Overview,Action::Media,Action::System,Action::Focus,Action::Settings,Action::Shelf,Action::Audio};const Icon glyphs[]={Icon::Home,Icon::Music,Icon::Stats,Icon::Focus,Icon::Settings,Icon::Shelf,Icon::Audio};
        for(int slot=0;slot<7;++slot){int p=s.settings.navigation[slot];float x=std::round((slot*navStep+4)*scale_)/scale_;bool selected=int(s.page)==p;targets.push_back({actions[p],x,navY,47,44});icon(actions[p],glyphs[p],x+14,navY+5,19,selected?ink:muted,p);label(labels[p],x,navY+26,47,16,9.5f,selected?ink:muted);if(selected){if(s.reducedMotion)navX.reset(x,seconds());else if(navX.target()!=x)navX.retarget(x,seconds(),MotionTokens::navigation);}}
        if(debug){b->SetColor(D2D1::ColorF(0xf98585));for(auto& target:targets)rt->DrawRectangle({target.x,target.y,target.x+target.width,target.y+target.height},b.Get());}
    });drawingContent_=false;updateCaret(s,caretTarget_,accent);for(auto request:iconRequests_)icon(request.action,request.glyph,request.x,request.y,request.size,request.color,request.slot);content_->SetContent(contentSurface_.Get());for(auto& item:icons_)if(!item.used)item.effect->SetOpacity(0.f);auto nx=animation(navX,seconds(),scale_,20*scale_);nav_->SetOffsetX(nx.Get());nav_->SetOffsetY((38+navY)*scale_);commit();
}

ComPtr<IDCompositionAnimation> Renderer::animation(const Spring& s,double now,float factor,float bias) {
    ComPtr<IDCompositionAnimation>a;check(device_->CreateAnimation(&a));
    check(a->SetAbsoluteBeginTime(ticks(now)));
    double duration;auto curve=s.curve(now,duration);
    for(auto& c:curve)check(a->AddCubic(c.time,float(c.p)*factor+bias,float(c.v)*factor,float(c.quadratic)*factor,float(c.cubic)*factor));
    check(a->End(duration,float(s.target())*factor+bias));return a;
}
ComPtr<IDCompositionAnimation> Renderer::visibility(const MotionEngine& m,double now,bool compact){
    ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now)));double duration=0;auto curve=approximateCurve([&](double t){return m.visibility(t,compact);},[&](double t){return m.height.settled(t)&&m.reveal.settled(t);},now,duration);for(auto& c:curve)check(a->AddCubic(c.time,float(c.p),float(c.v),float(c.quadratic),float(c.cubic)));check(a->End(duration,float(m.visibility(now+10,compact).position)));return a;
}
void Renderer::animate(const MotionEngine& m,double now) {
    // At rest the body sits on whole physical pixels, so its edges meet the shoulders exactly.
    const bool rest=m.width.settled(now)&&m.height.settled(now)&&m.radius.settled(now);
    const float restW=std::round(float(m.width.target()*scale_)),restH=std::round(float(m.height.target()*scale_));
    const float restX=std::round(float((edge_?canvasWidth-m.width.target():(canvasWidth-m.width.target())/2)*scale_)),restY=edge_?std::round(float((canvasHeight-m.height.target())*scale_/2)):0.f;
    auto w=animation(m.width,now,scale_),h=animation(m.height,now,scale_),r=animation(m.radius,now,scale_);
    auto iw=animation(m.width,now,scale_,-2),ih=animation(m.height,now,scale_,-2),ir=animation(m.radius,now,scale_,-1);
    if(rest){check(clip_->SetRight(restW));check(clip_->SetBottom(restH));check(innerClip_->SetRight(restW-2));check(innerClip_->SetBottom(restH-2));}
    else{check(clip_->SetRight(w.Get()));check(clip_->SetBottom(h.Get()));check(innerClip_->SetRight(iw.Get()));check(innerClip_->SetBottom(ih.Get()));}
    bool tl=!attached_||edge_,tr=!attached_,bl=true,br=!attached_||!edge_;

    auto corners=[&](IDCompositionRectangleClip* clip,IDCompositionAnimation* radius){
        if(tl){clip->SetTopLeftRadiusX(radius);clip->SetTopLeftRadiusY(radius);}else{clip->SetTopLeftRadiusX(0.f);clip->SetTopLeftRadiusY(0.f);}
        if(tr){clip->SetTopRightRadiusX(radius);clip->SetTopRightRadiusY(radius);}else{clip->SetTopRightRadiusX(0.f);clip->SetTopRightRadiusY(0.f);}
        if(bl){clip->SetBottomLeftRadiusX(radius);clip->SetBottomLeftRadiusY(radius);}else{clip->SetBottomLeftRadiusX(0.f);clip->SetBottomLeftRadiusY(0.f);}
        if(br){clip->SetBottomRightRadiusX(radius);clip->SetBottomRightRadiusY(radius);}else{clip->SetBottomRightRadiusX(0.f);clip->SetBottomRightRadiusY(0.f);}
    };corners(clip_.Get(),r.Get());corners(innerClip_.Get(),ir.Get());
    auto x=animation(m.width,now,edge_?-scale_:-scale_/2,canvasWidth*scale_/(edge_?1:2));if(m.width.settled(now))check(body_->SetOffsetX(restX));else check(body_->SetOffsetX(x.Get()));
    if(edge_){auto y=animation(m.height,now,-scale_/2,canvasHeight*scale_/2);if(m.height.settled(now))check(body_->SetOffsetY(restY));else check(body_->SetOffsetY(y.Get()));}else check(body_->SetOffsetY(0.f));
    auto dx=animation(m.dragX,now,scale_),dy=animation(m.dragY,now,scale_);check(root_->SetOffsetX(dx.Get()));check(root_->SetOffsetY(dy.Get()));
    // Shoulders: redrawn whenever the target radius changes; scaled only while it animates.
    if(std::abs(float(m.radius.target())-wingRadius_)>1e-3f)wings(float(m.radius.target()));
    const float along=float(wingAlong_),depth=float(wingDepth_);
    auto shoulder=animation(m.radius,now,1.f/wingRadius_);for(auto p:{leftScale_.Get(),rightScale_.Get()}){if(m.radius.settled(now)){check(p->SetScaleX(1.f));check(p->SetScaleY(1.f));}else{check(p->SetScaleX(shoulder.Get()));check(p->SetScaleY(shoulder.Get()));}}
    if(edge_){
        // Anchored at the screen edge; the inner end of each wing meets the body's top or bottom.
        leftScale_->SetCenterX(depth);leftScale_->SetCenterY(along);rightScale_->SetCenterX(depth);rightScale_->SetCenterY(1.f);
        wingLeft_->SetOffsetX(canvasWidth*scale_-depth);wingRight_->SetOffsetX(canvasWidth*scale_-depth);
        if(m.height.settled(now)){wingLeft_->SetOffsetY(restY-along);wingRight_->SetOffsetY(restY+restH-1);}
        else{auto ly=animation(m.height,now,-scale_/2,canvasHeight*scale_/2-along),ry=animation(m.height,now,scale_/2,canvasHeight*scale_/2-1);wingLeft_->SetOffsetY(ly.Get());wingRight_->SetOffsetY(ry.Get());}
        header_->SetOffsetX(expanded_?std::round(20*scale_):0.f);}
    else{leftScale_->SetCenterX(along);leftScale_->SetCenterY(0.f);rightScale_->SetCenterX(1.f);rightScale_->SetCenterY(0.f);wingLeft_->SetOffsetY(0.f);wingRight_->SetOffsetY(0.f);
        if(m.width.settled(now)){wingLeft_->SetOffsetX(restX-along);wingRight_->SetOffsetX(restX+restW-1);}
        else{auto lx=animation(m.width,now,-scale_/2,canvasWidth*scale_/2-along),rx=animation(m.width,now,scale_/2,canvasWidth*scale_/2-1);wingLeft_->SetOffsetX(lx.Get());wingRight_->SetOffsetX(rx.Get());}auto hx=animation(m.width,now,expanded_?0.f:scale_/2,expanded_?20*scale_:float(-m.compactWidth/2)*scale_);if(m.width.settled(now))header_->SetOffsetX(std::round(float(expanded_?20*scale_:(m.width.target()-m.compactWidth)*scale_/2)));else header_->SetOffsetX(hx.Get());}
    auto opacity=visibility(m,now);auto headerOpacity=visibility(m,now,!expanded_);
    restExpanded_=expanded_&&m.height.settled(now)&&m.width.settled(now)&&m.reveal.settled(now);restCompact_=!expanded_&&m.height.settled(now)&&m.width.settled(now);
    const bool contentEntering=restExpanded_&&now<contentEntrance_+.23,headerEntering=restCompact_&&now<headerEntrance_+.23;
    if(headerEntering){auto a=entrance(headerEntrance_,.35f);headerEffect_->SetOpacity(a.Get());}else headerEffect_->SetOpacity(headerOpacity.Get());
    // Only the content surface eases in; navigation and controls stay steady.
    if(contentEntering){auto a=entrance(contentEntrance_,.5f);check(contentEffect_->SetOpacity(a.Get()));}else check(contentEffect_->SetOpacity(opacity.Get()));if(privacyVisible_)privacyEffect_->SetOpacity(opacity.Get());else privacyEffect_->SetOpacity(0.f);check(iconEffect_->SetOpacity(opacity.Get()));if(m.live)navEffect_->SetOpacity(0.f);else check(navEffect_->SetOpacity(opacity.Get()));auto shift=animation(m.contentShift,now,scale_,std::round((m.card?16:m.live?20:38)*scale_));check(content_->SetOffsetY(shift.Get()));auto iconsShift=animation(m.contentShift,now,scale_,m.live?-18*scale_:0.f);iconLayer_->SetOffsetY(iconsShift.Get());
    auto barWidth=animation(m.volume,now,262*scale_);check(barClip_->SetRight(barWidth.Get()));
    auto ax=animation(m.artX,now,scale_),ay=animation(m.artY,now,scale_),size=animation(m.artSize,now,1.f/256),ao=animation(m.artOpacity,now);art_->SetOffsetX(ax.Get());art_->SetOffsetY(ay.Get());artScale_->SetScaleX(size.Get());artScale_->SetScaleY(size.Get());artEffect_->SetOpacity(ao.Get());
    auto blend=animation(handoff_.mix,now);incomingEffect_->SetOpacity(blend.Get());
    auto fx=animation(m.artX,now,scale_),fy=animation(m.artY,now,scale_),corner=animation(m.artSize,now,scale_,-15*scale_);artFrame_->SetOffsetX(fx.Get());artFrame_->SetOffsetY(fy.Get());badge_->SetOffsetX(corner.Get());badge_->SetOffsetY(corner.Get());
    auto slide=animation(m.swipe,now,scale_,std::round(20*scale_));check(content_->SetOffsetX(slide.Get()));
    if(barMode_>=0&&barMode_<3){auto bx=animation(m.width,now,scale_,-barInset_*scale_);spectrum_->SetOffsetX(bx.Get());}
    auto hx=animation(m.width,now,scale_/2,-81*scale_),level=animation(m.level,now,150*scale_);hudTrack_->SetOffsetX(hx.Get());hudFill_->SetOffsetX(hx.Get());hudClip_->SetRight(level.Get());
    auto nx=animation(navX,now,scale_,20*scale_);nav_->SetOffsetX(nx.Get());nav_->SetOffsetY((38+navY)*scale_);
    auto rx=animation(m.width,now,edge_&&!expanded_?0.f:scale_,edge_&&!expanded_?8*scale_:-58*scale_);rings_->SetOffsetX(rx.Get());rings_->SetOffsetY(edge_&&!expanded_?38*scale_:0.f);
    auto ba=animation(batteryAngle,now),ta=animation(timerAngle,now);batteryRotation_->SetAngle(ba.Get());timerRotation_->SetAngle(ta.Get());
    auto pulse=animation(m.pulse,now);pulseEffect_->SetOpacity(pulse.Get());glass_.animate(m,now,edge_);
    {auto slide=animation(m.slide,now,edge_?74*scale_:-44*scale_);if(edge_){stage_->SetOffsetX(slide.Get());stage_->SetOffsetY(0.f);}else{stage_->SetOffsetY(slide.Get());stage_->SetOffsetX(0.f);}
        ComPtr<IDCompositionAnimation> fade;check(device_->CreateAnimation(&fade));check(fade->SetAbsoluteBeginTime(ticks(now)));double duration=0;auto curve=approximateCurve([&](double t){return m.stageOpacity(t);},[&](double t){return m.slide.settled(t);},now,duration);
        for(auto& c:curve)check(fade->AddCubic(c.time,float(c.p),float(c.v),float(c.quadratic),float(c.cubic)));check(fade->End(duration,float(m.stageOpacity(now+10).position)));stageEffect_->SetOpacity(fade.Get());}
    if(m.card)cardIconEffect_->SetOpacity(opacity.Get());
    auto hoverX=animation(m.hoverX,now,scale_),hoverY=animation(m.hoverY,now,scale_),hoverW=animation(m.hoverW,now,scale_),hoverH=animation(m.hoverH,now,scale_),hoverOpacity=animation(m.hoverOpacity,now);hoverVisual_->SetOffsetX(hoverX.Get());hoverVisual_->SetOffsetY(hoverY.Get());hoverClip_->SetRight(hoverW.Get());hoverClip_->SetBottom(hoverH.Get());hoverEffect_->SetOpacity(hoverOpacity.Get());
    commit();
}
}
