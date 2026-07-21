cmake_minimum_required(VERSION 3.20)

project(omniclicker
    VERSION 1.0.0
    DESCRIPTION "Native Linux autoclicker inspired by OP Auto Clicker"
    HOMEPAGE_URL "https://github.com/limonyx/omniclicker"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets DBus Network)
find_package(X11 REQUIRED)
find_package(KF6GlobalAccel QUIET)
find_library(XTST_LIBRARY Xtst REQUIRED)

set(OMNICLICKER_SOURCES
    src/main.cpp
    src/Types.cpp
    src/Config.cpp
    src/ClickEngine.cpp
    src/MainWindow.cpp
    src/backends/BackendFactory.cpp
    src/backends/UInputBackend.cpp
    src/backends/X11Backend.cpp
    src/hotkeys/EvdevHotkeyBackend.cpp
    src/hotkeys/GnomeHotkeyBackend.cpp
    src/hotkeys/HyprlandHotkeyBackend.cpp
    src/hotkeys/SwayHotkeyBackend.cpp
    src/hotkeys/Hotkey.cpp
    src/hotkeys/HotkeyBackendFactory.cpp
    src/hotkeys/HotkeyCaptureEdit.cpp
    src/hotkeys/HotkeyManager.cpp
    src/hotkeys/PortalHotkeyBackend.cpp
    src/hotkeys/HotkeyCaptureEdit.h
    src/hotkeys/X11HotkeyBackend.cpp
    src/hotkeys/PortalHotkeyBackend.h
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

if(KF6GlobalAccel_FOUND)
    target_link_libraries(omniclicker PRIVATE KF6::GlobalAccel)
    target_compile_definitions(omniclicker PRIVATE HAVE_KGLOBALACCEL=1)
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
install(FILES data/io.github.omniclicker.desktop DESTINATION share/applications)
install(FILES data/io.github.omniclicker.metainfo.xml DESTINATION share/metainfo)
install(FILES data/io.github.omniclicker.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES data/io.github.omniclicker.png DESTINATION share/pixmaps)
