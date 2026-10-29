#pragma once
#include <string>
#include <cwctype>
#include <functional>

namespace nexus::setup {
// 0.28: where Arnav Island lives once installed, and the names it is known by to Windows. Plain logic with no Windows
// calls, so the tests can try it with made-up registry values.
inline constexpr wchar_t runKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",runValue[]=L"ArnavIsland";
inline constexpr wchar_t uninstallKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ArnavIsland";
inline constexpr wchar_t exeName[]=L"ArnavIsland.exe",shortcutName[]=L"Arnav Island.lnk",publisher[]=L"Arnav Dugad";
// Everything a release puts in the island's folder. An uninstall removes these (and what an install or update leaves beside
// them, below) and nothing else, so a folder someone unzipped the island into keeps whatever else is in it.
inline constexpr const wchar_t* payloadNames[]={L"ArnavIsland.exe",L"ArnavIslandCamera.dll",L"LICENSE",L"THIRD_PARTY_NOTICES.md",L"QUICK_START.md"};
inline constexpr wchar_t freshSuffix[]=L".setup-new";

inline std::wstring lower(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
inline std::wstring trimmed(const std::wstring& s){const auto a=s.find_first_not_of(L" \t\r\n");if(a==std::wstring::npos)return {};return s.substr(a,s.find_last_not_of(L" \t\r\n")-a+1);}
// The folder part of a path (no trailing slash), or empty for a bare name.
inline std::wstring folderOf(const std::wstring& path){const auto at=path.find_last_of(L"\\/");return at==std::wstring::npos?std::wstring():path.substr(0,at);}
// Two folders are one when they differ only in case or a trailing slash (Windows' paths don't mind either).
inline bool sameFolder(std::wstring a,std::wstring b){while(!a.empty()&&(a.back()==L'\\'||a.back()==L'/'))a.pop_back();while(!b.empty()&&(b.back()==L'\\'||b.back()==L'/'))b.pop_back();
    for(auto* s:{&a,&b})for(auto& c:*s)if(c==L'/')c=L'\\';return !a.empty()&&lower(a)==lower(b);}

// A file in use (a running island) can be renamed, not replaced: it steps aside as name.old.ext, the name the island's own
// updater gives its exe (ArnavIsland.old.exe, which the island deletes when it next starts).
inline std::wstring asideName(const std::wstring& name){const auto dot=name.find_last_of(L'.');return dot==std::wstring::npos||dot==0?name+L".old":name.substr(0,dot)+L".old"+name.substr(dot);}

// The program a "start at sign-in" command runs: `"C:\...\ArnavIsland.exe" --startup` as the island writes it, or an older
// unquoted one. Empty unless it really is ArnavIsland.exe (a value someone pointed elsewhere isn't ours to follow).
inline std::wstring exeFromCommand(const std::wstring& command){
    std::wstring c=trimmed(command),path;
    if(!c.empty()&&c.front()==L'"'){const auto end=c.find(L'"',1);if(end==std::wstring::npos)return {};path=c.substr(1,end-1);}
    else{const auto at=lower(c).find(L".exe");if(at==std::wstring::npos)return {};path=c.substr(0,at+4);}
    path=trimmed(path);const auto leaf=path.find_last_of(L"\\/");if(leaf==std::wstring::npos||lower(path.substr(leaf+1))!=lower(exeName))return {};
    return path;
}

enum class Found{Fresh,Startup,Installed};
struct Choice{std::wstring folder;Found found=Found::Fresh;};
// Where to install: over the copy that starts at sign-in, when there is one (the one this person actually uses, wherever
// they unzipped it), else over a previous install, else the default per-user Programs folder. `exists` says whether a file
// is there (the real one asks Windows; tests make it up).
inline Choice chooseFolder(const std::wstring& runCommand,const std::wstring& installLocation,const std::wstring& defaultFolder,const std::function<bool(const std::wstring&)>& exists){
    if(const auto exe=exeFromCommand(runCommand);!exe.empty()&&exists(exe))return {folderOf(exe),Found::Startup};
    std::wstring at=trimmed(installLocation);if(at.size()>1&&at.front()==L'"'&&at.back()==L'"')at=at.substr(1,at.size()-2);
    while(at.size()>3&&(at.back()==L'\\'||at.back()==L'/'))at.pop_back();
    if(!at.empty()&&exists(at+L"\\"+exeName))return {at,Found::Installed};
    return {defaultFolder,Found::Fresh};
}
// The command that starts the island at sign-in, the same one the island's own setting writes (Settings/Startup.h).
inline std::wstring startupCommand(const std::wstring& folder){return L"\""+folder+L"\\"+exeName+L"\" --startup";}
}
