#include <winsock2.h>
#include <ws2tcpip.h>
#include "ShareService.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
namespace nexus {
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
using Clock=std::chrono::steady_clock;
constexpr uint8_t protocolVersion=uint8_t(shareProtocol);
constexpr size_t chunkSize=256*1024,maxFrame=chunkSize+64;
constexpr uint64_t maxFile=16ull<<30;
constexpr char magic[4]={'A','R','N','V'};
constexpr uint8_t modePair='P',modeSend='S',modeMusic='H';
bool success(LONG status){return status>=0;}
std::string hex(const uint8_t* p,size_t n){static const char* digits="0123456789abcdef";std::string s;s.reserve(n*2);for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;}
std::string hex(const Bytes& b){return hex(b.data(),b.size());}
Bytes unhex(const std::string& s){if(s.size()%2)return {};auto v=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};Bytes b;b.reserve(s.size()/2);
    for(size_t i=0;i<s.size();i+=2){const int a=v(s[i]),c=v(s[i+1]);if(a<0||c<0)return {};b.push_back(uint8_t(a*16+c));}return b;}
std::string utf8(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(std::max(0,n)),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
std::wstring wide(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(std::max(0,n)),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
// A peer's name as shown: printable, at most 64 characters.
std::wstring cleanName(std::wstring n){std::wstring out;for(wchar_t c:n)if(c>=32&&c!=127)out+=c;if(out.size()>64)out.resize(64);return out.empty()?L"A PC":out;}
void put64(Bytes& b,uint64_t v){for(int i=0;i<8;++i)b.push_back(uint8_t(v>>(8*i)));}
uint64_t get64(const uint8_t* p){uint64_t v=0;for(int i=0;i<8;++i)v|=uint64_t(p[i])<<(8*i);return v;}
void append(Bytes& b,const Bytes& more){b.insert(b.end(),more.begin(),more.end());}
void append(Bytes& b,const std::string& more){b.insert(b.end(),more.begin(),more.end());}
void wipe(Bytes& b){if(!b.empty())SecureZeroMemory(b.data(),b.size());b.clear();}

// Cryptography: Windows CNG (bcrypt). Providers open once for the process.
struct Providers{BCRYPT_ALG_HANDLE ecdh=nullptr,aes=nullptr,sha=nullptr;bool ok=false;
    Providers(){ok=success(BCryptOpenAlgorithmProvider(&ecdh,BCRYPT_ECDH_P256_ALGORITHM,nullptr,0))&&success(BCryptOpenAlgorithmProvider(&aes,BCRYPT_AES_ALGORITHM,nullptr,0))&&
        success(BCryptSetProperty(aes,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),ULONG(sizeof(BCRYPT_CHAIN_MODE_GCM)),0))&&
        success(BCryptOpenAlgorithmProvider(&sha,BCRYPT_SHA256_ALGORITHM,nullptr,0));}};
Providers& providers(){static Providers p;return p;}
struct Sha{BCRYPT_HASH_HANDLE h=nullptr;Sha(){BCryptCreateHash(providers().sha,&h,nullptr,0,nullptr,0,0);}~Sha(){if(h)BCryptDestroyHash(h);}Sha(const Sha&)=delete;Sha& operator=(const Sha&)=delete;
    Sha& add(const void* p,size_t n){if(h&&n)BCryptHashData(h,static_cast<PUCHAR>(const_cast<void*>(p)),ULONG(n),0);return *this;}Sha& add(const Bytes& b){return add(b.data(),b.size());}Sha& add(const std::string& s){return add(s.data(),s.size());}
    Bytes done(){Bytes o(32);if(!h||!success(BCryptFinishHash(h,o.data(),32,0)))return {};return o;}};
Bytes randomBytes(size_t n){Bytes b(n);if(!success(BCryptGenRandom(nullptr,b.data(),ULONG(n),BCRYPT_USE_SYSTEM_PREFERRED_RNG)))return {};return b;}
Bytes exportKey(BCRYPT_KEY_HANDLE k,LPCWSTR type){ULONG n=0;if(!success(BCryptExportKey(k,nullptr,type,nullptr,0,&n,0)))return {};Bytes b(n);if(!success(BCryptExportKey(k,nullptr,type,b.data(),n,&n,0)))return {};b.resize(n);return b;}
// A public key on the wire is X and Y (64 bytes).
Bytes publicOf(BCRYPT_KEY_HANDLE k){auto b=exportKey(k,BCRYPT_ECCPUBLIC_BLOB);if(b.size()!=sizeof(BCRYPT_ECCKEY_BLOB)+64)return {};return Bytes(b.begin()+sizeof(BCRYPT_ECCKEY_BLOB),b.end());}
BCRYPT_KEY_HANDLE importPublic(const Bytes& xy){if(xy.size()!=64)return nullptr;Bytes b(sizeof(BCRYPT_ECCKEY_BLOB)+64);auto* head=reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(b.data());head->dwMagic=BCRYPT_ECDH_PUBLIC_P256_MAGIC;head->cbKey=32;
    std::copy(xy.begin(),xy.end(),b.begin()+sizeof(BCRYPT_ECCKEY_BLOB));BCRYPT_KEY_HANDLE k=nullptr;if(!success(BCryptImportKeyPair(providers().ecdh,nullptr,BCRYPT_ECCPUBLIC_BLOB,&k,b.data(),ULONG(b.size()),0)))return nullptr;return k;}
// ECDH P-256, hashed with SHA-256.
bool agree(BCRYPT_KEY_HANDLE mine,const Bytes& peer,Bytes& out){
    BCRYPT_KEY_HANDLE k=importPublic(peer);if(!k)return false;BCRYPT_SECRET_HANDLE secret=nullptr;bool ok=success(BCryptSecretAgreement(mine,k,&secret,0));
    if(ok){BCryptBuffer param{ULONG((wcslen(BCRYPT_SHA256_ALGORITHM)+1)*sizeof(wchar_t)),KDF_HASH_ALGORITHM,const_cast<wchar_t*>(BCRYPT_SHA256_ALGORITHM)};BCryptBufferDesc desc{BCRYPTBUFFER_VERSION,1,&param};
        out.assign(32,0);ULONG got=0;ok=success(BCryptDeriveKey(secret,BCRYPT_KDF_HASH,&desc,out.data(),32,&got,0))&&got==32;BCryptDestroySecret(secret);}
    BCryptDestroyKey(k);return ok;
}
// The private key rests encrypted for this Windows user (DPAPI).
Bytes dpapi(const Bytes& b,bool unprotect){DATA_BLOB in{DWORD(b.size()),const_cast<BYTE*>(b.data())},out{};
    const BOOL ok=unprotect?CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out):CryptProtectData(&in,L"Arnav Island sharing key",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out);
    if(!ok)return {};Bytes r(out.pbData,out.pbData+out.cbData);SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);return r;}
// AES-256-GCM, one direction byte and a frame counter as the nonce, so no nonce ever repeats under a session key.
struct Channel{BCRYPT_KEY_HANDLE key=nullptr;uint8_t sendDir=1,recvDir=2;uint64_t sent=0,received=0;
    Channel()=default;Channel(const Channel&)=delete;Channel& operator=(const Channel&)=delete;~Channel(){if(key)BCryptDestroyKey(key);}
    bool init(const Bytes& k,bool initiator){sendDir=initiator?1:2;recvDir=initiator?2:1;return success(BCryptGenerateSymmetricKey(providers().aes,&key,nullptr,0,const_cast<PUCHAR>(k.data()),ULONG(k.size()),0));}
    static std::array<uint8_t,12> nonce(uint8_t dir,uint64_t n){std::array<uint8_t,12> v{};v[0]=dir;for(int i=0;i<8;++i)v[size_t(4+i)]=uint8_t(n>>(8*i));return v;}
    bool seal(const Bytes& plain,Bytes& out){if(!key||plain.empty())return false;auto iv=nonce(sendDir,sent++);out.assign(plain.size()+16,0);BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce=iv.data();info.cbNonce=12;info.pbTag=out.data()+plain.size();info.cbTag=16;ULONG got=0;
        return success(BCryptEncrypt(key,const_cast<PUCHAR>(plain.data()),ULONG(plain.size()),&info,nullptr,0,out.data(),ULONG(plain.size()),&got,0))&&got==plain.size();}
    bool open(const Bytes& in,Bytes& plain){if(!key||in.size()<=16)return false;auto iv=nonce(recvDir,received++);const size_t n=in.size()-16;plain.assign(n,0);BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce=iv.data();info.cbNonce=12;info.pbTag=const_cast<PUCHAR>(in.data()+n);info.cbTag=16;ULONG got=0;
        return success(BCryptDecrypt(key,const_cast<PUCHAR>(in.data()),ULONG(n),&info,nullptr,0,plain.data(),ULONG(n),&got,0))&&got==n;}};
// Waiting on a socket: in fifth-of-a-second slices, so a transfer stopped from another thread (the thread's
// `abort` flag) ends promptly; a blocking call on Windows does not wake when the socket is shut down.
// `limit` is the longest wait for the socket to be ready, set by timeout().
struct Wire{const std::atomic<bool>* abort=nullptr;int limit=15000;};
thread_local Wire wire;
bool ready(SOCKET s,bool write){
    for(int waited=0;waited<wire.limit;waited+=200){if(wire.abort&&wire.abort->load())return false;
        fd_set set;FD_ZERO(&set);FD_SET(s,&set);fd_set failed;FD_ZERO(&failed);FD_SET(s,&failed);timeval slice{0,200000};
        const int r=select(0,write?nullptr:&set,write?&set:nullptr,&failed,&slice);if(r<0||FD_ISSET(s,&failed))return false;if(r>0)return true;}
    return false;
}
// Framing: a 32-bit little-endian length, then the bytes.
bool sendAll(SOCKET s,const uint8_t* p,size_t n){while(n){if(!ready(s,true))return false;const int k=::send(s,reinterpret_cast<const char*>(p),int(std::min<size_t>(n,64*1024)),0);if(k<=0)return false;p+=k;n-=size_t(k);}return true;}
bool recvAll(SOCKET s,uint8_t* p,size_t n){while(n){if(!ready(s,false))return false;const int k=::recv(s,reinterpret_cast<char*>(p),int(std::min<size_t>(n,1<<20)),0);if(k<=0)return false;p+=k;n-=size_t(k);}return true;}
// Whether the other side has closed its end (checked while a person decides, without reading anything).
bool closedByPeer(SOCKET s){fd_set set;FD_ZERO(&set);FD_SET(s,&set);timeval now{0,0};if(select(0,&set,nullptr,nullptr,&now)!=1)return false;char c;const int k=::recv(s,&c,1,MSG_PEEK);return k<=0;}
bool sendFrame(SOCKET s,const Bytes& b){if(b.size()>maxFrame)return false;uint8_t h[4];for(int i=0;i<4;++i)h[i]=uint8_t(b.size()>>(8*i));return sendAll(s,h,4)&&sendAll(s,b.data(),b.size());}
bool recvFrame(SOCKET s,Bytes& b){uint8_t h[4];if(!recvAll(s,h,4))return false;const uint32_t n=uint32_t(h[0])|uint32_t(h[1])<<8|uint32_t(h[2])<<16|uint32_t(h[3])<<24;if(n>maxFrame)return false;b.assign(n,0);return recvAll(s,b.data(),n);}
bool sealed(SOCKET s,Channel& c,const Bytes& plain){Bytes out;return c.seal(plain,out)&&sendFrame(s,out);}
bool opened(SOCKET s,Channel& c,Bytes& plain){Bytes in;return recvFrame(s,in)&&c.open(in,plain);}
void timeout(SOCKET s,int ms){wire.limit=ms;const DWORD t=DWORD(ms);setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&t),sizeof(t));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&t),sizeof(t));}
SOCKET connectTo(const std::string& address,uint16_t port){
    addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;addrinfo* found=nullptr;
    if(getaddrinfo(address.c_str(),std::to_string(port).c_str(),&hints,&found)!=0||!found)return INVALID_SOCKET;
    SOCKET s=socket(found->ai_family,found->ai_socktype,found->ai_protocol);if(s==INVALID_SOCKET){freeaddrinfo(found);return s;}
    u_long nonblocking=1;ioctlsocket(s,FIONBIO,&nonblocking);connect(s,found->ai_addr,int(found->ai_addrlen));freeaddrinfo(found);
    fd_set writable,failed;FD_ZERO(&writable);FD_ZERO(&failed);FD_SET(s,&writable);FD_SET(s,&failed);timeval wait{5,0};
    const bool ok=select(0,nullptr,&writable,&failed,&wait)==1&&FD_ISSET(s,&writable);if(!ok){closesocket(s);return INVALID_SOCKET;}
    nonblocking=0;ioctlsocket(s,FIONBIO,&nonblocking);return s;
}
std::wstring sizeText(uint64_t b){wchar_t t[32];if(b<1024)swprintf(t,32,L"%llu bytes",static_cast<unsigned long long>(b));else if(b<1024*1024)swprintf(t,32,L"%.0f KB",b/1024.);else if(b<(1ull<<30))swprintf(t,32,L"%.1f MB",b/1048576.);else swprintf(t,32,L"%.2f GB",b/1073741824.);return t;}
fs::path uniquePath(const fs::path& dir,const std::wstring& name){fs::path p=dir/name;std::error_code e;if(!fs::exists(p,e))return p;const bool folder=fs::is_directory(p,e);const std::wstring stem=folder?name:p.stem().wstring(),ext=folder?L"":p.extension().wstring();
    for(int i=2;i<1000;++i){fs::path q=dir/(stem+L" ("+std::to_wstring(i)+L")"+ext);if(!fs::exists(q,e))return q;}return dir/(stem+L" ("+std::to_wstring(GetTickCount64())+L")"+ext);}
void putF64(Bytes& b,double v){uint64_t u=0;std::memcpy(&u,&v,8);put64(b,u);}
double getF64(const uint8_t* p){const uint64_t u=get64(p);double v=0;std::memcpy(&v,&u,8);return std::isfinite(v)?v:0;}
void put32(Bytes& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(uint8_t(v>>(8*i)));}
uint32_t get32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
// Frames inside a transfer (all sealed): the offer, then per file its header, data and end, then the batch end.
constexpr uint8_t frameData=1,frameEnd=2,frameHeader=3,frameBatchEnd=4,frameOffer=1,frameMusic=5;
// Answers: 1 yes, 0 no, 2 (files) not enough space / (music) yes and send the song's file.
constexpr uint64_t maxTotal=1ull<<40;constexpr uint32_t maxFiles=20000;
// One file of an outgoing transfer: where it is, its path as sent ("Photos/a.jpg") and its size.
struct Item{fs::path path;std::string rel;uint64_t size=0;};
bool reparse(const fs::path& p){const DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT);}
// Everything under the dropped paths: files as they are, folders with their trees (links and junctions are not followed).
bool collect(const std::vector<std::wstring>& paths,std::vector<Item>& items,std::wstring& why){
    std::error_code e;
    for(auto& raw:paths){const fs::path p(raw);const std::wstring name=p.filename().wstring();if(name.empty())continue;const auto status=fs::symlink_status(p,e);if(e){why=L"Couldn't read "+name;return false;}
        if(fs::is_regular_file(status)){const uint64_t size=fs::file_size(p,e);if(e){why=L"Couldn't read "+name;return false;}items.push_back({p,utf8(name),size});}
        else if(fs::is_directory(status)&&!reparse(p)){
            for(fs::recursive_directory_iterator it(p,fs::directory_options::skip_permission_denied,e),end;!e&&it!=end;it.increment(e)){
                const auto& entry=*it;std::error_code s;const auto st=entry.symlink_status(s);if(s)continue;
                if(fs::is_directory(st)){if(reparse(entry.path())||it.depth()>=22)it.disable_recursion_pending();continue;}
                if(!fs::is_regular_file(st))continue;const uint64_t size=entry.file_size(s);if(s)continue;
                std::wstring rel=name+L"/"+entry.path().lexically_relative(p).generic_wstring();items.push_back({entry.path(),utf8(rel),size});
                if(items.size()>maxFiles){why=L"Up to 20,000 files can be sent at once";return false;}}
            if(e){why=L"Couldn't read the folder "+name;return false;}}
        else{why=L"Only files and folders can be sent";return false;}
        if(items.size()>maxFiles){why=L"Up to 20,000 files can be sent at once";return false;}}
    if(items.empty()&&why.empty())why=L"There's nothing to send in it";
    return !items.empty();
}
}
std::wstring safeShareName(const std::wstring& name){
    std::wstring n=name;if(auto cut=n.find_last_of(L"/\\");cut!=std::wstring::npos)n=n.substr(cut+1);
    std::wstring out;for(wchar_t c:n)out+=(c<32||c==127||std::wcschr(L"<>:\"/\\|?*",c))?L'_':c;
    while(!out.empty()&&(out.back()==L'.'||out.back()==L' '))out.pop_back();while(!out.empty()&&(out.front()==L'.'||out.front()==L' '))out.erase(0,1);
    if(out.size()>120){const auto dot=out.rfind(L'.');const std::wstring ext=dot!=std::wstring::npos&&out.size()-dot<=12?out.substr(dot):L"";out=out.substr(0,120-ext.size())+ext;}
    if(out.empty())return L"file";
    std::wstring stem=out.substr(0,out.find(L'.'));for(auto& c:stem)c=wchar_t(std::towupper(c));
    static const wchar_t* reserved[]={L"CON",L"PRN",L"AUX",L"NUL",L"COM1",L"COM2",L"COM3",L"COM4",L"COM5",L"COM6",L"COM7",L"COM8",L"COM9",L"LPT1",L"LPT2",L"LPT3",L"LPT4",L"LPT5",L"LPT6",L"LPT7",L"LPT8",L"LPT9"};
    for(auto* r:reserved)if(stem==r)return L"_"+out;
    return out;
}
std::vector<std::wstring> safeSharePath(const std::wstring& path){
    std::vector<std::wstring> parts;size_t at=0;
    while(at<=path.size()){size_t end=path.find_first_of(L"/\\",at);if(end==std::wstring::npos)end=path.size();
        std::wstring part=path.substr(at,end-at);at=end+1;
        std::wstring trimmed=part;while(!trimmed.empty()&&(trimmed.back()==L' '||trimmed.back()==L'.'))trimmed.pop_back();while(!trimmed.empty()&&(trimmed.front()==L' '))trimmed.erase(0,1);
        if(trimmed.empty())continue;
        parts.push_back(safeShareName(part));if(parts.size()>=24)break;}
    return parts;
}
std::wstring shareTitle(const std::vector<std::wstring>& names){
    if(names.empty())return L"Nothing";if(names.size()==1)return names[0];
    return names[0]+L" and "+std::to_wstring(names.size()-1)+L" more";
}
struct ShareService::Core:std::enable_shared_from_this<Core>{
    struct Peer{std::wstring name;std::string address;uint16_t port=0;int version=1;Clock::time_point seen{};bool online=false;Bytes key;/* the paired public key; empty when not paired */};
    // A person's answer (pairing or an offer), waited for by a session thread.
    struct Decision{std::mutex m;std::condition_variable cv;int value=-1;
        void set(int v){{std::lock_guard lock(m);if(value<0)value=v;}cv.notify_all();}
        int wait(int secondsToWait){std::unique_lock lock(m);cv.wait_for(lock,std::chrono::seconds(secondsToWait),[&]{return value>=0;});return value<0?0:value;}};
    // A transfer under way, either way: its socket (to stop it) and whether this PC stopped it.
    struct Live{SOCKET socket=INVALID_SOCKET;std::shared_ptr<std::atomic<bool>> stop=std::make_shared<std::atomic<bool>>(false);};
    struct Session{Bytes peerId,peerPub;std::wstring peerName;Channel channel;uint32_t code=0;bool rejected=false,outdated=false;};
    HWND notify=nullptr;ShareOptions o;bool wsa=false,ok=false;std::string failure;
    Bytes id,pub;BCRYPT_KEY_HANDLE key=nullptr;
    mutable std::mutex m;std::map<std::string,Peer> peers;std::vector<ShareEvent> events;std::shared_ptr<Decision> pairing;std::map<uint32_t,std::shared_ptr<Decision>> offers;std::map<uint32_t,Live> live;uint32_t nextTransfer=1;std::set<SOCKET> open;
    std::atomic<bool> stopping{false};SOCKET udp=INVALID_SOCKET,listener=INVALID_SOCKET;std::thread discovery,listening;
    ~Core(){if(key)BCryptDestroyKey(key);if(wsa)WSACleanup();}
    fs::path identityFile()const{return fs::path(o.folder)/L"share-identity.nexus";}
    fs::path peersFile()const{return fs::path(o.folder)/L"share-peers.nexus";}
    void post(ShareEvent e){{std::lock_guard lock(m);events.push_back(std::move(e));}if(notify)PostMessageW(notify,ShareMessage,0,0);}
    void postPeers(){ShareEvent e;e.kind=ShareEvent::Kind::Peers;post(std::move(e));}
    void fail(ShareEvent::Kind kind,const std::string& peer,const std::wstring& name,const std::wstring& file,const std::wstring& detail,uint32_t transfer=0,bool outgoing=false){ShareEvent e;e.kind=kind;e.peer=peer;e.name=name;e.file=file;e.detail=detail;e.transfer=transfer;e.outgoing=outgoing;post(std::move(e));}
    void track(SOCKET s){std::lock_guard lock(m);open.insert(s);}
    void untrack(SOCKET s){std::lock_guard lock(m);open.erase(s);}
    uint32_t newTransfer(){std::lock_guard lock(m);const uint32_t t=nextTransfer++;live[t]=Live{};return t;}
    // Attaches a transfer's socket (and its stop flag to this thread's waits); false when it was stopped before it got one.
    bool attach(uint32_t t,SOCKET s){std::lock_guard lock(m);auto it=live.find(t);if(it==live.end()||it->second.stop->load())return false;it->second.socket=s;wire.abort=it->second.stop.get();return true;}
    bool stopped(uint32_t t)const{std::lock_guard lock(m);auto it=live.find(t);return it==live.end()||it->second.stop->load();}
    // Waits for a person's answer, giving up (0) if the other PC closes the connection meanwhile.
    int decide(SOCKET s,const std::shared_ptr<Decision>& d,int seconds,bool& gone){gone=false;
        for(int k=0;k<seconds*4;++k){{std::unique_lock lock(d->m);if(d->cv.wait_for(lock,std::chrono::milliseconds(250),[&]{return d->value>=0;}))return d->value;}
            if(closedByPeer(s)){gone=true;d->set(0);return 0;}}
        d->set(0);return 0;}
    void finish(uint32_t t){std::lock_guard lock(m);live.erase(t);offers.erase(t);wire.abort=nullptr;}
    std::wstring nameOf(const std::string& peer,const std::wstring& fallback)const{std::lock_guard lock(m);auto it=peers.find(peer);return it!=peers.end()&&!it->second.name.empty()?it->second.name:cleanName(fallback);}
    bool identity(){
        {std::ifstream in(identityFile());std::string line,idText,keyText;while(std::getline(in,line)){if(line.starts_with("id="))idText=line.substr(3);else if(line.starts_with("key="))keyText=line.substr(4);}
            Bytes blob=dpapi(unhex(keyText),true);const Bytes loaded=unhex(idText);
            if(loaded.size()==16&&!blob.empty()&&success(BCryptImportKeyPair(providers().ecdh,nullptr,BCRYPT_ECCPRIVATE_BLOB,&key,blob.data(),ULONG(blob.size()),0)))id=loaded;
            wipe(blob);}
        if(!key){
            id=randomBytes(16);if(id.size()!=16||!success(BCryptGenerateKeyPair(providers().ecdh,&key,256,0))||!success(BCryptFinalizeKeyPair(key,0)))return false;
            Bytes blob=exportKey(key,BCRYPT_ECCPRIVATE_BLOB);const Bytes sealedKey=dpapi(blob,false);wipe(blob);if(sealedKey.empty())return false;
            std::error_code e;fs::create_directories(o.folder,e);std::ofstream out(identityFile(),std::ios::trunc);out<<"id="<<hex(id)<<"\nkey="<<hex(sealedKey)<<"\n";if(!out)return false;}
        pub=publicOf(key);return pub.size()==64;
    }
    void loadPeers(){std::ifstream in(peersFile());std::string line;while(std::getline(in,line)){const auto a=line.find('\t'),b=line.find('\t',a==std::string::npos?a:a+1);if(a==std::string::npos||b==std::string::npos)continue;
        const std::string peer=line.substr(0,a);const Bytes k=unhex(line.substr(a+1,b-a-1));const Bytes nameBytes=unhex(line.substr(b+1));if(unhex(peer).size()!=16||k.size()!=64)continue;
        auto& p=peers[peer];p.key=k;p.version=shareProtocol;p.name=cleanName(wide(std::string(nameBytes.begin(),nameBytes.end())));}}
    // Called with m held.
    void savePeers(){std::error_code e;fs::create_directories(o.folder,e);std::ofstream out(peersFile(),std::ios::trunc);for(auto& [peer,p]:peers)if(!p.key.empty()){const std::string n=utf8(p.name);out<<peer<<'\t'<<hex(p.key)<<'\t'<<hex(reinterpret_cast<const uint8_t*>(n.data()),n.size())<<'\n';}}
    bool start(){
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0){failure="Networking is unavailable";return false;}wsa=true;
        if(!providers().ok){failure="Encryption is unavailable";return false;}
        if(!identity()){failure="The sharing key could not be created";return false;}
        loadPeers();
        if(o.name.empty()){wchar_t n[256]{};DWORD size=256;if(GetComputerNameExW(ComputerNamePhysicalDnsHostname,n,&size))o.name=n;}o.name=cleanName(o.name);
        listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);BOOL exclusive=TRUE;setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
        sockaddr_in at{};at.sin_family=AF_INET;at.sin_addr.s_addr=htonl(o.loopback?INADDR_LOOPBACK:INADDR_ANY);at.sin_port=htons(o.tcpPort);
        if(listener==INVALID_SOCKET||bind(listener,reinterpret_cast<sockaddr*>(&at),sizeof(at))!=0||::listen(listener,8)!=0){failure="Port "+std::to_string(o.tcpPort)+" is in use";return false;}
        if(o.discovery){udp=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);BOOL on=TRUE;setsockopt(udp,SOL_SOCKET,SO_BROADCAST,reinterpret_cast<const char*>(&on),sizeof(on));setsockopt(udp,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&on),sizeof(on));
            sockaddr_in u{};u.sin_family=AF_INET;u.sin_addr.s_addr=htonl(INADDR_ANY);u.sin_port=htons(o.udpPort);if(udp==INVALID_SOCKET||bind(udp,reinterpret_cast<sockaddr*>(&u),sizeof(u))!=0){failure="Discovery port "+std::to_string(o.udpPort)+" is in use";return false;}}
        ok=true;auto self=shared_from_this();
        listening=std::thread([self]{self->listen();});if(o.discovery)discovery=std::thread([self]{self->discover();});
        return true;
    }
    void stop(){
        stopping=true;if(listener!=INVALID_SOCKET){closesocket(listener);listener=INVALID_SOCKET;}if(udp!=INVALID_SOCKET){closesocket(udp);udp=INVALID_SOCKET;}
        if(listening.joinable())listening.join();if(discovery.joinable())discovery.join();
        std::lock_guard lock(m);for(SOCKET s:open)shutdown(s,SD_BOTH);if(pairing)pairing->set(0);for(auto& [t,d]:offers)d->set(0);
    }
    // Discovery: an announcement every 3 s; a PC not heard from for 12 s is offline. The port line carries
    // ";2", the protocol (an older version reads the number before it and ignores the rest).
    void discover(){
        const std::string announce="ARNAVSHARE1\n"+hex(id)+"\n"+std::to_string(o.tcpPort)+";"+std::to_string(shareProtocol)+"\n"+utf8(o.name);auto last=Clock::now()-std::chrono::seconds(10);
        while(!stopping){
            if(Clock::now()-last>=std::chrono::seconds(3)){last=Clock::now();sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=htonl(INADDR_BROADCAST);to.sin_port=htons(o.udpPort);
                sendto(udp,announce.data(),int(announce.size()),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
                bool changed=false;{std::lock_guard lock(m);for(auto& [peer,p]:peers)if(p.online&&Clock::now()-p.seen>std::chrono::seconds(12)){p.online=false;changed=true;}}if(changed)postPeers();}
            fd_set readable;FD_ZERO(&readable);FD_SET(udp,&readable);timeval wait{0,500000};if(select(0,&readable,nullptr,nullptr,&wait)!=1)continue;
            char buffer[600];sockaddr_in from{};int fromSize=sizeof(from);const int n=recvfrom(udp,buffer,sizeof(buffer),0,reinterpret_cast<sockaddr*>(&from),&fromSize);if(n<=0)continue;
            const std::string text(buffer,size_t(n));std::vector<std::string> parts;size_t start=0;for(int k=0;k<3;++k){const auto at=text.find('\n',start);if(at==std::string::npos)break;parts.push_back(text.substr(start,at-start));start=at+1;}
            if(parts.size()!=3||parts[0]!="ARNAVSHARE1"||unhex(parts[1]).size()!=16||parts[1]==hex(id))continue;
            const auto semi=parts[2].find(';');const int port=std::atoi(parts[2].substr(0,semi).c_str());if(port<=0||port>65535)continue;
            const int version=semi==std::string::npos?1:std::clamp(std::atoi(parts[2].c_str()+semi+1),1,99);
            char address[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&from.sin_addr,address,sizeof(address));const std::wstring name=cleanName(wide(text.substr(start)));
            bool changed=false;{std::lock_guard lock(m);auto& p=peers[parts[1]];changed=!p.online||p.address!=address||p.port!=port||p.version!=version||(p.key.empty()&&p.name!=name);
                p.address=address;p.port=uint16_t(port);p.version=version;p.online=true;p.seen=Clock::now();if(p.key.empty()||p.name.empty())p.name=name;}
            if(changed)postPeers();
        }
    }
    void listen(){
        while(!stopping){sockaddr_in from{};int size=sizeof(from);SOCKET s=accept(listener,reinterpret_cast<sockaddr*>(&from),&size);
            if(s==INVALID_SOCKET){if(stopping)break;std::this_thread::sleep_for(std::chrono::milliseconds(100));continue;}
            track(s);std::thread([self=shared_from_this(),s]{self->incoming(s);self->untrack(s);closesocket(s);}).detach();}
    }
    // Both sides' keys, then the session key and the pairing code from both public keys and both nonces.
    bool keys(Session& ss,bool initiator,const Bytes& mine,const Bytes& theirs){
        Bytes secret;if(!agree(key,ss.peerPub,secret))return false;
        const Bytes &nc=initiator?mine:theirs,&ns=initiator?theirs:mine,&pc=initiator?pub:ss.peerPub,&ps=initiator?ss.peerPub:pub,&ic=initiator?id:ss.peerId,&is=initiator?ss.peerId:id;
        Bytes k=Sha().add(std::string("arnav-share-v1")).add(secret).add(nc).add(ns).add(ic).add(is).done();
        const Bytes c=Sha().add(std::string("arnav-pair-v1")).add(pc).add(ps).add(nc).add(ns).done();wipe(secret);if(k.size()!=32||c.size()!=32)return false;
        ss.code=(uint32_t(c[0])|uint32_t(c[1])<<8|uint32_t(c[2])<<16|uint32_t(c[3])<<24)%1000000;const bool made=ss.channel.init(k,initiator);wipe(k);return made;
    }
    // The initiator commits to its nonce (a hash) before it sees the other's, so neither side can steer the code.
    bool greet(SOCKET s,uint8_t mode,Session& ss){
        const Bytes nonce=randomBytes(32);if(nonce.size()!=32)return false;
        Bytes hello(magic,magic+4);hello.push_back(protocolVersion);hello.push_back(mode);append(hello,id);append(hello,pub);append(hello,Sha().add(nonce).done());append(hello,utf8(o.name));
        Bytes reply;if(!sendFrame(s,hello)||!recvFrame(s,reply)||reply.size()<6||std::memcmp(reply.data(),magic,4)!=0)return false;
        if(reply[4]!=protocolVersion||reply[5]==2){ss.outdated=true;return false;}
        if(reply[5]!=0){ss.rejected=true;return false;}
        if(reply.size()<6+16+64+32)return false;
        ss.peerId.assign(reply.begin()+6,reply.begin()+22);ss.peerPub.assign(reply.begin()+22,reply.begin()+86);const Bytes theirs(reply.begin()+86,reply.begin()+118);ss.peerName=cleanName(wide(std::string(reply.begin()+118,reply.end())));
        return sendFrame(s,nonce)&&keys(ss,true,nonce,theirs);
    }
    // The responder takes pairing only when none is under way, and files and music only from a paired PC with its paired key.
    // A PC speaking another protocol is told so (answer 2), so it can say which PC needs updating.
    bool welcome(SOCKET s,Session& ss,uint8_t& mode,std::shared_ptr<Decision>& claim){
        Bytes hello;if(!recvFrame(s,hello)||hello.size()<6||std::memcmp(hello.data(),magic,4)!=0)return false;
        if(hello[4]!=protocolVersion){Bytes no(magic,magic+4);no.push_back(protocolVersion);no.push_back(2);sendFrame(s,no);return false;}
        if(hello.size()<6+16+64+32)return false;
        mode=hello[5];ss.peerId.assign(hello.begin()+6,hello.begin()+22);ss.peerPub.assign(hello.begin()+22,hello.begin()+86);const Bytes commit(hello.begin()+86,hello.begin()+118);
        ss.peerName=cleanName(wide(std::string(hello.begin()+118,hello.end())));if(ss.peerId==id)return false;
        bool allowed=false;{std::lock_guard lock(m);
            if(mode==modePair&&!pairing){pairing=claim=std::make_shared<Decision>();allowed=true;}
            else if(mode==modeSend||mode==modeMusic){auto it=peers.find(hex(ss.peerId));allowed=it!=peers.end()&&!it->second.key.empty()&&it->second.key==ss.peerPub;}}
        if(!allowed){Bytes no(magic,magic+4);no.push_back(protocolVersion);no.push_back(1);sendFrame(s,no);return false;}
        const Bytes nonce=randomBytes(32);if(nonce.size()!=32)return false;
        Bytes reply(magic,magic+4);reply.push_back(protocolVersion);reply.push_back(0);append(reply,id);append(reply,pub);append(reply,nonce);append(reply,utf8(o.name));
        Bytes theirs;if(!sendFrame(s,reply)||!recvFrame(s,theirs)||theirs.size()!=32||Sha().add(theirs).done()!=commit)return false;
        return keys(ss,false,nonce,theirs);
    }
    void releasePairing(const std::shared_ptr<Decision>& d){std::lock_guard lock(m);if(pairing==d)pairing.reset();}
    // Both PCs show the code; each person's answer goes to the other, sealed. Paired only when both said yes.
    void pairSession(SOCKET s,Session& ss,const std::shared_ptr<Decision>& d){
        const std::string peer=hex(ss.peerId);ShareEvent e;e.kind=ShareEvent::Kind::PairCode;e.peer=peer;e.name=ss.peerName;e.code=ss.code;post(e);
        timeout(s,90000);const int mine=d->wait(60);Bytes theirs;const bool talked=sealed(s,ss.channel,Bytes{uint8_t(mine==1?1:0)})&&opened(s,ss.channel,theirs)&&theirs.size()==1;
        const bool both=talked&&mine==1&&theirs[0]==1;
        if(both){std::lock_guard lock(m);auto& p=peers[peer];p.key=ss.peerPub;p.version=shareProtocol;if(p.name.empty()||p.name==L"A PC")p.name=ss.peerName;savePeers();}
        releasePairing(d);
        fail(both?ShareEvent::Kind::Paired:ShareEvent::Kind::PairFailed,peer,ss.peerName,{},both?L"You can send files between these PCs now":!talked?L"The other PC stopped answering":mine!=1?L"Not paired":L"Not confirmed on the other PC");postPeers();
    }
    void incoming(SOCKET s){
        timeout(s,15000);Session ss;uint8_t mode=0;std::shared_ptr<Decision> claim;
        if(!welcome(s,ss,mode,claim)){if(claim)releasePairing(claim);return;}
        if(mode==modePair)pairSession(s,ss,claim);else if(mode==modeMusic)receiveMusic(s,ss);else receive(s,ss);
    }
    // Progress for a transfer: at most one event per percent and per tenth of a second (and always the last).
    struct Progress{Core& core;ShareEvent base;uint64_t total=0,done=0;int shown=-1;Clock::time_point at{};
        void add(uint64_t n){done+=n;const int percent=total?int(done*100/total):100;const auto now=Clock::now();
            if(percent!=shown&&(now-at>=std::chrono::milliseconds(100)||done==total)){shown=percent;at=now;ShareEvent e=base;e.kind=ShareEvent::Kind::Progress;e.size=total;e.done=done;core.post(std::move(e));}}};
    // One file into a sealed stream: its header, its data in chunks, then its size and SHA-256.
    bool streamFile(SOCKET s,Channel& c,const Item& item,Progress& progress,uint32_t transfer){
        std::ifstream in(item.path,std::ios::binary);if(!in)return false;
        Bytes header{frameHeader};put64(header,item.size);append(header,item.rel);if(!sealed(s,c,header))return false;
        Sha hash;uint64_t done=0;Bytes frame;
        while(done<item.size){if(stopped(transfer))return false;frame.assign(1+std::min<uint64_t>(chunkSize,item.size-done),0);frame[0]=frameData;in.read(reinterpret_cast<char*>(frame.data()+1),std::streamsize(frame.size()-1));
            if(in.gcount()!=std::streamsize(frame.size()-1)||!sealed(s,c,frame))return false;hash.add(frame.data()+1,frame.size()-1);done+=frame.size()-1;progress.add(frame.size()-1);}
        Bytes end{frameEnd};put64(end,item.size);append(end,hash.done());return sealed(s,c,end);
    }
    // One incoming file (after its header): written as .arnavpart, checked, then given its name.
    bool takeFile(SOCKET s,Channel& c,uint64_t size,const fs::path& target,fs::path& saved,Progress& progress,uint32_t transfer){
        std::error_code error;fs::create_directories(target.parent_path(),error);
        const fs::path part=uniquePath(target.parent_path(),target.filename().wstring()+L".arnavpart");bool done=false;
        {std::ofstream out(part,std::ios::binary|std::ios::trunc);Sha hash;uint64_t got=0;
            while(out){if(stopped(transfer))break;Bytes f;if(!opened(s,c,f)||f.empty())break;
                if(f[0]==frameData){const size_t n=f.size()-1;if(got+n>size)break;out.write(reinterpret_cast<const char*>(f.data()+1),std::streamsize(n));hash.add(f.data()+1,n);got+=n;progress.add(n);}
                else if(f[0]==frameEnd&&f.size()==1+8+32){done=get64(f.data()+1)==size&&got==size&&hash.done()==Bytes(f.begin()+9,f.end());break;}
                else break;}
            out.close();done=done&&!out.fail();}
        if(!done){fs::remove(part,error);return false;}
        saved=uniquePath(target.parent_path(),target.filename().wstring());fs::rename(part,saved,error);if(error){fs::remove(part,error);return false;}return true;
    }
    static bool enoughSpace(const fs::path& dir,uint64_t size){ULARGE_INTEGER free{};std::error_code e;fs::create_directories(dir,e);if(!GetDiskFreeSpaceExW(dir.c_str(),&free,nullptr,nullptr))return true;return free.QuadPart>size+(64ull<<20);}
    void receive(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);const std::wstring from=nameOf(peer,ss.peerName);
        Bytes offer;if(!opened(s,ss.channel,offer)||offer.size()<1+4+8+1+1||offer[0]!=frameOffer)return;
        const uint32_t count=get32(offer.data()+1);const uint64_t total=get64(offer.data()+5);const bool folder=offer[13]!=0;
        const std::wstring title=cleanName(wide(std::string(offer.begin()+14,offer.end())));if(!count||count>maxFiles||total>maxTotal)return;
        auto d=std::make_shared<Decision>();const uint32_t transfer=newTransfer();{std::lock_guard lock(m);offers[transfer]=d;}attach(transfer,s);
        {ShareEvent e;e.kind=ShareEvent::Kind::Offer;e.peer=peer;e.name=from;e.file=title;e.size=total;e.count=count;e.folder=folder;e.transfer=transfer;post(e);}
        timeout(s,90000);bool gone=false;int yes=decide(s,d,60,gone);{std::lock_guard lock(m);offers.erase(transfer);}
        // The other PC stopped before this one answered: the offer goes away.
        if(gone){finish(transfer);fail(ShareEvent::Kind::Failed,peer,from,title,from+L" stopped sending it",transfer);return;}
        const fs::path dir=o.downloads;const bool room=yes!=1||enoughSpace(dir,total);if(yes==1&&!room)yes=2;
        const bool told=sealed(s,ss.channel,Bytes{uint8_t(yes)});
        if(!told||yes!=1){const bool mine=stopped(transfer);finish(transfer);
            if(yes==2)fail(ShareEvent::Kind::Failed,peer,from,title,L"There isn't room in Downloads for "+sizeText(total),transfer);
            else if(yes==1)fail(ShareEvent::Kind::Failed,peer,from,title,mine?L"You stopped it":from+L" stopped sending it",transfer);return;}
        timeout(s,30000);Progress progress{*this,{}};progress.base.peer=peer;progress.base.name=from;progress.base.file=title;progress.base.transfer=transfer;progress.base.count=count;progress.total=total;
        // Top-level folders get a free name in Downloads once ("Photos (2)"); files inside keep their paths.
        std::map<std::wstring,fs::path> tops;std::vector<fs::path> shown;uint32_t files=0;bool whole=false;std::wstring why;
        for(;;){Bytes f;if(stopped(transfer)||!opened(s,ss.channel,f)||f.empty())break;
            if(f[0]==frameBatchEnd){whole=files==count&&progress.done==total;break;}
            if(f[0]!=frameHeader||f.size()<10||files>=count)break;
            const uint64_t size=get64(f.data()+1);const auto parts=safeSharePath(wide(std::string(f.begin()+9,f.end())));if(parts.empty()||size>maxFile||progress.done+size>total)break;
            fs::path target;
            if(parts.size()==1)target=dir/parts[0];
            else{auto it=tops.find(parts[0]);if(it==tops.end()){it=tops.emplace(parts[0],uniquePath(dir,parts[0])).first;shown.push_back(it->second);}target=it->second;for(size_t k=1;k<parts.size();++k)target/=parts[k];}
            fs::path saved;if(!takeFile(s,ss.channel,size,target,saved,progress,transfer)){why=L"couldn't be saved";break;}
            ++files;if(parts.size()==1)shown.push_back(saved);}
        const bool stoppedHere=stopped(transfer);
        if(whole)sealed(s,ss.channel,Bytes{1});
        finish(transfer);
        if(!whole){fail(ShareEvent::Kind::Failed,peer,from,title,stoppedHere?L"You stopped it":files?std::to_wstring(files)+L" of "+std::to_wstring(count)+L" files arrived from "+from:L"It didn't arrive whole from "+from,transfer);return;}
        ShareEvent e;e.kind=ShareEvent::Kind::Received;e.peer=peer;e.name=from;e.count=count;e.size=total;e.transfer=transfer;e.folder=folder;
        e.file=shown.size()==1?shown[0].filename().wstring():title;e.detail=shown.empty()?dir.wstring():shown[0].wstring();post(e);
    }
    // A sender connection: reached, greeted, and checked against the pairing. False (with why) otherwise.
    bool reach(const Peer& target,const std::string& peer,uint8_t mode,uint32_t transfer,SOCKET& s,Session& ss,std::wstring& why){
        s=connectTo(target.address,target.port);if(s==INVALID_SOCKET){why=L"Couldn't reach "+target.name;return false;}
        track(s);if(!attach(transfer,s)){why=L"You stopped it";return false;}timeout(s,15000);
        if(!greet(s,mode,ss)){why=ss.outdated?L"Update Arnav Island on "+target.name+L" to share with it":ss.rejected?target.name+L" doesn't have this PC paired. Pair again from Nearby.":L"Couldn't reach "+target.name;return false;}
        if(hex(ss.peerId)!=peer||ss.peerPub!=target.key){why=target.name+L" answered with a different key. Pair again from Nearby.";return false;}
        return true;
    }
    bool ready(const std::string& peer,Peer& target,std::wstring& why){
        {std::lock_guard lock(m);auto it=peers.find(peer);if(it!=peers.end())target=it->second;}
        if(target.key.empty()){why=L"Pair with this PC first";return false;}
        if(!target.online){why=target.name+L" isn't on this network right now";return false;}
        if(target.version<shareProtocol){why=L"Update Arnav Island on "+target.name+L" to share with it";return false;}
        return true;
    }
    void sendBatch(const std::string& peer,const std::vector<std::wstring>& paths,uint32_t transfer){
        std::vector<std::wstring> names;for(auto& p:paths){auto n=fs::path(p).filename().wstring();if(!n.empty())names.push_back(n);}const std::wstring title=shareTitle(names);
        Peer target;std::wstring why;std::vector<Item> items;
        if(!ready(peer,target,why)||!collect(paths,items,why)){finish(transfer);fail(ShareEvent::Kind::Failed,peer,target.name,title,why,transfer,true);return;}
        uint64_t total=0;for(auto& i:items){if(i.size>maxFile){finish(transfer);fail(ShareEvent::Kind::Failed,peer,target.name,title,L"Files up to 16 GB can be sent",transfer,true);return;}total+=i.size;}
        if(total>maxTotal){finish(transfer);fail(ShareEvent::Kind::Failed,peer,target.name,title,L"Up to 1 TB can be sent at once",transfer,true);return;}
        bool folder=false;for(auto& p:paths){std::error_code e;if(fs::is_directory(p,e))folder=true;}
        SOCKET s=INVALID_SOCKET;Session ss;bool sent=false;
        if(reach(target,peer,modeSend,transfer,s,ss,why)){
            Bytes offer{frameOffer};put32(offer,uint32_t(items.size()));put64(offer,total);offer.push_back(folder?1:0);append(offer,utf8(title));Bytes reply;timeout(s,90000);
            if(!sealed(s,ss.channel,offer)||!opened(s,ss.channel,reply)||reply.size()!=1)why=stopped(transfer)?L"You stopped it":target.name+L" didn't answer";
            else if(reply[0]==2)why=L"There isn't room on "+target.name+L" for "+sizeText(total);
            else if(reply[0]!=1)why=target.name+L" declined it";
            else{timeout(s,30000);Progress progress{*this,{}};progress.base.peer=peer;progress.base.name=target.name;progress.base.file=title;progress.base.transfer=transfer;progress.base.count=uint32_t(items.size());progress.base.outgoing=true;progress.total=total;
                bool flowing=true;for(auto& item:items)if(!streamFile(s,ss.channel,item,progress,transfer)){flowing=false;break;}
                Bytes ack;timeout(s,60000);sent=flowing&&sealed(s,ss.channel,Bytes{frameBatchEnd})&&opened(s,ss.channel,ack)&&ack.size()==1&&ack[0]==1;
                if(!sent)why=stopped(transfer)?L"You stopped it":L"The transfer to "+target.name+L" didn't finish";}}
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        finish(transfer);
        if(sent){ShareEvent e;e.kind=ShareEvent::Kind::Sent;e.peer=peer;e.name=target.name;e.file=title;e.size=total;e.count=uint32_t(items.size());e.transfer=transfer;e.outgoing=true;e.folder=folder;e.detail=sizeText(total);post(e);}
        else fail(ShareEvent::Kind::Failed,peer,target.name,title,why,transfer,true);
    }
    // Music: what plays, as one sealed frame. The answer 2 asks for the song's own file too.
    void sendMusic(const std::string& peer,const ShareHandoff& music,const std::wstring& file,uint32_t transfer){
        Peer target;std::wstring why;SOCKET s=INVALID_SOCKET;Session ss;int code=-1;
        if(ready(peer,target,why)&&reach(target,peer,modeMusic,transfer,s,ss,why)){
            std::error_code e;uint64_t size=0;if(!file.empty()&&fs::is_regular_file(file,e))size=fs::file_size(file,e);if(e||size>(2ull<<30))size=0;
            Bytes offer{frameMusic};putF64(offer,music.position);putF64(offer,music.duration);offer.push_back(music.playing?1:0);put64(offer,size);
            auto line=[](const std::wstring& v){std::wstring t=v.substr(0,512);for(auto& c:t)if(c==L'\n'||c==L'\r')c=L' ';return t;};
            append(offer,utf8(line(music.title)+L"\n"+line(music.artist)+L"\n"+line(music.album)+L"\n"+line(music.app)+L"\n"+line(size?fs::path(file).filename().wstring():std::wstring())));
            Bytes reply;timeout(s,90000);
            if(!sealed(s,ss.channel,offer)||!opened(s,ss.channel,reply)||reply.size()!=1)why=target.name+L" didn't answer";
            else{code=reply[0]>2?0:reply[0];ShareEvent a;a.kind=ShareEvent::Kind::HandoffAnswered;a.peer=peer;a.name=target.name;a.code=uint32_t(code);a.transfer=transfer;a.outgoing=true;a.handoff=music;post(a);
                if(code==2&&size){timeout(s,30000);Progress progress{*this,{}};progress.base.peer=peer;progress.base.name=target.name;progress.base.file=music.title;progress.base.transfer=transfer;progress.base.count=1;progress.base.outgoing=true;progress.total=size;
                    Item item{fs::path(file),utf8(fs::path(file).filename().wstring()),size};Bytes ack;
                    if(!streamFile(s,ss.channel,item,progress,transfer)||!sealed(s,ss.channel,Bytes{frameBatchEnd})||!opened(s,ss.channel,ack)||ack.size()!=1||ack[0]!=1)why=L"The song didn't reach "+target.name;}}}
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        finish(transfer);
        if(!why.empty())fail(ShareEvent::Kind::Failed,peer,target.name,music.title,why,transfer,true);
    }
    void receiveMusic(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);const std::wstring from=nameOf(peer,ss.peerName);
        Bytes offer;if(!opened(s,ss.channel,offer)||offer.size()<1+8+8+1+8||offer[0]!=frameMusic)return;
        ShareHandoff music;music.position=std::max(0.,getF64(offer.data()+1));music.duration=std::max(0.,getF64(offer.data()+9));music.playing=offer[17]!=0;const uint64_t size=get64(offer.data()+18);
        {const std::wstring text=wide(std::string(offer.begin()+26,offer.end()));std::vector<std::wstring> lines;size_t at=0;for(int k=0;k<5;++k){size_t end=text.find(L'\n',at);if(end==std::wstring::npos)end=text.size();lines.push_back(at<=text.size()?text.substr(at,std::min<size_t>(end-at,512)):L"");at=end+1;}
            music.title=lines[0];music.artist=lines[1];music.album=lines[2];music.app=lines[3];music.fileName=size?safeShareName(lines[4]):L"";music.fileSize=size;}
        if(music.title.empty()||size>(2ull<<30))return;
        auto d=std::make_shared<Decision>();const uint32_t transfer=newTransfer();{std::lock_guard lock(m);offers[transfer]=d;}attach(transfer,s);
        {ShareEvent e;e.kind=ShareEvent::Kind::Handoff;e.peer=peer;e.name=from;e.file=music.title;e.transfer=transfer;e.handoff=music;post(e);}
        timeout(s,90000);bool gone=false;int code=decide(s,d,60,gone);{std::lock_guard lock(m);offers.erase(transfer);}
        if(gone){finish(transfer);fail(ShareEvent::Kind::Failed,peer,from,music.title,from+L" took the music back",transfer);return;}if(code<0||code>2||(code==2&&!size))code=code==2?1:0;
        const fs::path dir=o.handoff.empty()?fs::path(o.folder)/L"Handoff":fs::path(o.handoff);if(code==2&&!enoughSpace(dir,size))code=1;
        if(!sealed(s,ss.channel,Bytes{uint8_t(code)})||code!=2){finish(transfer);return;}
        timeout(s,30000);Progress progress{*this,{}};progress.base.peer=peer;progress.base.name=from;progress.base.file=music.title;progress.base.transfer=transfer;progress.base.count=1;progress.total=size;
        Bytes f;fs::path saved;bool whole=false;
        if(opened(s,ss.channel,f)&&f.size()>=10&&f[0]==frameHeader&&get64(f.data()+1)==size&&takeFile(s,ss.channel,size,dir/music.fileName,saved,progress,transfer)){Bytes end;whole=opened(s,ss.channel,end)&&end.size()==1&&end[0]==frameBatchEnd;}
        if(whole)sealed(s,ss.channel,Bytes{1});
        finish(transfer);
        if(!whole){std::error_code e;if(!saved.empty())fs::remove(saved,e);fail(ShareEvent::Kind::Failed,peer,from,music.title,L"The song didn't arrive whole from "+from,transfer);return;}
        ShareEvent e;e.kind=ShareEvent::Kind::HandoffFile;e.peer=peer;e.name=from;e.file=music.title;e.detail=saved.wstring();e.transfer=transfer;e.handoff=music;post(e);
    }
    void pairWith(const std::string& peer,const std::shared_ptr<Decision>& d){
        Peer target;{std::lock_guard lock(m);auto it=peers.find(peer);if(it!=peers.end())target=it->second;}
        if(target.online&&target.version<shareProtocol){releasePairing(d);fail(ShareEvent::Kind::PairFailed,peer,target.name,{},L"Update Arnav Island on "+target.name+L" first");return;}
        SOCKET s=target.online?connectTo(target.address,target.port):INVALID_SOCKET;
        if(s==INVALID_SOCKET){releasePairing(d);fail(ShareEvent::Kind::PairFailed,peer,target.name,{},L"Couldn't reach "+(target.name.empty()?std::wstring(L"that PC"):target.name));return;}
        track(s);timeout(s,15000);Session ss;
        if(!greet(s,modePair,ss)||hex(ss.peerId)!=peer){releasePairing(d);fail(ShareEvent::Kind::PairFailed,peer,target.name,{},ss.outdated?L"Update Arnav Island on "+target.name+L" first":ss.rejected?target.name+L" is busy pairing":L"Pairing didn't start");}
        else pairSession(s,ss,d);
        untrack(s);closesocket(s);
    }
};
ShareService::ShareService(HWND notify,ShareOptions options):core_(std::make_shared<Core>()){core_->notify=notify;core_->o=std::move(options);core_->start();}
ShareService::~ShareService(){core_->stop();}
bool ShareService::running()const{return core_->ok;}
std::string ShareService::error()const{return core_->failure;}
std::string ShareService::id()const{return hex(core_->id);}
std::vector<SharePeer> ShareService::peers()const{
    std::vector<SharePeer> list;{std::lock_guard lock(core_->m);for(auto& [peer,p]:core_->peers)if(p.online||!p.key.empty())list.push_back({peer,p.name,!p.key.empty(),p.online,p.version});}
    std::stable_sort(list.begin(),list.end(),[](auto& a,auto& b){const int ra=(a.online?0:2)+(a.paired?0:1),rb=(b.online?0:2)+(b.paired?0:1);return ra!=rb?ra<rb:a.name<b.name;});return list;
}
void ShareService::addPeer(const std::string& peer,const std::wstring& name,const std::string& address,uint16_t port,int version){
    {std::lock_guard lock(core_->m);auto& p=core_->peers[peer];if(p.key.empty()||p.name.empty())p.name=cleanName(name);p.address=address;p.port=port;p.version=version;p.online=true;p.seen=Clock::now()+std::chrono::hours(24);}core_->postPeers();}
void ShareService::pair(const std::string& peer){
    std::shared_ptr<Core::Decision> d;{std::lock_guard lock(core_->m);if(core_->pairing||!core_->ok)return;d=core_->pairing=std::make_shared<Core::Decision>();}
    std::thread([core=core_,peer,d]{core->pairWith(peer,d);}).detach();
}
void ShareService::confirmPair(bool yes){std::lock_guard lock(core_->m);if(core_->pairing)core_->pairing->set(yes?1:0);}
void ShareService::forget(const std::string& peer){{std::lock_guard lock(core_->m);auto it=core_->peers.find(peer);if(it!=core_->peers.end()){it->second.key.clear();core_->savePeers();}}core_->postPeers();}
uint32_t ShareService::send(const std::string& peer,const std::vector<std::wstring>& paths){
    if(!core_->ok||paths.empty())return 0;const uint32_t transfer=core_->newTransfer();
    std::thread([core=core_,peer,paths,transfer]{core->sendBatch(peer,paths,transfer);}).detach();return transfer;}
void ShareService::answer(uint32_t transfer,bool accept){std::lock_guard lock(core_->m);auto it=core_->offers.find(transfer);if(it!=core_->offers.end())it->second->set(accept?1:0);}
void ShareService::cancel(uint32_t transfer){
    std::lock_guard lock(core_->m);auto it=core_->live.find(transfer);if(it==core_->live.end())return;it->second.stop->store(true);
    if(auto d=core_->offers.find(transfer);d!=core_->offers.end())d->second->set(0);
    if(it->second.socket!=INVALID_SOCKET)shutdown(it->second.socket,SD_BOTH);
}
uint32_t ShareService::handoff(const std::string& peer,const ShareHandoff& music,const std::wstring& file){
    if(!core_->ok)return 0;const uint32_t transfer=core_->newTransfer();
    std::thread([core=core_,peer,music,file,transfer]{core->sendMusic(peer,music,file,transfer);}).detach();return transfer;}
void ShareService::answerHandoff(uint32_t transfer,int code){std::lock_guard lock(core_->m);auto it=core_->offers.find(transfer);if(it!=core_->offers.end())it->second->set(std::clamp(code,0,2));}
std::vector<ShareEvent> ShareService::take(){std::lock_guard lock(core_->m);std::vector<ShareEvent> out;out.swap(core_->events);return out;}
}
