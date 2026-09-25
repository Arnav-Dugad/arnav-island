#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <istream>
#include <ostream>
#include <optional>
#include <string>
#include <vector>
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
// Phase 5F: the battery's full-charge capacity once a day (about a year of days), kept only on
// this PC, for the health trend; and when the weekly card was last shown.
struct BatteryHealthLog {
    struct Day{int64_t day=0;long long full=0,design=0;unsigned cycles=0;bool operator==(const Day&)const=default;};
    static constexpr size_t limit=400;std::vector<Day> days;int64_t lastCard=0;
    static double health(const Day& d){return d.design>0&&d.full>0?std::min(1.,double(d.full)/double(d.design)):-1;}
    // One reading a day, the first of the day; readings in relative units carry no capacity.
    bool add(int64_t unixTime,const BatteryReading& r){if(r.relative||r.fullMwh<=0||r.designMwh<=0)return false;const int64_t day=unixTime/86400;if(!days.empty()&&days.back().day>=day)return false;
        days.push_back({day,r.fullMwh,r.designMwh,r.cycles});while(days.size()>limit)days.erase(days.begin());return true;}
    // Health on the latest day and about a week before it (a day 5 to 10 days earlier).
    std::optional<std::pair<double,double>> week()const{if(days.size()<2)return std::nullopt;const auto& last=days.back();
        for(auto it=days.rbegin()+1;it!=days.rend();++it){const int64_t gap=last.day-it->day;if(gap>10)break;if(gap>=5)return std::pair{health(*it),health(last)};}return std::nullopt;}
    void write(std::ostream& out)const{out<<"battery_health 1\ncard "<<lastCard<<'\n';for(auto& d:days)out<<d.day<<' '<<d.full<<' '<<d.design<<' '<<d.cycles<<'\n';}
    static BatteryHealthLog read(std::istream& in){BatteryHealthLog h;std::string magic,word;int version=0;if(!(in>>magic>>version>>word>>h.lastCard)||magic!="battery_health"||version!=1||word!="card")return {};
        Day d;int64_t last=-1;while(in>>d.day>>d.full>>d.design>>d.cycles){if(d.day<=last||d.full<=0||d.design<=0)return {};last=d.day;h.days.push_back(d);if(h.days.size()>limit)h.days.erase(h.days.begin());}return in.eof()?h:BatteryHealthLog{};}
};
// The last seven days of charge history: how often a charge began, how much of the battery an
// average day used while on battery, and the hours spent on battery. usedPerDay is -1 without data.
struct BatteryWeek {int charges=0;double usedPerDay=-1,hoursOnBattery=0,days=0;};
inline BatteryWeek summarizeWeek(const BatteryHistory& h,int64_t now){
    BatteryWeek w;const BatteryHistory::Sample* prev=nullptr;double drop=0;int64_t first=0;
    for(auto& s:h.samples){if(now-s.time>7*86400||s.time>now)continue;if(!first)first=s.time;
        // Samples more than 20 minutes apart (asleep, switched off) are not counted as use.
        if(prev){const int64_t gap=s.time-prev->time;if(!prev->charging&&s.charging)++w.charges;if(gap<=20*60&&!prev->charging&&!s.charging){if(prev->percent>s.percent)drop+=prev->percent-s.percent;w.hoursOnBattery+=double(gap)/3600;}}
        prev=&s;}
    if(first&&now>first){w.days=std::max(1.,double(now-first)/86400);w.usedPerDay=drop/w.days;}return w;
}
}
