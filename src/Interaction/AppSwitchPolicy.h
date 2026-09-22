#pragma once
namespace nexus {
struct AppSwitchPolicy {
    static bool collapse(bool enabled,bool pinned,bool dragging,bool dropping) {
        return enabled&&!pinned&&!dragging&&!dropping;
    }
    static bool fullscreen(long left,long top,long right,long bottom,
                           long monitorLeft,long monitorTop,long monitorRight,long monitorBottom) {
        return right>left&&bottom>top&&left<=monitorLeft&&top<=monitorTop&&right>=monitorRight&&bottom>=monitorBottom;
    }
};
}
