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
struct WeatherNow {double temperature=0;int code=-1;bool day=true;};
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
    return WeatherNow{t->number,int(code->number),current->num("is_day",1)!=0};
}
inline std::wstring geocodePath(const std::wstring& town,int count=1){return L"/v1/search?count="+std::to_wstring(std::clamp(count,1,10))+L"&language=en&format=json&name="+fromUtf8(urlEncode(toUtf8(town)));}
inline std::wstring forecastPath(const WeatherPlace& p){wchar_t b[160];swprintf(b,160,L"/v1/forecast?latitude=%.2f&longitude=%.2f&current=temperature_2m,weather_code,is_day&timezone=auto",p.latitude,p.longitude);return b;}
// weather.nexus: the chosen place only.
inline void writePlace(std::ostream& out,const WeatherPlace& p){char b[64];snprintf(b,sizeof b,"%.2f %.2f",p.latitude,p.longitude);out<<"weather 1\n"<<b<<'\n'<<toUtf8(p.name)<<'\n';}
inline std::optional<WeatherPlace> readPlace(std::istream& in){std::string magic,name;int version=0;WeatherPlace p;if(!(in>>magic>>version>>p.latitude>>p.longitude)||magic!="weather"||version!=1||std::abs(p.latitude)>90||std::abs(p.longitude)>180)return std::nullopt;
    std::getline(in,name);std::getline(in,name);if(name.empty()||name.size()>200)return std::nullopt;p.name=fromUtf8(name);return p;}
}
