#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace nexus {
inline constexpr std::array<int,7> defaultNavigation{0,1,2,3,5,6,4};
inline constexpr std::array<int,3> defaultMetrics{0,1,2};
inline bool validNavigation(const std::array<int,7>& order){auto sorted=order;std::sort(sorted.begin(),sorted.end());return sorted==std::array<int,7>{0,1,2,3,4,5,6};}
inline void moveNavigation(std::array<int,7>& order,int& slot,int direction){slot=std::clamp(slot,0,6);int next=std::clamp(slot+direction,0,6);std::swap(order[slot],order[next]);slot=next;}
inline void cycleMetric(std::array<int,3>& metrics,int slot){for(int n=1;n<=7;++n){int candidate=(metrics[slot]+n)%7;bool occupied=false;for(int i=0;i<3;++i)if(i!=slot&&metrics[i]==candidate)occupied=true;if(!occupied){metrics[slot]=candidate;return;}}}
inline int volumeAt(double x,double left,double width){return width>0?int(std::lround(std::clamp((x-left)/width,0.,1.)*100)):0;}
inline double normalizedProgress(double value,double total){return total>0?std::clamp(value/total,0.,1.):0.;}
inline constexpr float contentX=20,contentY=38,contentWidth=380,navY=234,navHeight=44,navStep=54.285714f;
}
