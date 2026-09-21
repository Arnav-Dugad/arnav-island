#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <stdexcept>
#include <sstream>
namespace nexus {
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
inline void check(HRESULT hr) {if(FAILED(hr)){std::ostringstream s;s<<"Windows API failed: 0x"<<std::hex<<unsigned(hr);throw std::runtime_error(s.str());}}
inline double seconds() {LARGE_INTEGER q,f;QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return double(q.QuadPart)/double(f.QuadPart);}
inline LARGE_INTEGER ticks(double t) {LARGE_INTEGER f;QueryPerformanceFrequency(&f);return LARGE_INTEGER{.QuadPart=LONGLONG(t*f.QuadPart)};}
}
