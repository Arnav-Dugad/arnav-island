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
// An answer (a converted amount) is laid out one character per column. Digits are
// windows onto a strip of 0-9 repeated three times: an answer that appears spins each
// digit a full turn into place, leftmost first; a changed answer rolls only the digits
// that changed. Columns are matched from the right, so the units keep their column.
void Renderer::updateAnswer(const ContentSnapshot& s,UINT32 ink){
    if(!answer_){
        check(device_->CreateVisual(&answer_));check(device_->CreateEffectGroup(&answerEffect_));answer_->SetEffect(answerEffect_.Get());answerEffect_->SetOpacity(0.f);check(content_->AddVisual(answer_.Get(),FALSE,nullptr));
        for(auto& c:answerColumns_){check(device_->CreateVisual(&c.column));check(device_->CreateVisual(&c.strip));check(device_->CreateRectangleClip(&c.clip));c.clip->SetLeft(0.f);c.clip->SetTop(0.f);c.column->SetClip(c.clip.Get());c.column->AddVisual(c.strip.Get(),FALSE,nullptr);answer_->AddVisual(c.column.Get(),FALSE,nullptr);}
    }
    const bool show=answerY_>=0&&!answerText_.empty()&&s.command.active;
    if(!show){if(!answerShown_.empty()){answerShown_.clear();answerEffect_->SetOpacity(0.f);for(auto& c:answerColumns_){c.strip->SetContent(nullptr);c.digit=-1;}}return;}
    constexpr float size=17;const float cell=std::ceil(size*1.34f);
    // Tabular figures: every digit has one advance, so a rolling column never changes width.
    auto layoutOf=[&](const std::wstring& v){ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(size),nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);ComPtr<IDWriteTextLayout> l;check(write_->CreateTextLayout(v.c_str(),UINT32(v.size()),f.Get(),2000,cell,&l));
        ComPtr<IDWriteTypography> typography;if(SUCCEEDED(write_->CreateTypography(&typography))){typography->AddFontFeature({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES,1});l->SetTypography(typography.Get(),{0,UINT32(v.size())});}return l;};
    if(answerInk_!=ink||!answerDigits_){answerInk_=ink;answerGlyphs_.clear();{auto zero=layoutOf(L"0");DWRITE_TEXT_METRICS m{};zero->GetMetrics(&m);answerDigitWidth_=m.widthIncludingTrailingWhitespace;}
        answerDigits_.Reset();surface(answerDigits_,int(std::ceil(answerDigitWidth_))+2,int(cell*30),[&](auto* rt){for(int k=0;k<30;++k){auto l=layoutOf(std::wstring(1,wchar_t(L'0'+k%10)));drawLyric(rt,l.Get(),0,k*cell,ink,1);}});}
    const std::wstring t=answerText_.size()>answerColumns_.size()?answerText_.substr(answerText_.size()-answerColumns_.size()):answerText_;
    // Each column sits exactly where the whole answer, laid out as text, puts that character.
    std::vector<float> xs,ws;{auto whole=layoutOf(t);for(UINT32 i=0;i<t.size();++i){float px=0,py=0;DWRITE_HIT_TEST_METRICS m{};whole->HitTestTextPosition(i,FALSE,&px,&py,&m);xs.push_back(46+px);ws.push_back(m.width);}}
    const bool appearing=answerShown_.empty();const double now=seconds();
    for(size_t j=0;j<answerColumns_.size();++j){auto& col=answerColumns_[j];
        if(j>=t.size()){col.strip->SetContent(nullptr);col.digit=-1;continue;}
        const size_t i=t.size()-1-j;const wchar_t ch=t[i];
        // The clip is two pixels wider than the advance, for the glyph's antialiased edge.
        col.column->SetOffsetX(std::round(xs[i]*scale_));col.column->SetOffsetY(std::round(answerY_*scale_));col.clip->SetRight(std::ceil((ws[i]+2)*scale_));
        // A digit's window is just its own height (figures sit 5.4 to 17.3 DIPs down the 23 DIP cell), so a
        // rolling column shows only the digits passing through it; separators keep the whole cell for the comma's tail.
        const bool digit=ch>=L'0'&&ch<=L'9';col.clip->SetTop(digit?std::floor(4*scale_):0.f);col.clip->SetBottom(digit?std::ceil((cell-4)*scale_):std::ceil(cell*scale_));
        if(digit){const int d=ch-L'0';col.strip->SetContent(answerDigits_.Get());
            // Stiffer springs to the left, so the leading digits settle first.
            const double k=std::min(420.,170.+18.*double(i)),spec=2*std::sqrt(k)*.86;const SpringSpec roll{1,k,spec};
            if(s.reducedMotion)col.roll.reset(10+d,now);
            else if(appearing||col.digit<0){col.roll.reset(d,now);col.roll.retarget(10+d,now,roll);}
            else if(col.digit!=d)col.roll.retarget(10+d,now,roll);
            col.digit=d;auto a=animation(col.roll,now,-cell*scale_,0);col.strip->SetOffsetY(a.Get());}
        else{auto& glyph=answerGlyphs_[ch];if(!glyph)surface(glyph,int(std::ceil(ws[i]))+2,int(cell),[&](auto* rt){auto l=layoutOf(std::wstring(1,ch));drawLyric(rt,l.Get(),0,0,ink,1);});
            col.strip->SetContent(glyph.Get());col.strip->SetOffsetY(0.f);col.digit=-1;}
    }
    answerEffect_->SetOpacity(1.f);answerShown_=t;
}
}
