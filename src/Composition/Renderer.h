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
#include "Productivity/Weather.h"
#include "Productivity/Update.h"
#include "Animation/GlassDrops.h"
#include "Design/Backdrop.h"
#include "Productivity/ShareService.h"
#include "Media/Library.h"
namespace nexus {
struct ContentSnapshot {
    Page page=Page::Overview;bool expanded=false,live=false,dropHover=false,light=false,blur=true;int layoutSlot=0;bool reducedMotion=false;int settingsPage=0,shelfOffset=0,audioOffset=0;std::wstring activity;std::vector<ShelfItem> shelf;std::vector<AudioDevice> outputs;std::wstring feedback;Action hovered=Action::None;bool pinned=false;
    MediaSnapshot playback;SystemSnapshot system;Settings settings;FocusClock focus;ScrubGesture scrub;
    // Adaptive text: what is behind the compact island (Clear glass over bare wallpaper only), else null.
    std::shared_ptr<const LumaGrid> adapt;
    // Sharing: your PCs on this network, the paired one sends go to, and this PC's name.
    std::vector<SharePeer> nearby;std::string nearbyTarget;std::wstring shareName;
    // Phase 5G: transfers under way (a Nearby row's progress and Stop, the compact island's chip), and the zone a drag
    // over the island would drop into (DropShelf, or NearbyBase + the PC's row), shown while files are dragged over it.
    // Phase 5H: rate, its speed in bytes a second (smoothed; 0 until known), sampled at rateAt (island seconds) when rateDone had arrived.
    struct Transfer{uint32_t id=0;std::string peer;std::wstring title,name;uint64_t done=0,total=0;uint32_t count=0;bool outgoing=true;double rate=0,rateAt=0;uint64_t rateDone=0;};std::vector<Transfer> transfers;
    Action dropZone=Action::None;
    // Phase 5G: the Media page's library (songs in the Music folder, the rows on screen and their covers, the song the
    // island plays), and the paired PCs offered for continuing the music there.
    // Phase 5G: an alert waiting below the one that shows, as a bud (kind 0: none): its kind, what it says, how many more wait.
    struct Bud{int kind=0;std::wstring title;int more=0;} bud;
    // Island DJ: the colour of what plays next, when the island's own queue knows it (0 otherwise).
    UINT32 djAccent=0;
    bool library=false,libraryScanning=false,handoffPicking=false;int libraryOffset=0;std::shared_ptr<const std::vector<LibraryTrack>> libraryTracks;
    std::vector<std::shared_ptr<const Artwork>> libraryArt;std::wstring libraryPlaying;
    // Phase 5H: Up next (the island's own queue after the song playing): up to 200 of its songs, the first row shown,
    // the covers of the rows on screen, and a row being dragged to a new place (from: its index in upNextTracks; y:
    // where the pointer holds it, in content coordinates; grab: where on the row it was taken). nextArt: the next
    // song's cover, peeking out from behind the one playing.
    bool upNext=false;int upNextOffset=0;std::vector<LibraryTrack> upNextTracks;std::vector<std::shared_ptr<const Artwork>> upNextArt;
    struct QueueDrag{bool active=false;int from=-1;float y=0,grab=0;} queueDrag;std::shared_ptr<const Artwork> nextArt;
    // Phase 5H: a paired PC's Shelf, seen from Nearby (state 0 asking, 1 shown, 2 kept to itself, 3 couldn't ask:
    // status says why), the first row shown and the items' previews.
    struct RemoteShelf{bool open=false;std::string peer;std::wstring name,status;int state=0,offset=0;std::vector<ShareShelfItem> items;std::vector<std::shared_ptr<const Artwork>> previews;} remote;
    // The Media page shows what plays (not the library, not Up next).
    bool mediaPage()const{return page==Page::Media&&!library&&!upNext;}
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
    // progress: music from another PC (kind 17), how far into the song it is (0..1; -1 unknown).
    struct Notice{int kind=0;BluetoothDevice device;std::wstring app;std::shared_ptr<const Artwork> icon;bool switchBack=false;std::wstring detail;uint32_t colour=0;std::wstring path;double progress=-1;} notice;
    // Phase 4: clipboard history on the Shelf, privacy indicators and the command bar.
    int shelfTab=0,clipOffset=0;bool clipsPaused=false;std::wstring clipStatus;double clipStatusUntil=0;struct Clip{uint64_t id=0;int kind=0;std::wstring preview,meta;std::shared_ptr<const Artwork> thumbnail,icon;bool pinned=false,secret=false;
        // Phase 5F, rich rows: a colour code's colour, code (preview is then its first line), a link's host and path and its site icon.
        bool hasColour=false,code=false;uint32_t colour=0;std::wstring host,path;std::shared_ptr<const Artwork> favicon;};std::vector<Clip> clips;
    // Phase 5C: one Shelf item opened for its actions, and what it is.
    int shelfDetail=-1;bool shelfBusy=false;std::wstring shelfStatus;double shelfStatusUntil=0;
    struct ShelfInfo{std::wstring kind,size,folder,extension;bool image=false,directory=false;int width=0,height=0;} shelfInfo;
    std::vector<PrivacyUse> privacy;
    // Phase 5B: synced lyrics of the current track (state is LyricsService::State), the seek
    // preview, detent pulses, the volume of the app under the compact logo, and the microphone.
    std::shared_ptr<const std::vector<LyricLine>> lyrics;int lyricsState=0,lyricLine=-1;bool lyricsView=true;
    float seekHover=-1;unsigned detentPulse=0;int appVolume=-1;std::wstring appVolumeName;bool micMuted=false,micAvailable=false;
    // Clipboard mode ("clip ..." or Alt+Shift+V) lists copies and shows more rows.
    // Phase 5F: the weather at the chosen place (Open-Meteo, opt-in), when known.
    // sunrise, sunset: today's at the place (Unix seconds; 0 unknown), for the dawn and dusk sky.
    // now (0.18): everything else the forecast brought, for the weather's own view (weatherView, opened from Home).
    struct Weather{bool valid=false;double temperature=0;int code=-1;bool day=true;std::wstring place;int64_t sunrise=0,sunset=0;WeatherNow now;} weather;bool weatherView=false;
    // Phase 5F: the Controls page. Radios and dark mode: 1 on, 0 off, -1 unknown, -2 none. busy: switches
    // being changed (1 Wi-Fi, 2 Bluetooth, 4 airplane, 8 dark mode).
    struct Controls{int wifi=-1,bluetooth=-1,dark=-1,busy=0;} controls;
    // Phase 5F: the weekly battery card (notice kind 13): the week summarised, and health now and a week ago.
    BatteryWeek week;double healthNow=-1,healthBefore=-1;
    // 0.18: the Battery tab's details (paged by rows of three), the week so far, and the first day of the health log.
    bool batteryDetails=false;int batteryOffset=0;BatteryWeek batteryWeek;double healthFirst=-1;int64_t healthSince=0;
    // 0.18.1: the health chart (one reading a day: Unix day, health 0-1) in place of the last 24 hours; and after an update,
    // its notes for the What's new sheet (opened from the Updated card), paged by the wheel.
    bool batteryHealthChart=false;std::vector<std::pair<int64_t,float>> healthDays;std::vector<NoteLine> whatsNew;bool whatsNewView=false;int whatsNewOffset=0;
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
    int cachedEdge_=-1,edge_=0;bool attached_=true,expanded_=false,live_=false;UINT32 baseColor_=0,accentColor_=0;int material_=-1;GlassBackdrop glass_,shadow_;std::shared_ptr<const Artwork> artwork_;
        // strip: the icon itself, or (Phase 5F) a flipbook of frames stepped behind the visual's one-frame clip.
        struct IconVisual {ComPtr<IDCompositionVisual> visual,strip;ComPtr<IDCompositionRectangleClip> clip;ComPtr<IDCompositionSurface> frames;ComPtr<IDCompositionSurface> surface;ComPtr<IDCompositionScaleTransform> scale;ComPtr<IDCompositionEffectGroup> effect;Spring x{0},y{0},lift{0},zoom{1};Action action=Action::None;int key=-1;float drawnSize=0,baseY=0;bool used=false;};
    struct IconRequest{Action action;Icon glyph;float x,y,size;UINT32 color;int slot;bool celebrate=false;};std::vector<IconRequest> iconRequests_;bool drawingContent_=false;
    std::array<IconVisual,64> icons_;size_t iconCursor_=pageCount;bool iconMotion_=true;Action hoverAction_=Action::None;bool pressing_=false;
    ComPtr<IDCompositionVisual> iconLayer_,artFrom_,artTo_,nav_,rings_,batteryDot_,timerDot_;
    ComPtr<IDCompositionSurface> artFromSurface_,artToSurface_,navSurface_,ringsSurface_,dotSurface_;
    ComPtr<IDCompositionEffectGroup> iconEffect_,incomingEffect_,navEffect_,ringsEffect_,batteryDotEffect_,timerDotEffect_;
    ComPtr<IDCompositionRotateTransform> batteryRotation_,timerRotation_;
    // Phase 5G: the navigation pill is liquid. Its two ends have their own springs, the leading one quicker, so it
    // stretches like a droplet as it moves and draws itself back in; two caps and a middle keep its corners round.
    Spring navLeft_{4},navRight_{51};ComPtr<IDCompositionVisual> navCapLeft_,navCapRight_,navMiddle_;ComPtr<IDCompositionRectangleClip> navClipLeft_,navClipRight_;ComPtr<IDCompositionScaleTransform> navMiddleScale_;ComPtr<IDCompositionSurface> navMiddleSurface_;
    void placeNav(double now,float navY);
    // Phase 5H: Up next's rows glide to their places (per song, in content DIPs) while one is dragged among them.
    std::map<std::wstring,float> queueY_;double queueAt_=0;bool queueGliding_=false;
public:bool queueGliding()const{return queueGliding_;}private:
    ArtworkHandoff handoff_;Spring navX{4},batteryAngle{0},timerAngle{0};bool ringsEnabled_=true;int ringCount_=2;double ringBattery_=-2,ringTimer_=-2;int ringFlags_=-1;UINT32 ringColor_=0,ringTrack_=0;
    // celebrate: play the glyph's own animation once (a page just chosen, a switch just turned on).
    void icon(Action,Icon,float,float,float,UINT32,int stableSlot=-1,bool celebrate=false);
    // Phase 5F: an icon visual's picture, as a still or a flipbook stepped on compositor time.
    void paintIcon(IconVisual&,Icon,float size,UINT32 color,bool celebrate);void flip(IconVisual&,int frames,double duration,const std::function<void(ID2D1RenderTarget*,float)>& draw);void createIconVisual(IconVisual&);
    int celebratedPage_=-1;std::array<bool,6> controlOn_{};
    // The colour an app's icon lends when there is no album art (cached per icon).
    std::vector<std::pair<std::weak_ptr<const Artwork>,std::pair<UINT32,UINT32>>> iconColours_;std::pair<UINT32,UINT32> iconColour(const std::shared_ptr<const Artwork>&);
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
    // Phase 5D: the lyrics scroller. Neighbouring lines share one layer that springs a line
    // at a time; the sung line has its own layer that grows into place while a
    // compositor-timed fill sweeps its rows; the line it replaces fades out as a ghost;
    // instrumental gaps show three dots that fill across the gap. Also the seek time
    // bubble and the badge a double-click skip leaves on the artwork.
    ComPtr<IDCompositionVisual> lyricsLayer_,lyricsList_,lyricsLine_,lyricsGhost_,lyricsDots_,bubble_,skipBadge_;std::array<ComPtr<IDCompositionVisual>,2> lyricsRows_;std::array<ComPtr<IDCompositionVisual>,3> lyricsDot_;
    ComPtr<IDCompositionSurface> lyricsSurface_,lyricsLineSurface_,lyricsFillSurface_,lyricsGhostSurface_,lyricsDotSurface_,bubbleSurface_,skipSurface_;
    ComPtr<IDCompositionEffectGroup> lyricsEffect_,lyricsGhostEffect_,bubbleEffect_,skipEffect_;std::array<ComPtr<IDCompositionEffectGroup>,3> lyricsDotEffect_;
    ComPtr<IDCompositionScaleTransform> skipScale_,lyricsLineScale_,lyricsGhostScale_,lyricsDotsScale_;ComPtr<IDCompositionRectangleClip> lyricsClip_;std::array<ComPtr<IDCompositionRectangleClip>,2> lyricsRowClip_;
    Spring lyricsScroll_{0},lyricsFade_{0},bubbleOpacity_{0};std::wstring lyricsKey_,lyricsTiming_,bubbleKey_;UINT32 bubbleColors_[3]{};UINT32 lyricsDotColor_=1;float lyricsWidth_=0;
    // Where the last drawn lines sat (DIPs from the scroller's top), so the next change can travel from there.
    struct LyricsPlacement{int line=-2;bool gap=false;float current=0,height=0,previous=0,next=0;};LyricsPlacement lyricsPlaced_;
    struct LyricRow{float top,height,width;UINT32 first,length;std::vector<float> xs;};std::vector<LyricRow> lyricsRowsLaid_;
    ComPtr<IDWriteTextLayout> lyricLayout(const std::wstring&,float size,float width,float height,DWRITE_FONT_WEIGHT);void drawLyric(ID2D1RenderTarget*,IDWriteTextLayout*,float x,float y,UINT32 color,float alpha);
    void timeLyrics(const ContentSnapshot&,double now);
    // Phase 5D, command bar v2: matched letters in titles, and answers whose digits roll into place.
    void markedText(ID2D1RenderTarget*,const std::wstring&,const MatchMarks&,float x,float y,float w,float size,UINT32 color,UINT32 highlight);
    // Phase 5E, rolling numbers. Where a number is drawn, its place is recorded as a spot and
    // an odometer shows it instead: one column per character, digits as windows onto a strip
    // of tabular 0-9 (three times over) that springs to the new digit.
    // Spot ids: 0 currency answer, 1 level indicator, 2 focus clock, 3-5 Home statistics,
    // 6-8 compact volume, battery and timer, 9 the compact timer label, 10-11 the idle glance CPU and GPU.
    // box: the height the text is centred in (0: top-aligned).
    struct OdometerSpot{int id;std::wstring text;float x,y,size;DWRITE_FONT_WEIGHT weight;UINT32 ink;float box=0;bool trailing=false,spin=false;};
    // blur: a copy of the strip smeared along the roll, faded in with the roll's speed on big jumps; stretch: the column lengthens with it.
    struct OdometerColumn{ComPtr<IDCompositionVisual> column,strip,blur;ComPtr<IDCompositionRectangleClip> clip;ComPtr<IDCompositionEffectGroup> sharpFade,blurFade;ComPtr<IDCompositionScaleTransform> stretch;Spring roll{10};int digit=-1;};
    struct Odometer{ComPtr<IDCompositionVisual> root;ComPtr<IDCompositionEffectGroup> effect;std::vector<OdometerColumn> columns;ComPtr<IDCompositionSurface> digits,blurDigits;std::map<wchar_t,ComPtr<IDCompositionSurface>> glyphs;
        float size=0,cell=0,top=0,line=0;int weight=0;UINT32 ink=1;float halo=-1;std::wstring shown;float x=0,y=0,box=0;bool trailing=false;
        // Adaptive text: the same figures in the ink for a bright backdrop, used by the columns over one.
        ComPtr<IDCompositionSurface> altDigits,altBlur;std::map<wchar_t,ComPtr<IDCompositionSurface>> altGlyphs;UINT32 altInk=0;int serial=-1;};
    std::array<Odometer,13> odometers_;std::vector<OdometerSpot> contentSpots_,headerSpots_;float answerY_=-1;std::wstring answerText_;double cascadeStart_=-10;
    void odometer(Odometer&,IDCompositionVisual* parent,const OdometerSpot*,bool reduced);void placeOdometers(const std::vector<OdometerSpot>&,std::initializer_list<int> ids,IDCompositionVisual* parent,bool reduced);
    // The fade and rise of content band k in a cascade that began at `start`.
    void bandCascade(double start,size_t k,ComPtr<IDCompositionAnimation>& fade,ComPtr<IDCompositionAnimation>& rise);
    // Phase 5F: the sung line on one row, as the compact island and the Live Island show it, with
    // the Command Center's look: a dim line lit by a fill that follows the song (word by word when
    // the lyrics are word-timed), the next line rising in as the last lifts away, and three dots
    // across instrumental gaps. Two slots let a line morph into the next.
    struct LineLyric{ComPtr<IDCompositionVisual> host,dotLayer;ComPtr<IDCompositionRectangleClip> clip;
        struct Slot{ComPtr<IDCompositionVisual> visual,fill;ComPtr<IDCompositionSurface> base,lit;ComPtr<IDCompositionEffectGroup> effect;ComPtr<IDCompositionRectangleClip> fillClip;};std::array<Slot,2> slots;
        std::array<ComPtr<IDCompositionVisual>,3> dots;std::array<ComPtr<IDCompositionEffectGroup>,3> dotEffects;ComPtr<IDCompositionSurface> dotSurface;UINT32 dotColor=1;
        int front=0,line=-2;bool gap=false;std::wstring key,timing;std::vector<float> xs;};
    LineLyric compactLyric_,liveLyric_;
    void lineLyric(LineLyric&,IDCompositionVisual* parent,const ContentSnapshot&,bool visible,float x,float y,float w,float h,float size,UINT32 lit,bool reduced);
    // A value following (song seconds, value) points while the song plays from `position`; held while paused.
    ComPtr<IDCompositionAnimation> track(double now,double position,bool playing,const std::vector<std::pair<double,double>>& keys);
    // Phase 5F: the spectrum ring around the compact cover (24 ticks, one per band, mirrored left and
    // right), the cover turning round while it shows, and a nudge of the header when a swipe changes track.
    struct RingTick{ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionScaleTransform> scale;ComPtr<IDCompositionRotateTransform> rotate;Glide glide{.22,0,.22,0,0};};
    // Phase 5G, Island DJ: two halos behind the ring, one that blooms as a track starts and one that breathes through its last seconds.
    struct Halo{ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionEffectGroup> effect;ComPtr<IDCompositionScaleTransform> scale;ComPtr<IDCompositionSurface> surface;UINT32 colour=1;};
    Halo djBloom_,djEnd_;std::wstring djTitle_,djKey_;void halo(Halo&,UINT32 colour);
    std::array<RingTick,24> ringTicks_;ComPtr<IDCompositionVisual> ring_;ComPtr<IDCompositionEffectGroup> ringEffect_;ComPtr<IDCompositionSurface> ringSurface_;UINT32 spectrumRingColor_=1;bool ringOn_=false;float artRound_=-1;
    ComPtr<IDCompositionTranslateTransform> headerKick_;Spring kick_{0};
    // Phase 5G: the skip chip of a sideways drag (swipeFollow), and how far the header follows it now.
    ComPtr<IDCompositionVisual> swipeHint_;ComPtr<IDCompositionSurface> swipeNext_,swipePrevious_;ComPtr<IDCompositionEffectGroup> swipeEffect_;ComPtr<IDCompositionScaleTransform> swipeScale_;
    int swipeSide_=0;bool swipeArmed_=false;float swipeProgress_=0,swipeOffset_=0,swipeX_=0,swipeY_=0;UINT32 swipeInk_=0xf1f3f7,swipeAccent_=0,swipeDrawn_=0;
    // The lean while dragged (top dock): a shear about the top edge and a stretch about the top centre.
    ComPtr<IDCompositionSkewTransform> leanSkew_;ComPtr<IDCompositionScaleTransform> leanScale_;ComPtr<IDCompositionTransform> leanGroup_;bool leaning_=false;
    // The docked stub of the island while a notification pill has dropped out of it (solid material; glass draws its own).
    ComPtr<IDCompositionVisual> stub_;ComPtr<IDCompositionRectangleClip> stubClip_;ComPtr<IDCompositionEffectGroup> stubEffect_;
    // Phase 5G: a waiting alert's bud below the drop pill: its fill (solid material; glass draws its own) and its label.
    ComPtr<IDCompositionVisual> bud_,budLabel_;ComPtr<IDCompositionRectangleClip> budClip_;ComPtr<IDCompositionEffectGroup> budEffect_,budLabelEffect_;ComPtr<IDCompositionSurface> budLabelSurface_;std::wstring budKey_;
    void updateBud(const ContentSnapshot&,UINT32 ink,UINT32 muted,UINT32 accent,UINT32 raised);
    // The Home weather tile's animated sky (Sky.cpp).
    struct SkyPart{ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionEffectGroup> effect;};std::array<SkyPart,14> skyParts_;
    ComPtr<IDCompositionVisual> sky_;ComPtr<IDCompositionRectangleClip> skyClip_;ComPtr<IDCompositionEffectGroup> skyEffect_;ComPtr<IDCompositionRotateTransform> skyRays_;
    ComPtr<IDCompositionSurface> skySun_,skyStreak_,skyFlake_,skyCloud_,skyFog_,skyStar_,skyFlash_,skyGlow_;
    // 0.18: the weather on the glass (inside the body, above the glass and beneath everything drawn on it): falling rain
    // (or snow) as two stacked tiles scrolled down forever, drops beaded on the pane, and fog as two tiles drifting sideways.
    ComPtr<IDCompositionVisual> weatherFx_,rainFx_,dropsFx_,fogFx_;std::array<ComPtr<IDCompositionVisual>,4> fxTiles_;ComPtr<IDCompositionEffectGroup> weatherFxEffect_;
    ComPtr<IDCompositionSurface> rainTile_,dropsTile_,fogTile_;std::string weatherFxKey_;void updateWeatherGlass(const ContentSnapshot&);
    // 0.18.1: drops that run (a live GlassDrops field, stepped four times a second while it rains), lightning in a storm
    // (a glow from inside that flickers now and then), and a warm haze on poor-air days. shakeAt_: when the island last
    // moved (it shakes drops loose); bodyW_, bodyH_: the island's size the drops live in.
    struct LiveDrop{ComPtr<IDCompositionVisual> visual;ComPtr<IDCompositionEffectGroup> effect;ComPtr<IDCompositionScaleTransform> scale;float x=0,y=0,r=0,o=0;};std::array<LiveDrop,14> liveDrops_;
    ComPtr<IDCompositionVisual> liveDropsFx_,flashFx_,hazeFx_;ComPtr<IDCompositionEffectGroup> flashEffect_;ComPtr<IDCompositionSurface> beadSurface_,flashTile_,hazeTile_;
    GlassDrops dropField_;bool dropsOn_=false;double dropAt_=0,shakeAt_=-10,shapeSignature_=0;float bodyW_=0,bodyH_=0;
public:
    // While it rains on the glass: step the drops (the island calls this about every 250 ms).
    bool wantsDropSteps()const{return dropsOn_;}void stepDrops();
    // Lines of the What's new sheet that fit (for paging with the wheel), and how many there are.
    int whatsNewShown_=0;
private:
    // 0.18.1: the weather view's "now" marker glides along the hourly curve (x linear in time, y the curve's cubic).
    ComPtr<IDCompositionVisual> nowMarker_;ComPtr<IDCompositionSurface> nowMarkerSurface_;UINT32 nowMarkerColor_=1;bool nowMarkerAdded_=false;
    struct NowCurve{bool on=false;float x0=0,dx=0,p0=0,p1=0,m0=0,m1=0;int64_t start=0;} nowCurve_;void updateNowMarker(UINT32 accent);
    // The tile the sky plays in, drawn beneath it (the content's own box is left out), so the tile's text stays above the weather.
    ComPtr<IDCompositionVisual> skyBase_;ComPtr<IDCompositionSurface> skyBaseSurface_;UINT32 skyTile_=0x14171d;float skyTileAlpha_=1;std::wstring skyKey_;bool skyShown_=false,skyWanted_=false;float skyX_=0;
    void skyScene(const ContentSnapshot&,float x);void skyHide();
    // Adaptive text, while the compact header draws: the backdrop grid, where the header sits in the canvas, and the inks it flips.
    std::shared_ptr<const LumaGrid> adapt_;const LumaGrid* adaptSeen_=nullptr;int adaptSerial_=0;float adaptX_=0;UINT32 adaptInk_=0,adaptMuted_=0;
    // 1 over a bright backdrop, 0 over a dark one, -1 when not adapting (header-local DIPs).
    int backdropAt(float x,float y)const{return adapt_?(adapt_->at(adaptX_+x,y)>brightBackdrop?1:0):-1;}
    UINT32 adaptVariant(UINT32 c,bool bright)const{return c==adaptInk_?(bright?0x15171cu:0xf5f7fau):c==adaptMuted_?(bright?0x3f4651u:0xbcc3ceu):c;}
    UINT32 adaptInk(UINT32 c,float x,float y)const{const int b=backdropAt(x,y);return b<0?c:adaptVariant(c,b==1);}
    // A curve that follows any function of time (sampled with its slope) until `settled`.
    ComPtr<IDCompositionAnimation> curveOf(const std::function<double(double)>& value,double now,const std::function<bool(double)>& settled);
    // Phase 5F: light that runs along the island's edge when an alert arrives: a bright core inside a dimmer and a soft tier
    // sweeping out from the middle of the free edge, over a stroke of the outline.
    // The alert glint: two tiers (a crisp core, a wide glow) x two directions x six nested windows, each a sixth as bright,
    // so the light falls off smoothly at both ends of each run (hard-edged windows looked like tearing lines on glass).
    static constexpr int splashSteps=6;ComPtr<IDCompositionVisual> splash_;std::array<ComPtr<IDCompositionVisual>,4*splashSteps> splashBands_;std::array<ComPtr<IDCompositionRectangleClip>,4*splashSteps> splashClips_;std::array<ComPtr<IDCompositionEffectGroup>,4*splashSteps> splashBandEffects_;ComPtr<IDCompositionEffectGroup> splashEffect_;std::array<ComPtr<IDCompositionSurface>,2> splashSurfaces_;void updateRing(const ContentSnapshot&,UINT32 accent);
    // Ease-out cubic from `from` to `to` over `duration`, beginning at `start`.
    ComPtr<IDCompositionAnimation> ease(double start,float from,float to,double duration);
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
    // Phase 5F: one line of code in a monospaced face, coloured by kind of token.
    void codeText(ID2D1RenderTarget*,const std::wstring&,float x,float y,float w,float size,UINT32 ink,UINT32 muted,bool light);
public:
    static constexpr float canvasWidth=680,canvasHeight=500;
    std::vector<HitTarget> targets;
    Action hit(float x,float y)const;
    // Phase 5F: the compact island's own targets (media controls), in header coordinates.
    std::vector<HitTarget> compactTargets;Action compactHit(float x,float y)const{for(auto& t:compactTargets)if(t.contains(x,y))return t.action;return Action::None;}
    // Phase 5H: what can be pressed, for screen readers, in the body's coordinates: the compact island's controls (header at
    // headerLeft), or the page's targets and the navigation.
    std::vector<HitTarget> accessibleTargets(bool compact,float headerLeft)const;
    // The compact header slides the way a swipe went, then springs back.
    void trackSkip(int direction,bool reduced);
    // Phase 5G: a sideways drag that can skip a track. The compact header follows the drag (header), and a chip on the
    // leading side fades and grows in with it, popping once letting go would skip. dx in DIPs; swipeEnd lets it go.
    void swipeFollow(float dx,float bodyWidth,float bodyHeight,bool header,bool reduced);void swipeEnd(bool skipped,bool reduced);
    // Runs a glint around an island of this size (the outline it will have), after `delay` seconds.
    void splash(float width,float height,float radius,bool attached,double delay,bool reduced);
    unsigned commits=0,redraws=0; bool software=false;
    // shadow: the click-through window under the island that shows its soft drop shadow (optional).
    void initialize(HWND,float,HWND shadow=nullptr);
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
    // Test runs only (--qa-glass-only): the DirectComposition layers hidden, so the glass alone can be measured.
    bool qaGlassOnly=false;
    // Test runs only (--qa-frost): the resting frost settles in about a second instead of half a minute.
    double qaFrostPace=1;void qaFrost(double pace){qaFrostPace=pace;if(frostOn_){frostOn_=false;glass_.frost(false,seconds());}updateFrost();}std::string glassFailure()const{return glass_.failure()+" frostOn="+std::to_string(frostOn_)+" allowed="+std::to_string(frostAllowed_)+" expanded="+std::to_string(expanded_)+" inside="+std::to_string(pointerInside_)+" wx="+weatherFxKey_+" mat="+std::to_string(material_)+" drops="+[this]{int alive=0,running=0;float area=0;for(auto& d:dropField_.drops)if(d.alive){++alive;running+=d.speed>0;area+=d.r*d.r;}return std::to_string(dropsOn_)+"/"+std::to_string(alive)+"/"+std::to_string(running)+"/"+std::to_string(int(area));}();}
    // 0.18: rows of three in the Battery tab's details (for paging them with the wheel).
    int batteryRows_=0;
    bool glassAvailable()const{return glass_.available();}std::string glassError()const{return glass_.lastError_.empty()?shadow_.lastError_:glass_.lastError_;}GlassStyle glassStyle()const{return glass_.current();}UINT32 accentColor()const{return accentColor_;}
    void spectrum(const SpectrumFrame&);
    // Phase 5E: the artwork beat pulse (a scale about the cover's centre, before its size scale).
    // 0.17.0-preview.3: the edge light that breathes with the beat. Glass carries it in its rim; solid islands show four
    // soft strips inside their free edges (beatLight_), in the accent. edgeBeat_ is its level (0-1).
    static constexpr float beatBand=12;Glide edgeBeat_{0,0,0,0,0};double edgeAttack_=0;bool beatEdgeOn_=false;int beatMaterial_=-1,beatStripKey_=-1;UINT32 beatColor_=1,beatSurfaceColor_=1;
    ComPtr<IDCompositionVisual> beatLight_;ComPtr<IDCompositionEffectGroup> beatLightEffect_;std::array<ComPtr<IDCompositionVisual>,4> beatStrips_;std::array<ComPtr<IDCompositionSurface>,4> beatSurfaces_;
    void setBeatEdge(bool on,UINT32 color,bool glass,bool reduced);void edgeBeat(float hit,double now);void edgeLight(double now);
    // 0.17.0-preview.3: Frosted glass thickens while the island rests (compact, the pointer away, no alert just in) and
    // clears when reached for.
    bool frostAllowed_=false,frostOn_=false,pointerInside_=false;double frostHold_=0;void updateFrost();
    ComPtr<IDCompositionScaleTransform> artPulse_;Glide artBeat_{1,0,1,0,0};float bassAverage_=0;double beatAt_=0;bool artPulseOn_=false;void beat(const SpectrumFrame&);void setArtPulse(bool on);void energize(bool reduced,bool charging);void meters(const std::vector<MixerEntry>&,int offset,bool visible);
    // The Media page shows lyric lines instead of title and artist.
    static bool lyricsPanel(const ContentSnapshot&);
    // The seek bubble alone, while the pointer moves over the timeline.
    void seekPreview(const ContentSnapshot& s){updateBubble(s);commit();}
    // "-10 s" / "+10 s" at a point in body coordinates.
    void skipFeedback(bool forward,float x,float y,bool reduced);
    void absorb(const std::shared_ptr<const Artwork>&,float,float,float,float,bool); void routeConfirmed(bool,Action selected=Action::None); void commit(){check(device_->Commit());++commits;}
};
}
