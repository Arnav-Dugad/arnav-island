#include "Island/IslandWindow.h"
#include <wlanapi.h>
#include <algorithm>
#include <chrono>
#include <thread>

namespace nexus {
// 0.22: a phone's hotspot, joined in one tap. The phone says its name and password (typed once in the app on the phone;
// Android doesn't tell apps), and the island makes a Wi-Fi profile for it (connecting by hand only, so the PC doesn't
// reach for the phone later by itself) and connects.
// Windows 11 counts Wi-Fi names as location: reading them asks for location access the first time. So nothing here reads
// them until Join is pressed. With location allowed, the network is found first, joined as it's secured and confirmed by
// name; without it, it's joined as phones secure hotspots (WPA2, then WPA3) and confirmed by the Wi-Fi's own state.
// wlanapi.dll is loaded when it's needed: PCs without Wi-Fi may not have the service running at all.
namespace hotspot {
struct Wlan {
    HMODULE dll=nullptr;
    decltype(&WlanOpenHandle) open=nullptr;decltype(&WlanCloseHandle) close=nullptr;decltype(&WlanEnumInterfaces) interfaces=nullptr;decltype(&WlanFreeMemory) free=nullptr;
    decltype(&WlanScan) scan=nullptr;decltype(&WlanGetAvailableNetworkList) networks=nullptr;decltype(&WlanSetProfile) setProfile=nullptr;decltype(&WlanDeleteProfile) deleteProfile=nullptr;
    decltype(&WlanConnect) connect=nullptr;decltype(&WlanQueryInterface) query=nullptr;
    HANDLE handle=nullptr;
    Wlan(){dll=LoadLibraryExW(L"wlanapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll)return;
        auto get=[&](auto& f,const char* name){f=reinterpret_cast<std::remove_reference_t<decltype(f)>>(reinterpret_cast<void*>(GetProcAddress(dll,name)));};
        get(open,"WlanOpenHandle");get(close,"WlanCloseHandle");get(interfaces,"WlanEnumInterfaces");get(free,"WlanFreeMemory");get(scan,"WlanScan");get(networks,"WlanGetAvailableNetworkList");
        get(setProfile,"WlanSetProfile");get(deleteProfile,"WlanDeleteProfile");get(connect,"WlanConnect");get(query,"WlanQueryInterface");
        if(!open||!close||!interfaces||!free||!scan||!networks||!setProfile||!deleteProfile||!connect||!query)return;
        DWORD version=0;if(open(2,nullptr,&version,&handle)!=ERROR_SUCCESS)handle=nullptr;}
    ~Wlan(){if(handle&&close)close(handle,nullptr);if(dll)FreeLibrary(dll);}
    bool ok()const{return handle!=nullptr;}
};
std::string utf8(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(std::max(n,0)),'\0');
    if(n>0)WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
bool sameSsid(const DOT11_SSID& a,const std::string& b){return a.uSSIDLength==b.size()&&std::equal(b.begin(),b.end(),a.ucSSID);}
std::wstring escaped(const std::wstring& t){std::wstring o;for(wchar_t c:t){switch(c){case L'&':o+=L"&amp;";break;case L'<':o+=L"&lt;";break;case L'>':o+=L"&gt;";break;case L'"':o+=L"&quot;";break;case L'\'':o+=L"&apos;";break;default:o+=c;}}return o;}
// The SSID the interface is connected to ("" when it isn't); false when Windows won't say (location access off).
bool connected(Wlan& w,const GUID& id,std::string& out){
    out.clear();DWORD size=0;void* data=nullptr;WLAN_OPCODE_VALUE_TYPE type{};
    const DWORD r=w.query(w.handle,&id,wlan_intf_opcode_current_connection,nullptr,&size,&data,&type);
    if(r==ERROR_SUCCESS&&data){auto* c=static_cast<WLAN_CONNECTION_ATTRIBUTES*>(data);
        if(c->isState==wlan_interface_state_connected){const auto& s=c->wlanAssociationAttributes.dot11Ssid;out.assign(reinterpret_cast<const char*>(s.ucSSID),std::min<ULONG>(s.uSSIDLength,32));}
        w.free(data);return true;}
    if(data)w.free(data);
    // Not connected at all answers ERROR_INVALID_STATE; only a refusal means Windows won't say.
    return r!=ERROR_ACCESS_DENIED;
}
int state(Wlan& w,const GUID& id){DWORD size=0;void* data=nullptr;WLAN_OPCODE_VALUE_TYPE type{};int s=-1;
    if(w.query(w.handle,&id,wlan_intf_opcode_interface_state,nullptr,&size,&data,&type)==ERROR_SUCCESS&&data)s=int(*static_cast<WLAN_INTERFACE_STATE*>(data));if(data)w.free(data);return s;}
// The profile, secured one way; its connection, and whether it came up (confirmed by name when Windows says names).
bool attempt(Wlan& w,const GUID& id,const std::wstring& ssidWide,const std::string& ssid,const std::wstring& key,const wchar_t* authentication,const wchar_t* encryption,bool named){
    const bool open=wcscmp(authentication,L"open")==0;
    wchar_t hex[3];std::wstring ssidHex;for(unsigned char c:ssid){swprintf(hex,3,L"%02X",c);ssidHex+=hex;}
    std::wstring xml=L"<?xml version=\"1.0\"?><WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\"><name>"+escaped(ssidWide)+L"</name>"
        L"<SSIDConfig><SSID><hex>"+ssidHex+L"</hex><name>"+escaped(ssidWide)+L"</name></SSID></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode>"
        L"<MSM><security><authEncryption><authentication>"+authentication+L"</authentication><encryption>"+encryption+L"</encryption><useOneX>false</useOneX></authEncryption>";
    if(!open)xml+=L"<sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>"+escaped(key)+L"</keyMaterial></sharedKey>";
    xml+=L"</security></MSM></WLANProfile>";
    DWORD reason=0;if(w.setProfile(w.handle,&id,0,xml.c_str(),nullptr,TRUE,nullptr,&reason)!=ERROR_SUCCESS)return false;
    const int before=state(w,id);
    WLAN_CONNECTION_PARAMETERS p{};p.wlanConnectionMode=wlan_connection_mode_profile;p.strProfile=ssidWide.c_str();p.dot11BssType=dot11_BSS_type_infrastructure;
    if(w.connect(w.handle,&id,&p,nullptr)!=ERROR_SUCCESS){w.deleteProfile(w.handle,&id,ssidWide.c_str(),nullptr);return false;}
    // Up to 20 s (a phone's hotspot can take a while to hand out an address). Without names: connected once the old
    // network (if any) was left, with no disconnect after joining this one began (a failure disconnects, and only then
    // does Windows fall back to another network).
    bool left=before!=wlan_interface_state_connected,began=false,dropped=false;
    for(int i=0;i<200;++i){std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(named){std::string now;if(connected(w,id,now)&&now==ssid)return true;continue;}
        const int s=state(w,id);if(s<0)continue;
        if(s!=wlan_interface_state_connected)left=true;
        if(s==wlan_interface_state_associating||s==wlan_interface_state_discovering||s==wlan_interface_state_authenticating)began=true;
        else if(s==wlan_interface_state_disconnected&&began)dropped=true;
        else if(s==wlan_interface_state_connected&&left&&!dropped)return true;
        if(dropped)break;}
    // Not with this profile (the password, most likely): it goes, so another can be tried.
    w.deleteProfile(w.handle,&id,ssidWide.c_str(),nullptr);return false;
}
}

// A phone's hotspot came on (with its name and password) or went off.
void IslandWindow::phoneHotspot(const std::string& peer,const std::wstring& phone,bool on,const std::wstring& ssid,const std::wstring& key){
    const double now=seconds();
    if(!on){if(auto it=hotspots_.find(peer);it!=hotspots_.end()&&it->second.ssid==hotspotJoined_)hotspotJoined_.clear();hotspots_.erase(peer);content_.hotspots.erase(peer);
        if(state_==IslandState::Notification&&content_.notice.kind==21&&content_.notice.peer==peer){events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);}
        refresh();return;}
    // 0.22.1: new whenever the island didn't know it was on (a phone that went away and came back says so again), except
    // the hotspot this PC joined, while it's still on.
    const auto was=hotspots_.find(peer);const bool fresh=was==hotspots_.end()||was->second.ssid!=ssid||was->second.key!=key;
    hotspots_[peer]={ssid,key,now};content_.hotspots[peer]=ssid;refresh();
    if(fresh&&ssid!=hotspotJoined_)hotspotCard(peer,phone);
}
// Its card: up longer when this PC has no internet of its own.
void IslandWindow::hotspotCard(const std::string& peer,const std::wstring& phone){
    auto it=hotspots_.find(peer);if(it==hotspots_.end()||!renderer_)return;
    const bool offline=!share_||!share_->internet();
    content_.notice={};content_.notice.kind=21;content_.notice.phone=true;content_.notice.peer=peer;content_.notice.app=phone+L"\u2019s hotspot";
    content_.notice.detail=it->second.ssid+(offline?L"  \u00b7  this PC is offline":L"  \u00b7  its internet, here");
    content_.pinned=false;{const Activity a{ActivityKind::Notification,"hotspot",72,21.,2.4,offline?60.:30.};if(holdCard(a))return;events_.publish(a,seconds());}
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","hotspot_card_shown");
}
// Joins it on a thread of its own (it takes a few seconds): the card says so, and then how it went.
void IslandWindow::joinHotspot(const std::string& peer){
    auto it=hotspots_.find(peer);if(it==hotspots_.end()){shareCard(16,L"The hotspot is off",L"Turn it on on the phone first",{},4);return;}
    if(hotspotBusy_)return;hotspotBusy_=true;const std::wstring ssid=it->second.ssid,key=it->second.key;hotspotSsid_=ssid;
    shareCard(16,L"Joining "+ssid+L"\u2026",peerName(peer)+L"\u2019s hotspot",{},25);store_.log("Info","hotspot_join");
    std::thread([w=window_,ssid,key]{PostMessageW(w,HotspotMessage,WPARAM(IslandWindow::joinNetwork(ssid,key)),0);}).detach();
}
// 0 joined, 1 not in sight, 2 couldn't connect (the password, most likely), 3 no Wi-Fi here, 4 turned on but not ready.
int IslandWindow::joinNetwork(const std::wstring& ssidWide,const std::wstring& key){
    hotspot::Wlan w;if(!w.ok())return 3;
    PWLAN_INTERFACE_INFO_LIST list=nullptr;if(w.interfaces(w.handle,nullptr,&list)!=ERROR_SUCCESS||!list)return 3;
    if(list->dwNumberOfItems==0){w.free(list);return 3;}
    const GUID id=list->InterfaceInfo[0].InterfaceGuid;w.free(list);
    const std::string ssid=hotspot::utf8(ssidWide);if(ssid.empty()||ssid.size()>32)return 1;
    // Wi-Fi off: on first (as the island's Wi-Fi switch would), then a moment for it to wake.
    if(radioOn(1)==0){setRadios(1,true);for(int i=0;i<30&&radioOn(1)!=1;++i)std::this_thread::sleep_for(std::chrono::milliseconds(200));std::this_thread::sleep_for(std::chrono::seconds(2));}
    std::string now;bool named=hotspot::connected(w,id,now);
    if(named&&now==ssid)return 0;
    const bool open=key.empty();
    if(named){
        // A fresh scan, then up to 6 s for the network to show; joined as it's secured.
        DOT11_SSID wanted{};wanted.uSSIDLength=ULONG(ssid.size());std::copy(ssid.begin(),ssid.end(),wanted.ucSSID);
        w.scan(w.handle,&id,&wanted,nullptr,nullptr);
        DOT11_AUTH_ALGORITHM auth=DOT11_AUTH_ALGO_RSNA_PSK;DOT11_CIPHER_ALGORITHM cipher=DOT11_CIPHER_ALGO_CCMP;bool seen=false,refused=false;
        for(int i=0;i<24&&!seen&&!refused;++i){
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            PWLAN_AVAILABLE_NETWORK_LIST nets=nullptr;const DWORD r=w.networks(w.handle,&id,0,nullptr,&nets);
            if(r==ERROR_ACCESS_DENIED){if(nets)w.free(nets);refused=true;break;}
            if(r==DWORD(0x80342002)/*the radio is off*/||r==ERROR_NOT_READY){if(nets)w.free(nets);if(i==23)return 4;continue;}
            if(r!=ERROR_SUCCESS||!nets)continue;
            for(DWORD k=0;k<nets->dwNumberOfItems;++k){const auto& n=nets->Network[k];if(hotspot::sameSsid(n.dot11Ssid,ssid)&&n.dot11BssType==dot11_BSS_type_infrastructure){auth=n.dot11DefaultAuthAlgorithm;cipher=n.dot11DefaultCipherAlgorithm;seen=true;break;}}
            w.free(nets);}
        if(refused)named=false;
        else{
            if(!seen)return 1;
            const int a=int(auth);const bool none=a==DOT11_AUTH_ALGO_80211_OPEN&&cipher==DOT11_CIPHER_ALGO_NONE;
            const wchar_t* authentication=none?L"open":a==9/*WPA3 SAE*/?L"WPA3SAE":a==DOT11_AUTH_ALGO_WPA_PSK?L"WPAPSK":L"WPA2PSK";
            const wchar_t* encryption=none?L"none":cipher==DOT11_CIPHER_ALGO_TKIP?L"TKIP":L"AES";
            return hotspot::attempt(w,id,ssidWide,ssid,key,authentication,encryption,true)?0:2;
        }
    }
    // Without names: as phones secure their hotspots, WPA2 (and WPA2/WPA3) first, then WPA3 alone.
    if(open)return hotspot::attempt(w,id,ssidWide,ssid,key,L"open",L"none",false)?0:2;
    if(hotspot::attempt(w,id,ssidWide,ssid,key,L"WPA2PSK",L"AES",false))return 0;
    return hotspot::attempt(w,id,ssidWide,ssid,key,L"WPA3SAE",L"AES",false)?0:2;
}
void IslandWindow::hotspotMessage(WPARAM w,LPARAM){
    hotspotBusy_=false;const std::wstring ssid=hotspotSsid_;
    switch(int(w)){
    case 0:hotspotJoined_=ssid;shareCard(16,L"Connected to "+ssid,L"The phone\u2019s internet, on this PC",{},4);store_.log("Info","hotspot_joined");break;
    case 1:shareCard(16,L"Can\u2019t see "+ssid+L" from here",L"Bring the phone closer, or check its hotspot name in the app",{},6);store_.log("Info","hotspot_not_seen");break;
    case 3:shareCard(16,L"This PC has no Wi-Fi",L"It can\u2019t join a hotspot",{},5);break;
    case 4:shareCard(16,L"Wi-Fi isn\u2019t ready",L"Try again in a moment",{},5);break;
    default:shareCard(16,L"Couldn\u2019t join "+ssid,L"Bring the phone closer, or check the hotspot\u2019s password in the app",{},6);store_.log("Info","hotspot_failed");break;}
}
}
