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

/**
 * Minimum retained-geometry mutation announced by a sample owner.
 *
 * REBUILD is the safe default for authored curves. PATCH_LAST and ADVANCE are
 * reserved for pixel-bucketed rolling traces whose logical samples move in
 * lockstep with the retained screen columns.
 */
enum class CurvePreviewGeometryUpdate : uint8_t {
    REBUILD = 0,
    PATCH_LAST,
    ADVANCE,
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

struct CurvePreviewSampleRange {
    std::size_t begin = 0U;
    std::size_t end = 0U;

    [[nodiscard]] constexpr bool empty() const { return begin >= end; }
    [[nodiscard]] constexpr std::size_t size() const {
        return empty() ? 0U : end - begin;
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

/**
 * Resolve the smallest sample range needed to draw an X clip. One neighbour
 * is retained on each side so line segments crossing the clip boundary remain
 * continuous. This lets marker-only invalidations transform a handful of
 * samples instead of rebuilding every display column.
 */
[[nodiscard]] constexpr CurvePreviewSampleRange curvePreviewSampleRangeForClip(
    int32_t originX,
    int32_t width,
    std::size_t sampleCount,
    int32_t clipX1,
    int32_t clipX2
) {
    if (width < 2 || sampleCount < 2U || clipX1 > clipX2 ||
        clipX2 < originX || clipX1 >= originX + width) {
        return {};
    }
    const int32_t lastPixel = width - 1;
    const int32_t clippedX1 = std::clamp(
        clipX1 - originX,
        int32_t{0},
        lastPixel
    );
    const int32_t clippedX2 = std::clamp(
        clipX2 - originX,
        int32_t{0},
        lastPixel
    );
    const auto lastSample = sampleCount - 1U;
    const auto first = static_cast<std::size_t>(
        (static_cast<uint64_t>(clippedX1) * lastSample) /
        static_cast<uint64_t>(lastPixel)
    );
    const auto last = static_cast<std::size_t>(
        (static_cast<uint64_t>(clippedX2) * lastSample +
         static_cast<uint64_t>(lastPixel - 1)) /
        static_cast<uint64_t>(lastPixel)
    );
    return {
        .begin = first > 0U ? first - 1U : 0U,
        .end = std::min(sampleCount, last + 2U),
    };
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

    [[nodiscard]] bool patchLast(
        CurvePreviewSampleProvider provider,
        void* context
    ) {
        if (sampleCount < 2U || provider == nullptr) return false;
        return replaceSample(sampleCount - 1U, provider, context);
    }

    /**
     * Shift a rolling trace left and sample only the newly exposed columns.
     * The caller must request a full rebuild when the provider is not a
     * pixel-aligned rolling trace or when advanceCount spans the full surface.
     */
    [[nodiscard]] bool advance(
        uint16_t advanceCount,
        CurvePreviewSampleProvider provider,
        void* context
    ) {
        if (sampleCount < 2U || provider == nullptr || advanceCount == 0U ||
            advanceCount >= sampleCount) {
            return false;
        }
        const std::size_t retained = sampleCount - advanceCount;
        std::move(
            curve.begin() + advanceCount,
            curve.begin() + sampleCount,
            curve.begin()
        );
        std::move(
            base.begin() + advanceCount,
            base.begin() + sampleCount,
            base.begin()
        );
        std::move(
            impact.begin() + advanceCount,
            impact.begin() + sampleCount,
            impact.begin()
        );
        for (std::size_t index = 0U; index < retained; ++index) {
            discontinuities[index] =
                discontinuities[index + advanceCount];
        }
        for (std::size_t index = retained; index < sampleCount; ++index) {
            discontinuities[index] = false;
            if (!replaceSample(index, provider, context)) return false;
        }
        discontinuities[0] = false;
        return true;
    }

private:
    [[nodiscard]] bool replaceSample(
        std::size_t index,
        CurvePreviewSampleProvider provider,
        void* context
    ) {
        if (index >= sampleCount || provider == nullptr) return false;
        CurvePreviewSample sample{};
        if (!provider(
                context,
                curvePreviewPositionQ16(index, sampleCount),
                sample
            )) {
            return false;
        }
        curve[index] = sample.curve;
        base[index] = sample.base;
        impact[index] = sample.impact;
        discontinuities[index] = index > 0U && sample.discontinuityBefore;
        return true;
    }
};

}  // namespace ms::ui
