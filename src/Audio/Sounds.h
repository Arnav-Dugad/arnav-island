#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace nexus {
// Phase 5G: the island's two sounds, made here rather than read from files: a faint two-note chime
// for alerts (a bell's partial on each note, the second a fifth above, just after the first) and a
// soft click as chips move into place. Both are quiet on purpose (peaks near -18 and -20 dBFS).
enum class Sound{Chime,Click};
// A 16-bit mono WAV file in memory.
inline std::vector<uint8_t> soundWave(Sound kind){
    constexpr int rate=44100;constexpr double pi=3.14159265358979;
    const double length=kind==Sound::Chime?1.2:.05;const size_t n=size_t(rate*length);std::vector<int16_t> pcm(n);
    auto bell=[&](double t,double f){if(t<0)return 0.;const double attack=1-std::exp(-t/.004),decay=std::exp(-t/.34);
        return attack*decay*(std::sin(2*pi*f*t)+.16*std::exp(-t/.07)*std::sin(2*pi*2.76*f*t));};
    for(size_t i=0;i<n;++i){const double t=double(i)/rate;double v=0;
        if(kind==Sound::Chime)v=.075*bell(t,1318.51)+.06*bell(t-.075,1975.53);
        else v=.075*std::sin(2*pi*2400*t)*std::exp(-t/.0055)+.04*std::sin(2*pi*880*t)*std::exp(-t/.011);
        // The last 5 ms fade to silence, so nothing clicks at the end.
        const double tail=std::min(1.,(length-t)/.005);pcm[i]=int16_t(std::lround(std::clamp(v*tail,-1.,1.)*32767));}
    std::vector<uint8_t> wav(44+n*2);auto put32=[&](size_t at,uint32_t v){std::memcpy(wav.data()+at,&v,4);};auto put16=[&](size_t at,uint16_t v){std::memcpy(wav.data()+at,&v,2);};
    std::memcpy(wav.data(),"RIFF",4);put32(4,uint32_t(36+n*2));std::memcpy(wav.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);
    std::memcpy(wav.data()+36,"data",4);put32(40,uint32_t(n*2));std::memcpy(wav.data()+44,pcm.data(),n*2);return wav;
}
// Test runs stay silent.
inline bool& soundsMuted(){static bool muted=false;return muted;}
// Plays without waiting; the waves live for the life of the process, as PlaySound needs.
inline void playSound(Sound kind){
    if(soundsMuted())return;
    static const auto chime=soundWave(Sound::Chime),click=soundWave(Sound::Click);const auto& w=kind==Sound::Chime?chime:click;
    PlaySoundW(reinterpret_cast<LPCWSTR>(w.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
}
}
