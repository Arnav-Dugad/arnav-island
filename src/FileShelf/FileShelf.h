#pragma once
#include "Common/Win32.h"
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <functional>
#include <vector>
#include <string>
#include "Interaction/DashboardModel.h"
namespace nexus {
struct ShelfItem {enum class Kind{File,Text}kind=Kind::File;std::wstring value,label;std::shared_ptr<const Artwork> preview;};
inline FORMATETC shelfFormat(CLIPFORMAT format){return {format,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};}
inline bool shelfAccepts(IDataObject* data){if(!data)return false;auto files=shelfFormat(CF_HDROP),text=shelfFormat(CF_UNICODETEXT);return SUCCEEDED(data->QueryGetData(&files))||SUCCEEDED(data->QueryGetData(&text));}
inline std::vector<ShelfItem> shelfRead(IDataObject* data){std::vector<ShelfItem> items;auto format=shelfFormat(CF_HDROP);STGMEDIUM medium{};
    if(SUCCEEDED(data->GetData(&format,&medium))){auto drop=static_cast<HDROP>(medium.hGlobal);UINT count=std::min(32u,DragQueryFileW(drop,0xffffffff,nullptr,0));for(UINT i=0;i<count;++i){UINT length=DragQueryFileW(drop,i,nullptr,0);if(length>32767)continue;std::wstring path(length+1,0);DragQueryFileW(drop,i,path.data(),length+1);path.resize(length);auto pos=path.find_last_of(L"\\/");items.push_back({ShelfItem::Kind::File,path,path.substr(pos==std::wstring::npos?0:pos+1)});}ReleaseStgMedium(&medium);return items;}
    format=shelfFormat(CF_UNICODETEXT);if(SUCCEEDED(data->GetData(&format,&medium))){SIZE_T bytes=GlobalSize(medium.hGlobal);if(bytes<=131072){auto p=static_cast<const wchar_t*>(GlobalLock(medium.hGlobal));if(p){size_t length=0,limit=bytes/sizeof(wchar_t);while(length<limit&&p[length])++length;if(length&&length<limit){std::wstring text(p,length),label=text.substr(0,64);for(auto& c:label)if(c<32)c=L' ';items.push_back({ShelfItem::Kind::Text,std::move(text),std::move(label)});}GlobalUnlock(medium.hGlobal);}}ReleaseStgMedium(&medium);}return items;
}
inline ComPtr<IDataObject> shelfData(const ShelfItem& item){ComPtr<IDataObject> data;check(SHCreateDataObject(nullptr,0,nullptr,nullptr,IID_PPV_ARGS(&data)));bool file=item.kind==ShelfItem::Kind::File;size_t bytes=(item.value.size()+(file?2:1))*sizeof(wchar_t)+(file?sizeof(DROPFILES):0);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);if(!memory)throw std::bad_alloc();auto p=static_cast<BYTE*>(GlobalLock(memory));if(!p){GlobalFree(memory);throw std::bad_alloc();}size_t offset=0;if(file){auto d=reinterpret_cast<DROPFILES*>(p);d->pFiles=sizeof(DROPFILES);d->fWide=TRUE;offset=sizeof(DROPFILES);}memcpy(p+offset,item.value.c_str(),(item.value.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);STGMEDIUM medium{TYMED_HGLOBAL};medium.hGlobal=memory;auto format=shelfFormat(file?CF_HDROP:CF_UNICODETEXT);HRESULT hr=data->SetData(&format,&medium,TRUE);if(FAILED(hr)){GlobalFree(memory);check(hr);}return data;}
// Phase 5G: several files as one drag (the whole Shelf as a stack).
inline ComPtr<IDataObject> shelfFilesData(const std::vector<std::wstring>& files){ComPtr<IDataObject> data;check(SHCreateDataObject(nullptr,0,nullptr,nullptr,IID_PPV_ARGS(&data)));size_t chars=1;for(auto& f:files)chars+=f.size()+1;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(DROPFILES)+chars*sizeof(wchar_t));if(!memory)throw std::bad_alloc();auto p=static_cast<BYTE*>(GlobalLock(memory));if(!p){GlobalFree(memory);throw std::bad_alloc();}
    auto d=reinterpret_cast<DROPFILES*>(p);d->pFiles=sizeof(DROPFILES);d->fWide=TRUE;auto* out=reinterpret_cast<wchar_t*>(p+sizeof(DROPFILES));for(auto& f:files){memcpy(out,f.c_str(),f.size()*sizeof(wchar_t));out+=f.size()+1;}GlobalUnlock(memory);
    STGMEDIUM medium{TYMED_HGLOBAL};medium.hGlobal=memory;auto format=shelfFormat(CF_HDROP);HRESULT hr=data->SetData(&format,&medium,TRUE);if(FAILED(hr)){GlobalFree(memory);check(hr);}return data;}
class ShelfDragSource final:public IDropSource {ULONG refs_=1;public:
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IDropSource){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape,DWORD keys)override{return escape?DRAGDROP_S_CANCEL:!(keys&MK_LBUTTON)?DRAGDROP_S_DROP:S_OK;}
 HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD)override{return DRAGDROP_S_USEDEFAULTCURSORS;}
};
// Phase 5G: over, where a drag is (to light the zone under it); send, a drop that goes to a PC instead of the Shelf (true when it did).
class ShelfDropTarget final:public IDropTarget {ULONG refs_=1;bool acceptable_=false;HWND window_=nullptr;ComPtr<IDropTargetHelper> helper_;std::function<void(bool)> hover_;std::function<void(std::vector<ShelfItem>)> drop_;std::function<void(const std::vector<ShelfItem>&)> incoming_;
    std::function<void(POINT)> over_;std::function<bool(const std::vector<ShelfItem>&,POINT)> send_;public:
 void route(std::function<void(POINT)> over,std::function<bool(const std::vector<ShelfItem>&,POINT)> send){over_=std::move(over);send_=std::move(send);}
 ShelfDropTarget(std::function<void(bool)> hover,std::function<void(std::vector<ShelfItem>)> drop):hover_(std::move(hover)),drop_(std::move(drop)){} void attach(HWND window,std::function<void(const std::vector<ShelfItem>&)> incoming){window_=window;incoming_=std::move(incoming);CoCreateInstance(CLSID_DragDropHelper,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&helper_));}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IDropTarget){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data,DWORD,POINTL point,DWORD* effect)override{try{acceptable_=shelfAccepts(data);*effect=acceptable_?(*effect&DROPEFFECT_COPY):DROPEFFECT_NONE;hover_(acceptable_);if(acceptable_&&incoming_)incoming_(shelfRead(data));if(acceptable_&&over_)over_({point.x,point.y});if(helper_){POINT p{point.x,point.y};helper_->DragEnter(window_,data,&p,*effect);}return S_OK;}catch(...){*effect=DROPEFFECT_NONE;return E_FAIL;}}
 HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL point,DWORD* effect)override{*effect=acceptable_?(*effect&DROPEFFECT_COPY):DROPEFFECT_NONE;if(acceptable_&&over_)try{over_({point.x,point.y});}catch(...){}if(helper_){POINT p{point.x,point.y};helper_->DragOver(&p,*effect);}return S_OK;}
 HRESULT STDMETHODCALLTYPE DragLeave()override{if(helper_)helper_->DragLeave();acceptable_=false;try{hover_(false);}catch(...){}return S_OK;}
 HRESULT STDMETHODCALLTYPE Drop(IDataObject* data,DWORD,POINTL point,DWORD* effect)override{try{auto items=shelfRead(data);*effect=items.empty()?DROPEFFECT_NONE:(*effect&DROPEFFECT_COPY);if(helper_){POINT p{point.x,point.y};helper_->Drop(data,&p,*effect);}if(*effect&&!(send_&&send_(items,{point.x,point.y})))drop_(std::move(items));hover_(false);return S_OK;}catch(...){*effect=DROPEFFECT_NONE;return E_FAIL;}}
};
}
namespace nexus {
inline void shelfDragImage(IDataObject* data,const std::shared_ptr<const Artwork>& image){
    if(!image||!image->width||!image->height)return;ComPtr<IDragSourceHelper> helper;if(FAILED(CoCreateInstance(CLSID_DragDropHelper,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&helper))))return;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=96;info.bmiHeader.biHeight=-96;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap)return;
    memset(bits,0,96*96*4);auto* out=static_cast<BYTE*>(bits);double scale=std::min(96./image->width,96./image->height);unsigned w=std::max(1u,unsigned(image->width*scale)),h=std::max(1u,unsigned(image->height*scale));
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned sx=std::min(image->width-1,unsigned(x/scale)),sy=std::min(image->height-1,unsigned(y/scale));size_t source=(size_t(sy)*image->width+sx)*4,dest=(size_t(y+(96-h)/2)*96+x+(96-w)/2)*4;BYTE alpha=image->pixels[source+3];out[dest+3]=alpha;for(int c=0;c<3;++c)out[dest+c]=alpha?BYTE(std::min(255u,unsigned(image->pixels[source+c])*255/alpha)):0;}
    SHDRAGIMAGE drag{{96,96},{48,48},bitmap,CLR_NONE};helper->InitializeFromBitmap(&drag,data);DeleteObject(bitmap);
}
}
