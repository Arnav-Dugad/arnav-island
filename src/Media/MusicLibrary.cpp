#include "Media/MusicLibrary.h"
#include "Design/Accent.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <propkey.h>
#include <wincodec.h>
#include <filesystem>
namespace nexus {
namespace {
namespace fs=std::filesystem;
// The shell's property keys for a song (propkey.h), declared here so no GUID library is needed.
constexpr PROPERTYKEY titleKey{{0xf29f85e0,0x4ff9,0x1068,{0xab,0x91,0x08,0x00,0x2b,0x27,0xb3,0xd9}},2};
constexpr PROPERTYKEY artistKey{{0x56a3372e,0xce9c,0x11d2,{0x9f,0x0e,0x00,0x60,0x97,0xc6,0x86,0xf6}},2};
constexpr PROPERTYKEY albumKey{{0x56a3372e,0xce9c,0x11d2,{0x9f,0x0e,0x00,0x60,0x97,0xc6,0x86,0xf6}},4};
constexpr PROPERTYKEY trackKey{{0x56a3372e,0xce9c,0x11d2,{0x9f,0x0e,0x00,0x60,0x97,0xc6,0x86,0xf6}},7};
constexpr PROPERTYKEY durationKey{{0x64440490,0x4c8b,0x11d1,{0x8b,0x70,0x08,0x00,0x36,0xb1,0x1a,0x03}},3};
std::wstring text(IPropertyStore* store,const PROPERTYKEY& key){
    PROPVARIANT v;PropVariantInit(&v);std::wstring out;
    if(SUCCEEDED(store->GetValue(key,&v))){
        if(v.vt==VT_LPWSTR&&v.pwszVal)out=v.pwszVal;
        // Artists are a list; the first is the one shown.
        else if(v.vt==(VT_VECTOR|VT_LPWSTR)&&v.calpwstr.cElems&&v.calpwstr.pElems[0])out=v.calpwstr.pElems[0];}
    PropVariantClear(&v);if(out.size()>256)out.resize(256);for(auto& c:out)if(c<32)c=L' ';return out;
}
uint64_t number(IPropertyStore* store,const PROPERTYKEY& key){
    PROPVARIANT v;PropVariantInit(&v);uint64_t out=0;
    if(SUCCEEDED(store->GetValue(key,&v))){if(v.vt==VT_UI8)out=v.uhVal.QuadPart;else if(v.vt==VT_UI4)out=v.ulVal;else if(v.vt==VT_I4&&v.lVal>0)out=uint64_t(v.lVal);}
    PropVariantClear(&v);return out;
}
bool skipped(const fs::path& p){const DWORD a=GetFileAttributesW(p.c_str());return a==INVALID_FILE_ATTRIBUTES||(a&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE|0x00400000/*recall on data access*/|0x00040000/*recall on open*/));}
}
LibraryTrack MusicLibrary::read(const std::wstring& path){
    LibraryTrack t;t.path=path;std::error_code e;t.size=fs::file_size(path,e);if(e)t.size=0;
    ComPtr<IPropertyStore> store;
    if(SUCCEEDED(SHGetPropertyStoreFromParsingName(path.c_str(),nullptr,GPS_DEFAULT,IID_PPV_ARGS(&store)))){
        t.title=text(store.Get(),titleKey);t.artist=text(store.Get(),artistKey);t.album=text(store.Get(),albumKey);
        t.number=uint32_t(std::min<uint64_t>(number(store.Get(),trackKey),9999));t.duration=double(number(store.Get(),durationKey))/1e7;}
    if(t.title.empty())t.title=titleFromFileName(path);
    return t;
}
std::shared_ptr<const Artwork> audioArtwork(const std::wstring& path){
    if(skipped(path))return nullptr;
    ComPtr<IShellItemImageFactory> item;if(FAILED(SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(&item))))return nullptr;
    // Only a real picture: no generic music-note icon.
    HBITMAP bitmap=nullptr;if(FAILED(item->GetImage({256,256},SIIGBF_THUMBNAILONLY|SIIGBF_RESIZETOFIT,&bitmap))||!bitmap)return nullptr;
    struct Cleanup{HBITMAP b;~Cleanup(){DeleteObject(b);}} cleanup{bitmap};
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return nullptr;
    ComPtr<IWICBitmap> image;ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapIgnoreAlpha,&image))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(image.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    auto art=std::make_shared<Artwork>();if(FAILED(converter->GetSize(&art->width,&art->height))||!art->width||!art->height||art->width>512||art->height>512)return nullptr;
    art->pixels.resize(size_t(art->width)*art->height*4);if(FAILED(converter->CopyPixels(nullptr,art->width*4,UINT(art->pixels.size()),art->pixels.data())))return nullptr;
    // Thumbnails without alpha come back transparent; covers are opaque.
    for(size_t i=3;i<art->pixels.size();i+=4)art->pixels[i]=255;
    paintArtwork(*art);return art;
}
MusicLibrary::MusicLibrary(HWND notify,std::vector<std::wstring> roots):notify_(notify),roots_(std::move(roots)),tracks_(std::make_shared<const std::vector<LibraryTrack>>()){worker_=std::thread([this]{run();});}
MusicLibrary::~MusicLibrary(){{std::lock_guard lock(mutex_);stopping_=true;}wake_.notify_all();if(worker_.joinable())worker_.join();}
void MusicLibrary::scan(bool again){{std::lock_guard lock(mutex_);if(scanning_||scanWanted_||(scanned_&&!again))return;scanWanted_=true;}wake_.notify_all();}
bool MusicLibrary::scanning()const{std::lock_guard lock(mutex_);return scanning_||scanWanted_;}
bool MusicLibrary::scanned()const{std::lock_guard lock(mutex_);return scanned_;}
std::shared_ptr<const std::vector<LibraryTrack>> MusicLibrary::tracks()const{std::lock_guard lock(mutex_);return tracks_;}
std::shared_ptr<const Artwork> MusicLibrary::artwork(const std::wstring& path){
    std::lock_guard lock(mutex_);if(auto it=art_.find(path);it!=art_.end())return it->second;
    if(std::find(wanted_.begin(),wanted_.end(),path)==wanted_.end()){wanted_.push_back(path);if(wanted_.size()>24)wanted_.pop_front();wake_.notify_all();}
    return nullptr;
}
void MusicLibrary::run(){
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    for(;;){
        bool doScan=false;std::wstring picture;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stopping_||scanWanted_||!wanted_.empty();});if(stopping_)break;
            if(scanWanted_){scanWanted_=false;scanning_=true;doScan=true;}else{picture=wanted_.back();wanted_.pop_back();}}
        if(doScan){
            // Every song under the roots: links and cloud-only files are skipped, at most 8,000 songs, 12 folders deep.
            auto list=std::make_shared<std::vector<LibraryTrack>>();
            for(auto& root:roots_){std::error_code e;if(root.empty()||!fs::is_directory(root,e))continue;
                for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,e),end;!e&&it!=end&&list->size()<8000;it.increment(e)){
                    {std::lock_guard lock(mutex_);if(stopping_)break;}
                    const auto& entry=*it;std::error_code s;const auto st=entry.symlink_status(s);if(s)continue;
                    if(fs::is_directory(st)){if(skipped(entry.path())||it.depth()>=12)it.disable_recursion_pending();continue;}
                    if(!fs::is_regular_file(st)||!isAudioFile(entry.path().wstring())||skipped(entry.path()))continue;
                    list->push_back(read(entry.path().wstring()));}}
            sortLibrary(*list);
            {std::lock_guard lock(mutex_);tracks_=list;scanning_=false;scanned_=true;}
            if(notify_)PostMessageW(notify_,LibraryMessage,0,0);continue;}
        auto art=audioArtwork(picture);
        {std::lock_guard lock(mutex_);art_[picture]=art;artOrder_.push_back(picture);while(artOrder_.size()>64){art_.erase(artOrder_.front());artOrder_.pop_front();}}
        if(notify_)PostMessageW(notify_,LibraryMessage,1,0);
    }
    if(SUCCEEDED(com))CoUninitialize();
}
}
