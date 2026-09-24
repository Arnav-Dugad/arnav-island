#pragma once
#include "Productivity/Commands.h"
#include "Media/Lyrics.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace nexus {
// Instant file results. Typed text becomes a FileSpec (words, types, a date range and
// a known folder), which becomes a Windows Search index query; the same spec drives
// the slower folder scan used when the index is off. Pure and unit-tested.
struct FileSpec {std::vector<std::wstring> terms,extensions;std::wstring kind,date,folder;bool empty()const{return terms.empty()&&extensions.empty()&&kind.empty()&&date.empty();}};
// Folder words: "in downloads", "on the desktop".
inline constexpr std::wstring_view searchFolders[]={L"downloads",L"documents",L"desktop",L"pictures",L"music",L"videos"};
inline FileSpec fileSpec(const std::wstring& typed){
    FileSpec s;std::wstring rest=L" "+lowered(trimmed(typed))+L" ";
    for(auto f:searchFolders)for(auto prefix:{L" in the ",L" on the ",L" in my ",L" in ",L" on ",L" from "}){const std::wstring key=prefix+std::wstring(f)+L" ";if(auto at=rest.find(key);at!=std::wstring::npos){rest.replace(at,key.size(),L" ");if(s.folder.empty())s.folder=f;}}
    for(auto& d:searchDates){const auto key=L" "+std::wstring(d.phrase)+L" ";for(auto at=rest.find(key);at!=std::wstring::npos;at=rest.find(key)){rest.replace(at,key.size(),L" ");if(s.date.empty())s.date=d.phrase;}}
    for(auto& k:searchKinds){const auto key=L" "+std::wstring(k.phrase)+L" ";auto at=rest.find(key);if(at==std::wstring::npos)continue;rest.replace(at,key.size(),L" ");const std::wstring aqs(k.aqs);
        if(aqs.starts_with(L"ext:")){auto e=aqs.substr(4);if(std::find(s.extensions.begin(),s.extensions.end(),e)==s.extensions.end())s.extensions.push_back(e);}else if(s.kind.empty())s.kind=aqs.substr(5);}
    for(auto& w:words(rest)){if(w==L"from"||w==L"modified"||w==L"in"||w==L"named"||w==L"called"||w==L"the"||w==L"on"||w==L"my"||w==L"a")continue;s.terms.push_back(w);}
    return s;
}
// A calendar range [start, end) in local days, from a date phrase and today's date (Monday starts a week).
struct Day {int y=0,m=0,d=0;bool operator==(const Day&)const=default;};
inline int daysInMonth(int y,int m){static const int n[]={31,28,31,30,31,30,31,31,30,31,30,31};return m==2&&((y%4==0&&y%100!=0)||y%400==0)?29:n[m-1];}
inline Day addDays(Day day,int delta){while(delta>0){if(++day.d>daysInMonth(day.y,day.m)){day.d=1;if(++day.m>12){day.m=1;++day.y;}}--delta;}while(delta<0){if(--day.d<1){if(--day.m<1){day.m=12;--day.y;}day.d=daysInMonth(day.y,day.m);}++delta;}return day;}
inline std::optional<std::pair<Day,Day>> dateRange(const std::wstring& phrase,Day today,int weekday/*0 Sunday*/){
    const int sinceMonday=(weekday+6)%7;const Day monday=addDays(today,-sinceMonday);
    if(phrase==L"today")return std::pair{today,addDays(today,1)};
    if(phrase==L"yesterday")return std::pair{addDays(today,-1),today};
    if(phrase==L"this week")return std::pair{monday,addDays(monday,7)};
    if(phrase==L"last week")return std::pair{addDays(monday,-7),monday};
    if(phrase==L"this month")return std::pair{Day{today.y,today.m,1},today.m==12?Day{today.y+1,1,1}:Day{today.y,today.m+1,1}};
    if(phrase==L"last month")return std::pair{today.m==1?Day{today.y-1,12,1}:Day{today.y,today.m-1,1},Day{today.y,today.m,1}};
    if(phrase==L"this year")return std::pair{Day{today.y,1,1},Day{today.y+1,1,1}};
    if(phrase==L"last year")return std::pair{Day{today.y-1,1,1},Day{today.y,1,1}};
    return std::nullopt;
}
// Files nobody opens from a launcher: compiler and cache output, and the folders tools keep them in.
inline constexpr std::wstring_view noiseExtensions[]={L".pyc",L".pyo",L".obj",L".o",L".pdb",L".ilk",L".tmp",L".cache",L".lock",L".class",L".dll",L".d",L".tlog",L".idb"};
inline constexpr std::wstring_view noiseFolders[]={L"node_modules",L".git",L"__pycache__",L".venv",L"site-packages",L".cache",L"CMakeFiles"};
inline bool noisyFile(const std::wstring& path){const auto p=lowered(path);for(auto e:noiseExtensions)if(p.ends_with(e))return true;for(auto d:noiseFolders){const auto part=L"\\"+lowered(std::wstring(d))+L"\\";if(p.find(part)!=std::wstring::npos)return true;}return false;}
// Windows Search SQL: string literals double their quotes; LIKE escapes its wildcards.
inline std::wstring sqlLiteral(const std::wstring& s){std::wstring o;for(wchar_t c:s){if(c==L'\'')o+=L"''";else if(c>=0x20)o+=c;}return o;}
inline bool plainTerm(const std::wstring& w){if(w.empty()||w.size()>64)return false;for(wchar_t c:w)if(!((c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')))return false;return true;}
inline std::wstring likeTerm(const std::wstring& w){std::wstring o;for(wchar_t c:w){if(c==L'%'||c==L'_'||c==L'['){o+=L'[';o+=c;o+=L']';}else o+=c;}return sqlLiteral(o);}
// `since`/`before` are UTC timestamps ("2026-09-21 00:00:00"), empty for no bound.
inline std::wstring searchSql(const FileSpec& s,const std::wstring& scopeUrl,const std::wstring& since,const std::wstring& before,size_t top){
    // System.ItemUrl carries the real path; ItemPathDisplay shows localized names ("Public Documents").
    std::wstring sql=L"SELECT TOP "+std::to_wstring(top)+L" \"System.ItemUrl\",\"System.FileName\",\"System.DateModified\",\"System.Size\",\"System.ItemType\" FROM \"SystemIndex\" WHERE SCOPE='"+sqlLiteral(scopeUrl)+L"'";
    // Word prefixes use the full-text index; anything with punctuation falls back to LIKE.
    for(auto& t:s.terms)sql+=plainTerm(t)?L" AND CONTAINS(\"System.FileName\",'\"" +t+L"*\"')":L" AND \"System.FileName\" LIKE '%"+likeTerm(t)+L"%'";
    if(!s.extensions.empty()){sql+=L" AND (";for(size_t i=0;i<s.extensions.size();++i){if(i)sql+=L" OR ";sql+=L"\"System.FileExtension\"='"+sqlLiteral(s.extensions[i])+L"'";}sql+=L")";}
    if(!s.kind.empty())sql+=L" AND \"System.Kind\"='"+sqlLiteral(s.kind)+L"'";
    for(auto e:noiseExtensions)sql+=L" AND \"System.FileExtension\"<>'"+std::wstring(e)+L"'";
    for(auto d:noiseFolders)sql+=L" AND NOT \"System.ItemPathDisplay\" LIKE '%\\"+likeTerm(std::wstring(d))+L"\\%'";
    if(!since.empty())sql+=L" AND \"System.DateModified\">='"+sqlLiteral(since)+L"'";if(!before.empty())sql+=L" AND \"System.DateModified\"<'"+sqlLiteral(before)+L"'";
    sql+=s.terms.empty()?L" ORDER BY \"System.DateModified\" DESC":L" ORDER BY \"System.Search.Rank\" DESC";
    return sql;
}
// "file:C:/Users/Public/Documents/a%20b.pdf" -> "C:\\Users\\Public\\Documents\\a b.pdf" (escapes decoded when present).
inline std::wstring pathFromItemUrl(const std::wstring& url){
    if(!lowered(url).starts_with(L"file:"))return {};std::wstring p=url.substr(5);while(p.starts_with(L"///"))p.erase(0,1);if(p.starts_with(L"//")&&p.size()>3&&p[3]==L':')p.erase(0,2);
    std::wstring out;for(size_t i=0;i<p.size();++i){wchar_t c=p[i];
        if(c==L'%'&&i+2<p.size()&&std::iswxdigit(p[i+1])&&std::iswxdigit(p[i+2])){auto hex=[](wchar_t h){return h<=L'9'?h-L'0':(h|32)-L'a'+10;};std::string bytes;
            while(i+2<p.size()&&p[i]==L'%'&&std::iswxdigit(p[i+1])&&std::iswxdigit(p[i+2])){bytes+=char(hex(p[i+1])*16+hex(p[i+2]));i+=3;}--i;out+=fromUtf8(bytes);continue;}
        out+=c==L'/'?L'\\':c;}
    return out;
}
// The scan fallback's own tests of a file against the spec.
inline std::wstring kindOfExtension(const std::wstring& ext){
    static constexpr std::wstring_view pictures[]={L".png",L".jpg",L".jpeg",L".gif",L".bmp",L".webp",L".heic",L".tif",L".tiff",L".svg",L".ico",L".raw"},videos[]={L".mp4",L".mkv",L".mov",L".avi",L".wmv",L".webm",L".m4v"},
        music[]={L".mp3",L".flac",L".wav",L".m4a",L".aac",L".ogg",L".wma",L".opus"},documents[]={L".pdf",L".doc",L".docx",L".txt",L".rtf",L".odt",L".xls",L".xlsx",L".csv",L".ppt",L".pptx",L".md",L".epub"};
    auto e=lowered(ext);for(auto x:pictures)if(e==x)return L"picture";for(auto x:videos)if(e==x)return L"video";for(auto x:music)if(e==x)return L"music";for(auto x:documents)if(e==x)return L"document";return {};
}
inline bool fileMatches(const FileSpec& s,const std::wstring& name,bool folder){
    const auto n=lowered(name);for(auto& t:s.terms)if(n.find(t)==std::wstring::npos)return false;
    const auto dot=n.find_last_of(L'.');const std::wstring ext=folder||dot==std::wstring::npos?L"":n.substr(dot);
    if(!s.extensions.empty()&&std::find(s.extensions.begin(),s.extensions.end(),ext)==s.extensions.end())return false;
    if(!s.kind.empty()){if(s.kind==L"folder")return folder;if(folder||kindOfExtension(ext)!=s.kind)return false;}
    return true;
}
// Ranking: how the name matches, how often and how lately you opened it, how new it is.
inline double fileRank(const std::wstring& name,const FileSpec& s,double frecency,double ageDays){
    double score=0;const auto n=lowered(name);const std::wstring whole=[&]{std::wstring q;for(auto& t:s.terms){if(!q.empty())q+=L' ';q+=t;}return q;}();
    if(!whole.empty()){if(n.starts_with(whole))score+=60;else{auto m=matchMarks(name,whole);score+=m.empty()?0:m.front().first==0?45:30;}}
    score+=std::min(40.,frecency*12);score+=ageDays<1?14:ageDays<7?9:ageDays<30?4:0;score-=std::min(10.,double(n.size())/12);
    return score;
}
inline std::wstring fileAge(double seconds){
    if(seconds<90)return L"just now";if(seconds<3600)return std::to_wstring(int(seconds/60))+L" min ago";if(seconds<86400)return std::to_wstring(int(seconds/3600))+(seconds<7200?L" hour ago":L" hours ago");
    const int days=int(seconds/86400);if(days==1)return L"yesterday";if(days<31)return std::to_wstring(days)+L" days ago";if(days<365)return std::to_wstring(days/30)+(days<60?L" month ago":L" months ago");
    return std::to_wstring(days/365)+(days<730?L" year ago":L" years ago");
}
}
