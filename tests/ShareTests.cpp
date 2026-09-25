// Sharing between your own PCs, end to end over loopback: two services with their own identities
// pair (both confirm the same code), refuse files before pairing and after a PC forgets the other,
// carry a declined and an accepted transfer (byte for byte), and keep their identity and pairing.
// Phase 5G: several files and a folder tree in one transfer, a transfer stopped by either side, a PC
// that announced the older protocol, and music handed off with and without the song's file.
// Phase 5H: the cover travels with the music (only to a PC of revision 1), and one PC looks into the other's
// Shelf and takes a file and a folder from it (not while that Shelf is kept to itself, nor an item since removed).
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
    CHECK(safeShareName(L"  .hidden. ")==L"hidden");
    CHECK((safeSharePath(L"Photos/2024/a.jpg")==std::vector<std::wstring>{L"Photos",L"2024",L"a.jpg"}));CHECK((safeSharePath(L"../../etc/./passwd")==std::vector<std::wstring>{L"etc",L"passwd"}));
    CHECK((safeSharePath(L"C:\\Windows\\x.dll")==std::vector<std::wstring>{L"C_",L"Windows",L"x.dll"}));CHECK(safeSharePath(L"/../..").empty());CHECK(safeSharePath(std::wstring(100,L'a')+L"/"+std::wstring(100,L'/')).size()==1);
    {std::wstring deep;for(int i=0;i<40;++i)deep+=L"d/";deep+=L"f.txt";CHECK(safeSharePath(deep).size()==24);}
    CHECK((safeSharePath(L"a/CON/b.txt")==std::vector<std::wstring>{L"a",L"_CON",L"b.txt"}));
    CHECK(shareTitle({L"Photos"})==L"Photos");CHECK(shareTitle({L"a.txt",L"b.txt",L"c.txt"})==L"a.txt and 2 more");CHECK(safeShareName(L"")==L"file");CHECK(safeShareName(std::wstring(300,L'a')+L".pdf").size()==120);CHECK(safeShareName(std::wstring(300,L'a')+L".pdf").ends_with(L".pdf"));
    const fs::path root=fs::temp_directory_path()/(L"arnav-share-test-"+std::to_wstring(GetTickCount64()));fs::create_directories(root);
    const fs::path file=root/L"hello.bin";{std::mt19937 random(7);std::ofstream out(file,std::ios::binary);for(int i=0;i<1000003;++i)out.put(char(random()&255));}
    auto options=[&](const wchar_t* who,uint16_t port){ShareOptions o;o.tcpPort=port;o.discovery=false;o.loopback=true;o.folder=(root/who).wstring();o.downloads=(root/(std::wstring(who)+L"-downloads")).wstring();o.handoff=(root/(std::wstring(who)+L"-music")).wstring();o.name=std::wstring(L"PC ")+who;return o;};
    std::string idA;
    {
        auto a=std::make_unique<ShareService>(nullptr,options(L"A",47920));ShareService b(nullptr,options(L"B",47930));std::vector<ShareEvent> pa,pb;ShareEvent e,f;
        CHECK(a->running());CHECK(b.running());CHECK(a->id().size()==32);CHECK(a->id()!=b.id());idA=a->id();
        a->addPeer(b.id(),L"PC B","127.0.0.1",47930);b.addPeer(a->id(),L"PC A","127.0.0.1",47920);
        // Before pairing, nothing is sent.
        a->send(b.id(),{file.wstring()});CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"Pair")!=std::wstring::npos);
        // A declined pairing pairs neither.
        a->pair(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::PairCode,e));CHECK(waitFor(b,pb,ShareEvent::Kind::PairCode,f));CHECK(e.code==f.code);
        a->confirmPair(true);b.confirmPair(false);CHECK(waitFor(*a,pa,ShareEvent::Kind::PairFailed,e));CHECK(waitFor(b,pb,ShareEvent::Kind::PairFailed,f));
        CHECK(!a->peers().empty()&&!a->peers()[0].paired);
        // Both confirm the same six-digit code.
        a->pair(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::PairCode,e));CHECK(waitFor(b,pb,ShareEvent::Kind::PairCode,f));CHECK(e.code==f.code);CHECK(e.code<1000000);CHECK(f.name==L"PC A");
        a->confirmPair(true);b.confirmPair(true);CHECK(waitFor(*a,pa,ShareEvent::Kind::Paired,e));CHECK(waitFor(b,pb,ShareEvent::Kind::Paired,f));
        CHECK(a->peers().size()==1&&a->peers()[0].paired&&a->peers()[0].online);CHECK(b.peers().size()==1&&b.peers()[0].paired);
        // A declined file.
        a->send(b.id(),{file.wstring()});CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));CHECK(f.file==L"hello.bin");CHECK(f.size==1000003);CHECK(f.name==L"PC A");
        b.answer(f.transfer,false);CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"declined")!=std::wstring::npos);
        // An accepted file arrives whole, in Downloads; a second copy gets its own name.
        for(int round=0;round<2;++round){
            a->send(b.id(),{file.wstring()});CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));b.answer(f.transfer,true);
            CHECK(waitFor(b,pb,ShareEvent::Kind::Received,f,30000));CHECK(waitFor(*a,pa,ShareEvent::Kind::Sent,e,30000));
            CHECK(f.file==(round==0?L"hello.bin":L"hello (2).bin"));CHECK(read(f.detail)==read(file));}
        CHECK(!fs::exists(root/L"B-downloads"/L"hello.bin.arnavpart"));
        // A folder keeps its tree, under a free name; loose files sit beside it; the offer counts every file.
        {const fs::path album=root/L"Album";fs::create_directories(album/L"Disc 2"/L"Art");
            {std::ofstream(album/L"one.txt",std::ios::binary)<<"first";std::ofstream(album/L"Disc 2"/L"two.txt",std::ios::binary)<<"second";std::ofstream(album/L"Disc 2"/L"Art"/L"cover.bin",std::ios::binary)<<std::string(300000,'x');}
            const fs::path loose=root/L"loose.txt";std::ofstream(loose,std::ios::binary)<<"loose";
            for(int round=0;round<2;++round){
                a->send(b.id(),{album.wstring(),loose.wstring()});CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));CHECK(f.count==4);CHECK(f.size==5+6+300000+5);CHECK(f.folder);CHECK(f.file==L"Album and 1 more");
                b.answer(f.transfer,true);CHECK(waitFor(b,pb,ShareEvent::Kind::Received,f,30000));CHECK(waitFor(*a,pa,ShareEvent::Kind::Sent,e,30000));CHECK(e.count==4);
                const fs::path got=root/L"B-downloads"/(round==0?L"Album":L"Album (2)");CHECK(fs::path(f.detail)==got);
                CHECK(read(got/L"one.txt")==read(album/L"one.txt"));CHECK(read(got/L"Disc 2"/L"two.txt")==read(album/L"Disc 2"/L"two.txt"));CHECK(read(got/L"Disc 2"/L"Art"/L"cover.bin")==read(album/L"Disc 2"/L"Art"/L"cover.bin"));
                CHECK(fs::exists(root/L"B-downloads"/(round==0?L"loose.txt":L"loose (2).txt")));}
            // Stopped by the sender while the other PC is still deciding, and by the receiver halfway.
            pa.clear();pb.clear();
            {const uint32_t t=a->send(b.id(),{album.wstring()});CHECK(t!=0);CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));a->cancel(t);CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e,3000));CHECK(e.transfer==t);CHECK(e.detail==L"You stopped it");
                // The other PC sees the offer go away.
                CHECK(waitFor(b,pb,ShareEvent::Kind::Failed,f,5000));CHECK(f.detail==L"PC A stopped sending it");}
            pa.clear();pb.clear();
            {const fs::path big=root/L"big.bin";{std::ofstream out(big,std::ios::binary);std::string chunk(1<<20,'b');for(int i=0;i<48;++i)out<<chunk;}
                a->send(b.id(),{big.wstring()});CHECK(waitFor(b,pb,ShareEvent::Kind::Offer,f));const uint32_t t=f.transfer;b.answer(t,true);
                CHECK(waitFor(b,pb,ShareEvent::Kind::Progress,f));b.cancel(t);CHECK(waitFor(b,pb,ShareEvent::Kind::Failed,f,30000));CHECK(f.detail==L"You stopped it");CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e,30000));
                CHECK(!fs::exists(root/L"B-downloads"/L"big.bin"));bool parts=false;for(auto& x:fs::directory_iterator(root/L"B-downloads"))if(x.path().extension()==L".arnavpart")parts=true;CHECK(!parts);}}
        // Music: declined, accepted as it is, and accepted with the song's own file.
        {ShareHandoff music;music.title=L"Test Song";music.artist=L"Test Artist";music.album=L"Test Album";music.app=L"Island";music.position=83.5;music.duration=200;music.playing=true;
            const fs::path song=root/L"song.mp3";std::ofstream(song,std::ios::binary)<<std::string(123457,'s');
            a->handoff(b.id(),music,{});CHECK(waitFor(b,pb,ShareEvent::Kind::Handoff,f));CHECK(f.handoff.title==L"Test Song");CHECK(f.handoff.artist==L"Test Artist");CHECK(f.handoff.position==83.5);CHECK(f.handoff.fileSize==0);
            b.answerHandoff(f.transfer,0);CHECK(waitFor(*a,pa,ShareEvent::Kind::HandoffAnswered,e));CHECK(e.code==0);
            a->handoff(b.id(),music,{});CHECK(waitFor(b,pb,ShareEvent::Kind::Handoff,f));b.answerHandoff(f.transfer,1);CHECK(waitFor(*a,pa,ShareEvent::Kind::HandoffAnswered,e));CHECK(e.code==1);
            a->handoff(b.id(),music,song.wstring());CHECK(waitFor(b,pb,ShareEvent::Kind::Handoff,f));CHECK(f.handoff.fileSize==123457);CHECK(f.handoff.fileName==L"song.mp3");
            b.answerHandoff(f.transfer,2);CHECK(waitFor(*a,pa,ShareEvent::Kind::HandoffAnswered,e));CHECK(e.code==2);CHECK(waitFor(b,pb,ShareEvent::Kind::HandoffFile,f,30000));
            CHECK(read(f.detail)==read(song));CHECK(fs::path(f.detail).parent_path()==root/L"B-music");CHECK(f.handoff.position==83.5);
            // The cover, byte for byte (zero bytes included); none goes to a PC that announced revision 0.
            music.cover.resize(40000);for(size_t i=0;i<music.cover.size();++i)music.cover[i]=uint8_t(i*37+(i>>8));music.cover[0]=0;music.cover[5]=0;
            a->handoff(b.id(),music,{});CHECK(waitFor(b,pb,ShareEvent::Kind::Handoff,f));CHECK(f.handoff.cover==music.cover);CHECK(f.handoff.title==L"Test Song");CHECK(f.handoff.fileSize==0);b.answerHandoff(f.transfer,0);CHECK(waitFor(*a,pa,ShareEvent::Kind::HandoffAnswered,e));
            a->addPeer(b.id(),L"PC B","127.0.0.1",47930,shareProtocol,0);a->handoff(b.id(),music,{});CHECK(waitFor(b,pb,ShareEvent::Kind::Handoff,f));CHECK(f.handoff.cover.empty());CHECK(f.handoff.title==L"Test Song");b.answerHandoff(f.transfer,0);CHECK(waitFor(*a,pa,ShareEvent::Kind::HandoffAnswered,e));
            // A revision 0 PC isn't asked for its Shelf.
            a->askShelf(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::ShelfList,e));CHECK(e.code==2);CHECK(e.detail.find(L"Update Arnav Island")!=std::wstring::npos);
            a->addPeer(b.id(),L"PC B","127.0.0.1",47930);CHECK(a->peers()[0].revision==shareRevision);}
        // The Shelf: kept to itself, then shared (a file with its preview, and a folder), taken from, and changed meanwhile.
        {const fs::path notes=root/L"shelf notes.txt";std::ofstream(notes,std::ios::binary)<<std::string(70001,'n');
            const fs::path trip=root/L"Trip";fs::create_directories(trip/L"Day 1");std::ofstream(trip/L"plan.txt",std::ios::binary)<<"plan";std::ofstream(trip/L"Day 1"/L"photo.bin",std::ios::binary)<<std::string(250000,'p');
            b.offerShelf({{notes.wstring(),{1,2,3}},{trip.wstring(),{}}},false);
            a->askShelf(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::ShelfList,e));CHECK(e.code==1);CHECK(e.shelf.empty());CHECK(e.name==L"PC B");
            a->takeFromShelf(b.id(),0,L"shelf notes.txt");CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"no longer")!=std::wstring::npos);
            b.offerShelf({{notes.wstring(),{1,2,3}},{trip.wstring(),{}}},true);
            a->askShelf(b.id());CHECK(waitFor(*a,pa,ShareEvent::Kind::ShelfList,e));CHECK(e.code==0);CHECK(e.shelf.size()==2);
            if(e.shelf.size()==2){CHECK(e.shelf[0].name==L"shelf notes.txt");CHECK(e.shelf[0].size==70001);CHECK(!e.shelf[0].folder);CHECK((e.shelf[0].preview==std::vector<uint8_t>{1,2,3}));
                CHECK(e.shelf[1].name==L"Trip");CHECK(e.shelf[1].folder);CHECK(e.shelf[1].size==4+250000);CHECK(e.shelf[1].preview.empty());}
            // A file: it arrives in Downloads, and the other PC hears it was taken.
            a->takeFromShelf(b.id(),0,L"shelf notes.txt");CHECK(waitFor(*a,pa,ShareEvent::Kind::Received,e,30000));CHECK(e.code==1);CHECK(read(e.detail)==read(notes));CHECK(fs::path(e.detail).parent_path()==root/L"A-downloads");
            CHECK(waitFor(b,pb,ShareEvent::Kind::ShelfTaken,f,30000));CHECK(f.file==L"shelf notes.txt");CHECK(f.name==L"PC A");
            // A folder keeps its tree.
            a->takeFromShelf(b.id(),1,L"Trip");CHECK(waitFor(*a,pa,ShareEvent::Kind::Received,e,30000));CHECK(e.count==2);CHECK(fs::path(e.detail)==root/L"A-downloads"/L"Trip");
            CHECK(read(root/L"A-downloads"/L"Trip"/L"Day 1"/L"photo.bin")==read(trip/L"Day 1"/L"photo.bin"));CHECK(waitFor(b,pb,ShareEvent::Kind::ShelfTaken,f,30000));
            // The Shelf changed since the list: a name at the wrong place is refused.
            b.offerShelf({{trip.wstring(),{}}},true);a->takeFromShelf(b.id(),0,L"shelf notes.txt");CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"no longer")!=std::wstring::npos);
            a->takeFromShelf(b.id(),7,L"Trip");CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"no longer")!=std::wstring::npos);}
        // The identity and the pairing survive a restart.
        a.reset();a=std::make_unique<ShareService>(nullptr,options(L"A",47922));CHECK(a->id()==idA);CHECK(a->peers().size()==1&&a->peers()[0].paired);
        // A PC that announced the older protocol is asked to update first.
        pa.clear();pb.clear();a->addPeer(b.id(),L"PC B","127.0.0.1",47930,1);a->send(b.id(),{file.wstring()});CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"Update Arnav Island")!=std::wstring::npos);
        a->addPeer(b.id(),L"PC B","127.0.0.1",47930);
        // A PC that forgot the other refuses its files.
        b.forget(a->id());a->send(b.id(),{file.wstring()});CHECK(waitFor(*a,pa,ShareEvent::Kind::Failed,e));CHECK(e.detail.find(L"doesn't have this PC paired")!=std::wstring::npos);
    }
    std::error_code error;fs::remove_all(root,error);
    std::printf("share tests: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
