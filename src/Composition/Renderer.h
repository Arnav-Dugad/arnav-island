#pragma once
#include "Common/Win32.h"
#include "Animation/MotionEngine.h"
#include <d3d11.h>
#include <dcomp.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include "Media/MediaProvider.h"
#include "Hardware/SystemProvider.h"
#include "Persistence/Settings.h"
#include "FileShelf/FileShelf.h"
#include "Audio/AudioProvider.h"

namespace nexus {
struct ContentSnapshot {
    Page page=Page::Overview;bool expanded=false,dropHover=false,glassActive=false,light=false;int settingsPage=0,shelfOffset=0,audioOffset=0;std::wstring activity;std::vector<ShelfItem> shelf;std::vector<AudioDevice> outputs;std::wstring feedback;Action hovered=Action::None;bool pinned=false;
    MediaSnapshot playback;SystemSnapshot system;Settings settings;FocusClock focus;
    std::wstring headline=L"Your space, in rhythm.";
    std::wstring detail=L"A quieter home for the things happening now.";
    std::wstring media=L"No media session";
    std::wstring artist=L"Play something in a compatible player";
    int battery=-1,volume=0; bool charging=false,muted=false;
};

class Renderer {
    ComPtr<ID3D11Device> d3d_;
    ComPtr<IDXGIDevice> dxgi_;
    ComPtr<IDCompositionDevice> device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> write_;
    ComPtr<IDCompositionVisual> root_,body_,inner_,header_,content_,bar_,art_,wingLeft_,wingRight_,pulseVisual_,hoverVisual_;
    ComPtr<IDCompositionRectangleClip> clip_,innerClip_,barClip_,artClip_;
    ComPtr<IDCompositionEffectGroup> contentEffect_,barEffect_,headerEffect_,artEffect_,pulseEffect_,hoverEffect_;
    ComPtr<IDCompositionSurface> baseSurface_,innerSurface_,headerSurface_,contentSurface_,barSurface_,artSurface_,leftSurface_,rightSurface_,pulseSurface_,hoverSurface_;
    ComPtr<IDCompositionScaleTransform> artScale_,leftScale_,rightScale_,hoverScale_;
    int cachedEdge_=-1,edge_=0;bool attached_=true,expanded_=false;UINT32 baseColor_=0,accentColor_=0;bool material_=false;std::shared_ptr<const Artwork> artwork_;
    float dpi_=96,scale_=1;
    ComPtr<IDCompositionAnimation> animation(const Spring&,double,float factor=1,float bias=0);
    void surface(ComPtr<IDCompositionSurface>&,int,int,std::function<void(ID2D1RenderTarget*)>);
    void text(ID2D1RenderTarget*,const std::wstring&,float,float,float,float,UINT32,DWRITE_FONT_WEIGHT=DWRITE_FONT_WEIGHT_NORMAL);
public:
    static constexpr float canvasWidth=600,canvasHeight=500;
    std::vector<HitTarget> targets;
    Action hit(float x,float y)const{for(auto& t:targets)if(t.contains(x-20,y-38))return t.action;return Action::None;}
    unsigned commits=0,redraws=0; bool software=false;
    void initialize(HWND,float);
    void redraw(const ContentSnapshot&,bool debug=false,bool headerOnly=false);
    void animate(const MotionEngine&,double);
    void commit(){check(device_->Commit());++commits;}
};
}
