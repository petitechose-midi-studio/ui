# Headless rendering tests use their own LVGL target/configuration, so a parent
# superbuild's production LVGL configuration cannot leak into the fixture.
enable_language(C)
set(MS_UI_OPEN_CONTROL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../open-control"
    CACHE PATH "OpenControl workspace for UI tests")
set(MS_UI_LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../core/.pio/libdeps/dev/lvgl"
    CACHE PATH "LVGL source checkout for UI tests (same revision as the product)")
foreach(required
        "${MS_UI_LVGL_DIR}/lvgl.h"
        "${MS_UI_OPEN_CONTROL_ROOT}/framework/src/config/PlatformCompat.hpp"
        "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl/src/oc/ui/lvgl/FontUtils.cpp"
        "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl-components/src/widget/VirtualList.cpp")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "UI test dependency missing: ${required}. Set MS_UI_LVGL_DIR and MS_UI_OPEN_CONTROL_ROOT.")
    endif()
endforeach()

# Match LVGL's upstream source discovery, without demos, SDL or asset loading.
file(GLOB_RECURSE MS_UI_TEST_LVGL_SOURCES CONFIGURE_DEPENDS
    "${MS_UI_LVGL_DIR}/src/*.c" "${MS_UI_LVGL_DIR}/src/*.cpp")
add_library(ms_ui_test_lvgl STATIC ${MS_UI_TEST_LVGL_SOURCES})
target_include_directories(ms_ui_test_lvgl PUBLIC "${MS_UI_LVGL_DIR}")
target_compile_definitions(ms_ui_test_lvgl PUBLIC
    LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test/test_VirtualListOverlay/lv_conf.h"
    LV_KCONFIG_IGNORE)
set_target_properties(ms_ui_test_lvgl PROPERTIES C_EXTENSIONS ON)

add_executable(test_VirtualListOverlay
    test/test_VirtualListOverlay/test_main.cpp
    src/ms/ui/component/LayoutOverlay.cpp
    src/ms/ui/component/VirtualListOverlay.cpp
    src/ms/ui/widget/ListOverlay.cpp
    src/ms/ui/widget/MenuListView.cpp
    src/ms/ui/widget/TextOverflow.cpp
    src/ms/ui/widget/VirtualListSelectorOverlay.cpp
    src/ms/ui/widget/VirtualListKeyValueOverlay.cpp
    "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl-components/src/widget/VirtualList.cpp"
    "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl-components/src/widget/Label.cpp"
    "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl/src/oc/ui/lvgl/FontUtils.cpp")
target_include_directories(test_VirtualListOverlay PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${MS_UI_OPEN_CONTROL_ROOT}/framework/src"
    "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl/src"
    "${MS_UI_OPEN_CONTROL_ROOT}/ui-lvgl-components/include")
target_link_libraries(test_VirtualListOverlay PRIVATE ms_ui_test_lvgl)
if(MSVC)
    target_compile_options(test_VirtualListOverlay PRIVATE /UNDEBUG)
else()
    target_compile_options(test_VirtualListOverlay PRIVATE -UNDEBUG)
endif()
add_test(NAME test_VirtualListOverlay COMMAND test_VirtualListOverlay)
set_tests_properties(test_VirtualListOverlay PROPERTIES TIMEOUT 30)
