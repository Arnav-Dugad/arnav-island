#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <list>
#include <string>
namespace nexus {
// The loudness of one track across its timeline, learned from the audio actually
// heard (loopback levels). Nothing is invented: stretches not yet heard stay
// unknown and are drawn as quiet dots until they play.
struct TrackWaveform {
    static constexpr int bars=64;
    std::array<float,bars> sum{};std::array<uint32_t,bars> count{};
    static int bucket(double position,double duration){return std::clamp(int(position/duration*bars),0,bars-1);}
    // Returns the bucket that changed, or -1.
    int hear(double position,double duration,float level){
        if(!(duration>0)||!std::isfinite(position)||!std::isfinite(level)||position<0||position>duration)return -1;
        int i=bucket(position,duration);sum[size_t(i)]+=std::clamp(level,0.f,1.f);++count[size_t(i)];return i;
    }
    bool known(int i)const{return count[size_t(i)]>0;}
    float mean(int i)const{return known(i)?sum[size_t(i)]/float(count[size_t(i)]):0.f;}
    // Heights in 0..1, relative to the loudest stretch heard so far (so a quiet track
    // still fills the timeline); unknown stretches are -1.
    std::array<float,bars> heights()const{
        float loudest=0;for(int i=0;i<bars;++i)if(known(i))loudest=std::max(loudest,mean(i));const float scale=std::max(loudest,.2f);
        std::array<float,bars> h;for(int i=0;i<bars;++i)h[size_t(i)]=known(i)?std::clamp(mean(i)/scale,0.f,1.f):-1.f;return h;
    }
    int heard()const{int n=0;for(auto c:count)n+=c>0;return n;}
};
// Recently played tracks, in memory only; the oldest is forgotten first.
class WaveformLibrary {
    struct Entry {std::wstring key;TrackWaveform wave;};std::list<Entry> entries_;
public:
    static constexpr size_t limit=32;
    // A track is its title, artist and length; the same song replayed keeps its waveform.
    static std::wstring key(const std::wstring& title,const std::wstring& artist,double duration){return title+L"\n"+artist+L"\n"+std::to_wstring(int(std::lround(duration)));}
    TrackWaveform& track(const std::wstring& k){
        for(auto it=entries_.begin();it!=entries_.end();++it)if(it->key==k){entries_.splice(entries_.begin(),entries_,it);return entries_.front().wave;}
        entries_.push_front({k,{}});if(entries_.size()>limit)entries_.pop_back();return entries_.front().wave;
    }
    const TrackWaveform* find(const std::wstring& k)const{for(auto& e:entries_)if(e.key==k)return &e.wave;return nullptr;}
    size_t size()const{return entries_.size();}
};
}
