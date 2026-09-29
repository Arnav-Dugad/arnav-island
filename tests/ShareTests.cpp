// Sharing between your own PCs, end to end over loopback: two services with their own identities
// pair (both confirm the same code), refuse files before pairing and after a PC forgets the other,
// carry a declined and an accepted transfer (byte for byte), and keep their identity and pairing.
// Phase 5G: several files and a folder tree in one transfer, a transfer stopped by either side, a PC
// that announced the older protocol, and music handed off with and without the song's file.
// Phase 5H: the cover travels with the music (only to a PC of revision 1), and one PC looks into the other's
// Shelf and takes a file and a folder from it (not while that Shelf is kept to itself, nor an item since removed).
#include "Productivity/ShareRelay.h"
#include "Productivity/ShareService.h"
#include "Productivity/QrCode.h"
#include <chrono>
#include <cstdio>
#include <cstring>
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
    // 0.20: pairing codes as typed, and a tunnel's two local ends.
    CHECK(relayCode("7k2p mx4q")=="7K2PMX4Q");CHECK(relayCode("7K2P-MX4Q")=="7K2PMX4Q");CHECK(relayCode("7K2P-MX4")=="");CHECK(relayCode("7K2P-MX4O")=="");CHECK(relayCode("")=="");
    {WSADATA wsa{};WSAStartup(MAKEWORD(2,2),&wsa);SOCKET a,b;CHECK(loopbackPair(a,b));if(a!=INVALID_SOCKET){const char out[]="relay";CHECK(::send(a,out,5,0)==5);char in[8]{};CHECK(::recv(b,in,8,0)==5&&std::memcmp(in,out,5)==0);closesocket(a);closesocket(b);}WSACleanup();}
    // Revision 2: the remote's status (flags, position, volume, battery, CPU, the cover once, then "unchanged", five lines).
    {RemoteStatus st;st.available=st.playing=st.canNext=true;st.batteryPresent=true;st.position=61.5;st.duration=200;st.volume=142;st.battery=77;st.cpu=-1;st.title=L"A\nB";st.name=L"Desk";st.cover.assign(900,7);
        const auto a=remoteStatusAnswer(st,{});CHECK(a.size()>30&&a[0]==remoteOk);const uint16_t flags=uint16_t(a[1]|a[2]<<8);CHECK(flags==(1|2|8|256));
        double position=0;std::memcpy(&position,a.data()+3,8);CHECK(position==61.5);CHECK(a[19]==100&&a[20]==77&&a[21]==255&&a[22]==1);
        const std::vector<uint8_t> hash(a.begin()+23,a.begin()+55);const auto b=remoteStatusAnswer(st,hash);CHECK(b[22]==2&&b.size()==a.size()-32-4-900);
        const uint32_t n=uint32_t(b[23]|b[24]<<8|b[25]<<16|b[26]<<24);CHECK(std::string(b.begin()+27,b.end())=="A B\n\n\nDesk\n"&&n==b.size()-27);
        st.cover.clear();CHECK(remoteStatusAnswer(st,hash)[22]==0);
        // 0.20: bit 9 says the universal clipboard is on.
        st.clipboard=true;{const auto c=remoteStatusAnswer(st,{});CHECK(uint16_t(c[1]|c[2]<<8)==(1|2|8|256|512));}}
    // 0.20.1: the pairing link's QR code. The modules are those of the reference encoder (Nayuki's qrcodegen) for the same
    // text, version 3 at level M, and decode (zxing-cpp; and ZXing in the phone's own tests) to the link.
    {const auto q=qrEncode("arnavisland://pair/7K2PMX4Q");CHECK(q.size==29&&q.modules.size()==29u*29u);
        const char* golden[]={"1fcd317f","104b0441","175a565d","1754115d","175eee5d","1058f141","1fd5557f","001da200","17c4627c","1ea039d3","0d5f0548","12ac574b","06681dad","11b5e2db","0352f154","1d84a77b","05616206","18a13275","15f70488","110f52a8","134015fe","0012ef13","1fccfb5c","105eaf19","175261f7","175b360c","175285f2","1048d1ba","1fdb1614"};bool same=q.size==29;
        for(int y=0;y<29&&same;++y){const unsigned long row=std::stoul(golden[y],nullptr,16);for(int x=0;x<29;++x)if(q.dark(x,y)!=(((row>>(28-x))&1)!=0))same=false;}
        CHECK(same);
        // The finder patterns in three corners, and the dark module by the lower one.
        CHECK(q.dark(0,0)&&q.dark(6,6)&&!q.dark(1,1)&&q.dark(28,0)&&q.dark(0,28)&&q.dark(8,21));
        CHECK(qrEncode("A").size==21&&qrEncode(std::string(213,'x')).size==57&&qrEncode(std::string(214,'x')).size==0);
        for(int m=0;m<8;++m)CHECK(qrEncode("arnavisland://pair/23456789",m).size==29);}
    CHECK(shareTitle({L"Photos"})==L"Photos");CHECK(shareTitle({L"a.txt",L"b.txt",L"c.txt"})==L"a.txt and 2 more");CHECK(safeShareName(L"")==L"file");CHECK(safeShareName(std::wstring(300,L'a')+L".pdf").size()==120);CHECK(safeShareName(std::wstring(300,L'a')+L".pdf").ends_with(L".pdf"));
    const fs::path root=fs::temp_directory_path()/(L"arnav-share-test-"+std::to_wstring(GetTickCount64()));fs::create_directories(root);
    const fs::path file=root/L"hello.bin";{std::mt19937 random(7);std::ofstream out(file,std::ios::binary);for(int i=0;i<1000003;++i)out.put(char(random()&255));}
    auto options=[&](const wchar_t* who,uint16_t port){ShareOptions o;o.tcpPort=port;o.discovery=false;o.loopback=true;o.folder=(root/who).wstring();o.downloads=(root/(std::wstring(who)+L"-downloads")).wstring();o.handoff=(root/(std::wstring(who)+L"-music")).wstring();o.name=std::wstring(L"PC ")+who;return o;};
    std::string idA;
    {
        auto a=std::make_unique<ShareService>(nullptr,options(L"A",47920));ShareService b(nullptr,options(L"B",47930));std::vector<ShareEvent> pa,pb;ShareEvent e,f;
        CHECK(a->running());CHECK(b.running());CHECK(a->id().size()==32);CHECK(a->id()!=b.id());idA=a->id();
        // The pairing link: the code without its dash, and this PC's key fingerprint (each PC its own); its QR code is version 4.
        {const std::string l=a->pairingLink("7K2P-MX4Q");CHECK(l.starts_with("arnavisland://pair/7K2PMX4Q?k=")&&l.size()==50);CHECK(l==a->pairingLink("7k2pmx4q"));
            CHECK(l.substr(30).find_first_not_of("0123456789abcdef")==std::string::npos);CHECK(l!=b.pairingLink("7K2P-MX4Q"));CHECK(a->pairingLink("").empty());
            CHECK(qrEncode(l).size==33);}
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
