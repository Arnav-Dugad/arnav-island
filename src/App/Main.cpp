#include "Island/IslandWindow.h"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HANDLE single=CreateMutexW(nullptr,TRUE,L"Local\\ArnavIsland.App");
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        std::wstring args=command?command:L"";HWND existing=FindWindowW(L"ArnavIsland.Surface",nullptr);
        if(existing&&args.find(L"--lab")!=std::wstring::npos)PostMessageW(existing,WM_APP+50,1,0);
        else if(existing&&args.find(L"--enable-startup")!=std::wstring::npos)PostMessageW(existing,WM_APP+50,3,0);
        else if(existing&&args.find(L"--expanded")!=std::wstring::npos)PostMessageW(existing,WM_APP+50,2,0);
        if(single)CloseHandle(single);return 0;
    }
    HRESULT hr=OleInitialize(nullptr);
    int result=1;
    try{nexus::IslandWindow app;result=app.run(instance,command?command:L"");}
    catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Arnav Island could not start",MB_ICONERROR);}
    if(SUCCEEDED(hr))OleUninitialize();if(single)CloseHandle(single);return result;
}
