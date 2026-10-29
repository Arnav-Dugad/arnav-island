#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus {
// 0.27 (revision 10): a phone's live activity (notice 7): a food delivery, a ride, navigation, a timer, a download, an Android 16
// live update. op 1 shown or updated, 2 ended (then only the key is set).
struct LiveActivity {
    static constexpr uint8_t Ongoing=1,Promoted=2,HasProgress=4,Indeterminate=8,CountsDown=16,Delivery=32,Navigation=64;
    int op=0;std::string key;std::wstring app,package,title,text,status;uint8_t flags=0;uint16_t progress=0;
    // Segments (relative lengths) and points (0..1000) along the progress, each with its colour (ARGB).
    struct Segment{uint16_t length=0;uint32_t argb=0;};struct Point{uint16_t position=0;uint32_t argb=0;};std::vector<Segment> segments;std::vector<Point> points;
    // when: ms since 1970 (a chronometer's base or an ETA; 0 none); accent: the notification's colour (ARGB, 0 none).
    uint64_t when=0;uint32_t accent=0;
    // Pictures as sent (PNG, the large icon PNG or JPEG, the snapshot JPEG; empty when absent).
    std::vector<uint8_t> appIcon,trackerIcon,startIcon,endIcon,largeIcon,snapshot;
    bool has(uint8_t f)const{return (flags&f)!=0;}
    // Progress 0..1 when it has any (structured, not indeterminate), else -1.
    double fraction()const{return has(HasProgress)&&!has(Indeterminate)?std::min(1.,progress/1000.):-1.;}
    // Where each segment ends along the progress (0..1), in order.
    std::vector<double> segmentEnds()const;
};
// notice: the notice's bytes from its op on ([0x30, 7] before it are skipped when present). False when malformed.
bool parseLive(const std::vector<uint8_t>& notice,LiveActivity& out);
// "12 min" (or "1 h 5 min") from a line like "Arriving in 12 mins" / "ETA 7 minutes"; empty when it says no such thing.
std::wstring liveEtaFrom(const std::wstring& line);
// What the island's ETA says for an activity at this moment (unix ms): a countdown to `when` ("12 min", "4:59"), a
// chronometer counting up from it, the minutes a line mentions, else empty. tick: how often it changes (seconds; 0 never).
std::wstring liveEta(const LiveActivity&,int64_t nowMs,double* tick=nullptr);
}
