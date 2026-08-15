#pragma once

#include <cstdint>

#include <lvgl.h>

#include <oc/ui/lvgl/theme/BaseTheme.hpp>

namespace ms::ui {

struct ListVisualTokens {
    uint32_t focusColor;
    uint32_t selectedSurfaceColor;
    uint32_t primaryTextColor;
    uint32_t secondaryTextColor;
    uint32_t disabledTextColor;
    uint32_t positiveTextColor;
    uint32_t warningTextColor;
    uint32_t destructiveTextColor;
    lv_opa_t selectedSurfaceOpacity;
    lv_coord_t selectedSurfaceRadius;
};

inline constexpr ListVisualTokens DEFAULT_LIST_VISUAL_TOKENS{
    oc::ui::lvgl::base_theme::color::ACTIVE,
    oc::ui::lvgl::base_theme::color::INACTIVE,
    oc::ui::lvgl::base_theme::color::TEXT_PRIMARY,
    oc::ui::lvgl::base_theme::color::TEXT_SECONDARY,
    oc::ui::lvgl::base_theme::color::INACTIVE_LIGHTER,
    oc::ui::lvgl::base_theme::color::MACRO_4_GREEN,
    oc::ui::lvgl::base_theme::color::MACRO_2_ORANGE,
    oc::ui::lvgl::base_theme::color::MACRO_1_RED,
    LV_OPA_40,
    2,
};

namespace detail {

constexpr lv_coord_t LIST_FOCUS_RAIL_WIDTH = 2;

inline const ListVisualTokens& resolveListVisualTokens(
    const ListVisualTokens* tokens
) {
    return tokens ? *tokens : DEFAULT_LIST_VISUAL_TOKENS;
}

inline void applyListFocusRail(
    lv_obj_t* row,
    bool focused,
    const ListVisualTokens& tokens
) {
    if (!row) return;

    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(
        row,
        focused ? LIST_FOCUS_RAIL_WIDTH : 0,
        LV_STATE_DEFAULT
    );
    lv_obj_set_style_border_color(
        row,
        lv_color_hex(tokens.focusColor),
        LV_STATE_DEFAULT
    );
    lv_obj_set_style_border_opa(
        row,
        focused ? LV_OPA_COVER : LV_OPA_TRANSP,
        LV_STATE_DEFAULT
    );
}

}  // namespace detail

}  // namespace ms::ui
