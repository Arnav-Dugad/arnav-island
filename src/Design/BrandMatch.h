#pragma once
#include "Design/Brands.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>
namespace nexus {
inline std::wstring folded(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
// Keyword at the start of a word, so "s24" matches "Galaxy S24" but not "TAS2400".
inline bool hasWord(const std::wstring& text,std::wstring_view key){for(size_t at=text.find(key);at!=std::wstring::npos;at=text.find(key,at+1))if(at==0||!std::iswalnum(text[at-1]))return true;return false;}
// A website or service identified from browser window titles. `slug` names a
// Simple Icons mark; services without one carry a brand colour and monogram.
struct ServiceRule {std::string_view slug;std::wstring_view name;std::array<std::wstring_view,2> needles;bool brandOnly;uint32_t color;wchar_t monogram;};
inline constexpr ServiceRule serviceRules[]={
    {"youtubemusic",L"YouTube Music",{L"youtube music"},false,0,0},{"youtubetv",L"YouTube TV",{L"youtube tv"},false,0,0},{"youtubekids",L"YouTube Kids",{L"youtube kids"},false,0,0},
    {"youtube",L"YouTube",{L"- youtube",L"| youtube"},false,0,0},
    {"spotify",L"Spotify",{L"spotify"},true,0,0},{"soundcloud",L"SoundCloud",{L"soundcloud"},false,0,0},{"twitch",L"Twitch",{L"twitch"},false,0,0},
    {"netflix",L"Netflix",{L"netflix"},true,0,0},{"primevideo",L"Prime Video",{L"prime video"},true,0,0},{"disneyplus",L"Disney+",{L"disney+",L"disneyplus"},true,0x113ccf,L'D'},
    {"jiohotstar",L"JioHotstar",{L"hotstar"},true,0,0},{"jio",L"JioCinema",{L"jiocinema"},true,0,0},{"jiosaavn",L"JioSaavn",{L"jiosaavn"},true,0x2bc5b4,L'S'},
    {"gaana",L"Gaana",{L"gaana"},true,0xe72c30,L'G'},{"wynk",L"Wynk Music",{L"wynk"},true,0xef3e4a,L'W'},{"amazonmusic",L"Amazon Music",{L"amazon music"},true,0x25d1da,L'A'},
    {"applemusic",L"Apple Music",{L"apple music"},true,0,0},{"appletv",L"Apple TV",{L"apple tv"},true,0,0},{"deezer",L"Deezer",{L"deezer"},true,0,0},{"tidal",L"TIDAL",{L"tidal"},true,0,0},
    {"pandora",L"Pandora",{L"pandora"},true,0,0},{"vimeo",L"Vimeo",{L"vimeo"},false,0,0},{"dailymotion",L"Dailymotion",{L"dailymotion"},false,0,0},{"bilibili",L"Bilibili",{L"bilibili"},false,0,0},
    {"crunchyroll",L"Crunchyroll",{L"crunchyroll"},true,0,0},{"hbomax",L"HBO Max",{L"hbo max"},true,0,0},{"paramountplus",L"Paramount+",{L"paramount+"},true,0,0},{"mubi",L"MUBI",{L"mubi"},true,0,0},
    {"plex",L"Plex",{L"plex"},true,0,0},{"jellyfin",L"Jellyfin",{L"jellyfin"},true,0,0},{"kick",L"Kick",{L"| kick",L"- kick"},false,0,0},{"audible",L"Audible",{L"audible"},true,0,0},
    {"bandcamp",L"Bandcamp",{L"bandcamp"},false,0,0},{"mixcloud",L"Mixcloud",{L"mixcloud"},false,0,0},{"pocketcasts",L"Pocket Casts",{L"pocket casts"},true,0,0},{"castbox",L"Castbox",{L"castbox"},true,0,0},
    {"applepodcasts",L"Apple Podcasts",{L"apple podcasts"},true,0,0},{"iheartradio",L"iHeartRadio",{L"iheart"},true,0,0},{"napster",L"Napster",{L"napster"},true,0,0},{"lastdotfm",L"Last.fm",{L"last.fm"},true,0,0},
    {"tiktok",L"TikTok",{L"tiktok"},false,0,0},{"instagram",L"Instagram",{L"instagram"},false,0,0},{"facebook",L"Facebook",{L"facebook"},false,0,0},{"x",L"X",{L"/ x"},false,0,0},
    {"reddit",L"Reddit",{L"reddit"},false,0,0},{"discord",L"Discord",{L"discord"},false,0,0},
    {"hulu",L"Hulu",{L"hulu"},true,0,0},{"f1",L"F1 TV",{L"f1 tv",L"formula 1"},true,0,0},{"dazn",L"DAZN",{L"dazn"},true,0,0},{"fubo",L"Fubo",{L"fubo"},true,0,0},
    {"nba",L"NBA",{L"nba league pass",L"| nba"},true,0,0},{"audiomack",L"Audiomack",{L"audiomack"},true,0,0},{"linkedin",L"LinkedIn",{L"linkedin"},false,0,0},
    {"pinterest",L"Pinterest",{L"pinterest"},false,0,0},{"snapchat",L"Snapchat",{L"snapchat"},false,0,0},{"threads",L"Threads",{L"on threads"},false,0,0},
};
inline const ServiceRule* findService(std::string_view slug){for(auto& r:serviceRules)if(r.slug==slug)return &r;return nullptr;}
// Assigns a service to every browser media session at once, from the titles of
// all open tabs. A tab containing a session's playing title identifies that
// session and is claimed, so one tab never labels two sessions. Services whose
// tab titles omit the media name (brand-only rules) can identify a session only
// when exactly one session is still unmatched and exactly one such brand remains
// among unclaimed tabs. Everything else stays unidentified (browser icon).
struct MediaTitle {std::wstring title,artist;};
inline std::vector<std::string_view> assignServices(const std::vector<std::wstring>& tabs,const std::vector<MediaTitle>& sessions){
    std::vector<std::wstring> folds;folds.reserve(tabs.size());for(auto& t:tabs)folds.push_back(folded(t));
    std::vector<std::string_view> result(sessions.size());std::vector<bool> claimed(tabs.size()),matched(sessions.size());
    auto strong=[&](size_t tab,const MediaTitle& m){auto needle=folded(m.title.substr(0,std::min<size_t>(m.title.size(),24)));return needle.size()>=3&&folds[tab].find(needle)!=std::wstring::npos;};
    auto serviceOf=[&](size_t tab,const MediaTitle& m)->std::string_view{const auto& t=folds[tab];
        if(!m.artist.empty()&&t.find(folded(m.title)+L" \u2022 "+folded(m.artist))!=std::wstring::npos)return "spotify";
        for(auto& r:serviceRules)for(auto n:r.needles)if(!n.empty()&&t.find(n)!=std::wstring::npos)return r.slug;return {};};
    // Unclaimed tabs first; a second session with the same title may share a claimed tab.
    for(int pass=0;pass<2;++pass)for(size_t i=0;i<sessions.size();++i){if(matched[i])continue;
        for(size_t j=0;j<tabs.size();++j)if((pass==1||!claimed[j])&&strong(j,sessions[i])){result[i]=serviceOf(j,sessions[i]);claimed[j]=matched[i]=true;break;}}
    size_t unmatched=0,which=0;for(size_t i=0;i<sessions.size();++i)if(!matched[i]){++unmatched;which=i;}
    if(unmatched!=1)return result;
    std::string_view brand;int brands=0;
    for(size_t j=0;j<tabs.size();++j){if(claimed[j])continue;for(auto& r:serviceRules){if(!r.brandOnly)continue;bool hit=false;for(auto n:r.needles)if(!n.empty()&&folds[j].find(n)!=std::wstring::npos)hit=true;if(hit&&r.slug!=brand){brand=r.slug;++brands;}if(hit)break;}}
    if(brands==1)result[which]=brand;
    return result;
}
// Native apps: executable, AUMID or display name to a mark, used only when
// Windows cannot supply the app's own icon.
inline std::string_view appBrand(const std::wstring& identity){
    auto v=folded(identity);
    struct Key{std::wstring_view key;std::string_view slug;};
    static constexpr Key keys[]={{L"spotify","spotify"},{L"discord","discord"},{L"telegram","telegram"},{L"whatsapp","whatsapp"},{L"zoom","zoom"},{L"steam","steam"},{L"epicgames","epicgames"},{L"epic games","epicgames"},
        {L"vlc","vlcmediaplayer"},{L"foobar","foobar2000"},{L"winamp","winamp"},{L"itunes","itunes"},{L"obs","obsstudio"},{L"chrome","googlechrome"},{L"firefox","firefoxbrowser"},{L"brave","brave"},
        {L"opera gx","operagx"},{L"opera","opera"},{L"vivaldi","vivaldi"},{L"applemusic","applemusic"},{L"apple music","applemusic"},{L"appletv","appletv"},{L"apple tv","appletv"},{L"tidal","tidal"},
        {L"deezer","deezer"},{L"plex","plex"},{L"jellyfin","jellyfin"},{L"audible","audible"},{L"pandora","pandora"},{L"netflix","netflix"},{L"tiktok","tiktok"},{L"instagram","instagram"},{L"shazam","shazam"},
        {L"xbox","xbox"},{L"gamingapp","xbox"},{L"minecraft","minecraft"},{L"kodi","kodi"},{L"stremio","stremio"},{L"mpv","mpv"},{L"hulu","hulu"},{L"primevideo","primevideo"},{L"prime video","primevideo"},
        {L"msteams","microsoftteams"},{L"microsoft teams","microsoftteams"},{L"slack","slack"},{L"skype","skype"},{L"linkedin","linkedin"},{L"messenger","messenger"},{L"signal","signal"},{L"notion","notion"},
        {L"figma","figma"},{L"davinci","davinciresolve"},{L"streamlabs","streamlabs"},{L"battle.net","battledotnet"},{L"ubisoft","ubisoft"},{L"eadesktop","ea"},{L"ea app","ea"},{L"riot client","riotgames"},{L"riotclient","riotgames"},
        {L"roblox","roblox"},{L"retroarch","retroarch"},{L"itch.io","itchdotio"},{L"itchio","itchdotio"},{L"msedge","microsoftedge"},{L"microsoft edge","microsoftedge"},{L"chatgpt","openai"},{L"openai","openai"},{L"claude","claude"},
        {L"perplexity","perplexity"},{L"audiomack","audiomack"},{L"teamspeak","teamspeak"},{L"mumble","mumble"},{L"guilded","guilded"},{L"logi options","logitech"},{L"logitech","logitech"}};
    for(auto& k:keys)if(v.find(k.key)!=std::wstring::npos)return k.slug;return {};
}
enum class DeviceKind { Other,Headphones,Earbuds,Speaker,Phone,Computer,Keyboard,Mouse,Gamepad,Watch,Audio };
// Bluetooth Class of Device first; names cover LE devices, which have no CoD.
inline DeviceKind deviceKind(uint32_t cod,const std::wstring& name){
    auto n=folded(name);auto has=[&](std::initializer_list<std::wstring_view> keys){for(auto k:keys)if(hasWord(n,k))return true;return false;};
    if(has({L"buds",L"airpods",L"airdopes",L"earbuds",L"freebuds",L"enco",L"tws",L"pods"}))return DeviceKind::Earbuds;
    if(cod){const uint32_t major=(cod>>8)&0x1f,minor=(cod>>2)&0x3f;
        if(major==4){if(minor==1||minor==2||minor==6)return DeviceKind::Headphones;if(minor==5||minor==7||minor==10||minor==8)return DeviceKind::Speaker;return DeviceKind::Audio;}
        if(major==5){const uint32_t input=(cod>>6)&3,sub=(cod>>2)&0xf;if(sub==1||sub==2)return DeviceKind::Gamepad;if(input==1||input==3)return DeviceKind::Keyboard;if(input==2)return DeviceKind::Mouse;}
        if(major==2)return DeviceKind::Phone;if(major==1)return DeviceKind::Computer;if(major==7)return DeviceKind::Watch;}
    if(has({L"controller",L"gamepad",L"dualsense",L"dualshock"}))return DeviceKind::Gamepad;if(has({L"keyboard",L"kb",L"keychron"}))return DeviceKind::Keyboard;
    if(has({L"mouse",L"mice",L"basilisk",L"deathadder",L"viper",L"g pro",L"mx master"}))return DeviceKind::Mouse;if(has({L"watch",L"band",L"fit"}))return DeviceKind::Watch;
    if(has({L"speaker",L"soundlink",L"partybox",L"partypal",L"stone",L"flip",L"charge",L"boom"}))return DeviceKind::Speaker;
    if(has({L"headphone",L"wh-",L"quietcomfort",L"momentum",L"arctis",L"cloud",L"rockerz",L"headset"}))return DeviceKind::Headphones;
    if(has({L"phone",L"galaxy s",L"iphone",L"pixel",L"s2",L"oneplus"}))return DeviceKind::Phone;return DeviceKind::Other;
}
// Device maker from its name, then from the vendor ID it reports (USB or
// Bluetooth SIG company identifiers).
inline std::string_view deviceBrand(const std::wstring& name,uint32_t vid=0,int vidSource=0){
    auto n=folded(name);
    struct Key{std::wstring_view key;std::string_view slug;};
    static constexpr Key keys[]={{L"wh-1000","sony"},{L"wf-1000","sony"},{L"linkbuds","sony"},{L"sony","sony"},{L"srs-","sony"},{L"airpods","apple"},{L"iphone","apple"},{L"ipad","apple"},{L"macbook","apple"},{L"apple","apple"},
        {L"beats","beats"},{L"powerbeats","beats"},{L"bose","bose"},{L"quietcomfort","bose"},{L"soundlink","bose"},{L"jbl","jbl"},{L"partybox","jbl"},{L"galaxy","samsung"},{L"buds3","samsung"},{L"buds2","samsung"},{L"buds pro","samsung"},{L"buds live","samsung"},{L"buds fe","samsung"},{L"samsung","samsung"},{L"s22","samsung"},{L"s23","samsung"},{L"s24","samsung"},{L"s25","samsung"},{L"z fold","samsung"},{L"z flip","samsung"},
        {L"boat","boat"},{L"stone ","boat"},{L"rockerz","boat"},{L"airdopes","boat"},{L"partypal","boat"},{L"aavante","boat"},{L"immortal","boat"},{L"sennheiser","sennheiser"},{L"momentum","sennheiser"},
        {L"steelseries","steelseries"},{L"arctis","steelseries"},{L"corsair","corsair"},{L"virtuoso","corsair"},{L"hyperx","hyperx"},{L"razer","razer"},{L"barracuda","razer"},{L"hammerhead","razer"},
        {L"xiaomi","xiaomi"},{L"redmi","xiaomi"},{L"oneplus","oneplus"},{L"nord buds","oneplus"},{L"pixel","google"},{L"huawei","huawei"},{L"freebuds","huawei"},{L"oppo","oppo"},{L"enco","oppo"},{L"vivo","vivo"},{L"nokia","nokia"},
        {L"lenovo","lenovo"},{L"legion","lenovo"},{L"thinkpad","lenovo"},{L"dell","dell"},{L"acer","acer"},{L"msi ","msi"},{L"asus","asus"},{L"rog ","asus"},{L"dualsense","playstation"},{L"dualshock","playstation"},{L"playstation","playstation"},
        {L"xbox","xbox"},{L"surface","microsoft"},{L"microsoft","microsoft"},{L"powera","powera"},{L"aula","aula"},{L"philips","philips"},{L"jabra","jabra"},
        {L"marshall","marshall"},{L"major iv","marshall"},{L"major v","marshall"},{L"minor iv","marshall"},{L"motif","marshall"},{L"emberton","marshall"},{L"stanmore","marshall"},{L"acton","marshall"},
        {L"logitech","logitech"},{L"logi ","logitech"},{L"mx master","logitech"},{L"mx keys","logitech"},{L"mx anywhere","logitech"},{L"nintendo","nintendo"},{L"pro controller","nintendo"},{L"joy-con","nintendo"},
        {L"audio-technica","audiotechnica"},{L"ath-","audiotechnica"},{L"sonos","sonos"},{L"motorola","motorola"},{L"moto buds","motorola"},{L"moto g","motorola"},{L"honor","honor"},{L"redragon","redragon"},
        {L"alienware","alienware"},{L"nzxt","nzxt"},{L"cooler master","coolermaster"},{L"astro a","astro"}};
    for(auto& k:keys)if(hasWord(n,k.key))return k.slug;
    if(n==L"wireless controller")return "playstation";
    // Philips audio model codes: TAS (speakers), TAT (earbuds), TAH (headphones), TAA (sport), then digits.
    for(size_t at=0;at+3<n.size();++at)if((at==0||!std::iswalnum(n[at-1]))&&n[at]==L't'&&n[at+1]==L'a'&&(n[at+2]==L's'||n[at+2]==L't'||n[at+2]==L'h'||n[at+2]==L'a')&&std::iswdigit(n[at+3]))return "philips";
    if(vidSource==2)switch(vid){case 0x054c:return "sony";case 0x05ac:return "apple";case 0x04e8:return "samsung";case 0x1532:return "razer";case 0x1038:return "steelseries";case 0x1b1c:return "corsair";case 0x0b05:return "asus";case 0x2717:return "xiaomi";case 0x045e:return "microsoft";case 0x046d:return "logitech";case 0x057e:return "nintendo";case 0x20d6:return "powera";case 0x0471:return "philips";case 0x0b0e:return "jabra";default:break;}
    if(vidSource==1)switch(vid){case 0x0075:return "samsung";case 0x004c:return "apple";case 0x012d:return "sony";case 0x009e:return "bose";case 0x0057:return "jbl";case 0x00e0:return "google";case 0x038f:return "xiaomi";case 0x0006:return "microsoft";case 0x0067:return "jabra";case 0x01da:return "logitech";case 0x0553:return "nintendo";default:break;}
    return {};
}
}
