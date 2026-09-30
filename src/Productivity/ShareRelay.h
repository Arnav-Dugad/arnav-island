#pragma once
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace nexus {
// 0.20: paired devices reach each other on any network, through free public MQTT brokers (HiveMQ, EMQX and Eclipse
// Mosquitto), over TLS. 0.20.1: every broker at once, so two devices always share one. Nothing there can be read or
// linked to you:
// - Each pair talks on topics named from its own secret: SHA-256 of the pair's static ECDH, which only the two paired
//   devices can compute.
// - Every message is also sealed with AES-256-GCM under a key from that secret. The protocol inside is the same one used
//   on your own network, with its own handshake and encryption.
// A connection through the broker is a tunnel. Its bytes are carried in numbered messages with a window of
// acknowledgements (0.20.1: what a broker drops is sent again), and the service gets one end of a local socket pair, so
// the protocol runs through it unchanged.
// Devices say hello every minute (and when they arrive or leave), which is how each knows the other is there.
// 0.21: a direct path. The hellos also carry each device's addresses (its global IPv6 addresses, its LAN IPv4 addresses,
// and the public address STUN servers see); both then send sealed UDP probes to all of them at once, which opens each
// side's NAT and firewall for the other (hole punching). A probe answered is a path: new tunnels go straight there, in
// small datagrams paced by the acknowledgements, and fall back to the brokers if it goes quiet.
// Pairing across networks uses a short code shown by one device and typed on the other; the six digits are still
// confirmed on both, as on a local network.
struct RelayPair{std::vector<uint8_t> peer;/* the peer's 16-byte id */ std::vector<uint8_t> agreed;/* SHA-256 of the pair's static ECDH (32 bytes) */};
struct RelayPresence{bool here=false,phone=false;int revision=0;std::wstring name;};
// How a paired device is reached now: kind 0 not at all, 1 through the brokers, 2 directly; rtt the round trip that way
// (milliseconds, 0 unknown); brokers how many brokers it was heard on; v6 a direct path over IPv6.
struct RelayPath{int kind=0;double rtt=0;int brokers=0;bool v6=false;};
struct RelayOptions{
    std::vector<uint8_t> id;std::wstring name;bool phone=false;int revision=3;
    // A tunnel a paired device opened (peer: its id in hex), or a device pairing with this one's code (peer empty).
    std::function<void(SOCKET inner,const std::string& peer)> incoming;
    // Presence or the connection to the broker changed.
    std::function<void()> changed;
    // Brokers to stay on (host, WebSocket port; path /mqtt). Empty: HiveMQ, EMQX, Eclipse Mosquitto.
    std::vector<std::pair<std::wstring,uint16_t>> brokers;
    // Tests: the first sending of every Nth data message is left out (as a broker might drop it), 0 none.
    int loseEvery=0;
    // 0.21: the direct path (directLoopback, tests: on this PC's loopback only; directExtra: more addresses to offer, as
    // written, for an emulator that reaches this PC at 10.0.2.2).
    bool direct=true,directLoopback=false;std::vector<std::string> directExtra;
};
class Relay{
public:
    explicit Relay(RelayOptions options);~Relay();
    Relay(const Relay&)=delete;Relay& operator=(const Relay&)=delete;
    // The paired devices it listens for, and says hello to.
    void pairs(std::vector<RelayPair> list);
    RelayPresence presence(const std::string& peer)const;
    // Connected to at least one broker (and which, and how many), and ready for tunnels.
    bool connected()const;std::wstring broker()const;int brokersUp()const;
    // 0.21: how a paired device is reached; and (after a network change) every address looked at again.
    RelayPath path(const std::string& peer)const;void networkChanged();
    // Tests: the direct path goes quiet (its sockets close), as when a network drops it.
    void stopDirect();
    // A tunnel to a paired device that is here; INVALID_SOCKET (why says why) otherwise. The caller owns the socket.
    SOCKET open(const std::string& peer,std::wstring& why);
    // Offers a pairing code for ten minutes ("7K2P-MX4Q"); the device that types it arrives through `incoming` with an
    // empty peer. Empty when the broker can't be reached.
    std::string host();void stopHosting();std::string hosting()const;
    // A tunnel to the device showing this code.
    SOCKET openCode(const std::string& code,std::wstring& why);
    struct Core;
private:
    std::shared_ptr<Core> core_;
};
// A typed code made canonical ("7k2p mx4q" becomes "7K2PMX4Q"), or empty when it can't be one.
std::string relayCode(const std::string& typed);
// Two connected sockets on this PC's loopback (for a tunnel's two ends); false when they couldn't be made.
bool loopbackPair(SOCKET& a,SOCKET& b);
}
