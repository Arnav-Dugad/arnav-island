#pragma once
#include "Composition/Renderer.h"
#include "Events/EventOrchestrator.h"
#include "TelemetryLocal/LocalStore.h"
#include "Audio/AudioProvider.h"
#include "Media/MediaProvider.h"
#include "Composition/DockGeometry.h"
#include "Composition/GlassMaterial.h"
#include "Settings/Startup.h"
#include <memory>
#include <vector>
#include <commctrl.h>
#include <shellapi.h>

namespace nexus {
enum class InteractionState { Rest,Hover,Pressed,Dragging };
class IslandWindow {
    HWND window_=nullptr,lab_=nullptr,qaMatte_=nullptr;HBRUSH qaBrush_=nullptr;HINSTANCE instance_{};HWINEVENTHOOK foregroundHook_=nullptr;
    std::vector<HPOWERNOTIFY> powerNotifications_;
    std::unique_ptr<Renderer> renderer_;std::unique_ptr<AudioProvider> audio_;std::unique_ptr<MediaProvider> media_;
    std::unique_ptr<SystemProvider> system_;bool systemRequested_=false;Action pressedAction_=Action::None;
    void perform(Action);Action hit(LPARAM);void refresh();void clockTimer();
    ComPtr<ShelfDropTarget> dropTarget_;GlassMaterial glass_;bool testing_=false,positioning_=false;
    bool showGlass();void feedback(Action,float x=0,float y=0,bool press=false);void applySettings(bool rebuild=false);void dragShelf(size_t);
    LocalStore store_;Settings settings_;MotionEngine motion_;EventOrchestrator events_;ContentSnapshot content_;
    IslandState state_=IslandState::Compact;InteractionState interaction_=InteractionState::Rest;
    float dpi_=96;double lastMotion_=0,dragTime_=0,dragVelocity_=0;
    POINT down_{},lastPointer_{},qaCursor_{};bool hud_=false,debug_=false,benchmark_=false,labMode_=false;
    unsigned scenarioStep_=0;double benchmarkStart_=0;FILETIME initialKernel_{},initialUser_{};
    NOTIFYICONDATAW tray_{};
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK labProcedure(HWND,UINT,WPARAM,LPARAM);
    static void CALLBACK foregroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
    LRESULT message(UINT,WPARAM,LPARAM);
    void position();void animate();void transition(IslandState);void updateRegion(bool envelope);void presentActivity();
    void power(bool notify);void showMenu();void openLab(bool settings=false);void updateHud();void fullscreen();
    void drawLab(HWND);void labCommand(int);void finishBenchmark();
public:
    int run(HINSTANCE,const std::wstring& command);
    ~IslandWindow();
};
}
