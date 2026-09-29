#include <winsock2.h>
#include <ws2tcpip.h>
#include "ShareRelay.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
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
constexpr auto helloEvery=std::chrono::seconds(60),helloGone=std::chrono::seconds(150);constexpr auto hostFor=std::chrono::minutes(10);
const char topicPrefix[]="arnavisland/r1/";
const char codeAlphabet[]="23456789ABCDEFGHJKMNPQRSTUVWXYZ";
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
    // Where messages to this device arrive (its inbox topic): whose they are, how they're sealed, where replies go.
    struct Route{std::string peer;std::shared_ptr<Seal> seal;std::string outbox;bool code=false,host=false;Clock::time_point heard{},helloed{};RelayPresence presence;};
    struct Tunnel{uint64_t conn=0;SOCKET outer=INVALID_SOCKET;std::string outbox;std::shared_ptr<Seal> seal;
        std::mutex m;std::condition_variable cv;uint32_t sent=0,acked=0,expected=0,acknowledged=0;std::deque<Bytes> inbound;bool inEnd=false,dead=false,closeSent=false;int finished=0;};
    mutable std::mutex m;std::map<std::string,Route> routes;std::map<uint64_t,std::shared_ptr<Tunnel>> tunnels;
    std::string code;Clock::time_point codeUntil{};
    // The broker connection.
    std::mutex wsMutex,sendMutex;HINTERNET session=nullptr,link=nullptr,ws=nullptr;std::atomic<bool> up{false},stopping{false};std::wstring brokerName;
    std::atomic<int64_t> lastIn{0},lastPing{0};uint16_t nextPacket=1;std::map<uint16_t,bool> subacks;std::condition_variable subacked;
    std::thread runner,ticker;std::mutex sleepMutex;std::condition_variable sleeper;
    static int64_t now(){return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();}
    std::vector<std::pair<std::wstring,uint16_t>> brokers()const{return o.brokers.empty()?std::vector<std::pair<std::wstring,uint16_t>>{{L"broker.hivemq.com",8884},{L"broker.emqx.io",8084},{L"test.mosquitto.org",8081}}:o.brokers;}

    // ---- the broker ----
    bool write(const Bytes& b){HINTERNET h;{std::lock_guard lock(wsMutex);h=ws;}if(!h)return false;std::lock_guard lock(sendMutex);
        return WinHttpWebSocketSend(h,WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,const_cast<uint8_t*>(b.data()),DWORD(b.size()))==NO_ERROR;}
    void abortSocket(){std::lock_guard lock(wsMutex);if(ws){WinHttpCloseHandle(ws);ws=nullptr;}}
    void closeHandles(){abortSocket();if(link){WinHttpCloseHandle(link);link=nullptr;}if(session){WinHttpCloseHandle(session);session=nullptr;}}
    bool open(const std::wstring& host,uint16_t port){
        session=WinHttpOpen(L"ArnavIsland/0.20",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session)return false;
        WinHttpSetTimeouts(session,10000,10000,10000,10000);
        link=WinHttpConnect(session,host.c_str(),port,0);if(!link)return false;
        HINTERNET request=WinHttpOpenRequest(link,L"GET",L"/mqtt",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!request)return false;
        bool made=WinHttpSetOption(request,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0)&&WinHttpAddRequestHeaders(request,L"Sec-WebSocket-Protocol: mqtt",DWORD(-1),WINHTTP_ADDREQ_FLAG_ADD)&&
            WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,nullptr,0,0,0)&&WinHttpReceiveResponse(request,nullptr);
        DWORD status=0,size=sizeof(status);if(made)made=WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)&&status==101;
        HINTERNET socket=made?WinHttpWebSocketCompleteUpgrade(request,0):nullptr;WinHttpCloseHandle(request);if(!socket)return false;
        {std::lock_guard lock(wsMutex);ws=socket;}return true;
    }
    // The broker's packets until the connection ends.
    void receive(){
        Bytes stream,buffer(64*1024),body;uint8_t head=0;
        for(;;){HINTERNET h;{std::lock_guard lock(wsMutex);h=ws;}if(!h)return;
            DWORD got=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};if(WinHttpWebSocketReceive(h,buffer.data(),DWORD(buffer.size()),&got,&type)!=NO_ERROR||type==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)return;
            lastIn=now();stream.insert(stream.end(),buffer.begin(),buffer.begin()+got);
            for(;;){const int r=mqttNext(stream,head,body);if(r<0)return;if(r==0)break;handle(head,body);}}
    }
    void handle(uint8_t head,const Bytes& body){
        switch(head>>4){
        case 2:if(body.size()>=2&&body[1]==0&&!up.exchange(true)){onConnected();}break;
        case 9:if(body.size()>=2){std::lock_guard lock(m);auto it=subacks.find(uint16_t(body[0]<<8|body[1]));if(it!=subacks.end())it->second=true;subacked.notify_all();}break;
        case 3:{if(body.size()<2)return;const size_t n=size_t(body[0])<<8|body[1];if(body.size()<2+n)return;size_t at=2+n;if((head>>1)&3)at+=2;if(at>body.size())return;
            onPublish(std::string(body.begin()+2,body.begin()+long(2+n)),body.data()+at,body.size()-at);break;}
        default:break;}
    }
    void run(){
        size_t which=0;int fails=0;
        while(!stopping){
            const auto list=brokers();const auto& [host,port]=list[which%list.size()];
            bool connected=false;
            if(open(host,port)){lastIn=now();
                Bytes client=randomBytes(10);if(write(mqttConnect("ai"+hex(client)))){{std::lock_guard lock(m);brokerName=host;}receive();connected=up.load();}}
            const bool was=up.exchange(false);closeHandles();dropAll();if(was&&o.changed)o.changed();
            if(stopping)break;
            if(connected)fails=0;else{++fails;++which;}
            const int wait=std::min(30,connected?1:(1<<std::min(fails,5)));
            std::unique_lock lock(sleepMutex);sleeper.wait_for(lock,std::chrono::seconds(wait),[&]{return stopping.load();});
        }
    }
    // Once a minute each paired device hears from this one; a device not heard from in 150 s is gone; the broker is pinged,
    // and a silent broker (or one that never answered) is left.
    void tick(){
        while(!stopping){
            {std::unique_lock lock(sleepMutex);sleeper.wait_for(lock,std::chrono::seconds(5),[&]{return stopping.load();});}if(stopping)break;
            const int64_t t=now();HINTERNET h;{std::lock_guard lock(wsMutex);h=ws;}
            if(h&&t-lastIn>(up?100000:15000)){abortSocket();continue;}
            if(!up)continue;
            if(t-lastPing>=30000){lastPing=t;write(Bytes{0xC0,0});}
            std::vector<std::string> hello,expired;bool changed=false;
            {std::lock_guard lock(m);const auto clock=Clock::now();
                for(auto& [inbox,r]:routes){if(r.code)continue;
                    if(clock-r.helloed>=helloEvery)hello.push_back(inbox);
                    if(r.presence.here&&clock-r.heard>helloGone){r.presence.here=false;changed=true;}}
                if(!code.empty()&&clock>codeUntil)expired=stopHostingLocked();}
            if(!expired.empty())unsubscribe(expired);
            for(auto& inbox:hello)sayHello(inbox,false,false);
            if(changed&&o.changed)o.changed();
        }
    }
    void onConnected(){
        std::vector<std::string> inboxes;{std::lock_guard lock(m);for(auto& [inbox,r]:routes)inboxes.push_back(inbox);}
        if(!inboxes.empty())subscribe(inboxes,false);
        std::vector<std::string> pairs;{std::lock_guard lock(m);for(auto& [inbox,r]:routes)if(!r.code)pairs.push_back(inbox);}
        for(auto& inbox:pairs)sayHello(inbox,true,false);
        if(o.changed)o.changed();
    }
    bool subscribe(const std::vector<std::string>& topics,bool wait){
        uint16_t id;{std::lock_guard lock(m);id=nextPacket++;if(!nextPacket)nextPacket=1;if(wait)subacks[id]=false;}
        if(!write(mqttSubscribe(id,topics))){std::lock_guard lock(m);subacks.erase(id);return false;}if(!wait)return true;
        std::unique_lock lock(m);const bool done=subacked.wait_for(lock,std::chrono::seconds(8),[&]{return subacks[id];});subacks.erase(id);return done;
    }
    // ---- messages ----
    Bytes envelope(uint8_t kind)const{Bytes b{envelopeVersion,kind};b.insert(b.end(),o.id.begin(),o.id.end());return b;}
    bool publish(const std::string& topic,const Seal& seal,const Bytes& plain){const Bytes sealed=seal.seal(plain,topic);return !sealed.empty()&&write(mqttPublish(topic,sealed));}
    void sayHello(const std::string& inbox,bool reply,bool leaving){
        std::string outbox;std::shared_ptr<Seal> seal;{std::lock_guard lock(m);auto it=routes.find(inbox);if(it==routes.end())return;outbox=it->second.outbox;seal=it->second.seal;it->second.helloed=Clock::now();}
        Bytes b=envelope(kindHello);b.push_back(uint8_t((reply?helloReply:0)|(o.phone?helloPhone:0)|(leaving?helloLeaving:0)));b.push_back(uint8_t(o.revision));
        std::string name=utf8(o.name);if(name.size()>120)name.resize(120);b.push_back(uint8_t(name.size()));b.insert(b.end(),name.begin(),name.end());
        publish(outbox,*seal,b);
    }
    void onPublish(const std::string& topic,const uint8_t* payload,size_t n){
        Route r;{std::lock_guard lock(m);auto it=routes.find(topic);if(it==routes.end())return;r=it->second;}
        Bytes plain;if(!r.seal->open(payload,n,topic,plain)||plain.size()<18||plain[0]!=envelopeVersion)return;
        const Bytes sender(plain.begin()+2,plain.begin()+18);if(sender==o.id)return;if(!r.code&&hex(sender)!=r.peer)return;
        const uint8_t* p=plain.data()+18;const size_t left=plain.size()-18;
        switch(plain[1]){
        case kindHello:{if(r.code||left<3)return;const uint8_t flags=p[0];const size_t nameSize=std::min<size_t>(p[2],left-3);
            RelayPresence presence;presence.here=!(flags&helloLeaving);presence.phone=flags&helloPhone;presence.revision=p[1];presence.name=wide(std::string(p+3,p+3+nameSize));
            bool changed=false;{std::lock_guard lock(m);auto it=routes.find(topic);if(it==routes.end())return;auto& old=it->second.presence;
                changed=old.here!=presence.here||old.phone!=presence.phone||old.revision!=presence.revision||old.name!=presence.name;old=presence;it->second.heard=Clock::now();}
            if(flags&helloReply)sayHello(topic,false,false);
            if(changed&&o.changed)o.changed();break;}
        case kindOpen:{if(left<8)return;const uint64_t conn=get64(p);{std::lock_guard lock(m);if(tunnels.count(conn))return;}
            SOCKET inner,outer;if(!loopbackPair(inner,outer))return;
            auto t=start(conn,outer,r.outbox,r.seal);if(!t){closesocket(inner);return;}
            if(o.incoming){auto self=shared_from_this();const std::string peer=r.code?std::string():r.peer;std::thread([self,inner,peer]{self->o.incoming(inner,peer);}).detach();}
            else closesocket(inner);
            break;}
        case kindData:{if(left<13)return;const uint64_t conn=get64(p);const uint32_t seq=get32(p+8);const uint8_t flags=p[12];
            std::shared_ptr<Tunnel> t;{std::lock_guard lock(m);auto it=tunnels.find(conn);if(it!=tunnels.end())t=it->second;}if(!t)return;
            bool ack=false,lost=false;uint32_t next=0;
            {std::lock_guard lock(t->m);if(seq<t->expected)return;if(seq>t->expected)lost=true;
                else{t->inbound.emplace_back(p+13,p+left);++t->expected;if((flags&1)||t->expected-t->acknowledged>=ackEvery){ack=true;t->acknowledged=t->expected;}next=t->expected;}}
            t->cv.notify_all();
            if(lost){kill(t);break;}
            if(ack){Bytes b=envelope(kindAck);put64(b,conn);put32(b,next);publish(t->outbox,*t->seal,b);}
            break;}
        case kindAck:{if(left<12)return;const uint64_t conn=get64(p);const uint32_t next=get32(p+8);std::shared_ptr<Tunnel> t;{std::lock_guard lock(m);auto it=tunnels.find(conn);if(it!=tunnels.end())t=it->second;}
            if(!t)return;{std::lock_guard lock(t->m);if(next>t->acked&&next<=t->sent)t->acked=next;}t->cv.notify_all();break;}
        case kindClose:{if(left<8)return;const uint64_t conn=get64(p);std::shared_ptr<Tunnel> t;{std::lock_guard lock(m);auto it=tunnels.find(conn);if(it!=tunnels.end())t=it->second;}
            if(!t)return;{std::lock_guard lock(t->m);t->inEnd=true;t->closeSent=true;}t->cv.notify_all();break;}
        default:break;}
    }
    // ---- tunnels ----
    std::shared_ptr<Tunnel> start(uint64_t conn,SOCKET outer,const std::string& outbox,const std::shared_ptr<Seal>& seal){
        auto t=std::make_shared<Tunnel>();t->conn=conn;t->outer=outer;t->outbox=outbox;t->seal=seal;
        {std::lock_guard lock(m);if(!up){closesocket(outer);return nullptr;}tunnels[conn]=t;}
        auto self=shared_from_this();
        std::thread([self,t]{self->pumpOut(t);}).detach();std::thread([self,t]{self->pumpIn(t);}).detach();
        return t;
    }
    // Bytes the protocol wrote go out in numbered messages, never more than the window ahead of the acknowledgements.
    void pumpOut(std::shared_ptr<Tunnel> t){
        Bytes buffer(chunk);
        for(;;){const int n=::recv(t->outer,reinterpret_cast<char*>(buffer.data()),int(chunk),0);if(n<=0)break;
            uint32_t seq=0;bool ackNow=false;
            {std::unique_lock lock(t->m);if(!t->cv.wait_for(lock,std::chrono::seconds(30),[&]{return t->dead||t->sent-t->acked<window;})||t->dead)break;seq=t->sent++;ackNow=t->sent-t->acked>=ackEvery;}
            Bytes b=envelope(kindData);put64(b,t->conn);put32(b,seq);b.push_back(ackNow?1:0);b.insert(b.end(),buffer.begin(),buffer.begin()+n);
            if(!publish(t->outbox,*t->seal,b))break;}
        kill(t);
        finish(t);
    }
    // Messages from the other side, in order, into the protocol's socket; the end of them is its end of file.
    void pumpIn(std::shared_ptr<Tunnel> t){
        for(;;){Bytes b;{std::unique_lock lock(t->m);t->cv.wait(lock,[&]{return t->dead||!t->inbound.empty()||t->inEnd;});if(t->dead)break;if(t->inbound.empty())break;b=std::move(t->inbound.front());t->inbound.pop_front();}
            if(!sendAllTo(t->outer,b)){kill(t);break;}}
        shutdown(t->outer,SD_SEND);
        finish(t);
    }
    void sendClose(const std::shared_ptr<Tunnel>& t){{std::lock_guard lock(t->m);if(t->closeSent)return;t->closeSent=true;}Bytes b=envelope(kindClose);put64(b,t->conn);publish(t->outbox,*t->seal,b);}
    void kill(const std::shared_ptr<Tunnel>& t){{std::lock_guard lock(t->m);if(t->dead)return;t->dead=true;}t->cv.notify_all();shutdown(t->outer,SD_BOTH);sendClose(t);}
    void finish(const std::shared_ptr<Tunnel>& t){bool last;{std::lock_guard lock(t->m);last=++t->finished==2;}if(!last)return;closesocket(t->outer);std::lock_guard lock(m);tunnels.erase(t->conn);}
    void dropAll(){std::vector<std::shared_ptr<Tunnel>> all;{std::lock_guard lock(m);for(auto& [c,t]:tunnels)all.push_back(t);for(auto& [inbox,r]:routes)r.presence.here=false;}
        for(auto& t:all){{std::lock_guard lock(t->m);t->dead=true;t->closeSent=true;}t->cv.notify_all();shutdown(t->outer,SD_BOTH);}}
    SOCKET openTunnel(const std::string& outbox,const std::shared_ptr<Seal>& seal){
        SOCKET inner,outer;if(!loopbackPair(inner,outer))return INVALID_SOCKET;uint64_t conn=0;const Bytes r=randomBytes(8);for(int i=0;i<8&&r.size()==8;++i)conn|=uint64_t(r[size_t(i)])<<(8*i);
        auto t=start(conn,outer,outbox,seal);if(!t){closesocket(inner);return INVALID_SOCKET;}
        Bytes b=envelope(kindOpen);put64(b,conn);if(!publish(outbox,*seal,b)){kill(t);closesocket(inner);return INVALID_SOCKET;}
        return inner;
    }
    // Ends hosting a code; returns the topics to leave.
    std::vector<std::string> stopHostingLocked(){std::vector<std::string> left;code.clear();for(auto it=routes.begin();it!=routes.end();)if(it->second.host){left.push_back(it->first);it=routes.erase(it);}else ++it;return left;}
    void unsubscribe(const std::vector<std::string>& topics){uint16_t id;{std::lock_guard lock(m);id=nextPacket++;if(!nextPacket)nextPacket=1;}if(up)write(mqttUnsubscribe(id,topics));}
};

Relay::Relay(RelayOptions options):core_(std::make_shared<Core>()){
    core_->o=std::move(options);auto c=core_;
    c->runner=std::thread([c]{c->run();});c->ticker=std::thread([c]{c->tick();});
}
Relay::~Relay(){
    auto& c=*core_;
    // A goodbye, so the other devices know at once.
    if(c.up){std::vector<std::string> pairs;{std::lock_guard lock(c.m);for(auto& [inbox,r]:c.routes)if(!r.code)pairs.push_back(inbox);}for(auto& inbox:pairs)c.sayHello(inbox,false,true);c.write(Bytes{0xE0,0});}
    c.stopping=true;c.sleeper.notify_all();c.abortSocket();
    if(c.runner.joinable())c.runner.join();if(c.ticker.joinable())c.ticker.join();
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
    if(c.up&&!added.empty()){c.subscribe(added,false);for(auto& inbox:added)c.sayHello(inbox,true,false);}
}
RelayPresence Relay::presence(const std::string& peer)const{std::lock_guard lock(core_->m);for(auto& [inbox,r]:core_->routes)if(!r.code&&r.peer==peer)return r.presence;return {};}
bool Relay::connected()const{return core_->up;}
std::wstring Relay::broker()const{std::lock_guard lock(core_->m);return core_->up?core_->brokerName:std::wstring();}
SOCKET Relay::open(const std::string& peer,std::wstring& why){
    auto& c=*core_;std::string outbox;std::shared_ptr<Seal> seal;bool here=false;std::wstring name;
    {std::lock_guard lock(c.m);for(auto& [inbox,r]:c.routes)if(!r.code&&r.peer==peer){outbox=r.outbox;seal=r.seal;here=r.presence.here;name=r.presence.name;break;}}
    if(!seal){why=L"Pair with it first";return INVALID_SOCKET;}
    if(!c.up){why=L"This PC isn't connected to the internet";return INVALID_SOCKET;}
    if(!here){why=(name.empty()?std::wstring(L"It"):name)+L" isn't online";return INVALID_SOCKET;}
    const SOCKET s=c.openTunnel(outbox,seal);if(s==INVALID_SOCKET)why=L"The connection through the internet failed";return s;
}
std::string Relay::host(){
    auto& c=*core_;if(!c.up)return {};const Bytes r=randomBytes(8);if(r.size()!=8)return {};
    std::string code;for(uint8_t b:r)code+=codeAlphabet[b%31];
    const Bytes secret=sha256(cat({text("arnav-pair-code-v1"),Bytes(code.begin(),code.end())}));
    const std::string inbox=topicOf(sha256(cat({text("host"),secret}))),outbox=topicOf(sha256(cat({text("guest"),secret})));
    std::vector<std::string> left;{std::lock_guard lock(c.m);left=c.stopHostingLocked();Core::Route route;route.code=route.host=true;route.outbox=outbox;route.seal=std::make_shared<Seal>(sha256(cat({text("arnav-pair-code-key"),secret})));
        c.routes[inbox]=route;c.code=code;c.codeUntil=Clock::now()+hostFor;}
    if(!left.empty())c.unsubscribe(left);
    if(!c.subscribe({inbox},true)){stopHosting();return {};}
    return code.substr(0,4)+"-"+code.substr(4);
}
void Relay::stopHosting(){std::vector<std::string> left;{std::lock_guard lock(core_->m);left=core_->stopHostingLocked();}if(!left.empty())core_->unsubscribe(left);}
std::string Relay::hosting()const{std::lock_guard lock(core_->m);return core_->code.empty()?std::string():core_->code.substr(0,4)+"-"+core_->code.substr(4);}
SOCKET Relay::openCode(const std::string& typed,std::wstring& why){
    auto& c=*core_;const std::string code=relayCode(typed);if(code.empty()){why=L"That isn't a pairing code";return INVALID_SOCKET;}
    if(!c.up){why=L"This PC isn't connected to the internet";return INVALID_SOCKET;}
    const Bytes secret=sha256(cat({text("arnav-pair-code-v1"),Bytes(code.begin(),code.end())}));
    const std::string inbox=topicOf(sha256(cat({text("guest"),secret}))),outbox=topicOf(sha256(cat({text("host"),secret})));
    auto seal=std::make_shared<Seal>(sha256(cat({text("arnav-pair-code-key"),secret})));
    {std::lock_guard lock(c.m);Core::Route route;route.code=true;route.outbox=outbox;route.seal=seal;c.routes[inbox]=route;}
    if(!c.subscribe({inbox},true)){why=L"The connection through the internet failed";return INVALID_SOCKET;}
    const SOCKET s=c.openTunnel(outbox,seal);if(s==INVALID_SOCKET)why=L"The connection through the internet failed";return s;
}
}
