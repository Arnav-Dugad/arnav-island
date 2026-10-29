#include "Setup/Install.h"
#include "Camera/CameraPipe.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <ctime>
#include <vector>

namespace nexus::setup {
using Microsoft::WRL::ComPtr;
namespace {
std::wstring knownFolder(const KNOWNFOLDERID& id){PWSTR p=nullptr;std::wstring s;if(SUCCEEDED(SHGetKnownFolderPath(id,KF_FLAG_CREATE,nullptr,&p))&&p)s=p;CoTaskMemFree(p);return s;}
bool setString(HKEY k,const wchar_t* name,const std::wstring& v){return RegSetValueExW(k,name,0,REG_SZ,reinterpret_cast<const BYTE*>(v.c_str()),DWORD((v.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;}
bool setNumber(HKEY k,const wchar_t* name,DWORD v){return RegSetValueExW(k,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&v),sizeof(v))==ERROR_SUCCESS;}
const std::wstring cameraClassKey=std::wstring(L"Software\\Classes\\CLSID\\")+camera::sourceClsidText+L"\\InprocServer32";
}

std::wstring defaultFolder(){const auto programs=knownFolder(FOLDERID_UserProgramFiles);return programs.empty()?std::wstring():programs+L"\\Arnav Island";}
std::wstring userDataFolder(){const auto local=knownFolder(FOLDERID_LocalAppData);return local.empty()?std::wstring():local+L"\\ArnavIsland";}
std::wstring ownFolder(){std::wstring path(32768,L'\0');const DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));path.resize(n<path.size()?n:0);return folderOf(path);}
std::wstring readString(HKEY root,const wchar_t* key,const wchar_t* value){
    DWORD bytes=0;if(RegGetValueW(root,key,value,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,nullptr,&bytes)!=ERROR_SUCCESS||bytes<sizeof(wchar_t))return {};
    std::wstring s(bytes/sizeof(wchar_t),L'\0');if(RegGetValueW(root,key,value,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,s.data(),&bytes)!=ERROR_SUCCESS)return {};
    s.resize(wcslen(s.c_str()));return s;
}
bool fileExists(const std::wstring& path){const DWORD a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);}
Choice findInstall(){return chooseFolder(readString(HKEY_CURRENT_USER,runKey,runValue),readString(HKEY_CURRENT_USER,uninstallKey,L"InstallLocation"),defaultFolder(),fileExists);}

bool closeIsland(DWORD waitMs){
    // The island's window says it's running (one per sign-in, but a test copy may sit beside it): each is asked to close,
    // the way its tray menu's Quit does, so it saves the Shelf and its clipboard history on the way out.
    std::vector<HANDLE> running;HWND at=nullptr;
    while((at=FindWindowExW(nullptr,at,L"ArnavIsland.Surface",nullptr))!=nullptr){DWORD pid=0;GetWindowThreadProcessId(at,&pid);if(!pid||pid==GetCurrentProcessId())continue;
        if(HANDLE p=OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE,FALSE,pid)){running.push_back(p);PostMessageW(at,WM_CLOSE,0,0);}}
    bool gone=true;const ULONGLONG until=GetTickCount64()+waitMs;
    for(HANDLE p:running){const ULONGLONG now=GetTickCount64();if(WaitForSingleObject(p,now<until?DWORD(until-now):0)!=WAIT_OBJECT_0){
        // Only one that didn't close in time (stuck, or busy with a dialog) is ended.
        TerminateProcess(p,1);gone=WaitForSingleObject(p,5000)==WAIT_OBJECT_0&&gone;}CloseHandle(p);}
    return gone;
}

std::wstring shortcutPath(){const auto programs=knownFolder(FOLDERID_Programs);return programs.empty()?std::wstring():programs+L"\\"+shortcutName;}
std::wstring shortcutTarget(const std::wstring& link){
    ComPtr<IShellLinkW> shell;ComPtr<IPersistFile> file;wchar_t path[MAX_PATH]{};
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&shell)))||FAILED(shell.As(&file))||FAILED(file->Load(link.c_str(),STGM_READ)))return {};
    if(FAILED(shell->GetPath(path,MAX_PATH,nullptr,SLGP_RAWPATH)))return {};
    // A raw path may keep %variables% (the shell's own links do): spelled out, to compare with a folder.
    wchar_t full[MAX_PATH]{};const DWORD n=ExpandEnvironmentStringsW(path,full,MAX_PATH);return std::wstring(n&&n<=MAX_PATH?full:path);
}
bool writeShortcut(const std::wstring& folder,const std::wstring& link){
    const auto exe=folder+L"\\"+exeName;if(link.empty())return false;
    ComPtr<IShellLinkW> shell;ComPtr<IPersistFile> file;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&shell))))return false;
    shell->SetPath(exe.c_str());shell->SetWorkingDirectory(folder.c_str());shell->SetIconLocation(exe.c_str(),0);
    shell->SetDescription(L"The island at the top of your screen");
    return SUCCEEDED(shell.As(&file))&&SUCCEEDED(file->Save(link.c_str(),TRUE));
}

bool registerUninstall(const std::wstring& folder,const std::wstring& version,DWORD sizeKb){
    HKEY k=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,uninstallKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return false;
    const std::wstring exe=L"\""+folder+L"\\"+exeName+L"\"";wchar_t today[16]{};const time_t t=time(nullptr);tm local{};localtime_s(&local,&t);wcsftime(today,16,L"%Y%m%d",&local);
    // Uninstall runs the island itself (--uninstall), so the uninstaller is always the version that's there, updates and all.
    bool ok=setString(k,L"DisplayName",L"Arnav Island")&&setString(k,L"DisplayVersion",version)&&setString(k,L"Publisher",publisher)&&setString(k,L"DisplayIcon",exe+L",0")&&
        setString(k,L"InstallLocation",folder)&&setString(k,L"UninstallString",exe+L" --uninstall")&&setString(k,L"QuietUninstallString",exe+L" --uninstall --quiet")&&
        setString(k,L"URLInfoAbout",L"https://github.com/Arnav-Dugad/arnav-island")&&setString(k,L"InstallDate",today)&&
        setNumber(k,L"EstimatedSize",sizeKb)&&setNumber(k,L"NoModify",1)&&setNumber(k,L"NoRepair",1);
    RegCloseKey(k);return ok;
}
bool enableStartup(const std::wstring& folder){
    HKEY k=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,runKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return false;
    const bool ok=setString(k,runValue,startupCommand(folder));RegCloseKey(k);return ok;
}

bool cameraSupported(){
    using Get=LONG(WINAPI*)(OSVERSIONINFOW*);OSVERSIONINFOW v{sizeof(v)};auto get=reinterpret_cast<Get>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion")));
    if(!get||get(&v)!=0||v.dwBuildNumber<22000)return false;
    HMODULE m=LoadLibraryExW(L"mfsensorgroup.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!m)return false;
    const bool there=GetProcAddress(m,"MFCreateVirtualCamera")!=nullptr;FreeLibrary(m);return there;
}
bool cameraSetUpForPc(std::wstring* dll){const auto path=readString(HKEY_LOCAL_MACHINE,cameraClassKey.c_str(),nullptr);if(dll)*dll=path;return !path.empty()&&fileExists(path);}

bool startIsland(const std::wstring& folder,const std::wstring& args){
    const std::wstring exe=folder+L"\\"+exeName;std::wstring line=L"\""+exe+L"\""+(args.empty()?L"":L" "+args);
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,0,nullptr,folder.c_str(),&si,&pi))return false;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return true;
}
Elevated runElevated(const std::wstring& exe,const std::wstring& args,HWND owner,DWORD* code){
    SHELLEXECUTEINFOW e{};e.cbSize=sizeof(e);e.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;e.hwnd=owner;e.lpVerb=L"runas";e.lpFile=exe.c_str();e.lpParameters=args.c_str();e.nShow=SW_HIDE;
    if(!ShellExecuteExW(&e)||!e.hProcess)return GetLastError()==ERROR_CANCELLED?Elevated::Declined:Elevated::Failed;
    WaitForSingleObject(e.hProcess,5*60*1000);DWORD exit=1;GetExitCodeProcess(e.hProcess,&exit);CloseHandle(e.hProcess);if(code)*code=exit;
    return exit==0?Elevated::Done:Elevated::Failed;
}

void refreshInstalledVersion(const wchar_t* version){
    const auto at=readString(HKEY_CURRENT_USER,uninstallKey,L"InstallLocation");if(at.empty()||!sameFolder(at,ownFolder()))return;
    if(readString(HKEY_CURRENT_USER,uninstallKey,L"DisplayVersion")==version)return;
    HKEY k=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,uninstallKey,0,KEY_SET_VALUE,&k)!=ERROR_SUCCESS)return;setString(k,L"DisplayVersion",version);RegCloseKey(k);
}
}
