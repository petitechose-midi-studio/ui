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

static constexpr size_t KEY_VALUE_SPARKLINE_SAMPLE_COUNT = 12;

struct KeyValueSparkline {
    bool enabled = false;
    uint8_t sampleCount = 0;
    std::array<uint8_t, KEY_VALUE_SPARKLINE_SAMPLE_COUNT> samples{};
};

struct KeyValueRow {
    const char* key = "";
    const char* value = "";
    const char* icon = "";
    const lv_font_t* iconFont = nullptr;
    uint32_t iconColor = 0;
    KeyValueSparkline sparkline{};
};

struct VirtualListKeyValueOverlayProps {
    const char* title = "";
    const char* meta = "";
    const KeyValueRow* rows = nullptr;
    int rowCount = 0;
    int selectedIndex = 0;
    // Keep the default quiet detail grammar. Decision surfaces can opt out so
    // every visible fact stays readable while focus is still carried by the
    // selected-row background and active value color.
    bool dimUnselected = true;
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
        TextCache icon;
        const lv_font_t* iconFont = nullptr;
        uint32_t iconColor = 0;
        KeyValueSparkline sparkline{};
    };

    struct SlotWidgets {
        bool created = false;
        lv_obj_t* iconLabel = nullptr;
        lv_obj_t* keyLabel = nullptr;
        lv_obj_t* valueLabel = nullptr;
        lv_obj_t* sparklineLine = nullptr;
        bool highlighted = false;
        bool highlightStyleApplied = false;
        bool dimUnselected = true;
        int boundIndex = -1;
        TextCache iconCache;
        TextCache keyCache;
        TextCache valueCache;
        const lv_font_t* iconFont = nullptr;
        uint32_t iconColor = 0;
        bool sparklineVisible = false;
        std::array<lv_point_precise_t, KEY_VALUE_SPARKLINE_SAMPLE_COUNT> sparklinePoints{};
    };

    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(lv_obj_t* container, int slotIndex);
    void applyHighlightStyle(SlotWidgets& widgets, bool isSelected);
    void applySparkline(SlotWidgets& widgets, const RowCache& row);
    void syncRows(const VirtualListKeyValueOverlayProps& props,
                  std::array<int, MAX_ROWS>& dirtyIndices,
                  int& dirtyCount);
    void invalidateDirtyRows(const std::array<int, MAX_ROWS>& dirtyIndices, int dirtyCount);
    static bool copyTextIfChanged(TextCache& cache, const char* text);
    static bool copySparklineIfChanged(KeyValueSparkline& cache, const KeyValueSparkline& next);
    static void setLabelTextIfChanged(lv_obj_t* label, TextCache& cache, const char* text);

    VirtualListOverlay overlay_;
    std::array<SlotWidgets, VISIBLE_SLOTS> slot_widgets_{};
    std::array<RowCache, MAX_ROWS> rows_{};

    uint32_t last_data_revision_ = 0;
    int last_row_count_ = 0;
    int row_count_ = 0;
    bool dim_unselected_ = true;
};

}  // namespace ms::ui
