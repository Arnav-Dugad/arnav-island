#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace nexus {
// Colours taken from album art. accent: the most prominent vivid hue, lightened so it
// reads on the dark island; secondary: a second distinct hue (or a deeper shade of the
// accent) for gradients; ambient: the picture's overall tone, for the background glow;
// deep: the accent darkened for the light island.
struct ArtPalette {uint32_t accent=0xa5d8c5,secondary=0x7fbfaa,ambient=0x506a62,deep=0x3f6f62;bool colourful=false;};
inline void rgbToHsl(double r,double g,double b,double& h,double& s,double& l){
    const double mx=std::max({r,g,b}),mn=std::min({r,g,b});l=(mx+mn)/2;h=0;s=0;
    if(mx>mn){const double d=mx-mn;s=l>.5?d/(2-mx-mn):d/(mx+mn);h=mx==r?(g-b)/d+(g<b?6:0):mx==g?(b-r)/d+2:(r-g)/d+4;h/=6;}
}
inline uint32_t hslColor(double h,double s,double l){
    auto hue=[](double p,double q,double t){if(t<0)t+=1;if(t>1)t-=1;if(t<1./6)return p+(q-p)*6*t;if(t<.5)return q;if(t<2./3)return p+(q-p)*(2./3-t)*6;return p;};
    const double q=l<.5?l*(1+s):l+s-l*s,p=2*l-q;auto c=[&](double t){return uint32_t(std::clamp(hue(p,q,t),0.,1.)*255+.5);};
    return (c(h+1./3)<<16)|(c(h)<<8)|c(h-1./3);
}
// Pixels are BGRA, `count` of them; sampled sparsely (at most ~4096), transparent ones skipped.
inline ArtPalette artPalette(const uint8_t* bgra,size_t count){
    ArtPalette p;if(!bgra||!count)return p;
    constexpr int bins=24;std::array<double,bins> weight{},red{},green{},blue{};double ar=0,ag=0,ab=0,samples=0;
    const size_t step=std::max<size_t>(1,count/4096);
    for(size_t i=0;i<count;i+=step){const uint8_t* px=bgra+i*4;if(px[3]<200)continue;const double b=px[0]/255.,g=px[1]/255.,r=px[2]/255.;ar+=r;ag+=g;ab+=b;samples+=1;
        const double mx=std::max({r,g,b}),mn=std::min({r,g,b}),v=mx,s=mx>0?(mx-mn)/mx:0;if(v<.16||s<.2)continue;
        double h,sl,l;rgbToHsl(r,g,b,h,sl,l);const int bin=std::clamp(int(h*bins),0,bins-1);const double w=s*s*v;weight[bin]+=w;red[bin]+=r*w;green[bin]+=g*w;blue[bin]+=b*w;}
    if(samples<1)return p;ar/=samples;ag/=samples;ab/=samples;
    double ah,as,al;rgbToHsl(ar,ag,ab,ah,as,al);
    auto score=[&](int i){return weight[i]+.5*(weight[(i+bins-1)%bins]+weight[(i+1)%bins]);};
    int top=0;for(int i=1;i<bins;++i)if(score(i)>score(top))top=i;
    // Colourful only when the vivid pixels are a real part of the picture, not a stray speck.
    p.colourful=weight[top]/samples>.012;
    if(!p.colourful){
        // Grey, black-and-white or very dark art: neutral tones with a trace of its cast.
        const double s=std::min(as,.06);p.accent=hslColor(ah,s,.8);p.secondary=hslColor(ah,s,.64);p.ambient=hslColor(ah,std::min(as,.1),.42);p.deep=hslColor(ah,s,.3);return p;}
    auto binColor=[&](int i,double& h,double& s){const double w=weight[i];double l;rgbToHsl(red[i]/w,green[i]/w,blue[i]/w,h,s,l);};
    double h1,s1;binColor(top,h1,s1);
    p.accent=hslColor(h1,std::clamp(s1,.4,.78),.74);p.deep=hslColor(h1,std::clamp(s1,.38,.7),.34);
    // A second hue at least 45 degrees away that is a real presence; otherwise a deeper accent.
    int second=-1;for(int i=0;i<bins;++i){int d=std::abs(i-top);d=std::min(d,bins-d);if(d*360/bins<45||weight[i]<weight[top]*.22)continue;if(second<0||weight[i]>weight[second])second=i;}
    if(second>=0){double h2,s2;binColor(second,h2,s2);p.secondary=hslColor(h2,std::clamp(s2,.4,.78),.7);}else p.secondary=hslColor(h1,std::clamp(s1,.4,.8),.6);
    // The glow follows the picture's overall tone, steered toward its vivid hue when the average is muddy.
    const double gh=as<.15?h1:ah,gs=std::clamp(std::max(as,.3)*1.1,.25,.65);p.ambient=hslColor(gh,gs,.5);
    return p;
}
// Interpolates two 0xRRGGBB colours.
inline uint32_t mixColor(uint32_t a,uint32_t b,double t){t=std::clamp(t,0.,1.);auto ch=[&](int k){double x=double((a>>k)&255),y=double((b>>k)&255);return uint32_t(std::lround(x+(y-x)*t))<<k;};return ch(16)|ch(8)|ch(0);}
}
