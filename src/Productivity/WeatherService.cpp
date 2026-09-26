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
        std::wstring town,query;std::optional<WeatherPlace> picked;{std::lock_guard lock(mutex_);town.swap(pending_);query.swap(search_);picked.swap(chosen_);}
        // A search from Settings: up to six matches, posted as they are (nothing else changes).
        if(!query.empty()){auto body=httpsGet(L"geocoding-api.open-meteo.com",geocodePath(query,8));auto found=body?parseGeocodeAll(*body,6):std::vector<WeatherPlace>{};
            {std::lock_guard lock(mutex_);matches_=std::move(found);searched_=query;searchFailed_=!body;}PostMessageW(window_,WeatherMessage,2,0);
            if(town.empty()&&!picked)continue;}
        // A place chosen from those matches is saved as it is, and its weather is fetched straight away.
        auto save=[&](const WeatherPlace& p){std::lock_guard lock(mutex_);place_=p;now_.reset();auto temp=file_;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary);writePlace(out,p);}MoveFileExW(temp.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);};
        if(picked){save(*picked);town.clear();}
        std::wstring problem;bool offline=false;
        if(!town.empty()){
            auto body=httpsGet(L"geocoding-api.open-meteo.com",geocodePath(town));auto found=body?parseGeocode(*body):std::nullopt;
            if(found)save(*found);
            else if(body)problem=L"No town called “"+town+L"” was found. Pick it in Island settings, under Town";
            // Unreachable: keep the town (unless another was typed meanwhile) and try again soon.
            else{problem=L"Open-Meteo could not be reached. The island will try again";offline=true;std::lock_guard lock(mutex_);if(pending_.empty())pending_=town;}}
        std::optional<WeatherPlace> place;{std::lock_guard lock(mutex_);place=place_;}
        if(place&&problem.empty()){auto body=httpsGet(L"api.open-meteo.com",forecastPath(*place));auto parsed=body?parseForecast(*body):std::nullopt;
            // The air quality comes from a second service; without it the weather still shows.
            if(parsed){if(auto air=httpsGet(L"air-quality-api.open-meteo.com",airQualityPath(*place)))parseAirQuality(*air,*parsed);std::lock_guard lock(mutex_);now_=parsed;}else{problem=L"The weather could not be fetched. The island will try again";offline=true;}}
        wait=offline?2*60*1000:30*60*1000;
        {std::lock_guard lock(mutex_);status_=problem;}
        if(place||!problem.empty())PostMessageW(window_,WeatherMessage,problem.empty()?0:1,0);
    }
}
}
