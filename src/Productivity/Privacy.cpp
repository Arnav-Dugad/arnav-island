#include "Productivity/Privacy.h"
#include "Media/AppIdentity.h"
#include <appmodel.h>
#include <map>
namespace nexus {
namespace {
constexpr const wchar_t* storeKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore";
uint64_t qword(HKEY key,const wchar_t* name){uint64_t v=0;DWORD size=sizeof(v),type=0;if(RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&v),&size)!=ERROR_SUCCESS||type!=REG_QWORD)return 0;return v;}
void readApps(HKEY parent,Capability capability,bool packaged,std::vector<ConsentRecord>& out){
    wchar_t name[512];for(DWORD i=0;;++i){DWORD len=512;if(RegEnumKeyExW(parent,i,name,&len,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)break;
        if(packaged&&wcscmp(name,L"NonPackaged")==0)continue;HKEY app=nullptr;if(RegOpenKeyExW(parent,name,0,KEY_READ,&app)!=ERROR_SUCCESS)continue;
        ConsentRecord r{capability,name,packaged,qword(app,L"LastUsedTimeStart"),qword(app,L"LastUsedTimeStop")};RegCloseKey(app);if(r.start)out.push_back(std::move(r));}
}
struct Running {std::set<std::wstring> paths,families;};
Running running(){
    Running r;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return r;
    PROCESSENTRY32W e{sizeof(e)};for(BOOL ok=Process32FirstW(snapshot,&e);ok;ok=Process32NextW(snapshot,&e)){
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(!p)continue;
        wchar_t path[MAX_PATH*2];DWORD size=MAX_PATH*2;if(QueryFullProcessImageNameW(p,0,path,&size))r.paths.insert(lower(std::wstring(path,size)));
        UINT32 len=0;if(GetPackageFamilyName(p,&len,nullptr)==ERROR_INSUFFICIENT_BUFFER&&len){std::wstring family(len,L'\0');if(GetPackageFamilyName(p,&len,family.data())==ERROR_SUCCESS){family.resize(len?len-1:0);r.families.insert(lower(family));}}
        CloseHandle(p);}
    CloseHandle(snapshot);return r;
}
std::wstring describe(const std::wstring& path){
    DWORD handle=0,size=GetFileVersionInfoSizeW(path.c_str(),&handle);if(size){std::vector<BYTE> data(size);if(GetFileVersionInfoW(path.c_str(),0,size,data.data())){
        struct Lang{WORD language,codepage;}*langs=nullptr;UINT bytes=0;if(VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&langs),&bytes)&&bytes>=sizeof(Lang)){
            wchar_t query[64];swprintf(query,64,L"\\StringFileInfo\\%04x%04x\\FileDescription",langs[0].language,langs[0].codepage);wchar_t* value=nullptr;UINT n=0;
            if(VerQueryValueW(data.data(),query,reinterpret_cast<void**>(&value),&n)&&value&&n>1)return std::wstring(value);}}}
    auto slash=path.find_last_of(L"\\/");std::wstring base=slash==std::wstring::npos?path:path.substr(slash+1);if(base.size()>4&&lower(base).ends_with(L".exe"))base.resize(base.size()-4);return base;
}
}
std::vector<ConsentRecord> PrivacyProvider::records(){
    std::vector<ConsentRecord> out;
    // Screen capture is recorded under two keys: with and without the yellow capture border.
    for(auto [capability,name]:{std::pair{Capability::Microphone,L"microphone"},std::pair{Capability::Camera,L"webcam"},std::pair{Capability::Location,L"location"},std::pair{Capability::ScreenCapture,L"graphicsCaptureProgrammatic"},std::pair{Capability::ScreenCapture,L"graphicsCaptureWithoutBorder"}}){
        HKEY key=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,(std::wstring(storeKey)+L"\\"+name).c_str(),0,KEY_READ,&key)!=ERROR_SUCCESS)continue;
        readApps(key,capability,true,out);HKEY classic=nullptr;if(RegOpenKeyExW(key,L"NonPackaged",0,KEY_READ,&classic)==ERROR_SUCCESS){readApps(classic,capability,false,out);RegCloseKey(classic);}
        RegCloseKey(key);}
    return out;
}
std::vector<PrivacyUse> PrivacyProvider::current(){
    auto all=records();std::vector<const ConsentRecord*> open;for(auto& r:all)if(r.start&&!r.stop)open.push_back(&r);if(open.empty())return {};
    auto live=running();std::vector<PrivacyUse> out;static std::map<std::wstring,std::pair<std::wstring,std::shared_ptr<const Artwork>>> names;
    for(auto* r:open){if(!inUse(*r,live.paths,live.families))continue;PrivacyUse u{r->capability,{},r->key,r->packaged,r->start,nullptr};
        auto it=names.find(r->key);if(it==names.end()){std::wstring app;std::shared_ptr<const Artwork> icon;
            if(r->packaged){app=packagedAppName(r->key);auto id=packagedAppId(r->key);if(!id.empty())icon=shellIcon(L"shell:AppsFolder\\"+id,48);if(app.empty())app=r->key.substr(0,r->key.find(L'_'));}
            else{auto path=consentPath(r->key);app=describe(path);icon=shellIcon(path,48);}
            it=names.emplace(r->key,std::pair{app,icon}).first;}
        u.app=it->second.first;u.icon=it->second.second;out.push_back(std::move(u));}
    // Camera first, then microphone, screen capture and location; newest first within each.
    std::stable_sort(out.begin(),out.end(),[](auto& a,auto& b){auto rank=[](Capability c){return c==Capability::Camera?0:c==Capability::Microphone?1:c==Capability::ScreenCapture?2:3;};return rank(a.capability)<rank(b.capability)||(a.capability==b.capability&&a.since>b.since);});
    return out;
}
PrivacyProvider::PrivacyProvider(HWND w):window_(w),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)){if(!stop_)throw std::runtime_error("Privacy stop event failed");worker_=std::thread([this]{run();});}
PrivacyProvider::~PrivacyProvider(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);}
void PrivacyProvider::run(){
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HKEY store=nullptr;RegOpenKeyExW(HKEY_CURRENT_USER,storeKey,0,KEY_NOTIFY|KEY_READ,&store);HANDLE changed=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    std::vector<PrivacyUse> last;bool first=true;
    for(;;){
        if(store&&changed)RegNotifyChangeKeyValue(store,TRUE,REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET,changed,TRUE);
        auto now=current();bool differ=first||now.size()!=last.size();for(size_t i=0;!differ&&i<now.size();++i)differ=!(now[i]==last[i]);
        if(differ){{std::lock_guard lock(mutex_);uses_=now;}last=std::move(now);first=false;PostMessageW(window_,PrivacyMessage,0,0);}
        // Registry changes arrive at once; a slow check catches apps that exit without closing their record.
        HANDLE events[]={stop_,changed};DWORD wait=WaitForMultipleObjects(changed?2:1,events,FALSE,last.empty()?15000:3000);if(wait==WAIT_OBJECT_0)break;
        if(wait==WAIT_OBJECT_0+1)Sleep(120);// let a burst of writes settle
    }
    if(changed)CloseHandle(changed);if(store)RegCloseKey(store);CoUninitialize();
}
}
