#include "Capture/Ocr.h"
#include "Capture/CaptureModel.h"
#include <inspectable.h>
#include <winstring.h>
#include <roapi.h>
#include <asyncinfo.h>
#include <chrono>
#include <thread>
#include <vector>
// Minimal hand-written projection of Windows.Media.Ocr, Windows.Graphics.Imaging and
// CryptographicBuffer. Interface ids and method order were read from the Windows
// metadata (see docs/ARCHITECTURE.md); only the methods used are called.
namespace nexus {
// The interfaces need external linkage: in an anonymous namespace GCC sees that nothing in this
// file implements them and, at -O2 and above, treats calls through them as unreachable.
namespace ocrabi {
struct Language:IInspectable{virtual HRESULT STDMETHODCALLTYPE get_LanguageTag(HSTRING*)=0;};
struct Async:IInspectable{virtual HRESULT STDMETHODCALLTYPE put_Completed(IUnknown*)=0;virtual HRESULT STDMETHODCALLTYPE get_Completed(IUnknown**)=0;virtual HRESULT STDMETHODCALLTYPE GetResults(IInspectable**)=0;};
struct VectorView:IInspectable{virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32,IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE get_Size(UINT32*)=0;};
struct OcrLine:IInspectable{virtual HRESULT STDMETHODCALLTYPE get_Words(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE get_Text(HSTRING*)=0;};
struct OcrResult:IInspectable{virtual HRESULT STDMETHODCALLTYPE get_Lines(VectorView**)=0;virtual HRESULT STDMETHODCALLTYPE get_TextAngle(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE get_Text(HSTRING*)=0;};
struct OcrEngine:IInspectable{virtual HRESULT STDMETHODCALLTYPE RecognizeAsync(IInspectable*,Async**)=0;virtual HRESULT STDMETHODCALLTYPE get_RecognizerLanguage(Language**)=0;};
struct OcrEngineStatics:IInspectable{virtual HRESULT STDMETHODCALLTYPE get_MaxImageDimension(UINT32*)=0;virtual HRESULT STDMETHODCALLTYPE get_AvailableRecognizerLanguages(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE IsLanguageSupported(IInspectable*,boolean*)=0;virtual HRESULT STDMETHODCALLTYPE TryCreateFromLanguage(IInspectable*,OcrEngine**)=0;virtual HRESULT STDMETHODCALLTYPE TryCreateFromUserProfileLanguages(OcrEngine**)=0;};
struct SoftwareBitmapStatics:IInspectable{virtual HRESULT STDMETHODCALLTYPE Copy(IInspectable*,IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE Convert(IInspectable*,INT32,IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE ConvertWithAlpha(IInspectable*,INT32,INT32,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateCopyFromBuffer(IInspectable*,INT32,INT32,INT32,IInspectable**)=0;};
struct CryptographicBufferStatics:IInspectable{virtual HRESULT STDMETHODCALLTYPE Compare(IInspectable*,IInspectable*,boolean*)=0;virtual HRESULT STDMETHODCALLTYPE GenerateRandom(UINT32,IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE GenerateRandomNumber(UINT32*)=0;
    virtual HRESULT STDMETHODCALLTYPE CreateFromByteArray(UINT32,BYTE*,IInspectable**)=0;};
}
using namespace ocrabi;
namespace {
constexpr GUID IID_OcrEngineStatics{0x5bffa85a,0x3384,0x3540,{0x99,0x40,0x69,0x91,0x20,0xd4,0x28,0xa8}};
constexpr GUID IID_SoftwareBitmapStatics{0xdf0385db,0x672f,0x4a9d,{0x80,0x6e,0xc2,0x44,0x2f,0x34,0x3e,0x86}};
constexpr GUID IID_CryptographicBufferStatics{0x320b7e22,0x3cb0,0x4cdf,{0x86,0x63,0x1d,0x28,0x91,0x00,0x65,0xeb}};
struct Hstring{HSTRING h=nullptr;~Hstring(){if(h)WindowsDeleteString(h);}std::wstring str()const{UINT32 n=0;auto p=WindowsGetStringRawBuffer(h,&n);return p?std::wstring(p,n):std::wstring{};}};
template<class T> HRESULT factory(const wchar_t* name,const GUID& iid,T** out){HSTRING_HEADER header;HSTRING cls=nullptr;HRESULT hr=WindowsCreateStringReference(name,UINT32(wcslen(name)),&header,&cls);if(FAILED(hr))return hr;return RoGetActivationFactory(cls,iid,reinterpret_cast<void**>(out));}
// Scales BGRA with a box filter (down) or bilinear (up) to the recognition size.
std::vector<uint8_t> scaled(const uint8_t* src,int w,int h,int nw,int nh){
    std::vector<uint8_t> out(size_t(nw)*nh*4);
    for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){const double fx=(x+.5)*w/nw-.5,fy=(y+.5)*h/nh-.5;const int x0=std::clamp(int(std::floor(fx)),0,w-1),y0=std::clamp(int(std::floor(fy)),0,h-1),x1=std::min(x0+1,w-1),y1=std::min(y0+1,h-1);const double ax=std::clamp(fx-x0,0.,1.),ay=std::clamp(fy-y0,0.,1.);
        for(int c=0;c<4;++c){auto p=[&](int px,int py){return double(src[(size_t(py)*w+px)*4+c]);};const double v=(p(x0,y0)*(1-ax)+p(x1,y0)*ax)*(1-ay)+(p(x0,y1)*(1-ax)+p(x1,y1)*ax)*ay;out[(size_t(y)*nw+x)*4+c]=uint8_t(std::clamp(v+.5,0.,255.));}}
    return out;
}
}
std::optional<OcrText> recognizeText(const uint8_t* bgra,int width,int height){
    if(!bgra||width<1||height<1)return std::nullopt;
    const bool initialized=SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED));struct Uninit{bool on;~Uninit(){if(on)RoUninitialize();}} uninit{initialized};
    ComPtr<OcrEngineStatics> statics;if(FAILED(factory(L"Windows.Media.Ocr.OcrEngine",IID_OcrEngineStatics,statics.GetAddressOf())))return std::nullopt;
    ComPtr<OcrEngine> engine;if(FAILED(statics->TryCreateFromUserProfileLanguages(&engine))||!engine)return std::nullopt;
    UINT32 limit=4096;statics->get_MaxImageDimension(&limit);
    const double s=ocrScale(width,height,int(limit));const int w=std::max(1,int(std::lround(width*s))),h=std::max(1,int(std::lround(height*s)));
    std::vector<uint8_t> pixels=(w==width&&h==height)?std::vector<uint8_t>(bgra,bgra+size_t(width)*height*4):scaled(bgra,width,height,w,h);
    for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
    ComPtr<CryptographicBufferStatics> buffers;ComPtr<IInspectable> buffer;if(FAILED(factory(L"Windows.Security.Cryptography.CryptographicBuffer",IID_CryptographicBufferStatics,buffers.GetAddressOf()))||FAILED(buffers->CreateFromByteArray(UINT32(pixels.size()),pixels.data(),&buffer)))return std::nullopt;
    ComPtr<SoftwareBitmapStatics> bitmaps;ComPtr<IInspectable> bitmap;// BitmapPixelFormat::Bgra8 = 87
    if(FAILED(factory(L"Windows.Graphics.Imaging.SoftwareBitmap",IID_SoftwareBitmapStatics,bitmaps.GetAddressOf()))||FAILED(bitmaps->CreateCopyFromBuffer(buffer.Get(),87,w,h,&bitmap)))return std::nullopt;
    ComPtr<Async> operation;if(FAILED(engine->RecognizeAsync(bitmap.Get(),&operation)))return std::nullopt;
    ComPtr<IAsyncInfo> info;if(FAILED(operation.As(&info)))return std::nullopt;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);AsyncStatus status=AsyncStatus::Started;
    while(SUCCEEDED(info->get_Status(&status))&&status==AsyncStatus::Started){if(std::chrono::steady_clock::now()>deadline){info->Cancel();return std::nullopt;}std::this_thread::sleep_for(std::chrono::milliseconds(8));}
    if(status!=AsyncStatus::Completed)return std::nullopt;
    ComPtr<IInspectable> resultObject;if(FAILED(operation->GetResults(&resultObject))||!resultObject)return std::nullopt;
    auto* result=reinterpret_cast<OcrResult*>(resultObject.Get());// the operation returns the OcrResult's default interface
    ComPtr<VectorView> lines;std::vector<std::wstring> texts;
    if(SUCCEEDED(result->get_Lines(&lines))&&lines){UINT32 n=0;lines->get_Size(&n);for(UINT32 i=0;i<n&&i<4000;++i){ComPtr<IInspectable> item;if(FAILED(lines->GetAt(i,&item))||!item)continue;Hstring t;if(SUCCEEDED(reinterpret_cast<OcrLine*>(item.Get())->get_Text(&t.h)))texts.push_back(t.str());}}
    OcrText out;out.text=joinLines(texts);ComPtr<Language> language;if(SUCCEEDED(engine->get_RecognizerLanguage(&language))&&language){Hstring tag;if(SUCCEEDED(language->get_LanguageTag(&tag.h)))out.language=tag.str();}
    return out;
}
}
