#pragma once
#include "Common/Win32.h"
#include "Interaction/DashboardModel.h"
#include <shobjidl.h>
#include <wincodec.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <map>
#include <vector>
#include <filesystem>
namespace nexus {
constexpr UINT ShelfPreviewMessage=WM_APP+22;
class ShelfPreviews {
    HWND window_;std::thread thread_;std::mutex mutex_;std::condition_variable ready_;bool stopping_=false,pending_=false;
    std::vector<std::wstring> requested_;std::map<std::wstring,std::shared_ptr<const Artwork>> cache_;
    static std::shared_ptr<const Artwork> load(const std::wstring& path){
        DWORD flags=GetFileAttributesW(path.c_str());if(flags==INVALID_FILE_ATTRIBUTES||(flags&(FILE_ATTRIBUTE_OFFLINE|0x00400000|0x00040000)))return {};
        ComPtr<IShellItemImageFactory> item;if(FAILED(SHCreateItemFromParsingName(std::filesystem::path(path).make_preferred().c_str(),nullptr,IID_PPV_ARGS(&item))))return {};
        HBITMAP bitmap=nullptr;if(FAILED(item->GetImage({128,128},SIIGBF_RESIZETOFIT,&bitmap))||!bitmap)return {};
        struct Cleanup{HBITMAP b;~Cleanup(){DeleteObject(b);}} cleanup{bitmap};
        ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        ComPtr<IWICBitmap> image;check(factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapUseAlpha,&image));
        ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));check(converter->Initialize(image.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        auto result=std::make_shared<Artwork>();check(converter->GetSize(&result->width,&result->height));if(!result->width||!result->height||result->width>256||result->height>256)return {};
        result->pixels.resize(size_t(result->width)*result->height*4);check(converter->CopyPixels(nullptr,result->width*4,UINT(result->pixels.size()),result->pixels.data()));return result;
    }
    void run(){if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return;for(;;){std::vector<std::wstring> paths;{std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return stopping_||pending_;});if(stopping_)break;paths=requested_;pending_=false;}
        std::map<std::wstring,std::shared_ptr<const Artwork>> next;
        for(auto& path:paths){bool cached=false;{std::lock_guard lock(mutex_);if(stopping_)break;auto it=cache_.find(path);if(it!=cache_.end()){next[path]=it->second;cached=true;}}if(!cached)try{next[path]=load(path);}catch(...){next[path]={};}}
        {std::lock_guard lock(mutex_);if(stopping_)break;cache_=std::move(next);}PostMessageW(window_,ShelfPreviewMessage,0,0);
    }CoUninitialize();}
public:
    explicit ShelfPreviews(HWND window):window_(window){thread_=std::thread([this]{run();});}
    ~ShelfPreviews(){{std::lock_guard lock(mutex_);stopping_=true;}ready_.notify_one();thread_.join();}
    void request(std::vector<std::wstring> paths){if(paths.size()>32)paths.resize(32);{std::lock_guard lock(mutex_);requested_=std::move(paths);pending_=true;}ready_.notify_one();}
    std::shared_ptr<const Artwork> get(const std::wstring& path){std::lock_guard lock(mutex_);auto it=cache_.find(path);return it==cache_.end()?nullptr:it->second;}
};
}
