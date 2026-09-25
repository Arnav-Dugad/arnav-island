#pragma once
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <random>
#include <string>
#include <vector>
namespace nexus {
// The island's own player's session, told apart from Windows' sessions by this id (Media/IslandPlayer.h).
constexpr uint64_t islandSessionId=0x49534c414e440001ull;
constexpr const wchar_t* islandSource=L"ArnavIsland.Player";
// Phase 5G: songs in your Music folder, played by the island itself. This is the pure part:
// which files are songs, a title from a file name, the order the library is listed in,
// searching it, and finding a handed-off song in it. Nothing here reads files.
struct LibraryTrack{std::wstring path,title,artist,album;double duration=0;uint32_t number=0;uint64_t size=0;};
// Audio files Windows plays out of the box (Ogg and Opus need the free Web Media Extensions, usually present).
inline bool isAudioFile(const std::wstring& path){
    const auto dot=path.find_last_of(L".\\/");if(dot==std::wstring::npos||path[dot]!=L'.')return false;
    std::wstring ext=path.substr(dot);for(auto& c:ext)c=wchar_t(std::towlower(c));
    for(auto e:{L".mp3",L".m4a",L".aac",L".flac",L".wav",L".wma",L".ogg",L".opus"})if(ext==e)return true;
    return false;
}
// "03 - Blue in Green.mp3" -> "Blue in Green": no extension, no leading track number, underscores as spaces.
inline std::wstring titleFromFileName(const std::wstring& path){
    std::wstring name=path.substr(path.find_last_of(L"\\/")==std::wstring::npos?0:path.find_last_of(L"\\/")+1);
    if(auto dot=name.rfind(L'.');dot!=std::wstring::npos)name.resize(dot);
    for(auto& c:name)if(c==L'_')c=L' ';
    size_t i=0;while(i<name.size()&&std::iswdigit(name[i]))++i;
    // A number of one to three digits, then a separator, is a track number ("1-02" disc and track too).
    if(i>0&&i<=3&&i<name.size()){size_t j=i;if(name[j]==L'-'&&j+1<name.size()&&std::iswdigit(name[j+1])){j+=1;while(j<name.size()&&std::iswdigit(name[j]))++j;}
        size_t k=j;while(k<name.size()&&(name[k]==L' '||name[k]==L'.'||name[k]==L'-'||name[k]==L')'))++k;if(k>j&&k<name.size())name=name.substr(k);}
    size_t a=name.find_first_not_of(L' '),b=name.find_last_not_of(L' ');return a==std::wstring::npos?std::wstring(L"Untitled"):name.substr(a,b-a+1);
}
// Lower case, letters and digits only (spaces between words), for comparing names.
inline std::wstring foldName(const std::wstring& s){
    std::wstring out;bool space=false;
    for(wchar_t c:s){if(std::iswalnum(c)){if(space&&!out.empty())out+=L' ';space=false;out+=wchar_t(std::towlower(c));}else space=true;}
    return out;
}
// By artist, album, track number, then title; songs with no artist go last.
inline void sortLibrary(std::vector<LibraryTrack>& tracks){
    std::stable_sort(tracks.begin(),tracks.end(),[](const LibraryTrack& a,const LibraryTrack& b){
        if(a.artist.empty()!=b.artist.empty())return b.artist.empty();
        const auto fa=foldName(a.artist),fb=foldName(b.artist);if(fa!=fb)return fa<fb;
        const auto la=foldName(a.album),lb=foldName(b.album);if(la!=lb)return la<lb;
        if(a.number!=b.number)return a.number<b.number;return foldName(a.title)<foldName(b.title);});
}
// Songs whose title, artist or album contain every word of the query, best first: a title that starts with
// the query, then a title that contains it, then the rest (ties keep the library's order).
inline std::vector<size_t> searchLibrary(const std::vector<LibraryTrack>& tracks,const std::wstring& query,size_t limit=50){
    const std::wstring q=foldName(query);std::vector<std::wstring> words;{size_t at=0;while(at<q.size()){size_t end=q.find(L' ',at);if(end==std::wstring::npos)end=q.size();if(end>at)words.push_back(q.substr(at,end-at));at=end+1;}}
    std::vector<std::pair<int,size_t>> found;
    for(size_t i=0;i<tracks.size();++i){const auto title=foldName(tracks[i].title),hay=title+L" "+foldName(tracks[i].artist)+L" "+foldName(tracks[i].album);
        if(!std::all_of(words.begin(),words.end(),[&](auto& w){return hay.find(w)!=std::wstring::npos;}))continue;
        const int rank=q.empty()?2:title.starts_with(q)?0:title.find(q)!=std::wstring::npos?1:2;found.push_back({rank,i});}
    std::stable_sort(found.begin(),found.end(),[](auto& a,auto& b){return a.first<b.first;});
    std::vector<size_t> out;for(auto& f:found){if(out.size()>=limit)break;out.push_back(f.second);}return out;
}
// A handed-off song in this library: the same title (and artist, when both have one), else the same file name and size.
inline int matchTrack(const std::vector<LibraryTrack>& tracks,const std::wstring& title,const std::wstring& artist,const std::wstring& fileName,uint64_t size){
    const auto t=foldName(title),a=foldName(artist);
    if(!t.empty())for(size_t i=0;i<tracks.size();++i)if(foldName(tracks[i].title)==t){const auto ta=foldName(tracks[i].artist);if(a.empty()||ta.empty()||ta==a||ta.find(a)!=std::wstring::npos||a.find(ta)!=std::wstring::npos)return int(i);}
    if(!fileName.empty()){const auto f=foldName(fileName);for(size_t i=0;i<tracks.size();++i){const auto& p=tracks[i].path;if(foldName(p.substr(p.find_last_of(L"\\/")==std::wstring::npos?0:p.find_last_of(L"\\/")+1))==f&&(!size||tracks[i].size==size))return int(i);}}
    return -1;
}
// A shuffled order of n songs that starts with `first` (when given).
inline std::vector<size_t> shuffledOrder(size_t n,uint32_t seed,int first=-1){
    std::vector<size_t> order(n);for(size_t i=0;i<n;++i)order[i]=i;std::mt19937 random(seed);std::shuffle(order.begin(),order.end(),random);
    if(first>=0&&size_t(first)<n){auto it=std::find(order.begin(),order.end(),size_t(first));std::rotate(order.begin(),it,it+1);}
    return order;
}
}
