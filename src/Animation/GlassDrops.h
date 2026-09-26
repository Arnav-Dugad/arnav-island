#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace nexus {
// 0.18.1: raindrops on the glass that react. Each drop beads where it lands; small ones cling, bigger ones creep down,
// and when the island moves (shake 0-1) they let go and run faster. A running drop that meets another takes it in
// (their areas add up, so it grows and runs faster still). A drop that runs off the bottom, or was taken in, comes
// back as a new small drop near the top once it has faded. Positions and sizes are DIPs in the island's own frame.
struct GlassDrop {float x=0,y=0,r=1,speed=0,faded=0;bool alive=true;int into=-1;};
class GlassDrops {
public:
    std::vector<GlassDrop> drops;float width=0,height=0;uint32_t seed=0x5eed5u;
    static constexpr float largest=5.2f;
    float random(){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;}
    void reset(float w,float h,size_t count,uint32_t s=0x5eed5u){seed=s;width=w;height=h;drops.assign(count,{});for(auto& d:drops)spawn(d,true);}
    // The island changed size: drops keep their places, and any now outside it start again inside.
    void resize(float w,float h){width=w;height=h;for(auto& d:drops)if(d.x>w||d.y>h+d.r)spawn(d,false);}
    // A drop's running speed (DIPs per second) for its size and the island's movement.
    static float runSpeed(float r,float shake){return std::max(0.f,r-1.5f)*7.f*(1+3.f*shake)+(shake>.05f?18.f*shake:0.f);}
    void step(float dt,float shake){
        dt=std::clamp(dt,0.f,.5f);shake=std::clamp(shake,0.f,1.f);
        for(auto& d:drops){
            if(!d.alive){d.faded+=dt;if(d.faded>.45f)spawn(d,false);continue;}
            // Clinging: small drops hold on; a little movement, or size, makes one let go.
            const float target=runSpeed(d.r,shake);
            if(d.speed<=0){const float chance=(d.r>2.6f?.35f:.03f)+1.6f*shake;if(target>0&&random()<chance*dt)d.speed=.2f*target;}
            if(d.speed>0){d.speed+=(target-d.speed)*std::min(1.f,dt*3);if(target<=0&&shake<.01f)d.speed=std::max(0.f,d.speed-dt*6);d.y+=d.speed*dt;d.x+=(random()-.5f)*.35f*d.speed*dt;d.x=std::clamp(d.x,0.f,width);}
            if(d.y-d.r>height){d.alive=false;d.faded=.45f;d.into=-1;}}
        // A running drop that meets another takes it in: their areas add.
        for(size_t a=0;a<drops.size();++a){auto& p=drops[a];if(!p.alive||p.speed<=0)continue;
            for(size_t b=0;b<drops.size();++b){if(a==b)continue;auto& q=drops[b];if(!q.alive)continue;const float dx=p.x-q.x,dy=p.y-q.y;
                if(dx*dx+dy*dy<(p.r+q.r)*(p.r+q.r)*.8f){p.r=std::min(largest,std::sqrt(p.r*p.r+q.r*q.r));p.speed=std::max(p.speed,q.speed);q.alive=false;q.faded=0;q.into=int(a);}}}
    }
private:
    void spawn(GlassDrop& d,bool anywhere){d.alive=true;d.faded=0;d.into=-1;d.speed=0;d.x=random()*width;d.y=anywhere?random()*height:random()*height*.45f;d.r=.9f+random()*random()*2.2f;}
};
}
