#pragma once
#include "Composition/Renderer.h"
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
#include <memory>
#include <thread>
#include <vector>
#include <commctrl.h>
#include <shellapi.h>

namespace nexus {
enum class InteractionState { Rest,Hover,Pressed,Dragging };
class IslandWindow {
    HWND window_=nullptr,lab_=nullptr,qaMatte_=nullptr;HBRUSH qaBrush_=nullptr;HINSTANCE instance_{};HWINEVENTHOOK foregroundHook_=nullptr,locationHook_=nullptr;
    std::vector<HPOWERNOTIFY> powerNotifications_;
    std::unique_ptr<Renderer> renderer_;std::unique_ptr<AudioProvider> audio_;std::unique_ptr<MediaProvider> media_;
    std::thread platformThread_;std::unique_ptr<SystemProvider> system_;std::unique_ptr<LoopbackAnalyzer> analyzer_;std::unique_ptr<SessionMixer> mixer_;std::unique_ptr<BrightnessProvider> brightness_;std::unique_ptr<BatteryProvider> battery_;std::unique_ptr<BluetoothProvider> bluetooth_;std::unique_ptr<PowerModeWatcher> powerMode_;
    AutoHide autoHide_;bool autoHideTimer_=false;void autoHideTick();void showNotice(int kind,const BluetoothDevice& device={});bool deviceRequest_=false;void updateBattery();
    std::wstring selectedSource_;uint64_t selectedId_=0,followedId_=0;
    // With edge reveal, a pointer resting on the edge rows shows the compact island only;
    // the hover delay restarts once it moves onto the island itself.
    bool edgeHold_=false;bool pointerOffEdge();double swipeAccumulator_=0,swipeTime_=0;bool mediaReachable_=false;
    void updateSessions();void switchSession(int delta,bool absolute=false);void updateProviders();void levelIndicator();void setMixerAt(LPARAM);bool systemRequested_=false,contentDirty_=true;Action pressedAction_=Action::None;
    void setVolumeAt(LPARAM);void scrubAt(LPARAM,bool begin=false);void endScrub(bool commit);void requestPreviews(const std::vector<ShelfItem>& incoming={});void perform(Action);Action hit(LPARAM);void refresh();void clockTimer();
    std::unique_ptr<ShelfPreviews> previews_;RouteConfirmation route_;std::wstring routeName_;ComPtr<ShelfDropTarget> dropTarget_;bool visibilityAudit_=false,testing_=false,positioning_=false,motionStudy_=false;unsigned motionStudyStep_=0;
    void feedback(Action,float x=0,float y=0,bool press=false);void applySettings(bool rebuild=false,bool reposition=false);void dragShelf(size_t);
    std::unique_ptr<SettingsWindow> settingsWindow_;unsigned settingsSequence_=0;std::wstring settingsFile_=L"settings.nexus";int startupRequest_=-1;
    void openSettings(int section=-1);void receiveSettings(Settings);void settingsAction(SettingAction);SettingsContext settingsContext();void scheduleSave();
    bool settingsTest_=false,showcasePattern_=false;int settingsTestItem_=-1,settingsTestPhase_=0,settingsTestWait_=0,settingsTestValue_=0,settingsTestBefore_=0,settingsTestExpected_=0;std::vector<std::string> settingsTestLog_;int settingsTestFailures_=0;void settingsTestStep();std::string verifySetting(const SettingItem&);
    LocalStore store_;Settings settings_;MotionEngine motion_;EventOrchestrator events_;ContentSnapshot content_;
    IslandState state_=IslandState::Compact;InteractionState interaction_=InteractionState::Rest;
    float dpi_=96;double lastMotion_=0,dragTime_=0,dragVelocity_=0;
    POINT down_{},lastPointer_{},qaCursor_{},hoverSample_{};double hoverSampleTime_=0;bool hud_=false,debug_=false,benchmark_=false,labMode_=false;
    unsigned handoffStep_=0,scenarioStep_=0;double benchmarkStart_=0;FILETIME initialKernel_{},initialUser_{};
    NOTIFYICONDATAW tray_{};
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK labProcedure(HWND,UINT,WPARAM,LPARAM);
    static void CALLBACK foregroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
    LRESULT message(UINT,WPARAM,LPARAM);
    DisplayProfiles displays_;std::string currentDisplay_;bool selectDisplay_=false;void saveDisplays();
    void position();void animate();void transition(IslandState);void updateRegion(bool envelope);void presentActivity();
    void power(bool notify);void showMenu();void openLab(bool settings=false);void updateHud();void fullscreen();void yieldToApp();
    void drawLab(HWND);void labCommand(int);void finishBenchmark();
public:
    int run(HINSTANCE,const std::wstring& command);
    ~IslandWindow();
};
}
