#pragma once

/**
 * @file VirtualListOverlay.hpp
 * @brief Bitwig-like modal overlay shell with header + VirtualList
 */

#include <array>
#include <cstddef>
#include <cstdint>
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
    void setBreadcrumb(const char* text);
    void setMetaIcon(
        const char* text,
        const lv_font_t* font,
        uint32_t color
    );
    void setTextColors(uint32_t primary, uint32_t secondary);
    void setBackdropOpacity(lv_opa_t opacity) {
        overlay_.setBackdropOpacity(opacity);
    }
    void setContentVisible(bool visible);

    // Convenience
    void configureList(int visibleCount, int itemHeight);

    // IComponent
    void show() override;
    void hide() override;
    bool isVisible() const override { return overlay_.isVisible(); }
    lv_obj_t* getElement() const override { return overlay_.getElement(); }

private:
    static constexpr std::size_t TEXT_CACHE_SIZE = 48;
    using TextCache = std::array<char, TEXT_CACHE_SIZE>;

    void createHeader();
    void createList();
    static void setTextIfChanged(
        lv_obj_t* label,
        TextCache& cache,
        const char* text
    );

    LayoutOverlay overlay_;

    lv_obj_t* header_row_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* meta_icon_label_ = nullptr;
    lv_obj_t* meta_label_ = nullptr;
    lv_obj_t* breadcrumb_label_ = nullptr;
    TextCache title_cache_{};
    TextCache meta_icon_cache_{};
    TextCache meta_cache_{};
    TextCache breadcrumb_cache_{};
    const lv_font_t* meta_icon_font_ = nullptr;
    uint32_t meta_icon_color_ = 0U;
    uint32_t primary_text_color_ = 0;
    uint32_t secondary_text_color_ = 0;
    bool text_colors_applied_ = false;
    bool content_visible_ = true;

    std::unique_ptr<oc::ui::lvgl::widget::VirtualList> list_;
};

}  // namespace ms::ui
