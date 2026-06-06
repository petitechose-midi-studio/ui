#include "MenuListView.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <config/PlatformCompat.hpp>
#include <ms/ui/font/CoreFonts.hpp>
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    #include <oc/log/Log.hpp>
    #include <oc/time/Time.hpp>
#endif
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
constexpr lv_coord_t COL_GAP = base_theme::layout::SPACE_MD;

#if defined(MS_PROJECT_VIEW_PERF_LOG)
constexpr uint32_t MENU_LIST_PERF_WINDOW_PASSES = 8;
constexpr uint32_t MENU_LIST_PERF_SLOW_TOTAL_US = 1500;

uint32_t menuListPerfMicros() {
    return oc::time::isMicrosConfigured() ? oc::time::micros32() : 0;
}

struct MenuListPerfCallbacks {
    uint32_t binds = 0;
    uint32_t highlights = 0;
};

struct MenuListPerfWindow {
    uint32_t pass_count = 0;
    uint64_t total_us = 0;
    uint64_t header_us = 0;
    uint64_t sync_us = 0;
    uint64_t count_us = 0;
    uint64_t select_us = 0;
    uint64_t invalidate_us = 0;
    uint32_t max_total_us = 0;
    uint32_t max_header_us = 0;
    uint32_t max_sync_us = 0;
    uint32_t max_count_us = 0;
    uint32_t max_select_us = 0;
    uint32_t max_invalidate_us = 0;
    uint32_t bind_count = 0;
    uint32_t highlight_count = 0;
    uint32_t dirty_row_count = 0;
    uint32_t count_change_count = 0;
    int last_row_count = 0;
    int last_selected_index = 0;
    uint32_t last_data_revision = 0;

    static uint32_t avg(uint64_t total, uint32_t count) {
        return count > 0 ? static_cast<uint32_t>(total / count) : 0;
    }

    static uint32_t maxOf(uint32_t current, uint32_t next) {
        return next > current ? next : current;
    }

    void reset() {
        pass_count = 0;
        total_us = 0;
        header_us = 0;
        sync_us = 0;
        count_us = 0;
        select_us = 0;
        invalidate_us = 0;
        max_total_us = 0;
        max_header_us = 0;
        max_sync_us = 0;
        max_count_us = 0;
        max_select_us = 0;
        max_invalidate_us = 0;
        bind_count = 0;
        highlight_count = 0;
        dirty_row_count = 0;
        count_change_count = 0;
    }

    void record(
        uint32_t total,
        uint32_t header,
        uint32_t sync,
        uint32_t count,
        uint32_t select,
        uint32_t invalidate,
        uint32_t binds,
        uint32_t highlights,
        uint32_t dirtyRows,
        bool countChanged,
        int rowCount,
        int selectedIndex,
        uint32_t dataRevision
    ) {
        pass_count += 1;
        total_us += total;
        header_us += header;
        sync_us += sync;
        count_us += count;
        select_us += select;
        invalidate_us += invalidate;
        max_total_us = maxOf(max_total_us, total);
        max_header_us = maxOf(max_header_us, header);
        max_sync_us = maxOf(max_sync_us, sync);
        max_count_us = maxOf(max_count_us, count);
        max_select_us = maxOf(max_select_us, select);
        max_invalidate_us = maxOf(max_invalidate_us, invalidate);
        bind_count += binds;
        highlight_count += highlights;
        dirty_row_count += dirtyRows;
        count_change_count += countChanged ? 1 : 0;
        last_row_count = rowCount;
        last_selected_index = selectedIndex;
        last_data_revision = dataRevision;

        const bool slow = total >= MENU_LIST_PERF_SLOW_TOTAL_US;
        const bool active = binds > 0 || highlights > 0 || dirtyRows > 0 || countChanged;
        if (!slow && pass_count < MENU_LIST_PERF_WINDOW_PASSES) return;
        if (!slow && !active && bind_count == 0 && highlight_count == 0 && dirty_row_count == 0 && count_change_count == 0) {
            reset();
            return;
        }

        OC_LOG_INFO(
            "[Perf][MenuList] passes={} avg={}us max={}us header={}/{}us sync={}/{}us count={}/{}us select={}/{}us inv={}/{}us binds={} highlights={} dirty={} countChanges={} rows={} sel={} rev={}",
            pass_count,
            avg(total_us, pass_count),
            max_total_us,
            avg(header_us, pass_count),
            max_header_us,
            avg(sync_us, pass_count),
            max_sync_us,
            avg(count_us, pass_count),
            max_count_us,
            avg(select_us, pass_count),
            max_select_us,
            avg(invalidate_us, pass_count),
            max_invalidate_us,
            bind_count,
            highlight_count,
            dirty_row_count,
            count_change_count,
            last_row_count,
            last_selected_index,
            last_data_revision
        );
        reset();
    }
};

MenuListPerfCallbacks g_menu_list_perf_callbacks;
MenuListPerfWindow g_menu_list_perf;

void recordMenuListPerf(
    uint32_t total,
    uint32_t header,
    uint32_t sync,
    uint32_t count,
    uint32_t select,
    uint32_t invalidate,
    uint32_t binds,
    uint32_t highlights,
    uint32_t dirtyRows,
    bool countChanged,
    int rowCount,
    int selectedIndex,
    uint32_t dataRevision
) {
    g_menu_list_perf.record(
        total,
        header,
        sync,
        count,
        select,
        invalidate,
        binds,
        highlights,
        dirtyRows,
        countChanged,
        rowCount,
        selectedIndex,
        dataRevision
    );
}
#endif

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
    lv_obj_set_style_text_font(title_, fonts.inter_14_bold, 0);
    lv_obj_set_style_text_color(title_, lv_color_hex(base_theme::color::TEXT_PRIMARY), 0);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title_, 84);

    meta_ = lv_label_create(header_);
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
        const bool labelChanged = copyTextIfChanged(current.label, row ? row->label : "");
        const bool valueChanged = copyTextIfChanged(current.value, row ? row->value : "");
        const bool kindChanged = current.kind != nextKind;
        const bool enabledChanged = current.enabled != nextEnabled;
        current.kind = nextKind;
        current.enabled = nextEnabled;
        if ((labelChanged || valueChanged || kindChanged || enabledChanged) &&
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

#if defined(MS_PROJECT_VIEW_PERF_LOG)
    const uint32_t total_start_us = menuListPerfMicros();
    const uint32_t binds_before = g_menu_list_perf_callbacks.binds;
    const uint32_t highlights_before = g_menu_list_perf_callbacks.highlights;
    const uint32_t header_start_us = total_start_us;
#endif
    setLabelTextIfChanged(title_, title_cache_, props.title);
    setLabelTextIfChanged(meta_, meta_cache_, props.meta);
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    const uint32_t header_us = menuListPerfMicros() - header_start_us;
#endif

    std::array<int, MAX_ROWS> dirtyIndices{};
    int dirtyCount = 0;
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    const uint32_t sync_start_us = menuListPerfMicros();
#endif
    syncRows(props, dirtyIndices, dirtyCount);
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    const uint32_t sync_us = menuListPerfMicros() - sync_start_us;
    uint32_t count_us = 0;
    uint32_t select_us = 0;
    uint32_t invalidate_us = 0;
    bool countChanged = false;
#endif

    if (list_) {
#if defined(MS_PROJECT_VIEW_PERF_LOG)
        const uint32_t count_start_us = menuListPerfMicros();
        countChanged = list_->setTotalCount(row_count_);
        count_us = menuListPerfMicros() - count_start_us;
        const uint32_t select_start_us = menuListPerfMicros();
#else
        const bool countChanged = list_->setTotalCount(row_count_);
#endif
        list_->setSelectedIndex(props.selectedIndex);
#if defined(MS_PROJECT_VIEW_PERF_LOG)
        select_us = menuListPerfMicros() - select_start_us;
        const uint32_t invalidate_start_us = menuListPerfMicros();
#endif
        if (!countChanged && list_->isVisible()) {
            invalidateDirtyRows(dirtyIndices, dirtyCount);
        }
#if defined(MS_PROJECT_VIEW_PERF_LOG)
        invalidate_us = menuListPerfMicros() - invalidate_start_us;
#endif
    }
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    recordMenuListPerf(
        menuListPerfMicros() - total_start_us,
        header_us,
        sync_us,
        count_us,
        select_us,
        invalidate_us,
        g_menu_list_perf_callbacks.binds - binds_before,
        g_menu_list_perf_callbacks.highlights - highlights_before,
        static_cast<uint32_t>(dirtyCount),
        countChanged,
        row_count_,
        props.selectedIndex,
        props.dataRevision
    );
#endif
}

FLASHMEM void MenuListView::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    if (!list_) return;

    const int slotIndex = index - list_->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (index < 0 || index >= row_count_) return;
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    g_menu_list_perf_callbacks.binds += 1;
#endif

    ensureSlotWidgets(slot, slotIndex);
    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    const auto& row = rows_[static_cast<std::size_t>(index)];

    setLabelTextIfChanged(widgets.label, widgets.labelCache, row.label.text);
    setLabelTextIfChanged(widgets.value, widgets.valueCache, row.value.text);
    applyRowStyle(widgets, row);

    widgets.boundIndex = index;
    applyHighlightStyle(slot, widgets, isSelected, row);
}

FLASHMEM void MenuListView::updateSlotHighlight(widget::VirtualSlot& slot, bool isSelected) {
    if (!list_) return;

    const int slotIndex = slot.boundIndex - list_->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (slot.boundIndex < 0 || slot.boundIndex >= row_count_) return;
#if defined(MS_PROJECT_VIEW_PERF_LOG)
    g_menu_list_perf_callbacks.highlights += 1;
#endif

    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    const auto& row = rows_[static_cast<std::size_t>(slot.boundIndex)];
    applyHighlightStyle(slot, widgets, isSelected, row);
}

FLASHMEM void MenuListView::ensureSlotWidgets(widget::VirtualSlot& slot, int slotIndex) {
    auto& widgets = slot_widgets_[static_cast<std::size_t>(slotIndex)];
    if (widgets.created) return;

    lv_obj_t* row = slot.container;
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(row, ROW_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(row, ROW_PAD_RIGHT, 0);
    lv_obj_set_style_pad_column(row, COL_GAP, 0);

    widgets.label = lv_label_create(row);
    lv_obj_set_flex_grow(widgets.label, 1);
    lv_label_set_long_mode(widgets.label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(widgets.label, fonts.list_item_label, 0);

    widgets.value = lv_label_create(row);
    lv_obj_set_width(widgets.value, VALUE_COL_W);
    lv_obj_set_style_text_align(widgets.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(widgets.value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(widgets.value, fonts.inter_14_semibold, 0);

    widgets.created = true;
}

FLASHMEM void MenuListView::applyHighlightStyle(
    widget::VirtualSlot& slot,
    SlotWidgets& widgets,
    bool isSelected,
    const RowCache& row
) {
    (void)slot;
    (void)row;
    if (widgets.highlightStyleApplied && widgets.highlighted == isSelected) return;

    widgets.highlighted = isSelected;
    widgets.highlightStyleApplied = true;
}

FLASHMEM void MenuListView::applyRowStyle(SlotWidgets& widgets, const RowCache& row) {
    const bool enabled = row.enabled && row.kind != MenuRowKind::Disabled;
    const uint32_t labelColor = enabled ? base_theme::color::TEXT_PRIMARY
                                        : base_theme::color::INACTIVE_LIGHTER;
    const uint32_t valueColor = enabled ? valueColorFor(row.kind)
                                        : base_theme::color::INACTIVE_LIGHTER;
    const lv_opa_t labelOpa = enabled ? LV_OPA_80 : LV_OPA_60;
    const lv_opa_t valueOpa = enabled ? LV_OPA_80 : LV_OPA_50;

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
        widgets.valueOpa = valueOpa;
    }
    widgets.rowStyleApplied = true;
}

}  // namespace ms::ui
