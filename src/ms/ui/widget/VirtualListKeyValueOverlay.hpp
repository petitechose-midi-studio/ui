#pragma once

/**
 * @file VirtualListKeyValueOverlay.hpp
 * @brief Stateless key/value overlay using VirtualList (render(props))
 */

#include <string>
#include <vector>
#include <cstdint>

#include <lvgl.h>

#include <oc/ui/lvgl/widget/VirtualList.hpp>

#include <ms/ui/component/VirtualListOverlay.hpp>

namespace ms::ui {

struct KeyValueRow {
    const char* key = "";
    const char* value = "";
};

struct VirtualListKeyValueOverlayProps {
    const char* title = "";
    const char* meta = "";
    const KeyValueRow* rows = nullptr;
    int rowCount = 0;
    int selectedIndex = 0;
    bool visible = false;

    // Optional: bump when rows content changes (lets render() skip realloc/rebind).
    // 0 means "unknown".
    uint32_t dataRevision = 0;
};

class VirtualListKeyValueOverlay {
public:
    explicit VirtualListKeyValueOverlay(lv_obj_t* parent);
    ~VirtualListKeyValueOverlay();

    VirtualListKeyValueOverlay(const VirtualListKeyValueOverlay&) = delete;
    VirtualListKeyValueOverlay& operator=(const VirtualListKeyValueOverlay&) = delete;

    void render(const VirtualListKeyValueOverlayProps& props);

    lv_obj_t* getElement() const { return overlay_.getElement(); }

private:
    struct SlotWidgets {
        bool created = false;
        lv_obj_t* keyLabel = nullptr;
        lv_obj_t* valueLabel = nullptr;
    };

    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(oc::ui::lvgl::widget::VirtualSlot& slot, int slotIndex);
    void applyHighlightStyle(SlotWidgets& widgets, bool isSelected);

    VirtualListOverlay overlay_;
    std::vector<SlotWidgets> slot_widgets_;
    std::vector<std::pair<std::string, std::string>> rows_;
    VirtualListKeyValueOverlayProps current_props_{};

    uint32_t last_data_revision_ = 0;
    int last_row_count_ = 0;
};

}  // namespace ms::ui
