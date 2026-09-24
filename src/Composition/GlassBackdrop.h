#pragma once
#include "Common/Win32.h"
#include "Animation/SpringExpression.h"
#include <string>
namespace nexus {
// Real backdrop glass for the island body and its shoulders. A Windows.UI.Composition target sits
// beneath the existing DirectComposition tree on the same HWND and samples the
// desktop through the documented host backdrop brush. Its rounded geometry is
// driven by compositor expressions that evaluate the same analytical springs as
// the DirectComposition body, so the glass morphs at display cadence with no
// per-frame application work. Any failure leaves the solid material in place.
struct GlassStyle {bool visible=false,light=false,blur=true;int material=1;float tint=.5f;uint32_t accent=0;};
class GlassBackdrop {
    struct Impl;Impl* impl_=nullptr;bool available_=false;
public:
    GlassBackdrop()=default;GlassBackdrop(const GlassBackdrop&)=delete;GlassBackdrop& operator=(const GlassBackdrop&)=delete;
    ~GlassBackdrop();
    bool initialize(HWND,float scale,float canvasWidth,float canvasHeight);
    void style(const GlassStyle&);
    void animate(const MotionEngine&,double now,int edge,bool attached);
    bool vibrant()const;
    bool available()const{return available_;}
    GlassStyle current()const;
    static bool effectsEnabled();
    std::string lastError_;
};
}
