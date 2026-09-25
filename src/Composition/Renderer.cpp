#include "Renderer.h"
#include <dxgi1_2.h>
#include "Composition/DockGeometry.h"

namespace nexus {
void Renderer::initialize(HWND hwnd,float dpi,HWND shadow) {
    dpi_=dpi;scale_=dpi/96;
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr);
    if(FAILED(hr)){software=true;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr));}
    check(d3d_.As(&dxgi_));check(DCompositionCreateDevice(dxgi_.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(device_.GetAddressOf())));
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,d2d_.GetAddressOf()));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write_.GetAddressOf())));textParams_=sharpTextParams(write_.Get());
    check(device_->CreateTargetForHwnd(hwnd,TRUE,&target_));glass_.initialize(hwnd,scale_,canvasWidth,canvasHeight,false,d3d_.Get());if(shadow)shadow_.initialize(shadow,scale_,canvasWidth,canvasHeight,true,d3d_.Get());
    for(auto* p:{std::addressof(root_),std::addressof(body_),std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_),std::addressof(art_),std::addressof(wingLeft_),std::addressof(wingRight_),std::addressof(pulseVisual_),std::addressof(hoverVisual_)})check(device_->CreateVisual(p->GetAddressOf()));
    check(root_->AddVisual(wingLeft_.Get(),FALSE,nullptr));check(root_->AddVisual(wingRight_.Get(),FALSE,nullptr));check(device_->CreateVisual(&stage_));// Soft borders antialias every rounded clip and bitmap edge below (body corners, artwork, pills).
    check(stage_->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT));check(stage_->AddVisual(root_.Get(),FALSE,nullptr));check(target_->SetRoot(stage_.Get()));check(device_->CreateEffectGroup(&stageEffect_));check(stage_->SetEffect(stageEffect_.Get()));check(root_->AddVisual(body_.Get(),FALSE,nullptr));
    for(auto* p:{std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_),std::addressof(art_),std::addressof(pulseVisual_),std::addressof(hoverVisual_)})check(body_->AddVisual(p->Get(),FALSE,nullptr));
    check(device_->CreateRectangleClip(&clip_));check(clip_->SetLeft(0.f));check(clip_->SetTop(0.f));check(body_->SetClip(clip_.Get()));
    check(device_->CreateRectangleClip(&innerClip_));check(innerClip_->SetLeft(0.f));check(innerClip_->SetTop(0.f));check(inner_->SetClip(innerClip_.Get()));
    check(device_->CreateVisual(&stub_));check(device_->CreateRectangleClip(&stubClip_));check(stub_->SetClip(stubClip_.Get()));check(device_->CreateEffectGroup(&stubEffect_));check(stub_->SetEffect(stubEffect_.Get()));stubEffect_->SetOpacity(0.f);check(root_->AddVisual(stub_.Get(),FALSE,body_.Get()));
    check(device_->CreateVisual(&bud_));check(device_->CreateRectangleClip(&budClip_));check(bud_->SetClip(budClip_.Get()));check(device_->CreateEffectGroup(&budEffect_));check(bud_->SetEffect(budEffect_.Get()));budEffect_->SetOpacity(0.f);check(root_->AddVisual(bud_.Get(),FALSE,body_.Get()));
    check(device_->CreateVisual(&budLabel_));check(device_->CreateEffectGroup(&budLabelEffect_));check(budLabel_->SetEffect(budLabelEffect_.Get()));budLabelEffect_->SetOpacity(0.f);check(bud_->AddVisual(budLabel_.Get(),FALSE,nullptr));
    check(inner_->SetOffsetX(1));check(inner_->SetOffsetY(1));
    check(content_->SetOffsetX(std::round(20*scale_)));check(content_->SetOffsetY(std::round(38*scale_)));
    check(device_->CreateTranslateTransform(&headerKick_));check(header_->SetTransform(headerKick_.Get()));
    {check(device_->CreateSkewTransform(&leanSkew_));check(device_->CreateScaleTransform(&leanScale_));leanSkew_->SetCenterX(canvasWidth*scale_/2);leanSkew_->SetCenterY(0.f);leanScale_->SetCenterX(canvasWidth*scale_/2);leanScale_->SetCenterY(0.f);
        // Attached only while the island leans: any transform on the root, even an identity, makes the
        // compositor filter every surface below it, which can bleed a neighbouring surface's edge into view.
        IDCompositionTransform* chain[]={leanSkew_.Get(),leanScale_.Get()};check(device_->CreateTransformGroup(chain,2,&leanGroup_));}
    check(bar_->SetOffsetX(58*scale_));check(bar_->SetOffsetY(251*scale_));
    check(device_->CreateRectangleClip(&barClip_));check(barClip_->SetLeft(0.f));check(barClip_->SetTop(0.f));check(bar_->SetClip(barClip_.Get()));
    check(barClip_->SetBottom(2*scale_));
    check(device_->CreateEffectGroup(&contentEffect_));
    check(content_->SetEffect(contentEffect_.Get()));check(device_->CreateEffectGroup(&barEffect_));check(bar_->SetEffect(barEffect_.Get()));
    check(device_->CreateRectangleClip(&artClip_));check(artClip_->SetLeft(0.f));check(artClip_->SetTop(0.f));check(artClip_->SetRight(256*scale_));check(artClip_->SetBottom(256*scale_));
    check(artClip_->SetTopLeftRadiusX(32.f*scale_));check(artClip_->SetTopLeftRadiusY(32.f*scale_));check(artClip_->SetTopRightRadiusX(32.f*scale_));check(artClip_->SetTopRightRadiusY(32.f*scale_));check(artClip_->SetBottomLeftRadiusX(32.f*scale_));check(artClip_->SetBottomLeftRadiusY(32.f*scale_));check(artClip_->SetBottomRightRadiusX(32.f*scale_));check(artClip_->SetBottomRightRadiusY(32.f*scale_));check(art_->SetClip(artClip_.Get()));
    for(auto pair:{std::pair{std::addressof(headerEffect_),header_.Get()},std::pair{std::addressof(artEffect_),art_.Get()},std::pair{std::addressof(pulseEffect_),pulseVisual_.Get()},std::pair{std::addressof(hoverEffect_),hoverVisual_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));check(pair.second->SetEffect(pair.first->Get()));}
    // (art_ gets a transform group below, so its size scale joins the beat pulse.)
    for(auto pair:{std::pair{std::addressof(artScale_),art_.Get()},std::pair{std::addressof(leftScale_),wingLeft_.Get()},std::pair{std::addressof(rightScale_),wingRight_.Get()},std::pair{std::addressof(hoverScale_),hoverVisual_.Get()}}){check(device_->CreateScaleTransform(pair.first->GetAddressOf()));check(pair.second->SetTransform(pair.first->Get()));}
    for(auto* p:{std::addressof(iconLayer_),std::addressof(artFrom_),std::addressof(artTo_),std::addressof(nav_),std::addressof(rings_),std::addressof(batteryDot_),std::addressof(timerDot_)})check(device_->CreateVisual(p->GetAddressOf()));
    // The beat pulse scales the cover about its centre (the surface is 256 DIPs square), then its size scale applies.
    {check(device_->CreateScaleTransform(&artPulse_));artPulse_->SetCenterX(128*scale_);artPulse_->SetCenterY(128*scale_);IDCompositionTransform* chain[]={artPulse_.Get(),artScale_.Get()};
        ComPtr<IDCompositionTransform> group;check(device_->CreateTransformGroup(chain,2,&group));check(art_->SetTransform(group.Get()));}
    check(art_->AddVisual(artFrom_.Get(),FALSE,nullptr));check(art_->AddVisual(artTo_.Get(),FALSE,nullptr));
    check(body_->AddVisual(nav_.Get(),FALSE,nullptr));
    for(auto* p:{std::addressof(navCapLeft_),std::addressof(navCapRight_),std::addressof(navMiddle_)}){check(device_->CreateVisual(p->GetAddressOf()));check(nav_->AddVisual(p->Get(),FALSE,nullptr));}
    check(device_->CreateRectangleClip(&navClipLeft_));navClipLeft_->SetLeft(0.f);navClipLeft_->SetTop(0.f);navClipLeft_->SetRight(14*scale_);navClipLeft_->SetBottom(44*scale_);check(navCapLeft_->SetClip(navClipLeft_.Get()));
    check(device_->CreateRectangleClip(&navClipRight_));navClipRight_->SetLeft(33*scale_);navClipRight_->SetTop(0.f);navClipRight_->SetRight(47*scale_);navClipRight_->SetBottom(44*scale_);check(navCapRight_->SetClip(navClipRight_.Get()));
    check(device_->CreateScaleTransform(&navMiddleScale_));check(navMiddle_->SetTransform(navMiddleScale_.Get()));check(body_->AddVisual(iconLayer_.Get(),FALSE,nullptr));check(body_->AddVisual(rings_.Get(),FALSE,nullptr));check(rings_->AddVisual(batteryDot_.Get(),FALSE,nullptr));check(rings_->AddVisual(timerDot_.Get(),FALSE,nullptr));
    for(auto pair:{std::pair{std::addressof(iconEffect_),iconLayer_.Get()},std::pair{std::addressof(incomingEffect_),artTo_.Get()},std::pair{std::addressof(navEffect_),nav_.Get()},std::pair{std::addressof(ringsEffect_),rings_.Get()},std::pair{std::addressof(batteryDotEffect_),batteryDot_.Get()},std::pair{std::addressof(timerDotEffect_),timerDot_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));check(pair.second->SetEffect(pair.first->Get()));}
    for(auto pair:{std::pair{std::addressof(batteryRotation_),batteryDot_.Get()},std::pair{std::addressof(timerRotation_),timerDot_.Get()}}){check(device_->CreateRotateTransform(pair.first->GetAddressOf()));pair.first->Get()->SetCenterX(16*scale_);pair.first->Get()->SetCenterY(16*scale_);check(pair.second->SetTransform(pair.first->Get()));}
    batteryDot_->SetOffsetY(1*scale_);timerDot_->SetOffsetY(1*scale_);batteryDot_->SetOffsetX(-4*scale_);timerDot_->SetOffsetX(20*scale_);
    for(auto& i:icons_){createIconVisual(i);iconLayer_->AddVisual(i.visual.Get(),FALSE,nullptr);}
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
    // Bands share the one content surface; hard borders keep their seams invisible.
    for(size_t k=0;k<bands_.size();++k){auto& b=bands_[k];check(device_->CreateVisual(&b.visual));check(device_->CreateEffectGroup(&b.effect));check(device_->CreateRectangleClip(&b.clip));
        b.visual->SetEffect(b.effect.Get());b.visual->SetBorderMode(DCOMPOSITION_BORDER_MODE_HARD);b.clip->SetLeft(0.f);b.clip->SetRight(std::ceil(380*scale_));
        b.clip->SetTop(std::round(float(k)*48*scale_));b.clip->SetBottom(k+1==bands_.size()?std::ceil(340*scale_):std::round(float(k+1)*48*scale_));b.visual->SetClip(b.clip.Get());check(content_->AddVisual(b.visual.Get(),FALSE,nullptr));}
    check(content_->AddVisual(cardRing_.Get(),FALSE,nullptr));
    check(device_->CreateVisual(&sheen_));check(device_->CreateEffectGroup(&sheenEffect_));sheen_->SetEffect(sheenEffect_.Get());sheenEffect_->SetOpacity(0.f);check(body_->AddVisual(sheen_.Get(),TRUE,inner_.Get()));
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
// The liquid pill: the left cap at the left end, the right cap at the right end, and the middle stretched between them.
void Renderer::placeNav(double now,float navY){
    auto left=animation(navLeft_,now,scale_,20*scale_),right=animation(navRight_,now,scale_,(20-47)*scale_),middle=animation(navLeft_,now,scale_,(20+14)*scale_);
    navCapLeft_->SetOffsetX(left.Get());navCapRight_->SetOffsetX(right.Get());navMiddle_->SetOffsetX(middle.Get());
    auto span=curveOf([this](double t){return std::max(0.,(navRight_.sample(t).position-navLeft_.sample(t).position-28)/2);},now,[this](double t){return navLeft_.settled(t)&&navRight_.settled(t);});
    navMiddleScale_->SetScaleX(span.Get());nav_->SetOffsetX(0.f);nav_->SetOffsetY((38+navY)*scale_);
}
ComPtr<IDCompositionAnimation> Renderer::curveOf(const std::function<double(double)>& value,double now,const std::function<bool(double)>& settled){
    ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(now)));double duration=0;
    auto segments=approximateCurve([&](double t){const double h=.0005;return PhysicalState{value(t),(value(t+h)-value(t-h))/(2*h)};},settled,now,duration);
    for(auto& c:segments)check(a->AddCubic(c.time,float(c.p),float(c.v),float(c.quadratic),float(c.cubic)));check(a->End(duration,float(value(now+duration))));return a;
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
        auto p=[&](float x,float y){if(right)x=along+1-x;return edge_==1?D2D1::Point2F(depth-y,x):edge_==2?D2D1::Point2F(y,x):D2D1::Point2F(x,y);};
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
    const auto box=D2D1::RectF(std::round(x*scale_)/scale_,std::round(y*scale_)/scale_,std::round((x+w)*scale_)/scale_,std::round((y+(height>0?height:size*1.6f))*scale_)/scale_);
    // Adaptive text (the compact header on Clear glass over bare wallpaper): each letter dark over a bright
    // part of the wallpaper and light over a dark one, its halo the opposite; runs of letters share a brush.
    if(adapt_&&(color==adaptInk_||color==adaptMuted_)&&!value.empty()){
        ComPtr<IDWriteTextLayout> l;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),box.right-box.left,box.bottom-box.top,&l));
        std::array<ComPtr<ID2D1SolidColorBrush>,2> inks,halos;
        for(int k=0;k<2;++k){check(rt->CreateSolidColorBrush(D2D1::ColorF(adaptVariant(color,k==1)),&inks[k]));check(rt->CreateSolidColorBrush(D2D1::ColorF(k==1?0xffffff:0x000000,haloAlpha_>0?haloAlpha_:.35f),&halos[k]));}
        std::vector<size_t> bright(value.size());
        for(UINT32 i=0;i<value.size();++i){float px=0,py=0;DWRITE_HIT_TEST_METRICS hm{};l->HitTestTextPosition(i,FALSE,&px,&py,&hm);bright[i]=backdropAt(box.left+px+hm.width/2,box.top+hm.top+hm.height/2)==1?1:0;}
        auto paint=[&](std::array<ComPtr<ID2D1SolidColorBrush>,2>& brushes){for(UINT32 i=0;i<value.size();){UINT32 j=i;while(j<value.size()&&bright[j]==bright[i])++j;l->SetDrawingEffect(brushes[bright[i]].Get(),{i,j-i});i=j;}};
        const float d=.6f/scale_;paint(halos);for(auto [dx,dy]:{std::pair{-d,0.f},std::pair{d,0.f},std::pair{0.f,-d},std::pair{0.f,d}})rt->DrawTextLayout({box.left+dx,box.top+dy},l.Get(),halos[0].Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        paint(inks);rt->DrawTextLayout({box.left,box.top},l.Get(),inks[0].Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);return;}
    // On see-through glass, a faint halo keeps text readable over any wallpaper.
    if(haloAlpha_>0){ComPtr<ID2D1SolidColorBrush> halo;check(rt->CreateSolidColorBrush(D2D1::ColorF(haloColor_,haloAlpha_),&halo));const float d=.6f/scale_;
        for(auto [dx,dy]:{std::pair{-d,0.f},std::pair{d,0.f},std::pair{0.f,-d},std::pair{0.f,d}})rt->DrawText(value.c_str(),UINT32(value.size()),f.Get(),D2D1::RectF(box.left+dx,box.top+dy,box.right+dx,box.bottom+dy),halo.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);}
    rt->DrawText(value.c_str(),UINT32(value.size()),f.Get(),box,b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
// Monospaced (Cascadia Mono, or Consolas), one line, each token in its colour; on see-through glass the halo pass first.
void Renderer::codeText(ID2D1RenderTarget* rt,const std::wstring& value,float x,float y,float w,float size,UINT32 ink,UINT32 muted,bool light){
    static const wchar_t* family=[this]{ComPtr<IDWriteFontCollection> fonts;UINT32 at=0;BOOL found=FALSE;if(SUCCEEDED(write_->GetSystemFontCollection(&fonts)))fonts->FindFamilyName(L"Cascadia Mono",&at,&found);return found?L"Cascadia Mono":L"Consolas";}();
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(family,nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    ComPtr<IDWriteTextLayout> layout;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),w,size*1.7f,&layout));
    const UINT32 colours[]={ink,light?0x8250df:0xc099ff,light?0x1a7f37:0xa8e0a0,light?0xbc4c00:0xffb86c,muted,light?0x57606a:0xaab2bf};
    std::vector<ComPtr<ID2D1SolidColorBrush>> brushes;for(auto c:colours){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(c),&b));brushes.push_back(b);}
    for(auto& span:codeSpans(value))if(span.kind>0&&span.kind<6)layout->SetDrawingEffect(brushes[size_t(span.kind)].Get(),{span.start,span.length});
    const D2D1_POINT_2F at{std::round(x*scale_)/scale_,std::round((y+1)*scale_)/scale_};
    if(haloAlpha_>0){ComPtr<IDWriteTextLayout> plain;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),w,size*1.7f,&plain));ComPtr<ID2D1SolidColorBrush> halo;check(rt->CreateSolidColorBrush(D2D1::ColorF(haloColor_,haloAlpha_),&halo));const float d=.6f/scale_;for(auto [dx,dy]:{std::pair{-d,0.f},std::pair{d,0.f},std::pair{0.f,-d},std::pair{0.f,d}})rt->DrawTextLayout({at.x+dx,at.y+dy},plain.Get(),halo.Get());}
    rt->DrawTextLayout(at,layout.Get(),brushes[0].Get());
}
ComPtr<IDCompositionAnimation> Renderer::ease(double start,float from,float to,double duration){
    const double d=duration;const float span=to-from;ComPtr<IDCompositionAnimation> a;check(device_->CreateAnimation(&a));check(a->SetAbsoluteBeginTime(ticks(start)));
    check(a->AddCubic(0,from,float(3*span/d),float(-3*span/(d*d)),float(span/(d*d*d))));check(a->End(d,to));return a;
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
        if(key!=contentKey_){const double now=seconds();if(restExpanded_&&s.expanded&&!s.reducedMotion&&!contentKey_.empty())cascade(now);contentKey_=key;}
    }
    // Glass keeps text colors but lets fills breathe; solid surfaces stay opaque.
    const bool glass=s.settings.glassy()&&glass_.available();
    haloAlpha_=glass&&(s.settings.material==2||!s.blur)?(s.light?.40f:.48f):0.f;haloColor_=s.light?0xffffff:0x000000;
    sheenStrength_=s.reducedMotion?0.f:glass?(s.light?.55f:.3f):s.light?0.f:.09f;
    if(sheenStrength_>0&&sheenTone_!=0xffffff){sheenTone_=0xffffff;surface(sheenSurface_,260,260,[&](auto* rt){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xffffff,.55f)},{.45f,D2D1::ColorF(0xffffff,.16f)},{1,D2D1::ColorF(0xffffff,0.f)}};ComPtr<ID2D1GradientStopCollection> c;check(rt->CreateGradientStopCollection(stops,3,&c));ComPtr<ID2D1RadialGradientBrush> b;check(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({130,130},{0,0},130,130),c.Get(),&b));rt->FillRectangle({0,0,260,260},b.Get());});sheen_->SetContent(sheenSurface_.Get());}
    const UINT32 bg=s.light?0xf7f7f9:0x090a0c,ink=s.light?0x202329:0xf1f3f7,muted=s.light?0x656b75:(glass?0xa3aab6:0x8e96a4),raised=glass?0xffffff:s.light?0xeceef2:0x14171d,line=glass?(s.light?0x000000:0xffffff):s.light?0xdde1e7:0x242a33;
    const float raisedAlpha=glass?(s.light?.5f:.075f):1.f,lineAlpha=glass?(s.light?.1f:.12f):1.f;const UINT32 solidRaised=glass?(s.light?0xe6e8ec:0x1b1d23):raised,track=glass?(s.light?0xc5c9d0:0x3a3e46):line;
    UINT32 accent=islandAccent(s.settings.accent,s.light,s.platform.wallpaper,s.playback.artwork.get(),s.settings.albumAccents);
    // With no album art, the playing app's icon lends its colour (Spotify green, YouTube red).
    if(s.settings.appAccents&&s.settings.albumAccents&&!s.playback.artwork&&s.playback.available&&s.playback.appIcon){auto [bright,deep]=iconColour(s.playback.appIcon);if(bright)accent=s.light?(deep?deep:accent):bright;}
    // The artwork's second hue runs the timeline gradient (deepened on light islands).
    const Artwork* art=s.settings.albumAccents?s.playback.artwork.get():nullptr;const UINT32 accent2=art&&art->secondary?(s.light?mixColor(art->secondary,0x000000,.5):art->secondary):accent;
    swipeInk_=ink;swipeAccent_=accent;bubbleColors_[0]=ink;bubbleColors_[1]=muted;bubbleColors_[2]=s.light?0xffffff:0x23272e;
    bool attached=!s.settings.floating();
    GlassStyle style;style.visible=glass;style.light=s.light;style.blur=s.blur;style.material=s.settings.material;style.tint=s.settings.glassTint/100.f;style.accent=art?(art->ambient?art->ambient:art->accent):0;style.wallpaper=s.platform.wallpaper;glass_.style(style);
    // The soft shadow under the island: deeper under dark glass, lighter under light glass, faint under solid.
    {GlassStyle shade;shade.visible=true;shade.shadow=!s.settings.shadow?0.f:glass?(s.light?.24f:.42f):(s.light?.20f:.30f);shadow_.style(shade);}if(s.expanded&&!expanded_)skyKey_.clear();expanded_=s.expanded;live_=s.live;edge_=s.settings.edge;attached_=attached;iconMotion_=s.settings.animatedIcons&&!s.reducedMotion;
    if(!headerOnly){targets.clear();iconCursor_=pageCount;for(auto& i:icons_)i.used=false;}
    if(baseColor_!=bg||accentColor_!=accent||material_!=int(glass)||cachedEdge_!=edge_){
        baseColor_=bg;accentColor_=accent;material_=int(glass);cachedEdge_=edge_;
        surface(baseSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));});body_->SetContent(glass?nullptr:baseSurface_.Get());stub_->SetContent(glass?nullptr:baseSurface_.Get());bud_->SetContent(glass?nullptr:baseSurface_.Get());
        surface(innerSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));});inner_->SetContent(glass?nullptr:innerSurface_.Get());
        wings(wingRadius_>0?wingRadius_:17);
        {const UINT32 tone=s.light?0x1c2230:0xffffff;if(hoverColor_!=tone){hoverColor_=tone;surface(hoverSurface_,440,360,[&](auto* rt){rt->Clear(D2D1::ColorF(tone,s.light?.065f:.10f));});hoverVisual_->SetContent(hoverSurface_.Get());}}
        surface(barSurface_,380,2,[&](auto* rt){rt->Clear(D2D1::ColorF(accent));});bar_->SetContent(barSurface_.Get());
        surface(navSurface_,47,44,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink,s.light?.055f:.055f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,47,44},12,12),b.Get());});
        surface(navMiddleSurface_,2,44,[&](auto* rt){rt->Clear(D2D1::ColorF(ink,.055f));});
        navCapLeft_->SetContent(navSurface_.Get());navCapRight_->SetContent(navSurface_.Get());navMiddle_->SetContent(navMiddleSurface_.Get());
    }
    wingLeft_->SetContent(attached&&!glass?leftSurface_.Get():nullptr);wingRight_->SetContent(attached&&!glass?rightSurface_.Get():nullptr);barEffect_->SetOpacity(s.page==Page::Overview&&s.expanded&&!s.live?1.f:0.f);
    {const bool pulse=s.settings.artPulse&&!s.reducedMotion&&s.playback.playing&&bool(s.playback.artwork);
        setArtPulse(pulse);}
    updateAtmosphere(s);updatePeek(s);updateArtwork(s,solidRaised);updateTimeline(s,accent,track,accent2);updateLyrics(s,ink,muted,accent);updateBubble(s);updateRings(s,accent,muted,track);updateSpectrumLayout(s,accent);updateRing(s,accent);updateCard(s,track,accent,solidRaised,ink);updateBud(s,ink,muted,accent,solidRaised);updatePrivacyBand(s,ink,muted,solidRaised);{const bool panel=s.expanded&&!s.live;const bool stats=panel&&s.page==Page::System,audio=panel&&s.page==Page::Audio,shelf=panel&&s.page==Page::Shelf;updateTabs(s,0,stats?-3.f:30.f,stats||(shelf&&s.settings.sharing)?3:2,stats?s.statsTab:shelf?s.shelfTab:s.audioTab,stats||audio||(shelf&&!(s.shelfTab==0&&s.shelfDetail>=0&&size_t(s.shelfDetail)<s.shelf.size())&&!(s.dropHover&&s.settings.sharing&&std::any_of(s.nearby.begin(),s.nearby.end(),[](auto& p){return p.paired;}))),s.light?0x262c34:0xe8ecf2);}updateHud(s,accent,track);updateBadge(s,glass?(s.light?0xf1f2f4:0x15161a):bg);
    headerSpots_.clear();compactTargets.clear();bool lyricOn=false;float sungX=0,sungW=0;
    surface(headerSurface_,600,150,[&](auto* rt){
        if(s.expanded)return;
        if(edge_){text(rt,s.battery>=0?std::to_wstring(s.battery)+L"%":L"—",0,82,64,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER);text(rt,s.charging?L"Charging":L"Battery",0,105,64,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_CENTER);return;}
        bool mini=s.settings.uiMode==0;float width=mini?72.f:float(s.settings.compactWidth);
        adapt_=!s.expanded&&!s.hud?s.adapt:nullptr;adaptX_=(canvasWidth-width)/2;adaptInk_=ink;adaptMuted_=muted;if(s.adapt.get()!=adaptSeen_){adaptSeen_=s.adapt.get();++adaptSerial_;}
        if(s.hud){
            // Level indicator: icon, compositor-driven bar (hudTrack_/hudFill_) and value.
            float x=std::max(236.f,width)/2-109;bool brightness=s.hud==2,app=s.hud==3;int level=app?s.appVolume:brightness?s.brightness:(s.muted?0:s.volume);
            if(app&&(s.playback.appIcon||!s.playback.service.empty()))identity(rt,s.playback,x-1,7,20,solidRaised);else drawIcon(rt,d2d_.Get(),brightness?Icon::Brightness:level==0?Icon::Muted:Icon::Volume,x,8,18,ink);headerSpots_.push_back({1,level>=0?std::to_wstring(level):L"—",x+218,0,11.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD,ink,34,true});return;}
        // Bars at the end, unless the spectrum ring around the cover shows the sound instead.
        const bool bars=s.settings.waveform&&s.playback.playing&&s.waveform&&s.settings.compactMedia&&(s.settings.uiMode==0||s.settings.waveformStyle==0);
        // Privacy dots: green camera, orange microphone, purple screen capture, blue location (in that order, once each).
        std::vector<UINT32> dots;for(auto c:capabilityOrder)if(std::any_of(s.privacy.begin(),s.privacy.end(),[&](auto& u){return u.capability==c;}))dots.push_back(capabilityColour(c));
        // The dots are one target: clicking them shows who is using what.
        auto drawDots=[&](float right){ComPtr<ID2D1SolidColorBrush> d;rt->CreateSolidColorBrush(D2D1::ColorF(0),&d);float x=right-float(dots.size())*9+4;compactTargets.push_back({Action::PrivacyShow,x-4,5,float(dots.size())*9+6,24});for(auto color:dots){d->SetColor(D2D1::ColorF(color));rt->FillEllipse(D2D1::Ellipse({x+3,17},3.2f,3.2f),d.Get());x+=9;}};
        if(mini){if(!s.playback.artwork||!s.settings.compactMedia){drawIcon(rt,d2d_.Get(),s.focus.running?Icon::Focus:s.charging?Icon::Power:Icon::Music,14,10,14,adaptInk(muted,21,17));}if(!dots.empty()&&s.activity.empty()){drawDots(66);return;}if(!bars)text(rt,s.activity.empty()?L"···":s.activity.starts_with(L"Volume")?std::to_wstring(s.volume):s.activity==L"Muted"?L"—":L"•",42,0,26,10,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER,34);return;}
        bool hasArt=bool(s.playback.artwork)&&s.settings.compactMedia,logo=!hasArt&&s.settings.compactMedia&&s.settings.appIcons&&s.playback.available;if(logo)identity(rt,s.playback,13,7,20,solidRaised);float start=hasArt||logo?42.f:14.f,end=width-(ringsEnabled_?(ringCount_==2?64:40):10)-(bars?32:0);if(!dots.empty()){drawDots(end);end-=float(dots.size())*9+8;}
        // id: the value rolls as odometer `id` (-1: plain text).
        auto chip=[&](Icon glyph,const std::wstring& value,float span,int id=-1){if(end-start<span+78)return;end-=span;drawIcon(rt,d2d_.Get(),glyph,end+3,11,12,adaptInk(muted,end+9,17));
            if(id<0)text(rt,value,end+19,0,span-21,10,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);else headerSpots_.push_back({id,value,end+19,0,10,DWRITE_FONT_WEIGHT_MEDIUM,ink,34});end-=7;};
        const bool media=s.settings.compactMedia&&s.playback.available,timing=s.activity.empty()&&!media&&s.settings.compactTimer&&s.focus.running;
        // Idle glance: with nothing else to show, CPU, GPU and the weather (where there is room) and today's date.
        const bool glance=s.activity.empty()&&!media&&!s.focus.running&&s.settings.compactGlance&&s.transfers.empty();
        // Media controls at the end (previous, play or pause, next), ahead of any chip; the title keeps 96 DIPs.
        if(s.settings.compactControls&&s.playback.available&&s.activity.empty()&&end-start>=78+96){const float x0=end-78;
            auto control=[&](Action a,Icon glyph,float x,float size,float box,bool enabled){compactTargets.push_back({a,x,17-box/2,box,box,enabled});
                if(enabled&&s.hovered==a){ComPtr<ID2D1SolidColorBrush> h;rt->CreateSolidColorBrush(D2D1::ColorF(ink,s.light?.08f:.13f),&h);rt->FillEllipse(D2D1::Ellipse({x+box/2,17},box/2,box/2),h.Get());}
                drawIcon(rt,d2d_.Get(),glyph,x+(box-size)/2,17-size/2,size,adaptInk(enabled?ink:muted,x+box/2,17),enabled?1.f:.5f);};
            control(Action::Previous,Icon::Previous,x0,11,24,s.playback.canPrevious);control(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,x0+26,13,26,s.playback.canToggle);control(Action::Next,Icon::Next,x0+54,11,24,s.playback.canNext);
            end=x0-6;}
        // Chips in the order chosen in Settings, filled from the right.
        auto percent=[](double v){wchar_t b[16];swprintf(b,16,L"%.0f%%",v);return std::wstring(b);};
        auto skyIcon=[](int code,bool day){switch(skyOf(code)){case Sky::Clear:return day?Icon::Sun:Icon::Moon;case Sky::PartlyCloudy:return day?Icon::PartlyCloudy:Icon::Cloud;case Sky::Cloudy:return Icon::Cloud;case Sky::Fog:return Icon::Fog;case Sky::Drizzle:case Sky::Rain:return Icon::Rain;case Sky::Snow:return Icon::Snow;default:return Icon::Storm;}};
        // Phase 5G: files on their way to or from another PC, as a chip that fills with them.
        if(!s.transfers.empty()&&s.activity.empty()){uint64_t done=0,total=0;for(auto& t:s.transfers){done+=t.done;total+=t.total;}chip(s.transfers.front().outgoing?Icon::Send:Icon::Download,percent(total?100.*double(done)/double(total):0),52);}
        for(int k=chipCount-1;k>=0;--k)switch(s.settings.chips[size_t(k)]){
        case 0:if(s.settings.compactClock){SYSTEMTIME t{};GetLocalTime(&t);wchar_t value[12];swprintf(value,12,L"%02u:%02u",t.wHour,t.wMinute);chip(Icon::Clock,value,58);}break;
        case 1:if(s.settings.compactVolume)chip(s.muted?Icon::Muted:Icon::Volume,std::to_wstring(s.volume),48,6);break;
        case 2:if(s.settings.compactBattery&&s.battery>=0&&!ringsEnabled_)chip(s.charging?Icon::Power:Icon::Battery,std::to_wstring(s.battery)+L"%",56,7);break;
        case 3:if(s.settings.compactTimer&&s.focus.running)chip(Icon::Focus,clockText(std::ceil(s.focus.displayed(seconds()))),64,8);break;
        case 4:if(glance&&s.system.cpu>=0)chip(Icon::Processor,percent(s.system.cpu),52,10);break;
        case 5:if(glance&&s.system.gpu>=0)chip(Icon::Gauge,percent(s.system.gpu),52,11);break;
        case 6:if(glance&&s.settings.weather&&s.weather.valid)chip(skyIcon(s.weather.code,s.weather.day),temperatureText(s.weather.temperature,s.settings.weatherUnit),54,12);break;
        default:break;}
        std::wstring label=!s.activity.empty()?s.activity:media?s.playback.title:!s.transfers.empty()?(s.transfers.front().outgoing?L"Sending to ":L"Receiving from ")+s.transfers.front().name:L"Ready";
        // A running timer: its mode when the timer chip already shows the time, else the time itself, rolling.
        const bool timerChip=std::any_of(headerSpots_.begin(),headerSpots_.end(),[](auto& p){return p.id==8;}),rolling=timing&&!timerChip;
        if(timing)label=timerChip?(s.focus.mode==FocusClock::Mode::Break?L"Break":s.focus.mode==FocusClock::Mode::Stopwatch?L"Stopwatch":L"Focus"):clockText(std::ceil(s.focus.displayed(seconds())));
        else if(glance){SYSTEMTIME t{};GetLocalTime(&t);wchar_t date[40];if(GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&t,L"ddd d MMM",date,40,nullptr))label=date;}
        // While a synced song plays, the lyric line (or its gap dots) takes the label's place, on its own layers.
        const bool sung=s.activity.empty()&&s.settings.compactMedia&&s.settings.lyrics&&s.settings.lyricsCompact&&s.lyrics&&s.playback.playing&&s.lyricLine>=0&&size_t(s.lyricLine)<s.lyrics->size();
        if(sung){label=(*s.lyrics)[size_t(s.lyricLine)].text;lyricOn=true;sungX=start;sungW=std::max(12.f,end-start);}
        else if(rolling)headerSpots_.push_back({9,label,start,0,11.5f,DWRITE_FONT_WEIGHT_MEDIUM,ink,34});
        else text(rt,label,start,0,std::max(12.f,end-start),11.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);
        // A new label eases in, except a line of lyrics after another or a tick of the timer, which move on their own.
        const std::wstring labelKey=sung?L"\x1fsung":rolling?L"\x1ftimer":label;
        if(labelKey!=headerLabel_){if(restCompact_&&!s.expanded&&!s.reducedMotion&&!headerLabel_.empty()){headerEntrance_=seconds();auto a=entrance(headerEntrance_,.35f);headerEffect_->SetOpacity(a.Get());}headerLabel_=labelKey;}
    });header_->SetContent(headerSurface_.Get());placeOdometers(headerSpots_,{1,6,7,8,9,10,11,12},header_.Get(),s.reducedMotion);lineLyric(compactLyric_,header_.Get(),s,lyricOn,sungX,0,sungW,34,11.5f,ink,s.reducedMotion);adapt_=nullptr;if(headerOnly){commit();return;}
    drawingContent_=true;iconRequests_.clear();caretTarget_=42;answerY_=-1;answerText_.clear();contentSpots_.clear();bool liveLyricOn=false;
    // Four DIPs wider and taller than the bands show: a band sampled at its edge (a sub-pixel offset while a
    // spring settles) then reads this surface's own clear pixels, not whatever the compositor packed beside it.
    surface(contentSurface_,384,344,[&](auto* rt){
        const bool logoArt=s.settings.appIcons&&s.playback.available&&(!s.playback.service.empty()||s.playback.appIcon);
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink),&b);
        auto box=[&](float x,float y,float w,float h,UINT32 color,float radius=12){b->SetColor(D2D1::ColorF(color,color==raised?raisedAlpha:1.f));rt->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),b.Get());};
        auto hairline=[&](float x,float y,float w){b->SetColor(D2D1::ColorF(line,lineAlpha));rt->DrawLine({x,y},{x+w,y},b.Get(),1/scale_);};
        auto label=[&](const std::wstring& value,float x,float y,float w,float h,float size,UINT32 color,DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_MEDIUM){text(rt,value,x,y,w,size,color,weight,DWRITE_TEXT_ALIGNMENT_CENTER,h);};
        auto button=[&](Action a,const std::wstring& value,float x,float y,float w,float h,bool selected=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});box(x,y,w,h,selected?(s.light?0x262c34:0xe8ecf2):raised,9);label(value,x+6,y,w-12,h,11.5f,!enabled?muted:selected?(s.light?0xf8f8fa:0x171b22):ink);};
        auto iconButton=[&](Action a,Icon glyph,float x,float y,float w=32,float h=32,bool primary=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});if(primary)box(x,y,w,h,!enabled?raised:s.light?0x252b33:0xe8ecf2,h/2);icon(a,glyph,x+(w-18)/2,y+(h-18)/2,18,!enabled?muted:primary?(s.light?0xffffff:0x161b22):ink);};
        auto value=[](double v,int precision=0){if(v<0)return std::wstring(L"—");wchar_t buf[48];swprintf(buf,48,precision?L"%.1f":L"%.0f",v);return std::wstring(buf);};
        if(s.card&&s.command.active){
            const auto& c=s.command;const int rowsMax=5;box(0,0,380,42,raised,14);drawIcon(rt,d2d_.Get(),c.clips?Icon::Clipboard:Icon::Search,14,12,18,c.text.empty()?muted:ink);
            // The typed line scrolls left when it is wider than the field, keeping the caret visible.
            const float field=320;float width=measure(c.text,14),before=measure(c.text.substr(0,std::min(c.caret,c.text.size())),14),shift=std::max(0.f,std::min(width-field,before-field+8));
            if(c.text.empty())text(rt,c.clips?L"Search your copies":L"Type an app, file, command or setting",46,0,field,14,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,42);
            else{rt->PushAxisAlignedClip(D2D1::RectF(42,0,42+field+4,42),D2D1_ANTIALIAS_MODE_ALIASED);text(rt,c.text,42-shift,0,width+40,14,ink,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,42);rt->PopAxisAlignedClip();}
            caretTarget_=42+before-shift;
            // Ghost completion: the rest of the top result, faint after the caret, with a Tab keycap.
            const std::wstring ghost=!c.clips&&!c.results.empty()&&c.caret==c.text.size()?ghostSuffix(c.text,c.results[0].completion):std::wstring{};
            if(!ghost.empty()){const float gx=42-shift+width,right=42+field-36;if(right-gx>16){rt->PushAxisAlignedClip(D2D1::RectF(gx,0,right,42),D2D1_ANTIALIAS_MODE_ALIASED);text(rt,ghost,gx,0,right-gx+40,14,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,42);rt->PopAxisAlignedClip();
                box(right+4,11,30,20,raised,6);label(L"Tab",right+4,11,30,20,9.5f,muted,DWRITE_FONT_WEIGHT_SEMI_BOLD);}}
            auto glyph=[](CommandKind k){switch(k){case CommandKind::Volume:case CommandKind::VolumeStep:case CommandKind::Unmute:return Icon::Volume;case CommandKind::Mute:return Icon::Muted;case CommandKind::Play:return Icon::Play;case CommandKind::Pause:return Icon::Pause;
                case CommandKind::Next:return Icon::Next;case CommandKind::Previous:return Icon::Previous;case CommandKind::Timer:return Icon::Focus;case CommandKind::Stopwatch:return Icon::Clock;case CommandKind::StopTimer:return Icon::Reset;case CommandKind::OpenApp:return Icon::Apps;
                case CommandKind::SearchFiles:return Icon::Search;case CommandKind::OpenSettings:return Icon::Settings;case CommandKind::Workspace:case CommandKind::SaveWorkspace:case CommandKind::DeleteWorkspace:return Icon::Workspace;
                case CommandKind::Clipboard:case CommandKind::ClearClipboard:return Icon::Clipboard;case CommandKind::Lock:return Icon::Lock;case CommandKind::MicMute:return Icon::MicOff;case CommandKind::Snip:return Icon::Snip;case CommandKind::CopyText:return Icon::Text;case CommandKind::PickColour:return Icon::Eyedropper;case CommandKind::ClipPaste:return Icon::Clipboard;case CommandKind::MicUnmute:case CommandKind::MicToggle:return Icon::Microphone;
                case CommandKind::DarkMode:case CommandKind::Sleep:return Icon::Moon;case CommandKind::Bluetooth:return Icon::Bluetooth;case CommandKind::WiFi:return Icon::Wifi;case CommandKind::Airplane:return Icon::Plane;case CommandKind::EmptyBin:return Icon::Trash;
                case CommandKind::Restart:return Icon::Reset;case CommandKind::ShutDown:return Icon::Power;case CommandKind::OpenFile:return Icon::File;case CommandKind::Currency:return Icon::Exchange;default:return Icon::Info;}};
            if(c.results.empty()&&c.clips){drawIcon(rt,d2d_.Get(),Icon::Clipboard,14,62,16,muted);text(rt,s.settings.clipboardHistory?(c.text.empty()?L"Nothing copied yet":L"No copies match"):L"Turn on clipboard history to search your copies",46,52,330,11,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,38);}
            else if(c.results.empty()){
                // Nothing typed yet: a few things to try.
                const std::pair<Icon,const wchar_t*> hints[]={{Icon::Moon,L"dark mode  \u00b7  bluetooth off  \u00b7  volume 40"},{Icon::Focus,L"focus 25  \u00b7  timer 10 min  \u00b7  empty recycle bin"},{Icon::Search,L"spotify  \u00b7  budget pdfs from last month  \u00b7  100 usd to eur"}};
                for(int i=0;i<3;++i){float y=52+i*40.f;drawIcon(rt,d2d_.Get(),hints[i].first,14,y+10,16,muted);text(rt,hints[i].second,46,y,330,11,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,38);}
            }
            // Rows, with a small header before each group when the results span several (Apps, Files, Actions…).
            const auto rowsLaid=commandRows(c.results,size_t(rowsMax),!c.clips);
            for(auto [hy,group]:rowsLaid.headers)text(rt,groupName(group),8,hy,200,9.5f,muted,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,18);
            for(int i=0;i<int(rowsLaid.rows.size());++i){const auto& r=c.results[i];float y=rowsLaid.rows[size_t(i)];Action a=Action(int(Action::CommandResultBase)+i);targets.push_back({a,0,y,380,38,r.kind!=CommandKind::None});
                const auto* icon=size_t(i)<c.icons.size()?c.icons[i].get():nullptr;
                // A colour code shows itself as a swatch.
                if(r.kind==CommandKind::Colour){ComPtr<ID2D1SolidColorBrush> sw;rt->CreateSolidColorBrush(D2D1::ColorF(UINT32(r.value)),&sw);rt->FillEllipse(D2D1::Ellipse({21,y+19},14,14),sw.Get());sw->SetColor(D2D1::ColorF(ink,.25f));rt->DrawEllipse(D2D1::Ellipse({21,y+19},14,14),sw.Get(),1/scale_);}
                else if(icon)drawPreview(rt,*icon,7,y+5,28,28);else{box(6,y+4,30,30,raised,15);drawIcon(rt,d2d_.Get(),r.kind==CommandKind::DarkMode&&r.value==0?Icon::Sun:glyph(r.kind),13,y+11,16,r.kind==CommandKind::None?muted:ink);}
                const bool keycap=i==c.selected&&r.kind!=CommandKind::None;const wchar_t* cap=c.armed?L"Enter again":L"Enter";const float capWidth=keycap?measure(cap,9.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD)+14:0;
                // Rows of the empty bar say why they are there; the tag sits before the Enter keycap.
                const wchar_t* tag=!rowsLaid.headers.empty()?nullptr:r.section==1?L"Pinned":r.section==2?L"Suggested":r.section==3?L"Recent":nullptr;const float tagWidth=tag?measure(tag,9.5f,DWRITE_FONT_WEIGHT_MEDIUM)+2:0;
                const float tagRight=keycap?372-capWidth-8:374;const float room=(tag?tagRight-tagWidth-10:tagRight)-46;
                // An answer is drawn by its rolling-digit layer; matched letters are highlighted.
                if(r.kind==CommandKind::Currency&&!r.answer.empty()){if(answerY_<0){answerY_=y-1;answerText_=r.answer;}}
                else if(!r.marks.empty())markedText(rt,r.title,r.marks,46,y+2,room,12.5f,ink,accent);else text(rt,r.title,46,y+2,room,12.5f,r.kind==CommandKind::None?muted:ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
                text(rt,r.detail,46,y+21,room,10,muted);if(tag)text(rt,tag,tagRight-tagWidth,y+10,tagWidth,9.5f,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_TRAILING,19);
                if(keycap){box(372-capWidth,y+10,capWidth,19,s.light?0xffffff:0x2c323c,6);label(cap,372-capWidth,y+10,capWidth,19,9.5f,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);}}
            const float footer=c.results.empty()?commandFooterY(c.clips?1:3):rowsLaid.footer;
            if(!c.status.empty()){const bool workspace=size_t(c.selected)<c.results.size()&&c.results[size_t(c.selected)].kind==CommandKind::Workspace;drawIcon(rt,d2d_.Get(),c.error?Icon::Info:c.armed?(workspace?Icon::Workspace:Icon::Info):Icon::Check,2,footer+6,14,c.error?0xe5484d:accent);text(rt,c.status,22,footer,356,10.5f,c.error?0xe5484d:ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,26);}
            else{float x=0;
                // The footer names what the keys do for the selected row.
                const CommandResult* chosen=size_t(c.selected)<c.results.size()?&c.results[size_t(c.selected)]:nullptr;std::vector<std::pair<const wchar_t*,const wchar_t*>> hints;
                if(c.clips)hints=c.paste?decltype(hints){{L"Enter",L"Paste"},{L"Shift+Enter",L"Copy only"},{L"Esc",L"Close"}}:decltype(hints){{L"Enter",L"Copy"},{L"\u2191 \u2193",L"Choose"},{L"Esc",L"Close"}};
                else if(chosen&&chosen->kind==CommandKind::OpenFile)hints={{L"Enter",L"Open"},{L"Ctrl+Enter",L"Show in folder"},{L"Ctrl+C",L"Copy path"}};
                else if(chosen&&(chosen->kind==CommandKind::Currency||chosen->kind==CommandKind::Colour))hints={{L"Enter",L"Copy"},{L"Esc",L"Close"}};
                else if(!ghost.empty())hints={{L"Enter",L"Run"},{L"Tab",L"Complete"},{L"Esc",L"Close"}};
                else if(c.text.empty()&&chosen&&chosen->kind!=CommandKind::None)hints={{L"Enter",L"Run"},{L"Ctrl+P",L"Pin"},{L"Esc",L"Close"}};
                else hints={{L"Enter",L"Run"},{L"\u2191 \u2193",L"Choose"},{L"Esc",L"Close"}};
                for(auto [cap,what]:hints){float w=measure(cap,9,DWRITE_FONT_WEIGHT_SEMI_BOLD)+12;box(x,footer+4,w,18,raised,5);label(cap,x,footer+4,w,18,9,muted,DWRITE_FONT_WEIGHT_SEMI_BOLD);x+=w+6;float t=measure(what,10)+2;text(rt,what,x,footer,t,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,26);x+=t+16;}}
            return;
        }
        if(s.card&&((s.notice.kind>=5&&s.notice.kind<=7)||s.notice.kind==12)){
            // Privacy card: the app's own icon in a ring of the capability's colour.
            const auto& n=s.notice;const wchar_t* what=n.kind==5?L"Camera in use":n.kind==6?L"Microphone in use":n.kind==12?L"Screen being captured":L"Location in use";
            text(rt,what,72,6,196,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.app.empty()?std::wstring(L"An app"):n.app,72,30,118,11,muted);
            const UINT32 color=n.kind==5?0x30d158:n.kind==6?0xff9f0a:n.kind==12?0xbf5af2:0x0a84ff;box(292,10,40,40,raised,20);drawIcon(rt,d2d_.Get(),n.kind==5?Icon::Camera:n.kind==6?Icon::Microphone:n.kind==12?Icon::Snip:Icon::Location,302,20,20,color);
            // One click to that permission's page in Windows Settings.
            button(Action::PrivacySettings,L"Settings",198,26,82,24);
            return;
        }
        if(s.card&&s.notice.kind>=14&&s.notice.kind<=17){
            // Sharing: the pairing code both PCs show, a file to accept, or how a transfer or a pairing went.
            const auto& n=s.notice;
            if(n.kind==14){text(rt,L"Pair with "+n.app+L"?",72,2,176,13,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.detail,72,20,176,19,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,30);
                text(rt,L"The same code shows on both PCs",72,46,176,9,muted);button(Action::SharePair,L"Pair",256,4,76,26,true);button(Action::ShareDecline,L"Not now",256,34,76,22);}
            else if(n.kind==15){text(rt,n.app+L" is sending",72,6,176,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.detail,72,30,176,11,muted);
                button(Action::ShareAccept,L"Accept",256,4,76,26,true);button(Action::ShareDecline,L"Decline",256,34,76,22);}
            // Music from another PC: the song, its artist and where it comes from.
            else if(n.kind==17){text(rt,n.app,72,6,176,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.detail,72,30,176,11,muted);
                button(Action::HandoffPlay,L"Play here",256,4,76,26,true);button(Action::HandoffDecline,L"Not now",256,34,76,22);}
            else{const float room=n.path.empty()?256.f:176.f;text(rt,n.app,72,6,room,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.detail,72,30,room,11,muted);if(!n.path.empty())button(Action::ShareShow,L"Show",256,14,76,28,true);}
            return;
        }
        if(s.card&&s.notice.kind==13){
            // The week on battery: health against a week ago, how often it charged and a typical day's use.
            const auto& w=s.week;std::wstring detail=std::to_wstring(w.charges)+(w.charges==1?L" charge":L" charges");if(w.usedPerDay>=0)detail+=L"  \u00b7  "+std::to_wstring(int(std::lround(w.usedPerDay)))+L"% a day";
            text(rt,L"Your battery this week",72,6,186,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,detail,72,30,186,11,muted);
            if(s.healthNow>=0){text(rt,std::to_wstring(int(std::lround(s.healthNow*100)))+L"%",256,2,76,24,ink,DWRITE_FONT_WEIGHT_LIGHT,DWRITE_TEXT_ALIGNMENT_TRAILING,32);
                const double change=s.healthBefore>=0?(s.healthNow-s.healthBefore)*100:0;wchar_t d[48];if(s.healthBefore>=0&&std::abs(change)>=.05)swprintf(d,48,L"health  %+.1f",change);else swprintf(d,48,L"health");text(rt,d,236,32,96,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
            return;
        }
        if(s.card&&s.notice.kind==8){
            // Sound moved to headphones: say where from, and offer to go back.
            const auto& n=s.notice;const float room=n.switchBack?168.f:256.f;text(rt,n.device.name,72,6,room,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
            text(rt,n.app.empty()?std::wstring(L"Sound is playing here now"):L"Moved from "+n.app,72,30,room,11,muted);
            if(n.switchBack)button(Action::SwitchBack,L"Switch back",250,14,86,28,true);
            return;
        }
        if(s.card&&s.notice.kind>=9&&s.notice.kind<=11){
            // Capture results: what was copied or saved, in one line each.
            const auto& n=s.notice;const bool shelf=n.kind==11;const float room=shelf?172.f:256.f;
            text(rt,n.app,72,6,room,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,n.detail,72,30,room,11,muted);
            if(shelf)button(Action::Shelf,L"Open Shelf",252,14,84,28,true);
            return;
        }
        if(s.card&&s.notice.kind){
            auto& n=s.notice;const bool power=n.kind==3||n.kind==4;const int percent=power?s.battery:n.device.battery;
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
            // While a synced song plays, the lyric line (its own layers, like the Command Center's) replaces the artist.
            {const bool sung=s.playback.available&&s.settings.lyrics&&s.lyrics&&s.playback.playing&&s.lyricLine>=0&&size_t(s.lyricLine)<s.lyrics->size();liveLyricOn=sung;
            if(!sung)text(rt,s.playback.available?s.playback.artist:s.focus.running?clockText(std::ceil(s.focus.displayed(seconds()))):L"Sound · focus · things within reach",68,23,250,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);}
            if(!s.playback.artwork){box(0,0,52,52,raised,14);if(logoArt)identity(rt,s.playback,10,10,32,solidRaised);else drawIcon(rt,d2d_.Get(),s.focus.running?Icon::Focus:Icon::Music,15,15,22,accent);}
            iconButton(Action::Previous,Icon::Previous,66,44,36,32,false,s.playback.canPrevious);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,116,40,40,40,true,s.playback.canToggle);iconButton(Action::Next,Icon::Next,170,44,36,32,false,s.playback.canNext);iconButton(Action::Mute,s.muted?Icon::Muted:Icon::Volume,228,44,32,32);text(rt,std::to_wstring(s.volume)+L"%",266,52,54,11,muted);
            button(Action::Overview,L"Command Center",0,88,150,24);button(Action::Shelf,L"Shelf",160,88,73,24);iconButton(Action::Settings,Icon::Settings,284,86,36,28);
            if(s.sessions.size()>1){int n=int(std::min<size_t>(6,s.sessions.size()));float total=12+(n-1)*9.f,x=26-total/2;
                for(int i=0;i<n;++i){bool selected=i==s.session;float w=selected?12.f:5.f;b->SetColor(D2D1::ColorF(selected?ink:muted,selected?1.f:.55f));rt->FillRoundedRectangle(D2D1::RoundedRect({x,58,x+w,63},2.5f,2.5f),b.Get());targets.push_back({Action(int(Action::SessionBase)+i),x-2,54,w+4,13});x+=w+4;}}
            return;
        }
        const wchar_t* titles[]={L"",L"Now playing",L"System overview",L"Make room for focus",L"Make it yours",L"Within reach",L"Sound, your way",L"Quick controls"};
        int chips=s.mediaPage()?int(std::min<size_t>(5,s.sessions.size())):0;if(chips<2)chips=0;
        auto tabs=[&](std::initializer_list<std::pair<Action,const wchar_t*>> items,int selected,float y){int i=0;for(auto& [a,name]:items){float x=i*88.f;targets.push_back({a,x,y,82,26});label(name,x,y,82,26,11.5f,i==selected?(s.light?0xf8f8fa:0x171b22):muted,i==selected?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_MEDIUM);++i;}};
        if(s.page==Page::System)tabs({{Action::StatsSystem,L"System"},{Action::StatsBattery,L"Battery"},{Action::StatsDevices,L"Devices"}},s.statsTab,-3);
        const bool lyricsButton=s.mediaPage()&&s.settings.lyrics&&s.lyrics&&!s.lyrics->empty(),panel=lyricsPanel(s);
        const float titleRoom=(chips?340.f-chips*28:302.f)-(lyricsButton&&chips?32.f:0.f);
        if(s.page==Page::System){}
        else if(s.library&&s.page==Page::Media){iconButton(Action::LibraryBack,Icon::ArrowLeft,-4,-4,32,28);text(rt,L"Library",32,-4,150,18,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,28);
            const bool any=s.libraryTracks&&!s.libraryTracks->empty();targets.push_back({Action::LibraryShuffle,226,-3,118,26,any});box(226,-3,118,26,raised,13);drawIcon(rt,d2d_.Get(),Icon::Shuffle,238,3,14,any?accent:muted,any?1.f:.5f);text(rt,L"Shuffle all",258,-3,82,10.5f,any?ink:muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,26);}
        else if(panel){const float w=std::min(titleRoom,measure(s.playback.title,16,DWRITE_FONT_WEIGHT_SEMI_BOLD)+4);text(rt,s.playback.title,0,-4,w,16,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,28);if(titleRoom-w>40)text(rt,s.playback.artist,w+8,-3,titleRoom-w-8,11,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,28);}
        else if(*titles[int(s.page)])text(rt,titles[int(s.page)],0,-4,titleRoom,18,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,28);
        // Home carries today's date in the user's own format ("Thursday, 24 September").
        else if(s.page==Page::Overview){SYSTEMTIME t{};GetLocalTime(&t);wchar_t day[80]{};if(GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&t,L"dddd, d MMMM",day,80,nullptr))text(rt,day,0,-4,280,18,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,28);}
        if(lyricsButton){const float x=chips?344-float(chips)*28-30:318.f;targets.push_back({Action::LyricsToggle,x,-4,28,28});if(s.lyricsView)box(x,-4,28,28,s.light?0x262c34:0xe8ecf2,14);icon(Action::LyricsToggle,Icon::Lyrics,x+5,1,18,s.lyricsView?(s.light?0xf8f8fa:0x171b22):ink);}
        if(s.page==Page::Audio&&s.micAvailable){targets.push_back({Action::MicMute,318,-4,28,28});if(s.micMuted)box(318,-4,28,28,s.light?0xfde2e1:0x3a1d1f,14);icon(Action::MicMute,s.micMuted?Icon::MicOff:Icon::Microphone,323,1,18,s.micMuted?0xe5484d:ink);}
        iconButton(Action::Close,Icon::Close,352,-4,28,28);if(s.page==Page::Overview)iconButton(Action::CommandOpen,Icon::Search,318,-4,28,28);
        for(int i=0;i<chips;++i){auto& session=s.sessions[i];float x=344-float(chips-i)*28;bool selected=i==s.session;box(x,-2,24,24,raised,12);
            identity(rt,session,x+3,1,18,solidRaised);
            if(selected){b->SetColor(D2D1::ColorF(accent));rt->DrawEllipse(D2D1::Ellipse({x+12,10},12.5f,12.5f),b.Get(),1.6f);}
            targets.push_back({Action(int(Action::SessionBase)+i),x,-2,24,24});}
        // The weather statistic's sky plays while Home shows it.
        {int slot=-1;for(int i=0;i<3;++i)if(s.settings.homeMetrics[i]==8)slot=i;
            skyWanted_=s.page==Page::Overview&&s.expanded&&!s.live&&!s.card&&slot>=0&&s.settings.weather&&s.weather.valid&&!s.reducedMotion;skyX_=slot*130.f;}
        if(s.page==Page::Overview){
            if(!s.playback.artwork){box(0,43,64,64,raised,15);if(logoArt)identity(rt,s.playback,12,55,40,solidRaised);else drawIcon(rt,d2d_.Get(),Icon::Music,19,62,26,muted);}
            // Title and artist sit as one block centred on the artwork (43..107), as does the play button.
            text(rt,s.playback.available?s.playback.title:L"A quieter place for everything",80,52,252,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.available?s.playback.artist:L"Play something. Find your rhythm.",80,77,252,11,muted);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,340,55,40,40,true,s.playback.canToggle);targets.push_back({Action::Media,0,38,328,74});
            // The weather statistic shows the sky's icon and the place.
            auto skyIcon=[](int code,bool day){switch(skyOf(code)){case Sky::Clear:return day?Icon::Sun:Icon::Moon;case Sky::PartlyCloudy:return day?Icon::PartlyCloudy:Icon::Cloud;case Sky::Cloudy:return Icon::Cloud;case Sky::Fog:return Icon::Fog;case Sky::Drizzle:case Sky::Rain:return Icon::Rain;case Sky::Snow:return Icon::Snow;default:return Icon::Storm;}};
            const Icon glyphs[]={Icon::Processor,Icon::Memory,Icon::Battery,Icon::Download,Icon::Upload,Icon::Disk,Icon::Clock,Icon::Gauge,s.weather.valid?skyIcon(s.weather.code,s.weather.day):Icon::Cloud};
            const std::wstring place=s.weather.valid?s.weather.place.substr(0,s.weather.place.find(L',')):std::wstring(L"Weather");const wchar_t* names[]={L"CPU",L"Memory",L"Battery",L"Download",L"Upload",L"Disk free",L"Uptime",L"GPU",place.c_str()};
            for(int i=0;i<3;++i){int metric=s.settings.homeMetrics[i];float x=i*130.f;box(x,126,120,68,raised,13);drawIcon(rt,d2d_.Get(),glyphs[metric],x+12,137,14,muted);text(rt,names[metric],x+33,136,77,10,muted);std::wstring number;switch(metric){case 0:number=value(s.system.cpu)+ (s.system.cpu>=0?L"%":L"");break;case 1:number=s.system.ramTotalGiB?value(s.system.ramPercent)+L"%":L"—";break;case 2:number=s.battery>=0?std::to_wstring(s.battery)+L"%":L"—";break;case 3:number=s.system.networkAvailable?rateText(s.system.download):L"—";break;case 4:number=s.system.networkAvailable?rateText(s.system.upload):L"—";break;case 5:number=s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—";break;case 7:number=s.system.gpu<0?L"—":value(s.system.gpu)+L"%";break;case 8:number=s.settings.weather&&s.weather.valid?temperatureText(s.weather.temperature,s.settings.weatherUnit):L"—";break;default:number=clockText(double(s.system.uptime));break;}contentSpots_.push_back({3+i,number,x+12,156,metric>=3&&metric!=7&&metric!=8?18.f:23.f,DWRITE_FONT_WEIGHT_SEMI_BOLD,ink});
                if(metric==8&&s.weather.valid)text(rt,skyLabel(skyOf(s.weather.code)),x+60,160,56,9.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING,22);}
            iconButton(Action::Mute,s.muted?Icon::Muted:Icon::Volume,0,200,28,28);b->SetColor(D2D1::ColorF(line,lineAlpha));rt->DrawLine({38,214},{300,214},b.Get(),2);text(rt,s.muted?L"Muted":std::to_wstring(s.volume)+L"%",306,206,36,10.5f,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,16);iconButton(Action::Audio,Icon::Audio,346,200,34,28);targets.push_back({Action::VolumeSlider,38,201,262,26});
        }else if(s.page==Page::Media&&s.library){
            // Phase 5G: songs from the Music folder, played by the island itself. Five rows; the wheel or the arrows scroll.
            const size_t n=s.libraryTracks?s.libraryTracks->size():0;
            if(!n){box(0,40,380,150,raised,18);drawIcon(rt,d2d_.Get(),Icon::Library,176,58,28,s.libraryScanning?muted:accent);
                label(s.libraryScanning?L"Looking through your Music folder":L"No songs in your Music folder yet",20,96,340,24,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
                label(s.libraryScanning?L"Songs appear here as soon as they are found":L"MP3, M4A, FLAC, WAV and WMA files there play right here",20,122,340,20,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);}
            for(int row=0;row<5;++row){const size_t i=size_t(s.libraryOffset+row);if(i>=n)break;const auto& t=(*s.libraryTracks)[i];const float y=34+float(row)*34;const Action a=Action(int(Action::LibraryItemBase)+row);
                const bool playing=t.path==s.libraryPlaying;targets.push_back({a,0,y,380,31});box(0,y,380,31,raised,9);
                if(size_t(row)<s.libraryArt.size()&&s.libraryArt[size_t(row)])drawPreview(rt,*s.libraryArt[size_t(row)],5,y+3.5f,24,24);else drawIcon(rt,d2d_.Get(),Icon::Music,9,y+8,15,playing?accent:muted);
                text(rt,t.title,38,y+1,250,11,playing?accent:ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,16);text(rt,t.artist.empty()?(t.album.empty()?std::wstring(L"Unknown artist"):t.album):t.artist,38,y+15,250,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,14);
                if(playing)drawIcon(rt,d2d_.Get(),Icon::Audio,300,y+9,13,accent);text(rt,t.duration>0?clockText(t.duration):L"",310,y,62,9.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING,31);}
            if(n){wchar_t range[64];swprintf(range,64,L"%zu\u2013%zu of %zu songs",size_t(s.libraryOffset)+1,std::min(n,size_t(s.libraryOffset)+5),n);text(rt,range,0,206,220,9.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,22);
                iconButton(Action::LibraryUp,Icon::ArrowUp,316,204,30,24,false,s.libraryOffset>0);iconButton(Action::LibraryDown,Icon::ArrowDown,350,204,30,24,false,size_t(s.libraryOffset)+5<n);}
        }else if(s.page==Page::Media){
            bool video=s.settings.mediaLayout==2||(s.settings.mediaLayout==0&&s.playback.kind==MediaKind::Video);float tx=video?166.f:118.f,tw=380-tx;
            if(!s.playback.artwork){box(0,video?30:44,video?148:100,video?148:100,raised,18);if(logoArt)identity(rt,s.playback,video?42:22,video?72:66,video?64:56,solidRaised);else drawIcon(rt,d2d_.Get(),Icon::Music,video?56:34,video?85:77,32,muted);}
            if(s.playback.canSeek&&s.playback.duration>0){const float ay=video?30.f:44.f,as=video?148.f:100.f;targets.push_back({Action::SkipBack,0,ay,as/2,as});targets.push_back({Action::SkipForward,as/2,ay,as/2,as});}
            // Continue on another PC: your paired PCs that are here (and up to date).
            const bool handoff=s.settings.sharing&&s.settings.handoff&&s.playback.available&&std::any_of(s.nearby.begin(),s.nearby.end(),[](auto& p){return p.paired&&p.online&&p.version>=shareProtocol;});
            const bool picking=handoff&&s.handoffPicking&&!panel;
            if(picking){text(rt,L"Continue on",tx,96,74,9.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,24);float x=tx+76;
                std::vector<size_t> ready;for(size_t i=0;i<s.nearby.size()&&i<3;++i)if(s.nearby[i].paired&&s.nearby[i].online&&s.nearby[i].version>=shareProtocol)ready.push_back(i);
                const float w=std::min(118.f,(380-x-float(ready.size()-1)*6)/float(std::max<size_t>(1,ready.size())));
                for(size_t i:ready){button(Action(int(Action::HandoffPeerBase)+int(i)),s.nearby[i].name,x,96,w,24,true);x+=w+6;}}
            if(!panel){text(rt,s.playback.title,tx,45,tw,16,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist,tx,74,tw,11,muted);}if(!panel&&!picking){auto* rule=findService(s.playback.service);std::wstring app=rule?std::wstring(rule->name):s.playback.appName;if(app.empty())app=video?L"Video":L"Music";
                if(s.sessions.size()>1)app+=L"  ·  "+std::to_wstring(s.session+1)+L" of "+std::to_wstring(s.sessions.size());text(rt,app,tx,100,std::max(40.f,tw-(s.settings.waveform&&s.playback.playing&&s.waveform?104.f:0.f)),9.5f,muted);}
            iconButton(Action::Previous,Icon::Previous,tx,132,36,36,false,s.playback.canPrevious);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,tx+54,124,52,52,true,s.playback.canToggle);iconButton(Action::Next,Icon::Next,tx+124,132,36,36,false,s.playback.canNext);
            double position=std::clamp(s.playback.position+(s.playback.playing?std::max(0.,seconds()-s.playback.sampledAt):0.),0.,s.playback.duration);if(s.playback.canSeek)targets.push_back({Action::Seek,0,178,380,25});if(s.scrub.active)position=s.scrub.value;text(rt,s.playback.duration>0?clockText(position):L"No timeline provided",0,204,160,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,22);// While scrubbing, the fine-control hint takes the mode button's place.
            if(s.scrub.active)text(rt,L"Pull away for finer control",100,204,180,9.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_CENTER,22);else button(Action::MediaMode,s.settings.mediaLayout==0?L"Auto":s.settings.mediaLayout==1?L"Music":L"Video",167,204,54,22);
            // Phase 5G: the library either side of the mode (or, with nothing playing, as the way to start), and continuing elsewhere.
            if(!s.scrub.active&&s.playback.available){if(s.settings.musicLibrary)iconButton(Action::LibraryOpen,Icon::Library,129,201,30,28);if(handoff)iconButton(Action::HandoffOpen,Icon::Handoff,229,201,30,28,s.handoffPicking);}
            if(!s.playback.available&&s.settings.musicLibrary){button(Action::LibraryShuffle,L"Shuffle my music",tx,178,150,26,true);
                button(Action::LibraryOpen,L"Library",tx+158,178,96,26);}text(rt,s.playback.duration>0?clockText(s.playback.duration):L"",300,204,80,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING,22);
        }else if(s.page==Page::System&&s.statsTab==1){
            const auto& p=s.power;const int percent=s.battery;const bool charging=p.charging||(s.charging&&!p.present);
            drawRing(rt,d2d_.Get(),58,100,40,4,percent>=0?percent/100.:0,charging?0x5fd98a:accent,track);
            if(charging)drawIcon(rt,d2d_.Get(),Icon::Bolt,51,68,14,0x5fd98a);text(rt,percent>=0?std::to_wstring(percent)+L"%":L"—",18,86,80,20,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER,28);
            wchar_t rate[32]{};if(p.present&&!p.relative&&p.rateMw!=0)swprintf(rate,32,L"  ·  %.1f W",std::abs(p.rateMw)/1000.);
            text(rt,std::wstring(charging?L"Charging":s.charging?L"Plugged in":L"On battery")+rate,122,44,258,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
            text(rt,charging&&s.toFull>=0?L"Full in about "+durationText(s.toFull):!s.charging&&s.remaining>=0?L"About "+durationText(s.remaining)+L" left":s.charging&&percent>=99?L"Fully charged":s.charging&&!charging&&p.present?L"Charging paused, likely battery care":L"Time estimate appears once the rate settles",122,68,258,11,muted);
            auto stat=[&](Icon glyph,const wchar_t* name,const std::wstring& number,float x){drawIcon(rt,d2d_.Get(),glyph,x,98,12,muted);text(rt,name,x+17,96,70,9,muted);text(rt,number,x,112,84,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);};
            wchar_t full[32]{};if(p.fullMwh>0&&!p.relative)swprintf(full,32,L"%.1f Wh",p.fullMwh/1000.);double health=p.health();
            stat(Icon::Heart,L"HEALTH",health>=0?std::to_wstring(int(std::lround(health*100)))+L"%":L"—",122);
            if(s.healthNow>=0&&s.healthBefore>=0){const double change=(s.healthNow-s.healthBefore)*100;wchar_t d[32];swprintf(d,32,std::abs(change)<.05?L"steady":L"%+.1f",change);text(rt,std::wstring(d)+L" this week",160,116,84,8.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,14);}stat(Icon::Battery,L"FULL CHARGE",full[0]?full:L"—",208);stat(Icon::Reset,L"CYCLES",p.cycles?std::to_wstring(p.cycles):L"—",294);
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
            stat(Icon::Processor,L"PROCESSOR",s.system.cpu<0?L"—":value(s.system.cpu)+L"%",0,42,112);stat(Icon::Gauge,L"GPU",s.system.gpu<0?L"—":value(s.system.gpu)+L"%",124,42,104);stat(Icon::Memory,L"MEMORY / GB",s.system.ramTotalGiB?value(s.system.ramUsedGiB,1)+L" / "+value(s.system.ramTotalGiB,1):L"—",240,42,140);
            // The GPU line runs faintly under the processor line.
            if(s.system.gpu>=0){b->SetColor(D2D1::ColorF(accent2,.45f));for(unsigned i=1;i<s.system.samples;++i){unsigned base=40-s.system.samples;rt->DrawLine({float((i-1)*380./39),125-s.system.gpuHistory[base+i-1]*.25f},{float(i*380./39),125-s.system.gpuHistory[base+i]*.25f},b.Get(),1.2f);}}
            b->SetColor(D2D1::ColorF(accent));for(unsigned i=1;i<s.system.samples;++i){unsigned base=40-s.system.samples;rt->DrawLine({float((i-1)*380./39),125-s.system.cpuHistory[base+i-1]*.25f},{float(i*380./39),125-s.system.cpuHistory[base+i]*.25f},b.Get(),1.5f);}hairline(0,132,380);
            stat(Icon::Download,L"DOWN",s.system.networkAvailable?rateText(s.system.download):L"—",0,147,120);stat(Icon::Upload,L"UP",s.system.networkAvailable?rateText(s.system.upload):L"—",132,147,120);stat(Icon::Disk,L"FREE",s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—",264,147,116);{std::wstring line=s.platform.product.empty()?std::to_wstring(s.system.logicalProcessors)+L" threads  ·  up "+clockText(double(s.system.uptime)):s.platform.product+(s.powerMode>=0?L"  ·  "+std::wstring(powerModeName(s.powerMode)):L"");const bool armoury=!s.platform.armoury.empty();text(rt,line,0,208,armoury?262.f:380.f,10,muted);if(armoury)button(Action::Armoury,L"Armoury Crate",270,201,110,25);}
        }else if(s.page==Page::Control){
            // Six switches (a switch turned on plays its icon), then volume and brightness.
            const auto& c=s.controls;const bool airplane=c.wifi==0&&c.bluetooth==0;const UINT32 onAccent=s.light?0xffffff:0x111418;
            auto word=[&](int state,int bit){return (c.busy&bit)?std::wstring(L"Changing\u2026"):state==1?std::wstring(L"On"):state==0?std::wstring(L"Off"):state==-2?std::wstring(L"Not on this PC"):std::wstring(L"\u2014");};
            struct Tile{Action a;Icon on,off;const wchar_t* name;int state;std::wstring detail;};
            const Tile tiles[]={{Action::ControlWifi,Icon::Wifi,Icon::Wifi,L"Wi-Fi",c.wifi,word(c.wifi,1)},{Action::ControlBluetooth,Icon::Bluetooth,Icon::Bluetooth,L"Bluetooth",c.bluetooth,word(c.bluetooth,2)},
                {Action::ControlAirplane,Icon::Plane,Icon::Plane,L"Airplane",c.wifi==-2&&c.bluetooth==-2?-2:c.wifi<0&&c.bluetooth<0?-1:airplane?1:0,(c.busy&4)?std::wstring(L"Changing\u2026"):airplane?std::wstring(L"Radios off"):std::wstring(L"Off")},
                {Action::ControlDark,Icon::Moon,Icon::Sun,L"Dark mode",c.dark,word(c.dark,8)},
                {Action::ControlFocus,Icon::Focus,Icon::Focus,L"Focus",s.focus.running?1:0,s.focus.running?clockText(std::ceil(s.focus.displayed(seconds())))+L" left":std::wstring(L"25 minutes")},
                {Action::ControlMic,Icon::Microphone,Icon::MicOff,L"Microphone",!s.micAvailable?-2:s.micMuted?0:1,!s.micAvailable?std::wstring(L"None connected"):s.micMuted?std::wstring(L"Muted"):std::wstring(L"On")}};
            for(int i=0;i<6;++i){const auto& t=tiles[i];const float x=float(i%3)*130,y=34+float(i/3)*62;const bool on=t.state==1,enabled=t.state!=-2;targets.push_back({t.a,x,y,120,54,enabled});
                if(on){b->SetColor(D2D1::ColorF(accent));rt->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+120,y+54},14,14),b.Get());}else box(x,y,120,54,raised,14);
                const UINT32 fg=on?onAccent:enabled?ink:muted;icon(t.a,on?t.on:t.off,x+12,y+17,20,fg,-1,on&&!controlOn_[size_t(i)]&&s.expanded);controlOn_[size_t(i)]=on;
                text(rt,t.name,x+42,y+9,72,11,fg,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,t.detail,x+42,y+28,72,9.5f,on?fg:muted);}
            // A track with its fill and knob, and the value after it.
            auto slider=[&](Action a,float y,int value,bool enabled){const float v=float(std::clamp(value,0,100))/100.f;ComPtr<ID2D1StrokeStyle> round;auto props=D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND);d2d_->CreateStrokeStyle(props,nullptr,0,&round);
                b->SetColor(D2D1::ColorF(track));rt->DrawLine({38,y+14},{300,y+14},b.Get(),4,round.Get());if(enabled&&v>0){b->SetColor(D2D1::ColorF(accent));rt->DrawLine({38,y+14},{38+262*v,y+14},b.Get(),4,round.Get());}
                if(enabled){b->SetColor(D2D1::ColorF(ink));rt->FillEllipse(D2D1::Ellipse({38+262*v,y+14},6.5f,6.5f),b.Get());}
                text(rt,enabled?std::to_wstring(value)+L"%":std::wstring(L"\u2014"),310,y+4,70,10.5f,muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,20);targets.push_back({a,34,y,270,28,enabled});};
            iconButton(Action::Mute,s.muted?Icon::Muted:Icon::Volume,-2,160,32,28);slider(Action::VolumeSlider,160,s.muted?0:s.volume,true);
            drawIcon(rt,d2d_.Get(),Icon::Brightness,5,198,18,s.brightness>=0?ink:muted);slider(Action::ControlBrightness,192,std::max(0,s.brightness),s.brightness>=0);
        }else if(s.page==Page::Focus){
            button(Action::Timer25,L"Focus",0,36,120,28,s.focus.mode==FocusClock::Mode::Focus);button(Action::Timer5,L"Break",130,36,120,28,s.focus.mode==FocusClock::Mode::Break);button(Action::Stopwatch,L"Stopwatch",260,36,120,28,s.focus.mode==FocusClock::Mode::Stopwatch);
            double progress=s.focus.mode==FocusClock::Mode::Stopwatch?std::fmod(s.focus.elapsed(seconds()),60.)/60.:normalizedProgress(s.focus.displayed(seconds()),s.focus.duration);drawRing(rt,d2d_.Get(),63,130,38,3,progress,accent,line);drawIcon(rt,d2d_.Get(),Icon::Focus,50,117,26,accent);
            contentSpots_.push_back({2,clockText(std::ceil(s.focus.displayed(seconds()))),124,91,42,DWRITE_FONT_WEIGHT_LIGHT,ink});text(rt,s.focus.finished?L"A moment well spent":s.focus.running?L"One thing at a time":L"A little space to begin",127,144,245,11,muted);iconButton(Action::TimerToggle,s.focus.running?Icon::Pause:Icon::Play,130,180,42,42,true);iconButton(Action::TimerReset,Icon::Reset,190,183,36,36);text(rt,s.focus.running?L"Pause session":L"Start session",238,193,142,10,muted);
        }else if(s.page==Page::Shelf){
            const bool detail=s.shelfTab==0&&s.shelfDetail>=0&&size_t(s.shelfDetail)<s.shelf.size();
            // Phase 5G: files dragged over the island choose where they go: the Shelf, or one of your paired PCs (in place of the tabs and rows).
            std::vector<size_t> dropPeers;if(s.dropHover&&s.settings.sharing)for(size_t i=0;i<s.nearby.size()&&i<4;++i)if(s.nearby[i].paired)dropPeers.push_back(i);
            if(!detail&&dropPeers.empty()){if(s.settings.sharing)tabs({{Action::ShelfFiles,L"Files"},{Action::ShelfClipboard,L"Clipboard"},{Action::ShelfNearby,L"Nearby"}},s.shelfTab,30);else tabs({{Action::ShelfFiles,L"Files"},{Action::ShelfClipboard,L"Clipboard"}},s.shelfTab,30);}
            const bool status=!s.clipStatus.empty()&&seconds()<s.clipStatusUntil,shelfNote=!s.shelfStatus.empty()&&(s.shelfBusy||seconds()<s.shelfStatusUntil);
            // An icon and a label in one raised button.
            auto action=[&](Action a,Icon glyph,const wchar_t* name,float x,float y,float w,bool enabled=true){targets.push_back({a,x,y,w,30,enabled});box(x,y,w,30,raised,9);drawIcon(rt,d2d_.Get(),glyph,x+10,y+8,14,enabled?ink:muted,enabled?1.f:.5f);text(rt,name,x+29,y,w-33,10.5f,enabled?ink:muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,30);};
            if(!dropPeers.empty()){
                auto zone=[&](Action a,float x,float y,float w,float h,bool enabled){const bool on=enabled&&s.dropZone==a;targets.push_back({a,x,y,w,h,enabled});box(x,y,w,h,raised,16);
                    if(on){b->SetColor(D2D1::ColorF(accent,.16f));rt->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},16,16),b.Get());b->SetColor(D2D1::ColorF(accent,.95f));rt->DrawRoundedRectangle(D2D1::RoundedRect({x+1,y+1,x+w-1,y+h-1},15,15),b.Get(),2);}
                    return on;};
                const bool shelfOn=zone(Action::DropShelf,0,32,150,194,true);drawIcon(rt,d2d_.Get(),Icon::Shelf,57,82,36,shelfOn?accent:ink);
                label(L"Keep on Shelf",0,130,150,22,13,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);label(shelfOn?L"Release to keep":L"Drop here",0,154,150,18,10,shelfOn?accent:muted,DWRITE_FONT_WEIGHT_NORMAL);
                const float h=(194-float(dropPeers.size()-1)*8)/float(dropPeers.size());
                for(size_t k=0;k<dropPeers.size();++k){const auto& p=s.nearby[dropPeers[k]];const float y=32+float(k)*(h+8),cy=y+h/2;const bool ready=p.online&&p.version>=shareProtocol;
                    const bool on=zone(Action(int(Action::NearbyBase)+int(dropPeers[k])),160,y,220,h,ready);
                    b->SetColor(D2D1::ColorF(on?accent:ink,on?.24f:.08f));rt->FillEllipse(D2D1::Ellipse({190,cy},16,16),b.Get());drawIcon(rt,d2d_.Get(),Icon::Laptop,180,cy-10,20,on?accent:ready?ink:muted);
                    text(rt,p.name,216,cy-17,156,12,ready?ink:muted,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,18);
                    text(rt,!p.online?L"Away":!ready?L"Needs the latest Arnav Island":on?L"Release to send":L"Drop to send",216,cy+1,156,10,on?accent:muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,16);}
            }else if(detail){
                const auto& item=s.shelf[size_t(s.shelfDetail)];const auto& info=s.shelfInfo;const bool file=item.kind==ShelfItem::Kind::File;
                // With sharing on, a file can go to your paired PC from here.
                const bool sendable=s.settings.sharing&&file,reachable=std::any_of(s.nearby.begin(),s.nearby.end(),[&](auto& p){return p.paired&&p.online&&p.id==s.nearbyTarget;});
                iconButton(Action::ShelfBack,Icon::ArrowLeft,-4,26,32,30);text(rt,item.label,32,26,sendable?306.f:348.f,13,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,30);
                if(sendable)iconButton(Action::ShareSend,Icon::Send,346,26,34,30,false,reachable);
                box(0,66,72,72,raised,14);if(item.preview)drawPreview(rt,*item.preview,6,72,60,60);else drawIcon(rt,d2d_.Get(),!file?Icon::Text:info.directory?Icon::Folder:Icon::File,22,88,28,muted);
                std::wstring first=!file?std::to_wstring(item.value.size())+L" characters of text":info.kind+(info.size.empty()?L"":L"  \u00b7  "+info.size)+(info.width>0?L"  \u00b7  "+std::to_wstring(info.width)+L" \u00d7 "+std::to_wstring(info.height):L"");
                text(rt,first,84,68,296,11,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,20);text(rt,file?info.folder:clipPreview(item.value,120),84,90,296,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,20);
                if(shelfNote)text(rt,s.shelfStatus,84,112,296,10.5f,s.shelfBusy?muted:accent,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,22);
                const float w=89.5f,g=(380-4*w)/3;auto col=[&](int i){return float(i)*(w+g);};
                if(file){action(Action::ShelfOpen,Icon::External,L"Open",col(0),152,w);action(Action::ShelfOpenWith,Icon::Apps,L"Open with",col(1),152,w,!info.directory);action(Action::ShelfReveal,Icon::Folder,L"In folder",col(2),152,w);action(Action::ShelfCopyPath,Icon::Copy,L"Copy path",col(3),152,w);
                    if(info.image){action(Action::ShelfCopyText,Icon::Text,L"Copy text",col(0),188,w,!s.shelfBusy);action(Action::ShelfConvert,Icon::Convert,info.extension==L".png"?L"To JPG":L"To PNG",col(1),188,w,!s.shelfBusy);action(Action::ShelfHalf,Icon::Resize,L"Half size",col(2),188,w,!s.shelfBusy);}
                    else action(Action::ShelfZip,Icon::Archive,L"Zip",col(0),188,w,!s.shelfBusy);
                    action(Action::ShelfRemove,Icon::Trash,L"Remove",col(3),188,w);}
                else{action(Action::ShelfCopyPath,Icon::Copy,L"Copy",col(0),152,w);action(Action::ShelfRemove,Icon::Trash,L"Remove",col(3),152,w);}
            }else if(s.shelfTab==0){
                if(s.shelf.empty()){box(0,62,380,130,raised,18);icon(Action::None,Icon::Shelf,172,76,36,accent);label(s.dropHover?L"Release to keep it close":L"A place to keep things close",20,118,340,28,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);label(L"Drop files or text, or snip the screen below",20,148,340,22,11,muted);}else for(size_t i=s.shelfOffset;i<std::min(size_t(s.shelfOffset+4),s.shelf.size());++i){float y=62+float(i-s.shelfOffset)*36;auto& item=s.shelf[i];Action a=Action(int(Action::ShelfItemBase)+int(i));targets.push_back({a,0,y,380,32});box(0,y,380,32,raised,9);if(item.preview)drawPreview(rt,*item.preview,6,y+3,32,26);else icon(a,item.kind==ShelfItem::Kind::File?Icon::File:Icon::Text,14,y+8,16,muted);text(rt,item.label,46,y,300,11,ink,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,32);drawIcon(rt,d2d_.Get(),Icon::Chevron,354,y+9,14,muted,s.hovered==a?1.f:.55f);}
                // Capture straight onto the Shelf, then the item count and the whole-Shelf actions.
                iconButton(Action::CaptureSnip,Icon::Snip,-2,199,34,28);iconButton(Action::CaptureText,Icon::Text,34,199,34,28);iconButton(Action::CaptureColour,Icon::Eyedropper,70,199,34,28);
                const bool files=std::any_of(s.shelf.begin(),s.shelf.end(),[](auto& i){return i.kind==ShelfItem::Kind::File;});
                if(shelfNote)text(rt,s.shelfStatus,112,201,120,10,!s.shelfBusy?accent:muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,25);
                else{// Phase 5G: the stack. Drag it to send every file at once (onto a paired PC) or to drop them anywhere.
                    // Up to three cards, fanned about a common foot like a held stack.
                    std::vector<size_t> cards;for(size_t k=0;k<s.shelf.size()&&cards.size()<3;++k)if(s.shelf[k].kind==ShelfItem::Kind::File)cards.push_back(k);
                    const int fanned=int(cards.size());D2D1_MATRIX_3X2_F saved;rt->GetTransform(&saved);
                    for(int c=fanned-1;c>=0;--c){const float angle=fanned==1?0.f:(float(c)-float(fanned-1)/2)*11.f,x=113+float(c)*3,y=202;
                        rt->SetTransform(D2D1::Matrix3x2F::Rotation(angle,{x+10,y+26})*saved);box(x,y,20,20,solidRaised,5);
                        if(s.shelf[cards[size_t(c)]].preview)drawPreview(rt,*s.shelf[cards[size_t(c)]].preview,x+1.5f,y+1.5f,17,17);else drawIcon(rt,d2d_.Get(),Icon::File,x+4,y+4,12,muted);
                        b->SetColor(D2D1::ColorF(ink,.35f));rt->DrawRoundedRectangle(D2D1::RoundedRect({x+.5f,y+.5f,x+19.5f,y+19.5f},5,5),b.Get(),1);}
                    rt->SetTransform(saved);
                    const float tx0=fanned?146.f:112.f;text(rt,std::to_wstring(s.shelf.size())+(s.shelf.size()==1?L" item":L" items"),tx0,201,232-tx0,10,s.hovered==Action::ShelfStack?ink:muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,25);
                    if(fanned)targets.push_back({Action::ShelfStack,108,197,126,30});}
                button(Action::ShelfZip,L"Zip",240,201,68,25,false,files&&!s.shelfBusy);button(Action::ShelfClear,L"Clear",316,201,64,25,false,!s.shelf.empty());
            }else if(s.shelfTab==2){
                // Nearby: your PCs on this network with sharing on. Clicking a paired one makes it where Send goes; an unpaired one offers Pair.
                const size_t shown=std::min<size_t>(s.nearby.size(),4);
                if(!shown){box(0,62,380,130,raised,18);drawIcon(rt,d2d_.Get(),Icon::Laptop,172,74,36,accent);label(L"Looking for your PCs",20,114,340,24,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
                    label(L"Turn on Share with my PCs on another PC on this network",20,140,340,20,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);}
                const bool shelfFiles=std::any_of(s.shelf.begin(),s.shelf.end(),[](auto& i){return i.kind==ShelfItem::Kind::File;});
                for(size_t i=0;i<shown;++i){const auto& p=s.nearby[i];const float y=62+float(i)*36;const bool target=p.paired&&p.id==s.nearbyTarget,ready=p.online&&p.version>=shareProtocol;
                    const Action row=Action(int(Action::NearbyBase)+int(i)),forget=Action(int(Action::NearbyForgetBase)+int(i)),send=Action(int(Action::NearbySendBase)+int(i)),stop=Action(int(Action::NearbyCancelBase)+int(i));
                    const auto moving=std::find_if(s.transfers.begin(),s.transfers.end(),[&](auto& t){return t.peer==p.id;});
                    box(0,y,380,32,raised,9);if(target){b->SetColor(D2D1::ColorF(accent,.9f));rt->DrawRoundedRectangle(D2D1::RoundedRect({.75f,y+.75f,379.25f,y+31.25f},8.5f,8.5f),b.Get(),1.5f);}
                    drawIcon(rt,d2d_.Get(),Icon::Laptop,11,y+8,16,p.online?ink:muted);text(rt,p.name,36,y+1,182,11.5f,p.online?ink:muted,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,17);
                    std::wstring status=p.paired?(p.online?(!ready?L"Needs the latest Arnav Island":target?L"Sends go here":L"Paired"):L"Paired \u00b7 away"):L"Not paired";
                    if(moving!=s.transfers.end()){const double f=moving->total?double(moving->done)/double(moving->total):0;wchar_t pc[16];swprintf(pc,16,L"  \u00b7  %d%%",int(f*100));status=(moving->outgoing?L"Sending ":L"Receiving ")+moving->title+pc;
                        b->SetColor(D2D1::ColorF(track));rt->FillRoundedRectangle(D2D1::RoundedRect({36,y+28,306,y+30},1,1),b.Get());b->SetColor(D2D1::ColorF(accent));rt->FillRoundedRectangle(D2D1::RoundedRect({36,y+28,36+float(270*std::clamp(f,0.,1.)),y+30},1,1),b.Get());}
                    text(rt,status,36,moving!=s.transfers.end()?y+13:y+15,moving!=s.transfers.end()?270.f:182.f,9.5f,target||moving!=s.transfers.end()?accent:muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,12);
                    if(moving!=s.transfers.end()){targets.push_back({row,0,y,224,32});button(stop,L"Stop",316,y+4,58,24);}
                    else if(p.paired){targets.push_back({row,0,y,224,32});if(shelfFiles&&ready)button(send,L"Send Shelf",228,y+4,82,24,true);button(forget,L"Forget",316,y+4,58,24);}
                    else button(row,L"Pair",316,y+4,58,24,true,p.online&&ready);}
                text(rt,shelfNote?s.shelfStatus:(s.shareName.empty()?std::wstring(L"Visible to your PCs on this network"):L"This PC: "+s.shareName),0,201,380,10,shelfNote?accent:muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,25);
            }else if(!s.settings.clipboardHistory){
                box(0,62,380,130,raised,18);drawIcon(rt,d2d_.Get(),Icon::Clipboard,176,74,28,accent);label(L"Keep what you copy close",20,106,340,24,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);
                label(L"Your last 24 copies, in memory only. Private copies are skipped.",20,130,340,20,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);button(Action::ClipboardEnable,L"Turn on",150,156,80,26,true);
                text(rt,L"Off  \u00b7  Nothing is read until you turn it on",0,208,380,10,muted);
            }else{
                if(s.clips.empty()){box(0,62,380,130,raised,18);drawIcon(rt,d2d_.Get(),Icon::Clipboard,176,80,28,muted);label(s.clipsPaused?L"Paused":L"Copy something to keep it here",20,116,340,24,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);label(L"Click a copy to put it back on the clipboard",20,142,340,20,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL);}
                for(int i=s.clipOffset;i<std::min(s.clipOffset+4,int(s.clips.size()));++i){float y=62+float(i-s.clipOffset)*36;auto& c=s.clips[size_t(i)];const int row=i-s.clipOffset;Action a=Action(int(Action::ClipBase)+row),pin=Action(int(Action::ClipPinBase)+row);
                    targets.push_back({pin,346,y+2,32,28});targets.push_back({a,0,y,346,32});box(0,y,380,32,raised,9);
                    // Password-like copies stay masked until the pointer is on their row.
                    const bool masked=c.secret&&s.settings.hideSecrets&&s.hovered!=a&&s.hovered!=pin;
                    // Rich rows: a swatch for a colour, the site's icon (or its initial) for a link, a code mark for code.
                    if(c.thumbnail)drawPreview(rt,*c.thumbnail,6,y+3,32,26);
                    else if(!masked&&c.hasColour){ComPtr<ID2D1SolidColorBrush> sw;rt->CreateSolidColorBrush(D2D1::ColorF(c.colour),&sw);rt->FillEllipse(D2D1::Ellipse({22,y+16},11,11),sw.Get());sw->SetColor(D2D1::ColorF(ink,.25f));rt->DrawEllipse(D2D1::Ellipse({22,y+16},11,11),sw.Get(),1/scale_);}
                    else if(!masked&&c.favicon){box(10,y+4,24,24,raised,7);drawPreview(rt,*c.favicon,14,y+8,16,16);}
                    else if(!masked&&!c.host.empty()){const UINT32 hues[]={0x5b8def,0xe0679b,0x3fb68b,0xe6a23c,0x9b7bea,0x4fb3c9};const UINT32 tile=hues[std::hash<std::wstring>{}(c.host)%6];b->SetColor(D2D1::ColorF(tile,.9f));rt->FillRoundedRectangle(D2D1::RoundedRect({10,y+4,34,y+28},7,7),b.Get());
                        label(std::wstring(1,wchar_t(std::towupper(c.host[0]))),10,y+4,24,24,12,0xffffff,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
                    else if(!masked&&c.code){box(8,y+4,28,24,raised,7);label(L"</>",8,y+4,28,24,9,muted,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
                    else drawIcon(rt,d2d_.Get(),masked?Icon::Lock:c.kind==int(ClipEntry::Kind::Link)?Icon::Link:c.kind==int(ClipEntry::Kind::Files)?Icon::File:Icon::Text,14,y+8,16,muted);
                    if(masked)text(rt,std::wstring(L"\u2022\u2022\u2022\u2022\u2022\u2022\u2022\u2022"),46,y+2,294,11,ink);
                    else if(!c.host.empty()){const float hw=std::min(160.f,measure(c.host,11,DWRITE_FONT_WEIGHT_SEMI_BOLD)+2);text(rt,c.host,46,y+2,hw,11,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);if(!c.path.empty()&&294-hw>24)text(rt,c.path,46+hw+2,y+2,294-hw-2,11,muted);}
                    else if(c.code)codeText(rt,c.preview,46,y+2,294,10.5f,ink,muted,s.light);
                    else text(rt,c.preview,46,y+2,294,11,ink);float mx=46;if(c.icon){drawPreview(rt,*c.icon,46,y+18,11,11);mx=61;}text(rt,masked?L"Hidden  \u00b7  "+c.meta:c.meta,mx,y+17,340-mx,9,muted);
                    if(c.pinned||s.hovered==a||s.hovered==pin)icon(pin,Icon::Pin,354,y+8,16,c.pinned?accent:muted,-1);}
                text(rt,status?s.clipStatus:s.clipsPaused?std::wstring(L"Paused  \u00b7  not keeping copies"):std::to_wstring(s.clips.size())+(s.clips.size()==1?L" copy":L" copies"),0,201,190,10,status?ink:muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,25);
                iconButton(Action::ClipSearch,Icon::Search,202,199,34,28);
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
                // The arrow sits inside the button, after its label.
                targets.push_back({Action::MixerSettings,0,203,214,25});box(0,203,214,25,raised,9);text(rt,L"Windows volume mixer",14,203,170,11.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,25);drawIcon(rt,d2d_.Get(),Icon::ArrowRight,190,209,13,muted);
            }else{
            text(rt,s.feedback.empty()?L"Choose where your sound goes":s.feedback,176,37,204,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);for(size_t i=s.audioOffset;i<std::min(size_t(s.audioOffset+4),s.outputs.size());++i){float y=62+float(i-s.audioOffset)*34;auto& output=s.outputs[i];Action a=Action(int(Action::DeviceBase)+int(i));targets.push_back({a,0,y,380,30});box(0,y,380,30,raised,9);icon(a,Icon::Audio,10,y+7,16,output.current?accent:muted);text(rt,output.name,37,y+7,302,11,ink);if(output.current)drawIcon(rt,d2d_.Get(),Icon::Check,353,y+7,16,accent);}
            button(Action::SoundSettings,L"Windows sound settings",0,203,201,25);icon(Action::SoundSettings,Icon::ArrowRight,208,208,14,muted);text(rt,s.settings.directAudio?L"Direct switching":L"System picker",236,210,144,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
        }
        hairline(0,229,380);
        const wchar_t* labels[]={L"Home",L"Media",L"Stats",L"Focus",L"Settings",L"Shelf",L"Audio",L"Controls"};const Action actions[]={Action::Overview,Action::Media,Action::System,Action::Focus,Action::Settings,Action::Shelf,Action::Audio,Action::Control};const Icon glyphs[]={Icon::Home,Icon::Music,Icon::Stats,Icon::Focus,Icon::Settings,Icon::Shelf,Icon::Audio,Icon::Sliders};
        for(int slot=0;slot<pageCount;++slot){int p=s.settings.navigation[size_t(slot)];float x=std::round((slot*navStep+4)*scale_)/scale_;bool selected=int(s.page)==p;targets.push_back({actions[p],x,navY,47,44});
            // A page just chosen plays its icon's little animation once.
            const bool celebrate=selected&&celebratedPage_!=p&&s.expanded&&!s.live;if(celebrate)celebratedPage_=p;icon(actions[p],glyphs[p],x+14,navY+5,19,selected?ink:muted,p,celebrate);label(labels[p],x,navY+26,47,16,9.5f,selected?ink:muted);if(selected){const double now=seconds();if(s.reducedMotion||!s.expanded){navX.reset(x,now);navLeft_.reset(x,now);navRight_.reset(x+47,now);}
                else if(navX.target()!=x){const bool right=x>navX.target();navX.retarget(x,now,MotionTokens::navigation);
                    // The end it moves toward leads; the other follows a beat behind.
                    constexpr SpringSpec lead{.7,560,34},trail{.9,300,30};navLeft_.retarget(x,now,right?trail:lead);navRight_.retarget(x+47,now,right?lead:trail);}}}
        if(debug){b->SetColor(D2D1::ColorF(0xf98585));for(auto& target:targets)rt->DrawRectangle({target.x,target.y,target.x+target.width,target.y+target.height},b.Get());}
    });drawingContent_=false;
    // (Its surfaces are drawn once the content surface is closed: one surface draws at a time.)
    if(skyWanted_)skyScene(s,skyX_);else skyHide();
    updateCaret(s,caretTarget_,accent);lineLyric(liveLyric_,content_.Get(),s,liveLyricOn&&s.live&&!s.card,68,21,250,20,11.5f,ink,s.reducedMotion);
    if(answerY_>=0&&!answerText_.empty()&&s.command.active)contentSpots_.push_back({0,answerText_,46,answerY_,17,DWRITE_FONT_WEIGHT_SEMI_BOLD,ink,0,false,true});
    placeOdometers(contentSpots_,{0,2,3,4,5},content_.Get(),s.reducedMotion);for(auto request:iconRequests_)icon(request.action,request.glyph,request.x,request.y,request.size,request.color,request.slot,request.celebrate);for(auto& b:bands_)b.visual->SetContent(contentSurface_.Get());for(auto& item:icons_)if(!item.used)item.effect->SetOpacity(0.f);placeNav(seconds(),navY);commit();
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
    const float restX=std::round(float((edge_==1?canvasWidth-m.width.target():edge_==2?0.:(canvasWidth-m.width.target())/2)*scale_)),restY=edge_?std::round(float((canvasHeight-m.height.target())*scale_/2)):0.f;
    auto w=animation(m.width,now,scale_),h=animation(m.height,now,scale_),r=animation(m.radius,now,scale_);
    auto iw=animation(m.width,now,scale_,-2),ih=animation(m.height,now,scale_,-2),ir=animation(m.radius,now,scale_,-1);
    if(rest){check(clip_->SetRight(restW));check(clip_->SetBottom(restH));check(innerClip_->SetRight(restW-2));check(innerClip_->SetBottom(restH-2));}
    else{check(clip_->SetRight(w.Get()));check(clip_->SetBottom(h.Get()));check(innerClip_->SetRight(iw.Get()));check(innerClip_->SetBottom(ih.Get()));}
    // The drop pill (top dock, attached): the body moves down as a floating pill. Its outline starts above
    // the screen edge (square to it, as when docked) and comes down twice as fast as the body, so its
    // top corners round out as it leaves; the stub and the shoulders stay docked at the compact size.
    const bool dropping=edge_==0&&attached_&&(m.drop.target()>0||std::abs(m.drop.sample(now).position)>1e-4);
    auto settledAll=[&](double t){return m.drop.settled(t)&&m.width.settled(t)&&m.height.settled(t)&&m.radius.settled(t);};
    auto cornerAt=[&](double t){return std::min({m.radius.sample(t).position,m.width.sample(t).position/2,m.height.sample(t).position/2});};
    if(dropping){auto top=curveOf([&](double t){return std::min(m.drop.sample(t).position*dropDistance-cornerAt(t)-2,0.)*scale_;},now,settledAll);check(clip_->SetTop(top.Get()));check(innerClip_->SetTop(top.Get()));}
    else{check(clip_->SetTop(0.f));check(innerClip_->SetTop(0.f));}
    // The corners on the screen edge are square when attached (the shoulders take over there).
    bool tl=!attached_||edge_==1||dropping,tr=!attached_||edge_==2||dropping,bl=!attached_||edge_!=2,br=!attached_||edge_!=1;

    auto corners=[&](IDCompositionRectangleClip* clip,IDCompositionAnimation* radius){
        if(tl){clip->SetTopLeftRadiusX(radius);clip->SetTopLeftRadiusY(radius);}else{clip->SetTopLeftRadiusX(0.f);clip->SetTopLeftRadiusY(0.f);}
        if(tr){clip->SetTopRightRadiusX(radius);clip->SetTopRightRadiusY(radius);}else{clip->SetTopRightRadiusX(0.f);clip->SetTopRightRadiusY(0.f);}
        if(bl){clip->SetBottomLeftRadiusX(radius);clip->SetBottomLeftRadiusY(radius);}else{clip->SetBottomLeftRadiusX(0.f);clip->SetBottomLeftRadiusY(0.f);}
        if(br){clip->SetBottomRightRadiusX(radius);clip->SetBottomRightRadiusY(radius);}else{clip->SetBottomRightRadiusX(0.f);clip->SetBottomRightRadiusY(0.f);}
    };corners(clip_.Get(),r.Get());corners(innerClip_.Get(),ir.Get());
    auto x=animation(m.width,now,edge_==1?-scale_:-scale_/2,canvasWidth*scale_/(edge_==1?1:2));if(m.width.settled(now)||edge_==2)check(body_->SetOffsetX(restX));else check(body_->SetOffsetX(x.Get()));
    if(edge_){auto y=animation(m.height,now,-scale_/2,canvasHeight*scale_/2);if(m.height.settled(now))check(body_->SetOffsetY(restY));else check(body_->SetOffsetY(y.Get()));}else if(dropping){auto y=animation(m.drop,now,float(dropDistance)*scale_);check(body_->SetOffsetY(y.Get()));}else check(body_->SetOffsetY(0.f));
    auto dx=animation(m.dragX,now,scale_),dy=animation(m.dragY,now,scale_);check(root_->SetOffsetX(dx.Get()));check(root_->SetOffsetY(dy.Get()));
    // Pulled against its edge, the island leans the way it is dragged and stretches when pulled down (top dock).
    const bool lean=edge_==0&&!m.reduced&&(material_==0||glass_.leans())&&!(m.dragX.settled(now)&&m.dragY.settled(now)&&std::abs(m.dragX.target())<.01&&std::abs(m.dragY.target())<.01);
    if(lean){auto angle=animation(m.dragX,now,float(leanDegreesPerDip)),sy=animation(m.dragY,now,float(stretchPerDip),1.f),sx=animation(m.dragY,now,-float(narrowPerDip),1.f);leanSkew_->SetAngleX(angle.Get());leanScale_->SetScaleX(sx.Get());leanScale_->SetScaleY(sy.Get());}
    else{leanSkew_->SetAngleX(0.f);leanScale_->SetScaleX(1.f);leanScale_->SetScaleY(1.f);}
    if(lean!=leaning_){leaning_=lean;check(root_->SetTransform(lean?leanGroup_.Get():static_cast<IDCompositionTransform*>(nullptr)));}
    // Shoulders: redrawn whenever the target radius changes; scaled only while it animates.
    if(std::abs(float(m.radius.target())-wingRadius_)>1e-3f)wings(float(m.radius.target()));
    const float along=float(wingAlong_),depth=float(wingDepth_);
    auto shoulder=animation(m.radius,now,1.f/wingRadius_);for(auto p:{leftScale_.Get(),rightScale_.Get()}){if(m.radius.settled(now)){check(p->SetScaleX(1.f));check(p->SetScaleY(1.f));}else{check(p->SetScaleX(shoulder.Get()));check(p->SetScaleY(shoulder.Get()));}}
    if(edge_){
        // Anchored at the screen edge; the inner end of each wing meets the body's top or bottom.
        const float anchorX=edge_==2?0.f:depth,wingX=edge_==2?0.f:canvasWidth*scale_-depth;
        leftScale_->SetCenterX(anchorX);leftScale_->SetCenterY(along);rightScale_->SetCenterX(anchorX);rightScale_->SetCenterY(1.f);
        wingLeft_->SetOffsetX(wingX);wingRight_->SetOffsetX(wingX);
        if(m.height.settled(now)){wingLeft_->SetOffsetY(restY-along);wingRight_->SetOffsetY(restY+restH-1);}
        else{auto ly=animation(m.height,now,-scale_/2,canvasHeight*scale_/2-along),ry=animation(m.height,now,scale_/2,canvasHeight*scale_/2-1);wingLeft_->SetOffsetY(ly.Get());wingRight_->SetOffsetY(ry.Get());}
        header_->SetOffsetX(expanded_?std::round(20*scale_):0.f);stubEffect_->SetOpacity(0.f);budEffect_->SetOpacity(0.f);}
    else{leftScale_->SetCenterX(along);leftScale_->SetCenterY(0.f);rightScale_->SetCenterX(1.f);rightScale_->SetCenterY(0.f);wingLeft_->SetOffsetY(0.f);wingRight_->SetOffsetY(0.f);
        if(m.width.settled(now)){wingLeft_->SetOffsetX(restX-along);wingRight_->SetOffsetX(restX+restW-1);}
        else{auto lx=animation(m.width,now,-scale_/2,canvasWidth*scale_/2-along),rx=animation(m.width,now,scale_/2,canvasWidth*scale_/2-1);wingLeft_->SetOffsetX(lx.Get());wingRight_->SetOffsetX(rx.Get());}
        if(dropping){
            // Once the pill has left the edge, the shoulders slide in from its sides to the stub's over 12 DIPs (and back out on
            // the return before it touches the edge again), taking the stub's radius.
            auto blend=[&](double t){return std::clamp((m.drop.sample(t).position*dropDistance-(cornerAt(t)+2)/2)/12,0.,1.);};auto span=[&](double t){const double w=m.width.sample(t).position;return w+(m.stubWidth-w)*blend(t);};
            auto lx=curveOf([&](double t){return (canvasWidth-span(t))/2*scale_-along;},now,settledAll),rx=curveOf([&](double t){return (canvasWidth+span(t))/2*scale_-1;},now,settledAll);wingLeft_->SetOffsetX(lx.Get());wingRight_->SetOffsetX(rx.Get());
            auto k=curveOf([&](double t){const double r=m.radius.sample(t).position;return (r+(m.stubRadius-r)*blend(t))/wingRadius_;},now,settledAll);for(auto p:{leftScale_.Get(),rightScale_.Get()}){check(p->SetScaleX(k.Get()));check(p->SetScaleY(k.Get()));}
            // The stub: the compact island (at most the standard width), from the edge down to 1 DIP into the pill's top.
            // It spans what the shoulders span, so they always meet it.
            stub_->SetOffsetX(0.f);stub_->SetOffsetY(0.f);stubClip_->SetTop(0.f);auto sl=curveOf([&](double t){return (canvasWidth-span(t))/2*scale_;},now,settledAll),sr2=curveOf([&](double t){return (canvasWidth+span(t))/2*scale_;},now,settledAll);stubClip_->SetLeft(sl.Get());stubClip_->SetRight(sr2.Get());
            auto bottom=curveOf([&](double t){const double dp=m.drop.sample(t).position*dropDistance;return std::clamp(std::max(std::min(2*dp-cornerAt(t)-2,dp),0.)+1,0.,m.stubHeight)*scale_;},now,settledAll);stubClip_->SetBottom(bottom.Get());
            const float sr=float(m.stubRadius*scale_);stubClip_->SetBottomLeftRadiusX(sr);stubClip_->SetBottomLeftRadiusY(sr);stubClip_->SetBottomRightRadiusX(sr);stubClip_->SetBottomRightRadiusY(sr);stubEffect_->SetOpacity(1.f);}
        else stubEffect_->SetOpacity(0.f);
        // Phase 5G: a waiting alert's bud grows from the pill's foot, then lets go and settles just below it (budShape).
        if(dropping&&(m.bud.target()>0||std::abs(m.bud.sample(now).position)>1e-3)){auto settledBud=[&](double t){return settledAll(t)&&m.bud.settled(t);};
            auto shape=[&](double t){return budShape(m.bud.sample(t).position,m.drop.sample(t).position*dropDistance+m.height.sample(t).position,canvasWidth/2,m.budWidth,m.budHeight);};
            auto left=curveOf([&](double t){return shape(t).left*scale_;},now,settledBud),top=curveOf([&](double t){return shape(t).top*scale_;},now,settledBud),right=curveOf([&](double t){return shape(t).right*scale_;},now,settledBud),bottom=curveOf([&](double t){return shape(t).bottom*scale_;},now,settledBud),radius=curveOf([&](double t){return shape(t).radius*scale_;},now,settledBud);
            budClip_->SetLeft(left.Get());budClip_->SetTop(top.Get());budClip_->SetRight(right.Get());budClip_->SetBottom(bottom.Get());
            {auto* c=budClip_.Get();auto* a=radius.Get();c->SetTopLeftRadiusX(a);c->SetTopLeftRadiusY(a);c->SetTopRightRadiusX(a);c->SetTopRightRadiusY(a);c->SetBottomLeftRadiusX(a);c->SetBottomLeftRadiusY(a);c->SetBottomRightRadiusX(a);c->SetBottomRightRadiusY(a);}
            // The label sits at the bud's foot and shows once the bud has let go.
            budLabel_->SetOffsetX(std::round(float((canvasWidth-m.budWidth)/2*scale_)));auto labelY=curveOf([&](double t){return (shape(t).bottom-m.budHeight)*scale_;},now,settledBud);budLabel_->SetOffsetY(labelY.Get());
            auto shown=curveOf([&](double t){return std::clamp((m.bud.sample(t).position-.72)/.24,0.,1.);},now,settledBud);budLabelEffect_->SetOpacity(shown.Get());budEffect_->SetOpacity(1.f);}
        else budEffect_->SetOpacity(0.f);
        auto hx=animation(m.width,now,expanded_?0.f:scale_/2,expanded_?20*scale_:float(-m.compactWidth/2)*scale_);if(m.width.settled(now))header_->SetOffsetX(std::round(float(expanded_?20*scale_:(m.width.target()-m.compactWidth)*scale_/2)));else header_->SetOffsetX(hx.Get());}
    auto opacity=visibility(m,now);auto headerOpacity=visibility(m,now,!expanded_);
    restExpanded_=expanded_&&m.height.settled(now)&&m.width.settled(now)&&m.reveal.settled(now);restCompact_=!expanded_&&m.height.settled(now)&&m.width.settled(now);
    const bool contentEntering=restExpanded_&&now<contentEntrance_+.23,headerEntering=restCompact_&&now<headerEntrance_+.23;
    if(headerEntering){auto a=entrance(headerEntrance_,.35f);headerEffect_->SetOpacity(a.Get());}else headerEffect_->SetOpacity(headerOpacity.Get());
    if(ringEffect_){if(ringOn_)ringEffect_->SetOpacity(headerOpacity.Get());else ringEffect_->SetOpacity(0.f);}
    (void)contentEntering;check(contentEffect_->SetOpacity(opacity.Get()));if(privacyVisible_)privacyEffect_->SetOpacity(opacity.Get());else privacyEffect_->SetOpacity(0.f);check(iconEffect_->SetOpacity(opacity.Get()));if(m.live)navEffect_->SetOpacity(0.f);else check(navEffect_->SetOpacity(opacity.Get()));auto shift=animation(m.contentShift,now,scale_,std::round((m.card?16:m.live?20:38)*scale_));check(content_->SetOffsetY(shift.Get()));auto iconsShift=animation(m.contentShift,now,scale_,m.live?-18*scale_:0.f);iconLayer_->SetOffsetY(iconsShift.Get());
    auto barWidth=animation(m.volume,now,262*scale_);check(barClip_->SetRight(barWidth.Get()));
    auto ax=animation(m.artX,now,scale_),ay=animation(m.artY,now,scale_),size=animation(m.artSize,now,1.f/256),ao=animation(m.artOpacity,now);art_->SetOffsetX(ax.Get());art_->SetOffsetY(ay.Get());artScale_->SetScaleX(size.Get());artScale_->SetScaleY(size.Get());artEffect_->SetOpacity(ao.Get());
    auto blend=animation(handoff_.mix,now);incomingEffect_->SetOpacity(blend.Get());
    auto fx=animation(m.artX,now,scale_),fy=animation(m.artY,now,scale_),corner=animation(m.artSize,now,scale_,-15*scale_);artFrame_->SetOffsetX(fx.Get());artFrame_->SetOffsetY(fy.Get());badge_->SetOffsetX(corner.Get());badge_->SetOffsetY(corner.Get());
    auto slide=animation(m.swipe,now,scale_,std::round(20*scale_));check(content_->SetOffsetX(slide.Get()));
    if(barMode_>=0&&barMode_<3){auto bx=animation(m.width,now,scale_,-barInset_*scale_);spectrum_->SetOffsetX(bx.Get());}
    auto hx=animation(m.width,now,scale_/2,-81*scale_),level=animation(m.level,now,150*scale_);hudTrack_->SetOffsetX(hx.Get());hudFill_->SetOffsetX(hx.Get());hudClip_->SetRight(level.Get());
    placeNav(now,navY);
    auto rx=animation(m.width,now,edge_&&!expanded_?0.f:scale_,edge_&&!expanded_?8*scale_:-58*scale_);rings_->SetOffsetX(rx.Get());rings_->SetOffsetY(edge_&&!expanded_?38*scale_:0.f);
    auto ba=animation(batteryAngle,now),ta=animation(timerAngle,now);batteryRotation_->SetAngle(ba.Get());timerRotation_->SetAngle(ta.Get());
    auto pulse=animation(m.pulse,now);pulseEffect_->SetOpacity(pulse.Get());glass_.animate(m,now,edge_,attached_);shadow_.animate(m,now,edge_,attached_);
    {auto slide=animation(m.slide,now,edge_==1?74*scale_:edge_==2?-74*scale_:-44*scale_);if(edge_){stage_->SetOffsetX(slide.Get());stage_->SetOffsetY(0.f);}else{stage_->SetOffsetY(slide.Get());stage_->SetOffsetX(0.f);}
        ComPtr<IDCompositionAnimation> fade;check(device_->CreateAnimation(&fade));check(fade->SetAbsoluteBeginTime(ticks(now)));double duration=0;auto curve=approximateCurve([&](double t){return m.stageOpacity(t);},[&](double t){return m.slide.settled(t);},now,duration);
        for(auto& c:curve)check(fade->AddCubic(c.time,float(c.p),float(c.v),float(c.quadratic),float(c.cubic)));check(fade->End(duration,float(m.stageOpacity(now+10).position)));stageEffect_->SetOpacity(fade.Get());}
    if(m.card)cardIconEffect_->SetOpacity(opacity.Get());
    auto hoverX=animation(m.hoverX,now,scale_),hoverY=animation(m.hoverY,now,scale_),hoverW=animation(m.hoverW,now,scale_),hoverH=animation(m.hoverH,now,scale_),hoverOpacity=animation(m.hoverOpacity,now);hoverVisual_->SetOffsetX(hoverX.Get());hoverVisual_->SetOffsetY(hoverY.Get());hoverClip_->SetRight(hoverW.Get());hoverClip_->SetBottom(hoverH.Get());hoverEffect_->SetOpacity(hoverOpacity.Get());
    commit();
}
}
