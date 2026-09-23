#include "FileShelf/ShelfPreviews.h"
#include <filesystem>
#include "Audio/AudioProvider.h"
#include "FileShelf/FileShelf.h"
#include "Audio/AudioOutputCompatibility.h"
#include "Media/MediaProvider.h"
#include <iostream>
#include "Hardware/SystemProvider.h"
using namespace nexus;
int main(int argc,char** argv){
    OleInitialize(nullptr);
    HWND window=CreateWindowExW(0,L"STATIC",L"Nexus provider lifecycle test",0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandle(nullptr),nullptr);
    if(!window)return 1;
        if(argc>1&&std::string(argv[1])=="--verify-current-output"){ComPtr<IMMDeviceEnumerator> e;ComPtr<IMMDevice> console,multimedia;check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&e)));check(e->GetDefaultAudioEndpoint(eRender,eConsole,&console));check(e->GetDefaultAudioEndpoint(eRender,eMultimedia,&multimedia));LPWSTR a=nullptr,b=nullptr;console->GetId(&a);multimedia->GetId(&b);bool same=a&&b&&wcscmp(a,b)==0;HRESULT result=same?setAudioOutput(b,true):S_FALSE;CoTaskMemFree(a);CoTaskMemFree(b);std::cout<<(same&&SUCCEEDED(result)?"PASS reselect existing default output without changing route":"SKIP or FAIL current-output adapter check")<<" HRESULT "<<std::hex<<result<<'\n';return same&&FAILED(result)?6:0;}
    double start=seconds();
    for(int i=0;i<5;++i){AudioProvider audio(window);MediaProvider media(window);SystemProvider system(window);system.setActive(true);
        double deadline=seconds()+.3;MSG msg{};
        while(seconds()<deadline){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);}
        std::cout<<"cycle "<<i<<": audio "<<(audio.available?"available":"unavailable")<<"; media title state "<<(media.snapshot().title==L"Media access unavailable"?"unavailable":"manager connected or awaiting initialization")<<'\n';
    }
    {SystemProvider system(window);system.setActive(true);Sleep(1200);auto stats=system.snapshot();if(stats.samples<2||stats.ramTotalGiB<=0||stats.cpu<0||stats.cpu>100)return 2;system.setActive(false);std::cout<<"System provider produced real bounded CPU/RAM snapshots\n";}
        {for(auto item:{ShelfItem{ShelfItem::Kind::Text,L"A local note\nsecond line",L"note"},ShelfItem{ShelfItem::Kind::File,L"C:\\QA\\reference only.txt",L"reference only.txt"}}){auto object=shelfData(item);if(!shelfAccepts(object.Get()))return 3;auto result=shelfRead(object.Get());if(result.size()!=1||result[0].kind!=item.kind||result[0].value!=item.value)return 4;}std::cout<<"OLE file and text copy-only roundtrip passed; no file read/move/delete\n";if(setAudioOutput(L"unused",false)!=E_ACCESSDENIED)return 5;}
    {ShelfPreviews previews(window);auto path=(std::filesystem::current_path().parent_path()/L"docs/evidence/v0.4/artwork.png").wstring();previews.request({path});double end=seconds()+4;while(!previews.get(path)&&seconds()<end)Sleep(20);auto image=previews.get(path);if(!image||image->pixels.empty()||image->width>256||image->height>256){ComPtr<IShellItemImageFactory> f;auto hr=SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(&f));HBITMAP b=nullptr;auto result=f?f->GetImage({128,128},SIIGBF_RESIZETOFIT,&b):hr;std::cerr<<"Thumbnail failed: exists="<<std::filesystem::exists(path)<<" item="<<std::hex<<hr<<" image="<<result<<" bitmap="<<bool(b)<<std::dec<<"\n";if(b)DeleteObject(b);return 7;}auto data=shelfData({ShelfItem::Kind::File,path,L"QA artwork"});for(int n=0;n<4;++n){auto warm=shelfData({ShelfItem::Kind::File,path,L"QA artwork"});shelfDragImage(warm.Get(),image);}DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);for(int n=0;n<40;++n){auto object=shelfData({ShelfItem::Kind::File,path,L"QA artwork"});shelfDragImage(object.Get(),image);if(shelfRead(object.Get()).size()!=1)return 8;}DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);std::cout<<"Drag-image GDI handles: "<<before<<" -> "<<after<<"\n";if(after>before+4)return 9;previews.request({});std::cout<<"Shell thumbnail loaded; 40 drag-image/data roundtrips, bounded GDI handles\n";}
    DestroyWindow(window);OleUninitialize();double elapsed=seconds()-start;
    std::cout<<"Five real provider start/stop cycles completed in "<<elapsed<<" seconds; no audio changes requested\n";
    return elapsed<15?0:1;
}
