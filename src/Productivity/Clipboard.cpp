#include "Productivity/Clipboard.h"
#include "Media/AppIdentity.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <wincodec.h>
namespace nexus {
namespace {
struct Locked {HANDLE h;void* p;explicit Locked(HANDLE handle):h(handle),p(handle?GlobalLock(handle):nullptr){}~Locked(){if(p)GlobalUnlock(h);}};
UINT format(const wchar_t* name){return RegisterClipboardFormatW(name);}
// The flags Windows' own clipboard history respects: copies marked this way
// (passwords, one-time codes) are never kept.
bool markedPrivate(){
    if(IsClipboardFormatAvailable(format(L"ExcludeClipboardContentFromMonitorProcessing"))||IsClipboardFormatAvailable(format(L"Clipboard Viewer Ignore")))return true;
    UINT include=format(L"CanIncludeInClipboardHistory");
    if(IsClipboardFormatAvailable(include))if(HANDLE h=GetClipboardData(include)){Locked l(h);if(l.p&&GlobalSize(h)>=sizeof(DWORD)&&*static_cast<DWORD*>(l.p)==0)return true;}
    return false;
}
}
std::shared_ptr<const Artwork> dibThumbnail(const std::vector<uint8_t>& dib,UINT size,uint32_t* width,uint32_t* height){
    if(dib.size()<sizeof(BITMAPINFOHEADER))return nullptr;
    const auto* info=reinterpret_cast<const BITMAPINFOHEADER*>(dib.data());
    // A BMP file is the DIB with a file header in front, which WIC can decode.
    DWORD colors=info->biClrUsed?info->biClrUsed:(info->biBitCount<=8?(1u<<info->biBitCount):0);DWORD masks=(info->biCompression==BI_BITFIELDS&&info->biSize==sizeof(BITMAPINFOHEADER))?12:0;
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfSize=DWORD(sizeof(file)+dib.size());file.bfOffBits=DWORD(sizeof(file)+info->biSize+masks+colors*4);
    std::vector<uint8_t> bmp(sizeof(file)+dib.size());memcpy(bmp.data(),&file,sizeof(file));memcpy(bmp.data()+sizeof(file),dib.data(),dib.size());
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(bmp.data(),UINT(bmp.size())));if(!stream)return nullptr;
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return nullptr;
    ComPtr<IWICBitmapDecoder> decoder;if(FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder)))return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame;UINT w=0,h=0;if(FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&w,&h))||!w||!h||w>16384||h>16384)return nullptr;
    if(width)*width=w;if(height)*height=h;
    double scale=std::min(1.,double(size)/std::max(w,h));UINT tw=std::max(1u,UINT(w*scale+.5)),th=std::max(1u,UINT(h*scale+.5));
    ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(frame.Get(),tw,th,WICBitmapInterpolationModeFant))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    auto art=std::make_shared<Artwork>();art->width=tw;art->height=th;art->pixels.resize(size_t(tw)*th*4);
    if(FAILED(converter->CopyPixels(nullptr,tw*4,UINT(art->pixels.size()),art->pixels.data())))return nullptr;
    // 32-bit clipboard bitmaps often carry no alpha (all zero); show them opaque.
    bool anyAlpha=false;for(size_t i=3;i<art->pixels.size();i+=4)if(art->pixels[i]){anyAlpha=true;break;}
    if(!anyAlpha&&info->biBitCount==32)for(size_t i=3;i<art->pixels.size();i+=4)art->pixels[i]=255;
    return art;
}
namespace {
// A DIB as a BMP file in memory, which WIC can decode.
ComPtr<IWICBitmapFrameDecode> decodeDib(IWICImagingFactory* factory,const std::vector<uint8_t>& dib,std::vector<uint8_t>& bmp){
    if(dib.size()<sizeof(BITMAPINFOHEADER))return nullptr;const auto* info=reinterpret_cast<const BITMAPINFOHEADER*>(dib.data());
    DWORD colors=info->biClrUsed?info->biClrUsed:(info->biBitCount<=8?(1u<<info->biBitCount):0);DWORD masks=(info->biCompression==BI_BITFIELDS&&info->biSize==sizeof(BITMAPINFOHEADER))?12:0;
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfSize=DWORD(sizeof(file)+dib.size());file.bfOffBits=DWORD(sizeof(file)+info->biSize+masks+colors*4);
    bmp.resize(sizeof(file)+dib.size());memcpy(bmp.data(),&file,sizeof(file));memcpy(bmp.data()+sizeof(file),dib.data(),dib.size());
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(bmp.data(),UINT(bmp.size())));if(!stream)return nullptr;
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame)))return nullptr;return frame;
}
}
std::shared_ptr<const std::string> dibToPng(const std::vector<uint8_t>& dib){
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return nullptr;
    std::vector<uint8_t> bmp;auto frame=decodeDib(factory.Get(),dib,bmp);UINT w=0,h=0;if(!frame||FAILED(frame->GetSize(&w,&h))||!w||!h||w>16384||h>16384)return nullptr;
    // 32-bit copies with no alpha at all (most screenshots) are saved opaque.
    const auto* info=reinterpret_cast<const BITMAPINFOHEADER*>(dib.data());bool opaque=true;
    if(info->biBitCount==32){WICPixelFormatGUID f{};frame->GetPixelFormat(&f);if(f==GUID_WICPixelFormat32bppBGRA||f==GUID_WICPixelFormat32bppPBGRA){std::vector<uint8_t> px(size_t(w)*h*4);if(SUCCEEDED(frame->CopyPixels(nullptr,w*4,UINT(px.size()),px.data())))for(size_t i=3;i<px.size();i+=4)if(px[i]){opaque=false;break;}}}
    ComPtr<IWICFormatConverter> converter;if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),opaque?GUID_WICPixelFormat24bppBGR:GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    ComPtr<IStream> out;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&out)))return nullptr;
    ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> target;ComPtr<IPropertyBag2> options;WICPixelFormatGUID format=opaque?GUID_WICPixelFormat24bppBGR:GUID_WICPixelFormat32bppBGRA;
    if(FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||FAILED(encoder->Initialize(out.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&target,&options))||FAILED(target->Initialize(options.Get()))||
       FAILED(target->SetSize(w,h))||FAILED(target->SetPixelFormat(&format))||FAILED(target->WriteSource(converter.Get(),nullptr))||FAILED(target->Commit())||FAILED(encoder->Commit()))return nullptr;
    HGLOBAL memory=nullptr;if(FAILED(GetHGlobalFromStream(out.Get(),&memory)))return nullptr;STATSTG stat{};if(FAILED(out->Stat(&stat,STATFLAG_NONAME)))return nullptr;
    const size_t n=size_t(stat.cbSize.QuadPart);auto* p=static_cast<const char*>(GlobalLock(memory));if(!p)return nullptr;auto png=std::make_shared<std::string>(p,n);GlobalUnlock(memory);return png;
}
std::vector<uint8_t> pngToDib(const std::string& png){
    ComPtr<IWICImagingFactory> factory;if(png.empty()||FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return {};
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(png.data()),UINT(png.size())));if(!stream)return {};
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;UINT w=0,h=0;
    if(FAILED(factory->CreateDecoderFromStream(stream.Get(),&GUID_VendorMicrosoft,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&w,&h))||!w||!h||w>16384||h>16384||size_t(w)*h*4>(64u<<20))return {};
    ComPtr<IWICFormatConverter> converter;if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    std::vector<uint8_t> pixels(size_t(w)*h*4);if(FAILED(converter->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data())))return {};
    // A bottom-up DIB, as apps expect from the clipboard.
    std::vector<uint8_t> dib(sizeof(BITMAPINFOHEADER)+pixels.size());auto* info=reinterpret_cast<BITMAPINFOHEADER*>(dib.data());
    info->biSize=sizeof(BITMAPINFOHEADER);info->biWidth=LONG(w);info->biHeight=LONG(h);info->biPlanes=1;info->biBitCount=32;info->biCompression=BI_RGB;info->biSizeImage=DWORD(pixels.size());
    for(UINT y=0;y<h;++y)memcpy(dib.data()+sizeof(BITMAPINFOHEADER)+size_t(h-1-y)*w*4,pixels.data()+size_t(y)*w*4,size_t(w)*4);
    return dib;
}
ClipboardWatcher::Read ClipboardWatcher::capture(ClipEntry& out){
    if(GetClipboardSequenceNumber()==ownSequence_)return Read::Skipped;
    // The owner is the app that copied; its name and icon label the entry.
    HWND owner=GetClipboardOwner();DWORD pid=0;if(owner)GetWindowThreadProcessId(owner,&pid);
    std::wstring path,name;if(pid&&pid!=GetCurrentProcessId())name=processName(pid,&path);
    if(pid==GetCurrentProcessId())return Read::Skipped;
    if(privateSource(path)||privateSource(name))return Read::Skipped;
    if(!OpenClipboard(window_))return Read::Busy;
    struct Close{~Close(){CloseClipboard();}} close;
    if(markedPrivate())return Read::Skipped;
    ClipEntry e;e.source=name;e.sourcePath=path;
    if(IsClipboardFormatAvailable(CF_HDROP)){
        if(HANDLE h=GetClipboardData(CF_HDROP)){auto drop=static_cast<HDROP>(h);UINT n=DragQueryFileW(drop,0xffffffff,nullptr,0);
            for(UINT i=0;i<std::min(n,64u);++i){UINT len=DragQueryFileW(drop,i,nullptr,0);std::wstring f(len,L'\0');DragQueryFileW(drop,i,f.data(),len+1);e.files.push_back(std::move(f));}}
        if(e.files.empty())return Read::Skipped;e.kind=ClipEntry::Kind::Files;
    }else if(IsClipboardFormatAvailable(CF_UNICODETEXT)){
        HANDLE h=GetClipboardData(CF_UNICODETEXT);if(!h)return Read::Skipped;Locked l(h);if(!l.p)return Read::Skipped;
        size_t max=GlobalSize(h)/sizeof(wchar_t);const wchar_t* t=static_cast<const wchar_t*>(l.p);size_t len=0;while(len<max&&t[len])++len;
        if(len>1024*1024)len=1024*1024;e.text.assign(t,len);e.kind=isLink(e.text)?ClipEntry::Kind::Link:ClipEntry::Kind::Text;
    }else if(IsClipboardFormatAvailable(CF_DIB)){
        HANDLE h=GetClipboardData(CF_DIB);if(!h)return Read::Skipped;Locked l(h);size_t bytes=GlobalSize(h);if(!l.p||bytes<sizeof(BITMAPINFOHEADER)||bytes>32u*1024*1024)return Read::Skipped;
        e.dib.assign(static_cast<const uint8_t*>(l.p),static_cast<const uint8_t*>(l.p)+bytes);e.kind=ClipEntry::Kind::Image;
    }else return Read::Skipped;
    // Image thumbnails and source icons are made by the caller, after the clipboard is closed.
    out=std::move(e);return Read::Captured;
}
bool ClipboardWatcher::copy(const ClipEntry& e){
    if(!OpenClipboard(window_))return false;struct Close{~Close(){CloseClipboard();}} close;
    if(!EmptyClipboard())return false;HANDLE data=nullptr;UINT kind=0;
    if(e.kind==ClipEntry::Kind::Files){size_t chars=1;for(auto& f:e.files)chars+=f.size()+1;data=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(DROPFILES)+chars*sizeof(wchar_t));if(!data)return false;
        {Locked l(data);auto* drop=static_cast<DROPFILES*>(l.p);drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;auto* p=reinterpret_cast<wchar_t*>(reinterpret_cast<uint8_t*>(l.p)+sizeof(DROPFILES));for(auto& f:e.files){memcpy(p,f.c_str(),f.size()*sizeof(wchar_t));p+=f.size()+1;}}kind=CF_HDROP;}
    else if(e.kind==ClipEntry::Kind::Image){data=GlobalAlloc(GMEM_MOVEABLE,e.dib.size());if(!data)return false;{Locked l(data);memcpy(l.p,e.dib.data(),e.dib.size());}kind=CF_DIB;}
    else{data=GlobalAlloc(GMEM_MOVEABLE,(e.text.size()+1)*sizeof(wchar_t));if(!data)return false;{Locked l(data);memcpy(l.p,e.text.c_str(),(e.text.size()+1)*sizeof(wchar_t));}kind=CF_UNICODETEXT;}
    if(!SetClipboardData(kind,data)){GlobalFree(data);return false;}
    ownSequence_=GetClipboardSequenceNumber();return true;
}
}
