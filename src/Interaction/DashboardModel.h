#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cwctype>
#include <cwchar>
namespace nexus {
enum class Page { Overview,Media,System,Focus,Settings,Shelf,Audio };
enum class Action { None,Overview,Media,System,Focus,Settings,Pin,Close,Play,Previous,Next,MediaMode,VolumeDown,Mute,VolumeUp,Timer25,Timer5,Stopwatch,TimerToggle,TimerReset,HoverToggle,HoverDelay,FullscreenToggle,ReducedToggle,MotionPreset,Offset,Monitor,SoundSettings,DisplaySettings,NetworkSettings,BluetoothSettings,Lab,Shelf,Audio,SettingsNext,StartupToggle,Theme,Scale,Corner,Edge,CompactWidth,CollapseDelay,GlassToggle,AccentsToggle,MagneticToggle,CompactMediaToggle,CompactBatteryToggle,Accent,AudioCompatibility,ShelfClear,SettingsReset,LayoutSlot,LayoutLeft,LayoutRight,MetricOne,MetricTwo,MetricThree,Rings,IconsToggle,HandoffToggle,LayoutReset,VolumeSlider,AppSwitchToggle,WheelVolumeToggle,Seek,UiMode,CompactVolumeToggle,CompactTimerToggle,CompactClockToggle,ShelfPeekToggle,WidthDown,HorizontalOffset,AudioApps,AudioOutputs,MixerSettings,StatsSystem,StatsBattery,StatsDevices,Armoury,PowerSettings,ShelfFiles,ShelfClipboard,ClipboardEnable,ClipboardPause,ClipboardClear,CommandOpen,LyricsToggle,SkipBack,SkipForward,SwitchBack,MicMute,DeviceBase=100,ShelfItemBase=200,MixerSliderBase=300,MixerMuteBase=340,SessionBase=380,DeviceConnectBase=400,DeviceConnectEnd=440,ClipBase=440,CommandResultBase=470,ActionEnd=480 };
inline bool inRange(Action a,Action base,Action end){return int(a)>=int(base)&&int(a)<int(end);}
// Command bar layout: input 42, then 40 per row, then the key hints.
inline float commandFooterY(int rows){return 52+40.f*float(rows)+2;}
inline double commandIslandHeight(int rows){return 16+commandFooterY(rows)+26+12;}
struct HitTarget { Action action;float x,y,width,height;bool enabled=true;
    bool contains(float px,float py)const{return enabled&&px>=x&&px<x+width&&py>=y&&py<y+height;}
};
// accent, secondary, ambient and deep come from the picture (Design/Palette.h); 0 = not computed.
struct Artwork {uint32_t width=0,height=0,accent=0xa5d8c5,secondary=0,ambient=0,deep=0;std::vector<uint8_t> pixels;};
enum class MediaKind { Unknown,Music,Video };
inline MediaKind classifyMedia(int osType,std::wstring source){
    if(osType==1)return MediaKind::Music;if(osType==2)return MediaKind::Video;
    std::transform(source.begin(),source.end(),source.begin(),::towlower);
    if(source.find(L"spotify")!=std::wstring::npos||source.find(L"applemusic")!=std::wstring::npos||source.find(L"youtubemusic")!=std::wstring::npos)return MediaKind::Music;
    return MediaKind::Unknown;
}
struct FocusClock {
    enum class Mode { Focus,Break,Stopwatch } mode=Mode::Focus;
    bool running=false,finished=false;double duration=1500,held=0,epoch=0;
    double elapsed(double now)const{return held+(running?std::max(0.,now-epoch):0);}
    double displayed(double now)const{return mode==Mode::Stopwatch?elapsed(now):std::max(0.,duration-elapsed(now));}
    void select(Mode value,double now){mode=value;duration=value==Mode::Break?300:1500;held=0;epoch=now;running=false;finished=false;}
    void toggle(double now){if(finished){held=0;finished=false;}if(running){held=elapsed(now);running=false;}else{epoch=now;running=true;}}
    void reset(double now){held=0;epoch=now;running=false;finished=false;}
    bool tick(double now){if(running&&mode!=Mode::Stopwatch&&elapsed(now)>=duration){held=duration;running=false;finished=true;return true;}return false;}
};
inline std::wstring clockText(double seconds){int n=int(std::max(0.,seconds));wchar_t b[32];if(n>=3600)swprintf(b,32,L"%d:%02d:%02d",n/3600,(n/60)%60,n%60);else swprintf(b,32,L"%02d:%02d",n/60,n%60);return b;}
inline std::wstring rateText(double bytes){wchar_t b[32];if(bytes<1024)swprintf(b,32,L"%.0f B/s",bytes);else if(bytes<1048576)swprintf(b,32,L"%.0f KB/s",bytes/1024);else swprintf(b,32,L"%.1f MB/s",bytes/1048576);return b;}
}
