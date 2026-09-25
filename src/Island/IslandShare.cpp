#include "IslandWindow.h"
#include "Media/CoverCodec.h"
#include <shlobj.h>
#include <filesystem>
// Phase 5F: sharing between your own PCs. The service (Productivity/ShareService) does the
// network and the cryptography on its own threads; here its events become cards (the pairing
// code, an offer to accept, how a transfer went), the Shelf's Nearby tab and its Send button.
// Phase 5G: files and folders dropped straight onto a PC while dragging over the island, the whole
// Shelf sent at once (a row's Send Shelf, or its stack dragged onto a PC), transfers shown as they
// go (a Nearby row's progress and Stop, the compact island's chip), and music offers (IslandMusic.cpp).
namespace nexus {
namespace {
std::wstring bytesText(uint64_t b){wchar_t t[32];if(b<1024)swprintf(t,32,L"%llu bytes",static_cast<unsigned long long>(b));else if(b<1024*1024)swprintf(t,32,L"%.0f KB",b/1024.);else if(b<(1ull<<30))swprintf(t,32,L"%.1f MB",b/1048576.);else swprintf(t,32,L"%.2f GB",b/1073741824.);return t;}
std::wstring folderOf(const KNOWNFOLDERID& id){PWSTR p=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(id,KF_FLAG_CREATE,nullptr,&p))&&p)out=p;CoTaskMemFree(p);return out;}
std::wstring filesText(uint32_t count){return count==1?L"1 file":std::to_wstring(count)+L" files";}
}
// Sharing runs only while its setting is on (never in test runs: no sockets, no firewall prompts there).
void IslandWindow::syncSharing(){
    if(!window_)return;
    if(settings_.sharing&&!share_&&!testing_){
        ShareOptions o;std::filesystem::path data=std::filesystem::path(settingsFile_).parent_path();if(data.empty())data=std::filesystem::path(folderOf(FOLDERID_LocalAppData))/L"ArnavIsland";
        o.folder=data.wstring();o.downloads=folderOf(FOLDERID_Downloads);if(o.downloads.empty())o.downloads=(data/L"Received").wstring();o.handoff=(data/L"Handoff").wstring();
        share_=std::make_unique<ShareService>(window_,o);store_.log(share_->running()?"Info":"Warning",share_->running()?"share_started":"share_unavailable");
        content_.nearby=share_->peers();}
    else if(!settings_.sharing&&share_){share_.reset();content_.nearby.clear();content_.transfers.clear();shareTarget_.clear();content_.nearbyTarget.clear();content_.handoffPicking=false;content_.remote={};if(content_.shelfTab==2)content_.shelfTab=0;}
    publishShelf();
    if(content_.shareName.empty()){wchar_t n[256]{};DWORD size=256;if(GetComputerNameExW(ComputerNamePhysicalDnsHostname,n,&size))content_.shareName=n;}
}
// Phase 5H: what paired PCs see of this Shelf: its files and folders (not its text), each with a small preview.
// Previews are made once per picture and kept while it is on the Shelf.
void IslandWindow::publishShelf(){
    if(!share_)return;std::vector<ShareShelfEntry> items;std::map<std::wstring,std::pair<const Artwork*,std::vector<uint8_t>>> kept;
    for(auto& item:content_.shelf){if(item.kind!=ShelfItem::Kind::File)continue;ShareShelfEntry e;e.path=item.value;
        if(item.preview){auto it=shelfPreviewBytes_.find(item.value);if(it!=shelfPreviewBytes_.end()&&it->second.first==item.preview.get())e.preview=it->second.second;else e.preview=encodeCover(*item.preview,72,6*1024,.8f);kept[item.value]={item.preview.get(),e.preview};}
        items.push_back(std::move(e));}
    shelfPreviewBytes_=std::move(kept);share_->offerShelf(std::move(items),settings_.shelfOpen);
}
// A sharing card: 14 the pairing code, 15 an offer, 16 how something went (path: a received file, shown by its button),
// 17 music from another PC.
void IslandWindow::shareCard(int kind,const std::wstring& title,const std::wstring& detail,const std::wstring& path,double duration){
    if(!renderer_)return;
    content_.notice={};content_.notice.kind=kind;content_.notice.app=title;content_.notice.detail=detail;content_.notice.path=path;
    // From an open Shelf the card takes over, then the island settles to compact.
    content_.pinned=false;{const Activity a{ActivityKind::Notification,"share",75,double(kind),2.4,duration};if(holdCard(a))return;events_.publish(a,seconds());}
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","share_card_shown");
}
// Files and folders to a paired PC: shown on its Nearby row (and the compact island) until they arrive.
void IslandWindow::sendToPeer(const std::string& peer,std::vector<std::wstring> paths){
    auto it=std::find_if(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==peer;});
    if(!share_||it==content_.nearby.end()||paths.empty())return;
    const std::wstring name=it->name;const uint32_t id=share_->send(peer,paths);if(!id)return;
    std::vector<std::wstring> names;for(auto& p:paths)names.push_back(std::filesystem::path(p).filename().wstring());
    ContentSnapshot::Transfer t;t.id=id;t.peer=peer;t.name=name;t.title=shareTitle(names);t.outgoing=true;content_.transfers.push_back(t);
    content_.shelfStatus=L"Sending to "+name+L"…";content_.shelfStatusUntil=seconds()+3;store_.log("Info","share_send");refresh();
}
void IslandWindow::shareEvents(){
    if(!share_)return;
    bool redraw=false;const double now=seconds();
    auto drop=[&](uint32_t id){std::erase_if(content_.transfers,[&](auto& t){return t.id==id;});};
    for(auto& e:share_->take()){
        using K=ShareEvent::Kind;
        switch(e.kind){
        case K::Peers:{content_.nearby=share_->peers();
            // Sends go to the chosen paired PC; failing that, the first paired PC that is here.
            const bool keep=std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==shareTarget_&&p.paired;});
            if(!keep){shareTarget_.clear();for(auto& p:content_.nearby)if(p.paired&&p.online){shareTarget_=p.id;break;}}content_.nearbyTarget=shareTarget_;
            if(state_==IslandState::Expanded&&(content_.page==Page::Shelf||content_.page==Page::Media))redraw=true;break;}
        case K::PairCode:{wchar_t code[16];swprintf(code,16,L"%03u %03u",e.code/1000,e.code%1000);shareCard(14,e.name,code,{},60);break;}
        case K::Paired:shareCard(16,L"Paired with "+e.name,e.detail,{},4);break;
        case K::PairFailed:shareCard(16,L"Not paired with "+(e.name.empty()?std::wstring(L"that PC"):e.name),e.detail,{},4.5);break;
        case K::Offer:shareOffer_=e.transfer;shareCard(15,e.name,e.file+(e.count>1?L"  ·  "+filesText(e.count):L"")+L"  ·  "+bytesText(e.size),{},60);break;
        // Progress: the transfer's row and chip fill (redrawn at most about five times a second, and at the end).
        case K::Progress:{auto it=std::find_if(content_.transfers.begin(),content_.transfers.end(),[&](auto& t){return t.id==e.transfer;});
            if(it==content_.transfers.end()){ContentSnapshot::Transfer t;t.id=e.transfer;t.peer=e.peer;t.name=e.name;t.title=e.file;t.outgoing=e.outgoing;content_.transfers.push_back(t);it=content_.transfers.end()-1;}
            it->done=e.done;it->total=e.size;it->count=e.count;if(!e.file.empty())it->title=e.file;
            // Its speed: bytes over the last quarter second or more, eased (so the ring and the time left don't jitter).
            if(it->rateAt<=0){it->rateAt=now;it->rateDone=e.done;}else if(now-it->rateAt>=.25&&e.done>=it->rateDone){const double speed=double(e.done-it->rateDone)/(now-it->rateAt);it->rate=it->rate>0?it->rate*.7+speed*.3:speed;it->rateAt=now;it->rateDone=e.done;}
            if(now-transferDrawn_>=.2||e.done==e.size){transferDrawn_=now;redraw=true;}break;}
        // Show opens Downloads with the arrival selected (a folder, or the first file).
        case K::Received:drop(e.transfer);
            if(e.code==1){if(!e.detail.empty()&&std::none_of(content_.shelf.begin(),content_.shelf.end(),[&](auto& i){return i.value==e.detail;})&&content_.shelf.size()<32){content_.shelf.push_back({ShelfItem::Kind::File,e.detail,std::filesystem::path(e.detail).filename().wstring()});requestPreviews();}
                shareCard(16,L"Taken from "+e.name+L"\u2019s Shelf",e.file+(e.count>1?L"  ·  "+filesText(e.count):L"")+L"  ·  now on your Shelf",e.detail,6);store_.log("Info","share_shelf_taken_here");break;}
            shareCard(16,L"Received from "+e.name,e.file+(e.count>1?L"  ·  "+filesText(e.count):L""),e.detail,8);break;
        case K::Sent:drop(e.transfer);content_.shelfStatus.clear();shareCard(16,L"Sent to "+e.name,e.file+(e.count>1?L"  ·  "+filesText(e.count):L"")+L"  ·  "+e.detail,{},4);break;
        case K::Failed:{drop(e.transfer);content_.shelfStatus.clear();
            // Stopped here: a quiet note. An offer the other PC took back replaces its card.
            if(e.detail==L"You stopped it"){content_.shelfStatus=L"Stopped "+e.file;content_.shelfStatusUntil=now+3;redraw=true;break;}
            if(e.transfer&&e.transfer==handoffOffer_){handoffOffer_=0;if(state_==IslandState::Notification&&content_.notice.kind==17)shareCard(16,e.detail,e.file,{},3.5);break;}
            shareCard(16,e.file.empty()?std::wstring(L"Couldn't share"):L"Couldn't share "+e.file,e.detail,{},5);break;}
        case K::Handoff:case K::HandoffAnswered:case K::HandoffFile:handoffEvent(e);break;
        // Phase 5H: another PC's Shelf, as asked for; and this Shelf, taken from.
        case K::ShelfList:{auto& r=content_.remote;if(!r.open||r.peer!=e.peer)break;r.state=e.code==0?1:e.code==1?2:3;r.status=e.detail;if(!e.name.empty())r.name=e.name;r.items=e.shelf;r.offset=std::clamp(r.offset,0,std::max(0,int(r.items.size())-4));
            r.previews.clear();for(auto& item:r.items)r.previews.push_back(decodeCover(item.preview,64));redraw=true;break;}
        case K::ShelfTaken:drop(e.transfer);shareCard(16,e.name+L" took "+e.file,L"A copy, from your Shelf",{},3.5);store_.log("Info","share_shelf_taken_there");break;}
    }
    if(redraw&&renderer_)refresh();
}
// The zone a drag over the island is on (the Shelf, or a paired PC's), from a point on the screen.
Action IslandWindow::dropZoneAt(POINT screen){
    POINT p=screen;ScreenToClient(window_,&p);const Action a=hit(MAKELPARAM(p.x,p.y));
    return a==Action::DropShelf||inRange(a,Action::NearbyBase,Action::NearbyEnd)?a:Action::None;
}
// Files let go over a PC's zone go to it; anything else lands on the Shelf.
bool IslandWindow::dropOnPeer(const std::vector<ShelfItem>& items,POINT screen){
    const Action zone=dropZoneAt(screen);content_.dropZone=Action::None;
    if(!share_||!inRange(zone,Action::NearbyBase,Action::NearbyEnd))return false;
    const size_t i=size_t(int(zone)-int(Action::NearbyBase));if(i>=content_.nearby.size()||!content_.nearby[i].paired)return false;
    std::vector<std::wstring> paths;for(auto& item:items)if(item.kind==ShelfItem::Kind::File)paths.push_back(item.value);if(paths.empty())return false;
    const auto peer=content_.nearby[i].id;shareTarget_=peer;content_.nearbyTarget=peer;
    // The Nearby tab shows it on its way.
    content_.page=Page::Shelf;content_.shelfDetail=-1;content_.shelfTab=2;sendToPeer(peer,std::move(paths));return true;
}
// The whole Shelf, dragged as a stack: onto a paired PC in the island, or out of it anywhere files go.
void IslandWindow::dragStack(){
    std::vector<std::wstring> files;std::shared_ptr<const Artwork> preview;
    for(auto& item:content_.shelf)if(item.kind==ShelfItem::Kind::File){files.push_back(item.value);if(!preview)preview=item.preview;}
    if(files.empty())return;auto data=shelfFilesData(files);shelfDragImage(data.Get(),preview);
    ComPtr<ShelfDragSource> source;source.Attach(new ShelfDragSource);DWORD effect=0;DoDragDrop(data.Get(),source.Get(),DROPEFFECT_COPY,&effect);
    content_.dropHover=false;content_.dropZone=Action::None;refresh();store_.log("Info","shelf_stack_dragged");
}
bool IslandWindow::shareAction(Action a){
    const double now=seconds();
    auto close=[&]{events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);};
    auto note=[&](const std::wstring& text){content_.shelfStatus=text;content_.shelfStatusUntil=now+4;refresh();};
    auto shelfFiles=[&]{std::vector<std::wstring> files;for(auto& item:content_.shelf)if(item.kind==ShelfItem::Kind::File)files.push_back(item.value);return files;};
    switch(a){
    case Action::SharePair:if(share_)share_->confirmPair(true);close();return true;
    case Action::ShareDecline:if(share_){if(content_.notice.kind==14)share_->confirmPair(false);else if(content_.notice.kind==15)share_->answer(shareOffer_,false);}close();return true;
    case Action::ShareAccept:if(share_)share_->answer(shareOffer_,true);close();return true;
    case Action::ShareShow:{const std::wstring path=content_.notice.path;close();if(!path.empty())if(auto* pidl=ILCreateFromPathW(path.c_str())){SHOpenFolderAndSelectItems(pidl,0,nullptr,0);ILFree(pidl);}return true;}
    case Action::ShelfNearby:{content_.shelfDetail=-1;if(content_.shelfTab!=2&&!motion_.reduced){motion_.swipe.reset(14.,now);motion_.swipe.retarget(0,now,MotionTokens::content);}content_.shelfTab=2;if(share_)content_.nearby=share_->peers();refresh();animate();return true;}
    // A Shelf item (a file or a folder) to the PC sends go to.
    case Action::ShareSend:{const int index=content_.shelfDetail;if(index<0||size_t(index)>=content_.shelf.size())return true;const auto& item=content_.shelf[size_t(index)];
        auto it=std::find_if(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==shareTarget_&&p.paired&&p.online;});
        if(!share_||it==content_.nearby.end()){note(L"Pair a PC in Shelf › Nearby first");return true;}
        sendToPeer(it->id,{item.value});return true;}
    // Clicking the stack shows where it can go.
    case Action::ShelfStack:if(settings_.sharing)return shareAction(Action::ShelfNearby);note(L"Drag the stack to drop every file somewhere");return true;
    default:break;}
    if(inRange(a,Action::NearbyBase,Action::NearbyEnd)){const size_t i=size_t(int(a)-int(Action::NearbyBase));
        if(i<content_.nearby.size()){const auto p=content_.nearby[i];
            if(!p.paired){if(share_&&p.online){share_->pair(p.id);note(L"Pairing with "+p.name+L"…");}}
            else{shareTarget_=p.id;content_.nearbyTarget=p.id;refresh();}}
        return true;}
    if(inRange(a,Action::NearbyForgetBase,Action::NearbyForgetEnd)){const size_t i=size_t(int(a)-int(Action::NearbyForgetBase));
        if(share_&&i<content_.nearby.size()&&content_.nearby[i].paired){share_->forget(content_.nearby[i].id);note(L"Forgot "+content_.nearby[i].name);}return true;}
    // Send Shelf: every file on the Shelf to that PC, in one transfer.
    if(inRange(a,Action::NearbySendBase,Action::NearbySendEnd)){const size_t i=size_t(int(a)-int(Action::NearbySendBase));
        if(share_&&i<content_.nearby.size()&&content_.nearby[i].paired){auto files=shelfFiles();if(files.empty())note(L"There are no files on the Shelf");else{shareTarget_=content_.nearby[i].id;content_.nearbyTarget=shareTarget_;sendToPeer(content_.nearby[i].id,std::move(files));}}return true;}
    // Phase 5H: a paired PC's Shelf, looked into and taken from.
    if(inRange(a,Action::NearbyBrowseBase,Action::NearbyBrowseEnd)){const size_t i=size_t(int(a)-int(Action::NearbyBrowseBase));
        if(share_&&i<content_.nearby.size()&&content_.nearby[i].paired){auto& r=content_.remote;r={};r.open=true;r.peer=content_.nearby[i].id;r.name=content_.nearby[i].name;share_->askShelf(r.peer);
            if(!motion_.reduced){motion_.swipe.reset(22,now);motion_.swipe.retarget(0,now,MotionTokens::content);}refresh();animate();store_.log("Info","share_shelf_browsed");}return true;}
    if(a==Action::RemoteShelfBack){content_.remote.open=false;if(!motion_.reduced){motion_.swipe.reset(-22,now);motion_.swipe.retarget(0,now,MotionTokens::content);}refresh();animate();return true;}
    if(a==Action::RemoteShelfRefresh){auto& r=content_.remote;if(share_&&r.open){r.state=0;share_->askShelf(r.peer);refresh();}return true;}
    if(a==Action::RemoteShelfUp||a==Action::RemoteShelfDown){auto& r=content_.remote;r.offset=std::clamp(r.offset+(a==Action::RemoteShelfUp?-4:4),0,std::max(0,int(r.items.size())-4));refresh();return true;}
    if(inRange(a,Action::RemoteItemBase,Action::RemoteItemEnd)){auto& r=content_.remote;const size_t i=size_t(r.offset+int(a)-int(Action::RemoteItemBase));
        if(share_&&r.open&&r.state==1&&i<r.items.size()){const auto& item=r.items[i];const uint32_t id=share_->takeFromShelf(r.peer,uint32_t(i),item.name);
            if(id){ContentSnapshot::Transfer t;t.id=id;t.peer=r.peer;t.name=r.name;t.title=item.name;t.outgoing=false;t.total=item.size;content_.transfers.push_back(t);note(L"Taking "+item.name+L"\u2026");store_.log("Info","share_shelf_take");}}return true;}
    if(inRange(a,Action::NearbyCancelBase,Action::NearbyCancelEnd)){const size_t i=size_t(int(a)-int(Action::NearbyCancelBase));
        if(share_&&i<content_.nearby.size())for(auto& t:content_.transfers)if(t.peer==content_.nearby[i].id){share_->cancel(t.id);note(L"Stopping…");break;}return true;}
    return false;
}
}
