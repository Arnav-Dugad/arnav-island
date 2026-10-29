#include "Setup/InstallPlan.h"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace nexus::setup;
static int count=0;
void require(bool pass,const char* name){++count;if(!pass)throw std::runtime_error(name);}
int main(){try{
    // The start-at-sign-in command, as the island writes it and as older or hand-made ones look.
    require(exeFromCommand(L"\"C:\\Users\\a\\Desktop\\Arnav Island\\app\\ArnavIsland.exe\" --startup")==L"C:\\Users\\a\\Desktop\\Arnav Island\\app\\ArnavIsland.exe","Quoted command");
    require(exeFromCommand(L"  C:\\Tools\\ArnavIsland.EXE --startup")==L"C:\\Tools\\ArnavIsland.EXE","Unquoted command");
    require(exeFromCommand(L"C:\\Tools\\ArnavIsland.exe")==L"C:\\Tools\\ArnavIsland.exe","Bare path");
    require(exeFromCommand(L"\"C:\\Other\\Something.exe\" --startup").empty(),"Another program isn't followed");
    require(exeFromCommand(L"\"C:\\Tools\\NotArnavIsland.exe\"").empty(),"Only the island's own name");
    require(exeFromCommand(L"").empty()&&exeFromCommand(L"\"C:\\unterminated").empty()&&exeFromCommand(L"ArnavIsland.exe").empty(),"Nothing to follow");
    // Where it installs.
    const std::wstring fallback=L"C:\\Users\\a\\AppData\\Local\\Programs\\Arnav Island",dev=L"C:\\Users\\a\\Desktop\\Arnav Island\\app";
    std::set<std::wstring> files;auto exists=[&](const std::wstring& p){return files.count(lower(p))>0;};
    require(chooseFolder(L"",L"",fallback,exists).folder==fallback&&chooseFolder(L"",L"",fallback,exists).found==Found::Fresh,"Fresh install goes to Programs");
    require(chooseFolder(L"\""+dev+L"\\ArnavIsland.exe\" --startup",L"",fallback,exists).folder==fallback,"A start command for a missing copy is ignored");
    files.insert(lower(dev+L"\\ArnavIsland.exe"));
    auto c=chooseFolder(L"\""+dev+L"\\ArnavIsland.exe\" --startup",L"",fallback,exists);require(c.folder==dev&&c.found==Found::Startup,"The developer's own copy stays where it is");
    files.insert(lower(fallback+L"\\ArnavIsland.exe"));
    require(chooseFolder(L"\""+dev+L"\\ArnavIsland.exe\" --startup",fallback,fallback,exists).folder==dev,"The copy that starts at sign-in wins");
    c=chooseFolder(L"",L"\""+fallback+L"\\\"",L"D:\\x",exists);require(c.folder==fallback&&c.found==Found::Installed,"A previous install, quoted and with a trailing slash");
    files.clear();require(chooseFolder(L"",fallback,L"D:\\x",exists).folder==L"D:\\x","A previous install that's gone isn't used");
    // What it writes, and what an uninstall clears up.
    require(startupCommand(dev)==L"\""+dev+L"\\ArnavIsland.exe\" --startup"&&exeFromCommand(startupCommand(dev))==dev+L"\\ArnavIsland.exe","Start command round trip");
    require(asideName(L"ArnavIsland.exe")==L"ArnavIsland.old.exe","The updater's name for a stepped-aside exe");
    require(asideName(L"ArnavIslandCamera.dll")==L"ArnavIslandCamera.old.dll"&&asideName(L"LICENSE")==L"LICENSE.old"&&asideName(L"QUICK_START.md")==L"QUICK_START.old.md","Stepped-aside names");
    require(sameFolder(L"C:\\Users\\A\\App\\",L"c:/users/a/app")&&!sameFolder(L"C:\\a",L"C:\\ab")&&!sameFolder(L"",L""),"Folders compare as Windows does");
    require(folderOf(L"C:\\a\\b.exe")==L"C:\\a"&&folderOf(L"b.exe").empty(),"Folder of a path");
    require(std::size(payloadNames)==5&&std::wstring(payloadNames[0])==exeName,"The exe comes first (resource 201)");
    std::cout<<"PASS "<<count<<" setup checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
