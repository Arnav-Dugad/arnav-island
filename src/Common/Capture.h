#pragma once
#include "Common/Win32.h"
#include <wincodec.h>
#include <filesystem>
namespace nexus {
// Explicit QA capture only. Never scheduled in a normal run. Captures our HWND's
// visible screen rectangle, so developer captures must not be published blindly.
inline void captureWindow(HWND window,const std::filesystem::path& path){
    RECT r{};GetWindowRect(window,&r);int w=r.right-r.left,h=r.bottom-r.top;
    HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h);auto previous=SelectObject(memory,bitmap);
    BitBlt(memory,0,0,w,h,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);SelectObject(memory,previous);DeleteDC(memory);ReleaseDC(nullptr,screen);
    try{
        ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        ComPtr<IWICBitmap> source;check(factory->CreateBitmapFromHBITMAP(bitmap,nullptr,WICBitmapIgnoreAlpha,&source));
        ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
        ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
        ComPtr<IWICBitmapFrameEncode> frame;check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));check(frame->SetSize(w,h));WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGR;check(frame->SetPixelFormat(&format));check(frame->WriteSource(source.Get(),nullptr));check(frame->Commit());check(encoder->Commit());
    }catch(...){DeleteObject(bitmap);throw;}
    DeleteObject(bitmap);
}
}
