#pragma once
#include "Common/Win32.h"
namespace nexus::startup {
inline constexpr auto key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline bool enabled(){DWORD bytes=0,type=0;return RegGetValueW(HKEY_CURRENT_USER,key,L"ArnavIsland",RRF_RT_REG_SZ,&type,nullptr,&bytes)==ERROR_SUCCESS&&bytes>sizeof(wchar_t);}
inline bool apply(bool enable){HKEY h=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&h,nullptr)!=ERROR_SUCCESS)return false;LSTATUS result;
    if(enable){wchar_t path[32768]{};DWORD length=GetModuleFileNameW(nullptr,path,32768);if(!length||length>=32768){RegCloseKey(h);return false;}std::wstring command=L"\""+std::wstring(path)+L"\" --startup";result=RegSetValueExW(h,L"ArnavIsland",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t)));}
    else{result=RegDeleteValueW(h,L"ArnavIsland");if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}RegCloseKey(h);return result==ERROR_SUCCESS;
}
}
