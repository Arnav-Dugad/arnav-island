#include <winsock2.h>
#include <ws2tcpip.h>
#include "ShareService.h"
#include "ShareRelay.h"
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
constexpr uint8_t modePair='P',modeSend='S',modeMusic='H',modeList='L',modeTake='T',modeRemote='R',modeNotice='N',modeFind='F';
// Revision 2 frames.
constexpr uint8_t frameRequest=0x20,frameReply=0x21,frameNotice=0x30,frameNoticeAck=0x31,frameRing=0x40,frameRingAck=0x41;
// Revision 3: modes and frames. Input (phone to PC): frames 0x60-0x6F. To a phone: an action on one of its notifications,
// the clipboard, a photo for the Shelf. Each is answered [ack, status] (0 done, 1 the notification is gone, 2 failed).
constexpr uint8_t modeInput='I',modeAction='A',modeClip='C',modeCamera='K';
// Revision 6: asking a phone (this PC asks; kept open for more).
constexpr uint8_t modeQuery='Q',frameQuery=0x80,frameQueryReply=0x81;
// Revision 7: a screen, either way (a phone opens it).
constexpr uint8_t modeMirror='V';
constexpr uint8_t frameAction=0x70,frameActionAck=0x71,frameClip=0x50,frameClipAck=0x51,frameCamera=0x42,frameCameraAck=0x43;
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
// Each direction has a key object of its own (the same key): a CNG key object isn't safe to use from two threads at
// once, and a screen seals on one thread (input, feedback) while it opens on another.
struct Channel{BCRYPT_KEY_HANDLE key=nullptr,openKey=nullptr;uint8_t sendDir=1,recvDir=2;uint64_t sent=0,received=0;
    Channel()=default;Channel(const Channel&)=delete;Channel& operator=(const Channel&)=delete;~Channel(){if(key)BCryptDestroyKey(key);if(openKey)BCryptDestroyKey(openKey);}
    bool init(const Bytes& k,bool initiator){sendDir=initiator?1:2;recvDir=initiator?2:1;
        return success(BCryptGenerateSymmetricKey(providers().aes,&key,nullptr,0,const_cast<PUCHAR>(k.data()),ULONG(k.size()),0))&&success(BCryptGenerateSymmetricKey(providers().aes,&openKey,nullptr,0,const_cast<PUCHAR>(k.data()),ULONG(k.size()),0));}
    static std::array<uint8_t,12> nonce(uint8_t dir,uint64_t n){std::array<uint8_t,12> v{};v[0]=dir;for(int i=0;i<8;++i)v[size_t(4+i)]=uint8_t(n>>(8*i));return v;}
    bool seal(const Bytes& plain,Bytes& out){if(!key||plain.empty())return false;auto iv=nonce(sendDir,sent++);out.assign(plain.size()+16,0);BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce=iv.data();info.cbNonce=12;info.pbTag=out.data()+plain.size();info.cbTag=16;ULONG got=0;
        return success(BCryptEncrypt(key,const_cast<PUCHAR>(plain.data()),ULONG(plain.size()),&info,nullptr,0,out.data(),ULONG(plain.size()),&got,0))&&got==plain.size();}
    bool open(const Bytes& in,Bytes& plain){if(!openKey||in.size()<=16)return false;auto iv=nonce(recvDir,received++);const size_t n=in.size()-16;plain.assign(n,0);BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce=iv.data();info.cbNonce=12;info.pbTag=const_cast<PUCHAR>(in.data()+n);info.cbTag=16;ULONG got=0;
        return success(BCryptDecrypt(openKey,const_cast<PUCHAR>(in.data()),ULONG(n),&info,nullptr,0,plain.data(),ULONG(n),&got,0))&&got==n;}};
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
constexpr uint8_t frameData=1,frameEnd=2,frameHeader=3,frameBatchEnd=4,frameOffer=1,frameMusic=5,frameShelf=6,frameTake=7;
// A Shelf as listed to another PC: at most 32 items, each preview at most 6 KB.
constexpr size_t shelfListMax=32,shelfPreviewLimit=6*1024;
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
    struct Peer{std::wstring name;std::string address;uint16_t port=0;int version=1,revision=0;Clock::time_point seen{};bool online=false;Bytes key;/* the paired public key; empty when not paired */
        bool phone=false;int battery=-1;bool charging=false;};
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
    // 0.20: paired devices on other networks, through the relay (while options.relay).
    std::unique_ptr<Relay> relay;
    RelayPresence presence(const std::string& peer)const{return relay?relay->presence(peer):RelayPresence{};}
    RelayPath path(const std::string& peer)const{return relay?relay->path(peer):RelayPath{};}
    static std::wstring wideCode(const std::string& code){return std::wstring(code.begin(),code.end());}
    // The relay listens for every paired device: each pair's secret is the static ECDH of the two keys.
    void syncRelay(){
        if(!relay)return;std::vector<std::pair<Bytes,Bytes>> keys;{std::lock_guard lock(m);for(auto& [peer,p]:peers)if(!p.key.empty())keys.push_back({unhex(peer),p.key});}
        std::vector<RelayPair> list;for(auto& [peerId,pubKey]:keys){Bytes z;if(agree(key,pubKey,z))list.push_back({peerId,z});}relay->pairs(std::move(list));
    }
    // Phase 5H: this PC's Shelf as offered to paired PCs (guarded by m).
    std::vector<ShareShelfEntry> shelf;bool shelfOpen=false;
    ~Core(){if(key)BCryptDestroyKey(key);if(wsa)WSACleanup();}
    fs::path identityFile()const{return fs::path(o.folder)/L"share-identity.nexus";}
    fs::path peersFile()const{return fs::path(o.folder)/L"share-peers.nexus";}
    // Revision 2: which paired peers are phones, apart from the peers file (which older versions read as it was).
    fs::path phonesFile()const{return fs::path(o.folder)/L"share-phones.nexus";}
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
        auto& p=peers[peer];p.key=k;p.version=shareProtocol;p.name=cleanName(wide(std::string(nameBytes.begin(),nameBytes.end())));}
        std::ifstream phones(phonesFile());while(std::getline(phones,line)){auto it=peers.find(line);if(it!=peers.end()){it->second.phone=true;it->second.revision=shareRevision;}}}
    // Called with m held.
    void savePeers(){std::error_code e;fs::create_directories(o.folder,e);std::ofstream out(peersFile(),std::ios::trunc);for(auto& [peer,p]:peers)if(!p.key.empty()){const std::string n=utf8(p.name);out<<peer<<'\t'<<hex(p.key)<<'\t'<<hex(reinterpret_cast<const uint8_t*>(n.data()),n.size())<<'\n';}
        std::ofstream phones(phonesFile(),std::ios::trunc);for(auto& [peer,p]:peers)if(!p.key.empty()&&p.phone)phones<<peer<<'\n';}
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
        if(o.relay){RelayOptions ro;ro.id=id;ro.name=o.name;ro.revision=shareRevision;ro.brokers=o.relayBrokers;ro.loseEvery=o.relayLoseEvery;ro.direct=o.direct;ro.directLoopback=o.directLoopback;ro.directExtra=o.directExtra;std::weak_ptr<Core> weak=self;
            ro.incoming=[weak](SOCKET s,const std::string&){if(auto core=weak.lock()){core->track(s);core->incoming(s);core->untrack(s);}closesocket(s);};
            ro.changed=[weak]{if(auto core=weak.lock())core->postPeers();};
            relay=std::make_unique<Relay>(ro);syncRelay();}
        return true;
    }
    void stop(){
        relay.reset();stopping=true;if(listener!=INVALID_SOCKET){closesocket(listener);listener=INVALID_SOCKET;}if(udp!=INVALID_SOCKET){closesocket(udp);udp=INVALID_SOCKET;}
        if(listening.joinable())listening.join();if(discovery.joinable())discovery.join();
        std::lock_guard lock(m);for(SOCKET s:open)shutdown(s,SD_BOTH);if(pairing)pairing->set(0);for(auto& [t,d]:offers)d->set(0);
    }
    // Discovery: an announcement every 3 s; a PC not heard from for 12 s is offline. The port line carries
    // ";2.1", the protocol and its revision (an older version reads the number before ";" and ignores the rest,
    // and one a little newer reads the protocol and stops at the ".").
    void discover(){
        const std::string announce="ARNAVSHARE1\n"+hex(id)+"\n"+std::to_string(o.tcpPort)+";"+std::to_string(shareProtocol)+"."+std::to_string(shareRevision)+"\n"+utf8(o.name);auto last=Clock::now()-std::chrono::seconds(10);
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
            const auto dot=semi==std::string::npos?std::string::npos:parts[2].find('.',semi);const int revision=dot==std::string::npos?0:std::clamp(std::atoi(parts[2].c_str()+dot+1),0,99);
            const bool phone=parts[2].find(";phone")!=std::string::npos;
            char address[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&from.sin_addr,address,sizeof(address));const std::wstring name=cleanName(wide(text.substr(start)));
            bool changed=false;{std::lock_guard lock(m);auto& p=peers[parts[1]];changed=!p.online||p.address!=address||p.port!=port||p.version!=version||p.revision!=revision||(p.key.empty()&&p.name!=name);
                changed=changed||p.phone!=phone;if(phone&&!p.phone&&!p.key.empty()){p.phone=true;savePeers();}p.address=address;p.port=uint16_t(port);p.version=version;p.revision=revision;p.phone=phone;p.online=true;p.seen=Clock::now();if(p.key.empty()||p.name.empty())p.name=name;}
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
            else if(mode==modeSend||mode==modeMusic||mode==modeList||mode==modeTake||mode==modeRemote||mode==modeNotice||mode==modeInput||mode==modeMirror){auto it=peers.find(hex(ss.peerId));allowed=it!=peers.end()&&!it->second.key.empty()&&it->second.key==ss.peerPub;}}
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
        if(both){syncRelay();if(relay)relay->stopHosting();}
        releasePairing(d);
        fail(both?ShareEvent::Kind::Paired:ShareEvent::Kind::PairFailed,peer,ss.peerName,{},both?L"You can send files between these PCs now":!talked?L"The other PC stopped answering":mine!=1?L"Not paired":L"Not confirmed on the other PC");postPeers();
    }
    void incoming(SOCKET s){
        timeout(s,15000);Session ss;uint8_t mode=0;std::shared_ptr<Decision> claim;
        if(!welcome(s,ss,mode,claim)){if(claim)releasePairing(claim);return;}
        if(mode==modePair)pairSession(s,ss,claim);else if(mode==modeMusic)receiveMusic(s,ss);else if(mode==modeList)serveList(s,ss);else if(mode==modeTake)serveTake(s,ss);
        else if(mode==modeRemote)serveRemote(s,ss);else if(mode==modeNotice)serveNotice(s,ss);else if(mode==modeInput)serveInput(s,ss);else if(mode==modeMirror)serveMirror(s,ss);else receive(s,ss);
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
        // The flags byte: 1 a folder, 2 (revision 3) photos for the Shelf.
        const uint32_t count=get32(offer.data()+1);const uint64_t total=get64(offer.data()+5);const bool folder=(offer[13]&1)!=0,toShelf=(offer[13]&2)!=0;
        const std::wstring title=cleanName(wide(std::string(offer.begin()+14,offer.end())));if(!count||count>maxFiles||total>maxTotal)return;
        auto d=std::make_shared<Decision>();const uint32_t transfer=newTransfer();{std::lock_guard lock(m);offers[transfer]=d;}attach(transfer,s);
        {ShareEvent e;e.kind=ShareEvent::Kind::Offer;e.peer=peer;e.name=from;e.file=title;e.size=total;e.count=count;e.folder=folder;e.toShelf=toShelf;e.transfer=transfer;post(e);}
        timeout(s,90000);bool gone=false;int yes=decide(s,d,60,gone);{std::lock_guard lock(m);offers.erase(transfer);}
        // The other PC stopped before this one answered: the offer goes away.
        if(gone){finish(transfer);fail(ShareEvent::Kind::Failed,peer,from,title,from+L" stopped sending it",transfer);return;}
        const fs::path dir=o.downloads;const bool room=yes!=1||enoughSpace(dir,total);if(yes==1&&!room)yes=2;
        const bool told=sealed(s,ss.channel,Bytes{uint8_t(yes)});
        if(!told||yes!=1){const bool mine=stopped(transfer);finish(transfer);
            if(yes==2)fail(ShareEvent::Kind::Failed,peer,from,title,L"There isn't room in Downloads for "+sizeText(total),transfer);
            else if(yes==1)fail(ShareEvent::Kind::Failed,peer,from,title,mine?L"You stopped it":from+L" stopped sending it",transfer);return;}
        takeBatch(s,ss,peer,from,title,count,total,folder,transfer,0,toShelf);
    }
    // The files of an accepted offer, into Downloads, then the answer that they all arrived. code: the Received event's
    // (1: taken from the other PC's Shelf).
    void takeBatch(SOCKET s,Session& ss,const std::string& peer,const std::wstring& from,const std::wstring& title,uint32_t count,uint64_t total,bool folder,uint32_t transfer,uint32_t code,bool toShelf=false){
        const fs::path dir=o.downloads;
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
        ShareEvent e;e.kind=ShareEvent::Kind::Received;e.peer=peer;e.name=from;e.count=count;e.size=total;e.transfer=transfer;e.folder=folder;e.toShelf=toShelf;e.code=code;
        e.file=shown.size()==1?shown[0].filename().wstring():title;e.detail=shown.empty()?dir.wstring():shown[0].wstring();for(auto& p:shown)e.paths.push_back(p.wstring());post(e);
    }
    // A sender connection: reached, greeted, and checked against the pairing. False (with why) otherwise.
    // On this network directly; otherwise (or when that fails) through the relay.
    bool reach(const Peer& target,const std::string& peer,uint8_t mode,uint32_t transfer,SOCKET& s,Session& ss,std::wstring& why){
        s=target.online?connectTo(target.address,target.port):INVALID_SOCKET;
        if(s==INVALID_SOCKET&&relay){std::wstring through;s=relay->open(peer,through);if(s==INVALID_SOCKET){why=through.empty()?L"Couldn't reach "+target.name:through;return false;}}
        if(s==INVALID_SOCKET){why=L"Couldn't reach "+target.name;return false;}
        track(s);if(transfer&&!attach(transfer,s)){why=L"You stopped it";return false;}timeout(s,15000);
        if(!greet(s,mode,ss)){why=ss.outdated?L"Update Arnav Island on "+target.name+L" to share with it":ss.rejected?target.name+L" doesn't have this PC paired. Pair again from Nearby.":L"Couldn't reach "+target.name;return false;}
        if(hex(ss.peerId)!=peer||ss.peerPub!=target.key){why=target.name+L" answered with a different key. Pair again from Nearby.";return false;}
        return true;
    }
    bool ready(const std::string& peer,Peer& target,std::wstring& why){
        {std::lock_guard lock(m);auto it=peers.find(peer);if(it!=peers.end())target=it->second;}
        if(target.key.empty()){why=L"Pair with this PC first";return false;}
        // Not on this network: the relay may know it (and what it runs).
        if(!target.online){const auto p=presence(peer);if(!p.here){why=target.name+(relay?L" isn't reachable right now":L" isn't on this network right now");return false;}
            target.revision=p.revision;target.phone=target.phone||p.phone;}
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
        if(reach(target,peer,modeSend,transfer,s,ss,why))sent=offerBatch(s,ss,items,total,folder,title,peer,target.name,transfer,why);
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        finish(transfer);
        if(sent){ShareEvent e;e.kind=ShareEvent::Kind::Sent;e.peer=peer;e.name=target.name;e.file=title;e.size=total;e.count=uint32_t(items.size());e.transfer=transfer;e.outgoing=true;e.folder=folder;e.detail=sizeText(total);post(e);}
        else fail(ShareEvent::Kind::Failed,peer,target.name,title,why,transfer,true);
    }
    // An offer of these files, then (answered yes) the files and the batch's end, and the other PC's word that all arrived.
    bool offerBatch(SOCKET s,Session& ss,const std::vector<Item>& items,uint64_t total,bool folder,const std::wstring& title,const std::string& peer,const std::wstring& name,uint32_t transfer,std::wstring& why){
        Bytes offer{frameOffer};put32(offer,uint32_t(items.size()));put64(offer,total);offer.push_back(folder?1:0);append(offer,utf8(title));Bytes reply;timeout(s,90000);
        if(!sealed(s,ss.channel,offer)||!opened(s,ss.channel,reply)||reply.size()!=1)why=stopped(transfer)?L"You stopped it":name+L" didn't answer";
        else if(reply[0]==2)why=L"There isn't room on "+name+L" for "+sizeText(total);
        else if(reply[0]!=1)why=name+L" declined it";
        else{timeout(s,30000);Progress progress{*this,{}};progress.base.peer=peer;progress.base.name=name;progress.base.file=title;progress.base.transfer=transfer;progress.base.count=uint32_t(items.size());progress.base.outgoing=true;progress.total=total;
            bool flowing=true;for(auto& item:items)if(!streamFile(s,ss.channel,item,progress,transfer)){flowing=false;break;}
            Bytes ack;timeout(s,60000);const bool sent=flowing&&sealed(s,ss.channel,Bytes{frameBatchEnd})&&opened(s,ss.channel,ack)&&ack.size()==1&&ack[0]==1;
            if(!sent)why=stopped(transfer)?L"You stopped it":L"The transfer to "+name+L" didn't finish";return sent;}
        return false;
    }
    // ---- Phase 5H: the Shelf, seen and taken from another PC ----
    // An item's name as listed: its file name, at most 80 characters (so at most 240 bytes as UTF-8).
    static std::wstring listedName(const fs::path& p){std::wstring n=p.filename().wstring();if(n.size()>80)n.resize(IS_HIGH_SURROGATE(n[79])?79:80);return n;}
    // The list: whether it is shared, then per item its size, whether it is a folder, its name and its preview.
    void serveList(SOCKET s,Session& ss){
        std::vector<ShareShelfEntry> items;bool open=false;{std::lock_guard lock(m);items=shelf;open=shelfOpen;}
        Bytes list{frameShelf,uint8_t(open?1:0)};const size_t n=open?std::min(items.size(),shelfListMax):0;put32(list,uint32_t(n));
        for(size_t i=0;i<n;++i){const fs::path p(items[i].path);std::error_code e;const bool folder=fs::is_directory(p,e);uint64_t size=0;
            if(folder){std::vector<Item> inside;std::wstring why;if(collect({items[i].path},inside,why))for(auto& x:inside)size+=x.size;}else{size=fs::file_size(p,e);if(e)size=0;}
            const std::string name=utf8(listedName(p));const Bytes& preview=items[i].preview.size()<=shelfPreviewLimit?items[i].preview:Bytes{};
            put64(list,size);list.push_back(folder?1:0);list.push_back(uint8_t(name.size()));append(list,name);put32(list,uint32_t(preview.size()));append(list,preview);}
        sealed(s,ss.channel,list);
    }
    void serveTake(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);const std::wstring from=nameOf(peer,ss.peerName);
        Bytes ask;if(!opened(s,ss.channel,ask)||ask.size()<5||ask[0]!=frameTake)return;
        const uint32_t index=get32(ask.data()+1);const std::wstring name=wide(std::string(ask.begin()+5,ask.end()));
        // Only what is on the Shelf now, at the place and with the name the other PC saw.
        std::wstring path;{std::lock_guard lock(m);if(shelfOpen&&index<shelf.size()&&index<shelfListMax&&listedName(shelf[index].path)==name)path=shelf[index].path;}
        std::vector<Item> items;std::wstring why;
        if(path.empty()||!collect({path},items,why)){sealed(s,ss.channel,Bytes{frameOffer,0,0,0,0});return;}
        uint64_t total=0;for(auto& i:items)total+=i.size;std::error_code e;const bool folder=fs::is_directory(path,e);
        if(total>maxTotal||std::any_of(items.begin(),items.end(),[](auto& i){return i.size>maxFile;})){sealed(s,ss.channel,Bytes{frameOffer,0,0,0,0});return;}
        const uint32_t transfer=newTransfer();attach(transfer,s);
        const bool sent=offerBatch(s,ss,items,total,folder,name,peer,from,transfer,why);finish(transfer);
        if(sent){ShareEvent t;t.kind=ShareEvent::Kind::ShelfTaken;t.peer=peer;t.name=from;t.file=name;t.size=total;t.count=uint32_t(items.size());t.transfer=transfer;t.folder=folder;post(t);}
        else fail(ShareEvent::Kind::Failed,peer,from,name,why,transfer,true);
    }
    void askList(const std::string& peer){
        Peer target;std::wstring why;SOCKET s=INVALID_SOCKET;Session ss;ShareEvent e;e.kind=ShareEvent::Kind::ShelfList;e.peer=peer;e.code=2;
        const uint32_t transfer=newTransfer();
        if(ready(peer,target,why)&&(target.revision>=1||(why=L"Update Arnav Island on "+target.name+L" to see its Shelf",false))&&reach(target,peer,modeList,transfer,s,ss,why)){
            Bytes list;if(!opened(s,ss.channel,list)||list.size()<6||list[0]!=frameShelf)why=target.name+L" didn't answer";
            else{e.code=list[1]?0:1;const uint32_t n=std::min<uint32_t>(get32(list.data()+2),uint32_t(shelfListMax));size_t at=6;
                for(uint32_t i=0;i<n&&at+8+1+1<=list.size();++i){ShareShelfItem item;item.size=get64(list.data()+at);item.folder=list[at+8]!=0;const size_t len=list[at+9];at+=10;if(at+len+4>list.size())break;
                    item.name=wide(std::string(list.begin()+long(at),list.begin()+long(at+len)));at+=len;const uint32_t pl=get32(list.data()+at);at+=4;if(pl>shelfPreviewLimit||at+pl>list.size())break;
                    item.preview.assign(list.begin()+long(at),list.begin()+long(at+pl));at+=pl;e.shelf.push_back(std::move(item));}}}
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        finish(transfer);e.name=target.name;e.detail=why;post(e);
    }
    void takeItem(const std::string& peer,uint32_t index,const std::wstring& name,uint32_t transfer){
        Peer target;std::wstring why;SOCKET s=INVALID_SOCKET;Session ss;bool taken=false;
        if(ready(peer,target,why)&&(target.revision>=1||(why=L"Update Arnav Island on "+target.name+L" to take from its Shelf",false))&&reach(target,peer,modeTake,transfer,s,ss,why)){
            Bytes ask{frameTake};put32(ask,index);append(ask,utf8(name));Bytes offer;timeout(s,90000);
            if(!sealed(s,ss.channel,ask)||!opened(s,ss.channel,offer)||offer.size()<5||offer[0]!=frameOffer)why=stopped(transfer)?L"You stopped it":target.name+L" didn't answer";
            else if(offer.size()<14||!get32(offer.data()+1))why=L"It's no longer on "+target.name+L"\u2019s Shelf";
            else{const uint32_t count=get32(offer.data()+1);const uint64_t total=get64(offer.data()+5);const bool folder=offer[13]!=0;const std::wstring title=cleanName(wide(std::string(offer.begin()+14,offer.end())));
                if(count>maxFiles||total>maxTotal)why=L"That is too much to take at once";
                else{const bool room=enoughSpace(o.downloads,total);
                    if(!sealed(s,ss.channel,Bytes{uint8_t(room?1:2)}))why=target.name+L" stopped answering";
                    else if(!room)why=L"There isn't room in Downloads for "+sizeText(total);
                    // takeBatch reports how it went (Received, or Failed).
                    else{takeBatch(s,ss,peer,target.name,title,count,total,folder,transfer,1);taken=true;}}}}
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        if(!taken){finish(transfer);fail(ShareEvent::Kind::Failed,peer,target.name,name,why,transfer);}
    }
    // Music: what plays, as one sealed frame. The answer 2 asks for the song's own file too.
    void sendMusic(const std::string& peer,const ShareHandoff& music,const std::wstring& file,uint32_t transfer){
        Peer target;std::wstring why;SOCKET s=INVALID_SOCKET;Session ss;int code=-1;
        if(ready(peer,target,why)&&reach(target,peer,modeMusic,transfer,s,ss,why)){
            std::error_code e;uint64_t size=0;if(!file.empty()&&fs::is_regular_file(file,e))size=fs::file_size(file,e);if(e||size>(2ull<<30))size=0;
            Bytes offer{frameMusic};putF64(offer,music.position);putF64(offer,music.duration);offer.push_back(music.playing?1:0);put64(offer,size);
            auto line=[](const std::wstring& v){std::wstring t=v.substr(0,512);for(auto& c:t)if(c==L'\n'||c==L'\r'||c==L'\0')c=L' ';return t;};
            append(offer,utf8(line(music.title)+L"\n"+line(music.artist)+L"\n"+line(music.album)+L"\n"+line(music.app)+L"\n"+line(size?fs::path(file).filename().wstring():std::wstring())));
            if(target.revision>=1&&!music.cover.empty()&&music.cover.size()<=shareCoverLimit){offer.push_back(0);append(offer,music.cover);}
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
        const auto nul=std::find(offer.begin()+26,offer.end(),uint8_t(0));if(nul!=offer.end()&&size_t(offer.end()-nul-1)<=shareCoverLimit)music.cover.assign(nul+1,offer.end());
        {const std::wstring text=wide(std::string(offer.begin()+26,nul));std::vector<std::wstring> lines;size_t at=0;for(int k=0;k<5;++k){size_t end=text.find(L'\n',at);if(end==std::wstring::npos)end=text.size();lines.push_back(at<=text.size()?text.substr(at,std::min<size_t>(end-at,512)):L"");at=end+1;}
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
    // ---- revision 2: phones ----
    // One remote command, answered by the island (options.remote) on this thread.
    // Revision 4: the phone keeps the connection and asks again (one round trip a command, no new handshake), until it
    // closes it or says nothing for a minute. Revision 3 phones ask once and close.
    void serveRemote(SOCKET s,Session& ss){
        timeout(s,60000);
        for(;;){Bytes request;if(!opened(s,ss.channel,request)||request.size()<2||request[0]!=frameRequest)return;
            const RemoteCommand command=RemoteCommand(request[1]);const Bytes payload(request.begin()+2,request.end());
            Bytes answer=o.remote?o.remote(hex(ss.peerId),command,payload):Bytes{remoteUnsupported};if(answer.empty())answer={remoteFailed};
            Bytes reply{frameReply};append(reply,answer);if(reply.size()>maxFrame)reply={frameReply,remoteFailed};if(!sealed(s,ss.channel,reply))return;}
    }
    // A phone's status (battery) or its notifications, for the island (revision 4: as many as it sends, a minute apart at most).
    void serveNotice(SOCKET s,Session& ss){timeout(s,60000);while(serveOneNotice(s,ss));}
    bool serveOneNotice(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);Bytes f;if(!opened(s,ss.channel,f)||f.size()<2||f[0]!=frameNotice)return false;
        ShareEvent e;e.peer=peer;e.name=nameOf(peer,ss.peerName);
        if(f[1]==1&&f.size()>=4){e.kind=ShareEvent::Kind::PhoneStatus;e.battery=int(int8_t(f[2]));e.charging=f[3]!=0;if(e.battery<-1||e.battery>100)e.battery=-1;
            {std::lock_guard lock(m);auto it=peers.find(peer);if(it!=peers.end()){it->second.battery=e.battery;it->second.charging=e.charging;}}}
        else if(f[1]==2&&f.size()>=3+4){e.kind=ShareEvent::Kind::PhoneNotice;e.urgent=f[2]!=0;size_t at=3;const uint32_t n=get32(f.data()+at);at+=4;if(n>64*1024||at+n+4>f.size())return false;
            const std::wstring text=wide(std::string(f.begin()+long(at),f.begin()+long(at+n)));at+=n;const uint32_t icon=get32(f.data()+at);at+=4;if(icon>24*1024||at+icon>f.size())return false;
            e.icon.assign(f.begin()+long(at),f.begin()+long(at+icon));at+=icon;
            std::vector<std::wstring> lines;size_t from=0;for(int k=0;k<4;++k){size_t end=text.find(L'\n',from);if(end==std::wstring::npos)end=text.size();lines.push_back(from<=text.size()?text.substr(from,end-from):L"");from=end+1;}
            auto clean=[](std::wstring t,size_t limit){std::wstring out;for(wchar_t c:t)if(c>=32&&c!=127)out+=c;if(out.size()>limit)out.resize(limit);return out;};
            e.app=clean(lines[0],80);e.file=clean(lines[1],200);e.detail=clean(lines[2],600);if(e.app.empty()&&e.file.empty()&&e.detail.empty())return false;
            // Revision 3: its key, and up to three actions (a title each, and whether it takes a reply).
            if(at+4<=f.size()){const uint32_t keySize=get32(f.data()+at);at+=4;
                if(keySize<=512&&at+keySize<=f.size()){e.key.assign(f.begin()+long(at),f.begin()+long(at+keySize));at+=keySize;
                    if(at<f.size()){const size_t count=std::min<size_t>(f[at++],3);
                        for(size_t k=0;k<count&&at+2<=f.size();++k){const uint8_t flags=f[at],size=f[at+1];at+=2;if(at+size>f.size())break;
                            e.actions.push_back({clean(wide(std::string(f.begin()+long(at),f.begin()+long(at+size))),24),(flags&1)!=0});at+=size;}}}}}
        else if(f[1]==3&&f.size()>=6){e.kind=ShareEvent::Kind::PhoneDetails;const uint32_t n=get32(f.data()+2);if(n>16*1024||6+n>f.size())return false;e.detail=wide(std::string(f.begin()+6,f.begin()+long(6+n)));}
        else if(f[1]==4&&f.size()>=6){e.kind=ShareEvent::Kind::PhoneNoticeGone;const uint32_t n=get32(f.data()+2);if(n>512||6+n>f.size())return false;e.key.assign(f.begin()+6,f.begin()+long(6+n));}
        // Revision 5: its hotspot, [on] and when on its name (32 bytes at most) and password (64 at most).
        else if(f[1]==5&&f.size()>=3){e.kind=ShareEvent::Kind::PhoneHotspot;e.code=f[2]!=0;size_t at=3;
            auto text=[&](size_t limit,std::wstring& out){if(at+4>f.size())return false;const uint32_t n=get32(f.data()+at);at+=4;if(n>limit||at+n>f.size())return false;
                out=wide(std::string(f.begin()+long(at),f.begin()+long(at+n)));at+=n;return true;};
            if(e.code&&(!text(32,e.file)||!text(64,e.detail)||e.file.empty()))return false;}
        else return false;
        const bool acked=sealed(s,ss.channel,Bytes{frameNoticeAck});post(std::move(e));return acked;
    }
    // Revision 7: a screen, either way: the phone's request goes to the island with the connection, until it ends.
    void serveMirror(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);Bytes first;timeout(s,15000);if(!opened(s,ss.channel,first)||first.empty()||first[0]!=0xA0)return;
        if(!o.mirror){Bytes no{0xA1,remoteUnsupported};no.resize(14,0);sealed(s,ss.channel,no);return;}
        std::mutex sending;mirror::MirrorIo io;
        io.send=[&](const Bytes& b){std::lock_guard held(sending);timeout(s,15000);return sealed(s,ss.channel,b);};
        // Waits only for a frame to start (the socket's own timeouts stay as the sender set them); once one starts it's read
        // whole, since giving up halfway would lose the stream's place.
        io.receive=[&](Bytes& b,int ms){fd_set set;FD_ZERO(&set);FD_SET(s,&set);timeval t{ms/1000,(ms%1000)*1000};if(select(0,&set,nullptr,nullptr,&t)!=1)return false;wire.limit=15000;return opened(s,ss.channel,b);};
        // How it's reached: on this network, or through the relay (its direct path or a broker).
        bool here=false;{std::lock_guard lock(m);auto it=peers.find(peer);here=it!=peers.end()&&it->second.online;}
        if(!here){const RelayPath p=path(peer);io.path=p.kind==2?2:1;io.rtt=p.rtt;}
        o.mirror(peer,nameOf(peer,ss.peerName),first,io);
    }
    // A phone's trackpad and keyboard: its frames until it stops (or is idle for two minutes).
    void serveInput(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);timeout(s,120000);
        for(;;){Bytes f;if(!opened(s,ss.channel,f)||f.empty()||f[0]<0x60||f[0]>0x6F)break;if(o.input)o.input(peer,f);}
    }
    // One command to a paired phone (revision 3): a sealed frame, answered [ack, status].
    bool askPhone(const std::string& peer,uint8_t mode,const Bytes& frame,uint8_t ack,uint32_t transfer,std::wstring& why){
        Peer target;SOCKET s=INVALID_SOCKET;Session ss;bool done=false;
        if(ready(peer,target,why)&&(target.revision>=3||(why=L"Update Arnav Island on "+target.name,false))&&reach(target,peer,mode,transfer,s,ss,why)){
            Bytes a;done=sealed(s,ss.channel,frame)&&opened(s,ss.channel,a)&&a.size()>=2&&a[0]==ack&&a[1]==0;
            if(!done)why=a.size()>=2&&a[0]==ack?(a[1]==1?L"That notification is gone":target.name+L" couldn't do it"):target.name+L" didn't answer";}
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        finish(transfer);return done;
    }
    // Revision 6: one request to a phone on its kept query connection (opened as needed; a dead one is replaced once).
    struct Kept{std::mutex m;SOCKET s=INVALID_SOCKET;std::unique_ptr<Session> ss;Clock::time_point used{};};
    std::map<std::string,std::shared_ptr<Kept>> queries;
    bool askQuery(const std::string& peer,uint8_t command,const Bytes& payload,Bytes& answer){
        std::shared_ptr<Kept> k;{std::lock_guard lock(m);auto& slot=queries[peer];if(!slot)slot=std::make_shared<Kept>();k=slot;}
        std::lock_guard held(k->m);
        auto drop=[&]{if(k->s!=INVALID_SOCKET){untrack(k->s);closesocket(k->s);k->s=INVALID_SOCKET;}};
        // The phone closes a connection idle for a minute: one idle for 40 s isn't trusted. A kept one that died (the
        // phone restarted, or changed networks) is found within 6 s, and a fresh one tried.
        if(k->s!=INVALID_SOCKET&&Clock::now()-k->used>std::chrono::seconds(40))drop();
        for(int attempt=0;attempt<2;++attempt){
            const bool reused=k->s!=INVALID_SOCKET;
            if(!reused){Peer target;std::wstring why;if(!ready(peer,target,why)||target.revision<6)return false;
                SOCKET s=INVALID_SOCKET;auto ss=std::make_unique<Session>();if(!reach(target,peer,modeQuery,0,s,*ss,why)){if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}return false;}
                k->s=s;k->ss=std::move(ss);}
            timeout(k->s,reused?6000:15000);
            Bytes request{frameQuery,command};append(request,payload);Bytes a;
            if(sealed(k->s,k->ss->channel,request)&&opened(k->s,k->ss->channel,a)&&a.size()>=2&&a[0]==frameQueryReply){k->used=Clock::now();if(a[1]!=0)return false;answer.assign(a.begin()+2,a.end());return true;}
            drop();}
        return false;
    }
    void ringPhone(const std::string& peer,uint32_t transfer){
        Peer target;std::wstring why;SOCKET s=INVALID_SOCKET;Session ss;bool rang=false;
        if(ready(peer,target,why)&&(target.revision>=2||(why=L"Update Arnav Island on "+target.name+L" to ring it",false))&&reach(target,peer,modeFind,transfer,s,ss,why)){
            Bytes ack;rang=sealed(s,ss.channel,Bytes{frameRing})&&opened(s,ss.channel,ack)&&ack.size()==1&&ack[0]==frameRingAck;if(!rang)why=target.name+L" didn't answer";}
        if(s!=INVALID_SOCKET){untrack(s);closesocket(s);}
        finish(transfer);
        if(rang){ShareEvent e;e.kind=ShareEvent::Kind::Rang;e.peer=peer;e.name=target.name;post(e);}else fail(ShareEvent::Kind::Failed,peer,target.name,{},why,transfer,true);
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
    std::vector<SharePeer> list;{std::lock_guard lock(core_->m);for(auto& [peer,p]:core_->peers)if(p.online||!p.key.empty()){
        // Here directly, or through the relay (which says what the device runs).
        const RelayPresence r=p.key.empty()?RelayPresence{}:core_->presence(peer);const bool internet=!p.online&&r.here;
        SharePeer v{peer,p.name,!p.key.empty(),p.online||r.here,p.version,internet?r.revision:p.revision,p.phone||r.phone,p.battery,p.charging,internet};
        if(internet){const RelayPath path=core_->path(peer);v.path=path.kind;v.rtt=path.rtt;v.relays=path.brokers;v.v6=path.v6;}
        list.push_back(std::move(v));}}
    std::stable_sort(list.begin(),list.end(),[](auto& a,auto& b){const int ra=(a.online?0:2)+(a.paired?0:1),rb=(b.online?0:2)+(b.paired?0:1);return ra!=rb?ra<rb:a.name<b.name;});return list;
}
void ShareService::addPeer(const std::string& peer,const std::wstring& name,const std::string& address,uint16_t port,int version,int revision){
    {std::lock_guard lock(core_->m);auto& p=core_->peers[peer];if(p.key.empty()||p.name.empty())p.name=cleanName(name);p.address=address;p.port=port;p.version=version;p.revision=revision;p.online=true;p.seen=Clock::now()+std::chrono::hours(24);}core_->postPeers();}
void ShareService::pair(const std::string& peer){
    std::shared_ptr<Core::Decision> d;{std::lock_guard lock(core_->m);if(core_->pairing||!core_->ok)return;d=core_->pairing=std::make_shared<Core::Decision>();}
    std::thread([core=core_,peer,d]{core->pairWith(peer,d);}).detach();
}
void ShareService::confirmPair(bool yes){std::lock_guard lock(core_->m);if(core_->pairing)core_->pairing->set(yes?1:0);}
void ShareService::forget(const std::string& peer){{std::lock_guard lock(core_->m);auto it=core_->peers.find(peer);if(it!=core_->peers.end()){it->second.key.clear();core_->savePeers();}}core_->syncRelay();core_->postPeers();}
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
void ShareService::offerShelf(std::vector<ShareShelfEntry> items,bool open){std::lock_guard lock(core_->m);core_->shelf=std::move(items);core_->shelfOpen=open;}
void ShareService::askShelf(const std::string& peer){if(!core_->ok)return;std::thread([core=core_,peer]{core->askList(peer);}).detach();}
uint32_t ShareService::takeFromShelf(const std::string& peer,uint32_t index,const std::wstring& name){
    if(!core_->ok)return 0;const uint32_t transfer=core_->newTransfer();
    std::thread([core=core_,peer,index,name,transfer]{core->takeItem(peer,index,name,transfer);}).detach();return transfer;}
void ShareService::noticeAction(const std::string& peer,const std::string& key,int action,const std::wstring& reply){
    if(!core_->ok)return;const uint32_t transfer=core_->newTransfer();
    std::thread([core=core_,peer,key,action,reply,transfer]{Bytes f{frameAction};put32(f,uint32_t(key.size()));append(f,key);f.push_back(uint8_t(std::clamp(action,0,255)));const std::string r=utf8(reply);put32(f,uint32_t(r.size()));append(f,r);
        std::wstring why;if(!core->askPhone(peer,modeAction,f,frameActionAck,transfer,why))core->fail(ShareEvent::Kind::Failed,peer,core->nameOf(peer,L""),{},why,transfer,true);}).detach();}
void ShareService::pushClipboard(const std::string& peer,const std::wstring& text,bool sensitive){
    if(!core_->ok||text.empty())return;const uint32_t transfer=core_->newTransfer();
    std::thread([core=core_,peer,text,sensitive,transfer]{std::string t=utf8(text);if(t.size()>200*1024)t.resize(200*1024);Bytes f{frameClip,uint8_t(sensitive?1:0)};put32(f,uint32_t(t.size()));append(f,t);
        std::wstring why;core->askPhone(peer,modeClip,f,frameClipAck,transfer,why);}).detach();}
void ShareService::askPhoto(const std::string& peer){
    if(!core_->ok)return;const uint32_t transfer=core_->newTransfer();
    std::thread([core=core_,peer,transfer]{std::wstring why;if(!core->askPhone(peer,modeCamera,Bytes{frameCamera},frameCameraAck,transfer,why))core->fail(ShareEvent::Kind::Failed,peer,core->nameOf(peer,L""),{},why,transfer,true);}).detach();}
void ShareService::queryPhone(const std::string& peer){
    if(!core_->ok)return;
    std::thread([core=core_,peer]{Bytes a;if(!core->askQuery(peer,1,{},a))return;
        // [u32 n, the lines], [u32 n, the cover]: both bounded.
        if(a.size()<4)return;const uint32_t n=get32(a.data());if(n>32*1024||4+size_t(n)+4>a.size())return;
        ShareEvent e;e.kind=ShareEvent::Kind::PhoneLive;e.peer=peer;e.name=core->nameOf(peer,L"");e.detail=wide(std::string(a.begin()+4,a.begin()+long(4+n)));
        const size_t at=4+n;const uint32_t c=get32(a.data()+at);if(c<=48*1024&&at+4+c<=a.size())e.icon.assign(a.begin()+long(at+4),a.begin()+long(at+4+c));
        core->post(std::move(e));}).detach();}
void ShareService::tellPhoneFocus(const std::string& peer,const std::vector<uint8_t>& state){
    // Told again, a few seconds apart, if the phone didn't hear it (it may be changing networks).
    if(!core_->ok)return;std::thread([core=core_,peer,state]{for(int k=0;k<3;++k){Bytes a;if(core->askQuery(peer,2,state,a))return;std::this_thread::sleep_for(std::chrono::seconds(4));}}).detach();}
void ShareService::hostPairing(){
    if(!core_->ok)return;std::thread([core=core_]{ShareEvent e;e.kind=ShareEvent::Kind::PairingCode;e.detail=core->relay?core->wideCode(core->relay->host()):std::wstring();core->post(e);}).detach();}
void ShareService::stopPairing(){if(core_->relay)core_->relay->stopHosting();}
void ShareService::stopDirect(){if(core_->relay)core_->relay->stopDirect();}
std::string ShareService::pairingCode()const{return core_->relay?core_->relay->hosting():std::string();}
std::string ShareService::pairingLink(const std::string& code)const{
    std::string plain;for(char c:code){if(c=='-'||c==' ')continue;plain+=c>='a'&&c<='z'?char(c-'a'+'A'):c;}
    if(plain.empty()||!core_->ok)return {};Bytes print=Sha().add(core_->pub).done();if(print.size()<10)return {};print.resize(10);
    return "arnavisland://pair/"+plain+"?k="+hex(print);}
void ShareService::pairWithCode(const std::string& code){
    std::shared_ptr<Core::Decision> d;{std::lock_guard lock(core_->m);if(core_->pairing||!core_->ok)return;d=core_->pairing=std::make_shared<Core::Decision>();}
    std::thread([core=core_,code,d]{
        std::wstring why;const SOCKET s=core->relay?core->relay->openCode(code,why):INVALID_SOCKET;
        if(s==INVALID_SOCKET){core->releasePairing(d);core->fail(ShareEvent::Kind::PairFailed,{},{},{},why.empty()?L"Pairing from anywhere is off":why);return;}
        core->track(s);timeout(s,20000);Core::Session ss;
        if(!core->greet(s,modePair,ss)){core->releasePairing(d);core->fail(ShareEvent::Kind::PairFailed,{},{},{},L"No device is showing that code");}
        else core->pairSession(s,ss,d);
        core->untrack(s);closesocket(s);}).detach();
}
bool ShareService::internet()const{return core_->relay&&core_->relay->connected();}
std::wstring ShareService::relayBroker()const{return core_->relay?core_->relay->broker():std::wstring();}
void ShareService::ring(const std::string& peer){if(!core_->ok)return;const uint32_t transfer=core_->newTransfer();std::thread([core=core_,peer,transfer]{core->ringPhone(peer,transfer);}).detach();}
std::vector<uint8_t> remoteStatusAnswer(const RemoteStatus& st,const std::vector<uint8_t>& haveCover){
    Bytes b{remoteOk};uint16_t flags=0;auto bit=[&](int i,bool on){if(on)flags|=uint16_t(1u<<i);};
    bit(0,st.available);bit(1,st.playing);bit(2,st.canPrevious);bit(3,st.canNext);bit(4,st.canToggle);bit(5,st.muted);bit(6,st.charging);bit(7,st.canSeek);bit(8,st.batteryPresent);bit(9,st.clipboard);
    b.push_back(uint8_t(flags));b.push_back(uint8_t(flags>>8));putF64(b,st.position);putF64(b,st.duration);b.push_back(uint8_t(std::clamp(st.volume,0,100)));b.push_back(uint8_t(int8_t(std::clamp(st.battery,-1,100))));b.push_back(uint8_t(st.cpu<0?255:std::clamp(st.cpu,0,100)));
    if(st.cover.empty()||st.cover.size()>shareCoverLimit)b.push_back(0);
    else{const Bytes hash=Sha().add(st.cover).done();if(hash==haveCover)b.push_back(2);else{b.push_back(1);append(b,hash);put32(b,uint32_t(st.cover.size()));append(b,st.cover);}}
    auto line=[](const std::wstring& v){std::wstring t=v.substr(0,512);for(auto& c:t)if(c==L'\n'||c==L'\r'||c==L'\0')c=L' ';return t;};
    const std::string text=utf8(line(st.title)+L"\n"+line(st.artist)+L"\n"+line(st.app)+L"\n"+line(st.name)+L"\n"+line(st.weather));put32(b,uint32_t(text.size()));append(b,text);
    return b;
}
// ---- Revision 5 ----
namespace {
void putU16(Bytes& b,unsigned v){b.push_back(uint8_t(v&255));b.push_back(uint8_t((v>>8)&255));}
void putStr(Bytes& b,const std::wstring& s,size_t limit=600){const std::string u=utf8(s.substr(0,limit));put32(b,uint32_t(u.size()));append(b,u);}
void putStr(Bytes& b,const std::string& s){put32(b,uint32_t(s.size()));append(b,s);}
void putI32(Bytes& b,int v){put32(b,uint32_t(v));}
uint8_t percentByte(double v){return v<0||!std::isfinite(v)?255:uint8_t(std::clamp(int(std::lround(v)),0,100));}
}
std::wstring RemoteReader::text(size_t limit){const std::string u=bytes(limit);return wide(u);}
std::vector<uint8_t> remoteBatteryAnswer(const PcBattery& p){
    Bytes b{remoteOk,1};b.push_back(uint8_t(int8_t(std::clamp(p.percent,-1,100))));
    b.push_back(uint8_t((p.present?1:0)|(p.online?2:0)|(p.charging?4:0)|(p.saver?8:0)|(p.critical?16:0)));
    put32(b,uint32_t(p.minutesLeft));put32(b,uint32_t(p.minutesToFull));
    for(long long v:{p.designMwh,p.fullMwh,p.remainingMwh,p.rateMw,p.voltageMv})put64(b,uint64_t(v));
    put32(b,p.cycles);put32(b,uint32_t(p.temperatureDeciK));putF64(b,std::isfinite(p.health)?p.health:-1);putF64(b,std::isfinite(p.healthBefore)?p.healthBefore:-1);
    for(auto* t:{&p.chemistry,&p.manufacturer,&p.name})putStr(b,*t,80);
    const size_t n=std::min<size_t>(p.day.size(),400);putU16(b,unsigned(n));
    for(size_t i=p.day.size()-n;i<p.day.size();++i){put64(b,uint64_t(p.day[i].time));b.push_back(uint8_t(std::clamp(p.day[i].percent,0,100)));b.push_back(p.day[i].charging?1:0);}
    return b;}
std::vector<uint8_t> remoteStatsAnswer(const PcStats& s){
    Bytes b{remoteOk,1};for(double v:{s.cpu,s.gpu,s.ramUsedGiB,s.ramTotalGiB,s.ramPercent,s.diskUsedPercent,s.diskFreeGiB,s.diskTotalGiB,s.download,s.upload})putF64(b,std::isfinite(v)?v:-1);
    put64(b,s.uptime);putU16(b,std::min(s.logical,65535u));b.push_back(uint8_t(int8_t(std::clamp(s.battery,-1,100))));b.push_back(s.charging?1:0);putF64(b,s.batteryMinutes);
    const size_t n=std::min<size_t>({s.cpuHistory.size(),s.gpuHistory.size(),s.downloadHistory.size(),size_t(40)});b.push_back(uint8_t(n));
    for(size_t i=0;i<n;++i)b.push_back(percentByte(s.cpuHistory[s.cpuHistory.size()-n+i]));
    for(size_t i=0;i<n;++i)b.push_back(percentByte(s.gpuHistory[s.gpuHistory.size()-n+i]));
    for(size_t i=0;i<n;++i){const double d=s.downloadHistory[s.downloadHistory.size()-n+i];put32(b,uint32_t(std::clamp(d,0.,4e9)));}
    for(auto* t:{&s.name,&s.model,&s.os,&s.cpuName,&s.gpuName})putStr(b,*t,120);
    const size_t c=std::min<size_t>(s.cores.size(),64);b.push_back(uint8_t(c));for(size_t i=0;i<c;++i)b.push_back(percentByte(s.cores[i]));
    return b;}
std::vector<uint8_t> remoteSettingsAnswer(const std::vector<std::wstring>& sections,const std::vector<RemoteSetting>& items){
    Bytes b{remoteOk,1};b.push_back(uint8_t(std::min<size_t>(sections.size(),64)));for(size_t i=0;i<sections.size()&&i<64;++i)putStr(b,sections[i],80);
    const size_t n=std::min<size_t>(items.size(),1000);putU16(b,unsigned(n));
    for(size_t i=0;i<n;++i){const auto& it=items[i];b.push_back(uint8_t(it.section));b.push_back(uint8_t(it.control));putStr(b,it.key.substr(0,64));putStr(b,it.title,120);putStr(b,it.detail,300);
        putI32(b,it.lo);putI32(b,it.hi);putI32(b,it.step);putI32(b,it.value);b.push_back(uint8_t(it.action));putStr(b,it.unit,16);
        const size_t o=std::min<size_t>(it.options.size(),32);b.push_back(uint8_t(o));for(size_t k=0;k<o;++k)putStr(b,it.options[k],60);
        const size_t c=std::min<size_t>(it.colours.size(),32);b.push_back(uint8_t(c));for(size_t k=0;k<c;++k)put32(b,it.colours[k]);}
    return b;}
std::vector<uint8_t> remoteValueAnswer(int value){Bytes b{remoteOk};putI32(b,value);return b;}
std::vector<uint8_t> remoteControlsAnswer(const PcControls& c){
    Bytes b{remoteOk,1};for(int v:{c.wifi,c.bluetooth,c.dark,c.brightness})b.push_back(uint8_t(int8_t(std::clamp(v,-2,100))));b.push_back(uint8_t(std::clamp(c.volume,0,100)));
    b.push_back(uint8_t((c.muted?1:0)|(c.micAvailable?2:0)|(c.micMuted?4:0)|(c.focusRunning?8:0)|(c.focusFinished?16:0)));b.push_back(uint8_t(std::clamp(c.focusMode,0,2)));
    putF64(b,c.focusDuration);putF64(b,c.focusShown);b.push_back(uint8_t(c.busy&15));return b;}
std::vector<uint8_t> remoteCommandAnswer(bool final,const std::vector<RemoteResult>& results){
    Bytes b{remoteOk,1,uint8_t(final?1:0)};const size_t n=std::min<size_t>(results.size(),12);b.push_back(uint8_t(n));
    for(size_t i=0;i<n;++i){const auto& r=results[i];b.push_back(uint8_t(std::clamp(r.kind,0,255)));b.push_back(r.confirm?1:0);putStr(b,r.title,200);putStr(b,r.detail,300);putStr(b,r.answer,60);}
    return b;}
std::vector<uint8_t> remoteOutcome(int outcome,const std::wstring& message){Bytes b{remoteOk,uint8_t(std::clamp(outcome,0,3))};putStr(b,message,300);return b;}
std::vector<uint8_t> remoteAudioAnswer(const std::vector<RemoteOutput>& outputs){
    Bytes b{remoteOk,1};const size_t n=std::min<size_t>(outputs.size(),32);b.push_back(uint8_t(n));
    for(size_t i=0;i<n;++i){const auto& o=outputs[i];putStr(b,o.id,400);putStr(b,o.name,120);b.push_back(o.current?1:0);b.push_back(uint8_t(int8_t(std::clamp(o.form,-1,100))));}
    return b;}
std::vector<uint8_t> remoteLyricsAnswer(int state,const std::wstring& key,const std::vector<ShareLyricLine>& lines){
    Bytes b{remoteOk,uint8_t(std::clamp(state,0,3))};const std::string k=utf8(key.substr(0,300));put32(b,uint32_t(k.size()));append(b,k);
    const size_t count=std::min<size_t>(lines.size(),400);put32(b,uint32_t(count));
    for(size_t i=0;i<count;++i){const auto& l=lines[i];putF64(b,l.time);const std::string t=utf8(l.text.substr(0,300));put32(b,uint32_t(t.size()));append(b,t);
        const size_t words=std::min<size_t>(l.words.size(),64);b.push_back(uint8_t(words));
        for(size_t w=0;w<words;++w){putF64(b,l.words[w].first);const uint32_t at=std::min<uint32_t>(l.words[w].second,65535);b.push_back(uint8_t(at&255));b.push_back(uint8_t(at>>8));}}
    return b;
}
std::vector<ShareEvent> ShareService::take(){std::lock_guard lock(core_->m);std::vector<ShareEvent> out;out.swap(core_->events);return out;}
}
