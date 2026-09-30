#pragma once
#include <windows.h>
#include <string>
namespace nexus {
// 0.25: the page a browser shows (Chrome, Edge, Firefox, Brave, Opera, Vivaldi): its address (the toolbar's address
// field), its title (the window's) and how far down it's scrolled (0..1; below 0 when the page doesn't say), read
// through UI Automation. The browser's own interface is walked for the address; for the scroll only the page's top
// element is asked. With a null window, the frontmost browser window. Must run on a COM thread.
struct BrowserPage { std::wstring url, title; double scroll = -1; HWND window = nullptr; };
bool currentBrowserPage(HWND window, BrowserPage& out);
// Opens `url` in the default browser and, when `scroll` is known (0..1), scrolls it there once it has loaded (tried for
// up to 12 s, and again a moment later for pages that grow as they load). Returns at once; the scrolling runs on a
// thread of its own.
bool openPageAt(const std::wstring& url, double scroll);
// Scrolls the page in a browser window to `fraction` (0..1): false when it can't (not loaded, or not scrollable).
bool scrollPage(HWND window, double fraction);
// Whether `window` belongs to a browser.
bool isBrowserWindow(HWND window);
}
