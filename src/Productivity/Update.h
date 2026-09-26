#pragma once
#include "Media/Lyrics.h"
#include <climits>
#include <optional>
#include <string>
#include <string_view>
namespace nexus {
// 0.18: updating the island from its own GitHub releases. The pure parts: versions, choosing a release, checksums.
// A version: 0.18.0 or 0.18.0-preview.2 (a leading v allowed). A final release sorts after every preview of it.
struct AppVersion {
    int major=0,minor=0,patch=0,preview=INT_MAX;bool valid=false;
    auto operator<=>(const AppVersion& o)const{if(auto c=major<=>o.major;c!=0)return c;if(auto c=minor<=>o.minor;c!=0)return c;if(auto c=patch<=>o.patch;c!=0)return c;return preview<=>o.preview;}
    bool operator==(const AppVersion& o)const{return major==o.major&&minor==o.minor&&patch==o.patch&&preview==o.preview;}
};
inline AppVersion parseVersion(std::string_view text){
    AppVersion v;if(!text.empty()&&(text[0]=='v'||text[0]=='V'))text.remove_prefix(1);
    auto number=[&](int& out)->bool{if(text.empty()||text[0]<'0'||text[0]>'9')return false;long n=0;while(!text.empty()&&text[0]>='0'&&text[0]<='9'){n=n*10+(text[0]-'0');if(n>100000)return false;text.remove_prefix(1);}out=int(n);return true;};
    if(!number(v.major)||text.empty()||text[0]!='.')return {};text.remove_prefix(1);if(!number(v.minor)||text.empty()||text[0]!='.')return {};text.remove_prefix(1);if(!number(v.patch))return {};
    if(!text.empty()){constexpr std::string_view tag="-preview.";if(text.substr(0,tag.size())!=tag)return {};text.remove_prefix(tag.size());if(!number(v.preview)||!text.empty())return {};}
    v.valid=true;return v;
}
inline std::wstring versionText(const AppVersion& v){if(!v.valid)return {};std::wstring s=std::to_wstring(v.major)+L"."+std::to_wstring(v.minor)+L"."+std::to_wstring(v.patch);if(v.preview!=INT_MAX)s+=L"-preview."+std::to_wstring(v.preview);return s;}
// One release worth installing: its version, and where its Windows ZIP and that ZIP's checksum are.
struct AppRelease {AppVersion version;std::string tag,zipUrl,shaUrl,notes;};
// From GitHub's list of releases (newest first, previews included): the newest one after `current` that is published
// (not a draft) and has both "…-win-x64.zip" and its ".sha256", from this repository's own download address.
inline std::optional<AppRelease> newestRelease(std::string_view json,const AppVersion& current){
    auto doc=Json::parse(json);if(!doc||doc->type!=Json::Type::Array)return std::nullopt;std::optional<AppRelease> best;
    constexpr std::string_view origin="https://github.com/Arnav-Dugad/arnav-island/releases/download/";
    auto ends=[](std::string_view s,std::string_view tail){return s.size()>=tail.size()&&s.substr(s.size()-tail.size())==tail;};
    for(const auto& r:doc->items){if(r.type!=Json::Type::Object||r.flag("draft"))continue;AppRelease candidate;candidate.tag=r.string("tag_name");candidate.version=parseVersion(candidate.tag);candidate.notes=r.string("body");
        if(!candidate.version.valid||!(current<candidate.version))continue;
        auto* assets=r.find("assets");if(!assets||assets->type!=Json::Type::Array)continue;
        for(const auto& a:assets->items){if(a.type!=Json::Type::Object)continue;const auto name=a.string("name"),url=a.string("browser_download_url");if(url.substr(0,origin.size())!=origin)continue;
            if(ends(name,"-win-x64.zip"))candidate.zipUrl=url;else if(ends(name,"-win-x64.zip.sha256"))candidate.shaUrl=url;}
        if(candidate.zipUrl.empty()||candidate.shaUrl.empty())continue;if(!best||best->version<candidate.version)best=candidate;}
    return best;
}
// A checksum file ("<64 hex digits>  <name>") against a digest in hex: the same 64 digits, whatever their case.
inline bool checksumMatches(std::string_view file,std::string_view digest){
    size_t at=0;while(at<file.size()&&(file[at]==' '||file[at]=='\t'||file[at]=='\r'||file[at]=='\n'||file[at]=='\xef'||file[at]=='\xbb'||file[at]=='\xbf'))++at;
    if(file.size()<at+64||digest.size()!=64)return false;
    for(size_t k=0;k<64;++k){char a=file[at+k],b=digest[k];auto lower=[](char c){return c>='A'&&c<='F'?char(c-'A'+'a'):c;};a=lower(a);b=lower(b);if(!((a>='0'&&a<='9')||(a>='a'&&a<='f'))||a!=b)return false;}
    return at+64==file.size()||file[at+64]==' '||file[at+64]=='\t'||file[at+64]=='\r'||file[at+64]=='\n';
}
// "https://host/path" into its host and path (empty when it isn't an HTTPS address).
inline std::pair<std::wstring,std::wstring> splitUrl(const std::string& url){
    constexpr std::string_view scheme="https://";if(url.substr(0,scheme.size())!=scheme)return {};const auto slash=url.find('/',scheme.size());if(slash==std::string::npos)return {};
    return {fromUtf8(url.substr(scheme.size(),slash-scheme.size())),fromUtf8(url.substr(slash))};
}
// 0.18.1: a release's notes (GitHub Markdown) as lines for the island's What's new sheet: section headings, then their
// points, with the Markdown taken out (emphasis, code marks, links keep their words). The Checks section is left out,
// and at most `limit` lines are kept.
struct NoteLine {bool heading=false,sub=false;std::wstring text;bool operator==(const NoteLine&)const=default;};
inline std::vector<NoteLine> whatsNewLines(std::string_view md,size_t limit=48){
    std::vector<NoteLine> out;bool skipping=false;
    auto plain=[](std::string_view t){std::string o;for(size_t k=0;k<t.size();++k){const char c=t[k];
            if(c=='*'||c=='`'||(c=='_'&&(k==0||t[k-1]==' '||k+1==t.size()||t[k+1]==' ')))continue;
            // [words](address) keeps its words.
            if(c=='['){const auto close=t.find("](",k);const auto end=close==std::string_view::npos?close:t.find(')',close);if(end!=std::string_view::npos){o+=std::string(t.substr(k+1,close-k-1));k=end;continue;}}
            o+=c;}
        while(!o.empty()&&(o.back()==' '||o.back()=='\r'))o.pop_back();size_t a=o.find_first_not_of(' ');return a==std::string::npos?std::string():o.substr(a);};
    size_t at=0;while(at<=md.size()&&out.size()<limit){const auto end=md.find('\n',at);std::string_view line=md.substr(at,end==std::string_view::npos?std::string_view::npos:end-at);at=end==std::string_view::npos?md.size()+1:end+1;
        size_t indent=0;while(indent<line.size()&&line[indent]==' ')++indent;std::string_view body=line.substr(indent);
        if(body.empty()||body.front()=='|')continue;
        if(body.substr(0,2)=="# ")continue;
        if(body.substr(0,3)=="## "){const auto title=plain(body.substr(3));skipping=title=="Checks";if(!skipping&&!title.empty())out.push_back({true,false,fromUtf8(title)});continue;}
        if(skipping)continue;
        bool bullet=false;if(body.substr(0,2)=="- "||body.substr(0,2)=="* "){body.remove_prefix(2);bullet=true;}
        else{size_t digits=0;while(digits<body.size()&&body[digits]>='0'&&body[digits]<='9')++digits;if(digits&&body.substr(digits,2)==". "){body.remove_prefix(digits+2);bullet=true;}}
        const auto text=plain(body);if(text.empty())continue;out.push_back({false,bullet&&indent>=2,fromUtf8(text)});}
    return out;
}
}
