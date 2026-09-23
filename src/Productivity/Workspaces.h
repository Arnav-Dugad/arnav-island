#pragma once
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
namespace nexus {
// A named set of apps to open together. Each app is an installed app's ID
// (launched through the Start menu entry) or, for classic programs without one,
// the executable path. Nothing is closed when a workspace opens.
struct WorkspaceApp {std::wstring name,target;bool appId=false;bool operator==(const WorkspaceApp&)const=default;};
struct Workspace {std::wstring name;std::vector<WorkspaceApp> apps;bool operator==(const Workspace&)const=default;};
class WorkspaceStore {
    std::vector<Workspace> list_;
    static std::string utf8(const std::wstring& w){std::string s;for(size_t i=0;i<w.size();++i){uint32_t c=w[i];if(c>=0xd800&&c<0xdc00&&i+1<w.size())c=0x10000+((c-0xd800)<<10)+(w[++i]-0xdc00);
        if(c=='\t'||c=='\n'||c=='\r')c=' ';if(c<0x80)s.push_back(char(c));else if(c<0x800){s.push_back(char(0xc0|(c>>6)));s.push_back(char(0x80|(c&63)));}else if(c<0x10000){s.push_back(char(0xe0|(c>>12)));s.push_back(char(0x80|((c>>6)&63)));s.push_back(char(0x80|(c&63)));}
        else{s.push_back(char(0xf0|(c>>18)));s.push_back(char(0x80|((c>>12)&63)));s.push_back(char(0x80|((c>>6)&63)));s.push_back(char(0x80|(c&63)));}}return s;}
    static std::wstring wide(const std::string& s){std::wstring w;for(size_t i=0;i<s.size();){unsigned char c=s[i];uint32_t v;int n;if(c<0x80){v=c;n=1;}else if((c>>5)==6){v=c&31;n=2;}else if((c>>4)==14){v=c&15;n=3;}else if((c>>3)==30){v=c&7;n=4;}else{++i;continue;}
        if(i+n>s.size())break;for(int k=1;k<n;++k)v=(v<<6)|(s[i+k]&63);i+=n;if(v>=0x10000){v-=0x10000;w.push_back(wchar_t(0xd800+(v>>10)));w.push_back(wchar_t(0xdc00+(v&1023)));}else w.push_back(wchar_t(v));}return w;}
public:
    static constexpr size_t maxWorkspaces=8,maxApps=12,maxName=32;
    const std::vector<Workspace>& list()const{return list_;}
    std::vector<std::wstring> names()const{std::vector<std::wstring> n;for(auto& w:list_)n.push_back(w.name);return n;}
    // Names compare without regard to case.
    int index(const std::wstring& name)const{auto fold=[](std::wstring v){for(auto& c:v)c=wchar_t(std::towlower(c));return v;};auto key=fold(name);for(size_t i=0;i<list_.size();++i)if(fold(list_[i].name)==key)return int(i);return -1;}
    const Workspace* find(const std::wstring& name)const{int i=index(name);return i<0?nullptr:&list_[size_t(i)];}
    // Saving under an existing name replaces it; a new name needs a free slot.
    bool save(Workspace w){if(w.name.empty())return false;if(w.name.size()>maxName)w.name.resize(maxName);if(w.apps.size()>maxApps)w.apps.resize(maxApps);
        if(int i=index(w.name);i>=0){list_[size_t(i)]=std::move(w);return true;}if(list_.size()>=maxWorkspaces)return false;list_.push_back(std::move(w));return true;}
    bool remove(const std::wstring& name){int i=index(name);if(i<0)return false;list_.erase(list_.begin()+i);return true;}
    void write(std::ostream& out)const{out<<"workspaces 1\n";for(auto& w:list_){out<<"workspace\t"<<utf8(w.name)<<'\n';for(auto& a:w.apps)out<<"app\t"<<(a.appId?1:0)<<'\t'<<utf8(a.name)<<'\t'<<utf8(a.target)<<'\n';}}
    // Malformed lines are skipped; the file never grows past the limits.
    static WorkspaceStore read(std::istream& in){WorkspaceStore s;std::string line;if(!std::getline(in,line)||line!="workspaces 1")return s;bool current=false;
        while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();std::vector<std::string> f;std::stringstream ss(line);std::string part;while(std::getline(ss,part,'\t'))f.push_back(part);
            if(f.size()==2&&f[0]=="workspace"){current=false;if(s.list_.size()>=maxWorkspaces)break;Workspace w;w.name=wide(f[1]).substr(0,maxName);if(!w.name.empty()&&!s.find(w.name)){s.list_.push_back(std::move(w));current=true;}}
            else if(current&&f.size()==4&&f[0]=="app"&&(f[1]=="0"||f[1]=="1")&&s.list_.back().apps.size()<maxApps){WorkspaceApp a{wide(f[2]),wide(f[3]),f[1]=="1"};if(!a.target.empty())s.list_.back().apps.push_back(std::move(a));}}
        return s;}
};
// Readable list of a workspace's apps: "Edge, Spotify and 2 more".
inline std::wstring appSummary(const std::vector<WorkspaceApp>& apps,size_t shown=3){
    if(apps.empty())return L"No apps";std::wstring s;size_t n=std::min(shown,apps.size());
    for(size_t i=0;i<n;++i){if(i)s+=(i+1==n&&apps.size()<=shown)?L" and ":L", ";s+=apps[i].name;}
    if(apps.size()>shown)s+=L" and "+std::to_wstring(apps.size()-shown)+L" more";return s;
}
}
