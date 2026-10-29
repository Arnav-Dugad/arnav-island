// 0.27 (revision 10): a phone's live activities as the island reads them, and its microphone's codecs (the request both ways,
// the codec taken, AAC packets made by Windows' own encoder decoded back, Opus where Windows has a decoder).
#include "Productivity/ShareLive.h"
#include "Mirror/MirrorHost.h"
#include "Mirror/AudioCodec.h"
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace nexus;
static int checks=0;
static void test(bool pass,const char* label){++checks;if(!pass)throw std::runtime_error(label);}
namespace {
struct Writer{std::vector<uint8_t> b;void u8(uint8_t v){b.push_back(v);}void u16(uint16_t v){u8(uint8_t(v));u8(uint8_t(v>>8));}void u32(uint32_t v){for(int k=0;k<4;++k)u8(uint8_t(v>>(8*k)));}void u64(uint64_t v){u32(uint32_t(v));u32(uint32_t(v>>32));}
    void blob(const std::string& v){u32(uint32_t(v.size()));b.insert(b.end(),v.begin(),v.end());}};
int64_t nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
// Notice 7 as a phone sends it (op 1), with a UTF-8 title and a picture.
std::vector<uint8_t> delivery(bool prefix){Writer w;if(prefix){w.u8(0x30);w.u8(7);}w.u8(1);w.blob("k1");w.blob("Tiffin");w.blob("com.example.tiffin");w.blob("Order picked up \xE2\x80\x94 on the way");w.blob("2 items");w.blob("Arriving in 12 mins");
    w.u8(4|16|32);w.u16(620);w.u8(3);w.u16(240);w.u32(0xffff9f45);w.u16(260);w.u32(0xffffc145);w.u16(500);w.u32(0xff34c77b);w.u8(1);w.u16(500);w.u32(0xff112233);w.u64(1234);w.u32(0xffff7a2f);
    w.blob("PNG!");w.blob("");w.blob("");w.blob("");w.blob("");w.blob("");return w.b;}
// A made-up sound connection: what the PC sends is kept, what the phone sends waits in a queue.
struct FakeIo{std::vector<std::vector<uint8_t>> sent;std::deque<std::vector<uint8_t>> incoming;mirror::MirrorIo io;
    FakeIo(){io.send=[this](const std::vector<uint8_t>& f){sent.push_back(f);return true;};io.receive=[this](std::vector<uint8_t>& f,int){if(incoming.empty())return false;f=incoming.front();incoming.pop_front();return true;};}};
std::vector<uint8_t> micRequest(int rate,int channels,int codec,const std::string& name){Writer w;w.u8(0xA0);w.u8(4);w.u32(uint32_t(rate));w.u8(uint8_t(channels));if(codec>=0)w.u8(uint8_t(codec));w.blob(name);return w.b;}
struct Heard:mirror::AudioSink{size_t frames=0;double energy=0;int crossings=0;int16_t last=0;bool begin(int,int,const std::wstring&,std::wstring& device)override{device=L"Test cable";return true;}
    void samples(const int16_t* pcm,size_t n,uint64_t)override{for(size_t i=0;i<n;++i){energy+=double(pcm[i])*pcm[i];if((last<0)!=(pcm[i]<0))++crossings;last=pcm[i];}frames+=n;}void end()override{}};
// A second of a 1 kHz tone through Windows' AAC encoder, each raw frame wrapped in ADTS as a phone's encoder would send it.
std::vector<std::vector<uint8_t>> aacPackets(int rate){
    std::vector<std::vector<uint8_t>> packets;if(!mirror::micMedia())return packets;MFT_REGISTER_TYPE_INFO out{MFMediaType_Audio,MFAudioFormat_AAC};IMFActivate** list=nullptr;UINT32 count=0;ComPtr<IMFTransform> enc;
    if(FAILED(MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER,MFT_ENUM_FLAG_SYNCMFT|MFT_ENUM_FLAG_LOCALMFT|MFT_ENUM_FLAG_SORTANDFILTER,nullptr,&out,&list,&count)))return packets;
    for(UINT32 i=0;i<count;++i){if(!enc)list[i]->ActivateObject(IID_PPV_ARGS(&enc));list[i]->Release();}CoTaskMemFree(list);if(!enc)return packets;
    ComPtr<IMFMediaType> o,in;MFCreateMediaType(&o);o->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);o->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_AAC);o->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);o->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,UINT32(rate));o->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,1);o->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,12000);o->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE,0);
    MFCreateMediaType(&in);in->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);in->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM);in->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);in->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,UINT32(rate));in->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,1);in->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,2);in->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,UINT32(rate*2));
    if(FAILED(enc->SetOutputType(0,o.Get(),0))||FAILED(enc->SetInputType(0,in.Get(),0)))return packets;
    int index=3;{const int rates[]={96000,88200,64000,48000,44100,32000,24000,22050,16000};for(int i=0;i<9;++i)if(rates[i]==rate)index=i;}
    auto drain=[&]{for(;;){MFT_OUTPUT_STREAM_INFO info{};enc->GetOutputStreamInfo(0,&info);ComPtr<IMFSample> s;ComPtr<IMFMediaBuffer> b;MFCreateSample(&s);MFCreateMemoryBuffer(std::max<DWORD>(info.cbSize,8192),&b);s->AddBuffer(b.Get());
        MFT_OUTPUT_DATA_BUFFER db{};db.pSample=s.Get();DWORD st=0;if(FAILED(enc->ProcessOutput(0,1,&db,&st)))return;if(db.pEvents)db.pEvents->Release();BYTE* p=nullptr;DWORD n=0;b->Lock(&p,nullptr,&n);
        const size_t len=7+n;std::vector<uint8_t> a{0xFF,0xF1,uint8_t((1<<6)|(index<<2)),uint8_t((1<<6)|(len>>11)),uint8_t(len>>3),uint8_t(((len&7)<<5)|0x1F),0xFC};a.insert(a.end(),p,p+n);b->Unlock();packets.push_back(std::move(a));}};
    for(int block=0;block<rate/960;++block){std::vector<int16_t> pcm(960);for(int i=0;i<960;++i)pcm[size_t(i)]=int16_t(12000*std::sin(2*3.14159265358979*1000*(block*960+i)/double(rate)));
        ComPtr<IMFMediaBuffer> b;ComPtr<IMFSample> s;MFCreateMemoryBuffer(1920,&b);BYTE* p=nullptr;b->Lock(&p,nullptr,nullptr);std::memcpy(p,pcm.data(),1920);b->Unlock();b->SetCurrentLength(1920);MFCreateSample(&s);s->AddBuffer(b.Get());s->SetSampleTime(LONGLONG(block)*200000);
        if(FAILED(enc->ProcessInput(0,s.Get(),0))){drain();enc->ProcessInput(0,s.Get(),0);}drain();}
    return packets;
}
std::vector<uint8_t> audioFrame(uint32_t n,const std::vector<uint8_t>& payload){Writer w;w.u8(0xAA);w.u32(n);w.u64(uint64_t(n)*200000);w.b.insert(w.b.end(),payload.begin(),payload.end());return w.b;}
}
int main(){try{
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    // Notice 7: every field, with or without the frame's [0x30, 7] in front; the end (op 2); anything cut short is refused.
    for(bool prefix:{true,false}){LiveActivity a;test(parseLive(delivery(prefix),a),"live parses");test(a.op==1&&a.key=="k1"&&a.app==L"Tiffin"&&a.package==L"com.example.tiffin","live names");
        test(a.title==L"Order picked up — on the way"&&a.status==L"Arriving in 12 mins","live text is UTF-8");test(a.has(LiveActivity::HasProgress)&&a.has(LiveActivity::Delivery)&&!a.has(LiveActivity::Indeterminate),"live flags");
        test(a.progress==620&&std::abs(a.fraction()-.62)<1e-9&&a.segments.size()==3&&a.points.size()==1&&a.points[0].position==500,"live progress");test(a.when==1234&&a.accent==0xffff7a2f&&a.appIcon.size()==4&&a.snapshot.empty(),"live pictures");
        const auto ends=a.segmentEnds();test(ends.size()==3&&std::abs(ends[0]-.24)<1e-9&&std::abs(ends[2]-1)<1e-9,"segment ends");}
    {auto cut=delivery(true);cut.resize(cut.size()-3);LiveActivity a;test(!parseLive(cut,a),"a cut notice is refused");Writer w;w.u8(0x30);w.u8(7);w.u8(2);w.blob("k1");test(parseLive(w.b,a)&&a.op==2&&a.key=="k1","an ended activity");
        Writer bad;bad.u8(5);bad.blob("k1");test(!parseLive(bad.b,a),"an unknown op is refused");}
    // The ETA, from a line or from `when`.
    test(liveEtaFrom(L"Arriving in 12 mins")==L"12 min","minutes in a line");test(liveEtaFrom(L"ETA 7 minutes")==L"7 min","minutes spelt out");test(liveEtaFrom(L"12-15 mins away")==L"12\u201315 min","a range");
    test(liveEtaFrom(L"In 90 min")==L"1 h 30 min","over an hour");test(liveEtaFrom(L"On the way")==L""&&liveEtaFrom(L"2 items")==L"","no time said");test(liveEtaFrom(L"in 2 hrs")==L"2 h","hours");
    {const int64_t now=nowMs();LiveActivity d;d.flags=LiveActivity::CountsDown|LiveActivity::Delivery;d.when=uint64_t(now+12*60000+10000);double tick=0;test(liveEta(d,now,&tick)==L"12 min"&&tick>0,"a delivery counts down in minutes");
        d.when=uint64_t(now+20000);test(liveEta(d,now)==L"Now","arriving now");LiveActivity t;t.flags=LiveActivity::CountsDown;t.when=uint64_t(now+299000);test(liveEta(t,now,&tick)==L"4:59"&&tick==1,"a timer ticks");
        t.when=uint64_t(now+3723000);test(liveEta(t,now)==L"1:02:03","a long timer");LiveActivity c;c.when=uint64_t(now-65000);test(liveEta(c,now)==L"1:05","a chronometer counts up");
        LiveActivity posted;posted.flags=LiveActivity::Delivery;posted.when=uint64_t(now-65000);posted.status=L"Arriving in 3 mins";test(liveEta(posted,now)==L"3 min","a delivery's posting time isn't a clock");}
    // Kind 4's request, from an older phone (no codec) and a revision 10 one.
    {mirror::MirrorRequest r;test(mirror::readRequest(micRequest(48000,1,-1,"Pixel"),r)&&r.codec==-1&&r.name==L"Pixel"&&r.rate==48000,"an older phone's microphone");
        test(mirror::readRequest(micRequest(48000,2,2,"Pixel 9"),r)&&r.codec==2&&r.channels==2&&r.name==L"Pixel 9","a phone offering Opus");
        test(mirror::readRequest(micRequest(16000,1,1,""),r)&&r.codec==1&&r.name.empty(),"a nameless phone offering AAC");}
    // What this PC has, and what it takes for each offer.
    const bool aac=mirror::micCodecAvailable(mirror::micAac),opus=mirror::micCodecAvailable(mirror::micOpus);
    std::cout<<"codecs: AAC "<<(aac?"yes":"no")<<", Opus "<<(opus?"yes":"no")<<"\n";
    test(mirror::acceptMicCodec(0)==0&&mirror::acceptMicCodec(1)==(aac?1:0)&&mirror::acceptMicCodec(2)==(opus?2:aac?1:0)&&mirror::acceptMicCodec(9)==mirror::acceptMicCodec(2),"the codec taken");
    if(opus){mirror::MicDecoder d;test(d.open(mirror::micOpus,48000,1)&&d.open(mirror::micOpus,48000,2),"Opus decoder opens");}
    // AAC end to end: the reply says AAC, and a second of tone comes out as a second of tone.
    if(aac){auto packets=aacPackets(48000);test(packets.size()>30,"Windows' AAC encoder made packets");FakeIo f;uint32_t n=0;for(auto& p:packets)f.incoming.push_back(audioFrame(n++,p));f.incoming.push_back({mirror::frameStop});
        mirror::MirrorRequest r;test(mirror::readRequest(micRequest(48000,1,1,"Pixel"),r),"AAC request");Heard sink;const std::atomic<bool> stop{false};const auto rep=mirror::receiveAudio(f.io,r,sink,stop);
        test(!f.sent.empty()&&f.sent[0].size()>7&&f.sent[0][0]==mirror::frameMirrorReply&&f.sent[0][1]==0&&f.sent[0][6]==1,"the reply carries AAC");
        std::cout<<"AAC: "<<packets.size()<<" packets, "<<sink.frames<<" samples, "<<sink.crossings<<" crossings\n";
        test(sink.frames>40000&&sink.frames<52000,"a second of sound");test(sink.crossings>1700&&sink.crossings<2300,"still a 1 kHz tone");test(std::sqrt(sink.energy/double(sink.frames))>3000,"loud enough");test(std::string(rep.ended)=="their end","until the phone ends it");}
    // An older phone: PCM as before, and the reply's byte stays 0.
    {FakeIo f;std::vector<uint8_t> pcm(1920);f.incoming.push_back(audioFrame(0,pcm));f.incoming.push_back({mirror::frameStop});mirror::MirrorRequest r;mirror::readRequest(micRequest(48000,1,-1,"Old"),r);Heard sink;const std::atomic<bool> stop{false};
        mirror::receiveAudio(f.io,r,sink,stop);test(f.sent[0][6]==0&&sink.frames==960,"an older phone stays PCM");}
    std::cout<<"live and microphone tests passed: "<<checks<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<" (after "<<checks<<" checks)\n";return 1;}}
