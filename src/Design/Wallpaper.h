#pragma once
#include "Common/Win32.h"
#include "Design/Accent.h"
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace nexus {
// The desktop wallpaper's accent, or 0 when there is no picture wallpaper (a solid
// colour, slideshow in transition or unreadable file). Decoded small; call off the UI thread.
inline uint32_t wallpaperAccent(){
    wchar_t path[MAX_PATH*2]{};if(!SystemParametersInfoW(SPI_GETDESKWALLPAPER,MAX_PATH*2,path,0)||!path[0])return 0;
    ComPtr<IWICImagingFactory> f;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))return 0;
    ComPtr<IWICBitmapDecoder> d;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> cv;UINT w=0,h=0;
    if(FAILED(f->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&d))||FAILED(d->GetFrame(0,&frame))||FAILED(frame->GetSize(&w,&h))||!w||!h)return 0;
    const UINT side=48;if(FAILED(f->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(frame.Get(),side,side,WICBitmapInterpolationModeFant))||FAILED(f->CreateFormatConverter(&cv))||
        FAILED(cv->Initialize(scaler.Get(),GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return 0;
    uint8_t px[side*side*4];if(FAILED(cv->CopyPixels(nullptr,side*4,sizeof(px),px)))return 0;
    double r=0,g=0,b=0,weight=0;for(UINT i=0;i<side*side;++i){double B=px[i*4]/255.,G=px[i*4+1]/255.,R=px[i*4+2]/255.;const double sat=std::max({R,G,B})-std::min({R,G,B}),wt=.02+sat*sat;r+=R*wt;g+=G*wt;b+=B*wt;weight+=wt;}
    return weight>0?pastelAccent(r/weight,g/weight,b/weight):0;
}
}
