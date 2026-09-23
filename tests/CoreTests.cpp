#include "Animation/MotionEngine.h"
#include "Events/EventOrchestrator.h"
#include "Persistence/Settings.h"
#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
using namespace nexus;
static int count=0;
void require(bool pass,const char* name){++count;if(!pass)throw std::runtime_error(name);}
int main(){try{
    for(auto spec:{SpringSpec{1,400,30},SpringSpec{1,400,40},SpringSpec{1,400,60}}){
        Spring s{20};s.retarget(480,0,spec);auto a=s.sample(.137);s.retarget(204,.137,spec);auto b=s.sample(.137);
        require(std::abs(a.position-b.position)<1e-8,"Position continuous at interruption");require(std::abs(a.velocity-b.velocity)<1e-8,"Velocity continuous at interruption");
        require(std::abs(s.sample(12).position-204)<.001,"All damping regimes converge");
    }
    Spring s{204};s.retarget(480,0,preset(MotionPreset::Fluid));double duration;auto curve=s.curve(0,duration);
    double maxError=0;for(size_t i=0;i<curve.size();++i){auto c=curve[i];double end=i+1<curve.size()?curve[i+1].time:duration;
        for(int j=0;j<=10;++j){double u=(end-c.time)*j/10,p=c.p+u*(c.v+u*(c.quadratic+u*c.cubic));maxError=std::max(maxError,std::abs(p-s.sample(c.time+u).position));}}
    require(maxError<.003,"Compositor curve within 0.003 DIP of physical spring");
    for(double hz:{60.,90.,120.,144.,165.,240.}){Spring a{0};a.retarget(1,0,preset(MotionPreset::Balanced));for(double t=0;t<2;t+=1/hz)require(std::isfinite(a.sample(t).position),"Refresh independent finite state");require(std::abs(a.sample(2).position-1)<.001,"Time-based convergence");}
    std::mt19937 random(42);Spring stress{204};double now=0;for(int i=0;i<10000;++i){now+=.003;auto before=stress.sample(now);stress.retarget(random()%500,now,preset(MotionPreset(i%5)));auto after=stress.sample(now);require(std::abs(before.velocity-after.velocity)<1e-7,"Storm preserves velocity");}
    EventOrchestrator events;for(int i=0;i<100;++i)events.publish({ActivityKind::Volume,"volume",40,double(i),.5,2},i*.001);
    require(events.depth()==0&&events.active()->value==99,"Volume coalesces");
    events.publish({ActivityKind::Power,"critical",100,5,.5,1},.2);require(events.active()->key=="critical","Critical battery preempts");
    events.publish({ActivityKind::Media,"media",10,0,.5,2},.3);require(events.depth()==1,"Lower priority queues");
    require(!events.tick(.6),"Minimum and maximum lifetime respected");require(events.tick(1.3)&&events.active()->key=="media","Queue resumes");
    events.dismiss(1.4);require(!events.active(),"Dismiss active activity");
    for(int i=0;i<1000;++i)events.publish({ActivityKind::Notification,std::to_string(i),0,0,.2,2},2);
    require(events.depth()<=64,"Storm queue bounded");
    Settings settings;settings.preset=4;settings.verticalOffset=9;std::stringstream serial;settings.write(serial);auto round=Settings::parse(serial);require(round.preset==4&&round.verticalOffset==9,"Settings round trip");
    bool rejected=false;try{std::stringstream input("version "+std::to_string(Settings::currentVersion+1)+"\n");Settings::parse(input);}catch(...){rejected=true;}require(rejected,"Future settings rejected");
    std::stringstream invalid("version 1\npreset 999\nverticalOffset -100\n");auto clamped=Settings::parse(invalid);require(clamped.preset==4&&clamped.verticalOffset==0,"Settings bounds");
    for(int i=0;i<=10;++i){auto g=geometry(IslandState(i));require(g.width>0&&g.height>0&&g.radius<=std::min(g.width,g.height)/2,"Geometry valid");}
    require(rubberBand(10000)<90&&rubberBand(-10000)>-90,"Drag resistance bounded");
    MotionEngine reduced;reduced.reduced=true;reduced.target(IslandState::Expanded,0);require(reduced.width.sample(0).position==geometry(IslandState::Expanded).width,"Reduced motion has no spatial transition");
    auto start=std::chrono::steady_clock::now();size_t segments=0;for(int i=0;i<1000;++i){Spring a{204};a.retarget(480,0,preset(MotionPreset::Balanced));double d;segments+=a.curve(0,d).size();}
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"PASS "<<count<<" checks; max curve error "<<maxError<<" DIP; 1000 curves "<<ms<<" ms; "<<segments<<" segments\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
