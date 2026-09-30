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
// 0.20 (revision 3, "2.3"): any network. Paired devices reach each other through a public relay when they aren't on the
// same network (ShareRelay.h); a phone can pair from anywhere with a code this PC shows. Also: details of a phone
// (notices), its notifications' actions and replies, the clipboard both ways, input from a phone, finding this PC, the
// song's lyrics, photos for the Shelf.
constexpr UINT ShareMessage=WM_APP+45;
// Revision 4 (0.21): a remote connection stays open for more commands, and so does a notices one (one round trip each,
// with no new handshake); the relay has a direct path. Revision 5 (0.22): a phone controls the whole island (its stats,
// settings, controls, command bar, sound output and pages) and tells it about its hotspot.
constexpr int shareProtocol=2,shareRevision=5;
// Remote commands (mode R) and their answers.
// Revision 3: RingPC (find this PC), Lyrics (the song's lines and word times, when the island has them).
enum class RemoteCommand:uint8_t{Status=1,Media=2,Volume=3,Mute=4,Lock=5,ClipboardGet=6,ClipboardSet=7,Seek=8,Open=9,RingPC=10,Lyrics=11,
    // Revision 5 (0.22): a phone controls the whole island (see the answers below for each one's payloads).
    Stats=12,Settings=13,Controls=14,Command=15,Audio=16,Island=17};
constexpr uint8_t remoteOk=0,remoteNotAllowed=1,remoteUnsupported=2,remoteFailed=3;
// What the island reports to a phone's remote. cover: a small JPEG (at most shareCoverLimit bytes), or empty.
// clipboard (0.20): the island's universal clipboard is on, so the phone sends its own copies over as it opens.
struct RemoteStatus{bool available=false,playing=false,canPrevious=false,canNext=false,canToggle=false,canSeek=false,muted=false,charging=false,batteryPresent=false,clipboard=false;
    double position=0,duration=0;int volume=0,battery=-1,cpu=-1;std::wstring title,artist,app,name,weather;std::vector<uint8_t> cover;};
// version: the protocol the PC announced (1: an older Arnav Island that can't take protocol 2 transfers); revision: the
// revision within it (0 before Phase 5H).
// phone: Arnav Island for Android; battery (-1 unknown) and charging: as the phone last said.
// viaInternet: here only through the relay (not on this network).
// 0.21: path (reached over the internet): 1 through the relay, 2 directly; rtt its round trip (ms, 0 unknown); relays how
// many brokers it's heard on; v6 a direct path over IPv6.
struct SharePeer{std::string id;std::wstring name;bool paired=false,online=false;int version=shareProtocol,revision=0;bool phone=false;int battery=-1;bool charging=false;bool viaInternet=false;
    int path=0;double rtt=0;int relays=0;bool v6=false;};
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
    // Revision 3: PairingCode (detail the code to show, empty when one couldn't be made); PhoneDetails (detail: the phone's
    // readings, one "name<TAB>value" a line); PhoneNoticeGone (key: a notification the phone no longer shows). A
    // PhoneNotice carries its key and actions (their titles, and whether each takes a reply). An Offer or Received with
    // toShelf: photos a phone took for this PC's Shelf.
    enum class Kind{Peers,PairCode,Paired,PairFailed,Offer,Progress,Received,Sent,Failed,Handoff,HandoffAnswered,HandoffFile,ShelfList,ShelfTaken,PhoneStatus,PhoneNotice,Rang,PairingCode,PhoneDetails,PhoneNoticeGone,PhoneHotspot} kind=Kind::Peers;
    std::string peer;std::wstring name,file,detail;uint64_t size=0,done=0;uint32_t code=0,transfer=0,count=0;bool folder=false,outgoing=false;ShareHandoff handoff;std::vector<ShareShelfItem> shelf;
    std::wstring app;std::vector<uint8_t> icon;int battery=-1;bool charging=false,urgent=false;
    std::string key;std::vector<std::pair<std::wstring,bool>> actions;bool toShelf=false;std::vector<std::wstring> paths;
};
// loopback: listen on 127.0.0.1 only (tests; no firewall prompt). handoff: where a handed-off song's file is kept.
// remote: answers a phone's remote command (called on a network thread): its answer byte, then its payload. Unset: the
// commands are unsupported.
// relay: reach paired devices on other networks through the public relay (relayBrokers, relayLoseEvery: tests; empty and
// 0 for the usual ones).
// directLoopback, directExtra (tests): the relay's direct path on the loopback only, and addresses to offer as well.
struct ShareOptions{uint16_t tcpPort=47820,udpPort=47821;bool discovery=true,loopback=false,relay=false;std::wstring folder,downloads,name,handoff;std::vector<std::pair<std::wstring,uint16_t>> relayBrokers;int relayLoseEvery=0;
    bool direct=true,directLoopback=false;std::vector<std::string> directExtra;
    std::function<std::vector<uint8_t>(const std::string& peer,RemoteCommand command,const std::vector<uint8_t>& payload)> remote;
    // Revision 3: a phone's trackpad and keyboard, one frame at a time (0x60 move, 0x61 button, 0x62 scroll, 0x63 text,
    // 0x64 key), called on a network thread.
    std::function<void(const std::string& peer,const std::vector<uint8_t>& frame)> input;};
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
    // 0.20: pairing from anywhere. hostPairing offers a code for ten minutes (a PairingCode event says which); the device
    // that types it pairs as on a local network (PairCode, then Paired). pairWithCode is the other side of it.
    void hostPairing();void stopPairing();std::string pairingCode()const;void pairWithCode(const std::string& code);
    // 0.20.1: the link a pairing QR code carries, "arnavisland://pair/<code>?k=<fingerprint>": the fingerprint (the first
    // ten bytes of the SHA-256 of this PC's public key, in hex) lets the phone that scans it know it reached this PC, so it
    // pairs without asking a second time. Empty for an empty code.
    std::string pairingLink(const std::string& code)const;
    // Revision 3, to a paired phone: one of its notification's actions (index; reply: the text for one that takes it),
    // the clipboard (sensitive: marked so on the phone), and asking it for a photo for the Shelf. Failures come as Failed.
    void noticeAction(const std::string& peer,const std::string& key,int action,const std::wstring& reply);
    void pushClipboard(const std::string& peer,const std::wstring& text,bool sensitive);
    void askPhoto(const std::string& peer);
    // Tests: the relay's direct path goes quiet, as when a network drops it.
    void stopDirect();
    // Connected to the relay (and through which broker).
    bool internet()const;std::wstring relayBroker()const;
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
// Revision 3: the song's lyrics for a phone. state: 0 lyrics are off here, 1 being looked for, 2 found, 3 none found; key:
// which song ("title<TAB>artist"); each line's start (seconds), text and its words' starts (seconds, and where in the
// text each begins, in UTF-16 units). At most 400 lines.
struct ShareLyricLine{double time=0;std::wstring text;std::vector<std::pair<double,uint32_t>> words;};
std::vector<uint8_t> remoteLyricsAnswer(int state,const std::wstring& key,const std::vector<ShareLyricLine>& lines);
// ---- Revision 5 (0.22): a phone controls the whole island ----
// All little-endian; a string is a u32 length and UTF-8. Each answer starts with remoteOk and a version byte (1).
// Stats (no payload): the PC's numbers live, their last 40 samples (CPU and GPU in percent, 255 unknown; downloads in
// bytes a second) and what the PC is.
struct PcStats{double cpu=-1,gpu=-1,ramUsedGiB=0,ramTotalGiB=0,ramPercent=0,diskUsedPercent=-1,diskFreeGiB=0,diskTotalGiB=0,download=0,upload=0,batteryMinutes=-1;
    uint64_t uptime=0;unsigned logical=0;int battery=-1;bool charging=false;std::vector<float> cpuHistory,gpuHistory,downloadHistory;std::wstring name,model,os,cpuName,gpuName;};
std::vector<uint8_t> remoteStatsAnswer(const PcStats&);
// Settings: payload [0] reads every setting (its section, control, key, title, detail, range, value, the action a button
// runs, its unit, options and colours, for swatches); [1, key, i32] sets one (answered with the value it has now); [2, u8]
// runs a settings button's action. control: 0 switch, 1 slider, 2 choice, 3 stepper, 4 swatch, 5 button.
struct RemoteSetting{int section=0,control=0;std::string key;std::wstring title,detail,unit;int lo=0,hi=1,step=1,value=0,action=0;std::vector<std::wstring> options;std::vector<uint32_t> colours;};
std::vector<uint8_t> remoteSettingsAnswer(const std::vector<std::wstring>& sections,const std::vector<RemoteSetting>& items);
std::vector<uint8_t> remoteValueAnswer(int value);
// Controls: payload [0] reads them; [1, control, i32] changes one: 1 Wi-Fi, 2 Bluetooth, 3 dark mode, 4 airplane mode (1
// every radio off), 5 brightness, 6 volume, 7 mute, 8 microphone muted, 9 lock, 10 sleep, 11 restart, 12 shut down,
// 13 empty the recycle bin, 15 focus for N minutes, 16 a break of N minutes, 17 pause or resume, 18 reset, 19 stopwatch.
// Radios and dark mode are 1 on, 0 off, -1 unknown, -2 none; busy: switches changing (1 Wi-Fi, 2 Bluetooth, 4 radios, 8 dark).
struct PcControls{int wifi=-1,bluetooth=-1,dark=-1,brightness=-1,volume=0,busy=0,focusMode=0;bool muted=false,micAvailable=false,micMuted=false,focusRunning=false,focusFinished=false;double focusDuration=0,focusShown=0;};
std::vector<uint8_t> remoteControlsAnswer(const PcControls&);
// Command: payload [0, text] asks the command bar (final: these results answer that text; ask again until then);
// [1, text, index, title, confirmed] runs one, answered by an outcome (0 done, 1 needs a yes first, 2 failed, 3 the
// results have changed) and what happened.
struct RemoteResult{int kind=0;bool confirm=false;std::wstring title,detail,answer;};
std::vector<uint8_t> remoteCommandAnswer(bool final,const std::vector<RemoteResult>&);
std::vector<uint8_t> remoteOutcome(int outcome,const std::wstring& message);
// Audio: payload [0] lists the outputs; [1, id] makes one the default.
struct RemoteOutput{std::wstring id,name;bool current=false;int form=-1;};
std::vector<uint8_t> remoteAudioAnswer(const std::vector<RemoteOutput>&);
// Island: payload [0, page] opens one of its pages on the PC (Home, Media, Stats, Focus, Settings, Shelf, Audio,
// Controls); [1] closes it.
// A request's payload, read in order; any read past its end fails (and so does everything after).
class RemoteReader{const std::vector<uint8_t>& b_;size_t at_=0;bool ok_=true;
public:
    explicit RemoteReader(const std::vector<uint8_t>& b):b_(b){}
    bool ok()const{return ok_;}
    int u8(){if(!ok_||at_+1>b_.size()){ok_=false;return 0;}return b_[at_++];}
    int i32(){if(!ok_||at_+4>b_.size()){ok_=false;return 0;}uint32_t v=0;for(int k=0;k<4;++k)v|=uint32_t(b_[at_+size_t(k)])<<(8*k);at_+=4;return int(v);}
    std::string bytes(size_t limit){const int n=i32();if(!ok_||n<0||size_t(n)>limit||at_+size_t(n)>b_.size()){ok_=false;return {};}std::string s(b_.begin()+long(at_),b_.begin()+long(at_+size_t(n)));at_+=size_t(n);return s;}
    std::wstring text(size_t limit);
};
// "Photos", "notes.txt", or "notes.txt and 2 more", for a transfer of these top-level items.
std::wstring shareTitle(const std::vector<std::wstring>& names);
}
