#pragma once
// Minimal projection of public Windows.Media.Control ABI. Layouts checked against
// Microsoft windows-rs metadata, not private interfaces. Replace with C++/WinRT
// when the Microsoft SDK backend is introduced. Keep this boundary isolated.
#include "Common/Win32.h"
#include <inspectable.h>
#include <winstring.h>
#include <asyncinfo.h>
namespace nexus::mediaabi {
struct Async : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(IUnknown*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(IUnknown**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(IInspectable**)=0;
};
struct Properties : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Subtitle(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING*)=0;
};
struct Session : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_SourceAppUserModelId(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE TryGetMediaPropertiesAsync(Async**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetTimelineProperties(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPlaybackInfo(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryPlayAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryPauseAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryStopAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryRecordAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryFastForwardAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryRewindAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TrySkipNextAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TrySkipPreviousAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeChannelUpAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeChannelDownAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryTogglePlayPauseAsync(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeAutoRepeatModeAsync(INT32,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryChangePlaybackRateAsync(double,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryChangeShuffleActiveAsync(BYTE,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE TryChangePlaybackPositionAsync(INT64,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE add_TimelinePropertiesChanged(IUnknown*,INT64*)=0;
    virtual HRESULT STDMETHODCALLTYPE remove_TimelinePropertiesChanged(INT64)=0;
    virtual HRESULT STDMETHODCALLTYPE add_PlaybackInfoChanged(IUnknown*,INT64*)=0;
    virtual HRESULT STDMETHODCALLTYPE remove_PlaybackInfoChanged(INT64)=0;
    virtual HRESULT STDMETHODCALLTYPE add_MediaPropertiesChanged(IUnknown*,INT64*)=0;
    virtual HRESULT STDMETHODCALLTYPE remove_MediaPropertiesChanged(INT64)=0;
};
struct Manager : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSession(Session**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetSessions(IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE add_CurrentSessionChanged(IUnknown*,INT64*)=0;
    virtual HRESULT STDMETHODCALLTYPE remove_CurrentSessionChanged(INT64)=0;
    virtual HRESULT STDMETHODCALLTYPE add_SessionsChanged(IUnknown*,INT64*)=0;
    virtual HRESULT STDMETHODCALLTYPE remove_SessionsChanged(INT64)=0;
};
struct Statics : IInspectable {virtual HRESULT STDMETHODCALLTYPE RequestAsync(Async**)=0;};
inline GUID guid(const wchar_t* text){GUID value{};check(CLSIDFromString(text,&value));return value;}
}
