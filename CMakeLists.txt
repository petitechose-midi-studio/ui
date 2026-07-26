cmake_minimum_required(VERSION 3.29)

project(midi_studio_ui LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
