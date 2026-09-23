#pragma once
#include "Common/Win32.h"
#include "Design/Brands.h"
#include "Design/SvgPath.h"
#include <d2d1.h>
#include <map>
#include <string>
namespace nexus {
class D2DSvgSink final:public SvgSink {
    ID2D1GeometrySink* sink_;bool open_=false;float x_=0,y_=0,startX_=0,startY_=0;
    void begin(){if(!open_){sink_->BeginFigure({x_,y_},D2D1_FIGURE_BEGIN_FILLED);open_=true;}}
public:
    explicit D2DSvgSink(ID2D1GeometrySink* s):sink_(s){}
    void move(float x,float y)override{if(open_){sink_->EndFigure(D2D1_FIGURE_END_CLOSED);open_=false;}x_=startX_=x;y_=startY_=y;}
    void line(float x,float y)override{begin();sink_->AddLine({x,y});x_=x;y_=y;}
    void cubic(float a,float b,float c,float d,float x,float y)override{begin();sink_->AddBezier({{a,b},{c,d},{x,y}});x_=x;y_=y;}
    void quad(float a,float b,float x,float y)override{begin();sink_->AddQuadraticBezier({{a,b},{x,y}});x_=x;y_=y;}
    void arc(float rx,float ry,float rotation,bool large,bool sweep,float x,float y)override{
        begin();if(rx<=0||ry<=0){sink_->AddLine({x,y});x_=x;y_=y;return;}
        // SVG scales radii up when they cannot span the endpoints.
        const float r=rotation*3.14159265f/180,c=std::cos(r),s=std::sin(r),dx=(x_-x)/2,dy=(y_-y)/2,px=c*dx+s*dy,py=-s*dx+c*dy;
        float lambda=px*px/(rx*rx)+py*py/(ry*ry);if(lambda>1){float k=std::sqrt(lambda);rx*=k;ry*=k;}
        sink_->AddArc(D2D1::ArcSegment({x,y},{rx,ry},rotation,sweep?D2D1_SWEEP_DIRECTION_CLOCKWISE:D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,large?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL));x_=x;y_=y;}
    void close()override{if(open_){sink_->EndFigure(D2D1_FIGURE_END_CLOSED);open_=false;}x_=startX_;y_=startY_;}
    void finish(){if(open_){sink_->EndFigure(D2D1_FIGURE_END_CLOSED);open_=false;}}
};
// Geometry for a brand mark, cached per factory.
class BrandPainter {
    std::map<std::string,ComPtr<ID2D1PathGeometry>> cache_;
public:
    ID2D1PathGeometry* geometry(ID2D1Factory* factory,const BrandMark& mark){
        auto key=std::string(mark.slug);auto it=cache_.find(key);if(it!=cache_.end())return it->second.Get();
        ComPtr<ID2D1PathGeometry> path;ComPtr<ID2D1GeometrySink> sink;if(FAILED(factory->CreatePathGeometry(&path))||FAILED(path->Open(&sink)))return nullptr;
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);D2DSvgSink adapter(sink.Get());bool ok=SvgPathReader(mark.path).read(adapter);adapter.finish();
        if(FAILED(sink->Close())||!ok)path.Reset();cache_[key]=path;return path.Get();
    }
    // A round tile carrying the mark in its brand colour, like an app badge.
    void tile(ID2D1RenderTarget* rt,ID2D1Factory* factory,const BrandMark& mark,float x,float y,float size,bool plate=true){
        auto* g=geometry(factory,mark);if(!g)return;const uint32_t c=mark.color;const float luminance=(.2126f*((c>>16)&255)+.7152f*((c>>8)&255)+.0722f*(c&255))/255;
        ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(luminance>.86f?0x1c1d22:0xffffff),&brush);
        if(plate)rt->FillEllipse(D2D1::Ellipse({x+size/2,y+size/2},size/2,size/2),brush.Get());
        float glyph=plate?size*.58f:size;D2D1_MATRIX_3X2_F saved;rt->GetTransform(&saved);rt->SetTransform(D2D1::Matrix3x2F::Scale(glyph/24,glyph/24)*D2D1::Matrix3x2F::Translation(x+(size-glyph)/2,y+(size-glyph)/2)*saved);
        brush->SetColor(D2D1::ColorF(c));rt->FillGeometry(g,brush.Get());rt->SetTransform(saved);
    }
};
}
