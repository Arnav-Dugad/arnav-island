#pragma once
#include "FileShelf/FileShelf.h"
#include <string>
#include <vector>
namespace nexus {
// The pinned Shelf on disk: one line per item, "f<TAB>path" or "t<TAB>text". Only references
// and dropped text are kept; files themselves are never copied.
inline std::wstring saveShelf(const std::vector<ShelfItem>& items){
    auto escape=[](const std::wstring& v){std::wstring o;for(wchar_t c:v){if(c==L'\\')o+=L"\\\\";else if(c==L'\n')o+=L"\\n";else if(c==L'\r')o+=L"\\r";else if(c==L'\t')o+=L"\\t";else o+=c;}return o;};
    std::wstring out=L"shelf 1\n";for(auto& i:items)out+=(i.kind==ShelfItem::Kind::File?L"f\t":L"t\t")+escape(i.value)+L"\n";return out;
}
inline std::wstring shelfLabel(const ShelfItem& i){
    if(i.kind==ShelfItem::Kind::File){auto pos=i.value.find_last_of(L"\\/");return pos==std::wstring::npos||pos+1==i.value.size()?i.value:i.value.substr(pos+1);}
    std::wstring label=i.value.substr(0,64);for(auto& c:label)if(c<32)c=L' ';return label;
}
// `exists` decides which files are still there; missing ones are dropped. At most 32 items.
template<class Exists> std::vector<ShelfItem> loadShelf(const std::wstring& text,Exists exists){
    std::vector<ShelfItem> out;if(!text.starts_with(L"shelf 1\n"))return out;
    auto unescape=[](const std::wstring& v){std::wstring o;for(size_t i=0;i<v.size();++i){if(v[i]!=L'\\'||i+1==v.size()){o+=v[i];continue;}wchar_t n=v[++i];o+=n==L'n'?L'\n':n==L'r'?L'\r':n==L't'?L'\t':n;}return o;};
    size_t at=8;while(at<text.size()&&out.size()<32){size_t end=text.find(L'\n',at);if(end==std::wstring::npos)end=text.size();const std::wstring line=text.substr(at,end-at);at=end+1;
        if(line.size()<3||line[1]!=L'\t'||(line[0]!=L'f'&&line[0]!=L't'))continue;ShelfItem item;item.kind=line[0]==L'f'?ShelfItem::Kind::File:ShelfItem::Kind::Text;item.value=unescape(line.substr(2));
        if(item.value.empty()||item.value.size()>32767||(item.kind==ShelfItem::Kind::File&&!exists(item.value)))continue;
        if(std::any_of(out.begin(),out.end(),[&](auto& o){return o.kind==item.kind&&o.value==item.value;}))continue;
        item.label=shelfLabel(item);out.push_back(std::move(item));}
    return out;
}
}
