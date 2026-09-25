// MinGW declares IReference<BYTE> and IReference<boolean>, which are the same
// specialization because boolean is unsigned char. Keep only the first.
#ifndef ____FIReference_1_boolean_INTERFACE_DEFINED__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#endif
#include "Productivity/CommandService.h"
#include "Media/AppIdentity.h"
#include "Media/Lyrics.h"
#include "Common/Http.h"
#include <appmodel.h>
#include <asyncinfo.h>
#include <dwmapi.h>
#include <msdasc.h>
#include <oledb.h>
#include <oledberr.h>
#include <propkey.h>
#include <propsys.h>
#include <roapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.devices.radios.h>
#include <winhttp.h>
#include <winstring.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
namespace nexus {
namespace {
constexpr GUID commandTextDialect{0xc8b521fb,0x5cf3,0x11ce,{0xad,0xe5,0x00,0xaa,0x00,0x44,0x77,0x3d}};// DBGUID_DEFAULT
int64_t unixNow(){return int64_t(std::time(nullptr));}
// One HTTPS GET, at most 4 MB, five-second timeouts (Common/Http.h).
std::optional<std::string> httpGet(const wchar_t* host,const wchar_t* path){return httpsGet(host,path);}
// WinRT async operations are polled from a multithreaded apartment; nothing here pumps messages.
template<class Operation> bool finished(Operation* op,int milliseconds){
    ComPtr<IAsyncInfo> info;if(FAILED(op->QueryInterface(IID_PPV_ARGS(&info))))return false;AsyncStatus status=Started;
    for(int waited=0;waited<milliseconds;waited+=10){if(FAILED(info->get_Status(&status)))return false;if(status!=Started)break;Sleep(10);}return status==Completed;
}
namespace radios=ABI::Windows::Devices::Radios;
namespace foundation=ABI::Windows::Foundation;
struct Apartment {HRESULT hr;Apartment():hr(RoInitialize(RO_INIT_MULTITHREADED)){}~Apartment(){if(SUCCEEDED(hr))RoUninitialize();}};
ComPtr<radios::IRadioStatics> radioStatics(){HSTRING_HEADER header;HSTRING name=nullptr;static constexpr wchar_t cls[]=L"Windows.Devices.Radios.Radio";
    if(FAILED(WindowsCreateStringReference(cls,UINT32(std::size(cls)-1),&header,&name)))return nullptr;ComPtr<radios::IRadioStatics> s;if(FAILED(RoGetActivationFactory(name,IID_PPV_ARGS(&s))))return nullptr;return s;}
std::vector<ComPtr<radios::IRadio>> allRadios(radios::IRadioStatics* statics){
    std::vector<ComPtr<radios::IRadio>> out;ComPtr<foundation::IAsyncOperation<foundation::Collections::IVectorView<radios::Radio*>*>> op;
    if(FAILED(statics->GetRadiosAsync(&op))||!finished(op.Get(),2000))return out;ComPtr<foundation::Collections::IVectorView<radios::Radio*>> list;if(FAILED(op->GetResults(&list))||!list)return out;
    unsigned n=0;list->get_Size(&n);for(unsigned i=0;i<n;++i){ComPtr<radios::IRadio> r;if(SUCCEEDED(list->GetAt(i,&r))&&r)out.push_back(r);}return out;
}
// MinGW declares IRadio::get_State with one pointer too many; at the ABI it is still one out pointer.
radios::RadioState stateOf(radios::IRadio* r){radios::RadioState s=radios::RadioState_Unknown;r->get_State(reinterpret_cast<radios::RadioState**>(&s));return s;}
// 1 on, 0 off, -1 unknown, -2 no such radio.
void radioStates(int& wifi,int& bluetooth){
    wifi=bluetooth=-1;Apartment apartment;auto statics=radioStatics();if(!statics)return;auto list=allRadios(statics.Get());wifi=bluetooth=-2;
    for(auto& r:list){radios::RadioKind kind{};if(FAILED(r->get_Kind(&kind)))continue;const auto s=stateOf(r.Get());const int v=s==radios::RadioState_On?1:s==radios::RadioState_Off||s==radios::RadioState_Disabled?0:-1;
        int& slot=kind==radios::RadioKind_WiFi?wifi:kind==radios::RadioKind_Bluetooth?bluetooth:wifi;if(kind!=radios::RadioKind_WiFi&&kind!=radios::RadioKind_Bluetooth)continue;
        // Several radios of one kind: on if any is on.
        if(slot<v)slot=v;}
}
// "Documents › Work" for a folder inside the user folder, else the folder's own name.
std::wstring whereLabel(const std::filesystem::path& file,const std::wstring& profile){
    const auto parent=file.parent_path().wstring();const auto p=lowered(parent),home=lowered(profile);
    if(p==home)return L"Home folder";
    if(p.size()>home.size()&&p.starts_with(home)&&(p[home.size()]==L'\\'||p[home.size()]==L'/')){
        // Long paths keep their first folder and the last two: "Documents \u203a \u2026 \u203a work \u203a tools".
        std::vector<std::wstring> parts;std::wstring w;for(wchar_t c:parent.substr(home.size()+1)){if(c==L'\\'||c==L'/'){if(!w.empty())parts.push_back(w);w.clear();}else w+=c;}if(!w.empty())parts.push_back(w);
        if(parts.size()>3)parts={parts.front(),L"\u2026",parts[parts.size()-2],parts.back()};std::wstring out;for(auto& part:parts){if(!out.empty())out+=L" \u203a ";out+=part;}return out;}
    auto name=file.parent_path().filename().wstring();return name.empty()?parent:name;
}
std::wstring knownFolder(const std::wstring& word){
    const KNOWNFOLDERID* id=word==L"downloads"?&FOLDERID_Downloads:word==L"documents"?&FOLDERID_Documents:word==L"desktop"?&FOLDERID_Desktop:word==L"pictures"?&FOLDERID_Pictures:word==L"music"?&FOLDERID_Music:word==L"videos"?&FOLDERID_Videos:nullptr;
    if(!id)return {};PWSTR path=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(*id,0,nullptr,&path))&&path)out=path;CoTaskMemFree(path);return out;
}
// A local day's midnight as UTC, for the index query and the scan.
std::optional<SYSTEMTIME> utcMidnight(Day d){SYSTEMTIME local{};local.wYear=WORD(d.y);local.wMonth=WORD(d.m);local.wDay=WORD(d.d);SYSTEMTIME utc{};if(!TzSpecificLocalTimeToSystemTime(nullptr,&local,&utc))return std::nullopt;return utc;}
std::wstring sqlTime(const SYSTEMTIME& t){wchar_t b[32];swprintf(b,32,L"%04u-%02u-%02u %02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return b;}
uint64_t fileTimeValue(const FILETIME& f){return (uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;}
struct Hit {std::wstring path,name;FILETIME modified{};unsigned long long size=0;bool folder=false;};
}
struct CommandService::Rates {std::mutex m;ExchangeRates rates;bool loading=false,cacheRead=false;double failedAt=-1e9;std::filesystem::file_time_type fetched{};};
struct CommandService::Index {ComPtr<IDBCreateCommand> session;bool off=false;double retryAt=0;};
CommandService::CommandService(HWND w,std::wstring scope,std::filesystem::path data):window_(w),scope_(std::move(scope)),data_(std::move(data)),rates_(std::make_shared<Rates>()),index_(std::make_unique<Index>()){worker_=std::thread([this]{run();});}
CommandService::~CommandService(){{std::lock_guard lock(mutex_);stop_=true;}wake_.notify_all();if(worker_.joinable())worker_.join();}
void CommandService::query(const std::wstring& text,std::vector<std::wstring> workspaces,CommandContext context,std::vector<RememberedCommand> memory,bool currency,bool refreshState){
    {std::lock_guard lock(mutex_);query_=text;workspaces_=std::move(workspaces);context_=std::move(context);memory_=std::move(memory);currency_=currency;refresh_=refresh_||refreshState;++querySeq_;}wake_.notify_all();
}
std::shared_ptr<const Artwork> CommandService::icon(const CommandResult& r){
    std::wstring name=!r.appId.empty()?L"shell:AppsFolder\\"+r.appId:r.kind==CommandKind::OpenFile?r.target:std::wstring{};if(name.empty())return nullptr;
    auto it=iconCache_.find(name);if(it==iconCache_.end()){if(iconCache_.size()>256)iconCache_.clear();it=iconCache_.emplace(name,shellIcon(name,48)).first;}return it->second;
}
void CommandService::publish(uint64_t seq,std::vector<CommandResult> results){
    std::vector<std::shared_ptr<const Artwork>> icons;for(auto& r:results)icons.push_back(icon(r));
    {std::lock_guard lock(mutex_);if(seq<resultSeq_||querySeq_!=seq)return;results_=std::move(results);icons_=std::move(icons);resultSeq_=seq;}
    PostMessageW(window_,CommandMessage,0,LPARAM(seq));
}
void CommandService::refreshState(){
    env_.dark=darkModeNow();
    SHQUERYRBINFO bin{};bin.cbSize=sizeof(bin);if(SUCCEEDED(SHQueryRecycleBinW(nullptr,&bin))){env_.binItems=bin.i64NumItems;env_.binBytes=bin.i64Size;}else env_.binItems=env_.binBytes=-1;
    // Radios are read from a short-lived multithreaded apartment; this worker is single-threaded.
    int wifi=-1,bluetooth=-1;std::thread([&]{radioStates(wifi,bluetooth);}).join();env_.wifi=wifi;env_.bluetooth=bluetooth;stateAt_=seconds();
}
// Exchange rates: read from the day's cache, else fetched once (then at most every 12 hours).
void CommandService::wantRates(){
    auto r=rates_;const auto path=data_/L"rates.xml";std::unique_lock lock(r->m);
    if(!r->cacheRead){r->cacheRead=true;std::ifstream in(path,std::ios::binary);if(in){std::stringstream s;s<<in.rdbuf();if(auto parsed=parseEcbRates(s.str())){r->rates=*parsed;std::error_code ec;r->fetched=std::filesystem::last_write_time(path,ec);}}}
    const bool fresh=!r->rates.empty()&&std::filesystem::file_time_type::clock::now()-r->fetched<std::chrono::hours(12);
    if(fresh||r->loading||seconds()-r->failedAt<60)return;
    r->loading=true;lock.unlock();
    std::thread([r,path,window=window_]{
        std::optional<ExchangeRates> parsed;std::string body;if(auto got=httpGet(L"www.ecb.europa.eu",L"/stats/eurofxref/eurofxref-daily.xml")){body=std::move(*got);parsed=parseEcbRates(body);}
        {std::lock_guard lock(r->m);r->loading=false;
            if(parsed){r->rates=std::move(*parsed);r->fetched=std::filesystem::file_time_type::clock::now();
                std::error_code ec;auto temp=path;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<body;}std::filesystem::rename(temp,path,ec);}
            else r->failedAt=seconds();}
        PostMessageW(window,CommandMessage,1,0);}).detach();
}
std::optional<std::vector<CommandResult>> CommandService::indexed(const FileSpec& spec,const std::wstring& root,size_t count,const CommandMemory& memory){
    auto& ix=*index_;if(ix.off&&seconds()<ix.retryAt)return std::nullopt;
    std::vector<Hit> hits;
    try{
        if(!ix.session){
            ComPtr<IDataInitialize> init;check(CoCreateInstance(CLSID_MSDAINITIALIZE,nullptr,CLSCTX_INPROC_SERVER,IID_IDataInitialize,reinterpret_cast<void**>(init.GetAddressOf())));
            wchar_t connection[]=L"Provider=Search.CollatorDSO;Extended Properties='Application=Windows';";ComPtr<IDBInitialize> db;
            check(init->GetDataSource(nullptr,CLSCTX_INPROC_SERVER,connection,IID_IDBInitialize,reinterpret_cast<IUnknown**>(db.GetAddressOf())));check(db->Initialize());
            ComPtr<IDBCreateSession> sessions;check(db->QueryInterface(IID_IDBCreateSession,reinterpret_cast<void**>(sessions.GetAddressOf())));
            check(sessions->CreateSession(nullptr,IID_IDBCreateCommand,reinterpret_cast<IUnknown**>(ix.session.GetAddressOf())));
        }
        std::wstring since,before;
        if(!spec.date.empty()){SYSTEMTIME now{};GetLocalTime(&now);if(auto range=dateRange(spec.date,{now.wYear,now.wMonth,now.wDay},now.wDayOfWeek)){if(auto a=utcMidnight(range->first))since=sqlTime(*a);if(auto b=utcMidnight(range->second))before=sqlTime(*b);}}
        std::wstring scope=L"file:"+root;for(auto& c:scope)if(c==L'\\')c=L'/';
        const auto sql=searchSql(spec,scope,since,before,40);
        ComPtr<ICommandText> command;check(ix.session->CreateCommand(nullptr,IID_ICommandText,reinterpret_cast<IUnknown**>(command.GetAddressOf())));check(command->SetCommandText(commandTextDialect,sql.c_str()));
        ComPtr<IRowset> rows;check(command->Execute(nullptr,IID_IRowset,nullptr,nullptr,reinterpret_cast<IUnknown**>(rows.GetAddressOf())));
        ComPtr<IAccessor> accessor;check(rows->QueryInterface(IID_IAccessor,reinterpret_cast<void**>(accessor.GetAddressOf())));
        struct Row{DBSTATUS pathStatus;DBLENGTH pathLength;wchar_t path[1040];DBSTATUS nameStatus;DBLENGTH nameLength;wchar_t name[520];DBSTATUS timeStatus;DBLENGTH timeLength;FILETIME modified;DBSTATUS sizeStatus;DBLENGTH sizeLength;ULONGLONG size;};
        DBBINDING bindings[4]{};auto bind=[&](int i,DBORDINAL ordinal,DBTYPE type,size_t value,size_t length,size_t status,size_t max){auto& b=bindings[i];b.iOrdinal=ordinal;b.obValue=value;b.obLength=length;b.obStatus=status;b.dwPart=DBPART_VALUE|DBPART_LENGTH|DBPART_STATUS;b.dwMemOwner=DBMEMOWNER_CLIENTOWNED;b.eParamIO=DBPARAMIO_NOTPARAM;b.cbMaxLen=max;b.wType=type;};
        bind(0,1,DBTYPE_WSTR,offsetof(Row,path),offsetof(Row,pathLength),offsetof(Row,pathStatus),sizeof(Row::path));bind(1,2,DBTYPE_WSTR,offsetof(Row,name),offsetof(Row,nameLength),offsetof(Row,nameStatus),sizeof(Row::name));
        bind(2,3,DBTYPE_FILETIME,offsetof(Row,modified),offsetof(Row,timeLength),offsetof(Row,timeStatus),sizeof(FILETIME));bind(3,4,DBTYPE_UI8,offsetof(Row,size),offsetof(Row,sizeLength),offsetof(Row,sizeStatus),sizeof(ULONGLONG));
        HACCESSOR handle=0;DBBINDSTATUS statuses[4]{};check(accessor->CreateAccessor(DBACCESSOR_ROWDATA,4,bindings,sizeof(Row),&handle,statuses));
        struct Release{IAccessor* a;HACCESSOR h;~Release(){a->ReleaseAccessor(h,nullptr);}} release{accessor.Get(),handle};
        auto row=std::make_unique<Row>();
        for(;;){DBCOUNTITEM got=0;HROW* handles=nullptr;const HRESULT hr=rows->GetNextRows(DB_NULL_HCHAPTER,0,20,&got,&handles);if(FAILED(hr))break;
            for(DBCOUNTITEM i=0;i<got;++i){*row=Row{};if(FAILED(rows->GetData(handles[i],handle,row.get()))||row->pathStatus!=DBSTATUS_S_OK)continue;
                Hit h;h.path=pathFromItemUrl(row->path);if(h.path.empty()||noisyFile(h.path))continue;h.name=row->nameStatus==DBSTATUS_S_OK?std::wstring(row->name):std::filesystem::path(h.path).filename().wstring();if(row->timeStatus==DBSTATUS_S_OK)h.modified=row->modified;if(row->sizeStatus==DBSTATUS_S_OK)h.size=row->size;
                // The index can lag behind the disk: only what still exists is offered.
                const DWORD attributes=GetFileAttributesW(h.path.c_str());if(attributes==INVALID_FILE_ATTRIBUTES)continue;h.folder=attributes&FILE_ATTRIBUTE_DIRECTORY;hits.push_back(std::move(h));}
            if(got){rows->ReleaseRows(got,handles,nullptr,nullptr,nullptr);}CoTaskMemFree(handles);if(hr==DB_S_ENDOFROWSET||!got)break;}
        ix.off=false;
    }catch(...){ix.session.Reset();ix.off=true;ix.retryAt=seconds()+60;return std::nullopt;}
    (void)count;(void)memory;
    std::vector<CommandResult> out;for(auto& h:hits){CommandResult r;r.kind=CommandKind::OpenFile;r.title=h.name;r.target=h.path;r.value=h.folder;r.detail=std::to_wstring(fileTimeValue(h.modified))+L"|"+std::to_wstring(h.size);out.push_back(std::move(r));}
    return out;
}
std::vector<CommandResult> CommandService::scanned(const FileSpec& spec,const std::wstring& root,size_t,const CommandMemory&){
    std::vector<CommandResult> out;std::error_code ec;const auto start=std::chrono::steady_clock::now();
    uint64_t since=0,before=~0ull;if(!spec.date.empty()){SYSTEMTIME now{};GetLocalTime(&now);if(auto range=dateRange(spec.date,{now.wYear,now.wMonth,now.wDay},now.wDayOfWeek)){FILETIME f{};if(auto a=utcMidnight(range->first);a&&SystemTimeToFileTime(&*a,&f))since=fileTimeValue(f);if(auto b=utcMidnight(range->second);b&&SystemTimeToFileTime(&*b,&f))before=fileTimeValue(f);}}
    // A bounded walk of the user folder: at most 0.4 s, skipping app data and hidden trees.
    std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
    for(;!ec&&it!=end;it.increment(ec)){
        if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(400)||out.size()>=200)break;
        const auto name=it->path().filename().wstring();std::error_code typeError;const bool folder=it->is_directory(typeError);
        if(folder&&(name.starts_with(L".")||name==L"AppData"||name==L"$RECYCLE.BIN"||std::any_of(std::begin(noiseFolders),std::end(noiseFolders),[&](auto d){return lowered(name)==lowered(std::wstring(d));}))){it.disable_recursion_pending();continue;}
        if(noisyFile(it->path().wstring()))continue;
        if(!fileMatches(spec,name,folder))continue;
        WIN32_FILE_ATTRIBUTE_DATA data{};if(!GetFileAttributesExW(it->path().c_str(),GetFileExInfoStandard,&data)||(data.dwFileAttributes&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)))continue;
        const uint64_t modified=fileTimeValue(data.ftLastWriteTime);if(modified<since||modified>=before)continue;
        CommandResult r;r.kind=CommandKind::OpenFile;r.title=name;r.target=it->path().wstring();r.value=folder;r.detail=std::to_wstring(modified)+L"|"+std::to_wstring((uint64_t(data.nFileSizeHigh)<<32)|data.nFileSizeLow);out.push_back(std::move(r));
    }
    return out;
}
// Ranks raw hits (their detail holds "filetime|size" until now) and writes their real details.
std::vector<CommandResult> CommandService::files(const FileSpec& spec,size_t count,const CommandMemory& memory,bool& fromIndex){
    // Folder words resolve inside the search scope when it is not the user folder (test runs use Public).
    const std::wstring root=spec.folder.empty()?scope_:[&]{PWSTR home=nullptr;std::wstring profile;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile,0,nullptr,&home))&&home)profile=home;CoTaskMemFree(home);
        if(lowered(profile)!=lowered(scope_)){std::wstring word=spec.folder;word[0]=wchar_t(std::towupper(word[0]));return scope_+L"\\"+word;}auto f=knownFolder(spec.folder);return f.empty()?scope_:f;}();
    auto hits=indexed(spec,root,count,memory);fromIndex=hits.has_value();std::vector<CommandResult> raw=fromIndex?std::move(*hits):scanned(spec,root,count,memory);
    FILETIME nowFile{};GetSystemTimeAsFileTime(&nowFile);const uint64_t now=fileTimeValue(nowFile);const int64_t unix=unixNow();
    std::wstring terms;for(auto& t:spec.terms){if(!terms.empty())terms+=L' ';terms+=t;}
    std::vector<std::pair<double,CommandResult>> ranked;
    for(auto& r:raw){const auto bar=r.detail.find(L'|');const uint64_t modified=std::wcstoull(r.detail.substr(0,bar).c_str(),nullptr,10);const unsigned long long size=std::wcstoull(r.detail.substr(bar+1).c_str(),nullptr,10);
        const double age=modified&&modified<=now?double(now-modified)/1e7:0;const bool folder=r.value!=0;
        const double score=fileRank(r.title,spec,memory.frecency(CommandKind::OpenFile,r.target,unix),age/86400);
        std::wstring type=folder?L"Folder":L"File";if(!folder){const auto dot=r.title.find_last_of(L'.');if(dot!=std::wstring::npos&&dot+1<r.title.size()&&r.title.size()-dot<=6){type=r.title.substr(dot+1);for(auto& c:type)c=wchar_t(std::towupper(c));}}
        std::wstring detail=type;if(!folder&&size)detail+=L"  \u00b7  "+byteText(size);detail+=L"  \u00b7  "+whereLabel(r.target,scope_);if(modified)detail+=L"  \u00b7  "+fileAge(age);if(!fromIndex)detail+=L"  \u00b7  folder scan";
        r.detail=std::move(detail);r.value=0;r.marks=matchMarks(r.title,terms);ranked.push_back({score,std::move(r)});}
    std::stable_sort(ranked.begin(),ranked.end(),[](auto& a,auto& b){return a.first>b.first;});
    std::vector<CommandResult> out;for(auto& [score,r]:ranked){(void)score;out.push_back(std::move(r));if(out.size()==count)break;}
    return out;
}
std::vector<CommandResult> CommandService::idle(const CommandContext& ctx,const CommandMemory& memory,const std::vector<std::wstring>& workspaces){
    std::vector<CommandResult> rows;std::set<std::wstring> keys;int suggested=0;
    auto push=[&](CommandResult r,int section){if(rows.size()>=5)return false;const auto key=std::to_wstring(int(r.kind))+L"\x1f"+lowered(r.target.empty()?r.title:r.target);if(!keys.insert(key).second)return false;r.section=section;rows.push_back(std::move(r));return true;};
    // A remembered command is shown as it would run now (a toggle says what it will do today).
    auto revive=[&](const RememberedCommand& m)->std::optional<CommandResult>{
        if(m.kind==CommandKind::OpenFile){if(GetFileAttributesW(m.target.c_str())==INVALID_FILE_ATTRIBUTES)return std::nullopt;CommandResult r;r.kind=m.kind;r.title=m.title;r.detail=m.detail;r.target=m.target;r.phrase=m.phrase;return r;}
        if(!m.phrase.empty()){for(auto& p:parseCommand(m.phrase,installedApps(),workspaces,scope_,env_)){const bool targeted=p.kind==CommandKind::OpenApp||p.kind==CommandKind::Workspace||p.kind==CommandKind::OpenSettings;
            if(p.kind==m.kind&&(!targeted||lowered(p.target)==lowered(m.target))){p.phrase=m.phrase;p.marks.clear();p.completion.clear();return p;}}return std::nullopt;}
        CommandResult r;r.kind=m.kind;r.title=m.title;r.detail=m.detail;r.target=m.target;r.value=m.value;if(m.kind==CommandKind::OpenApp)r.appId=m.target;return r;};
    for(auto* m:memory.pinned())if(auto r=revive(*m))push(std::move(*r),1);
    auto suggest=[&](CommandKind k,std::wstring title,std::wstring detail,int value=0){if(suggested>=2)return;CommandResult r;r.kind=k;r.title=std::move(title);r.detail=std::move(detail);r.value=value;if(push(std::move(r),2))++suggested;};
    if(ctx.playing)suggest(CommandKind::Pause,L"Pause",ctx.track.empty()?L"Current media session":ctx.track);else if(ctx.media)suggest(CommandKind::Play,L"Play",ctx.track.empty()?L"Current media session":ctx.track);
    if(ctx.muted)suggest(CommandKind::Unmute,L"Unmute",L"System output");
    if(ctx.micMuted)suggest(CommandKind::MicUnmute,L"Unmute the microphone",L"Default recording device");
    if(ctx.timer)suggest(CommandKind::StopTimer,L"Stop the timer",L"Resets the focus timer");
    if((ctx.hour>=19||ctx.hour<6)&&env_.dark==0)suggest(CommandKind::DarkMode,L"Switch to dark mode",L"Light mode is on now",1);
    else if(ctx.hour>=7&&ctx.hour<11&&env_.dark==1)suggest(CommandKind::DarkMode,L"Switch to light mode",L"Dark mode is on now",0);
    for(auto* m:memory.recent(10)){if(rows.size()>=5)break;if(auto r=revive(*m))push(std::move(*r),3);}
    return rows;
}
void CommandService::run(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    installedApps();// load once, off the UI thread
    {wchar_t code[9]{};home_=GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT,LOCALE_SINTLSYMBOL,code,9)>1&&currencyByCode(code)?code:L"USD";}
    for(;;){
        std::wstring text;std::vector<std::wstring> workspaces;uint64_t seq=0;CommandContext context;std::vector<RememberedCommand> remembered;bool currency=false,refresh=false;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stop_||querySeq_!=doneSeq_;});if(stop_)break;text=query_;workspaces=workspaces_;seq=querySeq_;doneSeq_=seq;context=context_;remembered=memory_;currency=currency_;refresh=refresh_;refresh_=false;}
        const auto memory=CommandMemory::of(std::move(remembered));
        if(refresh||seconds()-stateAt_>5)refreshState();
        const std::wstring t=lowered(trimmed(text));const bool money=currency&&!t.empty()&&parseCurrency(t,home_).has_value();if(money)wantRates();
        ExchangeRates rates;bool loading=false;if(currency){std::lock_guard lock(rates_->m);rates=rates_->rates;loading=rates_->loading;}
        env_.currency=currency;env_.rates=rates.empty()?nullptr:&rates;env_.ratesLoading=loading;env_.home=home_;
        auto results=t.empty()?idle(context,memory,workspaces):parseCommand(text,installedApps(),workspaces,scope_,env_);
        if(!t.empty()&&!results.empty()&&results[0].completion.empty())results[0].completion=phraseCompletion(text);
        // Nothing matches yet but a command's name is being typed ("dark mo"): show what it will do.
        if(!t.empty()&&!results.empty()&&!results[0].completion.empty()&&std::all_of(results.begin(),results.end(),[](auto& r){return r.kind==CommandKind::None;})){
            const auto completion=results[0].completion;auto ahead=parseCommand(completion,installedApps(),workspaces,scope_,env_);
            if(!ahead.empty()&&ahead[0].kind!=CommandKind::None){results=std::move(ahead);results[0].completion=completion;}}
        publish(seq,results);
        // Files: "find ..." always; otherwise alongside apps once three letters are typed.
        if(t.empty()||money||superseded(seq))continue;
        const auto ws=words(t);const bool explicitFind=ws.size()>1&&(ws[0]==L"find"||ws[0]==L"search"||ws[0]==L"files"||ws[0]==L"file");
        const bool implicit=!explicitFind&&t.size()>=3&&std::all_of(results.begin(),results.end(),[](auto& r){return r.kind==CommandKind::OpenApp||r.kind==CommandKind::None;});
        if(!explicitFind&&!implicit)continue;
        const std::wstring query=explicitFind?trimmed(trimmed(text).substr(trimmed(text).find_first_of(L" \t"))):text;const auto spec=fileSpec(query);if(spec.empty())continue;
        const size_t apps=size_t(std::count_if(results.begin(),results.end(),[](auto& r){return r.kind==CommandKind::OpenApp;}));const size_t want=explicitFind?4:std::min<size_t>(3,apps>=5?0:5-apps);if(!want)continue;
        bool fromIndex=true;auto found=files(spec,want,memory,fromIndex);if(superseded(seq))continue;
        if(explicitFind){
            if(found.empty()){CommandResult none;none.kind=CommandKind::None;none.title=L"No files match";none.detail=fromIndex?L"Nothing in the search index matches":L"Nothing found in a quick scan of your user folder";found.push_back(std::move(none));}
            else if(!results.empty()&&results.back().kind==CommandKind::SearchFiles){auto& all=results.back();all.detail=all.title;all.title=L"Show all results in File Explorer";}
            found.insert(found.end(),results.begin(),results.end());results=std::move(found);
        }else{
            if(found.empty())continue;std::erase_if(results,[](auto& r){return r.kind==CommandKind::None;});
            if(!results.empty()||!found.empty()){const bool completed=!results.empty()&&!results[0].completion.empty();results.insert(results.end(),found.begin(),found.end());
                if(!completed&&lowered(results[0].title).starts_with(t)&&results[0].title.size()>trimmed(text).size())results[0].completion=trimmed(text)+results[0].title.substr(trimmed(text).size());}
        }
        publish(seq,std::move(results));
    }
    CoUninitialize();
}
int setRadios(int which,bool on){
    Apartment apartment;auto statics=radioStatics();if(!statics)return -1;
    ComPtr<foundation::IAsyncOperation<radios::RadioAccessStatus>> access;radios::RadioAccessStatus allowed=radios::RadioAccessStatus_Unspecified;
    if(FAILED(statics->RequestAccessAsync(&access))||!finished(access.Get(),3000)||FAILED(access->GetResults(&allowed))||allowed!=radios::RadioAccessStatus_Allowed)return -1;
    int found=0,changed=0;
    for(auto& r:allRadios(statics.Get())){radios::RadioKind kind{};if(FAILED(r->get_Kind(&kind)))continue;const bool wanted=(kind==radios::RadioKind_WiFi&&(which&1))||(kind==radios::RadioKind_Bluetooth&&(which&2));if(!wanted)continue;++found;
        const auto target=on?radios::RadioState_On:radios::RadioState_Off;if(stateOf(r.Get())==target)continue;
        ComPtr<foundation::IAsyncOperation<radios::RadioAccessStatus>> set;radios::RadioAccessStatus result=radios::RadioAccessStatus_Unspecified;
        if(SUCCEEDED(r->SetStateAsync(target,&set))&&finished(set.Get(),5000)&&SUCCEEDED(set->GetResults(&result))&&result==radios::RadioAccessStatus_Allowed)++changed;else return -1;}
    return found?changed:-2;
}
int radioOn(int which){int wifi=-1,bluetooth=-1;radioStates(wifi,bluetooth);return which==1?wifi:which==2?bluetooth:std::max(wifi,bluetooth);}
int darkModeNow(){DWORD value=1,size=sizeof(value);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size)!=ERROR_SUCCESS)return -1;return value==0?1:0;}
bool setDarkMode(bool dark){
    const DWORD light=dark?0:1;const wchar_t* key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    const bool ok=RegSetKeyValueW(HKEY_CURRENT_USER,key,L"AppsUseLightTheme",REG_DWORD,&light,sizeof(light))==ERROR_SUCCESS&&RegSetKeyValueW(HKEY_CURRENT_USER,key,L"SystemUsesLightTheme",REG_DWORD,&light,sizeof(light))==ERROR_SUCCESS;
    DWORD_PTR ignored=0;SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),SMTO_ABORTIFHUNG,1000,&ignored);return ok;
}
namespace {
// The AppUserModelID Windows groups a window under, if it has one.
std::wstring windowAppId(HWND h,DWORD pid){
    ComPtr<IPropertyStore> store;if(SUCCEEDED(SHGetPropertyStoreForWindow(h,IID_PPV_ARGS(&store)))){PROPVARIANT v;PropVariantInit(&v);if(SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID,&v))&&v.vt==VT_LPWSTR&&v.pwszVal&&*v.pwszVal){std::wstring id=v.pwszVal;PropVariantClear(&v);return id;}PropVariantClear(&v);}
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return {};UINT32 len=0;std::wstring id;
    if(GetApplicationUserModelId(p,&len,nullptr)==ERROR_INSUFFICIENT_BUFFER&&len){id.resize(len);if(GetApplicationUserModelId(p,&len,id.data())==ERROR_SUCCESS)id.resize(len?len-1:0);else id.clear();}
    CloseHandle(p);return id;
}
bool isInstalled(const std::wstring& id){auto key=lower(id);for(auto& a:installedApps())if(lower(a.id)==key)return true;return false;}
std::wstring nameFor(const std::wstring& id){auto key=lower(id);for(auto& a:installedApps())if(lower(a.id)==key)return a.name;return {};}
// Shell, input and system surfaces are never part of a workspace.
bool systemProcess(const std::wstring& exe){for(auto name:{L"explorer.exe",L"applicationframehost.exe",L"textinputhost.exe",L"shellexperiencehost.exe",L"startmenuexperiencehost.exe",L"searchhost.exe",L"lockapp.exe",L"systemsettingsbroker.exe",L"dwm.exe",L"widgets.exe",L"gamebar.exe"})if(exe==name)return true;return false;}
}
std::vector<WorkspaceApp> openApps(HWND self){
    struct Found {HWND self;std::vector<std::pair<HWND,DWORD>> windows;} found{self,{}};
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& f=*reinterpret_cast<Found*>(p);if(h==f.self||!IsWindowVisible(h)||GetWindow(h,GW_OWNER))return TRUE;
        if(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)return TRUE;BOOL cloaked=FALSE;DwmGetWindowAttribute(h,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)return TRUE;
        if(GetWindowTextLengthW(h)==0)return TRUE;DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(!pid||pid==GetCurrentProcessId())return TRUE;f.windows.push_back({h,pid});return TRUE;},reinterpret_cast<LPARAM>(&found));
    std::vector<WorkspaceApp> apps;std::set<std::wstring> seen;
    for(auto [h,pid]:found.windows){
        std::wstring path;auto name=processName(pid,&path);auto exe=lower(path.substr(path.find_last_of(L"\\/")+1));
        auto id=windowAppId(h,pid);WorkspaceApp app;
        if(!id.empty()&&isInstalled(id)){app={nameFor(id),id,true};}
        else if(!path.empty()&&!systemProcess(exe)){app={name.empty()?exe:name,path,false};}
        else continue;
        if(app.name.empty())app.name=name;if(seen.insert(lower(app.target)).second)apps.push_back(std::move(app));
        if(apps.size()>=WorkspaceStore::maxApps)break;
    }
    return apps;
}
LaunchReport openWorkspace(const Workspace& w,HWND self){
    LaunchReport report;auto open=openApps(self);std::set<std::wstring> running;for(auto& a:open)running.insert(lower(a.target));
    for(auto& a:w.apps){if(running.contains(lower(a.target))){++report.running;continue;}
        std::wstring target=a.appId?L"shell:AppsFolder\\"+a.target:a.target;
        if(!a.appId&&GetFileAttributesW(a.target.c_str())==INVALID_FILE_ATTRIBUTES){++report.failed;continue;}
        auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL));if(result>32)++report.opened;else ++report.failed;}
    return report;
}
std::shared_ptr<const Artwork> workspaceIcon(const WorkspaceApp& app){return shellIcon(app.appId?L"shell:AppsFolder\\"+app.target:app.target,48);}
}
