#include "Mirror/MirrorHost.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

namespace nexus::mirror {
namespace {
using Clock = std::chrono::steady_clock;
using Bytes = std::vector<uint8_t>;
void put16(Bytes& b, unsigned v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
void put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }
void put64(Bytes& b, uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(uint8_t(v >> (8 * i))); }
unsigned get16(const uint8_t* p) { return unsigned(p[0]) | unsigned(p[1]) << 8; }
uint32_t get32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint64_t get64(const uint8_t* p) { uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (8 * i); return v; }
std::string utf8(const std::wstring& w) { if (w.empty()) return {}; const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr); std::string s(size_t(std::max(n, 0)), '\0'); if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr); return s; }
std::wstring wide(const std::string& s) { if (s.empty()) return {}; const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0); std::wstring w(size_t(std::max(n, 0)), L'\0'); if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n); return w; }
double since(Clock::time_point t) { return std::chrono::duration<double>(Clock::now() - t).count(); }

// ---- sources ----
class MonitorSource : public FrameSource {
    ScreenCapture capture_; POINT on_;
public:
    explicit MonitorSource(POINT on) : on_(on) {}
    bool open() override { return capture_.open(on_); }
    ID3D11Device* device() override { return capture_.device(); }
    ScreenCapture::Result next(int ms, ComPtr<ID3D11Texture2D>& frame) override { return capture_.next(ms, frame); }
    int width() override { return capture_.width(); }
    int height() override { return capture_.height(); }
    DXGI_MODE_ROTATION rotation() override { return capture_.rotation(); }
    RECT area() override { return capture_.area(); }
    std::wstring what() override { return L"monitor"; }
};
// A made-up moving picture (tests): a soft gradient that drifts, a bar that sweeps across and a counter of squares.
class PatternSource : public FrameSource {
    int w_, h_; ComPtr<ID3D11Device> device_; ComPtr<ID3D11DeviceContext> context_; ComPtr<ID3D11Texture2D> texture_; std::vector<uint8_t> pixels_; int n_ = 0; Clock::time_point last_{};
public:
    PatternSource(int w, int h) : w_(w & ~1), h_(h & ~1) {}
    bool open() override {
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT, levels, 2, D3D11_SDK_VERSION, device_.GetAddressOf(), nullptr, context_.GetAddressOf()))) return false;
        ComPtr<ID3D10Multithread> threads; if (SUCCEEDED(device_.As(&threads))) threads->SetMultithreadProtected(TRUE);
        D3D11_TEXTURE2D_DESC t{}; t.Width = UINT(w_); t.Height = UINT(h_); t.MipLevels = 1; t.ArraySize = 1; t.Format = DXGI_FORMAT_B8G8R8A8_UNORM; t.SampleDesc.Count = 1; t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        pixels_.resize(size_t(w_) * h_ * 4); return SUCCEEDED(device_->CreateTexture2D(&t, nullptr, texture_.GetAddressOf()));
    }
    ID3D11Device* device() override { return device_.Get(); }
    ScreenCapture::Result next(int ms, ComPtr<ID3D11Texture2D>& frame) override {
        const double wait = 1.0 / 60 - since(last_); if (wait > 0) { if (wait * 1000 > ms) return ScreenCapture::Result::Timeout; std::this_thread::sleep_for(std::chrono::duration<double>(wait)); }
        last_ = Clock::now(); ++n_;
        const double t = n_ / 60.0; const int bar = int(std::fmod(t * 0.25, 1.0) * w_);
        for (int y = 0; y < h_; ++y) for (int x = 0; x < w_; ++x) {
            uint8_t* p = &pixels_[(size_t(y) * w_ + x) * 4]; const double u = double(x) / w_, v = double(y) / h_;
            p[0] = uint8_t(120 + 100 * std::sin(u * 3 + t)); p[1] = uint8_t(90 + 80 * std::sin(v * 4 - t * .7)); p[2] = uint8_t(60 + 50 * std::sin((u + v) * 2 + t * .4)); p[3] = 255;
            if (std::abs(x - bar) < 6) p[0] = p[1] = p[2] = 245;
        }
        // The frame's number in squares along the top, so a viewer can tell frames apart.
        for (int b = 0; b < 12; ++b) if (n_ >> b & 1) for (int y = 8; y < 28; ++y) for (int x = 8 + b * 26; x < 28 + b * 26 && x < w_; ++x) { uint8_t* p = &pixels_[(size_t(y) * w_ + x) * 4]; p[0] = p[1] = p[2] = 255; }
        context_->UpdateSubresource(texture_.Get(), 0, nullptr, pixels_.data(), UINT(w_ * 4), 0);
        frame = texture_; return ScreenCapture::Result::Frame;
    }
    int width() override { return w_; }
    int height() override { return h_; }
    std::wstring what() override { return L"pattern"; }
};
}
std::unique_ptr<FrameSource> monitorSource(POINT on) { return std::make_unique<MonitorSource>(on); }
std::unique_ptr<FrameSource> patternSource(int width, int height) { return std::make_unique<PatternSource>(width, height); }

bool readRequest(const Bytes& f, MirrorRequest& r) {
    if (f.size() < 7 || f[0] != frameMirror) return false;
    r.kind = f[1]; r.width = int(get16(&f[2])); r.height = int(get16(&f[4])); r.fps = f[6];
    if (r.kind == 1) { if (f.size() < 11) return false; r.bitrate = get32(&f[7]); return true; }
    if (r.kind == 2) { if (f.size() < 11) return false; const uint32_t n = get32(&f[7]); if (n > 1024 || 11 + size_t(n) > f.size()) return false; r.name = wide(std::string(f.begin() + 11, f.begin() + long(11 + n))); return r.width > 0 && r.height > 0; }
    return false;
}
Bytes refusal(int status) { Bytes b{frameMirrorReply, uint8_t(status)}; put16(b, 0); put16(b, 0); b.push_back(0); put32(b, 0); put32(b, 0); return b; }

// ---- this PC's screen to the phone ----
MirrorReport sendScreen(MirrorIo& io, const MirrorRequest& r, FrameSource& source, const std::function<void(const Bytes&)>& onInput, const std::function<void(RECT)>& onArea, const std::atomic<bool>& stop) {
    MirrorReport report;
    if (!source.open()) { io.send(refusal(3)); return report; }
    // What the path allows: on this network the most (up to 1440p at 60 fps), directly a lot, through the relay a little.
    struct Cap { int longest, fps; int start, most, least; };
    const Cap cap = io.path == 0 ? Cap{2560, 60, 16'000'000, 40'000'000, 2'000'000} : io.path == 2 ? Cap{1920, 60, 6'000'000, 16'000'000, 800'000} : Cap{1280, 20, 700'000, 1'800'000, 250'000};
    const bool turned = source.rotation() == DXGI_MODE_ROTATION_ROTATE90 || source.rotation() == DXGI_MODE_ROTATION_ROTATE270;
    const int sw = turned ? source.height() : source.width(), sh = turned ? source.width() : source.height();
    // The phone says its largest picture (its screen's pixels, long side first).
    auto fit = [&](int limitW, int limitH) { const double s = std::min({1.0, double(std::min(limitW, cap.longest)) / std::max(sw, sh), double(limitH) / std::min(sw, sh)}); return std::pair{std::max(16, int(sw * s) & ~1), std::max(16, int(sh * s) & ~1)}; };
    auto [w, h] = fit(r.width > 0 ? r.width : 1920, r.height > 0 ? r.height : 1080);
    int fps = std::clamp(r.fps > 0 ? int(r.fps) : 60, 5, cap.fps); int bitrate = r.bitrate > 0 ? std::clamp(int(r.bitrate), cap.least, cap.most) : cap.start;
    VideoEncoder encoder; if (!encoder.open(source.device(), w, h, fps, bitrate)) { io.send(refusal(3)); return report; }
    report.width = w; report.height = h; report.fps = fps; report.encoder = encoder.name(); report.hardware = encoder.hardware();
    { Bytes reply{frameMirrorReply, 0}; put16(reply, unsigned(w)); put16(reply, unsigned(h)); reply.push_back(uint8_t(fps)); put32(reply, uint32_t(bitrate)); const std::string n = utf8(encoder.name()); put32(reply, uint32_t(n.size())); reply.insert(reply.end(), n.begin(), n.end());
      if (!io.send(reply)) return report; }
    if (onArea) onArea(source.area());
    // What the phone says, on a thread of its own: how it's keeping up, key frames it wants, its input, its end.
    std::atomic<bool> done{false}, wantKey{false}, heard{false}; std::atomic<uint32_t> acked{0}; std::atomic<int> theirKbps{0}; std::atomic<const char*> ended{"stopped"};
    std::thread reader([&] {
        int silent = 0; Bytes f;
        while (!done && !stop) {
            if (!io.receive(f, 1000)) { if (++silent >= 8) { ended = "quiet"; done = true; } continue; }
            silent = 0; if (f.empty()) continue;
            switch (f[0]) {
            case frameFeedback: if (f.size() >= 12) { acked = get32(&f[1]); theirKbps = int(get32(&f[7])); heard = true; } break;
            case frameKeyframe: wantKey = true; break;
            case frameInput: if (f.size() >= 2 && onInput) onInput(Bytes(f.begin() + 1, f.end())); break;
            case frameStop: ended = "their end"; done = true; break;
            default: break;
            }
        }
    });
    uint32_t number = 0; auto started = Clock::now(), lastSent = Clock::now() - std::chrono::seconds(1), lastUp = Clock::now(), lastCheck = Clock::now(), lastOut = Clock::now();
    ComPtr<ID3D11Texture2D> last; bool pending = false; int slowSends = 0; uint64_t bytesSince = 0; std::vector<VideoEncoder::Frame> out;
    auto sendFrame = [&](VideoEncoder::Frame& f) -> bool {
        ++number; const auto t0 = Clock::now();
        for (size_t at = 0; at < f.data.size() || at == 0;) {
            const size_t n = std::min(videoChunk, f.data.size() - at);
            Bytes b{frameVideo, uint8_t((f.key ? 1 : 0) | (at == 0 ? 2 : 0) | (at + n >= f.data.size() ? 4 : 0))}; put32(b, number); put64(b, uint64_t(f.pts));
            b.insert(b.end(), f.data.begin() + long(at), f.data.begin() + long(at + n)); if (!io.send(b)) return false; at += n; if (f.data.empty()) break;
        }
        ++report.frames; if (f.key) ++report.keys; report.bytes += f.data.size(); bytesSince += f.data.size();
        // A send that takes longer than a frame's time means the path is full.
        if (since(t0) > 1.5 / fps) ++slowSends; else slowSends = std::max(0, slowSends - 1);
        return true;
    };
    while (!done && !stop) {
        ComPtr<ID3D11Texture2D> frame; const auto got = source.next(50, frame);
        if (got == ScreenCapture::Result::Lost) { std::this_thread::sleep_for(std::chrono::milliseconds(300)); if (source.open()) { if (onArea) onArea(source.area()); wantKey = true; } continue; }
        if (got == ScreenCapture::Result::Frame) { last = frame; pending = true; }
        // A still screen sends no frames: a word every second tells the phone this PC is still here.
        if (since(lastOut) >= 1.0) { Bytes alive{frameLimits}; put16(alive, unsigned(w)); put16(alive, unsigned(h)); alive.push_back(uint8_t(fps)); if (!io.send(alive)) { ended = "lost"; break; } lastOut = Clock::now(); }
        // The phone asked for a key frame: the last picture again, whole.
        if (wantKey.exchange(false)) { encoder.keyframe(); if (last) pending = true; }
        // At most `fps` frames a second, and none while the phone is a second of frames behind.
        const bool behind = heard && number > acked + uint32_t(fps);
        if (!pending || !last || since(lastSent) < 1.0 / fps || behind) continue;
        pending = false; lastSent = Clock::now(); out.clear();
        if (!encoder.encode(last.Get(), source.rotation(), int64_t(since(started) * 1e7), out)) { encoder.open(source.device(), w, h, fps, bitrate); continue; }
        for (auto& f : out) if (!sendFrame(f)) { ended = "lost"; done = true; break; }
        lastOut = Clock::now();
        // The bit rate follows the path: down a quarter when sends are slow or the phone falls behind, up a little after
        // four easy seconds.
        if (since(lastCheck) >= .5) {
            lastCheck = Clock::now();
            if (slowSends >= 3 || behind) { bitrate = std::max(cap.least, bitrate * 3 / 4); encoder.bitrate(bitrate); slowSends = 0; lastUp = Clock::now(); }
            else if (since(lastUp) >= 4 && bitrate < cap.most) { bitrate = std::min(cap.most, bitrate * 115 / 100); encoder.bitrate(bitrate); lastUp = Clock::now(); }
            bytesSince = 0;
        }
    }
    done = true; io.send(Bytes{frameStop}); reader.join(); report.ended = ended.load();
    return report;
}

// ---- the phone's screen to this PC ----
MirrorReport receiveScreen(MirrorIo& io, const MirrorRequest& r, FrameSink& sink, const std::atomic<bool>& stop) {
    MirrorReport report; report.width = r.width; report.height = r.height; report.fps = r.fps;
    VideoDecoder decoder;
    if (!sink.begin(r.width, r.height, r.name) || !decoder.open(sink.device())) { io.send(refusal(3)); sink.end(); return report; }
    { Bytes reply{frameMirrorReply, 0}; put16(reply, unsigned(r.width)); put16(reply, unsigned(r.height)); reply.push_back(uint8_t(r.fps)); put32(reply, 0); put32(reply, 0); if (!io.send(reply)) { sink.end(); return report; } }
    sink.send = [&](const Bytes& b) { io.send(b); };
    Bytes f, frame; uint32_t current = 0, last = 0; bool waitingKey = true; int silent = 0; auto told = Clock::now(); uint64_t bytesSince = 0; double decodeMs = 0; int shown = 0;
    // How it's keeping up, twice a second, whether or not frames come (a still screen sends none).
    auto tell = [&] {
        if (since(told) < .5) return;
        Bytes fb{frameFeedback}; put32(fb, last); put16(fb, unsigned(std::min(65535.0, decodeMs))); put32(fb, uint32_t(bytesSince * 8 / 1000 / std::max(.001, since(told))));
        fb.push_back(uint8_t(std::min(255, int(shown / since(told))))); io.send(fb); told = Clock::now(); bytesSince = 0; shown = 0;
    };
    report.ended = "stopped";
    while (!stop) {
        tell();
        if (!io.receive(f, 250)) { if (++silent >= 40) { report.ended = "quiet"; break; } continue; }
        silent = 0; if (f.empty()) continue;
        if (f[0] == frameStop) { report.ended = "their end"; break; }
        if (f[0] == frameLimits && f.size() >= 6) { report.width = int(get16(&f[1])); report.height = int(get16(&f[3])); continue; }
        if (f[0] != frameVideo || f.size() < 14) continue;
        const uint8_t flags = f[1]; const uint32_t n = get32(&f[2]); const int64_t pts = int64_t(get64(&f[6]));
        if (flags & 2) { frame.clear(); current = n; } else if (n != current) { frame.clear(); continue; }
        frame.insert(frame.end(), f.begin() + 14, f.end()); bytesSince += f.size();
        if (!(flags & 4)) continue;
        last = n;
        // After a break, nothing until a key frame (asked for).
        if (waitingKey && !(flags & 1)) continue;
        waitingKey = false;
        const auto t0 = Clock::now();
        const bool ok = decoder.decode(frame.data(), frame.size(), pts, [&](ID3D11Texture2D* t, UINT slice, int w, int h) { sink.frame(t, slice, w, h); ++report.frames; ++shown; });
        decodeMs = decodeMs * .8 + since(t0) * 1000 * .2;
        if (flags & 1) ++report.keys; report.bytes += frame.size();
        if (!ok) { decoder.open(sink.device()); waitingKey = true; io.send(Bytes{frameKeyframe}); }
    }
    io.send(Bytes{frameStop}); sink.send = nullptr; sink.end();
    return report;
}
}
