#pragma once
#include "Common/Win32.h"
#include "Media/Library.h"
#include "Media/MediaProvider.h"
#include <memory>
#include <vector>
namespace nexus {
constexpr UINT PlayerMessage=WM_APP+73;
// Phase 5G: songs played by the island itself (Media Foundation), from a queue of library songs.
// It registers with Windows' media controls for this window, so the keyboard's media keys, the
// volume flyout and the lock screen show and control it like any player. Events (the engine's and
// the media keys') arrive as PlayerMessage and are handled on the UI thread by handle().
class IslandPlayer {
public:
    explicit IslandPlayer(HWND window);
    ~IslandPlayer();
    IslandPlayer(const IslandPlayer&)=delete;IslandPlayer& operator=(const IslandPlayer&)=delete;
    // False when Media Foundation could not start (then nothing plays).
    bool ready()const;
    // Plays `queue` from song `index`, at `start` seconds, or holds it paused there.
    void play(std::vector<LibraryTrack> queue,size_t index,double start=0,bool playing=true);
    void toggle();void resume();void pause();void next();void previous();void seek(double seconds);
    // Silences the engine (test runs play Windows' own sounds without a sound).
    void mute(bool muted);
    // Phase 5H: songs blend into each other over `seconds` (0: they don't); a song skipped fades out in a moment.
    void crossfade(double seconds);
    // The song playing rises from silence (music arriving from another PC), or sinks to silence and then pauses
    // (music leaving for another PC).
    void fadeIn(double seconds);void fadeOut(double seconds);bool fading()const;
    // Volume ramps and the start of a crossfade; call again within the returned milliseconds (0: not needed).
    // changed: what shows changed (the next song began, or a fade-out paused).
    int tick(bool& changed);
    // Up next: the songs after the one playing (at most `count`); a song moved within them (queue positions, both
    // after the one playing); and a jump to a song in the queue.
    std::vector<LibraryTrack> upNext(size_t count)const;bool move(size_t from,size_t to);void jump(size_t index);
    // Stops and forgets the queue (the session goes away).
    void stop();
    bool active()const;bool playing()const;
    const LibraryTrack* track()const;const LibraryTrack* upcoming()const;size_t index()const;size_t size()const;
    double position()const;double duration()const;
    // The cover of the song playing (read by the library), shown by the island and Windows.
    void artwork(std::shared_ptr<const Artwork> art);std::shared_ptr<const Artwork> artwork()const;
    // What the island shows for this session, sampled at `now` (island seconds).
    MediaSnapshot snapshot(double now)const;
    // Handles one PlayerMessage; true when what shows changed.
    bool handle(WPARAM,LPARAM);
    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
};
}
