#include "Productivity/CommandService.h"
#include "Media/AppIdentity.h"
#include <appmodel.h>
#include <dwmapi.h>
#include <propkey.h>
#include <propsys.h>
#include <shellapi.h>
#include <set>
namespace nexus {
CommandService::CommandService(HWND w,std::wstring scope):window_(w),scope_(std::move(scope)){worker_=std::thread([this]{run();});}
CommandService::~CommandService(){{std::lock_guard lock(mutex_);stop_=true;}wake_.notify_all();if(worker_.joinable())worker_.join();}
void CommandService::query(const std::wstring& text,std::vector<std::wstring> workspaces){{std::lock_guard lock(mutex_);query_=text;workspaces_=std::move(workspaces);++querySeq_;}wake_.notify_all();}
void CommandService::run(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    installedApps();// load once, off the UI thread
    for(;;){
        std::wstring text;std::vector<std::wstring> workspaces;uint64_t seq=0;std::wstring scope;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stop_||querySeq_!=doneSeq_;});if(stop_)break;text=query_;workspaces=workspaces_;seq=querySeq_;doneSeq_=seq;scope=scope_;}
        auto results=parseCommand(text,installedApps(),workspaces,scope);std::vector<std::shared_ptr<const Artwork>> icons;
        for(auto& r:results){std::shared_ptr<const Artwork> icon;if(!r.appId.empty()){auto it=iconCache_.find(r.appId);if(it==iconCache_.end()){if(iconCache_.size()>256)iconCache_.clear();it=iconCache_.emplace(r.appId,shellIcon(L"shell:AppsFolder\\"+r.appId,48)).first;}icon=it->second;}icons.push_back(icon);}
        {std::lock_guard lock(mutex_);if(seq<resultSeq_)continue;results_=std::move(results);icons_=std::move(icons);resultSeq_=seq;}
        PostMessageW(window_,CommandMessage,0,LPARAM(seq));
    }
    CoUninitialize();
}
namespace {
// The AppUserModelID Windows groups a window under, if it has one.
std::wstring windowAppId(HWND h,DWORD pid){
    ComPtr<IPropertyStore> store;if(SUCCEEDED(SHGetPropertyStoreForWindow(h,IID_PPV_ARGS(&store)))){PROPVARIANT v;PropVariantInit(&v);if(SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID,&v))&&v.vt==VT_LPWSTR&&v.pwszVal&&*v.pwszVal){std::wstring id=v.pwszVal;PropVariantClear(&v);return id;}PropVariantClear(&v);}
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return {};UINT32 len=0;std::wstring id;
    if(GetApplicationUserModelId(p,&len,nullptr)==ERROR_INSUFFICIENT_BUFFER&&len){id.resize(len);if(GetApplicationUserModelId(p,&len,id.data())==ERROR_SUCCESS)id.resize(len?len-1:0);else id.clear();}
    CloseHandle(p);return id;
}
bool isInstalled(const std::wstring& id){auto key=lower(id);for(auto& a:installedApps())if(lower(a.id)==key)return true;return false;}
std::wstring nameFor(const std::wstring& id){auto key=lower(id);for(auto& a:installedApps())if(lower(a.id)==key)return a.name;return {};}
// Shell, input and system surfaces are never part of a workspace.
bool systemProcess(const std::wstring& exe){for(auto name:{L"explorer.exe",L"applicationframehost.exe",L"textinputhost.exe",L"shellexperiencehost.exe",L"startmenuexperiencehost.exe",L"searchhost.exe",L"lockapp.exe",L"systemsettingsbroker.exe",L"dwm.exe",L"widgets.exe",L"gamebar.exe"})if(exe==name)return true;return false;}
}
std::vector<WorkspaceApp> openApps(HWND self){
    struct Found {HWND self;std::vector<std::pair<HWND,DWORD>> windows;} found{self,{}};
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& f=*reinterpret_cast<Found*>(p);if(h==f.self||!IsWindowVisible(h)||GetWindow(h,GW_OWNER))return TRUE;
        if(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)return TRUE;BOOL cloaked=FALSE;DwmGetWindowAttribute(h,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)return TRUE;
        if(GetWindowTextLengthW(h)==0)return TRUE;DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(!pid||pid==GetCurrentProcessId())return TRUE;f.windows.push_back({h,pid});return TRUE;},reinterpret_cast<LPARAM>(&found));
    std::vector<WorkspaceApp> apps;std::set<std::wstring> seen;
    for(auto [h,pid]:found.windows){
        std::wstring path;auto name=processName(pid,&path);auto exe=lower(path.substr(path.find_last_of(L"\\/")+1));
        auto id=windowAppId(h,pid);WorkspaceApp app;
        if(!id.empty()&&isInstalled(id)){app={nameFor(id),id,true};}
        else if(!path.empty()&&!systemProcess(exe)){app={name.empty()?exe:name,path,false};}
        else continue;
        if(app.name.empty())app.name=name;if(seen.insert(lower(app.target)).second)apps.push_back(std::move(app));
        if(apps.size()>=WorkspaceStore::maxApps)break;
    }
    return apps;
}
LaunchReport openWorkspace(const Workspace& w,HWND self){
    LaunchReport report;auto open=openApps(self);std::set<std::wstring> running;for(auto& a:open)running.insert(lower(a.target));
    for(auto& a:w.apps){if(running.contains(lower(a.target))){++report.running;continue;}
        std::wstring target=a.appId?L"shell:AppsFolder\\"+a.target:a.target;
        if(!a.appId&&GetFileAttributesW(a.target.c_str())==INVALID_FILE_ATTRIBUTES){++report.failed;continue;}
        auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL));if(result>32)++report.opened;else ++report.failed;}
    return report;
}
std::shared_ptr<const Artwork> workspaceIcon(const WorkspaceApp& app){return shellIcon(app.appId?L"shell:AppsFolder\\"+app.target:app.target,48);}
}
