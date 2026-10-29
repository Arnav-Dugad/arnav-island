#pragma once
#include "Setup/InstallPlan.h"
#include <windows.h>
#include <string>

namespace nexus::setup {
// 0.28: what the installer (ArnavIslandSetup) and the island's own uninstaller (ArnavIsland.exe --uninstall) both do to
// Windows: find the install, close a running island, the Start menu shortcut, the Apps list entry, start at sign-in and
// the phone camera's one administrator step.
std::wstring defaultFolder();                 // %LOCALAPPDATA%\Programs\Arnav Island
std::wstring userDataFolder();                // %LOCALAPPDATA%\ArnavIsland (settings and logs)
std::wstring ownFolder();                     // the folder this program runs from
std::wstring readString(HKEY root,const wchar_t* key,const wchar_t* value);
bool fileExists(const std::wstring& path);
Choice findInstall();
// Asks every running island (but this process) to close, waits, and ends only the ones that didn't. True when none is left.
bool closeIsland(DWORD waitMs=10000);
std::wstring shortcutPath();
std::wstring shortcutTarget(const std::wstring& link);   // needs COM on this thread
bool writeShortcut(const std::wstring& folder,const std::wstring& link=shortcutPath());   // needs COM on this thread
bool registerUninstall(const std::wstring& folder,const std::wstring& version,DWORD sizeKb);
bool enableStartup(const std::wstring& folder);
// The phone camera for other apps needs Windows 11's virtual cameras; once set up its class is in HKLM, pointing at a copy
// in Program Files (PhoneCamera::setUp). `dll` gets that copy's path.
bool cameraSupported();
bool cameraSetUpForPc(std::wstring* dll=nullptr);
bool startIsland(const std::wstring& folder,const std::wstring& args=L"");
enum class Elevated{Done,Declined,Failed};
// Runs a program as administrator and waits for it (a declined prompt is Declined, not Failed). `code` gets its exit code.
Elevated runElevated(const std::wstring& exe,const std::wstring& args,HWND owner,DWORD* code=nullptr);
// The Apps list keeps saying what's installed after the island updates itself: called by the island at start.
void refreshInstalledVersion(const wchar_t* version);
// ArnavIsland.exe --uninstall [--quiet] [--sandbox] (Uninstall.cpp, in the island only).
int uninstall(const std::wstring& command);
}
