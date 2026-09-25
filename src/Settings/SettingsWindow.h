#pragma once
#include "Common/Win32.h"
#include "Settings/SettingsModel.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace nexus {
// Messages posted to the island. SettingsChanged carries an owned Settings*;
// SettingsAction carries a SettingAction in wParam.
// SettingsTown (Phase 5G): wParam 0 a town to search for (lParam an owned std::wstring*), 1 the match chosen (lParam its index).
constexpr UINT SettingsChangedMessage=WM_APP+60,SettingsActionMessage=WM_APP+61,SettingsTownMessage=WM_APP+62;
// labStats: the Animation Lab's live readout (display rate, frames in the last transition, process figures).
// Town: the weather's place now (with its temperature), the last search and its matches, and whether one is under way.
struct SettingsContext {int monitors=1;bool blur=true,glassAvailable=true,armoury=false,shortcutTaken=false;uint32_t wallpaper=0;std::wstring version,labStats,captureTaken;
    std::wstring weatherPlace,townQuery,townStatus;std::vector<std::wstring> townResults;bool townBusy=false;};
// A standalone preferences window on its own UI thread. Every edit is posted to
// the island immediately, so the island restyles while you drag or toggle.
class SettingsUi;
class SettingsWindow {
    friend class SettingsUi;
public:
    struct Probe {enum class Kind{Section,Control}kind=Kind::Control;int index=-1;RECT rect{};bool visible=false;std::vector<RECT> parts;};
    explicit SettingsWindow(HWND island);
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&)=delete;SettingsWindow& operator=(const SettingsWindow&)=delete;
    void show(const Settings&,const SettingsContext&,int section=-1);
    // Island state after it applied a change. `sequence` is the last change it consumed.
    void update(const Settings&,const SettingsContext&,unsigned sequence);
    bool open()const{return window_.load()!=nullptr;}
    // Test support: types into the Town field (it takes the keyboard and looks the town up).
    void typeTown(const std::wstring& text);
    HWND handle()const{return window_.load();}
    // Test support: current layout in client pixels, whether motion has settled.
    std::vector<Probe> probes();bool settled();int section();
private:
    struct State;
    HWND island_;std::atomic<HWND> window_{nullptr};std::atomic<bool> finished_{true};std::thread thread_;std::unique_ptr<State> state_;
    void run(Settings,SettingsContext,int section);
};
}
