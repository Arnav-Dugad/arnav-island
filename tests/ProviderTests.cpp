#include "Audio/AudioProvider.h"
#include "Media/MediaProvider.h"
#include <iostream>
#include "Hardware/SystemProvider.h"
using namespace nexus;
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    HWND window=CreateWindowExW(0,L"STATIC",L"Nexus provider lifecycle test",0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandle(nullptr),nullptr);
    if(!window)return 1;
    double start=seconds();
    for(int i=0;i<5;++i){AudioProvider audio(window);MediaProvider media(window);SystemProvider system(window);system.setActive(true);
        double deadline=seconds()+.3;MSG msg{};
        while(seconds()<deadline){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);}
        std::cout<<"cycle "<<i<<": audio "<<(audio.available?"available":"unavailable")<<"; media title state "<<(media.snapshot().title==L"Media access unavailable"?"unavailable":"manager connected or awaiting initialization")<<'\n';
    }
    {SystemProvider system(window);system.setActive(true);Sleep(1200);auto stats=system.snapshot();if(stats.samples<2||stats.ramTotalGiB<=0||stats.cpu<0||stats.cpu>100)return 2;system.setActive(false);std::cout<<"System provider produced real bounded CPU/RAM snapshots\n";}
    DestroyWindow(window);CoUninitialize();double elapsed=seconds()-start;
    std::cout<<"Five real provider start/stop cycles completed in "<<elapsed<<" seconds; no audio changes requested\n";
    return elapsed<15?0:1;
}
