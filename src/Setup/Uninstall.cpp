#include "Setup/Install.h"
#include <commctrl.h>
#include <objbase.h>
#include <filesystem>

namespace nexus::setup {
namespace {
bool has(const std::wstring& command,const wchar_t* flag){return command.find(flag)!=std::wstring::npos;}
// A question with two answers and, optionally, a box to tick (comctl32 6's task dialog, as the island's manifest asks for).
bool ask(const wchar_t* title,const std::wstring& text,const wchar_t* yes,const wchar_t* no,const wchar_t* tick,bool* ticked){
    TASKDIALOG_BUTTON buttons[]={{100,yes},{101,no}};TASKDIALOGCONFIG c{};c.cbSize=sizeof(c);c.hInstance=GetModuleHandleW(nullptr);c.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_POSITION_RELATIVE_TO_WINDOW;
    c.pszWindowTitle=L"Arnav Island";c.pszMainIcon=MAKEINTRESOURCEW(101);
    c.pszMainInstruction=title;c.pszContent=text.c_str();c.cButtons=2;c.pButtons=buttons;c.nDefaultButton=100;c.pszVerificationText=tick;
    int pressed=0;BOOL box=FALSE;if(FAILED(TaskDialogIndirect(&c,&pressed,nullptr,&box)))return false;if(ticked)*ticked=box;return pressed==100;
}
}
int uninstall(const std::wstring& command){
    // --sandbox (tests only) removes just the files in this folder: nothing of the account's (its island, registry,
    // Start menu or settings) is touched. --quiet asks nothing, and leaves the PC's camera and the settings.
    const bool quiet=has(command,L"--quiet"),sandbox=has(command,L"--sandbox");const std::wstring folder=ownFolder(),exe=folder+L"\\"+exeName;if(folder.empty())return 1;
    bool forget=false;
    if(!quiet&&!ask(L"Uninstall Arnav Island?",L"This removes the island from "+folder+L", its Start menu shortcut and starting it at sign-in.\n\nYour settings and Shelf stay, so installing it again picks up where you left off, unless you tick the box.",
        L"Uninstall",L"Cancel",L"Also remove my settings",&forget))return 2;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(!sandbox){
        closeIsland();
        // The phone camera other apps see was set up for the whole PC (by an administrator), so it goes only if asked (and allowed).
        if(!quiet&&cameraSetUpForPc()&&ask(L"Remove the phone camera from this PC too?",L"Other apps on this PC can still pick “Arnav Island Phone Camera” until it's removed. Removing it asks for administrator permission.",L"Remove it",L"Keep it",nullptr,nullptr))
            runElevated(exe,L"--remove-camera --quiet",nullptr);
        // Each of these goes only when it's this copy's: another copy of the island elsewhere keeps its own.
        if(const auto run=exeFromCommand(readString(HKEY_CURRENT_USER,runKey,runValue));!run.empty()&&sameFolder(folderOf(run),folder)){HKEY k=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,runKey,0,KEY_SET_VALUE,&k)==ERROR_SUCCESS){RegDeleteValueW(k,runValue);RegCloseKey(k);}}
        if(const auto link=shortcutPath();!link.empty()&&fileExists(link)&&sameFolder(folderOf(shortcutTarget(link)),folder))DeleteFileW(link.c_str());
        if(const auto at=readString(HKEY_CURRENT_USER,uninstallKey,L"InstallLocation");at.empty()||sameFolder(at,folder))RegDeleteTreeW(HKEY_CURRENT_USER,uninstallKey);
        if(forget){std::error_code e;if(const auto data=userDataFolder();!data.empty())std::filesystem::remove_all(data,e);}
    }
    CoUninitialize();
    for(const std::wstring name:payloadNames){if(lower(name)!=lower(exeName))DeleteFileW((folder+L"\\"+name).c_str());DeleteFileW((folder+L"\\"+asideName(name)).c_str());DeleteFileW((folder+L"\\"+name+freshSuffix).c_str());}
    // A running program can't delete itself: a hidden command prompt waits for this one to go, then removes it and the
    // folder (rd without /s: only when nothing else was put in there). It runs from the temporary folder, not this one.
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    std::wstring line=L"cmd.exe /d /q /c \"for /l %i in (1,1,30) do (if exist \""+exe+L"\" (ping -n 2 127.0.0.1 >nul & del /f /q \""+exe+L"\" 2>nul)) & rd \""+folder+L"\" 2>nul\"";
    wchar_t cmd[MAX_PATH]{};GetSystemDirectoryW(cmd,MAX_PATH);const std::wstring shell=std::wstring(cmd)+L"\\cmd.exe";
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(CreateProcessW(shell.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,temp,&si,&pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
    return 0;
}
}
