#pragma once
#include "Common/Win32.h"
#include "Design/Accent.h"
#include <shlwapi.h>
#include <wincodec.h>
#include <cstdint>
#include <memory>
#include <vector>
// Phase 5H: a cover (or a Shelf item's preview) as a small JPEG (a PNG when it has transparent parts, like an
// icon), to travel to another PC, and back into a picture there. A 192-pixel cover is 10-20 KB. Callers need COM
// initialised on their thread.
namespace nexus {
// At most `side` pixels on its longer side; empty when it couldn't be made (or would be over `limit` bytes).
inline std::vector<uint8_t> encodeCover(const Artwork& art,UINT side,size_t limit,float quality=.84f){
    if(!art.width||!art.height||art.pixels.size()<size_t(art.width)*art.height*4)return {};
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return {};
    ComPtr<IWICBitmap> bitmap;if(FAILED(factory->CreateBitmapFromMemory(art.width,art.height,GUID_WICPixelFormat32bppPBGRA,art.width*4,UINT(art.pixels.size()),const_cast<BYTE*>(art.pixels.data()),&bitmap)))return {};
    const double k=std::min(1.,double(side)/double(std::max(art.width,art.height)));const UINT w=std::max(1u,UINT(art.width*k+.5)),h=std::max(1u,UINT(art.height*k+.5));
    bool alpha=false;for(size_t i=3;i<art.pixels.size();i+=4)if(art.pixels[i]<250){alpha=true;break;}
    ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;WICPixelFormatGUID format=alpha?GUID_WICPixelFormat32bppBGRA:GUID_WICPixelFormat24bppBGR;
    if(FAILED(factory->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(bitmap.Get(),w,h,WICBitmapInterpolationModeFant))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(scaler.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    ComPtr<IStream> out;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&out)))return {};
    ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;
    if(FAILED(factory->CreateEncoder(alpha?GUID_ContainerFormatPng:GUID_ContainerFormatJpeg,nullptr,&encoder))||FAILED(encoder->Initialize(out.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&frame,&options)))return {};
    if(!alpha){PROPBAG2 name{};name.pstrName=const_cast<LPOLESTR>(L"ImageQuality");VARIANT v;VariantInit(&v);v.vt=VT_R4;v.fltVal=quality;options->Write(1,&name,&v);}
    if(FAILED(frame->Initialize(options.Get()))||FAILED(frame->SetSize(w,h))||FAILED(frame->SetPixelFormat(&format))||FAILED(frame->WriteSource(converter.Get(),nullptr))||FAILED(frame->Commit())||FAILED(encoder->Commit()))return {};
    STATSTG stat{};if(FAILED(out->Stat(&stat,STATFLAG_NONAME))||!stat.cbSize.QuadPart||stat.cbSize.QuadPart>limit)return {};
    std::vector<uint8_t> bytes(size_t(stat.cbSize.QuadPart));LARGE_INTEGER zero{};ULONG got=0;
    if(FAILED(out->Seek(zero,STREAM_SEEK_SET,nullptr))||FAILED(out->Read(bytes.data(),ULONG(bytes.size()),&got))||got!=bytes.size())return {};
    return bytes;
}
// A picture from another PC (any format Windows reads), at most `side` pixels on its longer side and at most
// 4096 on either side before that, with its colours; null when it isn't one.
inline std::shared_ptr<const Artwork> decodeCover(const std::vector<uint8_t>& bytes,UINT side=256){
    if(bytes.empty()||bytes.size()>(1u<<20))return nullptr;
    ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(bytes.data(),UINT(bytes.size())));if(!stream)return nullptr;
    ComPtr<IWICImagingFactory> factory;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return nullptr;
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;UINT w=0,h=0;
    if(FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder))||FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&w,&h))||!w||!h||w>4096||h>4096)return nullptr;
    const double k=std::min(1.,double(side)/double(std::max(w,h)));const UINT tw=std::max(1u,UINT(w*k+.5)),th=std::max(1u,UINT(h*k+.5));
    ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(frame.Get(),tw,th,WICBitmapInterpolationModeFant))||FAILED(factory->CreateFormatConverter(&converter))||
       FAILED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return nullptr;
    auto art=std::make_shared<Artwork>();art->width=tw;art->height=th;art->pixels.resize(size_t(tw)*th*4);
    if(FAILED(converter->CopyPixels(nullptr,tw*4,UINT(art->pixels.size()),art->pixels.data())))return nullptr;
    paintArtwork(*art);return art;
}
}
