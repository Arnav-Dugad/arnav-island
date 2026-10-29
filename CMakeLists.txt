cmake_minimum_required(VERSION 3.24)
project(ArnavIsland VERSION 0.28.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_library(nexus_core INTERFACE)
target_include_directories(nexus_core INTERFACE src)
add_executable(nexus_tests tests/CoreTests.cpp)
target_link_libraries(nexus_tests PRIVATE nexus_core)
add_test(NAME core COMMAND nexus_tests)
if(WIN32)
  enable_language(RC)
  add_executable(ArnavIsland WIN32 src/App/Main.cpp src/App/resources.rc src/Composition/Renderer.cpp src/Composition/Details.cpp src/Composition/GlassBackdrop.cpp src/Settings/SettingsWindow.cpp src/Island/IslandWindow.cpp src/Island/IslandRemote.cpp src/Island/IslandHotspot.cpp src/Mirror/ScreenCapture.cpp src/Mirror/VideoCodec.cpp src/Mirror/MirrorHost.cpp src/Mirror/MirrorWindow.cpp src/Mirror/PhoneMic.cpp src/Media/BrowserPage.cpp src/Island/IslandSettings.cpp src/Island/IslandMedia.cpp src/Composition/Activity.cpp src/Audio/LoopbackAnalyzer.cpp src/Audio/SessionMixer.cpp src/Hardware/BrightnessProvider.cpp src/Hardware/BatteryProvider.cpp src/Hardware/BluetoothProvider.cpp src/Hardware/Platform.cpp src/Composition/Devices.cpp src/Hardware/SystemProvider.cpp src/Audio/AudioProvider.cpp src/Media/MediaProvider.cpp src/Media/BrowserTabs.cpp src/Productivity/Clipboard.cpp src/Productivity/Privacy.cpp src/Productivity/CommandService.cpp src/Island/IslandProductivity.cpp src/Media/LyricsService.cpp src/Composition/NowPlaying.cpp src/Composition/Command.cpp src/Capture/CaptureOverlay.cpp src/Capture/Ocr.cpp src/Island/IslandCapture.cpp src/Productivity/WeatherService.cpp src/Productivity/UpdateService.cpp src/Productivity/SiteIcons.cpp src/Composition/Sky.cpp src/Productivity/ShareService.cpp src/Productivity/ShareRelay.cpp src/Productivity/QrCode.cpp src/Island/IslandShare.cpp src/Media/MusicLibrary.cpp src/Media/IslandPlayer.cpp src/Island/IslandMusic.cpp src/Island/IslandAccess.cpp src/Island/IslandLive.cpp src/Composition/Live.cpp src/Productivity/ShareLive.cpp src/Mirror/PhoneCamera.cpp src/Island/IslandCamera.cpp src/Setup/Install.cpp src/Setup/Uninstall.cpp)
  target_include_directories(ArnavIsland PRIVATE src/App)
  target_link_libraries(ArnavIsland PRIVATE nexus_core dcomp d3d11 dxgi d2d1 dwrite dwmapi ole32 uuid user32 gdi32 shell32 powrprof psapi wlanapi comctl32 runtimeobject windowscodecs uxtheme shcore iphlpapi advapi32 coremessaging version wbemuuid setupapi bluetoothapis oleaut32 winhttp crypt32 pdh ws2_32 bcrypt mfplat mfuuid shlwapi winmm uiautomationcore wintrust strmiids)
  target_compile_definitions(ArnavIsland PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MINGW)
    # Some recent MinGW distributions append a manifest object in their GCC
    # specs. Override only that endfile entry locally so our manifest is unique.
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpspecs OUTPUT_VARIABLE gcc_specs)
    string(REGEX MATCH "\\*endfile:\n([^\n]+)" endfile_match "${gcc_specs}")
    set(endfile_spec "${CMAKE_MATCH_1}")
    if(endfile_spec MATCHES "default-manifest")
      string(REPLACE "%{!shared:%:if-exists(default-manifest.o%s)}" "" endfile_spec "${endfile_spec}")
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/nexus-manifest.specs" "*endfile:\n${endfile_spec}\n")
      target_link_options(ArnavIsland PRIVATE "-specs=${CMAKE_CURRENT_BINARY_DIR}/nexus-manifest.specs")
      set(nexus_manifest_specs "-specs=${CMAKE_CURRENT_BINARY_DIR}/nexus-manifest.specs")
    endif()
    target_link_options(ArnavIsland PRIVATE -municode -static -static-libgcc -static-libstdc++)
    target_link_options(nexus_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  if(MSVC)
    target_link_options(ArnavIsland PRIVATE /MANIFEST:NO)
  endif()
  add_executable(nexus_provider_tests tests/ProviderTests.cpp src/Hardware/SystemProvider.cpp src/Audio/AudioProvider.cpp src/Media/MediaProvider.cpp src/Media/BrowserTabs.cpp src/Audio/SessionMixer.cpp src/Audio/LoopbackAnalyzer.cpp src/Hardware/BrightnessProvider.cpp src/Hardware/BatteryProvider.cpp src/Hardware/BluetoothProvider.cpp src/Hardware/Platform.cpp src/Productivity/Clipboard.cpp src/Productivity/Privacy.cpp src/Productivity/CommandService.cpp)
  target_link_libraries(nexus_provider_tests PRIVATE nexus_core ole32 uuid user32 shell32 runtimeobject windowscodecs shcore iphlpapi advapi32 version wbemuuid oleaut32 setupapi bluetoothapis dwmapi winhttp pdh)
  target_compile_definitions(nexus_provider_tests PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MINGW)
    target_link_options(nexus_provider_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME provider_lifecycle COMMAND nexus_provider_tests)
  set_tests_properties(provider_lifecycle PROPERTIES TIMEOUT 20)
endif()

add_executable(dashboard_tests tests/DashboardTests.cpp)
target_link_libraries(dashboard_tests PRIVATE nexus_core)
if(MINGW)
  target_link_options(dashboard_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME dashboard_models COMMAND dashboard_tests)

if(WIN32)
  add_executable(share_tests tests/ShareTests.cpp src/Productivity/ShareService.cpp src/Productivity/ShareRelay.cpp src/Productivity/QrCode.cpp)
  target_link_libraries(share_tests PRIVATE nexus_core ws2_32 bcrypt crypt32 winhttp iphlpapi)
  target_compile_definitions(share_tests PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MINGW)
    target_link_options(share_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME share_loopback COMMAND share_tests)
  # The PC's side of the protocol for Arnav Island for Android's interop test (not a test itself).
  # Tests only: the browser page reader against an isolated browser window.
  add_executable(browser_probe tests/BrowserProbe.cpp src/Media/BrowserPage.cpp)
  target_include_directories(browser_probe PRIVATE src)
  target_link_libraries(browser_probe PRIVATE ole32 oleaut32 uuid shell32)
  target_compile_definitions(browser_probe PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  add_executable(share_peer tests/SharePeer.cpp src/Productivity/ShareService.cpp src/Productivity/ShareRelay.cpp src/Mirror/ScreenCapture.cpp src/Mirror/VideoCodec.cpp src/Mirror/MirrorHost.cpp src/Mirror/MirrorWindow.cpp)
  target_link_libraries(share_peer PRIVATE nexus_core ws2_32 bcrypt crypt32 winhttp iphlpapi d3d11 dxgi mfplat mfuuid ole32 oleaut32 uuid strmiids d2d1 dwrite dwmapi)
  # 0.27: the made-up pictures it answers with (app icons, slides, a copied image).
  target_link_libraries(share_peer PRIVATE windowscodecs)
  target_compile_definitions(share_peer PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MINGW)
    target_link_options(share_peer PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  set_tests_properties(share_loopback PROPERTIES TIMEOUT 120)
endif()

add_executable(phase_tests tests/PhaseTests.cpp)
target_link_libraries(phase_tests PRIVATE nexus_core)
target_compile_definitions(phase_tests PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00 NEXUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(MINGW)
  target_link_options(phase_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME phase_models COMMAND phase_tests)

# 0.27 (revision 10): a phone's live activities as read, and its microphone's codecs (Windows' AAC encoder feeds the decoder).
if(WIN32)
  add_executable(live_tests tests/LiveTests.cpp src/Productivity/ShareLive.cpp src/Mirror/MirrorHost.cpp src/Mirror/VideoCodec.cpp src/Mirror/ScreenCapture.cpp)
  target_link_libraries(live_tests PRIVATE nexus_core d3d11 dxgi mfplat mfuuid ole32 oleaut32 uuid strmiids)
  target_compile_definitions(live_tests PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MINGW)
    target_link_options(live_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME live_and_microphone COMMAND live_tests)
endif()

# 0.27 (revision 10): the media source behind "Arnav Island Phone Camera" (Windows 11's virtual cameras load it into the Frame
# Server; the island registers it for its user and copies it where that service can read it). Built beside the island.
if(WIN32)
  add_library(ArnavIslandCamera SHARED src/Camera/CameraSource.cpp)
  target_include_directories(ArnavIslandCamera PRIVATE src)
  target_link_libraries(ArnavIslandCamera PRIVATE mfplat mfuuid ole32 uuid)
  target_compile_definitions(ArnavIslandCamera PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  set_target_properties(ArnavIslandCamera PROPERTIES PREFIX "")
  if(MINGW)
    target_link_options(ArnavIslandCamera PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_dependencies(ArnavIsland ArnavIslandCamera)
  # 0.27.1: the camera end to end, as an app sees it (a probe, not a test: it needs the camera set up for the PC, the island closed).
  add_executable(camera_probe tests/CameraProbe.cpp src/Mirror/PhoneCamera.cpp)
  target_include_directories(camera_probe PRIVATE src)
  target_link_libraries(camera_probe PRIVATE nexus_core d3d11 dxgi mfplat mf mfreadwrite mfuuid ole32 uuid shell32 advapi32)
  target_compile_definitions(camera_probe PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MINGW)
    target_link_options(camera_probe PRIVATE -static -static-libgcc -static-libstdc++ -municode)
  endif()
  add_dependencies(camera_probe ArnavIslandCamera)
endif()

# 0.28: the installer, Arnav-Island-Setup-<version>.exe: the release's files carried as resources (Setup/Payload.h), in the
# order of payloadNames (Setup/InstallPlan.h). They're the build's own exe and camera, so scripts/package.ps1 signs those
# first and builds this again: the setup carries the signed ones, then is signed itself. The version comes from Version.h.
if(WIN32)
  file(READ "${CMAKE_SOURCE_DIR}/src/App/Version.h" nexus_version_header)
  string(REGEX MATCH "appVersion\\[\\]=L\"([^\"]+)\"" nexus_version_match "${nexus_version_header}")
  set(nexus_version_text "${CMAKE_MATCH_1}")
  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[a-z]+\\.([0-9]+))?" nexus_version_parts "${nexus_version_text}")
  set(nexus_version_build "${CMAKE_MATCH_5}")
  if(nexus_version_build STREQUAL "")
    set(nexus_version_build 0)
  endif()
  set(nexus_version_numbers "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},${nexus_version_build}")
  if(NOT nexus_version_text OR NOT nexus_version_parts)
    message(FATAL_ERROR "Couldn't read the version from src/App/Version.h")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/App/Version.h")
  set(setup_payload "${CMAKE_CURRENT_BINARY_DIR}/setup-payload.rc")
  file(GENERATE OUTPUT "${setup_payload}" CONTENT "#include <windows.h>
#include \"Setup/Payload.h\"
1 RT_MANIFEST \"${CMAKE_SOURCE_DIR}/src/Setup/setup.manifest\"
101 ICON \"${CMAKE_SOURCE_DIR}/src/App/ArnavIsland.ico\"
PAYLOAD_EXE RCDATA \"$<TARGET_FILE:ArnavIsland>\"
PAYLOAD_CAMERA RCDATA \"$<TARGET_FILE:ArnavIslandCamera>\"
PAYLOAD_LICENSE RCDATA \"${CMAKE_SOURCE_DIR}/LICENSE\"
PAYLOAD_NOTICES RCDATA \"${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md\"
PAYLOAD_QUICK_START RCDATA \"${CMAKE_SOURCE_DIR}/docs/QUICK_START.md\"
1 VERSIONINFO
FILEVERSION ${nexus_version_numbers}
PRODUCTVERSION ${nexus_version_numbers}
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
 BLOCK \"StringFileInfo\"
 BEGIN
  BLOCK \"040904b0\"
  BEGIN
   VALUE \"CompanyName\", \"Arnav Dugad\"
   VALUE \"FileDescription\", \"Arnav Island Setup\"
   VALUE \"FileVersion\", \"${nexus_version_text}\"
   VALUE \"InternalName\", \"ArnavIslandSetup\"
   VALUE \"OriginalFilename\", \"Arnav-Island-Setup-${nexus_version_text}.exe\"
   VALUE \"ProductName\", \"Arnav Island\"
   VALUE \"ProductVersion\", \"${nexus_version_text}\"
   VALUE \"LegalCopyright\", \"2026 Arnav Island contributors\"
  END
 END
 BLOCK \"VarFileInfo\"
 BEGIN
  VALUE \"Translation\", 0x409, 1200
 END
END
")
  # Rebuilt whenever a payload file changes, signing included (Makefiles take no generator expressions here, so the
  # binaries are named where a single-configuration build puts them).
  set(setup_binaries "")
  if(NOT CMAKE_CONFIGURATION_TYPES)
    set(setup_binaries "${CMAKE_CURRENT_BINARY_DIR}/ArnavIsland.exe;${CMAKE_CURRENT_BINARY_DIR}/ArnavIslandCamera.dll;")
  endif()
  set_source_files_properties("${setup_payload}" PROPERTIES GENERATED TRUE OBJECT_DEPENDS "${setup_binaries}${CMAKE_SOURCE_DIR}/LICENSE;${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md;${CMAKE_SOURCE_DIR}/docs/QUICK_START.md;${CMAKE_SOURCE_DIR}/src/Setup/setup.manifest;${CMAKE_SOURCE_DIR}/src/Setup/Payload.h")
  add_executable(ArnavIslandSetup WIN32 src/Setup/Setup.cpp src/Setup/Install.cpp "${setup_payload}")
  target_include_directories(ArnavIslandSetup PRIVATE src src/App)
  target_link_libraries(ArnavIslandSetup PRIVATE d2d1 dwrite windowscodecs dwmapi ole32 uuid user32 gdi32 shell32 advapi32)
  target_compile_definitions(ArnavIslandSetup PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  add_dependencies(ArnavIslandSetup ArnavIsland ArnavIslandCamera)
  if(MINGW)
    if(nexus_manifest_specs)
      target_link_options(ArnavIslandSetup PRIVATE "${nexus_manifest_specs}")
    endif()
    target_link_options(ArnavIslandSetup PRIVATE -municode -static -static-libgcc -static-libstdc++ -s)
  endif()
  if(MSVC)
    target_link_options(ArnavIslandSetup PRIVATE /MANIFEST:NO)
  endif()
  # Where to install and what the setup writes, from made-up registry values (no Windows calls).
  add_executable(setup_tests tests/SetupTests.cpp)
  target_link_libraries(setup_tests PRIVATE nexus_core)
  if(MINGW)
    target_link_options(setup_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME setup_plan COMMAND setup_tests)
endif()
