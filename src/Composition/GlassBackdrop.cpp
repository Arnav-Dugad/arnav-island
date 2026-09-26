#include "GlassBackdrop.h"
#include "WinCompAbi.h"
#include "Composition/DockGeometry.h"
#include <dwmapi.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <random>
#include <functional>
#include <d2d1effects.h>
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
// A Direct2D effect described to composition: its CLSID, properties by index and
// sources. Composition validates and runs the graph on the GPU.
struct Property {enum class Kind{Floats,UInt,Boolean} kind=Kind::Floats;std::vector<float> floats;UINT32 value=0;};
class Effect final:public effects::IGraphicsEffect,public effects::IGraphicsEffectSource,public GraphicsEffectD2D1Interop {
    std::atomic<ULONG> refs_{1};HSTRING name_=nullptr;GUID id_;std::vector<Property> properties_;std::vector<ComPtr<effects::IGraphicsEffectSource>> sources_;
public:
    Effect(GUID id,std::vector<Property> properties,std::vector<ComPtr<effects::IGraphicsEffectSource>> sources):id_(id),properties_(std::move(properties)),sources_(std::move(sources)){}
    ~Effect(){WindowsDeleteString(name_);}
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
    HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id)override{if(!id)return E_POINTER;*id=id_;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR,UINT*,INT32*)override{return E_INVALIDARG;}
    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count)override{if(!count)return E_POINTER;*count=UINT(properties_.size());return S_OK;}
    HRESULT STDMETHODCALLTYPE GetProperty(UINT index,ABI::Windows::Foundation::IPropertyValue** value)override{
        if(!value)return E_POINTER;*value=nullptr;if(index>=properties_.size())return E_INVALIDARG;auto& p=properties_[index];
        HSTRING_HEADER header;HSTRING cls=nullptr;static constexpr wchar_t name[]=L"Windows.Foundation.PropertyValue";
        HRESULT hr=WindowsCreateStringReference(name,UINT32(std::size(name)-1),&header,&cls);if(FAILED(hr))return hr;
        ComPtr<ABI::Windows::Foundation::IPropertyValueStatics> statics;hr=RoGetActivationFactory(cls,__uuidof(ABI::Windows::Foundation::IPropertyValueStatics),reinterpret_cast<void**>(statics.GetAddressOf()));if(FAILED(hr))return hr;
        ComPtr<IInspectable> boxed;hr=p.kind==Property::Kind::UInt?statics->CreateUInt32(p.value,&boxed):p.kind==Property::Kind::Boolean?statics->CreateBoolean(p.value!=0,&boxed):p.floats.size()==1?statics->CreateSingle(p.floats[0],&boxed):statics->CreateSingleArray(UINT32(p.floats.size()),p.floats.data(),&boxed);if(FAILED(hr))return hr;
        return boxed->QueryInterface(__uuidof(ABI::Windows::Foundation::IPropertyValue),reinterpret_cast<void**>(value));
    }
    HRESULT STDMETHODCALLTYPE GetSource(UINT index,effects::IGraphicsEffectSource** source)override{if(!source)return E_POINTER;*source=nullptr;if(index>=sources_.size())return E_INVALIDARG;return sources_[index].CopyTo(source);}
    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count)override{if(!count)return E_POINTER;*count=UINT(sources_.size());return S_OK;}
};
constexpr GUID colorMatrixEffect{0x921f03d6,0x641c,0x47df,{0x85,0x2d,0xb4,0xbb,0x61,0x53,0xae,0x11}};
// The material: saturation about Rec. 709 luminance, then a luminosity map (gain and
// a coloured offset) that keeps the backdrop's colour while setting how dark or milky
// the glass is. One 5x4 row-vector colour matrix.
std::array<float,20> material(float s,float gain,const float offset[3]){
    const float l[3]={.2126f,.7152f,.0722f};std::array<float,20> m{};
    for(int in=0;in<3;++in)for(int out=0;out<3;++out)m[in*4+out]=((1-s)*l[in]+(in==out?s:0.f))*gain;
    m[15]=1;for(int c=0;c<3;++c)m[16+c]=offset[c];return m;
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
    // Layers of every part: material (blurred backdrop), tint, grain, depth. The body adds four lens strips.
    // frost: the denser, milkier material that settles over the glass while the island rests (Frosted only).
    struct Part{ComPtr<wuc::IContainerVisual> container;std::array<ComPtr<wuc::ISpriteVisual>,4> layers;ComPtr<IInspectable> clip;ComPtr<wuc::ISpriteVisual> sheen,frost;};
    // The pointer's light (drawn once, a soft white disc), shared by the body and both shoulders.
    ComPtr<wuc::ICompositionSurfaceBrush> sheenBrush;bool sheenTried=false;
    // The pointer's glint on the body's rim: a radial gradient stroke centred under the pointer, in its own clipped layer.
    ComPtr<IInspectable> glint,glintBrush;ComPtr<wuc::IInsetClip> glintClip;
    // The edge light that follows the beat: a soft and a crisp stroke on the body's rim and both shoulders, their
    // colour's alpha following p.be. beatColor: the colour its expression was built for (1: none yet).
    ComPtr<wuc::ICompositionColorBrush> beatGlow,beatCore;uint32_t beatColor=1;bool beatable=true;
    // The frost: its level now runs from frostFrom to frostTo, after frostDelay, over frostSpan (from frostStart).
    ComPtr<IInspectable> frostBrush;double frostFrom=0,frostTo=0,frostStart=0,frostDelay=0,frostSpan=0;
    double frostAt(double now)const{if(frostSpan<=0)return frostTo;const double u=std::clamp((now-frostStart-frostDelay)/frostSpan,0.,1.);return frostFrom+(frostTo-frostFrom)*u*u*(3-2*u);}
    std::array<ComPtr<wuc::ISpriteVisual>,4> lens;std::array<ComPtr<IInspectable>,4> lensMasks;// edge light strips: left, right, top, bottom
    // Drawn surfaces are made on first use: grain when glass is first shown, the shadow when it first has any strength.
    bool shadowOnly=false,edgeLight=false,grainTried=false,shadowDrawn=false;std::string failure;ComPtr<IInspectable> grainBrush;
    // Shadow mode: one sprite whose nine-grid brush stretches a pre-blurred rounded rectangle around the body.
    ComPtr<wuc::ISpriteVisual> shadowSprite;float shadowMargin=0;
    ComPtr<ID3D11Device> d3d;ComPtr<ID2D1Device> d2d;ComPtr<wuc::ICompositionGraphicsDevice> graphics;
    // Parts: the body, two shoulders, and (Phase 5F) the stub left docked while a notification pill drops out of the body.
    std::array<Part,5> parts;ComPtr<IInspectable> geometry;// body rounded rectangle, shared by its clip and its rim
    ComPtr<IInspectable> stubGeometry,stubRim;ComPtr<wuc::IInsetClip> stubRimClip;std::array<ComPtr<IInspectable>,3> stubRimShapes;ComPtr<wuc::ISpriteVisual> shadowStub;
    // Phase 5G: the bud of a waiting alert below the pill (part 4), its rim and its shadow.
    ComPtr<IInspectable> budGeometry,budRim;std::array<ComPtr<IInspectable>,3> budRimShapes;ComPtr<wuc::ISpriteVisual> shadowBud;
    ComPtr<IInspectable> rim;ComPtr<wuc::IInsetClip> rimClip;std::array<ComPtr<IInspectable>,3> rimShapes;
    struct ShoulderRim{ComPtr<IInspectable> visual;std::array<ComPtr<IInspectable>,5> shapes;};std::array<ShoulderRim,2> shoulderRims;
    ComPtr<IInspectable> hostBrush,materialBrush;ComPtr<wuc::ICompositionColorBrush> tintBrush;bool vibrancy=false;
    ComPtr<IInspectable> depthBrush,rimBrush,glowBrush;std::array<ComPtr<wuc::ICompositionColorBrush>,4> edgeBrushes;// rim top, rim bottom, glow top, glow bottom
    float scale=1,canvasWidth=600,canvasHeight=500,radius=0;int edge=-1;bool attached=false;GlassStyle current;bool styled=false;
    // Whether this Windows accepts the lean's matrix expression (if not, the glass does not lean).
    bool leanable=true;
    ComPtr<wuc::IVisual> visual(const ComPtr<IInspectable>& v){return as<wuc::IVisual>(v);}
    ComPtr<wuc::IExpressionAnimation> expression(const std::wstring& text){ComPtr<wuc::IExpressionAnimation> a;String s(text.c_str());check(compositor->CreateExpressionAnimationWithExpression(s,&a));String p(L"p");check(as<wuc::ICompositionAnimation>(a)->SetReferenceParameter(p,as<wuc::ICompositionObject>(properties).Get()));return a;}
    // A failure names the property and the start of its expression.
    template<class T> void start(const ComPtr<T>& object,const wchar_t* property,const std::wstring& text){try{auto a=expression(text);String name(property);check(as<wuc::ICompositionObject>(object)->StartAnimation(name,as<wuc::ICompositionAnimation>(a).Get()));}
        catch(const std::exception& e){std::string what(e.what());what+=" starting ";for(const wchar_t* c=property;*c;++c)what+=char(*c);what+=" = ";for(size_t k=0;k<text.size()&&k<90;++k)what+=char(text[k]);throw std::runtime_error(what);}}
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
    ComPtr<glassabi::effects::IGraphicsEffectSource> backdropParameter(){
        String parameterClass(L"Windows.UI.Composition.CompositionEffectSourceParameter"),name(L"backdrop");
        ComPtr<EffectSourceParameterFactory> parameters;check(RoGetActivationFactory(parameterClass,__uuidof(EffectSourceParameterFactory),reinterpret_cast<void**>(parameters.GetAddressOf())));
        ComPtr<IInspectable> parameter;check(parameters->Create(name,&parameter));return as<glassabi::effects::IGraphicsEffectSource>(parameter);
    }
    ComPtr<IInspectable> effectBrush(const ComPtr<glassabi::effects::IGraphicsEffect>& effect){
        ComPtr<wuc::ICompositionEffectFactory> factory;check(compositor->CreateEffectFactory(effect.Get(),&factory));
        // A factory may still be compiling (Pending); its brushes render once it is ready.
        wuc::CompositionEffectFactoryLoadStatus status{};check(factory->get_LoadStatus(&status));if(status!=wuc::CompositionEffectFactoryLoadStatus_Success&&status!=wuc::CompositionEffectFactoryLoadStatus_Pending)throw std::runtime_error("effect status "+std::to_string(int(status)));
        ComPtr<wuc::ICompositionEffectBrush> brush;check(factory->CreateBrush(&brush));String name(L"backdrop");check(brush->SetSourceParameter(name,as<wuc::ICompositionBrush>(hostBrush).Get()));return brush;
    }
    static ComPtr<glassabi::effects::IGraphicsEffect> make(GUID id,std::vector<glassabi::Property> p,std::vector<ComPtr<glassabi::effects::IGraphicsEffectSource>> sources){ComPtr<glassabi::effects::IGraphicsEffect> e;e.Attach(new glassabi::Effect(id,std::move(p),std::move(sources)));return e;}
    ComPtr<glassabi::effects::IGraphicsEffect> materialEffect(float saturation,float gain,const float offset[3],const ComPtr<glassabi::effects::IGraphicsEffectSource>& source){
        using K=glassabi::Property::Kind;const auto m=glassabi::material(saturation,gain,offset);
        return make(glassabi::colorMatrixEffect,{{K::Floats,std::vector<float>(m.begin(),m.end())},{K::UInt,{},1},{K::Boolean,{},0}},{source});
    }
    ComPtr<IInspectable> materialOf(float saturation,float gain,const float offset[3]){return effectBrush(materialEffect(saturation,gain,offset,backdropParameter()));}
    // The resting frost: the material paler and milkier, then blurred a little more.
    ComPtr<IInspectable> frostOf(float saturation,float gain,const float offset[3]){
        using K=glassabi::Property::Kind;constexpr GUID gaussianBlur{0x1feb6d69,0x2fe6,0x4ac9,{0x8c,0x58,0x1d,0x7f,0x93,0xe7,0xa6,0xa5}};
        auto milk=as<glassabi::effects::IGraphicsEffectSource>(materialEffect(saturation,gain,offset,backdropParameter()));
        return effectBrush(make(gaussianBlur,{{K::Floats,{10*scale}},{K::UInt,{},1},{K::UInt,{},1}},{milk}));
    }
    // A GPU drawing surface drawn once with Direct2D, as a surface brush.
    ComPtr<wuc::ICompositionSurfaceBrush> drawn(UINT w,UINT h,const std::function<void(ID2D1DeviceContext*)>& draw){
        if(!graphics){
            UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            if(!d3d&&FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d,nullptr,nullptr)))check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&d3d,nullptr,nullptr));
            ComPtr<IDXGIDevice> dxgi;check(d3d.As(&dxgi));ComPtr<ID2D1Factory1> factory;check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),nullptr,reinterpret_cast<void**>(factory.GetAddressOf())));
            check(factory->CreateDevice(dxgi.Get(),&d2d));check(as<wuc::ICompositorInterop>(compositor)->CreateGraphicsDevice(d2d.Get(),&graphics));
        }
        ComPtr<wuc::ICompositionDrawingSurface> surface;check(graphics->CreateDrawingSurface({float(w),float(h)},ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,ABI::Windows::Graphics::DirectX::DirectXAlphaMode_Premultiplied,&surface));
        auto interop=as<DrawingSurfaceInterop>(surface);ComPtr<ID2D1DeviceContext> dc;POINT at{};check(interop->BeginDraw(nullptr,__uuidof(ID2D1DeviceContext),reinterpret_cast<void**>(dc.GetAddressOf()),&at));
        try{dc->SetTransform(D2D1::Matrix3x2F::Translation(float(at.x),float(at.y)));dc->Clear(D2D1::ColorF(0,0));draw(dc.Get());}catch(...){interop->EndDraw();throw;}
        check(interop->EndDraw());
        ComPtr<wuc::ICompositionSurfaceBrush> brush;check(compositor->CreateSurfaceBrushWithSurface(as<wuc::ICompositionSurface>(surface).Get(),&brush));return brush;
    }
    // Fine grain, the texture that separates frosted glass from a flat tint (and hides gradient banding).
    ComPtr<IInspectable> grain(){
        const UINT w=UINT(std::ceil(canvasWidth*scale)),h=UINT(std::ceil(canvasHeight*scale));
        auto brush=drawn(w,h,[&](ID2D1DeviceContext* dc){
            // Two-tone noise: light and dark specks at most 7% strong, premultiplied.
            std::vector<uint32_t> pixels(size_t(w)*h);std::mt19937 random(0x151a4du);std::uniform_real_distribution<float> u(0.f,1.f);
            for(auto& px:pixels){const float v=u(random)*2-1,a=std::abs(v)*std::abs(v)*.07f;const uint32_t alpha=uint32_t(std::lround(a*255)),level=v>0?alpha:0;px=(alpha<<24)|(level<<16)|(level<<8)|level;}
            ComPtr<ID2D1Bitmap> bitmap;check(dc->CreateBitmap({w,h},pixels.data(),w*4,D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED)),&bitmap));
            dc->DrawBitmap(bitmap.Get(),D2D1::RectF(0,0,float(w),float(h)));});
        check(brush->put_Stretch(wuc::CompositionStretch_None));check(brush->put_HorizontalAlignmentRatio(0));check(brush->put_VerticalAlignmentRatio(0));return brush;
    }
    // The soft shadow: a rounded rectangle blurred once (Direct2D Gaussian blur) into a surface; the
    // nine-grid keeps its corners and stretches its edges to any size.
    ComPtr<IInspectable> shadowBrush(float margin,float corner){
        const UINT size=UINT(std::ceil(2*(margin+corner)+4));
        auto surface=drawn(size,size,[&](ID2D1DeviceContext* dc){
            ComPtr<ID2D1Bitmap1> shape;check(dc->CreateBitmap({size,size},nullptr,0,D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED)),&shape));
            ComPtr<ID2D1Image> previous;dc->GetTarget(&previous);D2D1_MATRIX_3X2_F transform;dc->GetTransform(&transform);
            dc->SetTarget(shape.Get());dc->SetTransform(D2D1::Matrix3x2F::Identity());dc->Clear(D2D1::ColorF(0,0));
            ComPtr<ID2D1SolidColorBrush> black;check(dc->CreateSolidColorBrush(D2D1::ColorF(0,1),&black));
            dc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(margin,margin,float(size)-margin,float(size)-margin),corner,corner),black.Get());
            dc->SetTarget(previous.Get());dc->SetTransform(transform);
            constexpr GUID gaussianBlur{0x1feb6d69,0x2fe6,0x4ac9,{0x8c,0x58,0x1d,0x7f,0x93,0xe7,0xa6,0xa5}};
            ComPtr<ID2D1Effect> blur;check(dc->CreateEffect(gaussianBlur,&blur));blur->SetInput(0,shape.Get());check(blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION,margin/2.6f));
            dc->DrawImage(blur.Get());});
        ComPtr<IInspectable> nine;check(as<Compositor2>(compositor)->CreateNineGridBrush(&nine));auto grid=as<NineGridBrush>(nine);check(grid->put_Source(as<wuc::ICompositionBrush>(surface).Get()));
        const float inset=margin+corner+1;check(grid->put_LeftInset(inset));check(grid->put_TopInset(inset));check(grid->put_RightInset(inset));check(grid->put_BottomInset(inset));
        return nine;
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
    // Top dock: the body spans L..Rt and 0..H. Side docks: the body spans T..B, flush with the right edge (X0..cw) or the left (0..W).
    // Phase 5H: spread side by side with a waiting alert, the pill moves left by p.sp*p.ss (the stub stays).
    const std::wstring L=L"Round(("+cw+L"-p.w)/2-p.sp*p.ss)",Rt=L"Round(("+cw+L"+p.w)/2-p.sp*p.ss)",H=L"Round(p.h)",X0=L"Round("+cw+L"-p.w)",W=L"Round(p.w)",T=L"Round(("+ch+L"-p.h)/2)",B=L"Round(("+ch+L"+p.h)/2)";
    const std::wstring rc=L"Min(p.r,Min(p.w,p.h)/2)",E=L"("+rc+L"+"+px(2)+L")",M=L"(2*p.r+"+px(4)+L")";
    const bool side=edge!=0,left=edge==2;
    // The drop pill (top dock, attached): the body sits dp below the edge; its outline starts above the edge and
    // comes down twice as fast (top), so its corners round out as it leaves. Once it has left the edge (dp past E/2)
    // the shoulders move in to the stub's sides over the next 12 DIPs (SW, between the body's width and the stub's)
    // and take the stub's radius; until then they stay at the body's sides, where they belong.
    const bool drops=!side&&attached;const std::wstring dp=L"(p.d*"+px(dropDistance)+L")",top=drops?L"Min(2*"+dp+L"-"+E+L","+dp+L")":L"0",detach=L"("+dp+L"-"+E+L"/2)";
    // The bud (budShape in MotionEngine.h, in these units): it grows from the pill's foot, then lets go budGap below it.
    const std::wstring pillFoot=L"(Round(p.h)+"+dp+L")",budGrow=L"Clamp(p.b/0.55,0,1)",budPart=L"Clamp((p.b-0.55)/0.45,0,1.25)",budOver=L"Max(p.b-1,0)",budW0=L"(p.bw*(0.35+0.65*Clamp(p.b/0.8,0,1)))";
    // Phase 5H: spread, the bud glides to a card as tall as the pill beside it (budSpreadShape).
    // Its edges are properties of their own (bt0, bb0, bw0 below the pill; bl, bt, bv, bb, bc mixed), each from a
    // short expression: written out in full, the corner's expression grew too long to start, and the glass stopped.
    const std::wstring e=L"Clamp(p.sp,0,1)",sideL=L"(("+cw+L"+p.w)/2-p.sp*p.ss+"+px(budGap)+L")",sideT=dp,sideB=pillFoot,sideC=L"Min(p.r,Min(("+sideB+L"-"+sideT+L")/2,p.bw/2))";
    auto mixed=[&](const std::wstring& a,const std::wstring& z){return L"("+a+L"+("+z+L"-"+a+L")*"+e+L")";};
    if(drops){start(properties,L"bt0",L"("+pillFoot+L"-"+px(1)+L"+"+px(1+budGap)+L"*"+budPart+L")");
        start(properties,L"bb0",L"Max(p.bt0,"+pillFoot+L"-"+px(1)+L"+("+px(1+budGap)+L"+p.bh)*"+budGrow+L"+"+budOver+L"*"+px(14)+L")");start(properties,L"bw0",budW0);
        start(properties,L"bl",mixed(L"(("+cw+L"-p.bw0)/2-p.sp*p.ss)",sideL));start(properties,L"bt",mixed(L"p.bt0",sideT));start(properties,L"bv",mixed(L"p.bw0",L"p.bw"));
        start(properties,L"bb",mixed(L"p.bb0",sideB));start(properties,L"bc",mixed(L"Min(p.bh/2,Min((p.bb0-p.bt0)/2,p.bw0/2))",sideC));}
    const std::wstring budLeft=L"p.bl",budTop=L"p.bt",budW=L"p.bv",budBottom=L"p.bb",budCorner=L"p.bc";
    const std::wstring blend=L"Clamp("+detach+L"/"+px(12)+L",0,1)",SW=L"(p.w+(p.sw-p.w)*"+blend+L")",Ls=L"Round(("+cw+L"-"+SW+L")/2)",Rs=L"Round(("+cw+L"+"+SW+L")/2)";
    start(glass,L"Offset",edge==1?L"Vector3(Round(p.dx+p.s*"+px(74)+L"),Round(p.dy),0)":left?L"Vector3(Round(p.dx-p.s*"+px(74)+L"),Round(p.dy),0)":L"Vector3(Round(p.dx),Round(p.dy-p.s*"+px(44)+L"),0)");
    start(glass,L"Opacity",L"1-Clamp((p.s-0.45)/0.55,0,1)*Clamp((p.s-0.45)/0.55,0,1)*(3-2*Clamp((p.s-0.45)/0.55,0,1))");
    // The lean while dragged, as the DirectComposition layers do it: shear about the top edge, stretch about the top centre.
    if(!side&&leanable){const std::wstring t=L"Tan(p.dx*"+number(leanDegreesPerDip*3.14159265358979/180/s)+L")",sy=L"(1+p.dy*"+number(stretchPerDip/s)+L")",sx=L"(1-p.dy*"+number(narrowPerDip/s)+L")",cx=number(canvasWidth*s/2);
        try{start(glass,L"TransformMatrix",L"Matrix4x4("+sx+L",0,0,0,"+t+L"*"+sx+L","+sy+L",0,0,0,0,1,0,"+cx+L"*(1-"+sx+L"),0,0,1)");}catch(...){leanable=false;}}
    // The visible body box, and the geometry that also runs past the screen edge when attached (E), so the body meets the screen square.
    std::wstring x0,x1,y0,y1,offset,size,layerOffset,layerSize;
    if(!side){x0=L;x1=Rt;y0=drops?dp:L"0";y1=H+(drops?L"+"+dp:L"");
        offset=L"Vector2("+L+L","+top+L")";size=L"Vector2("+Rt+L"-"+L+L","+y1+L"-"+top+L")";
        layerOffset=L"Vector3("+L+(attached?L"-"+M:L"")+L",0,0)";layerSize=L"Vector2("+Rt+L"-"+L+(attached?L"+2*"+M:L"")+L","+y1+L")";}
    else if(!left){x0=X0;x1=cw;y0=T;y1=B;
        offset=L"Vector2("+X0+L","+T+L")";size=L"Vector2("+cw+L"-"+X0+(attached?L"+"+E:L"")+L","+B+L"-"+T+L")";
        layerOffset=L"Vector3("+X0+L","+T+(attached?L"-"+M:L"")+L",0)";layerSize=L"Vector2("+cw+L"-"+X0+L","+B+L"-"+T+(attached?L"+2*"+M:L"")+L")";}
    else{x0=L"0";x1=W;y0=T;y1=B;
        offset=L"Vector2("+(attached?L"-"+E:L"0")+L","+T+L")";size=L"Vector2("+W+(attached?L"+"+E:L"")+L","+B+L"-"+T+L")";
        layerOffset=L"Vector3(0,"+T+(attached?L"-"+M:L"")+L",0)";layerSize=L"Vector2("+W+L","+B+L"-"+T+(attached?L"+2*"+M:L"")+L")";}
    if(shadowOnly){
        // The shadow hangs a little below the body and spreads beyond it by the blur's margin. As the
        // island grows it seems to rise off the desktop: the shadow drops further, spreads and deepens.
        const std::wstring lift=L"Clamp((p.h-"+px(34)+L")/"+px(300)+L",0,1)",m=L"("+number(shadowMargin)+L"+"+lift+L"*"+px(5)+L")",drop=L"("+px(7)+L"+"+lift+L"*"+px(8)+L")";
        start(shadowSprite,L"Offset",L"Vector3("+x0+L"-"+m+L","+(side?y0:attached?top:y0)+L"-"+m+L"+"+drop+L",0)");
        start(shadowSprite,L"Size",L"Vector2("+x1+L"-"+x0+L"+2*"+m+L","+y1+L"-"+(side?y0:attached?top:y0)+L"+2*"+m+L")");
        start(shadowSprite,L"Opacity",L"p.so*(0.72+0.28*"+lift+L")");
        // The stub's own (compact) shadow while a pill has dropped out.
        if(shadowStub){const std::wstring Es=L"(p.sr+"+px(2)+L")",sm=number(shadowMargin);
            start(shadowStub,L"Offset",L"Vector3("+Ls+L"-"+sm+L",-"+Es+L"-"+sm+L"+"+px(7)+L",0)");start(shadowStub,L"Size",L"Vector2("+Rs+L"-"+Ls+L"+2*"+sm+L",Min(p.sh,Max("+top+L",0)+"+px(1)+L")+"+Es+L"+2*"+sm+L")");
            start(shadowStub,L"Opacity",drops?L"p.so*0.72*Clamp("+detach+L"/"+px(10)+L",0,1)":L"0");}
        if(shadowBud){const std::wstring sm=number(shadowMargin);
            start(shadowBud,L"Offset",L"Vector3("+budLeft+L"-"+sm+L","+budTop+L"-"+sm+L"+"+px(6)+L",0)");start(shadowBud,L"Size",L"Vector2("+budW+L"+2*"+sm+L","+budBottom+L"-"+budTop+L"+2*"+sm+L")");
            start(shadowBud,L"Opacity",drops?L"p.so*0.6*Clamp(("+budBottom+L"-"+budTop+L")/"+px(20)+L",0,1)":L"0");}
        return;
    }
    start(geometry,L"Offset",offset);start(geometry,L"Size",size);start(geometry,L"CornerRadius",L"Vector2("+rc+L","+rc+L")");
    for(auto& part:parts)if(part.sheen){start(part.sheen,L"Offset",L"Vector3("+x0+L"+p.px,"+y0+L"+p.py,0)");start(part.sheen,L"Opacity",L"Clamp(p.po,0,1)");}
    for(size_t k=0;k<parts.size();++k){auto& part=parts[k];
        // The shoulders' layers follow SW (the body's frame until a pill drops); the stub's cover the stub.
        std::wstring o=layerOffset,z=layerSize;
        if(drops&&k>0&&k<3){o=L"Vector3("+Ls+L"-"+M+L",0,0)";z=L"Vector2("+Rs+L"-"+Ls+L"+2*"+M+L","+H+L")";}
        else if(k==3){o=L"Vector3("+Ls+L",0,0)";z=L"Vector2("+Rs+L"-"+Ls+L",p.sh)";}
        else if(k==4){o=L"Vector3(0,0,0)";z=L"Vector2("+cw+L","+ch+L")";}
        for(size_t l=0;l<part.layers.size();++l){if(!part.layers[l])continue;start(part.layers[l],L"Offset",o);
            // Grain covers the whole canvas and moves with the pane.
            start(part.layers[l],L"Size",l==2?L"Vector2("+cw+L","+ch+L")":z);}
        if(part.frost){start(part.frost,L"Offset",o);start(part.frost,L"Size",z);start(part.frost,L"Opacity",L"Clamp(p.fr,0,1)*0.9");}}
    {
        // Edge light runs along the free edges (not the screen edge the island is attached to).
        const std::wstring band=px(14);const bool free[4]={!(attached&&left),!(attached&&edge==1),!(attached&&!side),true};
        const std::wstring seam=drops?L"(p.r*(1-Clamp(p.d*8,0,1)))":L"0",sideTop=L"("+y0+L"+"+seam+L")";
        const std::wstring offsets[4]={L"Vector3("+x0+L","+sideTop+L",0)",L"Vector3("+x1+L"-"+band+L","+sideTop+L",0)",L"Vector3("+x0+L","+y0+L",0)",L"Vector3("+x0+L","+y1+L"-"+band+L",0)"};
        const std::wstring sizes[4]={L"Vector2("+band+L",Max("+y1+L"-"+sideTop+L",0))",L"Vector2("+band+L",Max("+y1+L"-"+sideTop+L",0))",L"Vector2("+x1+L"-"+x0+L","+band+L")",L"Vector2("+x1+L"-"+x0+L","+band+L")"};
        for(int k=0;k<4;++k)if(lens[k]){start(lens[k],L"Offset",offsets[k]);start(lens[k],L"Size",sizes[k]);check(visual(lens[k])->put_IsVisible(free[k]&&current.blur&&current.material!=2&&vibrancy&&edgeLight));}
        // The body's rim stops where a shoulder takes over (the shoulder is exactly r deep).
        for(auto* clip:{rimClip.Get(),glintClip.Get()})if(clip){ComPtr<wuc::IInsetClip> c(clip);start(c,L"TopInset",drops?L"(p.r*(1-Clamp(p.d*8,0,1)))":L"0");start(c,L"RightInset",edge==1&&attached?L"p.r":L"0");start(c,L"LeftInset",left&&attached?L"p.r":L"0");}
        // The glint sits under the pointer (the sheen's centre); it shows while the pointer's light does, a little stronger.
        if(glint){start(glintBrush,L"EllipseCenter",L"Vector2("+x0+L"+p.px+"+px(130)+L","+y0+L"+p.py+"+px(130)+L")");start(glint,L"Opacity",L"Clamp(p.po*1.8,0,1)");}
        // The rim's light tilts while the island moves: a drag or a change of size swings it, then it settles back.
        const std::wstring centre=L"Vector2(("+x0+L"+"+x1+L")/2,("+y0+L"+"+y1+L")/2)",tilt=L"Clamp(p.dx*0.35+(p.w-p.tw)*0.06+(p.h-p.th)*0.05,-26,26)";
        for(auto* brush:{std::addressof(rimBrush),std::addressof(glowBrush)}){start(*brush,L"StartPoint",side?L"Vector2(0,"+T+L")":attached?L"Vector2(0,p.r+"+dp+L")":L"Vector2(0,0)");start(*brush,L"EndPoint",side?L"Vector2(0,"+B+L")":L"Vector2(0,"+y1+L")");
            start(*brush,L"CenterPoint",centre);start(*brush,L"RotationAngleInDegrees",tilt);}
        for(int k=1;k<3;++k)check(visual(parts[k].container)->put_IsVisible(attached));
        for(auto& r:shoulderRims)check(visual(r.visual)->put_IsVisible(attached));
        // The stub spans what the shoulders span (SW), from the edge down to the pill's top (1 DIP into it, so no seam, and never
        // further, so see-through glass never shows the two overlapping), while a pill is out.
        check(visual(parts[3].container)->put_IsVisible(drops));
        if(drops){const std::wstring Es=L"(p.sr+"+px(2)+L")",tall=L"(Min(p.sh,Max("+top+L",0)+"+px(1)+L")+"+Es+L")",corner=L"Min(p.sr,"+tall+L"/2)";
            start(stubGeometry,L"Offset",L"Vector2("+Ls+L",-"+Es+L")");start(stubGeometry,L"Size",L"Vector2("+Rs+L"-"+Ls+L","+tall+L")");start(stubGeometry,L"CornerRadius",L"Vector2("+corner+L","+corner+L")");
            start(parts[3].container,L"Opacity",L"Clamp(("+detach+L"+"+px(1)+L")/"+px(1)+L",0,1)");start(stubRimClip,L"TopInset",L"p.sr");}
        check(visual(parts[4].container)->put_IsVisible(drops));if(drops)start(parts[4].container,L"Opacity",L"Clamp(p.b*8,0,1)");
        if(drops){start(budGeometry,L"Offset",L"Vector2("+budLeft+L","+budTop+L")");start(budGeometry,L"Size",L"Vector2("+budW+L","+budBottom+L"-"+budTop+L")");start(budGeometry,L"CornerRadius",L"Vector2("+budCorner+L","+budCorner+L")");}
        if(!attached)return;
    }
    // Shoulders are built at the target radius in physical pixels and scaled about
    // the point where they meet the body and the screen while the radius springs.
    const float Rp=std::max(1.f,radius*float(s)),along=2*Rp,depth=Rp,over=4*float(s),strip=8*float(s),pad=16*float(s);
    const float curveSide=float(shoulderSideControl),edgeControl=float(shoulderEdgeControl);
    const std::wstring k=L"((p.r+(p.sr-p.r)*"+blend+L")/"+number(Rp)+L")";
    for(int which=0;which<2;++which){
        // Top-edge frame: x runs along the screen edge (0 = outer tip), y away from it.
        // The second shoulder mirrors the first; side docking rotates the frame (right) or reflects it (left).
        auto point=[&](float x,float y,float shift){if(which)x=along-x;return edge==1?D2D1::Point2F(depth-y+shift,x+shift):left?D2D1::Point2F(y+shift,x+shift):D2D1::Point2F(x+shift,y+shift);};
        auto curveTo=[&](Figure& f,float shift){f.bezier(point(along,depth*curveSide,shift),point(along*edgeControl,0,shift),point(0,0,shift));};
        Figure fill;fill.start=point(0,-over,0);fill.line(point(along,-over,0));fill.line(point(along,depth,0));curveTo(fill,0);
        const auto anchor=point(along,0,0);
        const std::wstring tx=edge==1?cw:left?L"0":(which?Rs:Ls),ty=side?(which?B:T):L"0",ax=number(anchor.x),ay=number(anchor.y),ap=number(pad);
        auto& part=parts[which+1];part.clip=geometricClip(path(fill));check(visual(part.container)->put_Clip(as<wuc::ICompositionClip>(part.clip).Get()));
        start(part.clip,L"CenterPoint",L"Vector2("+ax+L","+ay+L")");start(part.clip,L"Scale",L"Vector2("+k+L","+k+L")");start(part.clip,L"Offset",L"Vector2("+tx+L"-"+ax+L","+ty+L"-"+ay+L")");
        // The rim's clip adds a strip inside the body, so the inner half of the curve's
        // stroke carries on into the body's own rim below the join.
        Figure rimArea;rimArea.start=point(0,-over,pad);rimArea.line(point(along+strip,-over,pad));rimArea.line(point(along+strip,depth,pad));rimArea.line(point(along,depth,pad));curveTo(rimArea,pad);
        Figure curve;curve.closed=false;curve.start=point(along,depth,pad);curveTo(curve,pad);
        auto& r=shoulderRims[which];const auto curveGeometry=path(curve);for(auto& sprite:r.shapes)if(sprite)check(as<SpriteShape>(sprite)->put_Geometry(curveGeometry.Get()));
        const float extent=along+2*pad+strip;check(visual(r.visual)->put_Size({extent,extent}));
        check(visual(r.visual)->put_Clip(as<wuc::ICompositionClip>(geometricClip(path(rimArea))).Get()));
        start(r.visual,L"CenterPoint",L"Vector3("+ax+L"+"+ap+L","+ay+L"+"+ap+L",0)");start(r.visual,L"Scale",L"Vector3("+k+L","+k+L",1)");
        start(r.visual,L"Offset",L"Vector3("+tx+L"-"+ax+L"-"+ap+L","+ty+L"-"+ay+L"-"+ap+L",0)");
        // Side docking: the lower shoulder takes the rim's lower colours.
        const bool lower=side&&which==1;check(as<SpriteShape>(r.shapes[0])->put_StrokeBrush(as<wuc::ICompositionBrush>(edgeBrushes[lower?3:2]).Get()));check(as<SpriteShape>(r.shapes[1])->put_StrokeBrush(as<wuc::ICompositionBrush>(edgeBrushes[lower?3:2]).Get()));check(as<SpriteShape>(r.shapes[2])->put_StrokeBrush(as<wuc::ICompositionBrush>(edgeBrushes[lower?1:0]).Get()));
    }
}
void GlassBackdrop::sheen(const Spring& x,const Spring& y,const Spring& opacity,double now,float strength){
    if(!available_||impl_->shadowOnly)return;auto& i=*impl_;
    try{
        // Its own clock (tp), so the island's springs keep theirs.
        String tp(L"tp");ComPtr<wuc::IScalarKeyFrameAnimation> clock;check(i.compositor->CreateScalarKeyFrameAnimation(&clock));
        ComPtr<wuc::ICompositionEasingFunction> linear;{ComPtr<wuc::ILinearEasingFunction> l;check(i.compositor->CreateLinearEasingFunction(&l));check(l.As(&linear));}
        const float span=6;check(clock->InsertKeyFrame(0,0));check(clock->InsertKeyFrameWithEasingFunction(1,span,linear.Get()));ABI::Windows::Foundation::TimeSpan duration{LONGLONG(span*1e7)};check(as<KeyFrameAnimation>(clock)->put_Duration(duration));
        ComPtr<IInspectable> props;check(i.properties.As(&props));const double s=i.scale;
        i.start(props,L"px",springExpression(SpringTerms::from(x,now),s,L"p.tp"));i.start(props,L"py",springExpression(SpringTerms::from(y,now),s,L"p.tp"));i.start(props,L"po",springExpression(SpringTerms::from(opacity,now),strength,L"p.tp"));
        check(as<wuc::ICompositionObject>(i.properties)->StartAnimation(tp,as<wuc::ICompositionAnimation>(clock).Get()));
    }catch(const std::exception& e){lastError_=std::string("sheen: ")+e.what();}
}
void GlassBackdrop::beat(double from,double velocity,double to,double span,uint32_t rgb){
    if(!available_||impl_->shadowOnly||!impl_->beatable)return;auto& i=*impl_;
    try{
        ComPtr<IInspectable> props;check(i.properties.As(&props));
        // The colour: its alpha follows the level (a soft band at 45%, the crisp line at 85%).
        if(rgb!=i.beatColor){i.beatColor=rgb;const std::wstring r=number((rgb>>16)&255),g=number((rgb>>8)&255),b=number(rgb&255);
            i.start(i.beatGlow,L"Color",L"ColorRGB(Clamp(p.be,0,1)*"+number(255*.45)+L","+r+L","+g+L","+b+L")");i.start(i.beatCore,L"Color",L"ColorRGB(Clamp(p.be,0,1)*"+number(255*.85)+L","+r+L","+g+L","+b+L")");}
        // The level: the glide's cubic on its own clock (tb), held at `to` once it has run.
        span=std::max(span,1e-3);const double a=(3*(to-from)-2*velocity*span)/(span*span),c=(2*(from-to)+velocity*span)/(span*span*span);const std::wstring S=L"Min(p.tb,"+number(span)+L")";
        i.start(props,L"be",number(from)+L"+"+S+L"*("+number(velocity)+L"+"+S+L"*("+number(a)+L"+"+S+L"*"+number(c)+L"))");
        String tb(L"tb");ComPtr<wuc::IScalarKeyFrameAnimation> clock;check(i.compositor->CreateScalarKeyFrameAnimation(&clock));
        ComPtr<wuc::ICompositionEasingFunction> linear;{ComPtr<wuc::ILinearEasingFunction> l;check(i.compositor->CreateLinearEasingFunction(&l));check(l.As(&linear));}
        const float length=2;check(clock->InsertKeyFrame(0,0));check(clock->InsertKeyFrameWithEasingFunction(1,length,linear.Get()));ABI::Windows::Foundation::TimeSpan duration{LONGLONG(length*1e7)};check(as<KeyFrameAnimation>(clock)->put_Duration(duration));
        check(as<wuc::ICompositionObject>(i.properties)->StartAnimation(tb,as<wuc::ICompositionAnimation>(clock).Get()));
    }catch(const std::exception& e){i.beatable=false;i.failure=std::string("beat light: ")+e.what();try{check(i.beatGlow->put_Color(color(0xffffff,0)));check(i.beatCore->put_Color(color(0xffffff,0)));}catch(...){}}
}
void GlassBackdrop::frost(bool resting,double now,double pace){
    if(!available_||impl_->shadowOnly)return;auto& i=*impl_;
    try{
        const double from=i.frostAt(now),to=resting?1:0,delay=resting?6*pace:0,span=resting?24*pace:.35;
        i.frostFrom=from;i.frostTo=to;i.frostStart=now;i.frostDelay=delay;i.frostSpan=span;
        // Smoothstep from the level now to the target, after the delay, on its own clock (tf).
        ComPtr<IInspectable> props;check(i.properties.As(&props));const std::wstring u=L"Clamp((p.tf-"+number(delay)+L")/"+number(span)+L",0,1)";
        i.start(props,L"fr",number(from)+L"+"+number(to-from)+L"*"+u+L"*"+u+L"*(3-2*"+u+L")");
        String tf(L"tf");ComPtr<wuc::IScalarKeyFrameAnimation> clock;check(i.compositor->CreateScalarKeyFrameAnimation(&clock));
        ComPtr<wuc::ICompositionEasingFunction> linear;{ComPtr<wuc::ILinearEasingFunction> l;check(i.compositor->CreateLinearEasingFunction(&l));check(l.As(&linear));}
        const float length=float(delay+span+1);check(clock->InsertKeyFrame(0,0));check(clock->InsertKeyFrameWithEasingFunction(1,length,linear.Get()));ABI::Windows::Foundation::TimeSpan duration{LONGLONG(double(length)*1e7)};check(as<KeyFrameAnimation>(clock)->put_Duration(duration));
        check(as<wuc::ICompositionObject>(i.properties)->StartAnimation(tf,as<wuc::ICompositionAnimation>(clock).Get()));
    }catch(const std::exception& e){lastError_=std::string("frost: ")+e.what();}
}
GlassBackdrop::~GlassBackdrop(){
    if(!impl_)return;
    if(impl_->target){ComPtr<ABI::Windows::Foundation::IClosable> closable;if(SUCCEEDED(impl_->target.As(&closable)))closable->Close();}
    delete impl_;
}
GlassStyle GlassBackdrop::current()const{return impl_?impl_->current:GlassStyle{};}
std::string GlassBackdrop::failure()const{return impl_?impl_->failure:std::string{};}
bool GlassBackdrop::vibrant()const{return impl_&&impl_->vibrancy;}
bool GlassBackdrop::leans()const{return !impl_||impl_->leanable;}
bool GlassBackdrop::effectsEnabled(){
    DWORD value=1,size=sizeof(value);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"EnableTransparency",RRF_RT_REG_DWORD,nullptr,&value,&size)!=ERROR_SUCCESS)value=1;
    SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);HIGHCONTRASTW contrast{sizeof(contrast)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);
    return value!=0&&!power.SystemStatusFlag&&!(contrast.dwFlags&HCF_HIGHCONTRASTON);
}
bool GlassBackdrop::initialize(HWND window,float scale,float canvasWidth,float canvasHeight,bool shadowOnly,ID3D11Device* device){
    try{
        auto impl=std::make_unique<Impl>();impl->scale=scale;impl->canvasWidth=canvasWidth;impl->canvasHeight=canvasHeight;impl->queue=dispatcherController();impl->shadowOnly=shadowOnly;impl->d3d=device;
        String name(L"Windows.UI.Composition.Compositor");ComPtr<IInspectable> instance;check(RoActivateInstance(name,&instance));check(instance.As(&impl->compositor));
        if(!shadowOnly){BOOL host=TRUE;check(DwmSetWindowAttribute(window,DWMWA_USE_HOSTBACKDROPBRUSH,&host,sizeof(host)));}
        check(as<CompositorDesktopInterop>(impl->compositor)->CreateDesktopWindowTarget(window,FALSE,&impl->target));
        auto& c=impl->compositor;auto& i=*impl;check(c->CreateContainerVisual(&i.root));
        // The shadow window draws one silhouette of the whole shape in a layer that casts a soft drop shadow.
        check(c->CreateContainerVisual(&i.glass));
        check(as<wuc::ICompositionTarget>(i.target)->put_Root(i.visual(i.root).Get()));
        ComPtr<wuc::IVisualCollection> rootChildren;check(i.root->get_Children(&rootChildren));check(rootChildren->InsertAtTop(i.visual(i.glass).Get()));
        check(i.visual(i.root)->put_Size({canvasWidth*scale,canvasHeight*scale}));check(i.visual(i.glass)->put_Size({canvasWidth*scale,canvasHeight*scale}));
        check(c->CreatePropertySet(&i.properties));for(auto key:glassProperties){String k(key);check(i.properties->InsertScalar(k,0.f));}
        check(c->CreateColorBrushWithColor(color(0x0c0d11,.55f),&i.tintBrush));
        check(as<Compositor5>(c)->CreateRoundedRectangleGeometry(&i.geometry));
        ComPtr<wuc::IVisualCollection> glassChildren;check(i.glass->get_Children(&glassChildren));
        if(shadowOnly){
            i.shadowMargin=28*scale;check(c->CreateSpriteVisual(&i.shadowSprite));
            check(glassChildren->InsertAtTop(i.visual(i.shadowSprite).Get()));
            try{check(c->CreateSpriteVisual(&i.shadowStub));check(glassChildren->InsertAtBottom(i.visual(i.shadowStub).Get()));check(i.visual(i.shadowStub)->put_Opacity(0));}catch(...){i.shadowStub.Reset();}
            try{check(c->CreateSpriteVisual(&i.shadowBud));check(glassChildren->InsertAtBottom(i.visual(i.shadowBud).Get()));check(i.visual(i.shadowBud)->put_Opacity(0));}catch(...){i.shadowBud.Reset();}check(i.visual(i.glass)->put_IsVisible(false));impl_=impl.release();available_=true;return true;
        }
        {
            check(as<Compositor3>(c)->CreateHostBackdropBrush(&i.hostBrush));
            i.depthBrush=i.gradient(false);i.rimBrush=i.gradient(true);i.glowBrush=i.gradient(true);
            for(auto& b:i.edgeBrushes)check(c->CreateColorBrushWithColor(color(0xffffff,0),&b));
        }
        for(size_t k=0;k<i.parts.size();++k){auto& part=i.parts[k];
            // The stub sits beneath the body, so where they overlap early in a drop the body's rim stays on top.
            check(c->CreateContainerVisual(&part.container));if(k>=3)check(glassChildren->InsertAtBottom(i.visual(part.container).Get()));else check(glassChildren->InsertAtTop(i.visual(part.container).Get()));
            ComPtr<wuc::IVisualCollection> children;check(part.container->get_Children(&children));
            auto add=[&](ComPtr<wuc::ISpriteVisual>& sprite,const ComPtr<IInspectable>& brush){check(c->CreateSpriteVisual(&sprite));if(brush)check(sprite->put_Brush(as<wuc::ICompositionBrush>(brush).Get()));check(children->InsertAtTop(i.visual(sprite).Get()));};
            add(part.layers[0],i.hostBrush);add(part.frost,nullptr);check(i.visual(part.frost)->put_IsVisible(false));
            if(k==0)for(int l=0;l<4;++l){add(i.lens[l],nullptr);check(i.visual(i.lens[l])->put_IsVisible(false));}
            add(part.layers[1],i.tintBrush);add(part.layers[2],nullptr);add(part.layers[3],i.depthBrush);
            if(k<3){add(part.sheen,nullptr);check(i.visual(part.sheen)->put_Size({260*scale,260*scale}));check(i.visual(part.sheen)->put_Opacity(0));}
        }
        i.parts[0].clip=i.geometricClip(i.geometry);check(i.visual(i.parts[0].container)->put_Clip(as<wuc::ICompositionClip>(i.parts[0].clip).Get()));
        {
            // The rim: a crisp specular line and a soft two-step inner glow, the inner half of each stroke (the body clip trims the outer half).
            check(as<Compositor5>(c)->CreateShapeVisual(&i.rim));check(i.visual(i.rim)->put_Size({canvasWidth*scale,canvasHeight*scale}));
            {ComPtr<wuc::IVisualCollection> children;check(i.parts[0].container->get_Children(&children));check(children->InsertAtTop(i.visual(i.rim).Get()));}
            check(c->CreateInsetClip(&i.rimClip));check(i.visual(i.rim)->put_Clip(as<wuc::ICompositionClip>(i.rimClip).Get()));
            const float widths[3]={8,3.5f,1.6f};
            for(int k=0;k<3;++k){i.rimShapes[k]=i.shape(i.rim,widths[k]);check(as<SpriteShape>(i.rimShapes[k])->put_Geometry(i.geometry.Get()));check(as<SpriteShape>(i.rimShapes[k])->put_StrokeBrush(as<wuc::ICompositionBrush>(k==2?i.rimBrush:i.glowBrush).Get()));}
            // The beat's light rides on the rim (a soft band and a crisp line), transparent until music plays.
            check(c->CreateColorBrushWithColor(color(0xffffff,0),&i.beatGlow));check(c->CreateColorBrushWithColor(color(0xffffff,0),&i.beatCore));
            for(int k=0;k<2;++k){auto s=i.shape(i.rim,k?2.f:9.f);check(as<SpriteShape>(s)->put_Geometry(i.geometry.Get()));check(as<SpriteShape>(s)->put_StrokeBrush(as<wuc::ICompositionBrush>(k?i.beatCore:i.beatGlow).Get()));}
            // The pointer's glint: brightest on the rim right under the pointer, fading along it both ways.
            try{check(as<Compositor5>(c)->CreateShapeVisual(&i.glint));check(i.visual(i.glint)->put_Size({canvasWidth*scale,canvasHeight*scale}));check(i.visual(i.glint)->put_Opacity(0));
                {ComPtr<wuc::IVisualCollection> children;check(i.parts[0].container->get_Children(&children));check(children->InsertAtTop(i.visual(i.glint).Get()));}
                check(c->CreateInsetClip(&i.glintClip));check(i.visual(i.glint)->put_Clip(as<wuc::ICompositionClip>(i.glintClip).Get()));
                check(as<CompositorWithRadialGradient>(c)->CreateRadialGradientBrush(&i.glintBrush));check(as<GradientBrush2>(i.glintBrush)->put_MappingMode(0));
                check(as<RadialGradientBrush>(i.glintBrush)->put_EllipseRadius({78*scale,78*scale}));
                {ComPtr<IInspectable> collection;check(as<GradientBrush>(i.glintBrush)->get_ColorStops(&collection));auto list=vector(collection,stopVector);for(int k=0;k<3;++k)check(list->Append(i.stop(k*.5f,color(0xffffff,0)).Get()));}
                for(int k=0;k<2;++k){auto s=i.shape(i.glint,k?1.8f:6.f);check(as<SpriteShape>(s)->put_Geometry(i.geometry.Get()));check(as<SpriteShape>(s)->put_StrokeBrush(as<wuc::ICompositionBrush>(i.glintBrush).Get()));}
            }catch(...){if(i.glint)(void)i.visual(i.glint)->put_IsVisible(false);i.glint.Reset();i.glintBrush.Reset();i.glintClip.Reset();}
            // The stub's rim, like the body's: its top is left to the shoulders (they are the stub's radius deep).
            check(as<Compositor5>(c)->CreateRoundedRectangleGeometry(&i.stubGeometry));i.parts[3].clip=i.geometricClip(i.stubGeometry);check(i.visual(i.parts[3].container)->put_Clip(as<wuc::ICompositionClip>(i.parts[3].clip).Get()));
            check(as<Compositor5>(c)->CreateShapeVisual(&i.stubRim));check(i.visual(i.stubRim)->put_Size({canvasWidth*scale,canvasHeight*scale}));
            {ComPtr<wuc::IVisualCollection> children;check(i.parts[3].container->get_Children(&children));check(children->InsertAtTop(i.visual(i.stubRim).Get()));}
            check(c->CreateInsetClip(&i.stubRimClip));check(i.visual(i.stubRim)->put_Clip(as<wuc::ICompositionClip>(i.stubRimClip).Get()));
            for(int k=0;k<3;++k){i.stubRimShapes[k]=i.shape(i.stubRim,widths[k]);check(as<SpriteShape>(i.stubRimShapes[k])->put_Geometry(i.stubGeometry.Get()));check(as<SpriteShape>(i.stubRimShapes[k])->put_StrokeBrush(as<wuc::ICompositionBrush>(k==2?i.rimBrush:i.glowBrush).Get()));}
            // The bud's rim, all the way round.
            check(as<Compositor5>(c)->CreateRoundedRectangleGeometry(&i.budGeometry));i.parts[4].clip=i.geometricClip(i.budGeometry);check(i.visual(i.parts[4].container)->put_Clip(as<wuc::ICompositionClip>(i.parts[4].clip).Get()));
            check(as<Compositor5>(c)->CreateShapeVisual(&i.budRim));check(i.visual(i.budRim)->put_Size({canvasWidth*scale,canvasHeight*scale}));
            {ComPtr<wuc::IVisualCollection> children;check(i.parts[4].container->get_Children(&children));check(children->InsertAtTop(i.visual(i.budRim).Get()));}
            for(int k=0;k<3;++k){i.budRimShapes[k]=i.shape(i.budRim,widths[k]);check(as<SpriteShape>(i.budRimShapes[k])->put_Geometry(i.budGeometry.Get()));check(as<SpriteShape>(i.budRimShapes[k])->put_StrokeBrush(as<wuc::ICompositionBrush>(k==2?i.rimBrush:i.glowBrush).Get()));}
            for(auto& r:i.shoulderRims){check(as<Compositor5>(c)->CreateShapeVisual(&r.visual));check(glassChildren->InsertAtTop(i.visual(r.visual).Get()));for(int k=0;k<3;++k)r.shapes[k]=i.shape(r.visual,widths[k]);
                // The beat's light carries on round the shoulders.
                r.shapes[3]=i.shape(r.visual,9.f);r.shapes[4]=i.shape(r.visual,2.f);check(as<SpriteShape>(r.shapes[3])->put_StrokeBrush(as<wuc::ICompositionBrush>(i.beatGlow).Get()));check(as<SpriteShape>(r.shapes[4])->put_StrokeBrush(as<wuc::ICompositionBrush>(i.beatCore).Get()));
                check(i.visual(r.visual)->put_IsVisible(false));}
            // Edge-light masks fade from the edge inward (a squircle-like falloff in three stops).
            const Vector2 from[4]={{0,.5f},{1,.5f},{.5f,0},{.5f,1}},to[4]={{1,.5f},{0,.5f},{.5f,1},{.5f,0}};
            for(int k=0;k<4;++k){i.lensMasks[k]=i.gradient(false);auto linear=as<LinearGradientBrush>(i.lensMasks[k]);check(linear->put_StartPoint(from[k]));check(linear->put_EndPoint(to[k]));
                i.stops(i.lensMasks[k],{{0,color(0xffffff,.9f)},{.4f,color(0xffffff,.32f)},{1,color(0xffffff,0)}});}
        }
        for(int k=1;k<5;++k)check(i.visual(i.parts[k].container)->put_IsVisible(false));
        check(i.visual(i.glass)->put_IsVisible(false));
        impl_=impl.release();available_=true;
    }catch(...){available_=false;}
    return available_;
}
void GlassBackdrop::style(const GlassStyle& s){
    if(!available_)return;auto& i=*impl_;
    try{
        const bool shown=i.shadowOnly?s.shadow>0:s.visible;check(i.visual(i.glass)->put_IsVisible(shown));if(!shown){i.current.visible=false;return;}
        if(i.styled&&i.current.light==s.light&&i.current.blur==s.blur&&i.current.material==s.material&&std::abs(i.current.tint-s.tint)<.001f&&i.current.accent==s.accent&&i.current.wallpaper==s.wallpaper&&std::abs(i.current.shadow-s.shadow)<.001f){i.current=s;return;}
        i.current=s;i.styled=true;
        if(i.shadowOnly){
            if(!i.shadowDrawn){i.shadowDrawn=true;try{auto brush=i.shadowBrush(i.shadowMargin,20*i.scale);check(i.shadowSprite->put_Brush(as<wuc::ICompositionBrush>(brush).Get()));if(i.shadowStub)check(i.shadowStub->put_Brush(as<wuc::ICompositionBrush>(brush).Get()));if(i.shadowBud)check(i.shadowBud->put_Brush(as<wuc::ICompositionBrush>(brush).Get()));}catch(...){}}
            {String so(L"so");check(i.properties->InsertScalar(so,s.shadow));}if(i.edge>=0)i.layout();return;}
        // Grain needs a GPU drawing surface; without one the glass simply has none.
        if(!i.grainTried){i.grainTried=true;try{i.grainBrush=i.grain();for(auto& part:i.parts)if(part.layers[2])check(part.layers[2]->put_Brush(as<wuc::ICompositionBrush>(i.grainBrush).Get()));}catch(...){i.grainBrush.Reset();}}
        if(!i.sheenTried){i.sheenTried=true;try{const UINT size=UINT(std::ceil(260*i.scale));const float half=float(size)/2;
                i.sheenBrush=i.drawn(size,size,[&](ID2D1DeviceContext* dc){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xffffff,.55f)},{.45f,D2D1::ColorF(0xffffff,.16f)},{1,D2D1::ColorF(0xffffff,0.f)}};
                    ComPtr<ID2D1GradientStopCollection> c;check(dc->CreateGradientStopCollection(stops,3,&c));ComPtr<ID2D1RadialGradientBrush> b;check(dc->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({half,half},{0,0},half,half),c.Get(),&b));
                    dc->FillRectangle(D2D1::RectF(0,0,float(size),float(size)),b.Get());});
                check(i.sheenBrush->put_Stretch(wuc::CompositionStretch_Fill));for(auto& part:i.parts)if(part.sheen)check(part.sheen->put_Brush(as<wuc::ICompositionBrush>(i.sheenBrush).Get()));}catch(...){i.sheenBrush.Reset();}}
        // Frosted: Windows' blurred backdrop run through the material matrix (vibrancy and a
        // luminosity map, tinted toward the wallpaper), with light gathered along the free
        // edges; otherwise a dense translucent frost. Clear: never blurred, a light tint you
        // see through. Neither ever falls back to an opaque fill.
        const bool clear=s.material==2,blurred=!clear&&s.blur;const float tint=std::clamp(s.tint,0.f,1.f);
        float wall[3]={0,0,0};if(s.wallpaper){for(int k=0;k<3;++k)wall[k]=float((s.wallpaper>>(16-8*k))&255)/255;}
        // Dark glass dims what is behind it well below the text; light glass lifts it toward milk.
        const float saturation=s.light?1.55f:1.8f,gain=s.light?.74f-.24f*tint:.34f-.18f*tint;
        // The wallpaper's colour tints the offset a little, so the glass belongs to the desktop it sits on.
        float offset[3];for(int k=0;k<3;++k)offset[k]=s.light?(1-gain)*.95f+(s.wallpaper?.05f*(wall[k]-.6f):0):.03f+(s.wallpaper?.05f*wall[k]:0);
        i.vibrancy=false;
        i.edgeLight=false;i.materialBrush.Reset();i.frostBrush.Reset();
        if(blurred){
            try{i.materialBrush=i.materialOf(saturation,gain,offset);i.vibrancy=true;}catch(const std::exception& e){i.materialBrush.Reset();i.failure=std::string("material: ")+e.what();}
            // Thick glass gathers light at its edges: the same backdrop, brighter and more saturated, fading inward.
            if(i.vibrancy)try{
                float edgeOffset[3];for(int k=0;k<3;++k)edgeOffset[k]=offset[k]+(s.light?.03f:.035f);
                const auto edgeLight=i.materialOf(saturation+.35f,std::min(1.f,gain*(s.light?1.12f:1.45f)),edgeOffset);
                for(int k=0;k<4;++k){ComPtr<IInspectable> mask;check(as<Compositor2>(i.compositor)->CreateMaskBrush(&mask));
                    check(as<MaskBrush>(mask)->put_Source(edgeLight.Get()));check(as<MaskBrush>(mask)->put_Mask(i.lensMasks[k].Get()));check(i.lens[k]->put_Brush(as<wuc::ICompositionBrush>(mask).Get()));}
                i.edgeLight=true;}catch(const std::exception& e){i.failure=std::string("edge light: ")+e.what();}
            // The resting frost: paler (less saturated), milkier and a little softer than the material.
            if(i.vibrancy)try{float frostOffset[3];for(int k=0;k<3;++k)frostOffset[k]=offset[k]+(s.light?.1f:.09f);i.frostBrush=i.frostOf(saturation*.5f,gain*(s.light?.86f:1.05f),frostOffset);}catch(const std::exception& e){i.frostBrush.Reset();i.failure=std::string("frost: ")+e.what();}}
        for(auto& part:i.parts)if(part.frost){check(i.visual(part.frost)->put_IsVisible(bool(i.frostBrush)));if(i.frostBrush)check(part.frost->put_Brush(as<wuc::ICompositionBrush>(i.frostBrush).Get()));}
        for(auto& part:i.parts){check(i.visual(part.layers[0])->put_IsVisible(blurred));check(part.layers[0]->put_Brush(as<wuc::ICompositionBrush>(i.materialBrush?i.materialBrush:i.hostBrush).Get()));}
        // The tint now only unifies the material; without blur (or on clear glass) it carries the frost itself.
        // Clear glass keeps a dimming scrim, enough for text over any wallpaper, like Apple's clear material.
        // Light glass needs more milk than dark glass needs smoke: over a dark desktop a thin light tint turns the pane
        // mid-grey and grey text on it disappears.
        float alpha=clear?(s.light?.44f:.46f)*(.6f+tint*1.1f):blurred?(i.vibrancy?(s.light?.30f:.12f):(s.light?.52f:.42f)*(.55f+tint*.9f)):(s.light?.74f:.68f)*(.8f+tint*.35f);
        alpha=std::clamp(alpha,clear?.08f:.06f,clear?(s.light?.78f:.62f):.92f);
        uint32_t ink=s.light?0xf6f7f9:0x0b0c10;const uint32_t hue=s.wallpaper?s.wallpaper:s.accent;
        if(hue){auto mix=[&](int shift){return uint32_t(((ink>>shift)&255)*(s.light?.92:.86)+((hue>>shift)&255)*(s.light?.08:.14))<<shift;};ink=mix(16)|mix(8)|mix(0);}
        check(i.tintBrush->put_Color(color(ink,alpha)));
        for(auto& part:i.parts)if(part.layers[2])check(i.visual(part.layers[2])->put_Opacity(clear?(s.light?.35f:.55f):(s.light?.55f:.85f)));
        // Depth: glass darkens slightly toward its lower edge, as thick glass does.
        const auto none=color(0x000000,0),shade=s.light?color(0x000000,clear?.06f:.03f):color(0x000000,clear?.20f:.10f);
        i.stops(i.depthBrush,{{0,none},{.6f,none},{1,shade}});
        // A thin specular rim: brightest where light enters at the top, a faint reflection underneath.
        const auto rimTop=color(0xffffff,s.light?.95f:clear?.46f:.36f),rimBottom=s.light?color(0xffffff,clear?.5f:.42f):color(0xffffff,clear?.16f:.13f);
        i.stops(i.rimBrush,{{0,rimTop},{.45f,s.light?color(0xffffff,clear?.34f:.26f):color(0xffffff,clear?.10f:.07f)},{1,rimBottom}});
        // A soft inner glow just inside the edge gives the pane thickness, like light caught in a lens.
        const auto glowTop=color(0xffffff,s.light?(clear?.26f:.18f):(clear?.06f:.045f)),glowBottom=color(0xffffff,s.light?(clear?.10f:.07f):(clear?.025f:.018f));
        i.stops(i.glowBrush,{{0,glowTop},{.4f,color(0xffffff,s.light?(clear?.12f:.08f):(clear?.02f:.012f))},{1,glowBottom}});
        if(i.glintBrush)i.stops(i.glintBrush,{{0,color(0xffffff,s.light?1.f:clear?.95f:.85f)},{.35f,color(0xffffff,s.light?.55f:clear?.42f:.34f)},{1,color(0xffffff,0)}});
        check(i.edgeBrushes[0]->put_Color(rimTop));check(i.edgeBrushes[1]->put_Color(rimBottom));check(i.edgeBrushes[2]->put_Color(glowTop));check(i.edgeBrushes[3]->put_Color(glowBottom));
        // Edge-light visibility depends on the material and the docking, which layout() knows.
        if(i.edge>=0)i.layout();else for(auto& l:i.lens)if(l)check(i.visual(l)->put_IsVisible(false));
    }catch(...){}
}
void GlassBackdrop::animate(const MotionEngine& m,double now,int edge,bool attached){
    if(!available_)return;auto& i=*impl_;const char* step="clock";
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
        ComPtr<IInspectable> props;check(i.properties.As(&props));step="targets";
        {String tw(L"tw"),th(L"th");check(i.properties->InsertScalar(tw,float(m.width.target()*s)));check(i.properties->InsertScalar(th,float(m.height.target()*s)));}
        {String sw(L"sw"),sr(L"sr"),sh(L"sh");check(i.properties->InsertScalar(sw,float(m.stubWidth*s)));check(i.properties->InsertScalar(sr,float(m.stubRadius*s)));check(i.properties->InsertScalar(sh,float(m.stubHeight*s)));}
        step="springs";i.start(props,L"d",springExpression(SpringTerms::from(m.drop,now),1));i.start(props,L"b",springExpression(SpringTerms::from(m.bud,now),1));
        {String bw(L"bw"),bh(L"bh"),ss(L"ss");check(i.properties->InsertScalar(bw,float(m.budWidth*s)));check(i.properties->InsertScalar(bh,float(m.budHeight*s)));check(i.properties->InsertScalar(ss,float(m.spreadShift*s)));}
        i.start(props,L"sp",springExpression(SpringTerms::from(m.spread,now),1));
        i.start(props,L"w",springExpression(SpringTerms::from(m.width,now),s));i.start(props,L"h",springExpression(SpringTerms::from(m.height,now),s));
        i.start(props,L"r",springExpression(SpringTerms::from(m.radius,now),s));i.start(props,L"dx",springExpression(SpringTerms::from(m.dragX,now),s));i.start(props,L"dy",springExpression(SpringTerms::from(m.dragY,now),s));i.start(props,L"s",springExpression(SpringTerms::from(m.slide,now),1));
        step="time";check(as<wuc::ICompositionObject>(i.properties)->StartAnimation(t,as<wuc::ICompositionAnimation>(clock).Get()));
        const float radius=float(m.radius.target());step="layout";
        if(edge!=i.edge||attached!=i.attached||std::abs(radius-i.radius)>1e-3f){i.edge=edge;i.attached=attached;i.radius=radius;i.layout();}
    }catch(const std::exception& e){lastError_=std::string(step)+": "+e.what();}
}
}
