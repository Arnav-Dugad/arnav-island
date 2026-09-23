#pragma once
#include "Common/Win32.h"
#include "Animation/MotionEngine.h"
#include <d3d11.h>
#include <dcomp.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include "Design/Icons.h"
#include "Design/Layout.h"
#include "Animation/ArtworkHandoff.h"
#include "Media/MediaProvider.h"
#include "Hardware/SystemProvider.h"
#include "Persistence/Settings.h"
#include "FileShelf/FileShelf.h"
#include "Audio/AudioProvider.h"
#include "Interaction/DetailModels.h"

namespace nexus {
struct ContentSnapshot {
    Page page=Page::Overview;bool expanded=false,live=false,dropHover=false,glassActive=false,light=false;int layoutSlot=0;bool reducedMotion=false;int settingsPage=0,shelfOffset=0,audioOffset=0;std::wstring activity;std::vector<ShelfItem> shelf;std::vector<AudioDevice> outputs;std::wstring feedback;Action hovered=Action::None;bool pinned=false;
    MediaSnapshot playback;SystemSnapshot system;Settings settings;FocusClock focus;ScrubGesture scrub;
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
    int cachedEdge_=-1,edge_=0;bool attached_=true,expanded_=false,live_=false;UINT32 baseColor_=0,accentColor_=0;bool material_=false;std::shared_ptr<const Artwork> artwork_;
        struct IconVisual {ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionSurface> surface;ComPtr<IDCompositionScaleTransform> scale;ComPtr<IDCompositionEffectGroup> effect;Spring x{0},y{0},lift{0},zoom{1};Action action=Action::None;int key=-1;float drawnSize=0,baseY=0;bool used=false;};
    struct IconRequest{Action action;Icon glyph;float x,y,size;UINT32 color;int slot;};std::vector<IconRequest> iconRequests_;bool drawingContent_=false;
    std::array<IconVisual,64> icons_;size_t iconCursor_=7;bool iconMotion_=true;Action hoverAction_=Action::None;bool pressing_=false;
    ComPtr<IDCompositionVisual> iconLayer_,artFrom_,artTo_,nav_,rings_,batteryDot_,timerDot_;
    ComPtr<IDCompositionSurface> artFromSurface_,artToSurface_,navSurface_,ringsSurface_,dotSurface_;
    ComPtr<IDCompositionEffectGroup> iconEffect_,incomingEffect_,navEffect_,ringsEffect_,batteryDotEffect_,timerDotEffect_;
    ComPtr<IDCompositionRotateTransform> batteryRotation_,timerRotation_;
    ArtworkHandoff handoff_;Spring navX{4},batteryAngle{0},timerAngle{0};bool ringsEnabled_=true;int ringCount_=2;double ringBattery_=-2,ringTimer_=-2;int ringFlags_=-1;UINT32 ringColor_=0,ringTrack_=0;
    void icon(Action,Icon,float,float,float,UINT32,int stableSlot=-1);
    void updateArtwork(const ContentSnapshot&,UINT32);
    void updateAtmosphere(const ContentSnapshot&);void updatePeek(const ContentSnapshot&);
    std::array<ComPtr<IDCompositionVisual>,3> atmosphere_;std::array<ComPtr<IDCompositionEffectGroup>,3> atmosphereEffect_;std::array<ComPtr<IDCompositionSurface>,3> atmosphereSurface_;std::array<Spring,3> atmosphereColor_;
    ComPtr<IDCompositionVisual> peek_;ComPtr<IDCompositionSurface> peekSurface_;ComPtr<IDCompositionScaleTransform> peekScale_;ComPtr<IDCompositionEffectGroup> peekEffect_;Spring peekZoom_{.85},peekOpacity_{0};std::shared_ptr<const Artwork> peekArtwork_;
    void updateRings(const ContentSnapshot&,UINT32,UINT32,UINT32);
    ComPtr<IDCompositionVisual> timeline_,seekTrack_,seekFill_,seekThumb_,dropGhost_;
    ComPtr<IDCompositionSurface> seekTrackSurface_,seekFillSurface_,seekThumbSurface_,dropSurface_;
    ComPtr<IDCompositionScaleTransform> seekTrackScale_,seekFillScale_,seekThumbScale_,dropScale_;
    ComPtr<IDCompositionEffectGroup> timelineEffect_,dropEffect_;
    Spring seekEmphasis_{1};UINT32 seekColor_=0;bool seeking_=false;
    void updateTimeline(const ContentSnapshot&,UINT32,UINT32);
    void drawPreview(ID2D1RenderTarget*,const Artwork&,float,float,float,float);
    float dpi_=96,scale_=1;
    ComPtr<IDCompositionAnimation> animation(const Spring&,double,float factor=1,float bias=0);
    ComPtr<IDCompositionAnimation> visibility(const MotionEngine&,double,bool compact=false);
    void surface(ComPtr<IDCompositionSurface>&,int,int,std::function<void(ID2D1RenderTarget*)>);
    void text(ID2D1RenderTarget*,const std::wstring&,float,float,float,float,UINT32,DWRITE_FONT_WEIGHT=DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT=DWRITE_TEXT_ALIGNMENT_LEADING,float height=0);
public:
    static constexpr float canvasWidth=600,canvasHeight=500;
    std::vector<HitTarget> targets;
    Action hit(float x,float y)const;
    unsigned commits=0,redraws=0; bool software=false;
    void initialize(HWND,float);
    void redraw(const ContentSnapshot&,bool debug=false,bool headerOnly=false);
    void animate(const MotionEngine&,double);
    void iconFeedback(Action,bool pressed,bool enabled);
    void absorb(const std::shared_ptr<const Artwork>&,float,float,float,float,bool); void routeConfirmed(bool,Action selected=Action::None); void commit(){check(device_->Commit());++commits;}
};
}
