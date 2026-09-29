#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace nexus {
// 0.20.1: a QR code (ISO/IEC 18004) for a short text, such as the link that pairs a phone from anywhere. Byte mode,
// error correction level M, the smallest version from 1 to 10 that holds it, and the best of the eight masks.
struct QrCode{
    int size=0;std::vector<uint8_t> modules;
    bool dark(int x,int y)const{return x>=0&&y>=0&&x<size&&y<size&&modules[size_t(y)*size_t(size)+size_t(x)]!=0;}
};
// The code for `text` (UTF-8), or an empty one (size 0) when it is too long. mask: -1 chooses the best; 0..7 forces one
// (tests compare each with a reference encoder).
QrCode qrEncode(const std::string& text,int mask=-1);
}
