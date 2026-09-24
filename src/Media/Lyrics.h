#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace nexus {
// ---- UTF-8 ------------------------------------------------------------------------------
inline std::string toUtf8(std::wstring_view w){
    std::string out;out.reserve(w.size());
    for(size_t i=0;i<w.size();++i){uint32_t c=w[i];
        if(c>=0xd800&&c<0xdc00&&i+1<w.size()&&w[i+1]>=0xdc00&&w[i+1]<0xe000){c=0x10000+((c-0xd800)<<10)+(uint32_t(w[i+1])-0xdc00);++i;}
        else if(c>=0xd800&&c<0xe000)c=0xfffd;
        if(c<0x80)out+=char(c);else if(c<0x800){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
        else if(c<0x10000){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
        else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}}
    return out;
}
// Invalid sequences become U+FFFD; never throws.
inline std::wstring fromUtf8(std::string_view s){
    std::wstring out;out.reserve(s.size());
    auto put=[&](uint32_t c){if(c>=0x10000){c-=0x10000;out+=wchar_t(0xd800+(c>>10));out+=wchar_t(0xdc00+(c&0x3ff));}else out+=wchar_t(c);};
    for(size_t i=0;i<s.size();){unsigned char b=s[i];int n=b<0x80?0:(b>>5)==6?1:(b>>4)==14?2:(b>>3)==30?3:-1;
        if(n<0||i+n>=s.size()+(n?0:1)){put(0xfffd);++i;continue;}
        uint32_t c=n==0?b:n==1?(b&31):n==2?(b&15):(b&7);bool ok=true;for(int k=1;k<=n;++k){if(i+k>=s.size()||(static_cast<unsigned char>(s[i+k])>>6)!=2){ok=false;break;}c=(c<<6)|(static_cast<unsigned char>(s[i+k])&63);}
        const uint32_t least[]={0,0x80,0x800,0x10000};if(!ok||c<least[n]||c>0x10ffff||(c>=0xd800&&c<0xe000)){put(0xfffd);++i;continue;}
        put(c);i+=size_t(n)+1;}
    return out;
}
// ---- A small JSON reader (the LRCLIB response) -------------------------------------------
struct Json {
    enum class Type{Null,Bool,Number,String,Array,Object} type=Type::Null;
    bool boolean=false;double number=0;std::string text;std::vector<Json> items;std::vector<std::pair<std::string,Json>> fields;
    const Json* find(std::string_view key)const{for(auto& [k,v]:fields)if(k==key)return &v;return nullptr;}
    std::string string(std::string_view key)const{auto* v=find(key);return v&&v->type==Type::String?v->text:std::string{};}
    double num(std::string_view key,double fallback=0)const{auto* v=find(key);return v&&v->type==Type::Number?v->number:fallback;}
    bool flag(std::string_view key)const{auto* v=find(key);return v&&v->type==Type::Bool&&v->boolean;}
    // Parses a whole document; nullopt on any syntax error or nesting deeper than 32.
    static std::optional<Json> parse(std::string_view s){size_t at=0;Json v;if(!value(s,at,v,0))return std::nullopt;space(s,at);if(at!=s.size())return std::nullopt;return v;}
private:
    static void space(std::string_view s,size_t& at){while(at<s.size()&&(s[at]==' '||s[at]=='\t'||s[at]=='\n'||s[at]=='\r'))++at;}
    static bool literal(std::string_view s,size_t& at,std::string_view word){if(s.substr(at,word.size())!=word)return false;at+=word.size();return true;}
    static bool hex4(std::string_view s,size_t at,uint32_t& v){if(at+4>s.size())return false;v=0;for(size_t i=at;i<at+4;++i){char c=s[i];v<<=4;if(c>='0'&&c<='9')v|=uint32_t(c-'0');else if(c>='a'&&c<='f')v|=uint32_t(c-'a'+10);else if(c>='A'&&c<='F')v|=uint32_t(c-'A'+10);else return false;}return true;}
    static bool str(std::string_view s,size_t& at,std::string& out){
        if(at>=s.size()||s[at]!='"')return false;++at;out.clear();
        while(at<s.size()){char c=s[at++];if(c=='"')return true;if(static_cast<unsigned char>(c)<0x20)return false;if(c!='\\'){out+=c;continue;}if(at>=s.size())return false;char e=s[at++];
            switch(e){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
            case 'u':{uint32_t c1=0;if(!hex4(s,at,c1))return false;at+=4;
                if(c1>=0xd800&&c1<0xdc00){uint32_t c2=0;if(at+6<=s.size()&&s[at]=='\\'&&s[at+1]=='u'&&hex4(s,at+2,c2)&&c2>=0xdc00&&c2<0xe000){at+=6;c1=0x10000+((c1-0xd800)<<10)+(c2-0xdc00);}else c1=0xfffd;}
                else if(c1>=0xdc00&&c1<0xe000)c1=0xfffd;
                std::wstring w;if(c1>=0x10000){w+=wchar_t(0xd800+((c1-0x10000)>>10));w+=wchar_t(0xdc00+((c1-0x10000)&0x3ff));}else w+=wchar_t(c1);out+=toUtf8(w);break;}
            default:return false;}}
        return false;
    }
    static bool value(std::string_view s,size_t& at,Json& v,int depth){
        if(depth>32)return false;space(s,at);if(at>=s.size())return false;char c=s[at];
        if(c=='n'){v.type=Type::Null;return literal(s,at,"null");}
        if(c=='t'){v.type=Type::Bool;v.boolean=true;return literal(s,at,"true");}
        if(c=='f'){v.type=Type::Bool;v.boolean=false;return literal(s,at,"false");}
        if(c=='"'){v.type=Type::String;return str(s,at,v.text);}
        if(c=='['){v.type=Type::Array;++at;space(s,at);if(at<s.size()&&s[at]==']'){++at;return true;}
            for(;;){Json item;if(!value(s,at,item,depth+1))return false;v.items.push_back(std::move(item));space(s,at);if(at>=s.size())return false;if(s[at]==','){++at;continue;}if(s[at]==']'){++at;return true;}return false;}}
        if(c=='{'){v.type=Type::Object;++at;space(s,at);if(at<s.size()&&s[at]=='}'){++at;return true;}
            for(;;){space(s,at);std::string key;if(!str(s,at,key))return false;space(s,at);if(at>=s.size()||s[at]!=':')return false;++at;Json item;if(!value(s,at,item,depth+1))return false;v.fields.emplace_back(std::move(key),std::move(item));space(s,at);if(at>=s.size())return false;if(s[at]==','){++at;continue;}if(s[at]=='}'){++at;return true;}return false;}}
        if(c=='-'||(c>='0'&&c<='9')){size_t start=at;if(s[at]=='-')++at;if(at>=s.size()||!(s[at]>='0'&&s[at]<='9'))return false;while(at<s.size()&&((s[at]>='0'&&s[at]<='9')||s[at]=='.'||s[at]=='e'||s[at]=='E'||s[at]=='+'||s[at]=='-'))++at;
            std::string digits(s.substr(start,at-start));char* end=nullptr;v.type=Type::Number;v.number=std::strtod(digits.c_str(),&end);return end&&*end==0&&std::isfinite(v.number);}
        return false;
    }
};
// ---- Synced lyrics --------------------------------------------------------------------------
struct LyricLine {double time=0;std::wstring text;bool operator==(const LyricLine&)const=default;};
// LRC: "[mm:ss.xx]text", several time tags per line allowed, optional [offset:+/-ms]
// (positive shows lines earlier). Metadata tags and untimed lines are ignored.
inline std::vector<LyricLine> parseLrc(std::wstring_view text){
    std::vector<LyricLine> lines;double offset=0;size_t start=0;
    auto trim=[](std::wstring_view v){while(!v.empty()&&std::iswspace(v.front()))v.remove_prefix(1);while(!v.empty()&&std::iswspace(v.back()))v.remove_suffix(1);return std::wstring(v);};
    while(start<=text.size()&&lines.size()<4000){size_t end=text.find(L'\n',start);if(end==std::wstring_view::npos)end=text.size();std::wstring_view line=text.substr(start,end-start);start=end+1;
        std::vector<double> times;size_t at=0;
        while(at<line.size()&&line[at]==L'['){size_t close=line.find(L']',at);if(close==std::wstring_view::npos)break;std::wstring_view tag=line.substr(at+1,close-at-1);at=close+1;
            if(tag.size()>7&&tag.substr(0,7)==L"offset:"){try{offset=std::stod(std::wstring(tag.substr(7)))/1000.;}catch(...){}continue;}
            size_t colon=tag.find(L':');if(colon==0||colon==std::wstring_view::npos)continue;
            bool digits=true;for(size_t i=0;i<tag.size();++i)if(i!=colon&&!std::iswdigit(tag[i])&&tag[i]!=L'.'&&tag[i]!=L':')digits=false;if(!digits)continue;
            try{double minutes=std::stod(std::wstring(tag.substr(0,colon)));std::wstring rest(tag.substr(colon+1));for(auto& ch:rest)if(ch==L':')ch=L'.';double secs=std::stod(rest);if(secs<60&&minutes>=0)times.push_back(minutes*60+secs);}catch(...){}}
        if(times.empty())continue;std::wstring words=trim(line.substr(at));for(double t:times)lines.push_back({t,words});}
    for(auto& l:lines)l.time=std::max(0.,l.time-offset);
    std::stable_sort(lines.begin(),lines.end(),[](auto& a,auto& b){return a.time<b.time;});
    return lines;
}
// The line being sung at `position` (seconds), or -1 before the first one.
inline int lyricIndex(const std::vector<LyricLine>& lines,double position){
    auto it=std::upper_bound(lines.begin(),lines.end(),position,[](double p,const LyricLine& l){return p<l.time;});return int(it-lines.begin())-1;
}
// Seconds until the next line starts after `position`, or -1 when none follows.
inline double nextLyricIn(const std::vector<LyricLine>& lines,double position){
    auto it=std::upper_bound(lines.begin(),lines.end(),position,[](double p,const LyricLine& l){return p<l.time;});return it==lines.end()?-1:it->time-position;
}
// ---- Search query from what the player reports -------------------------------------------------
struct LyricsQuery {std::wstring title,artist;bool operator==(const LyricsQuery&)const=default;};
inline std::wstring lyricsFold(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
// Browsers and video sites report "Artist - Song (Official Video)" by a channel; players report
// clean fields. Noise in brackets and remaster suffixes is dropped either way.
inline LyricsQuery cleanLyricsQuery(std::wstring title,std::wstring artist,bool browser){
    auto trim=[](std::wstring s){while(!s.empty()&&(std::iswspace(s.back())||s.back()==L'-'||s.back()==L'|'||s.back()==L'–'))s.pop_back();size_t a=0;while(a<s.size()&&std::iswspace(s[a]))++a;return s.substr(a);};
    static constexpr std::wstring_view noise[]={L"official",L"video",L"audio",L"lyric",L"visuali",L"m/v",L"mv",L"hd",L"4k",L"remaster",L"explicit",L"clean",L"radio edit",L"single",L"version",L"mono",L"stereo",L"out now",L"music video",L"full song",L"animated"};
    // After a dash only release notes count ("- Remastered 2011", "- Radio Edit"), so a title
    // such as "Video Games" or "Live Forever" is never mistaken for noise.
    auto suffix=[&](const std::wstring& part){auto f=lyricsFold(part);for(auto n:{L"remaster",L"radio edit",L" version",L"mono",L"stereo",L"bonus track",L"demo",L" mix",L" edit"})if(f.find(n)!=std::wstring::npos)return true;return false;};
    auto noisy=[&](const std::wstring& part){auto f=lyricsFold(part);for(auto n:noise){size_t at=f.find(n);while(at!=std::wstring::npos){bool left=at==0||!std::iswalnum(f[at-1]);if(left)return true;at=f.find(n,at+1);}}return false;};
    // Bracketed segments: (Official Video), [Lyrics], (feat. X), 【MV】.
    for(int pass=0;pass<6;++pass){bool changed=false;
        for(auto [open,close]:{std::pair{L'(',L')'},std::pair{L'[',L']'},std::pair{L'【',L'】'}}){size_t a=title.find(open);if(a==std::wstring::npos)continue;size_t b=title.find(close,a);if(b==std::wstring::npos)continue;std::wstring inner=title.substr(a+1,b-a-1);auto f=lyricsFold(inner);
            if(noisy(inner)||f.starts_with(L"feat")||f.starts_with(L"ft.")||f.starts_with(L"with ")||f.starts_with(L"prod")){title=trim(title.erase(a,b-a+1));changed=true;}}
        if(!changed)break;}
    // Trailing " feat. X" / " ft. X".
    {auto f=lyricsFold(title);for(auto key:{L" feat. ",L" feat ",L" ft. ",L" ft "}){size_t at=f.find(key);if(at!=std::wstring::npos&&at>0){title=trim(title.substr(0,at));break;}}}
    // "Artist - Title" (hyphen, en or em dash with spaces around it).
    for(auto sep:{L" - ",L" – ",L" — ",L" | "}){size_t at=title.find(sep);if(at==std::wstring::npos)continue;std::wstring left=trim(title.substr(0,at)),right=trim(title.substr(at+std::wstring_view(sep).size()));if(left.empty()||right.empty())break;
        auto fa=lyricsFold(artist),fl=lyricsFold(left);
        if(suffix(right)){title=left;break;}
        const bool channel=fa.ends_with(L"vevo")||fa.ends_with(L" - topic")||fa.ends_with(L"official")||fa.ends_with(L"records")||fa.ends_with(L"music")||fa.empty();
        if(browser||channel||(!fa.empty()&&(fa.find(fl)!=std::wstring::npos||fl.find(fa)!=std::wstring::npos))){artist=left;title=right;}
        break;}
    // A trailing "- Remastered 2011" style suffix after the split.
    for(auto sep:{L" - ",L" – "}){size_t at=title.rfind(sep);if(at!=std::wstring::npos&&at>0&&suffix(title.substr(at+3)))title=trim(title.substr(0,at));}
    {auto f=lyricsFold(artist);if(f.ends_with(L" - topic"))artist=trim(artist.substr(0,artist.size()-8));else if(f.ends_with(L"vevo")&&artist.size()>4)artist=trim(artist.substr(0,artist.size()-4));}
    // Quotes some channels wrap titles in.
    if(title.size()>2&&(title.front()==L'"'||title.front()==L'“')&&(title.back()==L'"'||title.back()==L'”'))title=title.substr(1,title.size()-2);
    return {trim(title),trim(artist)};
}
// "A, B & C" -> "A": the fallback query when the full credit finds nothing.
inline std::wstring primaryArtist(const std::wstring& artist){
    auto f=lyricsFold(artist);size_t cut=artist.size();for(auto sep:{L",",L" & ",L" x ",L" feat",L" ft.",L" and ",L";",L"/"}){size_t at=f.find(sep);if(at!=std::wstring::npos&&at>0)cut=std::min(cut,at);}
    std::wstring a=artist.substr(0,cut);while(!a.empty()&&std::iswspace(a.back()))a.pop_back();return a;
}
// ---- Choosing among search results ---------------------------------------------------------
struct LyricsResult {enum class Kind{None,Synced,Instrumental} kind=Kind::None;std::wstring lrc;};
// Only a version whose length matches (within 3 s, or 10 s when nothing closer exists) is
// trusted, so a live or extended cut never scrolls out of step. Without a known length the
// first synced result is used.
inline LyricsResult pickLyrics(const Json& results,double duration){
    LyricsResult best;double bestGap=1e9;if(results.type!=Json::Type::Array)return best;
    for(auto& r:results.items){if(r.type!=Json::Type::Object)continue;const double d=r.num("duration",0);const double gap=duration>0&&d>0?std::abs(d-duration):duration>0?1e6:0;
        if(duration>0&&gap>10)continue;const bool instrumental=r.flag("instrumental");auto synced=r.string("syncedLyrics");
        if(!instrumental&&synced.find('[')==std::string::npos)continue;
        // Synced beats instrumental at the same distance; closer beats both.
        const double score=gap+(instrumental?.5:0);if(score<bestGap){bestGap=score;best.kind=instrumental?LyricsResult::Kind::Instrumental:LyricsResult::Kind::Synced;best.lrc=instrumental?std::wstring{}:fromUtf8(synced);}}
    if(best.kind==LyricsResult::Kind::Synced&&parseLrc(best.lrc).empty())return {};
    return best;
}
// ---- Local cache ---------------------------------------------------------------------------
inline uint64_t fnv1a(std::string_view s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}
// One file per track: what LRCLIB returned, or that it had nothing (retried after 14 days).
struct LyricsCacheEntry {enum class Status{Found,Missing,Instrumental} status=Status::Missing;int64_t time=0;std::wstring lrc;bool operator==(const LyricsCacheEntry&)const=default;};
inline std::string lyricsCacheName(const LyricsQuery& q,double duration){char b[32];snprintf(b,sizeof b,"%016llx.lrc",static_cast<unsigned long long>(fnv1a(toUtf8(lyricsFold(q.artist)+L"\n"+lyricsFold(q.title)+L"\n"+std::to_wstring(int(std::lround(duration)))))));return b;}
inline std::string writeLyricsCache(const LyricsCacheEntry& e){std::string s="arnav-lyrics 1\nstatus ";s+=e.status==LyricsCacheEntry::Status::Found?"found":e.status==LyricsCacheEntry::Status::Instrumental?"instrumental":"missing";s+="\ntime "+std::to_string(e.time)+"\n";if(e.status==LyricsCacheEntry::Status::Found)s+=toUtf8(e.lrc);return s;}
inline std::optional<LyricsCacheEntry> readLyricsCache(std::string_view s){
    auto line=[&](std::string_view& rest){size_t n=rest.find('\n');if(n==std::string_view::npos)return std::optional<std::string_view>{};auto l=rest.substr(0,n);rest.remove_prefix(n+1);return std::optional<std::string_view>{l};};
    std::string_view rest=s;auto a=line(rest),b=line(rest),c=line(rest);if(!a||!b||!c||*a!="arnav-lyrics 1"||!b->starts_with("status ")||!c->starts_with("time "))return std::nullopt;
    LyricsCacheEntry e;auto status=b->substr(7);if(status=="found")e.status=LyricsCacheEntry::Status::Found;else if(status=="missing")e.status=LyricsCacheEntry::Status::Missing;else if(status=="instrumental")e.status=LyricsCacheEntry::Status::Instrumental;else return std::nullopt;
    try{e.time=std::stoll(std::string(c->substr(5)));}catch(...){return std::nullopt;}
    if(e.status==LyricsCacheEntry::Status::Found){e.lrc=fromUtf8(rest);if(parseLrc(e.lrc).empty())return std::nullopt;}
    return e;
}
inline bool lyricsCacheFresh(const LyricsCacheEntry& e,int64_t now){return e.status!=LyricsCacheEntry::Status::Missing||now-e.time<14*86400;}
// ---- Percent-encoding for the query string ----------------------------------------------------
inline std::string urlEncode(std::string_view utf8){static const char* hex="0123456789ABCDEF";std::string out;for(unsigned char c:utf8){if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
inline std::string lyricsSearchPath(const LyricsQuery& q){return "/api/search?track_name="+urlEncode(toUtf8(q.title))+"&artist_name="+urlEncode(toUtf8(q.artist));}
}
