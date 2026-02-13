#pragma once

/**
 * @file LayoutView.hpp
 * @brief Basic view scaffold: header + content slots
 *
 * Pure layout primitive (no state, no logic) used by views to get a consistent
 * structure:
 * - header: auto-height (typically a top bar)
 * - content: flex-grow (main view body)
 */

#include <lvgl.h>

namespace ms::ui {

/**
 * @brief View layout scaffold with header + content slots
 */
class LayoutView {
public:
    explicit LayoutView(lv_obj_t* parent);
    ~LayoutView();

    LayoutView(const LayoutView&) = delete;
    LayoutView& operator=(const LayoutView&) = delete;
    LayoutView(LayoutView&&) = delete;
    LayoutView& operator=(LayoutView&&) = delete;

    lv_obj_t* getElement() const { return container_; }
    lv_obj_t* header() const { return header_; }
    lv_obj_t* content() const { return content_; }

    void show() { if (container_) lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN); }
    void hide() { if (container_) lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); }

private:
    lv_obj_t* container_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* content_ = nullptr;
};

}  // namespace ms::ui
