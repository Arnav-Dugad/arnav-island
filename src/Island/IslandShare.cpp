#include "IslandWindow.h"
#include <shlobj.h>
#include <filesystem>
// Phase 5F: sharing between your own PCs. The service (Productivity/ShareService) does the
// network and the cryptography on its own threads; here its events become cards (the pairing
// code, an offer to accept, how a transfer went), the Shelf's Nearby tab and its Send button.
namespace nexus {
namespace {
std::wstring bytesText(uint64_t b){wchar_t t[32];if(b<1024)swprintf(t,32,L"%llu bytes",static_cast<unsigned long long>(b));else if(b<1024*1024)swprintf(t,32,L"%.0f KB",b/1024.);else if(b<(1ull<<30))swprintf(t,32,L"%.1f MB",b/1048576.);else swprintf(t,32,L"%.2f GB",b/1073741824.);return t;}
std::wstring folderOf(const KNOWNFOLDERID& id){PWSTR p=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(id,KF_FLAG_CREATE,nullptr,&p))&&p)out=p;CoTaskMemFree(p);return out;}
}
// Sharing runs only while its setting is on (never in test runs: no sockets, no firewall prompts there).
void IslandWindow::syncSharing(){
    if(!window_)return;
    if(settings_.sharing&&!share_&&!testing_){
        ShareOptions o;std::filesystem::path data=std::filesystem::path(settingsFile_).parent_path();if(data.empty())data=std::filesystem::path(folderOf(FOLDERID_LocalAppData))/L"ArnavIsland";
        o.folder=data.wstring();o.downloads=folderOf(FOLDERID_Downloads);if(o.downloads.empty())o.downloads=(data/L"Received").wstring();
        share_=std::make_unique<ShareService>(window_,o);store_.log(share_->running()?"Info":"Warning",share_->running()?"share_started":"share_unavailable");
        content_.nearby=share_->peers();}
    else if(!settings_.sharing&&share_){share_.reset();content_.nearby.clear();shareTarget_.clear();content_.nearbyTarget.clear();if(content_.shelfTab==2)content_.shelfTab=0;}
    if(content_.shareName.empty()){wchar_t n[256]{};DWORD size=256;if(GetComputerNameExW(ComputerNamePhysicalDnsHostname,n,&size))content_.shareName=n;}
}
// A sharing card: 14 the pairing code, 15 an offer, 16 how something went (path: a received file, shown by its button).
void IslandWindow::shareCard(int kind,const std::wstring& title,const std::wstring& detail,const std::wstring& path,double duration){
    if(!renderer_)return;
    content_.notice={};content_.notice.kind=kind;content_.notice.app=title;content_.notice.detail=detail;content_.notice.path=path;
    // From an open Shelf the card takes over, then the island settles to compact.
    content_.pinned=false;events_.publish({ActivityKind::Notification,"share",75,double(kind),2.4,duration},seconds());
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","share_card_shown");
}
void IslandWindow::shareEvents(){
    if(!share_)return;
    for(auto& e:share_->take()){
        using K=ShareEvent::Kind;
        switch(e.kind){
        case K::Peers:{content_.nearby=share_->peers();
            // Sends go to the chosen paired PC; failing that, the first paired PC that is here.
            const bool keep=std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==shareTarget_&&p.paired;});
            if(!keep){shareTarget_.clear();for(auto& p:content_.nearby)if(p.paired&&p.online){shareTarget_=p.id;break;}}content_.nearbyTarget=shareTarget_;
            if(state_==IslandState::Expanded&&content_.page==Page::Shelf)refresh();break;}
        case K::PairCode:{wchar_t code[16];swprintf(code,16,L"%03u %03u",e.code/1000,e.code%1000);shareCard(14,e.name,code,{},60);break;}
        case K::Paired:shareCard(16,L"Paired with "+e.name,e.detail,{},4);break;
        case K::PairFailed:shareCard(16,L"Not paired with "+(e.name.empty()?std::wstring(L"that PC"):e.name),e.detail,{},4.5);break;
        case K::Offer:shareOffer_=e.transfer;shareCard(15,e.name,e.file+L"  \u00b7  "+bytesText(e.size),{},60);break;
        case K::Progress:content_.shelfStatus=e.file+L"  \u00b7  "+std::to_wstring(e.size?int(e.done*100/e.size):100)+L"%";content_.shelfStatusUntil=seconds()+3;
            if(state_==IslandState::Expanded&&content_.page==Page::Shelf)refresh();break;
        // Show opens Downloads with the file selected.
        case K::Received:shareCard(16,L"Received from "+e.name,e.file,e.detail,8);break;
        case K::Sent:content_.shelfStatus.clear();shareCard(16,L"Sent to "+e.name,e.file+L"  \u00b7  "+e.detail,{},4);break;
        case K::Failed:content_.shelfStatus.clear();shareCard(16,e.file.empty()?std::wstring(L"Couldn't share"):L"Couldn't share "+e.file,e.detail,{},5);break;}
    }
}
bool IslandWindow::shareAction(Action a){
    const double now=seconds();
    auto close=[&]{events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);};
    auto note=[&](const std::wstring& text){content_.shelfStatus=text;content_.shelfStatusUntil=now+4;refresh();};
    switch(a){
    case Action::SharePair:if(share_)share_->confirmPair(true);close();return true;
    case Action::ShareDecline:if(share_){if(content_.notice.kind==14)share_->confirmPair(false);else if(content_.notice.kind==15)share_->answer(shareOffer_,false);}close();return true;
    case Action::ShareAccept:if(share_)share_->answer(shareOffer_,true);close();return true;
    case Action::ShareShow:{const std::wstring path=content_.notice.path;close();if(!path.empty())if(auto* pidl=ILCreateFromPathW(path.c_str())){SHOpenFolderAndSelectItems(pidl,0,nullptr,0);ILFree(pidl);}return true;}
    case Action::ShelfNearby:{content_.shelfDetail=-1;if(content_.shelfTab!=2&&!motion_.reduced){motion_.swipe.reset(14.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.shelfTab=2;if(share_)content_.nearby=share_->peers();refresh();animate();return true;}
    case Action::ShareSend:{const int index=content_.shelfDetail;if(index<0||size_t(index)>=content_.shelf.size())return true;const auto& item=content_.shelf[size_t(index)];
        auto it=std::find_if(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==shareTarget_&&p.paired&&p.online;});
        if(!share_||it==content_.nearby.end()){note(L"Pair a PC in Shelf \u203a Nearby first");return true;}
        share_->send(it->id,item.value);note(L"Sending to "+it->name+L"\u2026");store_.log("Info","share_send");return true;}
    default:break;}
    if(inRange(a,Action::NearbyBase,Action::NearbyEnd)){const size_t i=size_t(int(a)-int(Action::NearbyBase));
        if(i<content_.nearby.size()){const auto p=content_.nearby[i];
            if(!p.paired){if(share_&&p.online){share_->pair(p.id);note(L"Pairing with "+p.name+L"\u2026");}}
            else{shareTarget_=p.id;content_.nearbyTarget=p.id;refresh();}}
        return true;}
    if(inRange(a,Action::NearbyForgetBase,Action::NearbyForgetEnd)){const size_t i=size_t(int(a)-int(Action::NearbyForgetBase));
        if(share_&&i<content_.nearby.size()&&content_.nearby[i].paired){share_->forget(content_.nearby[i].id);note(L"Forgot "+content_.nearby[i].name);}return true;}
    return false;
}
}
