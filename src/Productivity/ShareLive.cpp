#include "Productivity/ShareLive.h"
#include <cwctype>

namespace nexus {
namespace {
// Little-endian reads that fail softly: past the end, ok turns false and zeros come back.
struct Reader{const std::vector<uint8_t>& b;size_t at=0;bool ok=true;
    bool need(size_t n){if(!ok||at>b.size()||b.size()-at<n){ok=false;return false;}return true;}
    uint8_t u8(){if(!need(1))return 0;return b[at++];}
    uint16_t u16(){if(!need(2))return 0;uint16_t v=uint16_t(b[at]|(b[at+1]<<8));at+=2;return v;}
    uint32_t u32(){if(!need(4))return 0;uint32_t v=uint32_t(b[at])|(uint32_t(b[at+1])<<8)|(uint32_t(b[at+2])<<16)|(uint32_t(b[at+3])<<24);at+=4;return v;}
    uint64_t u64(){const uint64_t lo=u32();return lo|(uint64_t(u32())<<32);}
    std::vector<uint8_t> blob(size_t limit){const uint32_t n=u32();if(!need(n)||n>limit){ok=false;return {};}std::vector<uint8_t> v(b.begin()+std::ptrdiff_t(at),b.begin()+std::ptrdiff_t(at+n));at+=n;return v;}
    std::string bytes(size_t limit){auto v=blob(1u<<20);if(v.size()>limit)v.resize(limit);return {v.begin(),v.end()};}
    // UTF-8 to UTF-16 (malformed bytes become U+FFFD), cut at `limit` characters.
    std::wstring text(size_t limit){const std::string s=bytes(4*limit+4);std::wstring w;size_t i=0;
        while(i<s.size()&&w.size()<limit){const unsigned char c=s[i];uint32_t cp=0xfffd;size_t n=c<0x80?1:(c>>5)==6?2:(c>>4)==14?3:(c>>3)==30?4:0;
            if(n==0||i+n>s.size()){++i;w+=wchar_t(0xfffd);continue;}cp=n==1?c:n==2?(c&31):n==3?(c&15):(c&7);
            for(size_t k=1;k<n;++k){const unsigned char d=s[i+k];if((d>>6)!=2){cp=0xfffd;n=k;break;}cp=(cp<<6)|(d&63);}i+=n;
            if(cp>=0x10000&&cp<=0x10ffff){cp-=0x10000;w+=wchar_t(0xd800+(cp>>10));w+=wchar_t(0xdc00+(cp&0x3ff));}else w+=wchar_t(cp>0x10ffff?0xfffd:cp);}
        return w;}};
}
std::vector<double> LiveActivity::segmentEnds()const{
    double total=0;for(auto& s:segments)total+=s.length;std::vector<double> ends;if(total<=0)return ends;double at=0;for(auto& s:segments){at+=s.length;ends.push_back(at/total);}return ends;
}
bool parseLive(const std::vector<uint8_t>& notice,LiveActivity& a){
    Reader r{notice};if(notice.size()>=2&&notice[0]==0x30&&notice[1]==7)r.at=2;else if(!notice.empty()&&notice[0]==7&&notice.size()>=2&&(notice[1]==1||notice[1]==2))r.at=1;
    a=LiveActivity{};a.op=r.u8();a.key=r.bytes(200);if(!r.ok||a.key.empty()||(a.op!=1&&a.op!=2))return false;if(a.op==2)return true;
    a.app=r.text(120);a.package=[&]{auto p=r.bytes(256);return std::wstring(p.begin(),p.end());}();a.title=r.text(200);a.text=r.text(400);a.status=r.text(200);a.flags=r.u8();a.progress=std::min<uint16_t>(r.u16(),1000);
    const uint8_t segments=r.u8();for(uint8_t i=0;i<segments&&r.ok;++i){LiveActivity::Segment s;s.length=r.u16();s.argb=r.u32();if(i<10)a.segments.push_back(s);}
    const uint8_t points=r.u8();for(uint8_t i=0;i<points&&r.ok;++i){LiveActivity::Point p;p.position=std::min<uint16_t>(r.u16(),1000);p.argb=r.u32();if(i<10)a.points.push_back(p);}
    a.when=r.u64();a.accent=r.u32();a.appIcon=r.blob(8*1024+512);a.trackerIcon=r.blob(8*1024+512);a.startIcon=r.blob(8*1024+512);a.endIcon=r.blob(8*1024+512);a.largeIcon=r.blob(24*1024+512);a.snapshot=r.blob(64*1024+1024);
    return r.ok;
}
std::wstring liveEtaFrom(const std::wstring& line){
    auto lower=[](wchar_t c){return wchar_t(std::towlower(c));};
    for(size_t i=0;i<line.size();++i){if(!std::iswdigit(line[i])||(i>0&&std::iswdigit(line[i-1])))continue;
        size_t j=i;int a=0;while(j<line.size()&&std::iswdigit(line[j])&&a<10000)a=a*10+(line[j++]-L'0');
        // A range ("12-15 mins", "12 \u2013 15 min").
        int b=-1;{size_t k=j;while(k<line.size()&&line[k]==L' ')++k;if(k<line.size()&&(line[k]==L'-'||line[k]==L'\u2013'||line[k]==L'—')){++k;while(k<line.size()&&line[k]==L' ')++k;if(k<line.size()&&std::iswdigit(line[k])){b=0;while(k<line.size()&&std::iswdigit(line[k])&&b<10000)b=b*10+(line[k++]-L'0');j=k;}}}
        size_t k=j;while(k<line.size()&&line[k]==L' ')++k;std::wstring word;while(k<line.size()&&std::iswalpha(line[k])&&word.size()<8)word+=lower(line[k++]);
        const bool minutes=word==L"m"||word.rfind(L"min",0)==0,hours=word==L"h"||word==L"hr"||word==L"hrs"||word.rfind(L"hour",0)==0;if(!minutes&&!hours)continue;
        if(hours)return std::to_wstring(a)+(b>=0?L"\u2013"+std::to_wstring(b):L"")+L" h";
        if(b<0&&a>=60)return std::to_wstring(a/60)+L" h "+std::to_wstring(a%60)+L" min";
        return std::to_wstring(a)+(b>=0?L"\u2013"+std::to_wstring(b):L"")+L" min";}
    return {};
}
std::wstring liveEta(const LiveActivity& a,int64_t nowMs,double* tick){
    if(tick)*tick=0;const bool travel=a.has(LiveActivity::Delivery)||a.has(LiveActivity::Navigation);
    auto clock=[](int64_t s){wchar_t b[24];if(s>=3600)swprintf(b,24,L"%lld:%02lld:%02lld",(long long)(s/3600),(long long)(s/60%60),(long long)(s%60));else swprintf(b,24,L"%lld:%02lld",(long long)(s/60),(long long)(s%60));return std::wstring(b);};
    auto minutes=[&](int64_t s){if(tick)*tick=15;if(s<45)return std::wstring(L"Now");const int64_t m=std::max<int64_t>(1,(s+30)/60);return m>=60?std::to_wstring(m/60)+L" h "+std::to_wstring(m%60)+L" min":std::to_wstring(m)+L" min";};
    if(a.when>0){const int64_t left=(int64_t(a.when)-nowMs)/1000;
        // A countdown: minutes for a delivery, a ride or a route; a ticking clock for a timer.
        if(a.has(LiveActivity::CountsDown)){if(travel)return minutes(std::max<int64_t>(0,left));if(tick)*tick=1;return clock(std::max<int64_t>(0,(int64_t(a.when)-nowMs+999)/1000));}
        // A time ahead is an ETA; one behind is a chronometer counting up (not for travel: there it's only when it was posted).
        if(left>30)return minutes(left);if(!travel&&!a.has(LiveActivity::HasProgress)){if(tick)*tick=1;return clock(std::max<int64_t>(0,-left));}}
    for(const auto* line:{&a.status,&a.title,&a.text}){auto eta=liveEtaFrom(*line);if(!eta.empty())return eta;}
    return {};
}
}
