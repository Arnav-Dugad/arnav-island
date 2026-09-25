#pragma once
#include "Common/Win32.h"
#include <winhttp.h>
#include <optional>
#include <string>
namespace nexus {
// One HTTPS GET attempt. With decompress, WinHTTP asks for gzip/deflate and inflates the answer itself.
inline std::optional<std::string> httpsGetOnce(const wchar_t* host,const std::wstring& path,bool decompress,bool* unreadable=nullptr){
    struct Handle {HINTERNET h=nullptr;~Handle(){if(h)WinHttpCloseHandle(h);}};
    const wchar_t* agent=L"ArnavIsland (https://github.com/Arnav-Dugad/arnav-island)";
    Handle session{WinHttpOpen(agent,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h)session.h=WinHttpOpen(agent,WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session.h)return std::nullopt;
    WinHttpSetTimeouts(session.h,5000,5000,5000,5000);
    if(decompress){DWORD flags=WINHTTP_DECOMPRESSION_FLAG_ALL;WinHttpSetOption(session.h,WINHTTP_OPTION_DECOMPRESSION,&flags,sizeof(flags));}
    Handle connection{WinHttpConnect(session.h,host,INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.h)return std::nullopt;
    Handle request{WinHttpOpenRequest(connection.h,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.h)return std::nullopt;
    if(!WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))return std::nullopt;
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return std::nullopt;
    // A 200 whose body cannot be read is reported through unreadable.
    auto failed=[&]{if(unreadable)*unreadable=true;return std::nullopt;};
    std::string body;for(;;){DWORD available=0;if(!WinHttpQueryDataAvailable(request.h,&available))return failed();if(!available)break;
        if(body.size()+available>(4u<<20))return std::nullopt;size_t at=body.size();body.resize(at+available);DWORD read=0;if(!WinHttpReadData(request.h,body.data()+at,available,&read))return failed();body.resize(at+read);if(!read)break;}
    return body;
}
// One HTTPS GET, at most 4 MB, five-second timeouts. Blocks; call it off the UI thread.
// Some servers (Open-Meteo among them) send a compressed body WinHTTP cannot inflate: it answers 200 and
// then fails the read. Then the same request is made once more without asking for compression.
inline std::optional<std::string> httpsGet(const wchar_t* host,const std::wstring& path){
    bool unreadable=false;if(auto body=httpsGetOnce(host,path,true,&unreadable))return body;
    return unreadable?httpsGetOnce(host,path,false):std::nullopt;
}
}
