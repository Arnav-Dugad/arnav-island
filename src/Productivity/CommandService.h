#pragma once
#include "Common/Win32.h"
#include "Interaction/DashboardModel.h"
#include "Productivity/Commands.h"
#include "Productivity/CommandMemory.h"
#include "Productivity/FileSearch.h"
#include "Productivity/Workspaces.h"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT CommandMessage=WM_APP+31;// wParam 0: results ready; 1: exchange rates arrived, query again
// What the island is doing, for the empty bar's suggestions.
struct CommandContext {bool media=false,playing=false,muted=false,timer=false,micMuted=false;std::wstring track;int hour=12;};
// Parses what is typed into the command bar on a worker (the installed-app list,
// app and file icons, the search index, radios and the recycle bin are read there,
// never on the UI thread) and posts the results. Results for a query can arrive
// twice: commands first, then the same list with matching files added.
class CommandService {
    HWND window_;std::thread worker_;std::mutex mutex_;std::condition_variable wake_;bool stop_=false;
    std::wstring query_;uint64_t querySeq_=0,doneSeq_=0;std::vector<std::wstring> workspaces_;std::wstring scope_;std::filesystem::path data_;
    CommandContext context_;std::vector<RememberedCommand> memory_;bool currency_=false,refresh_=false;
    std::vector<CommandResult> results_;std::vector<std::shared_ptr<const Artwork>> icons_;uint64_t resultSeq_=0;
    std::map<std::wstring,std::shared_ptr<const Artwork>> iconCache_;void run();
    // Worker-only state.
    CommandEnv env_;double stateAt_=-100;std::wstring home_;
    struct Rates;std::shared_ptr<Rates> rates_;
    struct Index;std::unique_ptr<Index> index_;
    std::optional<std::vector<CommandResult>> indexed(const FileSpec&,const std::wstring& root,size_t count,const CommandMemory&);
    std::vector<CommandResult> scanned(const FileSpec&,const std::wstring& root,size_t count,const CommandMemory&);
    std::vector<CommandResult> files(const FileSpec&,size_t count,const CommandMemory&,bool& fromIndex);
    std::vector<CommandResult> idle(const CommandContext&,const CommandMemory&,const std::vector<std::wstring>& workspaces);
    void refreshState();void wantRates();
    std::shared_ptr<const Artwork> icon(const CommandResult&);
    bool superseded(uint64_t seq){std::lock_guard lock(mutex_);return stop_||querySeq_!=seq;}
    void publish(uint64_t seq,std::vector<CommandResult>);
public:
    CommandService(HWND,std::wstring scope,std::filesystem::path data);~CommandService();
    void query(const std::wstring& text,std::vector<std::wstring> workspaces,CommandContext context,std::vector<RememberedCommand> memory,bool currency,bool refreshState=false);
    // Latest results, the sequence they answer, and one icon per result (may be null).
    uint64_t results(std::vector<CommandResult>& out,std::vector<std::shared_ptr<const Artwork>>& icons){std::lock_guard lock(mutex_);out=results_;icons=icons_;return resultSeq_;}
};
// Visible app windows, as launchable targets (installed app ID or executable).
std::vector<WorkspaceApp> openApps(HWND self);
struct LaunchReport {int opened=0,running=0,failed=0;};
// Opens the workspace's apps that are not already running. Nothing is closed.
LaunchReport openWorkspace(const Workspace& w,HWND self);
std::shared_ptr<const Artwork> workspaceIcon(const WorkspaceApp& app);
// Windows.Devices.Radios: Wi-Fi (1), Bluetooth (2) or both (3) on or off. Returns how many
// radios changed, -1 when Windows refused, -2 when there is no such radio. Blocks; call off the UI thread.
int setRadios(int which,bool on);
// Whether any Wi-Fi (1) or Bluetooth (2) radio is on: 1, 0, -1 unknown, -2 none. Blocks briefly.
int radioOn(int which);
// 1 dark, 0 light, -1 unknown (the app mode Windows Settings shows).
int darkModeNow();
// The Colors page's "Choose your mode": apps and Windows together; then tells running apps.
bool setDarkMode(bool dark);
}
