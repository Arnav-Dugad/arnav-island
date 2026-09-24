#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace nexus {
struct SpringSpec { double mass=1, stiffness=390, damping=36; };
namespace MotionTokens {
inline constexpr SpringSpec artwork{.7,420,30},artworkOpacity{1,500,44},icon{.65,640,35},iconPosition{.7,500,35},navigation{.8,450,33},handoff{1,120,22},ring{1,150,25},content{1,380,37},lyrics{1,210,27},drop{1,190,28},dropFade{1,34,12},peek{1,310,35},atmosphere{1,45,14};
}
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

template<class Sample,class Settled>
inline std::vector<CubicSegment> approximateCurve(Sample sample,Settled settled,double now,double& duration){
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
    const SpringSpec& spec() const {return spec_;}
    bool settled(double now) const {auto s=sample(now);return std::abs(s.position-target_)<.015&&std::abs(s.velocity)<.08;}
    // Adaptive cubic Hermite approximation. This is a time curve, never a frame schedule.
    std::vector<CubicSegment> curve(double now,double& duration) const {
        return approximateCurve([this](double t){return sample(t);},[this](double t){return settled(t);},now,duration);
    }
};
// The same damped oscillator as Spring::sample, re-based at `now` so a
// compositor expression can evaluate it from a local clock that starts at zero.
struct SpringTerms {
    enum class Regime { Settled,Critical,Under,Over } regime=Regime::Settled;
    double target=0,x=0,b=0,a=0,w=0,c1=0,c2=0,r1=0,r2=0;
    static SpringTerms from(const Spring& s,double now) {
        SpringTerms t;auto state=s.sample(now);t.target=s.target();t.x=state.position-t.target;const double v=state.velocity;
        if(std::abs(t.x)<1e-6&&std::abs(v)<1e-6)return t;
        const auto& spec=s.spec();t.a=spec.damping/(2*spec.mass);const double w2=spec.stiffness/spec.mass,d=w2-t.a*t.a;
        if(std::abs(d)<1e-7*w2){t.regime=Regime::Critical;t.b=v+t.a*t.x;}
        else if(d>0){t.regime=Regime::Under;t.w=std::sqrt(d);t.b=(v+t.a*t.x)/t.w;}
        else {t.regime=Regime::Over;const double q=std::sqrt(-d);t.r1=-t.a+q;t.r2=-t.a-q;t.c1=(v-t.r2*t.x)/(t.r1-t.r2);t.c2=t.x-t.c1;}
        return t;
    }
    double evaluate(double t) const {
        switch(regime){
        case Regime::Critical:return target+(x+b*t)*std::exp(-a*t);
        case Regime::Under:return target+std::exp(-a*t)*(x*std::cos(w*t)+b*std::sin(w*t));
        case Regime::Over:return target+c1*std::exp(r1*t)+c2*std::exp(r2*t);
        default:return target;
        }
    }
};
// Smoothly follows values that arrive faster than a spring can settle (audio
// levels, meters). Each update is a cubic Hermite segment from the current
// position and velocity to the new value, so the compositor interpolates at the
// display's own refresh rate with continuous velocity.
struct Glide {
    double p0=0,v0=0,p1=0,t0=0,span=0;
    PhysicalState sample(double now)const{double s=now-t0;if(span<=0||s>=span)return {p1,0};s=std::max(0.,s);const double a=(3*(p1-p0)-2*v0*span)/(span*span),b=(2*(p0-p1)+v0*span)/(span*span*span);return {p0+s*(v0+s*(a+s*b)),v0+s*(2*a+3*s*b)};}
    void to(double value,double now,double duration){auto c=sample(now);p0=c.position;v0=c.velocity;p1=value;t0=now;span=duration;}
    CubicSegment segment()const{return {0,p0,v0,(3*(p1-p0)-2*v0*span)/(span*span),(2*(p0-p1)+v0*span)/(span*span*span)};}
};
// Rolling digits. A column shows a strip of 0-9 repeated three times; `position` is where
// the strip stands, in digits. Every turn looks the same, so the column re-bases into the
// middle turn and rolls to the nearest copy of the new digit: 9 to 0 is one step, not nine.
// Targets stay within 5..25, so a spring's overshoot never runs off the strip.
struct DigitRoll{double from,to;};
inline DigitRoll digitRoll(double position,int digit){
    const double base=position-10*std::floor(position/10)+10;double target=10.+digit;
    for(double c:{double(digit),20.+digit})if(std::abs(c-base)<std::abs(target-base))target=c;return {base,target};
}
inline double rubberBand(double displacement,double limit=90) {
    return std::copysign(limit*(1-1/(std::abs(displacement)/limit+1)),displacement);
}
enum class IslandState { Dot,Compact,Standard,LiveActivity,Expanded,Dashboard,FileDrop,Notification,Media,Hardware,Gaming,Command };
struct Geometry {double width,height,radius;};
inline Geometry geometry(IslandState s) {
    switch(s) {
    case IslandState::Dot:return {44,28,14};
    case IslandState::Compact:return {196,34,17};
    case IslandState::Expanded:return {420,334,22};
    case IslandState::Dashboard:return {420,334,22};
    case IslandState::FileDrop:return {440,240,32};
    case IslandState::Media:return {420,176,28};
    case IslandState::Hardware:return {440,230,28};
    case IslandState::Gaming:return {300,58,24};
    case IslandState::Notification:return {372,92,30};
    case IslandState::LiveActivity:return {360,154,22};
    case IslandState::Command:return {420,224,26};
    default:return {300,64,26};
    }
}
struct MotionEngine {
    Spring width{196},height{34},radius{17},lift{0},reveal{0},volume{.5},dragX{0},dragY{0};
    SpringSpec body=preset(MotionPreset::Balanced);
    bool reduced=false,live=false,card=false;int edge=0;double compactWidth=196,corner=22,commandHeight=224;
    Spring artX{12},artY{6},artSize{22},artOpacity{0},pulse{0},hoverX{20},hoverY{38},hoverW{40},hoverH{26},hoverOpacity{0},contentShift{0},swipe{0},level{0},slide{0};
    // Auto-hide fades the island only in the last part of its slide.
    PhysicalState stageOpacity(double now)const{auto s=slide.sample(now);double q=std::clamp((s.position-.45)/.55,0.,1.),dq=(s.position>.45&&s.position<1)?s.velocity/.55:0;return {1-q*q*(3-2*q),-6*q*(1-q)*dq};}
    // A short sideways kick to the resting width when something new arrives.
    void nudge(double now,double strength=160){if(reduced)return;auto s=width.sample(now);double target=width.target();width.reset(s.position,now,s.velocity+strength);width.retarget(target,now,{1,420,20});}
    PhysicalState visibility(double now,bool compactHeader=false)const {
        auto h=height.sample(now),a=reveal.sample(now);double low=compactHeader?(edge?150.:34.):(card?44.:live?76.:230.),range=compactHeader?100.:(card?40.:live?68.:90.);double q=std::clamp((h.position-low)/range,0.,1.),gate=q*q*(3-2*q),speed=(q>0&&q<1)?6*q*(1-q)*h.velocity/range:0;
        if(compactHeader)return {1-gate,-speed};return {gate*a.position,speed*a.position+gate*a.velocity};
    }
    void target(IslandState state,double now,bool hover=false,bool pressed=false) {
        auto g=geometry(state);if(state==IslandState::Compact)g=edge?Geometry{64,150,22}:Geometry{compactWidth,34,17};else if(state==IslandState::Expanded||state==IslandState::Dashboard)g={420,334,corner};else if(state==IslandState::Command)g.height=commandHeight;
        if(reduced){width.reset(g.width,now);height.reset(g.height,now);radius.reset(g.radius,now);reveal.retarget(state!=IslandState::Compact&&g.height>80?1:0,now,{1,1800,85});return;}
        auto s=reduced?SpringSpec{1,1800,85}:body;
        // Liquid morph: the growing dimension leads on a stiffer spring and the other follows
        // on a softer one with a touch more give, so the shape flows instead of scaling.
        const bool opening=g.height>height.target()+1,closing=g.height<height.target()-1;
        const SpringSpec lead{s.mass,s.stiffness*1.3,s.damping*.98},lag{s.mass,s.stiffness*.8,s.damping*.86};
        width.retarget(g.width+(hover?4:0)-(pressed?5:0),now,opening?lead:closing?lag:s);
        height.retarget(g.height-(pressed?2:0),now,opening?lag:closing?lead:s);
        // The corners swell a little mid-morph (a kick on a softer spring), so the outline
        // rounds like a drop of liquid before it settles into its new shape.
        if(opening||closing){auto r=radius.sample(now);radius.reset(r.position,now,r.velocity+(opening?95:70));radius.retarget(g.radius,now,{s.mass,s.stiffness*.62,s.damping*.72});}
        else radius.retarget(g.radius,now,s);
        reveal.retarget(state!=IslandState::Compact&&g.height>80?1:0,now,{1,320,36});
    }
};
}
