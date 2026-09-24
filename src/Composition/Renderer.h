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
#include "Productivity/Commands.h"
#include "Productivity/Privacy.h"
#include "Productivity/ClipboardModel.h"

#include "Design/Accent.h"
#include "Media/Lyrics.h"
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
    std::vector<BluetoothDevice> devices;std::wstring deviceFeedback;PlatformInfo platform;
    // Notice kinds 5-7: camera, microphone and location use (app and icon below).
    // Kind 8: sound moved to headphones (device = the output, app = the previous output, switchBack offered).
    // Kinds 9-11: a colour was picked, text was copied from the screen, a snip went to the Shelf (detail and colour describe it).
    struct Notice{int kind=0;BluetoothDevice device;std::wstring app;std::shared_ptr<const Artwork> icon;bool switchBack=false;std::wstring detail;uint32_t colour=0;} notice;
    // Phase 4: clipboard history on the Shelf, privacy indicators and the command bar.
    int shelfTab=0,clipOffset=0;bool clipsPaused=false;std::wstring clipStatus;double clipStatusUntil=0;struct Clip{uint64_t id=0;int kind=0;std::wstring preview,meta;std::shared_ptr<const Artwork> thumbnail,icon;bool pinned=false,secret=false;};std::vector<Clip> clips;
    // Phase 5C: one Shelf item opened for its actions, and what it is.
    int shelfDetail=-1;bool shelfBusy=false;std::wstring shelfStatus;double shelfStatusUntil=0;
    struct ShelfInfo{std::wstring kind,size,folder,extension;bool image=false,directory=false;int width=0,height=0;} shelfInfo;
    std::vector<PrivacyUse> privacy;
    // Phase 5B: synced lyrics of the current track (state is LyricsService::State), the seek
    // preview, detent pulses, the volume of the app under the compact logo, and the microphone.
    std::shared_ptr<const std::vector<LyricLine>> lyrics;int lyricsState=0,lyricLine=-1;bool lyricsView=true;
    float seekHover=-1;unsigned detentPulse=0;int appVolume=-1;std::wstring appVolumeName;bool micMuted=false,micAvailable=false;
    // Clipboard mode ("clip ..." or Alt+Shift+V) lists copies and shows more rows.
    struct Command{bool clips=false,paste=false;bool active=false,armed=false,error=false;std::wstring text,status;size_t caret=0;int selected=0;std::vector<CommandResult> results;std::vector<std::shared_ptr<const Artwork>> icons;} command;
};

class Renderer {
    ComPtr<ID3D11Device> d3d_;
    ComPtr<IDXGIDevice> dxgi_;
    ComPtr<IDCompositionDevice> device_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> write_;ComPtr<IDWriteRenderingParams> textParams_;
    ComPtr<IDCompositionVisual> stage_,root_,body_,inner_,header_,content_,bar_,art_,wingLeft_,wingRight_,pulseVisual_,hoverVisual_;
    ComPtr<IDCompositionRectangleClip> clip_,innerClip_,barClip_,artClip_,hoverClip_;UINT32 hoverColor_=1;
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
    void updateTimeline(const ContentSnapshot&,UINT32,UINT32,UINT32 accent2);std::array<ComPtr<IDCompositionSurface>,8> waveFillSurfaces_;UINT32 waveSecond_=1;unsigned detentSeen_=0;
    // Real-audio bars, level indicator, app badge and mixer meters.
    struct Bar {ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionScaleTransform> scale;Glide glide;};
    // Waveform timeline: 64 bars of the track's heard loudness; the played part is the
    // accent layer, revealed by a clip that advances with playback in the compositor.
    struct WaveBar {ComPtr<IDCompositionVisual> base,fill;ComPtr<IDCompositionScaleTransform> baseScale,fillScale;Glide glide;float target=-2;};
    std::array<WaveBar,64> waveBars_;ComPtr<IDCompositionVisual> wave_,waveBase_,waveFill_;ComPtr<IDCompositionScaleTransform> waveScale_;ComPtr<IDCompositionRectangleClip> waveClip_;ComPtr<IDCompositionEffectGroup> waveEffect_;
    ComPtr<IDCompositionSurface> waveBaseSurface_;UINT32 waveColors_[2]{1,1};int seekStyle_=-1;void ensureWave();
    std::array<Bar,24> bars_;std::array<Bar,4> meters_;ComPtr<IDCompositionVisual> spectrum_,meterLayer_,hudTrack_,hudFill_,artFrame_,badge_;
    ComPtr<IDCompositionSurface> spectrumSurface_,meterSurface_,hudTrackSurface_,hudFillSurface_,badgeSurface_;ComPtr<IDCompositionEffectGroup> spectrumEffect_,meterEffect_,hudEffect_,badgeEffect_;
    ComPtr<IDCompositionRectangleClip> hudClip_;Spring spectrumOpacity_{0},hudOpacity_{0},badgeOpacity_{0};UINT32 barColor_=0,meterColor_=0;int barMode_=-1,barCount_=0;float barInset_=0;
    std::shared_ptr<const Artwork> badgeIcon_;int badgeService_=-1;bool badgeLight_=false;int meterRows_=0;
    void updateSpectrumLayout(const ContentSnapshot&,UINT32 accent);void updateHud(const ContentSnapshot&,UINT32 accent,UINT32 track);void updateBadge(const ContentSnapshot&,UINT32 bg);
    BrandPainter brands_;ComPtr<IDCompositionVisual> tabPill_,cardIcon_,energy_,cardRing_;ComPtr<IDCompositionSurface> tabSurface_,cardIconSurface_,energySurface_,cardRingSurface_;int cardRingKey_=-1;ComPtr<IDCompositionEffectGroup> stageEffect_,tabEffect_,cardIconEffect_,energyEffect_;
    ComPtr<IDCompositionScaleTransform> cardIconScale_;ComPtr<IDCompositionRotateTransform> energyRotation_;Spring tabX_{0},tabOpacity_{0},cardPop_{1},energySpin_{0},energyGlow_{0};int tabKey_=-1;UINT32 tabColor_=0;int cardKey_=-1;
    // Phase 4: blinking caret for the command bar, privacy band on expanded pages.
    ComPtr<IDCompositionVisual> caret_,privacyBand_;ComPtr<IDCompositionSurface> caretSurface_,privacySurface_;ComPtr<IDCompositionEffectGroup> caretEffect_,privacyEffect_;
    Spring caretX_{42};float caretTarget_=42;float haloAlpha_=0;UINT32 haloColor_=0;/* drawn after the content surface, never inside its draw */UINT32 caretColor_=1;std::wstring privacyKey_;bool privacyVisible_=false;
    void updateCaret(const ContentSnapshot&,float x,UINT32 accent);void updatePrivacyBand(const ContentSnapshot&,UINT32 ink,UINT32 muted,UINT32 raised);
    float measure(const std::wstring&,float size,DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_NORMAL);
    void updateTabs(const ContentSnapshot&,float x,float y,int count,int selected,bool visible,UINT32 fill);void updateCard(const ContentSnapshot&,UINT32 track,UINT32 accent,UINT32 raised,UINT32 ink);
    void identity(ID2D1RenderTarget*,const MediaSnapshot&,float x,float y,float size,UINT32 plate);void deviceBadge(ID2D1RenderTarget*,const BluetoothDevice&,float x,float y,float size,UINT32 plate,UINT32 ink);
    void drawPreview(ID2D1RenderTarget*,const Artwork&,float,float,float,float);
    // Phase 5B: lyric lines on their own layer (a new line rolls up into place), the seek
    // time bubble, and the badge a double-click skip leaves on the artwork.
    ComPtr<IDCompositionVisual> lyricsLayer_,bubble_,skipBadge_;ComPtr<IDCompositionSurface> lyricsSurface_,bubbleSurface_,skipSurface_;ComPtr<IDCompositionEffectGroup> lyricsEffect_,bubbleEffect_,skipEffect_;ComPtr<IDCompositionScaleTransform> skipScale_;
    Spring lyricsRise_{0},lyricsFade_{0},bubbleOpacity_{0};std::wstring lyricsKey_,bubbleKey_;UINT32 bubbleColors_[3]{};
    void ensureNowPlaying();void updateLyrics(const ContentSnapshot&,UINT32 ink,UINT32 muted,UINT32 accent);void updateBubble(const ContentSnapshot&);
    // Word-wrapped text, at most `lines` lines (shrinking once if needed), with an optional soft glow.
    void wrappedText(ID2D1RenderTarget*,const std::wstring&,float x,float y,float w,float h,float size,UINT32 color,DWRITE_FONT_WEIGHT,UINT32 glow=0,int lines=2);
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
    // Heights 0..1 per waveform bar, -1 for stretches not heard yet.
    void waveform(const std::array<float,64>& heights,bool reduced);
    // Entrances: new content on a settled island eases in (compositor-timed), instead of snapping.
    double contentEntrance_=-1,headerEntrance_=-1;std::string contentKey_;std::wstring headerLabel_;bool restExpanded_=false,restCompact_=false;
    ComPtr<IDCompositionAnimation> entrance(double start,float from);
    // The content surface is shown through horizontal bands, so rows can cascade in.
    struct Band {ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionEffectGroup> effect;ComPtr<IDCompositionRectangleClip> clip;};std::array<Band,6> bands_;void cascade(double start);
    // A soft light that follows the pointer across the island.
    ComPtr<IDCompositionVisual> sheen_;ComPtr<IDCompositionSurface> sheenSurface_;ComPtr<IDCompositionEffectGroup> sheenEffect_;Spring sheenX_{0},sheenY_{0},sheenOpacity_{0};float sheenStrength_=0;UINT32 sheenTone_=1;
    // Pointer position in body coordinates, or outside to fade the light away.
    void pointer(float x,float y,bool inside,bool reduced);
    // Whether the island is resting open or resting compact right now, from the motion springs.
    void setRest(bool expanded,bool compact){restExpanded_=expanded;restCompact_=compact;}
    void animate(const MotionEngine&,double);
    void iconFeedback(Action,bool pressed,bool enabled);
    bool glassAvailable()const{return glass_.available();}GlassStyle glassStyle()const{return glass_.current();}UINT32 accentColor()const{return accentColor_;}
    void spectrum(const SpectrumFrame&);void energize(bool reduced,bool charging);void meters(const std::vector<MixerEntry>&,int offset,bool visible);
    // The Media page shows lyric lines instead of title and artist.
    static bool lyricsPanel(const ContentSnapshot&);
    // The seek bubble alone, while the pointer moves over the timeline.
    void seekPreview(const ContentSnapshot& s){updateBubble(s);commit();}
    // "-10 s" / "+10 s" at a point in body coordinates.
    void skipFeedback(bool forward,float x,float y,bool reduced);
    void absorb(const std::shared_ptr<const Artwork>&,float,float,float,float,bool); void routeConfirmed(bool,Action selected=Action::None); void commit(){check(device_->Commit());++commits;}
};
}
