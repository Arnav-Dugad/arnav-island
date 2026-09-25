#pragma once
#include "Common/Win32.h"
#include "Interaction/DashboardModel.h"
#include <map>
#include <memory>
#include <mutex>
#include <deque>
#include <set>
#include <string>
#include <thread>
namespace nexus {
constexpr UINT SiteIconMessage=WM_APP+43;
// Phase 5F (opt-in): the icon of a site whose link was copied, fetched once from that site
// itself (https://<host>/favicon.ico) on a worker and kept in memory only.
class SiteIcons {
    HWND window_;HANDLE stop_,wake_;std::thread worker_;std::mutex mutex_;std::deque<std::wstring> queue_;std::map<std::wstring,std::shared_ptr<const Artwork>> icons_;std::set<std::wstring> asked_;void run();
public:
    explicit SiteIcons(HWND);~SiteIcons();
    // The icon when it has arrived; otherwise asks for it (once) and returns nothing.
    std::shared_ptr<const Artwork> get(const std::wstring& host);
};
// Decodes an ICO, PNG or other WIC-readable image to a 32 x 32 premultiplied BGRA picture
// (the frame closest to 32 pixels); nullptr for anything unreadable.
std::shared_ptr<const Artwork> decodeIcon(const std::string& bytes);
}
