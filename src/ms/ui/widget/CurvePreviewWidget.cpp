#include <ms/ui/widget/CurvePreviewWidget.hpp>

#include <algorithm>
#include <array>

#include <config/PlatformCompat.hpp>
#include <oc/diagnostics/Performance.hpp>
#include <oc/ui/lvgl/StaticSurfaceInvalidation.hpp>

namespace ms::ui {
namespace {

FLASHMEM bool sameArea(const lv_area_t& lhs, const lv_area_t& rhs) {
    return lhs.x1 == rhs.x1 && lhs.y1 == rhs.y1 &&
           lhs.x2 == rhs.x2 && lhs.y2 == rhs.y2;
}

FLASHMEM bool sameMarker(
    const CurvePreviewMarker& lhs,
    const CurvePreviewMarker& rhs
) {
    return lhs.visible == rhs.visible &&
           lhs.positionQ16 == rhs.positionQ16 &&
           lhs.valueQ16 == rhs.valueQ16;
}

FLASHMEM void drawLine(
    lv_layer_t* layer,
    lv_point_precise_t* points,
    uint32_t count,
    uint32_t color,
    lv_opa_t opacity,
    lv_coord_t width
) {
    if (layer == nullptr || points == nullptr || count < 2U ||
        opacity == LV_OPA_TRANSP || width <= 0) {
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

FLASHMEM void drawGuide(
    lv_layer_t* layer,
    const lv_area_t& area,
    uint16_t valueQ16,
    uint32_t color,
    lv_opa_t opacity
) {
    const auto y = static_cast<lv_value_precise_t>(curvePreviewY(
        valueQ16,
        area.y1,
        lv_area_get_height(&area)
    ));
    std::array<lv_point_precise_t, 2> points{{
        {static_cast<lv_value_precise_t>(area.x1), y},
        {static_cast<lv_value_precise_t>(area.x2), y},
    }};
    drawLine(layer, points.data(), points.size(), color, opacity, 1);
}

FLASHMEM void drawVerticalGuide(
    lv_layer_t* layer,
    const lv_area_t& area,
    uint16_t positionQ16,
    uint32_t color,
    lv_opa_t opacity
) {
    const auto x = static_cast<lv_value_precise_t>(curvePreviewCoordinate(
        positionQ16,
        area.x1,
        lv_area_get_width(&area)
    ));
    std::array<lv_point_precise_t, 2> points{{
        {x, static_cast<lv_value_precise_t>(area.y1)},
        {x, static_cast<lv_value_precise_t>(area.y2)},
    }};
    drawLine(layer, points.data(), points.size(), color, opacity, 1);
}

FLASHMEM void populatePoints(
    const std::array<uint16_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>& values,
    std::size_t sampleCount,
    const CurvePreviewSampleRange& range,
    const lv_area_t& area,
    std::array<lv_point_precise_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>& out
) {
    const int32_t width = lv_area_get_width(&area);
    const int32_t height = lv_area_get_height(&area);
    for (std::size_t index = range.begin; index < range.end; ++index) {
        const uint16_t position = curvePreviewPositionQ16(index, sampleCount);
        out[index - range.begin] = {
            static_cast<lv_value_precise_t>(curvePreviewCoordinate(
                position,
                area.x1,
                width
            )),
            static_cast<lv_value_precise_t>(curvePreviewY(
                values[index],
                area.y1,
                height
            )),
        };
    }
}

FLASHMEM void drawCurveWithDiscontinuities(
    lv_layer_t* layer,
    const CurvePreviewGeometry& geometry,
    const CurvePreviewSampleRange& range,
    const lv_area_t& area,
    std::array<lv_point_precise_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>& points,
    uint32_t color,
    lv_opa_t opacity,
    lv_coord_t width
) {
    if (range.size() < 2U) return;
    populatePoints(
        geometry.curve,
        geometry.sampleCount,
        range,
        area,
        points
    );
    std::size_t runStart = 0U;
    for (std::size_t index = range.begin + 1U;
         index < range.end;
         ++index) {
        // index is bounded by geometry.sampleCount (native display width), so the unchecked
        // bitset accessor is both safe and avoids an exception-format string.
        if (!geometry.discontinuities[index]) continue;
        const std::size_t relative = index - range.begin;
        if (relative - runStart >= 2U) {
            drawLine(
                layer,
                points.data() + runStart,
                static_cast<uint32_t>(relative - runStart),
                color,
                opacity,
                width
            );
        }
        std::array<lv_point_precise_t, 3> step{{
            points[relative - 1U],
            {points[relative].x, points[relative - 1U].y},
            points[relative],
        }};
        drawLine(layer, step.data(), step.size(), color, opacity, width);
        runStart = relative;
    }
    if (range.size() - runStart >= 2U) {
        drawLine(
            layer,
            points.data() + runStart,
            static_cast<uint32_t>(range.size() - runStart),
            color,
            opacity,
            width
        );
    }
}

FLASHMEM void drawImpactBand(
    lv_layer_t* layer,
    const CurvePreviewGeometry& geometry,
    const CurvePreviewSampleRange& range,
    const lv_area_t& area,
    uint32_t color,
    lv_opa_t opacity
) {
    const std::size_t count = geometry.sampleCount;
    if (range.size() < 2U || opacity == LV_OPA_TRANSP) return;
    const int32_t areaWidth = lv_area_get_width(&area);
    const int32_t areaHeight = lv_area_get_height(&area);
    for (std::size_t index = range.begin; index < range.end; ++index) {
        const int32_t x = curvePreviewCoordinate(
            curvePreviewPositionQ16(index, count),
            area.x1,
            areaWidth
        );
        if (x < layer->_clip_area.x1 || x > layer->_clip_area.x2) {
            continue;
        }
        std::array<lv_point_precise_t, 2> points{{
            {
                static_cast<lv_value_precise_t>(x),
                static_cast<lv_value_precise_t>(curvePreviewY(
                    geometry.base[index],
                    area.y1,
                    areaHeight
                )),
            },
            {
                static_cast<lv_value_precise_t>(x),
                static_cast<lv_value_precise_t>(curvePreviewY(
                    geometry.impact[index],
                    area.y1,
                    areaHeight
                )),
            },
        }};
        drawLine(layer, points.data(), points.size(), color, opacity, 1);
    }
}

FLASHMEM void drawMarker(
    lv_layer_t* layer,
    const lv_area_t& area,
    const CurvePreviewWidgetProps& props
) {
    if (!props.marker.visible || layer == nullptr) return;
    const auto rect = curvePreviewMarkerRect(
        area.x1,
        area.y1,
        lv_area_get_width(&area),
        lv_area_get_height(&area),
        props.marker.positionQ16,
        props.marker.valueQ16,
        props.markerRadius
    );
    if (!rect.valid()) return;
    lv_area_t markerArea{
        .x1 = static_cast<lv_coord_t>(rect.x1),
        .y1 = static_cast<lv_coord_t>(rect.y1),
        .x2 = static_cast<lv_coord_t>(rect.x2),
        .y2 = static_cast<lv_coord_t>(rect.y2),
    };
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(props.markerColor);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = LV_RADIUS_CIRCLE;
    lv_draw_rect(layer, &dsc, &markerArea);
}

}  // namespace

FLASHMEM CurvePreviewWidget::CurvePreviewWidget(lv_obj_t* parent) {
    createUi(parent);
}

FLASHMEM CurvePreviewWidget::~CurvePreviewWidget() {
    markerTimer_.reset();
    if (surface_ != nullptr) {
        lv_obj_delete(surface_);
        surface_ = nullptr;
    }
}

FLASHMEM void CurvePreviewWidget::createUi(lv_obj_t* parent) {
    if (parent == nullptr) return;
    surface_ = lv_obj_create(parent);
    lv_obj_remove_style_all(surface_);
    lv_obj_add_flag(surface_, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(surface_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(surface_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(surface_, onDrawEvent, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(
        surface_,
        onSizeChangedEvent,
        LV_EVENT_SIZE_CHANGED,
        this
    );
    lv_obj_add_flag(surface_, LV_OBJ_FLAG_HIDDEN);
    markerTimer_.emplace(
        MARKER_SERVICE_PERIOD_MS,
        &CurvePreviewWidget::onMarkerTimer,
        this
    );
}

FLASHMEM bool CurvePreviewWidget::staticStyleChanged(
    const CurvePreviewWidgetProps& props
) const {
    const auto& previous = *renderedProps_;
    return previous.showImpactBand != props.showImpactBand ||
           previous.showCenterGuide != props.showCenterGuide ||
           previous.showRestGuide != props.showRestGuide ||
           previous.showVerticalGuide != props.showVerticalGuide ||
           previous.restValueQ16 != props.restValueQ16 ||
           previous.verticalGuidePositionQ16 !=
               props.verticalGuidePositionQ16 ||
           previous.paddingX != props.paddingX ||
           previous.paddingY != props.paddingY ||
           previous.curveColor != props.curveColor ||
           previous.baseColor != props.baseColor ||
           previous.impactColor != props.impactColor ||
           previous.guideColor != props.guideColor ||
           previous.markerColor != props.markerColor ||
           previous.curveOpacity != props.curveOpacity ||
           previous.baseOpacity != props.baseOpacity ||
           previous.impactOpacity != props.impactOpacity ||
           previous.bandOpacity != props.bandOpacity ||
           previous.guideOpacity != props.guideOpacity ||
           previous.curveWidth != props.curveWidth ||
           previous.baseWidth != props.baseWidth ||
           previous.impactWidth != props.impactWidth ||
           previous.markerRadius != props.markerRadius;
}

FLASHMEM void CurvePreviewWidget::invalidateMarker(
    const CurvePreviewMarker& marker
) const {
    if (surface_ == nullptr || !marker.visible) return;
    const auto rect = curvePreviewMarkerRect(
        renderedArea_->x1,
        renderedArea_->y1,
        lv_area_get_width(&*renderedArea_),
        lv_area_get_height(&*renderedArea_),
        marker.positionQ16,
        marker.valueQ16,
        renderedProps_->markerRadius + 1
    );
    if (!rect.valid()) return;
    oc::ui::lvgl::invalidateStaticSurfaceArea(
        surface_,
        {
            .x1 = static_cast<lv_coord_t>(rect.x1),
            .y1 = static_cast<lv_coord_t>(rect.y1),
            .x2 = static_cast<lv_coord_t>(rect.x2),
            .y2 = static_cast<lv_coord_t>(rect.y2),
        }
    );
}

FLASHMEM bool CurvePreviewWidget::sameMarkerPixel(
    const CurvePreviewMarker& lhs,
    const CurvePreviewMarker& rhs
) const {
    if (lhs.visible != rhs.visible) return false;
    if (!lhs.visible) return true;
    if (!renderedArea_ || !renderedProps_) return sameMarker(lhs, rhs);
    const auto rectFor = [this](const CurvePreviewMarker& marker) {
        return curvePreviewMarkerRect(
            renderedArea_->x1,
            renderedArea_->y1,
            lv_area_get_width(&*renderedArea_),
            lv_area_get_height(&*renderedArea_),
            marker.positionQ16,
            marker.valueQ16,
            renderedProps_->markerRadius
        );
    };
    const auto left = rectFor(lhs);
    const auto right = rectFor(rhs);
    return left.x1 == right.x1 && left.y1 == right.y1 &&
           left.x2 == right.x2 && left.y2 == right.y2;
}

void CurvePreviewWidget::invalidateTail() const {
    if (surface_ == nullptr || !renderedArea_ || !renderedProps_ ||
        geometry_.sampleCount < 2U) {
        return;
    }
    const auto& area = *renderedArea_;
    const auto& props = *renderedProps_;
    const int32_t firstX = curvePreviewCoordinate(
        curvePreviewPositionQ16(
            geometry_.sampleCount - 2U,
            geometry_.sampleCount
        ),
        area.x1,
        lv_area_get_width(&area)
    );
    const lv_coord_t margin = std::max<lv_coord_t>(
        1,
        std::max(props.curveWidth, props.impactWidth)
    );
    oc::ui::lvgl::invalidateStaticSurfaceArea(
        surface_,
        {
            .x1 = static_cast<lv_coord_t>(firstX - margin),
            .y1 = static_cast<lv_coord_t>(area.y1 - margin),
            .x2 = static_cast<lv_coord_t>(area.x2 + margin),
            .y2 = static_cast<lv_coord_t>(area.y2 + margin),
        }
    );
}

bool CurvePreviewWidget::updateRollingGeometry(
    uint32_t geometryRevision,
    CurvePreviewGeometryUpdate update,
    uint16_t advanceCount
) {
    if (!visible_ || !rendered_ || surface_ == nullptr || !renderedProps_ ||
        update == CurvePreviewGeometryUpdate::REBUILD) {
        return false;
    }
    if (renderedProps_->geometryRevision == geometryRevision) return true;
    bool updated = false;
    {
        OC_PERF_SCOPE(perfGeometry, "ui.curve-preview.geometry-hot");
        if (update == CurvePreviewGeometryUpdate::PATCH_LAST) {
            updated = geometry_.patchLast(
                renderedProps_->sampleProvider,
                renderedProps_->sampleContext
            );
        } else if (update == CurvePreviewGeometryUpdate::ADVANCE) {
            updated = geometry_.advance(
                advanceCount,
                renderedProps_->sampleProvider,
                renderedProps_->sampleContext
            );
        }
        OC_PERF_UNITS(
            perfGeometry,
            update == CurvePreviewGeometryUpdate::PATCH_LAST
                ? 1U
                : advanceCount,
            geometry_.sampleCount
        );
    }
    if (!updated) return false;
    renderedProps_->geometryRevision = geometryRevision;
    renderedProps_->geometryUpdate = update;
    renderedProps_->geometryAdvance = advanceCount;
    if (update == CurvePreviewGeometryUpdate::PATCH_LAST) {
        invalidateTail();
    } else {
        lv_obj_invalidate(surface_);
    }
    return true;
}

FLASHMEM void CurvePreviewWidget::serviceMarker() {
    if (!visible_ || !rendered_ || !renderedProps_ ||
        renderedProps_->markerProvider == nullptr) {
        if (markerTimer_) markerTimer_->pause();
        return;
    }
    if (!lv_obj_is_visible(surface_)) return;
    CurvePreviewMarker next{};
    if (!renderedProps_->markerProvider(
            renderedProps_->markerContext,
            next
        )) {
        next = {};
    }
    const CurvePreviewMarker previous = renderedProps_->marker;
    const bool rasterChanged = !sameMarkerPixel(previous, next);
    if (rasterChanged) invalidateMarker(previous);
    // Always retain the latest sub-pixel value. A later movement is then
    // compared with the true current marker rather than a stale coordinate.
    renderedProps_->marker = next;
    if (rasterChanged) invalidateMarker(next);
}

FLASHMEM void CurvePreviewWidget::draw(lv_layer_t* layer) {
    if (!rendered_ || geometry_.sampleCount < 2U || layer == nullptr) return;
    const auto& props = *renderedProps_;
    const auto sampleRange = curvePreviewSampleRangeForClip(
        renderedArea_->x1,
        lv_area_get_width(&*renderedArea_),
        geometry_.sampleCount,
        layer->_clip_area.x1,
        layer->_clip_area.x2
    );
    if (sampleRange.empty()) return;
    if (props.showCenterGuide) {
        drawGuide(
            layer,
            *renderedArea_,
            32768U,
            props.guideColor,
            props.guideOpacity
        );
    }
    if (props.showRestGuide &&
        (!props.showCenterGuide || props.restValueQ16 != 32768U)) {
        drawGuide(
            layer,
            *renderedArea_,
            props.restValueQ16,
            props.guideColor,
            props.guideOpacity
        );
    }
    if (props.showVerticalGuide) {
        drawVerticalGuide(
            layer,
            *renderedArea_,
            props.verticalGuidePositionQ16,
            props.guideColor,
            props.guideOpacity
        );
    }
    if (props.showImpactBand) {
        drawImpactBand(
            layer,
            geometry_,
            sampleRange,
            *renderedArea_,
            props.impactColor,
            props.bandOpacity
        );
        populatePoints(
            geometry_.base,
            geometry_.sampleCount,
            sampleRange,
            *renderedArea_,
            drawPoints_
        );
        drawLine(
            layer,
            drawPoints_.data(),
            static_cast<uint32_t>(sampleRange.size()),
            props.baseColor,
            props.baseOpacity,
            props.baseWidth
        );
        populatePoints(
            geometry_.impact,
            geometry_.sampleCount,
            sampleRange,
            *renderedArea_,
            drawPoints_
        );
        drawLine(
            layer,
            drawPoints_.data(),
            static_cast<uint32_t>(sampleRange.size()),
            props.impactColor,
            props.impactOpacity,
            props.impactWidth
        );
    }
    drawCurveWithDiscontinuities(
        layer,
        geometry_,
        sampleRange,
        *renderedArea_,
        drawPoints_,
        props.curveColor,
        props.curveOpacity,
        props.curveWidth
    );
    drawMarker(layer, *renderedArea_, props);
}

FLASHMEM void CurvePreviewWidget::onDrawEvent(lv_event_t* event) {
    auto* self = static_cast<CurvePreviewWidget*>(
        lv_event_get_user_data(event)
    );
    if (self == nullptr) return;
    self->draw(lv_event_get_layer(event));
}

FLASHMEM void CurvePreviewWidget::onSizeChangedEvent(lv_event_t* event) {
    auto* self = static_cast<CurvePreviewWidget*>(
        lv_event_get_user_data(event)
    );
    if (self != nullptr) self->layout_dirty_ = true;
}

FLASHMEM void CurvePreviewWidget::onMarkerTimer(lv_timer_t* timer) {
    auto* self = static_cast<CurvePreviewWidget*>(
        lv_timer_get_user_data(timer)
    );
    if (self != nullptr) self->serviceMarker();
}

FLASHMEM void CurvePreviewWidget::render(
    const CurvePreviewWidgetProps& props
) {
    if (surface_ == nullptr) return;
    if (!props.visible) {
        if (visible_) {
            if (markerTimer_) markerTimer_->pause();
            lv_obj_add_flag(surface_, LV_OBJ_FLAG_HIDDEN);
            visible_ = false;
            rendered_ = false;
            geometry_.clear();
        }
        return;
    }
    if (!visible_) {
        lv_obj_clear_flag(surface_, LV_OBJ_FLAG_HIDDEN);
        visible_ = true;
        rendered_ = false;
    }

    // A retained parent can become visible in the same presentation pass.
    // Resolve its pending layout before deriving width-dependent geometry so
    // the first frame cannot cache an empty 0x0 surface.
    if (!rendered_ || layout_dirty_) {
        lv_obj_update_layout(surface_);
        layout_dirty_ = false;
    }
    lv_area_t surfaceArea{};
    lv_obj_get_coords(surface_, &surfaceArea);
    const lv_coord_t paddingX = std::max<lv_coord_t>(0, props.paddingX);
    const lv_coord_t paddingY = std::max<lv_coord_t>(0, props.paddingY);
    const lv_area_t area{
        .x1 = static_cast<lv_coord_t>(surfaceArea.x1 + paddingX),
        .y1 = static_cast<lv_coord_t>(surfaceArea.y1 + paddingY),
        .x2 = static_cast<lv_coord_t>(surfaceArea.x2 - paddingX),
        .y2 = static_cast<lv_coord_t>(surfaceArea.y2 - paddingY),
    };
    const bool areaChanged = !rendered_ || !sameArea(*renderedArea_, area);
    const bool geometryChanged = areaChanged || !rendered_ ||
        renderedProps_->sampleProvider != props.sampleProvider ||
        renderedProps_->sampleContext != props.sampleContext ||
        renderedProps_->geometryRevision != props.geometryRevision;
    const bool styleChanged = !rendered_ || staticStyleChanged(props);
    CurvePreviewMarker resolvedMarker = props.marker;
    if (props.markerProvider != nullptr &&
        !props.markerProvider(props.markerContext, resolvedMarker)) {
        resolvedMarker = {};
    }
    const bool markerChanged = !rendered_ ||
        !sameMarkerPixel(renderedProps_->marker, resolvedMarker);
    const bool markerServiceChanged = !rendered_ ||
        renderedProps_->markerProvider != props.markerProvider ||
        renderedProps_->markerContext != props.markerContext;

    if (!geometryChanged && !styleChanged && !markerChanged &&
        !markerServiceChanged) {
        return;
    }

    const CurvePreviewMarker previousMarker = rendered_
        ? renderedProps_->marker
        : CurvePreviewMarker{};
    renderedArea_ = area;
    bool tailPatched = false;
    if (geometryChanged) {
        OC_PERF_SCOPE(perfGeometry, "ui.curve-preview.geometry");
        const bool sameSampler = rendered_ && !areaChanged &&
            renderedProps_->sampleProvider == props.sampleProvider &&
            renderedProps_->sampleContext == props.sampleContext;
        bool updated = false;
        if (sameSampler && props.geometryUpdate ==
                CurvePreviewGeometryUpdate::PATCH_LAST) {
            updated = geometry_.patchLast(
                props.sampleProvider,
                props.sampleContext
            );
            tailPatched = updated;
        } else if (sameSampler && props.geometryUpdate ==
                       CurvePreviewGeometryUpdate::ADVANCE) {
            updated = geometry_.advance(
                props.geometryAdvance,
                props.sampleProvider,
                props.sampleContext
            );
        }
        if (!updated) {
            (void)geometry_.rebuild(
                lv_area_get_width(&area),
                lv_area_get_height(&area),
                props.sampleProvider,
                props.sampleContext
            );
            tailPatched = false;
        }
        OC_PERF_UNITS(
            perfGeometry,
            tailPatched
                ? 1U
                : (props.geometryUpdate ==
                           CurvePreviewGeometryUpdate::ADVANCE && updated
                       ? props.geometryAdvance
                       : geometry_.sampleCount),
            static_cast<uint32_t>(lv_area_get_width(&area))
        );
    }
    const bool fullInvalidation =
        (geometryChanged && !tailPatched) || styleChanged;
    if (!fullInvalidation && markerChanged) {
        invalidateMarker(previousMarker);
    }
    renderedProps_ = props;
    renderedProps_->marker = resolvedMarker;
    rendered_ = true;
    if (fullInvalidation) {
        lv_obj_invalidate(surface_);
    } else {
        if (tailPatched) invalidateTail();
        if (markerChanged) invalidateMarker(resolvedMarker);
    }
    if (markerTimer_) {
        if (props.markerProvider != nullptr) {
            markerTimer_->resume();
        } else {
            markerTimer_->pause();
        }
    }
}

}  // namespace ms::ui
