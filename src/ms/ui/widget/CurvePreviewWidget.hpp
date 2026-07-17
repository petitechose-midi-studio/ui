#pragma once

#include <cstdint>

#include <lvgl.h>

#include <oc/ui/lvgl/IWidget.hpp>

#include <ms/ui/widget/CurvePreviewGeometry.hpp>

namespace ms::ui {

struct CurvePreviewMarker {
    bool visible = false;
    uint16_t positionQ16 = 0U;
    uint16_t valueQ16 = 0U;
};

struct CurvePreviewWidgetProps {
    bool visible = false;
    CurvePreviewSampleProvider sampleProvider = nullptr;
    void* sampleContext = nullptr;
    uint32_t geometryRevision = 0U;

    bool showImpactBand = false;
    bool showCenterGuide = false;
    bool showRestGuide = false;
    uint16_t restValueQ16 = 0U;

    uint32_t curveColor = 0xFFFFFFU;
    uint32_t baseColor = 0xFFFFFFU;
    uint32_t impactColor = 0xFFFFFFU;
    uint32_t guideColor = 0xFFFFFFU;
    uint32_t markerColor = 0xFFFFFFU;
    lv_opa_t curveOpacity = LV_OPA_COVER;
    lv_opa_t baseOpacity = LV_OPA_60;
    lv_opa_t impactOpacity = LV_OPA_COVER;
    lv_opa_t bandOpacity = LV_OPA_20;
    lv_opa_t guideOpacity = LV_OPA_30;
    lv_coord_t curveWidth = 2;
    lv_coord_t baseWidth = 1;
    lv_coord_t impactWidth = 2;
    lv_coord_t markerRadius = 2;
    CurvePreviewMarker marker{};
};

/**
 * Retained, allocation-free curve presentation surface.
 *
 * The owner is responsible for allocating this object in the desired memory
 * region. MIDI Studio owners use makeExtmemUnique, so all fixed geometry stays
 * in PSRAM. render() never creates LVGL objects or allocates sample storage.
 */
class CurvePreviewWidget : public oc::ui::lvgl::IWidget {
public:
    explicit CurvePreviewWidget(lv_obj_t* parent);
    ~CurvePreviewWidget() override;

    CurvePreviewWidget(const CurvePreviewWidget&) = delete;
    CurvePreviewWidget& operator=(const CurvePreviewWidget&) = delete;

    void render(const CurvePreviewWidgetProps& props);
    lv_obj_t* getElement() const override { return surface_; }

    [[nodiscard]] uint8_t activeSampleCount() const {
        return geometry_.sampleCount;
    }

private:
    void createUi(lv_obj_t* parent);
    void draw(lv_layer_t* layer);
    void invalidateMarker(const CurvePreviewMarker& marker) const;
    [[nodiscard]] bool staticStyleChanged(
        const CurvePreviewWidgetProps& props
    ) const;
    static void onDrawEvent(lv_event_t* event);

    lv_obj_t* surface_ = nullptr;
    CurvePreviewGeometry geometry_{};
    std::array<lv_point_precise_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>
        drawPoints_{};
    CurvePreviewWidgetProps renderedProps_{};
    lv_area_t renderedArea_{};
    bool rendered_ = false;
    bool visible_ = false;
};

static_assert(
    sizeof(CurvePreviewGeometry) <= 600U,
    "Curve preview retained geometry exceeds the accepted PSRAM budget"
);
static_assert(
    sizeof(CurvePreviewWidget) <= 1536U,
    "Curve preview widget exceeds the accepted retained PSRAM budget"
);

}  // namespace ms::ui
