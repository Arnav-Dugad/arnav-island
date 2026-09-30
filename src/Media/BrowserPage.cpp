#include "Media/BrowserPage.h"
#include <uiautomation.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <thread>

namespace nexus {
namespace {
using Microsoft::WRL::ComPtr;
// UI Automation's "leave this axis as it is" (not in every SDK's headers).
constexpr double keepAxis = -1;
constexpr CLSID automationClass{0xff48dba4,0x60ef,0x4201,{0xaa,0x87,0x54,0x10,0x3e,0xef,0x59,0x4e}}; // CUIAutomation
// The browsers read here (as the island's app identity knows them).
bool isBrowserName(std::wstring v) { std::transform(v.begin(), v.end(), v.begin(), ::towlower); for (auto name : {L"chrome", L"msedge", L"microsoft edge", L"firefox", L"brave", L"opera", L"vivaldi", L"chromium"}) if (v.find(name) != std::wstring::npos) return true; return v == L"arc.exe" || v == L"zen.exe"; }
std::wstring take(BSTR value) { std::wstring s = value ? std::wstring(value, SysStringLen(value)) : std::wstring{}; SysFreeString(value); return s; }
std::wstring exeOf(HWND window) {
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid); if (!pid) return {};
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!p) return {};
    wchar_t path[MAX_PATH]; DWORD n = MAX_PATH; std::wstring name;
    if (QueryFullProcessImageNameW(p, 0, path, &n)) { name.assign(path, n); const auto slash = name.find_last_of(L"\\/"); if (slash != std::wstring::npos) name = name.substr(slash + 1); }
    CloseHandle(p); return name;
}
ComPtr<IUIAutomation> automation() {
    ComPtr<IUIAutomation> a; if (FAILED(CoCreateInstance(automationClass, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&a)))) return nullptr;
    // A busy browser must not hold the island up for UI Automation's 20 s default.
    ComPtr<IUIAutomation2> timed; if (SUCCEEDED(a.As(&timed))) { timed->put_ConnectionTimeout(1500); timed->put_TransactionTimeout(1500); }
    return a;
}
// An address field's text as an address: with its scheme (browsers hide "https://"), none for text that isn't one.
std::wstring asAddress(std::wstring v) {
    while (!v.empty() && iswspace(v.back())) v.pop_back(); while (!v.empty() && iswspace(v.front())) v.erase(v.begin());
    if (v.empty() || v.size() > 4096 || v.find(L' ') != std::wstring::npos) return {};
    std::wstring low = v; std::transform(low.begin(), low.end(), low.begin(), ::towlower);
    if (low.starts_with(L"https://") || low.starts_with(L"http://") || low.starts_with(L"file:///")) return v;
    if (low.find(L"://") != std::wstring::npos) return {};
    // A host ("example.com/…") without its scheme.
    const auto end = low.find_first_of(L"/?#"); const std::wstring host = low.substr(0, end);
    if (host.find(L'.') == std::wstring::npos && !host.starts_with(L"localhost")) return {};
    return L"https://" + v;
}
// The toolbar's address field: the first edit control (outside the page) whose text is an address.
std::wstring addressOf(IUIAutomation* a, IUIAutomationElement* root) {
    ComPtr<IUIAutomationCacheRequest> cache; ComPtr<IUIAutomationCondition> all;
    if (FAILED(a->CreateCacheRequest(&cache)) || FAILED(a->CreateTrueCondition(&all))) return {};
    cache->AddProperty(UIA_ControlTypePropertyId); cache->AddProperty(UIA_ClassNamePropertyId); cache->AddPattern(UIA_ValuePatternId); cache->AddProperty(UIA_ValueValuePropertyId);
    std::deque<ComPtr<IUIAutomationElement>> queue{root}; int visited = 0;
    while (!queue.empty() && visited++ < 500) {
        auto element = std::move(queue.front()); queue.pop_front(); ComPtr<IUIAutomationElementArray> children;
        if (FAILED(element->FindAllBuildCache(TreeScope_Children, all.Get(), cache.Get(), &children)) || !children) continue;
        int count = 0; children->get_Length(&count);
        for (int i = 0; i < count; ++i) {
            ComPtr<IUIAutomationElement> child; if (FAILED(children->GetElement(i, &child)) || !child) continue;
            CONTROLTYPEID type = 0; child->get_CachedControlType(&type); BSTR rawClass = nullptr; child->get_CachedClassName(&rawClass); const auto className = take(rawClass);
            if (type == UIA_EditControlTypeId) {
                VARIANT v; VariantInit(&v);
                if (SUCCEEDED(child->GetCachedPropertyValue(UIA_ValueValuePropertyId, &v)) && v.vt == VT_BSTR) { const auto url = asAddress(std::wstring(v.bstrVal, SysStringLen(v.bstrVal))); VariantClear(&v); if (!url.empty()) return url; }
                VariantClear(&v); continue;
            }
            // The page itself is never walked.
            if (type != UIA_DocumentControlTypeId && className != L"Chrome_RenderWidgetHostHWND") queue.push_back(std::move(child));
        }
    }
    return {};
}
// The page's top element (its document).
ComPtr<IUIAutomationElement> documentOf(IUIAutomation* a, IUIAutomationElement* root) {
    ComPtr<IUIAutomationCondition> document; VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = UIA_DocumentControlTypeId;
    if (FAILED(a->CreatePropertyCondition(UIA_ControlTypePropertyId, v, &document))) return nullptr;
    ComPtr<IUIAutomationElement> page; if (FAILED(root->FindFirst(TreeScope_Descendants, document.Get(), &page))) return nullptr; return page;
}
// The document's own address (browsers give the page's full address as its value; the address bar hides "https://").
std::wstring documentAddress(IUIAutomationElement* page) {
    ComPtr<IUIAutomationValuePattern> value; BSTR raw = nullptr;
    if (!page || FAILED(page->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value))) || !value || FAILED(value->get_CurrentValue(&raw))) return {};
    std::wstring v = take(raw); std::wstring low = v; std::transform(low.begin(), low.end(), low.begin(), ::towlower);
    return (low.starts_with(L"https://") || low.starts_with(L"http://") || low.starts_with(L"file:///")) && v.size() <= 4096 && v.find(L' ') == std::wstring::npos ? v : std::wstring();
}
// The page's scroll.
ComPtr<IUIAutomationScrollPattern> scrollOf(IUIAutomation* a, IUIAutomationElement* root) {
    ComPtr<IUIAutomationElement> page = documentOf(a, root); if (!page) return nullptr;
    ComPtr<IUIAutomationScrollPattern> scroll;
    if (FAILED(page->GetCurrentPatternAs(UIA_ScrollPatternId, IID_PPV_ARGS(&scroll))) || !scroll) {
        // Some browsers scroll the page's first child rather than the document itself.
        ComPtr<IUIAutomationTreeWalker> walker; ComPtr<IUIAutomationElement> child;
        if (SUCCEEDED(a->get_ControlViewWalker(&walker)) && SUCCEEDED(walker->GetFirstChildElement(page.Get(), &child)) && child) child->GetCurrentPatternAs(UIA_ScrollPatternId, IID_PPV_ARGS(&scroll));
    }
    return scroll;
}
std::wstring titleOf(HWND window) {
    wchar_t caption[512]; const int n = GetWindowTextW(window, caption, 512); std::wstring t(caption, std::max(0, n));
    // "Page - Google Chrome", "Page — Mozilla Firefox": the browser's name goes.
    for (const wchar_t* sep : {L" — ", L" - "}) { const auto at = t.rfind(sep); if (at != std::wstring::npos && at > 0) { const auto tail = t.substr(at); if (isBrowserName(tail) || tail.find(L"Edge") != std::wstring::npos || tail.find(L"Chrome") != std::wstring::npos || tail.find(L"Firefox") != std::wstring::npos) { t.resize(at); break; } } }
    return t;
}
HWND frontmostBrowser() {
    struct Found { HWND window = nullptr; } found;
    // Top-level windows come in z-order: the first visible browser window is the one used last.
    EnumWindows([](HWND h, LPARAM p) -> BOOL { auto& f = *reinterpret_cast<Found*>(p); if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER) || IsIconic(h)) return TRUE;
        if (isBrowserWindow(h)) { f.window = h; return FALSE; } return TRUE; }, reinterpret_cast<LPARAM>(&found));
    return found.window;
}
}

bool isBrowserWindow(HWND window) { return window && isBrowserName(exeOf(window)); }

bool currentBrowserPage(HWND window, BrowserPage& out) {
    if (!isBrowserWindow(window)) window = frontmostBrowser();
    if (!window) return false;
    auto a = automation(); if (!a) return false;
    ComPtr<IUIAutomationElement> root; if (FAILED(a->ElementFromHandle(window, &root)) || !root) return false;
    // A browser builds its page's accessibility the first time it's asked: a moment later, it answers.
    out = {}; out.window = window;
    for (int attempt = 0; attempt < 4 && out.url.empty(); ++attempt) {
        if (attempt) std::this_thread::sleep_for(std::chrono::milliseconds(350));
        out.url = documentAddress(documentOf(a.Get(), root.Get()).Get()); if (out.url.empty()) out.url = addressOf(a.Get(), root.Get());
    }
    if (out.url.empty()) return false;
    out.title = titleOf(window);
    if (auto scroll = scrollOf(a.Get(), root.Get())) { double v = -1; BOOL can = FALSE; scroll->get_CurrentVerticallyScrollable(&can);
        if (SUCCEEDED(scroll->get_CurrentVerticalScrollPercent(&v)) && v >= 0) out.scroll = std::clamp(v / 100.0, 0.0, 1.0); else if (!can) out.scroll = 0; }
    return true;
}

bool scrollPage(HWND window, double fraction) {
    auto a = automation(); if (!a || !window) return false;
    ComPtr<IUIAutomationElement> root; if (FAILED(a->ElementFromHandle(window, &root)) || !root) return false;
    auto s = scrollOf(a.Get(), root.Get()); BOOL can = FALSE; if (!s || FAILED(s->get_CurrentVerticallyScrollable(&can)) || !can) return false;
    return SUCCEEDED(s->SetScrollPercent(keepAxis, std::clamp(fraction, 0.0, 1.0) * 100));
}

bool openPageAt(const std::wstring& url, double scroll) {
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) return false;
    if (!(scroll > 0.005) || !std::isfinite(scroll)) return true;
    std::thread([url, scroll] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        {
            auto a = automation();
            // The page's host, to know its window from others.
            std::wstring host = url; if (auto at = host.find(L"://"); at != std::wstring::npos) host = host.substr(at + 3); host = host.substr(0, host.find_first_of(L"/?#"));
            std::transform(host.begin(), host.end(), host.begin(), ::towlower);
            const auto began = std::chrono::steady_clock::now(); int placed = 0; auto placedAt = began;
            while (a && std::chrono::steady_clock::now() - began < std::chrono::seconds(12) && placed < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                const HWND window = frontmostBrowser(); if (!window) continue;
                ComPtr<IUIAutomationElement> root; if (FAILED(a->ElementFromHandle(window, &root)) || !root) continue;
                std::wstring shown = documentAddress(documentOf(a.Get(), root.Get()).Get()); if (shown.empty()) shown = addressOf(a.Get(), root.Get()); std::transform(shown.begin(), shown.end(), shown.begin(), ::towlower);
                if (host.empty() || shown.find(host) == std::wstring::npos) continue;
                auto s = scrollOf(a.Get(), root.Get()); BOOL can = FALSE; if (!s || FAILED(s->get_CurrentVerticallyScrollable(&can)) || !can) continue;
                // Once when it can scroll, and again a moment later (a page that grows as it loads moves its content).
                if (placed == 1 && std::chrono::steady_clock::now() - placedAt < std::chrono::milliseconds(1400)) continue;
                if (SUCCEEDED(s->SetScrollPercent(keepAxis, std::clamp(scroll, 0.0, 1.0) * 100))) { ++placed; placedAt = std::chrono::steady_clock::now(); }
            }
        }
        CoUninitialize();
    }).detach();
    return true;
}
}
