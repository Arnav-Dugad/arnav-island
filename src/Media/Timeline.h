#pragma once
#include <algorithm>
#include <cstdint>
namespace nexus {
// Where a player really is. Windows reports a session's position as of the moment the player last told it
// (the timeline's LastUpdatedTime), and many players, Spotify among them, tell it only now and then; so while
// playing, the position has moved on by the time since. updated and now are 100 ns ticks since 1601 (UTC, as
// FILETIME). An update from the future, none at all, or one more than six hours old is taken as it is.
inline double timelinePosition(double position,double duration,bool playing,int64_t updated,int64_t now){
    if(!playing||updated<=0||now<=updated)return position;
    const double age=double(now-updated)/1e7;if(age>6*3600)return position;
    const double moved=position+age;return duration>0?std::min(moved,duration):moved;
}
}
