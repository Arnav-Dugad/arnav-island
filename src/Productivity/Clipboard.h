#pragma once
#include "Common/Win32.h"
#include "Productivity/ClipboardModel.h"
#include <optional>
namespace nexus {
// Reads the Windows clipboard when it changes (WM_CLIPBOARDUPDATE on the island
// window) and can put a kept item back. Content is never logged; it reaches the disk
// only encrypted for this Windows user, when the history is kept across restarts. Copies marked private by their source (the formats Windows' own
// clipboard history honours) and copies from password managers are skipped.
class ClipboardWatcher {
    HWND window_=nullptr;bool listening_=false;DWORD ownSequence_=0;
public:
    enum class Read { Captured,Skipped,Busy };
    ~ClipboardWatcher(){stop();}
    // The island owns what it copies (captures, recognised text, colours) even with history off.
    void attach(HWND window){window_=window;}
    void start(HWND window){window_=window;if(!listening_)listening_=AddClipboardFormatListener(window)!=FALSE;}
    void stop(){if(listening_&&window_)RemoveClipboardFormatListener(window_);listening_=false;}
    bool listening()const{return listening_;}
    // Busy means another app still holds the clipboard; try again shortly.
    Read capture(ClipEntry& out);
    bool copy(const ClipEntry& entry);
};
// A device-independent bitmap as a small premultiplied thumbnail (longest side `size`).
std::shared_ptr<const Artwork> dibThumbnail(const std::vector<uint8_t>& dib,UINT size,uint32_t* width=nullptr,uint32_t* height=nullptr);
// Phase 5G: an image copy as PNG (to keep it across restarts; call off the UI thread, COM initialised) and back
// (a bottom-up 32-bit DIB). Empty on failure.
std::shared_ptr<const std::string> dibToPng(const std::vector<uint8_t>& dib);
std::vector<uint8_t> pngToDib(const std::string& png);
}
