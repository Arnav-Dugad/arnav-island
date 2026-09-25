#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace nexus {
// Phase 5F: sharing between your own PCs on the local network (opt-in, off by default).
// PCs with sharing on announce themselves by UDP broadcast (an id, a port and the computer's
// name). Two PCs pair once: each shows the same six-digit code (derived from both public keys
// and both nonces, the initiator's committed before it sees the other's), and each person
// confirms it. After that, files go only between paired PCs, after the receiver accepts them,
// over TCP encrypted with AES-256-GCM under a key from static ECDH P-256 (both keys checked
// against the pairing) and fresh nonces from both sides. Files land in Downloads.
constexpr UINT ShareMessage=WM_APP+45;
struct SharePeer{std::string id;std::wstring name;bool paired=false,online=false;};
struct ShareEvent{
    enum class Kind{Peers,PairCode,Paired,PairFailed,Offer,Progress,Received,Sent,Failed} kind=Kind::Peers;
    std::string peer;std::wstring name,file,detail;uint64_t size=0,done=0;uint32_t code=0,transfer=0;
};
// loopback: listen on 127.0.0.1 only (tests; no firewall prompt).
struct ShareOptions{uint16_t tcpPort=47820,udpPort=47821;bool discovery=true,loopback=false;std::wstring folder,downloads,name;};
class ShareService{
public:
    // notify: posted ShareMessage whenever events are waiting (null: poll take()).
    ShareService(HWND notify,ShareOptions options);
    ~ShareService();
    ShareService(const ShareService&)=delete;ShareService& operator=(const ShareService&)=delete;
    // False when the identity or the sockets could not be set up (error() says why).
    bool running()const;std::string error()const;std::string id()const;
    std::vector<SharePeer> peers()const;
    // A peer at a known address (tests; discovery fills these in otherwise).
    void addPeer(const std::string& id,const std::wstring& name,const std::string& address,uint16_t port);
    // Pairing: start one with a peer, then answer the code both PCs show.
    void pair(const std::string& peer);void confirmPair(bool yes);
    void forget(const std::string& peer);
    // Sending a file to a paired peer, and answering an offer from one.
    void send(const std::string& peer,const std::wstring& path);
    void answer(uint32_t transfer,bool accept);
    std::vector<ShareEvent> take();
    struct Core;
private:
    std::shared_ptr<Core> core_;
};
// A received file's name, made safe: no folders, no reserved names or characters, at most 120 characters.
std::wstring safeShareName(const std::wstring& name);
}
