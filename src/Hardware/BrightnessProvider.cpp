#include "BrightnessProvider.h"
#include <wbemidl.h>
namespace nexus {
namespace {
struct Bstr {BSTR value;explicit Bstr(const wchar_t* s):value(SysAllocString(s)){}~Bstr(){SysFreeString(value);}operator BSTR()const{return value;}};
int readByte(IWbemClassObject* object,const wchar_t* name){VARIANT v;VariantInit(&v);int result=-1;if(SUCCEEDED(object->Get(name,0,&v,nullptr,nullptr))){if(v.vt==VT_UI1)result=v.bVal;else if(v.vt==VT_I4)result=v.lVal;}VariantClear(&v);return result;}
void secure(IUnknown* proxy){CoSetProxyBlanket(proxy,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);}
}
BrightnessProvider::BrightnessProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){if(!stop_||!wake_)throw std::runtime_error("Brightness event creation failed");worker_=std::thread([this]{run();});setter_=std::thread([this]{setLoop();});}
BrightnessProvider::~BrightnessProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();if(setter_.joinable())setter_.join();CloseHandle(stop_);CloseHandle(wake_);}
void BrightnessProvider::setLoop(){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;
    {
        ComPtr<IWbemServices> services;ComPtr<IWbemClassObject> input;std::wstring path;bool tried=false;
        // Connects once, on the first request: the monitor's WmiMonitorBrightnessMethods instance and its method's arguments.
        auto connect=[&]{if(input)return true;if(tried)return false;tried=true;ComPtr<IWbemLocator> locator;
            if(FAILED(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&locator)))||FAILED(locator->ConnectServer(Bstr(L"ROOT\\WMI"),nullptr,nullptr,nullptr,0,nullptr,nullptr,&services)))return false;secure(services.Get());
            ComPtr<IEnumWbemClassObject> found;if(FAILED(services->ExecQuery(Bstr(L"WQL"),Bstr(L"SELECT * FROM WmiMonitorBrightnessMethods"),WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY,nullptr,&found)))return false;secure(found.Get());
            ComPtr<IWbemClassObject> instance;ULONG returned=0;if(FAILED(found->Next(3000,1,&instance,&returned))||!returned)return false;
            VARIANT v;VariantInit(&v);if(FAILED(instance->Get(L"__PATH",0,&v,nullptr,nullptr))||v.vt!=VT_BSTR){VariantClear(&v);return false;}path=v.bstrVal;VariantClear(&v);
            ComPtr<IWbemClassObject> type,definition;if(FAILED(services->GetObject(Bstr(L"WmiMonitorBrightnessMethods"),0,nullptr,&type,nullptr))||FAILED(type->GetMethod(L"WmiSetBrightness",0,&definition,nullptr))||!definition)return false;
            return SUCCEEDED(definition->SpawnInstance(0,&input));};
        HANDLE waits[]={stop_,wake_};
        while(WaitForMultipleObjects(2,waits,FALSE,INFINITE)==WAIT_OBJECT_0+1){const int v=wanted.exchange(-1);if(v<0||!connect())continue;
            VARIANT timeout;VariantInit(&timeout);timeout.vt=VT_I4;timeout.lVal=0;VARIANT level;VariantInit(&level);level.vt=VT_UI1;level.bVal=BYTE(v);
            if(SUCCEEDED(input->Put(L"Timeout",0,&timeout,0))&&SUCCEEDED(input->Put(L"Brightness",0,&level,0)))services->ExecMethod(Bstr(path.c_str()),Bstr(L"WmiSetBrightness"),0,nullptr,input.Get(),nullptr,nullptr);}
    }
    CoUninitialize();
}
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
