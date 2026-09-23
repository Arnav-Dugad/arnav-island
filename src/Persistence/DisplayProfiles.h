#pragma once
#include <algorithm>
#include <iomanip>
#include <map>
#include <string>
#include "Persistence/Settings.h"
namespace nexus {
struct DisplayPlacement {
    int x=0,y=0,width=196,scale=100,edge=0;
    static DisplayPlacement from(const Settings& s){return {s.horizontalOffset,s.verticalOffset,s.compactWidth,s.scale,s.edge};}
    void apply(Settings& s)const{s.horizontalOffset=x;s.verticalOffset=y;s.compactWidth=width;s.scale=scale;s.edge=edge;}
};
struct DisplayProfiles {
    std::string preferred;
    std::map<std::string,DisplayPlacement> placements;
    void remember(const std::string& id,const Settings& s){if(!id.empty()&&(placements.contains(id)||placements.size()<32))placements[id]=DisplayPlacement::from(s);}
    bool restore(const std::string& id,Settings& s)const{auto it=placements.find(id);if(it==placements.end())return false;it->second.apply(s);return true;}
    void write(std::ostream& out)const{out<<"display_profiles 1\n"<<std::quoted(preferred)<<'\n';for(const auto& [id,p]:placements)out<<std::quoted(id)<<' '<<p.x<<' '<<p.y<<' '<<p.width<<' '<<p.scale<<' '<<p.edge<<'\n';}
    static DisplayProfiles read(std::istream& in){DisplayProfiles result;std::string magic,id;int version; if(!(in>>magic>>version)||magic!="display_profiles"||version!=1||!(in>>std::quoted(result.preferred))||result.preferred.size()>512)return {};DisplayPlacement p;while(in>>std::quoted(id)){if(!(in>>p.x>>p.y>>p.width>>p.scale>>p.edge))return {};if(id.empty()||id.size()>512||result.placements.size()>=32)return {};p.x=std::clamp(p.x,-2000,2000);p.y=std::clamp(p.y,0,200);p.width=std::clamp(p.width,160,560);p.scale=std::clamp(p.scale,80,120);p.edge=std::clamp(p.edge,0,1);result.placements[id]=p;}return in.eof()?result:DisplayProfiles{};}
};
}
