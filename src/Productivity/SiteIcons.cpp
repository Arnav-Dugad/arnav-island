#include "SiteIcons.h"
#include "Common/Http.h"
#include <shlwapi.h>
#include <wincodec.h>
namespace nexus {
std::shared_ptr<const Artwork> decodeIcon(const std::string& bytes){
    if(bytes.empty()||bytes.size()>(1u<<20))return nullptr;
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()),UINT(bytes.size())));if(!stream)return nullptr;
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return nullptr;
    ComPtr<IWICBitmapDecoder> decoder;if(FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder)))return nullptr;
    UINT frames=0;if(FAILED(decoder->GetFrameCount(&frames))||!frames)return nullptr;
    ComPtr<IWICBitmapFrameDecode> best;UINT bestSize=0;
    for(UINT i=0;i<std::min(frames,16u);++i){ComPtr<IWICBitmapFrameDecode> f;UINT w=0,h=0;if(FAILED(decoder->GetFrame(i,&f))||FAILED(f->GetSize(&w,&h))||!w||!h||w>1024||h>1024)continue;
        const UINT size=std::max(w,h);auto better=[&]{if(!best)return true;const int d=std::abs(int(size)-32),bd=std::abs(int(bestSize)-32);return (size>=32)!=(bestSize>=32)?size>=32:d<bd;};if(better()){best=f;bestSize=size;}}
    if(!best)return nullptr;
    ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(best.Get(),32,32,WICBitmapInterpolationModeFant))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    auto art=std::make_shared<Artwork>();art->width=art->height=32;art->pixels.resize(32*32*4);if(FAILED(converter->CopyPixels(nullptr,32*4,UINT(art->pixels.size()),art->pixels.data())))return nullptr;
    return art;
}
SiteIcons::SiteIcons(HWND window):window_(window),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){if(!stop_||!wake_)throw std::runtime_error("Site icon event creation failed");worker_=std::thread([this]{run();});}
SiteIcons::~SiteIcons(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
std::shared_ptr<const Artwork> SiteIcons::get(const std::wstring& host){
    if(host.empty())return nullptr;std::lock_guard lock(mutex_);auto it=icons_.find(host);if(it!=icons_.end())return it->second;
    if(asked_.insert(host).second&&asked_.size()<=256){queue_.push_back(host);SetEvent(wake_);}return nullptr;
}
void SiteIcons::run(){
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);HANDLE waits[]={stop_,wake_};
    while(WaitForMultipleObjects(2,waits,FALSE,INFINITE)==WAIT_OBJECT_0+1){
        for(;;){std::wstring host;{std::lock_guard lock(mutex_);if(queue_.empty())break;host=queue_.front();queue_.pop_front();}
            if(WaitForSingleObject(stop_,0)==WAIT_OBJECT_0)break;
            std::shared_ptr<const Artwork> icon;if(auto body=httpsGet(host.c_str(),L"/favicon.ico"))icon=decodeIcon(*body);
            if(icon){{std::lock_guard lock(mutex_);icons_[host]=icon;}PostMessageW(window_,SiteIconMessage,0,0);}}
    }
    CoUninitialize();
}
}
