#pragma once
#include "Common/Win32.h"
#include <dwrite_3.h>
namespace nexus {
// Segoe UI Variable's optical sizes: Small keeps captions open and legible, Text
// suits body copy, Display tightens large headings.
inline const wchar_t* fontFamily(float size){return size<11.5f?L"Segoe UI Variable Small":size>=20?L"Segoe UI Variable Display":L"Segoe UI Variable Text";}
// Text on composition surfaces is grayscale-antialiased (the surfaces are
// transparent, so ClearType cannot apply). Grid-fitted, symmetric rendering with
// raised contrast keeps stems on whole pixels, which reads markedly sharper.
inline ComPtr<IDWriteRenderingParams> sharpTextParams(IDWriteFactory* factory){
    ComPtr<IDWriteFactory3> modern;ComPtr<IDWriteRenderingParams3> params;
    if(!factory||FAILED(factory->QueryInterface(IID_PPV_ARGS(&modern)))||FAILED(modern->CreateCustomRenderingParams(1.8f,1.f,1.f,0.f,DWRITE_PIXEL_GEOMETRY_FLAT,DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC,DWRITE_GRID_FIT_MODE_ENABLED,&params)))return nullptr;
    return params;
}
}
