// Sharing between your own PCs, end to end over loopback: two services with their own identities
// pair (both confirm the same code), refuse files before pairing and after a PC forgets the other,
// carry a declined and an accepted transfer (byte for byte), and keep their identity and pairing.
#include "Productivity/ShareService.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
using namespace nexus;
namespace fs=std::filesystem;
static int failures=0,checks=0;
#define CHECK(c) do{++checks;if(!(c)){++failures;std::printf("FAIL %s:%d  %s\n",__FILE__,__LINE__,#c);}}while(0)
// Waits for an event of `kind`, keeping the rest for later waits.
static bool waitFor(ShareService& s,std::vector<ShareEvent>& pending,ShareEvent::Kind kind,ShareEvent& out,int ms=15000){
    const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
    for(;;){for(auto e:s.take())pending.push_back(e);
        for(size_t i=0;i<pending.size();++i)if(pending[i].kind==kind){out=pending[i];pending.erase(pending.begin()+long(i));return true;}
        if(std::chrono::steady_clock::now()>until)return false;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
}
static std::vector<char> read(const fs::path& p){std::ifstream in(p,std::ios::binary);return std::vector<char>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
int main(){
    CHECK(safeShareName(L"..\\x/evil.txt")==L"evil.txt");CHECK(safeShareName(L"CON.txt")==L"_CON.txt");CHECK(safeShareName(L"a<b>.txt")==L"a_b_.txt");
    CHECK(safeShareName(L"  .hidden. ")==L"hidden");CHECK(safeShareName(L"")==L"file");CHECK(safeShareName(std::wstring(300,L'a')+L".pdf").size()==120);CHECK(safeShareName(std::wstring(300,L'a')+L".pdf").ends_with(L".pdf"));
    const fs::path root=fs::temp_directory_path()/(L"arnav-share-test-"+std::to_wstring(GetTickCount64()));fs::create_directories(root);
    const fs::path file=root/L"hello.bin";{std::mt19937 random(7);std::ofstream out(file,std::ios::binary);for(int i=0;i<1000003;++i)out.put(char(random()&255));}
    auto options=[&](const wchar_t* who,uint16_t port){ShareOptions o;o.tcpPort=port;o.discovery=false;o.loopback=true;o.folder=(root/who).wstring();o.downloads=(root/(std::wstring(who)+L"-downloads")).wstring();o.name=std::wstring(L"PC ")+who;return o;};
    std::string idA;
    {
        auto a=std::make_unique<ShareService>(nullptr,options(L"A",47920));ShareService b(nullptr,options(L"B",47930));std::vector<ShareEvent> pa,pb;ShareEvent e,f;
        CHECK(a->running());CHECK(b.running());CHECK(a->id().size()==32);CHECK(a->id()!=b.id());idA=a->id();
        a->addPeer(b.id(),L"PC B","127.0.0.1",47930);b.addPeer(a->id(),L"PC A","127.0.0.1",47920);
        // Before pairing, nothing is sent.
        a->send(b.id(),file.wstring());CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"Pair")!=std::wstring::npos);
        // A declined pairing pairs neither.
        a->pair(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::PairCode,e));CHECK(waitFor(b,pb,ShareEvent::Kind::PairCode,f));CHECK(e.code==f.code);
        a->confirmPair(true);b.confirmPair(false);CHECK(waitFor(*a,pa,ShareEvent::Kind::PairFailed,e));CHECK(waitFor(b,pb,ShareEvent::Kind::PairFailed,f));
        CHECK(!a->peers().empty()&&!a->peers()[0].paired);
        // Both confirm the same six-digit code.
        a->pair(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::PairCode,e));CHECK(waitFor(b,pb,ShareEvent::Kind::PairCode,f));CHECK(e.code==f.code);CHECK(e.code<1000000);CHECK(f.name==L"PC A");
        a->confirmPair(true);b.confirmPair(true);CHECK(waitFor(*a,pa,ShareEvent::Kind::Paired,e));CHECK(waitFor(b,pb,ShareEvent::Kind::Paired,f));
        CHECK(a->peers().size()==1&&a->peers()[0].paired&&a->peers()[0].online);CHECK(b.peers().size()==1&&b.peers()[0].paired);
        // A declined file.
        a->send(b.id(),file.wstring());CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));CHECK(f.file==L"hello.bin");CHECK(f.size==1000003);CHECK(f.name==L"PC A");
        b.answer(f.transfer,false);CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"declined")!=std::wstring::npos);
        // An accepted file arrives whole, in Downloads; a second copy gets its own name.
        for(int round=0;round<2;++round){
            a->send(b.id(),file.wstring());CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));b.answer(f.transfer,true);
            CHECK(waitFor(b,pb,ShareEvent::Kind::Received,f,30000));CHECK(waitFor(*a,pa,ShareEvent::Kind::Sent,e,30000));
            CHECK(f.file==(round==0?L"hello.bin":L"hello (2).bin"));CHECK(read(f.detail)==read(file));}
        CHECK(!fs::exists(root/L"B-downloads"/L"hello.bin.arnavpart"));
        // The identity and the pairing survive a restart.
        a.reset();a=std::make_unique<ShareService>(nullptr,options(L"A",47922));CHECK(a->id()==idA);CHECK(a->peers().size()==1&&a->peers()[0].paired);
        a->addPeer(b.id(),L"PC B","127.0.0.1",47930);
        // A PC that forgot the other refuses its files.
        b.forget(a->id());a->send(b.id(),file.wstring());CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"doesn't have this PC paired")!=std::wstring::npos);
    }
    std::error_code error;fs::remove_all(root,error);
    std::printf("share tests: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
