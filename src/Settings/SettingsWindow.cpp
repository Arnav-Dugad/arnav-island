#include "Audio/Sounds.h"
#include "SettingsWindow.h"
#include "Animation/MotionEngine.h"
#include "Design/Icons.h"
#include "Design/Type.h"
#include "Composition/GlassBackdrop.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <shellapi.h>
#include <windowsx.h>
#include <map>
#include <optional>
namespace nexus {
namespace {
constexpr UINT RefreshMessage=WM_APP+1,ShowMessage=WM_APP+2;
constexpr float Sidebar=236,Pad=20,TitleTop=30,CardTop=112;
constexpr SpringSpec Knob{1,520,36},Pill{.9,480,36},Hover{1,700,52},Page{1,300,32},Scroll{1,260,34},Indicator{.8,420,32};
const wchar_t* subtitles[]={L"How the island behaves while you work",L"Size, position and everyday mode",L"Theme, glass and color",L"Springs, feedback and accessibility",L"What the resting island shows",L"Players, logos and audio output",L"Bluetooth, battery and performance",L"Arrange the Command Center",L"Clipboard, privacy dots, commands and workspaces",L"Version, diagnostics and reset"};
const Icon sectionIcons[]={Icon::Settings,Icon::Island,Icon::Sun,Icon::Spark,Icon::Stats,Icon::Music,Icon::Bluetooth,Icon::Home,Icon::Shield,Icon::Info};
const UINT32 swatchColors[]={0xa4deca,0xa6cafa,0xccb8f1,0xefc7a6};
struct Palette {UINT32 bg,card,border,ink,muted,accent,onAccent,pill;float bgAlpha,cardAlpha;bool light;};
bool systemLight(){DWORD light=0,size=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);return light!=0;}
struct Row {int item=-1;D2D1_RECT_F row{},control{};std::vector<D2D1_RECT_F> parts;};
struct Hit {int item=-1,part=-1,section=-1;bool operator==(const Hit&)const=default;};
float lerp(float a,float b,float t){return a+(b-a)*t;}
bool inside(const D2D1_RECT_F& r,float x,float y){return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;}
RECT pixels(const D2D1_RECT_F& r,float scale){return {LONG(std::lround(r.left*scale)),LONG(std::lround(r.top*scale)),LONG(std::lround(r.right*scale)),LONG(std::lround(r.bottom*scale))};}
}
struct SettingsWindow::State {
    std::mutex mutex;Settings incoming;SettingsContext incomingContext;unsigned incomingSequence=0;bool pending=false;int requestedSection=-1;
    std::vector<Probe> probes;bool settled=true;int section=0;
};

class SettingsUi {
public:
    SettingsUi(HWND island,SettingsWindow::State& shared,Settings s,SettingsContext c,int section):island_(island),shared_(shared),s_(s),context_(c),section_(std::max(0,section)){items_=settingItems(c.monitors);if(section_==3)replay();}
    HWND create();
    LRESULT message(HWND,UINT,WPARAM,LPARAM);
    bool animating();void render();bool dirty=true;
private:
    HWND island_,hwnd_=nullptr;SettingsWindow::State& shared_;Settings s_;SettingsContext context_;std::vector<SettingItem> items_;int section_=0;unsigned posted_=0;
    ComPtr<ID3D11Device> d3d_;ComPtr<IDXGISwapChain1> swap_;ComPtr<ID2D1Factory1> factory_;ComPtr<ID2D1Device> device2d_;ComPtr<ID2D1DeviceContext> dc_;ComPtr<IDWriteFactory> write_;
    ComPtr<IDCompositionDevice> composition_;ComPtr<IDCompositionTarget> target_;ComPtr<IDCompositionVisual> visual_;ComPtr<ID2D1SolidColorBrush> brush_;
    std::map<int,ComPtr<IDWriteTextFormat>> formats_;float dpi_=96;UINT width_=0,height_=0;bool mica_=false,keyboard_=false;
    Spring navY_{0},page_{1},scroll_{0};double scrollTarget_=0;std::map<int,Spring> knobs_,hovers_,pillX_,pillW_,thumbs_,rings_;
    // A chip being dragged in the Chips control: its slot, where it was grabbed and the pointer.
    int chipDrag_=-1,chipHeard_=-1;float chipGrab_=0,chipX_=0;int chipTarget(const Row&)const;
    Hit hover_,press_,focus_;int dragItem_=-1;double confirmUntil_=0;int confirmItem_=-1;bool tracking_=false;
    // Animation Lab: a spring that plays the island's own motion in miniature.
    Spring preview_{0};double previewStart_=-10;bool previewForward_=false;void replay();void drawPreview(const D2D1_RECT_F& area,const Palette& p,float alpha);
    float W()const{return width_*96.f/dpi_;}float H()const{return height_*96.f/dpi_;}
    Palette palette()const;bool enabled(const SettingItem&)const;std::wstring detail(const SettingItem&)const;
    IDWriteTextFormat* format(float size,DWRITE_FONT_WEIGHT weight);float measure(const std::wstring&,float size,DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_NORMAL);
    void text(const std::wstring&,D2D1_RECT_F,float size,UINT32 color,float alpha=1,DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING);
    void fill(D2D1_RECT_F,float radius,UINT32 color,float alpha);void stroke(D2D1_RECT_F,float radius,UINT32 color,float alpha,float width=1);
    std::vector<Row> layout(float& contentHeight,std::vector<D2D1_RECT_F>* nav=nullptr);std::vector<D2D1_RECT_F> navRects();
    Hit hit(float x,float y);void publish();void resize();void theme();
    void apply(int item,int value);void activate(const Hit&,float x);void slide(int item,float x);void post();void selectSection(int);void key(WPARAM);
    Spring& spring(std::map<int,Spring>& map,int key,double initial){auto it=map.find(key);if(it==map.end())it=map.emplace(key,Spring(initial)).first;return it->second;}
    void aim(Spring& spring,double target,SpringSpec spec){double now=seconds();if(s_.reduceMotion)spring.reset(target,now);else if(std::abs(spring.target()-target)>1e-4)spring.retarget(target,now,spec);}
    double at(Spring& spring){return spring.sample(seconds()).position;}
    std::vector<Hit> focusOrder();void refresh();bool about()const{return section_==int(settingSections().size())-1;}
};

Palette SettingsUi::palette()const{
    bool light=s_.theme==1||(s_.theme==2&&systemLight());UINT32 accent=s_.accent==4?(context_.wallpaper?context_.wallpaper:swatchColors[0]):swatchColors[std::clamp(s_.accent,0,3)];
    // The wallpaper swatch darkens its colour for light windows, as its swatch does.
    if(light){const UINT32 deep[]={0x2e7d68,0x2f6fb8,0x7453b8,0xb4652c};const UINT32 wall=context_.wallpaper?context_.wallpaper:0x9aa0aa;const UINT32 deepWall=((((wall>>16)&255)*5/10)<<16)|((((wall>>8)&255)*5/10)<<8)|((wall&255)*5/10);
        return {0xf3f3f6,0xffffff,0xe2e3e8,0x1b1c20,0x696c75,s_.accent==4?deepWall:deep[std::clamp(s_.accent,0,3)],0xffffff,0xffffff,mica_?0.f:1.f,mica_?.72f:1.f,true};}
    return {0x141518,0x1e1f24,0x2b2d33,0xf2f3f6,0x9b9ea8,accent,0x101114,0x34363d,mica_?0.f:1.f,mica_?.62f:1.f,false};
}
bool SettingsUi::enabled(const SettingItem& i)const{
    if(i.key=="glassTint")return s_.material!=0;
    if(i.key=="compactWidth")return s_.uiMode!=0;
    if(i.key=="hoverDelay")return s_.hoverOpen;
    if(i.key=="accent")return true;
    return true;
}
std::wstring SettingsUi::detail(const SettingItem& i)const{
    if(i.action==SettingAction::TransparencySettings)return context_.blur?L"On — Frosted glass blurs what is behind the island":L"Off — Frosted glass uses a translucent frost; turn on for real blur";
    if(i.key=="material"&&!context_.glassAvailable)return L"Glass needs Windows 11 composition support";
    if(i.key=="commandShortcut"&&context_.shortcutTaken)return L"Another app already uses this shortcut \u2014 choose another";
    if(i.key=="captureShortcuts"&&!context_.captureTaken.empty()&&s_.captureShortcuts)return L"Another app already uses "+context_.captureTaken+L"; the rest work";
    return i.detail;
}
IDWriteTextFormat* SettingsUi::format(float size,DWRITE_FONT_WEIGHT weight){
    int key=int(size*10)*1000+int(weight);auto it=formats_.find(key);if(it!=formats_.end())return it->second.Get();
    ComPtr<IDWriteTextFormat> f;check(write_->CreateTextFormat(fontFamily(size),nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f));
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};ComPtr<IDWriteInlineObject> ellipsis;write_->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    formats_[key]=f;return f.Get();
}
float SettingsUi::measure(const std::wstring& value,float size,DWRITE_FONT_WEIGHT weight){ComPtr<IDWriteTextLayout> l;if(FAILED(write_->CreateTextLayout(value.c_str(),UINT32(value.size()),format(size,weight),4096,64,&l)))return value.size()*size*.55f;DWRITE_TEXT_METRICS m{};l->GetMetrics(&m);return m.widthIncludingTrailingWhitespace;}
void SettingsUi::text(const std::wstring& value,D2D1_RECT_F r,float size,UINT32 color,float alpha,DWRITE_FONT_WEIGHT weight,DWRITE_TEXT_ALIGNMENT align){auto* f=format(size,weight);f->SetTextAlignment(align);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);brush_->SetColor(D2D1::ColorF(color,alpha));dc_->DrawText(value.c_str(),UINT32(value.size()),f,r,brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);}
void SettingsUi::fill(D2D1_RECT_F r,float radius,UINT32 color,float alpha){if(alpha<=.001f)return;brush_->SetColor(D2D1::ColorF(color,alpha));dc_->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush_.Get());}
void SettingsUi::stroke(D2D1_RECT_F r,float radius,UINT32 color,float alpha,float width){if(alpha<=.001f)return;brush_->SetColor(D2D1::ColorF(color,alpha));float h=width/2;dc_->DrawRoundedRectangle(D2D1::RoundedRect({r.left+h,r.top+h,r.right-h,r.bottom-h},radius,radius),brush_.Get(),width);}

std::vector<D2D1_RECT_F> SettingsUi::navRects(){std::vector<D2D1_RECT_F> r;for(size_t i=0;i<settingSections().size();++i){float y=104+i*44.f;r.push_back({12,y,Sidebar-12,y+40});}return r;}
std::vector<Row> SettingsUi::layout(float& contentHeight,std::vector<D2D1_RECT_F>* nav){
    if(nav)*nav=navRects();
    std::vector<Row> rows;const float left=Sidebar+20,right=W()-32,R=right-Pad,scroll=float(at(scroll_));float y=CardTop-scroll+(about()?92:0);
    for(size_t index=0;index<items_.size();++index){auto& item=items_[index];if(item.section!=section_||(item.action==SettingAction::OpenArmoury&&!context_.armoury))continue;
        Row row;row.item=int(index);float h=item.control==SettingControl::Order?52:item.control==SettingControl::Preview?216:item.control==SettingControl::Chips?122:64;row.row={left,y,right,y+h};float cy=y+h/2;
        switch(item.control){
        case SettingControl::Toggle:row.control={R-44,cy-11,R,cy+11};row.parts={row.control};break;
        case SettingControl::Slider:row.control={R-220,cy-12,R,cy+12};row.parts={row.control};break;
        case SettingControl::Choice:{float total=0;std::vector<float> widths;for(auto& o:item.options){float w=std::max(66.f,measure(o,13,DWRITE_FONT_WEIGHT_MEDIUM)+30);widths.push_back(w);total+=w;}float x=R-total-4;row.control={x,cy-17,R,cy+17};x+=2;for(float w:widths){row.parts.push_back({x,cy-15,x+w,cy+15});x+=w;}break;}
        case SettingControl::Stepper:row.control={R-190,cy-16,R,cy+16};row.parts={{R-190,cy-16,R-158,cy+16},{R-32,cy-16,R,cy+16}};break;
        case SettingControl::Swatch:{float x=R-float(item.options.size())*38+10;row.control={x,cy-14,R,cy+14};for(size_t k=0;k<item.options.size();++k)row.parts.push_back({x+k*38.f,cy-14,x+k*38.f+28,cy+14});break;}
        case SettingControl::Button:{float w=std::max(96.f,measure(confirmItem_==int(index)?L"Click again to confirm":item.options.front(),13,DWRITE_FONT_WEIGHT_MEDIUM)+36);row.control={R-w,cy-16,R,cy+16};row.parts={row.control};break;}
        case SettingControl::Order:row.control={R-72,cy-15,R,cy+15};row.parts={{R-72,cy-15,R-40,cy+15},{R-32,cy-15,R,cy+15}};break;
        case SettingControl::Preview:row.control={left+Pad,y+44,R,y+h-14};row.parts={row.control};break;
        // A strip like the compact island, with the chips in their order.
        case SettingControl::Chips:{const float x0=left+Pad;row.control={x0,y+62,R,y+106};const float gap=6,w=(R-x0-12-gap*(chipCount-1))/chipCount;for(int k=0;k<chipCount;++k){float x=x0+6+k*(w+gap);row.parts.push_back({x,y+68,x+w,y+100});}break;}
        case SettingControl::Actions:{float total=0;std::vector<float> widths;for(auto& o:item.options){float w=std::max(74.f,measure(o,13,DWRITE_FONT_WEIGHT_MEDIUM)+32);widths.push_back(w);total+=w+8;}total-=8;float x=R-total;row.control={x,cy-16,R,cy+16};for(float w:widths){row.parts.push_back({x,cy-16,x+w,cy+16});x+=w+8;}break;}
        default:break;
        }
        rows.push_back(std::move(row));y+=h;
    }
    contentHeight=(y+scroll)+40;return rows;
}
Hit SettingsUi::hit(float x,float y){
    float height;std::vector<D2D1_RECT_F> nav;auto rows=layout(height,&nav);
    for(size_t i=0;i<nav.size();++i)if(inside(nav[i],x,y))return {-1,-1,int(i)};
    if(x<Sidebar)return {};
    for(auto& row:rows){if(y<0||!inside(row.row,x,y))continue;auto& item=items_[row.item];
        for(size_t p=0;p<row.parts.size();++p){auto r=row.parts[p];if(item.control==SettingControl::Slider){r.top-=8;r.bottom+=8;r.left-=8;r.right+=8;}if(inside(r,x,y))return {row.item,int(p),-1};}
        if(item.control==SettingControl::Toggle)return {row.item,99,-1};
        return {row.item,-2,-1};}
    return {};
}
void SettingsUi::publish(){
    float height;std::vector<D2D1_RECT_F> nav;auto rows=layout(height,&nav);float s=dpi_/96;std::vector<SettingsWindow::Probe> probes;
    for(size_t i=0;i<nav.size();++i)probes.push_back({SettingsWindow::Probe::Kind::Section,int(i),pixels(nav[i],s),true,{}});
    for(auto& row:rows){SettingsWindow::Probe p{SettingsWindow::Probe::Kind::Control,row.item,pixels(row.control,s),row.control.top>=CardTop-24&&row.control.bottom<=H()-8,{}};for(auto& r:row.parts)p.parts.push_back(pixels(r,s));probes.push_back(std::move(p));}
    bool still=!animating();
    std::lock_guard lock(shared_.mutex);shared_.probes=std::move(probes);shared_.settled=still;shared_.section=section_;
}
// The slot a dragged chip would land in: under the chip's centre.
int SettingsUi::chipTarget(const Row& row)const{if(row.parts.empty())return 0;const float w=row.parts[0].right-row.parts[0].left,step=chipCount>1?row.parts[1].left-row.parts[0].left:w;const float centre=chipX_-chipGrab_+w/2;return std::clamp(int(std::floor((centre-row.parts[0].left+(step-w)/2)/step)),0,chipCount-1);}
void SettingsUi::post(){++posted_;auto copy=std::make_unique<Settings>(s_);if(PostMessageW(island_,SettingsChangedMessage,posted_,reinterpret_cast<LPARAM>(copy.get())))copy.release();}
void SettingsUi::apply(int index,int value){
    auto& item=items_[index];if(!enabled(item))return;
    if(item.control==SettingControl::Order){item.set(s_,value);post();dirty=true;return;}
    auto spring=bodySpring(s_);int before=item.get(s_);item.set(s_,value);if(item.get(s_)!=before){post();dirty=true;}
    auto after=bodySpring(s_);if(section_==3&&(after.mass!=spring.mass||after.stiffness!=spring.stiffness||after.damping!=spring.damping))replay();
}
// Each replay flips direction: expand, then collapse, with the island's current spring.
void SettingsUi::replay(){double now=seconds();previewForward_=!previewForward_;previewStart_=now;auto spec=bodySpring(s_);if(s_.reduceMotion)preview_.reset(previewForward_?1:0,now);else preview_.retarget(previewForward_?1:0,now,spec);dirty=true;}
void SettingsUi::slide(int index,float x){
    float height;auto rows=layout(height);for(auto& row:rows)if(row.item==index){auto& item=items_[index];auto r=row.control;float f=std::clamp((x-r.left)/(r.right-r.left),0.f,1.f);int v=item.lo+int(std::lround(f*(item.hi-item.lo)/item.step))*item.step;apply(index,std::clamp(v,item.lo,item.hi));}
}
void SettingsUi::selectSection(int section){
    section=std::clamp(section,0,int(settingSections().size())-1);if(section==section_)return;section_=section;double now=seconds();
    if(!s_.reduceMotion){page_.reset(0,now);page_.retarget(1,now,Page);}scrollTarget_=0;scroll_.reset(0,now);confirmItem_=-1;dirty=true;
    if(section_==3){previewForward_=false;preview_.reset(0,now);replay();}
}
void SettingsUi::activate(const Hit& h,float x){
    if(h.section>=0){selectSection(h.section);return;}
    if(h.item<0)return;auto& item=items_[h.item];if(!enabled(item))return;int v=item.get?item.get(s_):0;
    switch(item.control){
    case SettingControl::Toggle:apply(h.item,v?0:1);break;
    case SettingControl::Choice:case SettingControl::Swatch:if(h.part>=0)apply(h.item,h.part);break;
    case SettingControl::Stepper:if(h.part>=0){int n=item.hi-item.lo+1;apply(h.item,item.lo+((v-item.lo+(h.part?1:-1))%n+n)%n);}break;
    case SettingControl::Order:if(h.part>=0)apply(h.item,h.part?1:-1);break;
    case SettingControl::Slider:slide(h.item,x);break;
    case SettingControl::Preview:replay();break;
    case SettingControl::Actions:if(h.part>=0){PostMessageW(island_,SettingsActionMessage,WPARAM(item.action),LPARAM(h.part));dirty=true;}break;
    case SettingControl::Button:{
        if(item.action==SettingAction::TransparencySettings){ShellExecuteW(nullptr,L"open",L"ms-settings:personalization-colors",nullptr,nullptr,SW_SHOWNORMAL);break;}
        if(item.action==SettingAction::SoundSettings){ShellExecuteW(nullptr,L"open",L"ms-settings:sound",nullptr,nullptr,SW_SHOWNORMAL);break;}
        if(item.action==SettingAction::BluetoothSettings){ShellExecuteW(nullptr,L"open",L"ms-settings:bluetooth",nullptr,nullptr,SW_SHOWNORMAL);break;}
        if(item.action==SettingAction::PowerSettings){ShellExecuteW(nullptr,L"open",L"ms-settings:powersleep",nullptr,nullptr,SW_SHOWNORMAL);break;}
        if(item.action==SettingAction::PrivacySettings){ShellExecuteW(nullptr,L"open",L"ms-settings:privacy",nullptr,nullptr,SW_SHOWNORMAL);break;}
        bool destructive=item.action==SettingAction::ResetAll||item.action==SettingAction::ClearLogs||item.action==SettingAction::ClearClipboard||item.action==SettingAction::ClearWorkspaces||item.action==SettingAction::ClearLyrics;
        if(destructive&&!(confirmItem_==h.item&&seconds()<confirmUntil_)){confirmItem_=h.item;confirmUntil_=seconds()+4;dirty=true;SetTimer(hwnd_,1,4100,nullptr);break;}
        confirmItem_=-1;PostMessageW(island_,SettingsActionMessage,WPARAM(item.action),0);dirty=true;break;}
    default:break;
    }
}
std::vector<Hit> SettingsUi::focusOrder(){std::vector<Hit> order;for(int i=0;i<int(settingSections().size());++i)order.push_back({-1,-1,i});float height;for(auto& row:layout(height)){auto& item=items_[row.item];if(!enabled(item))continue;order.push_back({row.item,item.control==SettingControl::Toggle?0:item.control==SettingControl::Choice||item.control==SettingControl::Swatch?item.get(s_):0,-1});}return order;}
void SettingsUi::key(WPARAM k){
    keyboard_=true;dirty=true;auto order=focusOrder();
    auto index=[&]{for(size_t i=0;i<order.size();++i)if(order[i].section==focus_.section&&order[i].item==focus_.item)return int(i);return -1;};
    if(k==VK_TAB){int i=index();int n=int(order.size());i=(GetKeyState(VK_SHIFT)&0x8000)?(i<=0?n-1:i-1):(i+1)%n;focus_=order[i];}
    else if(focus_.section>=0&&(k==VK_UP||k==VK_DOWN)){int next=std::clamp(focus_.section+(k==VK_DOWN?1:-1),0,int(settingSections().size())-1);selectSection(next);focus_={-1,-1,next};}
    else if(focus_.item>=0&&(k==VK_LEFT||k==VK_RIGHT)){auto& item=items_[focus_.item];int d=k==VK_RIGHT?1:-1,v=item.get(s_);
        if(item.control==SettingControl::Slider)apply(focus_.item,std::clamp(v+d*item.step,item.lo,item.hi));
        else if(item.control==SettingControl::Choice||item.control==SettingControl::Swatch){apply(focus_.item,std::clamp(v+d,item.lo,item.hi));focus_.part=item.get(s_);}
        else if(item.control==SettingControl::Stepper)activate({focus_.item,d>0?1:0,-1},0);
        else if(item.control==SettingControl::Order)apply(focus_.item,d);
        else if(item.control==SettingControl::Chips){if(s_.sounds&&focus_.part+d>=0&&focus_.part+d<chipCount)playSound(Sound::Click);auto order=moveChip(s_.chips,focus_.part,focus_.part+d);apply(focus_.item,encodeChips(order));focus_.part=std::clamp(focus_.part+d,0,chipCount-1);}}
    else if(k==VK_SPACE||k==VK_RETURN){if(focus_.section>=0)selectSection(focus_.section);else if(focus_.item>=0){auto& item=items_[focus_.item];if(item.control==SettingControl::Toggle||item.control==SettingControl::Button)activate({focus_.item,0,-1},0);else if(item.control==SettingControl::Actions||item.control==SettingControl::Preview)activate({focus_.item,std::max(0,focus_.part),-1},0);}}
    // Keep the focused row inside the viewport.
    if(focus_.item>=0){float height;for(auto& row:layout(height))if(row.item==focus_.item){float top=row.row.top,bottom=row.row.bottom;if(top<CardTop-10)scrollTarget_-=CardTop-10-top;else if(bottom>H()-20)scrollTarget_+=bottom-(H()-20);float max=std::max(0.f,height-H());scrollTarget_=std::clamp(scrollTarget_,0.,double(max));aim(scroll_,scrollTarget_,Scroll);}}
}
void SettingsUi::refresh(){
    std::lock_guard lock(shared_.mutex);
    if(shared_.requestedSection>=0){selectSection(shared_.requestedSection);shared_.requestedSection=-1;}
    if(!shared_.pending)return;shared_.pending=false;
    // Ignore echoes of older edits while the pointer is still moving a control.
    if(shared_.incomingSequence<posted_){if(shared_.incomingContext.labStats!=context_.labStats){context_.labStats=shared_.incomingContext.labStats;dirty=true;}return;}
    bool monitorsChanged=shared_.incomingContext.monitors!=context_.monitors;s_=shared_.incoming;context_=shared_.incomingContext;if(monitorsChanged)items_=settingItems(context_.monitors);theme();dirty=true;
}
void SettingsUi::theme(){
    auto p=palette();BOOL dark=!p.light;DwmSetWindowAttribute(hwnd_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    bool wantMica=GlassBackdrop::effectsEnabled();if(wantMica!=mica_){mica_=wantMica;DWM_SYSTEMBACKDROP_TYPE type=mica_?DWMSBT_MAINWINDOW:DWMSBT_NONE;DwmSetWindowAttribute(hwnd_,DWMWA_SYSTEMBACKDROP_TYPE,&type,sizeof(type));}
    p=palette();COLORREF caption=mica_?DWMWA_COLOR_DEFAULT:RGB((p.bg>>16)&255,(p.bg>>8)&255,p.bg&255),ink=RGB((p.ink>>16)&255,(p.ink>>8)&255,p.ink&255);DwmSetWindowAttribute(hwnd_,DWMWA_CAPTION_COLOR,&caption,sizeof(caption));DwmSetWindowAttribute(hwnd_,DWMWA_TEXT_COLOR,&ink,sizeof(ink));
}
HWND SettingsUi::create(){
    // Messages sent from another thread are handled inside GetMessage without it returning, so wake the loop to render them.
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=[](HWND h,UINT m,WPARAM w,LPARAM l)->LRESULT{auto self=reinterpret_cast<SettingsUi*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){self=static_cast<SettingsUi*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->hwnd_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(self)try{auto result=self->message(h,m,w,l);if((self->dirty||self->animating())&&InSendMessage())PostMessageW(h,WM_NULL,0,0);return result;}catch(...){}return DefWindowProcW(h,m,w,l);};
    wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ArnavIsland.Settings";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(101));wc.hIconSm=wc.hIcon;RegisterClassExW(&wc);
    POINT cursor{};GetCursorPos(&cursor);HMONITOR monitor=MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY);UINT dx=96,dy=96;GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dx,&dy);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(monitor,&mi);
    float s=dx/96.f;RECT r{0,0,LONG(980*s),LONG(700*s)};AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_NOREDIRECTIONBITMAP,dx);int w=r.right-r.left,h=r.bottom-r.top;auto& work=mi.rcWork;w=std::min<int>(w,work.right-work.left);h=std::min<int>(h,work.bottom-work.top);
    hwnd_=CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"Island settings",WS_OVERLAPPEDWINDOW,work.left+(work.right-work.left-w)/2,work.top+(work.bottom-work.top-h)/2,w,h,nullptr,nullptr,wc.hInstance,this);
    if(!hwnd_)throw std::runtime_error("Settings window creation failed");dpi_=float(GetDpiForWindow(hwnd_));
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr)==S_OK?S_OK:D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d3d_,nullptr,nullptr));
    ComPtr<IDXGIDevice> dxgi;check(d3d_.As(&dxgi));ComPtr<IDXGIAdapter> adapter;check(dxgi->GetAdapter(&adapter));ComPtr<IDXGIFactory2> dxgiFactory;check(adapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));
    RECT client{};GetClientRect(hwnd_,&client);width_=std::max<LONG>(1,client.right);height_=std::max<LONG>(1,client.bottom);
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=width_;desc.Height=height_;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc={1,0};desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    check(dxgiFactory->CreateSwapChainForComposition(d3d_.Get(),&desc,nullptr,&swap_));
    D2D1_FACTORY_OPTIONS options{};check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),&options,reinterpret_cast<void**>(factory_.GetAddressOf())));
    check(factory_->CreateDevice(dxgi.Get(),&device2d_));check(device2d_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&dc_));dc_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write_.GetAddressOf())));if(auto params=sharpTextParams(write_.Get()))dc_->SetTextRenderingParams(params.Get());
    check(DCompositionCreateDevice(dxgi.Get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(composition_.GetAddressOf())));check(composition_->CreateTargetForHwnd(hwnd_,TRUE,&target_));check(composition_->CreateVisual(&visual_));check(visual_->SetContent(swap_.Get()));check(target_->SetRoot(visual_.Get()));check(composition_->Commit());
    check(dc_->CreateSolidColorBrush(D2D1::ColorF(0xffffff),&brush_));
    MARGINS margins{-1,-1,-1,-1};DwmExtendFrameIntoClientArea(hwnd_,&margins);theme();resize();
    double now=seconds();navY_.reset(104+section_*44.,now);page_.reset(1,now);
    return hwnd_;
}
void SettingsUi::resize(){
    RECT client{};GetClientRect(hwnd_,&client);width_=std::max<LONG>(1,client.right);height_=std::max<LONG>(1,client.bottom);dc_->SetTarget(nullptr);
    check(swap_->ResizeBuffers(2,width_,height_,DXGI_FORMAT_B8G8R8A8_UNORM,0));ComPtr<IDXGISurface> surface;check(swap_->GetBuffer(0,IID_PPV_ARGS(&surface)));
    auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),dpi_,dpi_);
    ComPtr<ID2D1Bitmap1> bitmap;check(dc_->CreateBitmapFromDxgiSurface(surface.Get(),&props,&bitmap));dc_->SetTarget(bitmap.Get());dc_->SetDpi(dpi_,dpi_);dirty=true;
}
bool SettingsUi::animating(){
    double now=seconds();auto moving=[&](const Spring& s){return !s.settled(now);};
    if(moving(navY_)||moving(page_)||moving(scroll_)||moving(preview_))return true;
    for(auto* map:{&knobs_,&hovers_,&pillX_,&pillW_,&thumbs_,&rings_})for(auto& [k,s]:*map)if(moving(s))return true;
    return false;
}
void SettingsUi::render(){
    if(!dc_)return;auto p=palette();double now=seconds();
    float contentHeight;std::vector<D2D1_RECT_F> nav;auto rows=layout(contentHeight,&nav);
    float maxScroll=std::max(0.f,contentHeight-H());if(scrollTarget_>maxScroll){scrollTarget_=maxScroll;aim(scroll_,scrollTarget_,Scroll);}
    dc_->BeginDraw();dc_->Clear(D2D1::ColorF(p.bg,p.bgAlpha));
    // Sidebar with a travelling, velocity-stretched selection indicator.
    text(L"ARNAV ISLAND",{24,26,Sidebar,44},11,p.muted,1,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(L"Settings",{23,44,Sidebar,80},24,p.ink,1,DWRITE_FONT_WEIGHT_SEMI_BOLD);
    aim(navY_,nav[section_].top,Indicator);auto indicator=navY_.sample(now);float iy=float(indicator.position);
    fill({12,iy,Sidebar-12,iy+40},8,p.light?0x000000:0xffffff,p.light?.055f:.075f);
    float stretch=std::clamp(float(std::abs(indicator.velocity))*.018f,0.f,14.f);brush_->SetColor(D2D1::ColorF(p.accent));dc_->FillRoundedRectangle(D2D1::RoundedRect({13,iy+12-stretch/2,16,iy+28+stretch/2},1.5f,1.5f),brush_.Get());
    for(size_t i=0;i<nav.size();++i){auto r=nav[i];auto& h=spring(hovers_,1000000+int(i),0);aim(h,hover_.section==int(i)&&int(i)!=section_?1:0,Hover);fill(r,8,p.light?0x000000:0xffffff,float(at(h))*(p.light?.035f:.045f));
        bool selected=int(i)==section_;drawIcon(dc_.Get(),factory_.Get(),sectionIcons[i],r.left+14,r.top+11,18,selected?p.ink:p.muted);text(settingSections()[i],{r.left+44,r.top,r.right-8,r.bottom},14,selected?p.ink:p.muted,1,selected?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL);
        if(keyboard_&&focus_.section==int(i))stroke({r.left-2,r.top-2,r.right+2,r.bottom+2},10,p.accent,1,2);}
    // Content, fading and rising in on section changes.
    float in=float(std::clamp(at(page_),0.,1.)),rise=(1-in)*14;dc_->PushAxisAlignedClip({Sidebar,0,W(),H()},D2D1_ANTIALIAS_MODE_ALIASED);
    D2D1_MATRIX_3X2_F base;dc_->GetTransform(&base);dc_->SetTransform(D2D1::Matrix3x2F::Translation(0,rise)*base);
    const float left=Sidebar+20,right=W()-32,scroll=float(at(scroll_));
    ComPtr<ID2D1Layer> layer;dc_->CreateLayer(nullptr,&layer);dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),nullptr,D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,D2D1::IdentityMatrix(),in),layer.Get());
    text(settingSections()[section_],{left,TitleTop-scroll,right,TitleTop+40-scroll},28,p.ink,1,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(subtitles[section_],{left,TitleTop+42-scroll,right,TitleTop+64-scroll},13,p.muted);
    if(about()){D2D1_RECT_F card{left,CardTop-scroll,right,CardTop+76-scroll};fill(card,10,p.card,p.cardAlpha);stroke(card,10,p.border,1);
        brush_->SetColor(D2D1::ColorF(p.ink));dc_->FillRoundedRectangle(D2D1::RoundedRect({left+20,card.top+26,left+64,card.top+50},12,12),brush_.Get());brush_->SetColor(D2D1::ColorF(p.accent));dc_->FillEllipse(D2D1::Ellipse({left+55,card.top+38},3,3),brush_.Get());
        text(L"Arnav Island "+context_.version,{left+80,card.top+14,right-20,card.top+38},15,p.ink,1,DWRITE_FONT_WEIGHT_SEMI_BOLD);text(L"Native Windows preview · No account, cloud or telemetry upload. Preferences stay on this device.",{left+80,card.top+38,right-20,card.top+60},12,p.muted);}
    if(!rows.empty()){D2D1_RECT_F card{left,rows.front().row.top,right,rows.back().row.bottom};fill(card,10,p.card,p.cardAlpha);stroke(card,10,p.border,1);}
    for(size_t r=0;r<rows.size();++r){auto& row=rows[r];auto& item=items_[row.item];bool on=enabled(item);float alpha=on?1:.42f;float cy=(row.row.top+row.row.bottom)/2,R=row.row.right-Pad;
        if(row.row.bottom<0||row.row.top>H()+20)continue;
        if(r>0){brush_->SetColor(D2D1::ColorF(p.border));dc_->DrawLine({row.row.left+Pad,row.row.top},{row.row.right-Pad,row.row.top},brush_.Get(),1);}
        auto& rowHover=spring(hovers_,row.item*100+98,0);aim(rowHover,hover_.item==row.item&&on?1:0,Hover);fill({row.row.left+4,row.row.top+4,row.row.right-4,row.row.bottom-4},7,p.light?0x000000:0xffffff,float(at(rowHover))*(p.light?.02f:.025f));
        float textRight=item.control==SettingControl::Chips?row.row.right-Pad:row.control.left-16;
        if(item.control==SettingControl::Preview){text(item.title,{row.row.left+Pad,row.row.top+12,row.row.right-Pad,row.row.top+32},14,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);text(detail(item),{row.row.left+Pad,row.row.top+12,row.row.right-Pad,row.row.top+32},12,p.muted,alpha,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
        else if(item.control==SettingControl::Order){int page=item.get(s_);text(item.title,{row.row.left+Pad,row.row.top+6,textRight,row.row.top+22},11,p.muted,alpha);drawIcon(dc_.Get(),factory_.Get(),std::array<Icon,pageCount>{Icon::Home,Icon::Music,Icon::Stats,Icon::Focus,Icon::Settings,Icon::Shelf,Icon::Audio,Icon::Sliders}[size_t(std::clamp(page,0,pageCount-1))],row.row.left+Pad,row.row.top+25,16,p.ink);text(item.options[page],{row.row.left+Pad+24,row.row.top+22,textRight,row.row.top+44},14,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);}
        else if(item.control==SettingControl::Chips){text(item.title,{row.row.left+Pad,row.row.top+12,textRight,row.row.top+32},14,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);text(detail(item),{row.row.left+Pad,row.row.top+33,textRight,row.row.top+52},12,p.muted,alpha);}
        else{bool hasDetail=!detail(item).empty();text(item.title,{row.row.left+Pad,hasDetail?row.row.top+12:cy-11,textRight,hasDetail?row.row.top+32:cy+11},14,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);if(hasDetail)text(detail(item),{row.row.left+Pad,row.row.top+33,textRight,row.row.top+52},12,p.muted,alpha);}
        int value=item.get?item.get(s_):0;auto hovered=[&](int part){return hover_.item==row.item&&(hover_.part==part||hover_.part==99);};
        switch(item.control){
        case SettingControl::Toggle:{auto& k=spring(knobs_,row.item,value);aim(k,value,Knob);float t=float(std::clamp(at(k),0.,1.));auto c=row.control;
            fill(c,11,p.accent,t*alpha);stroke(c,11,p.ink,(1-t)*.55f*alpha,1);auto& h=spring(thumbs_,row.item,0);aim(h,press_.item==row.item?2:hovered(0)||hovered(99)?1:0,Hover);float grow=float(at(h));
            float radius=5.5f+std::min(grow,1.f)*1.2f,widen=std::max(0.f,grow-1)*4;float x=lerp(c.left+11,c.right-11,t);brush_->SetColor(D2D1::ColorF(t>.5f?p.onAccent:p.ink,alpha*(t>.5f?1:.8f)));dc_->FillRoundedRectangle(D2D1::RoundedRect({x-radius-widen*(1-t),cy-radius,x+radius+widen*t,cy+radius},radius,radius),brush_.Get());break;}
        case SettingControl::Slider:{auto c=row.control;float f=float(item.hi>item.lo?double(value-item.lo)/(item.hi-item.lo):0);auto& k=spring(knobs_,row.item,f);aim(k,f,Pill);float t=float(std::clamp(at(k),0.,1.));float x=lerp(c.left+9,c.right-9,t);
            fill({c.left,cy-2,c.right,cy+2},2,p.ink,.18f*alpha);fill({c.left,cy-2,x,cy+2},2,p.accent,alpha);
            auto& h=spring(thumbs_,row.item,0);aim(h,dragItem_==row.item?2:hovered(0)?1:0,Hover);float g=float(at(h));float inner=5+std::min(g,1.f)*1.6f-std::max(0.f,g-1)*2.4f;
            brush_->SetColor(D2D1::ColorF(p.light?0xffffff:0x45474e,alpha));dc_->FillEllipse(D2D1::Ellipse({x,cy},10,10),brush_.Get());brush_->SetColor(D2D1::ColorF(0x000000,.14f*alpha));dc_->DrawEllipse(D2D1::Ellipse({x,cy},10,10),brush_.Get(),1);brush_->SetColor(D2D1::ColorF(p.accent,alpha));dc_->FillEllipse(D2D1::Ellipse({x,cy},inner,inner),brush_.Get());
            text(std::to_wstring(value)+item.unit,{c.left-86,cy-10,c.left-12,cy+10},13,p.muted,alpha,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);break;}
        case SettingControl::Choice:{auto c=row.control;fill(c,9,p.ink,(p.light?.05f:.06f)*alpha);stroke(c,9,p.ink,.07f*alpha,1);auto target=row.parts[std::clamp(value,0,int(row.parts.size())-1)];
            auto& px=spring(pillX_,row.item,target.left-c.left);auto& pw=spring(pillW_,row.item,target.right-target.left);aim(px,target.left-c.left,Pill);aim(pw,target.right-target.left,Pill);float x=c.left+float(at(px)),w=float(at(pw));
            fill({x,target.top,x+w,target.bottom},7,p.pill,alpha);stroke({x,target.top,x+w,target.bottom},7,p.light?0x000000:0xffffff,(p.light?.08f:.06f)*alpha,1);
            for(size_t o=0;o<row.parts.size();++o){bool sel=int(o)==value;text(item.options[o],row.parts[o],13,sel?p.ink:hovered(int(o))?p.ink:p.muted,alpha*(sel||hovered(int(o))?1:.9f),sel?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER);}break;}
        case SettingControl::Stepper:{for(int b=0;b<2;++b){auto r=row.parts[b];auto& h=spring(thumbs_,100000+row.item*10+b,0);aim(h,press_.item==row.item&&press_.part==b?2:hovered(b)?1:0,Hover);float g=float(at(h));fill(r,16,p.ink,(.06f+std::min(g,1.f)*.06f-std::max(0.f,g-1)*.04f)*alpha);drawIcon(dc_.Get(),factory_.Get(),b?Icon::ArrowRight:Icon::ArrowLeft,r.left+9,r.top+9,14,p.ink,alpha);}
            text(value>=0&&value<int(item.options.size())?item.options[value]:std::to_wstring(value),{row.parts[0].right,cy-11,row.parts[1].left,cy+11},13,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER);break;}
        case SettingControl::Swatch:{auto target=row.parts[std::clamp(value,0,int(row.parts.size())-1)];auto& rx=spring(rings_,row.item,target.left);aim(rx,target.left,Pill);float x=float(at(rx))+14;
            for(size_t o=0;o<row.parts.size();++o){auto r=row.parts[o];const UINT32 wall=context_.wallpaper?context_.wallpaper:0x9aa0aa,deepWall=((((wall>>16)&255)*5/10)<<16)|((((wall>>8)&255)*5/10)<<8)|((wall&255)*5/10);
                brush_->SetColor(D2D1::ColorF(o==4?(p.light?deepWall:wall):p.light?std::array<UINT32,4>{0x2e7d68,0x2f6fb8,0x7453b8,0xb4652c}[o]:swatchColors[o],alpha));dc_->FillEllipse(D2D1::Ellipse({r.left+14,cy},hovered(int(o))?10.5f:9.5f,hovered(int(o))?10.5f:9.5f),brush_.Get());}
            brush_->SetColor(D2D1::ColorF(p.ink,alpha));dc_->DrawEllipse(D2D1::Ellipse({x,cy},14,14),brush_.Get(),2);break;}
        case SettingControl::Button:{auto r=row.control;bool confirm=confirmItem_==row.item&&now<confirmUntil_;auto& h=spring(thumbs_,100000+row.item*10,0);aim(h,press_.item==row.item?2:hovered(0)?1:0,Hover);float g=float(at(h));
            if(confirm){fill(r,7,0xd9434b,alpha);text(L"Click again to confirm",r,13,0xffffff,alpha,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_TEXT_ALIGNMENT_CENTER);}
            else{fill(r,7,p.ink,(.07f+std::min(g,1.f)*.05f-std::max(0.f,g-1)*.04f)*alpha);stroke(r,7,p.ink,.08f*alpha,1);text(item.options.front(),r,13,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER);}break;}
        case SettingControl::Order:{int slot=row.item;for(int b=0;b<2;++b){auto r=row.parts[b];bool possible=b?item.key!="nav"+std::to_string(pageCount-1):item.key!="nav0";auto& h=spring(thumbs_,100000+slot*10+b,0);aim(h,press_.item==row.item&&press_.part==b?2:hovered(b)?1:0,Hover);float g=float(at(h));fill(r,15,p.ink,(.06f+std::min(g,1.f)*.06f)*(possible?1:.4f));drawIcon(dc_.Get(),factory_.Get(),b?Icon::ArrowDown:Icon::ArrowUp,r.left+8,r.top+8,14,p.ink,possible?1:.35f);}break;}
        case SettingControl::Preview:drawPreview(row.control,p,alpha);break;
        case SettingControl::Chips:{
            // The strip, then each chip where its spring has it; a dragged chip follows the pointer and the others make room.
            fill(row.control,16,p.light?0x15171c:0x0b0c0f,alpha);const Icon glyphs[]={Icon::Clock,Icon::Volume,Icon::Battery,Icon::Focus,Icon::Processor,Icon::Gauge,Icon::Sun};
            auto order=s_.chips;int dragged=-1;if(chipDrag_>=0&&press_.item==row.item){dragged=order[size_t(chipDrag_)];order=moveChip(order,chipDrag_,chipTarget(row));}
            const float w=row.parts[0].right-row.parts[0].left;
            auto chipAt=[&](int id,float x,bool lifted){D2D1_RECT_F r{x,row.parts[0].top-(lifted?2.f:0.f),x+w,row.parts[0].bottom-(lifted?2.f:0.f)};
                if(lifted){fill({r.left+1,r.top+3,r.right+1,r.bottom+4},10,0x000000,.35f*alpha);fill(r,10,p.accent,alpha);}else fill(r,10,0xffffff,.08f*alpha);
                const UINT32 ink=lifted?p.onAccent:0xf1f3f7;const float label=measure(item.options[size_t(id)],12,DWRITE_FONT_WEIGHT_MEDIUM),total=16+6+label,start=r.left+std::max(6.f,(w-total)/2);
                drawIcon(dc_.Get(),factory_.Get(),glyphs[id],start,(r.top+r.bottom)/2-8,16,ink,alpha);text(item.options[size_t(id)],{start+22,r.top,r.right-4,r.bottom},12,ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);};
            for(int k=0;k<chipCount;++k){const int id=order[size_t(k)];auto& sx=spring(pillX_,300000+id,row.parts[size_t(k)].left);if(id==dragged)continue;aim(sx,row.parts[size_t(k)].left,Pill);chipAt(id,float(at(sx)),false);}
            if(dragged>=0){const float x=std::clamp(chipX_-chipGrab_,row.control.left,row.control.right-w);auto& sx=spring(pillX_,300000+dragged,x);sx.reset(x,now);chipAt(dragged,x,true);}
            break;}
        case SettingControl::Actions:{for(size_t b=0;b<row.parts.size();++b){auto r=row.parts[b];auto& h=spring(thumbs_,200000+row.item*10+int(b),0);aim(h,press_.item==row.item&&press_.part==int(b)?2:hovered(int(b))?1:0,Hover);float g=float(at(h));
            fill(r,8,b==0?p.accent:p.ink,(b==0?.9f+std::min(g,1.f)*.1f:.07f+std::min(g,1.f)*.05f-std::max(0.f,g-1)*.04f)*alpha);if(b)stroke(r,8,p.ink,.08f*alpha,1);text(item.options[b],r,13,b==0?p.onAccent:p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER);}break;}
        default:break;
        }
        if(keyboard_&&focus_.item==row.item){auto r=row.control;if(focus_.part>=0&&focus_.part<int(row.parts.size())&&item.control!=SettingControl::Slider)r=row.parts[focus_.part];stroke({r.left-3,r.top-3,r.right+3,r.bottom+3},9,p.accent,1,2);}
    }
    dc_->PopLayer();dc_->SetTransform(base);dc_->PopAxisAlignedClip();
    if(maxScroll>0){float track=H()-24,thumb=std::max(40.f,track*H()/contentHeight),y=12+(track-thumb)*scroll/maxScroll;fill({W()-7,y,W()-4,y+thumb},1.5f,p.ink,.22f);}
    HRESULT hr=dc_->EndDraw();if(hr==D2DERR_RECREATE_TARGET){resize();return;}check(hr);
    check(swap_->Present(1,0));dirty=false;publish();
}
// A miniature island on a screen edge, morphing with the island's spring, beside
// the spring's step response with its overshoot and settling time.
void SettingsUi::drawPreview(const D2D1_RECT_F& a,const Palette& p,float alpha){
    const double now=seconds();const auto spec=bodySpring(s_);const float split=a.left+(a.right-a.left)*.46f;
    fill(a,10,p.ink,(p.light?.035f:.05f)*alpha);stroke(a,10,p.ink,.07f*alpha,1);
    // Stage: the island hangs from a screen edge, compact (collapsed) to open (expanded).
    const float cx=(a.left+split)/2,top=a.top+14;brush_->SetColor(D2D1::ColorF(p.ink,.18f*alpha));dc_->DrawLine({a.left+18,top},{split-18,top},brush_.Get(),1);
    const float t=float(at(preview_)),w=lerp(62,164,t),h=lerp(14,84,t),r=lerp(7,15,t);
    fill({cx-w/2,top,cx+w/2,top+h},r,p.light?0x15171c:0x0b0c0f,alpha);
    const float c=std::clamp(t,0.f,1.f);if(c>.35f){float q=(c-.35f)/.65f;fill({cx-w/2+12,top+12,cx-w/2+40,top+40},7,p.accent,q*alpha);fill({cx-w/2+50,top+16,cx+w/2-14,top+23},3,0xffffff,.55f*q*alpha);fill({cx-w/2+50,top+29,cx+w/2-46,top+35},3,0xffffff,.3f*q*alpha);fill({cx-w/2+12,top+52,cx+w/2-12,top+56},2,0xffffff,.18f*q*alpha);fill({cx-w/2+12,top+52,cx-w/2+12+(w-24)*.4f,top+56},2,p.accent,q*alpha);}
    // Step response of the same spring, 0 to 1, over the next 1.4 s of simulated time.
    Spring probe{0};probe.retarget(1,0,spec);const float gx0=split+18,gx1=a.right-18,gy0=a.top+18,gy1=a.bottom-40;const double span=1.4*(s_.labSpeed==2?4:s_.labSpeed==1?2:1);
    auto yOf=[&](double v){return float(gy1-(gy1-gy0)*std::clamp(v,-.1,1.35)/1.35);};
    brush_->SetColor(D2D1::ColorF(p.ink,.12f*alpha));dc_->DrawLine({gx0,yOf(1)},{gx1,yOf(1)},brush_.Get(),1);dc_->DrawLine({gx0,gy1},{gx1,gy1},brush_.Get(),1);
    double peak=0,settle=-1;ComPtr<ID2D1PathGeometry> path;factory_->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;path->Open(&sink);
    for(int i=0;i<=120;++i){double tt=span*i/120;auto sample=probe.sample(tt);peak=std::max(peak,sample.position);D2D1_POINT_2F pt{lerp(gx0,gx1,float(i)/120),yOf(sample.position)};if(i==0)sink->BeginFigure(pt,D2D1_FIGURE_BEGIN_HOLLOW);else sink->AddLine(pt);}
    sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();
    for(int i=0;i<=1200;++i){double tt=span*i/1200;auto sample=probe.sample(tt);if(std::abs(sample.position-1)<.01&&std::abs(sample.velocity)<.05){if(settle<0)settle=tt;}else settle=-1;}
    brush_->SetColor(D2D1::ColorF(p.accent,alpha));ComPtr<ID2D1StrokeStyle> round;D2D1_STROKE_STYLE_PROPERTIES props{};props.startCap=D2D1_CAP_STYLE_ROUND;props.endCap=D2D1_CAP_STYLE_ROUND;props.lineJoin=D2D1_LINE_JOIN_ROUND;factory_->CreateStrokeStyle(props,nullptr,0,&round);
    dc_->DrawGeometry(path.Get(),brush_.Get(),2,round.Get());
    // A dot rides the curve while the preview plays.
    const double elapsed=now-previewStart_;if(elapsed>=0&&elapsed<=span){auto sample=probe.sample(elapsed);D2D1_POINT_2F pt{lerp(gx0,gx1,float(elapsed/span)),yOf(sample.position)};dc_->FillEllipse(D2D1::Ellipse(pt,4.5f,4.5f),brush_.Get());}
    const int overshoot=int(std::lround(std::max(0.,peak-1)*100)),settleMs=settle<0?-1:int(std::lround(settle*1000));
    std::wstring numbers=(settleMs<0?std::wstring(L"Still moving after "+std::to_wstring(int(span*1000))+L" ms"):L"Settles in "+std::to_wstring(settleMs)+L" ms")+L"   \u00b7   "+(overshoot?std::to_wstring(overshoot)+L"% overshoot":std::wstring(L"No overshoot"));
    text(numbers,{gx0,a.bottom-34,gx1,a.bottom-16},12.5f,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);
    text(context_.labStats.empty()?std::wstring(L"Measuring the display\u2026"):context_.labStats,{a.left+18,a.bottom-34,split-10,a.bottom-16},11,p.muted,alpha);
}
LRESULT SettingsUi::message(HWND h,UINT m,WPARAM w,LPARAM l){
    auto point=[&]{return std::pair<float,float>{GET_X_LPARAM(l)*96.f/dpi_,GET_Y_LPARAM(l)*96.f/dpi_};};
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);dirty=true;return 0;}
    case WM_SIZE:if(dc_&&w!=SIZE_MINIMIZED){resize();render();}return 0;
    case WM_GETMINMAXINFO:{auto* info=reinterpret_cast<MINMAXINFO*>(l);float s=dpi_/96;info->ptMinTrackSize={LONG(860*s),LONG(600*s)};return 0;}
    case WM_DPICHANGED:{dpi_=float(HIWORD(w));formats_.clear();auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);resize();return 0;}
    case WM_SETTINGCHANGE:theme();dirty=true;return 0;
    case RefreshMessage:refresh();return 0;
    case ShowMessage:ShowWindow(h,IsIconic(h)?SW_RESTORE:SW_SHOW);SetForegroundWindow(h);refresh();return 0;
    case WM_TIMER:if(w==1){KillTimer(h,1);dirty=true;}return 0;
    case WM_MOUSEMOVE:{auto [x,y]=point();if(!tracking_){TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);tracking_=true;}if(dragItem_>=0){slide(dragItem_,x);return 0;}if(chipDrag_>=0){chipX_=x;dirty=true;
            // Phase 5G: a soft click each time the dragged chip passes another's place.
            float height;for(auto& row:layout(height))if(row.item==press_.item){const int to=chipTarget(row);if(to!=chipHeard_){if(s_.sounds)playSound(Sound::Click);chipHeard_=to;}}return 0;}auto next=hit(x,y);if(!(next==hover_)){hover_=next;dirty=true;}return 0;}
    case WM_MOUSELEAVE:tracking_=false;if(dragItem_<0){hover_={};dirty=true;}return 0;
    case WM_LBUTTONDOWN:{auto [x,y]=point();keyboard_=false;press_=hit(x,y);SetCapture(h);if(press_.item>=0&&items_[press_.item].control==SettingControl::Slider&&press_.part==0&&enabled(items_[press_.item])){dragItem_=press_.item;slide(dragItem_,x);}
        if(press_.item>=0&&items_[press_.item].control==SettingControl::Chips&&press_.part>=0){float height;for(auto& row:layout(height))if(row.item==press_.item){chipDrag_=press_.part;chipHeard_=press_.part;chipGrab_=x-row.parts[size_t(press_.part)].left;chipX_=x;}}if(press_.item>=0||press_.section>=0)focus_=press_.section>=0?press_:Hit{press_.item,std::max(0,press_.part),-1};dirty=true;return 0;}
    case WM_LBUTTONUP:{
        // ReleaseCapture sends WM_CAPTURECHANGED synchronously, which clears the press.
        auto [x,y]=point();Hit pressed=press_;bool dragging=dragItem_>=0;
        // A chip dropped in a new slot reorders the chips.
        if(chipDrag_>=0&&pressed.item>=0){chipX_=x;float height;for(auto& row:layout(height))if(row.item==pressed.item){const int to=chipTarget(row);if(to!=chipDrag_)apply(pressed.item,encodeChips(moveChip(s_.chips,chipDrag_,to)));focus_={pressed.item,to,-1};}
            chipDrag_=-1;ReleaseCapture();press_={};hover_=hit(x,y);dirty=true;return 0;}
        ReleaseCapture();auto up=hit(x,y);
        if(!dragging&&(up==pressed||(up.item==pressed.item&&pressed.item>=0&&items_[pressed.item].control==SettingControl::Toggle)))activate(pressed,x);
        dragItem_=-1;press_={};hover_=up;dirty=true;return 0;}
    case WM_CAPTURECHANGED:dragItem_=-1;chipDrag_=-1;press_={};dirty=true;return 0;
    case WM_MOUSEWHEEL:{float height;layout(height);float max=std::max(0.f,height-H());scrollTarget_=std::clamp(scrollTarget_-GET_WHEEL_DELTA_WPARAM(w)*.9,0.,double(max));aim(scroll_,scrollTarget_,Scroll);dirty=true;return 0;}
    case WM_KEYDOWN:key(w);return 0;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

SettingsWindow::SettingsWindow(HWND island):island_(island),state_(std::make_unique<State>()){}
SettingsWindow::~SettingsWindow(){if(HWND h=window_.load())PostMessageW(h,WM_CLOSE,0,0);if(thread_.joinable())thread_.join();}
void SettingsWindow::show(const Settings& s,const SettingsContext& c,int section){
    if(HWND h=window_.load()){{std::lock_guard lock(state_->mutex);if(section>=0)state_->requestedSection=section;}PostMessageW(h,ShowMessage,0,0);return;}
    if(thread_.joinable())thread_.join();
    finished_=false;thread_=std::thread([this,s,c,section]{try{run(s,c,section);}catch(...){}window_=nullptr;finished_=true;});
    // Wait briefly for the handle so tests and callers can address the window.
    for(int i=0;i<400&&!window_.load()&&!finished_.load();++i)Sleep(5);
}
void SettingsWindow::update(const Settings& s,const SettingsContext& c,unsigned sequence){HWND h=window_.load();if(!h)return;{std::lock_guard lock(state_->mutex);state_->incoming=s;state_->incomingContext=c;state_->incomingSequence=sequence;state_->pending=true;}PostMessageW(h,RefreshMessage,0,0);}
std::vector<SettingsWindow::Probe> SettingsWindow::probes(){std::lock_guard lock(state_->mutex);return state_->probes;}
bool SettingsWindow::settled(){std::lock_guard lock(state_->mutex);return state_->settled;}
int SettingsWindow::section(){std::lock_guard lock(state_->mutex);return state_->section;}
void SettingsWindow::run(Settings s,SettingsContext c,int section){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    {
        SettingsUi ui(island_,*state_,s,c,section);HWND h=ui.create();ShowWindow(h,SW_SHOW);SetForegroundWindow(h);ui.render();window_=h;
        MSG msg{};bool running=true,moving=false;
        while(running){
            if(ui.animating()){moving=true;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(running&&IsWindow(h))ui.render();}
            // The last animated frame can land just before the springs settle; draw the resting frame once before blocking.
            else if(moving){moving=false;if(IsWindow(h))ui.render();}
            else{if(GetMessageW(&msg,nullptr,0,0)<=0)break;TranslateMessage(&msg);DispatchMessageW(&msg);if(ui.dirty&&IsWindow(h))ui.render();}
        }
        window_=nullptr;
    }
    CoUninitialize();
}
}
