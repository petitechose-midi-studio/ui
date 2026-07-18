#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

namespace ms::ui {

// MIDI Studio's native display is 320 pixels wide. Detailed authoring curves
// retain one derived sample per drawable pixel; compact rows naturally request
// fewer samples because their drawable width is smaller.
inline constexpr std::size_t CURVE_PREVIEW_MAX_SAMPLE_COUNT = 320U;
inline constexpr uint16_t CURVE_PREVIEW_NORMALIZED_MAX = 65535U;

struct CurvePreviewSample {
    uint16_t curve = 0U;
    uint16_t base = 0U;
    uint16_t impact = 0U;
    bool discontinuityBefore = false;
};

using CurvePreviewSampleProvider = bool (*)(
    void* context,
    uint16_t positionQ16,
    CurvePreviewSample& out
);

struct CurvePreviewRect {
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t x2 = -1;
    int32_t y2 = -1;

    [[nodiscard]] constexpr bool valid() const {
        return x1 <= x2 && y1 <= y2;
    }
};

[[nodiscard]] constexpr std::size_t curvePreviewSampleCountForWidth(
    int32_t width
) {
    if (width < 2) return 0U;
    return std::clamp<std::size_t>(
        static_cast<std::size_t>(width),
        2U,
        CURVE_PREVIEW_MAX_SAMPLE_COUNT
    );
}

[[nodiscard]] constexpr uint16_t curvePreviewPositionQ16(
    std::size_t index,
    std::size_t count
) {
    if (count < 2U) return 0U;
    const auto clamped = std::min(index, count - 1U);
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(clamped) * CURVE_PREVIEW_NORMALIZED_MAX +
         static_cast<uint32_t>((count - 1U) / 2U)) /
        static_cast<uint32_t>(count - 1U)
    );
}

[[nodiscard]] constexpr int32_t curvePreviewCoordinate(
    uint16_t valueQ16,
    int32_t origin,
    int32_t extent
) {
    if (extent <= 1) return origin;
    return origin + static_cast<int32_t>(
        (static_cast<uint32_t>(valueQ16) *
             static_cast<uint32_t>(extent - 1) +
         CURVE_PREVIEW_NORMALIZED_MAX / 2U) /
        CURVE_PREVIEW_NORMALIZED_MAX
    );
}

[[nodiscard]] constexpr int32_t curvePreviewY(
    uint16_t valueQ16,
    int32_t originY,
    int32_t height
) {
    return curvePreviewCoordinate(
        static_cast<uint16_t>(CURVE_PREVIEW_NORMALIZED_MAX - valueQ16),
        originY,
        height
    );
}

[[nodiscard]] constexpr CurvePreviewRect curvePreviewMarkerRect(
    int32_t originX,
    int32_t originY,
    int32_t width,
    int32_t height,
    uint16_t positionQ16,
    uint16_t valueQ16,
    int32_t radius
) {
    if (width < 1 || height < 1 || radius < 0) {
        return {
            .x1 = originX,
            .y1 = originY,
            .x2 = originX - 1,
            .y2 = originY - 1,
        };
    }
    const int32_t x = curvePreviewCoordinate(positionQ16, originX, width);
    const int32_t y = curvePreviewY(valueQ16, originY, height);
    return {
        .x1 = std::max(originX, x - radius),
        .y1 = std::max(originY, y - radius),
        .x2 = std::min(originX + width - 1, x + radius),
        .y2 = std::min(originY + height - 1, y + radius),
    };
}

struct CurvePreviewGeometry {
    std::array<uint16_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT> curve{};
    std::array<uint16_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT> base{};
    std::array<uint16_t, CURVE_PREVIEW_MAX_SAMPLE_COUNT> impact{};
    std::bitset<CURVE_PREVIEW_MAX_SAMPLE_COUNT> discontinuities{};
    uint16_t sampleCount = 0U;

    void clear() {
        sampleCount = 0U;
        discontinuities.reset();
    }

    [[nodiscard]] bool rebuild(
        int32_t width,
        int32_t height,
        CurvePreviewSampleProvider provider,
        void* context
    ) {
        clear();
        const std::size_t count = curvePreviewSampleCountForWidth(width);
        if (count == 0U || height < 2 || provider == nullptr) return false;

        for (std::size_t index = 0; index < count; ++index) {
            const uint16_t position = curvePreviewPositionQ16(index, count);
            CurvePreviewSample sample{};
            if (!provider(context, position, sample)) {
                clear();
                return false;
            }
            curve[index] = sample.curve;
            base[index] = sample.base;
            impact[index] = sample.impact;
            if (index > 0U && sample.discontinuityBefore) {
                // index is bounded by CURVE_PREVIEW_MAX_SAMPLE_COUNT above;
                // unchecked access avoids pulling the embedded exception path.
                discontinuities[index] = true;
            }
        }
        sampleCount = static_cast<uint16_t>(count);
        return true;
    }
};

}  // namespace ms::ui
