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
    for(auto* p:{std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_)})check(body_->AddVisual(p->Get(),FALSE,nullptr));
    check(device_->CreateRectangleClip(&clip_));check(clip_->SetLeft(0.f));check(clip_->SetTop(0.f));check(body_->SetClip(clip_.Get()));
    check(device_->CreateRectangleClip(&innerClip_));check(innerClip_->SetLeft(0.f));check(innerClip_->SetTop(0.f));check(inner_->SetClip(innerClip_.Get()));
    check(inner_->SetOffsetX(1));check(inner_->SetOffsetY(1));
    check(content_->SetOffsetX(24*scale_));check(content_->SetOffsetY(58*scale_));
    check(bar_->SetOffsetX(24*scale_));check(bar_->SetOffsetY(430*scale_));
    check(device_->CreateRectangleClip(&barClip_));check(barClip_->SetLeft(0.f));check(barClip_->SetTop(0.f));check(bar_->SetClip(barClip_.Get()));
    check(barClip_->SetBottom(4*scale_));
    check(device_->CreateEffectGroup(&contentEffect_));
    check(content_->SetEffect(contentEffect_.Get()));check(device_->CreateEffectGroup(&barEffect_));check(bar_->SetEffect(barEffect_.Get()));
    surface(baseSurface_,640,560,[](auto* rt){rt->Clear(D2D1::ColorF(0x303033));});
    check(body_->SetContent(baseSurface_.Get()));
    surface(innerSurface_,640,560,[](auto* rt){
        rt->Clear(D2D1::ColorF(0x09090b));
        D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0x161619)},{.48f,D2D1::ColorF(0x101012)},{1,D2D1::ColorF(0x09090b)}};
        ComPtr<ID2D1GradientStopCollection> sc;check(rt->CreateGradientStopCollection(stops,3,&sc));
        ComPtr<ID2D1LinearGradientBrush> b;check(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{320,320}),sc.Get(),&b));
        rt->FillRectangle(D2D1::RectF(0,0,640,560),b.Get());
    });check(inner_->SetContent(innerSurface_.Get()));
    surface(barSurface_,512,4,[](auto* rt){rt->Clear(D2D1::ColorF(0x92dcca));});check(bar_->SetContent(barSurface_.Get()));
}
void Renderer::surface(ComPtr<IDCompositionSurface>& s,int w,int h,std::function<void(ID2D1RenderTarget*)> draw) {
    if(!s)check(device_->CreateSurface(UINT(std::ceil(w*scale_)),UINT(std::ceil(h*scale_)),DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_ALPHA_MODE_PREMULTIPLIED,&s));
    ComPtr<IDXGISurface> dx;POINT offset{};
    check(s->BeginDraw(nullptr,__uuidof(IDXGISurface),reinterpret_cast<void**>(dx.GetAddressOf()),&offset));
    try {
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),dpi_,dpi_);
        ComPtr<ID2D1RenderTarget> rt;check(d2d_->CreateDxgiSurfaceRenderTarget(dx.Get(),&properties,&rt));
        rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Translation(offset.x/scale_,offset.y/scale_));
        rt->PushAxisAlignedClip(D2D1::RectF(0,0,float(w),float(h)),D2D1_ANTIALIAS_MODE_ALIASED);rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);rt->Clear(D2D1::ColorF(0,0));draw(rt.Get());rt->PopAxisAlignedClip();check(rt->EndDraw());
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
    targets.clear();check(barEffect_->SetOpacity(s.page==Page::Overview?1.f:0.f));
    surface(headerSurface_,204,42,[&](auto* rt){
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(s.playback.playing?0xa9daca:0x9999a2),&b);
        rt->FillEllipse(D2D1::Ellipse({29,19},2.5f,2.5f),b.Get());
        text(rt,L"Arnav Island",43,9,130,13,0xf3f3f5,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        b->SetColor(D2D1::ColorF(0x606067));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(169,17,181,20),1.5f,1.5f),b.Get());
    });check(header_->SetContent(headerSurface_.Get()));
    surface(contentSurface_,512,414,[&](auto* rt){
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(0xffffff),&b);
        auto box=[&](float x,float y,float w,float h,UINT32 color,float radius=14){b->SetColor(D2D1::ColorF(color));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),b.Get());};
        auto line=[&](float x,float y,float w,UINT32 color){b->SetColor(D2D1::ColorF(color));rt->DrawLine({x,y},{x+w,y},b.Get(),1);};
        auto button=[&](Action action,const std::wstring& label,float x,float y,float w,float h,bool selected=false,bool enabled=true){
            targets.push_back({action,x,y,w,h,enabled});box(x,y,w,h,!enabled?0x171719:s.hovered==action?0x3a3a40:selected?0xe8e8ed:0x222225,11);
            text(rt,label,x+12,y+(h-18)/2,w-24,12,!enabled?0x65656c:selected?0x151518:0xe1e1e7,DWRITE_FONT_WEIGHT_MEDIUM);
        };
        auto art=[&](float x,float y,float w,float h){
            box(x,y,w,h,0x222227,14);
            if(s.playback.artwork){const auto& a=*s.playback.artwork;ComPtr<ID2D1Bitmap> bitmap;auto props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
                if(SUCCEEDED(rt->CreateBitmap(D2D1::SizeU(a.width,a.height),a.pixels.data(),a.width*4,props,&bitmap))){float k=std::min(w/a.width,h/a.height),dw=a.width*k,dh=a.height*k;rt->DrawBitmap(bitmap.Get(),D2D1::RectF(x+(w-dw)/2,y+(h-dh)/2,x+(w+dw)/2,y+(h+dh)/2));}
            }else{b->SetColor(D2D1::ColorF(0x696975));float cx=x+w/2,cy=y+h/2;for(int i=0;i<4;++i){float v=10+((i*11)%23);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx-15+i*9,cy-v/2,cx-11+i*9,cy+v/2),2,2),b.Get());}}
        };
        auto value=[](double v,int decimals=0){wchar_t result[64];if(v<0)return std::wstring(L"—");swprintf(result,64,decimals?L"%.1f":L"%.0f",v);return std::wstring(result);};
        auto stat=[&](float x,float y,float w,const std::wstring& name,const std::wstring& number,const std::wstring& detail){box(x,y,w,90,0x1a1a1d);text(rt,name,x+15,y+12,w-30,11,0x95959e);text(rt,number,x+14,y+31,w-28,25,0xf4f4f7,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,detail,x+15,y+66,w-30,10,0x9e9ea7);};
        const wchar_t* pages[]={L"Overview",L"Media",L"System",L"Focus",L"Settings"};
        text(rt,pages[int(s.page)],0,0,350,25,0xf5f5f7,DWRITE_FONT_WEIGHT_SEMI_BOLD);
        button(Action::Pin,s.pinned?L"Pinned":L"Pin",378,0,76,30,s.pinned);button(Action::Close,L"×",464,0,48,30);
        line(0,47,512,0x29292d);
        if(s.page==Page::Overview){
            SYSTEMTIME now{};GetLocalTime(&now);wchar_t time[32]{},date[80]{};GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT,TIME_NOSECONDS,&now,nullptr,time,32);GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,0,&now,L"dddd, d MMMM",date,80,nullptr);
            text(rt,time,0,61,250,32,0xf4f4f7,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,date,1,105,300,12,0x9898a2);
            text(rt,s.charging?L"Connected to power":L"On battery",336,69,176,12,0xc5c5ce);text(rt,s.system.networkAvailable?L"Network connected":L"Network unavailable",336,93,176,11,0x91919b);
            box(0,140,512,91,0x1b1b1e);art(12,152,66,66);text(rt,s.playback.title,94,156,300,15,0xf0f0f4,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist.empty()?L"Windows media session":s.playback.artist,94,183,300,11,0x9999a5);
            button(Action::Play,s.playback.playing?L"Pause":L"Play",414,166,84,38,false,s.playback.canToggle);
            stat(0,246,164,L"CPU",value(s.system.cpu)+L"%",L"System utilization");stat(174,246,164,L"MEMORY",value(s.system.ramPercent)+L"%",value(s.system.ramUsedGiB,1)+L" / "+value(s.system.ramTotalGiB,1)+L" GB");stat(348,246,164,L"BATTERY",s.battery<0?L"—":std::to_wstring(s.battery)+L"%",s.charging?L"External power":L"Battery power");
            text(rt,L"↓ "+rateText(s.system.download)+L"    ↑ "+rateText(s.system.upload),0,350,240,11,0x92929e);button(Action::VolumeDown,L"−",252,341,44,30);button(Action::Mute,s.muted?L"Muted":std::to_wstring(s.volume)+L"%",304,341,90,30);button(Action::VolumeUp,L"+",402,341,44,30);button(Action::SoundSettings,L"↗",454,341,58,30);
        }else if(s.page==Page::Media){
            MediaKind kind=s.settings.mediaLayout==1?MediaKind::Music:s.settings.mediaLayout==2?MediaKind::Video:s.playback.kind;
            bool video=kind==MediaKind::Video;
            button(Action::MediaMode,s.settings.mediaLayout==0?L"Layout: Auto":s.settings.mediaLayout==1?L"Layout: Music":L"Layout: Video",350,60,162,28);
            text(rt,video?L"WATCHING":kind==MediaKind::Music?L"LISTENING":L"NOW PLAYING",0,66,260,10,0x9e9ea8,DWRITE_FONT_WEIGHT_SEMI_BOLD);
            if(video){art(0,102,228,128);text(rt,s.playback.title,246,113,266,19,0xf0f0f5,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist.empty()?L"Windows media session":s.playback.artist,246,150,266,12,0xa4a4ae);text(rt,s.playback.playing?L"Playing":s.playback.available?L"Paused":L"Open a compatible player",246,185,266,11,0x9dcaba);}
            else{art(0,102,144,144);text(rt,s.playback.title,168,115,344,22,0xf0f0f5,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist.empty()?L"Windows media session":s.playback.artist,168,154,344,13,0xa4a4ae);text(rt,s.playback.playing?L"Playing":s.playback.available?L"Paused":L"Open a compatible player",168,194,344,11,0x9dcaba);}
            double position=std::clamp(s.playback.position+(s.playback.playing?std::max(0.,seconds()-s.playback.sampledAt):0.),0.,s.playback.duration);
            box(0,265,512,3,0x303036,1.5f);if(s.playback.duration>0)box(0,265,float(512*position/s.playback.duration),3,0xcdcdd7,1.5f);
            text(rt,s.playback.duration>0?clockText(position):L"Timeline unavailable",0,279,220,10,0x90909d);text(rt,s.playback.duration>0?clockText(s.playback.duration):L"",460,279,52,10,0x90909d);
            button(Action::Previous,L"Previous",100,307,96,38,false,s.playback.canPrevious);button(Action::Play,s.playback.playing?L"Pause":L"Play",206,307,100,38,true,s.playback.canToggle);button(Action::Next,L"Next",316,307,96,38,false,s.playback.canNext);
            text(rt,s.playback.artwork?L"Artwork supplied by your player":L"Thumbnail appears when the player shares it with Windows",0,353,512,10,0x858590);
        }else if(s.page==Page::System){
            stat(0,64,248,L"PROCESSOR",value(s.system.cpu)+L"%",std::to_wstring(s.system.logicalProcessors)+L" logical processors");stat(264,64,248,L"MEMORY",value(s.system.ramUsedGiB,1)+L" GB",value(s.system.ramTotalGiB,1)+L" GB installed");
            b->SetColor(D2D1::ColorF(0x9ac9b9));for(unsigned i=1;i<s.system.samples;++i){unsigned base=40-s.system.samples;rt->DrawLine({float((i-1)*512./39),207-s.system.cpuHistory[base+i-1]*.36f},{float(i*512./39),207-s.system.cpuHistory[base+i]*.36f},b.Get(),1.5f);}text(rt,L"CPU · recent samples",0,214,250,10,0x858591);text(rt,L"Up "+clockText(double(s.system.uptime)),344,214,168,10,0x858591);
            stat(0,244,164,L"DOWNLOAD",rateText(s.system.download),L"Active adapters");stat(174,244,164,L"UPLOAD",rateText(s.system.upload),L"Active adapters");stat(348,244,164,L"DISK FREE",value(s.system.diskFreeGiB)+L" GB",value(s.system.diskTotalGiB)+L" GB system drive");
            button(Action::SoundSettings,L"Sound ↗",0,345,120,28);button(Action::DisplaySettings,L"Display ↗",130,345,120,28);button(Action::NetworkSettings,L"Network ↗",260,345,120,28);button(Action::BluetoothSettings,L"Bluetooth ↗",390,345,122,28);
        }else if(s.page==Page::Focus){
            button(Action::Timer25,L"25 min focus",0,66,164,34,s.focus.mode==FocusClock::Mode::Focus);button(Action::Timer5,L"5 min break",174,66,164,34,s.focus.mode==FocusClock::Mode::Break);button(Action::Stopwatch,L"Stopwatch",348,66,164,34,s.focus.mode==FocusClock::Mode::Stopwatch);
            text(rt,clockText(s.focus.mode==FocusClock::Mode::Stopwatch?s.focus.displayed(seconds()):std::ceil(s.focus.displayed(seconds()))),127,128,300,68,0xf6f6f9,DWRITE_FONT_WEIGHT_LIGHT);
            text(rt,s.focus.finished?L"Session complete. Take a breath.":s.focus.running?L"One thing at a time.":L"A little room to concentrate.",127,223,360,13,0x9696a3);
            button(Action::TimerToggle,s.focus.running?L"Pause":s.focus.finished?L"Again":L"Start",130,281,120,42,true);button(Action::TimerReset,L"Reset",262,281,120,42);
        }else{
            auto row=[&](float y,const std::wstring& title,const std::wstring& detail,Action action,const std::wstring& value){text(rt,title,0,y+4,310,13,0xe0e0e7);text(rt,detail,0,y+25,310,10,0x8b8b98);button(action,value,362,y,150,36);};
            row(65,L"Open on hover",L"Leave to close, or pin to keep it open",Action::HoverToggle,s.settings.hoverOpen?L"On":L"Off");
            row(117,L"Hover delay",L"A short pause prevents accidental opening",Action::HoverDelay,std::to_wstring(s.settings.hoverDelay)+L" ms");
            row(169,L"Motion",L"Continuous springs, with preserved velocity",Action::MotionPreset,std::array<std::wstring,5>{L"Balanced",L"Fluid",L"Playful",L"Snappy",L"Calm"}[s.settings.preset]);
            row(221,L"Reduce motion",L"Use quiet transitions",Action::ReducedToggle,s.settings.reduceMotion?L"On":L"Off");
            row(273,L"Hide in fullscreen",L"Give games and movies the display",Action::FullscreenToggle,s.settings.hideFullscreen?L"On":L"Off");
            text(rt,L"Saved automatically · Local only",0,349,330,11,0x9999a4);button(Action::Lab,L"Motion lab",382,339,130,30);
        }
        const Action actions[]={Action::Overview,Action::Media,Action::System,Action::Focus,Action::Settings};
        for(int i=0;i<5;++i)button(actions[i],pages[i],float(i*104),382,i==4?96:98,32,int(s.page)==i);
        if(debug){b->SetColor(D2D1::ColorF(0xfb8e85));for(auto& t:targets)rt->DrawRectangle(D2D1::RectF(t.x,t.y,t.x+t.width,t.y+t.height),b.Get());}
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
    auto barWidth=animation(m.volume,now,512*scale_);check(barClip_->SetRight(barWidth.Get()));
    commit();
}
}
