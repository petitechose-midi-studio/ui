cmake_minimum_required(VERSION 3.29)

project(midi_studio_ui LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MidiStudioUiSources.cmake")

# Consumers provide LVGL. Keep compilation in their final target while sharing
# one exact source inventory across firmware, SDL and WASM.
add_library(midi_studio_ui INTERFACE)
target_sources(midi_studio_ui INTERFACE ${MS_UI_SOURCES})
target_include_directories(midi_studio_ui
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(midi_studio_ui INTERFACE cxx_std_17)

if(BUILD_TESTING)
    add_executable(
        test_CurvePreviewGeometry
        test/test_CurvePreviewGeometry/test_main.cpp)
    target_include_directories(
        test_CurvePreviewGeometry
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(test_CurvePreviewGeometry PRIVATE /UNDEBUG)
    else()
        target_compile_options(test_CurvePreviewGeometry PRIVATE -UNDEBUG)
    endif()
    add_test(
        NAME test_CurvePreviewGeometry
        COMMAND test_CurvePreviewGeometry)
endif()
