#include "VirtualListKeyValueOverlay.hpp"

#include <algorithm>
#include <cstring>

#include <config/PlatformCompat.hpp>
#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>

#include <ms/ui/font/CoreFonts.hpp>

namespace ms::ui {

using namespace oc::ui::lvgl;
namespace style = oc::ui::lvgl::style;

namespace {
constexpr int ITEM_HEIGHT = 32;

constexpr int PAD_H = base_theme::layout::SPACE_XL; // 16
constexpr int COL_GAP = base_theme::layout::SPACE_MD; // 8
constexpr int VALUE_COL_W = 110; // stable alignment for values
}

FLASHMEM VirtualListKeyValueOverlay::VirtualListKeyValueOverlay(lv_obj_t* parent)
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
}

FLASHMEM VirtualListKeyValueOverlay::~VirtualListKeyValueOverlay() {
    // Overlay owns LVGL objects; VirtualListOverlay handles deletion.
}

FLASHMEM bool VirtualListKeyValueOverlay::copyTextIfChanged(TextCache& cache, const char* text) {
    const char* source = text ? text : "";
    char next[TEXT_CACHE_SIZE] = {};
    std::strncpy(next, source, TEXT_CACHE_SIZE - 1);
    next[TEXT_CACHE_SIZE - 1] = '\0';

    if (std::strncmp(cache.text, next, TEXT_CACHE_SIZE) == 0) return false;

    std::strncpy(cache.text, next, TEXT_CACHE_SIZE - 1);
    cache.text[TEXT_CACHE_SIZE - 1] = '\0';
    return true;
}

FLASHMEM void VirtualListKeyValueOverlay::setLabelTextIfChanged(
    lv_obj_t* label,
    TextCache& cache,
    const char* text
) {
    if (!label) return;
    if (!copyTextIfChanged(cache, text)) return;

    lv_label_set_text(label, cache.text);
}

FLASHMEM void VirtualListKeyValueOverlay::syncRows(
    const VirtualListKeyValueOverlayProps& props,
    std::array<int, MAX_ROWS>& dirtyIndices,
    int& dirtyCount
) {
    dirtyCount = 0;
    const int nextCount = std::clamp(props.rowCount, 0, MAX_ROWS);
    const bool canSkipRowDiff =
        (props.dataRevision != 0) &&
        (props.dataRevision == last_data_revision_) &&
        (nextCount == last_row_count_);

    if (canSkipRowDiff) return;

    for (int i = 0; i < nextCount; ++i) {
        const auto* row = props.rows ? &props.rows[i] : nullptr;
        auto& current = rows_[static_cast<size_t>(i)];
        const bool keyChanged = copyTextIfChanged(current.key, row ? row->key : "");
        const bool valueChanged = copyTextIfChanged(current.value, row ? row->value : "");
        if ((keyChanged || valueChanged) && dirtyCount < MAX_ROWS) {
            dirtyIndices[static_cast<size_t>(dirtyCount++)] = i;
        }
    }

    for (int i = nextCount; i < row_count_; ++i) {
        auto& current = rows_[static_cast<size_t>(i)];
        copyTextIfChanged(current.key, "");
        copyTextIfChanged(current.value, "");
    }

    last_data_revision_ = props.dataRevision;
    last_row_count_ = nextCount;
    row_count_ = nextCount;
}

FLASHMEM void VirtualListKeyValueOverlay::invalidateDirtyRows(
    const std::array<int, MAX_ROWS>& dirtyIndices,
    int dirtyCount
) {
    auto* list = overlay_.list();
    if (!list || dirtyCount <= 0) return;

    for (int i = 0; i < dirtyCount; ++i) {
        list->invalidateIndex(dirtyIndices[static_cast<size_t>(i)]);
    }
}

FLASHMEM void VirtualListKeyValueOverlay::render(const VirtualListKeyValueOverlayProps& props) {
    if (!props.visible) {
        overlay_.hide();
        return;
    }

    overlay_.setTitle(props.title);
    overlay_.setMeta(props.meta);

    std::array<int, MAX_ROWS> dirtyIndices{};
    int dirtyCount = 0;
    syncRows(props, dirtyIndices, dirtyCount);

    auto* list = overlay_.list();
    if (list) {
        const bool countChanged = list->setTotalCount(row_count_);
        list->setSelectedIndex(props.selectedIndex);

        if (!countChanged && overlay_.isVisible()) {
            invalidateDirtyRows(dirtyIndices, dirtyCount);
        }
    }

    if (!overlay_.isVisible()) {
        overlay_.show();
    }
}

FLASHMEM void VirtualListKeyValueOverlay::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = index - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (index < 0 || index >= row_count_) return;

    ensureSlotWidgets(slot, slotIndex);
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];

    if (widgets.keyLabel) {
        setLabelTextIfChanged(
            widgets.keyLabel,
            widgets.keyCache,
            rows_[static_cast<size_t>(index)].key.text
        );
    }
    if (widgets.valueLabel) {
        setLabelTextIfChanged(
            widgets.valueLabel,
            widgets.valueCache,
            rows_[static_cast<size_t>(index)].value.text
        );
    }

    widgets.boundIndex = index;
    applyHighlightStyle(widgets, isSelected);
}

FLASHMEM void VirtualListKeyValueOverlay::updateSlotHighlight(widget::VirtualSlot& slot, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = slot.boundIndex - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;

    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    applyHighlightStyle(widgets, isSelected);
}

FLASHMEM void VirtualListKeyValueOverlay::ensureSlotWidgets(widget::VirtualSlot& slot, int slotIndex) {
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    if (widgets.created) return;

    lv_obj_t* container = slot.container;
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(container, COL_GAP, LV_STATE_DEFAULT);

    widgets.keyLabel = lv_label_create(container);
    lv_obj_set_flex_grow(widgets.keyLabel, 1);
    lv_label_set_long_mode(widgets.keyLabel, LV_LABEL_LONG_DOT);
    if (fonts.list_item_label) {
        lv_obj_set_style_text_font(widgets.keyLabel, fonts.list_item_label, LV_STATE_DEFAULT);
    }
    style::apply(widgets.keyLabel).textColor(base_theme::color::INACTIVE);

    widgets.valueLabel = lv_label_create(container);
    lv_obj_set_width(widgets.valueLabel, VALUE_COL_W);
    lv_obj_set_style_text_align(widgets.valueLabel, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(widgets.valueLabel, LV_LABEL_LONG_DOT);
    if (fonts.inter_14_semibold) {
        lv_obj_set_style_text_font(widgets.valueLabel, fonts.inter_14_semibold, LV_STATE_DEFAULT);
    } else if (fonts.list_item_label) {
        lv_obj_set_style_text_font(widgets.valueLabel, fonts.list_item_label, LV_STATE_DEFAULT);
    }

    widgets.created = true;
}

FLASHMEM void VirtualListKeyValueOverlay::applyHighlightStyle(SlotWidgets& widgets, bool isSelected) {
    if (widgets.highlightStyleApplied && widgets.highlighted == isSelected) return;

    if (widgets.keyLabel) {
        style::apply(widgets.keyLabel).textColor(
            isSelected ? base_theme::color::TEXT_PRIMARY : base_theme::color::INACTIVE);
    }
    if (widgets.valueLabel) {
        style::apply(widgets.valueLabel).textColor(
            isSelected ? base_theme::color::ACTIVE : base_theme::color::INACTIVE);
    }

    widgets.highlighted = isSelected;
    widgets.highlightStyleApplied = true;
}

}  // namespace ms::ui
