#include "Renderer.h"
// Phase 5B, Now Playing Pro: synced lyric lines, the seek time bubble and skip feedback.
namespace nexus {
bool Renderer::lyricsPanel(const ContentSnapshot& s){
    return s.settings.lyrics&&s.lyricsView&&s.lyrics&&!s.lyrics->empty()&&s.expanded&&!s.live&&!s.card&&!s.command.active&&s.page==Page::Media;
}
void Renderer::ensureNowPlaying(){
    if(lyricsLayer_)return;
    for(auto* v:{std::addressof(lyricsLayer_),std::addressof(bubble_),std::addressof(skipBadge_)})check(device_->CreateVisual(v->GetAddressOf()));
    for(auto pair:{std::pair{std::addressof(lyricsEffect_),lyricsLayer_.Get()},std::pair{std::addressof(bubbleEffect_),bubble_.Get()},std::pair{std::addressof(skipEffect_),skipBadge_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));pair.second->SetEffect(pair.first->Get());pair.first->Get()->SetOpacity(0.f);}
    check(content_->AddVisual(lyricsLayer_.Get(),FALSE,nullptr));check(timeline_->AddVisual(bubble_.Get(),FALSE,nullptr));check(body_->AddVisual(skipBadge_.Get(),FALSE,nullptr));
    check(device_->CreateScaleTransform(&skipScale_));skipScale_->SetCenterX(36*scale_);skipScale_->SetCenterY(16*scale_);skipBadge_->SetTransform(skipScale_.Get());
}
void Renderer::wrappedText(ID2D1RenderTarget* rt,const std::wstring& value,float x,float y,float w,float h,float size,UINT32 color,DWRITE_FONT_WEIGHT weight,UINT32 glow,int lines){
    auto layoutAt=[&](float px){ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(px),nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,px,L"en-us",&f));
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_WORD,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
        ComPtr<IDWriteTextLayout> layout;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),w,h,&layout));return layout;};
    auto layout=layoutAt(size);DWRITE_TEXT_METRICS m{};if(SUCCEEDED(layout->GetMetrics(&m))&&int(m.lineCount)>lines)layout=layoutAt(size-2);
    const D2D1_POINT_2F at{std::round(x*scale_)/scale_,std::round(y*scale_)/scale_};
    if(glow){ComPtr<ID2D1SolidColorBrush> g;check(rt->CreateSolidColorBrush(D2D1::ColorF(glow,.22f),&g));for(auto [dx,dy]:{std::pair{-1.2f,0.f},std::pair{1.2f,0.f},std::pair{0.f,-1.2f},std::pair{0.f,1.2f},std::pair{-.8f,-.8f},std::pair{.8f,.8f},std::pair{-.8f,.8f},std::pair{.8f,-.8f}})rt->DrawTextLayout({at.x+dx,at.y+dy},layout.Get(),g.Get());}
    if(haloAlpha_>0){ComPtr<ID2D1SolidColorBrush> halo;check(rt->CreateSolidColorBrush(D2D1::ColorF(haloColor_,haloAlpha_),&halo));const float d=.6f/scale_;for(auto [dx,dy]:{std::pair{-d,0.f},std::pair{d,0.f},std::pair{0.f,-d},std::pair{0.f,d}})rt->DrawTextLayout({at.x+dx,at.y+dy},layout.Get(),halo.Get());}
    ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(color),&b));rt->DrawTextLayout(at,layout.Get(),b.Get());
}
// The line being sung, the one before and the one after. A new line rises into place.
void Renderer::updateLyrics(const ContentSnapshot& s,UINT32 ink,UINT32 muted,UINT32 accent){
    ensureNowPlaying();const double now=seconds();
    if(!lyricsPanel(s)){if(!lyricsKey_.empty()){lyricsKey_.clear();lyricsFade_.reset(0,now);lyricsEffect_->SetOpacity(0.f);lyricsLayer_->SetContent(nullptr);}return;}
    const bool video=s.settings.mediaLayout==2||(s.settings.mediaLayout==0&&s.playback.kind==MediaKind::Video);const float tx=video?166.f:118.f,tw=380-tx;
    const auto& lines=*s.lyrics;const int i=std::clamp(s.lyricLine,-1,int(lines.size())-1);
    auto at=[&](int k)->std::wstring{if(k<0||k>=int(lines.size()))return L"";return lines[size_t(k)].text.empty()?std::wstring(L"♪"):lines[size_t(k)].text;};
    const std::wstring before=i>=1?at(i-1):L"",current=i>=0?at(i):L"♪",after=at(i+1);
    const std::wstring key=before+L"\x1f"+current+L"\x1f"+after+L"\x1f"+std::to_wstring(ink)+L"."+std::to_wstring(accent)+L"."+std::to_wstring(int(tw))+L"."+std::to_wstring(int(haloAlpha_*100));
    lyricsLayer_->SetOffsetX(std::round(tx*scale_));
    if(key==lyricsKey_)return;const bool first=lyricsKey_.empty();const bool sameLine=!first&&lyricsKey_.substr(0,lyricsKey_.find(L'\x1f',lyricsKey_.find(L'\x1f')+1))==before+L"\x1f"+current;lyricsKey_=key;
    surface(lyricsSurface_,int(tw),88,[&](auto* rt){
        text(rt,before,0,0,tw,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,18);
        // A soft accent glow on dark islands; on light ones it would read as a smudge.
        wrappedText(rt,current,0,20,tw,46,15,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,s.light?0:accent);
        text(rt,after,0,68,tw,10.5f,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,18);});
    lyricsLayer_->SetContent(lyricsSurface_.Get());
    if(first||sameLine||s.reducedMotion){lyricsRise_.reset(0,now);lyricsFade_.reset(1,now);}
    else{lyricsRise_.reset(9,now);lyricsRise_.retarget(0,now,MotionTokens::content);lyricsFade_.reset(.3,now);lyricsFade_.retarget(1,now,{1,260,32});}
    auto y=animation(lyricsRise_,now,scale_,40*scale_),o=animation(lyricsFade_,now);lyricsLayer_->SetOffsetY(y.Get());lyricsEffect_->SetOpacity(o.Get());
}
// Time under the pointer (or the scrub position), with the lyric sung there when known.
void Renderer::updateBubble(const ContentSnapshot& s){
    ensureNowPlaying();const double now=seconds();const auto& p=s.playback;
    const bool timeline=s.expanded&&!s.live&&!s.card&&s.page==Page::Media&&p.duration>0&&p.canSeek;
    const bool visible=timeline&&(s.scrub.active||s.seekHover>=0);
    if(visible){
        const double time=s.scrub.active?s.scrub.value:std::clamp(double(s.seekHover)/380.,0.,1.)*p.duration;
        std::wstring label=clockText(time),line;
        if(s.settings.lyrics&&s.lyrics&&!s.lyrics->empty()){int k=lyricIndex(*s.lyrics,time);if(k>=0&&!(*s.lyrics)[size_t(k)].text.empty())line=(*s.lyrics)[size_t(k)].text;}
        const UINT32 ink=bubbleColors_[0],muted=bubbleColors_[1],fill=bubbleColors_[2];
        const float timeWidth=measure(label,10.5f,DWRITE_FONT_WEIGHT_SEMI_BOLD),lineWidth=line.empty()?0.f:std::min(150.f,measure(line,10)+2);
        const float w=std::ceil(timeWidth+20+(line.empty()?0:lineWidth+10));
        const std::wstring key=label+L"\x1f"+line+L"\x1f"+std::to_wstring(ink)+std::to_wstring(fill);
        if(key!=bubbleKey_){bubbleKey_=key;surface(bubbleSurface_,int(w)+2,26,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(fill,.97f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({1,1,w+1,25},12,12),b.Get());
            b->SetColor(D2D1::ColorF(ink,.12f));rt->DrawRoundedRectangle(D2D1::RoundedRect({1.5f,1.5f,w+.5f,24.5f},11.5f,11.5f),b.Get(),1/scale_);
            text(rt,label,11,1,timeWidth+2,10.5f,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_LEADING,24);if(!line.empty())text(rt,line,11+timeWidth+10,1,lineWidth,10,muted,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_LEADING,24);});
            bubble_->SetContent(bubbleSurface_.Get());}
        const float x=float(time/p.duration*380);bubble_->SetOffsetX(std::round(std::clamp(x-(w+2)/2,-6.f,386.f-(w+2))*scale_));bubble_->SetOffsetY(std::round(-44*scale_));
    }
    const double target=visible?1:0;if(std::abs(bubbleOpacity_.target()-target)>.001){if(s.reducedMotion)bubbleOpacity_.reset(target,now);else bubbleOpacity_.retarget(target,now,visible?SpringSpec{1,520,44}:SpringSpec{1,700,60});auto o=animation(bubbleOpacity_,now);bubbleEffect_->SetOpacity(o.Get());}
}
void Renderer::skipFeedback(bool forward,float x,float y,bool reduced){
    ensureNowPlaying();const UINT32 fill=bubbleColors_[2],ink=bubbleColors_[0];
    surface(skipSurface_,72,32,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(fill,.94f),&b);rt->FillRoundedRectangle(D2D1::RoundedRect({0,0,72,32},16,16),b.Get());
        drawIcon(rt,d2d_.Get(),forward?Icon::ArrowRight:Icon::ArrowLeft,forward?48.f:10.f,8,16,ink);text(rt,L"10 s",forward?12.f:28.f,0,34,11,ink,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER,32);});
    skipBadge_->SetContent(skipSurface_.Get());skipBadge_->SetOffsetX(std::round((x-36)*scale_));skipBadge_->SetOffsetY(std::round((y-16)*scale_));
    const double now=seconds();Spring zoom{.8},fade{1};zoom.reset(reduced?1:.8,now);fade.reset(1,now);if(!reduced)zoom.retarget(1,now,MotionTokens::icon);fade.retarget(0,now,{1,26,11});
    auto z=animation(zoom,now),o=animation(fade,now);skipScale_->SetScaleX(z.Get());skipScale_->SetScaleY(z.Get());skipEffect_->SetOpacity(o.Get());commit();
}
}
