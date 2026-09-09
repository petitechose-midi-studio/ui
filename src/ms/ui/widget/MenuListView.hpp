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

#include <oc/ui/lvgl/widget/Label.hpp>
#include <oc/ui/lvgl/widget/VirtualList.hpp>

#include <ms/ui/widget/ListVisualTokens.hpp>

namespace ms::ui {

enum class MenuRowKind : uint8_t {
    Value = 0,
    Folder,
    Action,
    Toggle,
    Disabled,
};

/**
 * Semantic tone of the displayed value.
 *
 * Row kind controls behavior and availability. Tone is deliberately separate
 * so a normal action does not look focused and an inherited toggle does not
 * look like a successful outcome.
 */
enum class MenuRowTone : uint8_t {
    Neutral = 0,
    Positive,
    Warning,
    Destructive,
};

enum class MenuRowValueRole : uint8_t {
    Value = 0,
    Description,
};

enum class MenuListHeaderLayout : uint8_t {
    Horizontal = 0,
    Stacked,
};

struct MenuRow {
    const char* label = "";
    const char* value = "";
    const char* icon = "";
    MenuRowKind kind = MenuRowKind::Value;
    MenuRowTone tone = MenuRowTone::Neutral;
    bool enabled = true;
    bool valueAutoScroll = false;
    MenuRowValueRole valueRole = MenuRowValueRole::Value;
};

struct MenuListViewProps {
    const char* title = "";
    const char* meta = "";
    const MenuRow* rows = nullptr;
    int rowCount = 0;
    int selectedIndex = 0;
    uint32_t dataRevision = 0;
    MenuListHeaderLayout headerLayout = MenuListHeaderLayout::Horizontal;
    const lv_font_t* iconFont = nullptr;
    const ListVisualTokens* visualTokens = nullptr;
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
    static constexpr std::size_t ICON_CACHE_SIZE = 8;

    struct TextCache {
        char text[TEXT_CACHE_SIZE] = {};
    };

    struct IconCache {
        char text[ICON_CACHE_SIZE] = {};
    };

    struct RowCache {
        TextCache label;
        TextCache value;
        IconCache icon;
        MenuRowKind kind = MenuRowKind::Value;
        MenuRowTone tone = MenuRowTone::Neutral;
        bool enabled = true;
        bool valueAutoScroll = false;
        MenuRowValueRole valueRole = MenuRowValueRole::Value;
    };

    struct SlotWidgets {
        bool created = false;
        lv_obj_t* icon = nullptr;
        lv_obj_t* label = nullptr;
        lv_obj_t* value = nullptr;
        std::unique_ptr<oc::ui::lvgl::Label> valueScroller;
        bool highlighted = false;
        bool highlightStyleApplied = false;
        bool rowStyleApplied = false;
        bool valueLayoutApplied = false;
        bool valueScrollerActive = false;
        bool iconVisible = false;
        MenuRowValueRole valueRole = MenuRowValueRole::Value;
        const lv_font_t* iconFont = nullptr;
        uint32_t labelColor = 0;
        uint32_t valueColor = 0;
        lv_opa_t labelOpa = LV_OPA_TRANSP;
        lv_opa_t valueOpa = LV_OPA_TRANSP;
        int boundIndex = -1;
        IconCache iconCache;
        TextCache labelCache;
        TextCache valueCache;
        TextCache valueScrollerCache;
    };

    void createUi(lv_obj_t* parent);
    void bindSlot(oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool isSelected);
    void updateSlotHighlight(oc::ui::lvgl::widget::VirtualSlot& slot, bool isSelected);
    void ensureSlotWidgets(lv_obj_t* container, int slotIndex);
    void ensureIcon(SlotWidgets& widgets);
    void ensureValueScroller(SlotWidgets& widgets, MenuRowValueRole role);
    void applyHighlightStyle(oc::ui::lvgl::widget::VirtualSlot& slot,
                             SlotWidgets& widgets,
                             bool isSelected);
    void applyValueLayout(SlotWidgets& widgets, MenuRowValueRole role, bool iconVisible);
    void syncValuePresentation(SlotWidgets& widgets, const RowCache& row,
                               bool isSelected);
    void applyHeaderLayout(MenuListHeaderLayout layout);
    void applyRowStyle(SlotWidgets& widgets, const RowCache& row);
    void syncRows(const MenuListViewProps& props,
                  std::array<int, MAX_ROWS>& dirtyIndices,
                  int& dirtyCount);
    void invalidateDirtyRows(const std::array<int, MAX_ROWS>& dirtyIndices, int dirtyCount);
    static bool copyTextIfChanged(TextCache& cache, const char* text);
    static bool copyIconIfChanged(IconCache& cache, const char* text);
    static void setLabelTextIfChanged(lv_obj_t* label, TextCache& cache, const char* text);
    static void setLabelTextIfChanged(oc::ui::lvgl::Label* label, TextCache& cache, const char* text);

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
    const lv_font_t* icon_font_ = nullptr;
    const ListVisualTokens* visual_tokens_ = nullptr;
    int last_row_count_ = 0;
    int row_count_ = 0;
    MenuListHeaderLayout header_layout_ = MenuListHeaderLayout::Horizontal;
    bool header_layout_applied_ = false;
};

}  // namespace ms::ui
