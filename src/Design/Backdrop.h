#pragma once
#include "Common/Win32.h"
#include <shobjidl.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace nexus {
// Phase 5F, adaptive text: what lies behind the island when nothing but the wallpaper does.
// The monitor's wallpaper, placed as Windows places it, reduced to a small luminance map
// (0..255 per cell; about 4 screen pixels per cell on a 1920-pixel monitor).
struct WallpaperLuma{RECT monitor{};int w=0,h=0;std::vector<uint8_t> luma;};
// The luminance seen through the glass under the compact island, in canvas DIPs (x0 + column*cell).
struct LumaGrid{float x0=0,cell=2;int cols=0,rows=0;std::vector<uint8_t> luma;
    uint8_t at(float x,float y)const{if(cols<=0||rows<=0)return 0;const int c=std::clamp(int((x-x0)/cell),0,cols-1),r=std::clamp(int(y/cell),0,rows-1);return luma[size_t(r)*size_t(cols)+size_t(c)];}};
// Behind this luminance a letter reads better dark than light.
inline constexpr int brightBackdrop=150;
inline uint8_t lumaOf(double r,double g,double b){return uint8_t(std::clamp(std::lround(.2126*r+.7152*g+.0722*b),0L,255L));}
// Decodes the monitor's wallpaper into a map (fill, fit, stretch, centre and tile as Windows
// does them; span is treated as fill). A solid colour desktop gives a uniform map. Call off the UI thread.
inline std::shared_ptr<WallpaperLuma> wallpaperLuma(RECT monitor){
    const int mw=monitor.right-monitor.left,mh=monitor.bottom-monitor.top;if(mw<=0||mh<=0)return nullptr;
    auto out=std::make_shared<WallpaperLuma>();out->monitor=monitor;out->w=480;out->h=std::max(1,int(std::lround(480.0*mh/mw)));
    std::wstring path;DESKTOP_WALLPAPER_POSITION position=DWPOS_FILL;COLORREF background=GetSysColor(COLOR_DESKTOP);
    {static constexpr GUID desktopWallpaper{0xc2cf3110,0x460e,0x4fc1,{0xb9,0xd0,0x8a,0x1c,0x0c,0x9c,0xc4,0xbd}};ComPtr<IDesktopWallpaper> desk;
        if(SUCCEEDED(CoCreateInstance(desktopWallpaper,nullptr,CLSCTX_ALL,IID_PPV_ARGS(&desk)))){desk->GetPosition(&position);desk->GetBackgroundColor(&background);UINT count=0;desk->GetMonitorDevicePathCount(&count);
            for(UINT i=0;i<count&&path.empty();++i){LPWSTR id=nullptr;if(FAILED(desk->GetMonitorDevicePathAt(i,&id))||!id)continue;RECT r{};if(SUCCEEDED(desk->GetMonitorRECT(id,&r))&&EqualRect(&r,&monitor)){LPWSTR file=nullptr;if(SUCCEEDED(desk->GetWallpaper(id,&file))&&file){path=file;CoTaskMemFree(file);}}CoTaskMemFree(id);}}}
    if(path.empty()){wchar_t file[MAX_PATH*2]{};if(SystemParametersInfoW(SPI_GETDESKWALLPAPER,MAX_PATH*2,file,0))path=file;}
    out->luma.assign(size_t(out->w)*size_t(out->h),lumaOf(GetRValue(background),GetGValue(background),GetBValue(background)));
    if(path.empty())return out;
    ComPtr<IWICImagingFactory> f;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))return out;
    ComPtr<IWICBitmapDecoder> d;ComPtr<IWICBitmapFrameDecode> frame;UINT iw=0,ih=0;
    if(FAILED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&d))||FAILED(d->GetFrame(0,&frame))||FAILED(frame->GetSize(&iw,&ih))||!iw||!ih)return out;
    // The picture's size on the map, by placement.
    const double perPixel=double(out->w)/mw;double sw=0,sh=0;
    switch(position){
    case DWPOS_STRETCH:sw=out->w;sh=out->h;break;
    case DWPOS_FIT:{const double s=std::min(double(out->w)/iw,double(out->h)/ih);sw=iw*s;sh=ih*s;break;}
    case DWPOS_CENTER:case DWPOS_TILE:sw=iw*perPixel;sh=ih*perPixel;break;
    default:{const double s=std::max(double(out->w)/iw,double(out->h)/ih);sw=iw*s;sh=ih*s;break;}}
    const UINT tw=UINT(std::clamp(std::lround(sw),1L,4096L)),th=UINT(std::clamp(std::lround(sh),1L,4096L));
    ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> cv;
    if(FAILED(f->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(frame.Get(),tw,th,WICBitmapInterpolationModeFant))||FAILED(f->CreateFormatConverter(&cv))||
        FAILED(cv->Initialize(scaler.Get(),GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return out;
    std::vector<uint8_t> px(size_t(tw)*th*4);if(FAILED(cv->CopyPixels(nullptr,tw*4,UINT(px.size()),px.data())))return out;
    const bool tile=position==DWPOS_TILE;const int ox=tile?0:(out->w-int(tw))/2,oy=tile?0:(out->h-int(th))/2;
    for(int y=0;y<out->h;++y)for(int x=0;x<out->w;++x){int sx=x-ox,sy=y-oy;if(tile){sx%=int(tw);sy%=int(th);}if(sx<0||sy<0||sx>=int(tw)||sy>=int(th))continue;
        const uint8_t* p=&px[(size_t(sy)*tw+size_t(sx))*4];out->luma[size_t(y)*size_t(out->w)+size_t(x)]=lumaOf(p[2],p[1],p[0]);}
    return out;
}
}
