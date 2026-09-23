#include "Media/BrowserTabs.h"
#include "Media/AppIdentity.h"
#include <uiautomation.h>
#include <deque>
#include <set>
namespace nexus {
namespace {
constexpr CLSID automationClass{0xff48dba4,0x60ef,0x4201,{0xaa,0x87,0x54,0x10,0x3e,0xef,0x59,0x4e}};// CUIAutomation
struct BrowserWindows {std::set<DWORD> processes;std::vector<HWND> windows;};
std::wstring text(BSTR value){std::wstring s=value?std::wstring(value,SysStringLen(value)):std::wstring{};SysFreeString(value);return s;}
}
std::vector<std::wstring> browserTabTitles(){
    BrowserWindows found;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot!=INVALID_HANDLE_VALUE){PROCESSENTRY32W entry{sizeof(entry)};for(BOOL ok=Process32FirstW(snapshot,&entry);ok;ok=Process32NextW(snapshot,&entry))if(isBrowserName(entry.szExeFile))found.processes.insert(entry.th32ProcessID);CloseHandle(snapshot);}
    if(found.processes.empty())return {};
    EnumWindows([](HWND h,LPARAM p)->BOOL{auto& f=*reinterpret_cast<BrowserWindows*>(p);if(!IsWindowVisible(h)||GetWindow(h,GW_OWNER))return TRUE;DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(f.processes.contains(pid)&&f.windows.size()<16)f.windows.push_back(h);return TRUE;},reinterpret_cast<LPARAM>(&found));
    std::vector<std::wstring> titles;
    ComPtr<IUIAutomation> automation;ComPtr<IUIAutomationCacheRequest> cache;ComPtr<IUIAutomationCondition> all;
    if(SUCCEEDED(CoCreateInstance(automationClass,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))){
        // A hung browser must not stall media updates for UI Automation's 20 s default.
        ComPtr<IUIAutomation2> timed;if(SUCCEEDED(automation.As(&timed))){timed->put_ConnectionTimeout(1000);timed->put_TransactionTimeout(1000);}
        if(SUCCEEDED(automation->CreateCacheRequest(&cache))){cache->AddProperty(UIA_ControlTypePropertyId);cache->AddProperty(UIA_NamePropertyId);cache->AddProperty(UIA_ClassNamePropertyId);}
        automation->CreateTrueCondition(&all);
    }
    for(HWND window:found.windows){
        size_t before=titles.size();
        if(automation&&cache&&all){
            ComPtr<IUIAutomationElement> root;
            if(SUCCEEDED(automation->ElementFromHandleBuildCache(window,cache.Get(),&root))&&root){
                // Breadth-first over the browser's own interface, one cached call per node.
                std::deque<ComPtr<IUIAutomationElement>> queue{root};int visited=0;
                while(!queue.empty()&&visited++<400&&titles.size()<256){
                    auto element=std::move(queue.front());queue.pop_front();ComPtr<IUIAutomationElementArray> children;
                    if(FAILED(element->FindAllBuildCache(TreeScope_Children,all.Get(),cache.Get(),&children))||!children)continue;
                    int count=0;children->get_Length(&count);
                    for(int i=0;i<count;++i){ComPtr<IUIAutomationElement> child;if(FAILED(children->GetElement(i,&child))||!child)continue;
                        CONTROLTYPEID type=0;child->get_CachedControlType(&type);BSTR rawClass=nullptr;child->get_CachedClassName(&rawClass);auto className=text(rawClass);
                        if(type==UIA_TabItemControlTypeId){BSTR name=nullptr;child->get_CachedName(&name);auto title=text(name);if(!title.empty())titles.push_back(std::move(title));}
                        else if(type!=UIA_DocumentControlTypeId&&className!=L"Chrome_RenderWidgetHostHWND")queue.push_back(std::move(child));}
                }
            }
        }
        if(titles.size()==before){wchar_t caption[512];int n=GetWindowTextW(window,caption,512);if(n>0)titles.emplace_back(caption,n);}
    }
    return titles;
}
}
