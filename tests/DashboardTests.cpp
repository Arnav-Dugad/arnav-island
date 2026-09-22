#include "Interaction/DashboardModel.h"
#include "Persistence/Settings.h"
#include "Composition/DockGeometry.h"
#include "Animation/MotionEngine.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace nexus;
static int checks=0;
static void test(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);}
int main(){try{
    FocusClock c;test(c.displayed(0)==1500,"focus default");c.toggle(10);test(c.displayed(70)==1440,"elapsed wall clock");c.toggle(70);test(c.displayed(700)==1440,"pause holds");c.toggle(800);test(!c.tick(2239),"does not finish early");test(c.tick(2240),"finish resumes remainder");test(!c.tick(3000),"completion only once");c.toggle(4000);test(c.running&&c.displayed(4000)==1500,"restart after completion");c.select(FocusClock::Mode::Break,0);test(c.displayed(0)==300,"break duration");c.select(FocusClock::Mode::Stopwatch,0);c.toggle(10);test(c.displayed(3610)==3600&&!c.tick(9000),"stopwatch unbounded");c.reset(9000);test(!c.running&&c.displayed(9000)==0,"stopwatch reset");
    test(classifyMedia(1,L"browser")==MediaKind::Music,"OS music wins");test(classifyMedia(2,L"Spotify")==MediaKind::Video,"OS video wins");test(classifyMedia(0,L"Spotify.exe")==MediaKind::Music,"known music app");test(classifyMedia(0,L"chrome.exe")==MediaKind::Unknown,"never guess browser content");
    HitTarget hit{Action::Play,10,10,30,30};test(hit.contains(10,10)&&!hit.contains(40,40),"half-open hit bounds");hit.enabled=false;test(!hit.contains(20,20),"disabled action cannot hit");
    Settings settings;settings.hoverOpen=false;settings.hoverDelay=430;settings.mediaLayout=2;std::stringstream f;settings.write(f);auto copy=Settings::parse(f);test(!copy.hoverOpen&&copy.hoverDelay==430&&copy.mediaLayout==2,"new settings roundtrip");
    std::stringstream legacy("version 1\npreset 2\n");auto migrated=Settings::parse(legacy);test(migrated.version==3&&migrated.hoverOpen&&migrated.preset==2,"v1 migration");
    std::stringstream bounds("version 2\nhoverDelay 999\nmediaLayout -2\n");auto bounded=Settings::parse(bounds);test(bounded.hoverDelay==700&&bounded.mediaLayout==0,"settings clamp");
    bool rejected=false;try{std::stringstream bad("version 2\nhoverDelay");Settings::parse(bad);}catch(...){rejected=true;}test(rejected,"truncated value rejected");
    test(clockText(3661)==L"1:01:01"&&clockText(-1)==L"00:00","time formatting");
        Settings appearance;appearance.edge=1;appearance.theme=2;appearance.scale=110;appearance.glass=false;appearance.directAudio=false;appearance.compactWidth=176;appearance.startAtLogin=false;std::stringstream pref;appearance.write(pref);auto restored=Settings::parse(pref);test(restored.edge==1&&restored.theme==2&&restored.scale==110&&!restored.glass&&!restored.directAudio&&restored.compactWidth==176&&!restored.startAtLogin,"v3 appearance roundtrip");
    for(int edge:{0,1})for(bool attached:{false,true})for(auto g:{geometry(IslandState::Compact),geometry(IslandState::Expanded)}){auto points=dockOutline(g.width,g.height,g.radius,edge,attached);test(points.size()>40,"curved polygon coverage");for(auto point:points)test(std::isfinite(point.x)&&std::isfinite(point.y)&&point.x>=-g.radius-.01&&point.x<=g.width+g.radius+.01&&point.y>=-g.radius-.01&&point.y<=g.height+g.radius+.01,"outline finite bounded");auto origin=bodyOrigin(g.width,g.height,600,500,edge);test(edge?origin.x+g.width==600:origin.x+g.width/2==300,"edge anchored");}
    MotionEngine motion;motion.edge=1;motion.target(IslandState::Compact,1);test(motion.reveal.target()==0,"right compact has no expanded content");motion.target(IslandState::Expanded,1.1);auto before=motion.width.sample(1.2);motion.target(IslandState::Compact,1.2);auto after=motion.width.sample(1.2);test(std::abs(before.position-after.position)<1e-9&&std::abs(before.velocity-after.velocity)<1e-9,"size reversal preserves momentum");
    std::cout<<"PASS "<<checks<<" dashboard, classification, timer, and migration checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
