#pragma once
#include "Audio/SessionMixer.h"
#include <tlhelp32.h>
#include <shobjidl.h>
#include <cwctype>
#include <map>
namespace nexus {
enum class MediaService { None,YouTube,YouTubeMusic };
struct AppIdentity {std::wstring name,exe;std::shared_ptr<const Artwork> icon;bool browser=false;};
inline std::wstring lower(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
inline bool isBrowserName(const std::wstring& value){auto v=lower(value);for(auto name:{L"chrome",L"msedge",L"microsoft edge",L"firefox",L"brave",L"opera",L"vivaldi",L"chromium"})if(v.find(name)!=std::wstring::npos)return true;return v==L"arc.exe"||v==L"zen.exe";}
// The real application logo and name come from Windows: the Start menu entry for
// the AppUserModelID, or the running executable. Nothing is bundled or guessed.
inline AppIdentity resolveApp(const std::wstring& aumid){
    AppIdentity id;if(aumid.empty())return id;
    std::wstring shell=L"shell:AppsFolder\\"+aumid;id.icon=shellIcon(shell,48);
    ComPtr<IShellItem> item;if(SUCCEEDED(SHCreateItemFromParsingName(shell.c_str(),nullptr,IID_PPV_ARGS(&item)))){PWSTR name=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&name){id.name=name;CoTaskMemFree(name);}}
    // Desktop players often publish their executable name instead of an AUMID.
    auto target=lower(aumid);if(!target.ends_with(L".exe"))target+=L".exe";
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot!=INVALID_HANDLE_VALUE){PROCESSENTRY32W entry{sizeof(entry)};for(BOOL ok=Process32FirstW(snapshot,&entry);ok;ok=Process32NextW(snapshot,&entry))if(lower(entry.szExeFile)==target){std::wstring path;auto name=processName(entry.th32ProcessID,&path);id.exe=lower(entry.szExeFile);if(!id.icon&&!path.empty())id.icon=shellIcon(path,48);if(id.name.empty())id.name=name;break;}CloseHandle(snapshot);}
    if(id.name.empty()){auto bang=aumid.find(L'!');std::wstring base=bang==std::wstring::npos?aumid:aumid.substr(bang+1);if(base.size()>4&&lower(base).ends_with(L".exe"))base.resize(base.size()-4);id.name=base.size()<=32?base:L"Media";}
    id.browser=isBrowserName(id.name)||isBrowserName(id.exe)||isBrowserName(aumid);return id;
}
// A browser session carries no site identity. The active tab's window title is
// used only when it contains the playing title, so YouTube is never assumed.
inline MediaService detectService(const AppIdentity& app,const std::wstring& title){
    if(!app.browser||title.size()<3)return MediaService::None;
    struct Search{std::wstring needle;MediaService found=MediaService::None;std::map<DWORD,bool> browsers;} search{lower(title.substr(0,24))};
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot!=INVALID_HANDLE_VALUE){PROCESSENTRY32W entry{sizeof(entry)};for(BOOL ok=Process32FirstW(snapshot,&entry);ok;ok=Process32NextW(snapshot,&entry))if(isBrowserName(entry.szExeFile))search.browsers[entry.th32ProcessID]=true;CloseHandle(snapshot);}
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& s=*reinterpret_cast<Search*>(p);if(!IsWindowVisible(h))return TRUE;DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(!s.browsers.contains(pid))return TRUE;
        wchar_t text[512];int n=GetWindowTextW(h,text,512);if(n<=0)return TRUE;auto t=lower(std::wstring(text,n));if(t.find(s.needle)==std::wstring::npos)return TRUE;
        if(t.find(L"youtube music")!=std::wstring::npos){s.found=MediaService::YouTubeMusic;return FALSE;}if(t.find(L"- youtube")!=std::wstring::npos){s.found=MediaService::YouTube;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));
    return search.found;
}
}
