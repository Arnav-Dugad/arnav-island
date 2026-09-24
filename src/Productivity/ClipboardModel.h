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
    // Pinned copies stay at the top and are never pushed out by newer ones.
    bool pinned=false;
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
    static constexpr size_t limit=24,memoryLimit=48u*1024*1024,pinLimit=12;
    bool paused=false;
    const std::deque<ClipEntry>& entries()const{return entries_;}
    bool add(ClipEntry e,double now){
        if(paused)return false;
        if(e.kind!=ClipEntry::Kind::Image&&e.kind!=ClipEntry::Kind::Files&&clipPreview(e.text).empty())return false;
        if(e.bytes()>memoryLimit)return false;
        auto same=std::find_if(entries_.begin(),entries_.end(),[&](auto& x){return x.sameContent(e);});
        if(same!=entries_.end()){e.id=same->id;e.pinned=e.pinned||same->pinned;entries_.erase(same);}else e.id=++next_;
        e.time=now;entries_.push_front(std::move(e));trim();
        return true;
    }
    // Oldest unpinned copies go first; pinned ones never count against the limit.
    void trim(){
        size_t total=0,unpinned=0;for(auto& x:entries_){total+=x.bytes();unpinned+=!x.pinned;}
        // The newest copy (index 0) is never the one evicted.
        for(size_t i=entries_.size();i-->1&&(unpinned>limit||total>memoryLimit);)if(!entries_[i].pinned){total-=entries_[i].bytes();--unpinned;entries_.erase(entries_.begin()+std::ptrdiff_t(i));}
    }
    // Returns false when pinning would pass the pin limit.
    bool togglePin(uint64_t id){for(auto& e:entries_)if(e.id==id){if(!e.pinned&&pinned()>=pinLimit)return false;e.pinned=!e.pinned;if(!e.pinned)trim();return true;}return false;}
    size_t pinned()const{size_t n=0;for(auto& e:entries_)n+=e.pinned;return n;}
    // Restores saved pins (oldest first), keeping any newer copies above them.
    void restore(std::vector<ClipEntry> saved,double now){for(auto& e:saved){e.pinned=true;e.id=++next_;e.time=now;if(pinned()<pinLimit&&std::none_of(entries_.begin(),entries_.end(),[&](auto& x){return x.sameContent(e);}))entries_.push_back(std::move(e));}}
    const ClipEntry* find(uint64_t id)const{for(auto& e:entries_)if(e.id==id)return &e;return nullptr;}
    void remove(uint64_t id){std::erase_if(entries_,[&](auto& e){return e.id==id;});}
    // Clear keeps pins; forget drops everything (turning history off).
    void clear(){std::erase_if(entries_,[](auto& e){return !e.pinned;});}
    void forget(){entries_.clear();}
};
// Copies whose text, file names or source contain every word of the query (any case).
inline bool clipMatches(const ClipEntry& e,const std::wstring& query){
    auto fold=[](std::wstring v){for(auto& c:v)c=wchar_t(std::towlower(c));return v;};
    std::wstring hay=fold(e.text+L"\n"+e.source);for(auto& f:e.files)hay+=L"\n"+fold(f);if(e.kind==ClipEntry::Kind::Image)hay+=L"\nimage picture screenshot";
    std::wstring q=fold(query);size_t at=0;
    while(at<q.size()){while(at<q.size()&&std::iswspace(q[at]))++at;size_t end=at;while(end<q.size()&&!std::iswspace(q[end]))++end;if(end>at&&hay.find(q.substr(at,end-at))==std::wstring::npos)return false;at=end;}
    return true;
}
// A single token that reads like a password, one-time code or key: shown masked until pointed at.
inline bool looksSecret(const std::wstring& raw){
    size_t a=raw.find_first_not_of(L" \t\r\n"),b=raw.find_last_not_of(L" \t\r\n");if(a==std::wstring::npos)return false;
    const std::wstring s=raw.substr(a,b-a+1);if(s.size()<4||s.size()>128||s.find_first_of(L" \t\r\n")!=std::wstring::npos||isLink(s))return false;
    int upper=0,lower=0,digit=0,symbol=0;for(wchar_t c:s){if(std::iswupper(c))++upper;else if(std::iswlower(c))++lower;else if(std::iswdigit(c))++digit;else ++symbol;}
    if(digit==int(s.size()))return s.size()>=4&&s.size()<=8;// one-time codes; longer digit runs are numbers
    for(auto prefix:{L"sk-",L"ghp_",L"gho_",L"github_pat_",L"xoxb-",L"xoxp-",L"AKIA",L"AIza",L"glpat-"})if(s.starts_with(prefix)&&s.size()>=16)return true;
    const int kinds=(upper>0)+(lower>0)+(digit>0)+(symbol>0);
    if(s.size()>=8&&kinds>=3&&digit>0)return true;// mixed-class passwords
    if(s.size()>=24&&kinds>=2&&upper+lower+digit>=int(s.size())*9/10&&digit>0)return true;// long random keys and tokens
    return false;
}
// Pinned copies (text, links and file lists; not images) as UTF-16 lines, for saving encrypted.
inline std::wstring savePins(const std::deque<ClipEntry>& entries){
    auto escape=[](const std::wstring& v){std::wstring o;for(wchar_t c:v){if(c==L'\\')o+=L"\\\\";else if(c==L'\n')o+=L"\\n";else if(c==L'\r')o+=L"\\r";else if(c==L'\t')o+=L"\\t";else o+=c;}return o;};
    std::wstring out=L"pins 1\n";
    for(auto it=entries.rbegin();it!=entries.rend();++it){auto& e=*it;if(!e.pinned||e.kind==ClipEntry::Kind::Image)continue;
        std::wstring body=e.text;if(e.kind==ClipEntry::Kind::Files){body.clear();for(auto& f:e.files){if(!body.empty())body+=L'\x1f';body+=f;}}
        out+=std::to_wstring(int(e.kind))+L"\t"+escape(e.source)+L"\t"+escape(body)+L"\n";}
    return out;
}
inline std::vector<ClipEntry> loadPins(const std::wstring& text){
    std::vector<ClipEntry> out;if(!text.starts_with(L"pins 1\n"))return out;
    auto unescape=[](const std::wstring& v){std::wstring o;for(size_t i=0;i<v.size();++i){if(v[i]!=L'\\'||i+1==v.size()){o+=v[i];continue;}wchar_t n=v[++i];o+=n==L'n'?L'\n':n==L'r'?L'\r':n==L't'?L'\t':n;}return o;};
    size_t at=7;while(at<text.size()&&out.size()<ClipboardHistory::pinLimit){size_t end=text.find(L'\n',at);if(end==std::wstring::npos)end=text.size();const std::wstring line=text.substr(at,end-at);at=end+1;
        size_t t1=line.find(L'\t'),t2=t1==std::wstring::npos?t1:line.find(L'\t',t1+1);if(t2==std::wstring::npos)continue;
        const int kind=_wtoi(line.substr(0,t1).c_str());if(line.substr(0,t1)!=std::to_wstring(kind)||kind<0||kind>3||kind==int(ClipEntry::Kind::Image))continue;
        ClipEntry e;e.kind=ClipEntry::Kind(kind);e.source=unescape(line.substr(t1+1,t2-t1-1));const std::wstring body=unescape(line.substr(t2+1));e.pinned=true;
        if(e.kind==ClipEntry::Kind::Files){size_t f=0;while(f<=body.size()){size_t g=body.find(L'\x1f',f);if(g==std::wstring::npos)g=body.size();if(g>f)e.files.push_back(body.substr(f,g-f));f=g+1;}if(e.files.empty())continue;}
        else{e.text=body;if(clipPreview(e.text).empty())continue;}
        out.push_back(std::move(e));}
    return out;
}
// "Just now", "4 min", "2 h", "3 d".
inline std::wstring ageText(double seconds){
    if(seconds<45)return L"Just now";if(seconds<3600)return std::to_wstring(int(std::max(1.,std::round(seconds/60))))+L" min";
    if(seconds<86400)return std::to_wstring(int(seconds/3600))+L" h";return std::to_wstring(int(seconds/86400))+L" d";
}
}
