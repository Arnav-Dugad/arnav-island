#pragma once
#include "Interaction/DashboardModel.h"
#include "Design/Palette.h"
#include <algorithm>
#include <cstdint>
namespace nexus {
// Pastel accent from average RGB, weighted toward colourful pixels: hue kept,
// saturation and lightness eased so it reads well on the dark island.
inline uint32_t pastelAccent(double r,double g,double b){
    const double mx=std::max({r,g,b}),mn=std::min({r,g,b}),l=(mx+mn)/2;double h=0,s=0;
    if(mx>mn){const double d=mx-mn;s=l>.5?d/(2-mx-mn):d/(mx+mn);h=mx==r?(g-b)/d+(g<b?6:0):mx==g?(b-r)/d+2:(r-g)/d+4;h/=6;}
    // Near-grey pictures stay neutral; colourful ones get a gentle, legible saturation.
    s=s<.08?s:std::clamp(s,.28,.62);const double L=.78;auto hue=[](double p,double q,double t){if(t<0)t+=1;if(t>1)t-=1;if(t<1./6)return p+(q-p)*6*t;if(t<.5)return q;if(t<2./3)return p+(q-p)*(2./3-t)*6;return p;};
    const double q=L<.5?L*(1+s):L+s-L*s,p=2*L-q;auto c=[&](double t){return uint32_t(std::clamp(hue(p,q,t),0.,1.)*255+.5);};
    return (c(h+1./3)<<16)|(c(h)<<8)|c(h-1./3);
}
// Colours an artwork's palette into it.
inline void paintArtwork(Artwork& art){auto p=artPalette(art.pixels.data(),art.pixels.size()/4);art.accent=p.accent;art.secondary=p.secondary;art.ambient=p.ambient;art.deep=p.deep;}
// Swatch 4 follows the wallpaper. Light islands use one deep accent, or the artwork's deep shade.
inline uint32_t islandAccent(int swatch,bool light,uint32_t wallpaper,const Artwork* artwork,bool albumAccents){
    const uint32_t accents[]={0xa4deca,0xa6cafa,0xccb8f1,0xefc7a6};if(albumAccents&&artwork)return light?(artwork->deep?artwork->deep:0x487467):artwork->accent;if(light)return 0x487467;
    return swatch==4?(wallpaper?wallpaper:accents[0]):accents[std::clamp(swatch,0,3)];
}
}
