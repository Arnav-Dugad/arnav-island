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
void Renderer::text(ID2D1RenderTarget* rt,const std::wstring& value,float x,float y,float w,float size,UINT32 color,DWRITE_FONT_WEIGHT weight,DWRITE_TEXT_ALIGNMENT alignment,float height) {
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(L"Segoe UI Variable Text",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);f->SetTextAlignment(alignment);if(height>0)f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(color),&b));
    rt->DrawText(value.c_str(),UINT32(value.size()),f.Get(),D2D1::RectF(std::round(x*scale_)/scale_,std::round(y*scale_)/scale_,std::round((x+w)*scale_)/scale_,std::round((y+(height>0?height:size*1.6f))*scale_)/scale_),b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Renderer::redraw(const ContentSnapshot& s,bool debug,bool headerOnly) {
    const UINT32 bg=s.light?0xf7f7f9:0x090a0c,ink=s.light?0x202329:0xf1f3f7,muted=s.light?0x656b75:0x8e96a4,raised=s.light?0xeceef2:0x14171d,line=s.light?0xdde1e7:0x242a33;
    const UINT32 accents[]={0xa4deca,0xa6cafa,0xccb8f1,0xefc7a6};UINT32 accent=s.settings.albumAccents&&s.playback.artwork?s.playback.artwork->accent:accents[s.settings.accent];if(s.light)accent=0x487467;
    bool attached=s.settings.verticalOffset==0;expanded_=s.expanded;edge_=s.settings.edge;attached_=attached;iconMotion_=s.settings.animatedIcons&&!s.reducedMotion;
    if(!headerOnly){targets.clear();iconCursor_=7;for(auto& i:icons_)i.used=false;}
    if(baseColor_!=bg||accentColor_!=accent||material_!=s.glassActive||cachedEdge_!=edge_){
        baseColor_=bg;accentColor_=accent;material_=s.glassActive;cachedEdge_=edge_;
        surface(baseSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));if(s.glassActive){rt->PushAxisAlignedClip({20,38,400,266},D2D1_ANTIALIAS_MODE_ALIASED);rt->Clear(D2D1::ColorF(bg,.08f));rt->PopAxisAlignedClip();}});body_->SetContent(baseSurface_.Get());
        surface(innerSurface_,640,500,[&](auto* rt){rt->Clear(D2D1::ColorF(bg));if(s.glassActive){rt->PushAxisAlignedClip({20-1/scale_,38-1/scale_,400-1/scale_,266-1/scale_},D2D1_ANTIALIAS_MODE_ALIASED);rt->Clear(D2D1::ColorF(bg,.96f));rt->PopAxisAlignedClip();}});inner_->SetContent(innerSurface_.Get());
        auto wing=[&](auto* rt,bool right){ComPtr<ID2D1PathGeometry> path;d2d_->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;path->Open(&sink);auto p=[&](float x,float y){if(right)x=32-x;return edge_?D2D1::Point2F(32-y,x):D2D1::Point2F(x,y);};sink->BeginFigure(p(0,0),D2D1_FIGURE_BEGIN_FILLED);sink->AddLine(p(32,0));sink->AddLine(p(32,32));sink->AddBezier({p(32,6.4f),p(25.6f,0),p(0,0)});sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(bg),&b);rt->FillGeometry(path.Get(),b.Get());};
        surface(leftSurface_,32,32,[&](auto* rt){wing(rt,false);});surface(rightSurface_,32,32,[&](auto* rt){wing(rt,true);});
        surface(barSurface_,380,2,[&](auto* rt){rt->Clear(D2D1::ColorF(accent));});bar_->SetContent(barSurface_.Get());
        surface(navSurface_,47,44,[&](auto* rt){ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink,s.light?.055f:.055f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,47,44},12,12),b.Get());});nav_->SetContent(navSurface_.Get());
    }
    wingLeft_->SetContent(attached?leftSurface_.Get():nullptr);wingRight_->SetContent(attached?rightSurface_.Get():nullptr);barEffect_->SetOpacity(s.page==Page::Overview&&s.expanded?1.f:0.f);
    updateArtwork(s,raised);updateTimeline(s,accent,line);updateRings(s,accent,muted,line);
    surface(headerSurface_,260,150,[&](auto* rt){
        if(s.expanded){text(rt,L"ARNAV  /  ISLAND",0,9,180,9,muted,DWRITE_FONT_WEIGHT_MEDIUM);return;}
        std::wstring label=s.activity.empty()?(s.focus.running?clockText(std::ceil(s.focus.displayed(seconds()))):s.settings.compactMedia&&s.playback.available?s.playback.title:L"Arnav Island"):s.activity;
        if(edge_){text(rt,s.battery>=0?std::to_wstring(s.battery)+L"%":L"—",0,82,64,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER);text(rt,s.charging?L"Charging":L"Battery",0,105,64,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_CENTER);}
        else{bool hasArt=bool(s.playback.artwork)&&s.settings.compactMedia;float start=hasArt?42.f:14.f,end=float(s.settings.compactWidth)-(ringsEnabled_?(ringCount_==2?64:40):45);text(rt,label,start,0,end-start,11.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);if(!ringsEnabled_&&s.settings.compactBattery&&s.battery>=0)text(rt,std::to_wstring(s.battery)+L"%",float(s.settings.compactWidth)-44,0,34,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_CENTER,34);}
    });header_->SetContent(headerSurface_.Get());if(headerOnly){commit();return;}
    drawingContent_=true;iconRequests_.clear();
    surface(contentSurface_,380,284,[&](auto* rt){
        ComPtr<ID2D1SolidColorBrush>b;rt->CreateSolidColorBrush(D2D1::ColorF(ink),&b);
        auto box=[&](float x,float y,float w,float h,UINT32 color,float radius=12){b->SetColor(D2D1::ColorF(color));rt->FillRoundedRectangle(D2D1::RoundedRect({x,y,x+w,y+h},radius,radius),b.Get());};
        auto hairline=[&](float x,float y,float w){b->SetColor(D2D1::ColorF(line));rt->DrawLine({x,y},{x+w,y},b.Get(),1/scale_);};
        auto label=[&](const std::wstring& value,float x,float y,float w,float h,float size,UINT32 color,DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_MEDIUM){text(rt,value,x,y,w,size,color,weight,DWRITE_TEXT_ALIGNMENT_CENTER,h);};
        auto button=[&](Action a,const std::wstring& value,float x,float y,float w,float h,bool selected=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});box(x,y,w,h,selected?(s.light?0x262c34:0xe8ecf2):raised,9);label(value,x+6,y,w-12,h,11.5f,!enabled?muted:selected?(s.light?0xf8f8fa:0x171b22):ink);};
        auto iconButton=[&](Action a,Icon glyph,float x,float y,float w=32,float h=32,bool primary=false,bool enabled=true){targets.push_back({a,x,y,w,h,enabled});if(primary)box(x,y,w,h,!enabled?raised:s.light?0x252b33:0xe8ecf2,h/2);icon(a,glyph,x+(w-18)/2,y+(h-18)/2,18,!enabled?muted:primary?(s.light?0xffffff:0x161b22):ink);};
        auto value=[](double v,int precision=0){if(v<0)return std::wstring(L"—");wchar_t buf[48];swprintf(buf,48,precision?L"%.1f":L"%.0f",v);return std::wstring(buf);};
        const wchar_t* titles[]={L"Your day, at a glance",L"Now playing",L"System overview",L"Make room for focus",L"Make it yours",L"Within reach",L"Sound, your way"};
        text(rt,titles[int(s.page)],0,0,302,18,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);iconButton(Action::Pin,Icon::Pin,312,-4,28,28,s.pinned);iconButton(Action::Close,Icon::Close,352,-4,28,28);
        if(s.page==Page::Overview){
            if(!s.playback.artwork){box(0,43,64,64,raised,15);drawIcon(rt,d2d_.Get(),Icon::Music,19,62,26,muted);}
            text(rt,s.playback.available?s.playback.title:L"A quieter place for everything",80,48,252,14,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.available?s.playback.artist:L"Play something. Find your rhythm.",80,74,252,11,muted);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,340,58,40,40,true,s.playback.canToggle);targets.push_back({Action::Media,0,38,328,74});
            const Icon glyphs[]={Icon::Processor,Icon::Memory,Icon::Battery,Icon::Download,Icon::Upload,Icon::Disk,Icon::Clock};const wchar_t* names[]={L"CPU",L"Memory",L"Battery",L"Download",L"Upload",L"Disk free",L"Uptime"};
            for(int i=0;i<3;++i){int metric=s.settings.homeMetrics[i];float x=i*130.f;box(x,126,120,68,raised,13);drawIcon(rt,d2d_.Get(),glyphs[metric],x+12,137,14,muted);text(rt,names[metric],x+33,136,77,10,muted);std::wstring number;switch(metric){case 0:number=value(s.system.cpu)+ (s.system.cpu>=0?L"%":L"");break;case 1:number=s.system.ramTotalGiB?value(s.system.ramPercent)+L"%":L"—";break;case 2:number=s.battery>=0?std::to_wstring(s.battery)+L"%":L"—";break;case 3:number=s.system.networkAvailable?rateText(s.system.download):L"—";break;case 4:number=s.system.networkAvailable?rateText(s.system.upload):L"—";break;case 5:number=s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—";break;default:number=clockText(double(s.system.uptime));break;}text(rt,number,x+12,156,100,metric>=3?18:23,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
            iconButton(Action::Mute,s.muted?Icon::Muted:Icon::Volume,0,200,28,28);b->SetColor(D2D1::ColorF(line));rt->DrawLine({38,215},{322,215},b.Get(),2);text(rt,s.muted?L"Muted":std::to_wstring(s.volume)+L"%",264,199,58,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);iconButton(Action::Audio,Icon::Audio,346,200,34,28);targets.push_back({Action::VolumeSlider,38,201,284,26});
        }else if(s.page==Page::Media){
            bool video=s.settings.mediaLayout==2||(s.settings.mediaLayout==0&&s.playback.kind==MediaKind::Video);float tx=video?166.f:118.f,tw=380-tx;
            if(!s.playback.artwork){box(0,video?30:44,video?148:100,video?148:100,raised,18);drawIcon(rt,d2d_.Get(),Icon::Music,video?56:34,video?85:77,32,muted);}
            text(rt,s.playback.title,tx,45,tw,16,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(rt,s.playback.artist,tx,74,tw,11,muted);text(rt,video?L"VIDEO SESSION":L"MUSIC SESSION",tx,102,tw,8.5f,muted);
            iconButton(Action::Previous,Icon::Previous,tx,132,36,36,false,s.playback.canPrevious);iconButton(Action::Play,s.playback.playing?Icon::Pause:Icon::Play,tx+54,124,52,52,true,s.playback.canToggle);iconButton(Action::Next,Icon::Next,tx+124,132,36,36,false,s.playback.canNext);
            double position=std::clamp(s.playback.position+(s.playback.playing?std::max(0.,seconds()-s.playback.sampledAt):0.),0.,s.playback.duration);if(s.playback.canSeek)targets.push_back({Action::Seek,0,178,380,25});if(s.scrub.active){position=s.scrub.value;text(rt,L"Pull away for finer control",tx,176,tw,8.5f,muted);}text(rt,s.playback.duration>0?clockText(position):L"No timeline provided",0,200,160,10,muted);button(Action::MediaMode,s.settings.mediaLayout==0?L"Auto":s.settings.mediaLayout==1?L"Music":L"Video",167,199,54,22);text(rt,s.playback.duration>0?clockText(s.playback.duration):L"",300,200,80,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);
        }else if(s.page==Page::System){
            auto stat=[&](Icon glyph,const wchar_t* name,const std::wstring& number,float x,float y,float w){drawIcon(rt,d2d_.Get(),glyph,x,y,14,muted);text(rt,name,x+22,y-1,w-22,10,muted);text(rt,number,x,y+20,w,22,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);};
            stat(Icon::Processor,L"PROCESSOR",s.system.cpu<0?L"—":value(s.system.cpu)+L"%",0,42,180);stat(Icon::Memory,L"MEMORY / GB",s.system.ramTotalGiB?value(s.system.ramUsedGiB,1)+L" / "+value(s.system.ramTotalGiB,1):L"—",208,42,172);
            b->SetColor(D2D1::ColorF(accent));for(unsigned i=1;i<s.system.samples;++i){unsigned base=40-s.system.samples;rt->DrawLine({float((i-1)*380./39),125-s.system.cpuHistory[base+i-1]*.25f},{float(i*380./39),125-s.system.cpuHistory[base+i]*.25f},b.Get(),1.5f);}hairline(0,132,380);
            stat(Icon::Download,L"DOWN",s.system.networkAvailable?rateText(s.system.download):L"—",0,147,120);stat(Icon::Upload,L"UP",s.system.networkAvailable?rateText(s.system.upload):L"—",132,147,120);stat(Icon::Disk,L"FREE",s.system.diskTotalGiB?value(s.system.diskFreeGiB)+L" GB":L"—",264,147,116);text(rt,std::to_wstring(s.system.logicalProcessors)+L" threads   /   Up "+clockText(double(s.system.uptime)),0,208,380,10,muted);
        }else if(s.page==Page::Focus){
            button(Action::Timer25,L"Focus",0,36,120,28,s.focus.mode==FocusClock::Mode::Focus);button(Action::Timer5,L"Break",130,36,120,28,s.focus.mode==FocusClock::Mode::Break);button(Action::Stopwatch,L"Stopwatch",260,36,120,28,s.focus.mode==FocusClock::Mode::Stopwatch);
            double progress=s.focus.mode==FocusClock::Mode::Stopwatch?std::fmod(s.focus.elapsed(seconds()),60.)/60.:normalizedProgress(s.focus.displayed(seconds()),s.focus.duration);drawRing(rt,d2d_.Get(),63,130,38,3,progress,accent,line);drawIcon(rt,d2d_.Get(),Icon::Focus,50,117,26,accent);
            text(rt,clockText(std::ceil(s.focus.displayed(seconds()))),124,91,252,42,ink,DWRITE_FONT_WEIGHT_LIGHT);text(rt,s.focus.finished?L"A moment well spent":s.focus.running?L"One thing at a time":L"A little space to begin",127,144,245,11,muted);iconButton(Action::TimerToggle,s.focus.running?Icon::Pause:Icon::Play,130,180,42,42,true);iconButton(Action::TimerReset,Icon::Reset,190,183,36,36);text(rt,s.focus.running?L"Pause session":L"Start session",238,193,142,10,muted);
        }else if(s.page==Page::Shelf){
            if(s.shelf.empty()){box(0,40,380,151,raised,18);icon(Action::None,Icon::Shelf,172,57,36,accent);label(s.dropHover?L"Release to keep it close":L"A place to keep things close",20,105,340,28,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD);label(L"Drop files or text. Drag them back out.",20,139,340,22,11,muted);}else for(size_t i=s.shelfOffset;i<std::min(size_t(s.shelfOffset+4),s.shelf.size());++i){float y=40+float(i-s.shelfOffset)*36;auto& item=s.shelf[i];Action a=Action(int(Action::ShelfItemBase)+int(i));targets.push_back({a,0,y,380,32});box(0,y,380,32,raised,9);if(item.preview)drawPreview(rt,*item.preview,6,y+3,32,26);else icon(a,item.kind==ShelfItem::Kind::File?Icon::File:Icon::Text,14,y+8,16,muted);text(rt,item.label,46,y+7,326,11,ink);}
            text(rt,std::to_wstring(s.shelf.size())+L" items  ·  Copy only",0,208,260,10,muted);button(Action::ShelfClear,L"Clear",316,201,64,25,false,!s.shelf.empty());
        }else if(s.page==Page::Audio){
            text(rt,s.feedback.empty()?L"Choose where your sound goes":s.feedback,0,34,380,11,muted);for(size_t i=s.audioOffset;i<std::min(size_t(s.audioOffset+4),s.outputs.size());++i){float y=59+float(i-s.audioOffset)*34;auto& output=s.outputs[i];Action a=Action(int(Action::DeviceBase)+int(i));targets.push_back({a,0,y,380,30});box(0,y,380,30,raised,9);icon(a,Icon::Audio,10,y+7,16,output.current?accent:muted);text(rt,output.name,37,y+7,302,11,ink);if(output.current)drawIcon(rt,d2d_.Get(),Icon::Check,353,y+7,16,accent);}
            button(Action::SoundSettings,L"Windows sound settings",0,203,201,25);icon(Action::SoundSettings,Icon::ArrowRight,208,208,14,muted);text(rt,s.settings.directAudio?L"Direct switching":L"System picker",236,210,144,9,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);
        }else{
            const wchar_t* groups[]={L"Behavior",L"Appearance",L"Motion",L"Activities",L"Advanced",L"Personal layout",L"Fine details",L"Multitasking"};button(Action::SettingsNext,groups[s.settingsPage],192,31,188,27);drawIcon(rt,d2d_.Get(),Icon::Chevron,359,38,12,ink);text(rt,L"Saved on this device",0,40,181,10,muted);
            auto row=[&](int i,const wchar_t* name,Action action,const std::wstring& val){float y=73+i*29.f;text(rt,name,0,y,232,11.5f,ink,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,25);button(action,val,242,y,138,25);};
            const wchar_t* metricNames[]={L"CPU",L"Memory",L"Battery",L"Download",L"Upload",L"Disk free",L"Uptime"};
            if(s.settingsPage==0){row(0,L"Start at sign-in",Action::StartupToggle,s.settings.startAtLogin?L"On":L"Off");row(1,L"Open on hover",Action::HoverToggle,s.settings.hoverOpen?L"On":L"Off");row(2,L"Hover delay",Action::HoverDelay,std::to_wstring(s.settings.hoverDelay)+L" ms");row(3,L"Close delay",Action::CollapseDelay,std::to_wstring(s.settings.collapseDelay)+L" ms");row(4,L"Hide in fullscreen",Action::FullscreenToggle,s.settings.hideFullscreen?L"On":L"Off");}
            else if(s.settingsPage==1){row(0,L"Theme",Action::Theme,std::array<std::wstring,3>{L"Dark",L"Light",L"System"}[s.settings.theme]);row(1,L"Scale",Action::Scale,std::to_wstring(s.settings.scale)+L"%");row(2,L"Dock edge",Action::Edge,s.settings.edge?L"Right":L"Top center");row(3,L"Compact width",Action::CompactWidth,std::to_wstring(s.settings.compactWidth)+L" px");row(4,L"Corners",Action::Corner,std::to_wstring(s.settings.corner)+L" px");}
            else if(s.settingsPage==2){row(0,L"Motion",Action::MotionPreset,std::array<std::wstring,5>{L"Balanced",L"Fluid",L"Playful",L"Snappy",L"Calm"}[s.settings.preset]);row(1,L"Reduce motion",Action::ReducedToggle,s.settings.reduceMotion?L"On":L"Off");row(2,L"Magnetic buttons",Action::MagneticToggle,s.settings.magnetic?L"On":L"Off");row(3,L"Album accent",Action::AccentsToggle,s.settings.albumAccents?L"On":L"Off");row(4,L"Native glass",Action::GlassToggle,s.settings.glass?L"Auto":L"Off");}
            else if(s.settingsPage==3){row(0,L"Compact media",Action::CompactMediaToggle,s.settings.compactMedia?L"On":L"Off");row(1,L"Compact battery",Action::CompactBatteryToggle,s.settings.compactBattery?L"On":L"Off");row(2,L"Accent",Action::Accent,std::array<std::wstring,4>{L"Mint",L"Sky",L"Lilac",L"Peach"}[s.settings.accent]);row(3,L"Output compatibility",Action::AudioCompatibility,s.settings.directAudio?L"Enabled":L"Off");row(4,L"Media layout",Action::MediaMode,std::array<std::wstring,3>{L"Auto",L"Music",L"Video"}[s.settings.mediaLayout]);}
            else if(s.settingsPage==4){row(0,L"Monitor",Action::Monitor,s.settings.monitor?std::to_wstring(s.settings.monitor):L"Primary");row(1,L"Edge offset",Action::Offset,std::to_wstring(s.settings.verticalOffset)+L" px");row(2,L"Motion tuning",Action::Lab,L"Animation Lab");row(3,L"Display scaling",Action::DisplaySettings,L"Windows settings");row(4,L"Reset preferences",Action::SettingsReset,L"Reset");}
            else if(s.settingsPage==5){const wchar_t* pages[]={L"Home",L"Media",L"Stats",L"Focus",L"Settings",L"Shelf",L"Audio"};row(0,L"Choose navigation item",Action::LayoutSlot,pages[s.settings.navigation[s.layoutSlot]]);text(rt,L"Move selected item",0,102,230,11.5f,ink,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,25);iconButton(Action::LayoutLeft,Icon::ArrowLeft,250,102,46,25,false,s.layoutSlot>0);iconButton(Action::LayoutRight,Icon::ArrowRight,322,102,46,25,false,s.layoutSlot<6);row(2,L"Home · first statistic",Action::MetricOne,metricNames[s.settings.homeMetrics[0]]);row(3,L"Home · second statistic",Action::MetricTwo,metricNames[s.settings.homeMetrics[1]]);row(4,L"Home · third statistic",Action::MetricThree,metricNames[s.settings.homeMetrics[2]]);}
            else if(s.settingsPage==6){row(0,L"Animated icons",Action::IconsToggle,s.settings.animatedIcons?L"On":L"Off");row(1,L"Track handoff",Action::HandoffToggle,s.settings.trackHandoff?L"Blend":L"Instant");row(2,L"Glance rings",Action::Rings,std::array<std::wstring,4>{L"Off",L"Battery",L"Timer",L"Both"}[s.settings.glanceRings]);row(3,L"Restore navigation & stats",Action::LayoutReset,L"Reset layout");row(4,L"Motion laboratory",Action::Lab,L"Open");}else{row(0,L"Collapse when switching apps",Action::AppSwitchToggle,s.settings.collapseOnAppSwitch?L"On":L"Off");row(1,L"Scroll anywhere for volume",Action::WheelVolumeToggle,s.settings.wheelVolume?L"On":L"Slider only");text(rt,L"Pinned panels stay open while you work.",0,143,380,11,muted);text(rt,L"Your page and timer are kept when collapsed.",0,166,380,11,muted);text(rt,L"Fullscreen playback respects your hide preference.",0,189,380,11,muted);}
        }
        hairline(0,229,380);
        const wchar_t* labels[]={L"Home",L"Media",L"Stats",L"Focus",L"Settings",L"Shelf",L"Audio"};const Action actions[]={Action::Overview,Action::Media,Action::System,Action::Focus,Action::Settings,Action::Shelf,Action::Audio};const Icon glyphs[]={Icon::Home,Icon::Music,Icon::Stats,Icon::Focus,Icon::Settings,Icon::Shelf,Icon::Audio};
        for(int slot=0;slot<7;++slot){int p=s.settings.navigation[slot];float x=std::round((slot*navStep+4)*scale_)/scale_;bool selected=int(s.page)==p;targets.push_back({actions[p],x,navY,47,44});icon(actions[p],glyphs[p],x+14,navY+5,19,selected?ink:muted,p);label(labels[p],x,navY+26,47,16,9.5f,selected?ink:muted);if(selected){if(s.reducedMotion)navX.reset(x,seconds());else if(navX.target()!=x)navX.retarget(x,seconds(),MotionTokens::navigation);}}
        if(debug){b->SetColor(D2D1::ColorF(0xf98585));for(auto& target:targets)rt->DrawRectangle({target.x,target.y,target.x+target.width,target.y+target.height},b.Get());}
    });drawingContent_=false;for(auto request:iconRequests_)icon(request.action,request.glyph,request.x,request.y,request.size,request.color,request.slot);content_->SetContent(contentSurface_.Get());for(auto& item:icons_)if(!item.used)item.effect->SetOpacity(0.f);auto nx=animation(navX,seconds(),scale_,20*scale_);nav_->SetOffsetX(nx.Get());nav_->SetOffsetY((38+navY)*scale_);commit();
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
    auto w=animation(m.width,now,scale_),h=animation(m.height,now,scale_),r=animation(m.radius,now,scale_);check(clip_->SetRight(w.Get()));check(clip_->SetBottom(h.Get()));
    auto iw=animation(m.width,now,scale_,-2),ih=animation(m.height,now,scale_,-2),ir=animation(m.radius,now,scale_,-1);check(innerClip_->SetRight(iw.Get()));check(innerClip_->SetBottom(ih.Get()));
    bool tl=!attached_||edge_,tr=!attached_,bl=true,br=!attached_||!edge_;

    auto corners=[&](IDCompositionRectangleClip* clip,IDCompositionAnimation* radius){
        if(tl){clip->SetTopLeftRadiusX(radius);clip->SetTopLeftRadiusY(radius);}else{clip->SetTopLeftRadiusX(0.f);clip->SetTopLeftRadiusY(0.f);}
        if(tr){clip->SetTopRightRadiusX(radius);clip->SetTopRightRadiusY(radius);}else{clip->SetTopRightRadiusX(0.f);clip->SetTopRightRadiusY(0.f);}
        if(bl){clip->SetBottomLeftRadiusX(radius);clip->SetBottomLeftRadiusY(radius);}else{clip->SetBottomLeftRadiusX(0.f);clip->SetBottomLeftRadiusY(0.f);}
        if(br){clip->SetBottomRightRadiusX(radius);clip->SetBottomRightRadiusY(radius);}else{clip->SetBottomRightRadiusX(0.f);clip->SetBottomRightRadiusY(0.f);}
    };corners(clip_.Get(),r.Get());corners(innerClip_.Get(),ir.Get());
    auto x=animation(m.width,now,edge_?-scale_:-scale_/2,canvasWidth*scale_/(edge_?1:2));if(m.width.settled(now))check(body_->SetOffsetX(std::round(float((edge_?canvasWidth-m.width.target():(canvasWidth-m.width.target())/2)*scale_))));else check(body_->SetOffsetX(x.Get()));
    if(edge_){auto y=animation(m.height,now,-scale_/2,canvasHeight*scale_/2);if(m.height.settled(now))check(body_->SetOffsetY(std::round(float((canvasHeight-m.height.target())*scale_/2))));else check(body_->SetOffsetY(y.Get()));}else check(body_->SetOffsetY(0.f));
    auto dx=animation(m.dragX,now,scale_),dy=animation(m.dragY,now,scale_);check(root_->SetOffsetX(dx.Get()));check(root_->SetOffsetY(dy.Get()));
    auto shoulder=animation(m.radius,now,1.f/32);for(auto p:{leftScale_.Get(),rightScale_.Get()}){check(p->SetScaleX(shoulder.Get()));check(p->SetScaleY(shoulder.Get()));p->SetCenterX(edge_?32*scale_:0.f);p->SetCenterY(0.f);}
    if(edge_){leftScale_->SetCenterY(32*scale_);wingLeft_->SetOffsetX((canvasWidth-32)*scale_);wingRight_->SetOffsetX((canvasWidth-32)*scale_);auto ly=animation(m.height,now,-scale_/2,(canvasHeight/2-32)*scale_),ry=animation(m.height,now,scale_/2,canvasHeight*scale_/2);wingLeft_->SetOffsetY(ly.Get());wingRight_->SetOffsetY(ry.Get());header_->SetOffsetX(expanded_?std::round(20*scale_):0.f);}
    else{leftScale_->SetCenterX(32*scale_);auto lx=animation(m.width,now,-scale_/2,(canvasWidth/2-32)*scale_),rx=animation(m.width,now,scale_/2,canvasWidth*scale_/2);wingLeft_->SetOffsetX(lx.Get());wingRight_->SetOffsetX(rx.Get());wingLeft_->SetOffsetY(0.f);wingRight_->SetOffsetY(0.f);auto hx=animation(m.width,now,expanded_?0.f:scale_/2,expanded_?20*scale_:float(-m.compactWidth/2)*scale_);if(m.width.settled(now))header_->SetOffsetX(std::round(float(expanded_?20*scale_:(m.width.target()-m.compactWidth)*scale_/2)));else header_->SetOffsetX(hx.Get());}
    auto opacity=visibility(m,now);auto headerOpacity=visibility(m,now,!expanded_);headerEffect_->SetOpacity(headerOpacity.Get());check(contentEffect_->SetOpacity(opacity.Get()));check(iconEffect_->SetOpacity(opacity.Get()));check(navEffect_->SetOpacity(opacity.Get()));auto shift=animation(m.contentShift,now,scale_,std::round(38*scale_));check(content_->SetOffsetY(shift.Get()));auto iconsShift=animation(m.contentShift,now,scale_);iconLayer_->SetOffsetY(iconsShift.Get());
    auto barWidth=animation(m.volume,now,284*scale_);check(barClip_->SetRight(barWidth.Get()));
    auto ax=animation(m.artX,now,scale_),ay=animation(m.artY,now,scale_),size=animation(m.artSize,now,1.f/256),ao=animation(m.artOpacity,now);art_->SetOffsetX(ax.Get());art_->SetOffsetY(ay.Get());artScale_->SetScaleX(size.Get());artScale_->SetScaleY(size.Get());artEffect_->SetOpacity(ao.Get());
    auto blend=animation(handoff_.mix,now);incomingEffect_->SetOpacity(blend.Get());
    auto nx=animation(navX,now,scale_,20*scale_);nav_->SetOffsetX(nx.Get());nav_->SetOffsetY((38+navY)*scale_);
    auto rx=animation(m.width,now,edge_&&!expanded_?0.f:scale_,edge_&&!expanded_?8*scale_:-58*scale_);rings_->SetOffsetX(rx.Get());rings_->SetOffsetY(edge_&&!expanded_?38*scale_:0.f);
    auto ba=animation(batteryAngle,now),ta=animation(timerAngle,now);batteryRotation_->SetAngle(ba.Get());timerRotation_->SetAngle(ta.Get());
    auto pulse=animation(m.pulse,now);pulseEffect_->SetOpacity(pulse.Get());
    auto hoverX=animation(m.hoverX,now,scale_),hoverY=animation(m.hoverY,now,scale_),hoverW=animation(m.hoverW,now,1.f/64),hoverH=animation(m.hoverH,now,1.f/32),hoverOpacity=animation(m.hoverOpacity,now);hoverVisual_->SetOffsetX(hoverX.Get());hoverVisual_->SetOffsetY(hoverY.Get());hoverScale_->SetScaleX(hoverW.Get());hoverScale_->SetScaleY(hoverH.Get());hoverEffect_->SetOpacity(hoverOpacity.Get());
    commit();
}
}
