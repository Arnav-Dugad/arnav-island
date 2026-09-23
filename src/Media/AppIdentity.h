#pragma once
#include "Audio/SessionMixer.h"
#include <tlhelp32.h>
#include <shobjidl.h>
#include <cwctype>
#include <map>
#include <mutex>
#include <vector>
#include <knownfolders.h>
#include <shlobj.h>
#include "Design/BrandMatch.h"
namespace nexus {
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
// AppUserModelID of an installed app found by its Start menu name (for example a
// streaming service's Store app, Xbox or Armoury Crate). Read once and cached.
inline std::wstring installedAppId(const std::wstring& name){
    static std::mutex mutex;static std::map<std::wstring,std::wstring> apps;static bool loaded=false;std::lock_guard lock(mutex);
    if(!loaded){loaded=true;ComPtr<IShellItem> folder;if(SUCCEEDED(SHCreateItemFromParsingName(L"shell:AppsFolder",nullptr,IID_PPV_ARGS(&folder)))){ComPtr<IEnumShellItems> items;
        static constexpr GUID enumItems{0x94f60519,0x2850,0x4924,{0xaa,0x5a,0xd1,0x5e,0x84,0x86,0x80,0x39}};// BHID_EnumItems
        if(SUCCEEDED(folder->BindToHandler(nullptr,enumItems,IID_PPV_ARGS(&items)))){ComPtr<IShellItem> item;ULONG fetched=0;int guard=0;while(guard++<4000&&items->Next(1,&item,&fetched)==S_OK&&fetched){PWSTR display=nullptr,id=nullptr;
            if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&display))&&SUCCEEDED(item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING,&id)))apps.emplace(lower(display),id);CoTaskMemFree(display);CoTaskMemFree(id);item.Reset();}}}}
    auto it=apps.find(lower(name));return it==apps.end()?std::wstring{}:it->second;
}
inline std::shared_ptr<const Artwork> installedAppIcon(const std::wstring& name){auto id=installedAppId(name);return id.empty()?nullptr:shellIcon(L"shell:AppsFolder\\"+id,48);}
}
