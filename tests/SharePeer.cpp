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
#include "Mirror/MirrorHost.h"
#include "Mirror/MirrorWindow.h"
#include <wincodec.h>
#include <algorithm>
#include <cstring>
#include <exception>
#include <ctime>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
using namespace nexus;
namespace {
std::mutex out;
void say(const std::string& line){std::lock_guard lock(out);std::cout<<line<<std::endl;}
// Revision 7: the phone screen showing now (its window's input, as the island's window sends it), for mtap/mkey/mtext.
std::mutex shownLock;nexus::mirror::FrameSink* shown=nullptr;
std::string narrow(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
std::wstring widen(const std::string& s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
// Revision 9/10: a made-up picture, made here with WIC: a rounded square in a colour, lighter towards the top, with a
// light disc (an app's icon as a PNG with its corners clear), or the whole frame (a JPEG: a slide, a copied image).
std::vector<uint8_t> picture(UINT w,UINT h,uint32_t rgb,bool png,int mark=0){
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::vector<uint8_t> px(size_t(w)*h*4);const double r=png?w*.22:0;
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){uint8_t* p=&px[(size_t(y)*w+x)*4];const double lift=.35*(1-double(y)/h);
        const double dx=std::max(0.,std::max(r-x-.5,x+.5-(w-r))),dy=std::max(0.,std::max(r-y-.5,y+.5-(h-r)));const bool inside=!png||dx*dx+dy*dy<=r*r;
        const double cx=x+.5-w*(.5+.18*(mark%3-1)),cy=y+.5-h*.5;const bool disc=cx*cx+cy*cy<=(w*.18)*(w*.18);
        for(int c=0;c<3;++c){const double v=((rgb>>(8*c))&255)/255.;p[c]=uint8_t(std::clamp(disc?255*(.85+.15*v):255*(v+(1-v)*lift),0.,255.));}p[3]=inside?255:0;}
    ComPtr<IWICImagingFactory> f;ComPtr<IWICBitmap> bitmap;ComPtr<IWICFormatConverter> converter;ComPtr<IStream> out;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;
    WICPixelFormatGUID format=png?GUID_WICPixelFormat32bppBGRA:GUID_WICPixelFormat24bppBGR;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f)))||FAILED(f->CreateBitmapFromMemory(w,h,GUID_WICPixelFormat32bppBGRA,w*4,UINT(px.size()),px.data(),&bitmap))||
       FAILED(f->CreateFormatConverter(&converter))||FAILED(converter->Initialize(bitmap.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&out))||
       FAILED(f->CreateEncoder(png?GUID_ContainerFormatPng:GUID_ContainerFormatJpeg,nullptr,&encoder))||FAILED(encoder->Initialize(out.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&frame,&options))||
       FAILED(frame->Initialize(options.Get()))||FAILED(frame->SetSize(w,h))||FAILED(frame->SetPixelFormat(&format))||FAILED(frame->WriteSource(converter.Get(),nullptr))||FAILED(frame->Commit())||FAILED(encoder->Commit()))return {};
    STATSTG st{};if(FAILED(out->Stat(&st,STATFLAG_NONAME)))return {};std::vector<uint8_t> bytes(size_t(st.cbSize.QuadPart));LARGE_INTEGER zero{};ULONG got=0;
    if(FAILED(out->Seek(zero,STREAM_SEEK_SET,nullptr))||FAILED(out->Read(bytes.data(),ULONG(bytes.size()),&got))||got!=bytes.size())return {};return bytes;}
}
// A crash says what and where (tests), so a failure on another thread is never silent.
LONG WINAPI crashed(EXCEPTION_POINTERS* e){
    char line[160];HMODULE m=nullptr;GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(e->ExceptionRecord->ExceptionAddress),&m);
    char name[MAX_PATH]{};if(m)GetModuleFileNameA(m,name,MAX_PATH);
    std::snprintf(line,sizeof(line),"CRASH %08lx at +0x%llx in %s",e->ExceptionRecord->ExceptionCode,static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress)-reinterpret_cast<uintptr_t>(m)),name);
    say(line);std::fflush(stdout);return EXCEPTION_CONTINUE_SEARCH;
}
int main(int argc,char** argv){
    SetUnhandledExceptionFilter(crashed);std::set_terminate([]{say("CRASH terminate");std::fflush(stdout);std::abort();});
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
    // Revision 7: screens. This PC's screen is a made-up moving picture (never the real screen); a phone's screen is
    // decoded and its frames counted.
    o.mirror=[](const std::string&,const std::wstring& phone,const std::vector<uint8_t>& request,mirror::MirrorIo& io){
        mirror::MirrorRequest r;if(!mirror::readRequest(request,r)){io.send(mirror::refusal(2));say("MIRROR refused");return;}
        std::atomic<bool> stop{false};
        if(r.kind==1){auto source=mirror::patternSource(1280,720);
            const auto rep=mirror::sendScreen(io,r,*source,[](const std::vector<uint8_t>& f){std::string h;static const char* d="0123456789abcdef";for(uint8_t b:f){h+=d[b>>4];h+=d[b&15];}say("MIRROR input "+h);},[](RECT){},stop);
            say("MIRROR sent "+std::to_string(rep.frames)+" "+std::to_string(rep.keys)+" "+std::to_string(rep.width)+"x"+std::to_string(rep.height)+" "+(rep.hardware?"gpu":"software"));say(std::string("MIRROR ended ")+rep.ended);}
        // Revision 9: a phone's microphone, counted (never played): its rate and channels, frames and samples heard.
        else if(r.kind==4){struct Counting:mirror::AudioSink{size_t heard=0;
                bool begin(int rate,int channels,const std::wstring& n,std::wstring& device)override{say("MIC hearing "+std::to_string(rate)+" "+std::to_string(channels)+" "+narrow(n));device=L"Test cable";return true;}
                void samples(const int16_t*,size_t frames,uint64_t)override{heard+=frames;}void end()override{}} sink;
            const auto rep=mirror::receiveAudio(io,r,sink,stop);say("MIC heard "+std::to_string(rep.frames)+" "+std::to_string(sink.heard));say(std::string("MIC ended ")+rep.ended);}
        else{struct Counting:mirror::FrameSink{ComPtr<ID3D11Device> d;int frames=0;
                ID3D11Device* device()override{if(!d&&SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_VIDEO_SUPPORT|D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d.GetAddressOf(),nullptr,nullptr))){
                    // The decoder uses it from its own threads too (as the island's window does).
                    ComPtr<ID3D10Multithread> threads;if(SUCCEEDED(d.As(&threads)))threads->SetMultithreadProtected(TRUE);}return d.Get();}
                bool begin(int w,int h,const std::wstring& n)override{say("MIRROR showing "+std::to_string(w)+"x"+std::to_string(h)+" "+narrow(n));std::lock_guard held(shownLock);shown=this;return true;}
                void frame(ID3D11Texture2D*,UINT,int wd,int ht)override{if(++frames==1||wd!=lastW){say("MIRROR frame "+std::to_string(wd)+"x"+std::to_string(ht));lastW=wd;}}
                void end()override{std::lock_guard held(shownLock);shown=nullptr;}int lastW=0;} sink;
            const auto rep=mirror::receiveScreen(io,r,sink,stop);say("MIRROR shown "+std::to_string(rep.frames)+" "+std::to_string(rep.keys));say(std::string("MIRROR ended ")+rep.ended);}
    };
    // Revision 5: a made-up PC to control (its settings and controls remember what they're set to).
    struct Made{std::mutex m;double focusAt=0;std::map<std::string,int> settings{{"hoverOpen",1},{"autoHide",0},{"hoverDelay",180},{"uiMode",1},{"theme",0},{"accent",0},{"sounds",1},{"weather",1},{"sharing",1},{"phoneControl",1},{"phoneLive",1}};
        PcControls controls;std::wstring output=L"speakers";
        // Revision 9.
        bool shuffle=false;int repeat=0;std::vector<RemoteTrack> upNext{{L"Northern Lines",L"Aurora Fields",201},{L"Paper Boats",L"Lumen Drive",187.5},{L"Slow Tide",L"Harbor Lights",0}};
        std::vector<RemoteMixerEntry> mixer{{4242,L"Spotify",72,0,false,false,true,{}},{5150,L"Microsoft Edge",100,0,false,false,false,{}},{0,L"System sounds",64,0,false,true,false,{}}};
        // Revision 10: a clipboard history (a note, a link, an image, two files, a password-like copy kept hidden) and a deck.
        std::vector<RemoteClip> clips;int slide=3;
        // Revision 11: startup apps that remember their switches, a storage measure and an update check that take a few
        // seconds (from when asked; -1 never), when this PC last checked for updates.
        std::vector<RemoteStartupApp> startup{{L"0:OneDrive",L"Microsoft OneDrive",L"\"C:\\Program Files\\Microsoft OneDrive\\OneDrive.exe\" /background",0,true,true,{}},
            {L"0:Spotify",L"Spotify",L"\"C:\\Users\\Arnav\\AppData\\Roaming\\Spotify\\Spotify.exe\" /minimized",0,false,true,{}},
            {L"0:ArnavIsland",L"Arnav Island",L"\"C:\\Users\\Arnav\\Desktop\\Arnav Island\\app\\ArnavIsland.exe\" --startup",0,true,true,{}},
            {L"2:Steam",L"Steam",L"\"C:\\Program Files (x86)\\Steam\\steam.exe\" -silent",2,false,false,{}},
            {L"1:SecurityHealth",L"Windows Security notification icon",L"%windir%\\system32\\SecurityHealthSystray.exe",1,true,false,{}},
            {L"3:Ollama.lnk",L"Ollama",L"\"C:\\Users\\Arnav\\AppData\\Local\\Programs\\Ollama\\ollama app.exe\"",3,true,true,{}}};
        double measuring=-1,checking=-1;uint64_t checked=0;};
    auto made=std::make_shared<Made>();
    {const uint64_t now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        made->clips={{5,0,L"Ship 0.27 on Friday: resumable transfers, live activities",now-60000,true,{}},{4,0,L"https://github.com/arnavdugad/arnav-island/releases",now-90000,false,{}},
            {3,1,L"Image  ·  640 × 480",now-300000,false,{}},{2,2,L"Quarterly review.pptx\nBudget 2026.xlsx",now-900000,false,{}},{1,0,L"•••••• · hidden",now-1200000,false,{}}};}
    made->controls.wifi=1;made->controls.bluetooth=1;made->controls.dark=1;made->controls.brightness=64;made->controls.volume=42;made->controls.micAvailable=true;made->controls.focusDuration=1500;made->controls.focusShown=1500;
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
        // Revision 8: a page to open here, where the phone had scrolled to.
        case RemoteCommand::Page:{if(payload.size()<8)return {remoteFailed};float f=0;std::memcpy(&f,payload.data(),4);uint32_t n=0;std::memcpy(&n,payload.data()+4,4);if(8+size_t(n)>payload.size())return {remoteFailed};
            char pct[16];std::snprintf(pct,16,"%.3f",double(f));say(std::string("PAGE ")+pct+" "+std::string(payload.begin()+8,payload.begin()+8+n));return {remoteOk};}
        case RemoteCommand::Stats:{const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();PcStats st;
            auto wave=[&](double base,double spread,double speed,double phase){return std::clamp(base+spread*std::sin(t*speed+phase)+spread*.4*std::sin(t*speed*2.3+phase*1.7),0.,100.);};
            st.cpu=wave(23,12,.9,0);st.gpu=wave(41,18,.6,1);st.ramTotalGiB=15.7;st.ramUsedGiB=11.3+.3*std::sin(t*.2);st.ramPercent=st.ramUsedGiB/st.ramTotalGiB*100;st.diskUsedPercent=63.8;st.diskFreeGiB=345.2;st.diskTotalGiB=953.9;
            st.download=2.4e6*(1.2+std::sin(t*.7));st.upload=3.1e5*(1.1+std::sin(t*.5+2));st.uptime=uint64_t(3*86400+7*3600+t);st.logical=16;st.battery=82;st.charging=true;
            for(int i=0;i<40;++i){const double at=t-(39-i);st.cpuHistory.push_back(float(std::clamp(23+12*std::sin(at*.9)+4.8*std::sin(at*2.07),0.,100.)));st.gpuHistory.push_back(float(std::clamp(41+18*std::sin(at*.6+1)+7.2*std::sin(at*1.38+1.7),0.,100.)));
                st.downloadHistory.push_back(float(2.4e6*(1.2+std::sin(at*.7))));}
            st.name=name;st.model=L"ASUS ROG Zephyrus G14";st.os=L"Windows 11 Home 24H2";st.cpuName=L"AMD Ryzen 9 7940HS w/ Radeon 780M Graphics";st.gpuName=L"NVIDIA GeForce RTX 4060 Laptop GPU";
            // Revision 6: sixteen made-up cores, two of them busy.
            for(int i=0;i<16;++i)st.cores.push_back(float(i==3?wave(86,10,1.3,0):i==11?wave(71,14,1.1,2):wave(14,9,.8+i*.05,i)));
            return remoteStatsAnswer(st);}
        // Revision 6: a made-up battery, charging, with a day of history.
        case RemoteCommand::Battery:{PcBattery b;b.present=true;b.online=true;b.charging=true;b.percent=82;b.minutesToFull=34;b.designMwh=76000;b.fullMwh=69920;b.remainingMwh=57334;b.rateMw=21500;b.voltageMv=16420;b.cycles=187;
            b.temperatureDeciK=3071;b.health=.92;b.healthBefore=.921;b.chemistry=L"LION";b.manufacturer=L"ASUSTeK";b.name=L"A32N2105";
            const int64_t now=int64_t(std::time(nullptr));for(int i=0;i<288;++i){const int64_t at=now-int64_t(287-i)*300;const int p=i<120?95-i/2:i<200?35+(i-120)/2:std::min(100,75+(i-200)/3);b.day.push_back({at,p,i>=120});}
            say("REMOTE battery");return remoteBatteryAnswer(b);}
        case RemoteCommand::Settings:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);auto& v=made->settings;
            if(op==1){const std::string key=in.bytes(64);const int value=in.i32();auto it=v.find(key);if(!in.ok()||it==v.end())return {remoteUnsupported};
                it->second=key=="hoverDelay"?std::clamp(value,100,700):key=="uiMode"||key=="theme"?std::clamp(value,0,2):key=="accent"?std::clamp(value,0,4):value!=0;say("SETTING "+key+" "+std::to_string(it->second));return remoteValueAnswer(it->second);}
            if(op==2){say("SETTING action "+std::to_string(in.u8()));return {remoteOk};}
            auto toggle=[&](int section,const wchar_t* title,const wchar_t* detail,const char* key){RemoteSetting r;r.section=section;r.control=0;r.key=key;r.title=title;r.detail=detail;r.value=v[key];return r;};
            std::vector<RemoteSetting> items{toggle(0,L"Open on hover",L"Rest the pointer on the island to expand it","hoverOpen"),toggle(0,L"Hide until the pointer reaches the edge",L"The island tucks away and slides in when you touch its screen edge","autoHide")};
            {RemoteSetting r;r.section=0;r.control=1;r.key="hoverDelay";r.title=L"Hover delay";r.detail=L"How long the pointer rests before opening";r.lo=100;r.hi=700;r.step=10;r.unit=L" ms";r.value=v["hoverDelay"];items.push_back(r);}
            {RemoteSetting r;r.section=1;r.control=2;r.key="uiMode";r.title=L"Everyday mode";r.detail=L"How much the island shows while resting";r.hi=2;r.options={L"Mini Pill",L"Live Island",L"Command Center"};r.value=v["uiMode"];items.push_back(r);}
            {RemoteSetting r;r.section=2;r.control=2;r.key="theme";r.title=L"Theme";r.detail=L"Colors for the island and this window";r.hi=2;r.options={L"Dark",L"Light",L"System"};r.value=v["theme"];items.push_back(r);}
            {RemoteSetting r;r.section=2;r.control=4;r.key="accent";r.title=L"Accent";r.detail=L"Used when artwork colors are off";r.hi=4;r.options={L"Mint",L"Sky",L"Lilac",L"Peach",L"Wallpaper"};r.colours={0x5fd9b5,0x5eaaff,0xb58cff,0xffa37a,0x8ea3c2};r.value=v["accent"];items.push_back(r);}
            items.push_back(toggle(5,L"Sounds",L"A soft chime for alerts","sounds"));items.push_back(toggle(7,L"Weather",L"The weather where you are, on Home","weather"));
            items.push_back(toggle(8,L"Share with my PCs",L"Send files between your PCs and phones","sharing"));items.push_back(toggle(8,L"My phone can control this PC",L"The remote, the trackpad and more","phoneControl"));
            items.push_back(toggle(8,L"Live activities from my phone",L"A delivery, a ride or a timer on your phone shows on the island","phoneLive"));
            {RemoteSetting r;r.section=9;r.control=5;r.title=L"Updates";r.detail=L"Arnav Island looks for new versions";r.options={L"Check now"};r.action=18;items.push_back(r);}
            return remoteSettingsAnswer({L"General",L"Island",L"Appearance",L"Motion",L"Compact",L"Media & sound",L"Devices & power",L"Home & navigation",L"Privacy & productivity",L"About"},items);}
        case RemoteCommand::Controls:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);auto& k=made->controls;
            // The focus clock runs as the island's does: down to zero (then finished), or up for the stopwatch.
            const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
            if(k.focusRunning){const double gone=t-made->focusAt;if(k.focusMode==2)k.focusShown+=gone;else{k.focusShown=std::max(0.,k.focusShown-gone);if(k.focusShown<=0){k.focusRunning=false;k.focusFinished=true;}}}
            made->focusAt=t;
            if(op==1){const int control=in.u8(),value=in.i32();if(!in.ok())return {remoteFailed};say("CONTROL "+std::to_string(control)+" "+std::to_string(value));
                switch(control){case 1:k.wifi=value!=0;break;case 2:k.bluetooth=value!=0;break;case 3:k.dark=value!=0;break;case 4:k.wifi=k.bluetooth=value?0:1;break;case 5:k.brightness=std::clamp(value,0,100);break;
                    case 6:k.volume=std::clamp(value,0,100);*volume=k.volume;break;case 7:k.muted=value!=0;break;case 8:k.micMuted=value!=0;break;
                    case 15:case 16:k.focusMode=control==15?0:1;k.focusDuration=std::clamp(value,1,600)*60.;k.focusShown=k.focusDuration;k.focusRunning=true;k.focusFinished=false;break;
                    case 17:k.focusRunning=!k.focusRunning;break;case 18:k.focusRunning=false;k.focusShown=k.focusDuration;break;case 19:k.focusMode=2;k.focusShown=0;k.focusRunning=true;break;default:break;}}
            return remoteControlsAnswer(k);}
        case RemoteCommand::Command:{RemoteReader in(payload);const int op=in.u8();const std::wstring text=in.text(600);if(!in.ok())return {remoteFailed};
            struct Row{int kind;bool confirm;const wchar_t* title;const wchar_t* detail;const wchar_t* answer;};
            static const Row rows[]={{12,false,L"Open Spotify",L"App",L""},{12,false,L"Open Visual Studio Code",L"App",L""},{20,false,L"Lock this PC",L"Windows",L""},{34,true,L"Restart",L"Windows",L""},
                {9,false,L"Timer 10 min",L"Focus",L""},{37,false,L"100 USD in EUR",L"Exchange rate",L"85.12 EUR"},{28,false,L"Switch to light mode",L"Dark mode is on now",L""}};
            std::vector<RemoteResult> out;std::wstring low=text;for(auto& c:low)c=wchar_t(towlower(c));
            for(auto& r:rows){std::wstring t=r.title;for(auto& c:t)c=wchar_t(towlower(c));if(low.empty()||t.find(low)!=std::wstring::npos||(low.find(L"usd")!=std::wstring::npos&&r.kind==37))out.push_back({r.kind,r.confirm,r.title,r.detail,r.answer});}
            if(op==0)return remoteCommandAnswer(true,out);
            const int index=in.u8();const std::wstring title=in.text(600);const bool confirmed=in.u8()!=0;if(!in.ok())return {remoteFailed};
            if(index<0||size_t(index)>=out.size()||out[size_t(index)].title!=title)return remoteOutcome(3,L"The results changed. Try again");
            if(out[size_t(index)].confirm&&!confirmed)return remoteOutcome(1,L"Restart the PC? Anything unsaved there may be lost");
            say("COMMAND "+narrow(title));return remoteOutcome(0,out[size_t(index)].kind==12?L"Opened "+title.substr(5):out[size_t(index)].kind==37?L"Copied 85.12 EUR on the PC":title);}
        case RemoteCommand::Audio:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);
            if(op==1){const std::wstring id=in.text(400);if(!in.ok())return {remoteFailed};made->output=id;say("OUTPUT "+narrow(id));return {remoteOk};}
            return remoteAudioAnswer({{L"speakers",L"Speakers (Realtek(R) Audio)",made->output==L"speakers",1},{L"buds",L"Galaxy Buds2 Pro",made->output==L"buds",3}});}
        case RemoteCommand::Island:{RemoteReader in(payload);const int op=in.u8();const int page=in.u8();say(op==1?std::string("ISLAND close"):"ISLAND page "+std::to_string(page));return {remoteOk};}
        // Revision 9: made-up shuffle, repeat and up next (the island's own player); an app mixer that remembers its volumes;
        // levels that move; one wired adapter.
        case RemoteCommand::Playback:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);
            if(op==1){made->shuffle=in.u8()!=0;say("PLAYBACK shuffle "+std::to_string(made->shuffle));}
            else if(op==2){made->repeat=std::clamp(in.u8(),0,2);say("PLAYBACK repeat "+std::to_string(made->repeat));}
            else if(op==3){const int i=in.u16();if(!in.ok()||i<0||size_t(i)>=made->upNext.size())return {remoteFailed};say("PLAYBACK play "+std::to_string(i)+" "+narrow(made->upNext[size_t(i)].title));made->upNext.erase(made->upNext.begin(),made->upNext.begin()+i+1);}
            else if(op!=0)return {remoteUnsupported};
            if(!in.ok())return {remoteFailed};
            RemotePlayback p;p.island=p.shuffleSupported=p.repeatSupported=true;p.shuffle=made->shuffle;p.repeat=made->repeat;p.upNext=made->upNext;return remotePlaybackAnswer(p);}
        case RemoteCommand::Mixer:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);
            if(op==1||op==2){const uint32_t pid=uint32_t(in.i32());const int v=in.u8();if(!in.ok())return {remoteFailed};auto it=std::find_if(made->mixer.begin(),made->mixer.end(),[&](auto& e){return e.pid==pid;});
                if(it==made->mixer.end())return {remoteFailed};if(op==1)it->volume=std::clamp(v,0,100);else it->muted=v!=0;say("MIXER "+std::to_string(op)+" "+std::to_string(pid)+" "+std::to_string(v));}
            else if(op!=0)return {remoteUnsupported};
            const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();auto list=made->mixer;
            for(size_t i=0;i<list.size();++i)list[i].peak=list[i].active&&!list[i].muted?int(std::clamp(40+35*std::sin(t*3+double(i)),0.,100.)):0;
            // Real 32-pixel PNGs (made once, an app's colour each) when icons are asked for.
            const bool icons=op==0&&payload.size()>=2&&payload[1]!=0;
            if(icons){static const std::vector<std::vector<uint8_t>> made32{picture(32,32,0x1db954,true),picture(32,32,0x0078d7,true),picture(32,32,0x8e8e93,true)};for(size_t i=0;i<list.size();++i)list[i].icon=made32[i%3];}
            return remoteMixerAnswer(list);}
        case RemoteCommand::Levels:{const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();std::vector<uint8_t> bands;
            for(int b=0;b<24;++b)bands.push_back(uint8_t(std::clamp(140+90*std::sin(t*4+b*.5)-b*3,0.,255.)));return remoteLevelsAnswer(int(160+60*std::sin(t*2)),false,bands);}
        // Revision 10: made-up temperatures, a clipboard history that remembers pins and deletes, a running show of twelve
        // slides (pictures made here), and the speed test. Files and screenshots are reserved (unsupported).
        case RemoteCommand::Sensors:{const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();PcSensors s;s.cpu=std::round((58+6*std::sin(t*.4))*10)/10;s.gpu=std::round((47+5*std::sin(t*.3+1))*10)/10;
            s.zones={{L"Thermal zone THM0",s.cpu},{L"Thermal zone TZ01",44.5},{L"NVIDIA GeForce RTX 4060 Laptop GPU",s.gpu}};s.fans={{L"Graphics fan",1850}};say("REMOTE sensors");return remoteSensorsAnswer(s);}
        case RemoteCommand::Clips:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);auto& list=made->clips;size_t most=40;bool thumbs=true;
            if(op==0){most=size_t(std::clamp(in.u8(),0,40));thumbs=in.u8()!=0;if(!in.ok())return {remoteFailed};}
            else if(op>=1&&op<=4){const uint32_t id=uint32_t(in.i32());auto it=std::find_if(list.begin(),list.end(),[&](auto& c){return c.id==id;});if(!in.ok()||it==list.end())return {remoteFailed};
                if(op==4){if(id==1)return {remoteNotAllowed};say("CLIPS bring "+std::to_string(id));return remoteClipAnswer(it->kind,it->kind==1?std::wstring():it->text,it->kind==1?picture(640,480,0x7a5cff,false,1):std::vector<uint8_t>{});}
                if(op==1){say("CLIPS copy "+std::to_string(id));}else if(op==2){it->pinned=in.u8()!=0;say("CLIPS pin "+std::to_string(id)+" "+std::to_string(it->pinned));}else{list.erase(it);say("CLIPS delete "+std::to_string(id));}}
            else return {remoteUnsupported};
            std::vector<RemoteClip> out;for(int pinned=1;pinned>=0;--pinned)for(auto& c:list)if(c.pinned==(pinned!=0)&&out.size()<most){out.push_back(c);if(thumbs&&c.kind==1)out.back().thumb=picture(96,72,0x7a5cff,false,1);}
            return remoteClipsAnswer(out);}
        case RemoteCommand::Slides:{RemoteReader in(payload);const int op=in.u8();std::lock_guard lock(made->m);if(!in.ok())return {remoteFailed};
            if(op==1){const int i=in.u16();if(!in.ok()||i<1||i>12)return {remoteFailed};made->slide=i;say("SLIDES go "+std::to_string(i));}else if(op!=0)return {remoteUnsupported};
            static const uint32_t colours[]={0x3b82f6,0x10b981,0xf59e0b,0xef4444,0x8b5cf6,0x06b6d4};const int i=made->slide;
            RemoteSlides r;r.state=2;r.index=i;r.count=12;r.deck=L"Quarterly review";r.title=i==1?std::wstring(L"Quarterly review"):L"Slide "+std::to_wstring(i)+L": "+(i%2?L"What shipped":L"What’s next");
            r.notes=L"Open with the numbers.\nThen the two launches, and thank the team.";r.seconds=uint32_t(std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count());
            if(op==0&&payload.size()>=2&&payload[1]){r.current=picture(640,360,colours[i%6],false,i);if(i<12)r.next=picture(480,270,colours[(i+1)%6],false,i+1);}
            return remoteSlidesAnswer(r);}
        case RemoteCommand::Speed:return remoteSpeedAnswer(payload);
        case RemoteCommand::Files:case RemoteCommand::Screenshot:return {remoteUnsupported};
        // Revision 11: top apps (read only; their CPU moves), a laptop's battery health, two drives (a measure again takes three
        // seconds), startup apps (the user's own switch), an Ethernet and a Wi-Fi adapter, three updates (a check takes three
        // seconds), and twelve Start menu apps to open. Icons are real PNGs made here.
        case RemoteCommand::Processes:{RemoteReader in(payload);const int op=in.u8();if(op==1)return {remoteUnsupported};const int sort=in.u8(),most=in.u8(),icons=in.u8();
            if(!in.ok())return {remoteFailed};if(op!=0)return {remoteUnsupported};
            const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();auto wave=[&](double base,double spread,double phase){return std::max(0.,base+spread*std::sin(t*.7+phase));};
            std::vector<RemoteProcess> list{{18244,L"Microsoft Edge",L"Arnav Island – GitHub – Microsoft Edge",wave(6.5,3,0),1890ull<<20,true,false,false,{}},{4242,L"Spotify",L"Glass Horizons – Aurora Fields",wave(2.1,1,1),412ull<<20,true,false,false,{}},
                {9120,L"Visual Studio Code",L"IslandRemote.cpp – Arnav Island – Visual Studio Code",wave(4.2,2.5,2),1240ull<<20,true,false,false,{}},{2210,L"Arnav Island",L"",wave(.8,.4,3),61ull<<20,false,false,true,{}},
                {6600,L"Windows Explorer",L"Downloads",wave(.6,.3,4),188ull<<20,true,false,false,{}},{4,L"System",L"",wave(1.4,.8,5),12ull<<20,false,true,false,{}},
                {3312,L"Antimalware Service Executable",L"",wave(3,2.8,6),305ull<<20,false,true,false,{}},{7788,L"Discord",L"#general – Discord",wave(1.2,.6,7),356ull<<20,true,false,false,{}}};
            if(icons){static const std::vector<std::vector<uint8_t>> made32{picture(32,32,0x0078d7,true),picture(32,32,0x1db954,true),picture(32,32,0x23a9f2,true),picture(32,32,0x5fd9b5,true),picture(32,32,0xf3c13a,true),picture(32,32,0x8e8e93,true),picture(32,32,0x2f6fdb,true),picture(32,32,0x5865f2,true)};
                for(size_t i=0;i<list.size();++i)list[i].icon=made32[i];}
            std::sort(list.begin(),list.end(),[&](auto& a,auto& b){return sort==1?a.memory>b.memory:a.cpu>b.cpu;});if(list.size()>size_t(std::min(most,30)))list.resize(size_t(std::min(most,30)));
            say("REMOTE processes");return remoteProcessesAnswer(list);}
        case RemoteCommand::BatteryHealth:{PcBatteryHealth h;h.present=true;h.designMwh=76000;h.fullMwh=66880;h.cycles=312;h.chemistry=L"Li-ion";h.manufacturer=L"ASUSTeK";h.name=L"A32N2105";h.currentMwh=54830;h.rateMw=-9850;h.voltageMv=15870;
            say("REMOTE battery health");return remoteBatteryHealthAnswer(h);}
        case RemoteCommand::Storage:{RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};if(op>1)return {remoteUnsupported};std::lock_guard lock(made->m);
            const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();if(op==1&&(made->measuring<0||t-made->measuring>=3)){made->measuring=t;say("STORAGE measure");}
            const double gone=made->measuring<0?3:t-made->measuring;PcStorage s;s.state=gone>=3?1:0;s.progress=s.state?100:int(gone/3*100);
            s.drives={{L"C:\\",L"Windows",0,1000202039296ull,312461942784ull,true},{L"D:\\",L"Data",0,2000396746752ull,1210347384832ull,false}};
            const std::vector<PcStorageGroup> all{{L"Apps",0,103079215104ull},{L"Documents",2,22548578304ull},{L"Downloads",3,36507222016ull},{L"Pictures",4,51539607552ull},{L"Videos",5,77309411328ull},{L"Music",6,12884901888ull},
                {L"Desktop",7,6442450944ull},{L"Recycle bin",8,1395864371ull},{L"System",1,62277025792ull},{L"Other",9,315537291264ull}};
            s.groups.assign(all.begin(),all.begin()+long(s.state?all.size():std::min(all.size(),size_t(gone/3*all.size()))));return remoteStorageAnswer(s);}
        case RemoteCommand::Startup:{RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};std::lock_guard lock(made->m);
            if(op==1){const std::wstring id=in.text(4096);const bool on=in.u8()!=0;if(!in.ok())return {remoteFailed};auto it=std::find_if(made->startup.begin(),made->startup.end(),[&](auto& a){return a.id==id;});
                if(it==made->startup.end())return {remoteFailed};if(!it->canChange)return {remoteNotAllowed};it->enabled=on;say("STARTUP "+narrow(id)+" "+std::to_string(on));}
            else if(op!=0)return {remoteUnsupported};
            static const std::vector<std::vector<uint8_t>> made32{picture(32,32,0x0364b8,true),picture(32,32,0x1db954,true),picture(32,32,0x5fd9b5,true),picture(32,32,0x1b2838,true),picture(32,32,0x2f6fdb,true),picture(32,32,0xe5e5e5,true)};
            auto list=made->startup;for(size_t i=0;i<list.size();++i)list[i].icon=made32[i%made32.size()];return remoteStartupAnswer(list);}
        case RemoteCommand::Network:{PcNetwork e;e.name=L"Ethernet";e.description=L"Realtek PCIe GbE Family Controller";e.kind=0;e.up=e.isDefault=true;e.mac="CC:28:AA:4A:1B:C9";e.speed=1000000000;e.ipv4={"192.168.1.20/24"};
            e.ipv6={"2401:4900:1f26:44e0:5c1a:2b3c:4d5e:6f70","fe80::2c4c:a9ed:e6e2:404a"};e.gateway="192.168.1.1";e.dns={"192.168.1.1","1.1.1.1"};
            PcNetwork w;w.name=L"Wi-Fi";w.description=L"Intel(R) Wi-Fi 6E AX211 160MHz";w.kind=1;w.up=true;w.mac="A0:D3:65:02:C5:56";w.speed=866700000;w.ipv4={"192.168.1.34/24"};w.ipv6={"fe80::f49e:d787:a31:bc36"};
            w.gateway="192.168.1.1";w.dns={"192.168.1.1"};w.ssid=L"Home-5G";w.signal=72;w.band="5 GHz";w.channel=44;w.security="WPA2-Personal";say("REMOTE network");return remoteNetworkAnswer({e,w});}
        case RemoteCommand::Updates:{RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};if(op>2)return {remoteUnsupported};std::lock_guard lock(made->m);
            const double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();const uint64_t now=uint64_t(std::time(nullptr));if(!made->checked)made->checked=now-5400;
            if(op==1&&(made->checking<0||t-made->checking>=3)){made->checking=t;say("UPDATES check");}if(op==2)say("UPDATES open");
            if(made->checking>=0&&t-made->checking>=3){made->checking=-1;made->checked=now;}
            PcUpdates u;u.state=made->checking>=0?4:2;u.checked=made->checked;u.installed=now-6*86400;
            u.updates={{L"2026-10 Cumulative Update for Windows 11 Version 25H2 for x64-based Systems (KB5070001)",0,851443712ull,true,true},{L"Intel Corporation - Display - 32.0.101.6881",1,671088640ull,false,true},
                {L"Security Intelligence Update for Microsoft Defender Antivirus - KB2267602 (Version 1.441.112.0)",3,12582912ull,true,false}};
            return remoteUpdatesAnswer(u);}
        case RemoteCommand::Apps:{RemoteReader in(payload);const int op=in.u8();if(!in.ok())return {remoteFailed};
            static const std::vector<RemoteApp> apps{{L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App",L"Calculator",{}},{L"microsoft.windowscommunicationsapps_8wekyb3d8bbwe!microsoft.windowslive.calendar",L"Calendar",{}},
                {L"Discord",L"Discord",{}},{L"{6D809377-6AF0-444B-8957-A3773F02200E}\\Microsoft VS Code\\Code.exe",L"Visual Studio Code",{}},{L"Microsoft.Windows.Explorer",L"File Explorer",{}},
                {L"MSEdge",L"Microsoft Edge",{}},{L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App",L"Notepad",{}},{L"Microsoft.Windows.Photos_8wekyb3d8bbwe!App",L"Photos",{}},
                {L"windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel",L"Settings",{}},{L"Microsoft.ScreenSketch_8wekyb3d8bbwe!App",L"Snipping Tool",{}},
                {L"Spotify.exe",L"Spotify",{}},{L"Microsoft.WindowsTerminal_8wekyb3d8bbwe!App",L"Terminal",{}}};
            if(op==1){const std::wstring id=in.text(8192);if(!in.ok())return {remoteFailed};if(std::none_of(apps.begin(),apps.end(),[&](auto& a){return a.id==id;}))return {remoteFailed};say("APPS open "+narrow(id));return {remoteOk,1};}
            if(op!=0)return {remoteUnsupported};const bool icons=in.u8()!=0;if(!in.ok())return {remoteFailed};auto list=apps;
            if(icons){static const uint32_t colours[]={0x404040,0x0f6cbd,0x5865f2,0x23a9f2,0xf3c13a,0x0078d7,0x2b88d8,0xc239b3,0x6b6b6b,0xe74856,0x1db954,0x2d2d2d};
                static const std::vector<std::vector<uint8_t>> made48=[]{std::vector<std::vector<uint8_t>> v;for(int i=0;i<12;++i)v.push_back(picture(48,48,colours[i],true,i));return v;}();
                for(size_t i=0;i<list.size();++i)list[i].icon=made48[i];}
            say("REMOTE apps");return remoteAppsAnswer(list);}
        case RemoteCommand::Machine:{PcAdapter a;const uint8_t mac[6]={0x00,0x1a,0x2b,0x3c,0x4d,0x5e};std::memcpy(a.mac,mac,6);a.name=L"Ethernet";a.ipv4="192.168.1.20";a.prefix=24;say("REMOTE machine");return remoteMachineAnswer({a});}
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
                case K::Offer:say("OFFER "+std::to_string(ev.count)+" "+std::to_string(ev.size)+(ev.toShelf?" shelf":""));
                    // Revision 8: the offer's picture (its bytes, whether it's a JPEG) and the ask it answers.
                    if(!ev.icon.empty()||ev.ask)say("OFFERPIC "+std::to_string(ev.icon.size())+" "+std::to_string(ev.icon.size()>3&&ev.icon[0]==0xFF&&ev.icon[1]==0xD8?1:0)+" "+std::to_string(ev.ask)+" "+narrow(ev.file));
                    share.answer(ev.transfer,true);break;
                case K::Received:say("RECEIVED "+narrow(ev.detail)+"|"+std::to_string(ev.code)+(ev.ask?"|ask "+std::to_string(ev.ask):""));break;
                // Revision 8: a photo just taken on the phone.
                case K::PhonePhoto:say("PHONEPHOTO "+ev.key+" "+narrow(ev.file)+" "+std::to_string(ev.size)+" "+std::to_string(ev.code>>16)+"x"+std::to_string(ev.code&0xffff)+" "+std::to_string(ev.icon.size()));break;
                case K::Sent:say("SENT "+narrow(ev.file));break;
                case K::Failed:say("FAILED "+narrow(ev.file)+"|"+narrow(ev.detail));break;
                case K::Handoff:say("HANDOFF "+narrow(ev.handoff.title)+"|"+std::to_string(ev.handoff.position));share.answerHandoff(ev.transfer,1);break;
                case K::HandoffAnswered:say("HANDOFFANSWER "+std::to_string(ev.code));break;
                case K::ShelfTaken:say("SHELFTAKEN "+narrow(ev.file));break;
                case K::PhoneStatus:say("PHONESTATUS "+std::to_string(ev.battery)+" "+std::to_string(ev.charging?1:0));break;
                case K::PhoneNotice:{std::string acts;for(auto& [title,reply]:ev.actions)acts+="|"+narrow(title)+(reply?"*":"");
                    say("PHONENOTICE "+narrow(ev.app)+"|"+narrow(ev.file)+"|"+narrow(ev.detail)+"|"+std::to_string(ev.icon.size())+"|"+std::to_string(ev.urgent?1:0)+(ev.key.empty()?"":"|"+ev.key+acts));break;}
                case K::PhoneDetails:say("DETAILS "+narrow(ev.detail).substr(0,narrow(ev.detail).find('\n')));break;
                // Revision 6: a phone's readings, asked for: how many lines, the first, and the cover's size.
                case K::PhoneLive:{const std::string d=narrow(ev.detail);say("PHONELIVE "+std::to_string(std::count(d.begin(),d.end(),'\n')+(d.empty()?0:1))+" "+d.substr(0,d.find('\n'))+" "+std::to_string(ev.icon.size()));break;}
                case K::PhoneNoticeGone:say("GONE "+ev.key);break;
                case K::PhoneHotspot:say(std::string("HOTSPOT ")+(ev.code?"on ":"off ")+narrow(ev.file)+" "+std::to_string(ev.detail.size()));break;
                case K::Rang:say("RANG");break;
                case K::PairingCode:say("CODE "+narrow(ev.detail));say("LINK "+share.pairingLink(narrow(ev.detail)));break;
                case K::Peers:for(auto& p:share.peers())if(p.paired)say("PRESENCE "+p.id+" "+std::to_string(p.online)+" "+std::to_string(p.viaInternet)+" "+std::to_string(p.revision)+" "+std::to_string(p.phone));break;
                default:break;}}
            std::this_thread::sleep_for(std::chrono::milliseconds(30));}});
    std::string line;std::vector<ShareShelfEntry> shelf;
    while(std::getline(std::cin,line)){
        std::istringstream in(line);std::string cmd;in>>cmd;
        if(cmd=="quit")break;
        if(cmd=="peer"){std::string id;int p=0;in>>id>>p;share.addPeer(id,L"Phone",("127.0.0.1"),uint16_t(p),shareProtocol,shareRevision);say("OK peer");}
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
        // Revision 6: ask a phone for its readings; tell it the focus clock (focus <id> <mode> <running> <shown> <duration>).
        else if(cmd=="query"){std::string id;in>>id;share.queryPhone(id);}
        // Revision 8: ask a phone for a photo just taken (photo-ask <id> <photo id> <1 paste|2 shelf> <ask>); send it a page
        // (page <id> <scroll> <url> <title...>).
        else if(cmd=="photo-ask"){std::string id;unsigned long long photo=0;int purpose=0;unsigned ask=0;in>>id>>photo>>purpose>>ask;share.askPhonePhoto(id,photo,purpose,ask);say("OK photo-ask");}
        else if(cmd=="page"){std::string id,url,title;double scroll=0;in>>id>>scroll>>url;std::getline(in>>std::ws,title);share.tellPhonePage(id,widen(url),widen(title),scroll);say("OK page");}
        // Revision 7: input for the phone screen showing, as the island's window sends it: a tap or a swipe (0-65535 across
        // its screen), a key (1 back, 2 home, 3 recent apps, 4 notifications), typing.
        else if(cmd=="mtap"||cmd=="mswipe"||cmd=="mkey"||cmd=="mtext"){std::lock_guard held(shownLock);
            auto touch=[&](int a,int x,int y){std::vector<uint8_t> b{0xA6,uint8_t(a),uint8_t(x),uint8_t(x>>8),uint8_t(y),uint8_t(y>>8)};if(shown&&shown->send)shown->send(b);};
            if(!shown||!shown->send)say("MIRROR none");
            else if(cmd=="mtap"){int x=0,y=0;in>>x>>y;touch(0,x,y);touch(2,x,y);say("OK mtap");}
            else if(cmd=="mswipe"){int x0=0,y0=0,x1=0,y1=0;in>>x0>>y0>>x1>>y1;touch(0,x0,y0);for(int i=1;i<=10;++i){std::this_thread::sleep_for(std::chrono::milliseconds(25));touch(1,x0+(x1-x0)*i/10,y0+(y1-y0)*i/10);}touch(2,x1,y1);say("OK mswipe");}
            else if(cmd=="mkey"){int k=0;in>>k;shown->send({0xA7,uint8_t(k)});say("OK mkey");}
            else{std::string t;std::getline(in>>std::ws,t);if(t=="\\n")t="\n";if(t=="\\b")t="\b";std::vector<uint8_t> b{0xA8};b.insert(b.end(),t.begin(),t.end());shown->send(b);say("OK mtext");}}
        // Revision 7 (tests): n frames of the made-up picture, encoded as a phone would send them, into a file: for each,
        // u32 size, u8 key, then its Annex B.
        else if(cmd=="encode"){std::string path;int n=0,w=0,h=0;in>>path>>n>>w>>h;auto source=mirror::patternSource(w,h);mirror::VideoEncoder enc;int wrote=0;
            if(source->open()&&enc.open(source->device(),w,h,30,1'500'000)){std::ofstream f(path,std::ios::binary);std::vector<mirror::VideoEncoder::Frame> out;
                for(int i=0;i<n*3&&wrote<n;++i){ComPtr<ID3D11Texture2D> t;if(source->next(1000,t)!=mirror::ScreenCapture::Result::Frame)continue;out.clear();enc.encode(t.Get(),DXGI_MODE_ROTATION_IDENTITY,int64_t(i)*333333,out);
                    for(auto& fr:out){const uint32_t sz=uint32_t(fr.data.size());f.write(reinterpret_cast<const char*>(&sz),4);const char k=fr.key?1:0;f.write(&k,1);f.write(reinterpret_cast<const char*>(fr.data.data()),std::streamsize(sz));++wrote;}}}
            say("ENCODED "+std::to_string(wrote)+" "+(enc.hardware()?"gpu":"software"));}
        // Revision 7 (tests): the island's phone window, fed in-process by the made-up picture at a phone's shape, then
        // turned: whether it opened, frames fed and drawn, and its size before and after the turn.
        else if(cmd=="mwindow"){mirror::MirrorWindow win;int fed=0;const bool ok=win.begin(720,1280,L"Test phone");RECT a{},b{};
            if(ok){mirror::VideoDecoder dec;dec.open(win.device());
                auto play=[&](int w,int h,int n){auto src=mirror::patternSource(w,h);mirror::VideoEncoder enc;if(!src->open()||!enc.open(src->device(),w,h,30,2'000'000))return;std::vector<mirror::VideoEncoder::Frame> out;
                    for(int i=0;i<n;++i){ComPtr<ID3D11Texture2D> t;if(src->next(1000,t)!=mirror::ScreenCapture::Result::Frame)continue;out.clear();enc.encode(t.Get(),DXGI_MODE_ROTATION_IDENTITY,int64_t(i)*333333,out);
                        for(auto& f:out)dec.decode(f.data.data(),f.data.size(),f.pts,[&](ID3D11Texture2D* tx,UINT slice,int fw,int fh){win.frame(tx,slice,fw,fh);++fed;});}};
                play(720,1280,60);std::this_thread::sleep_for(std::chrono::milliseconds(400));GetWindowRect(win.window(),&a);
                play(1280,720,60);std::this_thread::sleep_for(std::chrono::milliseconds(600));GetWindowRect(win.window(),&b);}
            say("MWINDOW "+std::to_string(ok)+" "+std::to_string(fed)+" "+std::to_string(win.presented.load())+" "+std::to_string(a.right-a.left)+"x"+std::to_string(a.bottom-a.top)+" "+std::to_string(b.right-b.left)+"x"+std::to_string(b.bottom-b.top));win.end();}
        // Revision 7 (tests): this PC's real screen, captured and encoded for n frames with nothing kept or sent: counts only
        // (frames captured, encoded, key frames, bytes, the size, the encoder).
        else if(cmd=="capture"){int n=0;in>>n;auto src=mirror::monitorSource({0,0});mirror::VideoEncoder enc;int got=0,encoded=0,keys=0;uint64_t bytes=0;
            if(src->open()&&enc.open(src->device(),src->width()&~1,src->height()&~1,30,6'000'000)){std::vector<mirror::VideoEncoder::Frame> out;const auto began=std::chrono::steady_clock::now();ComPtr<ID3D11Texture2D> last;
                while(encoded<n&&std::chrono::steady_clock::now()-began<std::chrono::seconds(20)){ComPtr<ID3D11Texture2D> t;const auto r=src->next(100,t);if(r==mirror::ScreenCapture::Result::Frame){last=t;++got;}
                    // A still screen gives no new frames: the last one again (as the key frames a phone asks for are).
                    if(!last)continue;out.clear();enc.encode(last.Get(),src->rotation(),int64_t(encoded)*333333,out);for(auto& f:out){++encoded;keys+=f.key;bytes+=f.data.size();}}}
            say("CAPTURE "+std::to_string(got)+" "+std::to_string(encoded)+" "+std::to_string(keys)+" "+std::to_string(bytes)+" "+std::to_string(src->width())+"x"+std::to_string(src->height())+" "+(enc.hardware()?"gpu":"software"));}
        else if(cmd=="focus"){std::string id;int mode=0,running=0;double shown=0,duration=0;in>>id>>mode>>running>>shown>>duration;std::vector<uint8_t> st{uint8_t(mode),uint8_t(running),0};
            auto f64=[&](double v){uint64_t u=0;std::memcpy(&u,&v,8);for(int i=0;i<8;++i)st.push_back(uint8_t(u>>(8*i)));};f64(shown);f64(duration);
            const std::string pc=narrow(name);for(int i=0;i<4;++i)st.push_back(uint8_t(pc.size()>>(8*i)));st.insert(st.end(),pc.begin(),pc.end());share.tellPhoneFocus(id,st);say("OK focus");}
        else if(cmd=="code"){std::string code;std::getline(in>>std::ws,code);share.pairWithCode(code);}
        else if(cmd=="peers"){for(auto& p:share.peers()){say("PEER "+p.id+" "+std::to_string(p.paired)+" "+std::to_string(p.online)+" "+std::to_string(p.viaInternet)+" "+narrow(p.name));
                if(p.paired)say("PATH "+p.id+" "+std::to_string(p.path)+" "+std::to_string(int(p.rtt))+" "+std::to_string(p.relays)+" "+std::to_string(p.v6?1:0));}
            say("INTERNET "+std::to_string(share.internet())+" "+narrow(share.relayBroker()));}
    }
    done=true;events.join();return 0;
}
