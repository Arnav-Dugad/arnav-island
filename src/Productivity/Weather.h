#pragma once
#include "Media/Lyrics.h"
#include <cmath>
#include <cstdio>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>
#include <algorithm>
namespace nexus {
// Phase 5F: weather from Open-Meteo (opt-in). WMO weather codes, as Open-Meteo reports them,
// fold into a few skies the island can draw and animate.
enum class Sky { Clear,PartlyCloudy,Cloudy,Fog,Drizzle,Rain,Snow,Storm };
inline Sky skyOf(int code){
    if(code<=0)return Sky::Clear;if(code<=2)return Sky::PartlyCloudy;if(code==3)return Sky::Cloudy;if(code==45||code==48)return Sky::Fog;
    if(code>=51&&code<=57)return Sky::Drizzle;if((code>=61&&code<=67)||(code>=80&&code<=82))return Sky::Rain;if((code>=71&&code<=77)||code==85||code==86)return Sky::Snow;if(code>=95)return Sky::Storm;return Sky::Cloudy;
}
inline const wchar_t* skyName(Sky s){switch(s){case Sky::Clear:return L"Clear";case Sky::PartlyCloudy:return L"Partly cloudy";case Sky::Cloudy:return L"Cloudy";case Sky::Fog:return L"Fog";case Sky::Drizzle:return L"Drizzle";case Sky::Rain:return L"Rain";case Sky::Snow:return L"Snow";default:return L"Thunderstorm";}}
// The same, short enough for the Home tile's corner (56 DIPs at 9.5 pt).
inline const wchar_t* skyLabel(Sky s){return s==Sky::Storm?L"Storm":skyName(s);}
// Whole degrees, in the chosen unit (0 Celsius, 1 Fahrenheit), with a degree sign.
inline std::wstring temperatureText(double celsius,int unit){const double v=unit?celsius*9/5+32:celsius;const long n=std::lround(v);return std::to_wstring(n==0?0:n)+L"°";}
// The chosen place: a name to show and coordinates rounded to two decimals (about a kilometre).
struct WeatherPlace {std::wstring name;double latitude=0,longitude=0;bool operator==(const WeatherPlace&)const=default;};
// 0.18: one hour of the forecast (time as Unix seconds; rain: the chance of rain in percent, -1 unknown).
struct WeatherHour {int64_t time=0;double temperature=0;int code=-1,rain=-1;bool day=true;bool operator==(const WeatherHour&)const=default;};
// sunrise, sunset: today's at the place, as Unix seconds (0: unknown).
// 0.18, everything else Open-Meteo reports (NaN when it didn't): feels (apparent temperature, C), humidity and cloud (%),
// dew point (C), wind and gusts (km/h), wind direction (degrees it comes from), pressure (hPa at sea level), visibility (m),
// precipitation now (mm), UV index; today's high and low (C), rain total (mm), chance of rain (%), highest UV, daylight (s);
// the air: US AQI and PM2.5 (µg/m³); and the next hours.
struct WeatherNow {double temperature=0;int code=-1;bool day=true;int64_t sunrise=0,sunset=0;
    double feels=NAN,humidity=NAN,dewPoint=NAN,wind=NAN,windFrom=NAN,gusts=NAN,pressure=NAN,visibility=NAN,cloud=NAN,precipitation=NAN,uv=NAN;
    double high=NAN,low=NAN,rainTotal=NAN,rainChance=NAN,uvMax=NAN,daylight=NAN,aqi=NAN,pm25=NAN;std::vector<WeatherHour> hours;};
// Where the wind comes from, as one of sixteen compass points.
inline const wchar_t* compassPoint(double degrees){static const wchar_t* points[]={L"N",L"NNE",L"NE",L"ENE",L"E",L"ESE",L"SE",L"SSE",L"S",L"SSW",L"SW",L"WSW",L"W",L"WNW",L"NW",L"NNW"};
    if(!std::isfinite(degrees))return L"";const double d=std::fmod(std::fmod(degrees,360)+360,360);return points[int(std::floor(d/22.5+.5))%16];}
// The UV index and the US air quality index in words.
inline const wchar_t* uvWord(double uv){return !std::isfinite(uv)?L"":uv<3?L"Low":uv<6?L"Moderate":uv<8?L"High":uv<11?L"Very high":L"Extreme";}
inline const wchar_t* aqiWord(double aqi){return !std::isfinite(aqi)?L"":aqi<=50?L"Good":aqi<=100?L"Moderate":aqi<=150?L"Sensitive":aqi<=200?L"Unhealthy":aqi<=300?L"Very unhealthy":L"Hazardous";}
// Wind in the chosen unit (0 km/h, 1 mph), a distance (m) as km or miles, pressure (hPa) as hPa or inHg, rain (mm) as mm or inches.
inline std::wstring windText(double kmh,int unit){if(!std::isfinite(kmh))return {};return std::to_wstring(long(std::lround(unit?kmh/1.609344:kmh)))+(unit?L" mph":L" km/h");}
inline std::wstring distanceText(double metres,int unit){if(!std::isfinite(metres)||metres<0)return {};const double v=unit?metres/1609.344:metres/1000;wchar_t b[32];swprintf(b,32,v<10?L"%.1f %ls":L"%.0f %ls",v,unit?L"mi":L"km");return b;}
inline std::wstring pressureText(double hpa,int unit){if(!std::isfinite(hpa)||hpa<=0)return {};wchar_t b[32];if(unit)swprintf(b,32,L"%.2f inHg",hpa*.02953);else swprintf(b,32,L"%.0f hPa",hpa);return b;}
inline std::wstring rainText(double mm,int unit){if(!std::isfinite(mm)||mm<0)return {};wchar_t b[32];if(unit)swprintf(b,32,L"%.2f in",mm/25.4);else swprintf(b,32,mm<10?L"%.1f mm":L"%.0f mm",mm);return b;}
// 0.17.0-preview.3: "2026-09-26T06:12" at a place utcOffset seconds ahead of UTC, as Unix seconds (0 when unreadable).
inline int64_t localToUnix(std::string_view text,int64_t utcOffset){
    int y=0,mo=0,d=0,h=0,mi=0;if(text.size()<16||std::sscanf(std::string(text.substr(0,16)).c_str(),"%d-%d-%dT%d:%d",&y,&mo,&d,&h,&mi)!=5||mo<1||mo>12||d<1||d>31||h<0||h>23||mi<0||mi>59)return 0;
    // Days from the civil date (Howard Hinnant's algorithm).
    y-=mo<=2;const int era=(y>=0?y:y-399)/400;const unsigned yoe=unsigned(y-era*400),doy=unsigned((153*(mo+(mo>2?-3:9))+2)/5+d-1),doe=yoe*365+yoe/4-yoe/100+doy;const int64_t days=int64_t(era)*146097+int64_t(doe)-719468;
    return days*86400+h*3600+mi*60-utcOffset;
}
// Where the sun is for the sky: dawn from 45 minutes before sunrise to 35 after, dusk from 40 before sunset to 35 after.
// Times from another day are moved by whole days to the ones nearest now. Unknown times: day or night as reported.
enum class SunPhase{Night,Dawn,Day,Dusk};
inline SunPhase sunPhase(int64_t now,int64_t sunrise,int64_t sunset,bool day){
    if(sunrise<=0||sunset<=0||sunset<=sunrise||sunset-sunrise>86400)return day?SunPhase::Day:SunPhase::Night;
    auto nearest=[&](int64_t t){while(t-now>43200)t-=86400;while(now-t>43200)t+=86400;return t;};const int64_t rise=nearest(sunrise),set=nearest(sunset);
    if(now>=rise-45*60&&now<rise+35*60)return SunPhase::Dawn;if(now>=set-40*60&&now<set+35*60)return SunPhase::Dusk;
    // Between the two: day when sunrise came first (today's), night when sunset did.
    return (rise<=now&&now<set)||(set<rise&&(now<set||now>=rise))?SunPhase::Day:SunPhase::Night;
}
// Open-Meteo's geocoding answer: every match, named "Town, Region, Country" (Phase 5G: the region
// tells towns of one name apart, "Manipal, Karnataka, India"), coordinates rounded to two decimals.
inline std::vector<WeatherPlace> parseGeocodeAll(std::string_view json,size_t limit=8){
    std::vector<WeatherPlace> out;auto doc=Json::parse(json);if(!doc)return out;auto* results=doc->find("results");if(!results||results->type!=Json::Type::Array)return out;
    for(const auto& r:results->items){if(out.size()>=limit)break;if(r.type!=Json::Type::Object)continue;auto* lat=r.find("latitude");auto* lon=r.find("longitude");
        if(!lat||!lon||lat->type!=Json::Type::Number||lon->type!=Json::Type::Number||std::abs(lat->number)>90||std::abs(lon->number)>180)continue;
        std::wstring name=fromUtf8(r.string("name"));if(name.empty())continue;const auto region=fromUtf8(r.string("admin1")),country=fromUtf8(r.string("country"));
        if(!region.empty()&&region!=name)name+=L", "+region;if(!country.empty())name+=L", "+country;if(name.size()>120)name.resize(120);
        const WeatherPlace p{name,std::round(lat->number*100)/100,std::round(lon->number*100)/100};
        // A name listed twice (a town and its district, a few kilometres apart) shows once, as Open-Meteo's first
        // (most likely) match: two rows that read the same couldn't be told apart.
        if(std::none_of(out.begin(),out.end(),[&](auto& q){return q.name==p.name;}))out.push_back(p);}
    return out;
}
// The first match (the weather command's choice).
inline std::optional<WeatherPlace> parseGeocode(std::string_view json){auto all=parseGeocodeAll(json,1);if(all.empty())return std::nullopt;return all.front();}
// The "current" block of Open-Meteo's forecast answer.
inline std::optional<WeatherNow> parseForecast(std::string_view json){
    auto doc=Json::parse(json);if(!doc)return std::nullopt;auto* current=doc->find("current");if(!current||current->type!=Json::Type::Object)return std::nullopt;
    auto* t=current->find("temperature_2m");auto* code=current->find("weather_code");if(!t||!code||t->type!=Json::Type::Number||code->type!=Json::Type::Number||!std::isfinite(t->number)||t->number<-100||t->number>70)return std::nullopt;
    WeatherNow w{t->number,int(code->number),current->num("is_day",1)!=0};
    // Anything missing, not a number or impossible stays NaN.
    auto number=[](const Json* o,const char* key,double lo,double hi){auto* v=o?o->find(key):nullptr;return v&&v->type==Json::Type::Number&&std::isfinite(v->number)&&v->number>=lo&&v->number<=hi?v->number:NAN;};
    w.feels=number(current,"apparent_temperature",-100,80);w.humidity=number(current,"relative_humidity_2m",0,100);w.dewPoint=number(current,"dew_point_2m",-100,60);
    w.wind=number(current,"wind_speed_10m",0,500);w.windFrom=number(current,"wind_direction_10m",0,360);w.gusts=number(current,"wind_gusts_10m",0,600);
    w.pressure=number(current,"pressure_msl",800,1100);w.visibility=number(current,"visibility",0,1e6);w.cloud=number(current,"cloud_cover",0,100);
    w.precipitation=number(current,"precipitation",0,1000);w.uv=number(current,"uv_index",0,30);
    // Today's sunrise and sunset (local times, with the place's offset from UTC).
    const int64_t offset=int64_t(doc->num("utc_offset_seconds",0));
    if(auto* daily=doc->find("daily");daily&&daily->type==Json::Type::Object){auto first=[&](const char* key)->int64_t{auto* list=daily->find(key);if(!list||list->type!=Json::Type::Array||list->items.empty()||list->items[0].type!=Json::Type::String)return 0;return localToUnix(list->items[0].text,offset);};
        w.sunrise=first("sunrise");w.sunset=first("sunset");if(w.sunrise<=0||w.sunset<=w.sunrise)w.sunrise=w.sunset=0;
        auto today=[&](const char* key,double lo,double hi){auto* list=daily->find(key);if(!list||list->type!=Json::Type::Array||list->items.empty()||list->items[0].type!=Json::Type::Number)return double(NAN);const double v=list->items[0].number;return std::isfinite(v)&&v>=lo&&v<=hi?v:double(NAN);};
        w.high=today("temperature_2m_max",-100,70);w.low=today("temperature_2m_min",-100,70);w.rainTotal=today("precipitation_sum",0,2000);w.rainChance=today("precipitation_probability_max",0,100);
        w.uvMax=today("uv_index_max",0,30);w.daylight=today("daylight_duration",0,86400);}
    // The next hours (the first is the hour now running), at most thirteen.
    if(auto* hourly=doc->find("hourly");hourly&&hourly->type==Json::Type::Object){auto list=[&](const char* key)->const Json*{auto* l=hourly->find(key);return l&&l->type==Json::Type::Array?l:nullptr;};
        auto* times=list("time");auto* temps=list("temperature_2m");auto* codes=list("weather_code");auto* rain=list("precipitation_probability");auto* day=list("is_day");
        if(times&&temps)for(size_t k=0;k<times->items.size()&&k<temps->items.size()&&w.hours.size()<13;++k){if(times->items[k].type!=Json::Type::String||temps->items[k].type!=Json::Type::Number)continue;
            WeatherHour h;h.time=localToUnix(times->items[k].text,offset);h.temperature=temps->items[k].number;if(h.time<=0||!std::isfinite(h.temperature)||h.temperature<-100||h.temperature>70)continue;
            if(codes&&k<codes->items.size()&&codes->items[k].type==Json::Type::Number)h.code=int(codes->items[k].number);
            if(rain&&k<rain->items.size()&&rain->items[k].type==Json::Type::Number)h.rain=std::clamp(int(rain->items[k].number),0,100);
            if(day&&k<day->items.size()&&day->items[k].type==Json::Type::Number)h.day=day->items[k].number!=0;w.hours.push_back(h);}}
    return w;
}
// 0.18: Open-Meteo's air quality answer (the US AQI and PM2.5 now) into w; false when it has neither.
inline bool parseAirQuality(std::string_view json,WeatherNow& w){
    auto doc=Json::parse(json);if(!doc)return false;auto* current=doc->find("current");if(!current||current->type!=Json::Type::Object)return false;
    auto* aqi=current->find("us_aqi");auto* pm=current->find("pm2_5");bool any=false;
    if(aqi&&aqi->type==Json::Type::Number&&std::isfinite(aqi->number)&&aqi->number>=0&&aqi->number<=1000){w.aqi=aqi->number;any=true;}
    if(pm&&pm->type==Json::Type::Number&&std::isfinite(pm->number)&&pm->number>=0&&pm->number<=2000){w.pm25=pm->number;any=true;}
    return any;
}
inline std::wstring geocodePath(const std::wstring& town,int count=1){return L"/v1/search?count="+std::to_wstring(std::clamp(count,1,10))+L"&language=en&format=json&name="+fromUtf8(urlEncode(toUtf8(town)));}
inline std::wstring forecastPath(const WeatherPlace& p){wchar_t b[64];swprintf(b,64,L"latitude=%.2f&longitude=%.2f",p.latitude,p.longitude);
    return std::wstring(L"/v1/forecast?")+b+L"&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,cloud_cover,pressure_msl,wind_speed_10m,wind_direction_10m,wind_gusts_10m,visibility,uv_index,dew_point_2m"
        L"&hourly=temperature_2m,weather_code,precipitation_probability,is_day&forecast_hours=13"
        L"&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max,uv_index_max,daylight_duration&forecast_days=1&timezone=auto";}
// 0.18: the air quality at the same rounded coordinates (Open-Meteo's air quality service).
inline std::wstring airQualityPath(const WeatherPlace& p){wchar_t b[128];swprintf(b,128,L"/v1/air-quality?latitude=%.2f&longitude=%.2f&current=us_aqi,pm2_5",p.latitude,p.longitude);return b;}
// weather.nexus: the chosen place only.
inline void writePlace(std::ostream& out,const WeatherPlace& p){char b[64];snprintf(b,sizeof b,"%.2f %.2f",p.latitude,p.longitude);out<<"weather 1\n"<<b<<'\n'<<toUtf8(p.name)<<'\n';}
inline std::optional<WeatherPlace> readPlace(std::istream& in){std::string magic,name;int version=0;WeatherPlace p;if(!(in>>magic>>version>>p.latitude>>p.longitude)||magic!="weather"||version!=1||std::abs(p.latitude)>90||std::abs(p.longitude)>180)return std::nullopt;
    std::getline(in,name);std::getline(in,name);if(name.empty()||name.size()>200)return std::nullopt;p.name=fromUtf8(name);return p;}
}
