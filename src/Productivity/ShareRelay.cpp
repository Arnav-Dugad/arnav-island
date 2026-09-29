#include <winsock2.h>
#include <ws2tcpip.h>
#include "ShareRelay.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
namespace nexus {
namespace {
using Bytes=std::vector<uint8_t>;
using Clock=std::chrono::steady_clock;
// Envelopes: [version, kind, sender id (16)] then the kind's fields. Sealed with AES-256-GCM (nonce first, tag last), the
// topic authenticated with them.
constexpr uint8_t envelopeVersion=1,kindHello=1,kindOpen=2,kindData=3,kindAck=4,kindClose=5;
// Hello flags.
constexpr uint8_t helloReply=1,helloPhone=2,helloLeaving=4;
// A tunnel's bytes go in messages of at most 48 KB; at most 64 are unacknowledged, and the receiver is asked to
// acknowledge once 24 are (measured: about 0.8 MB/s through a public broker, four times a window of 32 KB x 32).
constexpr size_t chunk=48*1024;constexpr uint32_t window=64,ackEvery=24;
// 0.20.1: a public broker may drop a message now and then (QoS 0). What isn't acknowledged in time is sent again (the
// wait doubling to 8 s), a receiver keeps what comes early and says at once what it's missing (at most every 300 ms), and
// a tunnel ends only when nothing got through for 45 s (or a gap stayed open 30 s: a sender that never sends again).
constexpr auto rtoBase=std::chrono::milliseconds(1500),rtoMost=std::chrono::milliseconds(8000),nackEvery=std::chrono::milliseconds(300);
constexpr auto giveUp=std::chrono::seconds(45),gapGiveUp=std::chrono::seconds(30);constexpr size_t resendBatch=8,dataFlags=2+16+8+4;
constexpr auto helloEvery=std::chrono::seconds(60),helloGone=std::chrono::seconds(150);constexpr auto hostFor=std::chrono::minutes(10);
const char topicPrefix[]="arnavisland/r1/";
const char codeAlphabet[]="23456789ABCDEFGHJKMNPQRSTUVWXYZ";
// WINHTTP_OPTION_IPV6_FAST_FALLBACK (not in MinGW's winhttp.h).
constexpr DWORD ipv6FastFallback=140;
bool good(LONG s){return s>=0;}
struct Algorithms{BCRYPT_ALG_HANDLE aes=nullptr,sha=nullptr;bool ready=false;
    Algorithms(){ready=good(BCryptOpenAlgorithmProvider(&aes,BCRYPT_AES_ALGORITHM,nullptr,0))&&
        good(BCryptSetProperty(aes,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),ULONG(sizeof(BCRYPT_CHAIN_MODE_GCM)),0))&&
        good(BCryptOpenAlgorithmProvider(&sha,BCRYPT_SHA256_ALGORITHM,nullptr,0));}};
Algorithms& algorithms(){static Algorithms a;return a;}
Bytes sha256(const Bytes& data){Bytes out(32);BCRYPT_HASH_HANDLE h=nullptr;if(!good(BCryptCreateHash(algorithms().sha,&h,nullptr,0,nullptr,0,0)))return {};
    const bool done=good(BCryptHashData(h,const_cast<PUCHAR>(data.data()),ULONG(data.size()),0))&&good(BCryptFinishHash(h,out.data(),32,0));BCryptDestroyHash(h);return done?out:Bytes{};}
Bytes cat(std::initializer_list<Bytes> parts){Bytes out;for(auto& p:parts)out.insert(out.end(),p.begin(),p.end());return out;}
Bytes text(const char* s){return Bytes(s,s+std::strlen(s));}
Bytes randomBytes(size_t n){Bytes b(n);if(!good(BCryptGenRandom(nullptr,b.data(),ULONG(n),BCRYPT_USE_SYSTEM_PREFERRED_RNG)))return {};return b;}
std::string hex(const Bytes& b){static const char* d="0123456789abcdef";std::string s;for(uint8_t c:b){s+=d[c>>4];s+=d[c&15];}return s;}
std::string topicOf(const Bytes& hash){return topicPrefix+hex(hash).substr(0,40);}
void put32(Bytes& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(uint8_t(v>>(8*i)));}
void put64(Bytes& b,uint64_t v){for(int i=0;i<8;++i)b.push_back(uint8_t(v>>(8*i)));}
uint32_t get32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint64_t get64(const uint8_t* p){uint64_t v=0;for(int i=0;i<8;++i)v|=uint64_t(p[i])<<(8*i);return v;}
std::string utf8(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(std::max(0,n)),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
std::wstring wide(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(std::max(0,n)),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
// AES-256-GCM with a random nonce per message (a pair sends far fewer than 2^32), the topic as associated data.
struct Seal{
    BCRYPT_KEY_HANDLE key=nullptr;mutable std::mutex m;
    explicit Seal(const Bytes& k){if(k.size()==32)BCryptGenerateSymmetricKey(algorithms().aes,&key,nullptr,0,const_cast<PUCHAR>(k.data()),ULONG(k.size()),0);}
    ~Seal(){if(key)BCryptDestroyKey(key);}
    Seal(const Seal&)=delete;Seal& operator=(const Seal&)=delete;
    Bytes seal(const Bytes& plain,const std::string& topic)const{
        const Bytes nonce=randomBytes(12);if(!key||nonce.size()!=12||plain.empty())return {};Bytes out(12+plain.size()+16);std::copy(nonce.begin(),nonce.end(),out.begin());
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;BCRYPT_INIT_AUTH_MODE_INFO(info);info.pbNonce=out.data();info.cbNonce=12;
        info.pbAuthData=reinterpret_cast<PUCHAR>(const_cast<char*>(topic.data()));info.cbAuthData=ULONG(topic.size());info.pbTag=out.data()+12+plain.size();info.cbTag=16;ULONG got=0;
        std::lock_guard lock(m);
        if(!good(BCryptEncrypt(key,const_cast<PUCHAR>(plain.data()),ULONG(plain.size()),&info,nullptr,0,out.data()+12,ULONG(plain.size()),&got,0))||got!=plain.size())return {};
        return out;}
    bool open(const uint8_t* in,size_t n,const std::string& topic,Bytes& plain)const{
        if(!key||n<=12+16)return false;const size_t body=n-12-16;plain.assign(body,0);Bytes nonce(in,in+12),tag(in+12+body,in+n);
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;BCRYPT_INIT_AUTH_MODE_INFO(info);info.pbNonce=nonce.data();info.cbNonce=12;
        info.pbAuthData=reinterpret_cast<PUCHAR>(const_cast<char*>(topic.data()));info.cbAuthData=ULONG(topic.size());info.pbTag=tag.data();info.cbTag=16;ULONG got=0;
        std::lock_guard lock(m);
        return good(BCryptDecrypt(key,const_cast<PUCHAR>(in+12),ULONG(body),&info,nullptr,0,plain.data(),ULONG(body),&got,0))&&got==body;}
};
// MQTT 3.1.1: the few packets needed, QoS 0 throughout.
void varint(Bytes& b,size_t n){do{uint8_t d=uint8_t(n%128);n/=128;if(n)d|=128;b.push_back(d);}while(n);}
void mstring(Bytes& b,const std::string& s){b.push_back(uint8_t(s.size()>>8));b.push_back(uint8_t(s.size()&255));b.insert(b.end(),s.begin(),s.end());}
Bytes packet(uint8_t head,const Bytes& body){Bytes p{head};varint(p,body.size());p.insert(p.end(),body.begin(),body.end());return p;}
Bytes mqttConnect(const std::string& client){Bytes b;mstring(b,"MQTT");b.push_back(4);b.push_back(2);b.push_back(0);b.push_back(60);mstring(b,client);return packet(0x10,b);}
Bytes mqttSubscribe(uint16_t id,const std::vector<std::string>& topics){Bytes b{uint8_t(id>>8),uint8_t(id&255)};for(auto& t:topics){mstring(b,t);b.push_back(0);}return packet(0x82,b);}
Bytes mqttUnsubscribe(uint16_t id,const std::vector<std::string>& topics){Bytes b{uint8_t(id>>8),uint8_t(id&255)};for(auto& t:topics)mstring(b,t);return packet(0xA2,b);}
Bytes mqttPublish(const std::string& topic,const Bytes& payload){Bytes b;mstring(b,topic);b.insert(b.end(),payload.begin(),payload.end());return packet(0x30,b);}
// The next whole packet at the front of `in` (its first byte and body): 1, 0 when more is needed, -1 when malformed.
int mqttNext(Bytes& in,uint8_t& head,Bytes& body){
    if(in.size()<2)return 0;size_t n=0,mult=1,at=1;
    for(;;){if(at>=in.size())return 0;const uint8_t d=in[at++];n+=size_t(d&127)*mult;if(!(d&128))break;mult*=128;if(at>4)return -1;}
    if(in.size()<at+n)return 0;head=in[0];body.assign(in.begin()+long(at),in.begin()+long(at+n));in.erase(in.begin(),in.begin()+long(at+n));return 1;
}
bool sendAllTo(SOCKET s,const Bytes& b){size_t at=0;while(at<b.size()){const int k=::send(s,reinterpret_cast<const char*>(b.data()+at),int(std::min<size_t>(b.size()-at,64*1024)),0);if(k<=0)return false;at+=size_t(k);}return true;}
}

bool loopbackPair(SOCKET& a,SOCKET& b){
    a=b=INVALID_SOCKET;SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==INVALID_SOCKET)return false;
    sockaddr_in at{};at.sin_family=AF_INET;at.sin_addr.s_addr=htonl(INADDR_LOOPBACK);at.sin_port=0;int size=sizeof(at);
    bool made=bind(listener,reinterpret_cast<sockaddr*>(&at),sizeof(at))==0&&listen(listener,1)==0&&getsockname(listener,reinterpret_cast<sockaddr*>(&at),&size)==0;
    if(made){a=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);made=a!=INVALID_SOCKET&&connect(a,reinterpret_cast<sockaddr*>(&at),sizeof(at))==0;}
    if(made){sockaddr_in peer{};int peerSize=sizeof(peer);b=accept(listener,reinterpret_cast<sockaddr*>(&peer),&peerSize);
        // Only our own end may be the one accepted (another program could race to the port).
        sockaddr_in mine{};int mineSize=sizeof(mine);made=b!=INVALID_SOCKET&&getsockname(a,reinterpret_cast<sockaddr*>(&mine),&mineSize)==0&&mine.sin_port==peer.sin_port&&mine.sin_addr.s_addr==peer.sin_addr.s_addr;}
    closesocket(listener);
    if(!made){if(a!=INVALID_SOCKET)closesocket(a);if(b!=INVALID_SOCKET)closesocket(b);a=b=INVALID_SOCKET;return false;}
    BOOL on=TRUE;setsockopt(a,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&on),sizeof(on));setsockopt(b,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&on),sizeof(on));
    return true;
}
std::string relayCode(const std::string& typed){
    std::string out;for(char c:typed){if(c==' '||c=='-')continue;c=char(std::toupper(static_cast<unsigned char>(c)));if(!std::strchr(codeAlphabet,c)||c==0)return {};out+=c;}
    return out.size()==8?out:std::string();
}

struct Relay::Core:std::enable_shared_from_this<Core>{
    RelayOptions o;
    // 0.20.1: every broker at once. Two devices are sure to share one even when one of them can't reach (or briefly
    // lost) the others: hellos and pairing codes go out on all of them, and a tunnel keeps to one both are on.
    static constexpr size_t maxBrokers=6;
    // Where messages to this device arrive (its inbox topic): whose they are, how they're sealed, where replies go, and on
    // which brokers the other device was last heard.
    struct Route{std::string peer;std::shared_ptr<Seal> seal;std::string outbox;bool code=false,host=false;std::array<Clock::time_point,maxBrokers> heard{};Clock::time_point helloed{};RelayPresence presence;};
    // A tunnel keeps to one broker (-1 until the first message from the other side says which: a code's tunnel opens on all).
    struct Tunnel{uint64_t conn=0;SOCKET outer=INVALID_SOCKET;std::string outbox;std::shared_ptr<Seal> seal;int broker=-1;
        std::mutex m;std::condition_variable cv;uint32_t sent=0,acked=0,expected=0,acknowledged=0;std::deque<Bytes> inbound;bool inEnd=false,dead=false,closeSent=false;int finished=0;
        // 0.20.1: what was sent and not yet acknowledged (its messages, to send again), what came early, the OPEN this side
        // sent (again while the other side hasn't answered), and the clocks for them.
        std::map<uint32_t,Bytes> unacked,early;Bytes open;bool heard=false;uint32_t nackedFor=UINT32_MAX;std::chrono::milliseconds rto=rtoBase;
        Clock::time_point progressAt{},resendAt{},resentAt{},nackedAt{},dupAckAt{},gapSince{};};
    // One broker's WebSocket and what MQTT needs of it.
    struct Broker{size_t index=0;std::wstring host;uint16_t port=0;std::mutex wsMutex,sendMutex;HINTERNET session=nullptr,link=nullptr,ws=nullptr;
        std::atomic<bool> up{false};std::atomic<int64_t> lastIn{0},lastPing{0};std::thread runner;};
    std::vector<std::unique_ptr<Broker>> brokers;
    mutable std::mutex m;std::map<std::string,Route> routes;std::map<uint64_t,std::shared_ptr<Tunnel>> tunnels;
    std::string code;Clock::time_point codeUntil{};
    // Tunnels that ended lately: an OPEN sent again after one ended starts nothing.
    std::map<uint64_t,Clock::time_point> ended;std::thread resender;std::atomic<int> lost{0};
    uint16_t nextPacket=1;std::map<std::pair<size_t,uint16_t>,bool> subacks;std::condition_variable subacked;
    std::atomic<bool> stopping{false};std::thread ticker;std::mutex sleepMutex;std::condition_variable sleeper;
    static int64_t now(){return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();}
    void makeBrokers(){
        auto list=o.brokers.empty()?std::vector<std::pair<std::wstring,uint16_t>>{{L"broker.hivemq.com",8884},{L"broker.emqx.io",8084},{L"test.mosquitto.org",8081}}:o.brokers;
        if(list.size()>maxBrokers)list.resize(maxBrokers);
        for(size_t i=0;i<list.size();++i){auto b=std::make_unique<Broker>();b->index=i;b->host=list[i].first;b->port=list[i].second;brokers.push_back(std::move(b));}
    }
    bool anyUp()const{for(auto& b:brokers)if(b->up)return true;return false;}
    // Present on a broker: heard there within the last 150 s (the peer says hello on each once a minute).
    static bool hereOn(const Route& r,size_t b,Clock::time_point clock){return r.heard[b]!=Clock::time_point{}&&clock-r.heard[b]<=helloGone;}
    bool hereAnywhere(const Route& r,Clock::time_point clock)const{for(size_t b=0;b<brokers.size();++b)if(brokers[b]->up&&hereOn(r,b,clock))return true;return false;}
    // The broker to open a tunnel on: the one the other device was heard on most lately (and this one is on).
    int bestBroker(const Route& r)const{int best=-1;Clock::time_point when{};for(size_t b=0;b<brokers.size();++b)if(brokers[b]->up&&r.heard[b]>when){when=r.heard[b];best=int(b);}return best;}

    // ---- a broker ----
    bool write(Broker& k,const Bytes& b){HINTERNET h;{std::lock_guard lock(k.wsMutex);h=k.ws;}if(!h)return false;std::lock_guard lock(k.sendMutex);
        return WinHttpWebSocketSend(h,WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,const_cast<uint8_t*>(b.data()),DWORD(b.size()))==NO_ERROR;}
    // To one broker (0..n-1), or to every broker that is up (-1); true when at least one took it.
    bool writeTo(int broker,const Bytes& b){if(broker>=0)return size_t(broker)<brokers.size()&&brokers[size_t(broker)]->up&&write(*brokers[size_t(broker)],b);
        bool any=false;for(auto& k:brokers)if(k->up&&write(*k,b))any=true;return any;}
    void abortSocket(Broker& k){std::lock_guard lock(k.wsMutex);if(k.ws){WinHttpCloseHandle(k.ws);k.ws=nullptr;}}
    void closeHandles(Broker& k){abortSocket(k);if(k.link){WinHttpCloseHandle(k.link);k.link=nullptr;}if(k.session){WinHttpCloseHandle(k.session);k.session=nullptr;}}
    bool open(Broker& k){
        k.session=WinHttpOpen(L"ArnavIsland/0.20",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!k.session)return false;
        // A network whose IPv6 doesn't reach a broker would otherwise keep each attempt waiting on it before IPv4 is tried:
        // IPv4 and IPv6 race (Windows 10 2004 and later), and an address that doesn't answer is given 4 s.
        {DWORD fast=TRUE;WinHttpSetOption(k.session,ipv6FastFallback,&fast,sizeof(fast));}
        WinHttpSetTimeouts(k.session,5000,4000,10000,10000);
        k.link=WinHttpConnect(k.session,k.host.c_str(),k.port,0);if(!k.link)return false;
        HINTERNET request=WinHttpOpenRequest(k.link,L"GET",L"/mqtt",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!request)return false;
        bool made=WinHttpSetOption(request,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0)&&WinHttpAddRequestHeaders(request,L"Sec-WebSocket-Protocol: mqtt",DWORD(-1),WINHTTP_ADDREQ_FLAG_ADD)&&
            WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,nullptr,0,0,0)&&WinHttpReceiveResponse(request,nullptr);
        DWORD status=0,size=sizeof(status);if(made)made=WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)&&status==101;
        HINTERNET socket=made?WinHttpWebSocketCompleteUpgrade(request,0):nullptr;WinHttpCloseHandle(request);if(!socket)return false;
        {std::lock_guard lock(k.wsMutex);k.ws=socket;}return true;
    }
    // The broker's packets until the connection ends.
    void receive(Broker& k){
        Bytes stream,buffer(64*1024),body;uint8_t head=0;
        for(;;){HINTERNET h;{std::lock_guard lock(k.wsMutex);h=k.ws;}if(!h)return;
            DWORD got=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};if(WinHttpWebSocketReceive(h,buffer.data(),DWORD(buffer.size()),&got,&type)!=NO_ERROR||type==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)return;
            k.lastIn=now();stream.insert(stream.end(),buffer.begin(),buffer.begin()+got);
            for(;;){const int r=mqttNext(stream,head,body);if(r<0)return;if(r==0)break;handle(k,head,body);}}
    }
    void handle(Broker& k,uint8_t head,const Bytes& body){
        switch(head>>4){
        case 2:if(body.size()>=2&&body[1]==0&&!k.up.exchange(true)){onConnected(k);}break;
        case 9:if(body.size()>=2){std::lock_guard lock(m);auto it=subacks.find({k.index,uint16_t(body[0]<<8|body[1])});if(it!=subacks.end())it->second=true;subacked.notify_all();}break;
        case 3:{if(body.size()<2)return;const size_t n=size_t(body[0])<<8|body[1];if(body.size()<2+n)return;size_t at=2+n;if((head>>1)&3)at+=2;if(at>body.size())return;
            onPublish(k.index,std::string(body.begin()+2,body.begin()+long(2+n)),body.data()+at,body.size()-at);break;}
        default:break;}
    }
    // Each broker has its own connection, kept up, and called again soon after it drops (a broker that can't be reached
    // is tried less and less often, at most every minute).
    void run(Broker& k){
        int fails=0;
        while(!stopping){
            bool connected=false;
            if(open(k)){k.lastIn=now();const Bytes client=randomBytes(10);if(write(k,mqttConnect("ai"+hex(client))))receive(k);connected=k.up.load();}
            const bool was=k.up.exchange(false);closeHandles(k);brokerDown(k.index);if(was&&o.changed)o.changed();
            if(stopping)break;
            fails=connected?0:fails+1;
            const int wait=connected?1:std::min(60,1<<std::min(fails,6));
            std::unique_lock lock(sleepMutex);sleeper.wait_for(lock,std::chrono::seconds(wait),[&]{return stopping.load();});
        }
    }
    // A broker went: its tunnels end, and what was heard there no longer counts.
    void brokerDown(size_t b){
        std::vector<std::shared_ptr<Tunnel>> ending;
        {std::lock_guard lock(m);for(auto& [c,t]:tunnels)if(t->broker==int(b))ending.push_back(t);
            const auto clock=Clock::now();for(auto& [inbox,r]:routes){r.heard[b]={};r.presence.here=hereAnywhere(r,clock);}
            for(auto it=subacks.begin();it!=subacks.end();)if(it->first.first==b)it=subacks.erase(it);else ++it;}
        subacked.notify_all();
        for(auto& t:ending){{std::lock_guard lock(t->m);t->dead=true;t->closeSent=true;}t->cv.notify_all();shutdown(t->outer,SD_BOTH);}
    }
    // Once a minute each paired device hears from this one (on every broker); a device not heard from in 150 s is gone;
    // each broker is pinged, and a silent broker (or one that never answered) is left.
    void tick(){
        while(!stopping){
            {std::unique_lock lock(sleepMutex);sleeper.wait_for(lock,std::chrono::seconds(5),[&]{return stopping.load();});}if(stopping)break;
            const int64_t t=now();
            for(auto& k:brokers){HINTERNET h;{std::lock_guard lock(k->wsMutex);h=k->ws;}
                if(h&&t-k->lastIn>(k->up?100000:15000)){abortSocket(*k);continue;}
                if(k->up&&t-k->lastPing>=30000){k->lastPing=t;write(*k,Bytes{0xC0,0});}}
            if(!anyUp())continue;
            std::vector<std::string> hello,expired;bool changed=false;
            {std::lock_guard lock(m);const auto clock=Clock::now();
                for(auto& [inbox,r]:routes){if(r.code)continue;
                    if(clock-r.helloed>=helloEvery)hello.push_back(inbox);
                    const bool here=hereAnywhere(r,clock);if(r.presence.here!=here){r.presence.here=here;changed=true;}}
                if(!code.empty()&&clock>codeUntil)expired=stopHostingLocked();}
            if(!expired.empty())unsubscribe(expired);
            for(auto& inbox:hello)sayHello(inbox,false,false,-1);
            if(changed&&o.changed)o.changed();
        }
    }
    void onConnected(Broker& k){
        std::vector<std::string> inboxes,pairs;{std::lock_guard lock(m);for(auto& [inbox,r]:routes){inboxes.push_back(inbox);if(!r.code)pairs.push_back(inbox);}}
        if(!inboxes.empty())subscribe(inboxes,false,int(k.index));
        for(auto& inbox:pairs)sayHello(inbox,true,false,int(k.index));
        if(o.changed)o.changed();
    }
    // Subscribes on one broker, or on all that are up; waiting (up to 8 s) for them to confirm, true when one did.
    bool subscribe(const std::vector<std::string>& topics,bool wait,int only=-1){
        std::vector<std::pair<size_t,uint16_t>> asked;
        for(auto& k:brokers){if((only>=0&&k->index!=size_t(only))||!k->up)continue;
            uint16_t id;{std::lock_guard lock(m);id=nextPacket++;if(!nextPacket)nextPacket=1;if(wait)subacks[{k->index,id}]=false;}
            if(write(*k,mqttSubscribe(id,topics)))asked.push_back({k->index,id});else if(wait){std::lock_guard lock(m);subacks.erase({k->index,id});}}
        if(!wait)return !asked.empty();
        std::unique_lock lock(m);
        subacked.wait_for(lock,std::chrono::seconds(8),[&]{for(auto& a:asked){auto it=subacks.find(a);if(it!=subacks.end()&&!it->second)return false;}return true;});
        bool any=false;for(auto& a:asked){auto it=subacks.find(a);if(it!=subacks.end()){any=any||it->second;subacks.erase(it);}}
        return any;
    }
    // ---- messages ----
    Bytes envelope(uint8_t kind)const{Bytes b{envelopeVersion,kind};b.insert(b.end(),o.id.begin(),o.id.end());return b;}
    bool publish(const std::string& topic,const Seal& seal,const Bytes& plain,int broker){const Bytes sealed=seal.seal(plain,topic);return !sealed.empty()&&writeTo(broker,mqttPublish(topic,sealed));}
    void sayHello(const std::string& inbox,bool reply,bool leaving,int broker){
        std::string outbox;std::shared_ptr<Seal> seal;{std::lock_guard lock(m);auto it=routes.find(inbox);if(it==routes.end())return;outbox=it->second.outbox;seal=it->second.seal;if(broker<0)it->second.helloed=Clock::now();}
        Bytes b=envelope(kindHello);b.push_back(uint8_t((reply?helloReply:0)|(o.phone?helloPhone:0)|(leaving?helloLeaving:0)));b.push_back(uint8_t(o.revision));
        std::string name=utf8(o.name);if(name.size()>120)name.resize(120);b.push_back(uint8_t(name.size()));b.insert(b.end(),name.begin(),name.end());
        publish(outbox,*seal,b,broker);
    }
    void onPublish(size_t from,const std::string& topic,const uint8_t* payload,size_t n){
        Route r;{std::lock_guard lock(m);auto it=routes.find(topic);if(it==routes.end())return;r=it->second;}
        Bytes plain;if(!r.seal->open(payload,n,topic,plain)||plain.size()<18||plain[0]!=envelopeVersion)return;
        const Bytes sender(plain.begin()+2,plain.begin()+18);if(sender==o.id)return;if(!r.code&&hex(sender)!=r.peer)return;
        const uint8_t* p=plain.data()+18;const size_t left=plain.size()-18;
        // A tunnel's messages count only on its own broker (the first to carry one from the other side, when it had none).
        auto tunnelFor=[&](uint64_t conn)->std::shared_ptr<Tunnel>{std::lock_guard lock(m);auto it=tunnels.find(conn);if(it==tunnels.end())return nullptr;
            if(it->second->broker<0)it->second->broker=int(from);return it->second->broker==int(from)?it->second:nullptr;};
        switch(plain[1]){
        case kindHello:{if(r.code||left<3)return;const uint8_t flags=p[0];const size_t nameSize=std::min<size_t>(p[2],left-3);
            RelayPresence presence;presence.phone=flags&helloPhone;presence.revision=p[1];presence.name=wide(std::string(p+3,p+3+nameSize));
            bool changed=false;{std::lock_guard lock(m);auto it=routes.find(topic);if(it==routes.end())return;auto& route=it->second;const auto clock=Clock::now();
                // A goodbye is said on every broker: the device has gone from all of them.
                if(flags&helloLeaving)route.heard.fill({});else route.heard[from]=clock;
                presence.here=hereAnywhere(route,clock);auto& old=route.presence;
                changed=old.here!=presence.here||old.phone!=presence.phone||old.revision!=presence.revision||old.name!=presence.name;old=presence;}
            // Answered on the broker it came by, so the other device learns this one is there too.
            if(flags&helloReply)sayHello(topic,false,false,int(from));
            if(changed&&o.changed)o.changed();break;}
        case kindOpen:{if(left<8)return;const uint64_t conn=get64(p);{std::lock_guard lock(m);if(tunnels.count(conn)||ended.count(conn))return;}
            SOCKET inner,outer;if(!loopbackPair(inner,outer))return;
            auto t=start(conn,outer,r.outbox,r.seal,int(from));if(!t){closesocket(inner);return;}
            if(o.incoming){auto self=shared_from_this();const std::string peer=r.code?std::string():r.peer;std::thread([self,inner,peer]{self->o.incoming(inner,peer);}).detach();}
            else closesocket(inner);
            break;}
        case kindData:{if(left<13)return;const uint64_t conn=get64(p);const uint32_t seq=get32(p+8);const uint8_t flags=p[12];
            auto t=tunnelFor(conn);if(!t)return;
            bool reply=false;uint32_t next=0;
            {std::lock_guard lock(t->m);const auto clock=Clock::now();t->heard=true;
                // Had already (its acknowledgement was lost): said again, so the sender stops sending it.
                if(seq<t->expected){if(clock-t->dupAckAt>=nackEvery){t->dupAckAt=clock;reply=true;}}
                // Something before it was lost: this one is kept, and the sender told what's missing.
                else if(seq>t->expected){if(seq-t->expected<2*window)t->early[seq]=Bytes(p+13,p+left);if(t->gapSince==Clock::time_point{})t->gapSince=clock;
                    if(t->nackedFor!=t->expected||clock-t->nackedAt>=nackEvery){t->nackedFor=t->expected;t->nackedAt=clock;reply=true;}}
                else{t->inbound.emplace_back(p+13,p+left);++t->expected;bool filled=false;
                    for(auto it=t->early.find(t->expected);it!=t->early.end();it=t->early.find(t->expected)){t->inbound.push_back(std::move(it->second));t->early.erase(it);++t->expected;filled=true;}
                    t->gapSince=t->early.empty()?Clock::time_point{}:clock;
                    if((flags&1)||filled||t->expected-t->acknowledged>=ackEvery)reply=true;}
                if(reply){next=t->expected;t->acknowledged=std::max(t->acknowledged,next);}}
            t->cv.notify_all();
            if(reply){Bytes b=envelope(kindAck);put64(b,conn);put32(b,next);publish(t->outbox,*t->seal,b,brokerOf(t));}
            break;}
        case kindAck:{if(left<12)return;const uint64_t conn=get64(p);const uint32_t next=get32(p+8);auto t=tunnelFor(conn);if(!t)return;
            std::vector<Bytes> again;
            {std::lock_guard lock(t->m);const auto clock=Clock::now();t->heard=true;
                if(next>t->acked&&next<=t->sent){t->acked=next;t->unacked.erase(t->unacked.begin(),t->unacked.lower_bound(next));t->progressAt=clock;t->rto=rtoBase;t->resendAt=clock+rtoBase;}
                // The other side is missing this one: sent again at once, with a few after it.
                else if(next==t->acked&&next<t->sent&&clock-t->resentAt>=nackEvery){t->resentAt=clock;again=resendable(*t);}}
            t->cv.notify_all();
            if(!again.empty()){const int b=brokerOf(t);for(auto& msg:again)publish(t->outbox,*t->seal,msg,b);}
            break;}
        case kindClose:{if(left<8)return;const uint64_t conn=get64(p);auto t=tunnelFor(conn);
            if(!t)return;{std::lock_guard lock(t->m);t->inEnd=true;t->closeSent=true;}t->cv.notify_all();break;}
        default:break;}
    }
    // ---- tunnels ----
    std::shared_ptr<Tunnel> start(uint64_t conn,SOCKET outer,const std::string& outbox,const std::shared_ptr<Seal>& seal,int broker){
        auto t=std::make_shared<Tunnel>();t->conn=conn;t->outer=outer;t->outbox=outbox;t->seal=seal;t->broker=broker;
        {std::lock_guard lock(m);if(!anyUp()){closesocket(outer);return nullptr;}tunnels[conn]=t;}
        auto self=shared_from_this();
        std::thread([self,t]{self->pumpOut(t);}).detach();std::thread([self,t]{self->pumpIn(t);}).detach();
        return t;
    }
    int brokerOf(const std::shared_ptr<Tunnel>& t){std::lock_guard lock(m);return t->broker;}
    // Bytes the protocol wrote go out in numbered messages, never more than the window ahead of the acknowledgements.
    void pumpOut(std::shared_ptr<Tunnel> t){
        Bytes buffer(chunk);bool eof=false;
        for(;;){const int n=::recv(t->outer,reinterpret_cast<char*>(buffer.data()),int(chunk),0);if(n<=0){eof=n==0;break;}
            // The end of what the protocol wrote for now: the other side is asked to say it has it all.
            u_long more=0;const bool last=ioctlsocket(t->outer,FIONREAD,&more)!=0||more==0;
            uint32_t seq=0;bool ackNow=false;
            {std::unique_lock lock(t->m);if(!t->cv.wait_for(lock,std::chrono::seconds(60),[&]{return t->dead||t->sent-t->acked<window;})||t->dead)break;seq=t->sent++;ackNow=last||t->sent-t->acked>=ackEvery;}
            Bytes b=envelope(kindData);put64(b,t->conn);put32(b,seq);b.push_back(ackNow?1:0);b.insert(b.end(),buffer.begin(),buffer.begin()+n);
            {std::lock_guard lock(t->m);const auto clock=Clock::now();if(t->unacked.empty()){t->progressAt=clock;t->resendAt=clock+t->rto;}t->unacked[seq]=b;}
            if(o.loseEvery>0&&++lost%o.loseEvery==0)continue;
            if(!publish(t->outbox,*t->seal,b,brokerOf(t)))break;}
        // Everything written arrives before the end is said (what seems lost is sent again meanwhile), unless the other
        // side ended first.
        if(eof){std::unique_lock lock(t->m);t->cv.wait_for(lock,std::chrono::seconds(20),[&]{return t->dead||t->inEnd||t->acked>=t->sent;});}
        kill(t);
        finish(t);
    }
    // Called with t.m held: the first few unacknowledged messages, the last asking to be acknowledged.
    static std::vector<Bytes> resendable(Tunnel& t){std::vector<Bytes> out;for(auto& [seq,b]:t.unacked){if(out.size()==resendBatch)break;out.push_back(b);}
        if(!out.empty()&&out.back().size()>dataFlags)out.back()[dataFlags]|=1;return out;}
    // Five times a second: what wasn't acknowledged in time is sent again (with its OPEN while nothing was heard back); a
    // tunnel that got nowhere for too long ends.
    void resend(){
        while(!stopping){
            {std::unique_lock lock(sleepMutex);sleeper.wait_for(lock,std::chrono::milliseconds(200),[&]{return stopping.load();});}if(stopping)break;
            std::vector<std::shared_ptr<Tunnel>> all;
            {std::lock_guard lock(m);const auto clock=Clock::now();for(auto& [c,t]:tunnels)all.push_back(t);
                for(auto it=ended.begin();it!=ended.end();)if(clock-it->second>std::chrono::minutes(2))it=ended.erase(it);else ++it;}
            for(auto& t:all){std::vector<Bytes> again;Bytes open;bool end=false;
                {std::lock_guard lock(t->m);if(t->dead)continue;const auto clock=Clock::now();
                    if(!t->unacked.empty()&&clock>=t->resendAt){
                        if(clock-t->progressAt>giveUp)end=true;
                        else{again=resendable(*t);if(!t->heard)open=t->open;t->resentAt=clock;t->rto=std::min<std::chrono::milliseconds>(t->rto*2,rtoMost);t->resendAt=clock+t->rto;}}
                    if(t->gapSince!=Clock::time_point{}&&clock-t->gapSince>gapGiveUp)end=true;}
                if(end){kill(t);continue;}
                const int b=brokerOf(t);if(!open.empty())publish(t->outbox,*t->seal,open,b);for(auto& msg:again)publish(t->outbox,*t->seal,msg,b);}
        }
    }
    // Messages from the other side, in order, into the protocol's socket; the end of them is its end of file.
    void pumpIn(std::shared_ptr<Tunnel> t){
        for(;;){Bytes b;{std::unique_lock lock(t->m);t->cv.wait(lock,[&]{return t->dead||!t->inbound.empty()||t->inEnd;});if(t->dead)break;if(t->inbound.empty())break;b=std::move(t->inbound.front());t->inbound.pop_front();}
            if(!sendAllTo(t->outer,b)){kill(t);break;}}
        shutdown(t->outer,SD_SEND);
        finish(t);
    }
    void sendClose(const std::shared_ptr<Tunnel>& t){{std::lock_guard lock(t->m);if(t->closeSent)return;t->closeSent=true;}Bytes b=envelope(kindClose);put64(b,t->conn);publish(t->outbox,*t->seal,b,brokerOf(t));}
    void kill(const std::shared_ptr<Tunnel>& t){{std::lock_guard lock(t->m);if(t->dead)return;t->dead=true;}t->cv.notify_all();shutdown(t->outer,SD_BOTH);sendClose(t);}
    void finish(const std::shared_ptr<Tunnel>& t){bool last;{std::lock_guard lock(t->m);last=++t->finished==2;}if(!last)return;closesocket(t->outer);std::lock_guard lock(m);tunnels.erase(t->conn);ended[t->conn]=Clock::now();}
    void dropAll(){std::vector<std::shared_ptr<Tunnel>> all;{std::lock_guard lock(m);for(auto& [c,t]:tunnels)all.push_back(t);for(auto& [inbox,r]:routes){r.heard.fill({});r.presence.here=false;}}
        for(auto& t:all){{std::lock_guard lock(t->m);t->dead=true;t->closeSent=true;}t->cv.notify_all();shutdown(t->outer,SD_BOTH);}}
    SOCKET openTunnel(const std::string& outbox,const std::shared_ptr<Seal>& seal,int broker){
        SOCKET inner,outer;if(!loopbackPair(inner,outer))return INVALID_SOCKET;uint64_t conn=0;const Bytes r=randomBytes(8);for(int i=0;i<8&&r.size()==8;++i)conn|=uint64_t(r[size_t(i)])<<(8*i);
        auto t=start(conn,outer,outbox,seal,broker);if(!t){closesocket(inner);return INVALID_SOCKET;}
        Bytes b=envelope(kindOpen);put64(b,conn);{std::lock_guard lock(t->m);t->open=b;}
        if(!publish(outbox,*seal,b,broker)){kill(t);closesocket(inner);return INVALID_SOCKET;}
        return inner;
    }
    // Ends hosting a code; returns the topics to leave.
    std::vector<std::string> stopHostingLocked(){std::vector<std::string> left;code.clear();for(auto it=routes.begin();it!=routes.end();)if(it->second.host){left.push_back(it->first);it=routes.erase(it);}else ++it;return left;}
    void unsubscribe(const std::vector<std::string>& topics){for(auto& k:brokers){if(!k->up)continue;uint16_t id;{std::lock_guard lock(m);id=nextPacket++;if(!nextPacket)nextPacket=1;}write(*k,mqttUnsubscribe(id,topics));}}
};

Relay::Relay(RelayOptions options):core_(std::make_shared<Core>()){
    core_->o=std::move(options);core_->makeBrokers();auto c=core_;
    for(auto& k:c->brokers){Core::Broker* raw=k.get();k->runner=std::thread([c,raw]{c->run(*raw);});}
    c->ticker=std::thread([c]{c->tick();});c->resender=std::thread([c]{c->resend();});
}
Relay::~Relay(){
    auto& c=*core_;
    // A goodbye on every broker, so the other devices know at once.
    if(c.anyUp()){std::vector<std::string> pairs;{std::lock_guard lock(c.m);for(auto& [inbox,r]:c.routes)if(!r.code)pairs.push_back(inbox);}for(auto& inbox:pairs)c.sayHello(inbox,false,true,-1);c.writeTo(-1,Bytes{0xE0,0});}
    c.stopping=true;c.sleeper.notify_all();for(auto& k:c.brokers)c.abortSocket(*k);
    for(auto& k:c.brokers)if(k->runner.joinable())k->runner.join();if(c.ticker.joinable())c.ticker.join();if(c.resender.joinable())c.resender.join();
    c.dropAll();
}
void Relay::pairs(std::vector<RelayPair> list){
    auto& c=*core_;std::vector<std::string> added;
    {std::lock_guard lock(c.m);std::map<std::string,Core::Route> next;
        for(auto& p:list){if(p.peer.size()!=16||p.agreed.size()!=32)continue;
            const Bytes secret=sha256(cat({text("arnav-relay-v1"),p.agreed}));
            const std::string inbox=topicOf(sha256(cat({text("inbox"),secret,c.o.id}))),outbox=topicOf(sha256(cat({text("inbox"),secret,p.peer})));
            auto old=c.routes.find(inbox);
            if(old!=c.routes.end()&&!old->second.code){next[inbox]=old->second;continue;}
            Core::Route r;r.peer=hex(p.peer);r.outbox=outbox;r.seal=std::make_shared<Seal>(sha256(cat({text("arnav-relay-key"),secret})));next[inbox]=r;added.push_back(inbox);}
        for(auto& [inbox,r]:c.routes)if(r.code)next[inbox]=r;
        c.routes=std::move(next);}
    if(c.anyUp()&&!added.empty()){c.subscribe(added,false);for(auto& inbox:added)c.sayHello(inbox,true,false,-1);}
}
RelayPresence Relay::presence(const std::string& peer)const{std::lock_guard lock(core_->m);for(auto& [inbox,r]:core_->routes)if(!r.code&&r.peer==peer)return r.presence;return {};}
bool Relay::connected()const{return core_->anyUp();}
std::wstring Relay::broker()const{std::wstring names;for(auto& k:core_->brokers)if(k->up)names+=(names.empty()?L"":L", ")+k->host;return names;}
int Relay::brokersUp()const{int n=0;for(auto& k:core_->brokers)if(k->up)++n;return n;}
SOCKET Relay::open(const std::string& peer,std::wstring& why){
    auto& c=*core_;std::string outbox;std::shared_ptr<Seal> seal;bool here=false;std::wstring name;int broker=-1;
    {std::lock_guard lock(c.m);for(auto& [inbox,r]:c.routes)if(!r.code&&r.peer==peer){outbox=r.outbox;seal=r.seal;here=r.presence.here;name=r.presence.name;broker=c.bestBroker(r);break;}}
    if(!seal){why=L"Pair with it first";return INVALID_SOCKET;}
    if(!c.anyUp()){why=L"This PC isn't connected to the internet";return INVALID_SOCKET;}
    if(!here||broker<0){why=(name.empty()?std::wstring(L"It"):name)+L" isn't online";return INVALID_SOCKET;}
    const SOCKET s=c.openTunnel(outbox,seal,broker);if(s==INVALID_SOCKET)why=L"The connection through the internet failed";return s;
}
std::string Relay::host(){
    auto& c=*core_;if(!c.anyUp())return {};const Bytes r=randomBytes(8);if(r.size()!=8)return {};
    std::string code;for(uint8_t b:r)code+=codeAlphabet[b%31];
    const Bytes secret=sha256(cat({text("arnav-pair-code-v1"),Bytes(code.begin(),code.end())}));
    const std::string inbox=topicOf(sha256(cat({text("host"),secret}))),outbox=topicOf(sha256(cat({text("guest"),secret})));
    std::vector<std::string> left;{std::lock_guard lock(c.m);left=c.stopHostingLocked();Core::Route route;route.code=route.host=true;route.outbox=outbox;route.seal=std::make_shared<Seal>(sha256(cat({text("arnav-pair-code-key"),secret})));
        c.routes[inbox]=route;c.code=code;c.codeUntil=Clock::now()+hostFor;}
    if(!left.empty())c.unsubscribe(left);
    // Listened for on every broker, so whichever the other device reaches finds it.
    if(!c.subscribe({inbox},true)){stopHosting();return {};}
    return code.substr(0,4)+"-"+code.substr(4);
}
void Relay::stopHosting(){std::vector<std::string> left;{std::lock_guard lock(core_->m);left=core_->stopHostingLocked();}if(!left.empty())core_->unsubscribe(left);}
std::string Relay::hosting()const{std::lock_guard lock(core_->m);return core_->code.empty()?std::string():core_->code.substr(0,4)+"-"+core_->code.substr(4);}
SOCKET Relay::openCode(const std::string& typed,std::wstring& why){
    auto& c=*core_;const std::string code=relayCode(typed);if(code.empty()){why=L"That isn't a pairing code";return INVALID_SOCKET;}
    if(!c.anyUp()){why=L"This PC isn't connected to the internet";return INVALID_SOCKET;}
    const Bytes secret=sha256(cat({text("arnav-pair-code-v1"),Bytes(code.begin(),code.end())}));
    const std::string inbox=topicOf(sha256(cat({text("guest"),secret}))),outbox=topicOf(sha256(cat({text("host"),secret})));
    auto seal=std::make_shared<Seal>(sha256(cat({text("arnav-pair-code-key"),secret})));
    {std::lock_guard lock(c.m);Core::Route route;route.code=true;route.outbox=outbox;route.seal=seal;c.routes[inbox]=route;}
    if(!c.subscribe({inbox},true)){why=L"The connection through the internet failed";return INVALID_SOCKET;}
    // Opened on every broker: the tunnel keeps to whichever the other device answers on.
    const SOCKET s=c.openTunnel(outbox,seal,-1);if(s==INVALID_SOCKET)why=L"The connection through the internet failed";return s;
}
}
