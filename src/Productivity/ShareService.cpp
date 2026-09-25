#include <winsock2.h>
#include <ws2tcpip.h>
#include "ShareService.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
constexpr uint8_t protocolVersion=1;
constexpr size_t chunkSize=256*1024,maxFrame=chunkSize+64;
constexpr uint64_t maxFile=16ull<<30;
constexpr char magic[4]={'A','R','N','V'};
constexpr uint8_t modePair='P',modeSend='S';
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
// Framing: a 32-bit little-endian length, then the bytes.
bool sendAll(SOCKET s,const uint8_t* p,size_t n){while(n){const int k=::send(s,reinterpret_cast<const char*>(p),int(std::min<size_t>(n,1<<20)),0);if(k<=0)return false;p+=k;n-=size_t(k);}return true;}
bool recvAll(SOCKET s,uint8_t* p,size_t n){while(n){const int k=::recv(s,reinterpret_cast<char*>(p),int(std::min<size_t>(n,1<<20)),0);if(k<=0)return false;p+=k;n-=size_t(k);}return true;}
bool sendFrame(SOCKET s,const Bytes& b){if(b.size()>maxFrame)return false;uint8_t h[4];for(int i=0;i<4;++i)h[i]=uint8_t(b.size()>>(8*i));return sendAll(s,h,4)&&sendAll(s,b.data(),b.size());}
bool recvFrame(SOCKET s,Bytes& b){uint8_t h[4];if(!recvAll(s,h,4))return false;const uint32_t n=uint32_t(h[0])|uint32_t(h[1])<<8|uint32_t(h[2])<<16|uint32_t(h[3])<<24;if(n>maxFrame)return false;b.assign(n,0);return recvAll(s,b.data(),n);}
bool sealed(SOCKET s,Channel& c,const Bytes& plain){Bytes out;return c.seal(plain,out)&&sendFrame(s,out);}
bool opened(SOCKET s,Channel& c,Bytes& plain){Bytes in;return recvFrame(s,in)&&c.open(in,plain);}
void timeout(SOCKET s,int ms){const DWORD t=DWORD(ms);setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&t),sizeof(t));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&t),sizeof(t));}
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
fs::path uniquePath(const fs::path& dir,const std::wstring& name){fs::path p=dir/name;std::error_code e;if(!fs::exists(p,e))return p;const std::wstring stem=p.stem().wstring(),ext=p.extension().wstring();
    for(int i=2;i<1000;++i){fs::path q=dir/(stem+L" ("+std::to_wstring(i)+L")"+ext);if(!fs::exists(q,e))return q;}return dir/(stem+L" ("+std::to_wstring(GetTickCount64())+L")"+ext);}
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
struct ShareService::Core:std::enable_shared_from_this<Core>{
    struct Peer{std::wstring name;std::string address;uint16_t port=0;Clock::time_point seen{};bool online=false;Bytes key;/* the paired public key; empty when not paired */};
    // A person's answer (pairing or an offer), waited for by a session thread.
    struct Decision{std::mutex m;std::condition_variable cv;int value=-1;
        void set(int v){{std::lock_guard lock(m);if(value<0)value=v;}cv.notify_all();}
        int wait(int secondsToWait){std::unique_lock lock(m);cv.wait_for(lock,std::chrono::seconds(secondsToWait),[&]{return value>=0;});return value<0?0:value;}};
    struct Session{Bytes peerId,peerPub;std::wstring peerName;Channel channel;uint32_t code=0;bool rejected=false;};
    HWND notify=nullptr;ShareOptions o;bool wsa=false,ok=false;std::string failure;
    Bytes id,pub;BCRYPT_KEY_HANDLE key=nullptr;
    mutable std::mutex m;std::map<std::string,Peer> peers;std::vector<ShareEvent> events;std::shared_ptr<Decision> pairing;std::map<uint32_t,std::shared_ptr<Decision>> offers;uint32_t nextTransfer=1;std::set<SOCKET> open;
    std::atomic<bool> stopping{false};SOCKET udp=INVALID_SOCKET,listener=INVALID_SOCKET;std::thread discovery,listening;
    ~Core(){if(key)BCryptDestroyKey(key);if(wsa)WSACleanup();}
    fs::path identityFile()const{return fs::path(o.folder)/L"share-identity.nexus";}
    fs::path peersFile()const{return fs::path(o.folder)/L"share-peers.nexus";}
    void post(ShareEvent e){{std::lock_guard lock(m);events.push_back(std::move(e));}if(notify)PostMessageW(notify,ShareMessage,0,0);}
    void postPeers(){ShareEvent e;e.kind=ShareEvent::Kind::Peers;post(std::move(e));}
    void fail(ShareEvent::Kind kind,const std::string& peer,const std::wstring& name,const std::wstring& file,const std::wstring& detail){ShareEvent e;e.kind=kind;e.peer=peer;e.name=name;e.file=file;e.detail=detail;post(std::move(e));}
    void track(SOCKET s){std::lock_guard lock(m);open.insert(s);}
    void untrack(SOCKET s){std::lock_guard lock(m);open.erase(s);}
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
        auto& p=peers[peer];p.key=k;p.name=cleanName(wide(std::string(nameBytes.begin(),nameBytes.end())));}}
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
    // Discovery: an announcement every 3 s; a PC not heard from for 12 s is offline.
    void discover(){
        const std::string announce="ARNAVSHARE1\n"+hex(id)+"\n"+std::to_string(o.tcpPort)+"\n"+utf8(o.name);auto last=Clock::now()-std::chrono::seconds(10);
        while(!stopping){
            if(Clock::now()-last>=std::chrono::seconds(3)){last=Clock::now();sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=htonl(INADDR_BROADCAST);to.sin_port=htons(o.udpPort);
                sendto(udp,announce.data(),int(announce.size()),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
                bool changed=false;{std::lock_guard lock(m);for(auto& [peer,p]:peers)if(p.online&&Clock::now()-p.seen>std::chrono::seconds(12)){p.online=false;changed=true;}}if(changed)postPeers();}
            fd_set readable;FD_ZERO(&readable);FD_SET(udp,&readable);timeval wait{0,500000};if(select(0,&readable,nullptr,nullptr,&wait)!=1)continue;
            char buffer[600];sockaddr_in from{};int fromSize=sizeof(from);const int n=recvfrom(udp,buffer,sizeof(buffer),0,reinterpret_cast<sockaddr*>(&from),&fromSize);if(n<=0)continue;
            const std::string text(buffer,size_t(n));std::vector<std::string> parts;size_t start=0;for(int k=0;k<3;++k){const auto at=text.find('\n',start);if(at==std::string::npos)break;parts.push_back(text.substr(start,at-start));start=at+1;}
            if(parts.size()!=3||parts[0]!="ARNAVSHARE1"||unhex(parts[1]).size()!=16||parts[1]==hex(id))continue;
            const int port=std::atoi(parts[2].c_str());if(port<=0||port>65535)continue;
            char address[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&from.sin_addr,address,sizeof(address));const std::wstring name=cleanName(wide(text.substr(start)));
            bool changed=false;{std::lock_guard lock(m);auto& p=peers[parts[1]];changed=!p.online||p.address!=address||p.port!=port||(p.key.empty()&&p.name!=name);
                p.address=address;p.port=uint16_t(port);p.online=true;p.seen=Clock::now();if(p.key.empty()||p.name.empty())p.name=name;}
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
        Bytes reply;if(!sendFrame(s,hello)||!recvFrame(s,reply)||reply.size()<6||std::memcmp(reply.data(),magic,4)!=0||reply[4]!=protocolVersion)return false;
        if(reply[5]!=0){ss.rejected=true;return false;}
        if(reply.size()<6+16+64+32)return false;
        ss.peerId.assign(reply.begin()+6,reply.begin()+22);ss.peerPub.assign(reply.begin()+22,reply.begin()+86);const Bytes theirs(reply.begin()+86,reply.begin()+118);ss.peerName=cleanName(wide(std::string(reply.begin()+118,reply.end())));
        return sendFrame(s,nonce)&&keys(ss,true,nonce,theirs);
    }
    // The responder takes pairing only when none is under way, and files only from a paired PC with its paired key.
    bool welcome(SOCKET s,Session& ss,uint8_t& mode,std::shared_ptr<Decision>& claim){
        Bytes hello;if(!recvFrame(s,hello)||hello.size()<6+16+64+32||std::memcmp(hello.data(),magic,4)!=0||hello[4]!=protocolVersion)return false;
        mode=hello[5];ss.peerId.assign(hello.begin()+6,hello.begin()+22);ss.peerPub.assign(hello.begin()+22,hello.begin()+86);const Bytes commit(hello.begin()+86,hello.begin()+118);
        ss.peerName=cleanName(wide(std::string(hello.begin()+118,hello.end())));if(ss.peerId==id)return false;
        bool allowed=false;{std::lock_guard lock(m);
            if(mode==modePair&&!pairing){pairing=claim=std::make_shared<Decision>();allowed=true;}
            else if(mode==modeSend){auto it=peers.find(hex(ss.peerId));allowed=it!=peers.end()&&!it->second.key.empty()&&it->second.key==ss.peerPub;}}
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
        if(both){std::lock_guard lock(m);auto& p=peers[peer];p.key=ss.peerPub;if(p.name.empty()||p.name==L"A PC")p.name=ss.peerName;savePeers();}
        releasePairing(d);
        fail(both?ShareEvent::Kind::Paired:ShareEvent::Kind::PairFailed,peer,ss.peerName,{},both?L"You can send files between these PCs now":!talked?L"The other PC stopped answering":mine!=1?L"Not paired":L"Not confirmed on the other PC");postPeers();
    }
    void incoming(SOCKET s){
        timeout(s,15000);Session ss;uint8_t mode=0;std::shared_ptr<Decision> claim;
        if(!welcome(s,ss,mode,claim)){if(claim)releasePairing(claim);return;}
        if(mode==modePair)pairSession(s,ss,claim);else receive(s,ss);
    }
    void receive(SOCKET s,Session& ss){
        const std::string peer=hex(ss.peerId);const std::wstring from=nameOf(peer,ss.peerName);
        Bytes offer;if(!opened(s,ss.channel,offer)||offer.size()<9)return;
        const uint64_t size=get64(offer.data());const std::wstring file=safeShareName(wide(std::string(offer.begin()+8,offer.end())));if(size>maxFile)return;
        auto d=std::make_shared<Decision>();uint32_t transfer=0;{std::lock_guard lock(m);transfer=nextTransfer++;offers[transfer]=d;}
        {ShareEvent e;e.kind=ShareEvent::Kind::Offer;e.peer=peer;e.name=from;e.file=file;e.size=size;e.transfer=transfer;post(e);}
        timeout(s,90000);const int yes=d->wait(60);{std::lock_guard lock(m);offers.erase(transfer);}
        if(!sealed(s,ss.channel,Bytes{uint8_t(yes==1?1:0)})||yes!=1)return;
        timeout(s,30000);std::error_code error;const fs::path dir=o.downloads;fs::create_directories(dir,error);
        const fs::path part=uniquePath(dir,file+L".arnavpart");bool done=false;
        {std::ofstream out(part,std::ios::binary|std::ios::trunc);Sha hash;uint64_t got=0;int shown=-1;
            while(out){Bytes f;if(!opened(s,ss.channel,f)||f.empty())break;
                if(f[0]==1){const size_t n=f.size()-1;if(got+n>size)break;out.write(reinterpret_cast<const char*>(f.data()+1),std::streamsize(n));hash.add(f.data()+1,n);got+=n;
                    const int percent=size?int(got*100/size):100;if(percent/10!=shown/10){shown=percent;ShareEvent e;e.kind=ShareEvent::Kind::Progress;e.peer=peer;e.name=from;e.file=file;e.size=size;e.done=got;post(e);}}
                else if(f[0]==2&&f.size()==1+8+32){done=get64(f.data()+1)==size&&got==size&&hash.done()==Bytes(f.begin()+9,f.end());break;}
                else break;}
            out.close();done=done&&!out.fail();}
        if(!done){fs::remove(part,error);fail(ShareEvent::Kind::Failed,peer,from,file,L"The file from "+from+L" did not arrive whole");return;}
        const fs::path final=uniquePath(dir,file);fs::rename(part,final,error);if(error){fs::remove(part,error);fail(ShareEvent::Kind::Failed,peer,from,file,L"Couldn't save to Downloads");return;}
        sealed(s,ss.channel,Bytes{1});
        ShareEvent e;e.kind=ShareEvent::Kind::Received;e.peer=peer;e.name=from;e.file=final.filename().wstring();e.detail=final.wstring();e.size=size;post(e);
    }
    void sendFile(const std::string& peer,const std::wstring& path){
        Peer target;{std::lock_guard lock(m);auto it=peers.find(peer);if(it!=peers.end())target=it->second;}
        const std::wstring file=fs::path(path).filename().wstring();
        if(target.key.empty()){fail(ShareEvent::Kind::Failed,peer,target.name,file,L"Pair with this PC first");return;}
        if(!target.online){fail(ShareEvent::Kind::Failed,peer,target.name,file,target.name+L" isn't on this network right now");return;}
        std::error_code error;const uint64_t size=fs::file_size(path,error);if(error||!fs::is_regular_file(path,error)){fail(ShareEvent::Kind::Failed,peer,target.name,file,L"Only files can be sent");return;}
        if(size>maxFile){fail(ShareEvent::Kind::Failed,peer,target.name,file,L"Files up to 16 GB can be sent");return;}
        std::ifstream in(fs::path(path),std::ios::binary);if(!in){fail(ShareEvent::Kind::Failed,peer,target.name,file,L"The file couldn't be read");return;}
        SOCKET s=connectTo(target.address,target.port);if(s==INVALID_SOCKET){fail(ShareEvent::Kind::Failed,peer,target.name,file,L"Couldn't reach "+target.name);return;}
        track(s);timeout(s,15000);Session ss;bool sent=false;std::wstring why=L"The transfer stopped";
        if(!greet(s,modeSend,ss))why=ss.rejected?target.name+L" doesn't have this PC paired. Pair again from Nearby.":L"Couldn't reach "+target.name;
        else if(hex(ss.peerId)!=peer||ss.peerPub!=target.key)why=target.name+L" answered with a different key. Pair again from Nearby.";
        else{Bytes offer;put64(offer,size);append(offer,utf8(file));Bytes reply;timeout(s,90000);
            if(!sealed(s,ss.channel,offer)||!opened(s,ss.channel,reply)||reply.size()!=1)why=target.name+L" didn't answer";
            else if(reply[0]!=1)why=target.name+L" declined "+file;
            else{timeout(s,30000);Sha hash;uint64_t done=0;int shown=-1;Bytes frame;bool flowing=true;
                while(done<size){frame.assign(1+std::min<uint64_t>(chunkSize,size-done),0);frame[0]=1;in.read(reinterpret_cast<char*>(frame.data()+1),std::streamsize(frame.size()-1));
                    if(in.gcount()!=std::streamsize(frame.size()-1)||!sealed(s,ss.channel,frame)){flowing=false;break;}hash.add(frame.data()+1,frame.size()-1);done+=frame.size()-1;
                    const int percent=int(done*100/size);if(percent/10!=shown/10){shown=percent;ShareEvent e;e.kind=ShareEvent::Kind::Progress;e.peer=peer;e.name=target.name;e.file=file;e.size=size;e.done=done;post(e);}}
                Bytes end{2};put64(end,size);append(end,hash.done());Bytes ack;
                sent=flowing&&sealed(s,ss.channel,end)&&opened(s,ss.channel,ack)&&ack.size()==1&&ack[0]==1;if(!sent)why=L"The transfer to "+target.name+L" didn't finish";}}
        untrack(s);closesocket(s);
        if(sent){ShareEvent e;e.kind=ShareEvent::Kind::Sent;e.peer=peer;e.name=target.name;e.file=file;e.size=size;e.detail=sizeText(size);post(e);}
        else fail(ShareEvent::Kind::Failed,peer,target.name,file,why);
    }
    void pairWith(const std::string& peer,const std::shared_ptr<Decision>& d){
        Peer target;{std::lock_guard lock(m);auto it=peers.find(peer);if(it!=peers.end())target=it->second;}
        SOCKET s=target.online?connectTo(target.address,target.port):INVALID_SOCKET;
        if(s==INVALID_SOCKET){releasePairing(d);fail(ShareEvent::Kind::PairFailed,peer,target.name,{},L"Couldn't reach "+(target.name.empty()?std::wstring(L"that PC"):target.name));return;}
        track(s);timeout(s,15000);Session ss;
        if(!greet(s,modePair,ss)||hex(ss.peerId)!=peer){releasePairing(d);fail(ShareEvent::Kind::PairFailed,peer,target.name,{},ss.rejected?target.name+L" is busy pairing":L"Pairing didn't start");}
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
    std::vector<SharePeer> list;{std::lock_guard lock(core_->m);for(auto& [peer,p]:core_->peers)if(p.online||!p.key.empty())list.push_back({peer,p.name,!p.key.empty(),p.online});}
    std::stable_sort(list.begin(),list.end(),[](auto& a,auto& b){const int ra=(a.online?0:2)+(a.paired?0:1),rb=(b.online?0:2)+(b.paired?0:1);return ra!=rb?ra<rb:a.name<b.name;});return list;
}
void ShareService::addPeer(const std::string& peer,const std::wstring& name,const std::string& address,uint16_t port){
    {std::lock_guard lock(core_->m);auto& p=core_->peers[peer];if(p.key.empty()||p.name.empty())p.name=cleanName(name);p.address=address;p.port=port;p.online=true;p.seen=Clock::now()+std::chrono::hours(24);}core_->postPeers();}
void ShareService::pair(const std::string& peer){
    std::shared_ptr<Core::Decision> d;{std::lock_guard lock(core_->m);if(core_->pairing||!core_->ok)return;d=core_->pairing=std::make_shared<Core::Decision>();}
    std::thread([core=core_,peer,d]{core->pairWith(peer,d);}).detach();
}
void ShareService::confirmPair(bool yes){std::lock_guard lock(core_->m);if(core_->pairing)core_->pairing->set(yes?1:0);}
void ShareService::forget(const std::string& peer){{std::lock_guard lock(core_->m);auto it=core_->peers.find(peer);if(it!=core_->peers.end()){it->second.key.clear();core_->savePeers();}}core_->postPeers();}
void ShareService::send(const std::string& peer,const std::wstring& path){if(!core_->ok)return;std::thread([core=core_,peer,path]{core->sendFile(peer,path);}).detach();}
void ShareService::answer(uint32_t transfer,bool accept){std::lock_guard lock(core_->m);auto it=core_->offers.find(transfer);if(it!=core_->offers.end())it->second->set(accept?1:0);}
std::vector<ShareEvent> ShareService::take(){std::lock_guard lock(core_->m);std::vector<ShareEvent> out;out.swap(core_->events);return out;}
}
