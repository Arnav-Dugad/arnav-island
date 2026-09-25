#include "Renderer.h"
// Phase 5D, command bar v2: highlighted matches and answers that roll into place.
namespace nexus {
// A result title with the letters that matched drawn bolder in the accent colour.
void Renderer::markedText(ID2D1RenderTarget* rt,const std::wstring& value,const MatchMarks& marks,float x,float y,float w,float size,UINT32 color,UINT32 highlight){
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(size),nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    ComPtr<IDWriteTextLayout> layout;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),w,size*1.6f,&layout));
    ComPtr<ID2D1SolidColorBrush> ink,accent;check(rt->CreateSolidColorBrush(D2D1::ColorF(color),&ink));check(rt->CreateSolidColorBrush(D2D1::ColorF(highlight),&accent));
    for(auto [start,length]:marks){if(size_t(start)+length>value.size())continue;const DWRITE_TEXT_RANGE range{start,length};layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD,range);layout->SetDrawingEffect(accent.Get(),range);}
    const D2D1_POINT_2F at{std::round(x*scale_)/scale_,std::round(y*scale_)/scale_};
    if(haloAlpha_>0){ComPtr<ID2D1SolidColorBrush> halo;check(rt->CreateSolidColorBrush(D2D1::ColorF(haloColor_,haloAlpha_),&halo));
        // The halo pass draws every glyph in the halo colour, so it needs a copy without the accent effect.
        ComPtr<IDWriteTextLayout> plain;check(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),f.Get(),w,size*1.6f,&plain));for(auto [start,length]:marks)if(size_t(start)+length<=value.size())plain->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD,{start,length});
        const float d=.6f/scale_;for(auto [dx,dy]:{std::pair{-d,0.f},std::pair{d,0.f},std::pair{0.f,-d},std::pair{0.f,d}})rt->DrawTextLayout({at.x+dx,at.y+dy},plain.Get(),halo.Get());}
    rt->DrawTextLayout(at,layout.Get(),ink.Get());
}
// Rolling numbers. Each character is a column: digits are windows onto a strip of tabular
// 0-9 repeated three times, other characters are fixed glyphs. A changed number rolls only
// the digits that changed, each the short way round (9 to 0 rolls one step, as a counter
// would); an answer that appears (spin) turns every digit a full turn, leftmost first.
// Columns are matched from the right, so the units keep their column.
void Renderer::placeOdometers(const std::vector<OdometerSpot>& spots,std::initializer_list<int> ids,IDCompositionVisual* parent,bool reduced){
    for(int id:ids){const OdometerSpot* spot=nullptr;for(auto& p:spots)if(p.id==id)spot=&p;odometer(odometers_[size_t(id)],parent,spot,reduced);}
}
void Renderer::odometer(Odometer& o,IDCompositionVisual* parent,const OdometerSpot* spot,bool reduced){
    constexpr size_t columns=14;
    if(!o.root){check(device_->CreateVisual(&o.root));check(device_->CreateEffectGroup(&o.effect));o.root->SetEffect(o.effect.Get());
        // Hard borders: with soft ones, a strip moving under its clip could leave a one-pixel sliver of a digit on screen.
        o.root->SetBorderMode(DCOMPOSITION_BORDER_MODE_HARD);check(parent->AddVisual(o.root.Get(),FALSE,nullptr));o.columns.resize(columns);
        for(auto& c:o.columns){check(device_->CreateVisual(&c.column));check(device_->CreateVisual(&c.strip));check(device_->CreateVisual(&c.blur));check(device_->CreateRectangleClip(&c.clip));c.clip->SetLeft(0.f);c.clip->SetTop(0.f);c.column->SetClip(c.clip.Get());
            check(device_->CreateEffectGroup(&c.sharpFade));check(device_->CreateEffectGroup(&c.blurFade));c.strip->SetEffect(c.sharpFade.Get());c.blur->SetEffect(c.blurFade.Get());c.blurFade->SetOpacity(0.f);
            check(device_->CreateScaleTransform(&c.stretch));c.column->SetTransform(c.stretch.Get());c.column->AddVisual(c.strip.Get(),FALSE,nullptr);c.column->AddVisual(c.blur.Get(),FALSE,nullptr);o.root->AddVisual(c.column.Get(),FALSE,nullptr);}}
    if(!spot||spot->text.empty()){if(!o.shown.empty()){o.shown.clear();for(auto& c:o.columns){c.strip->SetContent(nullptr);c.blur->SetContent(nullptr);c.digit=-1;}}return;}
    const std::wstring t=spot->text.size()>columns?spot->text.substr(spot->text.size()-columns):spot->text;
    // Adaptive text (compact header): figures in two inks, each column taking the one for what is behind it.
    const bool adaptive=adapt_&&parent==header_.Get()&&(spot->ink==adaptInk_||spot->ink==adaptMuted_);const UINT32 inkA=adaptive?adaptVariant(spot->ink,false):spot->ink,inkB=adaptive?adaptVariant(spot->ink,true):0;
    const bool restyled=o.ink!=inkA||o.altInk!=inkB||o.size!=spot->size||o.weight!=int(spot->weight)||o.halo!=haloAlpha_||!o.digits;
    // Nothing moved or changed: the columns already show it.
    if(!restyled&&t==o.shown&&o.x==spot->x&&o.y==spot->y&&o.box==spot->box&&o.trailing==spot->trailing&&(!adaptive||o.serial==adaptSerial_))return;
    o.serial=adaptSerial_;
    const float size=spot->size;
    // Tabular figures: every digit has one advance, so a rolling column never changes width.
    auto layoutOf=[&](const std::wstring& v){ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(size),nullptr,spot->weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);ComPtr<IDWriteTextLayout> l;check(write_->CreateTextLayout(v.c_str(),UINT32(v.size()),f.Get(),2000,size*2,&l));
        ComPtr<IDWriteTypography> typography;if(SUCCEEDED(write_->CreateTypography(&typography))){typography->AddFontFeature({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES,1});l->SetTypography(typography.Get(),{0,UINT32(v.size())});}return l;};
    if(restyled){o.ink=inkA;o.altInk=inkB;o.size=size;o.weight=int(spot->weight);o.halo=haloAlpha_;o.glyphs.clear();o.altGlyphs.clear();
        // The cell is the font's line height rounded up to whole pixels, so a digit at rest sits on the pixel grid.
        float width=0;{auto zero=layoutOf(L"0");DWRITE_TEXT_METRICS m{};zero->GetMetrics(&m);width=m.widthIncludingTrailingWhitespace;o.line=m.height;}o.cell=std::ceil(o.line*scale_)/scale_;
        o.digits.Reset();surface(o.digits,int(std::ceil(width))+3,int(std::ceil(o.cell*30)),[&](auto* rt){for(int k=0;k<30;++k){auto l=layoutOf(std::wstring(1,wchar_t(L'0'+k%10)));drawLyric(rt,l.Get(),1,k*o.cell,o.ink,1);}});
        // The smeared strip: each figure drawn seven times along the roll, faint, so together they read as motion blur.
        auto strips=[&](ComPtr<IDCompositionSurface>& sharp,ComPtr<IDCompositionSurface>& smear,UINT32 ink){
            sharp.Reset();surface(sharp,int(std::ceil(width))+3,int(std::ceil(o.cell*30)),[&](auto* rt){for(int k=0;k<30;++k){auto l=layoutOf(std::wstring(1,wchar_t(L'0'+k%10)));drawLyric(rt,l.Get(),1,k*o.cell,ink,1);}});
            smear.Reset();surface(smear,int(std::ceil(width))+3,int(std::ceil(o.cell*30)),[&](auto* rt){for(int k=0;k<30;++k){auto l=layoutOf(std::wstring(1,wchar_t(L'0'+k%10)));for(float f:{-.18f,-.12f,-.06f,0.f,.06f,.12f,.18f})drawLyric(rt,l.Get(),1,(k+f)*o.cell,ink,.2f);}});};
        o.blurDigits.Reset();surface(o.blurDigits,int(std::ceil(width))+3,int(std::ceil(o.cell*30)),[&](auto* rt){for(int k=0;k<30;++k){auto l=layoutOf(std::wstring(1,wchar_t(L'0'+k%10)));for(float f:{-.18f,-.12f,-.06f,0.f,.06f,.12f,.18f})drawLyric(rt,l.Get(),1,(k+f)*o.cell,o.ink,.2f);}});
        if(adaptive)strips(o.altDigits,o.altBlur,o.altInk);else{o.altDigits.Reset();o.altBlur.Reset();}
        for(auto& c:o.columns)c.digit=-1;}
    // Each column sits exactly where the whole text, laid out as text, puts that character.
    std::vector<float> xs,ws;float total=0;{auto whole=layoutOf(t);DWRITE_TEXT_METRICS m{};whole->GetMetrics(&m);total=m.widthIncludingTrailingWhitespace;
        for(UINT32 i=0;i<t.size();++i){float px=0,py=0;DWRITE_HIT_TEST_METRICS hm{};whole->HitTestTextPosition(i,FALSE,&px,&py,&hm);xs.push_back(px);ws.push_back(hm.width);}}
    // Where text() would put the line: at y, or centred in the box.
    const float left=spot->trailing?spot->x-total:spot->x,top=std::round((spot->box>0?spot->y+(spot->box-o.line)/2:spot->y)*scale_)/scale_;
    const bool appearing=o.shown.empty();const double now=seconds();
    for(size_t j=0;j<o.columns.size();++j){auto& col=o.columns[j];
        if(j>=t.size()){col.strip->SetContent(nullptr);col.blur->SetContent(nullptr);col.digit=-1;continue;}
        const size_t i=t.size()-1-j;const wchar_t ch=t[i];const bool digit=ch>=L'0'&&ch<=L'9';
        // Glyphs are drawn a DIP in from their surface's left edge (room for the halo), and the
        // clip is two DIPs wider than the advance. A digit's window is only as tall as the figures
        // (about 17% to 83% of the line), so a rolling column shows just the digits passing through it.
        col.column->SetOffsetX(std::round((left+xs[i]-1)*scale_));col.column->SetOffsetY(std::round(top*scale_));col.clip->SetRight(std::ceil((ws[i]+2)*scale_));
        col.clip->SetTop(digit?std::floor(o.line*.17f*scale_):0.f);col.clip->SetBottom(digit?std::ceil(o.line*.83f*scale_):std::ceil(o.cell*scale_));
        const bool alt=adaptive&&o.altDigits&&backdropAt(left+xs[i]+ws[i]/2,top+o.line/2)==1;
        if(digit){const int d=ch-L'0';col.strip->SetContent(alt?o.altDigits.Get():o.digits.Get());
            // Stiffer springs to the left, so the leading digits settle first.
            const double k=std::min(420.,170.+18.*double(i)),damping=2*std::sqrt(k)*.86;const SpringSpec roll{1,k,damping};
            if(reduced||(col.digit<0&&!(spot->spin&&appearing)))col.roll.reset(10+d,now);
            else if(col.digit<0){col.roll.reset(d,now);col.roll.retarget(10+d,now,roll);}
            else if(col.digit!=d){
                // The short way round, from wherever the column is now (see digitRoll).
                auto p=col.roll.sample(now);const auto r=digitRoll(p.position,d);col.roll.reset(r.from,now,p.velocity);col.roll.retarget(r.to,now,roll);}
            col.digit=d;auto a=animation(col.roll,now,-o.cell*scale_,0);col.strip->SetOffsetY(a.Get());
            // A big jump blurs and stretches while it is fast: the smear fades in with the roll's speed and out as it lands.
            const auto state=col.roll.sample(now);const bool fast=!reduced&&std::abs(col.roll.target()-state.position)>=2.5;
            col.stretch->SetCenterX(std::round((ws[i]+2)*scale_/2));col.stretch->SetCenterY(std::round(o.line*scale_/2));
            if(fast){auto smear=[&](double t){return std::clamp((std::abs(col.roll.sample(t).velocity)-5)/15,0.,.85);};
                auto curve=[&](double factor,double bias){ComPtr<IDCompositionAnimation> an;check(device_->CreateAnimation(&an));check(an->SetAbsoluteBeginTime(ticks(now)));double duration=0;
                    auto segments=approximateCurve([&](double t){const double h=.001;return PhysicalState{smear(t)*factor+bias,(smear(t+h)-smear(t-h))/(2*h)*factor};},[&](double t){return col.roll.settled(t);},now,duration);
                    for(auto& c:segments)check(an->AddCubic(c.time,float(c.p),float(c.v),float(c.quadratic),float(c.cubic)));check(an->End(duration,float(bias)));return an;};
                col.blur->SetContent(alt?o.altBlur.Get():o.blurDigits.Get());col.blur->SetOffsetY(a.Get());auto blur=curve(1,0),sharp=curve(-.6,1),grow=curve(.1,1);col.blurFade->SetOpacity(blur.Get());col.sharpFade->SetOpacity(sharp.Get());col.stretch->SetScaleY(grow.Get());}
            else{col.blur->SetContent(nullptr);col.blurFade->SetOpacity(0.f);col.sharpFade->SetOpacity(1.f);col.stretch->SetScaleY(1.f);}}
        else{auto& glyph=(alt?o.altGlyphs:o.glyphs)[ch];if(!glyph)surface(glyph,int(std::ceil(ws[i]))+3,int(std::ceil(o.cell)),[&](auto* rt){auto l=layoutOf(std::wstring(1,ch));drawLyric(rt,l.Get(),1,0,alt?o.altInk:o.ink,1);});
            col.strip->SetContent(glyph.Get());col.strip->SetOffsetY(0.f);col.digit=-1;col.blur->SetContent(nullptr);col.blurFade->SetOpacity(0.f);col.sharpFade->SetOpacity(1.f);col.stretch->SetScaleY(1.f);}
    }
    // A number that appears while its band is cascading in joins the cascade.
    if(appearing){if(parent==content_.Get()&&now-cascadeStart_<.4){ComPtr<IDCompositionAnimation> fade,rise;bandCascade(cascadeStart_,size_t(std::clamp(int(top/48),0,5)),fade,rise);o.effect->SetOpacity(fade.Get());o.root->SetOffsetY(rise.Get());}
        else{o.effect->SetOpacity(1.f);o.root->SetOffsetY(0.f);}}
    o.shown=t;o.x=spot->x;o.y=spot->y;o.box=spot->box;o.trailing=spot->trailing;o.top=top;
}
}
