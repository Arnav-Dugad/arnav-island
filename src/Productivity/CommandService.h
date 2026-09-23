#pragma once
#include "Common/Win32.h"
#include "Interaction/DashboardModel.h"
#include "Productivity/Commands.h"
#include "Productivity/Workspaces.h"
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT CommandMessage=WM_APP+31;
// Parses what is typed into the command bar on a worker (the installed-app list
// and app icons are read there, never on the UI thread) and posts the results.
class CommandService {
    HWND window_;std::thread worker_;std::mutex mutex_;std::condition_variable wake_;bool stop_=false;
    std::wstring query_;uint64_t querySeq_=0,doneSeq_=0;std::vector<std::wstring> workspaces_;std::wstring scope_;
    std::vector<CommandResult> results_;std::vector<std::shared_ptr<const Artwork>> icons_;uint64_t resultSeq_=0;
    std::map<std::wstring,std::shared_ptr<const Artwork>> iconCache_;void run();
public:
    CommandService(HWND,std::wstring scope);~CommandService();
    void query(const std::wstring& text,std::vector<std::wstring> workspaces);
    // Latest results, the sequence they answer, and one icon per result (may be null).
    uint64_t results(std::vector<CommandResult>& out,std::vector<std::shared_ptr<const Artwork>>& icons){std::lock_guard lock(mutex_);out=results_;icons=icons_;return resultSeq_;}
};
// Visible app windows, as launchable targets (installed app ID or executable).
std::vector<WorkspaceApp> openApps(HWND self);
struct LaunchReport {int opened=0,running=0,failed=0;};
// Opens the workspace's apps that are not already running. Nothing is closed.
LaunchReport openWorkspace(const Workspace& w,HWND self);
std::shared_ptr<const Artwork> workspaceIcon(const WorkspaceApp& app);
}
