#include <ms/ui/widget/CurvePreviewWidget.hpp>

#include <algorithm>
#include <array>

#include <config/PlatformCompat.hpp>
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

FLASHMEM void populatePoints(
    const std::array<uint16_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>& values,
    uint8_t count,
    const lv_area_t& area,
    std::array<lv_point_precise_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>& out
) {
    const int32_t width = lv_area_get_width(&area);
    const int32_t height = lv_area_get_height(&area);
    for (uint8_t index = 0U; index < count; ++index) {
        const uint16_t position = curvePreviewPositionQ16(index, count);
        out[index] = {
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
    const lv_area_t& area,
    std::array<lv_point_precise_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>& points,
    uint32_t color,
    lv_opa_t opacity,
    lv_coord_t width
) {
    const std::size_t count = geometry.sampleCount;
    if (count < 2U) return;
    populatePoints(geometry.curve, geometry.sampleCount, area, points);
    std::size_t runStart = 0U;
    for (std::size_t index = 1U; index < count; ++index) {
        if (!geometry.discontinuities.test(index)) continue;
        if (index - runStart >= 2U) {
            drawLine(
                layer,
                points.data() + runStart,
                static_cast<uint32_t>(index - runStart),
                color,
                opacity,
                width
            );
        }
        std::array<lv_point_precise_t, 3> step{{
            points[index - 1U],
            {points[index].x, points[index - 1U].y},
            points[index],
        }};
        drawLine(layer, step.data(), step.size(), color, opacity, width);
        runStart = index;
    }
    if (count - runStart >= 2U) {
        drawLine(
            layer,
            points.data() + runStart,
            static_cast<uint32_t>(count - runStart),
            color,
            opacity,
            width
        );
    }
}

FLASHMEM void drawImpactBand(
    lv_layer_t* layer,
    const CurvePreviewGeometry& geometry,
    const lv_area_t& area,
    uint32_t color,
    lv_opa_t opacity
) {
    const std::size_t count = geometry.sampleCount;
    if (count < 2U || opacity == LV_OPA_TRANSP) return;
    const int32_t areaWidth = lv_area_get_width(&area);
    const int32_t areaHeight = lv_area_get_height(&area);
    for (std::size_t index = 0U; index < count; ++index) {
        const auto next = std::min(index + 1U, count - 1U);
        const int32_t x = curvePreviewCoordinate(
            curvePreviewPositionQ16(index, count),
            area.x1,
            areaWidth
        );
        const int32_t nextX = curvePreviewCoordinate(
            curvePreviewPositionQ16(next, count),
            area.x1,
            areaWidth
        );
        const lv_coord_t width = static_cast<lv_coord_t>(
            std::max<int32_t>(1, nextX - x + 1)
        );
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
        drawLine(layer, points.data(), points.size(), color, opacity, width);
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
    lv_obj_add_flag(surface_, LV_OBJ_FLAG_HIDDEN);
}

FLASHMEM bool CurvePreviewWidget::staticStyleChanged(
    const CurvePreviewWidgetProps& props
) const {
    const auto& previous = renderedProps_;
    return previous.showImpactBand != props.showImpactBand ||
           previous.showCenterGuide != props.showCenterGuide ||
           previous.showRestGuide != props.showRestGuide ||
           previous.restValueQ16 != props.restValueQ16 ||
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
        renderedArea_.x1,
        renderedArea_.y1,
        lv_area_get_width(&renderedArea_),
        lv_area_get_height(&renderedArea_),
        marker.positionQ16,
        marker.valueQ16,
        renderedProps_.markerRadius + 1
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

FLASHMEM void CurvePreviewWidget::draw(lv_layer_t* layer) {
    if (!rendered_ || geometry_.sampleCount < 2U || layer == nullptr) return;
    if (renderedProps_.showCenterGuide) {
        drawGuide(
            layer,
            renderedArea_,
            32768U,
            renderedProps_.guideColor,
            renderedProps_.guideOpacity
        );
    }
    if (renderedProps_.showRestGuide &&
        (!renderedProps_.showCenterGuide ||
         renderedProps_.restValueQ16 != 32768U)) {
        drawGuide(
            layer,
            renderedArea_,
            renderedProps_.restValueQ16,
            renderedProps_.guideColor,
            renderedProps_.guideOpacity
        );
    }
    if (renderedProps_.showImpactBand) {
        drawImpactBand(
            layer,
            geometry_,
            renderedArea_,
            renderedProps_.impactColor,
            renderedProps_.bandOpacity
        );
        populatePoints(
            geometry_.base,
            geometry_.sampleCount,
            renderedArea_,
            drawPoints_
        );
        drawLine(
            layer,
            drawPoints_.data(),
            geometry_.sampleCount,
            renderedProps_.baseColor,
            renderedProps_.baseOpacity,
            renderedProps_.baseWidth
        );
        populatePoints(
            geometry_.impact,
            geometry_.sampleCount,
            renderedArea_,
            drawPoints_
        );
        drawLine(
            layer,
            drawPoints_.data(),
            geometry_.sampleCount,
            renderedProps_.impactColor,
            renderedProps_.impactOpacity,
            renderedProps_.impactWidth
        );
    }
    drawCurveWithDiscontinuities(
        layer,
        geometry_,
        renderedArea_,
        drawPoints_,
        renderedProps_.curveColor,
        renderedProps_.curveOpacity,
        renderedProps_.curveWidth
    );
    drawMarker(layer, renderedArea_, renderedProps_);
}

FLASHMEM void CurvePreviewWidget::onDrawEvent(lv_event_t* event) {
    auto* self = static_cast<CurvePreviewWidget*>(
        lv_event_get_user_data(event)
    );
    if (self == nullptr) return;
    self->draw(lv_event_get_layer(event));
}

FLASHMEM void CurvePreviewWidget::render(
    const CurvePreviewWidgetProps& props
) {
    if (surface_ == nullptr) return;
    if (!props.visible) {
        if (visible_) {
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

    lv_area_t area{};
    lv_obj_get_coords(surface_, &area);
    const bool areaChanged = !rendered_ || !sameArea(renderedArea_, area);
    const bool geometryChanged = areaChanged || !rendered_ ||
        renderedProps_.sampleProvider != props.sampleProvider ||
        renderedProps_.sampleContext != props.sampleContext ||
        renderedProps_.geometryRevision != props.geometryRevision;
    const bool styleChanged = !rendered_ || staticStyleChanged(props);
    const bool markerChanged = !rendered_ ||
        !sameMarker(renderedProps_.marker, props.marker);

    if (!geometryChanged && !styleChanged && !markerChanged) return;

    const CurvePreviewMarker previousMarker = renderedProps_.marker;
    renderedArea_ = area;
    if (geometryChanged) {
        (void)geometry_.rebuild(
            lv_area_get_width(&area),
            lv_area_get_height(&area),
            props.sampleProvider,
            props.sampleContext
        );
    }
    const bool fullInvalidation = geometryChanged || styleChanged;
    if (!fullInvalidation && markerChanged) {
        invalidateMarker(previousMarker);
    }
    renderedProps_ = props;
    rendered_ = true;
    if (fullInvalidation) {
        lv_obj_invalidate(surface_);
    } else if (markerChanged) {
        invalidateMarker(props.marker);
    }
}

}  // namespace ms::ui
