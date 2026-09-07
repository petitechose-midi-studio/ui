#include "VirtualListSelectorOverlay.hpp"

#include <ms/ui/widget/TextOverflow.hpp>

#include <config/PlatformCompat.hpp>
#include <oc/type/TextFormat.hpp>
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
constexpr int INDEX_W = 24;
constexpr int ICON_W = 16;
constexpr int VALUE_W = 58;
}

FLASHMEM VirtualListSelectorOverlay::VirtualListSelectorOverlay(lv_obj_t* parent)
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

        list->prepare();
        const auto& slots = list->getSlots();
        for (int i = 0; i < VISIBLE_SLOTS && i < static_cast<int>(slots.size()); ++i) {
            ensureSlotWidgets(slots[static_cast<size_t>(i)].container, i);
        }
    }
}

FLASHMEM VirtualListSelectorOverlay::~VirtualListSelectorOverlay() {
    // Overlay owns LVGL objects; VirtualListOverlay handles deletion.
}

FLASHMEM bool VirtualListSelectorOverlay::copyTextIfChanged(TextCache& cache, const char* text) {
    return text::copyTruncatedIfChanged(cache.text, sizeof(cache.text), text);
}

FLASHMEM bool VirtualListSelectorOverlay::copyIconIfChanged(
    IconCache& cache,
    const char* text
) {
    return text::copyTruncatedIfChanged(cache.text, sizeof(cache.text), text);
}

FLASHMEM void VirtualListSelectorOverlay::setLabelTextIfChanged(
    lv_obj_t* label,
    TextCache& cache,
    const char* text
) {
    if (!label) return;
    if (!copyTextIfChanged(cache, text)) return;
    lv_label_set_text(label, cache.text);
}

FLASHMEM void VirtualListSelectorOverlay::setIconTextIfChanged(
    lv_obj_t* label,
    IconCache& cache,
    const char* text
) {
    if (!label) return;
    if (!copyIconIfChanged(cache, text)) return;
    lv_label_set_text(label, cache.text);
}

FLASHMEM void VirtualListSelectorOverlay::render(const VirtualListSelectorOverlayProps& props) {
    if (!props.visible) {
        overlay_.hide();
        return;
    }

    // The presentation registry can reveal the root before the first render.
    // Prepare styles as well as layout while hidden, then show the final state.
    if (!overlay_.isVisible()) lv_obj_add_flag(overlay_.getElement(), LV_OBJ_FLAG_HIDDEN);

    // Track whether we need to force a rebind (data or per-slot layout changed).
    bool dataChanged = false;
    const bool revisionChanged = props.dataRevision != 0 &&
        props.dataRevision != last_data_revision_;
    if (revisionChanged) {
        dataChanged = true;
        for (auto& widgets : slot_widgets_) {
            widgets.highlightStyleApplied = false;
        }
    }
    if (props.items != last_items_ || props.itemCount != last_item_count_) {
        dataChanged = true;
    }
    const bool iconStyleChanged =
        props.iconColors != last_icon_colors_;
    if (props.icons != last_icons_ || props.values != last_values_ ||
        iconStyleChanged ||
        props.iconFont != last_icon_font_) {
        dataChanged = true;
    }
    if (iconStyleChanged) {
        for (auto& widgets : slot_widgets_) {
            widgets.highlightStyleApplied = false;
        }
    }
    if (props.showIndexColumn != last_show_index_column_) {
        dataChanged = true;
    }
    if (props.dimUnselected != last_dim_unselected_) {
        dataChanged = true;
    }
    if (props.backdropOpacity != last_backdrop_opacity_) {
        overlay_.setBackdropOpacity(props.backdropOpacity);
    }
    const auto* nextVisuals = props.visualTokens
        ? props.visualTokens
        : &DEFAULT_LIST_VISUAL_TOKENS;
    const bool visualStyleChanged = nextVisuals != last_visual_tokens_;
    if (visualStyleChanged) {
        dataChanged = true;
        for (auto& widgets : slot_widgets_) {
            widgets.highlightStyleApplied = false;
        }
        overlay_.setTextColors(
            nextVisuals->primaryTextColor,
            nextVisuals->secondaryTextColor
        );
    }

    last_items_ = props.items;
    last_icons_ = props.icons;
    last_values_ = props.values;
    last_icon_colors_ = props.iconColors;
    last_icon_font_ = props.iconFont;
    last_item_count_ = props.itemCount;
    last_show_index_column_ = props.showIndexColumn;
    last_dim_unselected_ = props.dimUnselected;
    last_backdrop_opacity_ = props.backdropOpacity;
    last_data_revision_ = props.dataRevision;
    last_visual_tokens_ = nextVisuals;

    current_props_ = props;

    overlay_.setTitle(props.title);
    overlay_.setMeta(props.meta);
    overlay_.setBreadcrumb(props.breadcrumb);
    overlay_.setMetaIcon(
        props.metaIcon,
        props.metaIconFont,
        props.metaIconColor
    );

    const int totalCount = (props.items && props.itemCount > 0) ? props.itemCount : 0;
    auto* list = overlay_.list();
    if (list) {
        if (visualStyleChanged) {
            const auto& visuals = *last_visual_tokens_;
            list->selectionCursorStyle(
                visuals.selectedSurfaceColor,
                visuals.selectedSurfaceOpacity,
                visuals.selectedSurfaceRadius
            );
        }
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

FLASHMEM void VirtualListSelectorOverlay::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = index - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;

    ensureSlotWidgets(slot.container, slotIndex);
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];

    const char* name = "";
    if (current_props_.items && index >= 0 && index < current_props_.itemCount) {
        name = current_props_.items[index] ? current_props_.items[index] : "";
    }
    if (widgets.label) {
        setLabelTextIfChanged(widgets.label, widgets.labelCache, name);
    }

    const char* value = "";
    if (current_props_.values && index >= 0 &&
        index < current_props_.itemCount) {
        value = current_props_.values[index]
            ? current_props_.values[index]
            : "";
    }
    if (value[0] != '\0') ensureValue(widgets);
    if (widgets.value) {
        setLabelTextIfChanged(widgets.value, widgets.valueCache, value);
        if (value[0] != '\0') {
            lv_obj_clear_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const char* icon = "";
    if (current_props_.icons && index >= 0 && index < current_props_.itemCount) {
        icon = current_props_.icons[index] ? current_props_.icons[index] : "";
    }
    const bool iconVisible = current_props_.iconFont && icon[0] != '\0';
    if (iconVisible) ensureIcon(widgets);
    if (widgets.icon) {
        if (current_props_.iconFont && widgets.iconFont != current_props_.iconFont) {
            lv_obj_set_style_text_font(widgets.icon, current_props_.iconFont, 0);
            widgets.iconFont = current_props_.iconFont;
        }
        setIconTextIfChanged(widgets.icon, widgets.iconCache, icon);
        if (iconVisible) {
            lv_obj_clear_flag(widgets.icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (widgets.indexLabel) {
        char indexStr[12];
        oc::type::text::formatUnsigned(indexStr, sizeof(indexStr), static_cast<unsigned>(index + 1));
        setLabelTextIfChanged(widgets.indexLabel, widgets.indexCache, indexStr);

        if (!widgets.indexVisibilityApplied || widgets.indexVisible != current_props_.showIndexColumn) {
            if (current_props_.showIndexColumn) {
                lv_obj_clear_flag(widgets.indexLabel, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(widgets.indexLabel, LV_OBJ_FLAG_HIDDEN);
            }
            widgets.indexVisible = current_props_.showIndexColumn;
            widgets.indexVisibilityApplied = true;
        }
    }

    // A recycled slot can keep the same focus state but represent a different
    // semantic color. Its previous highlight style is no longer authoritative.
    if (widgets.boundIndex != index && current_props_.iconColors) {
        widgets.highlightStyleApplied = false;
    }
    widgets.boundIndex = index;
    applyHighlightStyle(widgets, isSelected);
}

FLASHMEM void VirtualListSelectorOverlay::updateSlotHighlight(widget::VirtualSlot& slot, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = slot.boundIndex - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;

    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    applyHighlightStyle(widgets, isSelected);
}

FLASHMEM void VirtualListSelectorOverlay::ensureSlotWidgets(lv_obj_t* container, int slotIndex) {
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    if (widgets.created || !container) return;

    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(container, PAD_H, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(container, COL_GAP, LV_STATE_DEFAULT);

    // All row text is single-line; intrinsic height measurement is unnecessary.
    widgets.indexLabel = lv_label_create(container);
    lv_obj_set_width(widgets.indexLabel, INDEX_W);
    lv_obj_set_style_text_align(widgets.indexLabel, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    if (fonts.list_item_label) {
        lv_obj_set_style_text_font(widgets.indexLabel, fonts.list_item_label, LV_STATE_DEFAULT);
    }
    style::apply(widgets.indexLabel).textColor(base_theme::color::INACTIVE);
    lv_obj_set_height(widgets.indexLabel, lv_obj_get_style_text_font(widgets.indexLabel, LV_PART_MAIN)->line_height);

    widgets.label = lv_label_create(container);
    lv_obj_set_width(widgets.label, 0);
    lv_obj_set_flex_grow(widgets.label, 1);
    lv_label_set_long_mode(widgets.label, LV_LABEL_LONG_DOT);
    if (fonts.list_item_label) {
        lv_obj_set_style_text_font(widgets.label, fonts.list_item_label, LV_STATE_DEFAULT);
    }
    lv_obj_set_height(widgets.label, lv_obj_get_style_text_font(widgets.label, LV_PART_MAIN)->line_height);

    widgets.created = true;
}

FLASHMEM void VirtualListSelectorOverlay::ensureValue(SlotWidgets& widgets) {
    if (widgets.value || !widgets.label) return;
    auto* parent = lv_obj_get_parent(widgets.label);
    if (!parent) return;

    widgets.value = lv_label_create(parent);
    lv_obj_set_width(widgets.value, VALUE_W);
    lv_obj_set_style_text_align(
        widgets.value, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT
    );
    lv_label_set_long_mode(widgets.value, LV_LABEL_LONG_DOT);
    if (fonts.meta_label()) {
        lv_obj_set_style_text_font(
            widgets.value, fonts.meta_label(), LV_STATE_DEFAULT
        );
    }
    lv_obj_add_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(widgets.value, lv_obj_get_style_text_font(widgets.value, LV_PART_MAIN)->line_height);

    widgets.highlightStyleApplied = false;
}

FLASHMEM void VirtualListSelectorOverlay::ensureIcon(SlotWidgets& widgets) {
    if (widgets.icon || !widgets.label) return;
    auto* parent = lv_obj_get_parent(widgets.label);
    if (!parent) return;

    widgets.icon = lv_label_create(parent);
    lv_label_set_text(widgets.icon, "");
    lv_obj_set_width(widgets.icon, ICON_W);
    lv_obj_set_style_text_align(widgets.icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(widgets.icon, LV_LABEL_LONG_CLIP);
    lv_obj_move_to_index(widgets.icon, 1);
    widgets.highlightStyleApplied = false;
}

FLASHMEM void VirtualListSelectorOverlay::applyHighlightStyle(SlotWidgets& widgets, bool isSelected) {
    if (widgets.highlightStyleApplied && widgets.highlighted == isSelected &&
        widgets.dimUnselected == current_props_.dimUnselected) {
        return;
    }

    auto* container = widgets.label
        ? lv_obj_get_parent(widgets.label)
        : nullptr;
    const auto& visuals = detail::resolveListVisualTokens(
        current_props_.visualTokens
    );
    detail::applyListFocusRail(container, isSelected, visuals);
    const lv_opa_t contentOpacity = isSelected
        ? LV_OPA_COVER
        : (current_props_.dimUnselected ? LV_OPA_60 : LV_OPA_80);

    if (widgets.label) {
        style::apply(widgets.label).textColor(
            isSelected ? visuals.primaryTextColor : visuals.secondaryTextColor
        );
        lv_obj_set_style_text_opa(
            widgets.label,
            contentOpacity,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.icon) {
        uint32_t iconColor = isSelected
            ? visuals.primaryTextColor
            : visuals.secondaryTextColor;
        if (current_props_.iconColors && widgets.boundIndex >= 0 &&
            widgets.boundIndex < current_props_.itemCount &&
            current_props_.iconColors[widgets.boundIndex] != 0U) {
            iconColor = current_props_.iconColors[widgets.boundIndex];
        }
        style::apply(widgets.icon).textColor(iconColor);
        lv_obj_set_style_text_opa(
            widgets.icon,
            contentOpacity,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.value) {
        style::apply(widgets.value).textColor(visuals.secondaryTextColor);
        lv_obj_set_style_text_opa(
            widgets.value,
            contentOpacity,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.indexLabel) {
        style::apply(widgets.indexLabel).textColor(visuals.secondaryTextColor);
        lv_obj_set_style_text_opa(
            widgets.indexLabel,
            contentOpacity,
            LV_STATE_DEFAULT
        );
    }

    widgets.highlighted = isSelected;
    widgets.dimUnselected = current_props_.dimUnselected;
    widgets.highlightStyleApplied = true;
}

}  // namespace ms::ui
