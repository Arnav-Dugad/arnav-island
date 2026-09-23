#include "GlassBackdrop.h"
#include "WinCompAbi.h"
#include <dwmapi.h>
#include <memory>
#include <array>
namespace nexus {
using namespace wincomp;
namespace {
std::wstring number(double value){wchar_t b[48];swprintf(b,48,L"(%.9g)",std::isfinite(value)?value:0.);return b;}
// IVector<T> IIDs are parameterized: SHA-1 over the documented pinterface
// signature, e.g. pinterface({913337e9-...};rc(...CompositionShape;{b47ce2f7-...})).
constexpr GUID shapeVector{0x42d4219a,0xbe1b,0x5091,{0x8f,0x1e,0x90,0x27,0x08,0x40,0xfc,0x2d}};
constexpr GUID stopVector{0xbf2e107e,0xf3db,0x56cd,{0x91,0xed,0xc1,0x12,0x94,0x06,0xd5,0x52}};
ComPtr<Vector> vector(const ComPtr<IInspectable>& collection,const GUID& iid){ComPtr<Vector> result;check(collection->QueryInterface(iid,reinterpret_cast<void**>(result.GetAddressOf())));return result;}
ComPtr<IInspectable> dispatcherController(){
    // One queue per UI thread; the island owns exactly one UI thread.
    thread_local ComPtr<ABI::Windows::System::IDispatcherQueueController> controller;
    if(!controller){DispatcherQueueOptions options{sizeof(options),DQTYPE_THREAD_CURRENT,DQTAT_COM_STA};check(CreateDispatcherQueueController(options,&controller));}
    return controller;
}
}
struct GlassBackdrop::Impl {
    ComPtr<IInspectable> queue;ComPtr<wuc::ICompositor> compositor;ComPtr<IInspectable> target;
    ComPtr<wuc::IContainerVisual> root,glass;ComPtr<wuc::ISpriteVisual> backdrop,tint,depth,sheen;ComPtr<IInspectable> rim,geometry,rimShape;
    ComPtr<wuc::ICompositionColorBrush> tintBrush;ComPtr<IInspectable> sheenBrush,rimBrush,depthBrush;ComPtr<wuc::ICompositionPropertySet> properties;
    std::array<ComPtr<IInspectable>,9> stops;float scale=1,canvasWidth=600,canvasHeight=500;int edge=-1;GlassStyle current;bool styled=false;
    ComPtr<wuc::IVisual> visual(const ComPtr<IInspectable>& v){return as<wuc::IVisual>(v);}
    void fill(const ComPtr<IInspectable>& v){check(as<Visual2>(v)->put_RelativeSizeAdjustment({1,1}));}
    ComPtr<wuc::IExpressionAnimation> expression(const std::wstring& text){ComPtr<wuc::IExpressionAnimation> a;String s(text.c_str());check(compositor->CreateExpressionAnimationWithExpression(s,&a));String p(L"p");check(as<wuc::ICompositionAnimation>(a)->SetReferenceParameter(p,as<wuc::ICompositionObject>(properties).Get()));return a;}
    void start(const ComPtr<IInspectable>& object,const wchar_t* property,const std::wstring& text){auto a=expression(text);String name(property);check(as<wuc::ICompositionObject>(object)->StartAnimation(name,as<wuc::ICompositionAnimation>(a).Get()));}
    ComPtr<IInspectable> stop(float offset,Color color){ComPtr<IInspectable> s;check(as<Compositor4>(compositor)->CreateColorGradientStopWithOffsetAndColor(offset,color,&s));return s;}
    ComPtr<IInspectable> gradient(Vector2 from,Vector2 to,std::initializer_list<std::pair<float,Color>> colors,size_t firstStop){
        ComPtr<IInspectable> brush;check(as<Compositor4>(compositor)->CreateLinearGradientBrush(&brush));auto linear=as<LinearGradientBrush>(brush);check(linear->put_StartPoint(from));check(linear->put_EndPoint(to));
        ComPtr<IInspectable> collection;check(as<GradientBrush>(brush)->get_ColorStops(&collection));auto list=vector(collection,stopVector);size_t i=firstStop;for(auto& [offset,color]:colors){stops[i]=stop(offset,color);check(list->Append(stops[i].Get()));++i;}return brush;
    }
};
GlassBackdrop::~GlassBackdrop(){
    if(!impl_)return;
    if(impl_->target){ComPtr<ABI::Windows::Foundation::IClosable> closable;if(SUCCEEDED(impl_->target.As(&closable)))closable->Close();}
    delete impl_;
}
GlassStyle GlassBackdrop::current()const{return impl_?impl_->current:GlassStyle{};}
bool GlassBackdrop::effectsEnabled(){
    DWORD value=1,size=sizeof(value);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"EnableTransparency",RRF_RT_REG_DWORD,nullptr,&value,&size)!=ERROR_SUCCESS)value=1;
    SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);HIGHCONTRASTW contrast{sizeof(contrast)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);
    return value!=0&&!power.SystemStatusFlag&&!(contrast.dwFlags&HCF_HIGHCONTRASTON);
}
bool GlassBackdrop::initialize(HWND window,float scale,float canvasWidth,float canvasHeight){
    try{
        auto impl=std::make_unique<Impl>();impl->scale=scale;impl->canvasWidth=canvasWidth;impl->canvasHeight=canvasHeight;impl->queue=dispatcherController();
        String name(L"Windows.UI.Composition.Compositor");ComPtr<IInspectable> instance;check(RoActivateInstance(name,&instance));check(instance.As(&impl->compositor));
        BOOL host=TRUE;check(DwmSetWindowAttribute(window,DWMWA_USE_HOSTBACKDROPBRUSH,&host,sizeof(host)));
        check(as<CompositorDesktopInterop>(impl->compositor)->CreateDesktopWindowTarget(window,FALSE,&impl->target));
        auto& c=impl->compositor;check(c->CreateContainerVisual(&impl->root));check(c->CreateContainerVisual(&impl->glass));
        check(as<wuc::ICompositionTarget>(impl->target)->put_Root(impl->visual(impl->root).Get()));
        ComPtr<wuc::IVisualCollection> rootChildren;check(impl->root->get_Children(&rootChildren));check(rootChildren->InsertAtTop(impl->visual(impl->glass).Get()));
        check(impl->visual(impl->root)->put_Size({canvasWidth*scale,canvasHeight*scale}));
        ComPtr<IInspectable> hostBrush;check(as<Compositor3>(c)->CreateHostBackdropBrush(&hostBrush));
        for(auto* sprite:{std::addressof(impl->backdrop),std::addressof(impl->tint),std::addressof(impl->depth),std::addressof(impl->sheen)})check(c->CreateSpriteVisual(sprite->GetAddressOf()));
        check(impl->backdrop->put_Brush(as<wuc::ICompositionBrush>(hostBrush).Get()));
        check(c->CreateColorBrushWithColor(color(0x0c0d11,.55f),&impl->tintBrush));check(impl->tint->put_Brush(as<wuc::ICompositionBrush>(impl->tintBrush).Get()));
        check(as<Compositor5>(c)->CreateRoundedRectangleGeometry(&impl->geometry));ComPtr<IInspectable> clip;check(as<Compositor6>(c)->CreateGeometricClipWithGeometry(impl->geometry.Get(),&clip));
        check(impl->visual(impl->glass)->put_Clip(as<wuc::ICompositionClip>(clip).Get()));
        check(as<Compositor5>(c)->CreateShapeVisual(&impl->rim));check(as<Compositor5>(c)->CreateSpriteShapeWithGeometry(impl->geometry.Get(),&impl->rimShape));
        ComPtr<IInspectable> shapes;check(as<ShapeVisual>(impl->rim)->get_Shapes(&shapes));check(vector(shapes,shapeVector)->Append(impl->rimShape.Get()));
        check(as<SpriteShape>(impl->rimShape)->put_StrokeThickness(2*scale));
        ComPtr<wuc::IVisualCollection> children;check(impl->glass->get_Children(&children));
        for(auto& v:{ComPtr<IInspectable>(impl->backdrop),ComPtr<IInspectable>(impl->tint),ComPtr<IInspectable>(impl->depth),ComPtr<IInspectable>(impl->sheen),impl->rim}){impl->fill(v);check(children->InsertAtTop(impl->visual(v).Get()));}
        check(c->CreatePropertySet(&impl->properties));for(auto key:{L"t",L"w",L"h",L"r",L"dx",L"dy",L"s"}){String k(key);check(impl->properties->InsertScalar(k,0.f));}
        check(impl->visual(impl->glass)->put_IsVisible(false));
        impl_=impl.release();available_=true;
    }catch(...){available_=false;}
    return available_;
}
void GlassBackdrop::style(const GlassStyle& s){
    if(!available_)return;auto& i=*impl_;
    try{
        check(i.visual(i.glass)->put_IsVisible(s.visible));if(!s.visible){i.current.visible=false;return;}
        if(i.styled&&i.current.light==s.light&&i.current.blur==s.blur&&i.current.material==s.material&&std::abs(i.current.tint-s.tint)<.001f&&i.current.accent==s.accent){i.current=s;return;}
        i.current=s;i.styled=true;
        // Frosted: Windows' blurred backdrop when it allows one, otherwise a dense
        // translucent frost. Clear: never blurred, a light tint you see through.
        // Neither ever falls back to an opaque fill.
        const bool clear=s.material==2,blurred=!clear&&s.blur;
        check(i.visual(i.backdrop)->put_IsVisible(blurred));
        const float tint=std::clamp(s.tint,0.f,1.f);
        float alpha=clear?(s.light?.22f:.30f)*(.55f+tint*1.1f):blurred?(s.light?.54f:.44f)*(.55f+tint*.9f):(s.light?.74f:.68f)*(.8f+tint*.35f);
        alpha=std::clamp(alpha,clear?.08f:.12f,clear?.62f:.92f);
        uint32_t ink=s.light?0xf6f7f9:0x0b0c10;
        if(s.accent&&!s.light){auto mix=[&](int shift){return uint32_t(((ink>>shift)&255)*.88+((s.accent>>shift)&255)*.12)<<shift;};ink=mix(16)|mix(8)|mix(0);}
        check(i.tintBrush->put_Color(color(ink,alpha)));
        // Depth: glass darkens slightly toward its lower edge, as thick glass does.
        auto shade=s.light?color(0x000000,clear?.07f:.035f):color(0x000000,clear?.22f:.12f),none=color(0x000000,0);
        i.depthBrush=i.gradient({0,0},{0,1},{{0,none},{.55f,none},{1,shade}},6);check(i.depth->put_Brush(as<wuc::ICompositionBrush>(i.depthBrush).Get()));
        auto top=color(0xffffff,clear?(s.light?.55f:.16f):(s.light?.42f:.10f)),transparent=color(0xffffff,0);
        i.sheenBrush=i.gradient({0,0},{0,1},{{0,top},{clear?.38f:.46f,transparent},{1,transparent}},0);check(i.sheen->put_Brush(as<wuc::ICompositionBrush>(i.sheenBrush).Get()));
        // The rim catches light at the top and fades underneath; stronger on clear glass, which has no blur to separate it.
        auto rimTop=color(0xffffff,s.light?.9f:clear?.42f:.30f),rimBottom=s.light?color(0x000000,clear?.14f:.08f):color(0xffffff,clear?.10f:.07f);
        i.rimBrush=i.gradient({0,0},{0,1},{{0,rimTop},{.5f,s.light?color(0xffffff,clear?.35f:.25f):color(0xffffff,clear?.16f:.11f)},{1,rimBottom}},3);check(as<SpriteShape>(i.rimShape)->put_StrokeBrush(as<wuc::ICompositionBrush>(i.rimBrush).Get()));
    }catch(...){}
}
void GlassBackdrop::animate(const MotionEngine& m,double now,int edge){
    if(!available_)return;auto& i=*impl_;
    try{
        const double s=i.scale;
        // The expression clock starts on the frame that receives this commit; begin
        // it one refresh ahead so it lines up with the absolute-time DComp curves.
        double lead=0;DWM_TIMING_INFO timing{sizeof(timing)};LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
        if(SUCCEEDED(DwmGetCompositionTimingInfo(nullptr,&timing))&&timing.qpcRefreshPeriod){const double period=double(timing.qpcRefreshPeriod)/frequency.QuadPart,vblank=double(timing.qpcVBlank)/frequency.QuadPart;double next=vblank;while(next<now)next+=period;lead=std::clamp(next-now+period,0.,.05);}
        String t(L"t");ComPtr<wuc::IScalarKeyFrameAnimation> clock;check(i.compositor->CreateScalarKeyFrameAnimation(&clock));
        ComPtr<wuc::ICompositionEasingFunction> linear;{ComPtr<wuc::ILinearEasingFunction> l;check(i.compositor->CreateLinearEasingFunction(&l));check(l.As(&linear));}
        const float span=6;check(clock->InsertKeyFrame(0,float(lead)));check(clock->InsertKeyFrameWithEasingFunction(1,float(lead+span),linear.Get()));
        ABI::Windows::Foundation::TimeSpan duration{LONGLONG(span*1e7)};check(as<KeyFrameAnimation>(clock)->put_Duration(duration));
        ComPtr<IInspectable> props;check(i.properties.As(&props));
        i.start(props,L"w",springExpression(SpringTerms::from(m.width,now),s));i.start(props,L"h",springExpression(SpringTerms::from(m.height,now),s));
        i.start(props,L"r",springExpression(SpringTerms::from(m.radius,now),s));i.start(props,L"dx",springExpression(SpringTerms::from(m.dragX,now),s));i.start(props,L"dy",springExpression(SpringTerms::from(m.dragY,now),s));i.start(props,L"s",springExpression(SpringTerms::from(m.slide,now),1));
        check(as<wuc::ICompositionObject>(i.properties)->StartAnimation(t,as<wuc::ICompositionAnimation>(clock).Get()));
        if(edge!=i.edge){
            i.edge=edge;const std::wstring cw=number(i.canvasWidth*s),ch=number(i.canvasHeight*s);
            ComPtr<IInspectable> glass;check(i.glass.As(&glass));
            i.start(glass,L"Offset",edge?L"Vector3("+cw+L"-p.w+p.dx+p.s*"+number(74*s)+L",("+ch+L"-p.h)/2+p.dy,0)":L"Vector3(("+cw+L"-p.w)/2+p.dx,p.dy-p.s*"+number(44*s)+L",0)");
            i.start(glass,L"Opacity",L"1-Clamp((p.s-0.45)/0.55,0,1)*Clamp((p.s-0.45)/0.55,0,1)*(3-2*Clamp((p.s-0.45)/0.55,0,1))");
            i.start(glass,L"Size",L"Vector2(p.w,p.h)");
            i.start(i.geometry,L"Size",L"Vector2(p.w,p.h)");i.start(i.geometry,L"CornerRadius",L"Vector2(Min(p.r,Min(p.w,p.h)/2),Min(p.r,Min(p.w,p.h)/2))");
        }
    }catch(const std::exception& e){lastError_=e.what();}
}
}
