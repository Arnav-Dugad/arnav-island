#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>
#include <stdexcept>
namespace nexus {
struct Settings {
    int version=3,preset=1,monitor=0,verticalOffset=0,horizontalOffset=0,hoverDelay=180,mediaLayout=0;
    int scale=100,corner=22,edge=0,theme=0,compactWidth=196,collapseDelay=650,accent=0;
    bool reduceMotion=false,hideFullscreen=true,hoverOpen=true,startAtLogin=true;
    bool glass=true,albumAccents=true,magnetic=true,compactMedia=true,compactBattery=true,directAudio=true;
    static Settings parse(std::istream& in){Settings s;std::string key;double value;while(in>>key){if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Invalid setting");
        if(key=="version"){if(value<1||value>3||std::floor(value)!=value)throw std::runtime_error("Unsupported settings version");}
#define NUM(name,lo,hi) else if(key==#name)s.name=int(std::clamp(value,double(lo),double(hi)));
        NUM(preset,0,4) NUM(monitor,0,16) NUM(verticalOffset,0,200) NUM(horizontalOffset,-2000,2000) NUM(hoverDelay,100,700) NUM(mediaLayout,0,2)
        NUM(scale,80,120) NUM(corner,14,28) NUM(edge,0,1) NUM(theme,0,2) NUM(compactWidth,160,260) NUM(collapseDelay,300,1600) NUM(accent,0,3)
#undef NUM
#define FLAG(name) else if(key==#name)s.name=value!=0;
        FLAG(reduceMotion) FLAG(hideFullscreen) FLAG(hoverOpen) FLAG(startAtLogin) FLAG(glass) FLAG(albumAccents) FLAG(magnetic) FLAG(compactMedia) FLAG(compactBattery) FLAG(directAudio)
#undef FLAG
        }if(!in.eof())throw std::runtime_error("Malformed settings");return s;}
    void write(std::ostream& out)const{out<<"version 3\n";
#define WRITE(name) out<<#name<<' '<<name<<'\n';
        WRITE(preset) WRITE(monitor) WRITE(verticalOffset) WRITE(horizontalOffset) WRITE(hoverDelay) WRITE(mediaLayout) WRITE(scale) WRITE(corner) WRITE(edge) WRITE(theme) WRITE(compactWidth) WRITE(collapseDelay) WRITE(accent)
        WRITE(reduceMotion) WRITE(hideFullscreen) WRITE(hoverOpen) WRITE(startAtLogin) WRITE(glass) WRITE(albumAccents) WRITE(magnetic) WRITE(compactMedia) WRITE(compactBattery) WRITE(directAudio)
#undef WRITE
    }
};
}
