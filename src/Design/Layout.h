#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace nexus {
// Pages in the navigation bar (v14: eight, with Controls, page 7, after Stats by default).
inline constexpr int pageCount=8;
inline constexpr std::array<int,pageCount> defaultNavigation{0,1,2,7,3,5,6,4};
inline constexpr std::array<int,3> defaultMetrics{0,1,2};
// Home statistics: CPU, memory, battery, download, upload, disk, uptime, GPU and (v14) weather.
inline constexpr int metricCount=9;
inline bool validNavigation(const std::array<int,pageCount>& order){auto sorted=order;std::sort(sorted.begin(),sorted.end());for(int i=0;i<pageCount;++i)if(sorted[size_t(i)]!=i)return false;return true;}
inline void moveNavigation(std::array<int,pageCount>& order,int& slot,int direction){slot=std::clamp(slot,0,pageCount-1);int next=std::clamp(slot+direction,0,pageCount-1);std::swap(order[size_t(slot)],order[size_t(next)]);slot=next;}
// A saved seven-page order (before v14) gains Controls just after Stats.
inline std::array<int,pageCount> withControls(const std::array<int,7>& legacy){
    auto sorted=legacy;std::sort(sorted.begin(),sorted.end());for(int i=0;i<7;++i)if(sorted[size_t(i)]!=i)return defaultNavigation;
    std::array<int,pageCount> out{};size_t k=0;for(int p:legacy){out[k++]=p;if(p==2)out[k++]=7;}return out;
}
inline void cycleMetric(std::array<int,3>& metrics,int slot){for(int n=1;n<=metricCount;++n){int candidate=(metrics[slot]+n)%metricCount;bool occupied=false;for(int i=0;i<3;++i)if(i!=slot&&metrics[i]==candidate)occupied=true;if(!occupied){metrics[slot]=candidate;return;}}}
// Compact chips (v14), in the order they sit from left to right: 0 clock, 1 volume, 2 battery,
// 3 timer, 4 CPU, 5 GPU, 6 weather. Chips fill from the right, so the rightmost win when room is short.
inline constexpr int chipCount=7;
inline constexpr std::array<int,chipCount> defaultChips{6,5,4,3,2,1,0};
inline bool validChips(const std::array<int,chipCount>& order){auto sorted=order;std::sort(sorted.begin(),sorted.end());for(int i=0;i<chipCount;++i)if(sorted[size_t(i)]!=i)return false;return true;}
// The chip order as one number (base 7, slot 0 lowest), for the Settings window's single control.
inline int encodeChips(const std::array<int,chipCount>& order){int v=0,place=1;for(int c:order){v+=c*place;place*=chipCount;}return v;}
inline std::array<int,chipCount> decodeChips(int v){std::array<int,chipCount> order{};for(auto& c:order){c=((v%chipCount)+chipCount)%chipCount;v/=chipCount;}return order;}
// Moves the chip in slot `from` to slot `to`, the others closing up around it.
inline std::array<int,chipCount> moveChip(std::array<int,chipCount> order,int from,int to){from=std::clamp(from,0,chipCount-1);to=std::clamp(to,0,chipCount-1);const int c=order[size_t(from)];
    if(from<to)for(int k=from;k<to;++k)order[size_t(k)]=order[size_t(k+1)];else for(int k=from;k>to;--k)order[size_t(k)]=order[size_t(k-1)];order[size_t(to)]=c;return order;}
inline int volumeAt(double x,double left,double width){return width>0?int(std::lround(std::clamp((x-left)/width,0.,1.)*100)):0;}
inline double normalizedProgress(double value,double total){return total>0?std::clamp(value/total,0.,1.):0.;}
inline constexpr float contentX=20,contentY=38,contentWidth=380,navY=234,navHeight=44,navStep=47.5f;
}
