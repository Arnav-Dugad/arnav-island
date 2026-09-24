#include "Island/IslandWindow.h"
#include "FileShelf/Zip.h"
#include "Media/Lyrics.h"
#include <shlobj.h>
#include <wincrypt.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <set>
#include <tuple>
// Phase 5C: capture (snip, text, colour), Shelf quick actions, the pinned Shelf, pinned copies
// and the clipboard picker. Heavy work (encoding, recognition, zipping) runs on worker threads
// and comes back as ShelfJobMessage.
namespace nexus {
namespace {
constexpr UINT ShelfJobMessage=WM_APP+35;
constexpr UINT_PTR PasteTimer=40;
constexpr int SnipHotkey=0x4e4a,TextHotkey=0x4e4b,ColourHotkey=0x4e4c,PickerHotkey=0x4e4d;
struct ShelfJob {enum Kind{Snip,CaptureText,FileText,Convert,Half,Zip} kind=Snip;std::wstring output,text,error;std::shared_ptr<const Artwork> preview;std::vector<uint8_t> dib;int width=0,height=0,items=0;uint64_t bytesIn=0,bytesOut=0;};
std::wstring knownFolder(const KNOWNFOLDERID& id){PWSTR p=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(id,KF_FLAG_CREATE,nullptr,&p))&&p)out=p;CoTaskMemFree(p);return out;}
std::wstring sizeText(uint64_t bytes){wchar_t b[32];if(bytes<1024)swprintf(b,32,L"%llu bytes",(unsigned long long)bytes);else if(bytes<1048576)swprintf(b,32,L"%.0f KB",bytes/1024.);else if(bytes<1073741824ull)swprintf(b,32,L"%.1f MB",bytes/1048576.);else swprintf(b,32,L"%.2f GB",bytes/1073741824.);return b;}
std::wstring lowerExtension(const std::wstring& path){auto e=std::filesystem::path(path).extension().wstring();for(auto& c:e)c=wchar_t(std::towlower(c));return e;}
bool imageExtension(const std::wstring& e){for(auto x:{L".png",L".jpg",L".jpeg",L".jfif",L".bmp",L".gif",L".tif",L".tiff",L".webp",L".heic",L".heif",L".ico",L".avif"})if(e==x)return true;return false;}
bool exists(const std::wstring& p){return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES;}
// ---- Images through WIC ------------------------------------------------------------------------
struct Pixels {int width=0,height=0;std::vector<uint8_t> bgra;};
ComPtr<IWICImagingFactory> wic(){ComPtr<IWICImagingFactory> f;CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f));return f;}
bool decode(const std::wstring& path,Pixels& out){
    auto f=wic();if(!f)return false;ComPtr<IWICBitmapDecoder> d;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> cv;UINT w=0,h=0;
    if(FAILED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&d))||FAILED(d->GetFrame(0,&frame))||FAILED(frame->GetSize(&w,&h))||!w||!h||w>20000||h>20000)return false;
    if(FAILED(f->CreateFormatConverter(&cv))||FAILED(cv->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    out.width=int(w);out.height=int(h);out.bgra.resize(size_t(w)*h*4);return SUCCEEDED(cv->CopyPixels(nullptr,w*4,UINT(out.bgra.size()),out.bgra.data()));
}
bool imageSize(const std::wstring& path,int& w,int& h){auto f=wic();ComPtr<IWICBitmapDecoder> d;ComPtr<IWICBitmapFrameDecode> frame;UINT uw=0,uh=0;if(!f||FAILED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&d))||FAILED(d->GetFrame(0,&frame))||FAILED(frame->GetSize(&uw,&uh)))return false;w=int(uw);h=int(uh);return true;}
// PNG keeps transparency; JPEG is flattened onto white at quality 0.92.
bool encode(const std::wstring& path,const Pixels& p,bool jpeg){
    auto f=wic();if(!f)return false;std::vector<uint8_t> data=p.bgra;
    if(jpeg)for(size_t i=0;i+3<data.size();i+=4){const unsigned a=data[i+3];for(int c=0;c<3;++c)data[i+c]=uint8_t((data[i+c]*a+255*(255-a))/255);data[i+3]=255;}
    ComPtr<IWICBitmap> bitmap;if(FAILED(f->CreateBitmapFromMemory(UINT(p.width),UINT(p.height),GUID_WICPixelFormat32bppBGRA,UINT(p.width*4),UINT(data.size()),data.data(),&bitmap)))return false;
    const auto temp=path+L".part";ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> e;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;
    if(FAILED(f->CreateStream(&stream))||FAILED(stream->InitializeFromFilename(temp.c_str(),GENERIC_WRITE))||FAILED(f->CreateEncoder(jpeg?GUID_ContainerFormatJpeg:GUID_ContainerFormatPng,nullptr,&e))||FAILED(e->Initialize(stream.Get(),WICBitmapEncoderNoCache))||FAILED(e->CreateNewFrame(&frame,&options)))return false;
    if(jpeg&&options){PROPBAG2 name{};name.pstrName=const_cast<LPOLESTR>(L"ImageQuality");VARIANT v;VariantInit(&v);v.vt=VT_R4;v.fltVal=.92f;options->Write(1,&name,&v);}
    WICPixelFormatGUID format=jpeg?GUID_WICPixelFormat24bppBGR:GUID_WICPixelFormat32bppBGRA;
    ComPtr<IWICFormatConverter> cv;if(FAILED(frame->Initialize(options.Get()))||FAILED(frame->SetSize(UINT(p.width),UINT(p.height)))||FAILED(frame->SetPixelFormat(&format))||FAILED(f->CreateFormatConverter(&cv))||FAILED(cv->Initialize(bitmap.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||
       FAILED(frame->WriteSource(cv.Get(),nullptr))||FAILED(frame->Commit())||FAILED(e->Commit())){cv.Reset();frame.Reset();e.Reset();stream.Reset();DeleteFileW(temp.c_str());return false;}
    // The encoder holds the stream (and the file) until it is released, so release it before renaming.
    cv.Reset();frame.Reset();options.Reset();e.Reset();stream.Reset();
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temp.c_str());return false;}return true;
}
Pixels halve(const Pixels& p){Pixels o;o.width=std::max(1,p.width/2);o.height=std::max(1,p.height/2);o.bgra.resize(size_t(o.width)*o.height*4);
    for(int y=0;y<o.height;++y)for(int x=0;x<o.width;++x)for(int c=0;c<4;++c){unsigned s=0;int n=0;for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const int sx=std::min(p.width-1,x*2+dx),sy=std::min(p.height-1,y*2+dy);s+=p.bgra[(size_t(sy)*p.width+sx)*4+c];++n;}o.bgra[(size_t(y)*o.width+x)*4+c]=uint8_t(s/n);}
    return o;}
// A small premultiplied preview (longest side `side`) for the Shelf and cards.
std::shared_ptr<const Artwork> thumbnail(const Pixels& p,int side){
    const double s=std::min(1.,double(side)/std::max(p.width,p.height));auto a=std::make_shared<Artwork>();a->width=uint32_t(std::max(1,int(p.width*s)));a->height=uint32_t(std::max(1,int(p.height*s)));a->pixels.resize(size_t(a->width)*a->height*4);
    for(uint32_t y=0;y<a->height;++y)for(uint32_t x=0;x<a->width;++x){const int x0=int(x/s),x1=std::max(x0+1,int((x+1)/s)),y0=int(y/s),y1=std::max(y0+1,int((y+1)/s));unsigned sum[4]{};int n=0;
        for(int sy=y0;sy<std::min(y1,p.height);++sy)for(int sx=x0;sx<std::min(x1,p.width);++sx){const uint8_t* q=&p.bgra[(size_t(sy)*p.width+sx)*4];for(int c=0;c<4;++c)sum[c]+=q[c];++n;}
        if(!n)n=1;const unsigned alpha=sum[3]/n;uint8_t* d=&a->pixels[(size_t(y)*a->width+x)*4];for(int c=0;c<3;++c)d[c]=uint8_t(sum[c]/n*alpha/255);d[3]=uint8_t(alpha);}
    return a;
}
// A 32-bit bottom-up DIB for the clipboard.
std::vector<uint8_t> dibOf(const Pixels& p){BITMAPINFOHEADER h{};h.biSize=sizeof(h);h.biWidth=p.width;h.biHeight=p.height;h.biPlanes=1;h.biBitCount=32;h.biCompression=BI_RGB;h.biSizeImage=DWORD(p.width*p.height*4);
    std::vector<uint8_t> out(sizeof(h)+size_t(h.biSizeImage));memcpy(out.data(),&h,sizeof(h));for(int y=0;y<p.height;++y)memcpy(&out[sizeof(h)+size_t(p.height-1-y)*p.width*4],&p.bgra[size_t(y)*p.width*4],size_t(p.width)*4);return out;}
// ---- ZIP to a file -------------------------------------------------------------------------------------
struct FileSink:zip::Sink{HANDLE h;explicit FileSink(HANDLE f):h(f){}
    bool write(const uint8_t* p,size_t n)override{while(n){DWORD done=0;if(!WriteFile(h,p,DWORD(std::min<size_t>(n,1u<<30)),&done,nullptr)||!done)return false;p+=done;n-=done;}return true;}
    uint64_t tell()override{LARGE_INTEGER z{},at{};SetFilePointerEx(h,z,&at,FILE_CURRENT);return uint64_t(at.QuadPart);}
    bool patch(uint64_t at,const uint8_t* p,size_t n)override{const uint64_t back=tell();LARGE_INTEGER to{};to.QuadPart=LONGLONG(at);if(!SetFilePointerEx(h,to,nullptr,FILE_BEGIN)||!write(p,n))return false;to.QuadPart=LONGLONG(back);return SetFilePointerEx(h,to,nullptr,FILE_BEGIN)!=FALSE;}};
void dosStamp(const std::wstring& path,uint16_t& time,uint16_t& date){WIN32_FILE_ATTRIBUTE_DATA a{};FILETIME local{};SYSTEMTIME t{};if(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&a)&&FileTimeToLocalFileTime(&a.ftLastWriteTime,&local)&&FileTimeToSystemTime(&local,&t)){time=zip::dosTime(t.wHour,t.wMinute,t.wSecond);date=zip::dosDate(t.wYear,t.wMonth,t.wDay);}else{time=0;date=zip::dosDate(1980,1,1);}}
bool zipItems(const std::vector<std::wstring>& items,const std::wstring& output,int& count,uint64_t& bytesIn,std::wstring& error){
    struct Source{std::wstring path;std::string name;};std::vector<Source> files;
    for(auto& item:items){std::filesystem::path top(item);std::error_code ec;
        if(std::filesystem::is_directory(top,ec)){for(auto it=std::filesystem::recursive_directory_iterator(top,std::filesystem::directory_options::skip_permission_denied,ec);!ec&&it!=std::filesystem::recursive_directory_iterator();it.increment(ec)){
                if(it->is_regular_file(ec)){auto rel=std::filesystem::relative(it->path(),top.parent_path(),ec);if(ec)continue;auto name=zip::entryName(toUtf8(rel.wstring()));if(!name.empty())files.push_back({it->path().wstring(),name});}if(files.size()>20000){error=L"Too many files to zip";return false;}}}
        else if(std::filesystem::is_regular_file(top,ec)){auto name=zip::entryName(toUtf8(top.filename().wstring()));if(!name.empty())files.push_back({item,name});}}
    if(files.empty()){error=L"Nothing to zip";return false;}
    // Same names from different folders get " (2)" before the extension.
    std::set<std::string> used;for(auto& f:files){std::string n=f.name;for(int i=2;used.contains(n)&&i<1000;++i){auto dot=f.name.find_last_of('.');n=dot==std::string::npos||dot<f.name.find_last_of('/')+1?f.name+" ("+std::to_string(i)+")":f.name.substr(0,dot)+" ("+std::to_string(i)+")"+f.name.substr(dot);}used.insert(n);f.name=n;}
    const auto temp=output+L".part";HANDLE out=CreateFileW(temp.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(out==INVALID_HANDLE_VALUE){error=L"Couldn't create the archive there";return false;}
    bool ok=true;{FileSink sink(out);zip::Writer w(sink);
        for(auto& f:files){HANDLE in=CreateFileW(f.path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(in==INVALID_HANDLE_VALUE)continue;uint16_t t=0,d=0;dosStamp(f.path,t,d);
            const bool added=w.add(f.name,t,d,[&](uint8_t* buf,size_t cap)->long long{DWORD n=0;if(!ReadFile(in,buf,DWORD(std::min<size_t>(cap,1u<<30)),&n,nullptr))return -1;bytesIn+=n;return (long long)n;});CloseHandle(in);
            if(!added){ok=false;error=L"The files are too large for a ZIP (4 GB)";break;}++count;}
        ok=ok&&count>0&&w.finish();if(ok==false&&error.empty())error=L"Couldn't write the archive";}
    CloseHandle(out);if(!ok){DeleteFileW(temp.c_str());return false;}
    if(!MoveFileExW(temp.c_str(),output.c_str(),0)){DeleteFileW(temp.c_str());error=L"Couldn't name the archive";return false;}return true;
}
// Pinned copies on disk, encrypted for this Windows account (DPAPI).
bool protect(const std::string& plain,std::string& out){DATA_BLOB in{DWORD(plain.size()),reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))},blob{};if(!CryptProtectData(&in,L"Arnav Island pinned copies",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&blob))return false;out.assign(reinterpret_cast<char*>(blob.pbData),blob.cbData);LocalFree(blob.pbData);return true;}
bool unprotect(const std::string& cipher,std::string& out){DATA_BLOB in{DWORD(cipher.size()),reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data()))},blob{};if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&blob))return false;out.assign(reinterpret_cast<char*>(blob.pbData),blob.cbData);LocalFree(blob.pbData);return true;}
void postJob(HWND window,ShelfJob* job){if(!PostMessageW(window,ShelfJobMessage,0,reinterpret_cast<LPARAM>(job)))delete job;}
}
// ---- Shortcuts ---------------------------------------------------------------------------------------
void IslandWindow::syncCaptureHotkeys(){
    const bool want=settings_.captureShortcuts&&!testing_;if(want==captureHotkeys_)return;captureHotkeys_=want;
    for(int id:{SnipHotkey,TextHotkey,ColourHotkey,PickerHotkey})UnregisterHotKey(window_,id);captureTaken_.clear();if(!want)return;
    for(auto [id,key,name]:{std::tuple{SnipHotkey,'S',L"Alt+Shift+S"},std::tuple{TextHotkey,'T',L"Alt+Shift+T"},std::tuple{ColourHotkey,'C',L"Alt+Shift+C"},std::tuple{PickerHotkey,'V',L"Alt+Shift+V"}})
        if(!RegisterHotKey(window_,id,MOD_ALT|MOD_SHIFT|MOD_NOREPEAT,UINT(key))){if(!captureTaken_.empty())captureTaken_+=L", ";captureTaken_+=name;}
    if(!captureTaken_.empty())store_.log("Warning","capture_shortcut_taken");
}
// ---- Capture -----------------------------------------------------------------------------------------
void IslandWindow::startCapture(CaptureMode mode){
    if(captureOverlay::active())return;
    if(content_.command.active)closeCommand(false);
    if(state_!=IslandState::Compact){content_.pinned=false;transition(IslandState::Compact);}
    const uint32_t accent=islandAccent(settings_.accent,false,content_.platform.wallpaper,nullptr,false);
    if(!captureOverlay::begin(instance_,window_,mode,accent,motion_.reduced)){captureCard(10,L"Couldn't capture the screen",L"Windows refused the screen copy");return;}
    store_.log("Info",mode==CaptureMode::Snip?"capture_snip":mode==CaptureMode::Text?"capture_text":"capture_colour");
}
void IslandWindow::captureCard(int kind,std::wstring title,std::wstring detail,std::shared_ptr<const Artwork> icon,uint32_t colour){
    if(!renderer_)return;if(state_!=IslandState::Compact&&state_!=IslandState::Notification){content_.shelfStatus=title;content_.shelfStatusUntil=seconds()+3;refresh();return;}
    content_.notice={kind,{},std::move(title),std::move(icon),false,std::move(detail),colour};
    events_.publish({ActivityKind::Notification,"capture",65,double(kind),2.2,kind==11?5:3.2},seconds());transition(IslandState::Notification);presentActivity();
}
// Puts text on the clipboard (the island owns the copy) and keeps it in history when that is on.
void IslandWindow::copyText(const std::wstring& text,bool keep){
    ClipEntry e;e.kind=isLink(text)?ClipEntry::Kind::Link:ClipEntry::Kind::Text;e.text=text;e.source=L"Arnav Island";
    if(!clipboard_.copy(e)){content_.shelfStatus=L"The clipboard is busy; try again";content_.shelfStatusUntil=seconds()+2.5;return;}
    if(keep&&settings_.clipboardHistory&&!clips_.paused){clips_.add(std::move(e),seconds());clipViews();}
}
void IslandWindow::captureDone(CaptureResult* raw){
    std::unique_ptr<CaptureResult> r(raw);if(!r){store_.log("Info","capture_cancelled");return;}
    if(r->mode==CaptureMode::Colour){const auto text=r->rgb?rgbText(r->colour):hexColor(r->colour);copyText(text);captureCard(9,hexColor(r->colour),r->rgb?L"Copied as "+rgbText(r->colour):L"Copied  ·  "+rgbText(r->colour),{},r->colour);return;}
    if(r->mode==CaptureMode::Text){captureCard(10,L"Reading text…",std::to_wstring(r->width)+L" × "+std::to_wstring(r->height));
        std::thread([window=window_,r=std::shared_ptr<CaptureResult>(std::move(r))]{auto job=new ShelfJob;job->kind=ShelfJob::CaptureText;auto text=recognizeText(r->pixels.data(),r->width,r->height);if(!text)job->error=L"Text recognition isn't available";else{job->text=text->text;job->output=text->language;}postJob(window,job);}).detach();return;}
    // Snip: saved as PNG in Pictures\Screenshots, put on the Shelf and on the clipboard.
    jobRunning_=true;content_.shelfBusy=true;
    std::thread([window=window_,r=std::shared_ptr<CaptureResult>(std::move(r))]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto job=new ShelfJob;job->kind=ShelfJob::Snip;Pixels p;p.width=r->width;p.height=r->height;p.bgra=r->pixels;job->width=p.width;job->height=p.height;
        auto folder=knownFolder(FOLDERID_Screenshots);if(folder.empty())folder=knownFolder(FOLDERID_Pictures);std::error_code ec;std::filesystem::create_directories(folder,ec);
        SYSTEMTIME t{};GetLocalTime(&t);std::wstring path;for(int copy=1;copy<100;++copy){path=(std::filesystem::path(folder)/snipName(t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,copy)).wstring();if(!exists(path))break;}
        if(!encode(path,p,false))job->error=L"Couldn't save the snip";else{job->output=path;job->preview=thumbnail(p,128);job->dib=dibOf(p);}
        CoUninitialize();postJob(window,job);}).detach();
}
// ---- Shelf item view and actions ----------------------------------------------------------------------
void IslandWindow::openShelfItem(int index){
    if(index<0||size_t(index)>=content_.shelf.size())return;const auto& item=content_.shelf[size_t(index)];auto& info=content_.shelfInfo;info={};
    if(item.kind==ShelfItem::Kind::File){WIN32_FILE_ATTRIBUTE_DATA a{};if(GetFileAttributesExW(item.value.c_str(),GetFileExInfoStandard,&a)){info.directory=(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;if(!info.directory)info.size=sizeText((uint64_t(a.nFileSizeHigh)<<32)|a.nFileSizeLow);}else info.kind=L"Missing";
        if(info.kind.empty()){SHFILEINFOW sfi{};if(SHGetFileInfoW(item.value.c_str(),0,&sfi,sizeof(sfi),SHGFI_TYPENAME))info.kind=sfi.szTypeName;if(info.kind.empty())info.kind=info.directory?L"Folder":L"File";}
        info.extension=lowerExtension(item.value);info.image=!info.directory&&imageExtension(info.extension)&&imageSize(item.value,info.width,info.height);info.folder=std::filesystem::path(item.value).parent_path().wstring();}
    const double now=seconds();if(!motion_.reduced){motion_.swipe.reset(14,now);motion_.swipe.retarget(0,now,MotionTokens::content);}
    content_.shelfDetail=index;content_.shelfStatus.clear();refresh();animate();
}
void IslandWindow::shelfAction(Action a){
    const double now=seconds();const int index=content_.shelfDetail;const bool detail=index>=0&&size_t(index)<content_.shelf.size();
    auto status=[&](std::wstring text,bool busy=false){content_.shelfStatus=std::move(text);content_.shelfBusy=busy;content_.shelfStatusUntil=now+(busy?600:3);refresh();};
    if(a==Action::ShelfBack){content_.shelfDetail=-1;if(!motion_.reduced){motion_.swipe.reset(-14,now);motion_.swipe.retarget(0,now,MotionTokens::content);}refresh();animate();return;}
    if(a==Action::ShelfZip){if(jobRunning_)return;std::vector<std::wstring> items;if(detail){if(content_.shelf[size_t(index)].kind==ShelfItem::Kind::File)items.push_back(content_.shelf[size_t(index)].value);}else for(auto& i:content_.shelf)if(i.kind==ShelfItem::Kind::File)items.push_back(i.value);
        if(items.empty())return;jobRunning_=true;status(L"Zipping "+std::to_wstring(items.size())+(items.size()==1?L" item…":L" items…"),true);
        // One item: "Report.zip" beside it; several: "Shelf 2026-09-24 0915.zip" beside the first.
        std::wstring folder=std::filesystem::path(items.front()).parent_path().wstring(),name;
        if(items.size()==1)name=std::filesystem::path(items.front()).filename().replace_extension().wstring();else{SYSTEMTIME t{};GetLocalTime(&t);wchar_t stem[64];swprintf(stem,64,L"Shelf %04d-%02d-%02d %02d%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute);name=stem;}
        std::thread([window=window_,items,folder,name]{auto job=new ShelfJob;job->kind=ShelfJob::Zip;auto target=[&](const std::wstring& dir){return (std::filesystem::path(dir)/freeName(name,L".zip",[&](const std::wstring& n){return exists((std::filesystem::path(dir)/n).wstring());})).wstring();};
            std::wstring out=target(folder);int count=0;uint64_t in=0;std::wstring error;bool ok=zipItems(items,out,count,in,error);
            if(!ok&&error==L"Couldn't create the archive there"){auto docs=knownFolder(FOLDERID_Documents);out=target(docs);count=0;in=0;error.clear();ok=zipItems(items,out,count,in,error);}
            if(ok){job->output=out;job->items=count;job->bytesIn=in;WIN32_FILE_ATTRIBUTE_DATA a{};if(GetFileAttributesExW(out.c_str(),GetFileExInfoStandard,&a))job->bytesOut=(uint64_t(a.nFileSizeHigh)<<32)|a.nFileSizeLow;}else job->error=error;
            postJob(window,job);}).detach();return;}
    if(!detail)return;const auto item=content_.shelf[size_t(index)];const bool file=item.kind==ShelfItem::Kind::File;
    switch(a){
    case Action::ShelfOpen:if(file&&reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",item.value.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)status(L"Windows couldn't open it");return;
    case Action::ShelfOpenWith:if(file){OPENASINFO o{item.value.c_str(),nullptr,OAIF_EXEC|OAIF_ALLOW_REGISTRATION};SHOpenWithDialog(nullptr,&o);}return;
    case Action::ShelfReveal:if(file){if(auto* pidl=ILCreateFromPathW(item.value.c_str())){SHOpenFolderAndSelectItems(pidl,0,nullptr,0);ILFree(pidl);}}return;
    case Action::ShelfCopyPath:copyText(item.value);status(file?L"Path copied":L"Text copied");return;
    case Action::ShelfRemove:content_.shelf.erase(content_.shelf.begin()+index);content_.shelfDetail=-1;content_.shelfOffset=std::clamp(content_.shelfOffset,0,std::max(0,int(content_.shelf.size())-4));requestPreviews();if(!motion_.reduced){motion_.swipe.reset(-14,now);motion_.swipe.retarget(0,now,MotionTokens::content);}refresh();animate();return;
    case Action::ShelfCopyText:case Action::ShelfConvert:case Action::ShelfHalf:{
        if(!file||jobRunning_||!content_.shelfInfo.image)return;jobRunning_=true;const int kind=a==Action::ShelfCopyText?ShelfJob::FileText:a==Action::ShelfConvert?ShelfJob::Convert:ShelfJob::Half;
        const bool toJpeg=content_.shelfInfo.extension==L".png";status(a==Action::ShelfCopyText?L"Reading text…":a==Action::ShelfConvert?(toJpeg?L"Saving a JPG…":L"Saving a PNG…"):L"Saving a half-size copy…",true);
        std::thread([window=window_,path=item.value,kind,toJpeg]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto job=new ShelfJob;job->kind=ShelfJob::Kind(kind);Pixels p;
            if(!decode(path,p))job->error=L"Windows couldn't read this image";
            else if(kind==ShelfJob::FileText){auto text=recognizeText(p.bgra.data(),p.width,p.height);if(!text)job->error=L"Text recognition isn't available";else job->text=text->text;}
            else{const auto source=std::filesystem::path(path);const bool jpegOut=kind==ShelfJob::Convert?toJpeg:(lowerExtension(path)==L".jpg"||lowerExtension(path)==L".jpeg"||lowerExtension(path)==L".jfif");
                Pixels out=kind==ShelfJob::Half?halve(p):p;const std::wstring stem=source.stem().wstring()+(kind==ShelfJob::Half?L" (half size)":L""),ext=jpegOut?L".jpg":L".png";
                const auto name=freeName(stem,ext,[&](const std::wstring& n){return exists((source.parent_path()/n).wstring());});auto target=(source.parent_path()/name).wstring();
                if(!encode(target,out,jpegOut)){auto docs=std::filesystem::path(knownFolder(FOLDERID_Documents));target=(docs/freeName(stem,ext,[&](const std::wstring& n){return exists((docs/n).wstring());})).wstring();if(!encode(target,out,jpegOut))target.clear();}
                if(target.empty())job->error=L"Couldn't save the new image";else{job->output=target;job->preview=thumbnail(out,128);job->width=out.width;job->height=out.height;}}
            CoUninitialize();postJob(window,job);}).detach();return;}
    default:return;
    }
}
// ---- Pinned Shelf and pinned copies ---------------------------------------------------------------------
void IslandWindow::shelfChanged(){
    if(testing_)return;const auto path=store_.directory/L"shelf.nexus";
    if(!settings_.pinnedShelf){std::error_code ec;std::filesystem::remove(path,ec);return;}
    const std::string data=toUtf8(saveShelf(content_.shelf));store_.submit([path,data]{auto temp=path;temp+=L".tmp";{std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<data;}MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);});
}
void IslandWindow::loadShelfFile(){
    if(testing_||!settings_.pinnedShelf)return;std::ifstream f(store_.directory/L"shelf.nexus",std::ios::binary);if(!f)return;std::stringstream text;text<<f.rdbuf();
    auto items=loadShelf(fromUtf8(text.str()),[](const std::wstring& p){return exists(p);});if(items.empty())return;content_.shelf=std::move(items);requestPreviews();
}
void IslandWindow::savePinnedClips(){
    if(testing_)return;const auto path=store_.directory/L"clips-pinned.nexus";std::error_code ec;
    if(!settings_.clipboardHistory||clips_.pinned()==0){std::filesystem::remove(path,ec);return;}
    std::string cipher;if(!protect(toUtf8(savePins(clips_.entries())),cipher))return;
    store_.submit([path,cipher]{auto temp=path;temp+=L".tmp";{std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<cipher;}MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);});
}
void IslandWindow::loadPinnedClips(){
    if(testing_||!settings_.clipboardHistory)return;std::ifstream f(store_.directory/L"clips-pinned.nexus",std::ios::binary);if(!f)return;std::stringstream data;data<<f.rdbuf();
    std::string plain;if(!unprotect(data.str(),plain))return;clips_.restore(loadPins(fromUtf8(plain)),seconds());clipViews();
}
// ---- Clipboard search and picker ---------------------------------------------------------------------
// The command bar in clipboard mode: pinned copies first, then the newest, filtered by what is typed.
void IslandWindow::clipResults(){
    auto& c=content_.command;c.results.clear();c.icons.clear();ids_.clear();const double now=seconds();
    std::vector<const ClipEntry*> order;for(auto& e:clips_.entries())if(e.pinned)order.push_back(&e);for(auto& e:clips_.entries())if(!e.pinned)order.push_back(&e);
    for(auto* e:order){if(c.results.size()>=5)break;if(!clipMatches(*e,c.text))continue;CommandResult r;r.kind=CommandKind::ClipPaste;
        const bool secret=settings_.hideSecrets&&(e->kind==ClipEntry::Kind::Text)&&looksSecret(e->text);
        switch(e->kind){case ClipEntry::Kind::Image:r.title=L"Image  ·  "+std::to_wstring(e->imageWidth)+L" × "+std::to_wstring(e->imageHeight);break;
            case ClipEntry::Kind::Files:{auto& f=e->files.front();auto slash=f.find_last_of(L"\\/");r.title=slash==std::wstring::npos?f:f.substr(slash+1);if(e->files.size()>1)r.title+=L" and "+std::to_wstring(e->files.size()-1)+L" more";break;}
            default:r.title=secret?std::wstring(L"••••••••  (hidden)"):clipPreview(e->text,90);}
        r.detail=(e->pinned?std::wstring(L"Pinned  ·  "):std::wstring())+(e->source.empty()?std::wstring(L"Copied"):e->source)+L"  ·  "+ageText(now-e->time);
        c.results.push_back(std::move(r));c.icons.push_back(e->thumbnail?e->thumbnail:e->sourceIcon);ids_.push_back(e->id);}
    c.selected=std::clamp(c.selected,0,std::max(0,int(c.results.size())-1));c.armed=false;
    const int rows=c.results.empty()?1:int(std::min<size_t>(5,c.results.size()));const double height=commandIslandHeight(rows);if(std::abs(motion_.commandHeight-height)>.5){motion_.commandHeight=height;animate();}
    refresh();commandSelect(c.selected);
}
void IslandWindow::clipSearch(bool picker){
    if(!content_.command.active)openCommand();auto& c=content_.command;c.clips=true;c.paste=picker;c.text.clear();c.caret=0;c.status.clear();c.selected=0;clipResults();
}
// Enter pastes into the app that was in front (the picker) or just copies (search from the Shelf).
void IslandWindow::pasteClip(size_t row,bool copyOnly){
    if(row>=ids_.size())return;const ClipEntry* e=clips_.find(ids_[row]);if(!e){commandShake();return;}ClipEntry copy=*e;
    if(!clipboard_.copy(copy)){commandStatus(L"The clipboard is busy; try again",true,false);return;}
    clips_.add(std::move(copy),seconds());clipViews();const bool paste=content_.command.paste&&!copyOnly;
    if(!paste){commandStatus(L"Copied");return;}
    closeCommand(true);SetTimer(window_,PasteTimer,90,nullptr);store_.log("Info","clipboard_pasted");
}
// Handles the Phase 5C window messages; returns true when one was handled.
bool IslandWindow::captureMessage(UINT m,WPARAM w,LPARAM l,LRESULT& result){
    switch(m){
    case WM_HOTKEY:{const int id=int(w);if(id!=SnipHotkey&&id!=TextHotkey&&id!=ColourHotkey&&id!=PickerHotkey)return false;result=0;
        if(id==PickerHotkey){if(content_.command.active&&content_.command.clips)closeCommand();else clipSearch(true);return true;}
        startCapture(id==SnipHotkey?CaptureMode::Snip:id==TextHotkey?CaptureMode::Text:CaptureMode::Colour);return true;}
    case CaptureMessage:captureDone(reinterpret_cast<CaptureResult*>(l));result=0;return true;
    case WM_TIMER:if(w==42){KillTimer(window_,42);if(qaOverlay_>=0)startCapture(CaptureMode(qaOverlay_));result=0;return true;}// QA: overlay after the backdrop is on screen
        if(w!=PasteTimer)return false;KillTimer(window_,PasteTimer);{
            // Ctrl+V into the app that is in front again; a held Shift is released first so it isn't Ctrl+Shift+V.
            std::vector<INPUT> keys;auto key=[&](WORD vk,bool up){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=vk;i.ki.dwFlags=up?KEYEVENTF_KEYUP:0;keys.push_back(i);};
            for(WORD vk:{VK_SHIFT,VK_MENU,VK_LWIN})if(GetAsyncKeyState(vk)&0x8000)key(vk,true);key(VK_CONTROL,false);key('V',false);key('V',true);key(VK_CONTROL,true);SendInput(UINT(keys.size()),keys.data(),sizeof(INPUT));}
        result=0;return true;
    case ShelfJobMessage:{std::unique_ptr<ShelfJob> job(reinterpret_cast<ShelfJob*>(l));result=0;const double now=seconds();
        auto status=[&](std::wstring text){content_.shelfStatus=std::move(text);content_.shelfBusy=false;content_.shelfStatusUntil=now+3.5;};
        auto addToShelf=[&](const std::wstring& path,std::shared_ptr<const Artwork> preview){if(std::any_of(content_.shelf.begin(),content_.shelf.end(),[&](auto& i){return i.value==path;}))return;if(content_.shelf.size()>=32)content_.shelf.erase(content_.shelf.begin());
            ShelfItem item{ShelfItem::Kind::File,path,std::filesystem::path(path).filename().wstring(),std::move(preview)};content_.shelf.push_back(std::move(item));requestPreviews();};
        switch(job->kind){
        case ShelfJob::CaptureText:{
            if(!job->error.empty()){captureCard(10,L"Couldn't read text",job->error);break;}
            if(job->text.empty()){captureCard(10,L"No text found",L"Try a larger area, or text with more contrast");break;}
            copyText(job->text);const int words=wordCount(job->text);auto first=job->text.substr(0,job->text.find(L'\n'));
            captureCard(10,L"Copied "+std::to_wstring(words)+(words==1?L" word":L" words"),clipPreview(first,80));break;}
        case ShelfJob::Snip:{jobRunning_=false;content_.shelfBusy=false;
            if(!job->error.empty()){captureCard(11,job->error,L"Pictures › Screenshots");break;}
            addToShelf(job->output,job->preview);ClipEntry image;image.kind=ClipEntry::Kind::Image;image.dib=std::move(job->dib);image.imageWidth=uint32_t(job->width);image.imageHeight=uint32_t(job->height);image.thumbnail=job->preview;image.source=L"Arnav Island";
            clipboard_.copy(image);if(settings_.clipboardHistory&&!clips_.paused){clips_.add(std::move(image),now);clipViews();}
            captureCard(11,L"Snip saved to the Shelf",std::to_wstring(job->width)+L" × "+std::to_wstring(job->height)+L"  ·  also copied",job->preview);store_.log("Info","snip_saved");break;}
        case ShelfJob::FileText:{jobRunning_=false;if(!job->error.empty()){status(job->error);break;}if(job->text.empty()){status(L"No text found in this image");break;}copyText(job->text);const int words=wordCount(job->text);status(L"Copied "+std::to_wstring(words)+(words==1?L" word":L" words"));break;}
        case ShelfJob::Convert:case ShelfJob::Half:{jobRunning_=false;if(!job->error.empty()){status(job->error);break;}addToShelf(job->output,job->preview);status(L"Saved "+std::filesystem::path(job->output).filename().wstring());break;}
        case ShelfJob::Zip:{jobRunning_=false;if(!job->error.empty()){status(job->error);break;}addToShelf(job->output,nullptr);
            status(L"Zipped "+std::to_wstring(job->items)+(job->items==1?L" file":L" files")+L"  ·  "+sizeText(job->bytesIn)+L" → "+sizeText(job->bytesOut));store_.log("Info","shelf_zipped");break;}
        }
        refresh();animate();return true;}
    default:return false;
    }
}
// QA: a full-screen painted "desktop" (soft gradient, a document window and colour swatches) so
// captures of the overlay never show the real screen.
void IslandWindow::qaBackdrop(){
    static bool registered=false;if(!registered){WNDCLASSW wc{};wc.hInstance=instance_;wc.lpszClassName=L"ArnavIsland.QABackdrop";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        wc.lpfnWndProc=[](HWND h,UINT m,WPARAM w,LPARAM l)->LRESULT{if(m!=WM_PAINT)return DefWindowProcW(h,m,w,l);PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
            for(int y=0;y<r.bottom;y+=4){const int t=y*255/std::max<LONG>(1,r.bottom);HBRUSH b=CreateSolidBrush(RGB(40+t/6,70+t/5,120+t/4));RECT band{0,y,r.right,y+4};FillRect(dc,&band,b);DeleteObject(b);}
            RECT doc{r.right/2-360,r.bottom/2-190,r.right/2+300,r.bottom/2+170};HBRUSH white=CreateSolidBrush(RGB(250,250,252));FillRect(dc,&doc,white);DeleteObject(white);
            RECT bar{doc.left,doc.top,doc.right,doc.top+34};HBRUSH chrome=CreateSolidBrush(RGB(232,234,238));FillRect(dc,&bar,chrome);DeleteObject(chrome);
            SetBkMode(dc,TRANSPARENT);HFONT title=CreateFontW(34,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI"),body=CreateFontW(24,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
            HGDIOBJ old=SelectObject(dc,title);SetTextColor(dc,RGB(24,26,32));TextOutW(dc,doc.left+36,doc.top+62,L"Quarterly review",16);SelectObject(dc,body);SetTextColor(dc,RGB(70,74,84));
            const wchar_t* lines[]={L"Launch on Thursday with the new island.",L"Owners: design, audio and capture.",L"Budget and timeline are on track."};for(int i=0;i<3;++i)TextOutW(dc,doc.left+36,doc.top+120+i*38,lines[i],int(wcslen(lines[i])));
            const COLORREF swatch[]={RGB(0x3A,0x7B,0xD5),RGB(0xE0,0x6C,0x4F),RGB(0x5F,0xD9,0x8A),RGB(0xF2,0xC1,0x4E)};for(int i=0;i<4;++i){RECT s{doc.left+36+i*120,doc.bottom-96,doc.left+136+i*120,doc.bottom-36};HBRUSH b=CreateSolidBrush(swatch[i]);FillRect(dc,&s,b);DeleteObject(b);}
            SelectObject(dc,old);DeleteObject(title);DeleteObject(body);EndPaint(h,&ps);return 0;};
        registered=RegisterClassW(&wc)!=0;}
    MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTOPRIMARY),&mi);const RECT& r=mi.rcMonitor;
    HWND backdrop=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE,L"ArnavIsland.QABackdrop",L"QA backdrop",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance_,nullptr);
    ShowWindow(backdrop,SW_SHOWNOACTIVATE);UpdateWindow(backdrop);SetWindowPos(window_,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    // The overlay freezes the screen next: wait until Windows has composed the backdrop.
    DwmFlush();DwmFlush();
}
}
