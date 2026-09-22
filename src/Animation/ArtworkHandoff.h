#pragma once
#include "Interaction/DashboardModel.h"
#include "Animation/MotionEngine.h"
namespace nexus {
// Flatten the current blend only on a new track, never on frame callbacks.
class ArtworkHandoff {
public:
    static constexpr unsigned side=256;
    std::vector<uint8_t> from,to;
    Spring mix{1};
    static std::vector<uint8_t> frame(const Artwork* art,uint32_t background){
        std::vector<uint8_t> pixels(side*side*4);
        for(size_t i=0;i<pixels.size();i+=4){pixels[i]=background&255;pixels[i+1]=(background>>8)&255;pixels[i+2]=(background>>16)&255;pixels[i+3]=255;}
        if(!art||!art->width||!art->height||art->pixels.size()<size_t(art->width)*art->height*4)return pixels;
        double scale=std::min(double(side)/art->width,double(side)/art->height);int w=std::max(1,int(art->width*scale)),h=std::max(1,int(art->height*scale)),ox=(side-w)/2,oy=(side-h)/2;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){double fx=std::clamp((x+.5)/scale-.5,0.,double(art->width-1)),fy=std::clamp((y+.5)/scale-.5,0.,double(art->height-1));unsigned x0=unsigned(fx),y0=unsigned(fy),x1=std::min(x0+1,art->width-1),y1=std::min(y0+1,art->height-1);double u=fx-x0,v=fy-y0;size_t dst=((y+oy)*side+x+ox)*4;for(unsigned c=0;c<3;++c){auto sample=[&](unsigned px,unsigned py){size_t i=(py*art->width+px)*4;return art->pixels[i+c]+pixels[dst+c]*(1-art->pixels[i+3]/255.);};pixels[dst+c]=uint8_t(std::clamp(std::lround((1-v)*((1-u)*sample(x0,y0)+u*sample(x1,y0))+v*((1-u)*sample(x0,y1)+u*sample(x1,y1))),0l,255l));}}
        return pixels;
    }
    std::vector<uint8_t> visible(double now)const{if(to.empty())return {};double alpha=std::clamp(mix.sample(now).position,0.,1.);if(from.empty())return to;auto result=to;for(size_t i=0;i<result.size();++i)result[i]=uint8_t(std::lround(from[i]*(1-alpha)+to[i]*alpha));return result;}
    void change(const Artwork* artwork,uint32_t background,double now,bool animate){auto current=visible(now);to=frame(artwork,background);from=current.empty()?to:std::move(current);mix.reset(animate?0:1,now);if(animate)mix.retarget(1,now,MotionTokens::handoff);}
};
}
