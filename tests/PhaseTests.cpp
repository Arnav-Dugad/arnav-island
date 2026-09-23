#include "Animation/SpringExpression.h"
#include "Audio/Spectrum.h"
#include "Settings/SettingsModel.h"
#include "Media/AppIdentity.h"
#include "Design/SvgPath.h"
#include "Design/BrandMatch.h"
#include "Interaction/AutoHide.h"
#include "Hardware/BatteryModel.h"
#include "Hardware/BluetoothProvider.h"
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <chrono>
using namespace nexus;
static int checks=0;
static void test(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);}
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
    for(auto& k:keys)test(covered.contains(k),"settings window exposes every persisted preference");
    for(auto& item:items){if(item.control==SettingControl::Button||item.control==SettingControl::Note||item.control==SettingControl::Order)continue;
        for(int v=item.lo;v<=item.hi;v+=std::max(1,(item.hi-item.lo)/12)){Settings s;item.set(s,v);std::stringstream io;s.write(io);auto back=Settings::parse(io);test(item.get(back)==item.get(s),"each control value round-trips through the settings file");}
        if(item.control==SettingControl::Slider||item.control==SettingControl::Choice||item.control==SettingControl::Swatch){Settings s;item.set(s,item.hi+1000);test(item.get(s)<=item.hi,"controls clamp above range");item.set(s,item.lo-1000);test(item.get(s)>=item.lo,"controls clamp below range");}}
    for(int a=0;a<7;++a)for(int slot=0;slot<3;++slot){auto metrics=defaultMetrics;assignMetric(metrics,slot,a);test(metrics[slot]==a&&metrics[0]!=metrics[1]&&metrics[1]!=metrics[2]&&metrics[0]!=metrics[2],"statistics stay unique when assigned");}
    {Settings s;for(auto& item:items)if(item.control==SettingControl::Order)for(int d:{1,-1,1,1,-1}){item.set(s,d);test(validNavigation(s.navigation),"navigation order stays a permutation");}}
    std::stringstream v5("version 5\nglass 1\nuiMode 2\ncompactWidth 300\n");auto migrated=Settings::parse(v5);test(migrated.material==0&&migrated.uiMode==2&&migrated.compactWidth==300&&migrated.waveform&&migrated.hud,"v5 settings migrate; legacy interior glass is not reinterpreted");
    Settings glass;glass.material=1;test(glass.floating()&&glass.gap()==8,"glass floats with a gap");glass.verticalOffset=20;test(glass.gap()==0,"explicit offset replaces the glass gap");Settings solid;test(!solid.floating()&&solid.gap()==0,"solid stays attached");
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
    std::cout<<"PASS "<<checks<<" glass expression, glide, spectrum, settings-model, identity, brand, device, battery and auto-hide checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
