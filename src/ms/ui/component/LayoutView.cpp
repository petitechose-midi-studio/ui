#include "LayoutView.hpp"

#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>

namespace ms::ui {

namespace theme = oc::ui::lvgl::base_theme;
namespace style = oc::ui::lvgl::style;

LayoutView::LayoutView(lv_obj_t* parent) {
    if (!parent) return;

    container_ = lv_obj_create(parent);
    style::apply(container_).fullSize().pad(0).bgColor(theme::color::BACKGROUND).noScroll().noBorder();
    lv_obj_set_layout(container_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(container_, 0, LV_STATE_DEFAULT);

    header_ = lv_obj_create(container_);
    lv_obj_set_size(header_, LV_PCT(100), LV_SIZE_CONTENT);
    style::apply(header_).transparent().noScroll().pad(0).noBorder();

    content_ = lv_obj_create(container_);
    lv_obj_set_width(content_, LV_PCT(100));
    lv_obj_set_flex_grow(content_, 1);
    style::apply(content_).transparent().noScroll().pad(0).noBorder();
}

LayoutView::~LayoutView() {
    if (container_) {
        lv_obj_delete(container_);
        container_ = nullptr;
        header_ = nullptr;
        content_ = nullptr;
    }
}

}  // namespace ms::ui
