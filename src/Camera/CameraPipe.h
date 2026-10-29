#pragma once
#include <windows.h>
#include <cstdint>

namespace nexus::camera {
// 0.27: what the island and its camera's media source (ArnavIslandCamera.dll, loaded by Windows' Frame Server) share: the
// source's class, the pipe frames cross (the island is its server and lets the Frame Server's account read it), and a
// frame: this header, then NV12 at the camera's one size (the island fits the phone's picture into it).
constexpr GUID sourceClsid{0x7d3c1a52, 0x4b8e, 0x4f61, {0x9a, 0x27, 0x5e, 0x0b, 0x6c, 0x21, 0xd9, 0x4f}};
constexpr const wchar_t* sourceClsidText = L"{7D3C1A52-4B8E-4F61-9A27-5E0B6C21D94F}";
constexpr const wchar_t* pipeName = L"\\\\.\\pipe\\ArnavIsland.PhoneCamera";
constexpr uint32_t frameWidth = 1280, frameHeight = 720, frameRate = 30, frameBytes = frameWidth * frameHeight * 3 / 2, frameMagic = 0x46434941; // "AICF"
struct FrameHeader { uint32_t magic = frameMagic, width = frameWidth, height = frameHeight, bytes = frameBytes; uint64_t time = 0; };
}
