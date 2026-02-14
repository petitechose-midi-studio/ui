#include "VirtualListOverlay.hpp"

#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>

#include <ms/ui/font/CoreFonts.hpp>

namespace ms::ui {

using namespace oc::ui::lvgl;
namespace style = oc::ui::lvgl::style;

namespace {
constexpr int HEADER_PAD_H = base_theme::layout::SPACE_XL;   // 16
constexpr int HEADER_PAD_TOP = base_theme::layout::SPACE_MD; // 8
constexpr int HEADER_PAD_BOTTOM = base_theme::layout::SPACE_SM; // 4
constexpr int HEADER_COL_GAP = base_theme::layout::SPACE_MD; // 8
}

VirtualListOverlay::VirtualListOverlay(lv_obj_t* parent)
    : overlay_(parent) {
    overlay_.showHeader(true);
    overlay_.showFooter(false);

    createHeader();
    createList();

    hide();
}

void VirtualListOverlay::createHeader() {
    header_row_ = lv_obj_create(overlay_.header());
    lv_obj_set_size(header_row_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(header_row_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_bg_opa(header_row_, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(header_row_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(header_row_, HEADER_PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(header_row_, HEADER_PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(header_row_, HEADER_PAD_TOP, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(header_row_, HEADER_PAD_BOTTOM, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(header_row_, HEADER_COL_GAP, LV_STATE_DEFAULT);

    title_label_ = lv_label_create(header_row_);
    lv_obj_set_flex_grow(title_label_, 1);
    lv_label_set_long_mode(title_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(title_label_, "");
    if (fonts.inter_14_semibold) {
        lv_obj_set_style_text_font(title_label_, fonts.inter_14_semibold, LV_STATE_DEFAULT);
    }
    style::apply(title_label_).textColor(base_theme::color::TEXT_PRIMARY);

    meta_label_ = lv_label_create(header_row_);
    lv_obj_set_width(meta_label_, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(meta_label_, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(meta_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(meta_label_, "");
    if (fonts.inter_13_medium) {
        lv_obj_set_style_text_font(meta_label_, fonts.inter_13_medium, LV_STATE_DEFAULT);
    }
    style::apply(meta_label_).textColor(base_theme::color::TEXT_SECONDARY);
}

void VirtualListOverlay::createList() {
    list_ = std::make_unique<widget::VirtualList>(overlay_.content());
    list_->visibleCount(5)
        .itemHeight(32)
        .scrollMode(widget::ScrollMode::CenterLocked);
}

void VirtualListOverlay::configureList(int visibleCount, int itemHeight) {
    if (!list_) return;
    list_->visibleCount(visibleCount).itemHeight(itemHeight);
}

void VirtualListOverlay::setTitle(const char* text) {
    if (title_label_) lv_label_set_text(title_label_, text ? text : "");
}

void VirtualListOverlay::setMeta(const char* text) {
    if (meta_label_) lv_label_set_text(meta_label_, text ? text : "");
}

void VirtualListOverlay::show() {
    if (overlay_.isVisible()) return;

    overlay_.show();
    if (list_) list_->show();
}

void VirtualListOverlay::hide() {
    if (!overlay_.isVisible()) return;

    overlay_.hide();
    if (list_) list_->hide();
}

}  // namespace ms::ui
