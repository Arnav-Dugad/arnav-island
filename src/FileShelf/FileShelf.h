#pragma once
#include "Common/Win32.h"
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <functional>
#include <vector>
#include <string>
namespace nexus {
struct ShelfItem {enum class Kind{File,Text}kind=Kind::File;std::wstring value,label;};
inline FORMATETC shelfFormat(CLIPFORMAT format){return {format,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};}
inline bool shelfAccepts(IDataObject* data){if(!data)return false;auto files=shelfFormat(CF_HDROP),text=shelfFormat(CF_UNICODETEXT);return SUCCEEDED(data->QueryGetData(&files))||SUCCEEDED(data->QueryGetData(&text));}
inline std::vector<ShelfItem> shelfRead(IDataObject* data){std::vector<ShelfItem> items;auto format=shelfFormat(CF_HDROP);STGMEDIUM medium{};
    if(SUCCEEDED(data->GetData(&format,&medium))){auto drop=static_cast<HDROP>(medium.hGlobal);UINT count=std::min(32u,DragQueryFileW(drop,0xffffffff,nullptr,0));for(UINT i=0;i<count;++i){UINT length=DragQueryFileW(drop,i,nullptr,0);if(length>32767)continue;std::wstring path(length+1,0);DragQueryFileW(drop,i,path.data(),length+1);path.resize(length);auto pos=path.find_last_of(L"\\/");items.push_back({ShelfItem::Kind::File,path,path.substr(pos==std::wstring::npos?0:pos+1)});}ReleaseStgMedium(&medium);return items;}
    format=shelfFormat(CF_UNICODETEXT);if(SUCCEEDED(data->GetData(&format,&medium))){SIZE_T bytes=GlobalSize(medium.hGlobal);if(bytes<=131072){auto p=static_cast<const wchar_t*>(GlobalLock(medium.hGlobal));if(p){size_t length=0,limit=bytes/sizeof(wchar_t);while(length<limit&&p[length])++length;if(length&&length<limit){std::wstring text(p,length),label=text.substr(0,64);for(auto& c:label)if(c<32)c=L' ';items.push_back({ShelfItem::Kind::Text,std::move(text),std::move(label)});}GlobalUnlock(medium.hGlobal);}}ReleaseStgMedium(&medium);}return items;
}
inline ComPtr<IDataObject> shelfData(const ShelfItem& item){ComPtr<IDataObject> data;check(SHCreateDataObject(nullptr,0,nullptr,nullptr,IID_PPV_ARGS(&data)));bool file=item.kind==ShelfItem::Kind::File;size_t bytes=(item.value.size()+(file?2:1))*sizeof(wchar_t)+(file?sizeof(DROPFILES):0);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);if(!memory)throw std::bad_alloc();auto p=static_cast<BYTE*>(GlobalLock(memory));if(!p){GlobalFree(memory);throw std::bad_alloc();}size_t offset=0;if(file){auto d=reinterpret_cast<DROPFILES*>(p);d->pFiles=sizeof(DROPFILES);d->fWide=TRUE;offset=sizeof(DROPFILES);}memcpy(p+offset,item.value.c_str(),(item.value.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);STGMEDIUM medium{TYMED_HGLOBAL};medium.hGlobal=memory;auto format=shelfFormat(file?CF_HDROP:CF_UNICODETEXT);HRESULT hr=data->SetData(&format,&medium,TRUE);if(FAILED(hr)){GlobalFree(memory);check(hr);}return data;}
class ShelfDragSource final:public IDropSource {ULONG refs_=1;public:
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IDropSource){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape,DWORD keys)override{return escape?DRAGDROP_S_CANCEL:!(keys&MK_LBUTTON)?DRAGDROP_S_DROP:S_OK;}
 HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD)override{return DRAGDROP_S_USEDEFAULTCURSORS;}
};
class ShelfDropTarget final:public IDropTarget {ULONG refs_=1;bool acceptable_=false;std::function<void(bool)> hover_;std::function<void(std::vector<ShelfItem>)> drop_;public:
 ShelfDropTarget(std::function<void(bool)> hover,std::function<void(std::vector<ShelfItem>)> drop):hover_(std::move(hover)),drop_(std::move(drop)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IDropTarget){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data,DWORD,POINTL,DWORD* effect)override{try{acceptable_=shelfAccepts(data);*effect=acceptable_?(*effect&DROPEFFECT_COPY):DROPEFFECT_NONE;hover_(acceptable_);return S_OK;}catch(...){*effect=DROPEFFECT_NONE;return E_FAIL;}}
 HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL,DWORD* effect)override{*effect=acceptable_?(*effect&DROPEFFECT_COPY):DROPEFFECT_NONE;return S_OK;}
 HRESULT STDMETHODCALLTYPE DragLeave()override{acceptable_=false;try{hover_(false);}catch(...){}return S_OK;}
 HRESULT STDMETHODCALLTYPE Drop(IDataObject* data,DWORD,POINTL,DWORD* effect)override{try{auto items=shelfRead(data);*effect=items.empty()?DROPEFFECT_NONE:(*effect&DROPEFFECT_COPY);if(*effect)drop_(std::move(items));hover_(false);return S_OK;}catch(...){*effect=DROPEFFECT_NONE;return E_FAIL;}}
};
}
