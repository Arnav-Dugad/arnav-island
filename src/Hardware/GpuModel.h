#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace nexus {
// GPU busy percentage from the "GPU Engine(*)\Utilization Percentage" counters, the way
// Task Manager reads them. Instances are named per process and engine, for example
// "pid_1234_luid_0x00000000_0x0000D1B5_phys_0_eng_3_engtype_3D": the utilisation of one
// engine is the sum over processes, and the GPU is as busy as its busiest engine.
// Returns -1 when there is no engine at all.
inline double gpuBusy(const std::vector<std::pair<std::wstring,double>>& samples){
    std::map<std::wstring,double> engines;
    for(auto& [name,value]:samples){const auto at=name.find(L"luid_");if(at==std::wstring::npos||!(value>=0))continue;engines[name.substr(at)]+=value;}
    if(engines.empty())return -1;double busiest=0;for(auto& [engine,value]:engines)busiest=std::max(busiest,value);
    return std::clamp(busiest,0.,100.);
}
}
