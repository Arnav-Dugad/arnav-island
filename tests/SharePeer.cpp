// A PC's side of the sharing protocol, for testing Arnav Island for Android against the real thing: the Windows
// ShareService over loopback (no discovery, no firewall prompt), driven by commands on stdin, reporting events on
// stdout, one per line. It confirms pairing and accepts offers by itself, and answers the phone's remote with a
// made-up status. Not part of the app.
//   share_peer <port> <folder> [cover.jpg name] [--relay]
//   then: peer <id> <port> | send <id> <path> | shelf <path> | handoff <id> <file|-> | ring <id> | host | code <code> | peers | quit
// --relay: also through the public relay (as on other networks); host offers a pairing code (CODE, then the LINK its QR
// code carries), code pairs with one. --relay-lose=N: every Nth data message is sent only the second time (as if dropped).
// The relay's direct path is off unless asked for (so a test never opens a socket on every interface): --direct-loopback
// on the loopback only, --direct on every interface (by hand); --direct-candidate=<address> offers that address too
// (10.0.2.2 for an emulator). `peers` also prints PATH <id> <kind> <rtt ms> <relays> <v6>.
// With a cover (screenshots of the phone app), the remote reports a made-up song that plays on, under that PC name, and
// keeps the volume it is given; each shelf command adds to the Shelf (a "<path>.preview" JPEG beside a file is its preview).
#include "Productivity/ShareService.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
using namespace nexus;
namespace {
std::mutex out;
void say(const std::string& line){std::lock_guard lock(out);std::cout<<line<<std::endl;}
std::string narrow(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
std::wstring widen(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
}
int main(int argc,char** argv){
    // --relay: reach devices through the public brokers too. --relay-only=N: through that broker alone (0 HiveMQ, 1 EMQX,
    // 2 Mosquitto), as a device that can't reach the others would (the interop tests pair such a PC with a phone on all).
    bool relay=false,directLoopback=false,direct=false;int only=-1,lose=0;std::vector<std::string> extra;
    {int kept=0;for(int i=0;i<argc;++i){const std::string a=argv[i];if(a=="--relay")relay=true;else if(a.rfind("--relay-only=",0)==0){relay=true;only=std::atoi(a.c_str()+13);}
        else if(a.rfind("--relay-lose=",0)==0)lose=std::atoi(a.c_str()+13);else if(a=="--direct-loopback")direct=directLoopback=true;else if(a=="--direct")direct=true;
        else if(a.rfind("--direct-candidate=",0)==0)extra.push_back(a.substr(19));else argv[kept++]=argv[i];}argc=kept;}
    if(argc<3){std::cerr<<"share_peer <port> <folder>\n";return 2;}
    const uint16_t port=uint16_t(std::atoi(argv[1]));const std::filesystem::path folder=widen(argv[2]);std::error_code e;std::filesystem::create_directories(folder/L"dl",e);
    std::vector<uint8_t> qaCover;std::wstring name=L"Interop PC";const bool qa=argc>=5;
    if(qa){std::ifstream in(argv[3],std::ios::binary);qaCover.assign(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());name=widen(argv[4]);}
    ShareOptions o;o.tcpPort=port;o.discovery=false;o.loopback=true;o.folder=folder.wstring();o.downloads=(folder/L"dl").wstring();o.handoff=(folder/L"handoff").wstring();o.name=name;o.relay=relay;o.relayLoseEvery=lose;o.direct=direct;o.directLoopback=directLoopback;o.directExtra=extra;
    if(only>=0&&only<=2){const std::pair<std::wstring,uint16_t> all[]={{L"broker.hivemq.com",8884},{L"broker.emqx.io",8084},{L"test.mosquitto.org",8081}};o.relayBrokers={all[only]};}
    const auto began=std::chrono::steady_clock::now();auto volume=std::make_shared<std::atomic<int>>(42);auto muted=std::make_shared<std::atomic<bool>>(false);auto playing=std::make_shared<std::atomic<bool>>(true);
    o.input=[](const std::string&,const std::vector<uint8_t>& f){std::string h;static const char* d="0123456789abcdef";for(uint8_t b:f){h+=d[b>>4];h+=d[b&15];}say("INPUT "+h);};
    o.remote=[=](const std::string& peer,RemoteCommand c,const std::vector<uint8_t>& payload)->std::vector<uint8_t>{
        switch(c){
        case RemoteCommand::Status:{RemoteStatus st;st.available=st.canNext=st.canPrevious=st.canToggle=st.canSeek=true;st.playing=*playing;st.position=61.5;st.duration=200;st.volume=*volume;st.muted=*muted;st.battery=77;st.batteryPresent=true;st.cpu=13;st.clipboard=true;
            st.title=L"Interop Song";st.artist=L"Test Artist";st.app=L"Spotify";st.name=name;st.weather=L"14° Rain";st.cover.assign(1500,0x5a);
            if(qa){const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();st.title=L"Glass Horizons";st.artist=L"Aurora Fields";st.duration=214;st.position=std::fmod(61.5+(*playing?t:0),214.);
                st.weather=L"18° Partly cloudy";st.charging=true;st.battery=82;st.cpu=9;st.cover=qaCover;
                // ARNAV_QA_WEATHER: the sky the phone's glass shows (Rain, Thunderstorm, Snow, Fog...).
                if(const char* sky=std::getenv("ARNAV_QA_WEATHER");sky&&*sky){std::wstring w;for(const char* c=sky;*c;++c)w.push_back(wchar_t(*c));st.weather=L"12° "+w;}}
            say("REMOTE status");return remoteStatusAnswer(st,payload);}
        case RemoteCommand::Media:if(!payload.empty()&&payload[0]==1)*playing=!*playing;say("REMOTE media "+std::to_string(payload.empty()?0:payload[0]));return {remoteOk};
        case RemoteCommand::Volume:if(!payload.empty())*volume=payload[0];say("REMOTE volume "+std::to_string(payload.empty()?0:payload[0]));return {remoteOk};
        case RemoteCommand::Mute:*muted=!*muted;say("REMOTE mute");return {remoteOk};
        case RemoteCommand::ClipboardGet:{std::vector<uint8_t> r{remoteOk};const std::string t="from pc \xe2\x9c\x93";for(int i=0;i<4;++i)r.push_back(uint8_t(t.size()>>(8*i)));r.insert(r.end(),t.begin(),t.end());return r;}
        case RemoteCommand::ClipboardSet:say("CLIP "+std::string(payload.begin(),payload.end()));return {remoteOk};
        case RemoteCommand::Lock:say("REMOTE lock");return {remoteNotAllowed};
        case RemoteCommand::RingPC:say("RINGPC");return {remoteOk};
        case RemoteCommand::Lyrics:{say("REMOTE lyrics");std::vector<ShareLyricLine> lines{{12.5,L"Glass on the water",{{12.5,0},{13.1,6},{13.6,9}}},{16.25,L"Light in the rain",{}}};
            return remoteLyricsAnswer(2,L"Interop Song\tTest Artist",lines);}
        default:return {remoteUnsupported};}};
    ShareService share(nullptr,o);
    if(!share.running()){say("ERROR "+share.error());return 1;}
    say("READY "+share.id()+" "+std::to_string(port));
    std::atomic<bool> done{false};
    std::thread events([&]{
        while(!done){for(auto& ev:share.take()){using K=ShareEvent::Kind;
                switch(ev.kind){
                // ARNAV_QA_CONFIRM_DELAY=<s> (screenshots): the PC says yes that much later.
                case K::PairCode:{say("PAIRCODE "+std::to_string(ev.code));say("PEERID "+ev.peer);const char* wait=std::getenv("ARNAV_QA_CONFIRM_DELAY");
                    if(wait){const int s=std::atoi(wait);std::thread([&share,s]{std::this_thread::sleep_for(std::chrono::seconds(s));share.confirmPair(true);}).detach();}else share.confirmPair(true);break;}
                case K::Paired:say("PAIRED ok");break;case K::PairFailed:say("PAIRED no "+narrow(ev.detail));break;
                case K::Offer:say("OFFER "+std::to_string(ev.count)+" "+std::to_string(ev.size)+(ev.toShelf?" shelf":""));share.answer(ev.transfer,true);break;
                case K::Received:say("RECEIVED "+narrow(ev.detail)+"|"+std::to_string(ev.code));break;
                case K::Sent:say("SENT "+narrow(ev.file));break;
                case K::Failed:say("FAILED "+narrow(ev.file)+"|"+narrow(ev.detail));break;
                case K::Handoff:say("HANDOFF "+narrow(ev.handoff.title)+"|"+std::to_string(ev.handoff.position));share.answerHandoff(ev.transfer,1);break;
                case K::HandoffAnswered:say("HANDOFFANSWER "+std::to_string(ev.code));break;
                case K::ShelfTaken:say("SHELFTAKEN "+narrow(ev.file));break;
                case K::PhoneStatus:say("PHONESTATUS "+std::to_string(ev.battery)+" "+std::to_string(ev.charging?1:0));break;
                case K::PhoneNotice:{std::string acts;for(auto& [title,reply]:ev.actions)acts+="|"+narrow(title)+(reply?"*":"");
                    say("PHONENOTICE "+narrow(ev.app)+"|"+narrow(ev.file)+"|"+narrow(ev.detail)+"|"+std::to_string(ev.icon.size())+"|"+std::to_string(ev.urgent?1:0)+(ev.key.empty()?"":"|"+ev.key+acts));break;}
                case K::PhoneDetails:say("DETAILS "+narrow(ev.detail).substr(0,narrow(ev.detail).find('\n')));break;
                case K::PhoneNoticeGone:say("GONE "+ev.key);break;
                case K::Rang:say("RANG");break;
                case K::PairingCode:say("CODE "+narrow(ev.detail));say("LINK "+share.pairingLink(narrow(ev.detail)));break;
                case K::Peers:for(auto& p:share.peers())if(p.paired)say("PRESENCE "+p.id+" "+std::to_string(p.online)+" "+std::to_string(p.viaInternet)+" "+std::to_string(p.revision)+" "+std::to_string(p.phone));break;
                default:break;}}
            std::this_thread::sleep_for(std::chrono::milliseconds(30));}});
    std::string line;std::vector<ShareShelfEntry> shelf;
    while(std::getline(std::cin,line)){
        std::istringstream in(line);std::string cmd;in>>cmd;
        if(cmd=="quit")break;
        if(cmd=="peer"){std::string id;int p=0;in>>id>>p;share.addPeer(id,L"Phone",("127.0.0.1"),uint16_t(p),2,3);say("OK peer");}
        else if(cmd=="send"){std::string id,path;in>>id;std::getline(in>>std::ws,path);share.send(id,{widen(path)});}
        else if(cmd=="shelf"){std::string path;std::getline(in>>std::ws,path);if(!qa)shelf.clear();ShareShelfEntry e{widen(path),{}};
            std::ifstream preview(path+".preview",std::ios::binary);if(preview)e.preview.assign(std::istreambuf_iterator<char>(preview),std::istreambuf_iterator<char>());shelf.push_back(e);share.offerShelf(shelf,true);say("OK shelf");}
        else if(cmd=="handoff"){std::string id,file;in>>id>>file;ShareHandoff h;h.title=L"PC Song";h.artist=L"PC Artist";h.position=12.5;h.duration=180;h.cover.assign(700,0x33);if(qa){h.title=L"Glass Horizons";h.artist=L"Aurora Fields";h.app=L"Spotify";h.position=8;h.duration=30;h.cover=qaCover;}share.handoff(id,h,file=="-"?L"":widen(file));}
        else if(cmd=="ring"){std::string id;in>>id;share.ring(id);}
        else if(cmd=="host")share.hostPairing();
        else if(cmd=="direct"){std::string what;in>>what;if(what=="off"){share.stopDirect();say("OK direct off");}}
        else if(cmd=="action"){std::string id,key,reply;int index=0;in>>id>>key>>index;std::getline(in>>std::ws,reply);share.noticeAction(id,key,index,widen(reply));}
        else if(cmd=="clip"){std::string id,text;in>>id;std::getline(in>>std::ws,text);share.pushClipboard(id,widen(text),false);}
        else if(cmd=="photo"){std::string id;in>>id;share.askPhoto(id);}
        else if(cmd=="code"){std::string code;std::getline(in>>std::ws,code);share.pairWithCode(code);}
        else if(cmd=="peers"){for(auto& p:share.peers()){say("PEER "+p.id+" "+std::to_string(p.paired)+" "+std::to_string(p.online)+" "+std::to_string(p.viaInternet)+" "+narrow(p.name));
                if(p.paired)say("PATH "+p.id+" "+std::to_string(p.path)+" "+std::to_string(int(p.rtt))+" "+std::to_string(p.relays)+" "+std::to_string(p.v6?1:0));}
            say("INTERNET "+std::to_string(share.internet())+" "+narrow(share.relayBroker()));}
    }
    done=true;events.join();return 0;
}
