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
constexpr int COMPACT_PAD_H = 8;
constexpr int COMPACT_COL_GAP = 4;
constexpr int COMPACT_ICON_COL_W = 14;
constexpr int COMPACT_DETAIL_COL_W = 78;
constexpr int COMPACT_SPARKLINE_W = 58;
constexpr uint32_t SPARKLINE_MARKER_PERIOD_MS = 4U;

FLASHMEM bool sameMarker(
    const KeyValueSparklineMarker& lhs,
    const KeyValueSparklineMarker& rhs
) {
    return lhs.visible == rhs.visible &&
        lhs.positionQ16 == rhs.positionQ16 &&
        lhs.valueQ16 == rhs.valueQ16;
}

FLASHMEM bool sameArea(const lv_area_t& lhs, const lv_area_t& rhs) {
    return lhs.x1 == rhs.x1 && lhs.y1 == rhs.y1 &&
        lhs.x2 == rhs.x2 && lhs.y2 == rhs.y2;
}

FLASHMEM void drawLine(
    lv_layer_t* layer,
    lv_point_precise_t* points,
    uint32_t count,
    uint32_t color,
    lv_opa_t opacity,
    lv_coord_t width
) {
    if (!layer || !points || count < 2U || opacity == LV_OPA_TRANSP ||
        width <= 0) {
        return;
    }
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.base.layer = layer;
    dsc.points = points;
    dsc.point_cnt = count;
    dsc.color = lv_color_hex(color);
    dsc.opa = opacity;
    dsc.width = width;
    lv_draw_line(layer, &dsc);
}

FLASHMEM lv_area_t markerDamageArea(
    lv_obj_t* surface,
    const KeyValueSparklineMarker& marker
) {
    lv_area_t surfaceArea{};
    lv_obj_get_coords(surface, &surfaceArea);
    const int width = lv_area_get_width(&surfaceArea);
    const int height = lv_area_get_height(&surfaceArea);
    const int x = surfaceArea.x1 + keyValueSparklineCoordinate(
        marker.positionQ16,
        width
    );
    const int y = surfaceArea.y1 + (height - 1) -
        keyValueSparklineCoordinate(marker.valueQ16, height);
    return {
        static_cast<lv_coord_t>(x - 3),
        static_cast<lv_coord_t>(y - 4),
        static_cast<lv_coord_t>(x + 3),
        static_cast<lv_coord_t>(y + 4),
    };
}
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
    if (marker_timer_) {
        lv_timer_delete(marker_timer_);
        marker_timer_ = nullptr;
    }
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
    const bool enabled = next.enabled && next.sampleProvider != nullptr;
    const bool changed = cache.enabled != enabled ||
        cache.centerLine != (enabled && next.centerLine) ||
        cache.context != next.context ||
        cache.identity != next.identity ||
        cache.geometryRevision != next.geometryRevision ||
        cache.runtimeIndex != next.runtimeIndex ||
        cache.sampleProvider != next.sampleProvider ||
        cache.markerProvider != next.markerProvider;
    cache = next;
    cache.enabled = enabled;
    cache.centerLine = enabled && next.centerLine;
    if (!enabled) cache.markerProvider = nullptr;
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
        const bool detailChanged = copyTextIfChanged(current.detail, row ? row->detail : "");
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
        if ((keyChanged || valueChanged || detailChanged || iconChanged || iconStyleChanged || sparklineChanged) &&
            dirtyCount < MAX_ROWS) {
            dirtyIndices[static_cast<size_t>(dirtyCount++)] = i;
        }
    }

    for (int i = nextCount; i < row_count_; ++i) {
        auto& current = rows_[static_cast<size_t>(i)];
        copyTextIfChanged(current.key, "");
        copyTextIfChanged(current.value, "");
        copyTextIfChanged(current.detail, "");
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
        visible_ = false;
        if (marker_timer_) lv_timer_pause(marker_timer_);
        overlay_.hide();
        return;
    }
    visible_ = true;

    overlay_.setTitle(props.title);
    overlay_.setMeta(props.meta);

    const bool dimStyleChanged = dim_unselected_ != props.dimUnselected;
    dim_unselected_ = props.dimUnselected;
    const bool compactStyleChanged = compact_facts_ != props.compactFacts;
    compact_facts_ = props.compactFacts;
    if (compactStyleChanged) {
        for (auto& widgets : slot_widgets_) {
            if (widgets.created) applyCompactLayout(widgets);
        }
    }

    std::array<int, MAX_ROWS> dirtyIndices{};
    int dirtyCount = 0;
    bool providerChanged = false;
    if (props.rowProvider != nullptr) {
        const int nextCount = std::clamp(
            props.rowCount,
            0,
            MAX_PROVIDER_ROWS
        );
        providerChanged = row_provider_ != props.rowProvider ||
            row_provider_context_ != props.rowProviderContext ||
            props.dataRevision == 0U ||
            last_data_revision_ != props.dataRevision ||
            row_count_ != nextCount;
        row_provider_ = props.rowProvider;
        row_provider_context_ = props.rowProviderContext;
        row_count_ = nextCount;
        last_row_count_ = nextCount;
        last_data_revision_ = props.dataRevision;
    } else {
        if (row_provider_ != nullptr) {
            row_provider_ = nullptr;
            row_provider_context_ = nullptr;
            last_data_revision_ = 0;
            last_row_count_ = -1;
            providerChanged = true;
        }
        syncRows(props, dirtyIndices, dirtyCount);
    }

    auto* list = overlay_.list();
    if (list) {
        const bool countChanged = list->setTotalCount(row_count_);
        list->setSelectedIndex(props.selectedIndex);

        if (!countChanged && overlay_.isVisible()) {
            if (dimStyleChanged || compactStyleChanged || providerChanged) {
                list->invalidate();
            } else {
                invalidateDirtyRows(dirtyIndices, dirtyCount);
            }
        }
    }

    if (!overlay_.isVisible()) {
        overlay_.show();
    }
    refreshSparklineMarkerTimer();
}

FLASHMEM void VirtualListKeyValueOverlay::bindSlot(widget::VirtualSlot& slot, int index, bool isSelected) {
    auto* list = overlay_.list();
    if (!list) return;

    const int slotIndex = index - list->getWindowStart();
    if (slotIndex < 0 || slotIndex >= VISIBLE_SLOTS) return;
    if (index < 0 || index >= row_count_) return;

    ensureSlotWidgets(slot.container, slotIndex);
    auto& widgets = slot_widgets_[static_cast<size_t>(slotIndex)];
    const auto& row = row_provider_ != nullptr
        ? materializeProviderRow(index)
        : rows_[static_cast<size_t>(index)];

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
    if (widgets.detailLabel) {
        setLabelTextIfChanged(widgets.detailLabel, widgets.detailCache, row.detail.text);
        if (row.detail.text[0] != '\0') {
            lv_obj_clear_flag(widgets.detailLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.detailLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    applySparkline(widgets, row);

    widgets.boundIndex = index;
    applyHighlightStyle(widgets, isSelected);
}

FLASHMEM const VirtualListKeyValueOverlay::RowCache&
VirtualListKeyValueOverlay::materializeProviderRow(int index) {
    provider_buffer_ = {};
    if (row_provider_ != nullptr) {
        row_provider_(row_provider_context_, index, provider_buffer_);
    }
    (void)copyTextIfChanged(provider_row_.key, provider_buffer_.key.data());
    (void)copyTextIfChanged(provider_row_.value, provider_buffer_.value.data());
    (void)copyTextIfChanged(provider_row_.detail, provider_buffer_.detail.data());
    (void)copyTextIfChanged(provider_row_.icon, provider_buffer_.icon.data());
    provider_row_.iconFont = provider_buffer_.iconFont;
    provider_row_.iconColor = provider_buffer_.iconColor;
    (void)copySparklineIfChanged(
        provider_row_.sparkline,
        provider_buffer_.sparkline
    );
    return provider_row_;
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

    widgets.detailLabel = lv_label_create(container);
    lv_obj_set_width(widgets.detailLabel, COMPACT_DETAIL_COL_W);
    lv_obj_set_style_text_align(widgets.detailLabel, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(widgets.detailLabel, LV_LABEL_LONG_DOT);
    const lv_font_t* detailFont = fonts.inter_12_medium
        ? fonts.inter_12_medium
        : keyFont;
    lv_obj_set_style_text_font(widgets.detailLabel, detailFont, LV_STATE_DEFAULT);
    lv_obj_set_height(widgets.detailLabel, lv_font_get_line_height(detailFont));
    style::apply(widgets.detailLabel).textColor(base_theme::color::TEXT_SECONDARY);
    lv_obj_add_flag(widgets.detailLabel, LV_OBJ_FLAG_HIDDEN);

    widgets.valueLabel = lv_label_create(container);
    lv_obj_set_width(widgets.valueLabel, VALUE_COL_W);
    lv_obj_set_style_text_align(widgets.valueLabel, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_label_set_long_mode(widgets.valueLabel, LV_LABEL_LONG_DOT);
    const lv_font_t* valueFont = fonts.inter_14_semibold
        ? fonts.inter_14_semibold
        : keyFont;
    lv_obj_set_style_text_font(widgets.valueLabel, valueFont, LV_STATE_DEFAULT);
    lv_obj_set_height(widgets.valueLabel, lv_font_get_line_height(valueFont));

    widgets.sparklineSurface = lv_obj_create(container);
    lv_obj_set_size(widgets.sparklineSurface, VALUE_COL_W, SPARKLINE_H);
    lv_obj_set_style_bg_opa(
        widgets.sparklineSurface,
        LV_OPA_TRANSP,
        LV_STATE_DEFAULT
    );
    lv_obj_set_style_border_width(
        widgets.sparklineSurface,
        0,
        LV_STATE_DEFAULT
    );
    lv_obj_set_style_pad_all(
        widgets.sparklineSurface,
        0,
        LV_STATE_DEFAULT
    );
    lv_obj_remove_flag(
        widgets.sparklineSurface,
        LV_OBJ_FLAG_SCROLLABLE
    );
    lv_obj_remove_flag(
        widgets.sparklineSurface,
        LV_OBJ_FLAG_CLICKABLE
    );
    lv_obj_add_event_cb(
        widgets.sparklineSurface,
        onSparklineDrawEvent,
        LV_EVENT_DRAW_MAIN,
        &widgets
    );
    lv_obj_add_flag(widgets.sparklineSurface, LV_OBJ_FLAG_HIDDEN);

    widgets.created = true;
    applyCompactLayout(widgets);
}

FLASHMEM void VirtualListKeyValueOverlay::applyCompactLayout(SlotWidgets& widgets) {
    if (!widgets.created || !widgets.keyLabel) return;
    lv_obj_t* container = lv_obj_get_parent(widgets.keyLabel);
    if (!container) return;

    lv_obj_set_style_pad_left(
        container,
        compact_facts_ ? COMPACT_PAD_H : PAD_H,
        LV_STATE_DEFAULT
    );
    lv_obj_set_style_pad_right(
        container,
        compact_facts_ ? COMPACT_PAD_H : PAD_H,
        LV_STATE_DEFAULT
    );
    lv_obj_set_style_pad_column(
        container,
        compact_facts_ ? COMPACT_COL_GAP : COL_GAP,
        LV_STATE_DEFAULT
    );
    if (widgets.iconLabel) {
        lv_obj_set_width(
            widgets.iconLabel,
            compact_facts_ ? COMPACT_ICON_COL_W : ICON_COL_W
        );
    }
    if (widgets.detailLabel) {
        lv_obj_set_width(widgets.detailLabel, COMPACT_DETAIL_COL_W);
    }
    if (widgets.valueLabel) {
        lv_obj_set_width(
            widgets.valueLabel,
            compact_facts_ ? COMPACT_SPARKLINE_W : VALUE_COL_W
        );
    }
    if (widgets.sparklineSurface) {
        lv_obj_set_width(
            widgets.sparklineSurface,
            compact_facts_ ? COMPACT_SPARKLINE_W : VALUE_COL_W
        );
    }
}

FLASHMEM void VirtualListKeyValueOverlay::applySparkline(
    SlotWidgets& widgets,
    const RowCache& row
) {
    const bool showSparkline =
        row.sparkline.enabled && row.sparkline.sampleProvider != nullptr;
    if (widgets.valueLabel) {
        if (showSparkline) {
            lv_obj_add_flag(widgets.valueLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(widgets.valueLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (!widgets.sparklineSurface) return;

    if (!showSparkline) {
        if (widgets.sparklineVisible) {
            lv_obj_add_flag(widgets.sparklineSurface, LV_OBJ_FLAG_HIDDEN);
            widgets.sparklineVisible = false;
        }
        widgets.sparkline = {};
        widgets.marker = {};
        refreshSparklineMarkerTimer();
        return;
    }

    const bool geometryChanged = copySparklineIfChanged(
        widgets.sparkline,
        row.sparkline
    );
    if (geometryChanged || !widgets.sparklineVisible) {
        widgets.marker = {};
        lv_obj_invalidate(widgets.sparklineSurface);
    }
    lv_obj_clear_flag(widgets.sparklineSurface, LV_OBJ_FLAG_HIDDEN);
    widgets.sparklineVisible = true;
    if (widgets.sparkline.markerProvider != nullptr && marker_timer_ == nullptr) {
        marker_timer_ = lv_timer_create(
            onSparklineMarkerTimer,
            SPARKLINE_MARKER_PERIOD_MS,
            this
        );
        if (marker_timer_) lv_timer_pause(marker_timer_);
    }
    refreshSparklineMarkerTimer();
}

FLASHMEM void VirtualListKeyValueOverlay::onSparklineDrawEvent(
    lv_event_t* event
) {
    auto* widgets = static_cast<SlotWidgets*>(lv_event_get_user_data(event));
    auto* layer = lv_event_get_layer(event);
    if (!widgets || !layer || !widgets->sparklineSurface ||
        !widgets->sparklineVisible || !widgets->sparkline.enabled ||
        widgets->sparkline.sampleProvider == nullptr) {
        return;
    }

    lv_area_t area{};
    lv_obj_get_coords(widgets->sparklineSurface, &area);
    const int width = std::min<int>(
        lv_area_get_width(&area),
        KEY_VALUE_SPARKLINE_MAX_WIDTH
    );
    const int height = lv_area_get_height(&area);
    if (width < 2 || height < 2) return;

    if (widgets->sparkline.centerLine) {
        std::array<lv_point_precise_t, 2> guide{{
            {
                static_cast<lv_value_precise_t>(area.x1),
                static_cast<lv_value_precise_t>(area.y1 + (height - 1) / 2),
            },
            {
                static_cast<lv_value_precise_t>(area.x1 + width - 1),
                static_cast<lv_value_precise_t>(area.y1 + (height - 1) / 2),
            },
        }};
        drawLine(
            layer,
            guide.data(),
            guide.size(),
            base_theme::color::INACTIVE_LIGHTER,
            LV_OPA_40,
            1
        );
    }

    const auto range = keyValueSparklineColumnsForClip(
        area.x1,
        width,
        layer->_clip_area.x1,
        layer->_clip_area.x2
    );
    if (!range.empty()) {
        std::array<
            lv_point_precise_t,
            KEY_VALUE_SPARKLINE_DRAW_CHUNK
        > points{};
        std::size_t pointCount = 0U;
        auto flush = [&]() {
            drawLine(
                layer,
                points.data(),
                static_cast<uint32_t>(pointCount),
                base_theme::color::ACTIVE,
                LV_OPA_COVER,
                2
            );
        };
        for (std::size_t column = range.begin; column < range.end; ++column) {
            const uint16_t positionQ16 = keyValueSparklinePositionQ16(
                column,
                static_cast<std::size_t>(width)
            );
            const uint16_t previousPositionQ16 = column > 0U
                ? keyValueSparklinePositionQ16(
                      column - 1U,
                      static_cast<std::size_t>(width)
                  )
                : 0U;
            KeyValueSparklineSample sample{};
            if (!widgets->sparkline.sampleProvider(
                    widgets->sparkline,
                    positionQ16,
                    previousPositionQ16,
                    column > 0U,
                    sample
                )) {
                flush();
                pointCount = 0U;
                continue;
            }
            if (sample.discontinuityBefore && pointCount > 0U) {
                flush();
                pointCount = 0U;
            }
            points[pointCount++] = {
                static_cast<lv_value_precise_t>(area.x1 +
                    static_cast<int>(column)),
                static_cast<lv_value_precise_t>(area.y1 + height - 1 -
                    keyValueSparklineCoordinate(sample.valueQ16, height)),
            };
            if (pointCount == points.size()) {
                flush();
                points[0] = points[pointCount - 1U];
                pointCount = 1U;
            }
        }
        flush();
    }

    if (widgets->marker.visible) {
        const int markerX = area.x1 + keyValueSparklineCoordinate(
            widgets->marker.positionQ16,
            width
        );
        const int markerY = area.y1 + height - 1 -
            keyValueSparklineCoordinate(widgets->marker.valueQ16, height);
        std::array<lv_point_precise_t, 2> marker{{
            {
                static_cast<lv_value_precise_t>(markerX),
                static_cast<lv_value_precise_t>(std::max<int32_t>(
                    area.y1,
                    static_cast<int32_t>(markerY - 3)
                )),
            },
            {
                static_cast<lv_value_precise_t>(markerX),
                static_cast<lv_value_precise_t>(std::min<int32_t>(
                    area.y2,
                    static_cast<int32_t>(markerY + 3)
                )),
            },
        }};
        drawLine(
            layer,
            marker.data(),
            marker.size(),
            base_theme::color::ACTIVE,
            LV_OPA_COVER,
            2
        );
    }
}

FLASHMEM void VirtualListKeyValueOverlay::serviceSparklineMarkers() {
    const uint32_t nowMs = lv_tick_get();
    for (auto& widgets : slot_widgets_) {
        if (!widgets.sparklineVisible || !widgets.sparklineSurface ||
            widgets.sparkline.markerProvider == nullptr) {
            continue;
        }
        KeyValueSparklineMarker next{};
        if (!widgets.sparkline.markerProvider(
                widgets.sparkline,
                nowMs,
                next
            )) {
            next = {};
        }
        if (sameMarker(widgets.marker, next)) continue;
        if (widgets.marker.visible && next.visible) {
            const auto oldArea = markerDamageArea(
                widgets.sparklineSurface,
                widgets.marker
            );
            const auto nextArea = markerDamageArea(
                widgets.sparklineSurface,
                next
            );
            if (sameArea(oldArea, nextArea)) {
                widgets.marker = next;
                continue;
            }
        }
        if (widgets.marker.visible) {
            const auto oldArea = markerDamageArea(
                widgets.sparklineSurface,
                widgets.marker
            );
            lv_obj_invalidate_area(widgets.sparklineSurface, &oldArea);
        }
        widgets.marker = next;
        if (widgets.marker.visible) {
            const auto newArea = markerDamageArea(
                widgets.sparklineSurface,
                widgets.marker
            );
            lv_obj_invalidate_area(widgets.sparklineSurface, &newArea);
        }
    }
}

FLASHMEM void VirtualListKeyValueOverlay::refreshSparklineMarkerTimer() {
    if (!marker_timer_) return;
    bool active = false;
    if (visible_) {
        for (const auto& widgets : slot_widgets_) {
            if (widgets.sparklineVisible &&
                widgets.sparkline.markerProvider != nullptr) {
                active = true;
                break;
            }
        }
    }
    if (active) {
        lv_timer_resume(marker_timer_);
    } else {
        lv_timer_pause(marker_timer_);
    }
}

FLASHMEM void VirtualListKeyValueOverlay::onSparklineMarkerTimer(
    lv_timer_t* timer
) {
    auto* self = static_cast<VirtualListKeyValueOverlay*>(
        lv_timer_get_user_data(timer)
    );
    if (self) self->serviceSparklineMarkers();
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
    if (widgets.sparklineSurface && widgets.sparklineVisible) {
        // Curve color remains semantic/active for every visible source. Focus
        // is conveyed by the row background, not by rebuilding its geometry.
        lv_obj_invalidate(widgets.sparklineSurface);
    }

    widgets.highlighted = isSelected;
    widgets.dimUnselected = dim_unselected_;
    widgets.highlightStyleApplied = true;
}

}  // namespace ms::ui
