#include "MenuListView.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <config/PlatformCompat.hpp>
#include <ms/ui/font/CoreFonts.hpp>
#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>

#include <ms/ui/widget/TextOverflow.hpp>

namespace ms::ui {

using namespace oc::ui::lvgl;
namespace style = oc::ui::lvgl::style;

namespace {

constexpr lv_coord_t HEADER_HEIGHT = 32;
constexpr lv_coord_t STACKED_HEADER_HEIGHT = 40;
constexpr lv_coord_t ITEM_HEIGHT = 28;
constexpr lv_coord_t HEADER_PAD_LEFT = 16;
constexpr lv_coord_t HEADER_PAD_RIGHT = 8;
constexpr lv_coord_t ROW_PAD_LEFT = 16;
constexpr lv_coord_t ROW_PAD_RIGHT = 16;
constexpr lv_coord_t ICON_W = 16;
constexpr lv_coord_t VALUE_COL_W = 106;
constexpr lv_coord_t DESCRIPTION_LABEL_COL_W = 136;
constexpr lv_coord_t DESCRIPTION_VALUE_COL_W = 144;
constexpr lv_coord_t COL_GAP = base_theme::layout::SPACE_MD;

uint32_t valueColorFor(
    MenuRowTone tone,
    const ListVisualTokens& visuals
) {
    switch (tone) {
        case MenuRowTone::Positive:
            return visuals.positiveTextColor;
        case MenuRowTone::Warning:
            return visuals.warningTextColor;
        case MenuRowTone::Destructive:
            return visuals.destructiveTextColor;
        case MenuRowTone::Neutral:
        default:
            return visuals.secondaryTextColor;
    }
}

}  // namespace

FLASHMEM MenuListView::MenuListView(lv_obj_t* parent) {
    createUi(parent);
}

FLASHMEM MenuListView::~MenuListView() {
    list_.reset();
    if (container_) {
        lv_obj_delete(container_);
        container_ = nullptr;
        header_ = nullptr;
        title_ = nullptr;
        meta_ = nullptr;
    }
}

FLASHMEM bool MenuListView::copyTextIfChanged(TextCache& cache, const char* text) {
    const char* source = text ? text : "";
    char next[TEXT_CACHE_SIZE] = {};
    std::strncpy(next, source, TEXT_CACHE_SIZE - 1);
    next[TEXT_CACHE_SIZE - 1] = '\0';

    if (std::strncmp(cache.text, next, TEXT_CACHE_SIZE) == 0) return false;

    std::strncpy(cache.text, next, TEXT_CACHE_SIZE - 1);
    cache.text[TEXT_CACHE_SIZE - 1] = '\0';
    return true;
}

FLASHMEM bool MenuListView::copyIconIfChanged(IconCache& cache, const char* text) {
    const char* source = text ? text : "";
    char next[ICON_CACHE_SIZE] = {};
    std::strncpy(next, source, ICON_CACHE_SIZE - 1);
    next[ICON_CACHE_SIZE - 1] = '\0';

    if (std::strncmp(cache.text, next, ICON_CACHE_SIZE) == 0) return false;

    std::strncpy(cache.text, next, ICON_CACHE_SIZE - 1);
    cache.text[ICON_CACHE_SIZE - 1] = '\0';
    return true;
}

FLASHMEM void MenuListView::setLabelTextIfChanged(
    lv_obj_t* label,
    TextCache& cache,
    const char* text
) {
    if (!label) return;
    if (!copyTextIfChanged(cache, text)) return;
    lv_label_set_text(label, cache.text);
}

FLASHMEM void MenuListView::setLabelTextIfChanged(
    oc::ui::lvgl::Label* label,
    TextCache& cache,
    const char* text
) {
    if (!label) return;
    if (!copyTextIfChanged(cache, text)) return;
    label->setText(cache.text);
}

FLASHMEM void MenuListView::createUi(lv_obj_t* parent) {
    if (!parent) return;

    container_ = lv_obj_create(parent);
    style::apply(container_)
        .size(LV_PCT(100), LV_PCT(100))
        .transparent()
        .noBorder()
        .pad(0)
        .noScroll();
    lv_obj_set_layout(container_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(
        container_,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );
    lv_obj_set_style_pad_row(container_, 0, 0);

    header_ = lv_obj_create(container_);
    style::apply(header_)
        .size(LV_PCT(100), HEADER_HEIGHT)
        .transparent()
        .noBorder()
        .pad(0)
        .noScroll();
    lv_obj_set_style_pad_left(header_, HEADER_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(header_, HEADER_PAD_RIGHT, 0);
    lv_obj_set_layout(header_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header_, 12, 0);

    title_ = lv_label_create(header_);
    lv_label_set_text(title_, "");
    lv_obj_set_style_text_font(title_, fonts.context_title(), 0);
    lv_obj_set_style_text_color(title_, lv_color_hex(base_theme::color::TEXT_PRIMARY), 0);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title_, 132);

    meta_ = lv_label_create(header_);
    lv_label_set_text(meta_, "");
    lv_obj_set_flex_grow(meta_, 1);
    lv_obj_set_style_text_font(meta_, fonts.inter_12_medium, 0);
    lv_obj_set_style_text_color(meta_, lv_color_hex(base_theme::color::TEXT_SECONDARY), 0);
    lv_obj_set_style_text_opa(meta_, LV_OPA_80, 0);
    lv_label_set_long_mode(meta_, LV_LABEL_LONG_DOT);

    list_ = std::make_unique<widget::VirtualList>(container_);
    list_->visibleCount(VISIBLE_SLOTS)
        .itemHeight(ITEM_HEIGHT)
        .scrollMode(widget::ScrollMode::PageBased)
        .padding(0)
        .itemGap(0)
        .marginH(0)
        .onBindSlot([this](widget::VirtualSlot& slot, int index, bool isSelected) {
            bindSlot(slot, index, isSelected);
        })
        .onUpdateHighlight([this](widget::VirtualSlot& slot, bool isSelected) {
            updateSlotHighlight(slot, isSelected);
        });
    list_->prepare();
    const auto& slots = list_->getSlots();
    for (int i = 0; i < VISIBLE_SLOTS && i < static_cast<int>(slots.size()); ++i) {
        ensureSlotWidgets(slots[static_cast<std::size_t>(i)].container, i);
    }
    list_->show();
}

FLASHMEM void MenuListView::show() {
    if (container_) {
        lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
}

FLASHMEM void MenuListView::hide() {
    if (container_) {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
}

FLASHMEM void MenuListView::syncRows(
    const MenuListViewProps& props,
    std::array<int, MAX_ROWS>& dirtyIndices,
    int& dirtyCount
) {
    dirtyCount = 0;
    const int nextCount = std::clamp(props.rowCount, 0, MAX_ROWS);
    const bool canSkipRowDiff =
        props.dataRevision != 0 &&
        props.dataRevision == last_data_revision_ &&
        nextCount == last_row_count_;

    if (canSkipRowDiff) return;

    for (int i = 0; i < nextCount; ++i) {
        const auto* row = props.rows ? &props.rows[i] : nullptr;
        auto& current = rows_[static_cast<std::size_t>(i)];
        const MenuRowKind nextKind = row ? row->kind : MenuRowKind::Value;
        const MenuRowTone nextTone = row ? row->tone : MenuRowTone::Neutral;
        const bool nextEnabled = row ? row->enabled : true;
        const bool nextValueAutoScroll = row ? row->valueAutoScroll : false;
        const MenuRowValueRole nextValueRole = row
            ? row->valueRole
            : MenuRowValueRole::Value;
        const bool labelChanged = copyTextIfChanged(current.label, row ? row->label : "");
        const bool valueChanged = copyTextIfChanged(current.value, row ? row->value : "");
        const bool iconChanged = copyIconIfChanged(current.icon, row ? row->icon : "");
        const bool kindChanged = current.kind != nextKind;
        const bool toneChanged = current.tone != nextTone;
        const bool enabledChanged = current.enabled != nextEnabled;
        const bool valueAutoScrollChanged = current.valueAutoScroll != nextValueAutoScroll;
        const bool valueRoleChanged = current.valueRole != nextValueRole;
        current.kind = nextKind;
        current.tone = nextTone;
        current.enabled = nextEnabled;
        current.valueAutoScroll = nextValueAutoScroll;
        current.valueRole = nextValueRole;
        if ((labelChanged || valueChanged || iconChanged || kindChanged || toneChanged ||
             enabledChanged || valueAutoScrollChanged || valueRoleChanged) &&
            dirtyCount < MAX_ROWS) {
            dirtyIndices[static_cast<std::size_t>(dirtyCount++)] = i;
        }
    }

    for (int i = nextCount; i < row_count_; ++i) {
        auto& current = rows_[static_cast<std::size_t>(i)];
        copyTextIfChanged(current.label, "");
        copyTextIfChanged(current.value, "");
        copyIconIfChanged(current.icon, "");
        current.kind = MenuRowKind::Value;
        current.tone = MenuRowTone::Neutral;
        current.enabled = true;
        current.valueAutoScroll = false;
        current.valueRole = MenuRowValueRole::Value;
    }

    last_data_revision_ = props.dataRevision;
    last_row_count_ = nextCount;
    row_count_ = nextCount;
}

FLASHMEM void MenuListView::invalidateDirtyRows(
    const std::array<int, MAX_ROWS>& dirtyIndices,
    int dirtyCount
) {
    if (!list_ || dirtyCount <= 0) return;

    for (int i = 0; i < dirtyCount; ++i) {
        list_->invalidateIndex(dirtyIndices[static_cast<std::size_t>(i)]);
    }
}

FLASHMEM void MenuListView::render(const MenuListViewProps& props) {
    if (!container_) return;

    const auto* nextVisuals = props.visualTokens
        ? props.visualTokens
        : &DEFAULT_LIST_VISUAL_TOKENS;
    const bool visualsChanged = visual_tokens_ != nextVisuals;
    const bool iconFontChanged = icon_font_ != props.iconFont;
    visual_tokens_ = nextVisuals;
    icon_font_ = props.iconFont;
    if (visualsChanged && list_) {
        const auto& visuals = *visual_tokens_;
        lv_obj_set_style_text_color(
            title_, lv_color_hex(visuals.primaryTextColor), LV_STATE_DEFAULT
        );
        lv_obj_set_style_text_color(
            meta_, lv_color_hex(visuals.secondaryTextColor), LV_STATE_DEFAULT
        );
        list_->selectionCursorStyle(
            visuals.selectedSurfaceColor,
            visuals.selectedSurfaceOpacity,
            visuals.selectedSurfaceRadius
        );
        for (auto& widgets : slot_widgets_) {
            widgets.rowStyleApplied = false;
            widgets.highlightStyleApplied = false;
        }
    }
    if (iconFontChanged) {
        for (auto& widgets : slot_widgets_) {
            widgets.iconFont = nullptr;
        }
    }

    applyHeaderLayout(props.headerLayout);
    setLabelTextIfChanged(title_, title_cache_, props.title);
    setLabelTextIfChanged(meta_, meta_cache_, props.meta);

    std::array<int, MAX_ROWS> dirtyIndices{};
    int dirtyCount = 0;
    syncRows(props, dirtyIndices, dirtyCount);

    if (list_) {
        const bool countChanged = list_->setTotalCount(row_count_);
        list_->setSelectedIndex(props.selectedIndex);
        if (!countChanged && list_->isVisible()) {
            if (visualsChanged || iconFontChanged) {
                list_->invalidate();
            } else {
                invalidateDirtyRows(dirtyIndices, dirtyCount);
            }
        }
    }
}

FLASHMEM void MenuListView::applyHeaderLayout(MenuListHeaderLayout layout) {
    if (!header_ || !title_ || !meta_) return;
    if (header_layout_applied_ && header_layout_ == layout) return;

    const bool stacked = layout == MenuListHeaderLayout::Stacked;
    lv_obj_set_height(header_, stacked ? STACKED_HEADER_HEIGHT : HEADER_HEIGHT);
    lv_obj_set_flex_flow(header_, stacked ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        header_,
        LV_FLEX_ALIGN_START,
        stacked ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER,
        stacked ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER
    );
    lv_obj_set_style_pad_column(header_, stacked ? 0 : 12, 0);
    lv_obj_set_style_pad_row(header_, 0, 0);
    lv_obj_set_style_pad_top(header_, stacked ? 2 : 0, 0);
    lv_obj_set_style_pad_bottom(header_, stacked ? 2 : 0, 0);

    lv_obj_set_flex_grow(title_, 0);
    lv_obj_set_width(title_, stacked ? LV_PCT(100) : 132);
    lv_obj_set_flex_grow(meta_, stacked ? 0 : 1);
    lv_obj_set_width(meta_, stacked ? LV_PCT(100) : 0);

    header_layout_ = layout;
    header_layout_applied_ = true;
}

FLASHMEM void MenuListView::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    if (!list_) return;

    const int slotIndex = index - list_->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (index < 0 || index >= row_count_) return;

    ensureSlotWidgets(slot.container, slotIndex);
    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    const auto& row = rows_[static_cast<std::size_t>(index)];

    const bool iconVisible = icon_font_ && row.icon.text[0] != '\0';
    if (iconVisible) ensureIcon(widgets);
    if (widgets.icon) {
        if (icon_font_ && widgets.iconFont != icon_font_) {
            lv_obj_set_style_text_font(widgets.icon, icon_font_, 0);
            widgets.iconFont = icon_font_;
        }
        if (copyIconIfChanged(widgets.iconCache, row.icon.text)) {
            lv_label_set_text(widgets.icon, widgets.iconCache.text);
        }
        if (iconVisible) {
            lv_obj_clear_flag(widgets.icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
    applyValueLayout(widgets, row.valueRole, iconVisible);
    setLabelTextIfChanged(widgets.label, widgets.labelCache, row.label.text);
    syncValuePresentation(widgets, row, isSelected);
    applyRowStyle(widgets, row);

    widgets.boundIndex = index;
    applyHighlightStyle(slot, widgets, isSelected, row);
}

FLASHMEM void MenuListView::updateSlotHighlight(widget::VirtualSlot& slot, bool isSelected) {
    if (!list_) return;

    const int slotIndex = slot.boundIndex - list_->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (slot.boundIndex < 0 || slot.boundIndex >= row_count_) return;

    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    const auto& row = rows_[static_cast<std::size_t>(slot.boundIndex)];
    applyHighlightStyle(slot, widgets, isSelected, row);
}

FLASHMEM void MenuListView::ensureSlotWidgets(lv_obj_t* row, int slotIndex) {
    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    if (widgets.created || !row) return;

    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(row, ROW_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(row, ROW_PAD_RIGHT, 0);
    lv_obj_set_style_pad_column(row, COL_GAP, 0);

    widgets.label = lv_label_create(row);
    lv_label_set_text(widgets.label, "");
    lv_obj_set_flex_grow(widgets.label, 1);
    lv_label_set_long_mode(widgets.label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(widgets.label, fonts.list_item_label, 0);

    widgets.value = lv_label_create(row);
    lv_label_set_text(widgets.value, "");
    lv_obj_set_width(widgets.value, VALUE_COL_W);
    lv_obj_set_style_text_align(widgets.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(widgets.value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(widgets.value, fonts.inter_14_semibold, 0);

    widgets.created = true;
}

FLASHMEM void MenuListView::ensureIcon(SlotWidgets& widgets) {
    if (widgets.icon || !widgets.label) return;
    auto* parent = lv_obj_get_parent(widgets.label);
    if (!parent) return;

    widgets.icon = lv_label_create(parent);
    lv_label_set_text(widgets.icon, "");
    lv_obj_set_width(widgets.icon, ICON_W);
    lv_obj_set_style_text_align(widgets.icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(widgets.icon, LV_LABEL_LONG_CLIP);
    lv_obj_move_to_index(widgets.icon, 0);
    widgets.rowStyleApplied = false;
    widgets.highlightStyleApplied = false;
}

FLASHMEM void MenuListView::ensureValueScroller(SlotWidgets& widgets, MenuRowValueRole role) {
    if (widgets.valueScroller) {
        applyValueLayout(widgets, role, widgets.iconVisible);
        return;
    }

    lv_obj_t* parent = widgets.value ? lv_obj_get_parent(widgets.value) : nullptr;
    if (!parent) return;

    widgets.valueScroller = std::make_unique<Label>(parent);
    widgets.valueScroller->font(fonts.inter_14_semibold)
        .autoScroll(false)
        .ownsLvglObjects(false);
    lv_obj_add_flag(widgets.valueScroller->getElement(), LV_OBJ_FLAG_HIDDEN);
    widgets.valueLayoutApplied = false;
    applyValueLayout(widgets, role, widgets.iconVisible);
}

FLASHMEM void MenuListView::applyValueLayout(
    SlotWidgets& widgets,
    MenuRowValueRole role,
    bool iconVisible
) {
    if (widgets.valueLayoutApplied && widgets.valueRole == role &&
        widgets.iconVisible == iconVisible) {
        return;
    }

    const bool description = role == MenuRowValueRole::Description;
    if (widgets.label) {
        if (description) {
            lv_obj_set_flex_grow(widgets.label, 0);
            lv_obj_set_width(
                widgets.label,
                DESCRIPTION_LABEL_COL_W - (iconVisible ? ICON_W + COL_GAP : 0)
            );
        } else {
            lv_obj_set_width(widgets.label, 0);
            lv_obj_set_flex_grow(widgets.label, 1);
        }
    }
    if (widgets.value) {
        lv_obj_set_width(widgets.value, description ? DESCRIPTION_VALUE_COL_W : VALUE_COL_W);
        lv_obj_set_style_text_align(
            widgets.value,
            description ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT,
            0
        );
    }
    if (widgets.valueScroller) {
        widgets.valueScroller->width(description ? DESCRIPTION_VALUE_COL_W : VALUE_COL_W)
            .alignment(description ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT);
        widgets.valueScrollerCache = {};
    }

    widgets.valueRole = role;
    widgets.iconVisible = iconVisible;
    widgets.valueLayoutApplied = true;
}

FLASHMEM void MenuListView::syncValuePresentation(
    SlotWidgets& widgets,
    const RowCache& row,
    bool isSelected
) {
    const bool shouldScroll = row.valueAutoScroll && isSelected;
    if (row.valueAutoScroll) ensureValueScroller(widgets, row.valueRole);

    if (shouldScroll && widgets.valueScroller) {
        if (widgets.value) lv_obj_add_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(widgets.valueScroller->getElement(), LV_OBJ_FLAG_HIDDEN);
        widgets.valueScroller->autoScroll(true);
        setLabelTextIfChanged(
            widgets.valueScroller.get(), widgets.valueScrollerCache, row.value.text);
        widgets.valueScrollerActive = true;
        return;
    }

    if (widgets.valueScroller) {
        if (widgets.valueScrollerActive) {
            widgets.valueScroller->autoScroll(false);
            widgets.valueScroller->setText("");
            widgets.valueScrollerCache = {};
        }
        widgets.valueScrollerActive = false;
        lv_obj_add_flag(widgets.valueScroller->getElement(), LV_OBJ_FLAG_HIDDEN);
    }
    if (!widgets.value) return;

    lv_obj_clear_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
    char displayed[TEXT_CACHE_SIZE] = {};
    const lv_coord_t width = row.valueRole == MenuRowValueRole::Description
        ? DESCRIPTION_VALUE_COL_W
        : VALUE_COL_W;
    text::formatEllipsized(
        displayed, sizeof(displayed), row.value.text,
        fonts.inter_14_semibold ? fonts.inter_14_semibold : LV_FONT_DEFAULT,
        width);
    setLabelTextIfChanged(widgets.value, widgets.valueCache, displayed);
}

FLASHMEM void MenuListView::applyHighlightStyle(
    widget::VirtualSlot& slot,
    SlotWidgets& widgets,
    bool isSelected,
    const RowCache& row
) {
    const bool shouldScroll = row.valueAutoScroll && isSelected;
    if (widgets.highlightStyleApplied &&
        widgets.highlighted == isSelected &&
        widgets.valueScrollerActive == shouldScroll) {
        return;
    }

    widgets.highlighted = isSelected;
    widgets.highlightStyleApplied = true;

    detail::applyListFocusRail(slot.container, isSelected, *visual_tokens_);

    if (widgets.label) {
        lv_obj_set_style_text_opa(
            widgets.label,
            isSelected ? LV_OPA_COVER : widgets.labelOpa,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.icon) {
        lv_obj_set_style_text_opa(
            widgets.icon,
            isSelected ? LV_OPA_COVER : widgets.labelOpa,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.value) {
        lv_obj_set_style_text_opa(
            widgets.value,
            isSelected ? LV_OPA_COVER : widgets.valueOpa,
            LV_STATE_DEFAULT
        );
    }
    if (widgets.valueScroller) {
        lv_obj_set_style_text_opa(
            widgets.valueScroller->getLabel(),
            isSelected ? LV_OPA_COVER : widgets.valueOpa,
            LV_STATE_DEFAULT
        );
    }

    syncValuePresentation(widgets, row, isSelected);
}

FLASHMEM void MenuListView::applyRowStyle(SlotWidgets& widgets, const RowCache& row) {
    const bool enabled = row.enabled && row.kind != MenuRowKind::Disabled;
    const bool description = row.valueRole == MenuRowValueRole::Description;
    const auto& visuals = *visual_tokens_;
    const uint32_t labelColor = enabled ? visuals.primaryTextColor
                                        : visuals.disabledTextColor;
    const uint32_t valueColor = enabled ? (description ? visuals.secondaryTextColor
                                                       : valueColorFor(row.tone, visuals))
                                        : visuals.disabledTextColor;
    const lv_opa_t labelOpa = enabled ? LV_OPA_80 : LV_OPA_60;
    const lv_opa_t valueOpa = enabled ? (description ? LV_OPA_70 : LV_OPA_80)
                                      : LV_OPA_50;

    if (!widgets.rowStyleApplied || widgets.labelColor != labelColor) {
        if (widgets.icon) {
            lv_obj_set_style_text_color(widgets.icon, lv_color_hex(labelColor), 0);
        }
        if (widgets.label) {
            lv_obj_set_style_text_color(widgets.label, lv_color_hex(labelColor), 0);
        }
        widgets.labelColor = labelColor;
    }
    if (!widgets.rowStyleApplied || widgets.valueColor != valueColor) {
        if (widgets.value) {
            lv_obj_set_style_text_color(widgets.value, lv_color_hex(valueColor), 0);
        }
        if (widgets.valueScroller) {
            lv_obj_set_style_text_color(widgets.valueScroller->getLabel(), lv_color_hex(valueColor), 0);
        }
        widgets.valueColor = valueColor;
    }
    if (!widgets.rowStyleApplied || widgets.labelOpa != labelOpa) {
        if (widgets.icon) {
            lv_obj_set_style_text_opa(widgets.icon, labelOpa, 0);
        }
        if (widgets.label) {
            lv_obj_set_style_text_opa(widgets.label, labelOpa, 0);
        }
        widgets.labelOpa = labelOpa;
    }
    if (!widgets.rowStyleApplied || widgets.valueOpa != valueOpa) {
        if (widgets.value) {
            lv_obj_set_style_text_opa(widgets.value, valueOpa, 0);
        }
        if (widgets.valueScroller) {
            lv_obj_set_style_text_opa(widgets.valueScroller->getLabel(), valueOpa, 0);
        }
        widgets.valueOpa = valueOpa;
    }
    widgets.rowStyleApplied = true;
    widgets.highlightStyleApplied = false;
}

}  // namespace ms::ui
