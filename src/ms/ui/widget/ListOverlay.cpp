#include "ListOverlay.hpp"

#include <cstring>

#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <ms/ui/font/CoreFonts.hpp>

namespace ms::ui {

using namespace oc::ui::lvgl;
namespace style = oc::ui::lvgl::style;

ListOverlay::ListOverlay(lv_obj_t* parent) : parent_(parent) {
    createOverlay();
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    ui_created_ = true;
    visible_ = false;
}

ListOverlay::~ListOverlay() { cleanup(); }

void ListOverlay::setTitle(const std::string& title) {
    title_ = title;

    if (ui_created_ && title_label_) {
        if (title_.empty()) {
            lv_obj_add_flag(title_label_->getElement(), LV_OBJ_FLAG_HIDDEN);
        } else {
            title_label_->setText(title_);
            lv_obj_clear_flag(title_label_->getElement(), LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ListOverlay::setItems(const std::vector<std::string>& items) {
    if (items_ == items) { return; }

    items_ = items;

    if (selected_index_ >= static_cast<int>(items_.size())) {
        selected_index_ = items_.empty() ? 0 : items_.size() - 1;
    }
    if (selected_index_ < 0) { selected_index_ = 0; }

    if (ui_created_ && list_) {
        destroyList();
        populateList();
        updateHighlight();
        scrollToSelected();
    }
}

void ListOverlay::setSelectedIndex(int index) {
    if (items_.empty()) {
        selected_index_ = 0;
        return;
    }

    int size = static_cast<int>(items_.size());
    int prev_index = selected_index_;
    index = ((index % size) + size) % size;

    if (selected_index_ != index) {
        selected_index_ = index;

        if (ui_created_ && visible_) {
            updateHighlight();
            int delta = index - prev_index;
            bool isWrap = (delta > 1) || (delta < -1);
            scrollToSelected(!isWrap);
        }
    }
}

void ListOverlay::show() {
    if (overlay_) {
        lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
        visible_ = true;
        updateHighlight();
        scrollToSelected();
    }
}

void ListOverlay::hide() {
    if (overlay_) {
        lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
        visible_ = false;
    }
}

bool ListOverlay::isVisible() const { return visible_ && ui_created_; }

int ListOverlay::getSelectedIndex() const { return items_.empty() ? -1 : selected_index_; }

int ListOverlay::getItemCount() const { return items_.size(); }

lv_obj_t* ListOverlay::getButton(size_t index) const {
    return (index < buttons_.size()) ? buttons_[index] : nullptr;
}

oc::ui::lvgl::Label* ListOverlay::getLabel(size_t index) const {
    return (index < labels_.size()) ? labels_[index].get() : nullptr;
}

void ListOverlay::setItemFont(size_t index, const lv_font_t* font) {
    if (!font) return;

    auto* label = getLabel(index);
    if (label) {
        label->font(font);
    }
}

void ListOverlay::removeLabel(size_t index) {
    if (index < labels_.size() && labels_[index]) {
        // Manually delete LVGL container since ownsLvglObjects is false
        if (labels_[index]->getElement()) {
            lv_obj_delete(labels_[index]->getElement());
        }
        labels_[index].reset();
    }
}

void ListOverlay::createOverlay() {
    overlay_ = lv_obj_create(parent_);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_FLOATING);
    style::apply(overlay_)
        .fullSize()
        .bgColor(base_theme::color::BACKGROUND, base_theme::opacity::OPA_90)
        .noScroll();
    lv_obj_align(overlay_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(overlay_, 0, LV_STATE_DEFAULT);

    container_ = lv_obj_create(overlay_);
    style::apply(container_).fullSize().transparent().noScroll();
    lv_obj_align(container_, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(container_, base_theme::layout::ROW_GAP_MD, LV_STATE_DEFAULT);

    createTitleLabel();
    createList();
    populateList();
}

void ListOverlay::createTitleLabel() {
    // Use framework Label widget with auto-scroll for overflow text
    title_label_ = std::make_unique<Label>(container_);
    title_label_->alignment(LV_TEXT_ALIGN_CENTER)
                 .color(base_theme::color::TEXT_PRIMARY)
                 .ownsLvglObjects(false);

    lv_obj_t* elem = title_label_->getElement();
    lv_obj_set_width(elem, lv_pct(100) - base_theme::layout::MARGIN_LG);
    lv_obj_set_style_margin_left(elem, base_theme::layout::MARGIN_MD, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_right(elem, base_theme::layout::MARGIN_MD, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_top(elem, base_theme::layout::MARGIN_MD, LV_STATE_DEFAULT);

    if (fonts.tempo_label) {
        title_label_->font(fonts.tempo_label);
    }

    if (title_.empty()) {
        lv_obj_add_flag(elem, LV_OBJ_FLAG_HIDDEN);
    } else {
        title_label_->setText(title_);
    }
}

void ListOverlay::createList() {
    list_ = lv_list_create(container_);
    lv_obj_set_size(list_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(list_, 1);
    lv_obj_set_style_bg_opa(list_, base_theme::opacity::OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(list_, base_theme::layout::LIST_PAD, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(list_, base_theme::layout::LIST_ITEM_GAP, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_left(list_, base_theme::layout::MARGIN_MD, LV_STATE_DEFAULT);
    lv_obj_set_style_margin_right(list_, base_theme::layout::MARGIN_MD, LV_STATE_DEFAULT);

    lv_obj_set_style_width(list_, base_theme::layout::SCROLLBAR_WIDTH, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list_, lv_color_hex(base_theme::color::INACTIVE_LIGHTER), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list_, base_theme::opacity::OPA_30, LV_PART_SCROLLBAR);

    lv_obj_add_event_cb(
        list_,
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
                lv_anim_t* anim = lv_event_get_scroll_anim(e);
                if (anim) { anim->duration = base_theme::animation::SCROLL_ANIM_MS; }
            }
        },
        LV_EVENT_SCROLL_BEGIN, nullptr);
}

void ListOverlay::populateList() {
    if (!list_) return;

    buttons_.clear();
    labels_.clear();
    previous_index_ = -1;  // Reset for optimized highlight tracking

    for (const auto& item : items_) {
        lv_obj_t* btn = lv_obj_create(list_);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_height(btn, LV_SIZE_CONTENT);

        lv_obj_set_style_bg_opa(btn, base_theme::opacity::OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn, base_theme::opacity::OPA_TRANSP, LV_STATE_CHECKED);

        lv_obj_set_style_pad_left(btn, base_theme::layout::PAD_BUTTON_H, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(btn, base_theme::layout::MARGIN_LG, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(btn, base_theme::layout::PAD_BUTTON_V, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(btn, base_theme::layout::PAD_BUTTON_V, LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(btn, base_theme::layout::MARGIN_MD, LV_STATE_DEFAULT);

        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(btn, 0, LV_STATE_DEFAULT);

        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Use framework Label widget with auto-scroll for overflow text
        // ownsLvglObjects(false) lets LVGL parent-child handle deletion
        auto label = std::make_unique<Label>(btn);
        label->flexGrow(true)
              .alignment(LV_TEXT_ALIGN_LEFT)
              .color(base_theme::color::INACTIVE_LIGHTER)
              .ownsLvglObjects(false);

        if (fonts.list_item_label) {
            label->font(fonts.list_item_label);
        }

        // Apply styles for focused state on the inner label element
        lv_obj_set_style_text_color(label->getLabel(), lv_color_hex(base_theme::color::TEXT_PRIMARY), LV_STATE_FOCUSED);

        label->setText(item);

        labels_.push_back(std::move(label));
        buttons_.push_back(btn);
    }

    updateHighlight();
}

static void applyStateRecursive(lv_obj_t* obj, lv_state_t state, bool apply) {
    if (!obj) return;

    if (apply) {
        lv_obj_add_state(obj, state);
    } else {
        lv_obj_clear_state(obj, state);
    }

    uint32_t child_count = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(obj, i);
        applyStateRecursive(child, state, apply);
    }
}

void ListOverlay::updateHighlight() {
    if (buttons_.empty() || selected_index_ < 0 ||
        selected_index_ >= static_cast<int>(buttons_.size())) {
        return;
    }

    // Optimization: Only update the items that changed (previous and current)
    // instead of iterating all buttons
    if (previous_index_ >= 0 && previous_index_ < static_cast<int>(buttons_.size()) &&
        previous_index_ != selected_index_) {
        applyStateRecursive(buttons_[previous_index_], LV_STATE_FOCUSED, false);
    }

    applyStateRecursive(buttons_[selected_index_], LV_STATE_FOCUSED, true);
    previous_index_ = selected_index_;
}

void ListOverlay::scrollToSelected(bool animate) {
    if (buttons_.empty() || selected_index_ < 0 ||
        selected_index_ >= static_cast<int>(buttons_.size()) || !list_) {
        return;
    }

    lv_obj_scroll_to_view(buttons_[selected_index_], animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void ListOverlay::destroyList() {
    // Labels have ownsLvglObjects(false), so they don't delete LVGL objects
    // LVGL parent-child handles cleanup when lv_obj_clean() is called
    labels_.clear();
    if (list_) {
        lv_obj_clean(list_);
    }
    buttons_.clear();
}

void ListOverlay::cleanup() {
    // Labels have ownsLvglObjects(false) - overlay deletion handles LVGL cleanup
    labels_.clear();
    title_label_.reset();
    if (overlay_) {
        lv_obj_delete(overlay_);
        overlay_ = nullptr;
        container_ = nullptr;
        list_ = nullptr;
    }
    buttons_.clear();
    ui_created_ = false;
    visible_ = false;
}

}  // namespace ms::ui
