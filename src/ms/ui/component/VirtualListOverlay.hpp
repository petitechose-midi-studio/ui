#pragma once

/**
 * @file VirtualListOverlay.hpp
 * @brief Bitwig-like modal overlay shell with header + VirtualList
 */

#include <memory>

#include <lvgl.h>

#include <oc/ui/lvgl/IComponent.hpp>
#include <oc/ui/lvgl/widget/VirtualList.hpp>

#include "LayoutOverlay.hpp"

namespace ms::ui {

class VirtualListOverlay : public oc::ui::lvgl::IComponent {
public:
    explicit VirtualListOverlay(lv_obj_t* parent);
    ~VirtualListOverlay() override = default;

    VirtualListOverlay(const VirtualListOverlay&) = delete;
    VirtualListOverlay& operator=(const VirtualListOverlay&) = delete;

    // Accessors
    lv_obj_t* headerRow() const { return header_row_; }
    oc::ui::lvgl::widget::VirtualList* list() const { return list_.get(); }

    void setTitle(const char* text);
    void setMeta(const char* text);

    // Convenience
    void configureList(int visibleCount, int itemHeight);

    // IComponent
    void show() override;
    void hide() override;
    bool isVisible() const override { return overlay_.isVisible(); }
    lv_obj_t* getElement() const override { return overlay_.getElement(); }

private:
    void createHeader();
    void createList();

    LayoutOverlay overlay_;

    lv_obj_t* header_row_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* meta_label_ = nullptr;

    std::unique_ptr<oc::ui::lvgl::widget::VirtualList> list_;
};

}  // namespace ms::ui
