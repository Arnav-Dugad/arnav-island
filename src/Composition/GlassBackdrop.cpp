#include "GlassBackdrop.h"
#include "WinCompAbi.h"
#include "Composition/DockGeometry.h"
#include <dwmapi.h>
#include <memory>
#include <array>
#include <atomic>
#include <vector>
namespace nexus {
using namespace wincomp;
// Named namespace: hand-declared interfaces must never sit in an anonymous one
// (GCC may treat calls through them as unreachable; see Ocr.cpp).
namespace glassabi {
namespace effects=ABI::Windows::Graphics::Effects;
std::wstring number(double value){return expressionNumber(value);}
// One Direct2D figure, rebuilt with whichever factory composition asks for.
struct Figure {
    struct Segment{bool curve;D2D1_POINT_2F a,b,c;};
    D2D1_POINT_2F start{};std::vector<Segment> segments;bool closed=true;
    void line(D2D1_POINT_2F p){segments.push_back({false,p,{},{}});}
    void bezier(D2D1_POINT_2F a,D2D1_POINT_2F b,D2D1_POINT_2F c){segments.push_back({true,a,b,c});}
};
class PathSource final:public GeometrySource2D,public GeometrySource2DInterop {
    std::atomic<ULONG> refs_{1};Figure figure_;ComPtr<ID2D1Factory> own_;
public:
    explicit PathSource(Figure figure):figure_(std::move(figure)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{
        if(!out)return E_POINTER;
        if(id==__uuidof(IUnknown)||id==__uuidof(IInspectable)||id==__uuidof(GeometrySource2D))*out=static_cast<GeometrySource2D*>(this);
        else if(id==__uuidof(GeometrySource2DInterop))*out=static_cast<GeometrySource2DInterop*>(this);
        else{*out=nullptr;return E_NOINTERFACE;}
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{const ULONG n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count,IID** ids)override{if(!count||!ids)return E_POINTER;*count=0;*ids=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name)override{if(!name)return E_POINTER;*name=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level)override{if(!level)return E_POINTER;*level=BaseTrust;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetGeometry(ID2D1Geometry** out)override{
        if(!own_){const HRESULT hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,own_.GetAddressOf());if(FAILED(hr))return hr;}
        return TryGetGeometryUsingFactory(own_.Get(),out);
    }
    HRESULT STDMETHODCALLTYPE TryGetGeometryUsingFactory(ID2D1Factory* factory,ID2D1Geometry** out)override{
        if(!out)return E_POINTER;*out=nullptr;if(!factory)return E_INVALIDARG;
        ComPtr<ID2D1PathGeometry> path;HRESULT hr=factory->CreatePathGeometry(&path);if(FAILED(hr))return hr;
        ComPtr<ID2D1GeometrySink> sink;hr=path->Open(&sink);if(FAILED(hr))return hr;
        sink->BeginFigure(figure_.start,figure_.closed?D2D1_FIGURE_BEGIN_FILLED:D2D1_FIGURE_BEGIN_HOLLOW);
        for(auto& s:figure_.segments){if(s.curve)sink->AddBezier(D2D1::BezierSegment(s.a,s.b,s.c));else sink->AddLine(s.a);}
        sink->EndFigure(figure_.closed?D2D1_FIGURE_END_CLOSED:D2D1_FIGURE_END_OPEN);hr=sink->Close();if(FAILED(hr))return hr;
        *out=path.Detach();return S_OK;
    }
};
// Direct2D's colour-matrix effect over one named source: composition validates
// and runs it on the GPU, so the backdrop gains vibrancy at no CPU cost.
constexpr GUID colorMatrixEffect{0x921f03d6,0x641c,0x47df,{0x85,0x2d,0xb4,0xbb,0x61,0x53,0xae,0x11}};
class ColorMatrix final:public effects::IGraphicsEffect,public effects::IGraphicsEffectSource,public GraphicsEffectD2D1Interop {
    std::atomic<ULONG> refs_{1};HSTRING name_=nullptr;std::array<float,20> matrix_;ComPtr<effects::IGraphicsEffectSource> source_;
public:
    ColorMatrix(const std::array<float,20>& matrix,ComPtr<effects::IGraphicsEffectSource> source):matrix_(matrix),source_(std::move(source)){}
    ~ColorMatrix(){WindowsDeleteString(name_);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{
        if(!out)return E_POINTER;
        if(id==__uuidof(IUnknown)||id==__uuidof(IInspectable)||id==__uuidof(effects::IGraphicsEffect))*out=static_cast<effects::IGraphicsEffect*>(this);
        else if(id==__uuidof(effects::IGraphicsEffectSource))*out=static_cast<effects::IGraphicsEffectSource*>(this);
        else if(id==__uuidof(GraphicsEffectD2D1Interop))*out=static_cast<GraphicsEffectD2D1Interop*>(this);
        else{*out=nullptr;return E_NOINTERFACE;}
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{const ULONG n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count,IID** ids)override{if(!count||!ids)return E_POINTER;*count=0;*ids=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name)override{if(!name)return E_POINTER;*name=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level)override{if(!level)return E_POINTER;*level=BaseTrust;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_Name(HSTRING* name)override{if(!name)return E_POINTER;return WindowsDuplicateString(name_,name);}
    HRESULT STDMETHODCALLTYPE put_Name(HSTRING name)override{HSTRING copy=nullptr;const HRESULT hr=WindowsDuplicateString(name,&copy);if(FAILED(hr))return hr;WindowsDeleteString(name_);name_=copy;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id)override{if(!id)return E_POINTER;*id=colorMatrixEffect;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR,UINT*,INT32*)override{return E_INVALIDARG;}
    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count)override{if(!count)return E_POINTER;*count=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetProperty(UINT index,ABI::Windows::Foundation::IPropertyValue** value)override{
        if(!value)return E_POINTER;*value=nullptr;if(index!=0)return E_INVALIDARG;
        HSTRING_HEADER header;HSTRING cls=nullptr;static constexpr wchar_t name[]=L"Windows.Foundation.PropertyValue";
        HRESULT hr=WindowsCreateStringReference(name,UINT32(std::size(name)-1),&header,&cls);if(FAILED(hr))return hr;
        ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> statics;hr=RoGetActivationFactory(cls,__uuidof(ABI::Windows::Foundation::IPropertyValueStatics),reinterpret_cast<void**>(statics.GetAddressOf()));if(FAILED(hr))return hr;
        ComPtr<IInspectable> boxed;hr=statics->CreateSingleArray(UINT32(matrix_.size()),matrix_.data(),&boxed);if(FAILED(hr))return hr;
        return boxed->QueryInterface(__uuidof(ABI::Windows::Foundation::IPropertyValue),reinterpret_cast<void**>(value));
    }
    HRESULT STDMETHODCALLTYPE GetSource(UINT index,effects::IGraphicsEffectSource** source)override{if(!source)return E_POINTER;*source=nullptr;if(index!=0)return E_INVALIDARG;return source_.CopyTo(source);}
    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count)override{if(!count)return E_POINTER;*count=1;return S_OK;}
};
// Saturation about Rec. 709 luminance, as a 5x4 row-vector colour matrix.
std::array<float,20> saturation(float s){
    const float l[3]={.2126f,.7152f,.0722f};std::array<float,20> m{};
    for(int in=0;in<3;++in)for(int out=0;out<3;++out)m[in*4+out]=(1-s)*l[in]+(in==out?s:0.f);
    m[15]=1;return m;
}
}
using glassabi::number;using glassabi::Figure;
namespace {
ComPtr<Vector> vector(const ComPtr<IInspectable>& collection,const GUID& iid){ComPtr<Vector> result;check(collection->QueryInterface(iid,reinterpret_cast<void**>(result.GetAddressOf())));return result;}
// IVector<T> IIDs are parameterized: SHA-1 over the documented pinterface
// signature, e.g. pinterface({913337e9-...};rc(...CompositionShape;{b47ce2f7-...})).
constexpr GUID shapeVector{0x42d4219a,0xbe1b,0x5091,{0x8f,0x1e,0x90,0x27,0x08,0x40,0xfc,0x2d}};
constexpr GUID stopVector{0xbf2e107e,0xf3db,0x56cd,{0x91,0xed,0xc1,0x12,0x94,0x06,0xd5,0x52}};
ComPtr<IInspectable> dispatcherController(){
    // One queue per UI thread; the island owns exactly one UI thread.
    thread_local ComPtr<ABI::Windows::System::IDispatcherQueueController> controller;
    if(!controller){DispatcherQueueOptions options{sizeof(options),DQTYPE_THREAD_CURRENT,DQTAT_COM_STA};check(CreateDispatcherQueueController(options,&controller));}
    return controller;
}
}
// The glass is one piece: a body whose screen-side corners run off the edge, and
// (when attached) two concave shoulders clipped by real path geometry. Every part
// holds its own copy of the same layers, sized to one shared frame, so tint,
// depth and sheen continue across the joins; the joins sit on whole pixels, so
// see-through glass never shows a seam.
struct GlassBackdrop::Impl {
    ComPtr<IInspectable> queue;ComPtr<wuc::ICompositor> compositor;ComPtr<IInspectable> target;ComPtr<wuc::ICompositionPropertySet> properties;
    ComPtr<wuc::IContainerVisual> root,glass;
    struct Part{ComPtr<wuc::IContainerVisual> container;std::array<ComPtr<wuc::ISpriteVisual>,4> layers;ComPtr<IInspectable> clip;};
    std::array<Part,3> parts;ComPtr<IInspectable> geometry;// body rounded rectangle, shared by its clip and its rim
    ComPtr<IInspectable> rim;ComPtr<wuc::IInsetClip> rimClip;std::array<ComPtr<IInspectable>,3> rimShapes;
    struct ShoulderRim{ComPtr<IInspectable> visual;std::array<ComPtr<IInspectable>,3> shapes;};std::array<ShoulderRim,2> shoulderRims;
    ComPtr<IInspectable> hostBrush,vibrantDark,vibrantLight;ComPtr<wuc::ICompositionColorBrush> tintBrush;
    ComPtr<IInspectable> depthBrush,sheenBrush,rimBrush,glowBrush;std::array<ComPtr<wuc::ICompositionColorBrush>,4> edgeBrushes;// rim top, rim bottom, glow top, glow bottom
    float scale=1,canvasWidth=600,canvasHeight=500,radius=0;int edge=-1;bool attached=false;GlassStyle current;bool styled=false;
    ComPtr<wuc::IVisual> visual(const ComPtr<IInspectable>& v){return as<wuc::IVisual>(v);}
    ComPtr<wuc::IExpressionAnimation> expression(const std::wstring& text){ComPtr<wuc::IExpressionAnimation> a;String s(text.c_str());check(compositor->CreateExpressionAnimationWithExpression(s,&a));String p(L"p");check(as<wuc::ICompositionAnimation>(a)->SetReferenceParameter(p,as<wuc::ICompositionObject>(properties).Get()));return a;}
    template<class T> void start(const ComPtr<T>& object,const wchar_t* property,const std::wstring& text){auto a=expression(text);String name(property);check(as<wuc::ICompositionObject>(object)->StartAnimation(name,as<wuc::ICompositionAnimation>(a).Get()));}
    ComPtr<IInspectable> stop(float offset,Color color){ComPtr<IInspectable> s;check(as<Compositor4>(compositor)->CreateColorGradientStopWithOffsetAndColor(offset,color,&s));return s;}
    ComPtr<IInspectable> gradient(bool absolute){
        ComPtr<IInspectable> brush;check(as<Compositor4>(compositor)->CreateLinearGradientBrush(&brush));auto linear=as<LinearGradientBrush>(brush);check(linear->put_StartPoint({0,0}));check(linear->put_EndPoint({0,1}));
        if(absolute)check(as<GradientBrush2>(brush)->put_MappingMode(0));
        ComPtr<IInspectable> collection;check(as<GradientBrush>(brush)->get_ColorStops(&collection));auto list=vector(collection,stopVector);for(int i=0;i<3;++i)check(list->Append(stop(i*.5f,color(0,0)).Get()));return brush;
    }
    // Stops are replaced in place, so brushes (and animations on them) persist across restyles.
    void stops(const ComPtr<IInspectable>& brush,std::initializer_list<std::pair<float,Color>> colors){
        ComPtr<IInspectable> collection;check(as<GradientBrush>(brush)->get_ColorStops(&collection));auto list=vector(collection,stopVector);UINT32 i=0;for(auto& [offset,c]:colors)check(list->SetAt(i++,stop(offset,c).Get()));
    }
    ComPtr<IInspectable> vibrancy(float amount){
        String parameterClass(L"Windows.UI.Composition.CompositionEffectSourceParameter"),name(L"backdrop");
        ComPtr<EffectSourceParameterFactory> parameters;check(RoGetActivationFactory(parameterClass,__uuidof(EffectSourceParameterFactory),reinterpret_cast<void**>(parameters.GetAddressOf())));
        ComPtr<IInspectable> parameter;check(parameters->Create(name,&parameter));
        ComPtr<glassabi::effects::IGraphicsEffect> effect;effect.Attach(new glassabi::ColorMatrix(glassabi::saturation(amount),as<glassabi::effects::IGraphicsEffectSource>(parameter)));
        ComPtr<wuc::ICompositionEffectFactory> factory;check(compositor->CreateEffectFactory(effect.Get(),&factory));
        wuc::CompositionEffectFactoryLoadStatus status{};check(factory->get_LoadStatus(&status));if(status!=wuc::CompositionEffectFactoryLoadStatus_Success)throw std::runtime_error("vibrancy effect unavailable");
        ComPtr<wuc::ICompositionEffectBrush> brush;check(factory->CreateBrush(&brush));check(brush->SetSourceParameter(name,as<wuc::ICompositionBrush>(hostBrush).Get()));
        return brush;
    }
    ComPtr<IInspectable> path(const Figure& figure){
        String cls(L"Windows.UI.Composition.CompositionPath");ComPtr<CompositionPathFactory> factory;check(RoGetActivationFactory(cls,__uuidof(CompositionPathFactory),reinterpret_cast<void**>(factory.GetAddressOf())));
        ComPtr<GeometrySource2D> source;source.Attach(new glassabi::PathSource(figure));
        ComPtr<IInspectable> compositionPath,geometry;check(factory->Create(source.Get(),&compositionPath));check(as<Compositor5>(compositor)->CreatePathGeometryWithPath(compositionPath.Get(),&geometry));return geometry;
    }
    ComPtr<IInspectable> geometricClip(const ComPtr<IInspectable>& geometry){ComPtr<IInspectable> clip;check(as<Compositor6>(compositor)->CreateGeometricClipWithGeometry(geometry.Get(),&clip));return clip;}
    ComPtr<IInspectable> shape(const ComPtr<IInspectable>& shapeVisual,float thickness){
        ComPtr<IInspectable> sprite;check(as<Compositor5>(compositor)->CreateSpriteShape(&sprite));check(as<SpriteShape>(sprite)->put_StrokeThickness(thickness*scale));check(as<SpriteShape>(sprite)->put_IsStrokeNonScaling(true));
        ComPtr<IInspectable> shapes;check(as<ShapeVisual>(shapeVisual)->get_Shapes(&shapes));check(vector(shapes,shapeVector)->Append(sprite.Get()));return sprite;
    }
    void layout();
};
void GlassBackdrop::Impl::layout(){
    const double s=scale;auto px=[&](double v){return number(v*s);};
    const std::wstring cw=number(canvasWidth*s),ch=number(canvasHeight*s);
    // Whole-pixel edges: the body and its shoulders meet on pixel boundaries at every frame.
    const std::wstring L=L"Round(("+cw+L"-p.w)/2)",Rt=L"Round(("+cw+L"+p.w)/2)",H=L"Round(p.h)",X0=L"Round("+cw+L"-p.w)",T=L"Round(("+ch+L"-p.h)/2)",B=L"Round(("+ch+L"+p.h)/2)";
    const std::wstring rc=L"Min(p.r,Min(p.w,p.h)/2)",E=L"("+rc+L"+"+px(2)+L")",M=L"(2*p.r+"+px(4)+L")";
    start(glass,L"Offset",edge?L"Vector3(Round(p.dx+p.s*"+px(74)+L"),Round(p.dy),0)":L"Vector3(Round(p.dx),Round(p.dy-p.s*"+px(44)+L"),0)");
    start(glass,L"Opacity",L"1-Clamp((p.s-0.45)/0.55,0,1)*Clamp((p.s-0.45)/0.55,0,1)*(3-2*Clamp((p.s-0.45)/0.55,0,1))");
    // Attached, the screen-side corners run past the edge (E), so the body meets the screen square.
    std::wstring offset,size,layerOffset,layerSize;
    if(!edge){
        offset=L"Vector2("+L+L","+(attached?L"-"+E:L"0")+L")";size=L"Vector2("+Rt+L"-"+L+L","+H+(attached?L"+"+E:L"")+L")";
        layerOffset=L"Vector3("+L+(attached?L"-"+M:L"")+L",0,0)";layerSize=L"Vector2("+Rt+L"-"+L+(attached?L"+2*"+M:L"")+L","+H+L")";
    }else{
        offset=L"Vector2("+X0+L","+T+L")";size=L"Vector2("+cw+L"-"+X0+(attached?L"+"+E:L"")+L","+B+L"-"+T+L")";
        layerOffset=L"Vector3("+X0+L","+T+(attached?L"-"+M:L"")+L",0)";layerSize=L"Vector2("+cw+L"-"+X0+L","+B+L"-"+T+(attached?L"+2*"+M:L"")+L")";
    }
    start(geometry,L"Offset",offset);start(geometry,L"Size",size);start(geometry,L"CornerRadius",L"Vector2("+rc+L","+rc+L")");
    for(auto& part:parts)for(auto& layer:part.layers){start(layer,L"Offset",layerOffset);start(layer,L"Size",layerSize);}
    // The body's rim stops where a shoulder takes over (the shoulder is exactly r deep).
    start(rimClip,L"TopInset",!edge&&attached?L"p.r":L"0");start(rimClip,L"RightInset",edge&&attached?L"p.r":L"0");
    for(auto* brush:{std::addressof(rimBrush),std::addressof(glowBrush)}){start(*brush,L"StartPoint",edge?L"Vector2(0,"+T+L")":attached?L"Vector2(0,p.r)":L"Vector2(0,0)");start(*brush,L"EndPoint",edge?L"Vector2(0,"+B+L")":L"Vector2(0,"+H+L")");}
    for(int k=1;k<3;++k)check(visual(parts[k].container)->put_IsVisible(attached));
    for(auto& r:shoulderRims)check(visual(r.visual)->put_IsVisible(attached));
    if(!attached)return;
    // Shoulders are built at the target radius in physical pixels and scaled about
    // the point where they meet the body and the screen while the radius springs.
    const float Rp=std::max(1.f,radius*float(s)),along=2*Rp,depth=Rp,over=4*float(s),strip=8*float(s),pad=16*float(s);
    const float side=float(shoulderSideControl),edgeControl=float(shoulderEdgeControl);
    const std::wstring k=L"(p.r/"+number(Rp)+L")";
    for(int which=0;which<2;++which){
        // Top-edge frame: x runs along the screen edge (0 = outer tip), y away from it.
        // The second shoulder mirrors the first; edge docking rotates the frame.
        auto point=[&](float x,float y,float shift){if(which)x=along-x;return edge?D2D1::Point2F(depth-y+shift,x+shift):D2D1::Point2F(x+shift,y+shift);};
        auto curveTo=[&](Figure& f,float shift){f.bezier(point(along,depth*side,shift),point(along*edgeControl,0,shift),point(0,0,shift));};
        Figure fill;fill.start=point(0,-over,0);fill.line(point(along,-over,0));fill.line(point(along,depth,0));curveTo(fill,0);
        // The rim's clip adds a strip inside the body, so the inner half of the curve's
        // stroke carries on into the body's own rim below the join.
        Figure rimArea;rimArea.start=point(0,-over,pad);rimArea.line(point(along+strip,-over,pad));rimArea.line(point(along+strip,depth,pad));rimArea.line(point(along,depth,pad));curveTo(rimArea,pad);
        Figure curve;curve.closed=false;curve.start=point(along,depth,pad);curveTo(curve,pad);
        const auto anchor=point(along,0,0);
        const std::wstring tx=edge?cw:(which?Rt:L),ty=edge?(which?B:T):L"0",ax=number(anchor.x),ay=number(anchor.y),ap=number(pad);
        auto& part=parts[which+1];part.clip=geometricClip(path(fill));check(visual(part.container)->put_Clip(as<wuc::ICompositionClip>(part.clip).Get()));
        start(part.clip,L"CenterPoint",L"Vector2("+ax+L","+ay+L")");start(part.clip,L"Scale",L"Vector2("+k+L","+k+L")");start(part.clip,L"Offset",L"Vector2("+tx+L"-"+ax+L","+ty+L"-"+ay+L")");
        auto& r=shoulderRims[which];const auto curveGeometry=path(curve);for(auto& sprite:r.shapes)check(as<SpriteShape>(sprite)->put_Geometry(curveGeometry.Get()));
        const float extent=along+2*pad+strip;check(visual(r.visual)->put_Size({extent,extent}));
        check(visual(r.visual)->put_Clip(as<wuc::ICompositionClip>(geometricClip(path(rimArea))).Get()));
        start(r.visual,L"CenterPoint",L"Vector3("+ax+L"+"+ap+L","+ay+L"+"+ap+L",0)");start(r.visual,L"Scale",L"Vector3("+k+L","+k+L",1)");
        start(r.visual,L"Offset",L"Vector3("+tx+L"-"+ax+L"-"+ap+L","+ty+L"-"+ay+L"-"+ap+L",0)");
        // Edge docking: the lower shoulder takes the rim's lower colours.
        const bool lower=edge&&which==1;check(as<SpriteShape>(r.shapes[0])->put_StrokeBrush(as<wuc::ICompositionBrush>(edgeBrushes[lower?3:2]).Get()));check(as<SpriteShape>(r.shapes[1])->put_StrokeBrush(as<wuc::ICompositionBrush>(edgeBrushes[lower?3:2]).Get()));check(as<SpriteShape>(r.shapes[2])->put_StrokeBrush(as<wuc::ICompositionBrush>(edgeBrushes[lower?1:0]).Get()));
    }
}
GlassBackdrop::~GlassBackdrop(){
    if(!impl_)return;
    if(impl_->target){ComPtr<ABI::Windows::Foundation::IClosable> closable;if(SUCCEEDED(impl_->target.As(&closable)))closable->Close();}
    delete impl_;
}
GlassStyle GlassBackdrop::current()const{return impl_?impl_->current:GlassStyle{};}
bool GlassBackdrop::vibrant()const{return impl_&&impl_->vibrantDark;}
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
        auto& c=impl->compositor;auto& i=*impl;check(c->CreateContainerVisual(&i.root));check(c->CreateContainerVisual(&i.glass));
        check(as<wuc::ICompositionTarget>(i.target)->put_Root(i.visual(i.root).Get()));
        ComPtr<wuc::IVisualCollection> rootChildren;check(i.root->get_Children(&rootChildren));check(rootChildren->InsertAtTop(i.visual(i.glass).Get()));
        check(i.visual(i.root)->put_Size({canvasWidth*scale,canvasHeight*scale}));
        check(c->CreatePropertySet(&i.properties));for(auto key:{L"t",L"w",L"h",L"r",L"dx",L"dy",L"s"}){String k(key);check(i.properties->InsertScalar(k,0.f));}
        check(as<Compositor3>(c)->CreateHostBackdropBrush(&i.hostBrush));
        // Vibrancy: the blurred desktop gains saturation, as Apple's materials do. A
        // failure here only costs the boost; the plain blur remains.
        try{i.vibrantDark=i.vibrancy(1.75f);i.vibrantLight=i.vibrancy(1.65f);}catch(...){i.vibrantDark.Reset();i.vibrantLight.Reset();}
        check(c->CreateColorBrushWithColor(color(0x0c0d11,.55f),&i.tintBrush));
        i.depthBrush=i.gradient(false);i.sheenBrush=i.gradient(false);i.rimBrush=i.gradient(true);i.glowBrush=i.gradient(true);
        for(auto& b:i.edgeBrushes)check(c->CreateColorBrushWithColor(color(0xffffff,0),&b));
        check(as<Compositor5>(c)->CreateRoundedRectangleGeometry(&i.geometry));
        ComPtr<wuc::IVisualCollection> glassChildren;check(i.glass->get_Children(&glassChildren));
        for(auto& part:i.parts){
            check(c->CreateContainerVisual(&part.container));check(glassChildren->InsertAtTop(i.visual(part.container).Get()));
            ComPtr<wuc::IVisualCollection> children;check(part.container->get_Children(&children));
            const ComPtr<IInspectable> brushes[4]={i.hostBrush,i.tintBrush,i.depthBrush,i.sheenBrush};
            for(int l=0;l<4;++l){check(c->CreateSpriteVisual(&part.layers[l]));check(part.layers[l]->put_Brush(as<wuc::ICompositionBrush>(brushes[l]).Get()));check(children->InsertAtTop(i.visual(part.layers[l]).Get()));}
        }
        i.parts[0].clip=i.geometricClip(i.geometry);check(i.visual(i.parts[0].container)->put_Clip(as<wuc::ICompositionClip>(i.parts[0].clip).Get()));
        // The rim: a crisp specular line and a two-step inner glow, the inner half of each stroke (the body clip trims the outer half).
        check(as<Compositor5>(c)->CreateShapeVisual(&i.rim));check(i.visual(i.rim)->put_Size({canvasWidth*scale,canvasHeight*scale}));
        {ComPtr<wuc::IVisualCollection> children;check(i.parts[0].container->get_Children(&children));check(children->InsertAtTop(i.visual(i.rim).Get()));}
        check(c->CreateInsetClip(&i.rimClip));check(i.visual(i.rim)->put_Clip(as<wuc::ICompositionClip>(i.rimClip).Get()));
        const float widths[3]={14,6,2};
        for(int k=0;k<3;++k){i.rimShapes[k]=i.shape(i.rim,widths[k]);check(as<SpriteShape>(i.rimShapes[k])->put_Geometry(i.geometry.Get()));check(as<SpriteShape>(i.rimShapes[k])->put_StrokeBrush(as<wuc::ICompositionBrush>(k==2?i.rimBrush:i.glowBrush).Get()));}
        for(auto& r:i.shoulderRims){check(as<Compositor5>(c)->CreateShapeVisual(&r.visual));check(glassChildren->InsertAtTop(i.visual(r.visual).Get()));for(int k=0;k<3;++k)r.shapes[k]=i.shape(r.visual,widths[k]);check(i.visual(r.visual)->put_IsVisible(false));}
        for(int k=1;k<3;++k)check(i.visual(i.parts[k].container)->put_IsVisible(false));
        check(i.visual(i.glass)->put_IsVisible(false));
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
        // Frosted: Windows' blurred backdrop, saturated for vibrancy, when Windows allows
        // one; otherwise a dense translucent frost. Clear: never blurred, a light tint
        // you see through. Neither ever falls back to an opaque fill.
        const bool clear=s.material==2,blurred=!clear&&s.blur;
        const auto& backdrop=s.light?i.vibrantLight:i.vibrantDark;
        for(auto& part:i.parts){check(i.visual(part.layers[0])->put_IsVisible(blurred));check(part.layers[0]->put_Brush(as<wuc::ICompositionBrush>(backdrop?backdrop:i.hostBrush).Get()));}
        const float tint=std::clamp(s.tint,0.f,1.f);
        float alpha=clear?(s.light?.22f:.30f)*(.55f+tint*1.1f):blurred?(s.light?.44f:.42f)*(.55f+tint*.9f):(s.light?.74f:.68f)*(.8f+tint*.35f);
        alpha=std::clamp(alpha,clear?.08f:.12f,clear?.62f:.92f);
        uint32_t ink=s.light?0xf6f7f9:0x0b0c10;
        if(s.accent&&!s.light){auto mix=[&](int shift){return uint32_t(((ink>>shift)&255)*.88+((s.accent>>shift)&255)*.12)<<shift;};ink=mix(16)|mix(8)|mix(0);}
        check(i.tintBrush->put_Color(color(ink,alpha)));
        // Depth: glass darkens slightly toward its lower edge, as thick glass does.
        const auto none=color(0x000000,0),shade=s.light?color(0x000000,clear?.07f:.035f):color(0x000000,clear?.22f:.12f);
        i.stops(i.depthBrush,{{0,none},{.55f,none},{1,shade}});
        const auto top=color(0xffffff,clear?(s.light?.55f:.16f):(s.light?.34f:.10f)),transparent=color(0xffffff,0);
        i.stops(i.sheenBrush,{{0,top},{clear?.38f:.46f,transparent},{1,transparent}});
        // The rim catches light at the top and fades underneath; stronger on clear glass, which has no blur to separate it.
        const auto rimTop=color(0xffffff,s.light?.9f:clear?.42f:.30f),rimBottom=s.light?color(0x000000,clear?.14f:.08f):color(0xffffff,clear?.10f:.07f);
        i.stops(i.rimBrush,{{0,rimTop},{.5f,s.light?color(0xffffff,clear?.35f:.25f):color(0xffffff,clear?.16f:.11f)},{1,rimBottom}});
        // A soft inner glow just inside the edge gives the pane thickness, like light caught in a lens.
        const auto glowTop=color(0xffffff,s.light?(clear?.30f:.22f):(clear?.055f:.035f)),glowBottom=color(0xffffff,s.light?(clear?.12f:.08f):(clear?.02f:.012f));
        i.stops(i.glowBrush,{{0,glowTop},{.35f,color(0xffffff,s.light?(clear?.16f:.11f):(clear?.03f:.018f))},{1,glowBottom}});
        check(i.edgeBrushes[0]->put_Color(rimTop));check(i.edgeBrushes[1]->put_Color(rimBottom));check(i.edgeBrushes[2]->put_Color(glowTop));check(i.edgeBrushes[3]->put_Color(glowBottom));
    }catch(...){}
}
void GlassBackdrop::animate(const MotionEngine& m,double now,int edge,bool attached){
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
        const float radius=float(m.radius.target());
        if(edge!=i.edge||attached!=i.attached||std::abs(radius-i.radius)>1e-3f){i.edge=edge;i.attached=attached;i.radius=radius;i.layout();}
    }catch(const std::exception& e){lastError_=e.what();}
}
}
