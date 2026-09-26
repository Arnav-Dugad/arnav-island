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
// 0.18: one HTTPS GET saved to a file (at most `limit` bytes; redirects followed, as GitHub's downloads need), with
// thirty-second timeouts. False, and no file, when anything goes wrong. Blocks; call it off the UI thread.
inline bool httpsGetToFile(const wchar_t* host,const std::wstring& path,const std::wstring& file,unsigned long long limit){
    struct Handle {HINTERNET h=nullptr;~Handle(){if(h)WinHttpCloseHandle(h);}};
    const wchar_t* agent=L"ArnavIsland (https://github.com/Arnav-Dugad/arnav-island)";
    Handle session{WinHttpOpen(agent,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.h)session.h=WinHttpOpen(agent,WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session.h)return false;
    WinHttpSetTimeouts(session.h,10000,10000,30000,30000);
    Handle connection{WinHttpConnect(session.h,host,INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.h)return false;
    Handle request{WinHttpOpenRequest(connection.h,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.h)return false;
    // Redirects only ever to HTTPS.
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
    if(!WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))return false;
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return false;
    HANDLE out=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(out==INVALID_HANDLE_VALUE)return false;
    bool ok=true;unsigned long long total=0;std::string buffer(64*1024,'\0');
    for(;;){DWORD read=0;if(!WinHttpReadData(request.h,buffer.data(),DWORD(buffer.size()),&read)){ok=false;break;}if(!read)break;total+=read;if(total>limit){ok=false;break;}
        DWORD written=0;if(!WriteFile(out,buffer.data(),read,&written,nullptr)||written!=read){ok=false;break;}}
    CloseHandle(out);if(!ok||!total){DeleteFileW(file.c_str());return false;}return true;
}
// One HTTPS GET, at most 4 MB, five-second timeouts. Blocks; call it off the UI thread.
// Some servers (Open-Meteo among them) send a compressed body WinHTTP cannot inflate: it answers 200 and
// then fails the read. Then the same request is made once more without asking for compression.
inline std::optional<std::string> httpsGet(const wchar_t* host,const std::wstring& path){
    bool unreadable=false;if(auto body=httpsGetOnce(host,path,true,&unreadable))return body;
    return unreadable?httpsGetOnce(host,path,false):std::nullopt;
}
}
