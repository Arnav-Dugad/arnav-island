#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <istream>
#include <ostream>
#include <string>
namespace nexus {
// Readings from the battery driver. Rates are milliwatts (positive while
// charging) only when the driver reports absolute capacity; relative units are
// never presented as watts or used for time estimates.
struct BatteryReading {
    bool present=false,online=false,charging=false,relative=false;int percent=-1;
    long long designMwh=0,fullMwh=0,remainingMwh=0,rateMw=0,voltageMv=0;unsigned cycles=0;std::wstring chemistry,manufacturer,name;
    // Full-charge capacity as a share of design capacity; -1 when unknown.
    double health()const{return !relative&&designMwh>0&&fullMwh>0?std::min(1.,double(fullMwh)/double(designMwh)):-1;}
};
// Smoothed rate and derived estimates. Charging slows near full, so the
// time-to-full estimate is always labelled approximate by the UI.
struct BatteryEstimate {
    double rate=0;bool valid=false;
    void observe(const BatteryReading& r){if(r.relative||!r.present||r.rateMw==0){valid=false;rate=0;return;}if(!valid||(rate>0)!=(r.rateMw>0))rate=double(r.rateMw);else rate+=(double(r.rateMw)-rate)*.35;valid=true;}
    int minutesToFull(const BatteryReading& r)const{if(!valid||!r.charging||rate<=0||r.fullMwh<=r.remainingMwh)return -1;double m=double(r.fullMwh-r.remainingMwh)/rate*60;return m>0&&m<24*60?int(std::lround(m)):-1;}
    int minutesRemaining(const BatteryReading& r)const{if(!valid||r.charging||rate>=0||r.remainingMwh<=0)return -1;double m=double(r.remainingMwh)/-rate*60;return m>0&&m<48*60?int(std::lround(m)):-1;}
};
inline std::wstring durationText(int minutes){if(minutes<0)return L"—";if(minutes<60)return std::to_wstring(minutes)+L" min";return std::to_wstring(minutes/60)+L" h "+std::to_wstring(minutes%60)+L" min";}
// A week of charge levels at five-minute spacing, stored only locally.
struct BatteryHistory {
    static constexpr size_t limit=7*24*12;struct Sample{int64_t time;int percent;bool charging;};std::deque<Sample> samples;
    bool add(int64_t now,int percent,bool charging){if(percent<0||percent>100)return false;if(!samples.empty()&&now-samples.back().time<290)return false;samples.push_back({now,percent,charging});while(samples.size()>limit)samples.pop_front();while(!samples.empty()&&now-samples.front().time>7*86400)samples.pop_front();return true;}
    void write(std::ostream& out)const{out<<"battery_history 1\n";for(auto& s:samples)out<<s.time<<' '<<s.percent<<' '<<int(s.charging)<<'\n';}
    static BatteryHistory read(std::istream& in){BatteryHistory h;std::string magic;int version=0;if(!(in>>magic>>version)||magic!="battery_history"||version!=1)return {};int64_t t;int p,c;int64_t last=0;while(in>>t>>p>>c){if(p<0||p>100||t<last)return {};last=t;h.samples.push_back({t,p,c!=0});if(h.samples.size()>limit)h.samples.pop_front();}return in.eof()?h:BatteryHistory{};}
};
}
