#pragma once
#include "Common/Win32.h"
#include "Animation/MotionEngine.h"
#include <d3d11.h>
#include <dcomp.h>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include "Design/Icons.h"
#include "Design/Type.h"
#include "Design/Layout.h"
#include "Animation/ArtworkHandoff.h"
#include "Media/MediaProvider.h"
#include "Hardware/SystemProvider.h"
#include "Persistence/Settings.h"
#include "FileShelf/FileShelf.h"
#include "Audio/AudioProvider.h"
#include "Audio/SessionMixer.h"
#include "Audio/LoopbackAnalyzer.h"
#include "Interaction/DetailModels.h"
#include "Composition/GlassBackdrop.h"
#include "Hardware/BatteryModel.h"
#include "Hardware/BluetoothProvider.h"
#include "Hardware/Platform.h"
#include "Design/BrandDraw.h"

namespace nexus {
struct ContentSnapshot {
    Page page=Page::Overview;bool expanded=false,live=false,dropHover=false,light=false,blur=true;int layoutSlot=0;bool reducedMotion=false;int settingsPage=0,shelfOffset=0,audioOffset=0;std::wstring activity;std::vector<ShelfItem> shelf;std::vector<AudioDevice> outputs;std::wstring feedback;Action hovered=Action::None;bool pinned=false;
    MediaSnapshot playback;SystemSnapshot system;Settings settings;FocusClock focus;ScrubGesture scrub;
    std::wstring headline=L"Your space, in rhythm.";
    std::wstring detail=L"A quieter home for the things happening now.";
    std::wstring media=L"No media session";
    std::wstring artist=L"Play something in a compatible player";
    int battery=-1,volume=0; bool charging=false,muted=false;
    std::vector<MediaSnapshot> sessions;int session=0;std::vector<MixerEntry> mixer;int audioTab=0,mixerOffset=0;
    int hud=0,brightness=-1;bool waveform=false;
    // Phase 3: power, devices and the notification card (kind 1 connected, 2 disconnected, 3 charging, 4 unplugged).
    bool card=false;int statsTab=0,deviceOffset=0,powerMode=-1,toFull=-1,remaining=-1;BatteryReading power;std::vector<float> history;
    std::vector<BluetoothDevice> devices;std::wstring deviceFeedback;PlatformInfo platform;struct Notice{int kind=0;BluetoothDevice device;} notice;
};

class Renderer {
    ComPtr<ID3D11Device> d3d_;
    ComPtr<IDXGIDevice> dxgi_;
    ComPtr<IDCompositionDevice> device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> write_;ComPtr<IDWriteRenderingParams> textParams_;
    ComPtr<IDCompositionVisual> stage_,root_,body_,inner_,header_,content_,bar_,art_,wingLeft_,wingRight_,pulseVisual_,hoverVisual_;
    ComPtr<IDCompositionRectangleClip> clip_,innerClip_,barClip_,artClip_;
    ComPtr<IDCompositionEffectGroup> contentEffect_,barEffect_,headerEffect_,artEffect_,pulseEffect_,hoverEffect_;
    ComPtr<IDCompositionSurface> baseSurface_,innerSurface_,headerSurface_,contentSurface_,barSurface_,artSurface_,leftSurface_,rightSurface_,pulseSurface_,hoverSurface_;
    ComPtr<IDCompositionScaleTransform> artScale_,leftScale_,rightScale_,hoverScale_;
    int cachedEdge_=-1,edge_=0;bool attached_=true,expanded_=false,live_=false;UINT32 baseColor_=0,accentColor_=0;int material_=-1;GlassBackdrop glass_;std::shared_ptr<const Artwork> artwork_;
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
    // Real-audio bars, level indicator, app badge and mixer meters.
    struct Bar {ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionScaleTransform> scale;Glide glide;};
    std::array<Bar,24> bars_;std::array<Bar,4> meters_;ComPtr<IDCompositionVisual> spectrum_,meterLayer_,hudTrack_,hudFill_,artFrame_,badge_;
    ComPtr<IDCompositionSurface> spectrumSurface_,meterSurface_,hudTrackSurface_,hudFillSurface_,badgeSurface_;ComPtr<IDCompositionEffectGroup> spectrumEffect_,meterEffect_,hudEffect_,badgeEffect_;
    ComPtr<IDCompositionRectangleClip> hudClip_;Spring spectrumOpacity_{0},hudOpacity_{0},badgeOpacity_{0};UINT32 barColor_=0,meterColor_=0;int barMode_=-1,barCount_=0;float barInset_=0;
    std::shared_ptr<const Artwork> badgeIcon_;int badgeService_=-1;bool badgeLight_=false;int meterRows_=0;
    void updateSpectrumLayout(const ContentSnapshot&,UINT32 accent);void updateHud(const ContentSnapshot&,UINT32 accent,UINT32 track);void updateBadge(const ContentSnapshot&,UINT32 bg);
    BrandPainter brands_;ComPtr<IDCompositionVisual> tabPill_,cardIcon_,energy_,cardRing_;ComPtr<IDCompositionSurface> tabSurface_,cardIconSurface_,energySurface_,cardRingSurface_;int cardRingKey_=-1;ComPtr<IDCompositionEffectGroup> stageEffect_,tabEffect_,cardIconEffect_,energyEffect_;
    ComPtr<IDCompositionScaleTransform> cardIconScale_;ComPtr<IDCompositionRotateTransform> energyRotation_;Spring tabX_{0},tabOpacity_{0},cardPop_{1},energySpin_{0},energyGlow_{0};int tabKey_=-1;UINT32 tabColor_=0;int cardKey_=-1;
    void updateTabs(const ContentSnapshot&,float x,float y,int count,int selected,bool visible,UINT32 fill);void updateCard(const ContentSnapshot&,UINT32 track,UINT32 accent,UINT32 raised,UINT32 ink);
    void identity(ID2D1RenderTarget*,const MediaSnapshot&,float x,float y,float size,UINT32 plate);void deviceBadge(ID2D1RenderTarget*,const BluetoothDevice&,float x,float y,float size,UINT32 plate,UINT32 ink);
    void drawPreview(ID2D1RenderTarget*,const Artwork&,float,float,float,float);
    float dpi_=96,scale_=1;
    ComPtr<IDCompositionAnimation> animation(const Spring&,double,float factor=1,float bias=0);
    ComPtr<IDCompositionAnimation> visibility(const MotionEngine&,double,bool compact=false);
    void surface(ComPtr<IDCompositionSurface>&,int,int,std::function<void(ID2D1RenderTarget*)>);
    // Exact physical-pixel surface; drawing coordinates are pixels.
    void pixelSurface(ComPtr<IDCompositionSurface>&,int,int,std::function<void(ID2D1RenderTarget*)>);
    // Shoulders drawn 1:1 for a radius: wingAlong_ px along the edge (plus a 1 px overlap into the body), wingDepth_ px deep.
    void wings(float radius);float wingRadius_=0;int wingAlong_=0,wingDepth_=0;
    void text(ID2D1RenderTarget*,const std::wstring&,float,float,float,float,UINT32,DWRITE_FONT_WEIGHT=DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT=DWRITE_TEXT_ALIGNMENT_LEADING,float height=0);
public:
    static constexpr float canvasWidth=680,canvasHeight=500;
    std::vector<HitTarget> targets;
    Action hit(float x,float y)const;
    unsigned commits=0,redraws=0; bool software=false;
    void initialize(HWND,float);
    void redraw(const ContentSnapshot&,bool debug=false,bool headerOnly=false);
    void animate(const MotionEngine&,double);
    void iconFeedback(Action,bool pressed,bool enabled);
    bool glassAvailable()const{return glass_.available();}GlassStyle glassStyle()const{return glass_.current();}UINT32 accentColor()const{return accentColor_;}
    void spectrum(const SpectrumFrame&);void energize(bool reduced,bool charging);void meters(const std::vector<MixerEntry>&,int offset,bool visible);
    void absorb(const std::shared_ptr<const Artwork>&,float,float,float,float,bool); void routeConfirmed(bool,Action selected=Action::None); void commit(){check(device_->Commit());++commits;}
};
}
