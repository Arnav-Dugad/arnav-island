#pragma once
#include "Common/Win32.h"
#include <mmdeviceapi.h>
namespace nexus {
// Optional compatibility boundary: Windows has no documented system-default
// output setter. Never load this adapter unless the directAudio flag is enabled.
// ABI slot positions cross-checked against EarTrumpet's public interop declarations.
struct AudioPolicy: IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Reserved1()=0;virtual HRESULT STDMETHODCALLTYPE Reserved2()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved3()=0;virtual HRESULT STDMETHODCALLTYPE Reserved4()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved5()=0;virtual HRESULT STDMETHODCALLTYPE Reserved6()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved7()=0;virtual HRESULT STDMETHODCALLTYPE Reserved8()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved9()=0;virtual HRESULT STDMETHODCALLTYPE Reserved10()=0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR,ERole)=0;
};
inline HRESULT setAudioOutput(const std::wstring& id,bool enabled){if(!enabled)return E_ACCESSDENIED;CLSID cls{};IID iid{};CLSIDFromString(L"{870af99c-171d-4f9e-af0d-e63df40c2bc9}",&cls);CLSIDFromString(L"{f8679f50-850a-41cf-9c72-430f290290c8}",&iid);ComPtr<AudioPolicy> policy;HRESULT hr=CoCreateInstance(cls,nullptr,CLSCTX_INPROC_SERVER,iid,reinterpret_cast<void**>(policy.GetAddressOf()));if(FAILED(hr))return hr;hr=policy->SetDefaultEndpoint(id.c_str(),eConsole);if(FAILED(hr))return hr;return policy->SetDefaultEndpoint(id.c_str(),eMultimedia);}
}
