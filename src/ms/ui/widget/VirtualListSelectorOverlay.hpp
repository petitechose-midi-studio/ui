#pragma once

/**
 * @file VirtualListSelectorOverlay.hpp
 * @brief Stateless VirtualList selector overlay (render(props))
 */

#include <cstdint>
#include <vector>

#include <lvgl.h>

#include <oc/ui/lvgl/widget/VirtualList.hpp>

#include <ms/ui/component/VirtualListOverlay.hpp>

namespace ms::ui {

struct VirtualListSelectorOverlayProps {
    const char* title = "";
    const char* meta = "";
    const char* const* items = nullptr;
    int itemCount = 0;
    int selectedIndex = 0;
    bool showIndexColumn = true;
    bool visible = false;

    // Optional: bump when list labels/shape changes (lets render() skip invalidations).
    // 0 means "unknown" (render() will fall back to pointer/count checks).
    uint32_t dataRevision = 0;
};

class VirtualListSelectorOverlay {
public:
    explicit VirtualListSelectorOverlay(lv_obj_t* parent);
    ~VirtualListSelectorOverlay();

    VirtualListSelectorOverlay(const VirtualListSelectorOverlay&) = delete;
    VirtualListSelectorOverlay& operator=(const VirtualListSelectorOverlay&) = delete;

    void render(const VirtualListSelectorOverlayProps& props);

    lv_obj_t* getElement() const { return overlay_.getElement(); }

private:
    struct SlotWidgets {
        bool created = false;
        lv_obj_t* indexLabel = nullptr;
        lv_obj_t* label = nullptr;
    };

    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(oc::ui::lvgl::widget::VirtualSlot& slot, int slotIndex);
    void applyHighlightStyle(SlotWidgets& widgets, bool isSelected);

    VirtualListOverlay overlay_;
    std::vector<SlotWidgets> slot_widgets_;

    VirtualListSelectorOverlayProps current_props_{};

    const char* const* last_items_ = nullptr;
    int last_item_count_ = 0;
    bool last_show_index_column_ = true;
    uint32_t last_data_revision_ = 0;
};

}  // namespace ms::ui
