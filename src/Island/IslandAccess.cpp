#include "IslandWindow.h"
#include <uiautomation.h>
#include <windowsx.h>
// Phase 5H: the island for screen readers. UI Automation sees the island's window as a pane (it expands and
// collapses) whose children are what can be pressed on it right now: buttons (Invoke), switches (Toggle) and
// sliders (RangeValue), each with a spoken name and its place on the screen, taken from the same hit targets the
// pointer uses. Alerts, the page that opens, the song that starts and level changes are announced
// (UiaRaiseNotificationEvent). The providers are called on the island's own thread; anything that changes the
// island is posted back to it (AccessMessage), so a screen reader never waits on an animation.
namespace nexus {
namespace {
constexpr UINT AccessMessage=WM_APP+80;
// AccessMessage wParam: LOWORD what (0 press, 1 set a value, 2 expand, 3 collapse), HIWORD the action; lParam the value x 1000.
enum class Role{Button,Toggle,Slider};
Role roleOf(Action a){
    switch(a){case Action::VolumeSlider:case Action::Seek:case Action::ControlBrightness:return Role::Slider;
    case Action::ControlWifi:case Action::ControlBluetooth:case Action::ControlAirplane:case Action::ControlDark:case Action::ControlFocus:case Action::ControlMic:
    case Action::Mute:case Action::MicMute:case Action::Pin:case Action::LyricsToggle:case Action::ClipboardPause:return Role::Toggle;default:break;}
    if(inRange(a,Action::MixerSliderBase,Action::MixerMuteBase))return Role::Slider;
    return Role::Button;
}
BSTR bstr(const std::wstring& s){return SysAllocStringLen(s.data(),UINT(s.size()));}
}
namespace access {
struct Root;
// One thing on the island that can be pressed, by its action (the first target with it: the same action twice, like
// the next cover and the Up next button, is one element).
struct Item final:IRawElementProviderSimple,IRawElementProviderFragment,IInvokeProvider,IToggleProvider,IRangeValueProvider{
    LONG refs=1;IslandWindow* island;Root* root;Action action;
    Item(IslandWindow* w,Root* r,Action a);~Item();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;const Role role=roleOf(action);
        if(iid==IID_IUnknown||iid==__uuidof(IRawElementProviderSimple))*p=static_cast<IRawElementProviderSimple*>(this);else if(iid==__uuidof(IRawElementProviderFragment))*p=static_cast<IRawElementProviderFragment*>(this);
        else if(iid==__uuidof(IInvokeProvider)&&role==Role::Button)*p=static_cast<IInvokeProvider*>(this);else if(iid==__uuidof(IToggleProvider)&&role==Role::Toggle)*p=static_cast<IToggleProvider*>(this);
        else if(iid==__uuidof(IRangeValueProvider)&&role==Role::Slider)*p=static_cast<IRangeValueProvider*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release()override{const LONG n=InterlockedDecrement(&refs);if(!n)delete this;return ULONG(n);}
    // IRawElementProviderSimple
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* o)override{if(!o)return E_POINTER;*o=ProviderOptions_ServerSideProvider;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID id,IUnknown** p)override{if(!p)return E_POINTER;*p=nullptr;const Role role=roleOf(action);
        if((id==UIA_InvokePatternId&&role==Role::Button)||(id==UIA_TogglePatternId&&role==Role::Toggle)||(id==UIA_RangeValuePatternId&&role==Role::Slider)){*p=static_cast<IRawElementProviderSimple*>(this);AddRef();}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id,VARIANT* v)override;
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** p)override{if(!p)return E_POINTER;*p=nullptr;return S_OK;}
    // IRawElementProviderFragment
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,IRawElementProviderFragment** p)override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** id)override{if(!id)return E_POINTER;int parts[]={UiaAppendRuntimeId,int(action)};*id=SafeArrayCreateVector(VT_I4,0,2);if(!*id)return E_OUTOFMEMORY;
        for(LONG i=0;i<2;++i)SafeArrayPutElement(*id,&i,&parts[i]);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* r)override;
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** p)override{if(!p)return E_POINTER;*p=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetFocus()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** p)override;
    // IInvokeProvider, IToggleProvider
    HRESULT STDMETHODCALLTYPE Invoke()override{if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;post(0,0);return S_OK;}
    HRESULT STDMETHODCALLTYPE Toggle()override{if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;post(0,0);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* s)override{if(!s)return E_POINTER;if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;*s=island->accessToggled(action)?ToggleState_On:ToggleState_Off;return S_OK;}
    // IRangeValueProvider
    HRESULT STDMETHODCALLTYPE SetValue(double v)override{if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;double lo=0,hi=100,now=0;island->accessRange(action,now,lo,hi);if(!(v>=lo&&v<=hi))return E_INVALIDARG;post(1,LPARAM(std::llround(v*1000)));return S_OK;}
    HRESULT STDMETHODCALLTYPE get_Value(double* v)override{if(!v)return E_POINTER;if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;double lo=0,hi=100;island->accessRange(action,*v,lo,hi);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* r)override{if(!r)return E_POINTER;*r=FALSE;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_Maximum(double* v)override{if(!v)return E_POINTER;if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;double now=0,lo=0;island->accessRange(action,now,lo,*v);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_Minimum(double* v)override{if(!v)return E_POINTER;if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;double now=0,hi=0;island->accessRange(action,now,*v,hi);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_LargeChange(double* v)override{if(!v)return E_POINTER;if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;double now=0,lo=0,hi=100;island->accessRange(action,now,lo,hi);*v=(hi-lo)/10;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_SmallChange(double* v)override{if(!v)return E_POINTER;if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;double now=0,lo=0,hi=100;island->accessRange(action,now,lo,hi);*v=action==Action::Seek?5:(hi-lo)/50;return S_OK;}
    bool alive()const;
    void post(WORD what,LPARAM value)const{PostMessageW(island->accessWindow(),AccessMessage,MAKEWPARAM(what,WORD(int(action))),value);}
};
// The island itself: the pane the items are on.
struct Root final:IRawElementProviderSimple,IRawElementProviderFragment,IRawElementProviderFragmentRoot,IExpandCollapseProvider{
    LONG refs=1;IslandWindow* island;bool gone=false;explicit Root(IslandWindow* w):island(w){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;
        if(iid==IID_IUnknown||iid==__uuidof(IRawElementProviderSimple))*p=static_cast<IRawElementProviderSimple*>(this);else if(iid==__uuidof(IRawElementProviderFragment))*p=static_cast<IRawElementProviderFragment*>(this);
        else if(iid==__uuidof(IRawElementProviderFragmentRoot))*p=static_cast<IRawElementProviderFragmentRoot*>(this);else if(iid==__uuidof(IExpandCollapseProvider))*p=static_cast<IExpandCollapseProvider*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ULONG(InterlockedIncrement(&refs));}
    ULONG STDMETHODCALLTYPE Release()override{const LONG n=InterlockedDecrement(&refs);if(!n)delete this;return ULONG(n);}
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* o)override{if(!o)return E_POINTER;*o=ProviderOptions_ServerSideProvider;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID id,IUnknown** p)override{if(!p)return E_POINTER;*p=nullptr;if(id==UIA_ExpandCollapsePatternId&&!gone){*p=static_cast<IRawElementProviderSimple*>(this);AddRef();}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id,VARIANT* v)override{if(!v)return E_POINTER;VariantInit(v);if(gone)return S_OK;
        switch(id){
        case UIA_NamePropertyId:v->vt=VT_BSTR;v->bstrVal=bstr(L"Island");break;
        case UIA_ControlTypePropertyId:v->vt=VT_I4;v->lVal=UIA_PaneControlTypeId;break;
        case UIA_LocalizedControlTypePropertyId:v->vt=VT_BSTR;v->bstrVal=bstr(L"island");break;
        case UIA_ItemStatusPropertyId:case UIA_HelpTextPropertyId:v->vt=VT_BSTR;v->bstrVal=bstr(island->accessSummary());break;
        case UIA_AutomationIdPropertyId:v->vt=VT_BSTR;v->bstrVal=bstr(L"island");break;
        case UIA_IsKeyboardFocusablePropertyId:v->vt=VT_BOOL;v->boolVal=VARIANT_FALSE;break;
        default:break;}
        return S_OK;}
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** p)override{if(!p)return E_POINTER;*p=nullptr;return gone?S_OK:UiaHostProviderFromHwnd(island->accessWindow(),p);}
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,IRawElementProviderFragment** p)override{if(!p)return E_POINTER;*p=nullptr;if(gone)return S_OK;
        if(direction==NavigateDirection_FirstChild||direction==NavigateDirection_LastChild){const auto items=island->accessTargets();if(!items.empty())*p=new Item(island,this,direction==NavigateDirection_FirstChild?items.front().action:items.back().action);}
        return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** id)override{if(!id)return E_POINTER;*id=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* r)override{if(!r)return E_POINTER;*r={};if(gone)return S_OK;const RECT b=island->accessBody();*r={double(b.left),double(b.top),double(b.right-b.left),double(b.bottom-b.top)};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** p)override{if(!p)return E_POINTER;*p=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetFocus()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** p)override{if(!p)return E_POINTER;*p=this;AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x,double y,IRawElementProviderFragment** p)override{if(!p)return E_POINTER;*p=nullptr;if(gone)return S_OK;
        const Action a=island->accessAt(LONG(std::lround(x)),LONG(std::lround(y)));if(a!=Action::None)*p=new Item(island,this,a);else{*p=this;AddRef();}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** p)override{if(!p)return E_POINTER;*p=nullptr;return S_OK;}
    // IExpandCollapseProvider: the island opens (and closes) like a menu.
    HRESULT STDMETHODCALLTYPE Expand()override{if(gone)return UIA_E_ELEMENTNOTAVAILABLE;PostMessageW(island->accessWindow(),AccessMessage,2,0);return S_OK;}
    HRESULT STDMETHODCALLTYPE Collapse()override{if(gone)return UIA_E_ELEMENTNOTAVAILABLE;PostMessageW(island->accessWindow(),AccessMessage,3,0);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(ExpandCollapseState* s)override{if(!s)return E_POINTER;if(gone)return UIA_E_ELEMENTNOTAVAILABLE;*s=island->accessExpanded()?ExpandCollapseState_Expanded:ExpandCollapseState_Collapsed;return S_OK;}
};
Item::Item(IslandWindow* w,Root* r,Action a):island(w),root(r),action(a){root->AddRef();}
Item::~Item(){root->Release();}
bool Item::alive()const{if(root->gone)return false;for(auto& t:island->accessTargets())if(t.action==action)return true;return false;}
HRESULT Item::GetPropertyValue(PROPERTYID id,VARIANT* v){
    if(!v)return E_POINTER;VariantInit(v);if(!alive())return UIA_E_ELEMENTNOTAVAILABLE;const Role role=roleOf(action);
    switch(id){
    case UIA_NamePropertyId:v->vt=VT_BSTR;v->bstrVal=bstr(island->accessibleName(action));break;
    case UIA_ControlTypePropertyId:v->vt=VT_I4;v->lVal=role==Role::Slider?UIA_SliderControlTypeId:UIA_ButtonControlTypeId;break;
    case UIA_AutomationIdPropertyId:v->vt=VT_BSTR;v->bstrVal=bstr(L"action-"+std::to_wstring(int(action)));break;
    case UIA_IsEnabledPropertyId:{bool enabled=true;for(auto& t:island->accessTargets())if(t.action==action){enabled=t.enabled;break;}v->vt=VT_BOOL;v->boolVal=enabled?VARIANT_TRUE:VARIANT_FALSE;break;}
    case UIA_IsKeyboardFocusablePropertyId:v->vt=VT_BOOL;v->boolVal=VARIANT_FALSE;break;
    case UIA_IsControlElementPropertyId:case UIA_IsContentElementPropertyId:v->vt=VT_BOOL;v->boolVal=VARIANT_TRUE;break;
    default:break;}
    return S_OK;
}
HRESULT Item::Navigate(NavigateDirection direction,IRawElementProviderFragment** p){
    if(!p)return E_POINTER;*p=nullptr;if(root->gone)return S_OK;
    if(direction==NavigateDirection_Parent){*p=root;root->AddRef();return S_OK;}
    if(direction!=NavigateDirection_NextSibling&&direction!=NavigateDirection_PreviousSibling)return S_OK;
    const auto items=island->accessTargets();for(size_t i=0;i<items.size();++i)if(items[i].action==action){
        if(direction==NavigateDirection_NextSibling&&i+1<items.size())*p=new Item(island,root,items[i+1].action);
        else if(direction==NavigateDirection_PreviousSibling&&i>0)*p=new Item(island,root,items[i-1].action);break;}
    return S_OK;
}
HRESULT Item::get_BoundingRectangle(UiaRect* r){
    if(!r)return E_POINTER;*r={};for(auto& t:island->accessTargets())if(t.action==action){const RECT b=island->accessRect(t);*r={double(b.left),double(b.top),double(b.right-b.left),double(b.bottom-b.top)};return S_OK;}
    return UIA_E_ELEMENTNOTAVAILABLE;
}
HRESULT Item::get_FragmentRoot(IRawElementProviderFragmentRoot** p){if(!p)return E_POINTER;*p=root;root->AddRef();return S_OK;}
}
// ---- The island's side -----------------------------------------------------------------------------
// What can be pressed now, in the body's coordinates (one per action, in reading order).
std::vector<HitTarget> IslandWindow::accessTargets()const{
    if(!renderer_)return {};const double now=seconds();const bool compact=state_==IslandState::Compact;
    auto all=renderer_->accessibleTargets(compact,float((motion_.width.sample(now).position-motion_.compactWidth)/2));
    std::vector<HitTarget> out;for(auto& t:all)if(t.action!=Action::None&&std::none_of(out.begin(),out.end(),[&](auto& o){return o.action==t.action;}))out.push_back(t);
    // Reading order: row by row (by their middles, 18 DIPs to a row), left to right.
    std::stable_sort(out.begin(),out.end(),[](const HitTarget& a,const HitTarget& b){const int ra=int((a.y+a.height/2)/18),rb=int((b.y+b.height/2)/18);return ra!=rb?ra<rb:a.x<b.x;});
    // The bud of a waiting alert (it brings that alert forward).
    if(content_.bud.kind&&dropped(now)&&motion_.bud.sample(now).position>=.6){const double w=motion_.width.sample(now).position,h=motion_.height.sample(now).position;const auto o=bodyAt(w,h,motion_.drop.sample(now).position);const auto b=budNow(now,o,w,h);
        out.push_back({Action::BudPromote,float(b.left-o.x),float(b.top-o.y),float(b.right-b.left),float(b.bottom-b.top)});}
    return out;
}
RECT IslandWindow::accessRect(const HitTarget& t)const{
    const double now=seconds();const auto o=bodyAt(motion_.width.sample(now).position,motion_.height.sample(now).position,motion_.drop.sample(now).position);
    const double k=dpi_/96.,x=(o.x+motion_.dragX.sample(now).position+t.x)*k,y=(o.y+motion_.dragY.sample(now).position+t.y)*k;
    POINT a{LONG(std::lround(x)),LONG(std::lround(y))},b{LONG(std::lround(x+t.width*k)),LONG(std::lround(y+t.height*k))};ClientToScreen(window_,&a);ClientToScreen(window_,&b);return {a.x,a.y,b.x,b.y};
}
RECT IslandWindow::accessBody()const{
    const double now=seconds();const double w=motion_.width.sample(now).position,h=motion_.height.sample(now).position;return accessRect({Action::None,0,0,float(w),float(h)});
}
Action IslandWindow::accessAt(LONG x,LONG y){POINT p{x,y};ScreenToClient(window_,&p);return hit(MAKELPARAM(p.x,p.y));}
bool IslandWindow::accessExpanded()const{return state_!=IslandState::Compact;}
bool IslandWindow::accessToggled(Action a)const{
    const auto& c=content_.controls;
    switch(a){case Action::ControlWifi:return c.wifi==1;case Action::ControlBluetooth:return c.bluetooth==1;case Action::ControlAirplane:return c.wifi==0&&c.bluetooth==0;case Action::ControlDark:return c.dark==1;
    case Action::ControlFocus:return content_.focus.running;case Action::ControlMic:return content_.micAvailable&&!content_.micMuted;case Action::Mute:return content_.muted;case Action::MicMute:return content_.micMuted;
    case Action::Pin:return content_.pinned;case Action::LyricsToggle:return content_.lyricsView;case Action::ClipboardPause:return content_.clipsPaused;default:return false;}
}
void IslandWindow::accessRange(Action a,double& value,double& lo,double& hi)const{
    lo=0;hi=100;value=0;const auto& p=content_.playback;
    if(a==Action::VolumeSlider)value=content_.muted?0:content_.volume;
    else if(a==Action::ControlBrightness)value=std::max(0,content_.brightness);
    else if(a==Action::Seek){hi=std::max(1.,p.duration);value=std::clamp(p.position+(p.playing?std::max(0.,seconds()-p.sampledAt):0.),0.,hi);}
    else if(inRange(a,Action::MixerSliderBase,Action::MixerMuteBase)){const size_t i=size_t(int(a)-int(Action::MixerSliderBase));if(i<content_.mixer.size())value=std::lround(content_.mixer[i].volume*100);}
}
std::wstring IslandWindow::accessibleName(Action a)const{
    const auto& p=content_.playback;auto row=[&](Action base){return size_t(int(a)-int(base));};
    auto peer=[&](Action base)->std::wstring{const size_t i=row(base);return i<content_.nearby.size()?content_.nearby[i].name:std::wstring(L"that PC");};
    switch(a){
    // Navigation.
    case Action::Overview:return L"Home";case Action::Media:return L"Media";case Action::System:return L"Stats";case Action::Focus:return L"Focus";case Action::Settings:return L"Settings";case Action::Shelf:return L"Shelf";case Action::Audio:return L"Audio";case Action::Control:return L"Controls";
    case Action::Close:return L"Close";case Action::Pin:return content_.pinned?L"Unpin":L"Keep open";case Action::CommandOpen:return L"Search and commands";
    // Media.
    case Action::Play:return p.playing?L"Pause "+p.title:L"Play "+p.title;case Action::Previous:return L"Previous track";case Action::Next:return L"Next track";
    case Action::MediaMode:return L"Media layout, "+std::wstring(settings_.mediaLayout==0?L"automatic":settings_.mediaLayout==1?L"music":L"video");
    case Action::SkipBack:return L"Back 10 seconds (double-click the cover)";case Action::SkipForward:return L"Forward 10 seconds (double-click the cover)";
    case Action::Seek:return L"Position in "+p.title;case Action::Mute:return L"Mute";case Action::VolumeSlider:return L"Volume";case Action::LyricsToggle:return L"Lyrics";
    case Action::LibraryOpen:return L"Library";case Action::LibraryBack:return L"Back to Now playing";case Action::LibraryShuffle:return L"Shuffle all songs";case Action::LibraryUp:return L"Earlier songs";case Action::LibraryDown:return L"More songs";
    case Action::HandoffOpen:return L"Continue on another PC";case Action::HandoffPlay:return L"Play here";case Action::HandoffDecline:return L"Not now";
    case Action::UpNextOpen:return L"Up next";case Action::UpNextBack:return L"Back to Now playing";case Action::UpNextUp:return L"Earlier in the queue";case Action::UpNextDown:return L"Later in the queue";
    // Stats and devices.
    case Action::StatsSystem:return L"System";case Action::StatsBattery:return L"Battery";case Action::StatsDevices:return L"Devices";case Action::PowerSettings:return L"Power and battery settings";case Action::BluetoothSettings:return L"Bluetooth settings";case Action::Armoury:return L"Armoury Crate";
    // Controls.
    case Action::ControlWifi:return L"Wi-Fi";case Action::ControlBluetooth:return L"Bluetooth";case Action::ControlAirplane:return L"Airplane mode";case Action::ControlDark:return L"Dark mode";case Action::ControlFocus:return L"Focus session";case Action::ControlMic:return L"Microphone";case Action::ControlBrightness:return L"Brightness";
    // Focus.
    case Action::Timer25:return L"Focus timer";case Action::Timer5:return L"Break timer";case Action::Stopwatch:return L"Stopwatch";case Action::TimerToggle:return content_.focus.running?L"Pause session":L"Start session";case Action::TimerReset:return L"Reset";
    // Audio.
    case Action::AudioApps:return L"Apps";case Action::AudioOutputs:return L"Outputs";case Action::MixerSettings:return L"Sound mixer settings";case Action::SoundSettings:return L"Sound settings";case Action::MicMute:return L"Microphone muted";
    // Shelf.
    case Action::ShelfFiles:return L"Files";case Action::ShelfClipboard:return L"Clipboard";case Action::ShelfNearby:return L"Nearby";case Action::ShelfClear:return L"Clear the Shelf";case Action::ShelfZip:return L"Zip";case Action::ShelfStack:return L"Every file on the Shelf";
    case Action::ShelfBack:return L"Back";case Action::ShelfOpen:return L"Open";case Action::ShelfOpenWith:return L"Open with";case Action::ShelfReveal:return L"Show in folder";case Action::ShelfCopyPath:return L"Copy path";case Action::ShelfCopyText:return L"Copy text in the picture";
    case Action::ShelfConvert:return L"Convert";case Action::ShelfHalf:return L"Half size";case Action::ShelfRemove:return L"Remove from the Shelf";case Action::CaptureSnip:return L"Snip the screen";case Action::CaptureText:return L"Copy text from the screen";case Action::CaptureColour:return L"Pick a colour";
    case Action::ClipboardEnable:return L"Turn on clipboard history";case Action::ClipboardPause:return L"Pause clipboard history";case Action::ClipboardClear:return L"Clear clipboard history";case Action::ClipSearch:return L"Search copies";
    case Action::DropShelf:return L"Keep on the Shelf";case Action::ShareSend:return L"Send to your PC";case Action::ShareAccept:return L"Accept";case Action::ShareDecline:return L"Decline";case Action::SharePair:return L"Pair";case Action::ShareShow:return L"Show";
    case Action::RemoteShelfBack:return L"Back to Nearby";case Action::RemoteShelfRefresh:return L"Look again";case Action::RemoteShelfUp:return L"Earlier items";case Action::RemoteShelfDown:return L"More items";
    // Cards.
    case Action::SwitchBack:return L"Switch back";case Action::PrivacyShow:return L"Who is using the camera, microphone or location";case Action::PrivacySettings:return L"Privacy settings";case Action::NoticeDismiss:return L"Dismiss";
    case Action::BudPromote:return L"Show the waiting alert: "+content_.bud.title;
    case Action::BatteryDetails:return content_.batteryDetails?L"Battery overview":L"All battery details";
    case Action::WeatherOpen:return L"Weather details";case Action::WeatherBack:return L"Back to Home";case Action::UpdateRestart:return L"Restart to update";
    case Action::BatteryChart:return content_.batteryHealthChart?L"Show the last 24 hours":L"Show health over time";case Action::WhatsNewOpen:return L"What\u2019s new";case Action::WhatsNewBack:return L"Close What\u2019s new";
    default:break;}
    // Rows.
    if(inRange(a,Action::ShelfItemBase,Action::MixerSliderBase)){const size_t i=row(Action::ShelfItemBase);return i<content_.shelf.size()?content_.shelf[i].label:L"Shelf item";}
    if(inRange(a,Action::MixerSliderBase,Action::MixerMuteBase)){const size_t i=row(Action::MixerSliderBase);return i<content_.mixer.size()?content_.mixer[i].name+L" volume":L"App volume";}
    if(inRange(a,Action::MixerMuteBase,Action::SessionBase)){const size_t i=row(Action::MixerMuteBase);return i<content_.mixer.size()?L"Mute "+content_.mixer[i].name:L"Mute app";}
    if(inRange(a,Action::SessionBase,Action::DeviceConnectBase)){const size_t i=row(Action::SessionBase);return i<content_.sessions.size()?L"Switch to "+content_.sessions[i].title:L"Switch player";}
    if(inRange(a,Action::DeviceConnectBase,Action::DeviceConnectEnd)){const size_t i=row(Action::DeviceConnectBase);return i<content_.devices.size()?(content_.devices[i].connected?L"Disconnect ":L"Connect ")+content_.devices[i].name:L"Connect";}
    if(inRange(a,Action::DeviceBase,Action::ShelfItemBase)){const size_t i=row(Action::DeviceBase);return i<content_.outputs.size()?L"Play sound on "+content_.outputs[i].name:L"Output";}
    if(inRange(a,Action::ClipBase,Action::CommandResultBase)){const size_t i=size_t(content_.clipOffset)+row(Action::ClipBase);return i<content_.clips.size()?(content_.clips[i].secret?std::wstring(L"Hidden copy"):L"Copy: "+content_.clips[i].preview):L"Copy";}
    if(inRange(a,Action::CommandResultBase,Action::CommandResultEnd)){const size_t i=row(Action::CommandResultBase);return i<content_.command.results.size()?content_.command.results[i].title:L"Result";}
    if(inRange(a,Action::ClipPinBase,Action::ClipPinEnd))return L"Pin this copy";
    if(inRange(a,Action::LyricLineBase,Action::LyricLineEnd))return L"Jump to this line";
    if(inRange(a,Action::NearbyBase,Action::NearbyEnd)){const size_t i=row(Action::NearbyBase);if(i<content_.nearby.size()){const auto& n=content_.nearby[i];return n.name+(n.paired?(n.online?L", paired":L", paired, away"):L", pair");}return L"PC";}
    if(inRange(a,Action::NearbyForgetBase,Action::NearbyForgetEnd))return L"Forget "+peer(Action::NearbyForgetBase);
    if(inRange(a,Action::NearbySendBase,Action::NearbySendEnd))return L"Send the Shelf to "+peer(Action::NearbySendBase);
    if(inRange(a,Action::NearbyCancelBase,Action::NearbyCancelEnd))return L"Stop the transfer with "+peer(Action::NearbyCancelBase);
    if(inRange(a,Action::NearbyBrowseBase,Action::NearbyBrowseEnd))return peer(Action::NearbyBrowseBase)+L"’s Shelf";
    if(inRange(a,Action::HandoffPeerBase,Action::HandoffPeerEnd))return L"Continue on "+peer(Action::HandoffPeerBase);
    if(inRange(a,Action::LibraryItemBase,Action::LibraryItemEnd)){const size_t i=size_t(content_.libraryOffset)+row(Action::LibraryItemBase);if(content_.libraryTracks&&i<content_.libraryTracks->size()){const auto& t=(*content_.libraryTracks)[i];return L"Play "+t.title+(t.artist.empty()?L"":L" by "+t.artist);}return L"Song";}
    if(inRange(a,Action::UpNextItemBase,Action::UpNextItemEnd)){const size_t i=size_t(content_.upNextOffset)+row(Action::UpNextItemBase);if(i<content_.upNextTracks.size()){const auto& t=content_.upNextTracks[i];return L"Play now: "+t.title+(t.artist.empty()?L"":L" by "+t.artist)+L", number "+std::to_wstring(i+1)+L" up next";}return L"Song";}
    if(inRange(a,Action::RemoteItemBase,Action::RemoteItemEnd)){const size_t i=size_t(content_.remote.offset)+row(Action::RemoteItemBase);return i<content_.remote.items.size()?L"Take a copy of "+content_.remote.items[i].name:L"Item";}
    return L"Button";
}
namespace {
// Text as it should sound: the middle dots that separate details read as pauses.
std::wstring spoken(std::wstring t){for(const wchar_t* dot:{L"  ·  ",L" · ",L"·"})for(size_t at;(at=t.find(dot))!=std::wstring::npos;)t.replace(at,wcslen(dot),L", ");return t;}
}
// An alert as a sentence, with what can be done about it.
std::wstring IslandWindow::accessAlert(const ContentSnapshot::Notice& n)const{
    switch(n.kind){
    case 14:return spoken(L"Pair with "+n.app+L"? The code on both PCs is "+n.detail+L". Pair, or not now");
    case 15:return spoken(n.app+L" is sending "+n.detail+L". Accept, or decline");
    case 16:return spoken(n.app+(n.detail.empty()?L"":L". "+n.detail));
    case 17:return spoken(L"Music from another PC: "+n.app+L", "+n.detail+L". Play here, or not now");
    default:return spoken(budTitle(n)+(n.detail.empty()?L"":L". "+n.detail));}
}
// The island in a sentence, for the pane's status.
std::wstring IslandWindow::accessSummary()const{
    const auto& p=content_.playback;
    if(state_==IslandState::Notification&&content_.notice.kind)return accessAlert(content_.notice);
    if(content_.command.active)return L"Command bar";
    std::wstring s=state_==IslandState::Compact?std::wstring(L"Closed"):L"Open on "+std::wstring(pageNames()[size_t(std::clamp(int(content_.page),0,pageCount-1))]);
    if(p.available)s+=L". "+std::wstring(p.playing?L"Playing ":L"Paused: ")+p.title+(p.artist.empty()?L"":L" by "+p.artist);
    return s;
}
LRESULT IslandWindow::accessObject(WPARAM w,LPARAM l){
    if(static_cast<long>(l)!=static_cast<long>(UiaRootObjectId))return DefWindowProcW(window_,WM_GETOBJECT,w,l);
    if(!accessRoot_)accessRoot_.Attach(static_cast<IRawElementProviderSimple*>(new access::Root(this)));
    return UiaReturnRawElementProvider(window_,w,l,static_cast<IRawElementProviderSimple*>(accessRoot_.Get()));
}
void IslandWindow::accessClose(){
    if(!accessRoot_)return;static_cast<access::Root*>(static_cast<IRawElementProviderSimple*>(accessRoot_.Get()))->gone=true;
    UiaReturnRawElementProvider(window_,0,0,nullptr);UiaDisconnectProvider(static_cast<IRawElementProviderSimple*>(accessRoot_.Get()));accessRoot_.Reset();
}
bool IslandWindow::accessMessage(UINT m,WPARAM w,LPARAM l){
    if(m!=AccessMessage)return false;const WORD what=LOWORD(w);const Action a=Action(int(HIWORD(w)));
    if(what==2){if(state_==IslandState::Compact){content_.pinned=true;transition(IslandState::Expanded);refresh();}return true;}
    if(what==3){if(state_!=IslandState::Compact)perform(Action::Close);return true;}
    if(what==1){const double v=double(l)/1000;
        if(a==Action::VolumeSlider&&audio_)audio_->setVolume(int(std::lround(v)));
        else if(a==Action::ControlBrightness&&brightness_&&content_.brightness>=0){brightness_->set(int(std::lround(v)));content_.brightness=int(std::lround(v));brightnessRequestAt_=seconds();refresh();}
        else if(a==Action::Seek&&content_.playback.canSeek){mediaSeek(v);content_.playback.position=v;content_.playback.sampledAt=seconds();refresh();}
        else if(inRange(a,Action::MixerSliderBase,Action::MixerMuteBase)){const size_t i=size_t(int(a)-int(Action::MixerSliderBase));if(mixer_&&i<content_.mixer.size()){auto& e=content_.mixer[i];e.volume=float(std::clamp(v/100,0.,1.));mixer_->setVolume(e.pid,e.volume);refresh();}}
        return true;}
    // A press: what the pointer would do (a switch, a button, a row).
    if(a==Action::Seek||a==Action::VolumeSlider||a==Action::ControlBrightness)return true;
    perform(a);return true;
}
void IslandWindow::announce(const std::wstring& text,bool important){
    if(!accessRoot_||text.empty()||!UiaClientsAreListening())return;BSTR words=bstr(text),id=bstr(important?L"island-alert":L"island-change");
    const HRESULT hr=UiaRaiseNotificationEvent(static_cast<IRawElementProviderSimple*>(accessRoot_.Get()),NotificationKind_Other,important?NotificationProcessing_ImportantMostRecent:NotificationProcessing_MostRecent,words,id);
    // Only whether it was said, never what.
    if(FAILED(hr))store_.log("Warning","access_announce_failed");else if(testing_)store_.log("Info",important?"access_alert_announced":"access_change_announced");
    SysFreeString(words);SysFreeString(id);
}
// After each redraw: what changed, said (an alert, the page that opened, the song that began, a level), and the
// tree marked stale when what can be pressed changed.
void IslandWindow::accessChanged(){
    if(!accessRoot_||!UiaClientsAreListening())return;
    const auto& n=content_.notice;const bool card=state_==IslandState::Notification&&n.kind;
    const std::wstring alert=card?accessAlert(n)+(content_.bud.kind?L". Another alert waits: "+spoken(content_.bud.title):L""):L"";
    const std::wstring view=state_==IslandState::Compact?L"":content_.command.active?L"Command bar":content_.upNext&&content_.page==Page::Media?L"Up next":content_.library&&content_.page==Page::Media?L"Library":std::wstring(pageNames()[size_t(std::clamp(int(content_.page),0,pageCount-1))]);
    const std::wstring song=content_.playback.available&&content_.playback.playing?spoken(content_.playback.title+(content_.playback.artist.empty()?L"":L" by "+content_.playback.artist)):L"";
    const int level=content_.hud==1?(content_.muted?-1:content_.volume):content_.hud==2?1000+content_.brightness:-2;
    if(alert!=accessAlert_&&!alert.empty())announce(alert,true);
    else if(view!=accessView_&&!view.empty())announce(view+(accessView_.empty()?L", open":L""));
    else if(view.empty()&&!accessView_.empty()&&alert.empty())announce(L"Closed");
    if(song!=accessSong_&&!song.empty()&&!accessSong_.empty()&&alert.empty())announce(L"Now playing "+song);
    if(level!=accessLevel_&&level!=-2)announce(level==-1?std::wstring(L"Muted"):level>=1000?L"Brightness "+std::to_wstring(level-1000):L"Volume "+std::to_wstring(level));
    const bool structure=alert!=accessAlert_||view!=accessView_||content_.shelfTab!=accessTab_||content_.statsTab!=accessTab2_;
    accessAlert_=alert;accessView_=view;accessSong_=song;accessLevel_=level;accessTab_=content_.shelfTab;accessTab2_=content_.statsTab;
    if(structure)UiaRaiseStructureChangedEvent(static_cast<IRawElementProviderSimple*>(accessRoot_.Get()),StructureChangeType_ChildrenInvalidated,nullptr,0);
}
}
