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
    for(auto& item:items){if(item.control==SettingControl::Button||item.control==SettingControl::Note||item.control==SettingControl::Order||item.control==SettingControl::Preview||item.control==SettingControl::Actions)continue;
        // Slow motion is a study aid and is deliberately never saved.
        if(item.key!="labSpeed")for(int v=item.lo;v<=item.hi;v+=std::max(1,(item.hi-item.lo)/12)){Settings s;item.set(s,v);std::stringstream io;s.write(io);auto back=Settings::parse(io);test(item.get(back)==item.get(s),"each control value round-trips through the settings file");}
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
    std::cout<<"PASS "<<checks<<" glass expression, glide, spectrum, settings-model, identity, brand, device, battery, auto-hide, command, clipboard, workspace, privacy, waveform, lab, accent, lyrics, palette, seeking and audio-route checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
