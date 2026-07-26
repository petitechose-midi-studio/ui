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
#include <ms/ui/widget/KeyValueSparkline.hpp>

namespace ms::ui {

static constexpr size_t KEY_VALUE_ROW_TEXT_CAPACITY = 48;

struct KeyValueRow {
    const char* key = "";
    const char* value = "";
    // Optional compact facts kept visible beside a sparkline (for example
    // Rate · Reach · destination count in a source registry).
    const char* detail = "";
    const char* icon = "";
    const lv_font_t* iconFont = nullptr;
    uint32_t iconColor = 0;
    KeyValueSparkline sparkline{};
};

/**
 * Allocation-free scratch populated only for a row entering the visible
 * VirtualList window. This keeps large logical lists virtual instead of
 * retaining one text/sparkline cache per item.
 */
struct KeyValueRowBuffer {
    std::array<char, KEY_VALUE_ROW_TEXT_CAPACITY> key{};
    std::array<char, KEY_VALUE_ROW_TEXT_CAPACITY> value{};
    std::array<char, KEY_VALUE_ROW_TEXT_CAPACITY> detail{};
    std::array<char, KEY_VALUE_ROW_TEXT_CAPACITY> icon{};
    const lv_font_t* iconFont = nullptr;
    uint32_t iconColor = 0;
    KeyValueSparkline sparkline{};
};

using KeyValueRowProvider = void (*)(
    void* context,
    int index,
    KeyValueRowBuffer& out
);

struct VirtualListKeyValueOverlayProps {
    const char* title = "";
    const char* meta = "";
    const KeyValueRow* rows = nullptr;
    KeyValueRowProvider rowProvider = nullptr;
    void* rowProviderContext = nullptr;
    int rowCount = 0;
    int selectedIndex = 0;
    // Keep the default quiet detail grammar. Decision surfaces can opt out so
    // every visible fact stays readable while focus is still carried by the
    // selected-row background and active value color.
    bool dimUnselected = true;
    // Dense source-registry layout: smaller gutters plus a dedicated facts
    // column, while the default overlay geometry remains unchanged.
    bool compactFacts = false;
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
    static constexpr size_t TEXT_CACHE_SIZE = KEY_VALUE_ROW_TEXT_CAPACITY;
    static constexpr int MAX_PROVIDER_ROWS = 4096;

    struct TextCache {
        char text[TEXT_CACHE_SIZE] = {};
    };

    struct RowCache {
        TextCache key;
        TextCache value;
        TextCache detail;
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
        lv_obj_t* detailLabel = nullptr;
        lv_obj_t* sparklineSurface = nullptr;
        bool highlighted = false;
        bool highlightStyleApplied = false;
        bool dimUnselected = true;
        int boundIndex = -1;
        TextCache iconCache;
        TextCache keyCache;
        TextCache valueCache;
        TextCache detailCache;
        const lv_font_t* iconFont = nullptr;
        uint32_t iconColor = 0;
        bool sparklineVisible = false;
        KeyValueSparkline sparkline{};
        KeyValueSparklineMarker marker{};
    };

    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(lv_obj_t* container, int slotIndex);
    void applyCompactLayout(SlotWidgets& widgets);
    void applyHighlightStyle(SlotWidgets& widgets, bool isSelected);
    void applySparkline(SlotWidgets& widgets, const RowCache& row);
    void serviceSparklineMarkers();
    void refreshSparklineMarkerTimer();
    static void onSparklineDrawEvent(lv_event_t* event);
    static void onSparklineMarkerTimer(lv_timer_t* timer);
    const RowCache& materializeProviderRow(int index);
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
    KeyValueRowBuffer provider_buffer_{};
    RowCache provider_row_{};

    KeyValueRowProvider row_provider_ = nullptr;
    void* row_provider_context_ = nullptr;

    uint32_t last_data_revision_ = 0;
    int last_row_count_ = 0;
    int row_count_ = 0;
    bool dim_unselected_ = true;
    bool compact_facts_ = false;
    bool visible_ = false;
    lv_timer_t* marker_timer_ = nullptr;
};

}  // namespace ms::ui
