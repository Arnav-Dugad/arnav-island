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
    {"youtubemusic",L"YouTube Music",{L"youtube music"},false,0,0},{"youtube",L"YouTube",{L"- youtube",L"| youtube"},false,0,0},
    {"spotify",L"Spotify",{L"spotify"},true,0,0},{"soundcloud",L"SoundCloud",{L"soundcloud"},false,0,0},{"twitch",L"Twitch",{L"twitch"},false,0,0},
    {"netflix",L"Netflix",{L"netflix"},true,0,0},{"primevideo",L"Prime Video",{L"prime video"},true,0x00a8e1,L'P'},{"disneyplus",L"Disney+",{L"disney+",L"disneyplus"},true,0x113ccf,L'D'},
    {"hotstar",L"JioHotstar",{L"hotstar"},true,0x1f80e0,L'H'},{"jio",L"JioCinema",{L"jiocinema"},true,0,0},{"jiosaavn",L"JioSaavn",{L"jiosaavn"},true,0x2bc5b4,L'S'},
    {"gaana",L"Gaana",{L"gaana"},true,0xe72c30,L'G'},{"wynk",L"Wynk Music",{L"wynk"},true,0xef3e4a,L'W'},{"amazonmusic",L"Amazon Music",{L"amazon music"},true,0x25d1da,L'A'},
    {"applemusic",L"Apple Music",{L"apple music"},true,0,0},{"appletv",L"Apple TV",{L"apple tv"},true,0,0},{"deezer",L"Deezer",{L"deezer"},true,0,0},{"tidal",L"TIDAL",{L"tidal"},true,0,0},
    {"pandora",L"Pandora",{L"pandora"},true,0,0},{"vimeo",L"Vimeo",{L"vimeo"},false,0,0},{"dailymotion",L"Dailymotion",{L"dailymotion"},false,0,0},{"bilibili",L"Bilibili",{L"bilibili"},false,0,0},
    {"crunchyroll",L"Crunchyroll",{L"crunchyroll"},true,0,0},{"hbomax",L"HBO Max",{L"hbo max"},true,0,0},{"paramountplus",L"Paramount+",{L"paramount+"},true,0,0},{"mubi",L"MUBI",{L"mubi"},true,0,0},
    {"plex",L"Plex",{L"plex"},true,0,0},{"jellyfin",L"Jellyfin",{L"jellyfin"},true,0,0},{"kick",L"Kick",{L"| kick",L"- kick"},false,0,0},{"audible",L"Audible",{L"audible"},true,0,0},
    {"bandcamp",L"Bandcamp",{L"bandcamp"},false,0,0},{"mixcloud",L"Mixcloud",{L"mixcloud"},false,0,0},{"pocketcasts",L"Pocket Casts",{L"pocket casts"},true,0,0},{"castbox",L"Castbox",{L"castbox"},true,0,0},
    {"applepodcasts",L"Apple Podcasts",{L"apple podcasts"},true,0,0},{"iheartradio",L"iHeartRadio",{L"iheart"},true,0,0},{"napster",L"Napster",{L"napster"},true,0,0},{"lastdotfm",L"Last.fm",{L"last.fm"},true,0,0},
    {"tiktok",L"TikTok",{L"tiktok"},false,0,0},{"instagram",L"Instagram",{L"instagram"},false,0,0},{"facebook",L"Facebook",{L"facebook"},false,0,0},{"x",L"X",{L"/ x"},false,0,0},
    {"reddit",L"Reddit",{L"reddit"},false,0,0},{"discord",L"Discord",{L"discord"},false,0,0},
};
inline const ServiceRule* findService(std::string_view slug){for(auto& r:serviceRules)if(r.slug==slug)return &r;return nullptr;}
// Chooses a service from visible browser window titles. A title that contains the
// playing title is strong evidence; a brand-only title (for services whose tab
// titles omit the media name) counts only when exactly one such brand is present.
inline std::string_view matchService(const std::vector<std::wstring>& titles,const std::wstring& mediaTitle,const std::wstring& artist){
    auto needle=folded(mediaTitle.substr(0,std::min<size_t>(mediaTitle.size(),24)));std::string_view brandOnly;int brandOnlyCount=0;
    for(auto& raw:titles){auto t=folded(raw);bool strong=needle.size()>=3&&t.find(needle)!=std::wstring::npos;
        if(strong&&!artist.empty()&&t.find(folded(mediaTitle)+L" • "+folded(artist))!=std::wstring::npos)return "spotify";
        for(auto& r:serviceRules)for(auto n:r.needles){if(n.empty()||t.find(n)==std::wstring::npos)continue;if(strong)return r.slug;if(r.brandOnly&&brandOnly!=r.slug){brandOnly=r.slug;++brandOnlyCount;}}}
    return brandOnlyCount==1?brandOnly:std::string_view{};
}
// Native apps: executable, AUMID or display name to a mark, used only when
// Windows cannot supply the app's own icon.
inline std::string_view appBrand(const std::wstring& identity){
    auto v=folded(identity);
    struct Key{std::wstring_view key;std::string_view slug;};
    static constexpr Key keys[]={{L"spotify","spotify"},{L"discord","discord"},{L"telegram","telegram"},{L"whatsapp","whatsapp"},{L"zoom","zoom"},{L"steam","steam"},{L"epicgames","epicgames"},{L"epic games","epicgames"},
        {L"vlc","vlcmediaplayer"},{L"foobar","foobar2000"},{L"winamp","winamp"},{L"itunes","itunes"},{L"obs","obsstudio"},{L"chrome","googlechrome"},{L"firefox","firefoxbrowser"},{L"brave","brave"},
        {L"opera gx","operagx"},{L"opera","opera"},{L"vivaldi","vivaldi"},{L"applemusic","applemusic"},{L"apple music","applemusic"},{L"appletv","appletv"},{L"apple tv","appletv"},{L"tidal","tidal"},
        {L"deezer","deezer"},{L"plex","plex"},{L"jellyfin","jellyfin"},{L"audible","audible"},{L"pandora","pandora"},{L"netflix","netflix"},{L"tiktok","tiktok"},{L"instagram","instagram"},{L"shazam","shazam"}};
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
        {L"lenovo","lenovo"},{L"legion","lenovo"},{L"thinkpad","lenovo"},{L"dell","dell"},{L"acer","acer"},{L"msi ","msi"},{L"asus","asus"},{L"rog ","asus"},{L"dualsense","playstation"},{L"dualshock","playstation"},{L"playstation","playstation"}};
    for(auto& k:keys)if(hasWord(n,k.key))return k.slug;
    if(n==L"wireless controller")return "playstation";
    if(vidSource==2)switch(vid){case 0x054c:return "sony";case 0x05ac:return "apple";case 0x04e8:return "samsung";case 0x1532:return "razer";case 0x1038:return "steelseries";case 0x1b1c:return "corsair";case 0x0b05:return "asus";case 0x2717:return "xiaomi";default:break;}
    if(vidSource==1)switch(vid){case 0x0075:return "samsung";case 0x004c:return "apple";case 0x012d:return "sony";case 0x009e:return "bose";case 0x0057:return "jbl";case 0x00e0:return "google";case 0x038f:return "xiaomi";default:break;}
    return {};
}
}
