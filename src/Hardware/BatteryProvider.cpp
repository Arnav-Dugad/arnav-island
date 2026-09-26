#include "BatteryProvider.h"
#include <setupapi.h>
#include <poclass.h>
#include <fstream>
#include <vector>
namespace nexus {
namespace {
constexpr GUID batteryInterface{0x72631e54,0x78a4,0x11d0,{0xbc,0xf7,0x00,0xaa,0x00,0xb7,0xb3,0x2a}};
std::wstring queryString(HANDLE h,ULONG tag,BATTERY_QUERY_INFORMATION_LEVEL level){BATTERY_QUERY_INFORMATION q{};q.BatteryTag=tag;q.InformationLevel=level;wchar_t text[128]{};DWORD bytes=0;if(!DeviceIoControl(h,IOCTL_BATTERY_QUERY_INFORMATION,&q,sizeof(q),text,sizeof(text)-sizeof(wchar_t),&bytes,nullptr))return {};return std::wstring(text,wcsnlen(text,127));}
}
BatteryReading BatteryProvider::query(){
    BatteryReading r;SYSTEM_POWER_STATUS power{};if(GetSystemPowerStatus(&power)){r.online=power.ACLineStatus==1;if(power.BatteryLifePercent<=100&&!(power.BatteryFlag&128))r.percent=power.BatteryLifePercent;
        r.saver=power.SystemStatusFlag!=0;if(power.BatteryLifeTime!=DWORD(-1)&&power.BatteryLifeTime<48*3600)r.windowsSeconds=int(power.BatteryLifeTime);}
    HDEVINFO set=SetupDiGetClassDevsW(&batteryInterface,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);if(set==INVALID_HANDLE_VALUE)return r;
    SP_DEVICE_INTERFACE_DATA data{sizeof(data)};
    // How many batteries the PC has (the first is read in full).
    for(DWORD k=0;k<8;++k){SP_DEVICE_INTERFACE_DATA d{sizeof(d)};if(!SetupDiEnumDeviceInterfaces(set,nullptr,&batteryInterface,k,&d))break;++r.count;}
    if(SetupDiEnumDeviceInterfaces(set,nullptr,&batteryInterface,0,&data)){
        DWORD need=0;SetupDiGetDeviceInterfaceDetailW(set,&data,nullptr,0,&need,nullptr);std::vector<BYTE> buffer(std::max<DWORD>(need,sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)));
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if(SetupDiGetDeviceInterfaceDetailW(set,&data,detail,need,nullptr,nullptr)){
            HANDLE h=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(h!=INVALID_HANDLE_VALUE){
                ULONG wait=0,tag=0;DWORD bytes=0;
                if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_TAG,&wait,sizeof(wait),&tag,sizeof(tag),&bytes,nullptr)&&tag){
                    BATTERY_QUERY_INFORMATION q{};q.BatteryTag=tag;q.InformationLevel=BatteryInformation;BATTERY_INFORMATION info{};
                    if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_INFORMATION,&q,sizeof(q),&info,sizeof(info),&bytes,nullptr)&&(info.Capabilities&BATTERY_SYSTEM_BATTERY)){
                        r.present=true;r.relative=(info.Capabilities&BATTERY_CAPACITY_RELATIVE)!=0;r.designMwh=info.DesignedCapacity;r.fullMwh=info.FullChargedCapacity;r.cycles=info.CycleCount;
                        std::string chemistry(reinterpret_cast<const char*>(info.Chemistry),4);r.chemistry.assign(chemistry.begin(),std::find(chemistry.begin(),chemistry.end(),'\0'));
                        r.manufacturer=queryString(h,tag,BatteryManufactureName);r.name=queryString(h,tag,BatteryDeviceName);r.serial=queryString(h,tag,BatterySerialNumber);
                        r.warningMwh=info.DefaultAlert1;r.lowMwh=info.DefaultAlert2;r.criticalBiasMwh=info.CriticalBias;r.rechargeable=info.Technology!=0;
                        // Optional readings: many batteries report no temperature or date; each simply stays unknown.
                        {BATTERY_QUERY_INFORMATION t{};t.BatteryTag=tag;t.InformationLevel=BatteryTemperature;ULONG value=0;if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_INFORMATION,&t,sizeof(t),&value,sizeof(value),&bytes,nullptr)&&value>0&&value<4000)r.temperatureDeciK=int(value);}
                        {BATTERY_QUERY_INFORMATION t{};t.BatteryTag=tag;t.InformationLevel=BatteryManufactureDate;BATTERY_MANUFACTURE_DATE date{};if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_INFORMATION,&t,sizeof(t),&date,sizeof(date),&bytes,nullptr)&&date.Year>=1990&&date.Year<2100&&date.Month>=1&&date.Month<=12&&date.Day>=1&&date.Day<=31){r.madeYear=date.Year;r.madeMonth=date.Month;r.madeDay=date.Day;}}
                        {BATTERY_QUERY_INFORMATION t{};t.BatteryTag=tag;t.InformationLevel=BatteryEstimatedTime;ULONG value=0;if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_INFORMATION,&t,sizeof(t),&value,sizeof(value),&bytes,nullptr)&&value!=BATTERY_UNKNOWN_TIME&&value<48*3600)r.estimateSeconds=int(value);}
                        BATTERY_WAIT_STATUS request{};request.BatteryTag=tag;BATTERY_STATUS status{};
                        if(DeviceIoControl(h,IOCTL_BATTERY_QUERY_STATUS,&request,sizeof(request),&status,sizeof(status),&bytes,nullptr)){
                            r.online=(status.PowerState&BATTERY_POWER_ON_LINE)!=0;r.charging=(status.PowerState&BATTERY_CHARGING)!=0;r.critical=(status.PowerState&BATTERY_CRITICAL)!=0;
                            if(status.Capacity!=BATTERY_UNKNOWN_CAPACITY)r.remainingMwh=status.Capacity;if(status.Voltage!=BATTERY_UNKNOWN_VOLTAGE)r.voltageMv=status.Voltage;
                            if(ULONG(status.Rate)!=BATTERY_UNKNOWN_RATE)r.rateMw=status.Rate;
                            if(!r.relative&&r.fullMwh>0&&r.remainingMwh>0&&r.percent<0)r.percent=int(std::lround(100.*r.remainingMwh/r.fullMwh));}
                    }
                }
                CloseHandle(h);
            }
        }
    }
    SetupDiDestroyDeviceInfoList(set);return r;
}
BatteryProvider::BatteryProvider(HWND w,std::filesystem::path file,bool keep):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)),historyFile_(std::move(file)){
    if(!stop_||!wake_)throw std::runtime_error("Battery event creation failed");keepHistory_=keep;
    if(!historyFile_.empty()){healthFile_=historyFile_.parent_path()/L"battery-health.nexus";
        if(keep){{std::ifstream in(historyFile_);if(in)history_=BatteryHistory::read(in);}{std::ifstream in(healthFile_);if(in)health_=BatteryHealthLog::read(in);}}
        else{std::error_code ec;std::filesystem::remove(historyFile_,ec);std::filesystem::remove(healthFile_,ec);}}
    worker_=std::thread([this]{run();});
}
void BatteryProvider::saveHealth(const BatteryHealthLog& log){if(healthFile_.empty()||!keepHistory_)return;auto temp=healthFile_;temp+=L".tmp";{std::ofstream out(temp);log.write(out);}MoveFileExW(temp.c_str(),healthFile_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
BatteryProvider::~BatteryProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
void BatteryProvider::run(){
    for(;;){
        auto r=query();bool appended=false,erase=false,logged=false;BatteryHistory copy;BatteryHealthLog healthCopy;
        {std::lock_guard lock(mutex_);estimate_.observe(r);reading_=r;
            if(!keepHistory_&&(!history_.samples.empty()||!health_.days.empty())){history_=BatteryHistory{};health_=BatteryHealthLog{};erase=true;}
            if(keepHistory_&&r.percent>=0&&!historyFile_.empty()){auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();appended=history_.add(now,r.percent,r.charging);if(appended)copy=history_;
                logged=health_.add(now,r);if(logged)healthCopy=health_;}}
        if(erase&&!historyFile_.empty()){std::error_code ec;std::filesystem::remove(historyFile_,ec);std::filesystem::remove(healthFile_,ec);}
        if(logged)saveHealth(healthCopy);
        if(appended){auto temp=historyFile_;temp+=L".tmp";{std::ofstream out(temp);copy.write(out);}MoveFileExW(temp.c_str(),historyFile_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
        PostMessageW(window_,BatteryMessage,0,0);
        HANDLE h[]={stop_,wake_};if(WaitForMultipleObjects(2,h,FALSE,fast_.load()?5000:60000)==WAIT_OBJECT_0)break;
    }
}
}
