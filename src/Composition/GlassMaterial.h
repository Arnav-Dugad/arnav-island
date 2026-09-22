#pragma once
#include "Common/Win32.h"
#include <dwmapi.h>
namespace nexus {
class GlassMaterial {
    HWND window_=nullptr;bool supported_=false;
public:
    ~GlassMaterial(){if(window_)DestroyWindow(window_);}
    bool show(HWND host,bool enabled,bool light,RECT interior){
        if(!enabled){if(window_)ShowWindow(window_,SW_HIDE);return false;}
        HIGHCONTRASTW contrast{sizeof(contrast)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);
        if(contrast.dwFlags&HCF_HIGHCONTRASTON||power.SystemStatusFlag){hide();return false;}
        if(!window_){WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ArnavIsland.Glass";wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));RegisterClassW(&wc);window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);}
        if(!window_)return false;DWM_SYSTEMBACKDROP_TYPE type=DWMSBT_TRANSIENTWINDOW;BOOL dark=!light;MARGINS margins{-1,-1,-1,-1};
        supported_=SUCCEEDED(DwmSetWindowAttribute(window_,DWMWA_SYSTEMBACKDROP_TYPE,&type,sizeof(type)))&&SUCCEEDED(DwmExtendFrameIntoClientArea(window_,&margins));
        if(!supported_){hide();return false;}DwmSetWindowAttribute(window_,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
                COLORREF border=DWMWA_COLOR_NONE;DwmSetWindowAttribute(window_,DWMWA_BORDER_COLOR,&border,sizeof(border));DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_DONOTROUND;DwmSetWindowAttribute(window_,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));RECT r{};GetWindowRect(host,&r);r.right=r.left+interior.right;r.bottom=r.top+interior.bottom;r.left+=interior.left;r.top+=interior.top;
        SetWindowPos(window_,host,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);return true;
    }
    void hide(){if(window_)ShowWindow(window_,SW_HIDE);}
};
}
