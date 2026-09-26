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
struct AppRelease {AppVersion version;std::string tag,zipUrl,shaUrl;};
// From GitHub's list of releases (newest first, previews included): the newest one after `current` that is published
// (not a draft) and has both "…-win-x64.zip" and its ".sha256", from this repository's own download address.
inline std::optional<AppRelease> newestRelease(std::string_view json,const AppVersion& current){
    auto doc=Json::parse(json);if(!doc||doc->type!=Json::Type::Array)return std::nullopt;std::optional<AppRelease> best;
    constexpr std::string_view origin="https://github.com/Arnav-Dugad/arnav-island/releases/download/";
    auto ends=[](std::string_view s,std::string_view tail){return s.size()>=tail.size()&&s.substr(s.size()-tail.size())==tail;};
    for(const auto& r:doc->items){if(r.type!=Json::Type::Object||r.flag("draft"))continue;AppRelease candidate;candidate.tag=r.string("tag_name");candidate.version=parseVersion(candidate.tag);
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
}
