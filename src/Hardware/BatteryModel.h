#pragma once
#include <algorithm>
#include <cmath>
#include <cwctype>
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
    // 0.18: everything else the battery and Windows report. warning and low: the levels (mWh) at which Windows warns;
    // criticalBias: capacity the firmware holds back; temperature in tenths of a kelvin (0 unknown); the driver's own
    // estimate and Windows' (seconds, -1 unknown); saver: Windows' battery saver; count: batteries in the PC.
    long long warningMwh=0,lowMwh=0,criticalBiasMwh=0;int temperatureDeciK=0,estimateSeconds=-1,windowsSeconds=-1,count=0,madeYear=0,madeMonth=0,madeDay=0;
    bool saver=false,critical=false,rechargeable=true;std::wstring serial;
    // Full-charge capacity as a share of design capacity; -1 when unknown.
    double health()const{return !relative&&designMwh>0&&fullMwh>0?std::min(1.,double(fullMwh)/double(designMwh)):-1;}
    // Current in milliamps (positive while charging), from the rate and the voltage; 0 when unknown.
    long long currentMa()const{return !relative&&voltageMv>0&&rateMw!=0?rateMw*1000/voltageMv:0;}
    // Degrees Celsius, or NaN when the battery doesn't say (or says something impossible).
    double celsius()const{const double c=temperatureDeciK/10.-273.15;return temperatureDeciK>0&&c>-40&&c<100?c:std::nan("");}
    // A level (mWh) as a share of the full charge, in percent; -1 when unknown.
    int percentOf(long long mwh)const{return !relative&&fullMwh>0&&mwh>0?int(std::lround(100.*double(mwh)/double(fullMwh))):-1;}
};
// The battery's chemistry code (four letters, as the firmware reports it) in words; empty when it isn't a standard code.
inline std::wstring chemistryName(const std::wstring& code){
    std::wstring c;for(wchar_t ch:code)if(ch!=L' ')c+=wchar_t(std::towupper(ch));
    if(c==L"LION"||c==L"LI-I"||c==L"LIION"||c==L"LI"||c==L"LI-ION")return L"Lithium-ion";if(c==L"LIP"||c==L"LIPO"||c==L"LI-P")return L"Lithium polymer";
    if(c==L"PBAC")return L"Lead acid";if(c==L"NICD")return L"Nickel-cadmium";if(c==L"NIMH")return L"Nickel-metal hydride";if(c==L"NIZN")return L"Nickel-zinc";if(c==L"RAM")return L"Rechargeable alkaline";
    // Anything else (some firmware reports a vendor code such as "OOI0") is left unsaid rather than shown as it is.
    return {};
}
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
// 0.18.1: the first day (index into days) the health fell below `level` after being at or above it; -1 when it hasn't.
inline int healthCrossing(const BatteryHealthLog& log,double level){
    bool above=false;for(size_t k=0;k<log.days.size();++k){const double h=BatteryHealthLog::health(log.days[k]);if(h<0)continue;if(h>=level)above=true;else if(above)return int(k);}return -1;
}
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
