#include "Island/IslandWindow.h"
// A crash leaves crash.txt next to the logs: the exception, and each stack frame as a module
// and offset (no memory contents, file names or user data), so it can be mapped to source.
static LONG WINAPI crashReport(EXCEPTION_POINTERS* info){
    wchar_t base[MAX_PATH]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH))return EXCEPTION_CONTINUE_SEARCH;
    std::wstring path=std::wstring(base)+L"\\ArnavIsland\\crash.txt";FILE* f=_wfopen(path.c_str(),L"w");if(!f)return EXCEPTION_CONTINUE_SEARCH;
    auto where=[&](void* address){HMODULE module=nullptr;wchar_t name[MAX_PATH]{};
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,static_cast<LPCWSTR>(address),&module)&&module){GetModuleFileNameW(module,name,MAX_PATH);const wchar_t* leaf=wcsrchr(name,L'\\');
            fwprintf(f,L"  %ls+0x%llx\n",leaf?leaf+1:name,(unsigned long long)(static_cast<char*>(address)-reinterpret_cast<char*>(module)));}
        else fwprintf(f,L"  0x%p\n",address);};
    fwprintf(f,L"exception 0x%08lx at\n",info->ExceptionRecord->ExceptionCode);where(info->ExceptionRecord->ExceptionAddress);
    fwprintf(f,L"return address on the stack\n");where(*reinterpret_cast<void**>(info->ContextRecord->Rsp));
    // Words on the faulting thread's stack that point into this program's code (likely return addresses).
    fwprintf(f,L"stack\n");{HMODULE self=GetModuleHandleW(nullptr);auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(self);auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<char*>(self)+dos->e_lfanew);
        const auto lo=reinterpret_cast<uintptr_t>(self),hi=lo+nt->OptionalHeader.SizeOfImage;auto* sp=reinterpret_cast<uintptr_t*>(info->ContextRecord->Rsp);int found=0;
        MEMORY_BASIC_INFORMATION mbi{};if(VirtualQuery(sp,&mbi,sizeof(mbi))){auto* end=reinterpret_cast<uintptr_t*>(static_cast<char*>(mbi.BaseAddress)+mbi.RegionSize);for(auto* p=sp;p<end&&p<sp+4096&&found<40;++p)if(*p>lo&&*p<hi){where(reinterpret_cast<void*>(*p));++found;}}}
    fclose(f);return EXCEPTION_CONTINUE_SEARCH;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int){
    SetUnhandledExceptionFilter(crashReport);
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
    catch(const std::exception& e){
        // Keep the reason next to the logs, so a failed start can be diagnosed without the dialog.
        wchar_t base[MAX_PATH]{};if(GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH)){std::wstring path=std::wstring(base)+L"\\ArnavIsland\\last-error.txt";if(FILE* f=_wfopen(path.c_str(),L"w")){fputs(e.what(),f);fclose(f);}}
        MessageBoxA(nullptr,e.what(),"Arnav Island could not start",MB_ICONERROR);}
    if(SUCCEEDED(hr))OleUninitialize();if(single)CloseHandle(single);return result;
}
