#pragma once
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace nexus {
// 0.20: paired devices reach each other on any network, through a free public MQTT broker (HiveMQ, then EMQX, then
// Eclipse Mosquitto), over TLS. Nothing there can be read or linked to you:
// - Each pair talks on topics named from its own secret: SHA-256 of the pair's static ECDH, which only the two paired
//   devices can compute.
// - Every message is also sealed with AES-256-GCM under a key from that secret. The protocol inside is the same one used
//   on your own network, with its own handshake and encryption.
// A connection through the broker is a tunnel. Its bytes are carried in numbered messages with a window of
// acknowledgements, and the service gets one end of a local socket pair, so the protocol runs through it unchanged.
// Devices say hello every minute (and when they arrive or leave), which is how each knows the other is there.
// Pairing across networks uses a short code shown by one device and typed on the other; the six digits are still
// confirmed on both, as on a local network.
struct RelayPair{std::vector<uint8_t> peer;/* the peer's 16-byte id */ std::vector<uint8_t> agreed;/* SHA-256 of the pair's static ECDH (32 bytes) */};
struct RelayPresence{bool here=false,phone=false;int revision=0;std::wstring name;};
struct RelayOptions{
    std::vector<uint8_t> id;std::wstring name;bool phone=false;int revision=3;
    // A tunnel a paired device opened (peer: its id in hex), or a device pairing with this one's code (peer empty).
    std::function<void(SOCKET inner,const std::string& peer)> incoming;
    // Presence or the connection to the broker changed.
    std::function<void()> changed;
    // Brokers to try in order (host, WebSocket port; path /mqtt). Empty: HiveMQ, EMQX, Eclipse Mosquitto.
    std::vector<std::pair<std::wstring,uint16_t>> brokers;
};
class Relay{
public:
    explicit Relay(RelayOptions options);~Relay();
    Relay(const Relay&)=delete;Relay& operator=(const Relay&)=delete;
    // The paired devices it listens for, and says hello to.
    void pairs(std::vector<RelayPair> list);
    RelayPresence presence(const std::string& peer)const;
    // Connected to a broker (which), and ready for tunnels.
    bool connected()const;std::wstring broker()const;
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
