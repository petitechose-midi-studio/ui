#pragma once

#include <cstdint>
#include <optional>

#include <lvgl.h>
#include <oc/ui/lvgl/PausableTimer.hpp>

#include <ms/ui/widget/CurvePreviewGeometry.hpp>

namespace ms::ui {

struct CurvePreviewMarker {
    bool visible = false;
    uint16_t positionQ16 = 0U;
    uint16_t valueQ16 = 0U;
};

using CurvePreviewMarkerProvider = bool (*)(
    void* context,
    CurvePreviewMarker& out
);

struct CurvePreviewWidgetProps {
    bool visible = false;
    CurvePreviewSampleProvider sampleProvider = nullptr;
    void* sampleContext = nullptr;
    uint32_t geometryRevision = 0U;
    CurvePreviewGeometryUpdate geometryUpdate =
        CurvePreviewGeometryUpdate::REBUILD;
    uint16_t geometryAdvance = 0U;
    CurvePreviewMarkerProvider markerProvider = nullptr;
    void* markerContext = nullptr;

    bool showImpactBand = false;
    bool showCenterGuide = false;
    bool showRestGuide = false;
    bool showVerticalGuide = false;
    uint16_t restValueQ16 = 0U;
    uint16_t verticalGuidePositionQ16 = 0U;
    lv_coord_t paddingX = 0;
    lv_coord_t paddingY = 0;

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
class CurvePreviewWidget {
public:
    explicit CurvePreviewWidget(lv_obj_t* parent);
    ~CurvePreviewWidget();

    CurvePreviewWidget(const CurvePreviewWidget&) = delete;
    CurvePreviewWidget& operator=(const CurvePreviewWidget&) = delete;

    void render(const CurvePreviewWidgetProps& props);
    /**
     * Hot rolling-trace path. Updates retained columns and invalidation only;
     * it never resolves layout, styles or marker providers.
     */
    [[nodiscard]] bool updateRollingGeometry(
        uint32_t geometryRevision,
        CurvePreviewGeometryUpdate update,
        uint16_t advanceCount = 0U
    );
    [[nodiscard]] lv_obj_t* getElement() const { return surface_; }

    [[nodiscard]] uint16_t activeSampleCount() const {
        return geometry_.sampleCount;
    }

private:
    static constexpr uint32_t MARKER_SERVICE_PERIOD_MS = 1U;

    void createUi(lv_obj_t* parent);
    void draw(lv_layer_t* layer);
    void invalidateDamage(const CurvePreviewDamage& damage) const;
    void invalidateTail() const;
    void invalidateMarker(const CurvePreviewMarker& marker) const;
    void serviceMarker();
    [[nodiscard]] bool sameMarkerPixel(
        const CurvePreviewMarker& lhs,
        const CurvePreviewMarker& rhs
    ) const;
    [[nodiscard]] bool staticStyleChanged(
        const CurvePreviewWidgetProps& props
    ) const;
    static void onDrawEvent(lv_event_t* event);
    static void onSizeChangedEvent(lv_event_t* event);
    static void onMarkerTimer(lv_timer_t* timer);

    lv_obj_t* surface_ = nullptr;
    CurvePreviewGeometry geometry_{};
    std::array<lv_point_precise_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT>
        drawPoints_{};
    // Keep the cache disengaged until the first render. Constructing a default
    // props value here emits a 100-byte initialized-data template on Teensy;
    // optional keeps that cold cache entirely inside the PSRAM-owned widget.
    std::optional<CurvePreviewWidgetProps> renderedProps_{};
    std::optional<lv_area_t> renderedArea_{};
    std::optional<oc::ui::lvgl::PausableTimer> markerTimer_{};
    bool rendered_ = false;
    bool visible_ = false;
    bool layout_dirty_ = true;
};

static_assert(
    sizeof(CurvePreviewGeometry) <= 2048U,
    "Curve preview retained geometry exceeds the accepted PSRAM budget"
);
static_assert(
    sizeof(CurvePreviewWidget) <= 8192U,
    "Curve preview widget exceeds the accepted retained PSRAM budget"
);

}  // namespace ms::ui
