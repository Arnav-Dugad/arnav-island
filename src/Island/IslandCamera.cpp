#include "Island/IslandWindow.h"
#include "Mirror/PhoneCamera.h"

// 0.27 (revision 10): a phone's camera (mirror kind 3) is also a camera any app can pick, "Arnav Island Phone Camera", on
// Windows 11. The stream's session opens it (MirrorHost's receiveScreen); the island says how it went.
namespace nexus {
void IslandWindow::cameraSetup(){mirror::cameraOutlets()=[]{return std::unique_ptr<mirror::CameraOutlet>(new mirror::PhoneCamera());};}
void IslandWindow::cameraMessage(WPARAM w,LPARAM l){
    std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(l));const std::wstring said=text?*text:std::wstring();
    if(w==1){phoneCard(L"Your phone is a camera now",L"Pick \u201cArnav Island Phone Camera\u201d in any app  \u00b7  the window is its preview",said,nullptr,6);store_.log("Info","phone_camera_on");}
    else if(w==2){phoneCard(L"The camera shows in a window only",said,L"Arnav Island",nullptr,6);store_.log("Warning","phone_camera_window_only");}
    else if(w==3){phoneCard(L"Your phone\u2019s camera is set up",said,L"Arnav Island Phone Camera",nullptr,6);store_.log("Info","phone_camera_set_up");}
    else if(w==4){phoneCard(L"The camera isn\u2019t set up",said,L"Arnav Island",nullptr,6);store_.log("Warning","phone_camera_not_set_up");}
    else store_.log("Info","phone_camera_off");
}
}
