#include "VirtualListKeyValueOverlay.hpp"

#include <algorithm>

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
constexpr int VALUE_COL_W = 110; // stable alignment for values
}

VirtualListKeyValueOverlay::VirtualListKeyValueOverlay(lv_obj_t* parent)
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

VirtualListKeyValueOverlay::~VirtualListKeyValueOverlay() {
    // Overlay owns LVGL objects; VirtualListOverlay handles deletion.
}

void VirtualListKeyValueOverlay::setLabelTextIfChanged(
    lv_obj_t* label,
    std::string& cache,
    const std::string& text
) {
    if (!label) return;
    if (cache == text) return;

    cache = text;
    lv_label_set_text(label, cache.c_str());
}

void VirtualListKeyValueOverlay::syncRows(
    const VirtualListKeyValueOverlayProps& props,
    std::vector<int>& dirtyIndices
) {
    const bool canSkipRowDiff =
        (props.dataRevision != 0) &&
        (props.dataRevision == last_data_revision_) &&
        (props.rowCount == last_row_count_);

    if (canSkipRowDiff) return;

    const size_t nextCount = static_cast<size_t>(std::max(0, props.rowCount));
    if (rows_.size() != nextCount) {
        rows_.assign(nextCount, {"", ""});
        dirtyIndices.reserve(nextCount);
    }

    for (int i = 0; i < props.rowCount; ++i) {
        const auto* row = props.rows ? &props.rows[i] : nullptr;
        const std::string nextKey = (row && row->key) ? row->key : "";
        const std::string nextValue = (row && row->value) ? row->value : "";

        auto& current = rows_[static_cast<size_t>(i)];
        if (current.first != nextKey || current.second != nextValue) {
            current.first = nextKey;
            current.second = nextValue;
            dirtyIndices.push_back(i);
        }
    }

    last_data_revision_ = props.dataRevision;
    last_row_count_ = props.rowCount;
}

void VirtualListKeyValueOverlay::invalidateDirtyRows(const std::vector<int>& dirtyIndices) {
    auto* list = overlay_.list();
    if (!list || dirtyIndices.empty()) return;

    for (int index : dirtyIndices) {
        list->invalidateIndex(index);
    }
}

void VirtualListKeyValueOverlay::render(const VirtualListKeyValueOverlayProps& props) {
    if (!props.visible) {
        overlay_.hide();
        return;
    }

    overlay_.setTitle(props.title);
    overlay_.setMeta(props.meta);

    std::vector<int> dirtyIndices;
    syncRows(props, dirtyIndices);

    auto* list = overlay_.list();
    if (list) {
        const bool countChanged = list->setTotalCount(static_cast<int>(rows_.size()));
        list->setSelectedIndex(props.selectedIndex);

        if (!countChanged && overlay_.isVisible()) {
            invalidateDirtyRows(dirtyIndices);
        }
    }

    if (!overlay_.isVisible()) {
        overlay_.show();
    }
}

void VirtualListKeyValueOverlay::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = index - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (index < 0 || index >= static_cast<int>(rows_.size())) return;

    ensureSlotWidgets(slot, slotIndex);
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];

    if (widgets.keyLabel) {
        setLabelTextIfChanged(
            widgets.keyLabel,
            widgets.keyCache,
            rows_[static_cast<size_t>(index)].first
        );
    }
    if (widgets.valueLabel) {
        setLabelTextIfChanged(
            widgets.valueLabel,
            widgets.valueCache,
            rows_[static_cast<size_t>(index)].second
        );
    }

    widgets.boundIndex = index;
    applyHighlightStyle(widgets, isSelected);
}

void VirtualListKeyValueOverlay::updateSlotHighlight(widget::VirtualSlot& slot, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = slot.boundIndex - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;

    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    applyHighlightStyle(widgets, isSelected);
}

void VirtualListKeyValueOverlay::ensureSlotWidgets(widget::VirtualSlot& slot, int slotIndex) {
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

void VirtualListKeyValueOverlay::applyHighlightStyle(SlotWidgets& widgets, bool isSelected) {
    if (widgets.highlighted == isSelected) return;

    if (widgets.keyLabel) {
        style::apply(widgets.keyLabel).textColor(
            isSelected ? base_theme::color::TEXT_PRIMARY : base_theme::color::INACTIVE);
    }
    if (widgets.valueLabel) {
        style::apply(widgets.valueLabel).textColor(
            isSelected ? base_theme::color::ACTIVE : base_theme::color::INACTIVE);
    }

    widgets.highlighted = isSelected;
}

}  // namespace ms::ui
