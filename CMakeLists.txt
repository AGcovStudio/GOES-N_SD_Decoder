cmake_minimum_required(VERSION 3.21)
project(GOES_N_SD_Decoder VERSION 1.0.0 LANGUAGES C CXX)

# Single-configuration generators (MinGW Makefiles, Ninja): default to Release so the per-sample DSP is optimised.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()
find_package(Threads REQUIRED)

# ---- receive chain: demodulator, block sync, scan assembly, pictures (Qt Core/Gui only, no widgets) ----------------
add_library(goessd_core STATIC
    src/dsp.cpp src/dsp.h
    src/goessd.cpp src/goessd.h src/gvar_fc_lut.inc
    src/iqreader.cpp src/iqreader.h src/recordingtime.h
    src/runtime_logger.cpp src/runtime_logger.h)
target_include_directories(goessd_core PUBLIC src)
target_link_libraries(goessd_core PUBLIC Threads::Threads Qt6::Core Qt6::Gui)

# ---- the application -------------------------------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp src/mainwindow.cpp src/mainwindow.h
    src/sdworker.cpp src/sdworker.h
    src/constellationwidget.cpp src/constellationwidget.h
    src/spectrumview.cpp src/spectrumview.h
    src/appinfo.h src/qtpath.h
    resources/app.qrc)
if(WIN32)
    enable_language(RC)
    list(APPEND APP_SOURCES resources/app.rc)
endif()
qt_add_executable(GoesSdDecoder WIN32 ${APP_SOURCES})
set_target_properties(GoesSdDecoder PROPERTIES OUTPUT_NAME GOES-N_SD_Decoder)
target_link_libraries(GoesSdDecoder PRIVATE goessd_core Qt6::Core Qt6::Gui Qt6::Widgets)

# Command line, no window: goessd_cli <recording> [sample rate in Hz] [output folder] [--hard]
add_executable(GoesSdCli tests/goessd_cli.cpp)
target_link_libraries(GoesSdCli PRIVATE goessd_core)
set_target_properties(GoesSdCli PROPERTIES OUTPUT_NAME goessd_cli)

install(TARGETS GoesSdDecoder GoesSdCli RUNTIME DESTINATION .)
