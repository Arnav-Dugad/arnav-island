#include "Animation/SpringExpression.h"
#include "Audio/Spectrum.h"
#include "Settings/SettingsModel.h"
#include "Media/AppIdentity.h"
#include "Design/SvgPath.h"
#include "Design/BrandMatch.h"
#include "Interaction/AutoHide.h"
#include "Hardware/BatteryModel.h"
#include "Hardware/BluetoothProvider.h"
#include "Productivity/Commands.h"
#include "Productivity/ClipboardModel.h"
#include "Productivity/Workspaces.h"
#include "Productivity/Privacy.h"
#include "Audio/Waveform.h"
#include "Design/Accent.h"
#include "Media/Lyrics.h"
#include "Audio/AudioRoute.h"
#include "Interaction/DetailModels.h"
#include "FileShelf/Zip.h"
#include "FileShelf/ShelfStore.h"
#include "Capture/CaptureModel.h"
#include "Productivity/Currency.h"
#include "Productivity/FileSearch.h"
#include "Productivity/CommandMemory.h"
#include "Hardware/GpuModel.h"
#include "Productivity/Weather.h"
#include "Design/Backdrop.h"
#include "Animation/MotionEngine.h"
#include "Media/Library.h"
#include "Audio/Sounds.h"
#include "Composition/GlassBackdrop.h"
#include "Media/Timeline.h"
#include "Productivity/Update.h"
#include <fstream>
#include <regex>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <chrono>
using namespace nexus;
static int checks=0;
static void test(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);}
// Minimal RFC 1951 inflater for stored and fixed-Huffman blocks, written independently of the deflater.
static std::vector<uint8_t> inflate(const std::vector<uint8_t>& in){
    std::vector<uint8_t> out;size_t pos=0;int bit=0;
    auto get=[&](int n){uint32_t v=0;for(int i=0;i<n;++i){if(pos>=in.size())throw std::runtime_error("inflate overrun");v|=uint32_t((in[pos]>>bit)&1)<<i;if(++bit==8){bit=0;++pos;}}return v;};
    auto code=[&]{// fixed literal/length code, read bit by bit (MSB first)
        uint32_t c=0;for(int n=1;n<=9;++n){c=(c<<1)|get(1);if(n==7&&c<=0x17)return int(c+256);if(n==8&&c>=0x30&&c<=0xbf)return int(c-0x30);if(n==8&&c>=0xc0&&c<=0xc7)return int(c-0xc0+280);if(n==9&&c>=0x190)return int(c-0x190+144);}throw std::runtime_error("bad code");};
    const int lb[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258},le[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    const int db[]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577},de[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    for(bool last=false;!last;){last=get(1);int type=int(get(2));
        if(type==0){if(bit){bit=0;++pos;}if(pos+4>in.size())throw std::runtime_error("stored header");size_t n=in[pos]|(in[pos+1]<<8);pos+=4;out.insert(out.end(),in.begin()+std::ptrdiff_t(pos),in.begin()+std::ptrdiff_t(pos+n));pos+=n;continue;}
        if(type!=1)throw std::runtime_error("unexpected block type");
        for(;;){int sym=code();if(sym<256){out.push_back(uint8_t(sym));continue;}if(sym==256)break;sym-=257;int len=lb[sym]+int(get(le[sym]));uint32_t d=0;for(int i=0;i<5;++i)d=(d<<1)|get(1);int dist=db[d]+int(get(de[d]));if(size_t(dist)>out.size())throw std::runtime_error("distance");for(int i=0;i<len;++i)out.push_back(out[out.size()-size_t(dist)]);}}
    return out;
}
// Minimal evaluator for the composition expression subset the glass layer emits.
struct Expression {
    const std::wstring& text;size_t at=0;double t;
    void space(){while(at<text.size()&&text[at]==L' ')++at;}
    bool eat(wchar_t c){space();if(at<text.size()&&text[at]==c){++at;return true;}return false;}
    double sum(){double v=product();for(;;){if(eat(L'+'))v+=product();else if(eat(L'-'))v-=product();else return v;}}
    double product(){double v=unary();for(;;){if(eat(L'*'))v*=unary();else if(eat(L'/'))v/=unary();else return v;}}
    double unary(){if(eat(L'-'))return -unary();return primary();}
    double primary(){
        space();if(eat(L'(')){double v=sum();if(!eat(L')'))throw std::runtime_error("expression: missing )");return v;}
        if(text.compare(at,3,L"p.t")==0){at+=3;return t;}
        for(auto name:{L"Pow",L"Cos",L"Sin"})if(text.compare(at,wcslen(name),name)==0){at+=wcslen(name);if(!eat(L'('))throw std::runtime_error("expression: call");double a=sum(),b=0;bool pow=name[0]==L'P';if(pow){if(!eat(L','))throw std::runtime_error("expression: comma");b=sum();}if(!eat(L')'))throw std::runtime_error("expression: close");return pow?std::pow(a,b):name[0]==L'C'?std::cos(a):std::sin(a);}
        size_t used=0;double v=std::stod(text.substr(at),&used);if(!used)throw std::runtime_error("expression: number");at+=used;return v;
    }
    static double evaluate(const std::wstring& text,double t){Expression e{text,0,t};double v=e.sum();e.space();if(e.at!=text.size())throw std::runtime_error("expression: trailing text");return v;}
};
int main(){try{
    // Glass geometry expressions reproduce the DirectComposition springs exactly.
    std::mt19937 random(7);
    for(auto spec:{preset(MotionPreset::Balanced),preset(MotionPreset::Playful),preset(MotionPreset::Calm),SpringSpec{1,400,40},SpringSpec{1,300,70},SpringSpec{.7,420,30}})
        for(int trial=0;trial<40;++trial){Spring s{double(random()%500)};s.retarget(double(random()%600),0,spec);double now=(random()%400)/1000.;s.retarget(double(random()%600),now,spec);double later=now+(random()%100)/1000.;
            auto terms=SpringTerms::from(s,later);auto text=springExpression(terms,1.5);
            for(double dt:{0.,.004,.016,.05,.12,.3,.8,2.}){double expected=s.sample(later+dt).position;test(std::abs(terms.evaluate(dt)-expected)<1e-6*std::max(1.,std::abs(expected)),"spring terms match the physical spring");
                test(std::abs(Expression::evaluate(text,dt)-expected*1.5)<2e-5*std::max(1.,std::abs(expected)),"compositor expression text evaluates to the spring");}}
    {SpringSpec critical{1,400,40};Spring s{0};s.retarget(100,0,critical);auto terms=SpringTerms::from(s,.05);test(terms.regime==SpringTerms::Regime::Critical,"critical damping recognized");test(std::abs(Expression::evaluate(springExpression(terms,1),.1)-s.sample(.15).position)<1e-4,"critical expression");}
    {Spring settled{42};auto terms=SpringTerms::from(settled,3);test(terms.regime==SpringTerms::Regime::Settled&&Expression::evaluate(springExpression(terms,2),9)==84,"settled spring is constant");}
    // Glides keep position and velocity continuous when data arrives mid-segment.
    Glide g;g.to(1,0,.05);for(int i=1;i<200;++i){double now=i*.013;auto before=g.sample(now);g.to((i*37%100)/100.,now,.055);auto after=g.sample(now);test(std::abs(before.position-after.position)<1e-12&&std::abs(before.velocity-after.velocity)<1e-9,"glide continuity");}
    test(std::abs(g.sample(1e6).position-g.p1)<1e-12&&g.sample(1e6).velocity==0,"glide settles on its value");
    {Glide h;h.to(.8,1,.05);auto seg=h.segment();double s=.05;test(std::abs(seg.p+s*(seg.v+s*(seg.quadratic+s*seg.cubic))-.8)<1e-9,"glide segment ends on target");}
    // Spectrum: a 1 kHz tone lights its own band; silence rests; values stay bounded.
    for(float frequency:{110.f,1000.f,6000.f}){Spectrum sp;std::vector<float> block(480);double phase=0;
        for(int step=0;step<60;++step){for(auto& v:block){v=.5f*float(std::sin(phase));phase+=2*3.14159265358979*frequency/48000;}sp.push(block.data(),int(block.size()));sp.analyze(48000,.01f);}
        int peak=int(std::max_element(sp.bands.begin(),sp.bands.end())-sp.bands.begin());float lo=50*std::pow(280.f,float(peak)/Spectrum::bandCount),hi=50*std::pow(280.f,float(peak+1)/Spectrum::bandCount);
        test(frequency>=lo*.8f&&frequency<=hi*1.25f,"tone energy lands in its band");test(sp.bands[peak]>.5f&&sp.level>.5f,"audible tone shows");
        for(float b:sp.bands)test(b>=0&&b<=1,"bands bounded");
        std::vector<float> quiet(480,0.f);for(int step=0;step<200;++step){sp.push(quiet.data(),480);sp.analyze(48000,.01f);}test(sp.resting(),"silence decays to rest");}
    {Spectrum sp;std::mt19937 noise(3);std::uniform_real_distribution<float> d(-1,1);std::vector<float> block(480);for(int step=0;step<100;++step){for(auto& v:block)v=d(noise);sp.push(block.data(),480);sp.analyze(44100,.01f);}for(float b:sp.bands)test(std::isfinite(b)&&b>=0&&b<=1,"full-scale noise bounded");}
    // Every persisted preference is reachable from the Settings window model.
    Settings defaults;std::stringstream written;defaults.write(written);std::set<std::string> keys;std::string key;double value;while(written>>key>>value)if(key!="version")keys.insert(key);
    auto items=settingItems(2);std::set<std::string> covered;for(auto& i:items)if(!i.key.empty())covered.insert(i.key);
    // The seven chip positions are one control ("chips").
    for(auto& k:keys)test(covered.contains(k)||(k.starts_with("chip")&&covered.contains("chips")),"settings window exposes every persisted preference");
    for(auto& item:items){if(item.control==SettingControl::Glass||item.control==SettingControl::Chips||item.control==SettingControl::Button||item.control==SettingControl::Note||item.control==SettingControl::Order||item.control==SettingControl::Preview||item.control==SettingControl::Actions||item.control==SettingControl::Town)continue;
        // Slow motion is a study aid and is deliberately never saved.
        if(item.key!="labSpeed")for(int v=item.lo;v<=item.hi;v+=std::max(1,(item.hi-item.lo)/12)){Settings s;item.set(s,v);std::stringstream io;s.write(io);auto back=Settings::parse(io);test(item.get(back)==item.get(s),"each control value round-trips through the settings file");}
        if(item.control==SettingControl::Slider||item.control==SettingControl::Choice||item.control==SettingControl::Swatch){Settings s;item.set(s,item.hi+1000);test(item.get(s)<=item.hi,"controls clamp above range");item.set(s,item.lo-1000);test(item.get(s)>=item.lo,"controls clamp below range");}}
    for(int a=0;a<7;++a)for(int slot=0;slot<3;++slot){auto metrics=defaultMetrics;assignMetric(metrics,slot,a);test(metrics[slot]==a&&metrics[0]!=metrics[1]&&metrics[1]!=metrics[2]&&metrics[0]!=metrics[2],"statistics stay unique when assigned");}
    {Settings s;for(auto& item:items)if(item.control==SettingControl::Order)for(int d:{1,-1,1,1,-1}){item.set(s,d);test(validNavigation(s.navigation),"navigation order stays a permutation");}}
    std::stringstream v5("version 5\nglass 1\nuiMode 2\ncompactWidth 300\n");auto migrated=Settings::parse(v5);test(migrated.material==0&&migrated.uiMode==2&&migrated.compactWidth==300&&migrated.waveform&&migrated.hud,"v5 settings migrate; legacy interior glass is not reinterpreted");
    Settings glass;glass.material=1;test(!glass.floating(),"glass meets the screen edge with shoulders, like solid");glass.material=2;test(!glass.floating(),"clear glass is attached too");glass.verticalOffset=20;test(glass.floating(),"an explicit offset floats glass");Settings solid;test(!solid.floating(),"solid stays attached");
    // Browser identity is exact enough not to capture unrelated apps.
    for(auto name:{L"Google Chrome",L"MSEdge",L"Microsoft Edge",L"firefox.exe",L"Brave",L"arc.exe"})test(isBrowserName(name),"browser recognized");
    for(auto name:{L"Spotify",L"Arcade Studio",L"Citizen",L"VLC media player",L"Apple Music",L"Search"})test(!isBrowserName(name),"non-browser not misclassified");
    // Every embedded brand mark parses and stays inside its 24x24 view box.
    struct Bounds:SvgSink{float x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;int segments=0;void add(float x,float y){x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);++segments;}
        void move(float x,float y)override{add(x,y);}void line(float x,float y)override{add(x,y);}void cubic(float,float,float,float,float x,float y)override{add(x,y);}void quad(float,float,float x,float y)override{add(x,y);}void arc(float,float,float,bool,bool,float x,float y)override{add(x,y);}void close()override{}};
    for(auto& mark:brandMarks){Bounds b;test(SvgPathReader(mark.path).read(b),"brand path parses");test(b.segments>2&&b.x0>=-.6f&&b.y0>=-.6f&&b.x1<=24.6f&&b.y1<=24.6f,"brand path inside view box");}
    test(std::size(brandMarks)>=170,"many real brand marks embedded");
    for(auto slug:{"xbox","aula","philips","powera","marshall","jabra","jiohotstar","logitech","nintendo","microsoft","openai","gmail","f1","googlegemini","claude","github","cloudflare","primevideo","hulu"})test(findBrand(slug)!=nullptr,"brand mark present");
    test(deviceBrand(L"AULA-F75 5.0 KB")=="aula","AULA keyboard");test(deviceBrand(L"Xbox Wireless Controller")=="xbox","Xbox controller");test(deviceBrand(L"PowerA Enhanced Wireless Controller")=="powera","PowerA controller");
    test(deviceBrand(L"Philips TAT3217")=="philips","Philips earbuds");test(deviceBrand(L"Marshall Major IV")=="marshall"&&deviceBrand(L"MAJOR IV")=="marshall","Marshall headphones");test(deviceBrand(L"Jabra Elite 85t")=="jabra","Jabra earbuds");
    test(deviceBrand(L"MX Master 3S")=="logitech"&&deviceBrand(L"Pro Controller")=="nintendo","Logitech and Nintendo by model");test(deviceBrand(L"Surface Headphones 2")=="microsoft","Surface is Microsoft");
    test(deviceBrand(L"Keyboard",0x045e,2)=="microsoft"&&deviceBrand(L"Gamepad",0x20d6,2)=="powera"&&deviceBrand(L"Mouse",0x01da,1)=="logitech","vendor and company IDs");
    test(appBrand(L"Patriot Memory").empty()&&appBrand(L"Twitch").empty(),"no substring false positives (riot, itch)");test(appBrand(L"Microsoft.GamingApp_8wekyb3d8bbwe!Microsoft.Xbox.App")=="xbox","Xbox app");
    test(assignServices({L"Episode 1 - YouTube TV"},{{L"Episode 1",L""}}).front()=="youtubetv","YouTube TV before YouTube");
    {Bounds b;test(SvgPathReader("M2 2h4v4H2zm8 0l2 2-2 2a2 2 0 1 1 0-4Z").read(b)&&b.x0==2&&b.x1==12,"relative commands and arcs");Bounds bad;test(!SvgPathReader("M2 2 Lx").read(bad),"malformed path rejected");}
    // Services: strong title matches win; brand-only titles need to be unique.
    {auto one=[](std::vector<std::wstring> tabs,std::wstring title,std::wstring artist){return assignServices(tabs,{{title,artist}}).front();};
    test(one({L"Lo-fi beats - YouTube - Memory usage - 120 MB"},L"Lo-fi beats",L"")=="youtube","YouTube tab identified");
    test(one({L"Blinding Lights - YouTube Music"},L"Blinding Lights",L"The Weeknd")=="youtubemusic","YouTube Music before YouTube");
    test(one({L"Blinding Lights \u2022 The Weeknd"},L"Blinding Lights",L"The Weeknd")=="spotify","Spotify web player title pattern");
    test(one({L"Netflix"},L"Episode 3",L"")=="netflix","brand-only service when unique");
    test(one({L"Netflix",L"Prime Video: Home"},L"Episode 3",L"").empty(),"ambiguous brand-only titles give nothing");
    test(one({L"Inbox - Outlook"},L"Episode 3",L"").empty(),"unrelated tabs give nothing");
    // Reported: a JioHotstar tab and a YouTube tab. Each session gets its own site, never one twice.
    auto pair=assignServices({L"Saudi Arabia's $39B Factory Deal with China - YouTube",L"Watch The Night Manager Season 1 on JioHotstar"},{{L"Saudi Arabia's $39B Factory Deal with China",L"Uptin"},{L"The Night Manager",L""}});
    test(pair[0]=="youtube"&&pair[1]=="jiohotstar","YouTube and JioHotstar sessions each identified from their own tab");
    auto brandTab=assignServices({L"Saudi Arabia's $39B Factory Deal - YouTube",L"JioHotstar - Home"},{{L"Saudi Arabia's $39B Factory Deal",L"Uptin"},{L"S1 E4 \u00b7 Episode title",L""}});
    test(brandTab[0]=="youtube"&&brandTab[1]=="jiohotstar","brand-only tab claims the one unmatched session");
    // Reported: one YouTube tab, two sessions (a preview). The second must not borrow YouTube.
    auto preview=assignServices({L"Saudi Arabia's $39B Factory Deal - YouTube"},{{L"Saudi Arabia's $39B Factory Deal",L"Uptin"},{L"Inside Apple Park",L"Neo"}});
    test(preview[0]=="youtube"&&preview[1].empty(),"one YouTube tab never labels a second session");
    auto twoUnmatched=assignServices({L"Netflix",L"Something - YouTube"},{{L"Episode 3",L""},{L"Episode 4",L""}});
    test(twoUnmatched[0].empty()&&twoUnmatched[1].empty(),"brand-only never guesses between two unmatched sessions");
    auto sameTitle=assignServices({L"Lo-fi beats - YouTube",L"Lo-fi beats - YouTube"},{{L"Lo-fi beats",L""},{L"Lo-fi beats",L""}});
    test(sameTitle[0]=="youtube"&&sameTitle[1]=="youtube","two tabs with the same video each identify their session");
    test(assignServices({},{{L"Anything",L""}}).front().empty()&&assignServices({L"x"},{}).empty(),"empty inputs are safe");}
    test(appBrand(L"Spotify.exe")=="spotify"&&appBrand(L"vlc media player")=="vlcmediaplayer"&&appBrand(L"Notepad").empty(),"app fallback marks");
    for(auto& r:serviceRules)test(findBrand(r.slug)||r.monogram,"every service has a mark or a monogram");
    // This laptop's paired devices classify sensibly.
    test(deviceKind(0x240404,L"WH-1000XM4")==DeviceKind::Headphones&&deviceBrand(L"WH-1000XM4")=="sony","Sony headphones");
    test(deviceKind(0,L"Sam's Buds3 Pro")==DeviceKind::Earbuds&&deviceBrand(L"Sam's Buds3 Pro")=="samsung","Galaxy Buds");
    test(deviceKind(0,L"Stone 352 Pro")==DeviceKind::Speaker&&deviceBrand(L"Stone 352 Pro")=="boat"&&deviceBrand(L"PartyPal 400")=="boat","boAt speakers");
    test(deviceKind(0x5a020c,L"Sam's S23+")==DeviceKind::Phone&&deviceBrand(L"Sam's S23+")=="samsung","Galaxy phone");
    test(deviceKind(0x002508,L"Xbox Wireless Controller")==DeviceKind::Gamepad&&deviceKind(0,L"AULA-F75 5.0 KB")==DeviceKind::Keyboard,"controller and keyboard");
    test(deviceBrand(L"Philips TAS2400")=="philips"&&deviceBrand(L"TAS2400")=="philips"&&deviceKind(0,L"TAS2400")==DeviceKind::Other&&deviceBrand(L"Galaxy S24 Ultra")=="samsung","model numbers match only at word starts (TAS2400 is Philips, not Samsung)");
    test(deviceBrand(L"Tatiana's phone").empty()&&deviceBrand(L"Metas2400").empty(),"Philips codes need a word start and digits");
    test(deviceBrand(L"Headset",0x054c,2)=="sony"&&deviceBrand(L"Buds",0x0075,1)=="samsung"&&deviceBrand(L"HBTS001").empty(),"vendor IDs and unknowns");
    for(auto& d:{L"sony",L"samsung",L"boat",L"apple",L"bose",L"jbl"}){std::string slug;for(wchar_t c:std::wstring(d))slug+=char(c);test(findBrand(slug)!=nullptr,"device brand marks present");}
    {BluetoothDevice a;a.name=L"WH-1000XM4";BluetoothDevice b=a;b.connected=true;auto on=bluetoothChanges({a},{b});test(on.size()==1&&on[0].connected,"connection event");auto off=bluetoothChanges({b},{a});test(off.size()==1&&!off[0].connected,"disconnection event");
        test(bluetoothChanges({a},{a}).empty(),"no change no event");BluetoothDevice fresh;fresh.name=L"New";test(bluetoothChanges({},{fresh}).empty(),"newly paired but not connected is silent");test(bluetoothChanges({b},{}).size()==1,"removed while connected");}
    // Auto-hide: only the edge reveals; leaving hides after the delay.
    {AutoHide h;test(!h.update(true,false,false,true,0,.6),"visible while over island");test(!h.update(true,false,false,false,1,.6),"grace period");test(h.update(true,false,false,false,1.7,.6),"hides after delay");
        test(h.update(true,false,false,true,2,.6),"passing over the hidden spot does not reveal");test(!h.update(true,false,true,false,2.1,.6),"touching the edge reveals");
        h.update(true,false,false,false,3,.6);h.update(true,false,false,false,4,.6);test(h.hidden&&!h.update(true,true,false,false,4.1,.6),"expanded or alerting island is shown");test(!h.update(false,false,false,false,9,.6),"disabled never hides");}
    test(atIslandEdge(960,0,0,0,1920,1080,0,960,160)&&!atIslandEdge(960,5,0,0,1920,1080,0,960,160)&&!atIslandEdge(400,0,0,0,1920,1080,0,960,160),"top edge band");
    test(atIslandEdge(1919,540,0,0,1920,1080,1,540,130)&&!atIslandEdge(1900,540,0,0,1920,1080,1,540,130),"right edge band");
    // Battery estimates never extrapolate from relative units.
    {BatteryReading r;r.present=true;r.charging=true;r.fullMwh=75949;r.remainingMwh=60000;r.rateMw=45000;BatteryEstimate e;e.observe(r);test(e.minutesToFull(r)==21&&e.minutesRemaining(r)==-1,"time to full");
        r.charging=false;r.rateMw=-15000;e.observe(r);test(e.minutesRemaining(r)==240&&e.minutesToFull(r)==-1,"time remaining");r.relative=true;e.observe(r);test(e.minutesRemaining(r)==-1,"relative units give no estimate");
        BatteryReading h;h.designMwh=90001;h.fullMwh=75949;test(std::abs(h.health()-.8439)<.001,"health from capacities");h.designMwh=0;test(h.health()<0,"unknown design capacity");
        test(durationText(65)==L"1 h 5 min"&&durationText(20)==L"20 min"&&durationText(-1)==L"—","durations");}
    {BatteryHistory h;for(int i=0;i<3000;++i)h.add(i*300,50+i%50,i%2);test(h.samples.size()<=BatteryHistory::limit,"history bounded");test(!h.add(h.samples.back().time+10,40,false),"five-minute spacing");
        std::stringstream io;h.write(io);auto back=BatteryHistory::read(io);test(back.samples.size()==h.samples.size()&&back.samples.back().percent==h.samples.back().percent,"history round trip");std::stringstream bad("battery_history 1\n5 50 0\n3 40 1\n");test(BatteryHistory::read(bad).samples.empty(),"out-of-order history rejected");}
    {Spectrum sp;std::vector<float> block(480,.1f);auto start=std::chrono::steady_clock::now();for(int i=0;i<2000;++i){sp.push(block.data(),480);sp.analyze(48000,.01f);}
        double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/2000;std::cout<<"Spectrum analysis: "<<us<<" us per 1024-point step (100 steps/s while audio plays)\n";test(us<2000,"analysis fits comfortably in its 10 ms cadence");}
    // ---- Phase 4: command language -------------------------------------------------
    {const std::vector<InstalledApp> apps{{L"Microsoft Edge",L"MSEdge"},{L"Visual Studio Code",L"Microsoft.VisualStudioCode"},{L"Spotify",L"SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify"},{L"Uninstall Spotify",L"u"},{L"Calculator",L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"}};
    const std::vector<std::wstring> saved{L"Study",L"Games"};const std::wstring scope=L"C:\\Users\\Sam";
    auto first=[&](const std::wstring& t){auto r=parseCommand(t,apps,saved,scope);return r.empty()?CommandResult{}:r.front();};
    test(parseCommand(L"   ",apps,saved,scope).empty(),"empty command has no results");
    test(first(L"volume 30").kind==CommandKind::Volume&&first(L"volume 30").value==30&&first(L"Set volume to 45%").value==45,"volume levels");
    test(first(L"volume up").kind==CommandKind::VolumeStep&&first(L"volume up").value==10&&first(L"quieter").value==-10,"volume steps");
    test(first(L"volume 130").kind==CommandKind::None&&first(L"volume").kind==CommandKind::None,"out-of-range and bare volume do nothing");
    test(first(L"mute mic").kind==CommandKind::MicMute&&first(L"Mic Off").kind==CommandKind::MicMute&&first(L"unmute microphone").kind==CommandKind::MicUnmute&&first(L"mic").kind==CommandKind::MicToggle&&first(L"mute").kind==CommandKind::Mute,"microphone words");
    test(first(L"mute").kind==CommandKind::Mute&&first(L"unmute").kind==CommandKind::Unmute&&first(L"PLAY").kind==CommandKind::Play&&first(L"skip").kind==CommandKind::Next&&first(L"prev").kind==CommandKind::Previous,"playback words");
    test(first(L"timer 10 min").value==600&&first(L"timer 90s").value==90&&first(L"timer 1 hour").value==3600&&first(L"timer for 5").value==300,"timer durations");
    test(first(L"focus").value==1500&&first(L"break").value==300&&first(L"focus 50").value==3000&&first(L"break 10").target==L"break","focus and break defaults");
    test(first(L"timer 10 min extra").kind!=CommandKind::Timer&&first(L"timer 13h").kind!=CommandKind::Timer&&first(L"timer 0").kind!=CommandKind::Timer,"malformed timers are not accepted");
    test(first(L"stopwatch").kind==CommandKind::Stopwatch&&first(L"stop timer").kind==CommandKind::StopTimer&&first(L"timer cancel").kind==CommandKind::StopTimer,"stopwatch and stop");
    test(first(L"open edge").kind==CommandKind::OpenApp&&first(L"open edge").target==L"MSEdge"&&first(L"edge").appId==L"MSEdge","apps by name");
    test(first(L"vsc").target==L"Microsoft.VisualStudioCode"&&first(L"calc").target.ends_with(L"!App"),"apps by initials and prefix");
    {auto r=parseCommand(L"spotify",apps,saved,scope);test(r.size()==1&&r[0].target.ends_with(L"!Spotify"),"uninstallers are never offered");}
    test(first(L"open zzzz").kind==CommandKind::None&&first(L"zzzz").kind==CommandKind::None,"unknown text does nothing");
    {auto r=first(L"find budget pdfs from last month");test(r.kind==CommandKind::SearchFiles&&r.target.starts_with(L"search-ms:query=")&&r.target.find(L"ext%3A.pdf")!=std::wstring::npos&&r.target.find(L"datemodified%3Alast%20month")!=std::wstring::npos&&r.target.ends_with(L"crumb=location:C%3A%5CUsers%5CSam"),"file search URI with filters and scope");}
    {auto r=first(L"search a&b;c|d \"e\"");test(r.kind==CommandKind::SearchFiles&&r.target.find_first_of(L"&;|\" ",16)==r.target.find(L"&crumb"),"typed search text is percent-encoded, never raw");}
    {auto q=fileQuery(L"report pdfs from last week");test(q.words==L"report"&&q.aqs==L"report datemodified:last week ext:.pdf","file query words and filters");auto p=fileQuery(L"photos today");test(p.words.empty()&&p.aqs==L"datemodified:today kind:picture","filters only");}
    test(percentEncode(L"caf\u00e9 \U0001F600")==L"caf%C3%A9%20%F0%9F%98%80","UTF-8 percent encoding");
    test(first(L"bluetooth settings").target==L"ms-settings:bluetooth"&&first(L"settings display").target==L"ms-settings:display"&&first(L"settings").target==L"ms-settings:","Settings pages");
    test(first(L"focus settings").target==L"ms-settings:quiethours","a Settings page wins over a malformed timer");
    test(first(L"save workspace Deep Work").kind==CommandKind::SaveWorkspace&&first(L"save workspace Deep Work").target==L"Deep Work"&&first(L"save workspace").kind==CommandKind::None,"save workspace");
    test(first(L"workspace st").kind==CommandKind::Workspace&&first(L"workspace st").target==L"Study"&&first(L"workspace st").confirm&&first(L"study").kind==CommandKind::Workspace,"open workspace by prefix or name, with confirmation");
    test(parseCommand(L"workspaces",apps,saved,scope).size()==2&&first(L"delete workspace games").kind==CommandKind::DeleteWorkspace&&first(L"delete workspace nope").kind==CommandKind::None,"list and delete workspaces");
    test(first(L"clipboard").kind==CommandKind::Clipboard&&first(L"clear clipboard").kind==CommandKind::ClearClipboard&&first(L"lock").kind==CommandKind::Lock,"clipboard and lock");
    for(auto t:{L"open calc & del *",L"run cmd /c format",L"powershell -enc AAAA"}){for(auto& r:parseCommand(t,apps,saved,scope))test(r.kind==CommandKind::None||r.kind==CommandKind::OpenApp,"nothing typed becomes a shell command");}
    test(appScore(L"Visual Studio Code",L"code")==70&&appScore(L"Calculator",L"calculator")==100&&appScore(L"Uninstall Tool",L"uninstall")==0,"app scores");}
    // ---- Phase 4: clipboard model ---------------------------------------------------
    {test(isLink(L" https://example.com/a?b=1 ")&&isLink(L"www.example.org")&&isLink(L"mailto:sam@example.com")&&!isLink(L"https://")&&!isLink(L"see https://example.com")&&!isLink(L"www.nodot"),"link detection");
    test(clipPreview(L"  one\n\n two\t three  ")==L"one two three"&&clipPreview(std::wstring(300,L'a'),10).size()==11&&clipPreview(L" \n\t ").empty(),"preview collapses whitespace and shortens");
    test(privateSource(L"C:\\Program Files\\KeePassXC\\KeePassXC.exe")&&privateSource(L"1Password.exe")&&privateSource(L"Bitwarden")&&!privateSource(L"notepad.exe"),"password managers are private sources");
    ClipboardHistory h;auto text=[](std::wstring t){ClipEntry e;e.text=std::move(t);return e;};
    test(h.add(text(L"alpha"),1)&&h.add(text(L"beta"),2)&&h.entries().front().text==L"beta","newest first");
    auto alphaId=h.entries().back().id;h.add(text(L"alpha"),3);test(h.entries().size()==2&&h.entries().front().text==L"alpha"&&h.entries().front().id==alphaId&&h.entries().front().time==3,"copying again moves to the top, keeping its id");
    test(!h.add(text(L"   \n "),4)&&h.entries().size()==2,"blank copies are ignored");
    h.paused=true;test(!h.add(text(L"gamma"),5)&&h.entries().size()==2,"paused history keeps nothing");h.paused=false;
    for(int i=0;i<40;++i)h.add(text(L"item "+std::to_wstring(i)),10+i);test(h.entries().size()==ClipboardHistory::limit&&h.entries().front().text==L"item 39","bounded to 24 entries");
    {ClipEntry big;big.kind=ClipEntry::Kind::Image;big.dib.resize(ClipboardHistory::memoryLimit+1);test(!h.add(std::move(big),60),"oversized images are refused");}
    {ClipboardHistory m;for(int i=0;i<5;++i){ClipEntry e;e.kind=ClipEntry::Kind::Image;e.dib.assign(ClipboardHistory::memoryLimit/3,uint8_t(i));m.add(std::move(e),i);}size_t total=0;for(auto& e:m.entries())total+=e.bytes();test(total<=ClipboardHistory::memoryLimit&&m.entries().front().dib[0]==4,"memory bound drops the oldest");}
    auto id=h.entries()[3].id;h.remove(id);test(!h.find(id)&&h.entries().size()==ClipboardHistory::limit-1,"remove by id");h.clear();test(h.entries().empty(),"clear");
    test(ageText(10)==L"Just now"&&ageText(240)==L"4 min"&&ageText(7300)==L"2 h"&&ageText(3*86400+5)==L"3 d","ages");}
    // ---- Phase 4: workspaces ---------------------------------------------------------
    {WorkspaceStore w;test(w.save({L"Study",{{L"Edge",L"MSEdge",true},{L"Notes",L"C:\\Tools\\notes.exe",false}}})&&w.find(L"study")&&w.index(L"STUDY")==0,"save and case-insensitive find");
    test(w.save({L"study",{{L"Edge",L"MSEdge",true}}})&&w.list().size()==1&&w.list()[0].apps.size()==1,"same name replaces");
    for(int i=0;i<10;++i)w.save({L"W"+std::to_wstring(i),{}});test(w.list().size()==WorkspaceStore::maxWorkspaces&&!w.save({L"Extra",{}}),"at most 8 workspaces");
    {Workspace many{L"Big",{}};for(int i=0;i<20;++i)many.apps.push_back({L"a",L"t"+std::to_wstring(i),false});WorkspaceStore b;b.save(many);test(b.list()[0].apps.size()==WorkspaceStore::maxApps,"at most 12 apps");}
    test(w.remove(L"STUDY")&&!w.find(L"Study")&&!w.remove(L"missing"),"remove");
    WorkspaceStore u;u.save({L"Caf\u00e9 \U0001F3B5\tnight",{{L"M\u00fcsic",L"C:\\M\u00fcsic\\app.exe",false},{L"Edge",L"MSEdge",true}}});std::stringstream io;u.write(io);auto back=WorkspaceStore::read(io);
    test(back.list().size()==1&&back.list()[0].name==L"Caf\u00e9 \U0001F3B5 night"&&back.list()[0].apps==u.list()[0].apps,"round trip with Unicode (tabs become spaces)");
    std::stringstream bad("workspaces 1\nworkspace\tA\napp\t2\tx\ty\napp\t1\tEdge\tMSEdge\nworkspace\ta\napp\t1\tLeak\tL\nnonsense\n");auto r=WorkspaceStore::read(bad);test(r.list().size()==1&&r.list()[0].apps.size()==1&&r.list()[0].apps[0].target==L"MSEdge","malformed lines skipped; duplicate names never take apps");
    std::stringstream wrong("workspaces 9\nworkspace\tA\n");test(WorkspaceStore::read(wrong).list().empty(),"unknown versions are ignored");
    test(appSummary({{L"Edge",L"e",true}})==L"Edge"&&appSummary({{L"Edge",L"e",true},{L"Spotify",L"s",true}})==L"Edge and Spotify"&&appSummary({{L"A",L"1"},{L"B",L"2"},{L"C",L"3"},{L"D",L"4"},{L"E",L"5"}})==L"A, B, C and 2 more","app summaries");}
    // ---- Phase 4: privacy records -----------------------------------------------------
    {test(consentPath(L"C:#Program Files#App#app.exe")==L"C:\\Program Files\\App\\app.exe","classic program keys decode to paths");
    std::set<std::wstring> paths{L"c:\\program files\\app\\app.exe"},families{L"microsoft.windowscamera_8wekyb3d8bbwe"};
    test(inUse({Capability::Camera,L"C:#Program Files#App#app.exe",false,5,0},paths,families),"running classic app in use");
    test(!inUse({Capability::Camera,L"C:#Program Files#App#app.exe",false,5,9},paths,families)&&!inUse({Capability::Camera,L"C:#Other#x.exe",false,5,0},paths,families),"stopped or not running is not in use");
    test(inUse({Capability::Microphone,L"Microsoft.WindowsCamera_8wekyb3d8bbwe",true,5,0},paths,families)&&!inUse({Capability::Microphone,L"Microsoft.WindowsCamera_8wekyb3d8bbwe",true,0,0},paths,families),"packaged apps by family name; never-started is not in use");}
    // ---- Phase 4: settings v8 --------------------------------------------------------
    {std::stringstream v7("version 7\nautoHide 1\n");auto old=Settings::parse(v7);test(!old.clipboardHistory&&old.privacyDots&&old.privacyCards&&old.commandShortcut==1&&old.clipboardConfirm,"v7 files get the v8 defaults (clipboard history off)");
    Settings changed;changed.clipboardHistory=true;changed.commandShortcut=3;std::stringstream out;changed.write(out);auto again=Settings::parse(out);test(again==changed,"v8 round trip");}
    // ---- Phase 5: learned waveform -------------------------------------------------------
    {TrackWaveform w;test(w.heard()==0&&w.heights()[0]==-1.f,"nothing heard is unknown, never invented");
    test(w.hear(-1,100,.5f)==-1&&w.hear(101,100,.5f)==-1&&w.hear(10,0,.5f)==-1&&w.hear(std::nan(""),100,.5f)==-1&&w.hear(10,100,std::nanf(""))==-1,"bad samples ignored");
    test(w.hear(0,100,.5f)==0&&w.hear(100,100,.5f)==TrackWaveform::bars-1&&w.hear(50,100,.25f)==32,"positions map to bars");
    w.hear(50,100,.75f);auto h=w.heights();test(std::abs(h[0]-1)<1e-5f&&std::abs(h[32]-1)<1e-5f&&h[1]==-1.f&&w.heard()==3,"heights normalise to the loudest mean");
    TrackWaveform quiet;quiet.hear(0,10,.05f);test(std::abs(quiet.heights()[0]-.25f)<1e-5f,"a near-silent track is not blown up to full height");
    TrackWaveform loud;loud.hear(0,10,7.f);test(loud.heights()[0]==1.f,"levels clamp");
    auto k=WaveformLibrary::key(L"Song",L"Artist",200.4);test(k==WaveformLibrary::key(L"Song",L"Artist",199.6)&&k!=WaveformLibrary::key(L"Song",L"Other",200),"track keys");
    WaveformLibrary lib;lib.track(k).hear(1,200,.5f);for(size_t i=0;i<WaveformLibrary::limit-1;++i)lib.track(L"t"+std::to_wstring(i));lib.track(k);lib.track(L"one more");
    test(lib.find(k)&&lib.find(k)->heard()==1&&!lib.find(L"t0")&&lib.find(L"t1"),"recently used tracks are kept; the oldest is forgotten");}
    // ---- Phase 5: motion lab springs -----------------------------------------------------
    {Settings s;for(int p=0;p<5;++p){s.preset=p;auto a=bodySpring(s),b=preset(MotionPreset(p));test(a.mass==b.mass&&a.stiffness==b.stiffness&&a.damping==b.damping,"named presets unchanged");}
    s.preset=5;s.springMass=80;s.springStiffness=700;s.springDamping=50;auto c=bodySpring(s);test(c.mass==.8&&c.stiffness==700&&c.damping==50,"custom spring");
    s.labSpeed=2;auto slow=bodySpring(s);test(slow.stiffness==700./16&&slow.damping==50./4&&slow.mass==.8,"slow motion keeps the curve's shape");
    auto items=settingItems();auto find=[&](const char* key)->const SettingItem&{for(auto& i:items)if(i.key==key)return i;throw std::runtime_error(key);};
    Settings t;find("preset").set(t,1);test(find("springStiffness").get(t)==360&&find("springDamping").get(t)==35&&find("springMass").get(t)==115,"sliders show the chosen preset");
    find("springDamping").set(t,20);test(t.preset==5&&t.springDamping==20&&t.springStiffness==360&&t.springMass==115,"moving a slider makes a custom spring from the preset");}
    // ---- Phase 5: wallpaper accent ----------------------------------------------------------
    {auto channel=[](uint32_t c,int k){return int((c>>(16-8*k))&255);};
    auto grey=pastelAccent(.5,.5,.5);test(channel(grey,0)==channel(grey,1)&&channel(grey,1)==channel(grey,2),"grey wallpapers give a neutral accent");
    auto red=pastelAccent(1,0,0),blue=pastelAccent(.1,.3,.9);test(channel(red,0)>channel(red,1)+40&&channel(blue,2)>channel(blue,0)+40,"hue is kept");
    for(auto c:{red,blue,grey,pastelAccent(0,0,0),pastelAccent(1,1,1)})test(channel(c,0)+channel(c,1)+channel(c,2)>3*150,"always light enough to read on the dark island");
    test(islandAccent(4,false,0x123456,nullptr,false)==0x123456&&islandAccent(4,false,0,nullptr,false)==0xa4deca&&islandAccent(9,false,0,nullptr,false)==0xefc7a6&&islandAccent(4,true,0x123456,nullptr,false)==0x487467,"accent swatches");}
    // ---- Phase 5: settings v9 ------------------------------------------------------------
    {std::stringstream v8("version 8\npreset 3\n");auto old=Settings::parse(v8);test(old.version==Settings::currentVersion&&old.preset==3&&old.waveTimeline&&old.springStiffness==390&&old.labSpeed==0,"v8 files get the v9 defaults");
    Settings changed;changed.preset=5;changed.springStiffness=820;changed.springDamping=14;changed.springMass=60;changed.waveTimeline=false;changed.accent=4;std::stringstream out;changed.write(out);auto again=Settings::parse(out);test(again==changed,"v9 round trip");
    changed.labSpeed=2;std::stringstream out2;changed.write(out2);test(Settings::parse(out2).labSpeed==0,"slow motion is never saved");
    std::stringstream wild("version 9\nspringStiffness 5000\nspringDamping 1\naccent 7\n");auto w=Settings::parse(wild);test(w.springStiffness==900&&w.springDamping==12&&w.accent==4,"v9 values are bounded");}
    // ---- Phase 5B: UTF-8 and JSON --------------------------------------------------------------
    {const std::wstring mixed=L"Caf\u00e9 \U0001F3B5 \u0928\u092e\u0938\u094d\u0924\u0947";test(fromUtf8(toUtf8(mixed))==mixed,"UTF-8 round trip");
    test(fromUtf8("a\xff")==L"a\ufffd"&&fromUtf8("\xe2\x82")==L"\ufffd\ufffd"&&fromUtf8("\xc0\xaf")==L"\ufffd\ufffd","invalid UTF-8 becomes U+FFFD");
    auto j=Json::parse(R"([{"trackName":"Caf\u00e9 \ud83c\udfb5","duration":-1.5e2,"instrumental":true,"syncedLyrics":null,"x":{"y":[1,2,{"z":"a\"b\\c\n"}]}}])");
    test(j&&j->type==Json::Type::Array&&j->items.size()==1,"JSON array of objects");
    if(j){auto& o=j->items[0];test(fromUtf8(o.string("trackName"))==L"Caf\u00e9 \U0001F3B5"&&o.num("duration")==-150&&o.flag("instrumental")&&o.string("syncedLyrics").empty()&&o.find("x")->find("y")->items[2].string("z")=="a\"b\\c\n","JSON escapes, numbers, nesting");}
    for(auto bad:{"[1,]","{\"a\":}","tru","\"open","[1] x","{\"a\" 1}","01x","[\"\x01\"]"})test(!Json::parse(bad),"malformed JSON rejected");
    test(!Json::parse(std::string(40,'[')+std::string(40,']')),"JSON nesting limit");test(Json::parse(" [ ] ")&&Json::parse("{}"),"empty containers");}
    // ---- Phase 5B: LRC and timing ----------------------------------------------------------------
    {auto lines=parseLrc(L"[ar:Someone]\r\n[00:13.42] Yeah\r\n[00:14.81] \n[00:26.95][01:02.00] Line two\nuntimed words\n[offset:+500]\n[00:05:50]Colon");
    test(lines.size()==5,"timed lines only, one per time tag");
    test(std::abs(lines[0].time-5.0)<1e-9&&lines[0].text==L"Colon"&&std::abs(lines[1].time-12.92)<1e-9&&lines[1].text==L"Yeah"&&lines[2].text.empty()&&std::abs(lines[4].time-61.5)<1e-9&&lines[4].text==L"Line two","offset applies to every line; sorted; trimmed");
    test(lyricIndex(lines,1)==-1&&lyricIndex(lines,12.92)==1&&lyricIndex(lines,20)==2&&lyricIndex(lines,999)==4,"current line");
    test(std::abs(nextLyricIn(lines,20)-6.45)<1e-9&&nextLyricIn(lines,70)<0,"next line countdown");
    test(parseLrc(L"").empty()&&parseLrc(L"[xx:yy]nope\n[00:61.00]bad seconds").empty(),"garbage is ignored");}
    // ---- Phase 5B: search query cleanup -----------------------------------------------------------
    {auto q=[](const wchar_t* t,const wchar_t* a,bool browser){return cleanLyricsQuery(t,a,browser);};
    test(q(L"The Weeknd - Blinding Lights (Official Video)",L"The Weeknd",true)==LyricsQuery{L"Blinding Lights",L"The Weeknd"},"YouTube title split and cleaned");
    test(q(L"Blinding Lights",L"The Weeknd",false)==LyricsQuery{L"Blinding Lights",L"The Weeknd"},"clean player fields unchanged");
    test(q(L"Something - Remastered 2009",L"The Beatles",false)==LyricsQuery{L"Something",L"The Beatles"},"remaster suffix dropped");
    test(q(L"Video Games",L"Lana Del Rey",false)==LyricsQuery{L"Video Games",L"Lana Del Rey"}&&q(L"Lana Del Rey - Video Games",L"LanaDelReyVEVO",true)==LyricsQuery{L"Video Games",L"Lana Del Rey"},"titles that look like noise survive");
    test(q(L"Live Forever",L"Oasis - Topic",true)==LyricsQuery{L"Live Forever",L"Oasis"},"topic channels");
    test(q(L"Levitating (feat. DaBaby)",L"Dua Lipa",false)==LyricsQuery{L"Levitating",L"Dua Lipa"}&&q(L"Stay ft. Justin Bieber",L"The Kid LAROI",false).title==L"Stay","featured artists dropped");
    test(q(L"\u201cHello\u201d",L"Adele",false).title==L"Hello"&&q(L"Kesariya [Lyrical Video]",L"Arijit Singh",false).title==L"Kesariya","quotes and bracket noise");
    test(primaryArtist(L"Post Malone, Swae Lee")==L"Post Malone"&&primaryArtist(L"Calvin Harris feat. Rihanna")==L"Calvin Harris"&&primaryArtist(L"Adele")==L"Adele","primary artist");
    test(urlEncode(toUtf8(L"Caf\u00e9 & Co"))=="Caf%C3%A9%20%26%20Co"&&lyricsSearchPath({L"A B",L"C"})=="/api/search?track_name=A%20B&artist_name=C","query string");}
    // ---- Phase 5B: choosing results --------------------------------------------------------------
    {auto results=Json::parse(R"([{"duration":248,"syncedLyrics":"[00:01.00]long"},{"duration":202,"syncedLyrics":null,"plainLyrics":"x"},{"duration":203,"syncedLyrics":"[00:01.00]A"},{"duration":200,"syncedLyrics":"[00:01.00]B"},{"duration":202,"instrumental":true}])");
    test(results.has_value(),"results parse");
    if(results){auto a=pickLyrics(*results,202.4);test(a.kind==LyricsResult::Kind::Synced&&a.lrc==L"[00:01.00]A","closest synced version by length");
        test(pickLyrics(*results,230).kind==LyricsResult::Kind::None,"no version within 10 s means no lyrics, not wrong ones");
        test(pickLyrics(*results,0).lrc==L"[00:01.00]long","unknown length takes the first synced result");}
    auto instrumental=Json::parse(R"([{"duration":180,"instrumental":true,"syncedLyrics":null}])");test(instrumental&&pickLyrics(*instrumental,181).kind==LyricsResult::Kind::Instrumental,"instrumental tracks");
    auto empty=Json::parse("[]");test(empty&&pickLyrics(*empty,100).kind==LyricsResult::Kind::None&&pickLyrics(Json{},100).kind==LyricsResult::Kind::None,"nothing found");}
    // ---- Phase 5B: lyrics cache -----------------------------------------------------------------
    {LyricsCacheEntry found{LyricsCacheEntry::Status::Found,1700000000,L"[00:01.00]Caf\u00e9 \U0001F3B5\n[00:02.00]Two"};auto back=readLyricsCache(writeLyricsCache(found));test(back&&*back==found,"found entries round trip");
    LyricsCacheEntry missing{LyricsCacheEntry::Status::Missing,1000,L""};auto m=readLyricsCache(writeLyricsCache(missing));test(m&&*m==missing&&lyricsCacheFresh(*m,1000+13*86400)&&!lyricsCacheFresh(*m,1000+15*86400)&&lyricsCacheFresh(found,found.time+400*86400),"missing entries expire after 14 days");
    test(!readLyricsCache("nonsense")&&!readLyricsCache("arnav-lyrics 1\nstatus found\ntime 5\nno timestamps")&&!readLyricsCache("arnav-lyrics 1\nstatus weird\ntime 5\n"),"corrupt cache files ignored");
    test(lyricsCacheName({L"Song",L"Artist"},200.2)==lyricsCacheName({L"SONG",L"artist"},199.8)&&lyricsCacheName({L"Song",L"Artist"},200)!=lyricsCacheName({L"Song",L"Artist"},230)&&lyricsCacheName({L"a",L"b"},1).size()==20,"cache names");}
    // ---- Phase 5B: artwork palette --------------------------------------------------------------
    {auto image=[](auto color){std::vector<uint8_t> px(64*64*4);for(int i=0;i<64*64;++i){uint32_t c=color(i%64,i/64);px[i*4]=uint8_t(c&255);px[i*4+1]=uint8_t((c>>8)&255);px[i*4+2]=uint8_t(c>>16);px[i*4+3]=255;}return px;};
    auto ch=[](uint32_t c,int k){return int((c>>(16-8*k))&255);};auto sum=[&](uint32_t c){return ch(c,0)+ch(c,1)+ch(c,2);};
    auto red=image([](int,int){return 0xd02020u;});auto p=artPalette(red.data(),red.size()/4);
    test(p.colourful&&ch(p.accent,0)>ch(p.accent,1)+40&&sum(p.accent)>450&&sum(p.deep)<sum(p.accent)-150&&ch(p.ambient,0)>ch(p.ambient,2),"a red cover gives a light red accent and a deep red for light islands");
    auto split=image([](int x,int){return x<40?0xe03030u:0x2050e0u;});auto q=artPalette(split.data(),split.size()/4);test(ch(q.accent,0)>ch(q.accent,2)&&ch(q.secondary,2)>ch(q.secondary,0),"the second hue becomes the secondary colour");
    auto grey=image([](int x,int y){uint32_t v=40+(x+y);return (v<<16)|(v<<8)|v;});auto g=artPalette(grey.data(),grey.size()/4);test(!g.colourful&&std::abs(ch(g.accent,0)-ch(g.accent,2))<8,"grey covers stay neutral");
    auto speck=image([](int x,int y){return x<3&&y<3?0xff0000u:0x707070u;});test(!artPalette(speck.data(),speck.size()/4).colourful,"a stray speck of colour is ignored");
    std::vector<uint8_t> clear(16*4,0);test(!artPalette(clear.data(),16).colourful&&artPalette(nullptr,0).accent==ArtPalette{}.accent,"transparent or empty art");
    test(mixColor(0x000000,0xffffff,.5)==0x808080&&mixColor(0x102030,0x405060,0)==0x102030&&mixColor(0x102030,0x405060,2)==0x405060,"colour mixing");
    Artwork art;art.pixels=red;art.width=art.height=64;paintArtwork(art);test(islandAccent(0,false,0,&art,true)==art.accent&&islandAccent(0,true,0,&art,true)==art.deep&&islandAccent(0,true,0,&art,false)==0x487467,"light islands use the artwork's deep shade");}
    // ---- Phase 5B: seeking detents and skips -------------------------------------------------------
    {test(detentStep(200,380)==10&&detentStep(3600,380)==120&&detentStep(0,380)==0&&detentStep(1e9,380)==0,"detent spacing stays at least 12 DIPs");
    test(seekDetents(35,380,{12.5,0.1,34.9})==std::vector<double>{10,12.5,20,30},"10 s marks plus lyric lines, away from the ends");
    bool snapped=false;test(snapToDetent(19.2,{10,20,30},1,&snapped)==20&&snapped&&snapToDetent(15,{10,20,30},1,&snapped)==15&&!snapped&&snapToDetent(5,{},1)==5,"snapping within tolerance only");
    ScrubGesture g;g.begin(190,380,0,200);test(g.value==100&&g.raw==100,"scrub begins at the pointer");g.move(228,0,380,0,200);test(std::abs(g.raw-120)<1e-9&&g.value==g.raw&&g.precision==1,"raw follows the pointer");g.move(266,80,380,0,200);test(g.precision==.12&&std::abs(g.raw-122.4)<1e-9,"pulling away slows the pointer");
    test(skipTarget(5,-10,0,200)==0&&skipTarget(195,10,0,200)==200&&skipTarget(50,10,0,0)==50&&skipTarget(50,-10,0,200)==40,"double-click skips stay in range");}
    // ---- Phase 5B: audio routes and the app under the logo ------------------------------------------
    {test(!isHeadphoneOutput(L"Speakers (Realtek(R) Audio)",FormSpeakers)&&isHeadphoneOutput(L"Headphones (WH-1000XM5)",FormHeadphones)&&isHeadphoneOutput(L"Galaxy Buds3 Pro",FormUnknown)&&!isHeadphoneOutput(L"Galaxy Buds3 Pro",FormSpeakers)&&isHeadphoneOutput(L"Headset (AirPods Hands-Free)",FormHeadset)&&!isHeadphoneOutput(L"DELL U2720Q",FormDisplay),"headphone outputs");
    std::vector<MixerName> apps{{L"Microsoft Edge",true},{L"Spotify",false},{L"System sounds",true,true},{L"Spotify Widget",true}};
    test(mixerIndexFor(L"Spotify",apps)==1&&mixerIndexFor(L"Edge",apps)==0&&mixerIndexFor(L"System sounds",apps)==-1&&mixerIndexFor(L"",apps)==-1&&mixerIndexFor(L"VLC media player",apps)==-1,"media app to mixer entry");}
    // ---- Phase 5B: output names and settings v10 ---------------------------------------------------
    {test(outputDisplayName(L"Headphones (WH-1000XM5)")==L"WH-1000XM5"&&outputDisplayName(L"Speakers (Realtek(R) Audio)")==L"Speakers"&&outputDisplayName(L"Headset (Galaxy Buds3 Pro Hands-Free)")==L"Galaxy Buds3 Pro Hands-Free"&&outputDisplayName(L"DELL U2720Q (NVIDIA High Definition Audio)")==L"DELL U2720Q (NVIDIA High Definition Audio)"&&outputDisplayName(L"Earbuds")==L"Earbuds","output display names");
    std::stringstream v9("version 9\nwaveTimeline 0\n");auto old=Settings::parse(v9);test(old.version==Settings::currentVersion&&!old.lyrics&&old.lyricsCompact&&old.headphoneCards&&!old.waveTimeline,"v9 files get the v10 defaults (lyrics off)");
    Settings changed;changed.lyrics=true;changed.lyricsCompact=false;changed.headphoneCards=false;std::stringstream out;changed.write(out);test(Settings::parse(out)==changed,"v10 round trip");}
    // ---- Phase 5C: deflate and ZIP -----------------------------------------------------------------
    {std::mt19937 rng(7);std::vector<uint8_t> text,noise(200000),empty;const char* words[]={"island ","shelf ","capture ","colour ","text ","the ","and "};for(int i=0;i<60000;++i)for(const char* c=words[rng()%7];*c;++c)text.push_back(uint8_t(*c));for(auto& b:noise)b=uint8_t(rng());
    auto packed=zip::deflate(text.data(),text.size());test(inflate(packed)==text&&packed.size()*3<text.size(),"deflate round trip; text shrinks at least 3x");
    auto raw=zip::deflate(noise.data(),noise.size());test(inflate(raw)==noise&&raw.size()<noise.size()+noise.size()/100,"random data is stored, not expanded");
    test(inflate(zip::deflate(empty.data(),0)).empty(),"empty input");
    {std::vector<uint8_t> out;zip::Deflater d(out);for(size_t at=0;at<text.size();at+=777)d.add(text.data()+at,std::min<size_t>(777,text.size()-at));d.finish();test(inflate(out)==text,"streamed chunks give the same data");}
    std::vector<uint8_t> runs(300000,'a');test(inflate(zip::deflate(runs.data(),runs.size()))==runs,"long runs (maximum-length matches, sliding window)");
    const uint8_t check[]={'1','2','3','4','5','6','7','8','9'};test(zip::crc32(0,check,9)==0xcbf43926u,"CRC-32 check value");
    struct Memory:zip::Sink{std::vector<uint8_t> b;bool write(const uint8_t* p,size_t n)override{b.insert(b.end(),p,p+n);return true;}uint64_t tell()override{return b.size();}bool patch(uint64_t at,const uint8_t* p,size_t n)override{if(at+n>b.size())return false;std::copy(p,p+n,b.begin()+std::ptrdiff_t(at));return true;}} sink;
    {zip::Writer w(sink);auto feed=[](const std::vector<uint8_t>& data){size_t at=0;return [&data,at](uint8_t* buf,size_t cap)mutable->long long{size_t n=std::min(cap,data.size()-at);std::copy(data.begin()+std::ptrdiff_t(at),data.begin()+std::ptrdiff_t(at+n),buf);at+=n;return (long long)n;};};
        test(w.add(toUtf8(L"caf\u00e9 notes.txt"),zip::dosTime(9,15,30),zip::dosDate(2026,9,24),feed(text))&&w.add("folder/noise.bin",0,zip::dosDate(1970,1,1),feed(noise))&&w.finish(),"archive written");
        auto u16=[&](size_t at){return uint32_t(sink.b[at]|(sink.b[at+1]<<8));};auto u32=[&](size_t at){return u16(at)|(u16(at+2)<<16);};
        const size_t end=sink.b.size()-22;test(u32(end)==0x06054b50&&u16(end+10)==2,"end record lists two entries");
        size_t cd=u32(end+16),at=cd;bool ok=true;std::vector<std::vector<uint8_t>> originals{text,noise};
        for(int k=0;k<2;++k){ok=ok&&u32(at)==0x02014b50&&u16(at+8)==0x0800&&u16(at+10)==8;const uint32_t crc=u32(at+16),csize=u32(at+20),usize=u32(at+24),nameLen=u16(at+28),local=u32(at+42);
            const size_t data=local+30+u16(local+26);std::vector<uint8_t> body(sink.b.begin()+std::ptrdiff_t(data),sink.b.begin()+std::ptrdiff_t(data+csize));auto back=inflate(body);
            ok=ok&&u32(local)==0x04034b50&&u32(local+14)==crc&&back==originals[size_t(k)]&&usize==back.size()&&zip::crc32(0,back.data(),back.size())==crc;
            if(k==0)ok=ok&&std::string(sink.b.begin()+std::ptrdiff_t(at+46),sink.b.begin()+std::ptrdiff_t(at+46+nameLen))==toUtf8(L"caf\u00e9 notes.txt");at+=46+nameLen;}
        test(ok&&u32(end+12)==end-cd,"central directory, local headers, UTF-8 names and contents agree");
        test(zip::dosDate(1970,1,1)==zip::dosDate(1980,1,1)&&zip::dosTime(23,59,59)==((23<<11)|(59<<5)|29),"DOS dates");}
    test(zip::entryName("a\\b.txt")=="a/b.txt"&&zip::entryName("../x").empty()&&zip::entryName("C:/x").empty()&&zip::entryName("/lead")=="lead","entry names never leave the archive");}
    // ---- Phase 5C: capture helpers ---------------------------------------------------------------
    {PixelRect screen{-1920,0,1920,1080};test(dragRect(500,400,100,50,screen)==PixelRect{100,50,500,400}&&dragRect(-3000,-10,10,2000,screen)==PixelRect{-1920,0,10,1080},"drag rectangles normalise and clip");
    std::vector<PixelRect> windows{{100,100,300,300},{0,0,800,600}};test(windowAt(windows,150,150)==0&&windowAt(windows,500,500)==1&&windowAt(windows,900,900)==-1,"topmost window under the pointer");
    test(intersect({0,0,10,10},{5,5,20,20})==PixelRect{5,5,10,10}&&intersect({0,0,10,10},{20,20,30,30}).empty(),"intersections");
    test(hexColor(0x3a7bd5)==L"#3A7BD5"&&rgbText(0x3a7bd5)==L"rgb(58, 123, 213)","colour text");
    test(snipName(2026,9,24,9,15,30)==L"Snip 2026-09-24 091530.png"&&snipName(2026,9,24,9,15,30,2)==L"Snip 2026-09-24 091530 (2).png","snip names");
    test(std::abs(ocrScale(200,50,10000)-3)<1e-9&&std::abs(ocrScale(800,400,10000)-2)<1e-9&&ocrScale(4000,100,10000)==1&&std::abs(ocrScale(20000,100,10000)-.5)<1e-9,"text recognition scale");
    test(joinLines({L"  Hello  ",L"",L"   ",L"world\t"})==L"Hello\nworld"&&wordCount(L"Hello  there,\nworld")==3&&wordCount(L"")==0,"recognised text tidying");
    std::set<std::wstring> taken{L"photo.png",L"photo (2).png"};test(freeName(L"photo",L".png",[&](const std::wstring& n){return taken.contains(n);})==L"photo (3).png","free file names");}
    // ---- Phase 5C: clipboard v2 ----------------------------------------------------------------------
    {ClipboardHistory h;auto text=[](const wchar_t* t){ClipEntry e;e.text=t;return e;};for(int i=0;i<5;++i)h.add(text((L"copy "+std::to_wstring(i)).c_str()),i);
    const auto first=h.entries().back().id;test(h.togglePin(first)&&h.entries().back().pinned,"pin");
    for(int i=5;i<40;++i)h.add(text((L"copy "+std::to_wstring(i)).c_str()),i);test(h.find(first)&&h.entries().size()==ClipboardHistory::limit+1,"pins are never pushed out");
    h.add(text(L"copy 0"),50);test(h.entries().front().id==first&&h.entries().front().pinned,"copying a pinned item again keeps it pinned");
    h.clear();test(h.entries().size()==1&&h.entries().front().pinned,"clear keeps pins");h.forget();test(h.entries().empty(),"forget drops everything");
    ClipboardHistory full;for(int i=0;i<14;++i){full.add(text((L"p"+std::to_wstring(i)).c_str()),i);}int pinnedOk=0;for(auto& e:std::vector<ClipEntry>(full.entries().begin(),full.entries().end()))pinnedOk+=full.togglePin(e.id);test(pinnedOk==int(ClipboardHistory::pinLimit)&&full.pinned()==ClipboardHistory::pinLimit,"at most 12 pins");
    ClipEntry files;files.kind=ClipEntry::Kind::Files;files.files={L"C:\\Docs\\Budget 2026.xlsx"};files.source=L"File Explorer";
    test(clipMatches(files,L"budget XLSX")&&!clipMatches(files,L"budget pdf")&&clipMatches(text(L"Meeting at 5"),L"")&&clipMatches(text(L"Launch review"),L"REVIEW launch"),"search matches every word, any case");
    for(auto secret:{L"Tr0ub4dor&3",L"482913",L"sk-proj-abcdefghijklmnop1234",L"ghp_16C7e42F292c6912E7710c838347Ae178B4a",L"AKIAIOSFODNN7EXAMPLE",L"aB3dE5fG7hJ9kL1mN3pQ5rS7tU9"})test(looksSecret(secret),"secrets are recognised");
    for(auto plain:{L"hello",L"Meeting at 5 pm",L"https://github.com/Arnav-Dugad",L"1234567890123",L"ToDo",L"Hello-World",L"C:\\Users\\Public",L"2026-09-24"})test(!looksSecret(plain),"ordinary text is not hidden");
    ClipboardHistory p;p.add(text(L"line one\nline\ttwo \\ end"),1);p.add(files,2);ClipEntry img;img.kind=ClipEntry::Kind::Image;img.dib.assign(64,1);p.add(img,3);for(auto& e:std::vector<ClipEntry>(p.entries().begin(),p.entries().end()))p.togglePin(e.id);
    auto loaded=loadPins(savePins(p.entries()));test(loaded.size()==2&&loaded[0].text==L"line one\nline\ttwo \\ end"&&loaded[1].files==files.files&&loaded[1].source==L"File Explorer"&&loaded[1].pinned,"pins save and load (images stay in memory)");
    test(loadPins(L"nonsense").empty()&&loadPins(L"pins 1\n9\tx\ty\n2\t\t\n").empty(),"bad pin files are ignored");
    ClipboardHistory r;r.add(text(L"newer"),5);r.restore(loaded,6);test(r.entries().size()==3&&r.entries().front().text==L"newer"&&r.pinned()==2,"restored pins sit below newer copies");}
    // ---- Phase 5C: pinned Shelf ---------------------------------------------------------------------
    {std::vector<ShelfItem> items{{ShelfItem::Kind::File,L"C:\\Docs\\a.txt",L"a.txt"},{ShelfItem::Kind::Text,L"note\twith\nlines",L"note"},{ShelfItem::Kind::File,L"C:\\Gone\\b.txt",L"b.txt"},{ShelfItem::Kind::File,L"C:\\Docs\\a.txt",L"a.txt"}};
    auto back=loadShelf(saveShelf(items),[](const std::wstring& path){return path.find(L"Gone")==std::wstring::npos;});
    test(back.size()==2&&back[0].value==L"C:\\Docs\\a.txt"&&back[0].label==L"a.txt"&&back[1].value==L"note\twith\nlines"&&back[1].label==L"note with lines","shelf round trip drops missing files and duplicates");
    test(loadShelf(L"shelf 2\nf\tC:\\x",[](auto&){return true;}).empty()&&loadShelf(L"shelf 1\nq\tx\nf\t\n",[](auto&){return true;}).empty(),"bad shelf files are ignored");}
    // ---- Phase 5D: currency ---------------------------------------------------------------------
    {const std::string xml="<?xml version=\"1.0\"?><gesmes:Envelope><Cube><Cube time='2026-09-24'><Cube currency='USD' rate='1.1000'/><Cube currency='JPY' rate='180.20'/><Cube currency=\"INR\" rate=\"92.40\"/><Cube currency='GBP' rate='0.8500'/><Cube currency='BAD' rate='x'/></Cube></Cube></gesmes:Envelope>";
        auto rates=parseEcbRates(xml);test(rates&&rates->date==L"2026-09-24"&&rates->perEuro.size()==4&&std::abs(*rates->rate(L"USD")-1.1)<1e-9&&*rates->rate(L"EUR")==1&&!rates->rate(L"CHF"),"ECB daily rates are read, bad entries skipped");
        test(!parseEcbRates("<html>no rates</html>")&&!parseEcbRates(""),"a page without rates is rejected");
        auto q=[&](const wchar_t* t,const wchar_t* home=L"USD"){return parseCurrency(t,home);};
        {auto a=q(L"100 usd to inr");test(a&&a->amount==100&&a->from==L"USD"&&a->to==L"INR"&&a->explicitAmount,"100 usd to inr");}
        {auto a=q(L"$50 in \u20ac");test(a&&a->amount==50&&a->from==L"USD"&&a->to==L"EUR","symbols on either side");}
        {auto a=q(L"convert 20 pounds into euros");test(a&&a->amount==20&&a->from==L"GBP"&&a->to==L"EUR","spoken names and 'convert'");}
        {auto a=q(L"usd inr");test(a&&a->amount==1&&!a->explicitAmount&&a->to==L"INR","a currency pair means one unit");}
        {auto a=q(L"20 eur",L"INR");test(a&&a->from==L"EUR"&&a->to==L"INR","an amount alone converts to the home currency");}
        {auto a=q(L"20 inr",L"INR");test(a&&a->to==L"USD","home currency amounts go to US dollars");}
        {auto a=q(L"1.5k jpy to usd");test(a&&a->amount==1500,"k suffix");auto b=q(L"12,5 eur to usd");test(b&&std::abs(b->amount-12.5)<1e-9,"decimal comma");auto c=q(L"1,234.56 usd to eur");test(c&&std::abs(c->amount-1234.56)<1e-9,"grouped thousands");auto d=q(L"100usd to inr");test(d&&d->amount==100&&d->from==L"USD","no space after the number");}
        for(const wchar_t* t:{L"next",L"real",L"pound cake",L"usd to",L"open spotify",L"100",L"volume 30",L"euro truck simulator",L"won",L"rand"})test(!q(t),"ordinary words are not currency questions");
        const ExchangeRates r=*rates;
        {auto a=convertCurrency({100,L"USD",L"INR",true},r);test(a&&a->answer==L"\u20b98,400.00"&&a->plain==L"8400.00"&&a->detail.find(L"ECB rate of 24 Sep")!=std::wstring::npos&&a->detail.starts_with(L"100 USD in Indian rupees"),"USD to INR through the euro");}
        {auto a=convertCurrency({250,L"EUR",L"JPY",true},r);test(a&&a->answer==L"\u00a545,050"&&a->plain==L"45050","yen has no minor unit");}
        test(!convertCurrency({1,L"USD",L"CHF",true},r),"a currency without a rate has no answer");
        test(groupedNumber(1234567.891,2)==L"1,234,567.89"&&groupedNumber(999,2)==L"999.00"&&groupedNumber(0.004567,2).starts_with(L"0.0045")&&groupedNumber(45050,0)==L"45,050","grouped numbers");
        test(rateDay(L"2026-09-24")==L"24 Sep"&&rateDay(L"bad")==L"bad","rate dates");
        std::vector<InstalledApp> none;CommandEnv off;auto offRows=parseCommand(L"100 usd to inr",none,{},L"C:\\",off);test(offRows.size()==1&&offRows[0].kind==CommandKind::None&&offRows[0].title==L"Currency conversion is off","currency stays off until turned on");
        CommandEnv on;on.currency=true;on.rates=&r;auto ans=parseCommand(L"100 usd to inr",none,{},L"C:\\",on);test(ans.size()==1&&ans[0].kind==CommandKind::Currency&&ans[0].answer==L"\u20b98,400.00"&&ans[0].target==L"8400.00","a currency answer row");
        CommandEnv loading;loading.currency=true;loading.ratesLoading=true;auto wait=parseCommand(L"5 gbp",none,{},L"C:\\",loading);test(wait.size()==1&&wait[0].kind==CommandKind::None&&wait[0].title.find(L"Getting")==0,"rates on their way");}
    {std::stringstream v11("version 11\npinnedShelf 1\n");auto old=Settings::parse(v11);test(old.version==Settings::currentVersion&&old.pinnedShelf&&!old.currency&&old.commandHistory,"v11 files get the v12 defaults (currency off, command history on)");
        Settings s;s.currency=true;s.commandHistory=false;std::stringstream out;s.write(out);auto back=Settings::parse(out);test(back.currency&&!back.commandHistory,"v12 preferences round trip");}
    // ---- Phase 5D: system actions ------------------------------------------------------------------
    {std::vector<InstalledApp> none;auto rows=[&](const wchar_t* t,CommandEnv e){return parseCommand(t,none,{},L"C:\\",e);};
        CommandEnv e;e.bluetooth=1;e.wifi=0;e.dark=0;e.binItems=3;e.binBytes=2048;
        {auto r=rows(L"bluetooth",e);test(r.size()==2&&r[0].kind==CommandKind::Bluetooth&&r[0].value==0&&r[0].title==L"Turn Bluetooth off"&&r[0].detail==L"Bluetooth is on now"&&r[1].kind==CommandKind::OpenSettings,"the Bluetooth row reads the live state");}
        {auto r=rows(L"bluetooth on",e);test(r[0].kind==CommandKind::None&&r[0].title==L"Bluetooth is already on","asking for the current state changes nothing");}
        {auto r=rows(L"turn on wifi",e);test(r[0].kind==CommandKind::WiFi&&r[0].value==1&&r[0].title==L"Turn Wi-Fi on","turn on wifi");}
        {CommandEnv x=e;x.bluetooth=-2;auto r=rows(L"bt",x);test(r[0].kind==CommandKind::None&&r[0].title==L"No Bluetooth radio found","no radio");x.bluetooth=-1;r=rows(L"bt",x);test(r[0].kind==CommandKind::Bluetooth&&r[0].value==-1,"unknown state toggles");}
        {auto r=rows(L"bluetooth settings",e);test(r[0].kind==CommandKind::OpenSettings&&r[0].target==L"ms-settings:bluetooth","Bluetooth settings still open the page");}
        {auto r=rows(L"airplane mode",e);test(r[0].kind==CommandKind::Airplane&&r[0].value==1,"airplane mode turns radios off");CommandEnv q=e;q.bluetooth=0;r=rows(L"flight mode",q);test(r[0].value==0&&r[0].title==L"Turn Wi-Fi and Bluetooth back on","and back on");}
        {auto r=rows(L"dark mode",e);test(r[0].kind==CommandKind::DarkMode&&r[0].value==1&&r[0].title==L"Switch to dark mode","dark mode");CommandEnv d=e;d.dark=1;r=rows(L"dark mode",d);test(r[0].value==0&&r[0].title==L"Switch to light mode","asking for dark when dark offers light");r=rows(L"theme",d);test(r[0].value==0,"theme toggles");}
        {auto r=rows(L"empty recycle bin",e);test(r[0].kind==CommandKind::EmptyBin&&r[0].confirm&&r[0].detail.find(L"3 items")==0&&r[0].detail.find(L"2.0 KB")!=std::wstring::npos,"recycle bin shows what it holds and asks first");CommandEnv z=e;z.binItems=0;r=rows(L"empty bin",z);test(r[0].kind==CommandKind::None,"an empty bin has nothing to do");}
        for(const wchar_t* t:{L"sleep",L"restart",L"shut down",L"lock"}){auto r=rows(t,e);test(!r.empty()&&r[0].confirm&&r[0].kind!=CommandKind::None,"sleep, restart, shut down and lock ask for a second Enter");}
        test(byteText(1)==L"1 byte"&&byteText(2048)==L"2.0 KB"&&byteText(56*1048576ull)==L"56 MB","byte sizes");}
    // ---- Phase 5D: matched letters, ghost completion -----------------------------------------------
    {auto marks=matchMarks(L"Visual Studio Code",L"vsc");test(marks.size()==3&&marks[0].first==0&&marks[1].first==7&&marks[2].first==14,"initials are marked");
        test(matchMarks(L"Spotify",L"spo")==MatchMarks{{0,3}},"prefix marked");test(matchMarks(L"EA SPORTS FC 26",L"spo")==MatchMarks{{3,3}},"word start marked");
        test(matchMarks(L"Quarterly budget 2026.xlsx",L"budget 2026")==MatchMarks{{10,11}},"a phrase at a word start is one run");test(matchMarks(L"Budget review notes",L"notes budget")==MatchMarks({{0,6},{14,5}}),"each word marked, in order");
        test(matchMarks(L"abc",L"zz").empty()&&matchMarks(L"",L"a").empty(),"no match, no marks");
        std::vector<InstalledApp> apps{{L"Spotify",L"spotify.id"}};auto r=parseCommand(L"spo",apps,{},L"C:\\");test(!r.empty()&&r[0].kind==CommandKind::OpenApp&&r[0].marks==MatchMarks{{5,3}}&&r[0].completion==L"spotify","app rows mark letters in \"Open …\" and complete the name");
        r=parseCommand(L"Open Spo",apps,{},L"C:\\");test(r[0].completion==L"Open Spotify","completion keeps what was typed");
        test(ghostSuffix(L"spo",L"spotify")==L"tify"&&ghostSuffix(L"SPO",L"spotify")==L"tify"&&ghostSuffix(L"spx",L"spotify").empty()&&ghostSuffix(L"",L"x").empty(),"ghost suffix");
        test(phraseCompletion(L"blu")==L"bluetooth"&&phraseCompletion(L"empty r")==L"empty recycle bin"&&phraseCompletion(L"x").empty()&&phraseCompletion(L"bluetooth").empty(),"command phrases complete");}
    // ---- Phase 5D: file search ------------------------------------------------------------------------
    {auto s=fileSpec(L"budget pdfs from last week in downloads");test(s.terms==std::vector<std::wstring>{L"budget"}&&s.extensions==std::vector<std::wstring>{L".pdf"}&&s.date==L"last week"&&s.folder==L"downloads","typed filters become a file spec");
        s=fileSpec(L"screenshots in pictures");test(s.kind==L"picture"&&s.folder==L"pictures"&&s.terms.empty(),"a folder word is not a kind");
        test(fileSpec(L"the").empty()&&!fileSpec(L"notes").empty(),"empty specs");
        auto range=dateRange(L"last week",{2026,9,24},4);test(range&&range->first==Day{2026,9,14}&&range->second==Day{2026,9,21},"last week runs Monday to Monday");
        range=dateRange(L"this month",{2026,9,24},4);test(range&&range->first==Day{2026,9,1}&&range->second==Day{2026,10,1},"this month");
        range=dateRange(L"last month",{2026,1,15},4);test(range&&range->first==Day{2025,12,1}&&range->second==Day{2026,1,1},"last month across a year");
        range=dateRange(L"today",{2026,12,31},4);test(range&&range->second==Day{2027,1,1},"today ends at midnight");test(addDays({2028,2,28},1)==Day{2028,2,29}&&addDays({2027,3,1},-1)==Day{2027,2,28},"leap years");
        auto sql=searchSql(fileSpec(L"budget pdfs"),L"file:C:/Users/Sam",L"2026-09-14 00:00:00",L"",40);
        test(sql.starts_with(L"SELECT TOP 40 \"System.ItemUrl\"")&&sql.find(L"SCOPE='file:C:/Users/Sam'")!=std::wstring::npos&&sql.find(L"CONTAINS(\"System.FileName\",'\"budget*\"')")!=std::wstring::npos&&sql.find(L"\"System.FileExtension\"='.pdf'")!=std::wstring::npos&&sql.find(L"\"System.DateModified\">='2026-09-14 00:00:00'")!=std::wstring::npos&&sql.ends_with(L"ORDER BY \"System.Search.Rank\" DESC"),"index SQL");
        test(sql.find(L"<>'.pyc'")!=std::wstring::npos&&sql.find(L"LIKE '%\\node[_]modules\\%'")!=std::wstring::npos,"build output and tool folders are excluded");
        auto odd=searchSql(fileSpec(L"o'brien c++ 100%_done"),L"file:C:/x's",L"",L"",5);test(odd.find(L"LIKE '%o''brien%'")!=std::wstring::npos&&odd.find(L"LIKE '%c++%'")!=std::wstring::npos&&odd.find(L"LIKE '%100[%][_]done%'")!=std::wstring::npos&&odd.find(L"SCOPE='file:C:/x''s'")!=std::wstring::npos&&odd.find(L"ORDER BY")!=std::wstring::npos,"quotes and wildcards are escaped");
        test(sqlLiteral(std::wstring(L"a\nb'")+wchar_t(7))==L"ab''","control characters never reach the SQL");
        test(pathFromItemUrl(L"file:C:/Users/Public/Documents/a%20b.pdf")==L"C:\\Users\\Public\\Documents\\a b.pdf"&&pathFromItemUrl(L"file:C:/x/caf%C3%A9.txt")==L"C:\\x\\caf\u00e9.txt"&&pathFromItemUrl(L"http://x").empty()&&pathFromItemUrl(L"file:///C:/a")==L"C:\\a","item URLs become paths");
        auto spec=fileSpec(L"budget pdf");test(fileMatches(spec,L"Budget notes.pdf",false)&&!fileMatches(spec,L"Budget.txt",false)&&!fileMatches(spec,L"notes.pdf",false),"scan matching");
        test(fileMatches(fileSpec(L"folders"),L"Work",true)&&!fileMatches(fileSpec(L"folders"),L"a.txt",false)&&fileMatches(fileSpec(L"photos"),L"x.JPG",false),"kinds in the scan");
        test(noisyFile(L"C:\\p\\node_modules\\a.js")&&noisyFile(L"C:\\p\\x.pyc")&&!noisyFile(L"C:\\p\\report.pdf"),"noise files");
        test(fileRank(L"budget.xlsx",fileSpec(L"budget"),0,100)>fileRank(L"old budget.xlsx",fileSpec(L"budget"),0,100)&&fileRank(L"old budget.xlsx",fileSpec(L"budget"),3,100)>fileRank(L"budget.xlsx",fileSpec(L"budget"),0,100),"names that start with the words rank first; files you open often rise");
        test(fileAge(30)==L"just now"&&fileAge(3*86400.)==L"3 days ago"&&fileAge(86400*1.5)==L"yesterday"&&fileAge(400*86400.)==L"1 year ago","file ages");}
    // ---- Phase 5D: command memory ----------------------------------------------------------------
    {CommandMemory m;CommandResult a;a.kind=CommandKind::DarkMode;a.title=L"Switch to dark mode";a.value=1;CommandResult b;b.kind=CommandKind::OpenApp;b.title=L"Open Spotify";b.target=L"spotify.id";
        m.record(a,L"dark mode",100);m.record(b,L"spo",200);m.record(a,L"dark",300);
        auto recent=m.recent(5);test(m.items().size()==2&&recent.size()==2&&recent[0]->kind==CommandKind::DarkMode&&recent[0]->count==2&&recent[0]->phrase==L"dark","recent commands, newest first, counted");
        test(m.togglePin(b,L"spo",400)==1&&m.isPinned(b)&&m.recent(5).size()==1&&m.pinned().size()==1,"pinning moves a command out of recent");
        CommandResult money;money.kind=CommandKind::Currency;money.title=L"x";m.record(money,L"1 usd",1);test(m.items().size()==2&&m.togglePin(money,L"",1)==-1,"answers are not remembered");
        for(int i=0;i<8;++i){CommandResult c;c.kind=CommandKind::OpenFile;c.title=L"f"+std::to_wstring(i);c.target=L"C:\\f"+std::to_wstring(i);m.togglePin(c,L"",500+i);}test(m.pinned().size()==CommandMemory::pinLimit,"at most six pins");
        test(m.togglePin(b,L"",600)==0&&!m.isPinned(b),"unpin");
        const size_t pins=m.pinned().size();for(int i=0;i<60;++i){CommandResult c;c.kind=CommandKind::OpenFile;c.title=L"g";c.target=L"C:\\g"+std::to_wstring(i);m.record(c,L"",1000+i);}test(pins==5&&m.items().size()==CommandMemory::limit&&m.pinned().size()==pins,"the list is capped, pins kept");
        CommandMemory n;CommandResult tricky;tricky.kind=CommandKind::OpenSettings;tricky.title=L"Open \u00e9 settings";tricky.detail=L"tab\there\nline\\slash";tricky.target=L"ms-settings:x";n.record(tricky,L"s\tx",42);n.togglePin(tricky,L"s\tx",43);
        std::stringstream io;n.write(io);auto back=CommandMemory::read(io);test(back.items().size()==1&&back.items()[0].detail==L"tab\there\nline\\slash"&&back.items()[0].phrase==L"s\tx"&&back.items()[0].pinned&&back.items()[0].title==L"Open \u00e9 settings","memory file round trip");
        std::stringstream bad("commands 2\n1\t0\t0\t0\t0\ta\tb\tc\td\n");test(CommandMemory::read(bad).items().empty(),"unknown versions are ignored");
        CommandMemory f;CommandResult file;file.kind=CommandKind::OpenFile;file.title=L"a";file.target=L"C:\\A";f.record(file,L"",0);f.record(file,L"",0);test(std::abs(f.frecency(CommandKind::OpenFile,L"c:\\a",7*86400)-1)<1e-9&&f.frecency(CommandKind::OpenFile,L"C:\\B",0)==0,"use counts fade by half each week");}
    // ---- Phase 5E: rolling digits ---------------------------------------------------------------------
    {auto r=digitRoll(18,7);test(r.from==18&&r.to==17,"a countdown rolls one step back");r=digitRoll(19,0);test(r.from==19&&r.to==20,"9 to 0 rolls one step forward");
        r=digitRoll(10,9);test(r.to==9,"0 to 9 rolls one step back");r=digitRoll(12,5);test(r.from==12&&r.to==15,"small changes roll directly");
        r=digitRoll(9.98,0);test(std::abs(r.from-19.98)<1e-9&&r.to==20,"a column still settling re-bases without a jump");
        bool bounded=true;for(double p=0;p<=30;p+=.37)for(int d=0;d<10;++d){auto k=digitRoll(p,d);bounded=bounded&&k.to>=5&&k.to<=25&&std::abs(k.to-k.from)<=5+1e-9&&std::abs(std::fmod(k.from-p+100,10.))<1e-9;}test(bounded,"rolls stay on the strip, at most five digits, from an equivalent place");}
    // ---- Phase 5E: GPU use ----------------------------------------------------------------------------
    {const std::vector<std::pair<std::wstring,double>> s{{L"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D",20},{L"pid_2_luid_0x0_0x1_phys_0_eng_0_engtype_3D",15},{L"pid_1_luid_0x0_0x1_phys_0_eng_1_engtype_Copy",30},{L"bogus",90},{L"pid_3_luid_0x0_0x1_phys_0_eng_2_engtype_VideoDecode",std::nan("")}};
        test(std::abs(gpuBusy(s)-35)<1e-9,"the GPU is as busy as its busiest engine, summed over processes");test(gpuBusy({})==-1&&gpuBusy({{L"bogus",5}})==-1,"no engines, no reading");
        test(gpuBusy({{L"pid_1_luid_0_eng_0",80},{L"pid_2_luid_0_eng_0",70}})==100,"at most 100%");}
    // ---- Phase 5E: colour codes, typos and grouped rows ---------------------------------------------
    {test(parseColourCode(L"#3A7BD5")==0x3A7BD5u&&parseColourCode(L"#39f")==0x3399FFu&&parseColourCode(L"colour 3a7bd5")==0x3A7BD5u&&parseColourCode(L"color #3A7BD5FF")==0x3A7BD5u&&parseColourCode(L"rgb(58, 123, 213)")==0x3A7BD5u,"colour codes");
        test(!parseColourCode(L"#12345")&&!parseColourCode(L"rgb(300,0,0)")&&!parseColourCode(L"#zzzzzz")&&!parseColourCode(L"3a7bd5")&&!parseColourCode(L"rgb(1,2)"),"not colour codes");
        test(colourHex(0x3A7BD5)==L"#3A7BD5"&&colourDetail(0x3A7BD5)==L"rgb(58, 123, 213)  \u00b7  hsl(215, 65%, 53%)"&&colourDetail(0x808080)==L"rgb(128, 128, 128)  \u00b7  hsl(0, 0%, 50%)","colour details");
        const std::vector<InstalledApp> apps{{L"Spotify",L"spotify.id"},{L"Microsoft Edge",L"MSEdge"}};
        auto c=parseCommand(L"#3a7bd5",apps,{},L"C:\\");test(c.size()==1&&c[0].kind==CommandKind::Colour&&c[0].value==0x3A7BD5&&c[0].title==L"#3A7BD5"&&!CommandMemory::memorable(CommandKind::Colour),"a colour row, not remembered");
        test(editDistance(L"ab",L"ba")==1&&editDistance(L"kitten",L"sitting")==3&&editDistance(L"",L"abc")==3&&typoAllowance(3)==0&&typoAllowance(5)==1&&typoAllowance(9)==2,"edit distance");
        auto t=parseCommand(L"spotfy",apps,{},L"C:\\");test(!t.empty()&&t[0].kind==CommandKind::OpenApp&&t[0].target==L"spotify.id","a misspelt app is found");
        t=parseCommand(L"bluetoth",apps,{},L"C:\\");test(!t.empty()&&t[0].kind==CommandKind::Bluetooth&&t[0].detail.find(L"Did you mean")==0,"a misspelt command is suggested");
        t=parseCommand(L"spx",apps,{},L"C:\\");test(t.size()==1&&t[0].kind==CommandKind::None,"short words are not guessed at");
        std::vector<CommandResult> rows(4);rows[0].kind=rows[1].kind=CommandKind::OpenApp;rows[2].kind=CommandKind::OpenFile;rows[3].kind=CommandKind::DarkMode;
        auto laid=commandRows(rows,5);test(laid.headers.size()==3&&laid.headers[0]==std::pair{52.f,1}&&laid.rows==std::vector<float>{70,110,168,226}&&laid.headers[2]==std::pair{208.f,4}&&laid.footer==268,"a header before each group");
        test(commandRows(rows,5,false).headers.empty()&&commandRows(rows,5,false).footer==214,"clipboard rows have no headers");
        rows.resize(2);laid=commandRows(rows,5);test(laid.headers.empty()&&laid.rows==std::vector<float>{52,92}&&laid.footer==134,"one group needs no header");
        rows[0].section=1;rows[1].section=2;test(commandRows(rows,5).headers.size()==2&&std::wstring(groupName(resultGroup(rows[1])))==L"Suggested","the empty bar's sections");}
    // ---- Phase 5E: left dock, screen capture, settings v13 ------------------------------------------
    test(atIslandEdge(0,540,0,0,1920,1080,2,540,130)&&!atIslandEdge(20,540,0,0,1920,1080,2,540,130)&&!atIslandEdge(0,100,0,0,1920,1080,2,540,130),"left edge band");
    test(capabilityOrder[2]==Capability::ScreenCapture&&std::wstring(capabilityName(Capability::ScreenCapture))==L"Screen capture"&&capabilityColour(Capability::ScreenCapture)==0xbf5af2,"screen capture is the purple dot, after the microphone");
    {std::stringstream v12("version 12\nedge 1\n");auto s=Settings::parse(v12);test(s.version==Settings::currentVersion&&s.edge==1&&s.shadow&&s.compactGlance&&s.artPulse,"v12 settings gain the shadow, the idle glance and the beat pulse");
        std::stringstream left("version 13\nedge 2\n"),beyond("version 13\nedge 3\n");test(Settings::parse(left).edge==2&&Settings::parse(beyond).edge==2,"the left dock is kept, beyond it clamped");
        Settings off;off.shadow=off.compactGlance=off.artPulse=false;std::stringstream io;off.write(io);auto back=Settings::parse(io);test(!back.shadow&&!back.compactGlance&&!back.artPulse,"the new switches round-trip");}
    // ---- Phase 5F: word-timed lyrics ----------------------------------------------------------------
    {auto w=parseLrc(L"[00:10.00]<00:10.00>Hello <00:10.50>bright <00:11.20>world\n[00:14.00]Next line\n");
        test(w.size()==2&&w[0].text==L"Hello bright world"&&w[0].words.size()==3&&w[0].words[1].start==6&&w[0].words[2].start==13&&std::abs(w[0].words[1].time-10.5)<1e-9&&w[1].words.empty(),"word tags become word timing");
        auto k=lyricFill(w,0);test(std::abs(keyedAt(k,10.25)-3)<1e-9&&std::abs(keyedAt(k,10.5)-6)<1e-9&&std::abs(keyedAt(k,11.2)-13)<1e-9&&std::abs(keyedAt(k,11.75)-18)<1e-9&&keyedAt(k,20)==18&&keyedAt(k,5)==0,"a word-timed line lights word by word");
        auto plain=lyricFill(w,1);test(plain.size()==2&&plain[0]==std::pair{14.,0.}&&plain[1].second==9&&plain[1].first>14,"an untimed line sweeps at a singing pace");
        test(parseLrc(L"[00:01.00]<00:02.00>a <00:01.50>b")[0].words.empty(),"out-of-order word tags are dropped");
        auto twice=parseLrc(L"[00:01.00][00:05.00]<00:01.00>la <00:01.50>la");test(twice.size()==2&&std::abs(twice[1].words[0].time-5)<1e-9&&std::abs(twice[1].words[1].time-5.5)<1e-9,"a repeated line's words shift with it");
        auto early=parseLrc(L"[offset:+500]\n[00:02.00]<00:02.00>go <00:02.40>on");test(std::abs(early[0].time-1.5)<1e-9&&std::abs(early[0].words[1].time-1.9)<1e-9,"the offset moves word timing too");}
    // ---- Phase 5F: rich clipboard rows ------------------------------------------------------------------
    {test(linkHost(L"https://www.GitHub.com:443/user/repo?tab=1#x")==L"github.com"&&linkPath(L"https://www.GitHub.com:443/user/repo?tab=1#x")==L"/user/repo","a link's host and path");
        test(linkHost(L"https://user@host.example.org/")==L"host.example.org"&&linkPath(L"https://example.com/").empty()&&linkHost(L"mailto:a@b.com").empty()&&linkHost(L"https://localhost/").empty(),"hosts are plain names");
        test(looksLikeCode(L"int main() {\n  return 0;\n}")&&looksLikeCode(L"def add(a, b):\n    return a + b")&&!looksLikeCode(L"Meet me at the station at five, then dinner.")&&!looksLikeCode(L"Hi"),"code is told from prose");
        const auto spans=codeSpans(L"return x == \"hi\"; // done");auto has=[&](CodeSpan c){return std::find(spans.begin(),spans.end(),c)!=spans.end();};
        test(has({0,6,1})&&has({12,4,2})&&has({9,1,5})&&has({18,7,4})&&!has({7,1,1}),"code is coloured by kind");
        test(codeSpans(L"#include <x>")[0].kind==1&&codeSpans(L"# a note")[0].kind==4,"a directive is not a comment");}
    // ---- Phase 5F: weather ---------------------------------------------------------------------------------
    {auto p=parseGeocode(R"({"results":[{"name":"Paris","country":"France","latitude":48.8566,"longitude":2.3522}]})");test(p&&p->name==L"Paris, France"&&p->latitude==48.86&&p->longitude==2.35,"a town becomes a place, rounded to about a kilometre");
        test(!parseGeocode(R"({"results":[]})")&&!parseGeocode(R"({"results":[{"name":"X","latitude":95,"longitude":0}]})")&&!parseGeocode("not json"),"no match, no place");
        auto now=parseForecast(R"({"current":{"temperature_2m":14.4,"weather_code":61,"is_day":0}})");test(now&&now->temperature==14.4&&now->code==61&&!now->day,"the current sky");
        test(!parseForecast("{}")&&!parseForecast(R"({"current":{"temperature_2m":500,"weather_code":1}})"),"impossible weather is refused");
        std::stringstream io;writePlace(io,{L"Z\u00fcrich, Switzerland",47.37,8.54});auto back=readPlace(io);test(back&&back->name==L"Z\u00fcrich, Switzerland"&&back->latitude==47.37,"the place is kept");
        std::stringstream bad("weather 1\n95 0\nNowhere\n");test(!readPlace(bad),"a damaged place file is ignored");
        test(skyOf(0)==Sky::Clear&&skyOf(2)==Sky::PartlyCloudy&&skyOf(45)==Sky::Fog&&skyOf(53)==Sky::Drizzle&&skyOf(81)==Sky::Rain&&skyOf(73)==Sky::Snow&&skyOf(96)==Sky::Storm,"WMO codes");
        test(std::wstring(skyLabel(Sky::Storm))==L"Storm"&&std::wstring(skyLabel(Sky::PartlyCloudy))==L"Partly cloudy"&&temperatureText(-.4,0)==L"0\u00b0"&&temperatureText(20,1)==L"68\u00b0","the tile's words and degrees");
        const std::vector<InstalledApp> apps;auto c=parseCommand(L"weather in Paris",apps,{},L"C:\\");test(!c.empty()&&c[0].kind==CommandKind::Weather&&c[0].detail.find(L"only the town is sent")!=std::wstring::npos&&!CommandMemory::memorable(CommandKind::Weather),"the weather command");
        c=parseCommand(L"weather",apps,{},L"C:\\");test(!c.empty()&&c[0].kind==CommandKind::Weather&&c[0].target.empty(),"a bare weather command opens the Town field");}
    // ---- Phase 5F: battery health and the week ---------------------------------------------------------
    {BatteryHealthLog log;BatteryReading r;r.fullMwh=45000;r.designMwh=50000;r.cycles=10;const int64_t day=86400;
        test(log.add(100*day,r)&&!log.add(100*day+3600,r),"one health reading a day");r.relative=true;test(!log.add(101*day,r),"relative readings carry no capacity");r.relative=false;
        r.fullMwh=44500;test(log.add(107*day,r),"a week later");auto w=log.week();test(w&&std::abs(w->first-.9)<1e-9&&std::abs(w->second-.89)<1e-9,"health now and a week ago");
        log.lastCard=123;std::stringstream io;log.write(io);auto back=BatteryHealthLog::read(io);test(back.days==log.days&&back.lastCard==123,"the health log is kept");
        std::stringstream bad("battery_health 1\ncard 0\n5 100 90 1\n4 100 90 1\n");test(BatteryHealthLog::read(bad).days.empty(),"a log out of order is refused");
        BatteryHistory h;const int64_t t0=10*day;for(int i=0;i<12;++i)h.add(t0+i*300,90-i,false);h.add(t0+3600,80,true);
        auto week=summarizeWeek(h,t0+3700);test(week.charges==1&&std::abs(week.usedPerDay-11)<1e-9&&std::abs(week.hoursOnBattery-11*300/3600.)<1e-9,"the week on battery");
        test(summarizeWeek({},t0).usedPerDay==-1,"no history, no summary");}
    // ---- Phase 5F: chips, the Controls page, settings v14 --------------------------------------------------
    {test(withControls({0,1,2,3,4,5,6})==std::array<int,pageCount>{0,1,2,7,3,4,5,6}&&withControls({0,0,2,3,4,5,6})==defaultNavigation,"Controls joins a saved order after Stats");
        test(decodeChips(encodeChips(defaultChips))==defaultChips&&moveChip(defaultChips,0,2)==std::array<int,chipCount>{5,4,6,3,2,1,0}&&moveChip(moveChip(defaultChips,0,2),2,0)==defaultChips,"chips move and round-trip");
        test(!validChips({0,0,1,2,3,4,5})&&validChips(defaultChips),"a chip order has each chip once");
        std::stringstream v13("version 13\nnav0 1\nnav1 0\nnav2 2\nnav3 3\nnav4 5\nnav5 6\nnav6 4\n");auto s=Settings::parse(v13);
        test(s.version==Settings::currentVersion&&s.navigation==std::array<int,pageCount>{1,0,2,7,3,5,6,4}&&s.chips==defaultChips&&!s.weather&&!s.siteIcons&&!s.sharing&&s.notifyStyle==1&&s.waveformStyle==1,"v13 settings gain Controls; online features stay off");
        Settings custom;custom.chips=moveChip(defaultChips,1,5);custom.sharing=true;custom.notifyStyle=0;std::stringstream io;custom.write(io);auto back=Settings::parse(io);
        test(back.chips==custom.chips&&back.sharing&&back.notifyStyle==0,"v14 settings round-trip");
        std::stringstream broken("version 14\nchip0 1\nchip1 1\n");test(Settings::parse(broken).chips==defaultChips,"a broken chip order falls back");}
    // ---- Phase 5F: the adaptive text grid and the drop ----------------------------------------------------
    {LumaGrid g;g.cols=4;g.rows=2;g.x0=10;g.cell=2;g.luma={1,2,3,4,5,6,7,8};test(g.at(10,0)==1&&g.at(13.9f,0)==2&&g.at(15,3)==7&&g.at(-50,-50)==1&&g.at(500,500)==8,"the backdrop grid clamps to its edges");
        test(LumaGrid{}.at(0,0)==0&&brightBackdrop>127,"an empty grid reads dark");test(dropDistance>34&&dropDistance<60,"a dropped pill clears the compact island");}
    // ---- Phase 5G: the clipboard across restarts ------------------------------------------------------
    {ClipboardHistory h;auto text=[](std::wstring t){ClipEntry e;e.text=std::move(t);e.kind=isLink(e.text)?ClipEntry::Kind::Link:ClipEntry::Kind::Text;e.source=L"Notepad";e.sourcePath=L"C:\\Windows\\notepad.exe";return e;};
        h.add(text(L"first copy"),10);h.add(text(L"https://example.com/a"),20);h.add(text(L"Tr0ub4dor&3xQ"),30);
        ClipEntry files;files.kind=ClipEntry::Kind::Files;files.files={L"C:\\a.txt",L"C:\\b c.txt"};h.add(files,40);
        ClipEntry raw;raw.kind=ClipEntry::Kind::Image;raw.dib.assign(64,1);h.add(raw,50);
        ClipEntry picture;picture.kind=ClipEntry::Kind::Image;picture.dib.assign(64,2);picture.png=std::make_shared<const std::string>(std::string("PNG\0DATA",8));h.add(picture,55);
        uint64_t firstId=0;for(auto& e:h.entries())if(e.text==L"first copy")firstId=e.id;test(h.togglePin(firstId),"a copy pins");
        const std::string saved=saveHistory(h.entries(),{1000,60});auto back=loadHistory(saved,2000,100);
        // The password-like copy and the image without its PNG yet are left out; the rest come back newest first.
        test(back.size()==4,"the history comes back without secrets or unfinished images");
        test(back[0].kind==ClipEntry::Kind::Image&&back[0].png&&*back[0].png==std::string("PNG\0DATA",8),"an image comes back as its PNG");
        test(back[1].kind==ClipEntry::Kind::Files&&back[1].files==files.files,"file lists come back");
        test(back[2].kind==ClipEntry::Kind::Link&&back[2].text==L"https://example.com/a","links come back");
        test(back[3].text==L"first copy"&&back[3].pinned&&back[3].source==L"Notepad"&&back[3].sourcePath==L"C:\\Windows\\notepad.exe","pins and sources come back");
        test(std::abs((100-back[3].time)-1050)<1.5&&std::abs((100-back[0].time)-1005)<1.5,"ages carry across the restart");
        {ClipboardHistory pinned;pinned.add(text(L"Tr0ub4dor&3xQ"),1);pinned.togglePin(pinned.entries().front().id);test(loadHistory(saveHistory(pinned.entries(),{10,5}),10,5).size()==1,"a pinned secret is kept");}
        test(loadHistory("garbage",1,1).empty()&&loadHistory("",1,1).empty(),"anything else reads as nothing");
        bool safe=true;for(size_t cut=0;cut<saved.size();cut+=7){try{auto part=loadHistory(saved.substr(0,cut),2000,100);if(part.size()>4)safe=false;}catch(...){safe=false;}}test(safe,"a cut-off file reads its whole copies only");
        ClipboardHistory fresh;fresh.add(text(L"newer"),200);fresh.add(text(L"first copy"),201);fresh.restoreHistory(back);
        test(fresh.entries().size()==5&&fresh.entries()[0].text==L"first copy"&&fresh.entries()[1].text==L"newer"&&fresh.entries()[2].kind==ClipEntry::Kind::Image,"kept copies go below new ones, once each");
        test(fresh.attachPng(fresh.entries()[2].id,std::make_shared<const std::string>("X"))&&!fresh.attachPng(999999,nullptr),"a PNG attaches to its image");}
    // ---- Phase 5G: the music library --------------------------------------------------------------------
    {test(isAudioFile(L"C:\\Music\\a.MP3")&&isAudioFile(L"x.flac")&&isAudioFile(L"y.m4a")&&!isAudioFile(L"notes.txt")&&!isAudioFile(L"mp3")&&!isAudioFile(L"C:\\m.mp3\\file"),"songs are known by their extension");
        test(titleFromFileName(L"C:\\M\\03 - Blue in Green.mp3")==L"Blue in Green"&&titleFromFileName(L"1-02 So_What.flac")==L"So What"&&titleFromFileName(L"2046.mp3")==L"2046"&&titleFromFileName(L"Alarm01.wav")==L"Alarm01"&&titleFromFileName(L".mp3")==L"Untitled","titles from file names");
        test(foldName(L"  So  What?! ")==L"so what"&&foldName(L"AC/DC")==L"ac dc","names fold for comparing");
        std::vector<LibraryTrack> t{{L"c:\\1.mp3",L"So What",L"Miles Davis",L"Kind of Blue",562,1},{L"c:\\2.mp3",L"Green Onions",L"Booker T",L"Green Onions",170,1},{L"c:\\3.mp3",L"Blue in Green",L"miles davis",L"Kind of Blue",337,3},{L"c:\\4.mp3",L"Untitled",L"",L"",60,0}};
        sortLibrary(t);test(t[0].title==L"Green Onions"&&t[1].title==L"So What"&&t[2].title==L"Blue in Green"&&t[3].artist.empty(),"the library lists by artist, album and track, unknown artists last");
        auto green=searchLibrary(t,L"green");test(green.size()==2&&t[green[0]].title==L"Green Onions"&&t[green[1]].title==L"Blue in Green","a search puts titles that start with it first");
        auto what=searchLibrary(t,L"MILES what");test(what.size()==1&&t[what[0]].title==L"So What","every word must match");test(searchLibrary(t,L"").size()==4&&searchLibrary(t,L"",2).size()==2,"an empty search lists all, up to the limit");
        test(matchTrack(t,L"so what!",L"MILES DAVIS",{},0)==1&&matchTrack(t,L"So What",L"",{},0)==1&&matchTrack(t,L"So What",L"Someone Else",{},0)==-1,"a handed-off song is found by title and artist");
        t[1].size=99;test(matchTrack(t,L"Nope",L"",L"1.mp3",99)==1&&matchTrack(t,L"Nope",L"",L"1.mp3",98)==-1,"or by its file name and size");
        auto order=shuffledOrder(10,7,3);std::set<size_t> seen(order.begin(),order.end());test(order.size()==10&&seen.size()==10&&order[0]==3&&shuffledOrder(10,7,3)==order,"a shuffle is a whole order from the chosen song");}
    // ---- Phase 5G: the bud, the sounds, settings v15 --------------------------------------------------
    {const auto none=budShape(0,120,340,236,36),full=budShape(1,120,340,236,36);
        test(none.bottom-none.top<1e-9&&std::abs(none.top-119)<1e-9,"a bud starts as nothing at the pill's foot");
        test(std::abs(full.top-(120+budGap))<1e-9&&std::abs(full.bottom-(120+budGap+36))<1e-9&&std::abs(full.left-222)<1e-9&&std::abs(full.right-458)<1e-9&&std::abs(full.radius-18)<1e-9,"a grown bud is its own pill below");
        bool grows=true,attached=true;double last=-1;for(int k=0;k<=100;++k){const auto b=budShape(k/100.,120,340,236,36);if(b.bottom<last-1e-9)grows=false;last=b.bottom;if(k<=55&&b.top>119+1e-9)attached=false;if(b.radius>(b.bottom-b.top)/2+1e-9||b.radius>(b.right-b.left)/2+1e-9)grows=false;}
        test(grows&&attached,"a bud grows down while it touches the pill, then lets go");test(budShape(1.2,120,340,236,36).bottom>full.bottom,"an overshoot stretches it a little");
        for(auto kind:{Sound::Chime,Sound::Click}){const auto w=soundWave(kind);test(w.size()>44&&std::memcmp(w.data(),"RIFF",4)==0&&std::memcmp(w.data()+8,"WAVEfmt ",8)==0,"the sounds are WAV files");
            int peak=0;for(size_t i=44;i+1<w.size();i+=2){int16_t v;std::memcpy(&v,w.data()+i,2);peak=std::max(peak,std::abs(int(v)));}int16_t tail;std::memcpy(&tail,w.data()+w.size()-2,2);
            test(peak>1000&&peak<32767/6&&std::abs(int(tail))<=2,"the sounds are quiet and end in silence");}
        std::stringstream v14("version 14\nsharing 1\n");auto s=Settings::parse(v14);test(s.clipboardKeep&&s.sounds&&s.handoff&&s.islandDj&&s.stackAlerts&&s.musicLibrary&&s.sharing,"v14 settings gain the v15 features, on");
        Settings off;off.clipboardKeep=off.sounds=off.handoff=off.islandDj=off.stackAlerts=off.musicLibrary=false;std::stringstream io;off.write(io);auto back=Settings::parse(io);
        test(!back.clipboardKeep&&!back.sounds&&!back.handoff&&!back.islandDj&&!back.stackAlerts&&!back.musicLibrary&&back.version==Settings::currentVersion,"v15 settings round-trip");}
    // ---- 0.17.0-preview.2: a playing session's position moves on from when the player last reported it --------
    {const int64_t at=133000000000000000ll;
        test(std::abs(timelinePosition(10,200,true,at,at+25000000)-12.5)<1e-9,"a playing position moves on by the time since the player's report");
        test(timelinePosition(10,200,false,at,at+25000000)==10&&timelinePosition(10,200,true,0,at)==10&&timelinePosition(10,200,true,at,at-5)==10,"paused, unreported or from the future, it stays");
        test(timelinePosition(199,200,true,at,at+50000000)==200&&timelinePosition(10,200,true,at,at+int64_t(7*3600)*10000000)==10,"never past the end, and a report hours old is not trusted");}
    // ---- 0.17.0-preview.2: numbers in glass expressions are plain decimals (no exponents) ------------------
    {bool plain=true,close=true;for(double v:{2.09591124e-05,-3.3e-7,1e-12,0.,8.,-8.5,1234567.891,1e11,0.000123456789,-0.999999999,15.2173913,std::nan(""),1e300}){
            const auto s=expressionNumber(v);if(s.find_first_of(L"eE")!=std::wstring::npos||s.find(L"nan")!=std::wstring::npos||s.find(L"inf")!=std::wstring::npos)plain=false;
            const double back=std::wcstod(s.c_str()+1,nullptr),want=std::isfinite(v)&&std::abs(v)>=1e-9?std::clamp(v,-1e12,1e12):0.;if(std::abs(back-want)>std::max(1e-9,std::abs(want)*1e-8))close=false;}
        test(plain&&expressionNumber(0)==L"(0)"&&expressionNumber(8)==L"(8)"&&expressionNumber(-0.5)==L"(-0.5)","expression numbers are plain decimals (the parser rejects exponents)");test(close,"and keep about nine significant digits");
        // A nearly settled spring (tiny terms) still makes an expression without exponents.
        Spring nearly{22};nearly.reset(22.00002,0,.0004);nearly.retarget(22,0,{1,380,30});const auto x=springExpression(SpringTerms::from(nearly,.9),1.25);test(x.find_first_of(L"eE")==std::wstring::npos,"a nearly settled spring's expression has no exponents");}
    // ---- 0.17.0-preview.2: every property the glass reads or animates exists from the start ------------
    {std::ifstream in(std::string(NEXUS_SOURCE_DIR)+"/src/Composition/GlassBackdrop.cpp");std::stringstream text;text<<in.rdbuf();const std::string src=text.str();test(src.size()>10000,"the glass source is read");
        std::set<std::string> known;for(auto* k:glassProperties){std::string n;for(const wchar_t* c=k;*c;++c)n+=char(*c);known.insert(n);}
        std::set<std::string> used;const std::regex read("p\\.([a-z]+)"),started("start\\(props,L\"([a-z]+)\"");
        // Expressions live in wide string literals; p.<name> is read only there (elsewhere p is often a C++ variable).
        const std::regex literal("L\"((?:[^\"\\\\]|\\\\.)*)\"");
        for(auto lit=std::sregex_iterator(src.begin(),src.end(),literal);lit!=std::sregex_iterator();++lit){const std::string body=(*lit)[1];
            for(auto it=std::sregex_iterator(body.begin(),body.end(),read);it!=std::sregex_iterator();++it)used.insert((*it)[1]);}
        for(auto it=std::sregex_iterator(src.begin(),src.end(),started);it!=std::sregex_iterator();++it)used.insert((*it)[1]);
        std::string missing;for(auto& u:used)if(!known.count(u))missing+=u+" ";
        test(used.size()>=15&&missing.empty(),("the glass's properties all exist before its expressions and springs start (missing: "+missing+")").c_str());}
    // ---- 0.18: updates, every weather reading, every battery reading, settings v18 ----------------------------
    {test(parseVersion("0.17.0-preview.3").valid&&parseVersion("v0.18.0").valid&&!parseVersion("0.18").valid&&!parseVersion("0.18.0-beta.1").valid&&!parseVersion("x0.1.0").valid&&!parseVersion("0.18.0-preview.").valid,"versions are read strictly");
        test(parseVersion("0.17.0-preview.3")<parseVersion("0.17.0-preview.10")&&parseVersion("0.17.0-preview.10")<parseVersion("0.17.0")&&parseVersion("0.17.0")<parseVersion("0.18.0-preview.1")&&parseVersion("0.9.9")<parseVersion("0.10.0"),"a final release follows its previews, and numbers compare as numbers");
        test(versionText(parseVersion("v0.18.0-preview.2"))==L"0.18.0-preview.2"&&versionText(parseVersion("1.0.0"))==L"1.0.0","versions print back as written");
        const std::string list=R"([{"tag_name":"v0.19.0-preview.1","draft":true,"assets":[{"name":"Arnav-Island-0.19.0-preview.1-win-x64.zip","browser_download_url":"https://github.com/Arnav-Dugad/arnav-island/releases/download/v0.19.0-preview.1/a-win-x64.zip"},{"name":"a-win-x64.zip.sha256","browser_download_url":"https://github.com/Arnav-Dugad/arnav-island/releases/download/v0.19.0-preview.1/a-win-x64.zip.sha256"}]},
            {"tag_name":"v0.18.0-preview.2","draft":false,"prerelease":true,"assets":[{"name":"Arnav-Island-0.18.0-preview.2-win-x64.zip","browser_download_url":"https://evil.example/x-win-x64.zip"},{"name":"Arnav-Island-0.18.0-preview.2-win-x64.zip.sha256","browser_download_url":"https://github.com/Arnav-Dugad/arnav-island/releases/download/v0.18.0-preview.2/x-win-x64.zip.sha256"}]},
            {"tag_name":"v0.18.0-preview.1","draft":false,"prerelease":true,"assets":[{"name":"Arnav-Island-0.18.0-preview.1-win-x64.zip","browser_download_url":"https://github.com/Arnav-Dugad/arnav-island/releases/download/v0.18.0-preview.1/Arnav-Island-0.18.0-preview.1-win-x64.zip"},{"name":"Arnav-Island-0.18.0-preview.1-win-x64.zip.sha256","browser_download_url":"https://github.com/Arnav-Dugad/arnav-island/releases/download/v0.18.0-preview.1/Arnav-Island-0.18.0-preview.1-win-x64.zip.sha256"}]},
            {"tag_name":"v0.17.0-preview.3","draft":false,"assets":[]}])";
        auto chosen=newestRelease(list,parseVersion("0.17.0-preview.3"));
        test(chosen&&versionText(chosen->version)==L"0.18.0-preview.1"&&chosen->zipUrl.ends_with("0.18.0-preview.1-win-x64.zip")&&chosen->shaUrl.ends_with(".sha256"),"the newest published release with both files from the repository itself is chosen (drafts and other addresses are skipped)");
        test(!newestRelease(list,parseVersion("0.18.0-preview.1"))&&!newestRelease("{}",parseVersion("0.1.0"))&&!newestRelease("not json",parseVersion("0.1.0")),"nothing newer, or nothing readable: no update");
        const std::string digest="03b44e71dadfbe4b041cafe53928399adc2bd1ada9d95d2d66457a6a127f5d21";
        test(checksumMatches(digest+"  Arnav-Island.zip\r\n",digest)&&checksumMatches("03B44E71DADFBE4B041CAFE53928399ADC2BD1ADA9D95D2D66457A6A127F5D21",digest)&&!checksumMatches(digest.substr(0,63)+"0  a.zip",digest)&&!checksumMatches(digest+"0",digest)&&!checksumMatches("",digest),"checksums must match exactly, in either case");
        auto [host,path]=splitUrl("https://github.com/Arnav-Dugad/arnav-island/releases/download/v1/a.zip");test(host==L"github.com"&&path==L"/Arnav-Dugad/arnav-island/releases/download/v1/a.zip"&&splitUrl("http://github.com/a").first.empty(),"only HTTPS addresses are followed");}
    {const auto full=parseForecast(R"({"utc_offset_seconds":3600,"current":{"temperature_2m":14.7,"relative_humidity_2m":62,"apparent_temperature":12.5,"is_day":0,"precipitation":0.2,"weather_code":61,"cloud_cover":100,"pressure_msl":1021.3,"wind_speed_10m":10.4,"wind_direction_10m":337,"wind_gusts_10m":22.7,"visibility":25840.0,"uv_index":0.0,"dew_point_2m":7.5},
            "hourly":{"time":["2026-09-26T04:00","2026-09-26T05:00","2026-09-26T06:00"],"temperature_2m":[15.0,14.4,999],"weather_code":[3,61,0],"precipitation_probability":[0,40,0],"is_day":[0,0,1]},
            "daily":{"sunrise":["2026-09-26T06:52"],"sunset":["2026-09-26T18:49"],"temperature_2m_max":[19.8],"temperature_2m_min":[13.2],"precipitation_sum":[1.4],"precipitation_probability_max":[60],"uv_index_max":[3.0],"daylight_duration":[43034.39]}})");
        test(full&&full->humidity==62&&full->feels==12.5&&full->windFrom==337&&full->gusts==22.7&&full->pressure==1021.3&&full->visibility==25840&&full->cloud==100&&full->dewPoint==7.5&&full->uv==0&&full->precipitation==.2,"the forecast brings every current reading");
        test(full&&full->high==19.8&&full->low==13.2&&full->rainTotal==1.4&&full->rainChance==60&&full->uvMax==3&&std::abs(full->daylight-43034.39)<1e-6,"and today's high, low, rain, UV and daylight");
        test(full&&full->hours.size()==2&&full->hours[1].code==61&&full->hours[1].rain==40&&full->hours[0].time==localToUnix("2026-09-26T04:00",3600),"and the next hours (an impossible one is left out)");
        const auto bare=parseForecast(R"({"current":{"temperature_2m":10,"weather_code":0,"relative_humidity_2m":150,"wind_speed_10m":"fast"}})");
        test(bare&&std::isnan(bare->humidity)&&std::isnan(bare->wind)&&std::isnan(bare->high)&&bare->hours.empty(),"missing or impossible readings stay unknown");
        WeatherNow air;test(parseAirQuality(R"({"current":{"us_aqi":55,"pm2_5":6.1}})",air)&&air.aqi==55&&air.pm25==6.1&&!parseAirQuality("{}",air),"air quality is read when it's there");
        test(std::wstring(compassPoint(0))==L"N"&&std::wstring(compassPoint(337))==L"NNW"&&std::wstring(compassPoint(359))==L"N"&&std::wstring(compassPoint(225))==L"SW"&&std::wstring(compassPoint(NAN)).empty(),"wind directions are compass points");
        test(std::wstring(uvWord(2))==L"Low"&&std::wstring(uvWord(7))==L"High"&&std::wstring(uvWord(12))==L"Extreme"&&std::wstring(aqiWord(40))==L"Good"&&std::wstring(aqiWord(120))==L"Sensitive"&&std::wstring(aqiWord(350))==L"Hazardous","UV and air quality in words");
        test(windText(16.09,1)==L"10 mph"&&windText(10.4,0)==L"10 km/h"&&distanceText(25840,0)==L"26 km"&&distanceText(9200,0)==L"9.2 km"&&distanceText(1609.344,1)==L"1.0 mi"&&pressureText(1013.25,0)==L"1013 hPa"&&pressureText(1013.25,1)==L"29.92 inHg"&&rainText(3.4,0)==L"3.4 mm"&&rainText(25.4,1)==L"1.00 in","readings in either unit");
        test(forecastPath(WeatherPlace{L"x",51.51,-0.13}).find(L"apparent_temperature")!=std::wstring::npos&&forecastPath(WeatherPlace{L"x",51.51,-0.13}).find(L"forecast_hours=13")!=std::wstring::npos&&airQualityPath(WeatherPlace{L"x",51.51,-0.13}).find(L"us_aqi")!=std::wstring::npos,"the forecast asks for all of it, at the rounded coordinates");}
    {BatteryReading r;r.voltageMv=12500;r.rateMw=-10000;r.fullMwh=50000;r.lowMwh=2500;r.temperatureDeciK=3041;
        test(r.currentMa()==-800&&r.percentOf(r.lowMwh)==5&&std::abs(r.celsius()-30.95)<1e-9,"current, levels and temperature come from the battery's readings");
        r.relative=true;BatteryReading hot;hot.temperatureDeciK=9000;test(r.currentMa()==0&&r.percentOf(2500)==-1&&std::isnan(hot.celsius())&&std::isnan(BatteryReading{}.celsius()),"relative units and impossible temperatures stay unknown");
        test(chemistryName(L"LION")==L"Lithium-ion"&&chemistryName(L"LiP")==L"Lithium polymer"&&chemistryName(L"NiMH")==L"Nickel-metal hydride"&&chemistryName(L"OOI0").empty(),"chemistry codes in words; unknown codes are left out");}
    {std::stringstream v17("version 17\nclearTint 30\n");auto s=Settings::parse(v17);test(s.version==Settings::currentVersion&&s.clearTint==30&&s.weatherGlass&&s.autoUpdate,"v17 settings gain the v18 features (weather on the glass, updates)");
        Settings off;off.weatherGlass=false;off.autoUpdate=false;std::stringstream io;off.write(io);auto back=Settings::parse(io);test(!back.weatherGlass&&!back.autoUpdate&&back.version==Settings::currentVersion,"v18 settings round-trip");
        int found=0;for(auto& i:settingItems()){if(i.key=="weatherGlass"&&i.section==2)++found;if(i.key=="autoUpdate"&&i.section==9)++found;if(i.action==SettingAction::CheckUpdates&&i.section==9)++found;}test(found==3,"Settings has weather on the glass, automatic updates and Check now");}
    // ---- 0.17.0-preview.3: settings v17, the sky at sunrise and sunset ---------------------------------------
    {std::stringstream v16("version 16\nglassTint 70\n");auto s=Settings::parse(v16);test(s.version==Settings::currentVersion&&s.glassTint==70&&s.clearTint==50&&s.beatEdge&&s.restFrost,"v16 settings gain the v17 features (Clear's own tint, the beat light, the settling frost)");
        Settings off;off.clearTint=20;off.beatEdge=false;off.restFrost=false;std::stringstream io;off.write(io);auto back=Settings::parse(io);test(back.clearTint==20&&!back.beatEdge&&!back.restFrost&&back.version==Settings::currentVersion,"v17 settings round-trip");
        std::stringstream wild("version 17\nclearTint 300\n");test(Settings::parse(wild).clearTint==100,"Clear's tint is at most 100%");
        Settings m;m.glassTint=30;m.clearTint=80;m.material=1;const int frosted=m.tintFor();m.material=2;const int clear=m.tintFor();test(frosted==30&&clear==80,"each glass uses its own tint");
        int found=0;for(auto& i:settingItems()){if(i.key=="clearTint"&&i.control==SettingControl::Slider&&i.section==2)++found;if(i.key=="restFrost"&&i.control==SettingControl::Toggle&&i.section==2)++found;if(i.key=="beatEdge"&&i.control==SettingControl::Toggle&&i.section==5)++found;if(i.control==SettingControl::Glass&&i.section==2&&i.key.empty())++found;}
        test(found==4,"Settings has Clear tint, the frost, the beat light and the glass preview");
        test(localToUnix("2026-09-26T06:12",19800)==1790383320&&localToUnix("2026-09-26T18:20",19800)==1790427000&&localToUnix("1969-12-31T23:00",0)==-3600,"local sunrise and sunset times become the right instants");
        test(localToUnix("sunrise",0)==0&&localToUnix("2026-13-01T06:00",0)==0&&localToUnix("2026-09-26",0)==0,"unreadable times are refused");
        const int64_t rise=1790383320,set=1790427000;
        test(sunPhase(rise-600,rise,set,true)==SunPhase::Dawn&&sunPhase(rise+7200,rise,set,true)==SunPhase::Day&&sunPhase(set-1200,rise,set,true)==SunPhase::Dusk&&sunPhase(set+1800,rise,set,false)==SunPhase::Dusk&&sunPhase(set+7200,rise,set,false)==SunPhase::Night,"dawn, day, dusk and night fall where the sun is");
        test(sunPhase(rise+86400-600,rise,set,false)==SunPhase::Dawn&&sunPhase(rise-86400+7200,rise,set,true)==SunPhase::Day,"yesterday's and tomorrow's times carry over to today");
        test(sunPhase(rise,0,set,true)==SunPhase::Day&&sunPhase(rise,rise,0,false)==SunPhase::Night&&sunPhase(rise,set,rise,true)==SunPhase::Day,"without both times the sky follows day and night");
        auto today=parseForecast(R"({"utc_offset_seconds":19800,"current":{"temperature_2m":24.5,"weather_code":2,"is_day":1},"daily":{"time":["2026-09-26"],"sunrise":["2026-09-26T06:12"],"sunset":["2026-09-26T18:20"]}})");
        test(today&&today->sunrise==rise&&today->sunset==set&&today->code==2,"the forecast brings today's sunrise and sunset");
        auto bare=parseForecast(R"({"current":{"temperature_2m":10,"weather_code":0,"is_day":1},"daily":{"sunrise":["2026-09-26T18:00"],"sunset":["2026-09-26T06:00"]}})");
        test(bare&&bare->sunrise==0&&bare->sunset==0,"a sunset before its sunrise is refused");
        test(forecastPath(WeatherPlace{L"x",13.35,74.79}).find(L"daily=sunrise,sunset")!=std::wstring::npos,"the forecast asks for sunrise and sunset");}
    // ---- Phase 5H: settings v16, the alerts side by side, time left, towns ------------------------------
    {std::stringstream v15("version 15\nsharing 1\n");auto s=Settings::parse(v15);test(s.shelfOpen&&s.crossfade==6&&s.sharing,"v15 settings gain the v16 features (the Shelf open to your PCs, a 6 s crossfade)");
        Settings off;off.shelfOpen=false;off.crossfade=0;std::stringstream io;off.write(io);auto back=Settings::parse(io);test(!back.shelfOpen&&back.crossfade==0&&back.version==Settings::currentVersion,"v16 settings round-trip");
        std::stringstream wild("version 16\ncrossfade 40\n");test(Settings::parse(wild).crossfade==12,"a crossfade is at most 12 s");
        bool listed=false,town=false;for(auto& i:settingItems())if(i.key=="crossfade"&&i.control==SettingControl::Slider&&i.hi==12)listed=true;else if(i.key=="weatherTown"&&i.control==SettingControl::Town&&i.section==4)town=true;
        test(listed&&town,"Settings has the crossfade slider and the Town field");
        const auto below=budShape(1,120,340,236,36);const auto same=budSpreadShape(1,0,28,120,340,526,30,236,36);
        test(std::abs(same.left-below.left)+std::abs(same.top-below.top)+std::abs(same.right-below.right)+std::abs(same.bottom-below.bottom)<1e-9,"unspread, the bud stays below the pill");
        const auto side=budSpreadShape(1,1,28,120,340,526,30,236,36);
        test(std::abs(side.left-(526+budGap))<1e-9&&std::abs(side.right-(526+budGap+236))<1e-9&&std::abs(side.top-28)<1e-9&&std::abs(side.bottom-120)<1e-9&&side.radius<=30+1e-9,"spread, it is a card as tall as the pill, beside it");
        bool smooth=true;BudShape last=same;for(int k=1;k<=50;++k){const auto b=budSpreadShape(1,k/50.,28,120,340,526,30,236,36);if(b.left<last.left-1e-9||b.top>last.top+1e-9||b.radius>(b.bottom-b.top)/2+1e-9||b.radius>(b.right-b.left)/2+1e-9)smooth=false;last=b;}
        test(smooth,"it glides there, its corners always fitting it");
        test(leftText(40)==L"40 s left"&&leftText(.3)==L"1 s left"&&leftText(719)==L"12 min left"&&leftText(3900)==L"1 h 5 min left"&&leftText(-1).empty()&&leftText(1e9).empty(),"time left reads naturally");
        const auto many=parseGeocodeAll(R"({"results":[{"name":"Manipal","admin1":"Karnataka","country":"India","latitude":13.35,"longitude":74.79},{"name":"Manipal","admin1":"Karnataka","country":"India","latitude":13.36,"longitude":74.78},{"name":"Manipal","admin1":"Gandaki Pradesh","country":"Nepal","latitude":28.2,"longitude":83.9},{"name":"Jubail","admin1":"Eastern Province","country":"Saudi Arabia","latitude":27.0,"longitude":49.66}]})",8);
        test(many.size()==3&&many[0].name==L"Manipal, Karnataka, India"&&many[1].name==L"Manipal, Gandaki Pradesh, Nepal"&&many[2].name==L"Jubail, Eastern Province, Saudi Arabia","towns are named with their region, and listed once");
        test(parseGeocodeAll(R"({"results":[{"name":"A","country":"X","latitude":1,"longitude":1},{"name":"B","country":"X","latitude":2,"longitude":2}]})",1).size()==1&&geocodePath(L"Udupi",8).find(L"count=8")!=std::string::npos&&geocodePath(L"x",99).find(L"count=10")!=std::string::npos,"a search asks for up to ten");}
    std::cout<<"PASS "<<checks<<" glass expression, glide, spectrum, settings-model, identity, brand, device, battery, auto-hide, command, clipboard, workspace, privacy, waveform, lab, accent, lyrics, palette, seeking, audio-route, currency, system-action, completion, file-search, command-memory, rolling-digit, GPU, colour, typo, row-group, left-dock, clipboard-history, library, bud, sound and v15 checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
