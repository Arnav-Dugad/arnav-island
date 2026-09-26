#include "UpdateService.h"
#include "Common/Http.h"
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <ctime>
#include <fstream>
#include <vector>
namespace nexus {
std::string fileSha256(const std::filesystem::path& file){
    BCRYPT_ALG_HANDLE algorithm=nullptr;if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)!=0)return {};
    BCRYPT_HASH_HANDLE hash=nullptr;std::string hex;
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0){
        std::ifstream in(file,std::ios::binary);std::vector<char> chunk(1<<16);bool ok=bool(in);
        while(ok&&in){in.read(chunk.data(),std::streamsize(chunk.size()));const auto got=in.gcount();if(got>0&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(chunk.data()),ULONG(got),0)!=0)ok=false;}
        UCHAR digest[32]{};if(ok&&BCryptFinishHash(hash,digest,32,0)==0){static const char* digits="0123456789abcdef";for(UCHAR b:digest){hex+=digits[b>>4];hex+=digits[b&15];}}
        BCryptDestroyHash(hash);}
    BCryptCloseAlgorithmProvider(algorithm,0);return hex;
}
static std::string sha256Hex(const void* data,size_t size){
    BCRYPT_ALG_HANDLE algorithm=nullptr;if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)!=0)return {};UCHAR digest[32]{};std::string hex;
    if(BCryptHash(algorithm,nullptr,0,static_cast<PUCHAR>(const_cast<void*>(data)),ULONG(size),digest,32)==0){static const char* digits="0123456789abcdef";for(UCHAR b:digest){hex+=digits[b>>4];hex+=digits[b&15];}}
    BCryptCloseAlgorithmProvider(algorithm,0);return hex;
}
std::string signerSha256(const std::filesystem::path& file){
    WINTRUST_FILE_INFO info{};info.cbStruct=sizeof(info);info.pcwszFilePath=file.c_str();
    WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&info;
    data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result=WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),&action,&data);std::string hex;
    // Intact, and either trusted or signed by a certificate Windows doesn't know: then which certificate it was.
    if(result==ERROR_SUCCESS||result==LONG(CERT_E_UNTRUSTEDROOT)){if(auto* provider=WTHelperProvDataFromStateData(data.hWVTStateData))if(auto* signer=WTHelperGetProvSignerFromChain(provider,0,FALSE,0))
        if(signer->csCertChain>0&&signer->pasCertChain&&signer->pasCertChain[0].pCert){auto* cert=signer->pasCertChain[0].pCert;hex=sha256Hex(cert->pbCertEncoded,cert->cbCertEncoded);}}
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),&action,&data);return hex;
}
std::wstring productVersion(const std::filesystem::path& file){
    DWORD ignored=0;const DWORD size=GetFileVersionInfoSizeW(file.c_str(),&ignored);if(!size)return {};std::vector<BYTE> data(size);if(!GetFileVersionInfoW(file.c_str(),0,size,data.data()))return {};
    struct Translation{WORD language,codePage;}* translations=nullptr;UINT length=0;if(!VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&length)||length<sizeof(Translation))return {};
    wchar_t key[64];swprintf(key,64,L"\\StringFileInfo\\%04x%04x\\ProductVersion",translations[0].language,translations[0].codePage);
    wchar_t* value=nullptr;UINT chars=0;if(!VerQueryValueW(data.data(),key,reinterpret_cast<void**>(&value),&chars)||!value)return {};return std::wstring(value);
}
UpdateService::UpdateService(HWND window,std::filesystem::path folder,AppVersion current,double firstDelay):window_(window),folder_(std::move(folder)),current_(current),firstDelay_(firstDelay),
    stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,FALSE,nullptr)){
    if(!stop_||!wake_)throw std::runtime_error("Update event creation failed");
    std::error_code ec;std::filesystem::remove_all(folder_,ec);
    worker_=std::thread([this]{run();});
}
UpdateService::~UpdateService(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
void UpdateService::set(State s,std::wstring status){{std::lock_guard lock(mutex_);state_=s;status_=std::move(status);}PostMessageW(window_,UpdateMessage,WPARAM(s),0);}
void UpdateService::run(){
    HANDLE waits[]={stop_,wake_};DWORD wait=DWORD(firstDelay_*1000);
    for(;;){const DWORD r=WaitForMultipleObjects(2,waits,FALSE,wait);if(r==WAIT_OBJECT_0||r==WAIT_FAILED)break;
        {std::lock_guard lock(mutex_);if(state_==State::Ready){wait=6*3600*1000;continue;}}
        check();State now;{std::lock_guard lock(mutex_);now=state_;}
        // Every six hours; after a failure (offline, a broken download), again in half an hour.
        wait=now==State::Failed?30*60*1000:6*3600*1000;}
}
void UpdateService::check(){
    auto at=[]{std::time_t t=std::time(nullptr);std::tm tm{};localtime_s(&tm,&t);wchar_t b[16];wcsftime(b,16,L"%H:%M",&tm);return std::wstring(b);};
    set(State::Checking,L"Checking for updates…");
    auto list=httpsGet(L"api.github.com",L"/repos/Arnav-Dugad/arnav-island/releases?per_page=12");
    if(!list){set(State::Failed,L"Couldn’t reach GitHub at "+at()+L". The island will try again");return;}
    auto release=newestRelease(*list,current_);
    if(!release){set(State::UpToDate,L"Up to date ("+versionText(current_)+L"), checked at "+at());return;}
    set(State::Downloading,L"Downloading "+versionText(release->version)+L"…");
    std::error_code ec;std::filesystem::remove_all(folder_,ec);std::filesystem::create_directories(folder_,ec);
    const auto zip=folder_/L"release.zip",unpacked=folder_/L"release";
    auto fail=[&](const std::wstring& why){std::filesystem::remove_all(folder_,ec);set(State::Failed,why);};
    auto [shaHost,shaPath]=splitUrl(release->shaUrl);auto [zipHost,zipPath]=splitUrl(release->zipUrl);
    auto sum=shaHost.empty()?std::nullopt:httpsGet(shaHost.c_str(),shaPath);
    if(!sum||zipHost.empty()||!httpsGetToFile(zipHost.c_str(),zipPath,zip.wstring(),256ull<<20)){fail(L"Couldn’t download "+versionText(release->version)+L". The island will try again");return;}
    if(!checksumMatches(*sum,fileSha256(zip))){fail(L"The download of "+versionText(release->version)+L" didn’t match its checksum, so it wasn’t used");return;}
    // Windows' own tar unpacks ZIP files.
    std::filesystem::create_directories(unpacked,ec);wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);const std::wstring tar=std::wstring(system)+L"\\tar.exe";
    std::wstring command=L"\""+tar+L"\" -xf \""+zip.wstring()+L"\" -C \""+unpacked.wstring()+L"\"";
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};DWORD code=1;
    if(CreateProcessW(tar.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){if(WaitForSingleObject(pi.hProcess,120000)==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);else TerminateProcess(pi.hProcess,1);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
    const auto exe=unpacked/L"ArnavIsland.exe";
    if(code!=0||!std::filesystem::exists(exe)){fail(L"Couldn’t unpack "+versionText(release->version));return;}
    // The program itself must say it is the version the release is.
    if(parseVersion(toUtf8(productVersion(exe)))!=release->version){fail(L"The download of "+versionText(release->version)+L" wasn’t that version, so it wasn’t used");return;}
    // And it must be signed by the island's publisher: a release someone else managed to put up is never installed.
    if(!signedByPublisher(exe)){fail(L"The download of "+versionText(release->version)+L" wasn\u2019t signed by the island\u2019s publisher, so it wasn\u2019t used");return;}
    std::filesystem::remove(zip,ec);{std::ofstream notes(folder_/L"notes.md",std::ios::binary);notes<<release->notes;}
    {std::lock_guard lock(mutex_);ready_=release->version;staged_=unpacked;}
    set(State::Ready,versionText(release->version)+L" is ready. It installs the next time the island is resting");
}
bool UpdateService::install(const std::wstring& args){
    std::filesystem::path staged;AppVersion version;{std::lock_guard lock(mutex_);if(state_!=State::Ready)return false;staged=staged_;version=ready_;}
    wchar_t self[MAX_PATH]{};if(!GetModuleFileNameW(nullptr,self,MAX_PATH))return false;const std::filesystem::path exe=self,dir=exe.parent_path(),old=dir/L"ArnavIsland.old.exe";
    std::error_code ec;std::filesystem::remove(old,ec);
    // A running program can be renamed, not overwritten: this one steps aside, the new one takes its name.
    if(!MoveFileExW(exe.c_str(),old.c_str(),MOVEFILE_REPLACE_EXISTING)){set(State::Failed,L"The island’s folder can’t be changed, so "+versionText(version)+L" can’t install. Download it from GitHub");return false;}
    if(!CopyFileW((staged/L"ArnavIsland.exe").c_str(),exe.c_str(),FALSE)){MoveFileExW(old.c_str(),exe.c_str(),MOVEFILE_REPLACE_EXISTING);set(State::Failed,L"Couldn’t install "+versionText(version));return false;}
    for(auto* name:{L"LICENSE",L"THIRD_PARTY_NOTICES.md",L"QUICK_START.md"})if(std::filesystem::exists(staged/name))CopyFileW((staged/name).c_str(),(dir/name).c_str(),FALSE);
    // The release's notes, for the new version's What's new.
    CopyFileW((folder_/L"notes.md").c_str(),(folder_.parent_path()/L"whats-new.md").c_str(),FALSE);
    pendingLaunch=L"\""+exe.wstring()+L"\" "+args+L" --after="+std::to_wstring(GetCurrentProcessId())+L" --updated="+versionText(current_);
    pendingFallback=L"\""+exe.wstring()+L"\" "+args;return true;
}
std::wstring UpdateService::pendingLaunch,UpdateService::pendingFallback;
void UpdateService::launchPending(){
    if(pendingLaunch.empty())return;wchar_t self[MAX_PATH]{};GetModuleFileNameW(nullptr,self,MAX_PATH);const std::filesystem::path exe=self,dir=exe.parent_path(),old=dir/L"ArnavIsland.old.exe";
    auto start=[&](std::wstring command){STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,dir.c_str(),&si,&pi))return false;CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return true;};
    if(start(pendingLaunch))return;
    // The new version didn't start: this version goes back in its place and starts again.
    DeleteFileW(exe.c_str());if(MoveFileExW(old.c_str(),exe.c_str(),MOVEFILE_REPLACE_EXISTING))start(pendingFallback);
}
void UpdateService::cleanUp(){
    wchar_t self[MAX_PATH]{};if(!GetModuleFileNameW(nullptr,self,MAX_PATH))return;const auto old=std::filesystem::path(self).parent_path()/L"ArnavIsland.old.exe";
    // The previous version may take a moment to close.
    for(int k=0;k<20&&std::filesystem::exists(old);++k){std::error_code ec;if(std::filesystem::remove(old,ec))break;Sleep(250);}
}
}
