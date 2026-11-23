cmake_minimum_required(VERSION 3.22)

project(
    VivoGuard
    VERSION 1.0.0
    DESCRIPTION "Native system monitor for Linux, Windows and macOS"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(VIVOGUARD_BUILD_UI "Build the Qt desktop application" ON)
option(VIVOGUARD_BUILD_TESTS "Build core smoke tests" ON)
option(VIVOGUARD_LOCAL_ARTWORK "Bundle the optional locally supplied illustration" OFF)
option(VIVOGUARD_DEPLOY_QT "Stage Qt runtime files on Windows and macOS during install" OFF)
set(VIVOGUARD_QT_RUNTIME_DIR "" CACHE PATH "Runtime libraries for a local Qt SDK")

add_library(vivoguard_core STATIC
    src/core/AlertEngine.cpp
    src/core/HardwareProfile.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(vivoguard_core PRIVATE src/core/Cleaner.cpp src/core/IntelGpuUsage.cpp src/core/NvmlMonitor.cpp
        src/core/ProcessController.cpp src/core/SystemSampler.cpp)
    target_link_libraries(vivoguard_core PUBLIC ${CMAKE_DL_LIBS})
elseif(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
    target_sources(vivoguard_core PRIVATE src/core/NvmlMonitor.cpp
        src/core/PortableSampler.cpp src/core/WindowsSampler.cpp src/core/MonitorOnlyActions.cpp)
    target_link_libraries(vivoguard_core PRIVATE advapi32 d3d12 dxgi iphlpapi pdh psapi ws2_32)
elseif(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    target_sources(vivoguard_core PRIVATE src/core/MacSampler.mm
        src/core/PortableSampler.cpp src/core/MonitorOnlyActions.cpp)
    set_target_properties(vivoguard_core PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED YES)
    set_source_files_properties(src/core/MacSampler.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    find_library(MONITOR_IOKIT IOKit REQUIRED)
    find_library(MONITOR_FOUNDATION Foundation REQUIRED)
    find_library(MONITOR_METAL Metal REQUIRED)
    target_link_libraries(vivoguard_core PRIVATE ${MONITOR_IOKIT} ${MONITOR_FOUNDATION} ${MONITOR_METAL})
else()
    message(FATAL_ERROR "Monitor Sistem supports desktop Linux, Windows and macOS.")
endif()

if(MSVC)
    add_compile_options(/utf-8)
    target_compile_options(vivoguard_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(vivoguard_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
target_include_directories(vivoguard_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(vivoguard-probe src/probe_main.cpp)
set_target_properties(vivoguard-probe PROPERTIES OUTPUT_NAME "sys-monit-anime-probe")
target_link_libraries(vivoguard-probe PRIVATE vivoguard_core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(vivoguard-browser-host src/browser_host_main.cpp)
    set_target_properties(vivoguard-browser-host PROPERTIES OUTPUT_NAME "sys-monit-anime-browser-host")
    install(TARGETS vivoguard-browser-host RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(VIVOGUARD_BUILD_UI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Svg)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    endif()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    qt_add_executable(vivoguard WIN32 MACOSX_BUNDLE
        assets/resources.qrc
        src/main.cpp
        src/ui/MainWindow.cpp
        src/ui/TelemetryWorker.cpp
        src/ui/widgets/FanWidget.cpp
        src/ui/widgets/GaugeCard.cpp
        src/ui/widgets/TimelineChart.cpp
    )
    set_target_properties(vivoguard PROPERTIES OUTPUT_NAME "sys-monit-anime")

    target_link_libraries(vivoguard PRIVATE
        vivoguard_core
        Qt6::Core
        Qt6::Widgets
        Qt6::Svg
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(vivoguard PRIVATE Qt6::DBus)
    elseif(APPLE)
        set_target_properties(vivoguard PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.monitorsistem.desktop"
            MACOSX_BUNDLE_BUNDLE_NAME "Monitor Sistem"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    endif()
    if(VIVOGUARD_LOCAL_ARTWORK)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/reference-artwork.qrc")
            message(FATAL_ERROR "The optional local artwork was not supplied with this source tree.")
        endif()
        target_sources(vivoguard PRIVATE assets/reference-artwork.qrc)
    endif()
    target_compile_definitions(vivoguard PRIVATE
        VIVOGUARD_VERSION="${PROJECT_VERSION}"
    )
    if(VIVOGUARD_QT_RUNTIME_DIR)
        set_target_properties(vivoguard PROPERTIES
            INSTALL_RPATH "${VIVOGUARD_QT_RUNTIME_DIR}")
    endif()

    install(TARGETS vivoguard
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION .
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        install(FILES assets/vivoguard.svg DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
            RENAME "sys-monit-anime.svg")
        install(FILES assets/vivoguard.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
            RENAME "sys-monit-anime.png")
        install(FILES packaging/sys-monit-anime.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    elseif(VIVOGUARD_DEPLOY_QT AND NOT CMAKE_CROSSCOMPILING)
        qt_generate_deploy_app_script(TARGET vivoguard OUTPUT_SCRIPT deploy_script)
        install(SCRIPT "${deploy_script}")
    endif()
endif()

install(TARGETS vivoguard-probe
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(FILES LICENSE THIRD_PARTY_NOTICES.md assets/fonts/Oxanium-OFL.txt
    DESTINATION "${CMAKE_INSTALL_DOCDIR}")

if(VIVOGUARD_BUILD_TESTS)
    enable_testing()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(vivoguard-core-smoke tests/core_smoke.cpp)
    else()
        add_executable(vivoguard-core-smoke tests/portable_smoke.cpp)
    endif()
    target_link_libraries(vivoguard-core-smoke PRIVATE vivoguard_core)
    add_test(NAME core-smoke COMMAND vivoguard-core-smoke)
    if(VIVOGUARD_BUILD_UI)
        add_executable(vivoguard-worker-smoke tests/worker_smoke.cpp)
        target_link_libraries(vivoguard-worker-smoke PRIVATE Qt6::Core)
        add_test(NAME worker-smoke COMMAND vivoguard-worker-smoke "$<TARGET_FILE:vivoguard>")
        set_tests_properties(worker-smoke PROPERTIES TIMEOUT 25)
    endif()
endif()

set(CPACK_PACKAGE_NAME "sys-monit-anime")
set(CPACK_PACKAGE_VENDOR "Monitor Sistem")
set(CPACK_PACKAGE_CONTACT "Monitor Sistem")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6 (>= 6.4), libqt6dbus6 (>= 6.4), libqt6svg6 (>= 6.4)")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB")
else()
    set(CPACK_GENERATOR "ZIP")
endif()
include(CPack)
