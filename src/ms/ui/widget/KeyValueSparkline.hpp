#pragma once

#include <cstdint>

#include <ms/ui/widget/KeyValueSparklineGeometry.hpp>

namespace ms::ui {

struct KeyValueSparkline;

struct KeyValueSparklineSample {
    uint16_t valueQ16 = 0U;
    bool discontinuityBefore = false;
};

struct KeyValueSparklineMarker {
    uint16_t positionQ16 = 0U;
    uint16_t valueQ16 = 0U;
    bool visible = false;
};

using KeyValueSparklineSampleProvider = bool (*)(
    const KeyValueSparkline& descriptor,
    uint16_t positionQ16,
    uint16_t previousPositionQ16,
    bool hasPrevious,
    KeyValueSparklineSample& out
);

using KeyValueSparklineMarkerProvider = bool (*)(
    const KeyValueSparkline& descriptor,
    uint32_t nowMs,
    KeyValueSparklineMarker& out
);

/**
 * Small retained descriptor. Musical rows never carry a width-sized table:
 * the five visible slots sample authored authority at their physical width.
 */
struct KeyValueSparkline {
    const void* context = nullptr;
    uint32_t identity = 0U;
    uint32_t geometryRevision = 0U;
    uint16_t runtimeIndex = UINT16_MAX;
    bool enabled = false;
    bool centerLine = false;
    KeyValueSparklineSampleProvider sampleProvider = nullptr;
    KeyValueSparklineMarkerProvider markerProvider = nullptr;
};

static_assert(
    sizeof(KeyValueSparkline) <= 40U,
    "Sparkline rows must retain only a compact sampler descriptor"
);
static_assert(
    sizeof(void*) != 4U || sizeof(KeyValueSparkline) == 24U,
    "32-bit controller descriptor footprint changed"
);

}  // namespace ms::ui
