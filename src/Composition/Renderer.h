#pragma once
#include "Common/Win32.h"
#include "Animation/MotionEngine.h"
#include <d3d11.h>
#include <dcomp.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>

namespace nexus {
struct ContentSnapshot {
    std::wstring headline=L"Your space, in rhythm.";
    std::wstring detail=L"A quieter home for the things happening now.";
    std::wstring media=L"No media session";
    std::wstring artist=L"Play something in a compatible player";
    int battery=-1,volume=0; bool charging=false,muted=false;
};
struct GlassMaterial { D2D1_COLOR_F base=D2D1::ColorF(0x080a0f); bool nativeBackdrop=false; };
class Renderer {
    ComPtr<ID3D11Device> d3d_;
    ComPtr<IDXGIDevice> dxgi_;
    ComPtr<IDCompositionDevice> device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> write_;
    ComPtr<IDCompositionVisual> root_,body_,inner_,header_,content_,bar_;
    ComPtr<IDCompositionRectangleClip> clip_,innerClip_,barClip_;
    ComPtr<IDCompositionEffectGroup> contentEffect_;
    ComPtr<IDCompositionSurface> baseSurface_,innerSurface_,headerSurface_,contentSurface_,barSurface_;
    float dpi_=96,scale_=1;
    ComPtr<IDCompositionAnimation> animation(const Spring&,double,float factor=1,float bias=0);
    void surface(ComPtr<IDCompositionSurface>&,int,int,std::function<void(ID2D1RenderTarget*)>);
    void text(ID2D1RenderTarget*,const std::wstring&,float,float,float,float,UINT32,DWRITE_FONT_WEIGHT=DWRITE_FONT_WEIGHT_NORMAL);
public:
    static constexpr float canvasWidth=760,canvasHeight=480;
    unsigned commits=0,redraws=0; bool software=false;
    void initialize(HWND,float);
    void redraw(const ContentSnapshot&,bool debug=false);
    void animate(const MotionEngine&,double);
    void commit(){check(device_->Commit());++commits;}
};
}
