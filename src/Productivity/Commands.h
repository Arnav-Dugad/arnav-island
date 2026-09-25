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
#include "Productivity/Currency.h"
namespace nexus {
// A small, fixed command language. Nothing typed is ever executed as a shell
// command: every result is one of these kinds, shown before it runs.
enum class CommandKind { None,Volume,VolumeStep,Mute,Unmute,Play,Pause,Next,Previous,Timer,Stopwatch,StopTimer,OpenApp,SearchFiles,OpenSettings,Workspace,SaveWorkspace,DeleteWorkspace,Clipboard,ClearClipboard,Lock,MicMute,MicUnmute,MicToggle,Snip,CopyText,PickColour,ClipPaste,
    // Phase 5D: system actions (value 1 = on / dark, 0 = off / light, -1 = toggle), files and answers.
    DarkMode,Bluetooth,WiFi,Airplane,EmptyBin,Sleep,Restart,ShutDown,OpenFile,Currency,
    // Phase 5E: a colour code, previewed as a swatch; value holds 0xRRGGBB.
    Colour,
    // Phase 5F: "weather <town>" chooses the weather's place (and turns weather on); target is the town.
    Weather,
    // Phase 5G: a song from the Music folder (target its path), shuffling that music, continuing the music on a paired PC (target its id).
    PlaySong,ShuffleMusic,ContinueOn };
struct InstalledApp {std::wstring name,id;};
struct CommandResult {
    CommandKind kind=CommandKind::None;std::wstring title,detail,target;int value=0;
    bool confirm=false;// needs a second Enter (launching several apps, sleep, restart, emptying the bin)
    std::wstring appId;// installed app, for its icon
    // Matched letters in the title as (start, length) runs, drawn highlighted.
    std::vector<std::pair<uint16_t,uint16_t>> marks;
    // What Tab would turn the typed text into (empty: no ghost completion).
    std::wstring completion;
    // Empty-bar rows: 1 pinned, 2 suggested, 3 recent. `phrase` is the text that produced a remembered command.
    int section=0;std::wstring phrase;
    // A number answer (currency), shown large with rolling digits.
    std::wstring answer;
};
using MatchMarks=std::vector<std::pair<uint16_t,uint16_t>>;
inline std::wstring lowered(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
inline std::wstring trimmed(const std::wstring& s){size_t a=s.find_first_not_of(L" \t"),b=s.find_last_not_of(L" \t");return a==std::wstring::npos?std::wstring{}:s.substr(a,b-a+1);}
inline std::vector<std::wstring> words(const std::wstring& s){std::vector<std::wstring> out;std::wstring w;for(wchar_t c:s){if(c==L' '||c==L'\t'){if(!w.empty())out.push_back(std::move(w));w.clear();}else w.push_back(c);}if(!w.empty())out.push_back(std::move(w));return out;}
inline std::optional<int> wholeNumber(std::wstring_view s){if(s.empty()||s.size()>6)return std::nullopt;int v=0;for(wchar_t c:s){if(c<L'0'||c>L'9')return std::nullopt;v=v*10+(c-L'0');}return v;}
inline bool wordStartAt(const std::wstring& s,size_t i){return i==0||!std::iswalnum(s[i-1]);}
// Which letters of `name` explain the match with `typed`: the whole query at a word start,
// then every typed word, then initials ("vsc" in Visual Studio Code), then a plain substring.
inline MatchMarks matchMarks(const std::wstring& name,const std::wstring& typed){
    MatchMarks out;const auto n=lowered(name),q=lowered(trimmed(typed));if(q.empty()||n.empty()||n.size()>0xffff)return out;
    auto atWordStart=[&](const std::wstring& w)->size_t{for(size_t at=n.find(w);at!=std::wstring::npos;at=n.find(w,at+1))if(wordStartAt(n,at))return at;return std::wstring::npos;};
    if(size_t at=atWordStart(q);at!=std::wstring::npos){out.push_back({uint16_t(at),uint16_t(q.size())});return out;}
    const auto ws=words(q);
    if(ws.size()>1){MatchMarks parts;for(auto& w:ws){size_t at=atWordStart(w);if(at==std::wstring::npos&&w.size()>=2)at=n.find(w);if(at==std::wstring::npos){parts.clear();break;}parts.push_back({uint16_t(at),uint16_t(w.size())});}
        if(!parts.empty()){std::sort(parts.begin(),parts.end());for(auto& p:parts){if(!out.empty()&&p.first<=out.back().first+out.back().second){out.back().second=uint16_t(std::max<size_t>(out.back().first+out.back().second,p.first+p.second)-out.back().first);}else out.push_back(p);}return out;}}
    if(ws.size()==1&&q.size()>=2){MatchMarks initials;size_t k=0;for(size_t i=0;i<n.size()&&k<q.size();++i)if(wordStartAt(n,i)&&std::iswalnum(n[i])&&n[i]==q[k]){initials.push_back({uint16_t(i),1});++k;}if(k==q.size())return initials;}
    if(size_t at=n.find(q);at!=std::wstring::npos)out.push_back({uint16_t(at),uint16_t(q.size())});
    return out;
}
inline MatchMarks shiftedMarks(MatchMarks m,size_t by){for(auto& r:m)r.first=uint16_t(r.first+by);return m;}
// Phrases the ghost completion offers once two letters are typed.
inline constexpr std::wstring_view commandPhrases[]={L"volume ",L"mute",L"unmute",L"play",L"pause",L"next track",L"previous track",L"focus 25",L"timer 10 min",L"stopwatch",L"stop timer",
    L"snip",L"copy text",L"pick colour",L"clipboard",L"clear clipboard",L"lock",L"dark mode",L"light mode",L"bluetooth",L"wifi",L"airplane mode",L"empty recycle bin",L"sleep",L"restart",
    L"shut down",L"find ",L"workspace ",L"save workspace ",L"mute mic",L"unmute mic",L"settings",L"convert "};
// Tab completion: the typed text keeps its own letters and gains the rest of the phrase.
inline std::wstring phraseCompletion(const std::wstring& typed){
    const auto t=lowered(typed);if(t.size()<2||t.find_first_not_of(L" ")==std::wstring::npos)return {};
    for(auto p:commandPhrases)if(p.size()>t.size()&&std::wstring(p).starts_with(t))return typed+std::wstring(p.substr(t.size()));return {};
}
// The ghost after the caret: only a completion that still starts with what is typed now.
inline std::wstring ghostSuffix(const std::wstring& typed,const std::wstring& completion){
    if(typed.empty()||completion.size()<=typed.size())return {};return lowered(completion).starts_with(lowered(typed))?completion.substr(typed.size()):std::wstring{};
}
inline std::wstring byteText(unsigned long long bytes){
    const wchar_t* units[]={L"bytes",L"KB",L"MB",L"GB",L"TB"};double v=double(bytes);int u=0;while(v>=1000&&u<4){v/=1024;++u;}
    wchar_t b[32];if(u==0)swprintf(b,32,L"%llu %ls",bytes,bytes==1?L"byte":L"bytes");else swprintf(b,32,v<10?L"%.1f %ls":L"%.0f %ls",v,units[u]);return b;
}
// Optimal string alignment distance (Damerau-Levenshtein with one transposition), for typo suggestions.
inline size_t editDistance(const std::wstring& a,const std::wstring& b){
    const size_t n=a.size(),m=b.size();std::vector<std::vector<size_t>> d(n+1,std::vector<size_t>(m+1));
    for(size_t i=0;i<=n;++i)d[i][0]=i;for(size_t j=0;j<=m;++j)d[0][j]=j;
    for(size_t i=1;i<=n;++i)for(size_t j=1;j<=m;++j){const size_t cost=a[i-1]==b[j-1]?0:1;d[i][j]=std::min({d[i-1][j]+1,d[i][j-1]+1,d[i-1][j-1]+cost});
        if(i>1&&j>1&&a[i-1]==b[j-2]&&a[i-2]==b[j-1])d[i][j]=std::min(d[i][j],d[i-2][j-2]+1);}
    return d[n][m];
}
// How far a typo may be: nothing under four letters, one edit up to six, then two.
inline size_t typoAllowance(size_t length){return length<4?0:length<=6?1:2;}
// "#3A7BD5", "#39f", "3a7bd5" after "colour", "rgb(58, 123, 213)" -> 0xRRGGBB.
inline std::optional<uint32_t> parseColourCode(const std::wstring& typed){
    auto t=lowered(trimmed(typed));for(auto prefix:{L"colour ",L"color "})if(t.starts_with(prefix)){t=trimmed(t.substr(wcslen(prefix)));if(!t.empty()&&t[0]!=L'#'&&!t.starts_with(L"rgb"))t=L"#"+t;}
    auto hex=[](wchar_t c)->int{return c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:-1;};
    if(t.size()>1&&t[0]==L'#'){const auto h=t.substr(1);for(wchar_t c:h)if(hex(c)<0)return std::nullopt;
        if(h.size()==3){uint32_t v=0;for(wchar_t c:h)v=(v<<8)|uint32_t(hex(c)*17);return v;}
        if(h.size()==6||h.size()==8){uint32_t v=0;for(size_t i=0;i<6;++i)v=(v<<4)|uint32_t(hex(h[i]));return v;}return std::nullopt;}
    if(t.starts_with(L"rgb(")&&t.ends_with(L")")){std::wstring inner=t.substr(4,t.size()-5);for(auto& c:inner)if(c==L',')c=L' ';const auto parts=words(inner);if(parts.size()!=3)return std::nullopt;
        uint32_t v=0;for(auto& p:parts){auto n=wholeNumber(p);if(!n||*n>255)return std::nullopt;v=(v<<8)|uint32_t(*n);}return v;}
    return std::nullopt;
}
inline std::wstring colourHex(uint32_t v){wchar_t b[8];swprintf(b,8,L"#%06X",v&0xffffff);return b;}
inline std::wstring colourDetail(uint32_t v){
    const int r=int(v>>16)&255,g=int(v>>8)&255,b=int(v)&255;const double R=r/255.,G=g/255.,B=b/255.,hi=std::max({R,G,B}),lo=std::min({R,G,B}),l=(hi+lo)/2,d=hi-lo;
    double h=0,s=0;if(d>1e-9){s=d/(1-std::abs(2*l-1));h=hi==R?std::fmod((G-B)/d,6.):hi==G?(B-R)/d+2:(R-G)/d+4;h*=60;if(h<0)h+=360;}
    wchar_t buf[96];swprintf(buf,96,L"rgb(%d, %d, %d)  \u00b7  hsl(%d, %d%%, %d%%)",r,g,b,int(std::lround(h))%360,int(std::lround(s*100)),int(std::lround(l*100)));return buf;
}
// Result groups, for the small headers between them: 0 none, 1 apps, 2 files, 3 answer, 4 actions,
// and the empty bar's 11 pinned, 12 suggested, 13 recent.
inline int resultGroup(const CommandResult& r){
    if(r.section)return 10+r.section;switch(r.kind){case CommandKind::None:return 0;case CommandKind::OpenApp:return 1;case CommandKind::OpenFile:return 2;case CommandKind::Currency:case CommandKind::Colour:return 3;default:return 4;}
}
inline const wchar_t* groupName(int g){switch(g){case 1:return L"Apps";case 2:return L"Files";case 3:return L"Answer";case 4:return L"Actions";case 11:return L"Pinned";case 12:return L"Suggested";case 13:return L"Recent";default:return L"";}}
// Row positions in the bar: 40 DIPs a row from y 52, and an 18 DIP header before each
// group when the rows shown span more than one group.
struct CommandRows {std::vector<float> rows;std::vector<std::pair<float,int>> headers;float footer=0;};
inline CommandRows commandRows(const std::vector<CommandResult>& results,size_t maxRows,bool grouped=true){
    CommandRows out;const size_t n=std::min(maxRows,results.size());std::vector<int> seen;
    for(size_t i=0;i<n;++i){const int g=resultGroup(results[i]);if(g&&std::find(seen.begin(),seen.end(),g)==seen.end())seen.push_back(g);}
    const bool headers=grouped&&seen.size()>1;float y=52;int previous=-1;
    for(size_t i=0;i<n;++i){const int g=resultGroup(results[i]);if(headers&&g&&g!=previous){out.headers.push_back({y,g});y+=18;}out.rows.push_back(y);y+=40;previous=g;}
    out.footer=y+2;return out;
}
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
// Live state the command language answers from (-1 = unknown), and the currency setup.
struct CommandEnv {
    int dark=-1,bluetooth=-1,wifi=-1;long long binItems=-1,binBytes=-1;
    bool currency=false,ratesLoading=false;const ExchangeRates* rates=nullptr;std::wstring home=L"USD";
};
// Parses what was typed into the intent to show (first) and alternatives.
// `workspaces` are saved workspace names; `scope` is the folder file searches cover.
inline std::vector<CommandResult> parseCommand(const std::wstring& typed,const std::vector<InstalledApp>& apps,const std::vector<std::wstring>& workspaces,const std::wstring& scope,const CommandEnv& env={}){
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
    // Capture: snip, text from the screen, colour picker.
    if(t==L"snip"||t==L"screenshot"||t==L"screen shot"||t==L"capture"||t==L"snip screen"||t==L"take a screenshot"){add(CommandKind::Snip,L"Snip part of the screen",L"Drag a region or click a window  \u00b7  Alt+Shift+S");return out;}
    if(t==L"copy text"||t==L"text from screen"||t==L"read text"||t==L"ocr"||t==L"grab text"||t==L"extract text"){add(CommandKind::CopyText,L"Copy text from the screen",L"Drag over text  \u00b7  Alt+Shift+T");return out;}
    if(t==L"colour"||t==L"color"||t==L"pick colour"||t==L"pick color"||t==L"colour picker"||t==L"color picker"||t==L"eyedropper"){add(CommandKind::PickColour,L"Pick a colour from the screen",L"Copies its HEX  \u00b7  Alt+Shift+C");return out;}
    // Microphone (the default recording device).
    if(t==L"mute mic"||t==L"mute microphone"||t==L"mute my mic"||t==L"mic off"||t==L"microphone off"){add(CommandKind::MicMute,L"Mute the microphone",L"Default recording device");return out;}
    if(t==L"unmute mic"||t==L"unmute microphone"||t==L"unmute my mic"||t==L"mic on"||t==L"microphone on"){add(CommandKind::MicUnmute,L"Unmute the microphone",L"Default recording device");return out;}
    if(t==L"mic"||t==L"microphone"||t==L"toggle mic"||t==L"toggle microphone"){add(CommandKind::MicToggle,L"Turn the microphone on or off",L"Default recording device");return out;}
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
    if(t==L"lock"||t==L"lock pc"||t==L"lock screen"){add(CommandKind::Lock,L"Lock this PC",L"Windows lock screen  \u00b7  asks first");out.back().confirm=true;return out;}
    // System actions. Each row reads the current state, so it says what will actually happen.
    {auto onOff=[&](const std::wstring& base,std::initializer_list<const wchar_t*> names)->int{for(auto n:names){const std::wstring w=n;if(base==w)return -1;if(base==w+L" on"||base==L"turn on "+w||base==L"turn "+w+L" on"||base==L"enable "+w)return 1;if(base==w+L" off"||base==L"turn off "+w||base==L"turn "+w+L" off"||base==L"disable "+w)return 0;}return -2;};
        auto radio=[&](CommandKind kind,const std::wstring& label,int state,int want,const wchar_t* settingsUri){
            if(state==-2){add(CommandKind::None,L"No "+label+L" radio found",L"This PC does not report one");return;}
            const int goal=want==-1?(state==1?0:1):want;
            if(state>=0&&want>=0&&state==want){add(CommandKind::None,label+(want?L" is already on":L" is already off"),L"Nothing to change");}
            else add(kind,state<0?L"Turn "+label+L" on or off":goal?L"Turn "+label+L" on":L"Turn "+label+L" off",state<0?L"Current state unknown":state?label+L" is on now":label+L" is off now",state<0&&want<0?-1:goal);
            add(CommandKind::OpenSettings,L"Open "+label+L" settings",L"Windows Settings",0,settingsUri);};
        if(int want=onOff(t,{L"bluetooth",L"bt"});want!=-2){radio(CommandKind::Bluetooth,L"Bluetooth",env.bluetooth,want,L"ms-settings:bluetooth");return out;}
        if(int want=onOff(t,{L"wifi",L"wi-fi",L"wlan",L"wireless"});want!=-2){radio(CommandKind::WiFi,L"Wi-Fi",env.wifi,want,L"ms-settings:network-wifi");return out;}
        if(int want=onOff(t,{L"airplane mode",L"airplane",L"aeroplane mode",L"flight mode"});want!=-2){
            // There is no public switch for Windows' airplane mode itself; this turns every radio off, or back on.
            const bool quiet=env.bluetooth<=0&&env.wifi<=0&&(env.bluetooth==0||env.wifi==0);const int goal=want==-1?(quiet?0:1):want;
            add(CommandKind::Airplane,goal?L"Turn Wi-Fi and Bluetooth off":L"Turn Wi-Fi and Bluetooth back on",goal?L"Airplane mode  \u00b7  every radio off":L"Leaves airplane mode",goal);
            add(CommandKind::OpenSettings,L"Open Airplane mode settings",L"Windows Settings",0,L"ms-settings:network-airplanemode");return out;}
        const bool darkWords=t==L"dark mode"||t==L"dark"||t==L"dark theme"||t==L"go dark",lightWords=t==L"light mode"||t==L"light"||t==L"light theme",themeWords=t==L"theme"||t==L"toggle theme"||t==L"switch theme";
        if(darkWords||lightWords||themeWords){const int goal=themeWords?(env.dark==1?0:1):darkWords?(env.dark==1?0:1):(env.dark==0?1:0);
            // Asking for the mode that is already on offers the other one.
            add(CommandKind::DarkMode,goal?L"Switch to dark mode":L"Switch to light mode",env.dark<0?L"Windows and apps":env.dark?L"Dark mode is on now":L"Light mode is on now",goal);
            add(CommandKind::OpenSettings,L"Open Colors settings",L"Windows Settings",0,L"ms-settings:colors");return out;}
        if(t==L"empty recycle bin"||t==L"empty the recycle bin"||t==L"empty bin"||t==L"recycle bin"||t==L"empty trash"||t==L"clear recycle bin"||t==L"empty the bin"){
            if(env.binItems==0){add(CommandKind::None,L"The recycle bin is already empty",L"Nothing to remove");return out;}
            add(CommandKind::EmptyBin,L"Empty the recycle bin",(env.binItems>0?std::to_wstring(env.binItems)+(env.binItems==1?L" item":L" items")+(env.binBytes>0?L"  \u00b7  "+byteText((unsigned long long)env.binBytes):L""):std::wstring(L"Deletes its files for good"))+L"  \u00b7  asks first");out.back().confirm=true;return out;}
        if(t==L"sleep"||t==L"go to sleep"||t==L"suspend"||t==L"sleep pc"){add(CommandKind::Sleep,L"Put this PC to sleep",L"Asks first");out.back().confirm=true;return out;}
        if(t==L"restart"||t==L"reboot"||t==L"restart pc"||t==L"restart computer"){add(CommandKind::Restart,L"Restart this PC",L"Apps with unsaved work can stop it  \u00b7  asks first");out.back().confirm=true;return out;}
        if(t==L"shut down"||t==L"shutdown"||t==L"power off"||t==L"turn off pc"||t==L"turn off computer"||t==L"shut down pc"){add(CommandKind::ShutDown,L"Shut down this PC",L"Apps with unsaved work can stop it  \u00b7  asks first");out.back().confirm=true;return out;}
    }
    // Currency, only when the words say so ("100 usd to inr", "$50 in eur").
    if(auto cq=parseCurrency(text,env.home)){
        if(!env.currency){add(CommandKind::None,L"Currency conversion is off",L"Turn it on in Settings \u203a Privacy & productivity (daily rates from the European Central Bank)");return out;}
        if(!env.rates||env.rates->empty()){add(CommandKind::None,env.ratesLoading?L"Getting today\u2019s exchange rates\u2026":L"Exchange rates are unavailable",env.ratesLoading?L"From the European Central Bank":L"Check your connection and try again");return out;}
        if(auto a=convertCurrency(*cq,*env.rates)){CommandResult r;r.kind=CommandKind::Currency;r.answer=a->answer;r.title=a->answer;r.detail=a->detail;r.target=a->plain;out.push_back(std::move(r));return out;}
        add(CommandKind::None,L"No rate for that currency",L"The European Central Bank publishes about 30 currencies");return out;
    }
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
    // Weather: "weather London" (or "weather in London") chooses the place.
    if(first==L"weather"||first==L"forecast"){std::wstring town=after(1);for(auto lead:{L"in ",L"for ",L"at "})if(lowered(town).starts_with(lead)){town=trimmed(town.substr(wcslen(lead)));break;}
        // Just "weather": the town is chosen in Island settings (an empty target opens its Town field).
        if(town.size()<2){add(CommandKind::Weather,L"Choose the weather\u2019s town",L"Opens Settings \u203a Compact \u203a Town");return out;}
        if(town.size()>60)town.resize(60);add(CommandKind::Weather,L"Show the weather for "+town,L"In the island\u2019s glance and on Home  \u00b7  from Open-Meteo; only the town is sent",0,town);return out;}
    // A colour code previews itself.
    if(auto colour=parseColourCode(text)){CommandResult r;r.kind=CommandKind::Colour;r.value=int(*colour);r.title=colourHex(*colour);r.answer=r.title;r.detail=colourDetail(*colour);r.target=r.title;out.push_back(std::move(r));return out;}
    // Apps.
    std::wstring appText=text;bool explicitOpen=false;for(auto verb:{L"open ",L"launch ",L"start ",L"run "})if(t.starts_with(verb)){appText=trimmed(text.substr(wcslen(verb)));explicitOpen=true;break;}
    for(auto& a:matchApps(apps,appText)){CommandResult r;r.kind=CommandKind::OpenApp;r.title=L"Open "+a.name;r.detail=L"App";r.target=a.id;r.appId=a.id;r.marks=shiftedMarks(matchMarks(a.name,appText),5);
        // Tab completes the app's name when what is typed is the start of it.
        if(lowered(a.name).starts_with(lowered(appText))&&a.name.size()>appText.size())r.completion=text+a.name.substr(appText.size());
        out.push_back(std::move(r));}
    // Did you mean…? The closest app name or command within a small edit distance.
    if(out.empty()){
        const auto typedApp=lowered(trimmed(appText));const size_t allowance=typoAllowance(typedApp.size());
        if(allowance){const InstalledApp* best=nullptr;size_t bestDistance=allowance+1;
            for(auto& a:apps){auto name=lowered(a.name);if(name.starts_with(L"uninstall"))continue;const auto first=words(name);
                size_t d=editDistance(typedApp,name);if(!first.empty())d=std::min(d,editDistance(typedApp,first[0]));if(d<bestDistance||(d==bestDistance&&best&&a.name.size()<best->name.size())){bestDistance=d;best=&a;}}
            if(best){CommandResult r;r.kind=CommandKind::OpenApp;r.title=L"Open "+best->name;r.detail=L"Did you mean "+best->name+L"?";r.target=best->id;r.appId=best->id;out.push_back(std::move(r));}
            if(!explicitOpen){std::wstring phrase;size_t phraseDistance=allowance+1;for(auto p:commandPhrases){const std::wstring w=trimmed(std::wstring(p));const size_t d=editDistance(t,w);if(d<phraseDistance){phraseDistance=d;phrase=w;}}
                if(!phrase.empty()&&phrase!=t){auto fixed=parseCommand(phrase,apps,workspaces,scope,env);if(!fixed.empty()&&fixed[0].kind!=CommandKind::None){fixed[0].detail=L"Did you mean \u201c"+phrase+L"\u201d?";fixed[0].completion=phrase;out.insert(out.begin(),fixed[0]);}}}
        }
    }
    if(out.empty())add(CommandKind::None,explicitOpen?L"No installed app by that name":L"Nothing to do yet",explicitOpen?L"Only apps in the Start menu can be opened":L"Try “open spotify”, “volume 30”, “focus 25” or “find notes pdf”");
    return out;
}
}
