#pragma once
#include "Interaction/DashboardModel.h"
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <memory>
#include <string>
#include <vector>
namespace nexus {
// One copied item, kept only in memory. Images keep their device-independent
// bitmap so they can be copied back; files keep their paths, never contents.
struct ClipEntry {
    enum class Kind { Text,Link,Image,Files };
    Kind kind=Kind::Text;std::wstring text;std::vector<std::wstring> files;std::vector<uint8_t> dib;
    std::shared_ptr<const Artwork> thumbnail,sourceIcon;std::wstring source,sourcePath;double time=0;uint64_t id=0;uint32_t imageWidth=0,imageHeight=0;
    size_t bytes()const{size_t n=text.size()*sizeof(wchar_t)+dib.size();for(auto& f:files)n+=f.size()*sizeof(wchar_t);if(thumbnail)n+=thumbnail->pixels.size();return n;}
    bool sameContent(const ClipEntry& o)const{return kind==o.kind&&text==o.text&&files==o.files&&dib.size()==o.dib.size()&&dib==o.dib;}
};
// A web or mail address on its own (surrounding spaces allowed).
inline bool isLink(const std::wstring& raw){
    size_t a=raw.find_first_not_of(L" \t\r\n"),b=raw.find_last_not_of(L" \t\r\n");if(a==std::wstring::npos)return false;
    std::wstring s=raw.substr(a,b-a+1);if(s.size()>2048||s.find_first_of(L" \t\r\n")!=std::wstring::npos)return false;
    std::wstring lower=s;for(auto& c:lower)c=wchar_t(std::towlower(c));
    for(auto scheme:{L"https://",L"http://",L"mailto:",L"ftp://"})if(lower.starts_with(scheme)&&lower.size()>wcslen(scheme)+2)return true;
    return lower.starts_with(L"www.")&&lower.find(L'.',4)!=std::wstring::npos;
}
// One tidy line for display: whitespace runs collapse, long text is shortened.
inline std::wstring clipPreview(const std::wstring& text,size_t limit=140){
    std::wstring out;out.reserve(std::min(text.size(),limit+1));bool space=false;
    for(wchar_t c:text){if(std::iswspace(c)){space=!out.empty();continue;}if(space){out.push_back(L' ');space=false;}out.push_back(c);if(out.size()>=limit)break;}
    if(out.size()>=limit&&text.size()>limit){while(!out.empty()&&out.back()==L' ')out.pop_back();out+=L"…";}
    return out;
}
// Password managers and credential tools, whose copies are never kept even if
// they forget to mark them private.
inline bool privateSource(const std::wstring& exe){
    std::wstring e=exe;for(auto& c:e)c=wchar_t(std::towlower(c));
    for(auto name:{L"keepass",L"keepassxc",L"1password",L"bitwarden",L"lastpass",L"dashlane",L"nordpass",L"roboform",L"enpass",L"keeper",L"proton pass",L"protonpass",L"credentialuibroker"})
        if(e.find(name)!=std::wstring::npos)return true;
    return false;
}
// Newest first, bounded by count and memory. Copying the same thing again moves
// it to the top instead of adding a duplicate.
class ClipboardHistory {
    std::deque<ClipEntry> entries_;uint64_t next_=0;
public:
    static constexpr size_t limit=24,memoryLimit=48u*1024*1024;
    bool paused=false;
    const std::deque<ClipEntry>& entries()const{return entries_;}
    bool add(ClipEntry e,double now){
        if(paused)return false;
        if(e.kind!=ClipEntry::Kind::Image&&e.kind!=ClipEntry::Kind::Files&&clipPreview(e.text).empty())return false;
        if(e.bytes()>memoryLimit)return false;
        auto same=std::find_if(entries_.begin(),entries_.end(),[&](auto& x){return x.sameContent(e);});
        if(same!=entries_.end()){e.id=same->id;entries_.erase(same);}else e.id=++next_;
        e.time=now;entries_.push_front(std::move(e));
        size_t total=0;for(auto& x:entries_)total+=x.bytes();
        while(entries_.size()>limit||(entries_.size()>1&&total>memoryLimit)){total-=entries_.back().bytes();entries_.pop_back();}
        return true;
    }
    const ClipEntry* find(uint64_t id)const{for(auto& e:entries_)if(e.id==id)return &e;return nullptr;}
    void remove(uint64_t id){std::erase_if(entries_,[&](auto& e){return e.id==id;});}
    void clear(){entries_.clear();}
};
// "Just now", "4 min", "2 h", "3 d".
inline std::wstring ageText(double seconds){
    if(seconds<45)return L"Just now";if(seconds<3600)return std::to_wstring(int(std::max(1.,std::round(seconds/60))))+L" min";
    if(seconds<86400)return std::to_wstring(int(seconds/3600))+L" h";return std::to_wstring(int(seconds/86400))+L" d";
}
}
