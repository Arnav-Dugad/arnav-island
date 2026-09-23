#pragma once
// Minimal projection of the public Windows.UI.Composition interfaces that the
// MinGW headers omit. Vtable order and IIDs were read from the operating
// system's own Windows.UI.winmd metadata (method declaration order), not from
// private headers. Only the methods used here are called; earlier slots are
// declared to keep the layout exact. Keep this boundary isolated.
#include "Common/Win32.h"
// MinGW declares IReference<BYTE> and IReference<boolean>, which are the same
// specialization because boolean is unsigned char. Keep only the first.
#ifndef ____FIReference_1_boolean_INTERFACE_DEFINED__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#endif
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>
#include <dispatcherqueue.h>
#include <winstring.h>
#include <roapi.h>
#include <algorithm>
#include <cmath>
namespace nexus::wincomp {
namespace wuc=ABI::Windows::UI::Composition;
using Vector2=ABI::Windows::Foundation::Numerics::Vector2;
using Vector3=ABI::Windows::Foundation::Numerics::Vector3;
using Color=ABI::Windows::UI::Color;
using Unknown=IInspectable;

struct CompositorDesktopInterop:IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopWindowTarget(HWND,BOOL,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE EnsureOnThread(DWORD)=0;
};
struct Compositor3:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateHostBackdropBrush(IInspectable**)=0;
};
struct Compositor4:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateColorGradientStop(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateColorGradientStopWithOffsetAndColor(FLOAT,Color,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateLinearGradientBrush(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpringScalarAnimation(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpringVector2Animation(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpringVector3Animation(IInspectable**)=0;
};
struct Compositor5:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Comment(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE put_Comment(HSTRING)=0;
    virtual HRESULT STDMETHODCALLTYPE get_GlobalPlaybackRate(FLOAT*)=0;
    virtual HRESULT STDMETHODCALLTYPE put_GlobalPlaybackRate(FLOAT)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateBounceScalarAnimation(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateBounceVector2Animation(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateBounceVector3Animation(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateContainerShape(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateEllipseGeometry(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateLineGeometry(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreatePathGeometry(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreatePathGeometryWithPath(IInspectable*,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreatePathKeyFrameAnimation(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateRectangleGeometry(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateRoundedRectangleGeometry(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateShapeVisual(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpriteShape(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpriteShapeWithGeometry(IInspectable*,IInspectable**)=0;
};
struct Compositor6:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateGeometricClip(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateGeometricClipWithGeometry(IInspectable*,IInspectable**)=0;
};
struct RoundedRectangleGeometry:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_CornerRadius(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_CornerRadius(Vector2)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Offset(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_Offset(Vector2)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_Size(Vector2)=0;
};
struct SpriteShape:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_FillBrush(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE put_FillBrush(IInspectable*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Geometry(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE put_Geometry(IInspectable*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_IsStrokeNonScaling(boolean*)=0;virtual HRESULT STDMETHODCALLTYPE put_IsStrokeNonScaling(boolean)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeBrush(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeBrush(IInspectable*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeDashArray(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeDashCap(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeDashCap(INT32)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeDashOffset(FLOAT*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeDashOffset(FLOAT)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeEndCap(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeEndCap(INT32)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeLineJoin(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeLineJoin(INT32)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeMiterLimit(FLOAT*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeMiterLimit(FLOAT)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeStartCap(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeStartCap(INT32)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StrokeThickness(FLOAT*)=0;virtual HRESULT STDMETHODCALLTYPE put_StrokeThickness(FLOAT)=0;
};
struct ShapeVisual:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Shapes(IInspectable**)=0;
};
// IVector<T> for the shape and gradient-stop collections; only Append is used.
struct Vector:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32,IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE get_Size(UINT32*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetView(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE IndexOf(IInspectable*,UINT32*,boolean*)=0;
    virtual HRESULT STDMETHODCALLTYPE SetAt(UINT32,IInspectable*)=0;virtual HRESULT STDMETHODCALLTYPE InsertAt(UINT32,IInspectable*)=0;
    virtual HRESULT STDMETHODCALLTYPE RemoveAt(UINT32)=0;virtual HRESULT STDMETHODCALLTYPE Append(IInspectable*)=0;
};
struct KeyFrameAnimation:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_DelayTime(ABI::Windows::Foundation::TimeSpan*)=0;virtual HRESULT STDMETHODCALLTYPE put_DelayTime(ABI::Windows::Foundation::TimeSpan)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Duration(ABI::Windows::Foundation::TimeSpan*)=0;virtual HRESULT STDMETHODCALLTYPE put_Duration(ABI::Windows::Foundation::TimeSpan)=0;
};
struct Visual2:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_ParentForTransform(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE put_ParentForTransform(IInspectable*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_RelativeOffsetAdjustment(Vector3*)=0;virtual HRESULT STDMETHODCALLTYPE put_RelativeOffsetAdjustment(Vector3)=0;
    virtual HRESULT STDMETHODCALLTYPE get_RelativeSizeAdjustment(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_RelativeSizeAdjustment(Vector2)=0;
};
struct GradientBrush:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_AnchorPoint(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_AnchorPoint(Vector2)=0;
    virtual HRESULT STDMETHODCALLTYPE get_CenterPoint(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_CenterPoint(Vector2)=0;
    virtual HRESULT STDMETHODCALLTYPE get_ColorStops(IInspectable**)=0;
};
struct LinearGradientBrush:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_EndPoint(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_EndPoint(Vector2)=0;
    virtual HRESULT STDMETHODCALLTYPE get_StartPoint(Vector2*)=0;virtual HRESULT STDMETHODCALLTYPE put_StartPoint(Vector2)=0;
};

class String {
    HSTRING value_=nullptr;
public:
    explicit String(const wchar_t* text){check(WindowsCreateString(text,UINT32(wcslen(text)),&value_));}
    ~String(){WindowsDeleteString(value_);}
    String(const String&)=delete;String& operator=(const String&)=delete;
    operator HSTRING()const{return value_;}
};
template<class T,class S> ComPtr<T> as(const ComPtr<S>& source){ComPtr<T> result;check(source.As(&result));return result;}
inline Color color(uint32_t rgb,float alpha){return Color{BYTE(std::lround(std::clamp(alpha,0.f,1.f)*255)),BYTE((rgb>>16)&255),BYTE((rgb>>8)&255),BYTE(rgb&255)};}
}
__CRT_UUID_DECL(nexus::wincomp::CompositorDesktopInterop,0x29E691FA,0x4567,0x4DCA,0xB3,0x19,0xD0,0xF2,0x07,0xEB,0x68,0x07)
__CRT_UUID_DECL(nexus::wincomp::Compositor3,0xc9dd8ef0,0x6eb1,0x4e3c,0xa6,0x58,0x67,0x5d,0x9c,0x64,0xd4,0xab)
__CRT_UUID_DECL(nexus::wincomp::Compositor4,0xae47e78a,0x7910,0x4425,0xa4,0x82,0xa0,0x5b,0x75,0x8a,0xdc,0xe9)
__CRT_UUID_DECL(nexus::wincomp::Compositor5,0x48ea31ad,0x7fcd,0x4076,0xa7,0x9c,0x90,0xcc,0x4b,0x85,0x2c,0x9b)
__CRT_UUID_DECL(nexus::wincomp::Compositor6,0x7a38b2bd,0xcec8,0x4eeb,0x83,0x0f,0xd8,0xd0,0x7a,0xed,0xeb,0xc3)
__CRT_UUID_DECL(nexus::wincomp::RoundedRectangleGeometry,0x8770c822,0x1d50,0x4b8b,0xb0,0x13,0x7c,0x9a,0x0e,0x46,0x93,0x5f)
__CRT_UUID_DECL(nexus::wincomp::SpriteShape,0x401b61bb,0x0007,0x4363,0xb1,0xf3,0x6b,0xcc,0x00,0x3f,0xb8,0x3e)
__CRT_UUID_DECL(nexus::wincomp::ShapeVisual,0xf2bd13c3,0xba7e,0x4b0f,0x91,0x26,0xff,0xb7,0x53,0x6b,0x81,0x76)
__CRT_UUID_DECL(nexus::wincomp::GradientBrush,0x1d9709e0,0xffc6,0x4c0e,0xa9,0xab,0x34,0x14,0x4d,0x4c,0x90,0x98)
__CRT_UUID_DECL(nexus::wincomp::LinearGradientBrush,0x983bc519,0xa9db,0x413c,0xa2,0xd8,0x2a,0x90,0x56,0xfc,0x52,0x5e)
__CRT_UUID_DECL(nexus::wincomp::Visual2,0x3052b611,0x56c3,0x4c3e,0x8b,0xf3,0xf6,0xe1,0xad,0x47,0x3f,0x06)
__CRT_UUID_DECL(nexus::wincomp::KeyFrameAnimation,0x126e7f22,0x3ae9,0x4540,0x9a,0x8a,0xde,0xae,0x8a,0x4a,0x4a,0x84)
