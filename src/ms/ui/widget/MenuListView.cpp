#include "MenuListView.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <config/PlatformCompat.hpp>
#include <ms/ui/font/CoreFonts.hpp>
#include <oc/ui/lvgl/style/StyleBuilder.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>

namespace ms::ui {

using namespace oc::ui::lvgl;
namespace style = oc::ui::lvgl::style;

namespace {

constexpr lv_coord_t HEADER_HEIGHT = 32;
constexpr lv_coord_t ITEM_HEIGHT = 28;
constexpr lv_coord_t HEADER_PAD_LEFT = 16;
constexpr lv_coord_t HEADER_PAD_RIGHT = 8;
constexpr lv_coord_t ROW_PAD_LEFT = 16;
constexpr lv_coord_t ROW_PAD_RIGHT = 16;
constexpr lv_coord_t VALUE_COL_W = 106;
constexpr lv_coord_t DESCRIPTION_LABEL_COL_W = 136;
constexpr lv_coord_t DESCRIPTION_VALUE_COL_W = 144;
constexpr lv_coord_t COL_GAP = base_theme::layout::SPACE_MD;
constexpr uint32_t DESCRIPTION_VALUE_COLOR = 0x8A8A8A;

uint32_t valueColorFor(MenuRowKind kind) {
    switch (kind) {
        case MenuRowKind::Folder:
            return base_theme::color::MACRO_5_CYAN;
        case MenuRowKind::Toggle:
            return base_theme::color::MACRO_4_GREEN;
        case MenuRowKind::Action:
            return base_theme::color::MACRO_2_ORANGE;
        case MenuRowKind::Disabled:
        case MenuRowKind::Value:
        default:
            return base_theme::color::ACTIVE;
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
    lv_obj_set_style_text_font(title_, fonts.inter_14_bold, 0);
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
        const bool nextEnabled = row ? row->enabled : true;
        const bool nextValueAutoScroll = row ? row->valueAutoScroll : false;
        const MenuRowValueRole nextValueRole = row ? row->valueRole : MenuRowValueRole::Value;
        const bool labelChanged = copyTextIfChanged(current.label, row ? row->label : "");
        const bool valueChanged = copyTextIfChanged(current.value, row ? row->value : "");
        const bool kindChanged = current.kind != nextKind;
        const bool enabledChanged = current.enabled != nextEnabled;
        const bool valueAutoScrollChanged = current.valueAutoScroll != nextValueAutoScroll;
        const bool valueRoleChanged = current.valueRole != nextValueRole;
        current.kind = nextKind;
        current.enabled = nextEnabled;
        current.valueAutoScroll = nextValueAutoScroll;
        current.valueRole = nextValueRole;
        if ((labelChanged || valueChanged || kindChanged || enabledChanged ||
             valueAutoScrollChanged || valueRoleChanged) &&
            dirtyCount < MAX_ROWS) {
            dirtyIndices[static_cast<std::size_t>(dirtyCount++)] = i;
        }
    }

    for (int i = nextCount; i < row_count_; ++i) {
        auto& current = rows_[static_cast<std::size_t>(i)];
        copyTextIfChanged(current.label, "");
        copyTextIfChanged(current.value, "");
        current.kind = MenuRowKind::Value;
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

    setLabelTextIfChanged(title_, title_cache_, props.title);
    setLabelTextIfChanged(meta_, meta_cache_, props.meta);

    std::array<int, MAX_ROWS> dirtyIndices{};
    int dirtyCount = 0;
    syncRows(props, dirtyIndices, dirtyCount);

    if (list_) {
        const bool countChanged = list_->setTotalCount(row_count_);
        list_->setSelectedIndex(props.selectedIndex);
        if (!countChanged && list_->isVisible()) {
            invalidateDirtyRows(dirtyIndices, dirtyCount);
        }
    }
}

FLASHMEM void MenuListView::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    if (!list_) return;

    const int slotIndex = index - list_->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (index < 0 || index >= row_count_) return;

    ensureSlotWidgets(slot.container, slotIndex);
    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    const auto& row = rows_[static_cast<std::size_t>(index)];

    applyValueLayout(widgets, row.valueRole);
    setLabelTextIfChanged(widgets.label, widgets.labelCache, row.label.text);
    if (row.valueAutoScroll) {
        ensureValueScroller(widgets, row.valueRole);
        if (widgets.value) {
            lv_obj_add_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
        }
        if (widgets.valueScroller) {
            lv_obj_clear_flag(widgets.valueScroller->getElement(), LV_OBJ_FLAG_HIDDEN);
        }
        setLabelTextIfChanged(widgets.valueScroller.get(), widgets.valueScrollerCache, row.value.text);
    } else {
        if (widgets.valueScroller) {
            if (widgets.valueScrollerActive) {
                widgets.valueScroller->autoScroll(false);
                widgets.valueScroller->setText("");
                widgets.valueScrollerActive = false;
                widgets.valueScrollerCache = {};
            }
            lv_obj_add_flag(widgets.valueScroller->getElement(), LV_OBJ_FLAG_HIDDEN);
        }
        if (widgets.value) {
            lv_obj_clear_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
        }
        setLabelTextIfChanged(widgets.value, widgets.valueCache, row.value.text);
    }
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

FLASHMEM void MenuListView::ensureValueScroller(SlotWidgets& widgets, MenuRowValueRole role) {
    if (widgets.valueScroller) {
        applyValueLayout(widgets, role);
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
    applyValueLayout(widgets, role);
}

FLASHMEM void MenuListView::applyValueLayout(SlotWidgets& widgets, MenuRowValueRole role) {
    if (widgets.valueLayoutApplied && widgets.valueRole == role) return;

    const bool description = role == MenuRowValueRole::Description;
    if (widgets.label) {
        if (description) {
            lv_obj_set_flex_grow(widgets.label, 0);
            lv_obj_set_width(widgets.label, DESCRIPTION_LABEL_COL_W);
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
    }

    widgets.valueRole = role;
    widgets.valueLayoutApplied = true;
}

FLASHMEM void MenuListView::applyHighlightStyle(
    widget::VirtualSlot& slot,
    SlotWidgets& widgets,
    bool isSelected,
    const RowCache& row
) {
    (void)slot;
    const bool shouldScroll = row.valueAutoScroll && isSelected;
    if (widgets.highlightStyleApplied &&
        widgets.highlighted == isSelected &&
        widgets.valueScrollerActive == shouldScroll) {
        return;
    }

    widgets.highlighted = isSelected;
    widgets.highlightStyleApplied = true;

    if (widgets.valueScroller && widgets.valueScrollerActive != shouldScroll) {
        widgets.valueScroller->autoScroll(shouldScroll);
        widgets.valueScroller->setText(widgets.valueScrollerCache.text);
        widgets.valueScrollerActive = shouldScroll;
    }
}

FLASHMEM void MenuListView::applyRowStyle(SlotWidgets& widgets, const RowCache& row) {
    const bool enabled = row.enabled && row.kind != MenuRowKind::Disabled;
    const bool description = row.valueRole == MenuRowValueRole::Description;
    const uint32_t labelColor = enabled ? base_theme::color::TEXT_PRIMARY
                                        : base_theme::color::INACTIVE_LIGHTER;
    const uint32_t valueColor = enabled ? (description ? DESCRIPTION_VALUE_COLOR
                                                       : valueColorFor(row.kind))
                                        : base_theme::color::INACTIVE_LIGHTER;
    const lv_opa_t labelOpa = enabled ? LV_OPA_80 : LV_OPA_60;
    const lv_opa_t valueOpa = enabled ? (description ? LV_OPA_70 : LV_OPA_80)
                                      : LV_OPA_50;

    if (!widgets.rowStyleApplied || widgets.labelColor != labelColor) {
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
}

}  // namespace ms::ui
