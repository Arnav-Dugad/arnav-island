#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace nexus {
struct ScrubGesture {
    bool active=false;double value=0,lastX=0;
    void begin(double x,double width,double lo,double hi){active=hi>lo&&width>0;lastX=x;value=active?std::clamp(x/width,0.,1.)*(hi-lo)+lo:lo;}
    void move(double x,double distanceY,double width,double lo,double hi){if(!active||width<=0||hi<=lo)return;double precision=std::abs(distanceY)>70?.12:std::abs(distanceY)>35?.35:1.;value=std::clamp(value+(x-lastX)*(hi-lo)/width*precision,lo,hi);lastX=x;}
};
struct RouteConfirmation {
    bool initialized=false;std::wstring current;
    bool observe(const std::wstring& id){bool changed=initialized&&!id.empty()&&id!=current;initialized=true;current=id;return changed;}
};
}
