#include <winsock2.h>
#include <ws2ipdef.h>
#include "SystemProvider.h"
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>
#include <vector>
#include "Hardware/GpuModel.h"
namespace nexus {
static uint64_t stamp(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
namespace cores {
// Each logical processor's idle, kernel and user times (NtQuerySystemInformation's processor performance class).
struct Times{LARGE_INTEGER idle,kernel,user,reserved[2];ULONG interrupts;};
using Query=LONG(WINAPI*)(ULONG,PVOID,ULONG,PULONG);
Query query(){static const Query q=[]{HMODULE nt=GetModuleHandleW(L"ntdll.dll");return nt?reinterpret_cast<Query>(reinterpret_cast<void*>(GetProcAddress(nt,"NtQuerySystemInformation"))):nullptr;}();return q;}
// Busy percentages since the last call (empty the first time, or when Windows won't say).
std::vector<float> sample(std::vector<std::pair<uint64_t,uint64_t>>& last,unsigned count){
    std::vector<float> out;const Query q=query();if(!q||count==0)return out;count=std::min(count,64u);
    std::vector<Times> t(count);ULONG got=0;if(q(8,t.data(),ULONG(t.size()*sizeof(Times)),&got)<0)return out;
    const size_t n=std::min<size_t>(count,got/sizeof(Times));const bool first=last.size()!=n;if(first)last.assign(n,{0,0});
    for(size_t i=0;i<n;++i){const uint64_t total=uint64_t(t[i].kernel.QuadPart)+uint64_t(t[i].user.QuadPart),idle=uint64_t(t[i].idle.QuadPart);
        if(!first&&total>last[i].first)out.push_back(float(std::clamp(100.*(1.-double(idle-last[i].second)/double(total-last[i].first)),0.,100.)));
        last[i]={total,idle};}
    if(out.size()!=n)out.clear();return out;
}
}
SystemProvider::SystemProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){if(!stop_||!wake_)throw std::runtime_error("System event creation failed");worker_=std::thread([this]{run();});}
SystemProvider::~SystemProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
void SystemProvider::run(){
    FILETIME idle{},kernel{},user{};uint64_t lastIdle=0,lastTotal=0,lastIn=0,lastOut=0;double lastTime=0;bool first=true;unsigned iteration=0;SystemSnapshot s;std::vector<std::pair<uint64_t,uint64_t>> lastCores;
    SYSTEM_INFO info{};GetNativeSystemInfo(&info);s.logicalProcessors=info.dwNumberOfProcessors;
    // GPU engines through the performance counters (no admin rights needed); a rate needs two samples.
    PDH_HQUERY query=nullptr;PDH_HCOUNTER engines=nullptr;bool gpuCounters=false;
    if(PdhOpenQueryW(nullptr,0,&query)==ERROR_SUCCESS){if(PdhAddEnglishCounterW(query,L"\\GPU Engine(*)\\Utilization Percentage",0,&engines)==ERROR_SUCCESS&&PdhCollectQueryData(query)==ERROR_SUCCESS)gpuCounters=true;else{PdhCloseQuery(query);query=nullptr;}}
    for(;;){
        bool active=active_.load();if(active){
            const double now=seconds();
            if(GetSystemTimes(&idle,&kernel,&user)){auto total=stamp(kernel)+stamp(user),idleValue=stamp(idle);if(lastTotal&&total>lastTotal)s.cpu=std::clamp(100.*(1.-double(idleValue-lastIdle)/double(total-lastTotal)),0.,100.);lastIdle=idleValue;lastTotal=total;}
            s.cores=cores::sample(lastCores,s.logicalProcessors);
            MEMORYSTATUSEX memory{sizeof(memory)};if(GlobalMemoryStatusEx(&memory)){s.ramPercent=memory.dwMemoryLoad;s.ramTotalGiB=memory.ullTotalPhys/1073741824.;s.ramUsedGiB=(memory.ullTotalPhys-memory.ullAvailPhys)/1073741824.;}
            PMIB_IF_TABLE2 table=nullptr;uint64_t in=0,out=0;s.networkAvailable=false;
            if(GetIfTable2(&table)==NO_ERROR){for(ULONG i=0;i<table->NumEntries;++i){auto& r=table->Table[i];if(r.OperStatus==IfOperStatusUp&&r.Type!=IF_TYPE_SOFTWARE_LOOPBACK&&r.InterfaceAndOperStatusFlags.HardwareInterface){in+=r.InOctets;out+=r.OutOctets;s.networkAvailable=true;}}FreeMibTable(table);}
            if(lastTime&&now>lastTime){s.download=in>=lastIn?double(in-lastIn)/(now-lastTime):0;s.upload=out>=lastOut?double(out-lastOut)/(now-lastTime):0;}lastIn=in;lastOut=out;lastTime=now;
            if(first||iteration++%30==0){wchar_t system[MAX_PATH]{};GetWindowsDirectoryW(system,MAX_PATH);wchar_t root[4]={system[0],L':',L'\\',0};ULARGE_INTEGER free{},total{},available{};if(GetDiskFreeSpaceExW(root,&available,&total,&free)&&total.QuadPart){s.diskTotalGiB=total.QuadPart/1073741824.;s.diskFreeGiB=free.QuadPart/1073741824.;s.diskUsedPercent=100.*(1.-double(free.QuadPart)/total.QuadPart);}}
            if(gpuCounters&&PdhCollectQueryData(query)==ERROR_SUCCESS){DWORD bytes=0,count=0;
                if(PdhGetFormattedCounterArrayW(engines,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,&bytes,&count,nullptr)==PDH_MORE_DATA&&bytes){std::vector<BYTE> buffer(bytes);auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
                    if(PdhGetFormattedCounterArrayW(engines,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,&bytes,&count,items)==ERROR_SUCCESS){std::vector<std::pair<std::wstring,double>> samples;samples.reserve(count);
                        for(DWORD k=0;k<count;++k)if(items[k].FmtValue.CStatus==PDH_CSTATUS_VALID_DATA||items[k].FmtValue.CStatus==PDH_CSTATUS_NEW_DATA)samples.push_back({items[k].szName?items[k].szName:L"",items[k].FmtValue.doubleValue});
                        s.gpu=gpuBusy(samples);}}}
            first=false;s.uptime=GetTickCount64()/1000;
            std::move(s.cpuHistory.begin()+1,s.cpuHistory.end(),s.cpuHistory.begin());s.cpuHistory.back()=float(std::max(0.,s.cpu));
            std::move(s.gpuHistory.begin()+1,s.gpuHistory.end(),s.gpuHistory.begin());s.gpuHistory.back()=float(std::max(0.,s.gpu));
            std::move(s.downloadHistory.begin()+1,s.downloadHistory.end(),s.downloadHistory.begin());s.downloadHistory.back()=float(s.download);s.samples=std::min(40u,s.samples+1);
            {std::lock_guard lock(mutex_);current_=s;}PostMessageW(window_,SystemMessage,0,0);
        }else{lastTime=0;lastTotal=0;lastCores.clear();s.cores.clear();}
        HANDLE handles[]={stop_,wake_};if(WaitForMultipleObjects(2,handles,FALSE,active?1000:INFINITE)==WAIT_OBJECT_0)break;
    }
    if(query)PdhCloseQuery(query);
}
}
