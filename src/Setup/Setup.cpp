// 0.28: Arnav Island's installer, one file (Arnav-Island-Setup-<version>.exe). It carries the signed release as resources
// (Payload.h) and installs it for this person only, no administrator needed: into %LOCALAPPDATA%\Programs\Arnav Island, or
// over the copy already in use. Then the Start menu, the Apps list (Uninstall runs ArnavIsland.exe --uninstall), start at
// sign-in and, if wanted, the phone camera's one administrator step. The island then updates itself from GitHub as ever.
//   ArnavIslandSetup.exe            the window
//   ArnavIslandSetup.exe /S         silently (no window, no camera step)
//   ArnavIslandSetup.exe /sandbox=<folder> [/S]   tests only: just the files, into that folder; nothing else is touched
#include "Setup/Install.h"
#include "Setup/Payload.h"
#include "Version.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <wrl/client.h>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <cstring>

using Microsoft::WRL::ComPtr;
namespace nexus::setup {
namespace {
struct Options{bool silent=false,sandboxed=false,unknown=false;std::wstring sandbox;};
Options options(){
    Options o;int n=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&n);
    for(int i=1;args&&i<n;++i){const std::wstring a=args[i],l=lower(a);if(l==L"/s"||l==L"-s"||l==L"/silent")o.silent=true;else if(l.rfind(L"/sandbox=",0)==0){o.sandboxed=true;o.sandbox=a.substr(9);}else o.unknown=true;}
    LocalFree(args);
    if(!o.sandbox.empty()){wchar_t full[MAX_PATH]{};if(GetFullPathNameW(o.sandbox.c_str(),MAX_PATH,full,nullptr))o.sandbox=full;while(o.sandbox.size()>3&&o.sandbox.back()==L'\\')o.sandbox.pop_back();}
    return o;
}

// The release inside: each file's bytes, straight from the resource section (nothing is unpacked anywhere else first).
struct File{std::wstring name;const BYTE* data=nullptr;DWORD size=0;};
std::vector<File> payload(){
    std::vector<File> files;
    for(int i=0;i<PAYLOAD_COUNT;++i){HRSRC r=FindResourceW(nullptr,MAKEINTRESOURCEW(PAYLOAD_FIRST+i),RT_RCDATA);HGLOBAL g=r?LoadResource(nullptr,r):nullptr;
        const auto* data=g?static_cast<const BYTE*>(LockResource(g)):nullptr;if(!data)return {};files.push_back({payloadNames[i],data,SizeofResource(nullptr,r)});}
    return files;
}
bool sameBytes(const File& f,const std::wstring& path){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size{};std::vector<BYTE> b;DWORD got=0;bool same=GetFileSizeEx(h,&size)&&size.QuadPart==f.size;
    if(same){b.resize(f.size);same=ReadFile(h,b.data(),f.size,&got,nullptr)&&got==f.size&&memcmp(b.data(),f.data,f.size)==0;}CloseHandle(h);return same;
}
bool writeFile(const std::wstring& path,const File& f){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
    DWORD done=0;const bool ok=WriteFile(h,f.data,f.size,&done,nullptr)&&done==f.size&&FlushFileBuffers(h);CloseHandle(h);if(!ok)DeleteFileW(path.c_str());return ok;
}
// Written whole beside the old file first, then swapped in, so a full disk or a pulled plug never leaves half a program.
bool place(const std::wstring& folder,const File& f){
    const std::wstring target=folder+L"\\"+f.name,fresh=target+freshSuffix,aside=folder+L"\\"+asideName(f.name);
    if(!writeFile(fresh,f))return false;if(MoveFileExW(fresh.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(aside.c_str());if(MoveFileExW(target.c_str(),aside.c_str(),MOVEFILE_REPLACE_EXISTING)&&MoveFileExW(fresh.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(fresh.c_str());return false;
}

struct Job{std::wstring folder;bool sandbox=false,camera=false;HWND owner=nullptr;};
struct Outcome{bool ok=false;std::wstring said;};
using Step=std::function<void(double,const std::wstring&)>;
Outcome install(const Job& job,const Step& step){
    const auto files=payload();if(files.size()!=PAYLOAD_COUNT)return {false,L"This setup is damaged. Download it again"};
    if(!job.sandbox){step(.04,L"Closing Arnav Island…");if(!closeIsland())return {false,L"Arnav Island is still running and wouldn’t close. Quit it from its tray menu, then try again"};}
    const int made=SHCreateDirectoryExW(nullptr,job.folder.c_str(),nullptr);if(made!=ERROR_SUCCESS&&made!=ERROR_ALREADY_EXISTS&&made!=ERROR_FILE_EXISTS)return {false,L"Couldn’t make the folder "+job.folder};
    ULONGLONG bytes=0;for(size_t i=0;i<files.size();++i){step(.1+.5*double(i)/files.size(),L"Copying "+files[i].name+L"…");
        if(!place(job.folder,files[i]))return {false,L"Couldn’t write "+files[i].name+L" into "+job.folder};bytes+=files[i].size;}
    if(job.sandbox){step(1,L"Test install done: files only");return {true,L"Test install done: files only"};}
    step(.64,L"Adding it to Start…");std::wstring note;
    const bool com=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
    if(!writeShortcut(job.folder))note=L"Installed, but the Start menu shortcut couldn’t be made";
    registerUninstall(job.folder,appVersion,DWORD((bytes+1023)/1024));enableStartup(job.folder);
    if(job.camera){step(.74,L"Setting up the phone camera (allow it on this PC)…");DWORD code=1;
        // Declined or not, the island is installed: it offers the camera's setup again the first time a phone's camera is used.
        const Elevated e=runElevated(job.folder+L"\\"+exeName,L"--set-up-camera --quiet",job.owner,&code);
        if(e==Elevated::Declined)note=L"Installed. The phone camera wasn’t set up: the island asks again when you first use it";
        else if(e==Elevated::Failed)note=L"Installed, but the phone camera couldn’t be set up: the island tries again when you first use it";}
    if(com)CoUninitialize();
    step(.94,L"Starting Arnav Island…");
    if(!startIsland(job.folder))return {true,L"Installed, but it didn’t start. Open it from Start"};
    return {true,note.empty()?L"Arnav Island is installed and running at the top of your screen":note};
}

// ---- The window: one small card, drawn with Direct2D, following the system's light or dark mode and the monitor's DPI.
constexpr UINT stepMessage=WM_APP+1,doneMessage=WM_APP+2;
constexpr float W=468,H=336,pad=28;
struct Theme{D2D1_COLOR_F bg,card,line,text,muted,accent,accentHover,accentPressed,onAccent,button,buttonHover,buttonLine,track,error;};
D2D1_COLOR_F rgb(UINT32 c,float a=1){return D2D1::ColorF(c,a);}
// The island's own accents: its mint on dark, its deep green on light (Design/Accent.h).
Theme theme(bool dark){
    if(dark)return {rgb(0x202020),rgb(0x2b2b2b),rgb(0x3a3a3a),rgb(0xffffff),rgb(0xababab),rgb(0xa4deca),rgb(0xb6e6d5),rgb(0x8fcab6),rgb(0x0f201a),rgb(0x2f2f2f),rgb(0x383838),rgb(0x454545),rgb(0x3c3c3c),rgb(0xff99a4)};
    return {rgb(0xf3f3f3),rgb(0xffffff),rgb(0xe3e3e3),rgb(0x1a1a1a),rgb(0x5f5f5f),rgb(0x487467),rgb(0x55857a),rgb(0x3d6359),rgb(0xffffff),rgb(0xfdfdfd),rgb(0xf5f5f5),rgb(0xd2d2d2),rgb(0xdedede),rgb(0xc42b1c)};
}
bool systemDark(){DWORD light=1,n=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&n);return light==0;}

enum class Stage{Ready,Working,Done,Failed};
enum Part{None=0,Check,Primary,Secondary};
enum class Camera{Offer,AlreadySetUp,NeedsWindows11,Skipped};
struct StepNote{double fraction;std::wstring text;};

class Window{
public:
    Window(const Options& o):options_(o){
        files_=payload();
        if(!o.sandbox.empty()){folder_=o.sandbox;where_=L"Test install (files only) into";}
        else{const auto c=findInstall();folder_=c.folder;where_=c.found==Found::Fresh?L"Installs to":L"Updates the copy in";}
        if(!cameraSupported())camera_=Camera::NeedsWindows11;
        else if(std::wstring dll;cameraSetUpForPc(&dll)&&files_.size()==PAYLOAD_COUNT&&sameBytes(files_[1],dll))camera_=Camera::AlreadySetUp;
        if(!o.sandbox.empty()&&camera_==Camera::Offer)camera_=Camera::Skipped;
        if(files_.size()!=PAYLOAD_COUNT){stage_=Stage::Failed;status_=L"This setup is damaged. Download it again";}
        else if(folder_.empty()){stage_=Stage::Failed;status_=L"Windows didn’t say where your programs go";}
    }
    int run(HINSTANCE instance){
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory_.GetAddressOf());
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write_.GetAddressOf()));
        CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic_));
        if(!factory_||!write_)return 1;fonts();
        WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"ArnavIsland.Setup";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));wc.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0));
        RegisterClassExW(&wc);
        constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
        hwnd_=CreateWindowExW(0,wc.lpszClassName,L"Arnav Island Setup",style,CW_USEDEFAULT,CW_USEDEFAULT,100,100,nullptr,nullptr,instance,this);if(!hwnd_)return 1;
        // Sized for the monitor it opens on, in the middle of its work area.
        dpi_=GetDpiForWindow(hwnd_);RECT r{0,0,px(W),px(H)};AdjustWindowRectExForDpi(&r,style,FALSE,0,dpi_);
        MONITORINFO m{sizeof(m)};GetMonitorInfoW(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTOPRIMARY),&m);const int w=r.right-r.left,h=r.bottom-r.top;
        SetWindowPos(hwnd_,nullptr,(m.rcWork.left+m.rcWork.right-w)/2,(m.rcWork.top+m.rcWork.bottom-h)/2,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
        applyTheme();ShowWindow(hwnd_,SW_SHOW);UpdateWindow(hwnd_);
        MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
        if(worker_.joinable())worker_.join();
        return stage_==Stage::Done?0:stage_==Stage::Failed?1:2;
    }
private:
    Options options_;std::vector<File> files_;std::wstring folder_,where_,status_;Stage stage_=Stage::Ready;Camera camera_=Camera::Offer;bool cameraWanted_=true,dark_=false,cues_=false;
    double fraction_=0;Part hover_=None,pressed_=None,focus_=Primary;HWND hwnd_=nullptr;UINT dpi_=96;std::thread worker_;
    ComPtr<ID2D1Factory> factory_;ComPtr<IDWriteFactory> write_;ComPtr<IWICImagingFactory> wic_;ComPtr<ID2D1HwndRenderTarget> target_;ComPtr<ID2D1SolidColorBrush> brush_;ComPtr<ID2D1Bitmap> icon_;
    ComPtr<IDWriteTextFormat> title_,body_,small_,path_,button_;

    int px(float dip)const{return int(dip*dpi_/96.f+.5f);}
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,LONG_PTR(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(self){self->hwnd_=h;return self->message(m,w,l);}return DefWindowProcW(h,m,w,l);
    }
    void fonts(){
        // Windows 11's Segoe UI Variable when it's there (its Display cut for the title), else Segoe UI.
        ComPtr<IDWriteFontCollection> system;write_->GetSystemFontCollection(&system,FALSE);UINT32 at=0;BOOL found=FALSE;
        const bool variable=system&&SUCCEEDED(system->FindFamilyName(L"Segoe UI Variable Text",&at,&found))&&found;
        const wchar_t* text=variable?L"Segoe UI Variable Text":L"Segoe UI";const wchar_t* display=variable?L"Segoe UI Variable Display":L"Segoe UI";
        auto make=[&](ComPtr<IDWriteTextFormat>& f,const wchar_t* family,float size,DWRITE_FONT_WEIGHT weight){write_->CreateTextFormat(family,nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-gb",&f);
            f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);};
        make(title_,display,24,DWRITE_FONT_WEIGHT_SEMI_BOLD);make(body_,text,14,DWRITE_FONT_WEIGHT_NORMAL);make(small_,text,12,DWRITE_FONT_WEIGHT_NORMAL);
        make(path_,text,14,DWRITE_FONT_WEIGHT_NORMAL);make(button_,text,14,DWRITE_FONT_WEIGHT_SEMI_BOLD);button_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        // A long folder keeps its last part ("…\Arnav Island"), the way File Explorer shortens paths.
        ComPtr<IDWriteInlineObject> dots;write_->CreateEllipsisTrimmingSign(path_.Get(),&dots);DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,L'\\',1};path_->SetTrimming(&trim,dots.Get());
        ComPtr<IDWriteInlineObject> dots2;write_->CreateEllipsisTrimmingSign(small_.Get(),&dots2);DWRITE_TRIMMING cut{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};small_->SetTrimming(&cut,dots2.Get());
    }
    void applyTheme(){
        dark_=systemDark();const BOOL on=dark_;DwmSetWindowAttribute(hwnd_,DWMWA_USE_IMMERSIVE_DARK_MODE,&on,sizeof(on));
        // Windows 11: the title bar takes the card's background, so window and title read as one surface (Windows 10 ignores it).
        const auto bg=theme(dark_).bg;const COLORREF caption=RGB(int(bg.r*255+.5f),int(bg.g*255+.5f),int(bg.b*255+.5f));DwmSetWindowAttribute(hwnd_,DWMWA_CAPTION_COLOR,&caption,sizeof(caption));
        InvalidateRect(hwnd_,nullptr,FALSE);
    }
    // Where each part sits, in DIPs.
    D2D1_RECT_F rectOf(Part p)const{
        if(p==Check)return {pad-4,170,W-pad,212};
        const float y=H-pad-34;
        if(p==Primary){const float w=opens()?172:116;return {W-pad-w,y,W-pad,y+34};}
        if(p==Secondary&&opens())return {W-pad-172-8-96,y,W-pad-172-8,y+34};
        return {0,0,0,0};
    }
    bool live(Part p)const{
        if(p==Check)return stage_==Stage::Ready&&camera_==Camera::Offer;
        if(p==Primary)return stage_!=Stage::Working;
        return p==Secondary&&opens();
    }
    // Finished, with an island to open (a sandbox test has none: it ends at Close).
    bool opens()const{return stage_==Stage::Done&&options_.sandbox.empty();}
    Part hit(LPARAM l)const{const float x=GET_X_LPARAM_(l)*96.f/dpi_,y=GET_Y_LPARAM_(l)*96.f/dpi_;
        for(Part p:{Check,Primary,Secondary}){const auto r=rectOf(p);if(live(p)&&x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)return p;}return None;}
    static int GET_X_LPARAM_(LPARAM l){return int(short(LOWORD(l)));}
    static int GET_Y_LPARAM_(LPARAM l){return int(short(HIWORD(l)));}

    void activate(Part p){
        if(!live(p))return;
        if(p==Check){cameraWanted_=!cameraWanted_;InvalidateRect(hwnd_,nullptr,FALSE);return;}
        if(p==Secondary){DestroyWindow(hwnd_);return;}
        if(stage_==Stage::Ready){begin();return;}
        // Done: the island is already running; this opens it (a second start hands --expanded to the one running).
        if(opens())startIsland(folder_,L"--expanded");
        DestroyWindow(hwnd_);
    }
    void begin(){
        stage_=Stage::Working;fraction_=0;status_=L"Starting…";focus_=Primary;InvalidateRect(hwnd_,nullptr,FALSE);
        Job job{folder_,!options_.sandbox.empty(),camera_==Camera::Offer&&cameraWanted_,hwnd_};HWND to=hwnd_;
        worker_=std::thread([job,to]{
            const Outcome o=install(job,[to](double f,const std::wstring& t){PostMessageW(to,stepMessage,0,LPARAM(new StepNote{f,t}));});
            PostMessageW(to,doneMessage,o.ok,LPARAM(new StepNote{1,o.said}));});
    }
    void cycle(bool back){
        std::vector<Part> parts;for(Part p:{Check,Secondary,Primary})if(live(p))parts.push_back(p);if(parts.empty())return;
        size_t i=0;while(i<parts.size()&&parts[i]!=focus_)++i;focus_=i==parts.size()?parts[0]:parts[(i+(back?parts.size()-1:1))%parts.size()];cues_=true;InvalidateRect(hwnd_,nullptr,FALSE);
    }

    LRESULT message(UINT m,WPARAM w,LPARAM l){
        switch(m){
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd_,&ps);paint();EndPaint(hwnd_,&ps);return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_SIZE:if(target_)target_->Resize(D2D1::SizeU(LOWORD(l),HIWORD(l)));return 0;
        case WM_DPICHANGED:{dpi_=HIWORD(w);const auto* r=reinterpret_cast<const RECT*>(l);icon_.Reset();if(target_)target_->SetDpi(float(dpi_),float(dpi_));
            SetWindowPos(hwnd_,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);InvalidateRect(hwnd_,nullptr,FALSE);return 0;}
        case WM_SETTINGCHANGE:if(l&&wcscmp(reinterpret_cast<const wchar_t*>(l),L"ImmersiveColorSet")==0)applyTheme();return 0;
        case WM_MOUSEMOVE:{const Part p=hit(l);if(p!=hover_){hover_=p;InvalidateRect(hwnd_,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,hwnd_,0};TrackMouseEvent(&t);return 0;}
        case WM_MOUSELEAVE:if(hover_){hover_=None;InvalidateRect(hwnd_,nullptr,FALSE);}return 0;
        case WM_LBUTTONDOWN:pressed_=hit(l);if(pressed_){SetCapture(hwnd_);focus_=pressed_;}InvalidateRect(hwnd_,nullptr,FALSE);return 0;
        case WM_LBUTTONUP:{const Part was=pressed_;pressed_=None;ReleaseCapture();InvalidateRect(hwnd_,nullptr,FALSE);if(was&&hit(l)==was)activate(was);return 0;}
        case WM_KEYDOWN:
            if(w==VK_TAB||w==VK_LEFT||w==VK_RIGHT||w==VK_UP||w==VK_DOWN){cycle(w==VK_LEFT||w==VK_UP||(w==VK_TAB&&GetKeyState(VK_SHIFT)<0));return 0;}
            if(w==VK_SPACE){cues_=true;activate(focus_);return 0;}
            if(w==VK_RETURN){activate(live(focus_)&&focus_!=Check?focus_:Primary);return 0;}
            if(w==VK_ESCAPE&&stage_!=Stage::Working){DestroyWindow(hwnd_);return 0;}
            break;
        case stepMessage:{std::unique_ptr<StepNote> n(reinterpret_cast<StepNote*>(l));fraction_=n->fraction;status_=n->text;InvalidateRect(hwnd_,nullptr,FALSE);return 0;}
        case doneMessage:{std::unique_ptr<StepNote> n(reinterpret_cast<StepNote*>(l));stage_=w?Stage::Done:Stage::Failed;fraction_=w?1:fraction_;status_=n->text;focus_=Primary;
            if(worker_.joinable())worker_.join();
            SetForegroundWindow(hwnd_);InvalidateRect(hwnd_,nullptr,FALSE);return 0;}
        case WM_CLOSE:if(stage_==Stage::Working)return 0;DestroyWindow(hwnd_);return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
        }
        return DefWindowProcW(hwnd_,m,w,l);
    }

    void makeIcon(){
        if(icon_||!wic_)return;const int size=px(44);HICON h=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101),IMAGE_ICON,size,size,0));if(!h)return;
        ComPtr<IWICBitmap> bitmap;ComPtr<IWICFormatConverter> convert;
        if(SUCCEEDED(wic_->CreateBitmapFromHICON(h,&bitmap))&&SUCCEEDED(wic_->CreateFormatConverter(&convert))&&
           SUCCEEDED(convert->Initialize(bitmap.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeMedianCut)))
            target_->CreateBitmapFromWicBitmap(convert.Get(),nullptr,&icon_);
        DestroyIcon(h);
    }
    void fill(const D2D1_RECT_F& r,float radius,const D2D1_COLOR_F& c){brush_->SetColor(c);target_->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush_.Get());}
    void stroke(const D2D1_RECT_F& r,float radius,const D2D1_COLOR_F& c,float width){brush_->SetColor(c);target_->DrawRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush_.Get(),width);}
    void text(const std::wstring& s,IDWriteTextFormat* f,const D2D1_RECT_F& r,const D2D1_COLOR_F& c){brush_->SetColor(c);target_->DrawTextW(s.c_str(),UINT32(s.size()),f,r,brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);}
    void tick(float x,float y,float size,const D2D1_COLOR_F& c,float width){
        brush_->SetColor(c);ComPtr<ID2D1PathGeometry> g;factory_->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> s;if(!g||FAILED(g->Open(&s)))return;
        s->BeginFigure(D2D1::Point2F(x+size*.22f,y+size*.52f),D2D1_FIGURE_BEGIN_HOLLOW);s->AddLine(D2D1::Point2F(x+size*.42f,y+size*.72f));s->AddLine(D2D1::Point2F(x+size*.78f,y+size*.32f));
        s->EndFigure(D2D1_FIGURE_END_OPEN);s->Close();ComPtr<ID2D1StrokeStyle> round;
        factory_->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND,D2D1_LINE_JOIN_ROUND),nullptr,0,&round);
        target_->DrawGeometry(g.Get(),brush_.Get(),width,round.Get());
    }
    void button(Part p,const std::wstring& label,bool primary,const Theme& t){
        const auto r=rectOf(p);const bool on=live(p),down=pressed_==p&&hover_==p,over=hover_==p;
        if(primary)fill(r,6,!on?t.track:down?t.accentPressed:over?t.accentHover:t.accent);
        else{fill(r,6,down?t.track:over?t.buttonHover:t.button);stroke({r.left+.5f,r.top+.5f,r.right-.5f,r.bottom-.5f},6,t.buttonLine,1);}
        text(label,button_.Get(),r,primary?(on?t.onAccent:t.muted):t.text);
        if(cues_&&focus_==p&&on)stroke({r.left-3,r.top-3,r.right+3,r.bottom+3},8,t.text,2);
    }
    void paint(){
        if(!target_){RECT c;GetClientRect(hwnd_,&c);
            if(FAILED(factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(),float(dpi_),float(dpi_)),
                D2D1::HwndRenderTargetProperties(hwnd_,D2D1::SizeU(c.right,c.bottom)),&target_)))return;
            target_->CreateSolidColorBrush(D2D1::ColorF(0),&brush_);icon_.Reset();}
        makeIcon();const Theme t=theme(dark_);
        target_->BeginDraw();target_->Clear(t.bg);
        // The heading: the island's icon, its name and the version this setup carries.
        if(icon_)target_->DrawBitmap(icon_.Get(),D2D1::RectF(pad,pad,pad+44,pad+44));
        text(L"Arnav Island",title_.Get(),{pad+58,pad-4,W-pad,pad+28},t.text);
        text(L"Version "+std::wstring(appVersion)+L"  ·  "+publisher,small_.Get(),{pad+58,pad+28,W-pad,pad+46},t.muted);
        // Where it goes.
        const D2D1_RECT_F card{pad,96,W-pad,154};fill(card,8,t.card);stroke({card.left+.5f,card.top+.5f,card.right-.5f,card.bottom-.5f},8,t.line,1);
        text(where_,small_.Get(),{pad+14,104,W-pad-14,122},t.muted);text(folder_,path_.Get(),{pad+14,122,W-pad-14,146},t.text);
        // The phone camera for other apps.
        const float cy=172;
        if(camera_==Camera::Offer||camera_==Camera::Skipped){
            const bool on=live(Check),checked=cameraWanted_&&camera_==Camera::Offer;const D2D1_RECT_F box{pad,cy+8,pad+20,cy+28};
            if(checked){fill(box,4,on?(hover_==Check?t.accentHover:t.accent):t.track);tick(box.left,box.top,20,on?t.onAccent:t.muted,2);}
            else{fill(box,4,hover_==Check?t.buttonHover:t.card);stroke({box.left+.75f,box.top+.75f,box.right-.75f,box.bottom-.75f},4,t.muted,1.5f);}
            text(L"Set up the phone camera for other apps",body_.Get(),{pad+32,cy,W-pad,cy+20},on||stage_!=Stage::Ready?t.text:t.muted);
            text(camera_==Camera::Skipped?L"(not in a test install)":L"(asks for administrator permission)",small_.Get(),{pad+32,cy+20,W-pad,cy+36},t.muted);
            if(cues_&&focus_==Check&&on)stroke({box.left-3,box.top-3,box.right+3,box.bottom+3},6,t.text,2);
        }else{
            tick(pad-2,cy+4,24,t.accent,2.2f);
            text(camera_==Camera::AlreadySetUp?L"The phone camera is set up for this PC":L"The phone camera for other apps needs Windows 11",body_.Get(),{pad+32,cy,W-pad,cy+20},t.text);
            text(camera_==Camera::AlreadySetUp?L"Other apps can pick “Arnav Island Phone Camera”":L"Everything else works here",small_.Get(),{pad+32,cy+20,W-pad,cy+36},t.muted);
        }
        // How it's going: a line and a bar once it starts; a hint before.
        const float sy=226;
        if(stage_==Stage::Ready)text(L"Takes a few seconds, no administrator needed. It starts when it’s done.",small_.Get(),{pad,sy,W-pad,sy+20},t.muted);
        else{text(status_,small_.Get(),{pad,sy,W-pad,sy+20},stage_==Stage::Failed?t.error:t.text);
            if(fraction_>0){const D2D1_RECT_F bar{pad,sy+26,W-pad,sy+30};fill(bar,2,t.track);fill({bar.left,bar.top,bar.left+float(bar.right-bar.left)*float(fraction_),bar.bottom},2,t.accent);}}
        // The buttons.
        if(opens()){button(Secondary,L"Done",false,t);button(Primary,L"Open Arnav Island",true,t);}
        else button(Primary,stage_==Stage::Ready?L"Install":stage_==Stage::Working?L"Installing…":L"Close",stage_!=Stage::Failed,t);
        if(target_->EndDraw()==HRESULT(D2DERR_RECREATE_TARGET)){target_.Reset();brush_.Reset();icon_.Reset();InvalidateRect(hwnd_,nullptr,FALSE);}
    }
};
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    using namespace nexus::setup;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const Options o=options();
    // Anything it doesn't know, or a test that meant a sandbox but named no folder, does nothing at all (rather than
    // install for real).
    if(o.unknown||(o.sandboxed&&o.sandbox.empty()))return 1;
    // One setup at a time: a second one brings the first forward.
    HANDLE single=CreateMutexW(nullptr,TRUE,L"Local\\ArnavIsland.Setup");
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(HWND other=FindWindowW(L"ArnavIsland.Setup",nullptr)){ShowWindow(other,SW_RESTORE);SetForegroundWindow(other);}if(single)CloseHandle(single);return 3;}
    int result=1;
    if(o.silent){
        Job job;if(!o.sandbox.empty()){job.folder=o.sandbox;job.sandbox=true;}else job.folder=findInstall().folder;
        result=job.folder.empty()?1:install(job,[](double,const std::wstring&){}).ok?0:1;
    }else{
        const bool com=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        {Window window(o);result=window.run(instance);}
        if(com)CoUninitialize();
    }
    if(single)CloseHandle(single);return result;
}
