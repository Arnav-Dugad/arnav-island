#include "LyricsService.h"
#include "App/Version.h"
#include <winhttp.h>
#include <chrono>
#include <fstream>
#include <sstream>
namespace nexus {
namespace {
int64_t unixNow(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
double monotonic(){return double(GetTickCount64())/1000.;}
struct Internet {HINTERNET h=nullptr;~Internet(){if(h)WinHttpCloseHandle(h);}};
}
LyricsService::LyricsService(HWND window,std::filesystem::path directory):window_(window),directory_(std::move(directory)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)){
    if(!wake_||!stop_)throw std::runtime_error("Lyrics event creation failed");worker_=std::thread([this]{run();});
}
LyricsService::~LyricsService(){SetEvent(stop_);
    // Closing the open request makes a blocking WinHTTP call return at once.
    if(void* h=active_.exchange(nullptr))WinHttpCloseHandle(h);
    if(worker_.joinable())worker_.join();CloseHandle(wake_);CloseHandle(stop_);}
void LyricsService::request(const Track& track){
    if(track.title.empty()||!(track.duration>=0))return;const auto k=key(track);
    {std::lock_guard lock(mutex_);for(auto& [rk,r]:results_)if(rk==k&&(r.state!=State::Offline||monotonic()<offlineUntil_))return;
        pending_=track;for(auto it=results_.begin();it!=results_.end();++it)if(it->first==k){results_.erase(it);break;}results_.push_front({k,{State::Loading,nullptr}});}
    SetEvent(wake_);
}
LyricsService::Result LyricsService::get(const std::wstring& k){std::lock_guard lock(mutex_);for(auto& [rk,r]:results_)if(rk==k)return r;return {};}
void LyricsService::store(const std::wstring& k,Result r){
    std::lock_guard lock(mutex_);for(auto it=results_.begin();it!=results_.end();++it)if(it->first==k){results_.erase(it);break;}
    results_.push_front({k,std::move(r)});while(results_.size()>64)results_.pop_back();
}
void LyricsService::clear(){
    {std::lock_guard lock(mutex_);results_.clear();pending_.reset();}
    std::error_code ignored;std::filesystem::remove_all(directory_,ignored);
}
void LyricsService::run(){
    for(;;){HANDLE events[]={stop_,wake_};if(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0)return;
        for(;;){std::optional<Track> track;{std::lock_guard lock(mutex_);track.swap(pending_);}if(!track)break;
            Result r;try{r=lookup(*track);}catch(...){r={State::Offline,nullptr};}
            if(WaitForSingleObject(stop_,0)==WAIT_OBJECT_0)return;
            store(key(*track),r);PostMessageW(window_,LyricsMessage,0,0);}}
}
std::optional<std::string> LyricsService::fetch(const std::string& path,bool& reachable){
    reachable=false;
    const std::wstring agent=std::wstring(L"ArnavIsland/")+appVersion+L" (https://github.com/Arnav-Dugad/arnav-island)";
    Internet session{WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h)session.h=WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session.h)return std::nullopt;
    WinHttpSetTimeouts(session.h,5000,5000,6000,6000);
    DWORD decompress=WINHTTP_DECOMPRESSION_FLAG_ALL;WinHttpSetOption(session.h,WINHTTP_OPTION_DECOMPRESSION,&decompress,sizeof(decompress));
    Internet connection{WinHttpConnect(session.h,L"lrclib.net",INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.h)return std::nullopt;
    const std::wstring target=fromUtf8(path);
    Internet request{WinHttpOpenRequest(connection.h,L"GET",target.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.h)return std::nullopt;
    // Whoever takes the handle back closes it: this call normally, the destructor when quitting.
    // Published first, then the stop checked, so a quit can never miss an open request.
    active_=request.h;struct Release{std::atomic<void*>& slot;Internet& r;~Release(){if(slot.exchange(nullptr)!=r.h)r.h=nullptr;}} release{active_,request};
    if(WaitForSingleObject(stop_,0)==WAIT_OBJECT_0)return std::nullopt;
    if(!WinHttpSendRequest(request.h,L"Accept: application/json\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))return std::nullopt;
    reachable=true;
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return std::nullopt;
    std::string body;for(;;){DWORD available=0;if(!WinHttpQueryDataAvailable(request.h,&available))return std::nullopt;if(!available)break;
        if(body.size()+available>(4u<<20))return std::nullopt;size_t at=body.size();body.resize(at+available);DWORD read=0;if(!WinHttpReadData(request.h,body.data()+at,available,&read))return std::nullopt;body.resize(at+read);if(!read)break;}
    return body;
}
LyricsService::Result LyricsService::lookup(const Track& t){
    const LyricsQuery q=cleanLyricsQuery(t.title,t.artist,t.browser);if(q.title.empty())return {State::Missing,nullptr};
    const auto file=directory_/lyricsCacheName(q,t.duration);const int64_t now=unixNow();
    auto result=[](const LyricsCacheEntry& e)->Result{
        if(e.status==LyricsCacheEntry::Status::Found){auto lines=std::make_shared<std::vector<LyricLine>>(parseLrc(e.lrc));return lines->empty()?Result{State::Missing,nullptr}:Result{State::Found,lines};}
        return {e.status==LyricsCacheEntry::Status::Instrumental?State::Instrumental:State::Missing,nullptr};};
    {std::ifstream in(file,std::ios::binary);if(in){std::stringstream text;text<<in.rdbuf();if(auto cached=readLyricsCache(text.str());cached&&lyricsCacheFresh(*cached,now))return result(*cached);}}
    // Ask LRCLIB: the full credit first, then the first-named artist.
    LyricsResult found;bool answered=false;
    for(const auto& artist:{q.artist,primaryArtist(q.artist)}){
        if(answered&&artist==q.artist)continue;bool reachable=false;auto body=fetch(lyricsSearchPath({q.title,artist}),reachable);
        if(!body){if(!answered){offlineUntil_=monotonic()+60;return {State::Offline,nullptr};}break;}
        auto json=Json::parse(*body);if(!json){if(!answered)return {State::Offline,nullptr};break;}
        answered=true;found=pickLyrics(*json,t.duration);if(found.kind!=LyricsResult::Kind::None||primaryArtist(q.artist)==q.artist||primaryArtist(q.artist).empty())break;}
    LyricsCacheEntry entry;entry.time=now;entry.status=found.kind==LyricsResult::Kind::Synced?LyricsCacheEntry::Status::Found:found.kind==LyricsResult::Kind::Instrumental?LyricsCacheEntry::Status::Instrumental:LyricsCacheEntry::Status::Missing;entry.lrc=found.lrc;
    // Atomic write, then keep the folder bounded (400 songs, oldest out first).
    std::error_code ec;std::filesystem::create_directories(directory_,ec);
    {auto temp=file;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<writeLyricsCache(entry);}if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING))std::filesystem::remove(temp,ec);}
    {std::vector<std::pair<std::filesystem::file_time_type,std::filesystem::path>> files;for(auto& f:std::filesystem::directory_iterator(directory_,ec))if(f.path().extension()==L".lrc")files.push_back({f.last_write_time(ec),f.path()});
        if(files.size()>400){std::sort(files.begin(),files.end());for(size_t i=0;i+400<files.size();++i)std::filesystem::remove(files[i].second,ec);}}
    return result(entry);
}
}
