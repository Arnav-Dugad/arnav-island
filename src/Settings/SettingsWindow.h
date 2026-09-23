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
constexpr UINT SettingsChangedMessage=WM_APP+60,SettingsActionMessage=WM_APP+61;
struct SettingsContext {int monitors=1;bool blur=true,glassAvailable=true,armoury=false,shortcutTaken=false;std::wstring version;};
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
    HWND handle()const{return window_.load();}
    // Test support: current layout in client pixels, whether motion has settled.
    std::vector<Probe> probes();bool settled();int section();
private:
    struct State;
    HWND island_;std::atomic<HWND> window_{nullptr};std::atomic<bool> finished_{true};std::thread thread_;std::unique_ptr<State> state_;
    void run(Settings,SettingsContext,int section);
};
}
