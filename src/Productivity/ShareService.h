#pragma once
#include <windows.h>
#include <cstdint>
#include <functional>
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
// Phase 5H (revision 1 of protocol 2, announced as "2.1"; older PCs read only the 2): a music offer carries the
// song's cover, and a paired PC can look into this PC's Shelf and take a file or folder from it (while this PC
// lets it: the "shelfOpen" setting). Revision 0 PCs are never asked for either.
// 0.19 (revision 2, announced "2.2"): phones. Arnav Island for Android pairs like a PC and adds ";phone" to its
// announcement; it can ask a paired PC for its status and control it (mode R: now playing with the cover, media keys,
// volume, mute, lock, clipboard both ways, seeking, opening a link), send the phone's battery and notifications to
// the island (mode N), and be rung from the island (mode F, find my phone).
constexpr UINT ShareMessage=WM_APP+45;
constexpr int shareProtocol=2,shareRevision=2;
// Remote commands (mode R) and their answers.
enum class RemoteCommand:uint8_t{Status=1,Media=2,Volume=3,Mute=4,Lock=5,ClipboardGet=6,ClipboardSet=7,Seek=8,Open=9};
constexpr uint8_t remoteOk=0,remoteNotAllowed=1,remoteUnsupported=2,remoteFailed=3;
// What the island reports to a phone's remote. cover: a small JPEG (at most shareCoverLimit bytes), or empty.
struct RemoteStatus{bool available=false,playing=false,canPrevious=false,canNext=false,canToggle=false,canSeek=false,muted=false,charging=false,batteryPresent=false;
    double position=0,duration=0;int volume=0,battery=-1,cpu=-1;std::wstring title,artist,app,name,weather;std::vector<uint8_t> cover;};
// version: the protocol the PC announced (1: an older Arnav Island that can't take protocol 2 transfers); revision: the
// revision within it (0 before Phase 5H).
// phone: Arnav Island for Android; battery (-1 unknown) and charging: as the phone last said.
struct SharePeer{std::string id;std::wstring name;bool paired=false,online=false;int version=shareProtocol,revision=0;bool phone=false;int battery=-1;bool charging=false;};
// Music on its way to another PC. file: the song's own file when the island plays it (sent only if asked for).
// cover: the song's cover as a small JPEG (at most shareCoverLimit bytes; revision 1).
constexpr size_t shareCoverLimit=96*1024;
struct ShareHandoff{std::wstring title,artist,album,app,fileName;double position=0,duration=0;bool playing=true;uint64_t fileSize=0;std::vector<uint8_t> cover;};
// A file or folder on a Shelf, as another PC sees it (preview: a small JPEG, or empty).
struct ShareShelfItem{std::wstring name;uint64_t size=0;bool folder=false;std::vector<uint8_t> preview;};
// What this PC's Shelf offers: a file or folder's path and its preview (a small JPEG, or empty).
struct ShareShelfEntry{std::wstring path;std::vector<uint8_t> preview;};
struct ShareEvent{
    // Offer: files to accept (count of them, size in all, file the title: a folder's or a file's name).
    // Progress: bytes so far (outgoing: this PC sends). Received / Sent: how it went; Received's detail is the
    // path to show. Handoff: music offered to this PC (answerHandoff). HandoffAnswered: the other PC's answer
    // (code 1 yes, 2 yes and it takes the file, 0 no). HandoffFile: the song's file arrived (detail its path).
    // ShelfList: a paired PC's Shelf (shelf; code 0 shown, 1 that PC keeps its Shelf to itself, 2 it couldn't be asked:
    // detail says why). ShelfTaken: a paired PC took `file` from this PC's Shelf. Received with code 1: the files
    // were taken from the other PC's Shelf (not offered by it).
    // Revision 2: PhoneStatus (battery, charging), PhoneNotice (a phone's notification: app, file its title, detail its
    // text, icon a small PNG, urgent for calls), Rang (a phone answered find-my-phone).
    enum class Kind{Peers,PairCode,Paired,PairFailed,Offer,Progress,Received,Sent,Failed,Handoff,HandoffAnswered,HandoffFile,ShelfList,ShelfTaken,PhoneStatus,PhoneNotice,Rang} kind=Kind::Peers;
    std::string peer;std::wstring name,file,detail;uint64_t size=0,done=0;uint32_t code=0,transfer=0,count=0;bool folder=false,outgoing=false;ShareHandoff handoff;std::vector<ShareShelfItem> shelf;
    std::wstring app;std::vector<uint8_t> icon;int battery=-1;bool charging=false,urgent=false;
};
// loopback: listen on 127.0.0.1 only (tests; no firewall prompt). handoff: where a handed-off song's file is kept.
// remote: answers a phone's remote command (called on a network thread): its answer byte, then its payload. Unset: the
// commands are unsupported.
struct ShareOptions{uint16_t tcpPort=47820,udpPort=47821;bool discovery=true,loopback=false;std::wstring folder,downloads,name,handoff;
    std::function<std::vector<uint8_t>(const std::string& peer,RemoteCommand command,const std::vector<uint8_t>& payload)> remote;};
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
    void addPeer(const std::string& id,const std::wstring& name,const std::string& address,uint16_t port,int version=shareProtocol,int revision=shareRevision);
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
    // Phase 5H: this PC's Shelf as paired PCs may see and take it (open false: they are told it is kept here).
    void offerShelf(std::vector<ShareShelfEntry> items,bool open);
    // A paired PC's Shelf (answered by a ShelfList event), and one of its items (by its place in that list and its
    // name) taken into Downloads: Progress, then Received with code 1, or Failed. Returns the transfer's id.
    void askShelf(const std::string& peer);
    uint32_t takeFromShelf(const std::string& peer,uint32_t index,const std::wstring& name);
    // Revision 2: rings a paired phone (Rang when it answered, or Failed).
    void ring(const std::string& peer);
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
// The remote's status answer (remoteOk, then the payload the phone reads): the cover is sent only when its SHA-256
// differs from haveCover, the one the phone already shows.
std::vector<uint8_t> remoteStatusAnswer(const RemoteStatus& status,const std::vector<uint8_t>& haveCover);
// "Photos", "notes.txt", or "notes.txt and 2 more", for a transfer of these top-level items.
std::wstring shareTitle(const std::vector<std::wstring>& names);
}
