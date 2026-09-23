#include "BluetoothProvider.h"
#include <setupapi.h>
#include <devpropdef.h>
#include <dbt.h>
#include <bluetoothapis.h>
#include <bthdef.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>
#include <map>
namespace nexus {
namespace {
constexpr GUID bluetoothClass{0xe0cbf06c,0xcd8b,0x4647,{0xbb,0x8a,0x26,0x3b,0x43,0xf0,0xf9,0x74}};
constexpr GUID hciEvent{0xfc240062,0x1541,0x49be,{0xb4,0x63,0x84,0xc4,0xdc,0xd7,0xbf,0x7f}};
// Device-node properties Windows Settings uses for Bluetooth devices.
constexpr DEVPROPKEY friendlyName{{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};
constexpr DEVPROPKEY connectedKey{{0x83da6326,0x97a6,0x4088,{0x94,0x53,0xa1,0x92,0x3f,0x57,0x3b,0x29}},15};
constexpr DEVPROPKEY batteryKey{{0x104ea319,0x6ee2,0x4701,{0xbd,0x47,0x8d,0xdb,0xf4,0x25,0xbb,0xe5}},2};
constexpr DEVPROPKEY classOfDevice{{0x2bd67d8b,0x8beb,0x48d5,{0x87,0xe0,0x6c,0xda,0x34,0x28,0x04,0x0a}},10};
// Bluetooth audio one-shot connection property set (KSPROPSETID_BtAudio).
constexpr GUID btAudio{0x7fa06c40,0xb8f6,0x4c7e,{0x85,0x56,0xe8,0xc3,0x3a,0x12,0xe5,0x4d}};
struct KsIdentifier{GUID set;ULONG id,flags;};
struct KsControl:IUnknown{virtual HRESULT STDMETHODCALLTYPE KsProperty(KsIdentifier*,ULONG,void*,ULONG,ULONG*)=0;virtual HRESULT STDMETHODCALLTYPE KsMethod(KsIdentifier*,ULONG,void*,ULONG,ULONG*)=0;virtual HRESULT STDMETHODCALLTYPE KsEvent(KsIdentifier*,ULONG,void*,ULONG,ULONG*)=0;};
std::wstring upper(std::wstring s){for(auto& c:s)c=wchar_t(std::towupper(c));return s;}
template<class T> bool property(HDEVINFO set,SP_DEVINFO_DATA& d,const DEVPROPKEY& key,T& out){DEVPROPTYPE type=0;return SetupDiGetDevicePropertyW(set,&d,&key,&type,reinterpret_cast<BYTE*>(&out),sizeof(T),nullptr,0)!=FALSE;}
std::wstring stringProperty(HDEVINFO set,SP_DEVINFO_DATA& d,const DEVPROPKEY& key){wchar_t text[256]{};DEVPROPTYPE type=0;if(!SetupDiGetDevicePropertyW(set,&d,&key,&type,reinterpret_cast<BYTE*>(text),sizeof(text)-sizeof(wchar_t),nullptr,0)||type!=DEVPROP_TYPE_STRING)return {};return text;}
// Twelve hex digits identify the device in every related instance ID.
std::wstring addressIn(const std::wstring& id){auto u=upper(id);auto dev=u.find(L"DEV_");if(dev!=std::wstring::npos&&u.size()>=dev+16){auto a=u.substr(dev+4,12);if(std::all_of(a.begin(),a.end(),iswxdigit))return a;}
    for(size_t i=0;i+12<=u.size();++i){if(i>0&&iswxdigit(u[i-1]))continue;size_t n=0;while(i+n<u.size()&&iswxdigit(u[i+n]))++n;if(n==12)return u.substr(i,12);i+=n;}return {};}
void vendor(const std::wstring& id,uint32_t& vid,int& source){auto u=upper(id);auto at=u.find(L"VID&");if(at==std::wstring::npos)return;size_t n=0;while(at+4+n<u.size()&&iswxdigit(u[at+4+n]))++n;auto hex=u.substr(at+4,n);
    if(n==8){source=int(std::wcstoul(hex.substr(0,4).c_str(),nullptr,16));vid=std::wcstoul(hex.substr(4).c_str(),nullptr,16);}else if(n==6){source=int(std::wcstoul(hex.substr(0,2).c_str(),nullptr,16));vid=std::wcstoul(hex.substr(2).c_str(),nullptr,16);}}
template<class F> void each(const GUID* cls,const wchar_t* enumerator,F f){HDEVINFO set=SetupDiGetClassDevsW(cls,enumerator,nullptr,DIGCF_PRESENT|(cls?0:DIGCF_ALLCLASSES));if(set==INVALID_HANDLE_VALUE)return;SP_DEVINFO_DATA d{sizeof(d)};
    for(DWORD i=0;SetupDiEnumDeviceInfo(set,i,&d);++i){wchar_t id[512]{};if(SetupDiGetDeviceInstanceIdW(set,&d,id,512,nullptr))f(set,d,std::wstring(id));}SetupDiDestroyDeviceInfoList(set);}
}
std::vector<BluetoothDevice> BluetoothProvider::enumerate(){
    std::map<std::wstring,BluetoothDevice> byName;std::map<std::wstring,std::wstring> nameOf;
    each(&bluetoothClass,nullptr,[&](HDEVINFO set,SP_DEVINFO_DATA& d,const std::wstring& id){auto u=upper(id);if(!u.starts_with(L"BTHENUM\\DEV_")&&!u.starts_with(L"BTHLE\\DEV_"))return;
        auto name=stringProperty(set,d,friendlyName);auto address=addressIn(id);if(name.empty()||address.empty())return;auto& dev=byName[folded(name)];dev.name=name;dev.addresses.push_back(address);nameOf[address]=folded(name);
        DEVPROP_BOOLEAN connected=DEVPROP_FALSE;if(property(set,d,connectedKey,connected)&&connected==DEVPROP_TRUE)dev.connected=true;
        BYTE battery=0;if(property(set,d,batteryKey,battery)&&battery<=100)dev.battery=std::max(dev.battery,int(battery));UINT32 cod=0;if(property(set,d,classOfDevice,cod)&&cod)dev.cod=cod;});
    // Service nodes carry profile identity, vendor IDs and hands-free battery levels.
    for(auto enumerator:{L"BTHENUM",L"BTHLEDEVICE"})each(nullptr,enumerator,[&](HDEVINFO set,SP_DEVINFO_DATA& d,const std::wstring& id){auto u=upper(id);std::wstring match;for(auto& [a,n]:nameOf)if(u.find(a)!=std::wstring::npos){match=n;break;}if(match.empty())return;auto& dev=byName[match];
        if(u.find(L"{0000110B-")!=std::wstring::npos||u.find(L"{0000111E-")!=std::wstring::npos)dev.audio=true;if(!dev.vid)vendor(id,dev.vid,dev.vidSource);
        BYTE battery=0;if(property(set,d,batteryKey,battery)&&battery<=100)dev.battery=std::max(dev.battery,int(battery));});
    std::vector<BluetoothDevice> result;for(auto& [k,dev]:byName){dev.kind=deviceKind(dev.cod,dev.name);dev.brand=std::string(deviceBrand(dev.name,dev.vid,dev.vidSource));if(dev.kind==DeviceKind::Headphones||dev.kind==DeviceKind::Earbuds||dev.kind==DeviceKind::Speaker)dev.audio=true;result.push_back(std::move(dev));}
    std::stable_sort(result.begin(),result.end(),[](auto& a,auto& b){if(a.connected!=b.connected)return a.connected;return _wcsicmp(a.name.c_str(),b.name.c_str())<0;});return result;
}
HRESULT BluetoothProvider::audioConnection(const std::wstring& name,bool connect,bool probeOnly){
    ComPtr<IMMDeviceEnumerator> enumerator;HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));if(FAILED(hr))return hr;
    ComPtr<IMMDeviceCollection> endpoints;hr=enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE|DEVICE_STATE_UNPLUGGED,&endpoints);if(FAILED(hr))return hr;
    UINT count=0;endpoints->GetCount(&count);auto wanted=folded(name);
    for(UINT i=0;i<count;++i){
        ComPtr<IMMDevice> endpoint;if(FAILED(endpoints->Item(i,&endpoint)))continue;ComPtr<IPropertyStore> props;std::wstring endpointName;
        if(SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ,&props))){PROPVARIANT v;PropVariantInit(&v);if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName,&v))&&v.vt==VT_LPWSTR)endpointName=v.pwszVal;PropVariantClear(&v);}
        if(folded(endpointName).find(wanted)==std::wstring::npos)continue;
        // Endpoint -> connected KS filter, which accepts the one-shot request.
        ComPtr<IDeviceTopology> topology;ComPtr<IConnector> connector,other;ComPtr<IPart> part;ComPtr<IDeviceTopology> filterTopology;LPWSTR filterId=nullptr;ComPtr<IMMDevice> filter;ComPtr<KsControl> control;
        if(FAILED(hr=endpoint->Activate(__uuidof(IDeviceTopology),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(topology.GetAddressOf())))||FAILED(hr=topology->GetConnector(0,&connector))||FAILED(hr=connector->GetConnectedTo(&other))||FAILED(hr=other.As(&part))||FAILED(hr=part->GetTopologyObjects(&filterTopology))||FAILED(hr=filterTopology->GetDeviceId(&filterId)))continue;
        hr=enumerator->GetDevice(filterId,&filter);CoTaskMemFree(filterId);if(FAILED(hr))continue;
        static const IID ksControl{0x28f54685,0x06fd,0x11d2,{0xb2,0x7a,0x00,0xa0,0xc9,0x22,0x31,0x96}};
        if(FAILED(hr=filter->Activate(ksControl,CLSCTX_ALL,nullptr,reinterpret_cast<void**>(control.GetAddressOf()))))continue;
        if(probeOnly)return S_OK;
        KsIdentifier request{btAudio,connect?0u:1u,1u/*KSPROPERTY_TYPE_GET*/};ULONG returned=0;return control->KsProperty(&request,sizeof(request),nullptr,0,&returned);
    }
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
BluetoothProvider::BluetoothProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)){if(!stop_)throw std::runtime_error("Bluetooth event creation failed");worker_=std::thread([this]{run();});}
BluetoothProvider::~BluetoothProvider(){SetEvent(stop_);if(HWND l=listener_)PostMessageW(l,WM_NULL,0,0);if(worker_.joinable())worker_.join();CloseHandle(stop_);}
void BluetoothProvider::request(const std::wstring& name,bool connect){{std::lock_guard lock(mutex_);commands_.push_back({name,connect});}if(HWND l=listener_)PostMessageW(l,WM_APP+1,0,0);}
void BluetoothProvider::run(){
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;
    {
        // A hidden top-level window receives HCI and device-node notifications.
        WNDCLASSW wc{};wc.lpszClassName=L"ArnavIsland.BluetoothListener";wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpfnWndProc=[](HWND h,UINT m,WPARAM w,LPARAM l)->LRESULT{
            if(m==WM_DEVICECHANGE){bool relevant=w==DBT_DEVNODES_CHANGED;if(w==DBT_CUSTOMEVENT){auto* hdr=reinterpret_cast<DEV_BROADCAST_HDR*>(l);if(hdr&&hdr->dbch_devicetype==DBT_DEVTYP_HANDLE&&IsEqualGUID(reinterpret_cast<DEV_BROADCAST_HANDLE*>(l)->dbch_eventguid,hciEvent))relevant=true;}
                if(relevant)SetTimer(h,1,1200,nullptr);return TRUE;}
            return DefWindowProcW(h,m,w,l);};
        RegisterClassW(&wc);HWND listener=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);listener_=listener;
        std::vector<HANDLE> radios;std::vector<HDEVNOTIFY> notes;BLUETOOTH_FIND_RADIO_PARAMS params{sizeof(params)};HANDLE radio=nullptr;
        if(HBLUETOOTH_RADIO_FIND find=BluetoothFindFirstRadio(&params,&radio)){do{radios.push_back(radio);DEV_BROADCAST_HANDLE filter{};filter.dbch_size=sizeof(filter);filter.dbch_devicetype=DBT_DEVTYP_HANDLE;filter.dbch_handle=radio;if(auto n=RegisterDeviceNotificationW(listener,&filter,DEVICE_NOTIFY_WINDOW_HANDLE))notes.push_back(n);}while(BluetoothFindNextRadio(find,&radio));BluetoothFindRadioClose(find);}
        available=!radios.empty();
        auto refresh=[&](bool announce){auto next=enumerate();std::vector<BluetoothEvent> changes;{std::lock_guard lock(mutex_);if(announce)changes=bluetoothChanges(devices_,next);devices_=std::move(next);for(auto& c:changes)if(events_.size()<16)events_.push_back(std::move(c));}PostMessageW(window_,BluetoothMessage,0,0);};
        refresh(false);SetTimer(listener,2,60000,nullptr);
        MSG msg{};
        while(true){
            DWORD wait=MsgWaitForMultipleObjects(1,&stop_,FALSE,INFINITE,QS_ALLINPUT);if(wait==WAIT_OBJECT_0)break;
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
                if(msg.message==WM_TIMER&&msg.hwnd==listener){KillTimer(listener,msg.wParam);refresh(true);if(msg.wParam==2)SetTimer(listener,2,60000,nullptr);continue;}
                if(msg.message==WM_APP+1&&msg.hwnd==listener){std::vector<Command> commands;{std::lock_guard lock(mutex_);commands.swap(commands_);}for(auto& c:commands)commandResult=int(audioConnection(c.name,c.connect));SetTimer(listener,1,2500,nullptr);continue;}
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
        }
        for(auto n:notes)UnregisterDeviceNotification(n);for(auto r:radios)CloseHandle(r);listener_=nullptr;DestroyWindow(listener);
    }
    CoUninitialize();
}
}
