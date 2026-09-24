#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
namespace nexus {
// `raw` follows the pointer; `value` is what a release would seek to (raw, or a detent it
// has snapped to). `precision` is the current pointer gain.
struct ScrubGesture {
    bool active=false;double value=0,raw=0,lastX=0,precision=1;
    void begin(double x,double width,double lo,double hi){active=hi>lo&&width>0;lastX=x;precision=1;value=raw=active?std::clamp(x/width,0.,1.)*(hi-lo)+lo:lo;}
    void move(double x,double distanceY,double width,double lo,double hi){if(!active||width<=0||hi<=lo)return;precision=std::abs(distanceY)>70?.12:std::abs(distanceY)>35?.35:1.;raw=std::clamp(raw+(x-lastX)*(hi-lo)/width*precision,lo,hi);value=raw;lastX=x;}
};
// Seek detents: evenly spaced time marks, as fine as 10 s while they stay at least 12 DIPs
// apart on the track, plus any extra marks (lyric line starts).
inline double detentStep(double duration,double width){if(!(duration>0)||!(width>0))return 0;for(double step:{10.,15.,30.,60.,120.,300.,600.,1800.})if(step*width/duration>=12)return step;return 0;}
inline std::vector<double> seekDetents(double duration,double width,const std::vector<double>& extra={}){
    std::vector<double> marks;if(!(duration>0))return marks;const double step=detentStep(duration,width);if(step>0)for(double t=step;t<duration-.5;t+=step)marks.push_back(t);
    for(double t:extra)if(t>.5&&t<duration-.5)marks.push_back(t);std::sort(marks.begin(),marks.end());return marks;
}
// The nearest mark within `tolerance`, or `value` itself. `snapped` reports which.
inline double snapToDetent(double value,const std::vector<double>& marks,double tolerance,bool* snapped=nullptr){
    double best=value,gap=tolerance;bool hit=false;auto it=std::lower_bound(marks.begin(),marks.end(),value);
    for(auto c:{it,it==marks.begin()?it:it-1})if(c!=marks.end()&&std::abs(*c-value)<=gap){gap=std::abs(*c-value);best=*c;hit=true;}
    if(snapped)*snapped=hit;return best;
}
// Double-click skip, kept inside what the player allows.
inline double skipTarget(double position,double delta,double lo,double hi){return hi>lo?std::clamp(position+delta,lo,hi):position;}
struct RouteConfirmation {
    bool initialized=false;std::wstring current;
    bool observe(const std::wstring& id){bool changed=initialized&&!id.empty()&&id!=current;initialized=true;current=id;return changed;}
};
}
