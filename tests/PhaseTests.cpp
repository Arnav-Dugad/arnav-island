#include "Animation/SpringExpression.h"
#include "Audio/Spectrum.h"
#include "Settings/SettingsModel.h"
#include "Media/AppIdentity.h"
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
    {Spectrum sp;std::vector<float> block(480,.1f);auto start=std::chrono::steady_clock::now();for(int i=0;i<2000;++i){sp.push(block.data(),480);sp.analyze(48000,.01f);}
        double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/2000;std::cout<<"Spectrum analysis: "<<us<<" us per 1024-point step (100 steps/s while audio plays)\n";test(us<2000,"analysis fits comfortably in its 10 ms cadence");}
    std::cout<<"PASS "<<checks<<" glass expression, glide, spectrum, settings-model and identity checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
