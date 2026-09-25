#pragma once
#include "Common/Win32.h"
#include <d3d11.h>
#include "Animation/SpringExpression.h"
#include <string>
namespace nexus {
// Real backdrop glass for the island body and its shoulders. A Windows.UI.Composition target sits
// beneath the existing DirectComposition tree on the same HWND and samples the
// desktop through the documented host backdrop brush. Its rounded geometry is
// driven by compositor expressions that evaluate the same analytical springs as
// the DirectComposition body, so the glass morphs at display cadence with no
// per-frame application work. Any failure leaves the solid material in place.
// wallpaper: the desktop's colour (0 when unknown) that tints the material; shadow: the drop shadow's opacity (shadow window only).
struct GlassStyle {bool visible=false,light=false,blur=true;int material=1;float tint=.5f;uint32_t accent=0,wallpaper=0;float shadow=0;};
class GlassBackdrop {
    struct Impl;Impl* impl_=nullptr;bool available_=false;
public:
    GlassBackdrop()=default;GlassBackdrop(const GlassBackdrop&)=delete;GlassBackdrop& operator=(const GlassBackdrop&)=delete;
    ~GlassBackdrop();
    // shadowOnly: the same shape as a silhouette in a layer that casts a soft drop shadow (for the click-through shadow window).
    // device: the Direct3D device the drawn surfaces (grain, the shadow) share, so the backdrop
    // never makes a GPU device of its own (each one costs tens of MB of driver memory).
    bool initialize(HWND,float scale,float canvasWidth,float canvasHeight,bool shadowOnly=false,ID3D11Device* device=nullptr);
    void style(const GlassStyle&);
    void animate(const MotionEngine&,double now,int edge,bool attached);
    bool vibrant()const;
    // Phase 5F: whether the glass can lean with the island while it is dragged.
    bool leans()const;
    bool available()const{return available_;}
    GlassStyle current()const;
    // Why the material or edge light fell back, if they did (for the log).
    std::string failure()const;
    static bool effectsEnabled();
    std::string lastError_;
};
}
