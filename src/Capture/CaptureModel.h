#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>
namespace nexus {
// What the capture overlay was opened to do.
enum class CaptureMode {Snip,Text,Colour};
struct PixelRect {int left=0,top=0,right=0,bottom=0;int width()const{return right-left;}int height()const{return bottom-top;}bool empty()const{return width()<=0||height()<=0;}
    bool contains(int x,int y)const{return x>=left&&x<right&&y>=top&&y<bottom;}bool operator==(const PixelRect&)const=default;};
// The rectangle between two drag points, clipped to `bounds`.
inline PixelRect dragRect(int x0,int y0,int x1,int y1,const PixelRect& bounds){
    PixelRect r{std::min(x0,x1),std::min(y0,y1),std::max(x0,x1),std::max(y0,y1)};
    r.left=std::clamp(r.left,bounds.left,bounds.right);r.right=std::clamp(r.right,bounds.left,bounds.right);r.top=std::clamp(r.top,bounds.top,bounds.bottom);r.bottom=std::clamp(r.bottom,bounds.top,bounds.bottom);return r;
}
inline PixelRect intersect(const PixelRect& a,const PixelRect& b){PixelRect r{std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)};if(r.empty())return {};return r;}
// Windows listed top to bottom (z-order): the first one under the point, clipped to the screen.
inline int windowAt(const std::vector<PixelRect>& windows,int x,int y){for(size_t i=0;i<windows.size();++i)if(windows[i].contains(x,y))return int(i);return -1;}
// Colour text for the clipboard: "#3A7BD5" and "rgb(58, 123, 213)".
inline std::wstring hexColor(uint32_t rgb){wchar_t b[16];swprintf(b,16,L"#%02X%02X%02X",(rgb>>16)&255,(rgb>>8)&255,rgb&255);return b;}
inline std::wstring rgbText(uint32_t rgb){wchar_t b[32];swprintf(b,32,L"rgb(%u, %u, %u)",(rgb>>16)&255,(rgb>>8)&255,rgb&255);return b;}
// "Snip 2026-09-24 091530.png"; the same second gets " (2)", " (3)"...
inline std::wstring snipName(int year,int month,int day,int hour,int minute,int second,int copy=1){wchar_t b[64];if(copy>1)swprintf(b,64,L"Snip %04d-%02d-%02d %02d%02d%02d (%d).png",year,month,day,hour,minute,second,copy);else swprintf(b,64,L"Snip %04d-%02d-%02d %02d%02d%02d.png",year,month,day,hour,minute,second);return b;}
// Scale for text recognition: small regions are enlarged (up to 3x, to about 1600 px on the long
// side) because small glyphs read poorly; large ones shrink to the engine's limit.
inline double ocrScale(int width,int height,int maxDimension){
    const double longest=std::max(width,height);if(longest<=0)return 1;double s=longest<1600?std::min(3.,1600./longest):1.;
    if(longest*s>maxDimension)s=maxDimension/longest;return std::max(s,.05);
}
// Recognised lines joined with newlines, blank and whitespace-only lines dropped.
inline std::wstring joinLines(const std::vector<std::wstring>& lines){
    std::wstring out;for(auto line:lines){while(!line.empty()&&std::iswspace(line.back()))line.pop_back();size_t a=0;while(a<line.size()&&std::iswspace(line[a]))++a;if(a==line.size())continue;if(!out.empty())out+=L'\n';out+=line.substr(a);}return out;
}
inline int wordCount(const std::wstring& text){int n=0;bool in=false;for(wchar_t c:text){bool space=std::iswspace(c)!=0;if(!space&&!in)++n;in=!space;}return n;}
// A file name next to `name` that does not exist yet: "photo.png" -> "photo (2).png".
template<class Exists> std::wstring freeName(const std::wstring& stem,const std::wstring& extension,Exists exists){
    for(int i=1;i<1000;++i){std::wstring n=i==1?stem+extension:stem+L" ("+std::to_wstring(i)+L")"+extension;if(!exists(n))return n;}return stem+L" (1000)"+extension;
}
}
