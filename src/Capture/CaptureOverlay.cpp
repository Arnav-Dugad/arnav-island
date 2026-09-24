#include "Capture/CaptureOverlay.h"
#include "Design/Type.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <cmath>
#include <tuple>
namespace nexus::captureOverlay {
namespace {
#ifndef WDA_EXCLUDEFROMCAPTURE
constexpr DWORD WDA_EXCLUDEFROMCAPTURE=0x11;
#endif
constexpr UINT_PTR AnimationTimer=1;
struct State {
    HWND window=nullptr,owner=nullptr;CaptureMode mode=CaptureMode::Snip;uint32_t accent=0xa4deca;bool reduced=false;
    PixelRect screen;int width=0,height=0;std::vector<uint8_t> pixels;std::vector<PixelRect> windows;size_t monitors=0;
    ComPtr<ID2D1Factory> factory;ComPtr<ID2D1HwndRenderTarget> target;ComPtr<ID2D1Bitmap> shot;ComPtr<IDWriteFactory> write;
    POINT cursor{};POINT anchor{};bool down=false,dragging=false;PixelRect selection;int hovered=-1;
    double opened=0,flashStart=-1;std::unique_ptr<CaptureResult> pending;bool closing=false;
};
State* current=nullptr;
double clockNow(){LARGE_INTEGER f,c;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return double(c.QuadPart)/double(f.QuadPart);}
double ease(double t){t=std::clamp(t,0.,1.);return 1-(1-t)*(1-t)*(1-t);}
float scaleAt(POINT p){UINT dx=96,dy=96;if(FAILED(GetDpiForMonitor(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),MDT_EFFECTIVE_DPI,&dx,&dy)))dx=96;return float(dx)/96.f;}
PixelRect monitorAt(POINT p){MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&mi);return {int(mi.rcMonitor.left),int(mi.rcMonitor.top),int(mi.rcMonitor.right),int(mi.rcMonitor.bottom)};}
uint32_t pixelAt(const State& s,int x,int y){x=std::clamp(x-s.screen.left,0,s.width-1);y=std::clamp(y-s.screen.top,0,s.height-1);const uint8_t* p=&s.pixels[(size_t(y)*s.width+x)*4];return (uint32_t(p[2])<<16)|(uint32_t(p[1])<<8)|p[0];}
// Frozen copy of every monitor, with the island left out of it.
bool grab(State& s){
    s.screen={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),0,0};s.screen.right=s.screen.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);s.screen.bottom=s.screen.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
    s.width=s.screen.width();s.height=s.screen.height();if(s.width<=0||s.height<=0)return false;
    const bool hidden=s.owner&&SetWindowDisplayAffinity(s.owner,WDA_EXCLUDEFROMCAPTURE);if(hidden)DwmFlush();
    HDC screenDc=GetDC(nullptr);HDC memory=CreateCompatibleDC(screenDc);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=s.width;bi.bmiHeader.biHeight=-s.height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(screenDc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);bool ok=false;
    if(bitmap&&memory){HGDIOBJ old=SelectObject(memory,bitmap);ok=BitBlt(memory,0,0,s.width,s.height,screenDc,s.screen.left,s.screen.top,SRCCOPY|CAPTUREBLT)!=FALSE;GdiFlush();
        if(ok){s.pixels.assign(static_cast<uint8_t*>(bits),static_cast<uint8_t*>(bits)+size_t(s.width)*s.height*4);for(size_t i=3;i<s.pixels.size();i+=4)s.pixels[i]=255;}SelectObject(memory,old);}
    if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);ReleaseDC(nullptr,screenDc);
    if(hidden)SetWindowDisplayAffinity(s.owner,WDA_NONE);
    return ok;
}
// Visible top-level windows, top to bottom, then each monitor (so a click on the desktop takes that screen).
void listWindows(State& s){
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& s=*reinterpret_cast<State*>(p);if(h==s.owner||!IsWindowVisible(h)||IsIconic(h))return TRUE;
        BOOL cloaked=FALSE;DwmGetWindowAttribute(h,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)return TRUE;
        wchar_t cls[64]{};GetClassNameW(h,cls,64);if(wcscmp(cls,L"Progman")==0||wcscmp(cls,L"WorkerW")==0)return TRUE;
        RECT r{};if(FAILED(DwmGetWindowAttribute(h,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r))))GetWindowRect(h,&r);
        PixelRect rect=intersect({int(r.left),int(r.top),int(r.right),int(r.bottom)},s.screen);if(rect.width()>=24&&rect.height()>=24&&s.windows.size()<512)s.windows.push_back(rect);return TRUE;},reinterpret_cast<LPARAM>(&s));
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR,HDC,LPRECT r,LPARAM p)->BOOL{auto& s=*reinterpret_cast<State*>(p);s.windows.push_back({int(r->left),int(r->top),int(r->right),int(r->bottom)});++s.monitors;return TRUE;},reinterpret_cast<LPARAM>(&s));
}
ComPtr<IDWriteTextFormat> format(State& s,float size,DWRITE_FONT_WEIGHT weight){ComPtr<IDWriteTextFormat> f;s.write->CreateTextFormat(fontFamily(size/1.25f),nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-us",&f);
    if(f){f->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);}return f;}
float textWidth(State& s,const std::wstring& text,IDWriteTextFormat* f){ComPtr<IDWriteTextLayout> l;if(!f||FAILED(s.write->CreateTextLayout(text.c_str(),UINT32(text.size()),f,4000,200,&l)))return 0;DWRITE_TEXT_METRICS m{};l->GetMetrics(&m);return m.widthIncludingTrailingWhitespace;}
// A rounded pill of text (dark glass look), centred at x.
void pill(State& s,ID2D1SolidColorBrush* b,const std::wstring& text,float cx,float top,float sf,float alpha,uint32_t swatch=0xffffffff){
    auto f=format(s,13*sf,DWRITE_FONT_WEIGHT_SEMI_BOLD);const float tw=textWidth(s,text,f.Get()),h=30*sf,extra=swatch!=0xffffffff?22*sf:0,w=tw+28*sf+extra,x=cx-w/2;
    b->SetColor(D2D1::ColorF(0x15171c,.92f*alpha));s.target->FillRoundedRectangle(D2D1::RoundedRect({x,top,x+w,top+h},h/2,h/2),b);
    b->SetColor(D2D1::ColorF(0xffffff,.14f*alpha));s.target->DrawRoundedRectangle(D2D1::RoundedRect({x+.5f,top+.5f,x+w-.5f,top+h-.5f},h/2,h/2),b,1);
    if(extra>0){b->SetColor(D2D1::ColorF(swatch,alpha));s.target->FillEllipse(D2D1::Ellipse({x+14*sf+7*sf,top+h/2},7*sf,7*sf),b);b->SetColor(D2D1::ColorF(0xffffff,.5f*alpha));s.target->DrawEllipse(D2D1::Ellipse({x+14*sf+7*sf,top+h/2},7*sf,7*sf),b,1);}
    b->SetColor(D2D1::ColorF(0xf4f5f7,alpha));s.target->DrawText(text.c_str(),UINT32(text.size()),f.Get(),{x+14*sf+extra,top,x+w-14*sf,top+h},b);
}
PixelRect focus(const State& s){if(s.dragging)return s.selection;if(s.hovered>=0&&size_t(s.hovered)<s.windows.size())return s.windows[size_t(s.hovered)];return {};}
void paint(State& s){
    if(!s.target)return;const double now=clockNow();const float k=s.reduced?1.f:float(ease((now-s.opened)/.16));
    const float ox=float(s.screen.left),oy=float(s.screen.top);const float sf=scaleAt(s.cursor);const PixelRect monitor=monitorAt(s.cursor);
    s.target->BeginDraw();s.target->SetTransform(D2D1::Matrix3x2F::Identity());s.target->DrawBitmap(s.shot.Get(),D2D1::RectF(0,0,float(s.width),float(s.height)),1,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    ComPtr<ID2D1SolidColorBrush> b;s.target->CreateSolidColorBrush(D2D1::ColorF(0,0.f),&b);
    auto local=[&](const PixelRect& r){return D2D1::RectF(float(r.left)-ox,float(r.top)-oy,float(r.right)-ox,float(r.bottom)-oy);};
    if(s.mode!=CaptureMode::Colour){
        const PixelRect f=focus(s);b->SetColor(D2D1::ColorF(0x05070b,.46f*k));
        if(f.empty())s.target->FillRectangle(D2D1::RectF(0,0,float(s.width),float(s.height)),b.Get());
        else{const auto r=local(f);s.target->FillRectangle(D2D1::RectF(0,0,float(s.width),r.top),b.Get());s.target->FillRectangle(D2D1::RectF(0,r.bottom,float(s.width),float(s.height)),b.Get());
            s.target->FillRectangle(D2D1::RectF(0,r.top,r.left,r.bottom),b.Get());s.target->FillRectangle(D2D1::RectF(r.right,r.top,float(s.width),r.bottom),b.Get());
            // Flash on capture, then a crisp border with accent corners.
            if(s.flashStart>=0){b->SetColor(D2D1::ColorF(0xffffff,float(.38*(1-ease((now-s.flashStart)/.14)))));s.target->FillRectangle(r,b.Get());}
            b->SetColor(D2D1::ColorF(0xffffff,.92f*k));s.target->DrawRectangle(D2D1::RectF(r.left+.5f,r.top+.5f,r.right-.5f,r.bottom-.5f),b.Get(),std::max(1.f,1.25f*sf));
            const float c=std::min({14*sf,(r.right-r.left)/2,(r.bottom-r.top)/2}),w=3*sf;b->SetColor(D2D1::ColorF(s.accent,k));
            for(auto [x,y,dx,dy]:{std::tuple{r.left,r.top,1.f,1.f},std::tuple{r.right,r.top,-1.f,1.f},std::tuple{r.left,r.bottom,1.f,-1.f},std::tuple{r.right,r.bottom,-1.f,-1.f}}){
                s.target->FillRectangle(D2D1::RectF(std::min(x,x+dx*c),std::min(y,y+dy*w),std::max(x,x+dx*c),std::max(y,y+dy*w)),b.Get());s.target->FillRectangle(D2D1::RectF(std::min(x,x+dx*w),std::min(y,y+dy*c),std::max(x,x+dx*w),std::max(y,y+dy*c)),b.Get());}
            // Size of what will be captured, just below it (above when there is no room).
            const std::wstring size=std::to_wstring(f.width())+L" × "+std::to_wstring(f.height());float top=r.bottom+10*sf;if(top+30*sf>float(monitor.bottom)-oy)top=r.top-40*sf;if(top<float(monitor.top)-oy)top=r.top+10*sf;
            pill(s,b.Get(),size,(r.left+r.right)/2,top,sf,k);}
    }else{
        b->SetColor(D2D1::ColorF(0x05070b,.12f*k));s.target->FillRectangle(D2D1::RectF(0,0,float(s.width),float(s.height)),b.Get());
        // Loupe: 11 x 11 pixels around the pointer, the centre one outlined.
        const int n=11;const float cell=std::round(12*sf),size=cell*n,cx=float(s.cursor.x)-ox,cy=float(s.cursor.y)-oy;float lx=cx+26*sf,ly=cy+26*sf;
        if(lx+size>float(monitor.right)-ox)lx=cx-26*sf-size;if(ly+size+44*sf>float(monitor.bottom)-oy)ly=cy-26*sf-size-44*sf;
        const auto frame=D2D1::RoundedRect({lx-3*sf,ly-3*sf,lx+size+3*sf,ly+size+3*sf},14*sf,14*sf);b->SetColor(D2D1::ColorF(0x15171c,.95f*k));s.target->FillRoundedRectangle(frame,b.Get());
        ComPtr<ID2D1RoundedRectangleGeometry> clip;s.factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect({lx,ly,lx+size,ly+size},11*sf,11*sf),&clip);
        s.target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clip.Get()),nullptr);
        for(int j=0;j<n;++j)for(int i=0;i<n;++i){b->SetColor(D2D1::ColorF(pixelAt(s,s.cursor.x+i-n/2,s.cursor.y+j-n/2),k));s.target->FillRectangle(D2D1::RectF(lx+i*cell,ly+j*cell,lx+(i+1)*cell,ly+(j+1)*cell),b.Get());}
        b->SetColor(D2D1::ColorF(0x000000,.10f*k));for(int i=1;i<n;++i){s.target->DrawLine({lx+i*cell,ly},{lx+i*cell,ly+size},b.Get(),1);s.target->DrawLine({lx,ly+i*cell},{lx+size,ly+i*cell},b.Get(),1);}
        s.target->PopLayer();
        const float mx=lx+(n/2)*cell,my=ly+(n/2)*cell;b->SetColor(D2D1::ColorF(0x000000,.85f*k));s.target->DrawRectangle(D2D1::RectF(mx-1,my-1,mx+cell+1,my+cell+1),b.Get(),2);b->SetColor(D2D1::ColorF(0xffffff,k));s.target->DrawRectangle(D2D1::RectF(mx,my,mx+cell,my+cell),b.Get(),1.5f);
        b->SetColor(D2D1::ColorF(0xffffff,.16f*k));s.target->DrawRoundedRectangle(frame,b.Get(),1);
        const uint32_t colour=pixelAt(s,s.cursor.x,s.cursor.y);const bool shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;
        pill(s,b.Get(),shift?rgbText(colour):hexColor(colour),lx+size/2,ly+size+10*sf,sf,k,colour);
    }
    // What to do, at the top of the monitor under the pointer.
    const wchar_t* hint=s.mode==CaptureMode::Snip?L"Drag to snip  ·  click a window or screen  ·  Esc to cancel":s.mode==CaptureMode::Text?L"Drag over text to copy it  ·  click a window  ·  Esc to cancel":L"Click to copy a colour  ·  Shift for RGB  ·  arrow keys to nudge  ·  Esc to cancel";
    if(!s.dragging)pill(s,b.Get(),hint,float(monitor.left+monitor.right)/2-ox,float(monitor.top)-oy+22*sf,sf,k);
    if(s.target->EndDraw()==D2DERR_RECREATE_TARGET){s.target.Reset();s.shot.Reset();}
}
void finish(State& s,std::unique_ptr<CaptureResult> r){
    // Exactly one answer per overlay: destroying the window deactivates it, which must not cancel again.
    if(s.closing)return;s.closing=true;HWND owner=s.owner;if(!PostMessageW(owner,CaptureMessage,0,reinterpret_cast<LPARAM>(r.get())))r.reset();else r.release();DestroyWindow(s.window);
}
void confirm(State& s){
    if(s.mode==CaptureMode::Colour){auto r=std::make_unique<CaptureResult>();r->mode=s.mode;r->colour=pixelAt(s,s.cursor.x,s.cursor.y);r->rgb=(GetKeyState(VK_SHIFT)&0x8000)!=0;finish(s,std::move(r));return;}
    const PixelRect f=intersect(focus(s),s.screen);if(f.width()<4||f.height()<4)return;
    auto r=std::make_unique<CaptureResult>();r->mode=s.mode;r->rect=f;r->width=f.width();r->height=f.height();r->pixels.resize(size_t(r->width)*r->height*4);
    for(int y=0;y<r->height;++y)memcpy(&r->pixels[size_t(y)*r->width*4],&s.pixels[(size_t(f.top-s.screen.top+y)*s.width+(f.left-s.screen.left))*4],size_t(r->width)*4);
    if(s.reduced){finish(s,std::move(r));return;}
    // A short flash, then hand over.
    s.dragging=true;s.selection=f;s.hovered=-1;s.flashStart=clockNow();s.pending=std::move(r);SetTimer(s.window,AnimationTimer,8,nullptr);
}
void update(State& s,POINT p){s.cursor=p;if(s.down&&!s.dragging&&(std::abs(p.x-s.anchor.x)>4||std::abs(p.y-s.anchor.y)>4))s.dragging=true;
    if(s.dragging&&s.flashStart<0)s.selection=dragRect(s.anchor.x,s.anchor.y,p.x+1,p.y+1,s.screen);
    s.hovered=s.dragging?-1:windowAt(s.windows,p.x,p.y);InvalidateRect(s.window,nullptr,FALSE);}
LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
    State* s=current&&current->window==h?current:nullptr;
    if(!s)return DefWindowProcW(h,m,w,l);
    auto screenPoint=[&]{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&p);return p;};
    switch(m){
    case WM_SETCURSOR:SetCursor(LoadCursor(nullptr,IDC_CROSS));return TRUE;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);paint(*s);EndPaint(h,&ps);return 0;}
    case WM_TIMER:if(w==AnimationTimer){const double t=clockNow();
            if(s->flashStart>=0&&t-s->flashStart>.15){KillTimer(h,AnimationTimer);finish(*s,std::move(s->pending));return 0;}
            if(s->flashStart<0&&t-s->opened>.18)KillTimer(h,AnimationTimer);InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_MOUSEMOVE:if(s->flashStart<0)update(*s,screenPoint());return 0;
    case WM_LBUTTONDOWN:if(s->flashStart>=0)return 0;SetCapture(h);s->down=true;s->anchor=screenPoint();s->cursor=s->anchor;if(s->mode==CaptureMode::Colour){s->down=false;ReleaseCapture();confirm(*s);}return 0;
    case WM_LBUTTONUP:if(!s->down)return 0;s->down=false;ReleaseCapture();update(*s,screenPoint());confirm(*s);if(current&&current->flashStart<0){current->dragging=false;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_RBUTTONUP:finish(*s,nullptr);return 0;
    // Esc works even when Windows kept the keyboard elsewhere (a capture started by a click).
    case WM_HOTKEY:if(w==0x4e50){finish(*s,nullptr);}return 0;
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){finish(*s,nullptr);return 0;}
        if(w==VK_RETURN||w==VK_SPACE){if(s->flashStart<0)confirm(*s);return 0;}
        if(w==VK_LEFT||w==VK_RIGHT||w==VK_UP||w==VK_DOWN){POINT p{};GetCursorPos(&p);p.x+=w==VK_LEFT?-1:w==VK_RIGHT?1:0;p.y+=w==VK_UP?-1:w==VK_DOWN?1:0;SetCursorPos(p.x,p.y);update(*s,p);return 0;}
        if(w==VK_SHIFT){InvalidateRect(h,nullptr,FALSE);return 0;}
        return 0;
    case WM_KEYUP:if(w==VK_SHIFT)InvalidateRect(h,nullptr,FALSE);return 0;
    // Not cancelled on deactivation: a capture started by a click may never get the keyboard, and
    // Esc (a hotkey while the overlay is open) and right-click always cancel.
    case WM_NCDESTROY:{UnregisterHotKey(h,0x4e50);delete current;current=nullptr;return 0;}
    }
    return DefWindowProcW(h,m,w,l);
}
}
bool active(){return current!=nullptr;}
bool begin(HINSTANCE instance,HWND owner,CaptureMode mode,uint32_t accent,bool reduced){
    if(current){SetForegroundWindow(current->window);return false;}
    auto s=std::make_unique<State>();s->owner=owner;s->mode=mode;s->accent=accent;s->reduced=reduced;
    if(!grab(*s))return false;if(mode!=CaptureMode::Colour)listWindows(*s);
    static bool registered=false;if(!registered){WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=L"ArnavIsland.Capture";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);registered=RegisterClassW(&wc)!=0;if(!registered)return false;}
    if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,s->factory.GetAddressOf()))||FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(s->write.GetAddressOf()))))return false;
    GetCursorPos(&s->cursor);s->opened=clockNow();current=s.release();
    current->window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"ArnavIsland.Capture",L"Arnav Island capture",WS_POPUP,current->screen.left,current->screen.top,current->width,current->height,nullptr,nullptr,instance,nullptr);
    if(!current->window){delete current;current=nullptr;return false;}
    auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);
    if(FAILED(current->factory->CreateHwndRenderTarget(props,D2D1::HwndRenderTargetProperties(current->window,D2D1::SizeU(UINT32(current->width),UINT32(current->height)),D2D1_PRESENT_OPTIONS_IMMEDIATELY),&current->target))||
       FAILED(current->target->CreateBitmap(D2D1::SizeU(UINT32(current->width),UINT32(current->height)),current->pixels.data(),UINT32(current->width*4),D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE)),&current->shot))){DestroyWindow(current->window);return false;}
    current->hovered=current->mode==CaptureMode::Colour?-1:windowAt(current->windows,current->cursor.x,current->cursor.y);
    ShowWindow(current->window,SW_SHOW);SetForegroundWindow(current->window);SetFocus(current->window);UpdateWindow(current->window);
    RegisterHotKey(current->window,0x4e50,MOD_NOREPEAT,VK_ESCAPE);
    if(!reduced)SetTimer(current->window,AnimationTimer,8,nullptr);
    return true;
}
}
