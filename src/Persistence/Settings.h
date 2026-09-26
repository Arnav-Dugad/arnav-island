#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>
#include <stdexcept>
#include "Design/Layout.h"
namespace nexus {
struct Settings {
    static constexpr int currentVersion=18;
    int version=currentVersion,uiMode=1,preset=1,monitor=0,verticalOffset=0,horizontalOffset=0,hoverDelay=180,mediaLayout=0;
    int scale=100,corner=22,edge=0,theme=0,compactWidth=196,collapseDelay=650,accent=0;
    int material=0,glassTint=50;
    std::array<int,pageCount> navigation=defaultNavigation;std::array<int,chipCount> chips=defaultChips;std::array<int,3> homeMetrics=defaultMetrics;
    int glanceRings=3;bool animatedIcons=true,trackHandoff=true,collapseOnAppSwitch=true,wheelVolume=false;
    bool compactVolume=true,compactTimer=true,compactClock=false,shelfPeek=true;
    bool reduceMotion=false,hideFullscreen=true,hoverOpen=true,startAtLogin=true;
    bool albumAccents=true,magnetic=true,compactMedia=true,compactBattery=true,directAudio=true;
    bool waveform=true,appIcons=true,hud=true,followSession=true;
    bool autoHide=true,alertsReveal=false,deviceCards=true,powerCards=true,batteryHistory=true;
    // v8: clipboard history is opt-in; privacy indicators and the command shortcut are on.
    bool clipboardHistory=false,clipboardConfirm=true,privacyDots=true,privacyCards=true;
    // Command bar shortcut: 0 off, 1 Alt+Shift+Space, 2 Ctrl+Alt+Space, 3 Win+Alt+Space.
    int commandShortcut=1;
    // v9: the custom spring (preset 5), and slow motion for studying it (never saved: 0 = 1x, 1 = 1/2x, 2 = 1/4x).
    int springStiffness=390,springDamping=36,springMass=100,labSpeed=0;bool waveTimeline=true;
    // v10: synced lyrics are opt-in (they are fetched from LRCLIB); the headphone switch card is on.
    bool lyrics=false,lyricsCompact=true,headphoneCards=true;
    // v11: capture and clipboard shortcuts, hiding copies that look like passwords, and the
    // pinned Shelf (opt-in: it remembers links to the files across restarts).
    bool captureShortcuts=true,hideSecrets=true,pinnedShelf=false;
    // v12: currency conversion is opt-in (it fetches the ECB's daily rates); the command bar
    // remembers recent and pinned commands on this PC unless this is turned off.
    bool currency=false,commandHistory=true;
    // v13: a soft shadow under the island, and glanceable extras in the compact island when it is idle.
    // artPulse: the album artwork swells a little with the bass.
    bool shadow=true,compactGlance=true,artPulse=true;
    // v14: media controls and swipes in the compact island, Now Playing at the edge of fullscreen apps,
    // the spectrum ring (waveformStyle 0 bars, 1 ring), colours from the playing app's icon, the weekly
    // battery card, weather (opt-in: Open-Meteo), rich clipboard rows and site icons (opt-in: fetched from
    // the site itself), alerts that drop a pill (notifyStyle 0 grow, 1 drop), light along the edge for
    // alerts, per-letter text colour on Clear glass, and sharing with your own PCs (opt-in).
    bool compactControls=true,swipeSkip=true,fullscreenPeek=true,appAccents=true,batteryWeekly=true,weather=false,richClips=true,siteIcons=false,edgeSplash=true,adaptiveText=true,sharing=false;
    int waveformStyle=1,notifyStyle=1,weatherUnit=0;
    // v15: clipboard history kept across restarts (encrypted for this Windows user), soft sounds for alerts and the
    // chips editor, continuing music on a paired PC (with sharing on), the ring glowing into the next track, a second
    // alert budding off the first, and songs from your Music folder played by the island itself.
    bool clipboardKeep=true,sounds=true,handoff=true,islandDj=true,stackAlerts=true,musicLibrary=true;
    // v16: your paired PCs may look into this Shelf and take from it (with sharing on), and the island's own songs
    // crossfade over this many seconds (0: no crossfade).
    bool shelfOpen=true;int crossfade=6;
    // v17: Clear glass has its own tint (glassTint is Frosted's), the edge light breathes with the beat while music
    // plays, and Frosted glass thickens while the island rests.
    int clearTint=50;bool beatEdge=true,restFrost=true;
    // v18: rain, drops, fog and snow on the glass when the weather has them; the island updates itself from its releases.
    bool weatherGlass=true,autoUpdate=true;
    // The tint of the material in use (0-100).
    int tintFor()const{return material==2?clearTint:glassTint;}
    // Every material meets the screen edge with the same concave shoulders; an
    // explicit vertical offset floats the island clear of the edge.
    bool glassy()const{return material!=0;}
    bool floating()const{return verticalOffset>0;}
    static Settings parse(std::istream& in){Settings s;std::string key;double value;bool controls=false,navigated=false;while(in>>key){if(key.starts_with("nav"))navigated=true;if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Invalid setting");
        if(key=="version"){if(value<1||value>currentVersion||std::floor(value)!=value)throw std::runtime_error("Unsupported settings version");}
        else if(key.size()==4&&key.starts_with("nav")&&key[3]>='0'&&key[3]<='7'){s.navigation[size_t(key[3]-'0')]=int(std::clamp(value,0.,double(pageCount-1)));if(key[3]=='7')controls=true;}
        else if(key.size()==5&&key.starts_with("home")&&key[4]>='0'&&key[4]<='2')s.homeMetrics[key[4]-'0']=int(std::clamp(value,0.,double(metricCount-1)));
        else if(key.size()==5&&key.starts_with("chip")&&key[4]>='0'&&key[4]<='6')s.chips[size_t(key[4]-'0')]=int(std::clamp(value,0.,double(chipCount-1)));
        else if(key=="glass"){/* v5 interior acrylic; superseded by material. */}
#define NUM(name,lo,hi) else if(key==#name)s.name=int(std::clamp(value,double(lo),double(hi)));
        NUM(glanceRings,0,3) NUM(preset,0,5) NUM(monitor,0,16) NUM(verticalOffset,0,200) NUM(horizontalOffset,-2000,2000) NUM(hoverDelay,100,700) NUM(mediaLayout,0,2)
        NUM(springStiffness,150,900) NUM(springDamping,12,90) NUM(springMass,50,200) NUM(scale,80,120) NUM(corner,14,28) NUM(edge,0,2) NUM(theme,0,2) NUM(compactWidth,160,560) NUM(uiMode,0,2) NUM(collapseDelay,300,1600) NUM(accent,0,4)
        NUM(material,0,2) NUM(glassTint,0,100) NUM(commandShortcut,0,3) NUM(waveformStyle,0,1) NUM(notifyStyle,0,1) NUM(weatherUnit,0,1) NUM(crossfade,0,12) NUM(clearTint,0,100)
#undef NUM
#define FLAG(name) else if(key==#name)s.name=value!=0;
        FLAG(compactVolume) FLAG(compactTimer) FLAG(compactClock) FLAG(shelfPeek) FLAG(collapseOnAppSwitch) FLAG(wheelVolume) FLAG(animatedIcons) FLAG(trackHandoff) FLAG(reduceMotion) FLAG(hideFullscreen) FLAG(hoverOpen) FLAG(startAtLogin) FLAG(albumAccents) FLAG(magnetic) FLAG(compactMedia) FLAG(compactBattery) FLAG(directAudio)
        FLAG(waveform) FLAG(appIcons) FLAG(hud) FLAG(followSession) FLAG(autoHide) FLAG(alertsReveal) FLAG(deviceCards) FLAG(powerCards) FLAG(batteryHistory) FLAG(clipboardHistory) FLAG(clipboardConfirm) FLAG(privacyDots) FLAG(privacyCards) FLAG(waveTimeline) FLAG(lyrics) FLAG(lyricsCompact) FLAG(headphoneCards) FLAG(captureShortcuts) FLAG(hideSecrets) FLAG(pinnedShelf) FLAG(currency) FLAG(commandHistory) FLAG(shadow) FLAG(compactGlance) FLAG(artPulse)
        FLAG(compactControls) FLAG(swipeSkip) FLAG(fullscreenPeek) FLAG(appAccents) FLAG(batteryWeekly) FLAG(weather) FLAG(richClips) FLAG(siteIcons) FLAG(edgeSplash) FLAG(adaptiveText) FLAG(sharing)
        FLAG(clipboardKeep) FLAG(sounds) FLAG(handoff) FLAG(islandDj) FLAG(stackAlerts) FLAG(musicLibrary) FLAG(shelfOpen) FLAG(beatEdge) FLAG(restFrost) FLAG(weatherGlass) FLAG(autoUpdate)
#undef FLAG
        }if(!in.eof())throw std::runtime_error("Malformed settings");
        // Before v14 there were seven pages: the saved order keeps its places and gains Controls after Stats.
        if(navigated&&!controls){std::array<int,7> legacy{};std::copy_n(s.navigation.begin(),7,legacy.begin());s.navigation=withControls(legacy);}
        if(!validNavigation(s.navigation))s.navigation=defaultNavigation;if(!validChips(s.chips))s.chips=defaultChips;auto metrics=s.homeMetrics;std::sort(metrics.begin(),metrics.end());if(std::adjacent_find(metrics.begin(),metrics.end())!=metrics.end())s.homeMetrics=defaultMetrics;return s;}
    void write(std::ostream& out)const{out<<"version "<<currentVersion<<'\n';for(int i=0;i<pageCount;++i)out<<"nav"<<i<<' '<<navigation[size_t(i)]<<'\n';for(int i=0;i<3;++i)out<<"home"<<i<<' '<<homeMetrics[i]<<'\n';for(int i=0;i<chipCount;++i)out<<"chip"<<i<<' '<<chips[size_t(i)]<<'\n';
#define WRITE(name) out<<#name<<' '<<name<<'\n';
        WRITE(uiMode) WRITE(compactVolume) WRITE(compactTimer) WRITE(compactClock) WRITE(shelfPeek) WRITE(collapseOnAppSwitch) WRITE(wheelVolume) WRITE(glanceRings) WRITE(animatedIcons) WRITE(trackHandoff) WRITE(preset) WRITE(monitor) WRITE(verticalOffset) WRITE(horizontalOffset) WRITE(hoverDelay) WRITE(mediaLayout) WRITE(scale) WRITE(corner) WRITE(edge) WRITE(theme) WRITE(compactWidth) WRITE(collapseDelay) WRITE(accent)
        WRITE(material) WRITE(glassTint) WRITE(reduceMotion) WRITE(hideFullscreen) WRITE(hoverOpen) WRITE(startAtLogin) WRITE(albumAccents) WRITE(magnetic) WRITE(compactMedia) WRITE(compactBattery) WRITE(directAudio)
        WRITE(waveform) WRITE(appIcons) WRITE(hud) WRITE(followSession) WRITE(autoHide) WRITE(alertsReveal) WRITE(deviceCards) WRITE(powerCards) WRITE(batteryHistory) WRITE(clipboardHistory) WRITE(clipboardConfirm) WRITE(privacyDots) WRITE(privacyCards) WRITE(springStiffness) WRITE(springDamping) WRITE(springMass) WRITE(commandShortcut) WRITE(waveTimeline) WRITE(lyrics) WRITE(lyricsCompact) WRITE(headphoneCards) WRITE(captureShortcuts) WRITE(hideSecrets) WRITE(pinnedShelf) WRITE(currency) WRITE(commandHistory) WRITE(shadow) WRITE(compactGlance) WRITE(artPulse)
        WRITE(compactControls) WRITE(swipeSkip) WRITE(fullscreenPeek) WRITE(appAccents) WRITE(batteryWeekly) WRITE(weather) WRITE(richClips) WRITE(siteIcons) WRITE(edgeSplash) WRITE(adaptiveText) WRITE(sharing) WRITE(waveformStyle) WRITE(notifyStyle) WRITE(weatherUnit)
        WRITE(clipboardKeep) WRITE(sounds) WRITE(handoff) WRITE(islandDj) WRITE(stackAlerts) WRITE(musicLibrary) WRITE(shelfOpen) WRITE(crossfade) WRITE(clearTint) WRITE(beatEdge) WRITE(restFrost) WRITE(weatherGlass) WRITE(autoUpdate)
#undef WRITE
    }
    bool operator==(const Settings&)const=default;
};
}
