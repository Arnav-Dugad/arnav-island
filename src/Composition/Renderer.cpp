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
    for(auto* p:{std::addressof(root_),std::addressof(body_),std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_),std::addressof(art_),std::addressof(wingLeft_),std::addressof(wingRight_),std::addressof(pulseVisual_),std::addressof(hoverVisual_)})check(device_->CreateVisual(p->GetAddressOf()));
    check(root_->AddVisual(wingLeft_.Get(),FALSE,nullptr));check(root_->AddVisual(wingRight_.Get(),FALSE,nullptr));check(target_->SetRoot(root_.Get()));check(root_->AddVisual(body_.Get(),FALSE,nullptr));
    for(auto* p:{std::addressof(inner_),std::addressof(header_),std::addressof(content_),std::addressof(bar_),std::addressof(art_),std::addressof(pulseVisual_),std::addressof(hoverVisual_)})check(body_->AddVisual(p->Get(),FALSE,nullptr));
    check(device_->CreateRectangleClip(&clip_));check(clip_->SetLeft(0.f));check(clip_->SetTop(0.f));check(body_->SetClip(clip_.Get()));
    check(device_->CreateRectangleClip(&innerClip_));check(innerClip_->SetLeft(0.f));check(innerClip_->SetTop(0.f));check(inner_->SetClip(innerClip_.Get()));
    check(inner_->SetOffsetX(1));check(inner_->SetOffsetY(1));
    check(content_->SetOffsetX(20*scale_));check(content_->SetOffsetY(38*scale_));
    check(bar_->SetOffsetX(20*scale_));check(bar_->SetOffsetY(244*scale_));
    check(device_->CreateRectangleClip(&barClip_));check(barClip_->SetLeft(0.f));check(barClip_->SetTop(0.f));check(bar_->SetClip(barClip_.Get()));
    check(barClip_->SetBottom(2*scale_));
    check(device_->CreateEffectGroup(&contentEffect_));
    check(content_->SetEffect(contentEffect_.Get()));check(device_->CreateEffectGroup(&barEffect_));check(bar_->SetEffect(barEffect_.Get()));
    check(device_->CreateRectangleClip(&artClip_));check(artClip_->SetLeft(0.f));check(artClip_->SetTop(0.f));check(artClip_->SetRight(128*scale_));check(artClip_->SetBottom(128*scale_));
    check(artClip_->SetTopLeftRadiusX(16.f*scale_));check(artClip_->SetTopLeftRadiusY(16.f*scale_));check(artClip_->SetTopRightRadiusX(16.f*scale_));check(artClip_->SetTopRightRadiusY(16.f*scale_));check(artClip_->SetBottomLeftRadiusX(16.f*scale_));check(artClip_->SetBottomLeftRadiusY(16.f*scale_));check(artClip_->SetBottomRightRadiusX(16.f*scale_));check(artClip_->SetBottomRightRadiusY(16.f*scale_));check(art_->SetClip(artClip_.Get()));
    for(auto pair:{std::pair{std::addressof(headerEffect_),header_.Get()},std::pair{std::addressof(artEffect_),art_.Get()},std::pair{std::addressof(pulseEffect_),pulseVisual_.Get()},std::pair{std::addressof(hoverEffect_),hoverVisual_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));check(pair.second->SetEffect(pair.first->Get()));}
    for(auto pair:{std::pair{std::addressof(artScale_),art_.Get()},std::pair{std::addressof(leftScale_),wingLeft_.Get()},std::pair{std::addressof(rightScale_),wingRight_.Get()},std::pair{std::addressof(hoverScale_),hoverVisual_.Get()}}){check(device_->CreateScaleTransform(pair.first->GetAddressOf()));check(pair.second->SetTransform(pair.first->Get()));}
    surface(pulseSurface_,640,500,[](auto* rt){rt->Clear(D2D1::ColorF(0x80e8ba,.22f));});check(pulseVisual_->SetContent(pulseSurface_.Get()));
    surface(hoverSurface_,64,32,[](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(0xffffff,.10f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0,0,64,32),10,10),b.Get());});check(hoverVisual_->SetContent(hoverSurface_.Get()));
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
void Renderer::redraw(const ContentSnapshot& s,bool debug,bool headerOnly) {
    const UINT32 bg=s.light?0xf5f5f7:0x0b0c0f,ink=s.light?0x17181b:0xf1f2f5,muted=s.light?0x62656b:0x999ea8,raised=s.light?0xe7e8eb:0x1a1c21;
    const UINT32 accents[]={0xa5d8c5,0xa6cafa,0xc9b5f2,0xf1c4a2};UINT32 accent=s.settings.albumAccents&&s.playback.artwork?s.playback.artwork->accent:accents[s.settings.accent];
    bool attached=s.settings.verticalOffset==0;if(!headerOnly)targets.clear();expanded_=s.expanded;edge_=s.settings.edge;attached_=attached;
    if(baseColor_!=bg||accentColor_!=accent||material_!=s.glassActive||cachedEdge_!=edge_){
        baseColor_=bg;accentColor_=accent;material_=s.glassActive;cachedEdge_=edge_;
        surface(baseSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));if(s.glassActive){rt->PushAxisAlignedClip(D2D1::RectF(20,38,400,246),D2D1_ANTIALIAS_MODE_ALIASED);rt->Clear(D2D1::ColorF(bg,.08f));rt->PopAxisAlignedClip();}});check(body_->SetContent(baseSurface_.Get()));
        surface(innerSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));if(s.glassActive){rt->PushAxisAlignedClip(D2D1::RectF(20-1/scale_,38-1/scale_,400-1/scale_,246-1/scale_),D2D1_ANTIALIAS_MODE_ALIASED);rt->Clear(D2D1::ColorF(bg,.94f));rt->PopAxisAlignedClip();}});check(inner_->SetContent(innerSurface_.Get()));
        auto wing=[&](auto* rt,bool right){ComPtr<ID2D1PathGeometry> path;d2d_->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;path->Open(&sink);auto p=[&](float x,float y){if(right)x=32-x;return edge_?D2D1::Point2F(32-y,x):D2D1::Point2F(x,y);};sink->BeginFigure(p(0,0),D2D1_FIGURE_BEGIN_FILLED);sink->AddLine(p(32,0));sink->AddLine(p(32,32));sink->AddBezier({p(32,6.4f),p(25.6f,0),p(0,0)});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(bg),&b);rt->FillGeometry(path.Get(),b.Get());};
        surface(leftSurface_,32,32,[&](auto* rt){wing(rt,false);});surface(rightSurface_,32,32,[&](auto* rt){wing(rt,true);});check(wingLeft_->SetContent(leftSurface_.Get()));check(wingRight_->SetContent(rightSurface_.Get()));
        surface(barSurface_,380,2,[&](auto* rt){rt->Clear(D2D1::ColorF(s.light?0x436f60:accent));});check(bar_->SetContent(barSurface_.Get()));
    }
    check(wingLeft_->SetContent(attached?leftSurface_.Get():nullptr));check(wingRight_->SetContent(attached?rightSurface_.Get():nullptr));
    check(barEffect_->SetOpacity(s.page==Page::Overview&&s.expanded?1.f:0.f));
    if(artwork_!=s.playback.artwork){artwork_=s.playback.artwork;if(artwork_)surface(artSurface_,128,128,[&](auto* rt){rt->Clear(D2D1::ColorF(raised));auto& a=*artwork_;ComPtr<ID2D1Bitmap> bitmap;auto props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));check(rt->CreateBitmap(D2D1::SizeU(a.width,a.height),a.pixels.data(),a.width*4,props,&bitmap));float k=std::min(128.f/a.width,128.f/a.height),w=a.width*k,h=a.height*k;rt->DrawBitmap(bitmap.Get(),D2D1::RectF((128-w)/2,(128-h)/2,(128+w)/2,(128+h)/2));});check(art_->SetContent(artwork_?artSurface_.Get():nullptr));}
    surface(headerSurface_,260,150,[&](auto* rt){
        if(s.expanded){text(rt,L"Arnav Island",0,8,180,10,muted);return;}
        std::wstring label=s.activity.empty()?(s.focus.running?clockText(s.focus.displayed(seconds())):s.settings.compactMedia&&s.playback.available?s.playback.title:L"Arnav Island"):s.activity;
        if(edge_){text(rt,s.focus.running?L"◷":L"●",20,47,40,15,s.light?0x477967:accent);text(rt,s.battery>=0?std::to_wstring(s.battery)+L"%":L"—",12,80,50,13,ink);text(rt,s.charging?L"Power":L"Island",12,105,50,9,muted);}
        else {bool hasArt=bool(s.playback.artwork)&&s.settings.compactMedia;text(rt,label,hasArt?42.f:14.f,9,float(s.settings.compactWidth)-(hasArt?94:66),11,ink,DWRITE_FONT_WEIGHT_MEDIUM);text(rt,s.settings.compactBattery&&s.battery>=0?std::to_wstring(s.battery)+L"%":s.focus.running?L"◷":L"·",float(s.settings.compactWidth)-43,10,40,10,muted);}
    });check(header_->SetContent(headerSurface_.Get()));if(headerOnly){commit();return;}
    surface(contentSurface_,380,248,[&](auto* rt){
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink),&b);
        auto box=[&](float x,float y,float w,float h,UINT32 color,float radius=10){b->SetColor(D2D1::ColorF(color));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),b.Get());};
        auto button=[&](Action a,const std::wstring& label,float x,float y,float w,float h,bool selected=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});box(x,y,w,h,selected?(s.light?0x24272d:0xe6e9ef):raised,9);text(rt,label,x+(w<30?5.f:7.f),y+(h-16)/2,std::max(12.f,w-12),11,!enabled?muted:selected?(s.light?0xf6f6f8:0x191b20):ink,DWRITE_FONT_WEIGHT_MEDIUM);};
        auto value=[](double v,int n=0){if(v<0)return std::wstring(L"—");wchar_t buf[48];swprintf(buf,48,n?L"%.1f":L"%.0f",v);return std::wstring(buf);};
        auto metric=[&](float x,float y,float w,const std::wstring& label,const std::wstring& number){text(rt,label,x,y,w,10,muted);text(rt,number,x,y+18,w,21,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);};
        const wchar_t* titles[]={L"At a glance",L"Now playing",L"System",L"Focus",L"Preferences",L"File shelf",L"Audio output"};text(rt,titles[int(s.page)],0,0,260,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);button(Action::Pin,s.pinned?L"●":L"○",310,-3,30,26,s.pinned);button(Action::Close,L"×",350,-3,30,26);
        if(s.page==Page::Overview){
            if(!s.playback.artwork){box(0,43,64,64,raised,14);b->SetColor(D2D1::ColorF(muted));for(int i=0;i<5;++i){float h=std::array<float,5>{8,19,28,17,10}[i];rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(17+i*6.f,75-h/2,19+i*6.f,75+h/2),1,1),b.Get());}}
            text(rt,s.playback.available?s.playback.title:L"A little space for your day",78,46,220,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.available?s.playback.artist:L"Media, focus and the essentials",78,70,270,11,muted);button(Action::Play,s.playback.playing?L"Ⅱ":L"▷",332,44,48,38,false,s.playback.canToggle);targets.push_back({Action::Media,0,38,314,74});
            metric(0,125,110,L"CPU",s.system.cpu<0?L"—":value(s.system.cpu)+L"%");metric(130,125,110,L"MEMORY",s.system.ramTotalGiB?value(s.system.ramPercent)+L"%":L"—");metric(260,125,120,s.charging?L"ON POWER":L"BATTERY",s.battery<0?L"—":std::to_wstring(s.battery)+L"%");
            text(rt,L"↓ "+rateText(s.system.download)+L"  ↑ "+rateText(s.system.upload),0,184,202,10,muted);button(Action::VolumeDown,L"−",213,176,32,27);button(Action::Mute,s.muted?L"Mute":std::to_wstring(s.volume)+L"%",252,176,62,27);button(Action::VolumeUp,L"+",321,176,32,27);button(Action::Audio,L"›",358,176,22,27);
        }else if(s.page==Page::Media){
            bool video=s.settings.mediaLayout==2||(s.settings.mediaLayout==0&&s.playback.kind==MediaKind::Video);
            float tx=video?166.f:118.f,tw=380-tx;
            if(!s.playback.artwork)box(0,video?30.f:44.f,video?148.f:100.f,video?148.f:100.f,raised,16);
            text(rt,s.playback.title,tx,45,tw,16,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist,tx,73,tw,11,muted);text(rt,video?L"VIDEO":L"MUSIC",tx,100,100,9,muted);button(Action::MediaMode,s.settings.mediaLayout==0?L"Auto":s.settings.mediaLayout==1?L"Music":L"Video",282,98,98,24);
            button(Action::Previous,L"‹‹",tx,133,42,30,false,s.playback.canPrevious);button(Action::Play,s.playback.playing?L"Pause":L"Play",tx+51,133,100,30,true,s.playback.canToggle);button(Action::Next,L"››",tx+160,133,42,30,false,s.playback.canNext);
            double p=std::clamp(s.playback.position+(s.playback.playing?std::max(0.,seconds()-s.playback.sampledAt):0.),0.,s.playback.duration);box(0,184,380,2,raised,1);if(s.playback.duration>0)box(0,184,float(380*p/s.playback.duration),2,s.light?0x587566:accent,1);text(rt,s.playback.duration>0?clockText(p):L"Timeline unavailable",0,193,220,9,muted);text(rt,s.playback.duration>0?clockText(s.playback.duration):L"",337,193,43,9,muted);
        }else if(s.page==Page::System){
            metric(0,40,185,L"PROCESSOR",s.system.cpu<0?L"—":value(s.system.cpu)+L"%");metric(204,40,176,L"MEMORY",s.system.ramTotalGiB?value(s.system.ramUsedGiB,1)+L" / "+value(s.system.ramTotalGiB,1)+L" GB":L"—");
            b->SetColor(D2D1::ColorF(s.light?0x547866:accent));for(unsigned i=1;i<s.system.samples;++i){unsigned base=40-s.system.samples;rt->DrawLine({float((i-1)*380./39),121-s.system.cpuHistory[base+i-1]*.25f},{float(i*380./39),121-s.system.cpuHistory[base+i]*.25f},b.Get(),1.3f);}
            metric(0,138,125,L"DOWNLOAD",rateText(s.system.download));metric(132,138,120,L"UPLOAD",rateText(s.system.upload));metric(264,138,116,L"DISK FREE",s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—");text(rt,std::to_wstring(s.system.logicalProcessors)+L" threads  ·  Up "+clockText(double(s.system.uptime)),0,196,380,10,muted);
        }else if(s.page==Page::Focus){
            button(Action::Timer25,L"Focus",0,36,120,28,s.focus.mode==FocusClock::Mode::Focus);button(Action::Timer5,L"Break",130,36,120,28,s.focus.mode==FocusClock::Mode::Break);button(Action::Stopwatch,L"Stopwatch",260,36,120,28,s.focus.mode==FocusClock::Mode::Stopwatch);
            text(rt,clockText(s.focus.mode==FocusClock::Mode::Stopwatch?s.focus.displayed(seconds()):std::ceil(s.focus.displayed(seconds()))),104,79,270,48,ink,DWRITE_FONT_WEIGHT_LIGHT);button(Action::TimerToggle,s.focus.running?L"Pause":L"Start",90,157,95,32,true);button(Action::TimerReset,L"Reset",195,157,95,32);text(rt,s.focus.finished?L"Session complete":s.focus.running?L"One thing at a time":L"25 minutes of focus · 5 minutes to reset",58,199,322,10,muted);
        }else if(s.page==Page::Shelf){
            if(s.shelf.empty()){box(0,40,380,122,raised,18);text(rt,s.dropHover?L"Release to keep here":L"Drop a file into the island",51,72,305,17,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,L"Drag it back out whenever you need it",61,105,290,11,muted);}else{for(size_t i=s.shelfOffset;i<std::min(size_t(s.shelfOffset+4),s.shelf.size());++i){float y=37+float(i-s.shelfOffset)*35;auto& item=s.shelf[i];button(Action(int(Action::ShelfItemBase)+int(i)),(item.kind==ShelfItem::Kind::File?L"▧  ":L"T  ")+item.label,0,y,380,29);}}
            text(rt,std::to_wstring(s.shelf.size())+L" items · scroll to browse · copy only",0,185,275,10,muted);button(Action::ShelfClear,L"Clear",306,178,74,29,false,!s.shelf.empty());
        }else if(s.page==Page::Audio){
            text(rt,s.feedback.empty()?L"Choose speakers or headphones":s.feedback,0,33,380,11,muted);for(size_t i=s.audioOffset;i<std::min(size_t(s.audioOffset+4),s.outputs.size());++i){auto& output=s.outputs[i];button(Action(int(Action::DeviceBase)+int(i)),(output.current?L"✓  ":L"   ")+output.name,0,58+float(i-s.audioOffset)*34,380,28,output.current);}
            button(Action::SoundSettings,L"Windows sound settings ↗",0,195,224,25);text(rt,s.settings.directAudio?L"Direct switching":L"System picker",245,199,135,9,muted);
        }else{
            const wchar_t* groups[]={L"Behavior",L"Appearance",L"Motion",L"Activities",L"Advanced"};button(Action::SettingsNext,std::wstring(groups[s.settingsPage])+L"  ›",180,28,200,26);text(rt,L"Saved automatically",0,35,174,10,muted);
            auto row=[&](int i,const wchar_t* name,Action action,const std::wstring& val){float y=64+i*28.f;text(rt,name,0,y+5,235,11,ink);button(action,val,246,y,134,24);};
            if(s.settingsPage==0){row(0,L"Start at sign-in",Action::StartupToggle,s.settings.startAtLogin?L"On":L"Off");row(1,L"Open on hover",Action::HoverToggle,s.settings.hoverOpen?L"On":L"Off");row(2,L"Hover delay",Action::HoverDelay,std::to_wstring(s.settings.hoverDelay)+L" ms");row(3,L"Close delay",Action::CollapseDelay,std::to_wstring(s.settings.collapseDelay)+L" ms");row(4,L"Hide in fullscreen",Action::FullscreenToggle,s.settings.hideFullscreen?L"On":L"Off");}
            else if(s.settingsPage==1){row(0,L"Theme",Action::Theme,std::array<std::wstring,3>{L"Dark",L"Light",L"System"}[s.settings.theme]);row(1,L"Scale",Action::Scale,std::to_wstring(s.settings.scale)+L"%");row(2,L"Dock edge",Action::Edge,s.settings.edge?L"Right":L"Top center");row(3,L"Compact width",Action::CompactWidth,std::to_wstring(s.settings.compactWidth)+L" px");row(4,L"Corners",Action::Corner,std::to_wstring(s.settings.corner)+L" px");}
            else if(s.settingsPage==2){row(0,L"Motion",Action::MotionPreset,std::array<std::wstring,5>{L"Balanced",L"Fluid",L"Playful",L"Snappy",L"Calm"}[s.settings.preset]);row(1,L"Reduce motion",Action::ReducedToggle,s.settings.reduceMotion?L"On":L"Off");row(2,L"Magnetic buttons",Action::MagneticToggle,s.settings.magnetic?L"On":L"Off");row(3,L"Album accent",Action::AccentsToggle,s.settings.albumAccents?L"On":L"Off");row(4,L"Native glass",Action::GlassToggle,s.settings.glass?L"Auto":L"Off");}
            else if(s.settingsPage==3){row(0,L"Compact media",Action::CompactMediaToggle,s.settings.compactMedia?L"On":L"Off");row(1,L"Compact battery",Action::CompactBatteryToggle,s.settings.compactBattery?L"On":L"Off");row(2,L"Accent",Action::Accent,std::array<std::wstring,4>{L"Mint",L"Sky",L"Lilac",L"Peach"}[s.settings.accent]);row(3,L"Output compatibility",Action::AudioCompatibility,s.settings.directAudio?L"Enabled":L"Off");row(4,L"Media layout",Action::MediaMode,std::array<std::wstring,3>{L"Auto",L"Music",L"Video"}[s.settings.mediaLayout]);}
            else{row(0,L"Monitor",Action::Monitor,s.settings.monitor?std::to_wstring(s.settings.monitor):L"Primary");row(1,L"Edge offset",Action::Offset,std::to_wstring(s.settings.verticalOffset)+L" px");row(2,L"Motion tuning",Action::Lab,L"Animation Lab");row(3,L"Display scaling",Action::DisplaySettings,L"Windows ↗");row(4,L"Reset appearance",Action::SettingsReset,L"Reset");}
        }
        const wchar_t* labels[]={L"Home",L"Media",L"Stats",L"Focus",L"Shelf",L"Audio",L"⋯"};const Action actions[]={Action::Overview,Action::Media,Action::System,Action::Focus,Action::Shelf,Action::Audio,Action::Settings};const Page pages[]={Page::Overview,Page::Media,Page::System,Page::Focus,Page::Shelf,Page::Audio,Page::Settings};for(int i=0;i<7;++i)button(actions[i],labels[i],i*55.f,222,50,26,s.page==pages[i]);
        if(debug){b->SetColor(D2D1::ColorF(0xf98585));for(auto& t:targets)rt->DrawRectangle(D2D1::RectF(t.x,t.y,t.x+t.width,t.y+t.height),b.Get());}
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
    auto w=animation(m.width,now,scale_),h=animation(m.height,now,scale_),r=animation(m.radius,now,scale_);check(clip_->SetRight(w.Get()));check(clip_->SetBottom(h.Get()));
    auto iw=animation(m.width,now,scale_,-2),ih=animation(m.height,now,scale_,-2),ir=animation(m.radius,now,scale_,-1);check(innerClip_->SetRight(iw.Get()));check(innerClip_->SetBottom(ih.Get()));
    bool tl=!attached_||edge_,tr=!attached_,bl=true,br=!attached_||!edge_;

    auto corners=[&](IDCompositionRectangleClip* clip,IDCompositionAnimation* radius){
        if(tl){clip->SetTopLeftRadiusX(radius);clip->SetTopLeftRadiusY(radius);}else{clip->SetTopLeftRadiusX(0.f);clip->SetTopLeftRadiusY(0.f);}
        if(tr){clip->SetTopRightRadiusX(radius);clip->SetTopRightRadiusY(radius);}else{clip->SetTopRightRadiusX(0.f);clip->SetTopRightRadiusY(0.f);}
        if(bl){clip->SetBottomLeftRadiusX(radius);clip->SetBottomLeftRadiusY(radius);}else{clip->SetBottomLeftRadiusX(0.f);clip->SetBottomLeftRadiusY(0.f);}
        if(br){clip->SetBottomRightRadiusX(radius);clip->SetBottomRightRadiusY(radius);}else{clip->SetBottomRightRadiusX(0.f);clip->SetBottomRightRadiusY(0.f);}
    };corners(clip_.Get(),r.Get());corners(innerClip_.Get(),ir.Get());
    auto x=animation(m.width,now,edge_?-scale_:-scale_/2,canvasWidth*scale_/(edge_?1:2));check(body_->SetOffsetX(x.Get()));
    if(edge_){auto y=animation(m.height,now,-scale_/2,canvasHeight*scale_/2);check(body_->SetOffsetY(y.Get()));}else check(body_->SetOffsetY(0.f));
    auto dx=animation(m.dragX,now,scale_),dy=animation(m.dragY,now,scale_);check(root_->SetOffsetX(dx.Get()));check(root_->SetOffsetY(dy.Get()));
    auto shoulder=animation(m.radius,now,1.f/32);for(auto p:{leftScale_.Get(),rightScale_.Get()}){check(p->SetScaleX(shoulder.Get()));check(p->SetScaleY(shoulder.Get()));p->SetCenterX(edge_?32*scale_:0.f);p->SetCenterY(0.f);}
    if(edge_){leftScale_->SetCenterY(32*scale_);wingLeft_->SetOffsetX((canvasWidth-32)*scale_);wingRight_->SetOffsetX((canvasWidth-32)*scale_);auto ly=animation(m.height,now,-scale_/2,(canvasHeight/2-32)*scale_),ry=animation(m.height,now,scale_/2,canvasHeight*scale_/2);wingLeft_->SetOffsetY(ly.Get());wingRight_->SetOffsetY(ry.Get());header_->SetOffsetX(expanded_?20*scale_:0.f);}
    else{leftScale_->SetCenterX(32*scale_);auto lx=animation(m.width,now,-scale_/2,(canvasWidth/2-32)*scale_),rx=animation(m.width,now,scale_/2,canvasWidth*scale_/2);wingLeft_->SetOffsetX(lx.Get());wingRight_->SetOffsetX(rx.Get());wingLeft_->SetOffsetY(0.f);wingRight_->SetOffsetY(0.f);auto hx=animation(m.width,now,expanded_?0.f:scale_/2,expanded_?20*scale_:float(-m.compactWidth/2)*scale_);header_->SetOffsetX(hx.Get());}
    auto opacity=animation(m.reveal,now);check(contentEffect_->SetOpacity(opacity.Get()));auto shift=animation(m.contentShift,now,scale_,38*scale_);check(content_->SetOffsetY(shift.Get()));
    auto barWidth=animation(m.volume,now,380*scale_);check(barClip_->SetRight(barWidth.Get()));
    auto ax=animation(m.artX,now,scale_),ay=animation(m.artY,now,scale_),size=animation(m.artSize,now,1.f/128),ao=animation(m.artOpacity,now);art_->SetOffsetX(ax.Get());art_->SetOffsetY(ay.Get());artScale_->SetScaleX(size.Get());artScale_->SetScaleY(size.Get());artEffect_->SetOpacity(ao.Get());
    auto pulse=animation(m.pulse,now);pulseEffect_->SetOpacity(pulse.Get());
    auto hoverX=animation(m.hoverX,now,scale_),hoverY=animation(m.hoverY,now,scale_),hoverW=animation(m.hoverW,now,1.f/64),hoverH=animation(m.hoverH,now,1.f/32),hoverOpacity=animation(m.hoverOpacity,now);hoverVisual_->SetOffsetX(hoverX.Get());hoverVisual_->SetOffsetY(hoverY.Get());hoverScale_->SetScaleX(hoverW.Get());hoverScale_->SetScaleY(hoverH.Get());hoverEffect_->SetOpacity(hoverOpacity.Get());
    commit();
}
}
