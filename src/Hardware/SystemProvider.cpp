#include <winsock2.h>
#include <ws2ipdef.h>
#include "SystemProvider.h"
#include <iphlpapi.h>
#include <netioapi.h>
#include <algorithm>
namespace nexus {
static uint64_t stamp(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
SystemProvider::SystemProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){if(!stop_||!wake_)throw std::runtime_error("System event creation failed");worker_=std::thread([this]{run();});}
SystemProvider::~SystemProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
void SystemProvider::run(){
    FILETIME idle{},kernel{},user{};uint64_t lastIdle=0,lastTotal=0,lastIn=0,lastOut=0;double lastTime=0;bool first=true;unsigned iteration=0;SystemSnapshot s;
    SYSTEM_INFO info{};GetNativeSystemInfo(&info);s.logicalProcessors=info.dwNumberOfProcessors;
    for(;;){
        bool active=active_.load();if(active){
            const double now=seconds();
            if(GetSystemTimes(&idle,&kernel,&user)){auto total=stamp(kernel)+stamp(user),idleValue=stamp(idle);if(lastTotal&&total>lastTotal)s.cpu=std::clamp(100.*(1.-double(idleValue-lastIdle)/double(total-lastTotal)),0.,100.);lastIdle=idleValue;lastTotal=total;}
            MEMORYSTATUSEX memory{sizeof(memory)};if(GlobalMemoryStatusEx(&memory)){s.ramPercent=memory.dwMemoryLoad;s.ramTotalGiB=memory.ullTotalPhys/1073741824.;s.ramUsedGiB=(memory.ullTotalPhys-memory.ullAvailPhys)/1073741824.;}
            PMIB_IF_TABLE2 table=nullptr;uint64_t in=0,out=0;s.networkAvailable=false;
            if(GetIfTable2(&table)==NO_ERROR){for(ULONG i=0;i<table->NumEntries;++i){auto& r=table->Table[i];if(r.OperStatus==IfOperStatusUp&&r.Type!=IF_TYPE_SOFTWARE_LOOPBACK&&r.InterfaceAndOperStatusFlags.HardwareInterface){in+=r.InOctets;out+=r.OutOctets;s.networkAvailable=true;}}FreeMibTable(table);}
            if(lastTime&&now>lastTime){s.download=in>=lastIn?double(in-lastIn)/(now-lastTime):0;s.upload=out>=lastOut?double(out-lastOut)/(now-lastTime):0;}lastIn=in;lastOut=out;lastTime=now;
            if(first||iteration++%30==0){wchar_t system[MAX_PATH]{};GetWindowsDirectoryW(system,MAX_PATH);wchar_t root[4]={system[0],L':',L'\\',0};ULARGE_INTEGER free{},total{},available{};if(GetDiskFreeSpaceExW(root,&available,&total,&free)&&total.QuadPart){s.diskTotalGiB=total.QuadPart/1073741824.;s.diskFreeGiB=free.QuadPart/1073741824.;s.diskUsedPercent=100.*(1.-double(free.QuadPart)/total.QuadPart);}}
            first=false;s.uptime=GetTickCount64()/1000;
            std::move(s.cpuHistory.begin()+1,s.cpuHistory.end(),s.cpuHistory.begin());s.cpuHistory.back()=float(std::max(0.,s.cpu));
            std::move(s.downloadHistory.begin()+1,s.downloadHistory.end(),s.downloadHistory.begin());s.downloadHistory.back()=float(s.download);s.samples=std::min(40u,s.samples+1);
            {std::lock_guard lock(mutex_);current_=s;}PostMessageW(window_,SystemMessage,0,0);
        }else{lastTime=0;lastTotal=0;}
        HANDLE handles[]={stop_,wake_};if(WaitForMultipleObjects(2,handles,FALSE,active?1000:INFINITE)==WAIT_OBJECT_0)break;
    }
}
}
