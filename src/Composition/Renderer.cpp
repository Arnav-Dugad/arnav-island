#include "Renderer.h"
#include <dxgi1_2.h>

namespace nexus {
void Renderer::initialize(HWND hwnd,float dpi) {
    dpi_=dpi;scale_=dpi/96;
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr);
    if(FAILED(hr)){software=true;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr));}
    check(d3d_.As(&dxgi_));check(DCompositionCreateDevice(dxgi_.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(device_.GetAddressOf())));
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,d2d_.GetAddressOf()));
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write_.GetAddressOf())));
    check(device_->CreateTargetForHwnd(hwnd,TRUE,&target_));
    for(auto* p:{std::addressof(root_),std::addressof(body_),std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_)})check(device_->CreateVisual(p->GetAddressOf()));
    check(target_->SetRoot(root_.Get()));check(root_->AddVisual(body_.Get(),FALSE,nullptr));
    for(auto* p:{std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_)})check(body_->AddVisual(p->Get(),TRUE,nullptr));
    check(device_->CreateRectangleClip(&clip_));check(body_->SetClip(clip_.Get()));
    check(device_->CreateRectangleClip(&innerClip_));check(inner_->SetClip(innerClip_.Get()));
    check(inner_->SetOffsetX(1));check(inner_->SetOffsetY(1));
    check(content_->SetOffsetX(24*scale_));check(content_->SetOffsetY(61*scale_));
    check(bar_->SetOffsetX(24*scale_));check(bar_->SetOffsetY(240*scale_));
    check(device_->CreateRectangleClip(&barClip_));check(bar_->SetClip(barClip_.Get()));
    check(barClip_->SetBottom(4*scale_));
    check(device_->CreateEffectGroup(&contentEffect_));
    check(content_->SetEffect(contentEffect_.Get()));check(bar_->SetEffect(contentEffect_.Get()));
    surface(baseSurface_,640,440,[](auto* rt){rt->Clear(D2D1::ColorF(0x303540));});
    check(body_->SetContent(baseSurface_.Get()));
    surface(innerSurface_,640,440,[](auto* rt){
        rt->Clear(D2D1::ColorF(0x080a0e));
        D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0x171c24)},{.48f,D2D1::ColorF(0x0b0e14)},{1,D2D1::ColorF(0x080a0e)}};
        ComPtr<ID2D1GradientStopCollection> sc;check(rt->CreateGradientStopCollection(stops,3,&sc));
        ComPtr<ID2D1LinearGradientBrush> b;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{320,320}),sc.Get(),&b));
        rt->FillRectangle(D2D1::RectF(0,0,640,440),b.Get());
    });check(inner_->SetContent(innerSurface_.Get()));
    surface(barSurface_,432,4,[](auto* rt){rt->Clear(D2D1::ColorF(0x92dcca));});check(bar_->SetContent(barSurface_.Get()));
}
void Renderer::surface(ComPtr<IDCompositionSurface>& s,int w,int h,std::function<void(ID2D1RenderTarget*)> draw) {
    if(!s)check(device_->CreateSurface(UINT(std::ceil(w*scale_)),UINT(std::ceil(h*scale_)),DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_ALPHA_MODE_PREMULTIPLIED,&s));
    ComPtr<IDXGISurface> dx;POINT offset{};
    check(s->BeginDraw(nullptr,__uuidof(IDXGISurface),reinterpret_cast<void**>(dx.GetAddressOf()),&offset));
    try {
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),dpi_,dpi_);
        ComPtr<ID2D1RenderTarget> rt;check(d2d_->CreateDxgiSurfaceRenderTarget(dx.Get(),&properties,&rt));
        rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Translation(offset.x/scale_,offset.y/scale_));
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);rt->Clear(D2D1::ColorF(0,0));draw(rt.Get());check(rt->EndDraw());
    }catch(...){s->EndDraw();throw;}
    check(s->EndDraw());++redraws;
}
void Renderer::text(ID2D1RenderTarget* rt,const std::wstring& value,float x,float y,float w,float size,UINT32 color,DWRITE_FONT_WEIGHT weight) {
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(L"Segoe UI Variable Text",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(color),&b));
    rt->DrawText(value.c_str(),UINT32(value.size()),f.Get(),D2D1::RectF(x,y,x+w,y+size*1.6f),b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Renderer::redraw(const ContentSnapshot& s,bool debug) {
    surface(headerSurface_,204,42,[&](auto* rt){
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(0x92dcca),&b);
        rt->FillEllipse(D2D1::Ellipse({34,19},3,3),b.Get());
        text(rt,L"N E X U S",51,9,114,13,0xe9edf3,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        b->SetColor(D2D1::ColorF(0x52616c));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(166,17,181,21),2,2),b.Get());
    });check(header_->SetContent(headerSurface_.Get()));
    surface(contentSurface_,472,360,[&](auto* rt){
        text(rt,s.headline,0,0,430,25,0xf1f3f8,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(rt,s.detail,0,37,430,12,0x99a5b5);
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(0x1c2430),&b);
        rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0,76,54,130),15,15),b.Get());
        b->SetColor(D2D1::ColorF(0x91d6c8));
        for(int i=0;i<4;++i){float h=10+float((i*13)%25);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(15+i*7,103-h/2,18+i*7,103+h/2),1.5f,1.5f),b.Get());}
        text(rt,s.media,70,79,344,16,0xe4eaf2,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(rt,s.artist,70,105,344,12,0x94a0b0);
        text(rt,s.muted?L"MUTED":L"OUTPUT VOLUME",0,153,190,10,0x95a4b8,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(rt,std::to_wstring(s.volume)+L"%",368,149,60,16,0xe6edf5);
        b->SetColor(D2D1::ColorF(0x2b323d));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0,179,432,183),2,2),b.Get());
        std::wstring battery=s.battery<0?L"Battery unavailable":std::to_wstring(s.battery)+L"% battery"+(s.charging?L"  /  plugged in":L"  /  on battery");
        text(rt,battery,0,198,268,11,0x93a4b5);
        text(rt,L"Scroll to adjust  /  Right-click for more",0,229,430,11,0x788797);
        if(debug) {b->SetColor(D2D1::ColorF(0xfb8e85));rt->DrawRectangle(D2D1::RectF(.5f,.5f,431.5f,251.5f),b.Get());}
    });check(content_->SetContent(contentSurface_.Get()));commit();
}
ComPtr<IDCompositionAnimation> Renderer::animation(const Spring& s,double now,float factor,float bias) {
    ComPtr<IDCompositionAnimation>a;check(device_->CreateAnimation(&a));
    check(a->SetAbsoluteBeginTime(ticks(now)));
    double duration;auto curve=s.curve(now,duration);
    for(auto& c:curve)check(a->AddCubic(c.time,float(c.p)*factor+bias,float(c.v)*factor,float(c.quadratic)*factor,float(c.cubic)*factor));
    check(a->End(duration,float(s.target())*factor+bias));return a;
}
void Renderer::animate(const MotionEngine& m,double now) {
    auto w=animation(m.width,now,scale_),h=animation(m.height,now,scale_),r=animation(m.radius,now,scale_);
    check(clip_->SetRight(w.Get()));check(clip_->SetBottom(h.Get()));

    check(clip_->SetTopLeftRadiusX(r.Get()));check(clip_->SetTopLeftRadiusY(r.Get()));
    check(clip_->SetTopRightRadiusX(r.Get()));check(clip_->SetTopRightRadiusY(r.Get()));
    check(clip_->SetBottomLeftRadiusX(r.Get()));check(clip_->SetBottomLeftRadiusY(r.Get()));
    check(clip_->SetBottomRightRadiusX(r.Get()));check(clip_->SetBottomRightRadiusY(r.Get()));
    auto iw=animation(m.width,now,scale_,-2),ih=animation(m.height,now,scale_,-2),ir=animation(m.radius,now,scale_,-1);
    check(innerClip_->SetRight(iw.Get()));check(innerClip_->SetBottom(ih.Get()));
    check(innerClip_->SetTopLeftRadiusX(ir.Get()));check(innerClip_->SetTopLeftRadiusY(ir.Get()));
    check(innerClip_->SetTopRightRadiusX(ir.Get()));check(innerClip_->SetTopRightRadiusY(ir.Get()));
    check(innerClip_->SetBottomLeftRadiusX(ir.Get()));check(innerClip_->SetBottomLeftRadiusY(ir.Get()));
    check(innerClip_->SetBottomRightRadiusX(ir.Get()));check(innerClip_->SetBottomRightRadiusY(ir.Get()));
    auto x=animation(m.width,now,-scale_/2,canvasWidth*scale_/2);
    check(body_->SetOffsetX(x.Get()));
    auto dx=animation(m.dragX,now,scale_),dy=animation(m.dragY,now,scale_);
    check(root_->SetOffsetX(dx.Get()));check(root_->SetOffsetY(dy.Get()));
    auto hx=animation(m.width,now,scale_/2,-102*scale_);check(header_->SetOffsetX(hx.Get()));
    auto opacity=animation(m.reveal,now);check(contentEffect_->SetOpacity(opacity.Get()));
    auto barWidth=animation(m.volume,now,432*scale_);check(barClip_->SetRight(barWidth.Get()));
    commit();
}
}

