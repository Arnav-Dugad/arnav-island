#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace nexus {
struct SpringSpec { double mass=1, stiffness=390, damping=36; };
enum class MotionPreset { Balanced, Fluid, Playful, Snappy, Calm };
inline SpringSpec preset(MotionPreset p) {
    switch(p) {
    case MotionPreset::Fluid: return {1.15,360,35};
    case MotionPreset::Playful: return {1,380,25};
    case MotionPreset::Snappy: return {.85,640,43};
    case MotionPreset::Calm: return {1.3,300,40};
    default: return {1,390,36};
    }
}
struct PhysicalState { double position=0, velocity=0; };
struct CubicSegment { double time, p, v, quadratic, cubic; };

class Spring {
    PhysicalState initial_{};
    double target_=0, epoch_=0;
    SpringSpec spec_{};
public:
    explicit Spring(double value=0): initial_{value,0}, target_(value) {}
    static void validate(SpringSpec s) {
        if(!std::isfinite(s.mass)||!std::isfinite(s.stiffness)||!std::isfinite(s.damping)||s.mass<=0||s.stiffness<=0||s.damping<=0)
            throw std::invalid_argument("Invalid physical spring parameters");
    }
    PhysicalState sample(double now) const {
        const double t=std::max(0.0,now-epoch_);
        const double a=spec_.damping/(2*spec_.mass), w2=spec_.stiffness/spec_.mass;
        const double x=initial_.position-target_, v=initial_.velocity, d=w2-a*a;
        double y,dy;
        if(std::abs(d)<1e-7*w2) {
            const double b=v+a*x, e=std::exp(-a*t);
            y=(x+b*t)*e; dy=(b-a*(x+b*t))*e;
        } else if(d>0) {
            const double w=std::sqrt(d), b=(v+a*x)/w, e=std::exp(-a*t);
            const double c=std::cos(w*t), s=std::sin(w*t);
            y=e*(x*c+b*s); dy=e*((-a*x+w*b)*c+(-a*b-w*x)*s);
        } else {
            const double q=std::sqrt(-d), r1=-a+q,r2=-a-q;
            const double c1=(v-r2*x)/(r1-r2),c2=x-c1;
            y=c1*std::exp(r1*t)+c2*std::exp(r2*t);
            dy=r1*c1*std::exp(r1*t)+r2*c2*std::exp(r2*t);
        }
        return {target_+y,dy};
    }
    void retarget(double target,double now,SpringSpec spec) {
        validate(spec);
        if(!std::isfinite(target)||!std::isfinite(now)) throw std::invalid_argument("Invalid target");
        initial_=sample(now); target_=target; epoch_=now; spec_=spec;
    }
    void reset(double value,double now,double velocity=0) { initial_={value,velocity}; target_=value; epoch_=now; }
    double target() const {return target_;}
    bool settled(double now) const {auto s=sample(now);return std::abs(s.position-target_)<.015&&std::abs(s.velocity)<.08;}
    // Adaptive cubic Hermite approximation. This is a time curve, never a frame schedule.
    std::vector<CubicSegment> curve(double now,double& duration) const {
        std::vector<CubicSegment> result;
        double t=0;
        while(t<8) {
            double h=.032;
            auto a=sample(now+t);
            double b2=0,b3=0;
            for(;;) {
                auto b=sample(now+t+h);
                b2=(3*(b.position-a.position)/h-2*a.velocity-b.velocity)/h;
                b3=(2*(a.position-b.position)/h+a.velocity+b.velocity)/(h*h);
                bool accurate=true;
                for(double f : {.25,.5,.75}) {
                    double u=h*f,p=a.position+u*(a.velocity+u*(b2+u*b3));
                    if(std::abs(p-sample(now+t+u).position)>.002) accurate=false;
                }
                if(accurate||h<.001) break;
                h*=.5;
            }
            result.push_back({t,a.position,a.velocity,b2,b3});t+=h;
            if(settled(now+t)) break;
        }
        duration=t;return result;
    }
};
inline double rubberBand(double displacement,double limit=90) {
    return std::copysign(limit*(1-1/(std::abs(displacement)/limit+1)),displacement);
}
enum class IslandState { Dot,Compact,Standard,LiveActivity,Expanded,Dashboard,FileDrop,Notification,Media,Hardware,Gaming };
struct Geometry {double width,height,radius;};
inline Geometry geometry(IslandState s) {
    switch(s) {
    case IslandState::Dot:return {44,28,14};
    case IslandState::Compact:return {204,38,19};
    case IslandState::Expanded:return {480,284,30};
    case IslandState::Dashboard:return {520,340,32};
    case IslandState::FileDrop:return {440,240,32};
    case IslandState::Media:return {420,176,28};
    case IslandState::Hardware:return {440,230,28};
    case IslandState::Gaming:return {300,58,24};
    case IslandState::Notification:return {440,138,26};
    case IslandState::LiveActivity:return {340,94,26};
    default:return {300,64,26};
    }
}
struct MotionEngine {
    Spring width{204},height{38},radius{19},lift{0},reveal{0},volume{.5},dragX{0},dragY{0};
    SpringSpec body=preset(MotionPreset::Balanced);
    bool reduced=false;
    void target(IslandState state,double now,bool hover=false,bool pressed=false) {
        auto g=geometry(state);
        auto s=reduced?SpringSpec{1,1800,85}:body;
        width.retarget(g.width+(hover?4:0)-(pressed?5:0),now,s);
        height.retarget(g.height-(pressed?2:0),now,s);
        radius.retarget(g.radius,now,s);
        reveal.retarget(g.height>110?1:0,now,{1,320,36});
    }
};
}
