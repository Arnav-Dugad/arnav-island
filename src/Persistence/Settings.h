#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <cmath>

namespace nexus {
struct Settings {
    int version=1,preset=0,monitor=0,verticalOffset=0,horizontalOffset=0;
    bool reduceMotion=false,hideFullscreen=true;
    static Settings parse(std::istream& in) {
        Settings s;std::string key;double value;
        while(in>>key>>value) {
            if(!std::isfinite(value))throw std::runtime_error("Nonfinite setting");
            if(key=="version") {if(value!=1)throw std::runtime_error("Unsupported settings version");}
            else if(key=="preset")s.preset=int(std::clamp(value,0.,4.));
            else if(key=="monitor")s.monitor=int(std::clamp(value,0.,16.));
            else if(key=="verticalOffset")s.verticalOffset=int(std::clamp(value,0.,200.));
            else if(key=="horizontalOffset")s.horizontalOffset=int(std::clamp(value,-2000.,2000.));
            else if(key=="reduceMotion")s.reduceMotion=value!=0;
            else if(key=="hideFullscreen")s.hideFullscreen=value!=0;
        }
        if(!in.eof())throw std::runtime_error("Malformed settings");
        return s;
    }
    void write(std::ostream& out) const {
        out<<"version 1\npreset "<<preset<<"\nmonitor "<<monitor<<"\nverticalOffset "<<verticalOffset
           <<"\nhorizontalOffset "<<horizontalOffset<<"\nreduceMotion "<<reduceMotion<<"\nhideFullscreen "<<hideFullscreen<<'\n';
    }
};
}
