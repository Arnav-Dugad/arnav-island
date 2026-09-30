#include "Mirror/MirrorWindow.h"
#include <dwmapi.h>
#include <dwrite.h>
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <cmath>


namespace nexus::mirror {
namespace {
constexpr int strip = 36, edge = 8;
constexpr UINT frameMessage = WM_APP + 1, reshapeMessage = WM_APP + 2;
std::string utf8(const std::wstring& w) { if (w.empty()) return {}; const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr); std::string s(size_t(std::max(n, 0)), '\0'); if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr); return s; }
void put16(std::vector<uint8_t>& b, unsigned v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
}

ID3D11Device* MirrorWindow::device() {
    if (!device_) {
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT, levels, 2, D3D11_SDK_VERSION, device_.GetAddressOf(), nullptr, context_.GetAddressOf()))) {
            // The decoder (on the session's thread) and the window's own redraws share it.
            ComPtr<ID3D10Multithread> threads; if (SUCCEEDED(device_.As(&threads))) threads->SetMultithreadProtected(TRUE);
        }
    }
    return device_.Get();
}

bool MirrorWindow::begin(int width, int height, const std::wstring& name) {
    if (!device()) return false;
    name_ = name.empty() ? std::wstring(L"Your phone") : name; phoneW_ = std::max(1, width); phoneH_ = std::max(1, height);
    made_ = 0; ui_ = std::thread([this, width, height] { run(width, height); if (made_ == 0) made_ = -1; });
    // run() makes the window, then says so (or that it couldn't).
    for (int i = 0; i < 300 && made_ == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (made_ != 1) { if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0); if (ui_.joinable()) ui_.join(); hwnd_ = nullptr; return false; }
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; ComPtr<IDXGIFactory2> factory;
    if (FAILED(device_.As(&dxgi)) || FAILED(dxgi->GetAdapter(adapter.GetAddressOf())) || FAILED(adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.GetAddressOf())))) { end(); return false; }
    DXGI_SWAP_CHAIN_DESC1 d{}; d.Width = UINT(std::max(1, clientW_.load())); d.Height = UINT(std::max(1, clientH_.load())); d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; d.BufferCount = 2; d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    if (FAILED(factory->CreateSwapChainForHwnd(device_.Get(), hwnd_, &d, nullptr, nullptr, swap_.GetAddressOf()))) { end(); return false; }
    swapW_ = int(d.Width); swapH_ = int(d.Height);
    D2D1_FACTORY_OPTIONS o{}; if (SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &o, reinterpret_cast<void**>(d2d_.GetAddressOf()))) &&
        SUCCEEDED(d2d_->CreateDevice(dxgi.Get(), d2dDevice_.GetAddressOf()))) d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dContext_.GetAddressOf());
    return true;
}

void MirrorWindow::end() {
    if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    if (ui_.joinable()) ui_.join();
    hwnd_ = nullptr; std::lock_guard held(placeLock_); lastFrame_.Reset(); swap_.Reset(); d2dContext_.Reset(); d2dDevice_.Reset(); d2d_.Reset(); scaler_.reset();
}

void MirrorWindow::frame(ID3D11Texture2D* nv12, UINT slice, int width, int height) {
    { std::lock_guard held(placeLock_); lastFrame_ = nv12; lastSlice_ = slice; lastW_ = width; lastH_ = height; }
    // Drawn on the window's thread; frames that come faster than it draws are skipped.
    if (hwnd_ && !pending_.exchange(true)) PostMessageW(hwnd_, frameMessage, 0, 0);
}

// Draws the last frame into the window, fitted, with the top strip while the pointer is over it.
void MirrorWindow::present() {
    // Only the window's thread draws; the frame is taken under the lock, and drawn without it.
    ComPtr<ID3D11Texture2D> frame; UINT slice = 0; int fw = 0, fh = 0;
    { std::lock_guard held(placeLock_); frame = lastFrame_; slice = lastSlice_; fw = lastW_; fh = lastH_; }
    if (!swap_ || !frame) return;
    // A turned phone: the window takes its new shape (once the frame at that shape comes).
    if (fw > 0 && fh > 0 && std::abs(double(fw) / fh - double(phoneW_) / std::max(1, phoneH_.load())) > .03) { phoneW_ = fw; phoneH_ = fh; PostMessageW(hwnd_, reshapeMessage, 0, 0); }
    const int cw = std::max(1, clientW_.load()), ch = std::max(1, clientH_.load());
    if (cw != swapW_ || ch != swapH_) {
        if (d2dContext_) d2dContext_->SetTarget(nullptr); scaler_.reset();
        if (FAILED(swap_->ResizeBuffers(0, UINT(cw), UINT(ch), DXGI_FORMAT_UNKNOWN, 0))) return;
        swapW_ = cw; swapH_ = ch;
    }
    ComPtr<ID3D11Texture2D> back; if (FAILED(swap_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back.GetAddressOf())))) return;
    scaler_.convert(device_.Get(), frame.Get(), slice, back.Get(), DXGI_MODE_ROTATION_IDENTITY, true);
    // Where the picture landed, for mapping clicks to the phone's screen.
    { const double sc = std::min(double(cw) / std::max(1, fw), double(ch) / std::max(1, fh)); const LONG w = LONG(fw * sc), h = LONG(fh * sc);
      std::lock_guard held(placeLock_); picture_ = {(cw - w) / 2, (ch - h) / 2, (cw - w) / 2 + w, (ch - h) / 2 + h}; }
    if (d2dContext_ && hover_) {
        ComPtr<IDXGISurface> surface; ComPtr<ID2D1Bitmap1> target;
        const D2D1_BITMAP_PROPERTIES1 p = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        if (SUCCEEDED(back.As(&surface)) && SUCCEEDED(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &p, target.GetAddressOf()))) {
            d2dContext_->SetTarget(target.Get()); d2dContext_->BeginDraw();
            ComPtr<ID2D1LinearGradientBrush> shade; ComPtr<ID2D1GradientStopCollection> stops; const D2D1_GRADIENT_STOP s[] = {{0, D2D1::ColorF(0, .62f)}, {1, D2D1::ColorF(0, 0)}};
            d2dContext_->CreateGradientStopCollection(s, 2, stops.GetAddressOf());
            d2dContext_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, float(strip + 18))), stops.Get(), shade.GetAddressOf());
            d2dContext_->FillRectangle(D2D1::RectF(0, 0, float(cw), float(strip + 18)), shade.Get());
            ComPtr<ID2D1SolidColorBrush> ink; d2dContext_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, .95f), ink.GetAddressOf());
            static ComPtr<IDWriteFactory> dw; static ComPtr<IDWriteTextFormat> text;
            if (!dw) DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dw.GetAddressOf()));
            if (dw && !text) dw->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13, L"", text.GetAddressOf());
            if (text) { text->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); d2dContext_->DrawText(name_.c_str(), UINT32(name_.size()), text.Get(), D2D1::RectF(14, 0, float(cw - 80), float(strip)), ink.Get()); }
            // Close (x) and pin (a dot, filled while pinned).
            const float cx = float(cw - 20), cy = float(strip) / 2;
            d2dContext_->DrawLine(D2D1::Point2F(cx - 5, cy - 5), D2D1::Point2F(cx + 5, cy + 5), ink.Get(), 1.6f); d2dContext_->DrawLine(D2D1::Point2F(cx - 5, cy + 5), D2D1::Point2F(cx + 5, cy - 5), ink.Get(), 1.6f);
            const D2D1_ELLIPSE pin = D2D1::Ellipse(D2D1::Point2F(float(cw - 52), cy), 5, 5);
            if (pinned_) d2dContext_->FillEllipse(pin, ink.Get()); else d2dContext_->DrawEllipse(pin, ink.Get(), 1.5f);
            d2dContext_->EndDraw(); d2dContext_->SetTarget(nullptr);
        }
    }
    if (SUCCEEDED(swap_->Present(1, 0))) ++presented;
}

void MirrorWindow::run(int width, int height) {
    static const wchar_t* cls = L"ArnavIslandPhoneScreen";
    static std::once_flag registered;
    std::call_once(registered, [] { WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(nullptr); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = cls; wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)); RegisterClassExW(&wc); });
    // The phone's shape, 70% of the screen's height, near the right edge.
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    const int wh = int((mi.rcWork.bottom - mi.rcWork.top) * .7), ww = std::max(160, int(wh * double(width) / std::max(1, height)));
    const int x = mi.rcWork.right - ww - 32, y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - wh) / 2;
    HWND h = CreateWindowExW(WS_EX_TOPMOST | WS_EX_APPWINDOW, cls, name_.c_str(), WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU, x, y, ww, wh, nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!h) return;
    const DWM_WINDOW_CORNER_PREFERENCE round = DWMWCP_ROUND; DwmSetWindowAttribute(h, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof(round));
    const BOOL dark = TRUE; DwmSetWindowAttribute(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const MARGINS shadow{0, 0, 0, 1}; DwmExtendFrameIntoClientArea(h, &shadow);
    RECT c{}; GetClientRect(h, &c); clientW_ = c.right; clientH_ = c.bottom;
    hwnd_ = h; made_ = 1; ShowWindow(h, SW_SHOWNOACTIVATE);
    MSG m; while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
}

LRESULT CALLBACK MirrorWindow::proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, LONG_PTR(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
    auto* self = reinterpret_cast<MirrorWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    return self ? self->handle(h, msg, w, l) : DefWindowProcW(h, msg, w, l);
}

void MirrorWindow::touch(int action, int x, int y) {
    RECT p; { std::lock_guard held(placeLock_); p = picture_; }
    if (p.right <= p.left || p.bottom <= p.top || !send) return;
    const int nx = std::clamp(int((x - p.left) * 65535LL / std::max<LONG>(1, p.right - p.left)), 0, 65535), ny = std::clamp(int((y - p.top) * 65535LL / std::max<LONG>(1, p.bottom - p.top)), 0, 65535);
    std::vector<uint8_t> b{frameTouch, uint8_t(action)}; put16(b, unsigned(nx)); put16(b, unsigned(ny)); send(b);
}

LRESULT MirrorWindow::handle(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_NCCALCSIZE: if (w) return 0; break; // the whole window is client area (no caption)
    case frameMessage: pending_ = false; present(); return 0;
    case reshapeMessage: {
        // The same long side (wider for a turned phone, up to 55% of the screen), around the same centre, on the screen.
        RECT r; GetWindowRect(h, &r); MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
        const RECT work = mi.rcWork; const double a = double(phoneW_) / std::max(1, phoneH_.load());
        int longSide = std::max(r.right - r.left, r.bottom - r.top); if (a >= 1) longSide = std::max(longSide, int((work.right - work.left) * .55));
        int nw = a >= 1 ? longSide : int(longSide * a), nh = a >= 1 ? int(longSide / a) : longSide;
        const double fit = std::min({1.0, double(work.right - work.left - 32) / std::max(1, nw), double(work.bottom - work.top - 32) / std::max(1, nh)}); nw = int(nw * fit); nh = int(nh * fit);
        const int x = std::clamp(int((r.left + r.right) / 2 - nw / 2), int(work.left), int(work.right - nw)), y = std::clamp(int((r.top + r.bottom) / 2 - nh / 2), int(work.top), int(work.bottom - nh));
        SetWindowPos(h, nullptr, x, y, nw, nh, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
    }
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(h, &pt); RECT c; GetClientRect(h, &c);
        const bool left = pt.x < edge, right = pt.x >= c.right - edge, top = pt.y < edge, bottom = pt.y >= c.bottom - edge;
        if (top && left) return HTTOPLEFT; if (top && right) return HTTOPRIGHT; if (bottom && left) return HTBOTTOMLEFT; if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT; if (right) return HTRIGHT; if (top) return HTTOP; if (bottom) return HTBOTTOM;
        if (pt.y < strip && pt.x < c.right - 68) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_SIZING: {
        // The phone's shape, whichever edge is dragged.
        auto* r = reinterpret_cast<RECT*>(l); const double a = double(phoneW_) / std::max(1, phoneH_.load());
        const int wd = r->right - r->left, ht = r->bottom - r->top;
        if (w == WMSZ_LEFT || w == WMSZ_RIGHT) r->bottom = r->top + int(wd / a); else if (w == WMSZ_TOP || w == WMSZ_BOTTOM) r->right = r->left + int(ht * a);
        else { const int nw = int(ht * a); if (w == WMSZ_TOPLEFT || w == WMSZ_BOTTOMLEFT) r->left = r->right - nw; else r->right = r->left + nw; }
        return TRUE;
    }
    case WM_SIZE: clientW_ = LOWORD(l); clientH_ = HIWORD(l); present(); return 0;
    case WM_MOUSEMOVE: {
        if (!hover_) { hover_ = true; TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0}; TrackMouseEvent(&t); present(); }
        if (dragging_) touch(1, GET_X_LPARAM(l), GET_Y_LPARAM(l));
        return 0;
    }
    case WM_MOUSELEAVE: hover_ = false; present(); return 0;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l); RECT c; GetClientRect(h, &c);
        if (y < strip && x >= c.right - 36) { closed = true; PostMessageW(h, WM_CLOSE, 0, 0); return 0; }
        if (y < strip && x >= c.right - 68) { pinned_ = !pinned_; SetWindowPos(h, pinned_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); present(); return 0; }
        dragging_ = true; SetCapture(h); touch(0, x, y); return 0;
    }
    case WM_LBUTTONUP: if (dragging_) { dragging_ = false; ReleaseCapture(); touch(2, GET_X_LPARAM(l), GET_Y_LPARAM(l)); } return 0;
    case WM_RBUTTONUP: if (send) send({frameButton, 1}); return 0;
    case WM_MBUTTONUP: if (send) send({frameButton, 2}); return 0;
    case WM_MOUSEWHEEL: {
        // The wheel scrolls: a short swipe from where the pointer is.
        POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(h, &pt); RECT p; { std::lock_guard held(placeLock_); p = picture_; }
        const int d = GET_WHEEL_DELTA_WPARAM(w), travel = int((p.bottom - p.top) * .22 * d / WHEEL_DELTA);
        touch(0, pt.x, pt.y); touch(1, pt.x, pt.y + travel / 2); touch(2, pt.x, pt.y + travel); return 0;
    }
    case WM_CHAR: {
        if (!send || w == VK_ESCAPE) return 0;
        std::wstring c(1, wchar_t(w)); if (w == L'\r') c = L"\n";
        std::vector<uint8_t> b{frameText}; const std::string u = utf8(c); b.insert(b.end(), u.begin(), u.end()); send(b); return 0;
    }
    case WM_KEYDOWN: if (w == VK_ESCAPE && send) send({frameButton, 1}); return 0;
    case WM_CLOSE: closed = true; DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_ERASEBKGND: return 1;
    default: break;
    }
    return DefWindowProcW(h, msg, w, l);
}
}
