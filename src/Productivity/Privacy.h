#pragma once
#include "Common/Win32.h"
#include "Interaction/DashboardModel.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
namespace nexus {
constexpr UINT PrivacyMessage=WM_APP+30;
// ScreenCapture: an app capturing the screen through Windows' capture API (Phase 5E).
enum class Capability { Microphone,Camera,Location,ScreenCapture };
// One app's use of a capability, as Windows' capability access manager records
// it (the records behind Settings > Privacy "Recent activity" and Windows' own
// in-use icons). Start and stop are FILETIMEs; a stop of 0 means still in use.
struct ConsentRecord {Capability capability;std::wstring key;bool packaged=false;uint64_t start=0,stop=0;};
struct PrivacyUse {Capability capability;std::wstring app,key;bool packaged=false;uint64_t since=0;std::shared_ptr<const Artwork> icon;
    bool operator==(const PrivacyUse& o)const{return capability==o.capability&&key==o.key;}};
// Classic programs are recorded under their path with '#' for '\'.
inline std::wstring consentPath(std::wstring key){for(auto& c:key)if(c==L'#')c=L'\\';return key;}
// In use: started, not stopped, and the recorded app is still running (a crashed
// app can leave a record open). `paths` are lower-case executable paths and
// `families` package family names of running processes.
inline bool inUse(const ConsentRecord& r,const std::set<std::wstring>& paths,const std::set<std::wstring>& families){
    if(r.start==0||r.stop!=0)return false;
    std::wstring key=r.packaged?r.key:consentPath(r.key);for(auto& c:key)c=wchar_t(towlower(c));
    return r.packaged?families.contains(key):paths.contains(key);
}
inline const wchar_t* capabilityName(Capability c){return c==Capability::Camera?L"Camera":c==Capability::Microphone?L"Microphone":c==Capability::ScreenCapture?L"Screen capture":L"Location";}
// The dot colours: green camera, orange microphone, purple screen capture, blue location.
inline uint32_t capabilityColour(Capability c){return c==Capability::Camera?0x30d158:c==Capability::Microphone?0xff9f0a:c==Capability::ScreenCapture?0xbf5af2:0x0a84ff;}
inline constexpr Capability capabilityOrder[]={Capability::Camera,Capability::Microphone,Capability::ScreenCapture,Capability::Location};
// Watches HKCU\...\CapabilityAccessManager\ConsentStore for microphone, camera,
// location and screen-capture use. Read-only; nothing is logged or stored.
class PrivacyProvider {
    HWND window_;HANDLE stop_;std::thread worker_;std::mutex mutex_;std::vector<PrivacyUse> uses_;void run();
public:
    explicit PrivacyProvider(HWND);~PrivacyProvider();
    std::vector<PrivacyUse> uses(){std::lock_guard lock(mutex_);return uses_;}
    static std::vector<ConsentRecord> records();
    static std::vector<PrivacyUse> current();
};
}
