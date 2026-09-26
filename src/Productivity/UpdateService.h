#pragma once
#include "Common/Win32.h"
#include "Productivity/Update.h"
#include <filesystem>
#include <mutex>
#include <thread>
namespace nexus {
constexpr UINT UpdateMessage=WM_APP+75;
// 0.18: keeps the island up to date from its GitHub releases, on a worker. A minute and a half after it starts, and
// then every six hours, it asks GitHub for the list of releases. When one is newer, it downloads that release's ZIP
// and checksum from the repository's own download address, checks the SHA-256, unpacks it with Windows' tar, and
// checks the unpacked program says that version. Then it posts UpdateMessage; the island installs it at a quiet
// moment (install) and starts the new version. Nothing about the PC is sent: GitHub sees a request like a browser's.
class UpdateService {
public:
    enum class State{Idle,Checking,UpToDate,Downloading,Ready,Failed};
    // current: the version running; folder: where downloads are unpacked (cleared on start); firstDelay in seconds.
    UpdateService(HWND window,std::filesystem::path folder,AppVersion current,double firstDelay);~UpdateService();
    void checkNow(){SetEvent(wake_);}
    State state(){std::lock_guard lock(mutex_);return state_;}
    // What to show in Settings (for example "Up to date, checked at 14:02").
    std::wstring status(){std::lock_guard lock(mutex_);return status_;}
    AppVersion ready(){std::lock_guard lock(mutex_);return state_==State::Ready?ready_:AppVersion{};}
    // Swaps the downloaded program in for this one (renamed ArnavIsland.old.exe, deleted by the next start) and readies
    // its start (args plus --after=<this process>). True when it's in place: the caller closes the island, and the
    // program, once it has let go of its single-instance lock, calls launchPending.
    bool install(const std::wstring& args);
    // Starts the installed version; if it can't start, puts this version back and starts it again instead.
    static void launchPending();static std::wstring pendingLaunch,pendingFallback;
    // At start: removes what a previous update left behind (the old program).
    static void cleanUp();
private:
    HWND window_;std::filesystem::path folder_;AppVersion current_;double firstDelay_;HANDLE stop_,wake_;std::thread worker_;std::mutex mutex_;
    State state_=State::Idle;std::wstring status_;AppVersion ready_;std::filesystem::path staged_;
    void run();void check();void set(State s,std::wstring status);
};
// 0.18.1: releases are signed (Authenticode) with the island's own publishing certificate. A program counts as the
// island's only if its signature is intact and its signer is that certificate (its SHA-256, below). Windows doesn't
// know this certificate (it isn't from a public authority), so "untrusted root" is expected; which certificate signed
// is what's checked. signerSha256: the signing certificate's SHA-256 (empty when unsigned or the signature is broken).
inline constexpr const char* publisherCertificate="d4cbca03ce626894a377bfa9da287808dcb20edd1dee2a8eb51c2e2fef0cb7d4";
std::string signerSha256(const std::filesystem::path&);
inline bool signedByPublisher(const std::filesystem::path& file){return signerSha256(file)==publisherCertificate;}
// SHA-256 of a file as 64 lowercase hex digits (empty when it can't be read).
std::string fileSha256(const std::filesystem::path&);
// A program's ProductVersion (from its version resource), or empty.
std::wstring productVersion(const std::filesystem::path&);
}
