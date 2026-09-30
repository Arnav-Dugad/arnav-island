// Tests only: the island's browser page reader against one browser window (a made-up page in an isolated profile).
// Prints whether the address matched what was expected (never the address itself) and the scroll, 0..1.
//   browser_probe read <hwnd> <expected url>     browser_probe scroll <hwnd> <fraction>
#include "Media/BrowserPage.h"
#include <objbase.h>
#include <cstdio>
#include <cstdlib>
#include <string>
int main(int argc, char** argv) {
    if (argc < 4) { std::puts("usage: browser_probe read|scroll <hwnd> <url|fraction>"); return 2; }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const HWND window = reinterpret_cast<HWND>(static_cast<uintptr_t>(std::strtoull(argv[2], nullptr, 10)));
    const std::string cmd = argv[1]; int code = 1;
    if (cmd == "read") {
        nexus::BrowserPage page; const bool ok = nexus::currentBrowserPage(window, page);
        std::string url; for (wchar_t c : page.url) url += c < 128 ? char(c) : '?';
        std::printf("READ %d match %d scroll %.3f title %d\n", ok ? 1 : 0, url == argv[3] ? 1 : 0, page.scroll, page.title.empty() ? 0 : 1); code = ok ? 0 : 1;
    } else if (cmd == "scroll") { const bool ok = nexus::scrollPage(window, std::atof(argv[3])); std::printf("SCROLL %d\n", ok ? 1 : 0); code = ok ? 0 : 1; }
    CoUninitialize(); return code;
}
