#include "Renderer.h"
// Phase 5B, Now Playing Pro: synced lyric lines, the seek time bubble and skip feedback.
namespace nexus {
bool Renderer::lyricsPanel(const ContentSnapshot& s){
    return s.settings.lyrics&&s.lyricsView&&s.lyrics&&!s.lyrics->empty()&&s.expanded&&!s.live&&!s.card&&!s.command.active&&s.page==Page::Media;
}
void Renderer::ensureNowPlaying(){
    if(lyricsLayer_)return;
    for(auto* v:{std::addressof(lyricsLayer_),std::addressof(lyricsList_),std::addressof(lyricsLine_),std::addressof(lyricsGhost_),std::addressof(lyricsDots_),std::addressof(lyricsRows_[0]),std::addressof(lyricsRows_[1]),std::addressof(lyricsDot_[0]),std::addressof(lyricsDot_[1]),std::addressof(lyricsDot_[2]),std::addressof(bubble_),std::addressof(skipBadge_)})check(device_->CreateVisual(v->GetAddressOf()));
    for(auto pair:{std::pair{std::addressof(lyricsEffect_),lyricsLayer_.Get()},std::pair{std::addressof(lyricsGhostEffect_),lyricsGhost_.Get()},std::pair{std::addressof(lyricsDotEffect_[0]),lyricsDot_[0].Get()},std::pair{std::addressof(lyricsDotEffect_[1]),lyricsDot_[1].Get()},std::pair{std::addressof(lyricsDotEffect_[2]),lyricsDot_[2].Get()},std::pair{std::addressof(bubbleEffect_),bubble_.Get()},std::pair{std::addressof(skipEffect_),skipBadge_.Get()}}){check(device_->CreateEffectGroup(pair.first->GetAddressOf()));pair.second->SetEffect(pair.first->Get());pair.first->Get()->SetOpacity(0.f);}
    check(content_->AddVisual(lyricsLayer_.Get(),FALSE,nullptr));check(timeline_->AddVisual(bubble_.Get(),FALSE,nullptr));check(body_->AddVisual(skipBadge_.Get(),FALSE,nullptr));
    // Back to front: neighbouring lines, the fading ghost, the sung line (its fill rows and gap dots ride on it).
    for(auto* v:{lyricsList_.Get(),lyricsGhost_.Get(),lyricsLine_.Get()})check(lyricsLayer_->AddVisual(v,FALSE,nullptr));
    for(auto& row:lyricsRows_)check(lyricsLine_->AddVisual(row.Get(),FALSE,nullptr));check(lyricsLine_->AddVisual(lyricsDots_.Get(),FALSE,nullptr));
    for(auto& dot:lyricsDot_)check(lyricsDots_->AddVisual(dot.Get(),FALSE,nullptr));
    check(device_->CreateScaleTransform(&skipScale_));skipScale_->SetCenterX(36*scale_);skipScale_->SetCenterY(16*scale_);skipBadge_->SetTransform(skipScale_.Get());
    for(auto pair:{std::pair{std::addressof(lyricsLineScale_),lyricsLine_.Get()},std::pair{std::addressof(lyricsGhostScale_),lyricsGhost_.Get()},std::pair{std::addressof(lyricsDotsScale_),lyricsDots_.Get()}}){check(device_->CreateScaleTransform(pair.first->GetAddressOf()));check(pair.second->SetTransform(pair.first->Get()));}
    lyricsDotsScale_->SetCenterX(17*scale_);lyricsDotsScale_->SetCenterY(7*scale_);
    check(device_->CreateRectangleClip(&lyricsClip_));lyricsClip_->SetLeft(0.f);lyricsClip_->SetTop(0.f);check(lyricsLayer_->SetClip(lyricsClip_.Get()));
    for(size_t k=0;k<2;++k){check(device_->CreateRectangleClip(&lyricsRowClip_[k]));lyricsRowClip_[k]->SetLeft(0.f);lyricsRowClip_[k]->SetRight(0.f);check(lyricsRows_[k]->SetClip(lyricsRowClip_[k].Get()));}
    for(size_t k=0;k<3;++k){lyricsDot_[k]->SetOffsetX(std::round(k*13*scale_));lyricsDot_[k]->SetOffsetY(std::round(3*scale_));}
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
ComPtr<IDWriteTextLayout> Renderer::lyricLayout(const std::wstring& value,float size,float width,float height,DWRITE_FONT_WEIGHT weight){
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(size),nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    const bool single=height<size*2;f->SetWordWrapping(single?DWRITE_WORD_WRAPPING_NO_WRAP:DWRITE_WORD_WRAPPING_WRAP);
    DWRITE_TRIMMING trim{single?DWRITE_TRIMMING_GRANULARITY_CHARACTER:DWRITE_TRIMMING_GRANULARITY_WORD,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    ComPtr<IDWriteTextLayout> layout;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),width,height,&layout));return layout;
}
void Renderer::drawLyric(ID2D1RenderTarget* rt,IDWriteTextLayout* layout,float x,float y,UINT32 color,float alpha){
    const D2D1_POINT_2F at{std::round(x*scale_)/scale_,std::round(y*scale_)/scale_};
    // Dimmed lines keep most of their halo, so they stay legible over a bright wallpaper.
    if(haloAlpha_>0){ComPtr<ID2D1SolidColorBrush> halo;check(rt->CreateSolidColorBrush(D2D1::ColorF(haloColor_,haloAlpha_*std::min(1.f,alpha*1.8f)),&halo));const float d=.6f/scale_;for(auto [dx,dy]:{std::pair{-d,0.f},std::pair{d,0.f},std::pair{0.f,-d},std::pair{0.f,d}})rt->DrawTextLayout({at.x+dx,at.y+dy},layout,halo.Get());}
    ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(color,alpha),&b));rt->DrawTextLayout(at,layout,b.Get());
}
// The compact island's sung line. A new line rises 9 DIPs into place and fades in while the
// one before it lifts away and fades, inside a clip the size of the label, so one line
// flows into the next instead of blinking.
void Renderer::updateCompactLyric(const std::wstring& line,float x,float w,UINT32 ink,bool reduced){
    if(!compactLyricHost_){check(device_->CreateVisual(&compactLyricHost_));check(device_->CreateRectangleClip(&compactLyricClip_));compactLyricHost_->SetClip(compactLyricClip_.Get());check(header_->AddVisual(compactLyricHost_.Get(),TRUE,nullptr));
        for(auto& l:compactLyric_){check(device_->CreateVisual(&l.visual));check(device_->CreateEffectGroup(&l.effect));l.visual->SetEffect(l.effect.Get());check(compactLyricHost_->AddVisual(l.visual.Get(),FALSE,nullptr));}}
    if(line.empty()){if(!compactLyricShown_.empty()){compactLyricShown_.clear();compactLyricKey_.clear();for(auto& l:compactLyric_)l.visual->SetContent(nullptr);}return;}
    // The clip is the label's box, with a DIP either side for the halo.
    compactLyricHost_->SetOffsetX(std::round((x-1)*scale_));compactLyricClip_->SetLeft(0.f);compactLyricClip_->SetTop(std::floor(3*scale_));compactLyricClip_->SetRight(std::ceil((w+2)*scale_));compactLyricClip_->SetBottom(std::ceil(31*scale_));
    const std::wstring key=line+L"\x1f"+std::to_wstring(ink)+L"."+std::to_wstring(int(w))+L"."+std::to_wstring(int(haloAlpha_*100));
    if(key==compactLyricKey_)return;
    const bool morph=!reduced&&!compactLyricShown_.empty()&&compactLyricShown_!=line;
    if(morph)compactFront_^=1;
    auto& in=compactLyric_[size_t(compactFront_)];auto& out=compactLyric_[size_t(compactFront_^1)];
    in.surface.Reset();surface(in.surface,int(std::ceil(w))+3,34,[&](auto* rt){text(rt,line,1,0,w,11.5f,ink,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_LEADING,34);});in.visual->SetContent(in.surface.Get());
    if(morph){const double t=seconds();auto rise=ease(t,9*scale_,0.f,.36),show=ease(t,0.f,1.f,.3),lift=ease(t,0.f,-9*scale_,.3),hide=ease(t,1.f,0.f,.2);
        in.visual->SetOffsetY(rise.Get());in.effect->SetOpacity(show.Get());out.visual->SetOffsetY(lift.Get());out.effect->SetOpacity(hide.Get());}
    else{in.visual->SetOffsetY(0.f);in.effect->SetOpacity(1.f);out.visual->SetContent(nullptr);}
    compactLyricShown_=line;compactLyricKey_=key;
}
// The sung line sits in the middle of the scroller, earlier lines above it and later ones below.
void Renderer::updateLyrics(const ContentSnapshot& s,UINT32 ink,UINT32 muted,UINT32 accent){
    ensureNowPlaying();const double now=seconds();(void)muted;(void)accent;
    targets.erase(std::remove_if(targets.begin(),targets.end(),[](const HitTarget& t){return int(t.action)>=int(Action::LyricLineBase)&&int(t.action)<int(Action::LyricLineEnd);}),targets.end());
    if(!lyricsPanel(s)){
        if(!lyricsKey_.empty()){lyricsKey_.clear();lyricsTiming_.clear();lyricsPlaced_={};lyricsFade_.reset(0,now);lyricsEffect_->SetOpacity(0.f);lyricsGhostEffect_->SetOpacity(0.f);
            for(auto* v:{lyricsList_.Get(),lyricsLine_.Get(),lyricsGhost_.Get(),lyricsRows_[0].Get(),lyricsRows_[1].Get()})v->SetContent(nullptr);for(auto& e:lyricsDotEffect_)e->SetOpacity(0.f);}
        return;
    }
    const bool video=s.settings.mediaLayout==2||(s.settings.mediaLayout==0&&s.playback.kind==MediaKind::Video);const float tx=video?166.f:118.f,tw=380-tx;
    // 94 DIPs between the title row and the transport; the sung line is centred at 46.
    constexpr float top=28,height=94,centre=46,big=17,small=13,row=18,gap=4,pad=44,lineRoom=52;
    const auto& lines=*s.lyrics;const int n=int(lines.size()),i=std::clamp(s.lyricLine,-1,n-1);
    const bool gapLine=i<0||lines[size_t(i)].text.empty();
    auto words=[&](int k)->std::wstring{if(k<0||k>=n)return L"";return lines[size_t(k)].text.empty()?std::wstring(L"♪"):lines[size_t(k)].text;};
    if(tw!=lyricsWidth_){lyricsWidth_=tw;for(auto* surface:{std::addressof(lyricsSurface_),std::addressof(lyricsLineSurface_),std::addressof(lyricsFillSurface_),std::addressof(lyricsGhostSurface_)})surface->Reset();lyricsKey_.clear();}
    // The sung line wraps to two rows at most, shrinking once before it trims.
    ComPtr<IDWriteTextLayout> layout;float h=14;lyricsRowsLaid_.clear();
    if(!gapLine){
        const auto& value=lines[size_t(i)].text;
        for(float size:{big,15.f}){layout=lyricLayout(value,size,tw,lineRoom,DWRITE_FONT_WEIGHT_BOLD);DWRITE_TEXT_METRICS m{};check(layout->GetMetrics(&m));if(m.lineCount<=2)break;}
        UINT32 count=0;layout->GetLineMetrics(nullptr,0,&count);std::vector<DWRITE_LINE_METRICS> metrics(count);if(count)check(layout->GetLineMetrics(metrics.data(),count,&count));
        float y=0;UINT32 at=0;
        for(UINT32 k=0;k<count&&k<2;++k){const auto& lm=metrics[k];const UINT32 visible=lm.length-std::min(lm.length,lm.trailingWhitespaceLength);float w=0;
            if(visible){UINT32 hits=0;layout->HitTestTextRange(at,visible,0,0,nullptr,0,&hits);std::vector<DWRITE_HIT_TEST_METRICS> hm(hits);if(hits&&SUCCEEDED(layout->HitTestTextRange(at,visible,0,0,hm.data(),hits,&hits)))for(UINT32 r=0;r<hits;++r)w=std::max(w,hm[r].left+hm[r].width);}
            lyricsRowsLaid_.push_back({y,lm.height,std::min(tw,std::ceil(w)+2),at,std::max(1u,visible)});y+=lm.height;at+=lm.length;}
        h=std::max(14.f,y);
    }
    const float current=std::round(centre-h/2),previous=current-gap-row,next=current+h+gap;
    // Any visible line is a place to jump to.
    if(s.playback.canSeek){auto target=[&](int k,float y,float span){if(k<0||k>=n)return;const float y0=std::max(0.f,y),y1=std::min(height,y+span);if(y1-y0<8)return;targets.push_back({Action(int(Action::LyricLineBase)+k-i+2),tx,top+y0,tw,y1-y0});};
        target(i-1,previous,row);if(!gapLine)target(i,current,h);target(i+1,next,row);}
    std::wstring key=std::to_wstring(i)+L"\x1f";for(int k=i-2;k<=i+2;++k)key+=words(k)+L"\x1f";
    key+=std::to_wstring(ink)+L"."+std::to_wstring(int(haloAlpha_*100))+L"."+std::to_wstring(int(tw))+(s.reducedMotion?L".r":L"");
    lyricsLayer_->SetOffsetX(std::round(tx*scale_));lyricsLayer_->SetOffsetY(std::round(top*scale_));lyricsClip_->SetRight(std::ceil(tw*scale_));lyricsClip_->SetBottom(std::round(height*scale_));
    if(key!=lyricsKey_){
        const bool first=lyricsKey_.empty();lyricsKey_=key;lyricsTiming_.clear();
        const auto was=lyricsPlaced_;const bool moved=!first&&was.line!=i,step=moved&&!s.reducedMotion&&(i==was.line+1||i==was.line-1);
        // The line being replaced leaves as a ghost of itself, drawn fully lit.
        const bool ghost=step&&!was.gap&&lyricsFillSurface_;if(ghost)std::swap(lyricsGhostSurface_,lyricsFillSurface_);
        // Neighbours share one layer; upcoming lines are a little brighter than past ones.
        surface(lyricsSurface_,int(std::ceil(tw)),int(height+2*pad),[&](auto* rt){
            const std::pair<int,float> rows[]={{i-2,previous-row-2},{i-1,previous},{i+1,next},{i+2,next+row+2}};
            // See-through glass needs brighter neighbours than a solid island.
            const bool clearGlass=haloAlpha_>0;const float alpha[]={clearGlass?.3f:.16f,clearGlass?.6f:.36f,clearGlass?.78f:.56f,clearGlass?.45f:.28f};
            // At rest a line is shown whole or not at all; only moving lines cross the scroller's edges.
            for(int k=0;k<4;++k){const auto w=words(rows[k].first);if(w.empty()||rows[k].second<0||rows[k].second+row>height)continue;auto l=lyricLayout(w,small,tw,row,DWRITE_FONT_WEIGHT_SEMI_BOLD);drawLyric(rt,l.Get(),0,rows[k].second+pad,ink,alpha[k]);}});
        lyricsList_->SetContent(lyricsSurface_.Get());
        // With reduced motion the sung line is simply lit; otherwise a lit copy is revealed row by row.
        const bool sweep=!s.reducedMotion;
        if(!gapLine){
            surface(lyricsLineSurface_,int(std::ceil(tw)),int(lineRoom),[&](auto* rt){drawLyric(rt,layout.Get(),0,0,ink,sweep?(haloAlpha_>0?.5f:.34f):1.f);});
            surface(lyricsFillSurface_,int(std::ceil(tw)),int(lineRoom),[&](auto* rt){drawLyric(rt,layout.Get(),0,0,ink,1.f);});
            lyricsLine_->SetContent(lyricsLineSurface_.Get());
            for(size_t k=0;k<2;++k){const bool used=sweep&&k<lyricsRowsLaid_.size();lyricsRows_[k]->SetContent(used?lyricsFillSurface_.Get():nullptr);
                if(used){const auto& r=lyricsRowsLaid_[k];lyricsRowClip_[k]->SetTop(std::floor(r.top*scale_));lyricsRowClip_[k]->SetBottom(std::ceil((r.top+r.height)*scale_));}}
            for(auto& e:lyricsDotEffect_)e->SetOpacity(0.f);
        }else{
            lyricsLine_->SetContent(nullptr);for(auto& r:lyricsRows_)r->SetContent(nullptr);
            if(lyricsDotColor_!=ink||!lyricsDotSurface_){lyricsDotColor_=ink;surface(lyricsDotSurface_,8,8,[&](auto* rt){ComPtr<ID2D1SolidColorBrush> b;check(rt->CreateSolidColorBrush(D2D1::ColorF(ink),&b));rt->FillEllipse(D2D1::Ellipse({4,4},4,4),b.Get());});}
            for(auto& d:lyricsDot_)d->SetContent(lyricsDotSurface_.Get());
        }
        // One spring carries everything: the lines travel a row, the new line grows from
        // the neighbours' size, and the ghost shrinks into its new place as it fades.
        const float ratio=small/big;float travel=0;
        if(step){travel=(i==was.line+1?was.next:was.previous)-current;lyricsScroll_.reset(1,now);lyricsScroll_.retarget(0,now,MotionTokens::lyrics);}else lyricsScroll_.reset(0,now);
        auto listY=animation(lyricsScroll_,now,travel*scale_,-pad*scale_);lyricsList_->SetOffsetY(listY.Get());
        auto lineY=animation(lyricsScroll_,now,travel*scale_,current*scale_);lyricsLine_->SetOffsetY(lineY.Get());
        auto grow=animation(lyricsScroll_,now,step?ratio-1:0.f,1);lyricsLineScale_->SetScaleX(grow.Get());lyricsLineScale_->SetScaleY(grow.Get());
        lyricsGhost_->SetContent(ghost?lyricsGhostSurface_.Get():nullptr);
        if(ghost){const float to=i==was.line+1?previous:next;auto gy=animation(lyricsScroll_,now,(was.current-to)*scale_,to*scale_),gs=animation(lyricsScroll_,now,1-ratio,ratio),go=animation(lyricsScroll_,now);
            lyricsGhost_->SetOffsetY(gy.Get());lyricsGhostScale_->SetScaleX(gs.Get());lyricsGhostScale_->SetScaleY(gs.Get());lyricsGhostEffect_->SetOpacity(go.Get());}
        else lyricsGhostEffect_->SetOpacity(0.f);
        lyricsPlaced_={i,gapLine,current,h,previous,next};
        // The scroller fades in when it appears, and dips briefly after a jump.
        if(first||(moved&&!step)){if(s.reducedMotion)lyricsFade_.reset(1,now);else{lyricsFade_.reset(first?0:.25,now);lyricsFade_.retarget(1,now,{1,260,32});}auto o=animation(lyricsFade_,now);lyricsEffect_->SetOpacity(o.Get());}
    }
    timeLyrics(s,now);
}
// The fill and the gap dots are timed against the song, so the compositor runs them
// between updates; while paused they hold where they are.
void Renderer::timeLyrics(const ContentSnapshot& s,double now){
    const auto& p=s.playback;const auto& lines=*s.lyrics;const int n=int(lines.size()),i=lyricsPlaced_.line;
    wchar_t stamp[96];swprintf(stamp,96,L"%d.%d.%.3f.%.3f.",i,int(p.playing),p.position,p.sampledAt);const std::wstring key=stamp+lyricsKey_;
    if(key==lyricsTiming_)return;lyricsTiming_=key;
    const double position=p.position+(p.playing?std::max(0.,now-p.sampledAt):0.);
    // `from` until a, then linear to `to` at b (seconds from now).
    auto ramp=[&](double a,double b,float from,float to){ComPtr<IDCompositionAnimation> an;check(device_->CreateAnimation(&an));check(an->SetAbsoluteBeginTime(ticks(now)));
        if(!p.playing||b<=0||a>=b){const float v=b<=0?to:a>=0?from:from+(to-from)*float(-a/(b-a));check(an->AddCubic(0,v,0,0,0));check(an->End(0,v));return an;}
        const float slope=float((to-from)/(b-a));
        if(a>0){check(an->AddCubic(0,from,0,0,0));check(an->AddCubic(a,from,slope,0,0));}else check(an->AddCubic(0,from+slope*float(-a),slope,0,0));
        check(an->End(b,to));return an;};
    if(!lyricsPlaced_.gap&&i>=0&&i<n&&!lyricsRowsLaid_.empty()&&!s.reducedMotion){
        // Lyrics are timed per line, so the fill moves at a singing pace (about fourteen
        // characters a second) and is always complete before the next line begins.
        const double start=lines[size_t(i)].time,next=i+1<n?lines[size_t(i+1)].time:start+6;
        UINT32 total=0;for(auto& r:lyricsRowsLaid_)total+=r.length;
        const double span=std::min(std::max(.8,next-start),std::clamp(total*.07+.35,.8,8.));
        UINT32 before=0;
        for(size_t k=0;k<lyricsRowsLaid_.size()&&k<2;++k){const auto& r=lyricsRowsLaid_[k];
            const double a=start+span*before/total-position,b=start+span*(before+r.length)/total-position;before+=r.length;
            auto right=ramp(a,b,0,std::ceil(r.width*scale_));lyricsRowClip_[k]->SetRight(right.Get());}
    }
    if(lyricsPlaced_.gap){
        // Three dots light one after another across the gap (the intro counts as one).
        const double start=i<0?0.:lines[size_t(i)].time,end=i+1<n?lines[size_t(i+1)].time:start+4,span=std::max(.3,(end-start)*.9);
        for(int k=0;k<3;++k){auto o=ramp(start+span*k/3-position,start+span*(k+1)/3-position,.28f,1);lyricsDotEffect_[k]->SetOpacity(o.Get());}
        if(s.reducedMotion){lyricsDotsScale_->SetScaleX(1.f);lyricsDotsScale_->SetScaleY(1.f);}
        else{auto g=ramp(start-position,start+span-position,1,1.12f);lyricsDotsScale_->SetScaleX(g.Get());lyricsDotsScale_->SetScaleY(g.Get());}
    }
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
