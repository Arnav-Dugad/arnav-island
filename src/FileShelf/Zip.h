#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
namespace nexus::zip {
// ---- CRC-32 (ZIP polynomial) ------------------------------------------------------------------
inline uint32_t crc32(uint32_t crc,const uint8_t* data,size_t n){
    static const auto table=[]{std::array<uint32_t,256> t{};for(uint32_t i=0;i<256;++i){uint32_t c=i;for(int k=0;k<8;++k)c=c&1?0xedb88320u^(c>>1):c>>1;t[i]=c;}return t;}();
    crc=~crc;for(size_t i=0;i<n;++i)crc=table[(crc^data[i])&255]^(crc>>8);return ~crc;
}
// ---- Deflate (RFC 1951): LZ77 with hash chains, fixed Huffman codes, stored blocks for
// data that would not shrink. Streams: call add() with any chunk sizes, then finish(). ----------
class Deflater {
    std::vector<uint8_t>& out_;uint32_t bits_=0;int count_=0;
    std::vector<uint8_t> window_;size_t base_=0;// window_[0] is stream position base_
    std::vector<int32_t> head_,prev_;
    struct Token {uint16_t length;uint16_t distance;uint8_t literal;};
    static constexpr size_t windowSize=32768,chunk=65536;
    void put(uint32_t value,int n){bits_|=value<<count_;count_+=n;while(count_>=8){out_.push_back(uint8_t(bits_));bits_>>=8;count_-=8;}}
    void align(){if(count_>0)out_.push_back(uint8_t(bits_));bits_=0;count_=0;}
    static uint32_t reverse(uint32_t code,int n){uint32_t r=0;for(int i=0;i<n;++i){r=(r<<1)|(code&1);code>>=1;}return r;}
    void huffman(int symbol){if(symbol<144)put(reverse(0x30+symbol,8),8);else if(symbol<256)put(reverse(0x190+symbol-144,9),9);else if(symbol<280)put(reverse(symbol-256,7),7);else put(reverse(0xc0+symbol-280,8),8);}
    static int bitsFor(int symbol){return symbol<144?8:symbol<256?9:symbol<280?7:8;}
    static constexpr uint16_t lengthBase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static constexpr uint8_t lengthExtra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static constexpr uint16_t distanceBase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static constexpr uint8_t distanceExtra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static int lengthCode(int length){int i=28;while(lengthBase[i]>length)--i;return i;}
    static int distanceCode(int distance){int i=29;while(distanceBase[i]>distance)--i;return i;}
    static uint32_t hash(const uint8_t* p){return ((uint32_t(p[0])<<10)^(uint32_t(p[1])<<5)^p[2])&0x7fff;}
    // Compresses window_[from,to) as one block (fixed Huffman, or stored when that is smaller).
    void block(size_t from,size_t to,bool final){
        std::vector<Token> tokens;tokens.reserve(to-from);size_t bitsNeeded=3+7;
        for(size_t i=from;i<to;){
            int bestLength=0,bestDistance=0;
            if(i+3<=window_.size()){const uint32_t h=hash(&window_[i]);int32_t candidate=head_[h];int chain=64;const size_t limit=std::min<size_t>(258,window_.size()-i);
                while(candidate>=0&&size_t(candidate)>=base_&&chain-->0){const size_t c=size_t(candidate)-base_;if(i-c>windowSize)break;
                    if(c<i&&window_[c+size_t(bestLength)]==window_[i+size_t(bestLength)]){size_t n=0;while(n<limit&&window_[c+n]==window_[i+n])++n;if(int(n)>bestLength){bestLength=int(n);bestDistance=int(i-c);if(n==limit)break;}}
                    candidate=prev_[size_t(candidate)&(windowSize-1)];}}
            auto insert=[&](size_t p){if(p+3<=window_.size()){const uint32_t h=hash(&window_[p]);prev_[(p+base_)&(windowSize-1)]=head_[h];head_[h]=int32_t(p+base_);}};
            if(bestLength>=3&&size_t(bestLength)<=to-i){const int lc=lengthCode(bestLength),dc=distanceCode(bestDistance);tokens.push_back({uint16_t(bestLength),uint16_t(bestDistance),0});
                bitsNeeded+=size_t(bitsFor(257+lc))+lengthExtra[lc]+5+distanceExtra[dc];for(int k=0;k<bestLength;++k)insert(i+size_t(k));i+=size_t(bestLength);}
            else{tokens.push_back({0,0,window_[i]});bitsNeeded+=size_t(bitsFor(window_[i]));insert(i);++i;}}
        const size_t storedBits=(to-from)*8+((to-from)/65535+1)*(5*8+8);
        if(bitsNeeded>storedBits){for(size_t at=from;;){const size_t n=std::min<size_t>(65535,to-at);const bool last=final&&at+n==to;put(last?1:0,1);put(0,2);align();
                out_.push_back(uint8_t(n));out_.push_back(uint8_t(n>>8));out_.push_back(uint8_t(~n));out_.push_back(uint8_t((~n)>>8));out_.insert(out_.end(),window_.begin()+std::ptrdiff_t(at),window_.begin()+std::ptrdiff_t(at+n));at+=n;if(at>=to)break;}
            return;}
        put(final?1:0,1);put(1,2);
        for(auto& t:tokens){if(!t.length){huffman(t.literal);continue;}const int lc=lengthCode(t.length),dc=distanceCode(t.distance);huffman(257+lc);put(uint32_t(t.length-lengthBase[lc]),lengthExtra[lc]);put(reverse(uint32_t(dc),5),5);put(uint32_t(t.distance-distanceBase[dc]),distanceExtra[dc]);}
        huffman(256);
    }
    size_t pending_=0;// window_ bytes not yet compressed
public:
    explicit Deflater(std::vector<uint8_t>& out):out_(out),head_(32768,-1),prev_(windowSize,-1){}
    void add(const uint8_t* data,size_t n){window_.insert(window_.end(),data,data+n);while(window_.size()-pending_>=chunk*2){block(pending_,pending_+chunk,false);pending_+=chunk;slide();}}
    void finish(){if(pending_<window_.size())block(pending_,window_.size(),true);else{put(1,1);put(1,2);huffman(256);}align();window_.clear();}
private:
    // Keeps the last 32 KiB of already-compressed data as history.
    void slide(){if(pending_<=windowSize)return;const size_t drop=pending_-windowSize;window_.erase(window_.begin(),window_.begin()+std::ptrdiff_t(drop));base_+=drop;pending_-=drop;}
};
inline std::vector<uint8_t> deflate(const uint8_t* data,size_t n){std::vector<uint8_t> out;Deflater d(out);d.add(data,n);d.finish();return out;}
// ---- ZIP container (no ZIP64: entries and archive stay under 4 GiB) --------------------------------
// Writes through a small sink so tests can capture bytes in memory.
struct Sink {virtual bool write(const uint8_t*,size_t)=0;virtual uint64_t tell()=0;virtual bool patch(uint64_t at,const uint8_t*,size_t)=0;virtual ~Sink()=default;};
struct Entry {std::string name;uint32_t crc=0,compressed=0,size=0,offset=0;uint16_t time=0,date=0;};
inline void le16(std::vector<uint8_t>& b,uint32_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));}
inline void le32(std::vector<uint8_t>& b,uint32_t v){le16(b,v&0xffff);le16(b,v>>16);}
class Writer {
    Sink& sink_;std::vector<Entry> entries_;bool failed_=false;
public:
    explicit Writer(Sink& s):sink_(s){}
    bool failed()const{return failed_;}
    const std::vector<Entry>& entries()const{return entries_;}
    // name: UTF-8 with '/' separators. read(buffer,capacity) returns bytes read, 0 at the end, -1 on error.
    template<class Read> bool add(const std::string& name,uint16_t dosTime,uint16_t dosDate,Read read){
        if(failed_)return false;Entry e;e.name=name;e.time=dosTime;e.date=dosDate;const uint64_t at=sink_.tell();if(at>0xfffffff0ull||entries_.size()>=65535){failed_=true;return false;}e.offset=uint32_t(at);
        std::vector<uint8_t> h;le32(h,0x04034b50);le16(h,20);le16(h,0x0800);le16(h,8);le16(h,e.time);le16(h,e.date);le32(h,0);le32(h,0);le32(h,0);le16(h,uint16_t(name.size()));le16(h,0);h.insert(h.end(),name.begin(),name.end());
        if(!sink_.write(h.data(),h.size())){failed_=true;return false;}
        std::vector<uint8_t> out;Deflater d(out);std::vector<uint8_t> buffer(1<<20);uint64_t size=0,compressed=0;uint32_t crc=0;
        auto flush=[&]{if(out.empty())return true;compressed+=out.size();bool ok=sink_.write(out.data(),out.size());out.clear();return ok;};
        for(;;){const long long n=read(buffer.data(),buffer.size());if(n<0){failed_=true;return false;}if(n==0)break;crc=crc32(crc,buffer.data(),size_t(n));size+=uint64_t(n);if(size>0xfffffff0ull){failed_=true;return false;}d.add(buffer.data(),size_t(n));if(!flush()){failed_=true;return false;}}
        d.finish();if(!flush()||compressed>0xfffffff0ull){failed_=true;return false;}
        e.crc=crc;e.size=uint32_t(size);e.compressed=uint32_t(compressed);std::vector<uint8_t> sizes;le32(sizes,e.crc);le32(sizes,e.compressed);le32(sizes,e.size);
        if(!sink_.patch(at+14,sizes.data(),sizes.size())){failed_=true;return false;}entries_.push_back(std::move(e));return true;
    }
    bool finish(){
        if(failed_)return false;const uint64_t start=sink_.tell();std::vector<uint8_t> c;
        for(auto& e:entries_){le32(c,0x02014b50);le16(c,20);le16(c,20);le16(c,0x0800);le16(c,8);le16(c,e.time);le16(c,e.date);le32(c,e.crc);le32(c,e.compressed);le32(c,e.size);le16(c,uint16_t(e.name.size()));le16(c,0);le16(c,0);le16(c,0);le16(c,0);le32(c,0);le32(c,e.offset);c.insert(c.end(),e.name.begin(),e.name.end());}
        if(start+c.size()>0xfffffff0ull)return false;
        le32(c,0x06054b50);le16(c,0);le16(c,0);le16(c,uint16_t(entries_.size()));le16(c,uint16_t(entries_.size()));le32(c,uint32_t(c.size()));le32(c,uint32_t(start));le16(c,0);
        // The central directory size excludes the end record itself (22 bytes).
        const uint32_t directory=uint32_t(c.size()-22);c[c.size()-10]=uint8_t(directory);c[c.size()-9]=uint8_t(directory>>8);c[c.size()-8]=uint8_t(directory>>16);c[c.size()-7]=uint8_t(directory>>24);
        return sink_.write(c.data(),c.size());
    }
};
// DOS date and time from calendar fields (years before 1980 clamp to 1980).
inline uint16_t dosTime(int hour,int minute,int second){return uint16_t((hour<<11)|(minute<<5)|(second/2));}
inline uint16_t dosDate(int year,int month,int day){return uint16_t(((std::max(year,1980)-1980)<<9)|(month<<5)|day);}
// Archive-relative name: the path below `root`, '/'-separated; names leaving the root are refused.
inline std::string entryName(const std::string& utf8Relative){std::string n=utf8Relative;for(auto& ch:n)if(ch=='\\')ch='/';while(!n.empty()&&n.front()=='/')n.erase(n.begin());
    if(n.empty()||n=="."||n.find("../")!=std::string::npos||n.starts_with("..")||n.find(':')!=std::string::npos)return {};return n;}
}
