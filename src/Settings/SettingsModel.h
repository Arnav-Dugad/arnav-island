#pragma once
#include "Persistence/Settings.h"
#include "Animation/MotionEngine.h"
#include <functional>
#include <string>
#include <vector>
namespace nexus {
// One declarative table describes every preference. The Settings window renders
// it, the island applies it, and tests walk it to prove each value is reachable,
// persisted and bounded. Keys match the names written by Settings::write.
// Preview is the Animation Lab's live spring; Actions is a row of one-shot buttons.
enum class SettingControl { Toggle,Slider,Choice,Stepper,Swatch,Button,Order,Note,Preview,Actions };
enum class SettingAction { None,OpenLab,ResetAll,OpenLogs,ClearLogs,TransparencySettings,ResetLayout,DisplaySettings,SoundSettings,BluetoothSettings,PowerSettings,OpenArmoury,ClearClipboard,PrivacySettings,ClearWorkspaces,OpenCommand,LabPlay,ClearLyrics };
struct SettingItem {
    int section=0;std::wstring title,detail;SettingControl control=SettingControl::Toggle;std::string key;
    int lo=0,hi=1,step=1;std::vector<std::wstring> options;std::wstring unit;SettingAction action=SettingAction::None;
    std::function<int(const Settings&)> get;std::function<void(Settings&,int)> set;
    int clamp(int v)const{return std::clamp(v,lo,hi);}
};
inline const std::vector<std::wstring>& settingSections(){static const std::vector<std::wstring> names{L"General",L"Island",L"Appearance",L"Motion",L"Compact",L"Media & sound",L"Devices & power",L"Home & navigation",L"Privacy & productivity",L"About"};return names;}
// The island's shape spring: a named preset or the custom one, slowed for study when asked.
inline SpringSpec bodySpring(const Settings& s){
    SpringSpec spec=s.preset==5?SpringSpec{s.springMass/100.,double(s.springStiffness),double(s.springDamping)}:preset(MotionPreset(std::clamp(s.preset,0,4)));
    // Slowing by k keeps the shape of the motion: stiffness / k squared, damping / k.
    const double k=s.labSpeed==2?4:s.labSpeed==1?2:1;spec.stiffness/=k*k;spec.damping/=k;return spec;
}
inline const std::vector<std::wstring>& pageNames(){static const std::vector<std::wstring> names{L"Home",L"Media",L"Stats",L"Focus",L"Settings",L"Shelf",L"Audio"};return names;}
inline const std::vector<std::wstring>& metricNames(){static const std::vector<std::wstring> names{L"CPU",L"Memory",L"Battery",L"Download",L"Upload",L"Disk free",L"Uptime",L"GPU"};return names;}
inline void assignMetric(std::array<int,3>& metrics,int slot,int value){value=std::clamp(value,0,7);for(int i=0;i<3;++i)if(i!=slot&&metrics[i]==value)metrics[i]=metrics[slot];metrics[slot]=value;}
inline std::vector<SettingItem> settingItems(int monitors=1){
    std::vector<SettingItem> v;
    auto toggle=[&](int section,std::wstring title,std::wstring detail,std::string key,bool Settings::*field){SettingItem i;i.section=section;i.title=std::move(title);i.detail=std::move(detail);i.key=std::move(key);i.control=SettingControl::Toggle;i.get=[field](const Settings& s){return int(s.*field);};i.set=[field](Settings& s,int x){s.*field=x!=0;};v.push_back(std::move(i));};
    auto number=[&](int section,SettingControl control,std::wstring title,std::wstring detail,std::string key,int Settings::*field,int lo,int hi,int step,std::vector<std::wstring> options={},std::wstring unit={}){SettingItem i;i.section=section;i.title=std::move(title);i.detail=std::move(detail);i.key=std::move(key);i.control=control;i.lo=lo;i.hi=hi;i.step=step;i.options=std::move(options);i.unit=std::move(unit);i.get=[field](const Settings& s){return s.*field;};i.set=[field,lo,hi](Settings& s,int x){s.*field=std::clamp(x,lo,hi);};v.push_back(std::move(i));};
    auto button=[&](int section,std::wstring title,std::wstring detail,std::wstring label,SettingAction action){SettingItem i;i.section=section;i.title=std::move(title);i.detail=std::move(detail);i.control=SettingControl::Button;i.options={std::move(label)};i.action=action;v.push_back(std::move(i));};
    using C=SettingControl;
    toggle(0,L"Start at sign-in",L"Open the island when you sign in to Windows",  "startAtLogin",&Settings::startAtLogin);
    toggle(0,L"Open on hover",L"Rest the pointer on the island to expand it",  "hoverOpen",&Settings::hoverOpen);
    toggle(0,L"Hide until the pointer reaches the edge",L"The island tucks away and slides in when you touch its screen edge","autoHide",&Settings::autoHide);
    toggle(0,L"Show alerts while hidden",L"Let connection, charging and volume cards slide in on their own","alertsReveal",&Settings::alertsReveal);
    number(0,C::Slider,L"Hover delay",L"How long the pointer rests before opening","hoverDelay",&Settings::hoverDelay,100,700,10,{},L" ms");
    number(0,C::Slider,L"Close delay",L"How long the island waits after you leave","collapseDelay",&Settings::collapseDelay,300,1600,50,{},L" ms");
    toggle(0,L"Hide in fullscreen",L"Stay out of games, videos and presentations","hideFullscreen",&Settings::hideFullscreen);
    toggle(0,L"Collapse when switching apps",L"Unpinned panels close when another app takes focus","collapseOnAppSwitch",&Settings::collapseOnAppSwitch);
    toggle(0,L"Scroll anywhere for volume",L"Otherwise only the volume slider responds to the wheel","wheelVolume",&Settings::wheelVolume);
    number(1,C::Choice,L"Everyday mode",L"How much the island shows while resting","uiMode",&Settings::uiMode,0,2,1,{L"Mini Pill",L"Live Island",L"Command Center"});
    number(1,C::Slider,L"Compact width",L"Resting width in Live Island and Command Center","compactWidth",&Settings::compactWidth,160,560,4,{},L" px");
    number(1,C::Choice,L"Dock edge",L"Where the island lives on the screen","edge",&Settings::edge,0,2,1,{L"Top",L"Right",L"Left"});
    std::vector<std::wstring> displays{L"Primary"};for(int i=1;i<=std::max(1,monitors);++i)displays.push_back(L"Display "+std::to_wstring(i));
    number(1,C::Stepper,L"Display",L"Each display remembers its own placement","monitor",&Settings::monitor,0,std::max(1,monitors),1,displays);
    number(1,C::Slider,L"Edge offset",L"Distance from the docked edge","verticalOffset",&Settings::verticalOffset,0,60,1,{},L" px");
    number(1,C::Slider,L"Horizontal placement",L"Shift the island left or right","horizontalOffset",&Settings::horizontalOffset,-600,600,10,{},L" px");
    number(1,C::Slider,L"Scale",L"Size relative to your display scaling","scale",&Settings::scale,80,120,5,{},L"%");
    number(1,C::Slider,L"Corner radius",L"Roundness of the expanded island","corner",&Settings::corner,14,28,1,{},L" px");
    number(2,C::Choice,L"Theme",L"Colors for the island and this window","theme",&Settings::theme,0,2,1,{L"Dark",L"Light",L"System"});
    number(2,C::Choice,L"Material",L"Frosted softens what is behind the island; Clear lets it show through","material",&Settings::material,0,2,1,{L"Solid",L"Frosted glass",L"Clear glass"});
    number(2,C::Slider,L"Glass tint",L"More tint improves text contrast","glassTint",&Settings::glassTint,0,100,1,{},L"%");
    button(2,L"Windows transparency effects",L"Lets Frosted glass blur what is behind it",L"Open Windows settings",SettingAction::TransparencySettings);
    number(2,C::Swatch,L"Accent",L"Used when artwork colors are off \u00b7 the last one follows your wallpaper","accent",&Settings::accent,0,4,1,{L"Mint",L"Sky",L"Lilac",L"Peach",L"Wallpaper"});
    toggle(2,L"Artwork colors",L"Tint controls and a soft glow from the current cover","albumAccents",&Settings::albumAccents);
    toggle(2,L"Soft shadow",L"A gentle shadow under the island lifts it off the desktop","shadow",&Settings::shadow);
    number(3,C::Choice,L"Motion character",L"","preset",&Settings::preset,0,5,1,{L"Balanced",L"Fluid",L"Playful",L"Snappy",L"Calm",L"Custom"});
    // Choosing a preset shows its values on the sliders below; moving a slider makes the spring Custom.
    v.back().set=[](Settings& s,int x){s.preset=std::clamp(x,0,5);if(s.preset<5){auto p=preset(MotionPreset(s.preset));s.springStiffness=int(std::lround(p.stiffness));s.springDamping=int(std::lround(p.damping));s.springMass=int(std::lround(p.mass*100));}};
    {SettingItem i;i.section=3;i.control=SettingControl::Preview;i.title=L"Live preview";i.detail=L"Click to replay";v.push_back(std::move(i));}
    number(3,C::Slider,L"Stiffness",L"How strongly the shape pulls toward its target","springStiffness",&Settings::springStiffness,150,900,10);
    number(3,C::Slider,L"Damping",L"Lower bounces more, higher settles sooner","springDamping",&Settings::springDamping,12,90,1);
    number(3,C::Slider,L"Weight",L"Heavier feels slower and more deliberate","springMass",&Settings::springMass,50,200,5,{},L"%");
    for(size_t k=v.size()-3;k<v.size();++k){auto field=k==v.size()-3?&Settings::springStiffness:k==v.size()-2?&Settings::springDamping:&Settings::springMass;auto& item=v[k];int lo=item.lo,hi=item.hi;item.set=[field,lo,hi](Settings& s,int x){if(s.preset<5){auto p=preset(MotionPreset(std::clamp(s.preset,0,4)));s.springStiffness=int(std::lround(p.stiffness));s.springDamping=int(std::lround(p.damping));s.springMass=int(std::lround(p.mass*100));}s.*field=std::clamp(x,lo,hi);s.preset=5;};
        // A named preset shows its own numbers; Custom shows the saved ones.
        item.get=[field](const Settings& s){if(s.preset>=5)return s.*field;auto p=preset(MotionPreset(std::clamp(s.preset,0,4)));return field==&Settings::springStiffness?int(std::lround(p.stiffness)):field==&Settings::springDamping?int(std::lround(p.damping)):int(std::lround(p.mass*100));};}
    number(3,C::Choice,L"Slow motion",L"Study the island's shape changes \u00b7 not saved","labSpeed",&Settings::labSpeed,0,2,1,{L"1\u00d7",L"\u00bd\u00d7",L"\u00bc\u00d7"});
    {SettingItem i;i.section=3;i.control=SettingControl::Actions;i.title=L"Try it on the island";i.options={L"Expand",L"Collapse",L"Interrupt",L"Card"};i.action=SettingAction::LabPlay;v.push_back(std::move(i));}
    toggle(3,L"Reduce motion",L"Change instantly instead of moving","reduceMotion",&Settings::reduceMotion);
    toggle(3,L"Magnetic buttons",L"Highlights lean toward the pointer","magnetic",&Settings::magnetic);
    toggle(3,L"Animated icons",L"Icons lift and press physically","animatedIcons",&Settings::animatedIcons);
    toggle(3,L"Track handoff",L"Blend album covers between tracks","trackHandoff",&Settings::trackHandoff);
    toggle(4,L"Media",L"Artwork and title of what is playing","compactMedia",&Settings::compactMedia);
    toggle(4,L"Live waveform",L"Bars that move with the real system audio","waveform",&Settings::waveform);
    toggle(4,L"Volume",L"Current output level","compactVolume",&Settings::compactVolume);
    toggle(4,L"Battery",L"Charge level and charging state","compactBattery",&Settings::compactBattery);
    toggle(4,L"Timer",L"Focus and break countdowns","compactTimer",&Settings::compactTimer);
    toggle(4,L"Clock",L"Time of day","compactClock",&Settings::compactClock);
    toggle(4,L"Glance when idle",L"With nothing else to show: today\u2019s date, and CPU and GPU use where there is room","compactGlance",&Settings::compactGlance);
    number(4,C::Choice,L"Glance rings",L"Progress rings at the end of the island","glanceRings",&Settings::glanceRings,0,3,1,{L"Off",L"Battery",L"Timer",L"Both"});
    toggle(4,L"Volume and brightness indicator",L"The island grows to show level changes","hud",&Settings::hud);
    number(5,C::Choice,L"Media layout",L"Artwork size on the Media page","mediaLayout",&Settings::mediaLayout,0,2,1,{L"Auto",L"Music",L"Video"});
    toggle(5,L"Waveform timeline",L"The Media timeline draws the track\u2019s loudness, filling in as it plays","waveTimeline",&Settings::waveTimeline);
    toggle(5,L"Synced lyrics",L"From LRCLIB, a free lyrics library. Only the song title and artist are sent; lyrics are saved on this PC","lyrics",&Settings::lyrics);
    toggle(5,L"Lyrics in the compact island",L"Show the line being sung while music plays","lyricsCompact",&Settings::lyricsCompact);
    toggle(5,L"Artwork pulses to the beat",L"The cover swells gently with the bass of what Windows is playing","artPulse",&Settings::artPulse);
    button(5,L"Saved lyrics",L"Remove the lyrics kept on this PC",L"Clear",SettingAction::ClearLyrics);
    toggle(5,L"App logos",L"Show the real icon of the app that is playing","appIcons",&Settings::appIcons);
    toggle(5,L"Follow the active player",L"Switch to whichever app Windows marks as current","followSession",&Settings::followSession);
    toggle(5,L"Direct output switching",L"Change the default output from the island","directAudio",&Settings::directAudio);
    toggle(5,L"Shelf previews",L"Enlarge file thumbnails on hover","shelfPeek",&Settings::shelfPeek);
    button(5,L"Windows sound settings",L"Devices, spatial sound and more",L"Open",SettingAction::SoundSettings);
    toggle(6,L"Device connection cards",L"Headphones, controllers and other Bluetooth devices announce themselves","deviceCards",&Settings::deviceCards);
    toggle(6,L"Headphone switch card",L"When Windows moves your sound to headphones, offer to switch back","headphoneCards",&Settings::headphoneCards);
    toggle(6,L"Charging card",L"Charge level, rate and time to full when you plug in or unplug","powerCards",&Settings::powerCards);
    toggle(6,L"Keep charge history",L"A week of battery levels, stored only on this device","batteryHistory",&Settings::batteryHistory);
    button(6,L"Bluetooth devices",L"Pair, remove and manage devices",L"Open",SettingAction::BluetoothSettings);
    button(6,L"Power and battery",L"Power mode, battery saver and screen timeouts",L"Open",SettingAction::PowerSettings);
    button(6,L"Armoury Crate",L"Performance profiles, GPU mode and lighting on ASUS ROG",L"Open",SettingAction::OpenArmoury);
    for(int slot=0;slot<7;++slot){SettingItem i;i.section=7;i.control=SettingControl::Order;i.key="nav"+std::to_string(slot);i.lo=0;i.hi=6;i.options=pageNames();i.title=L"Position "+std::to_wstring(slot+1);
        i.get=[slot](const Settings& s){return s.navigation[slot];};i.set=[slot](Settings& s,int direction){int from=slot;moveNavigation(s.navigation,from,direction<0?-1:1);};v.push_back(std::move(i));}
    for(int slot=0;slot<3;++slot){SettingItem i;i.section=7;i.control=SettingControl::Stepper;i.key="home"+std::to_string(slot);i.lo=0;i.hi=7;i.options=metricNames();i.title=std::wstring(L"Home statistic ")+wchar_t(L'1'+slot);i.detail=L"Values never repeat";
        i.get=[slot](const Settings& s){return s.homeMetrics[slot];};i.set=[slot](Settings& s,int x){assignMetric(s.homeMetrics,slot,x);};v.push_back(std::move(i));}
    button(7,L"Restore navigation and statistics",L"Other preferences stay as they are",L"Reset layout",SettingAction::ResetLayout);
    toggle(8,L"Clipboard history",L"Your last 24 copies, in memory only (pins are saved encrypted). Password managers are skipped","clipboardHistory",&Settings::clipboardHistory);
    toggle(8,L"Hide passwords and codes",L"Copies that look like a password, one-time code or key stay hidden until you point at them","hideSecrets",&Settings::hideSecrets);
    toggle(8,L"Copy confirmation",L"The island briefly shows what you copied","clipboardConfirm",&Settings::clipboardConfirm);
    button(8,L"Clear clipboard history",L"Forget every kept copy now, pinned ones too",L"Clear",SettingAction::ClearClipboard);
    toggle(8,L"Privacy indicators",L"Dots when an app uses the camera, microphone or location","privacyDots",&Settings::privacyDots);
    toggle(8,L"Privacy cards",L"Announce which app just started using the camera or microphone","privacyCards",&Settings::privacyCards);
    button(8,L"Windows privacy settings",L"Choose which apps may use the camera, microphone and location",L"Open",SettingAction::PrivacySettings);
    number(8,C::Choice,L"Command shortcut",L"Opens the command bar","commandShortcut",&Settings::commandShortcut,0,3,1,{L"Off",L"Alt+Shift+Space",L"Ctrl+Alt+Space",L"Win+Alt+Space"});
    button(8,L"Command bar",L"Apps, files, system switches, timers, settings and workspaces by typing",L"Open",SettingAction::OpenCommand);
    toggle(8,L"Remember recent commands",L"The empty command bar shows your pinned and recent commands; kept only on this PC","commandHistory",&Settings::commandHistory);
    toggle(8,L"Currency conversion",L"Type \u201c100 usd to inr\u201d. Fetches the European Central Bank\u2019s public daily rates, at most twice a day; nothing about you is sent","currency",&Settings::currency);
    toggle(8,L"Capture and clipboard shortcuts",L"Alt+Shift+S snip  \u00b7  Alt+Shift+T copy text  \u00b7  Alt+Shift+C pick a colour  \u00b7  Alt+Shift+V clipboard","captureShortcuts",&Settings::captureShortcuts);
    toggle(8,L"Keep the Shelf after restarts",L"Remembers links to your Shelf files and dropped text on this PC, never copies of the files","pinnedShelf",&Settings::pinnedShelf);
    button(8,L"Saved workspaces",L"Remove every saved app set; open apps are not affected",L"Remove",SettingAction::ClearWorkspaces);
    button(9,L"Local logs",L"Diagnostics stay on this device",L"Open folder",SettingAction::OpenLogs);
    button(9,L"Clear logs",L"Remove local diagnostic events",L"Clear",SettingAction::ClearLogs);
    button(9,L"Reset all preferences",L"Sign-in startup is kept",L"Reset",SettingAction::ResetAll);
    return v;
}
}
