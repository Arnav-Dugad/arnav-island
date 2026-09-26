#pragma once
#include "Composition/Renderer.h"
#include "Productivity/ShareService.h"
#include "Events/EventOrchestrator.h"
#include "TelemetryLocal/LocalStore.h"
#include "Audio/AudioProvider.h"
#include "Media/MediaProvider.h"
#include "Composition/DockGeometry.h"
#include "Settings/SettingsWindow.h"
#include "Settings/Startup.h"
#include "Interaction/AppSwitchPolicy.h"
#include "Persistence/DisplayProfiles.h"
#include "FileShelf/ShelfPreviews.h"
#include "Audio/LoopbackAnalyzer.h"
#include "Audio/SessionMixer.h"
#include "Hardware/BrightnessProvider.h"
#include "Hardware/BatteryProvider.h"
#include "Hardware/BluetoothProvider.h"
#include "Hardware/Platform.h"
#include "Interaction/AutoHide.h"
#include "Audio/Waveform.h"
#include "Productivity/Clipboard.h"
#include "Productivity/Privacy.h"
#include "Productivity/CommandService.h"
#include "Media/LyricsService.h"
#include "Audio/AudioRoute.h"
#include "Capture/CaptureOverlay.h"
#include "Capture/Ocr.h"
#include "FileShelf/ShelfStore.h"
#include "Productivity/UpdateService.h"
#include "Productivity/WeatherService.h"
#include "Productivity/SiteIcons.h"
#include "Media/MusicLibrary.h"
#include "Media/IslandPlayer.h"
#include "Audio/Sounds.h"
#include <deque>
#include <map>
#include <set>
#include <memory>
#include <thread>
#include <vector>
#include <commctrl.h>
#include <shellapi.h>

namespace nexus {
constexpr UINT ControlStateMessage=WM_APP+40,WallpaperLumaMessage=WM_APP+44;
enum class InteractionState { Rest,Hover,Pressed,Dragging };
class IslandWindow {
    HWND window_=nullptr,qaMatte_=nullptr;HBRUSH qaBrush_=nullptr;HINSTANCE instance_{};HWINEVENTHOOK foregroundHook_=nullptr,locationHook_=nullptr;
    std::vector<HPOWERNOTIFY> powerNotifications_;
    std::unique_ptr<Renderer> renderer_;std::unique_ptr<AudioProvider> audio_;std::unique_ptr<MediaProvider> media_;
    std::thread platformThread_;std::unique_ptr<SystemProvider> system_;std::unique_ptr<LoopbackAnalyzer> analyzer_;std::unique_ptr<SessionMixer> mixer_;std::unique_ptr<BrightnessProvider> brightness_;std::unique_ptr<BatteryProvider> battery_;std::unique_ptr<BluetoothProvider> bluetooth_;std::unique_ptr<PowerModeWatcher> powerMode_;
    AutoHide autoHide_;bool autoHideTimer_=false;void autoHideTick();void showNotice(int kind,const BluetoothDevice& device={});bool deviceRequest_=false;void updateBattery();
    std::wstring selectedSource_;uint64_t selectedId_=0,followedId_=0;
    // With edge reveal, a pointer resting on the edge rows shows the compact island only;
    // the hover delay restarts once it moves onto the island itself.
    // Phase 4: clipboard history (memory only), privacy indicators, command bar and workspaces.
    ClipboardWatcher clipboard_;ClipboardHistory clips_;int clipRetries_=0;std::map<std::wstring,std::shared_ptr<const Artwork>> clipIcons_;std::wstring copyLabel_;
    void onClipboard();void clipViews();void clearClips();void copyClip(size_t index);
    // Phase 5G: the history kept across restarts (IslandCapture.cpp): images get a PNG once, saves are debounced.
    std::set<uint64_t> clipPngPending_;bool clipHistoryReady_=false,clipHistoryLoading_=false;size_t clipSignature_=0;
    void clipsChanged();void saveClipHistory(bool now);void loadClipHistory();bool clipMessage(UINT,WPARAM,LPARAM,LRESULT&);
    std::unique_ptr<PrivacyProvider> privacy_;std::vector<PrivacyUse> privacyUses_,qaLater_;void updatePrivacy();void showPrivacyNotice(const PrivacyUse&);
    std::unique_ptr<CommandService> commands_;WorkspaceStore workspaces_;bool hotkey_=false;int hotkeyChoice_=0;HWND commandReturn_=nullptr;uint64_t commandSeq_=0;
    void syncProductivity();void ensureCommands();void syncHotkey();void openCommand();void closeCommand(bool restoreFocus=true);bool commandKey(WPARAM);void commandChar(wchar_t);void commandQuery(bool refreshState=false);void commandResults();
    // Phase 5D: what the command bar remembers (recent and pinned commands, file opens), background system actions.
    CommandMemory commandMemory_;bool commandMemoryLoaded_=false;void loadCommandMemory();void saveCommandMemory();void rememberCommand(const CommandResult&);CommandContext commandContext();void revealResult(size_t index);void commandJobDone(LPARAM);
    bool productivityMessage(UINT,WPARAM,LPARAM,LRESULT&);void runCommand(size_t index);void commandStatus(std::wstring text,bool error=false,bool close=true);void saveWorkspaces();void commandSelect(int index);void commandShake();
    // Heard loudness of recent tracks for the waveform timeline (memory only).
    WaveformLibrary waves_;void pushWaveform(float live=-1);
    // Phase 5B: synced lyrics (opt-in; the service starts only when they are on), double-click
    // skips, seek detents, the app under the compact logo, and the headphone switch card.
    std::unique_ptr<LyricsService> lyrics_;std::wstring lyricsKey_;void syncLyrics();void tickLyrics(bool redraw=true);void clearLyrics();
    double skipClickTime_=0,lastDetent_=-1;Action skipClickAction_=Action::None;POINT skipClickPoint_{};void seekBy(double delta);void seekLyric(int line);
    void appVolumeWheel(int delta);std::wstring switchBackId_,lastRouteName_;double routeRequestAt_=-10;bool qaMicMuted_=false,micKnown_=false;
    // Phase 5C: capture (snip, text, colour), Shelf item actions, the pinned Shelf, pinned copies and the clipboard picker.
    void startCapture(CaptureMode mode);void captureDone(CaptureResult* r);void captureCard(int kind,std::wstring title,std::wstring detail,std::shared_ptr<const Artwork> icon={},uint32_t colour=0);void copyText(const std::wstring& text,bool keep=true);
    void openShelfItem(int index);void shelfAction(Action a);void shelfChanged();void loadShelfFile();void savePinnedClips();void loadPinnedClips();
    void clipResults();void clipSearch(bool picker);void pasteClip(size_t row,bool copyOnly);void syncCaptureHotkeys();bool captureMessage(UINT,WPARAM,LPARAM,LRESULT&);
    void qaBackdrop();int qaOverlay_=-1;std::vector<uint64_t> ids_;int captureHotkeys_=-1;std::wstring captureTaken_;bool jobRunning_=false,pinsLoaded_=false,pinnedShelfWas_=false;bool showHeadphoneCard(const AudioDevice& output,const std::wstring& fromId,const std::wstring& fromName);
    bool edgeHold_=false;bool pointerOffEdge();double swipeAccumulator_=0,swipeTime_=0,hwheelAt_=0;
    // Phase 5F: track swipes in the compact island, and Now Playing over fullscreen apps.
    // The Controls page: switch jobs on workers, and the brightness slider.
    void controlJob(int which,int target,int busy=0);void toggleControl(Action);void setBrightnessAt(LPARAM);std::shared_ptr<std::atomic<bool>> controlQuery_=std::make_shared<std::atomic<bool>>(false);double brightnessRequestAt_=-10;
    // Weather (opt-in): the service runs only while weather is on; a command may be waiting on its answer.
    std::unique_ptr<WeatherService> weather_;bool weatherAsked_=false;void syncWeather();
    // 0.18: updates from the island's releases. launchArgs_: this run's arguments, passed on to the new version.
    std::unique_ptr<UpdateService> update_;std::wstring launchArgs_,updatedFrom_,qaVersion_;bool qaUpdate_=false,updateShown_=false,launchArgsQa_=false;static constexpr UINT_PTR UpdateTimer=77,UpdatedTimer=78;void syncUpdates(bool checkNow=false);bool quietForUpdate();void installUpdate();void showUpdated();
    // Phase 5G: Settings' Town field (a search under way, what to say about it).
    bool townBusy_=false,qaWeatherLive_=false;std::wstring townStatus_;void townMessage(WPARAM,LPARAM);void pushSettingsContext();
    // Site icons for copied links (opt-in), kept in memory while the setting is on.
    std::unique_ptr<SiteIcons> siteIcons_;
    // The glint along the island's edge for an alert, once its shape is known.
    void alertSplash();
    // Adaptive text: the wallpaper's luminance map (loaded off the UI thread), and the grid under the compact island
    // it gives while nothing but the wallpaper is behind it. qaBackdrop_: an illustrative map for captures.
    // Sharing with your own PCs (IslandShare.cpp): the service while its setting is on, the offer a card answers, where Send goes.
    std::unique_ptr<ShareService> share_;uint32_t shareOffer_=0;std::string shareTarget_;
    // Phase 5G (IslandShare.cpp): transfers shown as they go, files dropped straight onto a PC, the Shelf dragged as a stack.
    double transferDrawn_=0;void sendToPeer(const std::string& peer,std::vector<std::wstring> paths);Action dropZoneAt(POINT screen);bool dropOnPeer(const std::vector<ShelfItem>&,POINT screen);void dragStack();
    // Phase 5G (IslandMusic.cpp): the island's own player and your Music folder's songs; music handed between your PCs.
    std::unique_ptr<MusicLibrary> library_;std::unique_ptr<IslandPlayer> player_;std::wstring qaLibrary_;bool shufflePending_=false,qaPlay_=false,qaLibraryView_=false,qaUpNext_=false,qaQueueDrag_=false;
    void syncLibrary();bool ensurePlayer();void libraryRows();void playerArt();void pauseOthers();void playLibrary(const std::vector<size_t>& order,size_t first,double start=0,bool play=true);void shuffleLibrary();bool libraryAction(Action);
    void mediaCommand(int action);void mediaSeek(double target);bool musicMessage(UINT,WPARAM,LPARAM);
    uint32_t handoffOffer_=0;ShareHandoff handoffMusic_;std::wstring handoffFrom_;double handoffAt_=0,handoffStart_=0;int handoffMatch_=-1;
    struct HandoffWait{std::wstring app,title;double position=0,accepted=0,until=0;bool nudged=false;} handoffWait_;
    void handoffTo(size_t index);void handoffEvent(const ShareEvent&);bool launchApp(const std::wstring&);void handoffAccept();void handoffWait();
    void syncSharing();void shareEvents();bool shareAction(Action);void shareCard(int kind,const std::wstring& title,const std::wstring& detail,const std::wstring& path,double duration);
    std::shared_ptr<const WallpaperLuma> wallLuma_;bool wallLoading_=false,qaBackdrop_=false;void loadWallpaperLuma();void adaptBackdrop();bool backdropCovered()const;
    // The body's top-left in the canvas (DIPs), including how far a notification pill has dropped (drop: 0..1).
    // Phase 5H: spread side by side with a waiting alert, the pill sits further left.
    PointD bodyAt(double w,double h,double drop)const{auto o=bodyOrigin(w,h,Renderer::canvasWidth,Renderer::canvasHeight,settings_.edge);if(settings_.edge==0){o.y+=drop*dropDistance;o.x-=motion_.spread.sample(seconds()).position*motion_.spreadShift;}return o;}
    // The bud's shape at time t for a pill at `origin` (from bodyAt) of this size; spreadAlerts: hovered, the two alerts spread side by side.
    BudShape budNow(double t,PointD origin,double w,double h)const{return budSpreadShape(motion_.bud.sample(t).position,motion_.spread.sample(t).position,origin.y,origin.y+h,origin.x+w/2,origin.x+w,motion_.radius.sample(t).position,motion_.budWidth,motion_.budHeight);}
    void spreadAlerts(bool on);static constexpr UINT_PTR SpreadTimer=84,QueueGlideTimer=83;
    // Phase 5H: the player's crossfades and fades; music leaving for another PC fades out (the island's song, or an app's
    // volume in the mixer, put back after it pauses); Up next; this Shelf as paired PCs see it.
    void playerTick();void fadeOutForHandoff();void appFadeTick();int playerTickMs_=0;
    struct AppFade{DWORD pid=0;float from=1;double began=0,length=1.4;std::wstring source;uint64_t session=0;bool paused=false;} appFade_;
    void upNextRows();bool upNextAction(Action);void dropQueueRow();float pointerContentY(LPARAM)const;float downY_=0;
    void publishShelf();std::map<std::wstring,std::pair<const Artwork*,std::vector<uint8_t>>> shelfPreviewBytes_;
    // Whether a notification pill is out of the island (or on its way back) at time t.
    bool dropped(double t)const{return settings_.edge==0&&!settings_.floating()&&(motion_.drop.target()>0||std::abs(motion_.drop.sample(t).position)>1e-4);}
    // Phase 5G: a sideways drag over the music skips tracks everywhere it shows (1 while one is under way).
    int swipeAxis_=0,lastSkip_=0;unsigned skips_=0;bool mediaSwipe()const;void endSwipe(int direction);
    // Phase 5G: alerts that arrive while one shows wait below it as a bud (stackAlerts), then take its place in turn.
    // deferred: it arrived while the island was open (or busy) and waits for it to settle; such an alert is dropped once a
    // minute old (at: when it arrived).
    struct HeldCard{ContentSnapshot::Notice notice;Activity activity;bool deferred=false;double at=0;};std::deque<HeldCard> heldCards_;ContentSnapshot::Notice shownNotice_;
    // 0.18: an alert that arrives while the island is open, pinned, in Live or the command bar waits instead of being lost.
    bool deferCard(const Activity&);int lowBatteryAlerted_=101;bool qaBatterySample_=false;
    bool holdCard(const Activity&);bool budAt(double x,double y)const;bool promoteCard(bool swap=false);void syncBud();static std::wstring budTitle(const ContentSnapshot::Notice&);
    void skipTrack(int direction);bool qaFullscreen_=false,fullscreenHidden_=false,peeking_=false,peekTimer_=false;double peekIdle_=0;void syncPeek();void peekTick();bool mediaReachable_=false;
    void updateSessions();void switchSession(int delta,bool absolute=false);void updateProviders();void levelIndicator();void setMixerAt(LPARAM);long glanceShown_=-2;bool systemRequested_=false,contentDirty_=true,barsWanted_=false;Action pressedAction_=Action::None;
    void setVolumeAt(LPARAM);void scrubAt(LPARAM,bool begin=false);void endScrub(bool commit);void requestPreviews(const std::vector<ShelfItem>& incoming={});void perform(Action);Action hit(LPARAM);void refresh();void clockTimer();
    std::unique_ptr<ShelfPreviews> previews_;RouteConfirmation route_;std::wstring routeName_;ComPtr<ShelfDropTarget> dropTarget_;bool visibilityAudit_=false,testing_=false,positioning_=false,motionStudy_=false;unsigned motionStudyStep_=0;
    void feedback(Action,float x=0,float y=0,bool press=false);void applySettings(bool rebuild=false,bool reposition=false);void dragShelf(size_t);
    std::unique_ptr<SettingsWindow> settingsWindow_;unsigned settingsSequence_=0;std::wstring settingsFile_=L"settings.nexus";int startupRequest_=-1;
    void openSettings(int section=-1);void receiveSettings(Settings);void settingsAction(SettingAction,int argument=0);
    // Animation Lab: frames the compositor showed during the last try-it transition.
    void labPlay(int which);void labMeasure();UINT64 labFrames_=0;double labStart_=0;std::wstring labLast_;std::wstring labStats();SettingsContext settingsContext();void scheduleSave();
    bool settingsTest_=false,showcasePattern_=false;int settingsTestItem_=-1,settingsTestPhase_=0,settingsTestWait_=0,settingsTestValue_=0,settingsTestBefore_=0,settingsTestExpected_=0;std::vector<std::string> settingsTestLog_;int settingsTestFailures_=0;void settingsTestStep();std::string verifySetting(const SettingItem&);
    LocalStore store_;Settings settings_;MotionEngine motion_;EventOrchestrator events_;ContentSnapshot content_;
    IslandState state_=IslandState::Compact;InteractionState interaction_=InteractionState::Rest;
    float dpi_=96;double lastMotion_=0,dragTime_=0,dragVelocity_=0;
    POINT down_{},lastPointer_{},qaCursor_{},hoverSample_{};double hoverSampleTime_=0;bool hud_=false,debug_=false,benchmark_=false,labMode_=false;
    unsigned handoffStep_=0,scenarioStep_=0;int uiWait_=0;double benchmarkStart_=0;FILETIME initialKernel_{},initialUser_{};
    NOTIFYICONDATAW tray_{};
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    static void CALLBACK foregroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
    LRESULT message(UINT,WPARAM,LPARAM);
    DisplayProfiles displays_;std::string currentDisplay_;bool selectDisplay_=false;void saveDisplays();
    void position();void animate();void transition(IslandState);void updateRegion(bool envelope);void presentActivity();
    void power(bool notify);void showMenu();void fullscreen();void yieldToApp();
    void finishBenchmark();
    // Phase 5H (IslandAccess.cpp): the island for screen readers (UI Automation), and what it announces.
    ComPtr<IUnknown> accessRoot_;LRESULT accessObject(WPARAM,LPARAM);void accessClose();bool accessMessage(UINT,WPARAM,LPARAM);void announce(const std::wstring&,bool important=false);void accessChanged();std::wstring accessAlert(const ContentSnapshot::Notice&)const;
    std::wstring accessAlert_,accessView_,accessSong_;int accessLevel_=-2,accessTab_=-1,accessTab2_=-1,qaLateCard_=0;
public:
    int run(HINSTANCE,const std::wstring& command);
    // What UI Automation's providers ask of the island (on its own thread): what can be pressed, where, what it is called and holds.
    HWND accessWindow()const{return window_;}std::vector<HitTarget> accessTargets()const;RECT accessRect(const HitTarget&)const;RECT accessBody()const;Action accessAt(LONG x,LONG y);bool accessExpanded()const;
    bool accessToggled(Action)const;void accessRange(Action,double& value,double& lo,double& hi)const;std::wstring accessibleName(Action)const;std::wstring accessSummary()const;
    // Phase 5E: the shadow window follows the island (same rectangle, just beneath it, same visibility).
    HWND shadow_=nullptr;void syncShadow();
    ~IslandWindow();
};
}
