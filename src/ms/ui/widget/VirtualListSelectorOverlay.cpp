#include "VirtualListSelectorOverlay.hpp"

#include <cstdio>

#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>

#include <ms/ui/font/CoreFonts.hpp>

namespace ms::ui {

using namespace oc::ui::lvgl;
namespace style = oc::ui::lvgl::style;

namespace {
constexpr int VISIBLE_SLOTS = 5;
constexpr int ITEM_HEIGHT = 32;

constexpr int PAD_H = base_theme::layout::SPACE_XL; // 16
constexpr int COL_GAP = base_theme::layout::SPACE_MD; // 8
constexpr int INDEX_W = 24;
}

VirtualListSelectorOverlay::VirtualListSelectorOverlay(lv_obj_t* parent)
    : overlay_(parent) {
    overlay_.configureList(VISIBLE_SLOTS, ITEM_HEIGHT);

    auto* list = overlay_.list();
    if (list) {
        list->scrollMode(widget::ScrollMode::CenterLocked)
            .onBindSlot([this](widget::VirtualSlot& slot, int index, bool isSelected) {
                bindSlot(slot, index, isSelected);
            })
            .onUpdateHighlight([this](widget::VirtualSlot& slot, bool isSelected) {
                updateSlotHighlight(slot, isSelected);
            });
    }

    slot_widgets_.resize(VISIBLE_SLOTS);
}

VirtualListSelectorOverlay::~VirtualListSelectorOverlay() {
    // Overlay owns LVGL objects; VirtualListOverlay handles deletion.
}

void VirtualListSelectorOverlay::render(const VirtualListSelectorOverlayProps& props) {
    if (!props.visible) {
        overlay_.hide();
        return;
    }

    // Track whether we need to force a rebind (data or per-slot layout changed).
    bool dataChanged = false;
    if (props.dataRevision != 0 && props.dataRevision != last_data_revision_) {
        dataChanged = true;
    }
    if (props.items != last_items_ || props.itemCount != last_item_count_) {
        dataChanged = true;
    }
    if (props.showIndexColumn != last_show_index_column_) {
        dataChanged = true;
    }

    last_items_ = props.items;
    last_item_count_ = props.itemCount;
    last_show_index_column_ = props.showIndexColumn;
    last_data_revision_ = props.dataRevision;

    current_props_ = props;

    overlay_.setTitle(props.title);
    overlay_.setMeta(props.meta);

    const int totalCount = (props.items && props.itemCount > 0) ? props.itemCount : 0;
    auto* list = overlay_.list();
    if (list) {
        const bool countChanged = list->setTotalCount(totalCount);
        list->setSelectedIndex(props.selectedIndex);

        // Only rebind visible slots when data (not selection) changed.
        if (!countChanged && dataChanged && overlay_.isVisible()) {
            list->invalidate();
        }
    }

    if (!overlay_.isVisible()) {
        overlay_.show();
    }
}

void VirtualListSelectorOverlay::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = index - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;

    ensureSlotWidgets(slot, slotIndex);
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];

    const char* name = "";
    if (current_props_.items && index >= 0 && index < current_props_.itemCount) {
        name = current_props_.items[index] ? current_props_.items[index] : "";
    }
    if (widgets.label) {
        lv_label_set_text(widgets.label, name);
    }

    if (widgets.indexLabel) {
        char indexStr[12];
        snprintf(indexStr, sizeof(indexStr), "%d", index + 1);
        lv_label_set_text(widgets.indexLabel, indexStr);
        if (current_props_.showIndexColumn) {
            lv_obj_clear_flag(widgets.indexLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.indexLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    applyHighlightStyle(widgets, isSelected);
}

void VirtualListSelectorOverlay::updateSlotHighlight(widget::VirtualSlot& slot, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = slot.boundIndex - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;

    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    applyHighlightStyle(widgets, isSelected);
}

void VirtualListSelectorOverlay::ensureSlotWidgets(widget::VirtualSlot& slot, int slotIndex) {
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    if (widgets.created) return;

    lv_obj_t* container = slot.container;
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(container, COL_GAP, LV_STATE_DEFAULT);

    widgets.indexLabel = lv_label_create(container);
    lv_obj_set_width(widgets.indexLabel, INDEX_W);
    lv_obj_set_style_text_align(widgets.indexLabel, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    if (fonts.list_item_label) {
        lv_obj_set_style_text_font(widgets.indexLabel, fonts.list_item_label, LV_STATE_DEFAULT);
    }
    style::apply(widgets.indexLabel).textColor(base_theme::color::INACTIVE_LIGHTER);

    widgets.label = lv_label_create(container);
    lv_obj_set_flex_grow(widgets.label, 1);
    lv_label_set_long_mode(widgets.label, LV_LABEL_LONG_DOT);
    if (fonts.list_item_label) {
        lv_obj_set_style_text_font(widgets.label, fonts.list_item_label, LV_STATE_DEFAULT);
    }

    widgets.created = true;
}

void VirtualListSelectorOverlay::applyHighlightStyle(SlotWidgets& widgets, bool isSelected) {
    if (widgets.label) {
        style::apply(widgets.label).textColor(
            isSelected ? base_theme::color::TEXT_PRIMARY : base_theme::color::TEXT_SECONDARY);
    }
    if (widgets.indexLabel) {
        style::apply(widgets.indexLabel).textColor(
            isSelected ? base_theme::color::ACTIVE : base_theme::color::INACTIVE_LIGHTER);
    }
}

}  // namespace ms::ui
