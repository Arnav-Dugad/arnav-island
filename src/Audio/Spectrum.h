#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
namespace nexus {
// Real audio spectrum from loopback samples: Hann-windowed 1024-point FFT,
// log-spaced bands between 50 Hz and 14 kHz, decibel scaling and asymmetric
// attack/release smoothing. No synthetic motion is ever generated; silence
// decays to rest.
struct Spectrum {
    static constexpr int size=1024,bandCount=24;
    std::array<float,size> ring{};int write=0;
    std::array<float,bandCount> bands{};float level=0;
    void push(const float* mono,int count){for(int i=0;i<count;++i){ring[write]=mono[i];write=(write+1)%size;}}
    void pushSilence(int count){for(int i=0;i<std::min(count,size);++i){ring[write]=0;write=(write+1)%size;}}
    static void fft(std::array<std::complex<float>,size>& a){
        for(int i=1,j=0;i<size;++i){int bit=size>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
        for(int length=2;length<=size;length<<=1){float angle=-6.28318530718f/length;std::complex<float> w(std::cos(angle),std::sin(angle));
            for(int i=0;i<size;i+=length){std::complex<float> v(1);for(int j=0;j<length/2;++j){auto u=a[i+j],t=a[i+j+length/2]*v;a[i+j]=u+t;a[i+j+length/2]=u-t;v*=w;}}}
    }
    // One analysis step; `dt` is the time since the previous step in seconds.
    void analyze(float sampleRate,float dt){
        std::array<std::complex<float>,size> x;float energy=0;
        for(int i=0;i<size;++i){float s=ring[(write+i)%size];float hann=.5f-.5f*std::cos(6.28318530718f*i/(size-1));x[i]={s*hann,0};energy+=s*s;}
        fft(x);const float fullScale=(size/4.f)*(size/4.f);const float nyquist=sampleRate/2;
        for(int b=0;b<bandCount;++b){
            float lo=50*std::pow(280.f,float(b)/bandCount),hi=50*std::pow(280.f,float(b+1)/bandCount);hi=std::min(hi,nyquist);
            int k0=std::max(1,int(lo/sampleRate*size)),k1=std::max(k0+1,int(std::ceil(hi/sampleRate*size)));k1=std::min(k1,size/2);float peak=0;
            for(int k=k0;k<k1;++k)peak=std::max(peak,std::norm(x[k]));
            float db=peak>0?10*std::log10(peak/fullScale):-120.f;
            // Tilt compensates for the natural high-frequency roll-off of music.
            db+=float(b)/bandCount*9;
            float target=std::clamp((db+62)/50,0.f,1.f);target=target*target*(3-2*target);
            float rate=target>bands[b]?1-std::exp(-dt/.025f):1-std::exp(-dt/.16f);bands[b]+=(target-bands[b])*rate;
            if(!std::isfinite(bands[b]))bands[b]=0;
        }
        float rms=std::sqrt(energy/size);float target=std::clamp((20*std::log10(std::max(rms,1e-6f))+55)/50,0.f,1.f);level+=(target-level)*(target>level?1-std::exp(-dt/.03f):1-std::exp(-dt/.2f));
    }
    bool resting()const{return level<.01f&&std::all_of(bands.begin(),bands.end(),[](float b){return b<.01f;});}
};
}
