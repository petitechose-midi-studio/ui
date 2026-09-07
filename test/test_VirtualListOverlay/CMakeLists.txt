cmake_minimum_required(VERSION 3.29)
project(virtual_list_overlay_test LANGUAGES C CXX)

set(LVGL_DIR "" CACHE PATH "LVGL source checkout used by the application")
set(OC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../open-control" CACHE PATH "Open Control repositories")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_DIR}" lvgl)
add_executable(virtual_list_overlay test_main.cpp
    ../../src/ms/ui/component/VirtualListOverlay.cpp
    ../../src/ms/ui/component/LayoutOverlay.cpp
    ../../src/ms/ui/widget/VirtualListSelectorOverlay.cpp
    ../../src/ms/ui/widget/VirtualListKeyValueOverlay.cpp
    ../../src/ms/ui/widget/MenuListView.cpp
    ../../src/ms/ui/widget/TextOverflow.cpp
    "${OC_ROOT}/ui-lvgl-components/src/widget/Label.cpp"
    "${OC_ROOT}/ui-lvgl-components/src/widget/VirtualList.cpp")
target_compile_features(virtual_list_overlay PRIVATE cxx_std_17)
target_include_directories(virtual_list_overlay PRIVATE ../../src
    "${OC_ROOT}/ui-lvgl/src" "${OC_ROOT}/ui-lvgl-components/include"
    "${OC_ROOT}/framework/src")
target_link_libraries(virtual_list_overlay PRIVATE lvgl::lvgl)
target_compile_definitions(virtual_list_overlay PRIVATE _GLIBCXX_ASSERTIONS)
if(MSVC)
    target_compile_options(virtual_list_overlay PRIVATE /UNDEBUG)
else()
    target_compile_options(virtual_list_overlay PRIVATE -UNDEBUG)
endif()
enable_testing()
add_test(NAME virtual_list_overlay COMMAND virtual_list_overlay)
