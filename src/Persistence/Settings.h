#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>
#include <stdexcept>
#include "Design/Layout.h"
namespace nexus {
struct Settings {
    static constexpr int currentVersion=6;
    int version=currentVersion,uiMode=1,preset=1,monitor=0,verticalOffset=0,horizontalOffset=0,hoverDelay=180,mediaLayout=0;
    int scale=100,corner=22,edge=0,theme=0,compactWidth=196,collapseDelay=650,accent=0;
    int material=0,glassTint=50;
    std::array<int,7> navigation=defaultNavigation;std::array<int,3> homeMetrics=defaultMetrics;
    int glanceRings=3;bool animatedIcons=true,trackHandoff=true,collapseOnAppSwitch=true,wheelVolume=false;
    bool compactVolume=true,compactTimer=true,compactClock=false,shelfPeek=true;
    bool reduceMotion=false,hideFullscreen=true,hoverOpen=true,startAtLogin=true;
    bool albumAccents=true,magnetic=true,compactMedia=true,compactBattery=true,directAudio=true;
    bool waveform=true,appIcons=true,hud=true,followSession=true;
    // Glass floats a few DIPs from the screen edge, like a physical island.
    bool glassy()const{return material!=0;}
    bool floating()const{return verticalOffset>0||glassy();}
    int gap()const{return glassy()&&verticalOffset==0?8:0;}
    static Settings parse(std::istream& in){Settings s;std::string key;double value;while(in>>key){if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Invalid setting");
        if(key=="version"){if(value<1||value>currentVersion||std::floor(value)!=value)throw std::runtime_error("Unsupported settings version");}
        else if(key.size()==4&&key.starts_with("nav")&&key[3]>='0'&&key[3]<='6')s.navigation[key[3]-'0']=int(std::clamp(value,0.,6.));
        else if(key.size()==5&&key.starts_with("home")&&key[4]>='0'&&key[4]<='2')s.homeMetrics[key[4]-'0']=int(std::clamp(value,0.,6.));
        else if(key=="glass"){/* v5 interior acrylic; superseded by material. */}
#define NUM(name,lo,hi) else if(key==#name)s.name=int(std::clamp(value,double(lo),double(hi)));
        NUM(glanceRings,0,3) NUM(preset,0,4) NUM(monitor,0,16) NUM(verticalOffset,0,200) NUM(horizontalOffset,-2000,2000) NUM(hoverDelay,100,700) NUM(mediaLayout,0,2)
        NUM(scale,80,120) NUM(corner,14,28) NUM(edge,0,1) NUM(theme,0,2) NUM(compactWidth,160,560) NUM(uiMode,0,2) NUM(collapseDelay,300,1600) NUM(accent,0,3)
        NUM(material,0,2) NUM(glassTint,0,100)
#undef NUM
#define FLAG(name) else if(key==#name)s.name=value!=0;
        FLAG(compactVolume) FLAG(compactTimer) FLAG(compactClock) FLAG(shelfPeek) FLAG(collapseOnAppSwitch) FLAG(wheelVolume) FLAG(animatedIcons) FLAG(trackHandoff) FLAG(reduceMotion) FLAG(hideFullscreen) FLAG(hoverOpen) FLAG(startAtLogin) FLAG(albumAccents) FLAG(magnetic) FLAG(compactMedia) FLAG(compactBattery) FLAG(directAudio)
        FLAG(waveform) FLAG(appIcons) FLAG(hud) FLAG(followSession)
#undef FLAG
        }if(!in.eof())throw std::runtime_error("Malformed settings");if(!validNavigation(s.navigation))s.navigation=defaultNavigation;auto metrics=s.homeMetrics;std::sort(metrics.begin(),metrics.end());if(std::adjacent_find(metrics.begin(),metrics.end())!=metrics.end())s.homeMetrics=defaultMetrics;return s;}
    void write(std::ostream& out)const{out<<"version "<<currentVersion<<'\n';for(int i=0;i<7;++i)out<<"nav"<<i<<' '<<navigation[i]<<'\n';for(int i=0;i<3;++i)out<<"home"<<i<<' '<<homeMetrics[i]<<'\n';
#define WRITE(name) out<<#name<<' '<<name<<'\n';
        WRITE(uiMode) WRITE(compactVolume) WRITE(compactTimer) WRITE(compactClock) WRITE(shelfPeek) WRITE(collapseOnAppSwitch) WRITE(wheelVolume) WRITE(glanceRings) WRITE(animatedIcons) WRITE(trackHandoff) WRITE(preset) WRITE(monitor) WRITE(verticalOffset) WRITE(horizontalOffset) WRITE(hoverDelay) WRITE(mediaLayout) WRITE(scale) WRITE(corner) WRITE(edge) WRITE(theme) WRITE(compactWidth) WRITE(collapseDelay) WRITE(accent)
        WRITE(material) WRITE(glassTint) WRITE(reduceMotion) WRITE(hideFullscreen) WRITE(hoverOpen) WRITE(startAtLogin) WRITE(albumAccents) WRITE(magnetic) WRITE(compactMedia) WRITE(compactBattery) WRITE(directAudio)
        WRITE(waveform) WRITE(appIcons) WRITE(hud) WRITE(followSession)
#undef WRITE
    }
    bool operator==(const Settings&)const=default;
};
}
