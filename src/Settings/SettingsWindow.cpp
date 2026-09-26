#include "Audio/Sounds.h"
#include "SettingsWindow.h"
#include "Animation/MotionEngine.h"
#include "Design/Icons.h"
#include "Design/Type.h"
#include "Composition/GlassBackdrop.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <filesystem>
#include <fstream>
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
constexpr UINT RefreshMessage=WM_APP+1,ShowMessage=WM_APP+2,TypeTownMessage=WM_APP+3,SweepMessage=WM_APP+4;
constexpr float Sidebar=236,Pad=20,TitleTop=30,CardTop=112;
constexpr SpringSpec Knob{1,520,36},Pill{.9,480,36},Hover{1,700,52},Page{1,300,32},Scroll{1,260,34},Indicator{.8,420,32};
const wchar_t* subtitles[]={L"How the island behaves while you work",L"Size, position and everyday mode",L"Theme, glass and color",L"Springs, feedback and accessibility",L"What the resting island shows",L"Players, logos and audio output",L"Bluetooth, battery and performance",L"Arrange the Command Center",L"Clipboard, privacy dots, commands and workspaces",L"Version, diagnostics and reset"};
const Icon sectionIcons[]={Icon::Settings,Icon::Island,Icon::Sun,Icon::Spark,Icon::Stats,Icon::Music,Icon::Bluetooth,Icon::Home,Icon::Shield,Icon::Info};
const UINT32 swatchColors[]={0xa4deca,0xa6cafa,0xccb8f1,0xefc7a6};
struct Palette {UINT32 bg,card,border,ink,muted,accent,onAccent,pill;float bgAlpha,cardAlpha;bool light;};
bool systemLight(){DWORD light=0,size=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);return light!=0;}
// base: the row's own height (its controls are centred in it); open (0-1): how far its preview has opened below that, into preview.
struct Row {int item=-1;D2D1_RECT_F row{},control{};std::vector<D2D1_RECT_F> parts;float base=64,open=0;D2D1_RECT_F preview{};};
// 0.17.0-preview.3: a row's preview opens this far below it.
constexpr float PreviewHeight=98;
constexpr SpringSpec Reveal{1,380,36};
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
    // Test runs only: every section, a window's height at a time, saved as the window drew it (never a screen copy).
    std::wstring sweepDir_,sweepFile_;int sweepPage_=0,sweepPreview_=-1;bool sweepShot_=false;void sweepStep();void saveFrame(const std::wstring& file,UINT32 background);
    // A picture keeps moving (the glass preview on screen, or a row's preview open): the window draws at about 30 fps.
    bool live();
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
    // 0.17.0-preview.3: rows that show what they do. Resting the pointer on one for 450 ms opens a small moving picture under it
    // (a click first cancels the wait, so clicking through rows never opens one); it closes when the pointer moves to another row.
    std::map<int,Spring> opens_;int dwellItem_=-1,openItem_=-1;double openedAt_=0;float pointerX_=-1,pointerY_=-1;
    bool scene(const SettingItem&)const;void openPreview(int item,bool instant=false);void closePreview();
    // The pictures: a stand-in wallpaper, and a miniature island drawn in any theme and material over it.
    struct Mini{D2D1_RECT_F box{};float radius=12;bool top=true,shadow=true,content=true;float beat=0,frost=0,splash=-1;D2D1_POINT_2F glint{-1,-1};int material=-1,theme=-1,tint=-1;};
    void backdrop(const D2D1_RECT_F& area,double t,float blur,float alpha);ComPtr<ID2D1PathGeometry> islandPath(const D2D1_RECT_F& box,float radius,bool top,bool closed);
    void mini(const D2D1_RECT_F& area,const Mini&,double t,float alpha);void drawScene(const SettingItem&,const D2D1_RECT_F& area,float alpha);void drawGlass(const D2D1_RECT_F& area,float alpha,int item);
    double frostLevel_=0,frostAt_=0;
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
    // Phase 5G: the Town field: what is typed, the caret, whether it has the keyboard, the highlighted match.
    std::wstring townText_;size_t townCaret_=0;bool townFocus_=false;int townPick_=0;double caretEpoch_=0;
    bool townListed()const{return townFocus_&&!context_.townResults.empty()&&!townText_.empty()&&context_.townQuery==trimmedTown();}
    std::wstring trimmedTown()const{size_t a=townText_.find_first_not_of(L' '),b=townText_.find_last_not_of(L' ');return a==std::wstring::npos?std::wstring():townText_.substr(a,b-a+1);}
    void townFocus(bool on);void townEdited();void townChoose(int index);bool townKey(WPARAM);bool townChar(wchar_t);
};

Palette SettingsUi::palette()const{
    bool light=s_.theme==1||(s_.theme==2&&systemLight());UINT32 accent=s_.accent==4?(context_.wallpaper?context_.wallpaper:swatchColors[0]):swatchColors[std::clamp(s_.accent,0,3)];
    // The wallpaper swatch darkens its colour for light windows, as its swatch does.
    if(light){const UINT32 deep[]={0x2e7d68,0x2f6fb8,0x7453b8,0xb4652c};const UINT32 wall=context_.wallpaper?context_.wallpaper:0x9aa0aa;const UINT32 deepWall=((((wall>>16)&255)*5/10)<<16)|((((wall>>8)&255)*5/10)<<8)|((wall&255)*5/10);
        return {0xf3f3f6,0xffffff,0xe2e3e8,0x1b1c20,0x696c75,s_.accent==4?deepWall:deep[std::clamp(s_.accent,0,3)],0xffffff,0xffffff,mica_?0.f:1.f,mica_?.72f:1.f,true};}
    return {0x141518,0x1e1f24,0x2b2d33,0xf2f3f6,0x9b9ea8,accent,0x101114,0x34363d,mica_?0.f:1.f,mica_?.62f:1.f,false};
}
bool SettingsUi::enabled(const SettingItem& i)const{
    if(i.key=="glassTint")return s_.material==1;
    if(i.key=="clearTint")return s_.material==2;
    if(i.key=="restFrost")return s_.material==1;
    if(i.key=="weatherGlass")return s_.material!=0;
    if(i.key=="compactWidth")return s_.uiMode!=0;
    if(i.key=="hoverDelay")return s_.hoverOpen;
    if(i.key=="accent")return true;
    return true;
}
std::wstring SettingsUi::detail(const SettingItem& i)const{
    if(i.action==SettingAction::CheckUpdates)return context_.updateStatus.empty()?std::wstring(L"The island checks GitHub for new versions"):context_.updateStatus;
    if(i.action==SettingAction::TransparencySettings)return context_.blur?L"On — Frosted glass blurs what is behind the island":L"Off — Frosted glass uses a translucent frost; turn on for real blur";
    if(i.key=="material"&&!context_.glassAvailable)return L"Glass needs Windows 11 composition support";
    if(i.key=="commandShortcut"&&context_.shortcutTaken)return L"Another app already uses this shortcut \u2014 choose another";
    if(i.key=="captureShortcuts"&&!context_.captureTaken.empty()&&s_.captureShortcuts)return L"Another app already uses "+context_.captureTaken+L"; the rest work";
    if(i.control==SettingControl::Town){if(townFocus_&&context_.townBusy)return L"Looking for towns\u2026";if(!context_.townStatus.empty())return context_.townStatus;
        if(townFocus_&&!townText_.empty()&&context_.townQuery==trimmedTown()&&context_.townResults.empty())return L"No towns match \u201c"+trimmedTown()+L"\u201d";
        if(!context_.weatherPlace.empty())return L"Showing the weather for "+context_.weatherPlace;}
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
        const size_t listed=item.control==SettingControl::Town&&townListed()?context_.townResults.size():0;
        Row row;row.item=int(index);float h=item.control==SettingControl::Order?52:item.control==SettingControl::Preview?216:item.control==SettingControl::Glass?176:item.control==SettingControl::Chips?122:item.control==SettingControl::Town?64+(listed?float(listed)*38+10:0.f):64;
        const float base=h;float cy=y+base/2;row.base=base;
        if(scene(item)){auto it=opens_.find(int(index));if(it!=opens_.end())row.open=float(std::clamp(it->second.sample(seconds()).position,0.,1.));}
        h+=row.open*PreviewHeight;row.row={left,y,right,y+h};row.preview={left+Pad,y+base-4,R,y+base-4+PreviewHeight-14};
        switch(item.control){
        case SettingControl::Toggle:row.control={R-44,cy-11,R,cy+11};row.parts={row.control};break;
        case SettingControl::Slider:row.control={R-220,cy-12,R,cy+12};row.parts={row.control};break;
        case SettingControl::Choice:{float total=0;std::vector<float> widths;for(auto& o:item.options){float w=std::max(66.f,measure(o,13,DWRITE_FONT_WEIGHT_MEDIUM)+30);widths.push_back(w);total+=w;}float x=R-total-4;row.control={x,cy-17,R,cy+17};x+=2;for(float w:widths){row.parts.push_back({x,cy-15,x+w,cy+15});x+=w;}break;}
        case SettingControl::Stepper:row.control={R-190,cy-16,R,cy+16};row.parts={{R-190,cy-16,R-158,cy+16},{R-32,cy-16,R,cy+16}};break;
        case SettingControl::Swatch:{float x=R-float(item.options.size())*38+10;row.control={x,cy-14,R,cy+14};for(size_t k=0;k<item.options.size();++k)row.parts.push_back({x+k*38.f,cy-14,x+k*38.f+28,cy+14});break;}
        case SettingControl::Button:{float w=std::max(96.f,measure(confirmItem_==int(index)?L"Click again to confirm":item.options.front(),13,DWRITE_FONT_WEIGHT_MEDIUM)+36);row.control={R-w,cy-16,R,cy+16};row.parts={row.control};break;}
        case SettingControl::Order:row.control={R-72,cy-15,R,cy+15};row.parts={{R-72,cy-15,R-40,cy+15},{R-32,cy-15,R,cy+15}};break;
        case SettingControl::Preview:case SettingControl::Glass:row.control={left+Pad,y+44,R,y+base-14};row.parts={row.control};break;
        // A strip like the compact island, with the chips in their order.
        // The field at the right of the row's first line; the matches under it, across the row.
        case SettingControl::Town:{row.control={R-300,y+16,R,y+48};row.parts={row.control};for(size_t k=0;k<listed;++k){const float ty=y+64+float(k)*38;row.parts.push_back({left+Pad-6,ty,R,ty+34});}break;}
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
// The frame just drawn, over the window's own background colour, as a PNG.
void SettingsUi::saveFrame(const std::wstring& file,UINT32 background){
    ComPtr<ID2D1Image> image;dc_->GetTarget(&image);ComPtr<ID2D1Bitmap1> drawn;check(image.As(&drawn));const auto size=drawn->GetPixelSize();
    auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),dpi_,dpi_);
    ComPtr<ID2D1Bitmap1> cpu;check(dc_->CreateBitmap(size,nullptr,0,&props,&cpu));check(cpu->CopyFromBitmap(nullptr,drawn.Get(),nullptr));
    D2D1_MAPPED_RECT mapped{};check(cpu->Map(D2D1_MAP_OPTIONS_READ,&mapped));std::vector<BYTE> pixels(size_t(size.width)*size.height*3);
    const int br=int(background&0xff),bg=int((background>>8)&0xff),rr=int((background>>16)&0xff);
    for(UINT32 y=0;y<size.height;++y)for(UINT32 x=0;x<size.width;++x){const BYTE* s=mapped.bits+size_t(y)*mapped.pitch+size_t(x)*4;BYTE* d=pixels.data()+(size_t(y)*size.width+x)*3;const int a=s[3];
        d[0]=BYTE(std::min(255,s[0]+br*(255-a)/255));d[1]=BYTE(std::min(255,s[1]+bg*(255-a)/255));d[2]=BYTE(std::min(255,s[2]+rr*(255-a)/255));}
    cpu->Unmap();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(file.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));ComPtr<IWICBitmapFrameEncode> frame;check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));
    check(frame->SetSize(size.width,size.height));WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;check(frame->SetPixelFormat(&format));check(frame->WritePixels(size.height,size.width*3,UINT(pixels.size()),pixels.data()));check(frame->Commit());check(encoder->Commit());
}
// One shot, then the next: down the section a window's height at a time, then the next section.
void SettingsUi::sweepStep(){
    // After the sections: each row's preview, open, a little over a second into its motion.
    if(sweepPreview_>=0){
        if(!sweepFile_.empty()){sweepShot_=true;dirty=true;render();closePreview();opens_.clear();}
        int index=-1,seen=0;for(size_t k=0;k<items_.size();++k)if(scene(items_[k])&&seen++==sweepPreview_){index=int(k);break;}
        if(index<0){std::ofstream(std::filesystem::path(sweepDir_)/L"done.txt")<<"done";return;}
        ++sweepPreview_;selectSection(items_[size_t(index)].section);openPreview(index,true);
        float height;for(auto& row:layout(height))if(row.item==index){const float max=std::max(0.f,height-H());scrollTarget_=std::clamp(double(at(scroll_))+row.row.top-(CardTop+10),0.,double(max));scroll_.reset(scrollTarget_,seconds());}
        sweepFile_=L"settings-preview-"+std::wstring(items_[size_t(index)].key.begin(),items_[size_t(index)].key.end())+L".png";dirty=true;SetTimer(hwnd_,4,1300,nullptr);return;}
    sweepShot_=true;dirty=true;render();
    float height;layout(height);const float max=std::max(0.f,height-H()),pageStep=std::max(120.f,H()-CardTop-40);const double now=seconds();
    if((sweepPage_+1)*pageStep<max+pageStep-1&&max>0&&sweepPage_*pageStep<max){++sweepPage_;scrollTarget_=std::min(double(max),double(sweepPage_*pageStep));scroll_.reset(scrollTarget_,now);dirty=true;SetTimer(hwnd_,4,500,nullptr);return;}
    if(section_+1<int(settingSections().size())){sweepPage_=0;selectSection(section_+1);SetTimer(hwnd_,4,900,nullptr);return;}
    sweepPreview_=0;SetTimer(hwnd_,4,300,nullptr);
}
void SettingsUi::selectSection(int section){
    section=std::clamp(section,0,int(settingSections().size())-1);if(section==section_)return;section_=section;double now=seconds();
    if(!s_.reduceMotion){page_.reset(0,now);page_.retarget(1,now,Page);}scrollTarget_=0;scroll_.reset(0,now);confirmItem_=-1;dirty=true;
    if(section_==3){previewForward_=false;preview_.reset(0,now);replay();}
    KillTimer(hwnd_,5);dwellItem_=-1;openItem_=-1;opens_.clear();
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
    case SettingControl::Town:if(h.part>=1)townChoose(h.part-1);else if(h.part==0)townFocus(true);break;
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
std::vector<Hit> SettingsUi::focusOrder(){std::vector<Hit> order;for(int i=0;i<int(settingSections().size());++i)order.push_back({-1,-1,i});float height;for(auto& row:layout(height)){auto& item=items_[row.item];if(!enabled(item)||item.control==SettingControl::Glass)continue;order.push_back({row.item,item.control==SettingControl::Toggle?0:item.control==SettingControl::Choice||item.control==SettingControl::Swatch?item.get(s_):0,-1});}return order;}
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
    else if(k==VK_SPACE||k==VK_RETURN){if(focus_.section>=0)selectSection(focus_.section);else if(focus_.item>=0){auto& item=items_[focus_.item];if(item.control==SettingControl::Town)townFocus(true);else if(item.control==SettingControl::Toggle||item.control==SettingControl::Button)activate({focus_.item,0,-1},0);else if(item.control==SettingControl::Actions||item.control==SettingControl::Preview)activate({focus_.item,std::max(0,focus_.part),-1},0);}}
    // Keep the focused row inside the viewport.
    if(focus_.item>=0){float height;for(auto& row:layout(height))if(row.item==focus_.item){float top=row.row.top,bottom=row.row.bottom;if(top<CardTop-10)scrollTarget_-=CardTop-10-top;else if(bottom>H()-20)scrollTarget_+=bottom-(H()-20);float max=std::max(0.f,height-H());scrollTarget_=std::clamp(scrollTarget_,0.,double(max));aim(scroll_,scrollTarget_,Scroll);}}
}
// ---- Phase 5G: the Town field ---------------------------------------------------------------------------------
void SettingsUi::townFocus(bool on){
    if(on==townFocus_)return;townFocus_=on;caretEpoch_=seconds();townPick_=0;
    if(on){townCaret_=townText_.size();SetTimer(hwnd_,3,265,nullptr);
        // The field (and room for its matches) scrolls into view.
        float height;for(auto& row:layout(height))if(items_[row.item].control==SettingControl::Town){const float bottom=row.row.top+64+6*38+10;if(bottom>H()-20||row.row.top<CardTop-10){scrollTarget_=std::clamp(scrollTarget_+(bottom>H()-20?bottom-(H()-20):row.row.top-(CardTop-10)),0.,double(std::max(0.f,height+6*38-H())));aim(scroll_,scrollTarget_,Scroll);}}}
    else{KillTimer(hwnd_,3);KillTimer(hwnd_,2);}dirty=true;
}
void SettingsUi::townEdited(){caretEpoch_=seconds();townPick_=0;SetTimer(hwnd_,2,320,nullptr);dirty=true;}
void SettingsUi::townChoose(int index){
    if(index<0||size_t(index)>=context_.townResults.size())return;PostMessageW(island_,SettingsTownMessage,1,LPARAM(index));
    townText_.clear();townCaret_=0;townFocus(false);dirty=true;
}
bool SettingsUi::townChar(wchar_t c){
    if(c==8){if(townCaret_>0){townText_.erase(townCaret_-1,1);--townCaret_;townEdited();}return true;}
    if(c<32||c==127)return c==13||c==27||c==9?true:false;
    if(townText_.size()>=60)return true;townText_.insert(townCaret_,1,c);++townCaret_;townEdited();return true;
}
bool SettingsUi::townKey(WPARAM k){
    const bool ctrl=GetKeyState(VK_CONTROL)&0x8000;const int n=int(context_.townResults.size());
    switch(k){
    case VK_LEFT:if(townCaret_>0)--townCaret_;break;case VK_RIGHT:if(townCaret_<townText_.size())++townCaret_;break;
    case VK_HOME:townCaret_=0;break;case VK_END:townCaret_=townText_.size();break;
    case VK_DELETE:if(townCaret_<townText_.size()){townText_.erase(townCaret_,1);townEdited();}break;
    case VK_UP:if(townListed())townPick_=(townPick_+n-1)%n;break;case VK_DOWN:if(townListed())townPick_=(townPick_+1)%n;break;
    case VK_RETURN:if(townListed())townChoose(townPick_);return true;
    case VK_ESCAPE:if(!townText_.empty()){townText_.clear();townCaret_=0;}else townFocus(false);break;
    case VK_TAB:townFocus(false);return false;
    case 'V':if(!ctrl)return false;
        // Paste: the clipboard's text, one line of it.
        if(OpenClipboard(hwnd_)){if(HANDLE d=GetClipboardData(CF_UNICODETEXT))if(auto* t=static_cast<const wchar_t*>(GlobalLock(d))){std::wstring add(t);GlobalUnlock(d);for(auto& ch:add)if(ch<32)ch=L' ';
            add=add.substr(0,60-std::min<size_t>(60,townText_.size()));townText_.insert(townCaret_,add);townCaret_+=add.size();townEdited();}CloseClipboard();}break;
    case 'A':if(!ctrl)return false;townCaret_=townText_.size();break;
    default:return false;}
    caretEpoch_=seconds();dirty=true;return true;
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
    for(auto* map:{&knobs_,&hovers_,&pillX_,&pillW_,&thumbs_,&rings_,&opens_})for(auto& [k,s]:*map)if(moving(s))return true;
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
    for(size_t r=0;r<rows.size();++r){auto& row=rows[r];auto& item=items_[row.item];bool on=enabled(item);float alpha=on?1:.42f;float cy=row.row.top+row.base/2,R=row.row.right-Pad;
        if(row.row.bottom<0||row.row.top>H()+20)continue;
        if(r>0){brush_->SetColor(D2D1::ColorF(p.border));dc_->DrawLine({row.row.left+Pad,row.row.top},{row.row.right-Pad,row.row.top},brush_.Get(),1);}
        auto& rowHover=spring(hovers_,row.item*100+98,0);aim(rowHover,hover_.item==row.item&&on?1:0,Hover);fill({row.row.left+4,row.row.top+4,row.row.right-4,row.row.bottom-4},7,p.light?0x000000:0xffffff,float(at(rowHover))*(p.light?.02f:.025f));
        float textRight=item.control==SettingControl::Chips?row.row.right-Pad:row.control.left-16;
        if(item.control==SettingControl::Preview||item.control==SettingControl::Glass){text(item.title,{row.row.left+Pad,row.row.top+12,row.row.right-Pad,row.row.top+32},14,p.ink,alpha,DWRITE_FONT_WEIGHT_MEDIUM);text(detail(item),{row.row.left+Pad,row.row.top+12,row.row.right-Pad,row.row.top+32},12,p.muted,alpha,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_TEXT_ALIGNMENT_TRAILING);}
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
        case SettingControl::Glass:drawGlass(row.control,alpha,row.item);break;
        case SettingControl::Town:{
            // The field: a search mark, the text (or its hint) and a blinking caret while it has the keyboard.
            const auto c=row.parts[0];auto& h=spring(thumbs_,400000+row.item,0);aim(h,townFocus_?2:hovered(0)?1:0,Hover);const float g=float(at(h));
            fill(c,9,p.ink,(p.light?.04f:.05f)+std::min(g,1.f)*.02f);stroke(c,9,townFocus_?p.accent:p.ink,townFocus_?.95f:.12f,townFocus_?1.6f:1);
            drawIcon(dc_.Get(),factory_.Get(),Icon::Search,c.left+11,c.top+8,16,townFocus_?p.accent:p.muted);
            const D2D1_RECT_F field{c.left+36,c.top,c.right-12,c.bottom};
            if(townText_.empty()&&!townFocus_)text(context_.weatherPlace.empty()?L"Search for a town":context_.weatherPlace.substr(0,context_.weatherPlace.find(L',')),field,13,p.muted,.9f);
            else{dc_->PushAxisAlignedClip(field,D2D1_ANTIALIAS_MODE_ALIASED);const float before=measure(townText_.substr(0,townCaret_),13),shift=std::max(0.f,before-(field.right-field.left-4));
                text(townText_,{field.left-shift,field.top,field.right+400,field.bottom},13,p.ink);
                if(townFocus_&&std::fmod(now-caretEpoch_,1.06)<.53)fill({field.left-shift+before,c.top+8,field.left-shift+before+1.6f,c.bottom-8},.8f,p.accent,1);dc_->PopAxisAlignedClip();}
            // Matches: the town, then its region and country, the highlighted one outlined in the accent.
            for(size_t k=1;k<row.parts.size();++k){const auto r=row.parts[k];const int index=int(k)-1;const bool picked=index==townPick_;const std::wstring& full=context_.townResults[size_t(index)];
                auto& rh=spring(hovers_,500000+index,0);aim(rh,hovered(int(k))?1:0,Hover);fill(r,8,p.light?0x000000:0xffffff,(picked?(p.light?.05f:.07f):0.f)+float(at(rh))*(p.light?.03f:.04f));if(picked)stroke(r,8,p.accent,.7f,1.2f);
                drawIcon(dc_.Get(),factory_.Get(),Icon::Location,r.left+12,r.top+9,16,picked?p.accent:p.muted);
                const auto comma=full.find(L',');const std::wstring town=full.substr(0,comma),rest=comma==std::wstring::npos?L"":full.substr(comma+2);const float tw=std::min(260.f,measure(town,14,DWRITE_FONT_WEIGHT_MEDIUM)+2);
                text(town,{r.left+38,r.top,r.left+38+tw,r.bottom},14,p.ink,1,DWRITE_FONT_WEIGHT_MEDIUM);text(rest,{r.left+46+tw,r.top,r.right-12,r.bottom},12.5f,p.muted);}
            break;}
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
        // The row's preview, revealed from its top edge as it opens.
        if(row.open>.01f){dc_->PushAxisAlignedClip({row.row.left,row.row.top+row.base-6,row.row.right,row.row.bottom},D2D1_ANTIALIAS_MODE_ALIASED);drawScene(item,row.preview,alpha*std::min(1.f,row.open*1.3f));dc_->PopAxisAlignedClip();}
    }
    dc_->PopLayer();dc_->SetTransform(base);dc_->PopAxisAlignedClip();
    if(maxScroll>0){float track=H()-24,thumb=std::max(40.f,track*H()/contentHeight),y=12+(track-thumb)*scroll/maxScroll;fill({W()-7,y,W()-4,y+thumb},1.5f,p.ink,.22f);}
    HRESULT hr=dc_->EndDraw();if(hr==D2DERR_RECREATE_TARGET){resize();return;}check(hr);
    if(sweepShot_){sweepShot_=false;try{saveFrame(!sweepFile_.empty()?sweepDir_+L"\\"+sweepFile_:sweepDir_+L"\\settings-"+std::to_wstring(section_)+L"-"+std::to_wstring(sweepPage_)+L".png",p.bg);}catch(...){}sweepFile_.clear();}
    check(swap_->Present(1,0));dirty=false;publish();
}
// ---- 0.17.0-preview.3: pictures of what settings do -----------------------------------------------------------
bool SettingsUi::scene(const SettingItem& i)const{
    static const char* keys[]={"theme","glassTint","clearTint","restFrost","corner","shadow","edgeSplash","notifyStyle","stackAlerts","edge","compactWidth","waveformStyle","artPulse","beatEdge",
        // 0.18: more rows show what they do.
        "weatherGlass","uiMode","autoHide","hoverOpen","compactMedia","waveform","compactControls","swipeSkip","lyricsCompact","compactGlance","glanceRings","hud","weather","magnetic","animatedIcons",
        "trackHandoff","reduceMotion","islandDj","deviceCards","powerCards","privacyDots","clipboardConfirm","albumAccents"};
    for(auto* k:keys)if(i.key==k)return true;return false;
}
void SettingsUi::openPreview(int item,bool instant){
    if(openItem_>=0&&openItem_!=item)closePreview();openItem_=item;openedAt_=seconds();auto& s=spring(opens_,item,0);if(instant)s.reset(1,openedAt_);else aim(s,1,Reveal);dirty=true;
}
void SettingsUi::closePreview(){if(openItem_<0)return;aim(spring(opens_,openItem_,0),0,Reveal);openItem_=-1;dirty=true;}
bool SettingsUi::live(){
    if(s_.reduceMotion||!hwnd_||IsIconic(hwnd_)||!IsWindowVisible(hwnd_))return false;if(openItem_>=0)return true;
    float height;for(auto& row:layout(height))if(row.open>.01f||(items_[size_t(row.item)].control==SettingControl::Glass&&row.row.bottom>0&&row.row.top<H()))return true;
    return false;
}
// A stand-in wallpaper: three soft colour fields drifting slowly over a deep (or pale) base, and fine diagonal lines, so
// glass has something to blur, dim and bend. blur softens it the way frosted glass does (wider, fainter fields and lines).
void SettingsUi::backdrop(const D2D1_RECT_F& a,double t,float blur,float alpha){
    const auto p=palette();const float w=a.right-a.left,h=a.bottom-a.top;const UINT32 wall=context_.wallpaper?context_.wallpaper:0x5b7fd6;
    brush_->SetColor(D2D1::ColorF(p.light?0xdfe4ec:0x161a24,alpha));dc_->FillRectangle(a,brush_.Get());
    const UINT32 colours[3]={wall,p.accent,p.light?UINT32(0xf2a07b):UINT32(0xc0567a)};
    for(int k=0;k<3;++k){const double phase=t*(.13+.05*k)+k*2.1;const float base=h*(.62f+.12f*float(k)),r=base+blur*1.6f;
        const float cx=a.left+w*float(.5+.4*std::sin(phase)),cy=a.top+h*float(.5+.34*std::cos(phase*1.3+k)),peak=(p.light?.6f:.72f)*base/r*alpha;
        D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(colours[k],peak)},{.6f,D2D1::ColorF(colours[k],peak*.35f)},{1,D2D1::ColorF(colours[k],0.f)}};
        ComPtr<ID2D1GradientStopCollection> c;if(FAILED(dc_->CreateGradientStopCollection(stops,3,&c)))continue;ComPtr<ID2D1RadialGradientBrush> b;
        if(FAILED(dc_->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties({cx,cy},{0,0},r,r),c.Get(),&b)))continue;dc_->FillRectangle(a,b.Get());}
    brush_->SetColor(D2D1::ColorF(p.light?0x000000:0xffffff,(p.light?.13f:.10f)*alpha/(1+blur/2.5f)));
    for(float x=a.left-h;x<a.right;x+=22)dc_->DrawLine({x,a.bottom},{x+h,a.top},brush_.Get(),1+blur*.2f);
}
// The island's outline: hanging from the top edge with concave shoulders (top), or a free pill. Open (closed false): the
// same outline without the screen edge, for its rim.
ComPtr<ID2D1PathGeometry> SettingsUi::islandPath(const D2D1_RECT_F& b,float r,bool top,bool closed){
    r=std::max(1.f,std::min({r,(b.right-b.left)/2,(b.bottom-b.top)/2}));ComPtr<ID2D1PathGeometry> path;check(factory_->CreatePathGeometry(&path));ComPtr<ID2D1GeometrySink> s;check(path->Open(&s));
    const D2D1_SIZE_F arc{r,r};const auto ccw=D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,cw=D2D1_SWEEP_DIRECTION_CLOCKWISE;
    if(top){const float d=std::max(2.f,std::min(r*.8f,b.bottom-b.top-r));
        s->BeginFigure({b.left-d,b.top},closed?D2D1_FIGURE_BEGIN_FILLED:D2D1_FIGURE_BEGIN_HOLLOW);s->AddBezier({{b.left-d*.45f,b.top},{b.left,b.top+d*.55f},{b.left,b.top+d}});
        s->AddLine({b.left,b.bottom-r});s->AddArc({{b.left+r,b.bottom},arc,0,ccw,D2D1_ARC_SIZE_SMALL});s->AddLine({b.right-r,b.bottom});s->AddArc({{b.right,b.bottom-r},arc,0,ccw,D2D1_ARC_SIZE_SMALL});
        s->AddLine({b.right,b.top+d});s->AddBezier({{b.right,b.top+d*.55f},{b.right+d*.45f,b.top},{b.right+d,b.top}});s->EndFigure(closed?D2D1_FIGURE_END_CLOSED:D2D1_FIGURE_END_OPEN);}
    else{s->BeginFigure({b.left+r,b.top},D2D1_FIGURE_BEGIN_FILLED);s->AddLine({b.right-r,b.top});s->AddArc({{b.right,b.top+r},arc,0,cw,D2D1_ARC_SIZE_SMALL});s->AddLine({b.right,b.bottom-r});s->AddArc({{b.right-r,b.bottom},arc,0,cw,D2D1_ARC_SIZE_SMALL});
        s->AddLine({b.left+r,b.bottom});s->AddArc({{b.left,b.bottom-r},arc,0,cw,D2D1_ARC_SIZE_SMALL});s->AddLine({b.left,b.top+r});s->AddArc({{b.left+r,b.top},arc,0,cw,D2D1_ARC_SIZE_SMALL});s->EndFigure(D2D1_FIGURE_END_CLOSED);}
    check(s->Close());return path;
}
// The island in miniature, as the real one draws itself: solid; frosted (the wallpaper softened, dimmed by the tint, the
// frost settling as milk, light gathered at the edges); or clear (only the tint). Then its rim, and the lights: the beat,
// the alert glint (splash, 0-1 round the edge) and the pointer's glint, all on the inner half of the edge.
void SettingsUi::mini(const D2D1_RECT_F& area,const Mini& m,double t,float alpha){
    const int theme=m.theme>=0?m.theme:s_.theme;const bool light=theme==1||(theme==2&&systemLight());const auto p=palette();
    const int material=m.material>=0?m.material:s_.material;const float tint=float(m.tint>=0?m.tint:material==2?s_.clearTint:s_.glassTint)/100.f;
    auto shape=islandPath(m.box,m.radius,m.top,true),edge=islandPath(m.box,m.radius,m.top,false);
    if(m.shadow)for(int k=1;k<=4;++k){const float g=float(k)*2.4f;fill({m.box.left-g*.5f,m.box.top+1+g*.5f,m.box.right+g*.5f,m.box.bottom+2+g},m.radius+g,0x000000,(light?.03f:.055f)*alpha);}
    ComPtr<ID2D1Layer> layer;dc_->CreateLayer(nullptr,&layer);dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),shape.Get(),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,D2D1::IdentityMatrix(),alpha),layer.Get());
    const UINT32 ink=light?0xf6f7f9:0x0b0c10;
    if(material==0){brush_->SetColor(D2D1::ColorF(light?0xf7f7f9:0x090a0c));dc_->FillRectangle(area,brush_.Get());}
    else{if(material==1)backdrop(area,t,14+10*m.frost,1);
        const float a=material==2?std::clamp((light?.44f:.46f)*(.6f+tint*1.1f),.08f,light?.78f:.62f):std::clamp((light?.52f:.42f)*(.55f+tint*.9f),.06f,.92f);
        brush_->SetColor(D2D1::ColorF(ink,a));dc_->FillRectangle(area,brush_.Get());
        if(material==1&&m.frost>0){brush_->SetColor(D2D1::ColorF(light?0xffffff:0xa9b2c2,(light?.24f:.13f)*m.frost));dc_->FillRectangle(area,brush_.Get());}
        if(material==1){brush_->SetColor(D2D1::ColorF(0xffffff,light?.16f:.09f));dc_->DrawGeometry(edge.Get(),brush_.Get(),9);}}
    if(m.content){const float h=m.box.bottom-m.box.top,x=m.box.left+(h>=40?12.f:9.f);const UINT32 text=light?0x202329:0xf1f3f7;
        if(h>=40){const float s=h-24;fill({x,m.box.top+12,x+s,m.box.top+12+s},6,p.accent,.9f);fill({x+s+10,m.box.top+15,m.box.right-40,m.box.top+21},3,text,.7f);fill({x+s+10,m.box.top+27,m.box.right-70,m.box.top+32},2.5f,text,.35f);}
        else{const float cy=(m.box.top+m.box.bottom)/2;fill({x,cy-5,x+10,cy+5},3,p.accent,.9f);fill({x+16,cy-2,m.box.right-24,cy+2},2,text,.45f);}}
    if(m.beat>0){brush_->SetColor(D2D1::ColorF(p.accent,.4f*m.beat));dc_->DrawGeometry(edge.Get(),brush_.Get(),9);brush_->SetColor(D2D1::ColorF(p.accent,.85f*m.beat));dc_->DrawGeometry(edge.Get(),brush_.Get(),2.2f);}
    auto glint=[&](D2D1_POINT_2F at,float radius,float strength){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xffffff,strength)},{.4f,D2D1::ColorF(0xffffff,strength*.4f)},{1,D2D1::ColorF(0xffffff,0.f)}};
        ComPtr<ID2D1GradientStopCollection> c;if(FAILED(dc_->CreateGradientStopCollection(stops,3,&c)))return;ComPtr<ID2D1RadialGradientBrush> b;if(FAILED(dc_->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(at,{0,0},radius,radius),c.Get(),&b)))return;
        dc_->DrawGeometry(edge.Get(),b.Get(),6);dc_->DrawGeometry(edge.Get(),b.Get(),2);};
    // The alert's glint runs from the middle of the bottom edge outward both ways, fading as it goes.
    if(m.splash>=0&&m.splash<=1){const float cx=(m.box.left+m.box.right)/2,half=(m.box.right-m.box.left)/2+(m.box.bottom-m.box.top),f=m.splash,fade=f<.15f?f/.15f:1-(f-.15f)/.85f;
        for(float sign:{-1.f,1.f}){const float along=f*half;D2D1_POINT_2F at=along<(m.box.right-m.box.left)/2?D2D1_POINT_2F{cx+sign*along,m.box.bottom}:D2D1_POINT_2F{sign<0?m.box.left:m.box.right,m.box.bottom-(along-(m.box.right-m.box.left)/2)};glint(at,26,.95f*fade);}}
    if(m.glint.x>=0&&material!=0)glint(m.glint,34,light?.95f:.8f);
    dc_->PopLayer();
    if(material!=0){D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xffffff,(light?.95f:material==2?.46f:.36f)*alpha)},{1,D2D1::ColorF(0xffffff,(light?.42f:.13f)*alpha)}};ComPtr<ID2D1GradientStopCollection> c;ComPtr<ID2D1LinearGradientBrush> b;
        if(SUCCEEDED(dc_->CreateGradientStopCollection(stops,2,&c))&&SUCCEEDED(dc_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,m.box.top},{0,m.box.bottom}),c.Get(),&b)))dc_->DrawGeometry(edge.Get(),b.Get(),1.1f);}
}
// Each row's picture: the stand-in wallpaper in a rounded frame, and the island doing what the row changes.
void SettingsUi::drawScene(const SettingItem& item,const D2D1_RECT_F& a,float alpha){
    const auto p=palette();const double t=s_.reduceMotion?1.3:seconds()-openedAt_,now=s_.reduceMotion?2.:seconds();const std::string& k=item.key;
    ComPtr<ID2D1RoundedRectangleGeometry> frame;if(FAILED(factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(a,9,9),&frame)))return;ComPtr<ID2D1Layer> layer;dc_->CreateLayer(nullptr,&layer);
    dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),frame.Get(),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,D2D1::IdentityMatrix(),alpha),layer.Get());backdrop(a,now,0,1);
    const float cx=(a.left+a.right)/2,top=a.top;auto ease=[](double x){x=std::clamp(x,0.,1.);return float(x*x*(3-2*x));};
    auto phase=[&](double period){return std::fmod(t,period)/period;};
    auto box=[&](float w,float h,float y=0){return D2D1_RECT_F{cx-w/2,top+y,cx+w/2,top+y+h};};
    auto mix=[&](const D2D1_RECT_F& x,const D2D1_RECT_F& y,float f){return D2D1_RECT_F{lerp(x.left,y.left,f),lerp(x.top,y.top,f),lerp(x.right,y.right,f),lerp(x.bottom,y.bottom,f)};};
    // Open, hold, close, rest: the island breathing between compact and open every four seconds.
    const double c=phase(4.);const float open=ease(c<.25?c*4:c<.55?1.:c<.8?(.8-c)*4:0.);const D2D1_RECT_F compact=box(118,22),expanded=box(236,62);
    // One beat every half second (120 bpm): a flare that falls away.
    const float beat=.22f+.7f*float(std::exp(-std::fmod(t,.5)*9));
    Mini m;m.box=mix(compact,expanded,open);m.radius=lerp(11,16,open);
    if(k=="corner"){m.radius=lerp(11,float(s_.corner)*.72f,open);}
    else if(k=="glassTint"){m.material=1;m.box=expanded;}
    else if(k=="clearTint"){m.material=2;m.box=expanded;}
    else if(k=="restFrost"){m.material=1;m.box=expanded;const double f=phase(6.);m.frost=s_.restFrost?(f<.7?ease(f/.7):1-ease((f-.7)/.12)):0.f;
        // The pointer arrives, and the frost clears.
        if(f>.66){const float q=ease((f-.66)/.1);const D2D1_POINT_2F at{lerp(a.right-30,cx+60,q),lerp(a.bottom-10,expanded.bottom-12,q)};brush_->SetColor(D2D1::ColorF(0xffffff,.95f));dc_->FillEllipse(D2D1::Ellipse(at,4,4),brush_.Get());if(q>.9f)m.glint=at;}}
    else if(k=="shadow"){m.top=false;m.box=box(200,40,14+3*ease(phase(3.)<.5?phase(3.)*2:2-phase(3.)*2));m.radius=20;m.shadow=s_.shadow;}
    else if(k=="edgeSplash"){m.box=box(170,30);m.radius=15;if(s_.edgeSplash){const double f=std::fmod(t,2.6);if(f<1.)m.splash=float(f);}}
    else if(k=="beatEdge"){m.box=box(170,30);m.radius=15;m.beat=s_.beatEdge?beat:0.f;}
    else if(k=="notifyStyle"||k=="stackAlerts"){
        const double f=phase(4.2);const float out=ease(f<.3?f/.3:f<.75?1.:(.9-f)/.15);
        if(k=="notifyStyle"&&s_.notifyStyle==0){m.box=mix(compact,box(210,54),out);m.radius=lerp(11,18,out);}
        else{m.box=compact;m.content=false;mini(a,m,now,1);Mini pill;pill.top=false;pill.box=box(lerp(118,190,out),lerp(22,34,out),lerp(0,28,out));pill.radius=lerp(11,17,out);mini(a,pill,now,out);
            if(k=="stackAlerts"&&s_.stackAlerts){const float bud=ease((f-.3)/.2)*out;if(bud>0){Mini b;b.top=false;b.content=false;b.box=box(lerp(40,110,bud),lerp(8,22,bud),pill.box.bottom-top+4*bud);b.radius=lerp(4,11,bud);mini(a,b,now,bud);}}
            dc_->PopLayer();return;}}
    else if(k=="edge"){
        // A small screen, with the island on the chosen edge.
        const D2D1_RECT_F screen{cx-92,a.top+8,cx+92,a.bottom-8};fill(screen,6,0x000000,.28f);stroke(screen,6,0xffffff,.25f,1);dc_->PushAxisAlignedClip(screen,D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const float my=(screen.top+screen.bottom)/2;Mini s;s.content=false;s.shadow=false;
        if(s_.edge==0){s.box={cx-34,screen.top,cx+34,screen.top+14};s.radius=7;}else{s.top=false;s.radius=8;s.box=s_.edge==1?D2D1_RECT_F{screen.right-18,my-26,screen.right+8,my+26}:D2D1_RECT_F{screen.left-8,my-26,screen.left+18,my+26};}
        mini(a,s,now,1);dc_->PopAxisAlignedClip();dc_->PopLayer();return;}
    else if(k=="compactWidth"){m.box=box(float(s_.compactWidth)*.5f,22);m.radius=11;}
    else if(k=="waveformStyle"){m.box=box(170,26);m.radius=13;m.content=false;mini(a,m,now,1);
        // The title line stops short of the sound, which sits at the island's end.
        const float y=(m.box.top+m.box.bottom)/2,x=m.box.right-30;const bool light=s_.theme==1||(s_.theme==2&&systemLight());
        fill({m.box.left+9,y-5,m.box.left+19,y+5},3,p.accent,.9f);fill({m.box.left+25,y-2,x-10,y+2},2,light?0x202329:0xf1f3f7,.45f);brush_->SetColor(D2D1::ColorF(p.accent));
        if(s_.waveformStyle==0){for(int b=0;b<5;++b){const float v=.3f+.7f*float(std::abs(std::sin(t*5.3+b*1.7)));const float bh=4+9*v;dc_->FillRoundedRectangle(D2D1::RoundedRect({x+b*4.5f,y-bh/2,x+b*4.5f+2.4f,y+bh/2},1.2f,1.2f),brush_.Get());}}
        else{const float r=7+1.4f*beat;for(int q=0;q<16;++q){const float ang=float(q)*6.2831853f/16;const float v=.4f+.6f*float(std::abs(std::sin(t*4+q)));dc_->DrawLine({x+10+std::cos(ang)*r,y+std::sin(ang)*r},{x+10+std::cos(ang)*(r+2.5f*v),y+std::sin(ang)*(r+2.5f*v)},brush_.Get(),1.4f);}}
        dc_->PopLayer();return;}
    // ---- 0.18 ----
    else if(k=="weatherGlass"||k=="uiMode"||k=="autoHide"||k=="hoverOpen"||k=="compactMedia"||k=="waveform"||k=="compactControls"||k=="swipeSkip"||k=="lyricsCompact"||k=="compactGlance"||k=="glanceRings"||k=="hud"||k=="weather"||
            k=="magnetic"||k=="animatedIcons"||k=="trackHandoff"||k=="reduceMotion"||k=="islandDj"||k=="deviceCards"||k=="powerCards"||k=="privacyDots"||k=="clipboardConfirm"||k=="albumAccents"){
        const bool light=s_.theme==1||(s_.theme==2&&systemLight());const UINT32 tc=light?0x202329:0xf1f3f7;auto on=[&](const char* key){for(auto& i:items_)if(i.key==key&&i.get)return i.get(s_)!=0;return false;};
        m.content=false;m.box=box(170,26);m.radius=13;
        auto cy=[&]{return (m.box.top+m.box.bottom)/2;};auto line=[&](float x0,float x1,float alpha){fill({x0,cy()-2,x1,cy()+2},2,tc,alpha);};auto art=[&](float x,float size,UINT32 c){fill({x,cy()-size/2,x+size,cy()+size/2},size*.28f,c,.95f);};
        auto cursor=[&](D2D1_POINT_2F at){brush_->SetColor(D2D1::ColorF(0xffffff,.95f));dc_->FillEllipse(D2D1::Ellipse(at,4,4),brush_.Get());brush_->SetColor(D2D1::ColorF(0x000000,.35f));dc_->DrawEllipse(D2D1::Ellipse(at,4,4),brush_.Get(),1);};
        if(k=="weatherGlass"){m.material=s_.material?s_.material:1;m.box=expanded;m.radius=16;m.content=true;mini(a,m,now,1);
            // Rain running down the pane, and drops on it, while the setting is on.
            if(on("weatherGlass")){auto shape=islandPath(m.box,m.radius,true,true);ComPtr<ID2D1Layer> l;dc_->CreateLayer(nullptr,&l);dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),shape.Get()),l.Get());
                brush_->SetColor(D2D1::ColorF(0xffffff,light?.35f:.3f));for(int q=0;q<26;++q){const float x=m.box.left+std::fmod(q*37.f,m.box.right-m.box.left+30),y=m.box.top+float(std::fmod(q*23.+t*95.,90.))-14;dc_->DrawLine({x,y},{x-2.4f,y+12},brush_.Get(),1);}
                for(int q=0;q<9;++q){const float x=m.box.left+12+std::fmod(q*53.f,m.box.right-m.box.left-24),y=m.box.top+10+std::fmod(q*17.f,44.f),r=1.4f+float(q%3);brush_->SetColor(D2D1::ColorF(0xffffff,.55f));dc_->DrawEllipse(D2D1::Ellipse({x,y},r,r*1.1f),brush_.Get(),.7f);}
                dc_->PopLayer();}}
        else if(k=="uiMode"){const int mode=s_.uiMode;m.box=box(mode==0?72.f:mode==1?210.f:250.f,26);mini(a,m,now,1);
            if(mode==0){fill({cx-10,cy()-3,cx+10,cy()+3},3,tc,.35f);}
            else if(mode==1){art(m.box.left+10,16,p.accent);line(m.box.left+32,m.box.right-60,.55f);for(int b=0;b<4;++b){const float v=.35f+.65f*float(std::abs(std::sin(t*5+b)));fill({m.box.right-40+b*6.f,cy()-4*v-2,m.box.right-37.5f+b*6.f,cy()+4*v+2},1,p.accent,1);}}
            else{const Icon g[]={Icon::Clock,Icon::Volume,Icon::Battery,Icon::Processor};for(int q=0;q<4;++q){const float x=m.box.left+14+q*58.f;drawIcon(dc_.Get(),factory_.Get(),g[q],x,cy()-6,12,tc,.75f);fill({x+16,cy()-2,x+40,cy()+2},2,tc,.5f);}}}
        else if(k=="autoHide"){const double f=phase(4.);const float hide=on("autoHide")?ease(f<.2?f/.2:f<.6?1.:f<.75?(.75-f)/.15:0.):0.f;m.box=box(150,26,-23*hide);mini(a,m,now,1);art(m.box.left+10,14,p.accent);line(m.box.left+30,m.box.right-18,.5f);
            if(on("autoHide")&&f>.55&&f<.8)cursor({cx+20,a.top+4});}
        else if(k=="hoverOpen"){const double f=phase(3.6);const float o=on("hoverOpen")?ease(f<.35?0.:f<.55?(f-.35)/.2:f<.85?1.:(1-f)/.15):0.f;m.box=mix(box(150,26),box(236,70),o);m.radius=lerp(13,17,o);m.content=o>.5f;mini(a,m,now,1);
            if(o<=.5f){art(m.box.left+10,14,p.accent);line(m.box.left+30,m.box.right-18,.5f);}cursor({lerp(a.right-40,cx+30,ease(f*3)),lerp(a.bottom-10,a.top+14,ease(f*3))});}
        else if(k=="compactMedia"){mini(a,m,now,1);if(on("compactMedia")){art(m.box.left+8,16,p.accent);line(m.box.left+30,m.box.right-40,.6f);}else{drawIcon(dc_.Get(),factory_.Get(),Icon::Clock,m.box.left+10,cy()-6,12,tc,.75f);line(m.box.left+28,m.box.left+60,.55f);}}
        else if(k=="waveform"){mini(a,m,now,1);art(m.box.left+8,16,p.accent);line(m.box.left+30,m.box.right-50,.5f);
            for(int b=0;b<6;++b){const float v=on("waveform")?.3f+.7f*float(std::abs(std::sin(t*5.3+b*1.3))):.3f;fill({m.box.right-40+b*5.f,cy()-6*v,m.box.right-37.6f+b*5.f,cy()+6*v},1.2f,p.accent,1);}}
        else if(k=="compactControls"){m.box=box(210,26);mini(a,m,now,1);art(m.box.left+8,16,p.accent);line(m.box.left+30,m.box.right-(on("compactControls")?80:20),.5f);
            if(on("compactControls")){const Icon g[]={Icon::Previous,Icon::Play,Icon::Next};for(int q=0;q<3;++q)drawIcon(dc_.Get(),factory_.Get(),g[q],m.box.right-70+q*20.f,cy()-6,12,tc,.9f);}}
        else if(k=="swipeSkip"){mini(a,m,now,1);const double f=phase(2.6);const float slide=on("swipeSkip")?ease(f<.4?f/.4:f<.5?1.:0.)*-40:0.f;dc_->PushAxisAlignedClip(m.box,D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);art(m.box.left+8+slide,16,p.accent);line(m.box.left+30+slide,m.box.right-20+slide,.5f);dc_->PopAxisAlignedClip();
            if(on("swipeSkip")&&f<.5){cursor({cx+30-80*ease(f/.4),cy()});fill({m.box.right+8,cy()-10,m.box.right+30,cy()+10},10,p.accent,.9f*ease(f/.4));drawIcon(dc_.Get(),factory_.Get(),Icon::Next,m.box.right+12,cy()-6,12,0xffffff,ease(f/.4));}}
        else if(k=="lyricsCompact"){m.box=box(230,26);mini(a,m,now,1);art(m.box.left+8,16,p.accent);
            if(on("lyricsCompact")){const float x0=m.box.left+30,x1=m.box.right-16,sung=x0+(x1-x0)*float(phase(3.));line(x0,x1,.3f);fill({x0,cy()-2,sung,cy()+2},2,p.accent,1);}else line(m.box.left+30,m.box.right-60,.55f);}
        else if(k=="compactGlance"){mini(a,m,now,1);if(on("compactGlance")){text(L"Sat 26",{m.box.left+10,m.box.top,m.box.left+70,m.box.bottom},10.5f,tc,.9f,DWRITE_FONT_WEIGHT_SEMI_BOLD);drawIcon(dc_.Get(),factory_.Get(),Icon::Processor,m.box.right-70,cy()-6,12,tc,.7f);line(m.box.right-54,m.box.right-40,.5f);drawIcon(dc_.Get(),factory_.Get(),Icon::Gauge,m.box.right-34,cy()-6,12,tc,.7f);line(m.box.right-18,m.box.right-8,.5f);}}
        else if(k=="glanceRings"){mini(a,m,now,1);line(m.box.left+12,m.box.right-60,.5f);const int rings=s_.glanceRings;
            for(int q=0;q<2;++q){if(!(rings==3||rings==q+1))continue;const float rx=m.box.right-(rings==3?40.f-q*18:24.f),v=q?float(phase(5.)):.64f;brush_->SetColor(D2D1::ColorF(tc,.2f));dc_->DrawEllipse(D2D1::Ellipse({rx,cy()},6.5f,6.5f),brush_.Get(),2);
                ComPtr<ID2D1PathGeometry> g;factory_->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> sk;g->Open(&sk);const float ang=v*6.2831853f;sk->BeginFigure({rx,cy()-6.5f},D2D1_FIGURE_BEGIN_HOLLOW);sk->AddArc({{rx+6.5f*std::sin(ang),cy()-6.5f*std::cos(ang)},{6.5f,6.5f},0,D2D1_SWEEP_DIRECTION_CLOCKWISE,v>.5f?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL});sk->EndFigure(D2D1_FIGURE_END_OPEN);sk->Close();
                brush_->SetColor(D2D1::ColorF(q?0xf0b44c:0x5fd98a));dc_->DrawGeometry(g.Get(),brush_.Get(),2);}}
        else if(k=="hud"){const double f=phase(3.);const float o=on("hud")?ease(f<.15?f/.15:f<.7?1.:(.85-f)/.15):0.f;m.box=mix(box(150,26),box(236,30),o);mini(a,m,now,1);
            if(o>.3f){drawIcon(dc_.Get(),factory_.Get(),Icon::Volume,m.box.left+10,cy()-6,12,tc,o);fill({m.box.left+30,cy()-2,m.box.right-16,cy()+2},2,tc,.2f*o);fill({m.box.left+30,cy()-2,m.box.left+30+(m.box.right-m.box.left-46)*float(.3+.4*std::min(1.,f/.5)),cy()+2},2,p.accent,o);}
            else{art(m.box.left+10,14,p.accent);line(m.box.left+30,m.box.right-18,.5f);}}
        else if(k=="weather"){mini(a,m,now,1);line(m.box.left+12,m.box.right-60,.5f);if(on("weather")){drawIcon(dc_.Get(),factory_.Get(),Icon::PartlyCloudy,m.box.right-48,cy()-6,12,tc,.85f);text(L"14\u00b0",{m.box.right-32,m.box.top,m.box.right-6,m.box.bottom},10.5f,tc,.9f,DWRITE_FONT_WEIGHT_SEMI_BOLD);}}
        else if(k=="magnetic"||k=="animatedIcons"){m.box=expanded;m.radius=16;mini(a,m,now,1);const double f=phase(2.4);
            for(int q=0;q<3;++q){const float bx=cx-60+q*60.f,by=m.box.top+36;const bool hot=q==int(f*3);float dx=0,lift=0;if(hot&&k=="magnetic"&&on("magnetic"))dx=float(std::sin(f*18.8))*4;if(hot&&k=="animatedIcons"&&on("animatedIcons"))lift=-3*float(std::abs(std::sin(f*18.8)));
                if(hot)fill({bx-16+dx,by-14,bx+16+dx,by+14},10,tc,.12f);const Icon g[]={Icon::Home,Icon::Music,Icon::Settings};drawIcon(dc_.Get(),factory_.Get(),g[q],bx-8+dx,by-8+lift,16,tc,.85f);}}
        else if(k=="trackHandoff"){m.box=expanded;m.radius=16;mini(a,m,now,1);const double f=phase(3.);const float mixv=on("trackHandoff")?ease((f-.4)/.3):(f<.55?0.f:1.f);const float s0=38,x=m.box.left+12,y=m.box.top+12;
            fill({x,y,x+s0,y+s0},8,p.accent,.95f*(1-mixv));fill({x,y,x+s0,y+s0},8,0xf2a07b,.95f*mixv);line(x+50,m.box.right-40,.6f);}
        else if(k=="reduceMotion"){const double f=phase(3.);const bool open=f>.5;const float o=on("reduceMotion")?(open?1.f:0.f):ease(open?(f-.5)*6:(1-(f*6)));m.box=mix(compact,expanded,std::clamp(o,0.f,1.f));m.radius=lerp(11,16,std::clamp(o,0.f,1.f));m.content=o>.6f;mini(a,m,now,1);}
        else if(k=="islandDj"){m.box=box(170,30);m.radius=15;mini(a,m,now,1);const float glow=on("islandDj")?.5f+.5f*float(std::sin(t*3)):0.f;const float ax=m.box.left+8;
            if(glow>0){brush_->SetColor(D2D1::ColorF(p.accent,.6f*glow));dc_->DrawRoundedRectangle(D2D1::RoundedRect({ax-3,cy()-11,ax+21,cy()+11},7,7),brush_.Get(),2);}art(ax,18,p.accent);line(ax+28,m.box.right-18,.5f);}
        else if(k=="deviceCards"||k=="powerCards"||k=="clipboardConfirm"){const double f=phase(3.4);const bool show=on(k.c_str());const float o=show?ease(f<.2?f/.2:f<.75?1.:(.9-f)/.15):0.f;
            m.box=mix(box(150,26),box(250,54),o);m.radius=lerp(13,18,o);mini(a,m,now,1);
            if(o>.4f){const Icon g=k=="deviceCards"?Icon::Earbuds:k=="powerCards"?Icon::Bolt:Icon::Copy;const wchar_t* title=k=="deviceCards"?L"Earbuds connected":k=="powerCards"?L"Charging \u00b7 64%":L"Copied";
                drawIcon(dc_.Get(),factory_.Get(),g,m.box.left+14,cy()-9,18,k=="powerCards"?0x5fd98a:tc,o);text(title,{m.box.left+42,cy()-9,m.box.right-12,cy()+9},11.5f,tc,o,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
            else{art(m.box.left+10,14,p.accent);line(m.box.left+30,m.box.right-18,.5f);}}
        else if(k=="privacyDots"){mini(a,m,now,1);art(m.box.left+8,16,p.accent);line(m.box.left+30,m.box.right-40,.5f);if(on("privacyDots")){brush_->SetColor(D2D1::ColorF(0x4cd964,.6f+.4f*float(std::sin(t*2.4))));dc_->FillEllipse(D2D1::Ellipse({m.box.right-22,cy()},3.2f,3.2f),brush_.Get());brush_->SetColor(D2D1::ColorF(0xff9f0a));dc_->FillEllipse(D2D1::Ellipse({m.box.right-12,cy()},3.2f,3.2f),brush_.Get());}}
        else if(k=="albumAccents"){m.box=expanded;m.radius=16;mini(a,m,now,1);const UINT32 c=on("albumAccents")?0xe0567a:p.accent;const float x=m.box.left+12,y=m.box.top+12;fill({x,y,x+38,y+38},8,0xe0567a,.95f);
            fill({x+50,m.box.top+44,m.box.right-14,m.box.top+47},1.5f,tc,.2f);fill({x+50,m.box.top+44,x+50+(m.box.right-x-64)*float(phase(6.)),m.box.top+47},1.5f,c,1);line(x+50,m.box.right-40,.6f);}
        dc_->PopLayer();return;}
    else if(k=="artPulse"){m.box=expanded;m.radius=16;m.content=false;mini(a,m,now,1);const float s=(62-24)*(1+(s_.artPulse?.06f*(beat-.22f)/.7f:0.f)),x=m.box.left+12+19,y=m.box.top+12+19;
        fill({x-s/2,y-s/2,x+s/2,y+s/2},6,p.accent,.95f);const UINT32 text=(s_.theme==1||(s_.theme==2&&systemLight()))?0x202329:0xf1f3f7;fill({m.box.left+60,m.box.top+15,m.box.right-40,m.box.top+21},3,text,.7f);fill({m.box.left+60,m.box.top+27,m.box.right-70,m.box.top+32},2.5f,text,.35f);dc_->PopLayer();return;}
    mini(a,m,now,1);dc_->PopLayer();
}
// The glass preview: the island over a moving wallpaper in the chosen theme, material and tint. Point at it and its light
// follows the pointer on the rim; away from it Frosted glass frosts over (a few seconds here); music's beat lights the edge.
void SettingsUi::drawGlass(const D2D1_RECT_F& a,float alpha,int item){
    const auto p=palette();const double now=seconds();const bool over=hover_.item==item&&inside(a,pointerX_,pointerY_);
    ComPtr<ID2D1RoundedRectangleGeometry> frame;if(FAILED(factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(a,10,10),&frame)))return;ComPtr<ID2D1Layer> layer;dc_->CreateLayer(nullptr,&layer);
    const double bt=s_.reduceMotion?2.:now;dc_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),frame.Get(),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,D2D1::IdentityMatrix(),alpha),layer.Get());backdrop(a,bt,0,1);
    // The frost level eases toward its target: in over four seconds, out in a third of one.
    const bool frosting=s_.restFrost&&s_.material==1&&!over&&!s_.reduceMotion;const double dt=std::clamp(now-frostAt_,0.,.1);frostAt_=now;frostLevel_=frosting?std::min(1.,frostLevel_+dt/4):std::max(0.,frostLevel_-dt/.3);
    const float cx=(a.left+a.right)/2;Mini m;m.box={cx-std::min(170.f,(a.right-a.left)*.34f),a.top,cx+std::min(170.f,(a.right-a.left)*.34f),a.top+72};m.radius=std::clamp(float(s_.corner)*.8f,14.f,22.f);m.frost=float(frostLevel_*frostLevel_*(3-2*frostLevel_));
    if(s_.beatEdge&&!s_.reduceMotion)m.beat=.22f+.7f*float(std::exp(-std::fmod(now,.5)*9));if(over&&s_.material!=0)m.glint={pointerX_,pointerY_};m.shadow=s_.shadow;mini(a,m,bt,1);
    // What it shows, on a small chip.
    const wchar_t* names[]={L"Solid",L"Frosted glass",L"Clear glass"};std::wstring label=names[std::clamp(s_.material,0,2)];if(s_.material)label+=L"  ·  "+std::to_wstring(s_.material==2?s_.clearTint:s_.glassTint)+L"% tint";
    const float lw=measure(label,11.5f,DWRITE_FONT_WEIGHT_MEDIUM)+20;const D2D1_RECT_F chip{a.left+10,a.bottom-30,a.left+10+lw,a.bottom-10};fill(chip,10,p.card,.86f);text(label,chip,11.5f,p.ink,1,DWRITE_FONT_WEIGHT_MEDIUM,DWRITE_TEXT_ALIGNMENT_CENTER);
    dc_->PopLayer();
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
    case SweepMessage:{std::unique_ptr<std::wstring> dir(reinterpret_cast<std::wstring*>(l));if(dir){sweepDir_=*dir;sweepPage_=0;if(section_!=0)selectSection(0);SetTimer(h,4,1200,nullptr);}return 0;}
    case TypeTownMessage:{std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(l));if(text){townFocus(true);townText_=*text;townCaret_=townText_.size();townEdited();}return 0;}
    case ShowMessage:ShowWindow(h,IsIconic(h)?SW_RESTORE:SW_SHOW);SetForegroundWindow(h);refresh();return 0;
    case WM_TIMER:if(w==1){KillTimer(h,1);dirty=true;}
        // 2: the typing has paused, so the town is looked up; 3: the caret blinks.
        if(w==2){KillTimer(h,2);const auto q=trimmedTown();if(q.size()>=2){auto owned=std::make_unique<std::wstring>(q);if(PostMessageW(island_,SettingsTownMessage,0,reinterpret_cast<LPARAM>(owned.get())))owned.release();}}
        if(w==3)dirty=true;if(w==4){KillTimer(h,4);sweepStep();}
        if(w==5){KillTimer(h,5);if(dwellItem_>=0&&hover_.item==dwellItem_&&press_.item<0&&dragItem_<0)openPreview(dwellItem_);dwellItem_=-1;}return 0;
    case WM_MOUSEMOVE:{auto [x,y]=point();if(!tracking_){TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);tracking_=true;}if(dragItem_>=0){slide(dragItem_,x);return 0;}if(chipDrag_>=0){chipX_=x;dirty=true;
            // Phase 5G: a soft click each time the dragged chip passes another's place.
            float height;for(auto& row:layout(height))if(row.item==press_.item){const int to=chipTarget(row);if(to!=chipHeard_){if(s_.sounds)playSound(Sound::Click);chipHeard_=to;}}return 0;}auto next=hit(x,y);pointerX_=x;pointerY_=y;
            if(next.item!=hover_.item){KillTimer(h,5);dwellItem_=-1;if(openItem_>=0&&next.item!=openItem_)closePreview();if(next.item>=0&&next.item!=openItem_&&scene(items_[size_t(next.item)])&&enabled(items_[size_t(next.item)])){dwellItem_=next.item;SetTimer(h,5,450,nullptr);}}
            if(!(next==hover_)){hover_=next;dirty=true;}return 0;}
    case WM_MOUSELEAVE:tracking_=false;KillTimer(h,5);dwellItem_=-1;pointerX_=pointerY_=-1;if(dragItem_<0){hover_={};closePreview();dirty=true;}return 0;
    case WM_LBUTTONDOWN:{auto [x,y]=point();keyboard_=false;press_=hit(x,y);SetCapture(h);KillTimer(h,5);dwellItem_=-1;
        if(townFocus_&&!(press_.item>=0&&items_[press_.item].control==SettingControl::Town&&press_.part>=0))townFocus(false);if(press_.item>=0&&items_[press_.item].control==SettingControl::Slider&&press_.part==0&&enabled(items_[press_.item])){dragItem_=press_.item;slide(dragItem_,x);}
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
    case WM_KEYDOWN:if(townFocus_&&townKey(w))return 0;key(w);return 0;
    case WM_CHAR:if(townFocus_&&townChar(wchar_t(w)))return 0;break;
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
void SettingsWindow::sweep(const std::wstring& dir){HWND h=window_.load();if(!h)return;auto owned=std::make_unique<std::wstring>(dir);if(PostMessageW(h,SweepMessage,0,reinterpret_cast<LPARAM>(owned.get())))owned.release();}
void SettingsWindow::typeTown(const std::wstring& text){HWND h=window_.load();if(!h)return;auto owned=std::make_unique<std::wstring>(text);if(PostMessageW(h,TypeTownMessage,0,reinterpret_cast<LPARAM>(owned.get())))owned.release();}
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
            // A moving picture on screen: a frame about every 30 ms, waking at once for input.
            else if(ui.live()){moving=true;MsgWaitForMultipleObjects(0,nullptr,FALSE,28,QS_ALLINPUT);while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(running&&IsWindow(h))ui.render();}
            // The last animated frame can land just before the springs settle; draw the resting frame once before blocking.
            else if(moving){moving=false;if(IsWindow(h))ui.render();}
            else{if(GetMessageW(&msg,nullptr,0,0)<=0)break;TranslateMessage(&msg);DispatchMessageW(&msg);if(ui.dirty&&IsWindow(h))ui.render();}
        }
        window_=nullptr;
    }
    CoUninitialize();
}
}
