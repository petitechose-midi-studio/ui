#pragma once

/**
 * @file VirtualListKeyValueOverlay.hpp
 * @brief Stateless key/value overlay using VirtualList (render(props))
 */

#include <array>
#include <cstddef>
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
    static constexpr int VISIBLE_SLOTS = 5;
    static constexpr int MAX_ROWS = 16;
    static constexpr size_t TEXT_CACHE_SIZE = 48;

    struct TextCache {
        char text[TEXT_CACHE_SIZE] = {};
    };

    struct RowCache {
        TextCache key;
        TextCache value;
    };

    struct SlotWidgets {
        bool created = false;
        lv_obj_t* keyLabel = nullptr;
        lv_obj_t* valueLabel = nullptr;
        bool highlighted = false;
        bool highlightStyleApplied = false;
        int boundIndex = -1;
        TextCache keyCache;
        TextCache valueCache;
    };

    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(oc::ui::lvgl::widget::VirtualSlot& slot, int slotIndex);
    void applyHighlightStyle(SlotWidgets& widgets, bool isSelected);
    void syncRows(const VirtualListKeyValueOverlayProps& props,
                  std::array<int, MAX_ROWS>& dirtyIndices,
                  int& dirtyCount);
    void invalidateDirtyRows(const std::array<int, MAX_ROWS>& dirtyIndices, int dirtyCount);
    static bool copyTextIfChanged(TextCache& cache, const char* text);
    static void setLabelTextIfChanged(lv_obj_t* label, TextCache& cache, const char* text);

    VirtualListOverlay overlay_;
    std::array<SlotWidgets, VISIBLE_SLOTS> slot_widgets_{};
    std::array<RowCache, MAX_ROWS> rows_{};

    uint32_t last_data_revision_ = 0;
    int last_row_count_ = 0;
    int row_count_ = 0;
};

}  // namespace ms::ui
