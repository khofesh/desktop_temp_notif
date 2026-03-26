cmake_minimum_required(VERSION 3.16)
project(desktop_temp_notif CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBNOTIFY libnotify)

    # Detect libsensors (lm_sensors C API)
    find_path(LIBSENSORS_INCLUDE_DIR sensors/sensors.h)
    find_library(LIBSENSORS_LIBRARY NAMES sensors)
    if(LIBSENSORS_INCLUDE_DIR AND LIBSENSORS_LIBRARY)
        set(LIBSENSORS_FOUND TRUE)
        message(STATUS "libsensors found — using C API for sensor reading")
    else()
        set(LIBSENSORS_FOUND FALSE)
        message(STATUS "libsensors not found — using popen(sensors) fallback")
    endif()

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

    if(LIBSENSORS_FOUND)
        target_compile_definitions(desktop_temp_notif PRIVATE USE_LIBSENSORS)
        target_include_directories(desktop_temp_notif PRIVATE ${LIBSENSORS_INCLUDE_DIR})
        target_link_libraries(desktop_temp_notif PRIVATE ${LIBSENSORS_LIBRARY})
    endif()

elseif(WIN32)
    set(SOURCES
        src/main.cpp
        src/config.cpp
        platform/windows/windows_sensor_reader.cpp
        platform/windows/windows_notifier.cpp
        platform/windows/DesktopNotificationManagerCompat.cpp
    )

    add_executable(desktop_temp_notif ${SOURCES})
    target_include_directories(desktop_temp_notif PRIVATE src platform/windows)

    target_compile_options(desktop_temp_notif PRIVATE /utf-8)
    target_compile_definitions(desktop_temp_notif PRIVATE
        UNICODE _UNICODE
        _WIN32_WINNT=0x0A00   # Windows 10/11
        WINVER=0x0A00
    )

    # WinHTTP (sensor reader) + WinRT + COM (compat library)
    target_link_libraries(desktop_temp_notif PRIVATE
        winhttp
        runtimeobject
        ole32
        shlwapi
    )
endif()
