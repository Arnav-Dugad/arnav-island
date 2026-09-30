#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace nexus::mirror {
// A screen's connection, as the sharing engine hands it over (mode V, revision 7): send is safe from more than one thread;
// receive waits up to `ms`. path: 0 this network, 1 the relay, 2 a direct path; rtt its round trip (ms, 0 unknown).
struct MirrorIo { std::function<bool(const std::vector<uint8_t>&)> send; std::function<bool(std::vector<uint8_t>&, int ms)> receive; int path = 0; double rtt = 0; };
}
