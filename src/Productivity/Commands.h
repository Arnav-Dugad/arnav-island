#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace nexus {
// A small, fixed command language. Nothing typed is ever executed as a shell
// command: every result is one of these kinds, shown before it runs.
enum class CommandKind { None,Volume,VolumeStep,Mute,Unmute,Play,Pause,Next,Previous,Timer,Stopwatch,StopTimer,OpenApp,SearchFiles,OpenSettings,Workspace,SaveWorkspace,DeleteWorkspace,Clipboard,ClearClipboard,Lock };
struct InstalledApp {std::wstring name,id;};
struct CommandResult {
    CommandKind kind=CommandKind::None;std::wstring title,detail,target;int value=0;
    bool confirm=false;// needs a second Enter (launching several apps)
    std::wstring appId;// installed app, for its icon
};
inline std::wstring lowered(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
inline std::wstring trimmed(const std::wstring& s){size_t a=s.find_first_not_of(L" \t"),b=s.find_last_not_of(L" \t");return a==std::wstring::npos?std::wstring{}:s.substr(a,b-a+1);}
inline std::vector<std::wstring> words(const std::wstring& s){std::vector<std::wstring> out;std::wstring w;for(wchar_t c:s){if(c==L' '||c==L'\t'){if(!w.empty())out.push_back(std::move(w));w.clear();}else w.push_back(c);}if(!w.empty())out.push_back(std::move(w));return out;}
inline std::optional<int> wholeNumber(std::wstring_view s){if(s.empty()||s.size()>6)return std::nullopt;int v=0;for(wchar_t c:s){if(c<L'0'||c>L'9')return std::nullopt;v=v*10+(c-L'0');}return v;}
// How well an installed app's name matches what was typed (0 = no match).
inline int appScore(const std::wstring& name,const std::wstring& typed){
    auto n=lowered(name),q=lowered(trimmed(typed));if(q.empty()||n.starts_with(L"uninstall"))return 0;
    if(n==q)return 100;if(n.starts_with(q))return 90-int(std::min<size_t>(20,n.size()-q.size()));
    auto ws=words(n);for(auto& w:ws)if(w.starts_with(q))return 70;
    // Initials: "vsc" finds Visual Studio Code.
    std::wstring initials;for(auto& w:ws)initials.push_back(w[0]);if(q.size()>=2&&initials.starts_with(q))return 60;
    if(q.size()>=3&&n.find(q)!=std::wstring::npos)return 50;
    return 0;
}
inline std::vector<InstalledApp> matchApps(const std::vector<InstalledApp>& apps,const std::wstring& typed,size_t count=3){
    std::vector<std::pair<int,const InstalledApp*>> scored;for(auto& a:apps){int s=appScore(a.name,typed);if(s>=50)scored.push_back({s,&a});}
    std::stable_sort(scored.begin(),scored.end(),[](auto& a,auto& b){return a.first>b.first||(a.first==b.first&&a.second->name.size()<b.second->name.size());});
    std::vector<InstalledApp> out;for(auto& [s,a]:scored){if(std::any_of(out.begin(),out.end(),[&](auto& o){return lowered(o.name)==lowered(a->name);}))continue;out.push_back(*a);if(out.size()==count)break;}return out;
}
// Windows Settings pages that commands may open.
struct SettingsPage {std::wstring_view word,uri,name;};
inline constexpr SettingsPage settingsPages[]={
    {L"bluetooth",L"ms-settings:bluetooth",L"Bluetooth & devices"},{L"display",L"ms-settings:display",L"Display"},{L"sound",L"ms-settings:sound",L"Sound"},
    {L"wifi",L"ms-settings:network-wifi",L"Wi-Fi"},{L"wi-fi",L"ms-settings:network-wifi",L"Wi-Fi"},{L"network",L"ms-settings:network-status",L"Network & internet"},
    {L"battery",L"ms-settings:batterysaver",L"Battery"},{L"power",L"ms-settings:powersleep",L"Power"},{L"notifications",L"ms-settings:notifications",L"Notifications"},
    {L"personalization",L"ms-settings:personalization",L"Personalization"},{L"wallpaper",L"ms-settings:personalization-background",L"Background"},{L"colors",L"ms-settings:colors",L"Colors"},
    {L"apps",L"ms-settings:appsfeatures",L"Installed apps"},{L"startup",L"ms-settings:startupapps",L"Startup apps"},{L"default apps",L"ms-settings:defaultapps",L"Default apps"},
    {L"update",L"ms-settings:windowsupdate",L"Windows Update"},{L"updates",L"ms-settings:windowsupdate",L"Windows Update"},{L"privacy",L"ms-settings:privacy",L"Privacy & security"},
    {L"camera",L"ms-settings:privacy-webcam",L"Camera privacy"},{L"microphone",L"ms-settings:privacy-microphone",L"Microphone privacy"},{L"location",L"ms-settings:privacy-location",L"Location privacy"},
    {L"night light",L"ms-settings:nightlight",L"Night light"},{L"mouse",L"ms-settings:mousetouchpad",L"Mouse"},{L"touchpad",L"ms-settings:devices-touchpad",L"Touchpad"},
    {L"keyboard",L"ms-settings:typing",L"Typing"},{L"storage",L"ms-settings:storagesense",L"Storage"},{L"clipboard",L"ms-settings:clipboard",L"Clipboard"},{L"about",L"ms-settings:about",L"About"},
    {L"vpn",L"ms-settings:network-vpn",L"VPN"},{L"hotspot",L"ms-settings:network-mobilehotspot",L"Mobile hotspot"},{L"printers",L"ms-settings:printers",L"Printers & scanners"},
    {L"time",L"ms-settings:dateandtime",L"Date & time"},{L"date",L"ms-settings:dateandtime",L"Date & time"},{L"language",L"ms-settings:regionlanguage",L"Language & region"},
    {L"gaming",L"ms-settings:gaming-gamebar",L"Gaming"},{L"focus",L"ms-settings:quiethours",L"Focus"},{L"accessibility",L"ms-settings:easeofaccess",L"Accessibility"}};
// File search filters, translated to Windows Advanced Query Syntax.
struct SearchFilter {std::wstring_view phrase,aqs,label;};
inline constexpr SearchFilter searchDates[]={{L"today",L"datemodified:today",L"today"},{L"yesterday",L"datemodified:yesterday",L"yesterday"},
    {L"this week",L"datemodified:this week",L"this week"},{L"last week",L"datemodified:last week",L"last week"},{L"this month",L"datemodified:this month",L"this month"},
    {L"last month",L"datemodified:last month",L"last month"},{L"this year",L"datemodified:this year",L"this year"},{L"last year",L"datemodified:last year",L"last year"}};
inline constexpr SearchFilter searchKinds[]={{L"pdfs",L"ext:.pdf",L"PDFs"},{L"pdf",L"ext:.pdf",L"PDFs"},{L"images",L"kind:picture",L"pictures"},{L"photos",L"kind:picture",L"pictures"},{L"pictures",L"kind:picture",L"pictures"},
    {L"screenshots",L"kind:picture",L"pictures"},{L"videos",L"kind:video",L"videos"},{L"music",L"kind:music",L"music"},{L"songs",L"kind:music",L"music"},{L"documents",L"kind:document",L"documents"},
    {L"docs",L"kind:document",L"documents"},{L"spreadsheets",L"ext:.xlsx",L"spreadsheets"},{L"presentations",L"ext:.pptx",L"presentations"},{L"slides",L"ext:.pptx",L"presentations"},{L"folders",L"kind:folder",L"folders"}};
struct FileQuery {std::wstring words,aqs,description;};
// "report pdfs from last week" -> words "report", AQS "report ext:.pdf datemodified:last week".
inline FileQuery fileQuery(const std::wstring& typed){
    std::wstring rest=L" "+lowered(trimmed(typed))+L" ";std::vector<std::wstring> filters,labels;std::wstring date;
    for(auto& d:searchDates){auto key=L" "+std::wstring(d.phrase)+L" ";for(auto at=rest.find(key);at!=std::wstring::npos;at=rest.find(key)){rest.replace(at,key.size(),L" ");if(date.empty()){filters.emplace_back(d.aqs);date=d.label;}}}
    for(auto& k:searchKinds){auto key=L" "+std::wstring(k.phrase)+L" ";auto at=rest.find(key);if(at==std::wstring::npos)continue;rest.replace(at,key.size(),L" ");if(std::find(filters.begin(),filters.end(),std::wstring(k.aqs))==filters.end()){filters.emplace_back(k.aqs);labels.emplace_back(k.label);}}
    // Joining words that no longer carry meaning once filters are removed.
    auto ws=words(rest);std::wstring text;for(auto& w:ws){if(w==L"from"||w==L"modified"||w==L"in"||w==L"named"||w==L"called")continue;if(!text.empty())text+=L' ';text+=w;}
    FileQuery q;q.words=text;q.aqs=text;for(auto& f:filters){if(!q.aqs.empty())q.aqs+=L' ';q.aqs+=f;}
    q.description=labels.empty()?L"files":labels.front();if(labels.size()>1)for(size_t i=1;i<labels.size();++i)q.description+=L", "+labels[i];
    if(!text.empty())q.description+=L" matching “"+text+L"”";if(!date.empty())q.description+=L", changed "+date;
    return q;
}
inline std::wstring percentEncode(const std::wstring& s){
    std::string utf8;for(size_t i=0;i<s.size();++i){uint32_t c=s[i];if(c>=0xd800&&c<0xdc00&&i+1<s.size()){c=0x10000+((c-0xd800)<<10)+(s[++i]-0xdc00);}
        if(c<0x80)utf8.push_back(char(c));else if(c<0x800){utf8.push_back(char(0xc0|(c>>6)));utf8.push_back(char(0x80|(c&63)));}else if(c<0x10000){utf8.push_back(char(0xe0|(c>>12)));utf8.push_back(char(0x80|((c>>6)&63)));utf8.push_back(char(0x80|(c&63)));}
        else{utf8.push_back(char(0xf0|(c>>18)));utf8.push_back(char(0x80|((c>>12)&63)));utf8.push_back(char(0x80|((c>>6)&63)));utf8.push_back(char(0x80|(c&63)));}}
    std::wstring out;const char* hex="0123456789ABCDEF";for(unsigned char c:utf8){if(std::isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out.push_back(wchar_t(c));else{out.push_back(L'%');out.push_back(wchar_t(hex[c>>4]));out.push_back(wchar_t(hex[c&15]));}}return out;
}
// File Explorer search, limited to one folder tree.
inline std::wstring searchUri(const std::wstring& aqs,const std::wstring& scope){return L"search-ms:query="+percentEncode(aqs)+L"&crumb=location:"+percentEncode(scope);}
inline std::wstring durationWords(int seconds){if(seconds%3600==0)return std::to_wstring(seconds/3600)+(seconds==3600?L" hour":L" hours");if(seconds>=60&&seconds%60==0)return std::to_wstring(seconds/60)+(seconds==60?L" minute":L" minutes");return std::to_wstring(seconds)+(seconds==1?L" second":L" seconds");}
// "10", "10 min", "90s", "1 hour" -> seconds (1 s to 12 h). Bare numbers are minutes.
inline std::optional<int> durationSeconds(const std::vector<std::wstring>& ws,size_t from){
    if(from>=ws.size())return std::nullopt;std::wstring number=ws[from],unit;size_t used=1;
    size_t digits=0;while(digits<number.size()&&std::iswdigit(number[digits]))++digits;if(!digits)return std::nullopt;
    if(digits<number.size()){unit=number.substr(digits);number.resize(digits);}else if(from+1<ws.size()){unit=ws[from+1];used=2;}
    if(from+used!=ws.size())return std::nullopt;
    auto n=wholeNumber(number);if(!n||*n<=0)return std::nullopt;int scale=60;
    if(unit.empty()||unit==L"m"||unit==L"min"||unit==L"mins"||unit==L"minute"||unit==L"minutes")scale=60;else if(unit==L"s"||unit==L"sec"||unit==L"secs"||unit==L"second"||unit==L"seconds")scale=1;else if(unit==L"h"||unit==L"hr"||unit==L"hour"||unit==L"hours")scale=3600;else return std::nullopt;
    long long total=(long long)*n*scale;if(total<1||total>12*3600)return std::nullopt;return int(total);
}
// Parses what was typed into the intent to show (first) and alternatives.
// `workspaces` are saved workspace names; `scope` is the folder file searches cover.
inline std::vector<CommandResult> parseCommand(const std::wstring& typed,const std::vector<InstalledApp>& apps,const std::vector<std::wstring>& workspaces,const std::wstring& scope){
    std::vector<CommandResult> out;const std::wstring text=trimmed(typed),t=lowered(text);if(t.empty())return out;const auto ws=words(t);const auto& first=ws[0];
    auto add=[&](CommandKind k,std::wstring title,std::wstring detail,int value=0,std::wstring target={}){CommandResult r;r.kind=k;r.title=std::move(title);r.detail=std::move(detail);r.value=value;r.target=std::move(target);out.push_back(std::move(r));};
    auto after=[&](size_t n){size_t at=0;for(size_t i=0;i<n&&at!=std::wstring::npos;++i){at=text.find_first_not_of(L" \t",at);at=text.find_first_of(L" \t",at);}return at==std::wstring::npos?std::wstring{}:trimmed(text.substr(at));};
    auto workspaceNamed=[&](const std::wstring& name)->std::optional<std::wstring>{for(auto& w:workspaces)if(lowered(w)==lowered(name))return w;for(auto& w:workspaces)if(!name.empty()&&lowered(w).starts_with(lowered(name)))return w;return std::nullopt;};
    // Volume.
    if(first==L"volume"||first==L"vol"||(first==L"set"&&ws.size()>1&&(ws[1]==L"volume"||ws[1]==L"vol"))){
        size_t i=first==L"set"?2:1;if(i<ws.size()&&ws[i]==L"to")++i;
        if(i<ws.size()){std::wstring v=ws[i];if(v.ends_with(L"%"))v.pop_back();if(auto n=wholeNumber(v);n&&*n<=100&&i+1==ws.size()){add(CommandKind::Volume,L"Set volume to "+std::to_wstring(*n)+L"%",L"System output",*n);return out;}
            if(v==L"up"&&i+1==ws.size()){add(CommandKind::VolumeStep,L"Turn the volume up",L"By 10 points",10);return out;}if(v==L"down"&&i+1==ws.size()){add(CommandKind::VolumeStep,L"Turn the volume down",L"By 10 points",-10);return out;}}
        if(ws.size()==1){add(CommandKind::None,L"Volume",L"Add a level, such as “volume 30”, or up/down");return out;}
    }
    if(t==L"louder"){add(CommandKind::VolumeStep,L"Turn the volume up",L"By 10 points",10);return out;}
    if(t==L"quieter"||t==L"softer"){add(CommandKind::VolumeStep,L"Turn the volume down",L"By 10 points",-10);return out;}
    if(t==L"mute"){add(CommandKind::Mute,L"Mute",L"System output");return out;}if(t==L"unmute"){add(CommandKind::Unmute,L"Unmute",L"System output");return out;}
    // Playback.
    if(t==L"play"||t==L"resume"){add(CommandKind::Play,L"Play",L"Current media session");return out;}if(t==L"pause"||t==L"stop music"){add(CommandKind::Pause,L"Pause",L"Current media session");return out;}
    if(t==L"next"||t==L"skip"||t==L"next track"){add(CommandKind::Next,L"Next track",L"Current media session");return out;}
    if(t==L"previous"||t==L"prev"||t==L"back"||t==L"previous track"){add(CommandKind::Previous,L"Previous track",L"Current media session");return out;}
    // Timers.
    if(first==L"timer"||first==L"focus"||first==L"break"){
        if(ws.size()==1){int m=first==L"break"?5:first==L"focus"?25:10;add(CommandKind::Timer,first==L"break"?L"Take a 5-minute break":first==L"focus"?L"Focus for 25 minutes":L"Start a 10-minute timer",L"Timer on the island",m*60);out.back().target=first;return out;}
        size_t i=1;if(ws[i]==L"for")++i;if(auto s=durationSeconds(ws,i)){std::wstring what=first==L"break"?L"Break for ":first==L"focus"?L"Focus for ":L"Start a timer for ";add(CommandKind::Timer,what+durationWords(*s),L"Timer on the island",*s);out.back().target=first;return out;}
        if(first==L"timer"&&(ws[1]==L"stop"||ws[1]==L"cancel"||ws[1]==L"reset")){add(CommandKind::StopTimer,L"Stop the timer",L"Resets the focus timer");return out;}
    }
    if(t==L"stopwatch"){add(CommandKind::Stopwatch,L"Start the stopwatch",L"Counts up on the island");return out;}
    if(t==L"stop timer"||t==L"cancel timer"||t==L"reset timer"){add(CommandKind::StopTimer,L"Stop the timer",L"Resets the focus timer");return out;}
    // Clipboard and session.
    if(t==L"clipboard"||t==L"clipboard history"){add(CommandKind::Clipboard,L"Show clipboard history",L"On the Shelf");return out;}
    if(t==L"clear clipboard"||t==L"clear clipboard history"){add(CommandKind::ClearClipboard,L"Clear clipboard history",L"Removes every kept copy");return out;}
    if(t==L"lock"||t==L"lock pc"||t==L"lock screen"){add(CommandKind::Lock,L"Lock this PC",L"Windows lock screen");return out;}
    // Workspaces.
    if((first==L"save"||first==L"remember")&&ws.size()>=2&&(ws[1]==L"workspace"||ws[1]==L"ws")){auto name=after(2);if(name.empty()){add(CommandKind::None,L"Name the workspace",L"For example “save workspace study”");return out;}
        if(name.size()>32)name.resize(32);add(CommandKind::SaveWorkspace,L"Save open apps as “"+name+L"”",workspaceNamed(name)&&lowered(*workspaceNamed(name))==lowered(name)?L"Replaces the saved workspace":L"New workspace",0,name);return out;}
    if((first==L"delete"||first==L"remove"||first==L"forget")&&ws.size()>=2&&(ws[1]==L"workspace"||ws[1]==L"ws")){auto name=after(2);if(auto w=workspaceNamed(name)){add(CommandKind::DeleteWorkspace,L"Delete workspace “"+*w+L"”",L"Apps are not closed",0,*w);return out;}
        add(CommandKind::None,L"No workspace by that name",workspaces.empty()?L"Save one with “save workspace <name>”":L"Saved: "+[&]{std::wstring l;for(auto& w:workspaces){if(!l.empty())l+=L", ";l+=w;}return l;}());return out;}
    if(first==L"workspace"||first==L"ws"||first==L"workspaces"||(first==L"open"&&ws.size()>=2&&ws[1]==L"workspace")){auto name=first==L"open"?after(2):after(1);
        if(name.empty()){if(workspaces.empty())add(CommandKind::None,L"No saved workspaces",L"Save the apps you have open with “save workspace <name>”");for(auto& w:workspaces){add(CommandKind::Workspace,L"Open workspace “"+w+L"”",L"Opens its apps that are not running",0,w);out.back().confirm=true;}return out;}
        if(auto w=workspaceNamed(name)){add(CommandKind::Workspace,L"Open workspace “"+*w+L"”",L"Opens its apps that are not running",0,*w);out.back().confirm=true;return out;}
        add(CommandKind::None,L"No workspace by that name",L"Save one with “save workspace "+name+L"”");return out;}
    for(auto& w:workspaces)if(lowered(w)==t){add(CommandKind::Workspace,L"Open workspace “"+w+L"”",L"Opens its apps that are not running",0,w);out.back().confirm=true;}
    // Windows Settings pages.
    {std::wstring page=t;if(page.starts_with(L"settings "))page=trimmed(page.substr(9));else if(page.ends_with(L" settings"))page=trimmed(page.substr(0,page.size()-9));else if(page.starts_with(L"open ")&&page.ends_with(L" settings"))page=trimmed(page.substr(5,page.size()-14));
        if(page!=t||t==L"settings"){if(t==L"settings"){add(CommandKind::OpenSettings,L"Open Windows Settings",L"System settings",0,L"ms-settings:");return out;}
            for(auto& p:settingsPages)if(page==p.word){add(CommandKind::OpenSettings,L"Open "+std::wstring(p.name)+L" settings",L"Windows Settings",0,std::wstring(p.uri));return out;}}}
    // File search.
    if(first==L"find"||first==L"search"||first==L"files"||first==L"file"){auto rest=after(1);if(rest.empty()){add(CommandKind::None,L"Search your files",L"For example “find budget pdfs from last month”");return out;}
        auto q=fileQuery(rest);if(q.aqs.empty()){add(CommandKind::None,L"Search your files",L"Add a word to look for");return out;}
        add(CommandKind::SearchFiles,L"Find "+q.description,L"File Explorer search in your user folder",0,searchUri(q.aqs,scope));return out;}
    // Apps.
    std::wstring appText=text;bool explicitOpen=false;for(auto verb:{L"open ",L"launch ",L"start ",L"run "})if(t.starts_with(verb)){appText=trimmed(text.substr(wcslen(verb)));explicitOpen=true;break;}
    for(auto& a:matchApps(apps,appText)){CommandResult r;r.kind=CommandKind::OpenApp;r.title=L"Open "+a.name;r.detail=L"App";r.target=a.id;r.appId=a.id;out.push_back(std::move(r));}
    if(out.empty())add(CommandKind::None,explicitOpen?L"No installed app by that name":L"Nothing to do yet",explicitOpen?L"Only apps in the Start menu can be opened":L"Try “open spotify”, “volume 30”, “focus 25” or “find notes pdf”");
    return out;
}
}
