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
constexpr int ICON_COL_W = 16;
constexpr int VALUE_COL_W = 110; // stable alignment for values
constexpr int SPARKLINE_H = 18;
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

        // Build the fixed slot pool while this overlay is still parked.
        list->prepare();
        const auto& slots = list->getSlots();
        for (int i = 0; i < VISIBLE_SLOTS && i < static_cast<int>(slots.size()); ++i) {
            ensureSlotWidgets(slots[static_cast<size_t>(i)].container, i);
        }
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

FLASHMEM bool VirtualListKeyValueOverlay::copySparklineIfChanged(
    KeyValueSparkline& cache,
    const KeyValueSparkline& next
) {
    const uint8_t nextCount = next.enabled
        ? static_cast<uint8_t>(std::min<size_t>(
              next.sampleCount,
              KEY_VALUE_SPARKLINE_SAMPLE_COUNT
          ))
        : 0U;
    bool changed = cache.enabled != next.enabled || cache.sampleCount != nextCount;
    for (size_t i = 0; i < KEY_VALUE_SPARKLINE_SAMPLE_COUNT; ++i) {
        const uint8_t nextValue = i < nextCount ? next.samples[i] : 0U;
        if (cache.samples[i] != nextValue) {
            changed = true;
            cache.samples[i] = nextValue;
        }
    }
    cache.enabled = next.enabled && nextCount >= 2U;
    cache.sampleCount = cache.enabled ? nextCount : 0U;
    return changed;
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
        const bool iconChanged = copyTextIfChanged(current.icon, row ? row->icon : "");
        const bool iconStyleChanged =
            current.iconFont != (row ? row->iconFont : nullptr) ||
            current.iconColor != (row ? row->iconColor : 0U);
        const bool sparklineChanged = copySparklineIfChanged(
            current.sparkline,
            row ? row->sparkline : KeyValueSparkline{}
        );
        current.iconFont = row ? row->iconFont : nullptr;
        current.iconColor = row ? row->iconColor : 0U;
        if ((keyChanged || valueChanged || iconChanged || iconStyleChanged || sparklineChanged) &&
            dirtyCount < MAX_ROWS) {
            dirtyIndices[static_cast<size_t>(dirtyCount++)] = i;
        }
    }

    for (int i = nextCount; i < row_count_; ++i) {
        auto& current = rows_[static_cast<size_t>(i)];
        copyTextIfChanged(current.key, "");
        copyTextIfChanged(current.value, "");
        copyTextIfChanged(current.icon, "");
        current.iconFont = nullptr;
        current.iconColor = 0;
        copySparklineIfChanged(current.sparkline, KeyValueSparkline{});
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

    const bool dimStyleChanged = dim_unselected_ != props.dimUnselected;
    dim_unselected_ = props.dimUnselected;

    std::array<int, MAX_ROWS> dirtyIndices{};
    int dirtyCount = 0;
    syncRows(props, dirtyIndices, dirtyCount);

    auto* list = overlay_.list();
    if (list) {
        const bool countChanged = list->setTotalCount(row_count_);
        list->setSelectedIndex(props.selectedIndex);

        if (!countChanged && overlay_.isVisible()) {
            if (dimStyleChanged) {
                list->invalidate();
            } else {
                invalidateDirtyRows(dirtyIndices, dirtyCount);
            }
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

    ensureSlotWidgets(slot.container, slotIndex);
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    const auto& row = rows_[static_cast<size_t>(index)];

    if (widgets.iconLabel) {
        const bool hasIcon = row.icon.text[0] != '\0' && row.iconFont != nullptr;
        setLabelTextIfChanged(widgets.iconLabel, widgets.iconCache, hasIcon ? row.icon.text : "");
        if (hasIcon && widgets.iconFont != row.iconFont) {
            lv_obj_set_style_text_font(widgets.iconLabel, row.iconFont, LV_STATE_DEFAULT);
            widgets.iconFont = row.iconFont;
        }
        if (hasIcon && widgets.iconColor != row.iconColor) {
            lv_obj_set_style_text_color(widgets.iconLabel, lv_color_hex(row.iconColor), LV_STATE_DEFAULT);
            widgets.iconColor = row.iconColor;
        }
        if (!hasIcon) {
            widgets.iconFont = nullptr;
            widgets.iconColor = 0;
        }
    }

    if (widgets.keyLabel) {
        setLabelTextIfChanged(
            widgets.keyLabel,
            widgets.keyCache,
            row.key.text
        );
    }
    if (widgets.valueLabel) {
        setLabelTextIfChanged(widgets.valueLabel, widgets.valueCache, row.value.text);
    }
    applySparkline(widgets, row);

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

FLASHMEM void VirtualListKeyValueOverlay::ensureSlotWidgets(lv_obj_t* container, int slotIndex) {
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    if (widgets.created || !container) return;

    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(container, COL_GAP, LV_STATE_DEFAULT);

    widgets.iconLabel = lv_label_create(container);
    lv_obj_set_width(widgets.iconLabel, ICON_COL_W);
    lv_obj_set_style_text_align(widgets.iconLabel, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_label_set_long_mode(widgets.iconLabel, LV_LABEL_LONG_DOT);
    lv_label_set_text(widgets.iconLabel, "");

    widgets.keyLabel = lv_label_create(container);
    lv_obj_set_flex_grow(widgets.keyLabel, 1);
    lv_label_set_long_mode(widgets.keyLabel, LV_LABEL_LONG_DOT);
    const lv_font_t* keyFont = fonts.list_item_label
        ? fonts.list_item_label
        : LV_FONT_DEFAULT;
    lv_obj_set_style_text_font(widgets.keyLabel, keyFont, LV_STATE_DEFAULT);
    // A fixed one-line box is required for LONG_DOT. With auto height LVGL
    // wraps first, allowing a long key to escape its 32 px virtual row and
    // collide with the contextual strip below it.
    lv_obj_set_height(widgets.keyLabel, lv_font_get_line_height(keyFont));
    style::apply(widgets.keyLabel).textColor(base_theme::color::INACTIVE);

    widgets.valueLabel = lv_label_create(container);
    lv_obj_set_width(widgets.valueLabel, VALUE_COL_W);
    lv_obj_set_style_text_align(widgets.valueLabel, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(widgets.valueLabel, LV_LABEL_LONG_DOT);
    const lv_font_t* valueFont = fonts.inter_14_semibold
        ? fonts.inter_14_semibold
        : keyFont;
    lv_obj_set_style_text_font(widgets.valueLabel, valueFont, LV_STATE_DEFAULT);
    lv_obj_set_height(widgets.valueLabel, lv_font_get_line_height(valueFont));

    widgets.sparklineLine = lv_line_create(container);
    lv_obj_set_size(widgets.sparklineLine, VALUE_COL_W, SPARKLINE_H);
    lv_obj_set_style_line_width(widgets.sparklineLine, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(widgets.sparklineLine, true, LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(
        widgets.sparklineLine,
        lv_color_hex(base_theme::color::ACTIVE),
        LV_STATE_DEFAULT
    );
    lv_obj_add_flag(widgets.sparklineLine, LV_OBJ_FLAG_HIDDEN);

    widgets.created = true;
}

FLASHMEM void VirtualListKeyValueOverlay::applySparkline(
    SlotWidgets& widgets,
    const RowCache& row
) {
    const bool showSparkline = row.sparkline.enabled && row.sparkline.sampleCount >= 2U;
    if (widgets.valueLabel) {
        if (showSparkline) {
            lv_obj_add_flag(widgets.valueLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(widgets.valueLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (!widgets.sparklineLine) return;

    if (!showSparkline) {
        if (widgets.sparklineVisible) {
            lv_obj_add_flag(widgets.sparklineLine, LV_OBJ_FLAG_HIDDEN);
            widgets.sparklineVisible = false;
        }
        return;
    }

    const uint8_t count = static_cast<uint8_t>(std::min<size_t>(
        row.sparkline.sampleCount,
        KEY_VALUE_SPARKLINE_SAMPLE_COUNT
    ));
    const int width = VALUE_COL_W - 1;
    const int height = SPARKLINE_H - 1;
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t sample = row.sparkline.samples[i];
        widgets.sparklinePoints[i].x =
            static_cast<lv_value_precise_t>((static_cast<int>(i) * width) / (count - 1));
        widgets.sparklinePoints[i].y =
            static_cast<lv_value_precise_t>(height - ((static_cast<int>(sample) * height) / 255));
    }
    lv_line_set_points(widgets.sparklineLine, widgets.sparklinePoints.data(), count);
    lv_obj_clear_flag(widgets.sparklineLine, LV_OBJ_FLAG_HIDDEN);
    widgets.sparklineVisible = true;
}

FLASHMEM void VirtualListKeyValueOverlay::applyHighlightStyle(SlotWidgets& widgets, bool isSelected) {
    if (widgets.highlightStyleApplied && widgets.highlighted == isSelected &&
        widgets.dimUnselected == dim_unselected_) {
        return;
    }

    if (widgets.iconLabel) {
        lv_obj_set_style_text_opa(
            widgets.iconLabel,
            isSelected ? LV_OPA_COVER : LV_OPA_70,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.keyLabel) {
        style::apply(widgets.keyLabel).textColor(
            isSelected
                ? base_theme::color::TEXT_PRIMARY
                : (dim_unselected_
                       ? base_theme::color::INACTIVE
                       : base_theme::color::TEXT_SECONDARY));
    }
    if (widgets.valueLabel) {
        style::apply(widgets.valueLabel).textColor(
            isSelected
                ? base_theme::color::ACTIVE
                : (dim_unselected_
                       ? base_theme::color::INACTIVE
                       : base_theme::color::INACTIVE_LIGHTER));
    }
    if (widgets.sparklineLine) {
        const bool visualPreview = widgets.sparklineVisible;
        lv_obj_set_style_line_color(
            widgets.sparklineLine,
            lv_color_hex(
                (isSelected || visualPreview)
                    ? base_theme::color::ACTIVE
                    : base_theme::color::INACTIVE
            ),
            LV_STATE_DEFAULT
        );
        lv_obj_set_style_line_opa(
            widgets.sparklineLine,
            (isSelected || visualPreview) ? LV_OPA_COVER : LV_OPA_70,
            LV_STATE_DEFAULT
        );
    }

    widgets.highlighted = isSelected;
    widgets.dimUnselected = dim_unselected_;
    widgets.highlightStyleApplied = true;
}

}  // namespace ms::ui
