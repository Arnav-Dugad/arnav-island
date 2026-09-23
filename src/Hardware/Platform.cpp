#include "Platform.h"
#include "Media/AppIdentity.h"
namespace nexus {
namespace {
std::wstring bios(const wchar_t* value){wchar_t text[256]{};DWORD size=sizeof(text);if(RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\BIOS",value,RRF_RT_REG_SZ,nullptr,text,&size)!=ERROR_SUCCESS)return {};return text;}
using Register=HRESULT(WINAPI*)(ULONG,void(WINAPI*)(int,void*),void*,void**);
using Unregister=HRESULT(WINAPI*)(void*);
}
PlatformInfo platformInfo(){
    PlatformInfo info;info.manufacturer=bios(L"SystemManufacturer");info.product=bios(L"SystemProductName");
    // Product names such as "ROG Strix G614JV_G614JV" repeat the model after an underscore.
    if(auto cut=info.product.find(L'_');cut!=std::wstring::npos)info.product.resize(cut);
    auto m=lower(info.manufacturer),p=lower(info.product);info.asus=m.find(L"asus")!=std::wstring::npos;info.rog=info.asus&&(p.find(L"rog")!=std::wstring::npos||p.find(L"tuf")!=std::wstring::npos);
    info.armoury=installedAppId(L"Armoury Crate");return info;
}
const wchar_t* powerModeName(int mode){switch(mode){case 0:return L"Battery saver";case 1:return L"Best power efficiency";case 2:return L"Balanced";case 3:return L"Best performance";case 4:return L"Maximum performance";case 5:return L"Game mode";case 6:return L"Mixed reality";default:return L"Unknown";}}
PowerModeWatcher::PowerModeWatcher(HWND window){
    HMODULE power=GetModuleHandleW(L"powrprof.dll");if(!power)power=LoadLibraryW(L"powrprof.dll");if(!power)return;
    auto registerFn=reinterpret_cast<Register>(reinterpret_cast<void*>(GetProcAddress(power,"PowerRegisterForEffectivePowerModeNotifications")));if(!registerFn)return;
    registerFn(2,[](int mode,void* context){PostMessageW(static_cast<HWND>(context),PowerModeMessage,WPARAM(mode),0);},window,&registration_);
}
PowerModeWatcher::~PowerModeWatcher(){if(!registration_)return;HMODULE power=GetModuleHandleW(L"powrprof.dll");if(!power)return;auto unregisterFn=reinterpret_cast<Unregister>(reinterpret_cast<void*>(GetProcAddress(power,"PowerUnregisterFromEffectivePowerModeNotifications")));if(unregisterFn)unregisterFn(registration_);}
}
