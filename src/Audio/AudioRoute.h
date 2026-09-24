#pragma once
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>
namespace nexus {
// Endpoint form factors from Windows (EndpointFormFactor); -1 when not reported.
enum EndpointForm {FormUnknown=-1,FormSpeakers=1,FormLineLevel=2,FormHeadphones=3,FormMicrophone=4,FormHeadset=5,FormHandset=6,FormSpdif=8,FormDisplay=9};
inline std::wstring routeFold(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
// Something worn on the head or in the ears. Windows' own form factor wins; the name is a
// fallback for drivers that report none (never overriding "speakers" or a display).
inline bool isHeadphoneOutput(const std::wstring& name,int form){
    if(form==FormHeadphones||form==FormHeadset||form==FormHandset)return true;
    if(form==FormSpeakers||form==FormDisplay||form==FormSpdif||form==FormLineLevel)return false;
    auto n=routeFold(name);for(auto key:{L"headphone",L"headset",L"earbud",L"earphone",L"airpods",L"buds",L"hands-free",L"handsfree"})if(n.find(key)!=std::wstring::npos)return true;
    return false;
}
// Endpoint names read "Headphones (WH-1000XM5)" or "Speakers (Realtek(R) Audio)": the device
// is named for headsets and headphones, and speakers are just "Speakers".
inline std::wstring outputDisplayName(const std::wstring& name){
    const size_t open=name.find(L" (");if(open==std::wstring::npos||name.empty()||name.back()!=L')')return name;
    const std::wstring generic=routeFold(name.substr(0,open)),specific=name.substr(open+2,name.size()-open-3);
    if(generic.starts_with(L"speaker"))return L"Speakers";
    for(auto g:{L"headphones",L"headphone",L"headset",L"earphones",L"headset earphone",L"hands-free",L"hands-free ag audio",L"stereo",L"earbuds"})if(generic==g)return specific.empty()?name:specific;
    return name;
}
// Which mixer entry plays the current media session: an exact app name, then one name
// inside the other (at least 4 letters), preferring sessions that are producing sound.
struct MixerName {std::wstring name;bool active=false,system=false;};
inline int mixerIndexFor(const std::wstring& appName,const std::vector<MixerName>& entries){
    const auto app=routeFold(appName);if(app.size()<2)return -1;int best=-1,bestScore=0;
    for(size_t i=0;i<entries.size();++i){if(entries[i].system)continue;auto n=routeFold(entries[i].name);if(n.empty())continue;
        int score=n==app?4:(n.size()>=4&&app.find(n)!=std::wstring::npos)||(app.size()>=4&&n.find(app)!=std::wstring::npos)?2:0;if(!score)continue;score+=entries[i].active?1:0;
        if(score>bestScore){bestScore=score;best=int(i);}}
    return best;
}
}
