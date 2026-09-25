#include "WeatherService.h"
#include "Common/Http.h"
#include <fstream>
namespace nexus {
WeatherService::WeatherService(HWND window,std::filesystem::path file):window_(window),file_(std::move(file)),stop_(CreateEventW(nullptr,TRUE,FALSE,nullptr)),wake_(CreateEventW(nullptr,FALSE,TRUE,nullptr)){
    if(!stop_||!wake_)throw std::runtime_error("Weather event creation failed");
    {std::ifstream in(file_);if(in)place_=readPlace(in);}
    worker_=std::thread([this]{run();});
}
WeatherService::~WeatherService(){SetEvent(stop_);if(worker_.joinable())worker_.join();CloseHandle(stop_);CloseHandle(wake_);}
void WeatherService::run(){
    HANDLE waits[]={stop_,wake_};
    // Woken by a request (or at start), otherwise every 30 minutes; after a failed attempt, again in 2 minutes.
    DWORD wait=30*60*1000;
    for(;;){const DWORD r=WaitForMultipleObjects(2,waits,FALSE,wait);if(r==WAIT_OBJECT_0||r==WAIT_FAILED)break;
        std::wstring town;{std::lock_guard lock(mutex_);town.swap(pending_);}
        std::wstring problem;bool offline=false;
        if(!town.empty()){
            auto body=httpsGet(L"geocoding-api.open-meteo.com",geocodePath(town));auto found=body?parseGeocode(*body):std::nullopt;
            if(found){std::lock_guard lock(mutex_);place_=found;now_.reset();auto temp=file_;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary);writePlace(out,*found);}MoveFileExW(temp.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
            else if(body)problem=L"No town called “"+town+L"” was found";
            // Unreachable: keep the town (unless another was typed meanwhile) and try again soon.
            else{problem=L"Open-Meteo could not be reached. The island will try again";offline=true;std::lock_guard lock(mutex_);if(pending_.empty())pending_=town;}}
        std::optional<WeatherPlace> place;{std::lock_guard lock(mutex_);place=place_;}
        if(place&&problem.empty()){auto body=httpsGet(L"api.open-meteo.com",forecastPath(*place));auto parsed=body?parseForecast(*body):std::nullopt;
            if(parsed){std::lock_guard lock(mutex_);now_=parsed;}else{problem=L"The weather could not be fetched. The island will try again";offline=true;}}
        wait=offline?2*60*1000:30*60*1000;
        {std::lock_guard lock(mutex_);status_=problem;}
        if(place||!problem.empty())PostMessageW(window_,WeatherMessage,problem.empty()?0:1,0);
    }
}
}
