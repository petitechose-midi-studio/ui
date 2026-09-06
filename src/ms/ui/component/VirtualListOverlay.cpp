#include "VirtualListOverlay.hpp"

#include <cstring>

#include <config/PlatformCompat.hpp>
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

FLASHMEM VirtualListOverlay::VirtualListOverlay(lv_obj_t* parent)
    : overlay_(parent) {
    overlay_.showHeader(true);
    overlay_.showFooter(false);

    createHeader();
    createList();

    hide();
}

FLASHMEM void VirtualListOverlay::createHeader() {
    lv_obj_set_flex_flow(overlay_.header(), LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        overlay_.header(),
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
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

    meta_icon_label_ = lv_label_create(header_row_);
    // Generated icon fonts may report a wider advance than the visible
    // glyph. Keep header geometry semantic and bounded on the 320 px screen.
    lv_obj_set_width(meta_icon_label_, 14);
    lv_obj_set_style_text_align(
        meta_icon_label_, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT
    );
    lv_label_set_long_mode(meta_icon_label_, LV_LABEL_LONG_CLIP);
    lv_label_set_text(meta_icon_label_, "");
    lv_obj_add_flag(meta_icon_label_, LV_OBJ_FLAG_HIDDEN);

    meta_label_ = lv_label_create(header_row_);
    lv_obj_set_width(meta_label_, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(meta_label_, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(meta_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(meta_label_, "");
    if (fonts.inter_13_medium) {
        lv_obj_set_style_text_font(meta_label_, fonts.inter_13_medium, LV_STATE_DEFAULT);
    }
    style::apply(meta_label_).textColor(base_theme::color::TEXT_SECONDARY);

    breadcrumb_label_ = lv_label_create(overlay_.header());
    lv_obj_set_width(breadcrumb_label_, LV_PCT(100));
    lv_obj_set_style_pad_left(
        breadcrumb_label_, HEADER_PAD_H, LV_STATE_DEFAULT
    );
    lv_obj_set_style_pad_right(
        breadcrumb_label_, HEADER_PAD_H, LV_STATE_DEFAULT
    );
    lv_label_set_long_mode(breadcrumb_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(breadcrumb_label_, "");
    if (fonts.inter_13_medium) {
        lv_obj_set_style_text_font(
            breadcrumb_label_, fonts.inter_13_medium, LV_STATE_DEFAULT
        );
    }
    style::apply(breadcrumb_label_).textColor(
        base_theme::color::TEXT_SECONDARY
    );
    lv_obj_set_style_text_opa(
        breadcrumb_label_, LV_OPA_70, LV_STATE_DEFAULT
    );
    lv_obj_add_flag(breadcrumb_label_, LV_OBJ_FLAG_HIDDEN);
}

FLASHMEM void VirtualListOverlay::createList() {
    list_ = std::make_unique<widget::VirtualList>(overlay_.content());
    list_->visibleCount(5)
        .itemHeight(32)
        .scrollMode(widget::ScrollMode::CenterLocked);
}

FLASHMEM void VirtualListOverlay::configureList(int visibleCount, int itemHeight) {
    if (!list_) return;
    list_->visibleCount(visibleCount).itemHeight(itemHeight);
}

FLASHMEM void VirtualListOverlay::setTitle(const char* text) {
    setTextIfChanged(title_label_, title_cache_, text);
}

FLASHMEM void VirtualListOverlay::setMeta(const char* text) {
    setTextIfChanged(meta_label_, meta_cache_, text);
}

FLASHMEM void VirtualListOverlay::setBreadcrumb(const char* text) {
    const char* value = text ? text : "";
    setTextIfChanged(breadcrumb_label_, breadcrumb_cache_, value);
    if (!breadcrumb_label_) return;
    if (value[0] == '\0') {
        lv_obj_add_flag(breadcrumb_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(breadcrumb_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

FLASHMEM void VirtualListOverlay::setMetaIcon(
    const char* text,
    const lv_font_t* font,
    uint32_t color
) {
    if (!meta_icon_label_) return;
    const char* icon = text ? text : "";
    setTextIfChanged(meta_icon_label_, meta_icon_cache_, icon);
    if (icon[0] == '\0' || font == nullptr) {
        lv_obj_add_flag(meta_icon_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (meta_icon_font_ != font) {
        lv_obj_set_style_text_font(
            meta_icon_label_, font, LV_STATE_DEFAULT
        );
        meta_icon_font_ = font;
    }
    if (meta_icon_color_ != color) {
        lv_obj_set_style_text_color(
            meta_icon_label_, lv_color_hex(color), LV_STATE_DEFAULT
        );
        meta_icon_color_ = color;
    }
    lv_obj_clear_flag(meta_icon_label_, LV_OBJ_FLAG_HIDDEN);
}

FLASHMEM void VirtualListOverlay::setTextIfChanged(
    lv_obj_t* label,
    TextCache& cache,
    const char* text
) {
    if (!label) return;

    const char* source = text ? text : "";
    std::array<char, TEXT_CACHE_SIZE> next{};
    std::strncpy(next.data(), source, next.size() - 1U);
    next.back() = '\0';
    if (std::strncmp(cache.data(), next.data(), cache.size()) == 0) {
        return;
    }

    cache = next;
    lv_label_set_text(label, cache.data());
}

FLASHMEM void VirtualListOverlay::setTextColors(
    uint32_t primary,
    uint32_t secondary
) {
    if (!title_label_ || !meta_label_) return;
    if (text_colors_applied_ && primary_text_color_ == primary &&
        secondary_text_color_ == secondary) {
        return;
    }

    lv_obj_set_style_text_color(
        title_label_, lv_color_hex(primary), LV_STATE_DEFAULT
    );
    lv_obj_set_style_text_color(
        meta_label_, lv_color_hex(secondary), LV_STATE_DEFAULT
    );
    primary_text_color_ = primary;
    secondary_text_color_ = secondary;
    text_colors_applied_ = true;
}

FLASHMEM void VirtualListOverlay::setContentVisible(bool visible) {
    if (content_visible_ == visible) return;
    content_visible_ = visible;
    auto* listElement = list_ ? list_->getElement() : nullptr;
    if (visible) {
        if (header_row_) lv_obj_clear_flag(header_row_, LV_OBJ_FLAG_HIDDEN);
        if (listElement) lv_obj_clear_flag(listElement, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (header_row_) lv_obj_add_flag(header_row_, LV_OBJ_FLAG_HIDDEN);
    if (breadcrumb_label_) {
        lv_obj_add_flag(breadcrumb_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (listElement) lv_obj_add_flag(listElement, LV_OBJ_FLAG_HIDDEN);
}

FLASHMEM void VirtualListOverlay::show() {
    if (overlay_.isVisible()) return;

    // The presentation registry may already have revealed the retained root.
    // Bind and settle its children while hidden: no intermediate geometry is
    // displayed, and LVGL need not invalidate every row during the first layout.
    overlay_.hide();
    if (list_ && content_visible_) list_->show();
    lv_obj_update_layout(overlay_.getElement());
    overlay_.show();
}

FLASHMEM void VirtualListOverlay::hide() {
    if (!overlay_.isVisible()) return;

    overlay_.hide();
    if (list_) list_->hide();
}

}  // namespace ms::ui
