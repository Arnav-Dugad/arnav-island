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
    // ---- Phase 4: command language -------------------------------------------------
    {const std::vector<InstalledApp> apps{{L"Microsoft Edge",L"MSEdge"},{L"Visual Studio Code",L"Microsoft.VisualStudioCode"},{L"Spotify",L"SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify"},{L"Uninstall Spotify",L"u"},{L"Calculator",L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"}};
    const std::vector<std::wstring> saved{L"Study",L"Games"};const std::wstring scope=L"C:\\Users\\Sam";
    auto first=[&](const std::wstring& t){auto r=parseCommand(t,apps,saved,scope);return r.empty()?CommandResult{}:r.front();};
    test(parseCommand(L"   ",apps,saved,scope).empty(),"empty command has no results");
    test(first(L"volume 30").kind==CommandKind::Volume&&first(L"volume 30").value==30&&first(L"Set volume to 45%").value==45,"volume levels");
    test(first(L"volume up").kind==CommandKind::VolumeStep&&first(L"volume up").value==10&&first(L"quieter").value==-10,"volume steps");
    test(first(L"volume 130").kind==CommandKind::None&&first(L"volume").kind==CommandKind::None,"out-of-range and bare volume do nothing");
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
    std::cout<<"PASS "<<checks<<" glass expression, glide, spectrum, settings-model, identity, brand, device, battery, auto-hide, command, clipboard, workspace and privacy checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
