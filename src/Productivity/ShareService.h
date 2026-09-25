#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace nexus {
// Phase 5F: sharing between your own PCs on the local network (opt-in, off by default).
// PCs with sharing on announce themselves by UDP broadcast (an id, a port, the protocol version and
// the computer's name). Two PCs pair once: each shows the same six-digit code (derived from both public keys
// and both nonces, the initiator's committed before it sees the other's), and each person
// confirms it. After that, files go only between paired PCs, after the receiver accepts them,
// over TCP encrypted with AES-256-GCM under a key from static ECDH P-256 (both keys checked
// against the pairing) and fresh nonces from both sides. Files land in Downloads.
// Phase 5G (protocol 2): one transfer carries any number of files and folders (a folder keeps its
// tree, under a name of its own in Downloads), either side can stop it, and music can be handed off:
// what plays (title, artist, position), and for a song the island plays from a file, that file.
constexpr UINT ShareMessage=WM_APP+45;
constexpr int shareProtocol=2;
// version: the protocol the PC announced (1: an older Arnav Island that can't take protocol 2 transfers).
struct SharePeer{std::string id;std::wstring name;bool paired=false,online=false;int version=shareProtocol;};
// Music on its way to another PC. file: the song's own file when the island plays it (sent only if asked for).
struct ShareHandoff{std::wstring title,artist,album,app,fileName;double position=0,duration=0;bool playing=true;uint64_t fileSize=0;};
struct ShareEvent{
    // Offer: files to accept (count of them, size in all, file the title: a folder's or a file's name).
    // Progress: bytes so far (outgoing: this PC sends). Received / Sent: how it went; Received's detail is the
    // path to show. Handoff: music offered to this PC (answerHandoff). HandoffAnswered: the other PC's answer
    // (code 1 yes, 2 yes and it takes the file, 0 no). HandoffFile: the song's file arrived (detail its path).
    enum class Kind{Peers,PairCode,Paired,PairFailed,Offer,Progress,Received,Sent,Failed,Handoff,HandoffAnswered,HandoffFile} kind=Kind::Peers;
    std::string peer;std::wstring name,file,detail;uint64_t size=0,done=0;uint32_t code=0,transfer=0,count=0;bool folder=false,outgoing=false;ShareHandoff handoff;
};
// loopback: listen on 127.0.0.1 only (tests; no firewall prompt). handoff: where a handed-off song's file is kept.
struct ShareOptions{uint16_t tcpPort=47820,udpPort=47821;bool discovery=true,loopback=false;std::wstring folder,downloads,name,handoff;};
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
    void addPeer(const std::string& id,const std::wstring& name,const std::string& address,uint16_t port,int version=shareProtocol);
    // Pairing: start one with a peer, then answer the code both PCs show.
    void pair(const std::string& peer);void confirmPair(bool yes);
    void forget(const std::string& peer);
    // Sending files and folders to a paired peer in one transfer (its id, for cancel), and answering an offer from one.
    uint32_t send(const std::string& peer,const std::vector<std::wstring>& paths);
    void answer(uint32_t transfer,bool accept);
    // Stops a transfer either way (the other PC is told it stopped).
    void cancel(uint32_t transfer);
    // Music: offer what plays to a paired peer (file: the song's file, when the island plays one), and answer an offer.
    uint32_t handoff(const std::string& peer,const ShareHandoff& music,const std::wstring& file);
    void answerHandoff(uint32_t transfer,int code);
    std::vector<ShareEvent> take();
    struct Core;
private:
    std::shared_ptr<Core> core_;
};
// A received file's name, made safe: no folders, no reserved names or characters, at most 120 characters.
std::wstring safeShareName(const std::wstring& name);
// A received relative path ("Photos/2024/a.jpg"), each part made safe; "." and ".." parts and empty ones are
// dropped, and at most 24 parts are kept. Empty when nothing is left.
std::vector<std::wstring> safeSharePath(const std::wstring& path);
// "Photos", "notes.txt", or "notes.txt and 2 more", for a transfer of these top-level items.
std::wstring shareTitle(const std::vector<std::wstring>& names);
}
