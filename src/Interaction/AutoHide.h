#pragma once
#include <cmath>
namespace nexus {
// Keeps the island tucked away until the pointer touches its screen edge.
// Revealing needs the edge itself (so passing near the island does nothing);
// once shown it stays while the pointer is on the island or the edge band, and
// hides again after the pointer has been away for `delay` seconds.
struct AutoHide {
    bool hidden=false;double awaySince=-1;
    bool update(bool enabled,bool engaged,bool atEdge,bool overIsland,double now,double delay){
        if(!enabled||engaged){hidden=false;awaySince=-1;return hidden;}
        if(hidden){if(atEdge){hidden=false;awaySince=-1;}return hidden;}
        if(atEdge||overIsland){awaySince=-1;return hidden;}
        if(awaySince<0)awaySince=now;if(now-awaySince>=delay)hidden=true;return hidden;
    }
};
// Pointer on the docked edge within the island's band. Top dock: the top pixel
// rows; right dock: the rightmost columns. Coordinates are physical pixels.
inline bool atIslandEdge(long x,long y,long left,long top,long right,long bottom,int edge,double center,double halfSpan,int thickness=2){
    if(edge==0)return y>=top&&y<top+thickness&&x>=left&&x<right&&std::abs(x-center)<=halfSpan;
    return x<right&&x>=right-thickness&&y>=top&&y<bottom&&std::abs(y-center)<=halfSpan;
}
}
