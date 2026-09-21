#pragma once
#include "Common/Win32.h"
#include "Persistence/Settings.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <functional>
#include <shlobj.h>
namespace nexus {
class LocalStore {
    std::mutex mutex_;std::condition_variable cv_;std::deque<std::function<void()>> work_;bool closing_=false;std::thread worker_;
public:
    std::filesystem::path directory;
    LocalStore() {
        PWSTR path=nullptr;check(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path));directory=std::filesystem::path(path)/L"ArnavIsland";CoTaskMemFree(path);
        std::filesystem::create_directories(directory);
        auto legacy=directory.parent_path()/L"NexusIsland"/L"settings.nexus";if(!std::filesystem::exists(directory/L"settings.nexus")&&std::filesystem::exists(legacy))try{std::filesystem::copy_file(legacy,directory/L"settings.nexus");}catch(...){/* Fall back to defaults; never overwrite legacy data. */}
        worker_=std::thread([this]{for(;;){std::function<void()> job;{std::unique_lock lock(mutex_);cv_.wait(lock,[&]{return closing_||!work_.empty();});if(closing_&&work_.empty())break;job=std::move(work_.front());work_.pop_front();}try{job();}catch(...){OutputDebugStringW(L"Arnav Island: local storage operation failed\n");}}});
    }
    ~LocalStore(){{std::lock_guard lock(mutex_);closing_=true;}cv_.notify_one();if(worker_.joinable())worker_.join();}
    void submit(std::function<void()> f) {{std::lock_guard lock(mutex_);if(work_.size()<256)work_.push_back(std::move(f));}cv_.notify_one();}
    Settings load(){std::ifstream f(directory/L"settings.nexus");if(!f)return {};try{return Settings::parse(f);}catch(...){log("Warning","settings_invalid_defaults_used");return {};}}
    void save(Settings s){submit([this,s]{auto temp=directory/L"settings.tmp";{std::ofstream f(temp);s.write(f);f.flush();if(!f)throw std::runtime_error("Settings write failed");}if(!MoveFileExW(temp.c_str(),(directory/L"settings.nexus").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Settings replace failed");});}
    void log(std::string level,std::string event){submit([this,level=std::move(level),event=std::move(event)]{auto p=directory/L"events.log";if(std::filesystem::exists(p)&&std::filesystem::file_size(p)>1024*1024){auto old=directory/L"events.previous.log";MoveFileExW(p.c_str(),old.c_str(),MOVEFILE_REPLACE_EXISTING);}std::ofstream f(p,std::ios::app);f<<"{\"level\":\""<<level<<"\",\"event\":\""<<event<<"\",\"uptime\":"<<GetTickCount64()<<"}\n";});}
    void clear(){submit([this]{std::ofstream(directory/L"events.log",std::ios::trunc);std::ofstream(directory/L"events.previous.log",std::ios::trunc);});}
};
}
