cmake_minimum_required(VERSION 3.20)

project(omniclicker
    VERSION 1.2.2
    DESCRIPTION "Native Linux autoclicker inspired by OP Auto Clicker"
    HOMEPAGE_URL "https://github.com/limonyx/omniclicker"
    LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets DBus Network)
find_package(X11 REQUIRED)
find_package(KF6GlobalAccel QUIET)
find_package(KF6WindowSystem QUIET)
find_library(XTST_LIBRARY Xtst REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
endif()
find_package(LayerShellQt QUIET)

set(OMNICLICKER_WAYLAND_LAYER_SHELL_AVAILABLE FALSE)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8
   AND TARGET LayerShellQt::Interface
   AND TARGET PkgConfig::WAYLAND_CLIENT)
    set(OMNICLICKER_WAYLAND_LAYER_SHELL_AVAILABLE TRUE)
endif()

set(OMNICLICKER_SOURCES
    src/main.cpp
    src/Types.cpp
    src/Config.cpp
    src/ClickEngine.cpp
    src/MainWindow.cpp
    src/RunningOverlay.cpp
    src/RunningOverlay.h
    src/backends/BackendFactory.cpp
    src/backends/UInputBackend.cpp
    src/backends/X11Backend.cpp
    src/hotkeys/EvdevHotkeyBackend.cpp
    src/hotkeys/GnomeHotkeyBackend.cpp
    src/hotkeys/HyprlandHotkeyBackend.cpp
    src/hotkeys/NiriHotkeyBackend.cpp
    src/hotkeys/NiriHotkeyBackend.h
    src/hotkeys/SwayHotkeyBackend.cpp
    src/hotkeys/Hotkey.cpp
    src/hotkeys/HotkeyBackendFactory.cpp
    src/hotkeys/HotkeyCaptureEdit.cpp
    src/hotkeys/HotkeyManager.cpp
    src/hotkeys/PortalHotkeyBackend.cpp
    src/hotkeys/HotkeyCaptureEdit.h
    src/hotkeys/X11HotkeyBackend.cpp
    src/hotkeys/PortalHotkeyBackend.h
    data/tray-icons.qrc
)

if(KF6GlobalAccel_FOUND)
    list(APPEND OMNICLICKER_SOURCES
        src/hotkeys/KdeGlobalAccelBackend.cpp
        src/hotkeys/KdeGlobalAccelBackend.h
    )
endif()

add_executable(omniclicker ${OMNICLICKER_SOURCES})

target_include_directories(omniclicker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${X11_INCLUDE_DIR}
)

target_link_libraries(omniclicker PRIVATE
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network
    X11::X11
    ${XTST_LIBRARY}
)

# Empty X11 input shapes are required for reliable click-through behavior.
# Keep this optional so the rest of the application can still be built when
# the X Shape development library is not installed.
if(TARGET X11::Xext)
    target_link_libraries(omniclicker PRIVATE X11::Xext)
    target_compile_definitions(omniclicker PRIVATE HAVE_X11_SHAPE=1)
endif()

if(OMNICLICKER_WAYLAND_LAYER_SHELL_AVAILABLE)
    target_sources(omniclicker PRIVATE
        src/overlay/LayerShellWaylandBackend.cpp
        src/overlay/LayerShellWaylandBackend.h
        src/overlay/WaylandOverlayBackend.h
    )
    target_compile_definitions(omniclicker PRIVATE HAVE_WAYLAND_LAYER_SHELL=1)
    target_link_libraries(omniclicker PRIVATE LayerShellQt::Interface PkgConfig::WAYLAND_CLIENT)
endif()

if(KF6GlobalAccel_FOUND)
    target_link_libraries(omniclicker PRIVATE KF6::GlobalAccel)
    target_compile_definitions(omniclicker PRIVATE HAVE_KGLOBALACCEL=1)
endif()

# KWin's non-standard OSD window type is the supported way to place an X11
# overlay above fullscreen windows. Keep it optional for non-KDE builds.
if(KF6WindowSystem_FOUND)
    target_link_libraries(omniclicker PRIVATE KF6::WindowSystem)
    target_compile_definitions(omniclicker PRIVATE HAVE_KWINDOWSYSTEM=1)
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d")

target_compile_definitions(omniclicker PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    BUILD_TYPE="$<CONFIG>"
    GIT_HASH="${GIT_HASH}"
    BUILD_TIMESTAMP="${BUILD_TIMESTAMP}"
)

target_compile_options(omniclicker PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS omniclicker RUNTIME DESTINATION bin)
configure_file(data/io.github.omniclicker.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.omniclicker.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.omniclicker.desktop DESTINATION share/applications)
install(FILES data/io.github.omniclicker.metainfo.xml DESTINATION share/metainfo)
install(FILES data/io.github.omniclicker.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES data/omniclicker-active-tray-icon.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES data/io.github.omniclicker.png DESTINATION share/pixmaps)
install(FILES data/40-omniclicker-uinput.rules DESTINATION lib/udev/rules.d)
install(FILES data/omniclicker-uinput.conf DESTINATION lib/modules-load.d)

if(BUILD_TESTING)
    find_package(Qt6 6.4 QUIET COMPONENTS Test)
    if(TARGET Qt6::Test)
        add_executable(running_overlay_test
            tests/RunningOverlayTest.cpp
            src/RunningOverlay.cpp
            src/RunningOverlay.h
        )
        target_include_directories(running_overlay_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(running_overlay_test PRIVATE Qt6::Widgets Qt6::Test)
        if(TARGET X11::Xext)
            target_link_libraries(running_overlay_test PRIVATE X11::Xext)
            target_compile_definitions(running_overlay_test PRIVATE HAVE_X11_SHAPE=1)
        endif()
        if(OMNICLICKER_WAYLAND_LAYER_SHELL_AVAILABLE)
            target_sources(running_overlay_test PRIVATE
                src/overlay/LayerShellWaylandBackend.cpp
                src/overlay/LayerShellWaylandBackend.h
                src/overlay/WaylandOverlayBackend.h
            )
            target_compile_definitions(running_overlay_test PRIVATE HAVE_WAYLAND_LAYER_SHELL=1)
            target_link_libraries(running_overlay_test PRIVATE LayerShellQt::Interface PkgConfig::WAYLAND_CLIENT)
        endif()
        if(KF6WindowSystem_FOUND)
            target_link_libraries(running_overlay_test PRIVATE KF6::WindowSystem)
            target_compile_definitions(running_overlay_test PRIVATE HAVE_KWINDOWSYSTEM=1)
        endif()
        add_test(NAME running_overlay_state COMMAND running_overlay_test)
        set_tests_properties(running_overlay_state PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        add_executable(niri_hotkey_backend_test
            tests/NiriHotkeyBackendTest.cpp
            src/hotkeys/NiriHotkeyBackend.cpp
            src/hotkeys/NiriHotkeyBackend.h
            src/hotkeys/Hotkey.cpp
            src/hotkeys/Hotkey.h
        )
        target_include_directories(niri_hotkey_backend_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(niri_hotkey_backend_test PRIVATE Qt6::Gui Qt6::Test)
        add_test(NAME niri_hotkey_backend COMMAND niri_hotkey_backend_test)
    endif()
endif()
