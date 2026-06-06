#pragma once

/**
 * @file MenuListView.hpp
 * @brief Non-modal key/value menu surface backed by VirtualList.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <lvgl.h>

#include <oc/ui/lvgl/widget/VirtualList.hpp>

namespace ms::ui {

enum class MenuRowKind : uint8_t {
    Value = 0,
    Folder,
    Action,
    Toggle,
    Disabled,
};

struct MenuRow {
    const char* label = "";
    const char* value = "";
    MenuRowKind kind = MenuRowKind::Value;
    bool enabled = true;
};

struct MenuListViewProps {
    const char* title = "";
    const char* meta = "";
    const MenuRow* rows = nullptr;
    int rowCount = 0;
    int selectedIndex = 0;
    uint32_t dataRevision = 0;
};

class MenuListView {
public:
    explicit MenuListView(lv_obj_t* parent);
    ~MenuListView();

    MenuListView(const MenuListView&) = delete;
    MenuListView& operator=(const MenuListView&) = delete;

    void render(const MenuListViewProps& props);
    void show();
    void hide();

    lv_obj_t* getElement() const { return container_; }

private:
    static constexpr int VISIBLE_SLOTS = 5;
    static constexpr int MAX_ROWS = 16;
    static constexpr std::size_t TEXT_CACHE_SIZE = 48;

    struct TextCache {
        char text[TEXT_CACHE_SIZE] = {};
    };

    struct RowCache {
        TextCache label;
        TextCache value;
        MenuRowKind kind = MenuRowKind::Value;
        bool enabled = true;
    };

    struct SlotWidgets {
        bool created = false;
        lv_obj_t* label = nullptr;
        lv_obj_t* value = nullptr;
        bool highlighted = false;
        bool highlightStyleApplied = false;
        bool rowStyleApplied = false;
        uint32_t labelColor = 0;
        uint32_t valueColor = 0;
        lv_opa_t labelOpa = LV_OPA_TRANSP;
        lv_opa_t valueOpa = LV_OPA_TRANSP;
        int boundIndex = -1;
        TextCache labelCache;
        TextCache valueCache;
    };

    void createUi(lv_obj_t* parent);
    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(oc::ui::lvgl::widget::VirtualSlot& slot, int slotIndex);
    void applyHighlightStyle(oc::ui::lvgl::widget::VirtualSlot& slot,
                             SlotWidgets& widgets,
                             bool isSelected,
                             const RowCache& row);
    void applyRowStyle(SlotWidgets& widgets, const RowCache& row);
    void syncRows(const MenuListViewProps& props,
                  std::array<int, MAX_ROWS>& dirtyIndices,
                  int& dirtyCount);
    void invalidateDirtyRows(const std::array<int, MAX_ROWS>& dirtyIndices, int dirtyCount);
    static bool copyTextIfChanged(TextCache& cache, const char* text);
    static void setLabelTextIfChanged(lv_obj_t* label, TextCache& cache, const char* text);

    lv_obj_t* container_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* meta_ = nullptr;
    std::unique_ptr<oc::ui::lvgl::widget::VirtualList> list_;

    std::array<SlotWidgets, VISIBLE_SLOTS> slot_widgets_{};
    std::array<RowCache, MAX_ROWS> rows_{};
    TextCache title_cache_{};
    TextCache meta_cache_{};

    uint32_t last_data_revision_ = 0;
    int last_row_count_ = 0;
    int row_count_ = 0;
};

}  // namespace ms::ui
