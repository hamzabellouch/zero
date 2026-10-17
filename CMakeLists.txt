cmake_minimum_required(VERSION 3.16)
project(Zero VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Platform Sources
# ==============================================================================
set(CORE_SOURCES
    src/platform/platform_defs.h
    src/platform/app_scanner.h
    src/platform/process_engine.h
    src/platform/process_engine.cpp
    src/platform/leftover_cleaner.h
)

if(WIN32)
    set(PLATFORM_SOURCES
        src/platform/windows/win_platform.cpp
        src/app_manager.cpp
        src/process_manager.cpp
        src/cleaner.cpp
    )
    # Win32 Native Standalone Executable
    add_executable(Zero WIN32
        src/main.cpp
        src/app_manager.cpp
        src/cleaner.cpp
        src/process_manager.cpp
        src/ui_scan_dlg.cpp
        src/ui_leftovers_dlg.cpp
        src/app.rc
    )
    target_link_libraries(Zero PRIVATE
        comctl32 comdlg32 shell32 shlwapi ole32 dwmapi uxtheme gdiplus psapi version iphlpapi ws2_32
    )
    if(MSVC)
        target_compile_options(Zero PRIVATE /MT /W3 /O2 /permissive-)
    endif()

elseif(APPLE)
    set(PLATFORM_SOURCES
        src/platform/macos/mac_app_scanner.cpp
        src/platform/macos/mac_process_engine.cpp
        src/platform/macos/mac_cleaner.cpp
    )
    find_library(LIB_LIBPROC proc)
    find_library(FW_COCOA Cocoa)
    find_library(FW_IOKIT IOKit)

elseif(UNIX)
    set(PLATFORM_SOURCES
        src/platform/linux/linux_app_scanner.cpp
        src/platform/linux/linux_process_engine.cpp
        src/platform/linux/linux_cleaner.cpp
    )
endif()

# ==============================================================================
# Cross-Platform Qt 6 / Qt 5 Target (Linux, macOS, Windows)
# ==============================================================================
find_package(Qt6 COMPONENTS Widgets Core Gui QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets Core Gui QUIET)
endif()

if(Qt6_FOUND OR Qt5_FOUND)
    message(STATUS "Found Qt: Building Cross-Platform Zero UI")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    # Cross-Platform Qt UI Sources
    file(GLOB_RECURSE QT_SOURCES src/ui/qt/*.cpp src/ui/qt/*.h)

    if(QT_SOURCES)
        add_executable(Zero-CrossPlatform
            ${CORE_SOURCES}
            ${PLATFORM_SOURCES}
            ${QT_SOURCES}
        )

        if(Qt6_FOUND)
            target_link_libraries(Zero-CrossPlatform PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)
        else()
            target_link_libraries(Zero-CrossPlatform PRIVATE Qt5::Widgets Qt5::Core Qt5::Gui)
        endif()

        if(APPLE)
            target_link_libraries(Zero-CrossPlatform PRIVATE ${LIB_LIBPROC} ${FW_COCOA} ${FW_IOKIT})
            set_target_properties(Zero-CrossPlatform PROPERTIES
                OUTPUT_NAME "Zero"
                MACOSX_BUNDLE TRUE
                MACOSX_BUNDLE_BUNDLE_NAME "Zero"
                MACOSX_BUNDLE_BUNDLE_IDENTIFIER "com.zero.uninstaller"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "0.0.1"
            )
        elseif(UNIX)
            set_target_properties(Zero-CrossPlatform PROPERTIES
                OUTPUT_NAME "Zero"
            )
        endif()

        install(TARGETS Zero-CrossPlatform
            BUNDLE DESTINATION .
            RUNTIME DESTINATION bin
        )
    endif()
endif()

# ==============================================================================
# Packaging Configuration (AppImage for Linux, DMG for macOS, Portable for Win)
# ==============================================================================
set(CPACK_PACKAGE_NAME "Zero")
set(CPACK_PACKAGE_VENDOR "Zero")
set(CPACK_PACKAGE_VERSION "0.0.1")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast, lightweight standalone uninstaller and leftover cleaner.")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
