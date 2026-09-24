#pragma once
#include "Common/Win32.h"
#include "Capture/CaptureModel.h"
#include <memory>
#include <vector>
namespace nexus {
constexpr UINT CaptureMessage=WM_APP+34;
// What the overlay hands back (lParam of CaptureMessage; null when cancelled). Pixels are
// the selected region, 32-bit BGRA top-down; colour is 0xRRGGBB.
struct CaptureResult {CaptureMode mode=CaptureMode::Snip;PixelRect rect;int width=0,height=0;std::vector<uint8_t> pixels;uint32_t colour=0;bool rgb=false;};
// A full-screen, frozen view of every monitor for choosing a region, a window or a pixel.
// The island itself is excluded from the frozen image. One at a time; Esc or right-click cancels.
namespace captureOverlay {
bool begin(HINSTANCE instance,HWND owner,CaptureMode mode,uint32_t accent,bool reduced);
bool active();
}
}
