#pragma once
#include "Common/Win32.h"
#include "Design/BrandMatch.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
namespace nexus {
constexpr UINT BluetoothMessage=WM_APP+27;
struct BluetoothDevice {std::wstring name;std::vector<std::wstring> addresses;bool connected=false,audio=false;int battery=-1;uint32_t cod=0,vid=0;int vidSource=0;DeviceKind kind=DeviceKind::Other;std::string brand;};
struct BluetoothEvent {BluetoothDevice device;bool connected=false;};
// Paired Bluetooth devices and their connection changes. Enumeration uses
// SetupAPI device nodes (the same connection and battery properties Windows
// Settings shows); HCI connect/disconnect notifications and device-node changes
// trigger a re-read, and the difference produces events. Nothing is written.
class BluetoothProvider {
    HWND window_,listener_=nullptr;HANDLE stop_;std::thread worker_;std::mutex mutex_;std::vector<BluetoothDevice> devices_;std::vector<BluetoothEvent> events_;
    struct Command{std::wstring name;bool connect;};std::vector<Command> commands_;std::atomic<bool> ready_{false};void run();
public:
    std::atomic<bool> available{false};std::atomic<int> commandResult{0};
    explicit BluetoothProvider(HWND);~BluetoothProvider();
    std::vector<BluetoothDevice> devices(){std::lock_guard lock(mutex_);return devices_;}
    std::vector<BluetoothEvent> takeEvents(){std::lock_guard lock(mutex_);auto e=std::move(events_);events_.clear();return e;}
    // One-shot connect or disconnect of a Bluetooth audio device's endpoint.
    void request(const std::wstring& name,bool connect);
    static std::vector<BluetoothDevice> enumerate();
    static HRESULT audioConnection(const std::wstring& name,bool connect,bool probeOnly=false);
};
// Connection changes between two enumerations, matched by device name.
inline std::vector<BluetoothEvent> bluetoothChanges(const std::vector<BluetoothDevice>& before,const std::vector<BluetoothDevice>& after){
    std::vector<BluetoothEvent> events;for(auto& d:after){auto it=std::find_if(before.begin(),before.end(),[&](auto& b){return folded(b.name)==folded(d.name);});bool was=it!=before.end()&&it->connected;if(d.connected!=was&&(it!=before.end()||d.connected))events.push_back({d,d.connected});}
    for(auto& b:before)if(b.connected&&std::none_of(after.begin(),after.end(),[&](auto& d){return folded(b.name)==folded(d.name);}))events.push_back({b,false});
    return events;
}
}
