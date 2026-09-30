#include "Productivity/QrCode.h"
#include "IslandWindow.h"
#include "Mirror/MirrorHost.h"
#include "Mirror/MirrorWindow.h"
#include "Media/CoverCodec.h"
#include <shlobj.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <future>
// Phase 5F: sharing between your own PCs. The service (Productivity/ShareService) does the
// network and the cryptography on its own threads; here its events become cards (the pairing
// code, an offer to accept, how a transfer went), the Shelf's Nearby tab and its Send button.
// Phase 5G: files and folders dropped straight onto a PC while dragging over the island, the whole
// Shelf sent at once (a row's Send Shelf, or its stack dragged onto a PC), transfers shown as they
// go (a Nearby row's progress and Stop, the compact island's chip), and music offers (IslandMusic.cpp).
// 0.19: phones (Arnav Island for Android). Their remote is answered here, their notifications, low battery and
// find-my-phone become phone cards (kind 19), and a phone's Nearby row shows its battery and a Ring button.
namespace nexus {
namespace {
std::wstring bytesText(uint64_t b){wchar_t t[32];if(b<1024)swprintf(t,32,L"%llu bytes",static_cast<unsigned long long>(b));else if(b<1024*1024)swprintf(t,32,L"%.0f KB",b/1024.);else if(b<(1ull<<30))swprintf(t,32,L"%.1f MB",b/1048576.);else swprintf(t,32,L"%.2f GB",b/1073741824.);return t;}
std::wstring folderOf(const KNOWNFOLDERID& id){PWSTR p=nullptr;std::wstring out;if(SUCCEEDED(SHGetKnownFolderPath(id,KF_FLAG_CREATE,nullptr,&p))&&p)out=p;CoTaskMemFree(p);return out;}
std::wstring filesText(uint32_t count){return count==1?L"1 file":std::to_wstring(count)+L" files";}
// A phone's remote command on its way to the island's thread; the network thread waits for the answer.
struct RemoteCall{std::string peer;RemoteCommand command=RemoteCommand::Status;std::vector<uint8_t> payload;std::promise<std::vector<uint8_t>> answer;};
std::wstring fromUtf8Bytes(const std::vector<uint8_t>& b,size_t limit){if(b.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<const char*>(b.data()),int(std::min(b.size(),limit)),nullptr,0);
    std::wstring w(size_t(std::max(n,0)),L'\0');if(n>0)MultiByteToWideChar(CP_UTF8,0,reinterpret_cast<const char*>(b.data()),int(std::min(b.size(),limit)),w.data(),n);return w;}
std::string utf8Of(const std::wstring& w){if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);std::string s(size_t(std::max(n,0)),'\0');if(n>0)WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;}
// The first line of some text, at most 80 characters, for a card.
std::wstring cardLine(const std::wstring& t){std::wstring out;for(wchar_t c:t){if(c==L'\r'||c==L'\n'){if(!out.empty())break;continue;}if(c>=32)out+=c;if(out.size()>=80){out+=L"\u2026";break;}}return out;}
}
// Sharing runs only while its setting is on (never in test runs: no sockets, no firewall prompts there).
void IslandWindow::syncSharing(){
    if(!window_)return;
    // The relay's setting takes effect by starting the service again.
    if(share_&&relayOn_!=settings_.relay){share_.reset();content_.nearby.clear();content_.transfers.clear();content_.internet=false;}
    inputAllowed_->store(settings_.phoneControl);
    if(settings_.sharing&&!share_&&!testing_){
        ShareOptions o;std::filesystem::path data=std::filesystem::path(settingsFile_).parent_path();if(data.empty())data=std::filesystem::path(folderOf(FOLDERID_LocalAppData))/L"ArnavIsland";
        o.folder=data.wstring();o.downloads=folderOf(FOLDERID_Downloads);if(o.downloads.empty())o.downloads=(data/L"Received").wstring();o.handoff=(data/L"Handoff").wstring();
        // 0.19: a phone's remote, answered on this thread (a phone gives up on an answer that takes over 4 s).
        o.remote=[window=window_](const std::string& peer,RemoteCommand command,const std::vector<uint8_t>& payload){return remoteFromNetwork(window,peer,command,payload);};
        // 0.20: other networks through the relay; a phone's trackpad and keyboard, straight from the network thread.
        o.relay=settings_.relay;relayOn_=settings_.relay;
        o.input=[allowed=inputAllowed_](const std::string&,const std::vector<uint8_t>& frame){if(allowed->load())phoneInput(frame);};
        // 0.24: screens, either way, on the network thread. This screen goes to a phone only while it may control this
        // PC (its taps and typing come back as input); a phone's own screen opens in a window here.
        o.mirror=[window=window_,allowed=inputAllowed_,stop=mirrorStop_](const std::string&,const std::wstring& name,const std::vector<uint8_t>& request,mirror::MirrorIo& io){
            mirror::MirrorRequest r;if(!mirror::readRequest(request,r)){io.send(mirror::refusal(2));return;}
            if(r.kind==1){
                if(!allowed->load()){io.send(mirror::refusal(1));return;}
                stop->store(false);auto* phone=new std::wstring(name);if(!PostMessageW(window,MirrorMessage,1,LPARAM(phone)))delete phone;
                auto source=mirror::monitorSource({0,0});
                mirror::sendScreen(io,r,*source,[allowed](const std::vector<uint8_t>& f){if(allowed->load())phoneInput(f);},[](RECT a){mirrorArea(&a);},*stop);
                PostMessageW(window,MirrorMessage,0,0);return;}
            mirror::MirrorWindow shown;mirror::receiveScreen(io,r,shown,shown.closed);};
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
        case K::Peers:{content_.nearby=share_->peers();content_.internet=share_->internet();proximity();
            // 0.22.1: a phone that went away takes its hotspot with it (its "off" may never have arrived); when it's back
            // with the hotspot still on, it says so again and the card shows again.
            for(auto it=hotspots_.begin();it!=hotspots_.end();){const bool here=std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==it->first&&p.online;});
                if(here){++it;continue;}content_.hotspots.erase(it->first);it=hotspots_.erase(it);}
            // A phone that went away is told the focus clock again when it's back (it may have restarted meanwhile).
            std::erase_if(focusTold_,[&](const std::string& id){return std::none_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==id&&p.online;});});
            syncPhoneFocus();if(state_==IslandState::Expanded&&content_.page==Page::Phone){choosePhone(false);redraw=true;}
            if(content_.phoneView.open&&std::none_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==content_.phoneView.peer&&p.paired;}))content_.phoneView={};
            // Sends go to the chosen paired PC; failing that, the first paired PC that is here.
            const bool keep=std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==shareTarget_&&p.paired;});
            if(!keep){shareTarget_.clear();for(auto& p:content_.nearby)if(p.paired&&p.online){shareTarget_=p.id;break;}}content_.nearbyTarget=shareTarget_;
            if(state_==IslandState::Expanded&&(content_.page==Page::Shelf||content_.page==Page::Media))redraw=true;break;}
        case K::PairCode:{wchar_t code[16];swprintf(code,16,L"%03u %03u",e.code/1000,e.code%1000);shareCard(14,e.name,code,{},60);
            const bool phone=std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==e.peer&&p.phone;});if(content_.notice.kind==14&&content_.notice.phone!=phone){content_.notice.phone=phone;refresh();}break;}
        case K::Paired:{content_.pairing={};const bool phone=std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==e.peer&&p.phone;});
            shareCard(16,L"Paired with "+e.name,phone?std::wstring(L"Files go both ways, and it can control this PC"):e.detail,{},4);break;}
        case K::PairFailed:shareCard(16,L"Not paired with "+(e.name.empty()?std::wstring(L"that PC"):e.name),e.detail,{},4.5);break;
        // 0.20: photos a phone took for the Shelf come in without asking (while that's on).
        case K::Offer:if(e.toShelf&&settings_.continuity&&std::any_of(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==e.peer&&p.phone&&p.paired;})){share_->answer(e.transfer,true);store_.log("Info","continuity_accepted");break;}
            shareOffer_=e.transfer;shareCard(15,e.name,e.file+(e.count>1?L"  ·  "+filesText(e.count):L"")+L"  ·  "+bytesText(e.size),{},60);break;
        // Progress: the transfer's row and chip fill (redrawn at most about five times a second, and at the end).
        case K::Progress:{auto it=std::find_if(content_.transfers.begin(),content_.transfers.end(),[&](auto& t){return t.id==e.transfer;});
            if(it==content_.transfers.end()){ContentSnapshot::Transfer t;t.id=e.transfer;t.peer=e.peer;t.name=e.name;t.title=e.file;t.outgoing=e.outgoing;content_.transfers.push_back(t);it=content_.transfers.end()-1;}
            it->done=e.done;it->total=e.size;it->count=e.count;if(!e.file.empty())it->title=e.file;
            // Its speed: bytes over the last quarter second or more, eased (so the ring and the time left don't jitter).
            if(it->rateAt<=0){it->rateAt=now;it->rateDone=e.done;}else if(now-it->rateAt>=.25&&e.done>=it->rateDone){const double speed=double(e.done-it->rateDone)/(now-it->rateAt);it->rate=it->rate>0?it->rate*.7+speed*.3:speed;it->rateAt=now;it->rateDone=e.done;}
            if(now-transferDrawn_>=.2||e.done==e.size){transferDrawn_=now;redraw=true;}break;}
        // Show opens Downloads with the arrival selected (a folder, or the first file).
        case K::Received:drop(e.transfer);
            if(e.toShelf){std::wstring first;for(auto& path:e.paths)if(content_.shelf.size()<32&&std::none_of(content_.shelf.begin(),content_.shelf.end(),[&](auto& i){return i.value==path;})){
                    content_.shelf.push_back({ShelfItem::Kind::File,path,std::filesystem::path(path).filename().wstring()});if(first.empty())first=path;}
                requestPreviews();continuityPath_=first;
                phoneCard(e.count==1?L"Photo from "+e.name:std::to_wstring(e.count)+L" photos from "+e.name,L"On your Shelf",e.name,nullptr,6);store_.log("Info","continuity_arrived");break;}
            if(e.code==1){if(!e.detail.empty()&&std::none_of(content_.shelf.begin(),content_.shelf.end(),[&](auto& i){return i.value==e.detail;})&&content_.shelf.size()<32){content_.shelf.push_back({ShelfItem::Kind::File,e.detail,std::filesystem::path(e.detail).filename().wstring()});requestPreviews();}
                shareCard(16,L"Taken from "+e.name+L"\u2019s Shelf",e.file+(e.count>1?L"  ·  "+filesText(e.count):L"")+L"  ·  now on your Shelf",e.detail,6);store_.log("Info","share_shelf_taken_here");break;}
            shareCard(16,L"Received from "+e.name,e.file+(e.count>1?L"  ·  "+filesText(e.count):L""),e.detail,8);break;
        case K::Sent:drop(e.transfer);content_.shelfStatus.clear();shareCard(16,L"Sent to "+e.name,e.file+(e.count>1?L"  ·  "+filesText(e.count):L"")+L"  ·  "+e.detail,{},4);break;
        case K::Failed:{drop(e.transfer);content_.shelfStatus.clear();
            // Stopped here: a quiet note. An offer the other PC took back replaces its card.
            if(e.detail==L"You stopped it"){content_.shelfStatus=L"Stopped "+e.file;content_.shelfStatusUntil=now+3;redraw=true;break;}
            if(e.transfer&&e.transfer==handoffOffer_){handoffOffer_=0;if(state_==IslandState::Notification&&content_.notice.kind==17)shareCard(16,e.detail,e.file,{},3.5);break;}
            // A command to a phone that didn't go (an action, a photo) names the phone rather than a file.
            if(e.file.empty()&&e.outgoing&&!e.name.empty()){shareCard(16,L"Not done on "+e.name,e.detail,{},4.5);break;}
            shareCard(16,e.file.empty()?std::wstring(L"Couldn't share"):L"Couldn't share "+e.file,e.detail,{},5);break;}
        case K::Handoff:case K::HandoffAnswered:case K::HandoffFile:handoffEvent(e);break;
        // Phase 5H: another PC's Shelf, as asked for; and this Shelf, taken from.
        case K::ShelfList:{auto& r=content_.remote;if(!r.open||r.peer!=e.peer)break;r.state=e.code==0?1:e.code==1?2:3;r.status=e.detail;if(!e.name.empty())r.name=e.name;r.items=e.shelf;r.offset=std::clamp(r.offset,0,std::max(0,int(r.items.size())-4));
            r.previews.clear();for(auto& item:r.items)r.previews.push_back(decodeCover(item.preview,64));redraw=true;break;}
        case K::ShelfTaken:drop(e.transfer);shareCard(16,e.name+L" took "+e.file,L"A copy, from your Shelf",{},3.5);store_.log("Info","share_shelf_taken_there");break;
        // 0.19: a phone's battery (its row, and a card once when it runs low), its notifications, and find my phone.
        case K::PhoneStatus:{content_.nearby=share_->peers();auto it=phoneBattery_.try_emplace(e.peer,101).first;
            if(settings_.phoneNotices&&e.battery>=0&&e.battery<=20&&!e.charging&&it->second>20){
                // 0.22: with the phone's forecast, when it runs out.
                std::wstring lasts;if(auto p=content_.phones.find(e.peer);p!=content_.phones.end())for(auto& [k,v]:p->second.values)if(k==L"Lasts until")lasts=v;
                phoneCard(e.name+L" is at "+std::to_wstring(e.battery)+L"%",lasts.empty()?std::wstring(L"Charge it soon"):L"Lasts until "+lasts+L"  \u00b7  charge it soon",e.name,nullptr,5);}
            if(e.battery>=0)it->second=e.charging?101:e.battery;if(state_==IslandState::Expanded&&content_.page==Page::Shelf)redraw=true;break;}
        case K::PhoneNotice:{if(!settings_.phoneNotices)break;
            // A call stays up while it rings (the phone says when it has gone); others for a few seconds, longer with actions.
            const std::wstring title=e.file.empty()?e.app:e.file;
            phoneCard(title.empty()?e.name:title,e.detail,(e.app.empty()?std::wstring():e.app+L"  \u00b7  ")+e.name,decodeCover(e.icon,96),e.urgent?45:e.actions.empty()?6:10,e.peer,e.key,e.actions);store_.log("Info","phone_notice");break;}
        case K::PhoneNoticeGone:{
            std::erase_if(heldCards_,[&](auto& h){return h.notice.kind==19&&h.notice.peer==e.peer&&h.notice.key==e.key;});
            if(state_==IslandState::Notification&&content_.notice.kind==19&&content_.notice.peer==e.peer&&!e.key.empty()&&content_.notice.key==e.key){events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);}
            syncBud();break;}
        // 0.22: a phone's hotspot came on (its name and password) or went off.
        case K::PhoneHotspot:phoneHotspot(e.peer,e.name,e.code!=0,e.file,e.detail);break;
        // 0.23: a phone's readings now, asked for while the Phone page shows (with the cover of what plays on it).
        case K::PhoneLive:{auto& info=content_.phones[e.peer];info.values.clear();info.at=now;size_t from=0;
            while(from<e.detail.size()&&info.values.size()<48){size_t end=e.detail.find(L'\n',from);if(end==std::wstring::npos)end=e.detail.size();const std::wstring line=e.detail.substr(from,end-from);from=end+1;
                const auto tab=line.find(L'\t');if(tab!=std::wstring::npos&&tab>0)info.values.push_back({line.substr(0,tab).substr(0,32),line.substr(tab+1).substr(0,80)});}
            if(!e.icon.empty()){if(auto art=decodeCover(e.icon,96))content_.phoneCovers[e.peer]=art;}else content_.phoneCovers.erase(e.peer);
            phoneAsking_->store(false);if(state_==IslandState::Expanded&&content_.page==Page::Phone&&content_.phonePage==e.peer)redraw=true;break;}
        // A phone's readings, for its own view in Nearby.
        case K::PhoneDetails:{auto& info=content_.phones[e.peer];info.values.clear();info.at=now;size_t from=0;
            while(from<e.detail.size()&&info.values.size()<40){size_t end=e.detail.find(L'\n',from);if(end==std::wstring::npos)end=e.detail.size();const std::wstring line=e.detail.substr(from,end-from);from=end+1;
                const auto tab=line.find(L'\t');if(tab!=std::wstring::npos&&tab>0)info.values.push_back({line.substr(0,tab).substr(0,32),line.substr(tab+1).substr(0,80)});}
            if(content_.phoneView.open&&content_.phoneView.peer==e.peer)redraw=true;break;}
        // Pairing from anywhere: the code (for ten minutes) and its QR code, the link a phone's camera opens. Shown large in
        // Nearby when that's open; otherwise a card that opens it.
        case K::PairingCode:{if(e.detail.empty()){content_.pairing={};shareCard(16,L"Couldn\u2019t make a code",L"This PC isn\u2019t connected to the internet",{},4.5);break;}
            std::string plain;for(wchar_t c:e.detail)if(c!=L'-')plain+=char(c);const auto qr=qrEncode(share_?share_->pairingLink(plain):"arnavisland://pair/"+plain);
            content_.pairing={e.detail,now+600,qr.size,qr.modules};
            if(state_==IslandState::Expanded&&content_.page==Page::Shelf&&content_.shelfTab==2){content_.phoneView={};content_.remote.open=false;redraw=true;}
            else shareCard(20,e.detail,L"Scan its QR code, or type it on your phone",{},600);
            break;}
        case K::Rang:phoneCard(L"Ringing "+e.name,L"Loudly, even on silent. Stop it on the phone",e.name,nullptr,4);break;}
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
            else if(p.phone){content_.phoneView={true,p.id};if(!motion_.reduced){motion_.swipe.reset(22,now);motion_.swipe.retarget(0,now,MotionTokens::content);}refresh();animate();store_.log("Info","phone_view");}
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
    // 0.19: find my phone.
    if(inRange(a,Action::NearbyRingBase,Action::NearbyRingEnd)){const size_t i=size_t(int(a)-int(Action::NearbyRingBase));
        if(share_&&i<content_.nearby.size()&&content_.nearby[i].paired&&content_.nearby[i].phone){share_->ring(content_.nearby[i].id);note(L"Ringing "+content_.nearby[i].name+L"\u2026");store_.log("Info","phone_ring");}return true;}
    // 0.20: pairing from anywhere with a code; a phone's own view; its notification's actions (a reply opens a reply box).
    // A code already on offer is shown again rather than replaced.
    if(a==Action::PairAnywhere&&!content_.pairing.code.empty()&&now<content_.pairing.until){content_.phoneView={};refresh();return true;}
    if(a==Action::PairAnywhere){if(!share_)return true;if(!share_->internet()){note(L"This PC isn\u2019t connected to the internet");return true;}share_->hostPairing();note(L"Making a code\u2026");store_.log("Info","pair_anywhere");return true;}
    if(a==Action::PairTypeCode){if(!share_)return true;if(!share_->internet()){note(L"This PC isn\u2019t connected to the internet");return true;}openPairCode();return true;}
    if(a==Action::PairingStop){const bool card=state_==IslandState::Notification&&content_.notice.kind==20;content_.pairing={};if(share_)share_->stopPairing();
        if(card)close();else refresh();return true;}
    // The pairing card's QR code: Nearby opens with it, large enough for a phone's camera.
    if(a==Action::PairingShow){events_.dismiss(now);content_.activity.clear();content_.page=Page::Shelf;content_.shelfTab=2;content_.shelfDetail=-1;content_.phoneView={};content_.remote.open=false;
        content_.pinned=true;transition(IslandState::Expanded);refresh();animate();return true;}
    // 0.22: a phone's hotspot, joined from its card or its view.
    if(a==Action::HotspotJoin){const std::string peer=content_.notice.peer;events_.dismiss(now);content_.activity.clear();transition(IslandState::Compact);joinHotspot(peer);return true;}
    if(a==Action::HotspotLater){close();return true;}
    // 0.24: stop showing this screen on the phone.
    if(a==Action::MirrorStop){mirrorStop_->store(true);close();return true;}
    if(a==Action::PhoneHotspot){joinHotspot(content_.page==Page::Phone?content_.phonePage:content_.phoneView.peer);return true;}
    if(a==Action::PhoneBack){content_.phoneView={};if(!motion_.reduced){motion_.swipe.reset(-22,now);motion_.swipe.retarget(0,now,MotionTokens::content);}refresh();animate();return true;}
    if(a==Action::PhoneRing||a==Action::PhonePhoto||a==Action::PhoneClipboard){const std::string peer=content_.page==Page::Phone?content_.phonePage:content_.phoneView.peer;auto it=std::find_if(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==peer;});
        if(!share_||it==content_.nearby.end())return true;
        if(a==Action::PhoneRing){share_->ring(peer);note(L"Ringing "+it->name+L"\u2026");}
        else if(a==Action::PhonePhoto){share_->askPhoto(peer);note(L"On "+it->name+L": the camera opens, or tap its notification");}
        else{std::wstring text;if(OpenClipboard(window_)){if(HANDLE h=GetClipboardData(CF_UNICODETEXT))if(auto* t=static_cast<const wchar_t*>(GlobalLock(h))){text.assign(t,wcsnlen(t,GlobalSize(h)/sizeof(wchar_t)));GlobalUnlock(h);}CloseClipboard();}
            if(text.empty())note(L"There\u2019s no text on the clipboard");else{share_->pushClipboard(peer,text,looksSecret(text));note(L"On "+it->name+L"\u2019s clipboard");}}
        return true;}
    if(inRange(a,Action::NoticeActionBase,Action::NoticeActionEnd)){const size_t i=size_t(int(a)-int(Action::NoticeActionBase));const auto n=content_.notice;
        if(n.kind!=19||i>=n.actions.size())return true;
        if(n.actions[i].second){events_.dismiss(now);content_.activity.clear();openReply(n.peer,n.key,int(i),n.app);return true;}
        if(share_)share_->noticeAction(n.peer,n.key,int(i),L"");store_.log("Info","phone_action");close();return true;}
    if(inRange(a,Action::NearbyCancelBase,Action::NearbyCancelEnd)){const size_t i=size_t(int(a)-int(Action::NearbyCancelBase));
        if(share_&&i<content_.nearby.size())for(auto& t:content_.transfers)if(t.peer==content_.nearby[i].id){share_->cancel(t.id);note(L"Stopping…");break;}return true;}
    return false;
}
// ---- 0.19: phones ----
std::vector<uint8_t> IslandWindow::remoteFromNetwork(HWND window,const std::string& peer,RemoteCommand command,const std::vector<uint8_t>& payload){
    auto call=std::make_shared<RemoteCall>();call->peer=peer;call->command=command;call->payload=payload;auto answer=call->answer.get_future();
    auto* held=new std::shared_ptr<RemoteCall>(call);if(!PostMessageW(window,RemoteMessage,0,reinterpret_cast<LPARAM>(held))){delete held;return {remoteFailed};}
    if(answer.wait_for(std::chrono::seconds(4))!=std::future_status::ready)return {remoteFailed};return answer.get();
}
std::wstring IslandWindow::peerName(const std::string& peer)const{for(auto& p:content_.nearby)if(p.id==peer)return p.name;return L"Your phone";}
// A phone's card (kind 19): its app's icon or a phone, a title, a line of text and where it came from.
void IslandWindow::phoneCard(const std::wstring& title,const std::wstring& detail,const std::wstring& source,std::shared_ptr<const Artwork> icon,double duration,
    const std::string& peer,const std::string& key,const std::vector<std::pair<std::wstring,bool>>& actions){
    if(!renderer_)return;
    content_.notice={};content_.notice.kind=19;content_.notice.phone=true;content_.notice.app=title;content_.notice.detail=detail;content_.notice.source=source;content_.notice.icon=std::move(icon);
    content_.notice.peer=peer;content_.notice.key=key;content_.notice.actions=actions;if(content_.notice.actions.size()>2)content_.notice.actions.resize(2);
    content_.pinned=false;{const Activity a{ActivityKind::Notification,"phone",72,19.,2.4,duration};if(holdCard(a))return;events_.publish(a,seconds());}
    transition(IslandState::Notification);presentActivity();alertSplash();store_.log("Info","phone_card_shown");
}
void IslandWindow::remoteCall(LPARAM l){
    std::unique_ptr<std::shared_ptr<RemoteCall>> held(reinterpret_cast<std::shared_ptr<RemoteCall>*>(l));if(!held||!*held)return;auto& c=**held;
    std::vector<uint8_t> answer;try{answer=remoteAnswer(c.peer,c.command,c.payload);}catch(...){answer={remoteFailed};}
    try{c.answer.set_value(std::move(answer));}catch(...){}
}
// What a paired phone asked of this PC (with "My phone can control this PC" on).
std::vector<uint8_t> IslandWindow::remoteAnswer(const std::string& peer,RemoteCommand command,const std::vector<uint8_t>& payload){
    if(!settings_.phoneControl)return {remoteNotAllowed};
    const auto& p=content_.playback;const double now=seconds();const std::wstring from=peerName(peer);
    // What a card shows of copied text: its first line, or dots for something password-like.
    auto shownClip=[&](const std::wstring& t){return settings_.hideSecrets&&looksSecret(t)?std::wstring(L"\u2022\u2022\u2022\u2022\u2022\u2022  \u00b7  hidden"):cardLine(t);};
    switch(command){
    case RemoteCommand::Status:{RemoteStatus st;
        st.available=p.available;st.playing=p.playing;st.canPrevious=p.canPrevious;st.canNext=p.canNext;st.canToggle=p.canToggle;st.canSeek=p.canSeek;
        st.duration=std::max(0.,p.duration);st.position=std::clamp(p.position+(p.playing?std::max(0.,now-p.sampledAt):0.),0.,st.duration>0?st.duration:1e9);
        if(p.available){st.title=p.title;st.artist=p.artist;st.app=p.appName;}
        // The cover, encoded once per picture.
        if(p.available&&p.artwork){if(p.artwork.get()!=remoteCoverOf_){remoteCoverOf_=p.artwork.get();remoteCover_=encodeCover(*p.artwork,320,shareCoverLimit,.82f);if(remoteCover_.empty())remoteCover_=encodeCover(*p.artwork,200,shareCoverLimit,.75f);}st.cover=remoteCover_;}
        else{remoteCoverOf_=nullptr;remoteCover_.clear();}
        st.volume=audio_?audio_->value.load():content_.volume;st.muted=audio_?audio_->muted.load():content_.muted;
        st.battery=content_.battery;st.batteryPresent=content_.battery>=0;st.charging=content_.charging;st.clipboard=settings_.sharing&&settings_.universalClipboard;
        // CPU: busy time over all time since the last ask.
        FILETIME idle{},kernel{},user{};if(GetSystemTimes(&idle,&kernel,&user)){auto v=[](FILETIME f){return (ULONGLONG(f.dwHighDateTime)<<32)|f.dwLowDateTime;};const ULONGLONG i=v(idle),t=v(kernel)+v(user);
            if(cpuTotal_&&t>cpuTotal_&&t-cpuTotal_>=100000){cpuLast_=int(std::lround(100.*(1.-double(i-cpuIdle_)/double(t-cpuTotal_))));cpuLast_=std::clamp(cpuLast_,0,100);}if(!cpuTotal_||t-cpuTotal_>=100000){cpuIdle_=i;cpuTotal_=t;}}
        st.cpu=cpuLast_;st.name=content_.shareName;
        if(settings_.weather&&content_.weather.valid)st.weather=temperatureText(content_.weather.temperature,settings_.weatherUnit)+L" "+skyName(skyOf(content_.weather.code));
        return remoteStatusAnswer(st,payload);}
    // 1 play or pause, 2 previous, 3 next, 4 play, 5 pause.
    case RemoteCommand::Media:{if(payload.empty()||payload[0]<1||payload[0]>5)return {remoteFailed};if(!p.available)return {remoteUnsupported};mediaCommand(payload[0]);refresh();return {remoteOk};}
    case RemoteCommand::Volume:{if(payload.empty()||!audio_)return {remoteUnsupported};audio_->setVolume(std::min<int>(payload[0],100));return {remoteOk};}
    // 0 sound on, 1 muted, 2 the other way.
    case RemoteCommand::Mute:{if(payload.empty()||!audio_)return {remoteUnsupported};const bool want=payload[0]==2?!audio_->muted.load():payload[0]==1;if(want!=audio_->muted.load())audio_->toggleMute();return {remoteOk};}
    case RemoteCommand::Lock:return LockWorkStation()?std::vector<uint8_t>{remoteOk}:std::vector<uint8_t>{remoteFailed};
    case RemoteCommand::ClipboardGet:{std::wstring text;if(OpenClipboard(window_)){if(HANDLE h=GetClipboardData(CF_UNICODETEXT))if(auto* t=static_cast<const wchar_t*>(GlobalLock(h))){text.assign(t,wcsnlen(t,GlobalSize(h)/sizeof(wchar_t)));GlobalUnlock(h);}CloseClipboard();}
        std::string u=utf8Of(text);if(u.size()>60000){size_t n=60000;while(n>0&&(uint8_t(u[n])&0xc0)==0x80)--n;u.resize(n);}
        std::vector<uint8_t> r{remoteOk};const uint32_t n=uint32_t(u.size());for(int k=0;k<4;++k)r.push_back(uint8_t(n>>(8*k)));r.insert(r.end(),u.begin(),u.end());
        phoneCard(L"Clipboard sent to "+from,text.empty()?std::wstring(L"It was empty"):shownClip(text),from,nullptr,2.5);return r;}
    case RemoteCommand::ClipboardSet:{const std::wstring text=fromUtf8Bytes(payload,256*1024);if(text.empty())return {remoteFailed};lastPhoneClip_=text;copyText(text,true);phoneCard(L"Copied from "+from,shownClip(text),from,nullptr,3);return {remoteOk};}
    case RemoteCommand::Seek:{if(payload.size()<8||!p.canSeek)return {remoteUnsupported};double to=0;std::memcpy(&to,payload.data(),8);if(!std::isfinite(to))return {remoteFailed};
        to=std::clamp(to,0.,std::max(0.,p.duration));mediaSeek(to);content_.playback.position=to;content_.playback.sampledAt=now;refresh();return {remoteOk};}
    // Links only (http and https), opened in the default browser.
    case RemoteCommand::Open:{const std::wstring url=fromUtf8Bytes(payload,4096);std::wstring low=url;std::transform(low.begin(),low.end(),low.begin(),::towlower);
        if(!(low.starts_with(L"https://")||low.starts_with(L"http://"))||url.size()<10||url.find_first_of(L"\r\n\t \"")!=std::wstring::npos)return {remoteNotAllowed};
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)return {remoteFailed};
        phoneCard(L"Opened from "+from,cardLine(url),from,nullptr,3);store_.log("Info","phone_open");return {remoteOk};}
    // Find my PC: a chime, again and again, and the edge lighting up, until the card is closed (or after twelve seconds).
    // 0.22 (revision 5): everything else on the island (IslandRemote.cpp).
    case RemoteCommand::Stats:return remoteStats();
    case RemoteCommand::Settings:return remoteSettings(payload);
    case RemoteCommand::Controls:return remoteControls(payload);
    case RemoteCommand::Command:return remoteCommand(payload);
    case RemoteCommand::Audio:return remoteAudio(payload);
    case RemoteCommand::Island:return remoteIsland(payload);
    case RemoteCommand::Battery:return remoteBattery();
    case RemoteCommand::RingPC:{ringChimes_=8;playSound(Sound::Chime);SetTimer(window_,RingPCTimer,1400,nullptr);phoneCard(L"Here I am",from+L" is looking for this PC",from,nullptr,12);store_.log("Info","phone_find_pc");return {remoteOk};}
    // The song's lyrics, as the island has them (the phone shows them in time with the song).
    case RemoteCommand::Lyrics:{const std::wstring key=p.available?p.title+L"\t"+p.artist:std::wstring();int state=0;std::vector<ShareLyricLine> lines;
        if(settings_.lyrics&&p.available){using State=LyricsService::State;const auto st=State(content_.lyricsState);
            if(content_.lyrics&&!content_.lyrics->empty()){state=2;for(auto& l:*content_.lyrics){ShareLyricLine line;line.time=l.time;line.text=l.text;for(auto& w:l.words)line.words.push_back({w.time,w.start});lines.push_back(std::move(line));}}
            else state=st==State::Loading||st==State::Unknown?1:3;}
        return remoteLyricsAnswer(state,key,lines);}
    default:return {remoteUnsupported};}
}
// A phone's trackpad and keyboard, as Windows input (relative moves, the three buttons, both wheels, typed text, keys).
// 0.24: the part of the virtual desktop a phone sees (this screen, while shown), for its taps: read with null, set otherwise.
RECT IslandWindow::mirrorArea(const RECT* set){
    static std::mutex lock;static RECT area{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
    std::lock_guard held(lock);if(set)area=*set;return area;
}
void IslandWindow::phoneInput(const std::vector<uint8_t>& f){
    if(f.empty())return;auto i16=[&](size_t at){return int(int16_t(uint16_t(f[at]|(f[at+1]<<8))));};
    auto mouse=[](DWORD flags,LONG dx=0,LONG dy=0,DWORD data=0){INPUT m{};m.type=INPUT_MOUSE;m.mi.dx=dx;m.mi.dy=dy;m.mi.mouseData=data;m.mi.dwFlags=flags;return m;};
    auto key=[](WORD vk,WORD scan,DWORD flags){INPUT k{};k.type=INPUT_KEYBOARD;k.ki.wVk=vk;k.ki.wScan=scan;k.ki.dwFlags=flags;return k;};
    std::vector<INPUT> list;
    switch(f[0]){
    case 0x60:if(f.size()>=5)list.push_back(mouse(MOUSEEVENTF_MOVE,i16(1),i16(3)));break;
    // 0.24: a point on this screen as the phone shows it (0-65535 each way), onto the virtual desktop.
    case 0x65:if(f.size()>=5){const RECT a=mirrorArea(nullptr);const double x=a.left+(f[1]|(f[2]<<8))/65535.*(a.right-a.left-1),y=a.top+(f[3]|(f[4]<<8))/65535.*(a.bottom-a.top-1);
        const int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN),vw=std::max(2,GetSystemMetrics(SM_CXVIRTUALSCREEN)),vh=std::max(2,GetSystemMetrics(SM_CYVIRTUALSCREEN));
        list.push_back(mouse(MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK,LONG((x-vx)*65535./(vw-1)),LONG((y-vy)*65535./(vh-1))));}break;
    case 0x61:if(f.size()>=3){const uint8_t button=f[1],state=f[2];
        const DWORD down=button==1?MOUSEEVENTF_RIGHTDOWN:button==2?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_LEFTDOWN,up=button==1?MOUSEEVENTF_RIGHTUP:button==2?MOUSEEVENTF_MIDDLEUP:MOUSEEVENTF_LEFTUP;
        if(state==1||state==2)list.push_back(mouse(down));if(state==0||state==2)list.push_back(mouse(up));}break;
    case 0x62:if(f.size()>=5){const int v=i16(1),h=i16(3);if(v)list.push_back(mouse(MOUSEEVENTF_WHEEL,0,0,DWORD(v)));if(h)list.push_back(mouse(MOUSEEVENTF_HWHEEL,0,0,DWORD(h)));}break;
    case 0x63:{const std::wstring t=fromUtf8Bytes(std::vector<uint8_t>(f.begin()+1,f.end()),8000);
        for(wchar_t c:t){if(c==L'\r')continue;if(c==L'\n'){list.push_back(key(VK_RETURN,0,0));list.push_back(key(VK_RETURN,0,KEYEVENTF_KEYUP));continue;}
            if(c<32)continue;list.push_back(key(0,c,KEYEVENTF_UNICODE));list.push_back(key(0,c,KEYEVENTF_UNICODE|KEYEVENTF_KEYUP));}break;}
    case 0x64:if(f.size()>=4){const WORD vk=WORD(f[1]|(f[2]<<8));const uint8_t state=f[3];if(!vk||vk>0xFE)break;
        // Arrows, Home, End, Delete and the like are extended keys.
        const bool extended=(vk>=VK_PRIOR&&vk<=VK_DOWN)||vk==VK_INSERT||vk==VK_DELETE||vk==VK_LWIN||vk==VK_RWIN;const DWORD ext=extended?KEYEVENTF_EXTENDEDKEY:0;const WORD scan=WORD(MapVirtualKeyW(vk,MAPVK_VK_TO_VSC));
        if(state==1||state==2)list.push_back(key(vk,scan,ext));if(state==0||state==2)list.push_back(key(vk,scan,ext|KEYEVENTF_KEYUP));}break;
    default:break;}
    if(!list.empty())SendInput(UINT(list.size()),list.data(),sizeof(INPUT));
}
// The universal clipboard: text copied here, to every paired phone that is here (private copies stay on this PC).
void IslandWindow::pushClipboardToPhones(){
    if(!share_||!settings_.universalClipboard||testing_)return;
    static const UINT exclude=RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"),history=RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    std::wstring text;bool secret=false;
    if(OpenClipboard(window_)){secret=IsClipboardFormatAvailable(exclude);
        if(!secret&&IsClipboardFormatAvailable(history))if(HANDLE h=GetClipboardData(history))if(auto* v=static_cast<const DWORD*>(GlobalLock(h))){secret=*v==0;GlobalUnlock(h);}
        if(!secret)if(HANDLE h=GetClipboardData(CF_UNICODETEXT))if(auto* t=static_cast<const wchar_t*>(GlobalLock(h))){text.assign(t,wcsnlen(t,GlobalSize(h)/sizeof(wchar_t)));GlobalUnlock(h);}
        CloseClipboard();}
    if(secret||text.empty()||text==lastPhoneClip_||text.size()>100000)return;
    lastPhoneClip_=text;const bool sensitive=looksSecret(text);int sent=0;
    for(auto& p:content_.nearby)if(p.paired&&p.phone&&p.online&&p.revision>=3){share_->pushClipboard(p.id,text,sensitive);++sent;}
    if(sent)store_.log("Info","universal_clipboard");
}
// Proximity: a paired phone on this network is near; leaving (for 45 s, with nobody at this PC) can lock it, and coming
// back after two minutes away is welcomed.
void IslandWindow::proximity(){
    const double now=seconds();
    for(auto& p:content_.nearby){if(!p.paired||!p.phone)continue;const bool here=p.online&&!p.viaInternet;auto it=phoneNear_.find(p.id);
        if(it==phoneNear_.end()){phoneNear_[p.id]={here,now};continue;}if(it->second.here==here)continue;
        const double away=now-it->second.since;it->second={here,now};
        if(here){if(settings_.proximityWelcome&&away>=120)phoneCard(L"Welcome back",p.battery>=0?std::to_wstring(p.battery)+L"% battery"+(p.charging?std::wstring(L", charging"):std::wstring()):std::wstring(L"Your phone is here"),p.name,nullptr,4);}
        else if(settings_.proximityLock)SetTimer(window_,ProximityTimer,46000,nullptr);}
}
void IslandWindow::proximityCheck(){
    KillTimer(window_,ProximityTimer);if(!settings_.proximityLock||testing_)return;const double now=seconds();bool gone=false,anyHere=false;
    for(auto& [id,n]:phoneNear_){if(n.here)anyHere=true;else if(now-n.since>=45)gone=true;}
    if(!gone||anyHere)return;
    LASTINPUTINFO input{sizeof(input)};GetLastInputInfo(&input);const DWORD idle=GetTickCount()-input.dwTime;
    if(idle>=30000){LockWorkStation();store_.log("Info","proximity_lock");}else SetTimer(window_,ProximityTimer,15000,nullptr);
}
bool IslandWindow::shareTimer(UINT_PTR id){
    if(id==ProximityTimer){proximityCheck();return true;}
    if(id==PhoneStatsTimer){KillTimer(window_,PhoneStatsTimer);clockTimer();return true;}
    if(id==PhoneLiveTimer){askPhoneLive();return true;}
    if(id==PhonePowerTimer){KillTimer(window_,PhonePowerTimer);phonePower();return true;}
    if(id==RingPCTimer){const bool showing=state_==IslandState::Notification&&content_.notice.kind==19&&content_.notice.app==L"Here I am";
        if(--ringChimes_<=0||!showing){KillTimer(window_,RingPCTimer);ringChimes_=0;return true;}playSound(Sound::Chime);alertSplash();return true;}
    return false;
}
// 0.23: the Phone page's phone: the one chosen while it's still paired (or the next one), else the first here, else the
// first paired.
void IslandWindow::choosePhone(bool next){
    std::vector<const SharePeer*> phones;for(auto& p:content_.nearby)if(p.paired&&p.phone)phones.push_back(&p);
    if(phones.empty()){content_.phonePage.clear();return;}
    auto at=std::find_if(phones.begin(),phones.end(),[&](auto* p){return p->id==content_.phonePage;});
    if(next&&at!=phones.end()){content_.phonePage=(at+1==phones.end()?phones.front():*(at+1))->id;return;}
    if(at!=phones.end())return;
    auto here=std::find_if(phones.begin(),phones.end(),[](auto* p){return p->online;});content_.phonePage=(here!=phones.end()?*here:phones.front())->id;
}
// Asks the Phone page's phone for its readings (one question at a time; a lost one is given up after 6 s).
void IslandWindow::askPhoneLive(){
    if(!share_||content_.phonePage.empty())return;const double now=seconds();
    auto it=std::find_if(content_.nearby.begin(),content_.nearby.end(),[&](auto& p){return p.id==content_.phonePage;});
    if(it==content_.nearby.end()||!it->online||it->revision<6)return;
    if(phoneAsking_->load()&&now-phoneAskedAt_<6)return;phoneAsking_->store(true);phoneAskedAt_=now;share_->queryPhone(content_.phonePage);
}
// 0.23: the focus clock, told to paired phones (island 0.23 on the phone: its lock screen and widget follow it). Sent when
// it starts, changes or stops; a phone that arrives while it runs hears it then.
void IslandWindow::syncPhoneFocus(){
    if(!share_)return;const auto& f=content_.focus;const double now=seconds();
    const int shown=int(std::lround(f.displayed(now)));
    std::wstring key=std::to_wstring(int(f.mode))+L"|"+std::to_wstring(f.running)+L"|"+std::to_wstring(f.finished)+L"|"+std::to_wstring(int(std::lround(f.duration)))+(f.running?L"":L"|"+std::to_wstring(shown));
    if(key!=focusKey_){focusKey_=key;focusTold_.clear();}
    // Nothing to tell until it has run: a phone isn't told about a clock that never started.
    if(!f.running&&!focusShared_)return;
    std::vector<uint8_t> state{uint8_t(int(f.mode)),uint8_t(f.running?1:0),uint8_t(f.finished?1:0)};
    auto f64=[&](double v){uint64_t u=0;std::memcpy(&u,&v,8);for(int i=0;i<8;++i)state.push_back(uint8_t(u>>(8*i)));};f64(f.displayed(now));f64(f.duration);
    std::string name;{const std::wstring& w=content_.shareName;const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);name.resize(size_t(std::max(n,0)));if(n>0)WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),name.data(),n,nullptr,nullptr);}
    for(int i=0;i<4;++i)state.push_back(uint8_t(name.size()>>(8*i)));state.insert(state.end(),name.begin(),name.end());
    for(auto& p:content_.nearby)if(p.paired&&p.phone&&p.online&&p.revision>=6&&!focusTold_.count(p.id)){focusTold_.insert(p.id);share_->tellPhoneFocus(p.id,state);}
    focusShared_=f.running;
}
// 0.24: this screen on a phone: a card that says so (with Stop) and the screen dot while it lasts; a word when it ends.
void IslandWindow::mirrorMessage(WPARAM w,LPARAM l){
    std::unique_ptr<std::wstring> phone(reinterpret_cast<std::wstring*>(l));
    if(w==1&&phone){mirroring_=*phone;updatePrivacy();
        if(renderer_){content_.notice={};content_.notice.kind=22;content_.notice.app=L"Showing this screen on "+mirroring_;content_.notice.detail=L"Its taps and typing reach this PC";
            content_.pinned=false;{const Activity a{ActivityKind::Notification,"mirror",74,22.,2.4,8.};if(!holdCard(a)){events_.publish(a,seconds());transition(IslandState::Notification);presentActivity();alertSplash();}}}
        store_.log("Info","mirror_screen_started");return;}
    if(w==0){const std::wstring was=mirroring_;mirroring_.clear();updatePrivacy();
        if(state_==IslandState::Notification&&content_.notice.kind==22){events_.dismiss(seconds());content_.activity.clear();transition(IslandState::Compact);}
        if(!was.empty())shareCard(16,L"Stopped showing this screen",was,{},3);store_.log("Info","mirror_screen_ended");}
}
// Whether the card showing waits for someone's answer: pairing (14), files offered (15), music handed off (17), the
// pairing code (20), a phone's message with actions or a call (19). Such a card isn't folded away by the pointer.
bool IslandWindow::decisionShowing()const{
    if(state_!=IslandState::Notification||!events_.active())return false;const auto& n=content_.notice;
    return n.kind==14||n.kind==15||n.kind==17||n.kind==20||n.kind==21||(n.kind==19&&!n.actions.empty());
}
// 0.21: pairing with another PC's code (it shows on that PC's island: Shelf › Nearby › Pair with a code). The bar takes
// the eight letters and digits in glass cells; the last one pairs at once, and both PCs then show the same six digits.
void IslandWindow::openPairCode(){
    openCommand();auto& c=content_.command;c.pairCode=true;c.results.clear();c.icons.clear();
    motion_.commandHeight=commandIslandHeight(0);animate();refresh();store_.log("Info","pair_code_opened");
}
// Replying to a phone's notification from the island: the command bar, as a reply box.
void IslandWindow::openReply(const std::string& peer,const std::string& key,int action,const std::wstring& to){
    openCommand();auto& c=content_.command;c.reply=true;c.replyTo=to;c.replyPeer=peer;c.replyKey=key;c.replyAction=action;c.results.clear();c.icons.clear();
    motion_.commandHeight=commandIslandHeight(0);animate();refresh();store_.log("Info","phone_reply_opened");
}
}
