cmake_minimum_required(VERSION 3.16)
project(desktop_temp_notif CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBNOTIFY libnotify)

    set(SOURCES
        src/main.cpp
        src/config.cpp
        platform/linux/linux_sensor_reader.cpp
        platform/linux/linux_notifier.cpp
    )

    add_executable(desktop_temp_notif ${SOURCES})
    target_include_directories(desktop_temp_notif PRIVATE src platform/linux)

    if(LIBNOTIFY_FOUND)
        message(STATUS "libnotify found — using C API")
        target_compile_definitions(desktop_temp_notif PRIVATE USE_LIBNOTIFY)
        target_include_directories(desktop_temp_notif PRIVATE ${LIBNOTIFY_INCLUDE_DIRS})
        target_link_libraries(desktop_temp_notif PRIVATE ${LIBNOTIFY_LIBRARIES})
        target_compile_options(desktop_temp_notif PRIVATE ${LIBNOTIFY_CFLAGS_OTHER})
    else()
        message(STATUS "libnotify not found — using notify-send fallback")
    endif()

elseif(WIN32)
    set(SOURCES
        src/main.cpp
        src/config.cpp
        platform/windows/windows_sensor_reader.cpp
        platform/windows/windows_notifier.cpp
    )

    add_executable(desktop_temp_notif ${SOURCES})
    target_include_directories(desktop_temp_notif PRIVATE src platform/windows)

    # WinRT headers live under <SDK>/Include/<version>/winrt/
    # CMake's find_package(WindowsSDK) can locate this, but a robust fallback
    # is to let the compiler find them via the VS-injected include paths.
    # The pragma comment(lib, ...) directives in the .cpp files handle linking.

    target_compile_options(desktop_temp_notif PRIVATE /utf-8)
    target_compile_definitions(desktop_temp_notif PRIVATE
        UNICODE _UNICODE
        _WIN32_WINNT=0x0A00   # Windows 10
        WINVER=0x0A00
    )

    # Link WMI + WinRT + shell
    target_link_libraries(desktop_temp_notif PRIVATE
        wbemuuid
        ole32
        oleaut32
        runtimeobject
        shlwapi
    )
endif()
