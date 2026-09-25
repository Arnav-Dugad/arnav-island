#pragma once
#include "Productivity/Commands.h"
#include "Media/Lyrics.h"
#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
namespace nexus {
// What the command bar remembers, on this PC only (commands.nexus, UTF-8): the commands
// and files you ran, how often and when, and the ones you pinned. It feeds the empty
// bar (pinned, then recent) and ranks apps and files you use often a little higher.
struct RememberedCommand {
    CommandKind kind=CommandKind::None;std::wstring phrase,title,detail,target;int value=0;int64_t last=0;int count=0;bool pinned=false;
    std::wstring key()const{return std::to_wstring(int(kind))+L"\x1f"+lowered(target.empty()?title:target);}
};
class CommandMemory {
    std::vector<RememberedCommand> items_;
    static std::wstring keyOf(const CommandResult& r){return std::to_wstring(int(r.kind))+L"\x1f"+lowered(r.target.empty()?r.title:r.target);}
    void trim(){while(items_.size()>limit){auto oldest=items_.end();for(auto it=items_.begin();it!=items_.end();++it)if(!it->pinned&&(oldest==items_.end()||it->last<oldest->last))oldest=it;if(oldest==items_.end())break;items_.erase(oldest);}}
public:
    static constexpr size_t limit=40,pinLimit=6;
    // Answers, pastes and one-off maintenance are not worth offering again.
    static bool memorable(CommandKind k){switch(k){case CommandKind::None:case CommandKind::ClipPaste:case CommandKind::Currency:case CommandKind::Colour:case CommandKind::Weather:case CommandKind::SaveWorkspace:case CommandKind::DeleteWorkspace:case CommandKind::ClearClipboard:return false;default:return true;}}
    const std::vector<RememberedCommand>& items()const{return items_;}
    static CommandMemory of(std::vector<RememberedCommand> items){CommandMemory m;m.items_=std::move(items);m.trim();return m;}
    void record(const CommandResult& r,const std::wstring& phrase,int64_t now){
        if(!memorable(r.kind))return;const auto key=keyOf(r);
        for(auto& i:items_)if(i.key()==key){++i.count;i.last=now;i.title=r.title;i.detail=r.detail;i.value=r.value;if(!phrase.empty())i.phrase=phrase;return;}
        items_.push_back({r.kind,phrase,r.title,r.detail,r.target,r.value,now,1,false});trim();
    }
    // 1 pinned, 0 unpinned, -1 not possible (not memorable, or six pins already).
    int togglePin(const CommandResult& r,const std::wstring& phrase,int64_t now){
        if(!memorable(r.kind))return -1;const auto key=keyOf(r);
        for(auto& i:items_)if(i.key()==key){if(!i.pinned&&pinned().size()>=pinLimit)return -1;i.pinned=!i.pinned;if(i.pinned)i.last=std::max(i.last,now);return i.pinned?1:0;}
        if(pinned().size()>=pinLimit)return -1;items_.push_back({r.kind,phrase,r.title,r.detail,r.target,r.value,now,0,true});trim();return 1;
    }
    bool isPinned(const CommandResult& r)const{const auto key=keyOf(r);for(auto& i:items_)if(i.key()==key)return i.pinned;return false;}
    std::vector<const RememberedCommand*> pinned()const{std::vector<const RememberedCommand*> v;for(auto& i:items_)if(i.pinned)v.push_back(&i);return v;}
    // Most recent first, pinned ones excluded (they have their own place).
    std::vector<const RememberedCommand*> recent(size_t n)const{
        std::vector<const RememberedCommand*> v;for(auto& i:items_)if(!i.pinned&&i.count>0)v.push_back(&i);
        std::stable_sort(v.begin(),v.end(),[](auto* a,auto* b){return a->last>b->last;});if(v.size()>n)v.resize(n);return v;
    }
    // Uses, fading by half every week.
    double frecency(CommandKind kind,const std::wstring& target,int64_t now)const{
        const auto key=std::to_wstring(int(kind))+L"\x1f"+lowered(target);for(auto& i:items_)if(i.key()==key)return i.count*std::pow(.5,std::max<double>(0,double(now-i.last))/(7*86400.));return 0;
    }
    void forget(){items_.clear();}
    void forget(const std::wstring& key){std::erase_if(items_,[&](auto& i){return i.key()==key;});}
    // One command per line, fields separated by tabs; tabs, newlines and backslashes escaped.
    void write(std::ostream& out)const{
        auto field=[&](const std::wstring& s){std::wstring e;for(wchar_t c:s){if(c==L'\\')e+=L"\\\\";else if(c==L'\t')e+=L"\\t";else if(c==L'\n')e+=L"\\n";else if(c==L'\r')e+=L"\\r";else e+=c;}out<<toUtf8(e);};
        out<<"commands 1\n";for(auto& i:items_){out<<int(i.kind)<<'\t'<<i.value<<'\t'<<i.last<<'\t'<<i.count<<'\t'<<int(i.pinned)<<'\t';field(i.phrase);out<<'\t';field(i.title);out<<'\t';field(i.detail);out<<'\t';field(i.target);out<<'\n';}
    }
    static CommandMemory read(std::istream& in){
        CommandMemory m;std::string line;if(!std::getline(in,line)||line!="commands 1")return m;
        while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();std::vector<std::string> f;size_t a=0;for(;;){size_t b=line.find('\t',a);f.push_back(line.substr(a,b==std::string::npos?std::string::npos:b-a));if(b==std::string::npos)break;a=b+1;}
            if(f.size()!=9)continue;auto text=[](const std::string& s){std::wstring w=fromUtf8(s),o;for(size_t i=0;i<w.size();++i){if(w[i]==L'\\'&&i+1<w.size()){wchar_t c=w[++i];o+=c==L't'?L'\t':c==L'n'?L'\n':c==L'r'?L'\r':c;}else o+=w[i];}return o;};
            try{RememberedCommand r;const int kind=std::stoi(f[0]);if(kind<=0||kind>int(CommandKind::Weather))continue;r.kind=CommandKind(kind);r.value=std::stoi(f[1]);r.last=std::stoll(f[2]);r.count=std::max(0,std::stoi(f[3]));r.pinned=f[4]=="1";
                r.phrase=text(f[5]);r.title=text(f[6]);r.detail=text(f[7]);r.target=text(f[8]);if(r.title.empty()||!memorable(r.kind))continue;
                if(std::none_of(m.items_.begin(),m.items_.end(),[&](auto& i){return i.key()==r.key();}))m.items_.push_back(std::move(r));}catch(...){}}
        m.trim();return m;
    }
};
}
