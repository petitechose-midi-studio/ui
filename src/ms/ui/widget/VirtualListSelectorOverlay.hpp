#pragma once

/**
 * @file VirtualListSelectorOverlay.hpp
 * @brief Stateless VirtualList selector overlay (render(props))
 */

#include <array>
#include <cstddef>
#include <cstdint>

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
    // Keep the default subdued selector grammar. Decision/detail surfaces can
    // opt out so every available item remains readable while focus is still
    // conveyed by the selected-row background and primary text.
    bool dimUnselected = true;
    // Stacked decision surfaces can opt into an opaque backdrop so the
    // previous overlay cannot leak text through the modal layer.
    lv_opa_t backdropOpacity = LayoutOverlay::DEFAULT_BACKDROP_OPACITY;
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
    static constexpr int VISIBLE_SLOTS = 5;
    static constexpr size_t TEXT_CACHE_SIZE = 48;

    struct TextCache {
        char text[TEXT_CACHE_SIZE] = {};
    };

    struct SlotWidgets {
        bool created = false;
        lv_obj_t* indexLabel = nullptr;
        lv_obj_t* label = nullptr;
        bool highlighted = false;
        bool highlightStyleApplied = false;
        bool dimUnselected = true;
        bool indexVisible = true;
        bool indexVisibilityApplied = false;
        int boundIndex = -1;
        TextCache indexCache;
        TextCache labelCache;
    };

    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(lv_obj_t* container, int slotIndex);
    void applyHighlightStyle(SlotWidgets& widgets, bool isSelected);
    static bool copyTextIfChanged(TextCache& cache, const char* text);
    static void setLabelTextIfChanged(lv_obj_t* label, TextCache& cache, const char* text);

    VirtualListOverlay overlay_;
    std::array<SlotWidgets, VISIBLE_SLOTS> slot_widgets_{};

    VirtualListSelectorOverlayProps current_props_{};

    const char* const* last_items_ = nullptr;
    int last_item_count_ = 0;
    bool last_show_index_column_ = true;
    bool last_dim_unselected_ = true;
    lv_opa_t last_backdrop_opacity_ = LayoutOverlay::DEFAULT_BACKDROP_OPACITY;
    uint32_t last_data_revision_ = 0;
};

}  // namespace ms::ui
