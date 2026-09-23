#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
namespace nexus {
enum class ActivityKind { Volume,Media,Power,Device,Notification,Timer,Download,Brightness };
struct Activity {
    ActivityKind kind; std::string key; int priority=0; double value=0;
    double minimum=0.6, maximum=3; bool persistent=false;
};
class EventOrchestrator {
    std::optional<Activity> active_;
    std::vector<Activity> pending_;
    double shown_=0,updated_=0;
public:
    const std::optional<Activity>& active() const {return active_;}
    size_t depth() const {return pending_.size();}
    void publish(Activity a,double now) {
        if(active_&&active_->key==a.key) {active_=std::move(a);updated_=now;return;}
        if(!active_||a.priority>active_->priority) {
            if(active_&&active_->persistent) pending_.push_back(*active_);
            active_=std::move(a);shown_=updated_=now;return;
        }
        auto it=std::find_if(pending_.begin(),pending_.end(),[&](auto& p){return p.key==a.key;});
        if(it!=pending_.end()) *it=std::move(a);
        else if(pending_.size()<64) pending_.push_back(std::move(a));
    }
    bool tick(double now) {
        if(active_&&(active_->persistent||now-shown_<active_->minimum||now-updated_<active_->maximum))return false;
        if(!active_&&pending_.empty())return false;
        active_.reset();
        if(!pending_.empty()) {
            auto it=std::max_element(pending_.begin(),pending_.end(),[](auto&a,auto&b){return a.priority<b.priority;});
            active_=std::move(*it);pending_.erase(it);shown_=updated_=now;
        }
        return true;
    }
    void dismiss(double now) { active_.reset();tick(now); }
};
}
