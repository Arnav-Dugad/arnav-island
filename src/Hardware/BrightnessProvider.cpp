#include "BrightnessProvider.h"
#include <wbemidl.h>
namespace nexus {
namespace {
struct Bstr {BSTR value;explicit Bstr(const wchar_t* s):value(SysAllocString(s)){}~Bstr(){SysFreeString(value);}operator BSTR()const{return value;}};
int readByte(IWbemClassObject* object,const wchar_t* name){VARIANT v;VariantInit(&v);int result=-1;if(SUCCEEDED(object->Get(name,0,&v,nullptr,nullptr))){if(v.vt==VT_UI1)result=v.bVal;else if(v.vt==VT_I4)result=v.lVal;}VariantClear(&v);return result;}
void secure(IUnknown* proxy){CoSetProxyBlanket(proxy,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);}
}
BrightnessProvider::BrightnessProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)){if(!stop_)throw std::runtime_error("Brightness event creation failed");worker_=std::thread([this]{run();});}
BrightnessProvider::~BrightnessProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);}
void BrightnessProvider::run(){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;
    {
        ComPtr<IWbemLocator> locator;ComPtr<IWbemServices> services;
        if(SUCCEEDED(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&locator)))&&SUCCEEDED(locator->ConnectServer(Bstr(L"ROOT\\WMI"),nullptr,nullptr,nullptr,0,nullptr,nullptr,&services))){
            secure(services.Get());
            ComPtr<IEnumWbemClassObject> current;
            if(SUCCEEDED(services->ExecQuery(Bstr(L"WQL"),Bstr(L"SELECT CurrentBrightness FROM WmiMonitorBrightness"),WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY,nullptr,&current))){
                secure(current.Get());ComPtr<IWbemClassObject> object;ULONG returned=0;
                if(SUCCEEDED(current->Next(3000,1,&object,&returned))&&returned){int v=readByte(object.Get(),L"CurrentBrightness");if(v>=0){value=v;available=true;PostMessageW(window_,BrightnessMessage,0,0);}}
            }
            ComPtr<IEnumWbemClassObject> events;
            if(available&&SUCCEEDED(services->ExecNotificationQuery(Bstr(L"WQL"),Bstr(L"SELECT * FROM WmiMonitorBrightnessEvent"),WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY,nullptr,&events))){
                secure(events.Get());
                while(WaitForSingleObject(stop_,0)!=WAIT_OBJECT_0){
                    ComPtr<IWbemClassObject> object;ULONG returned=0;HRESULT hr=events->Next(500,1,&object,&returned);
                    if(hr==WBEM_S_TIMEDOUT)continue;if(FAILED(hr))break;
                    if(returned){int v=readByte(object.Get(),L"Brightness");if(v>=0&&v<=100){value=v;PostMessageW(window_,BrightnessMessage,1,0);}}
                }
            }
        }
    }
    CoUninitialize();
}
}
