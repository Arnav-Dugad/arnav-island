#pragma once
#include "Mirror/MirrorIo.h"
#include "Mirror/ScreenCapture.h"
#include "Mirror/VideoCodec.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nexus::mirror {
// ---- Revision 7 (0.24): screens, either way, on a connection a phone opens (mode V) ----
// The phone's first frame: [0xA0, kind]. Kind 1, this PC's screen to the phone: u16 its largest width and height, u8 fps,
// u32 bits a second (0: as fast as the path allows). Kind 2, the phone's screen to this PC: u16 width, height, u8 fps,
// then its name (u32 length and UTF-8).
// Answered [0xA1, status (0 ok, 1 not allowed, 2 unsupported, 3 failed), u16 width, u16 height, u8 fps, u32 bits a second,
// the encoder's name].
// Then, from whichever side sends the screen: frames [0xA2, flags (1 key, 2 first chunk, 4 last chunk), u32 number,
// u64 time (100 ns), up to 128 KB of Annex B]. From the side that shows it: [0xA3, u32 last number, u16 decode ms, u32 kbps,
// u8 fps] twice a second; [0xA4] asks for a key frame; [0xA5, u16 width, u16 height, u8 fps] new limits (a rotated phone).
// Controls: to this PC, [0xA9, an input frame (0x60 move, 0x61 button, 0x62 scroll, 0x63 text, 0x64 key, 0x65 point)];
// to the phone, [0xA6, action (0 down, 1 move, 2 up), u16 x, u16 y (0-65535 across its screen)], [0xA7, key (1 back,
// 2 home, 3 recent apps, 4 notifications)], [0xA8, text]. [0xAF] ends it, from either side.
constexpr uint8_t frameMirror = 0xA0, frameMirrorReply = 0xA1, frameVideo = 0xA2, frameFeedback = 0xA3, frameKeyframe = 0xA4, frameLimits = 0xA5,
    frameTouch = 0xA6, frameButton = 0xA7, frameText = 0xA8, frameInput = 0xA9, frameStop = 0xAF;
constexpr size_t videoChunk = 128 * 1024;


struct MirrorRequest { int kind = 0, width = 0, height = 0, fps = 0; uint32_t bitrate = 0; std::wstring name; };
bool readRequest(const std::vector<uint8_t>& frame, MirrorRequest& out);

// What a screen comes from: this PC's monitor (ScreenCapture), or a made-up picture (tests).
class FrameSource {
public:
    virtual ~FrameSource() = default;
    virtual bool open() = 0;
    virtual ID3D11Device* device() = 0;
    virtual ScreenCapture::Result next(int ms, ComPtr<ID3D11Texture2D>& frame) = 0;
    virtual int width() = 0; virtual int height() = 0;
    virtual DXGI_MODE_ROTATION rotation() { return DXGI_MODE_ROTATION_IDENTITY; }
    virtual RECT area() { return {0, 0, width(), height()}; }
    virtual std::wstring what() = 0;
};
std::unique_ptr<FrameSource> monitorSource(POINT on = {0, 0});
std::unique_ptr<FrameSource> patternSource(int width, int height);

// What a phone's screen goes to: a window (the island), or a count of frames (tests). begin is told its size first.
class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual ID3D11Device* device() = 0;
    virtual bool begin(int width, int height, const std::wstring& name) = 0;
    virtual void frame(ID3D11Texture2D* nv12, UINT slice, int width, int height) = 0;
    virtual void end() = 0;
    // Input at the window, for the phone (sent by the session as it comes).
    std::function<void(const std::vector<uint8_t>&)> send;
};

// How a screen went: frames sent (or shown), key frames, bytes, the size used and the encoder's name (tests and logs).
struct MirrorReport { int frames = 0, keys = 0, width = 0, height = 0, fps = 0; uint64_t bytes = 0; std::wstring encoder; bool hardware = false;
    // Why it ended: "stopped" (asked here), "their end" (the other side said so), "quiet" (nothing heard), "lost" (the
    // connection broke), "failed" (it couldn't start).
    const char* ended = "failed"; };

// This PC's screen to the phone, until either side ends it (or `stop`). onInput gets the phone's input frames;
// onArea says which part of the virtual desktop the phone sees (for its taps).
MirrorReport sendScreen(MirrorIo& io, const MirrorRequest& r, FrameSource& source, const std::function<void(const std::vector<uint8_t>&)>& onInput,
    const std::function<void(RECT)>& onArea, const std::atomic<bool>& stop);
// The phone's screen to this PC, until either side ends it (or `stop`).
MirrorReport receiveScreen(MirrorIo& io, const MirrorRequest& r, FrameSink& sink, const std::atomic<bool>& stop);
// The answer to a request this PC won't take (status 1 not allowed, 2 unsupported, 3 failed).
std::vector<uint8_t> refusal(int status);
}
