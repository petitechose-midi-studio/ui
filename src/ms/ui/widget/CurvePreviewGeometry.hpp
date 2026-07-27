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
 * REBUILD is the safe default for authored curves. REBUILD_DAMAGE samples the
 * complete authored curve but retains a compact damage map so the renderer can
 * invalidate only changed segments. PATCH_LAST and ADVANCE are reserved for
 * pixel-bucketed rolling traces whose logical samples move in lockstep with
 * the retained screen columns.
 */
enum class CurvePreviewGeometryUpdate : uint8_t {
    REBUILD = 0,
    REBUILD_DAMAGE,
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

// Damage is grouped into a bounded number of horizontal tiles. This keeps
// LVGL's invalidation queue bounded while still avoiding a full-height redraw
// when an authored curve changes across the whole display width.
inline constexpr std::size_t CURVE_PREVIEW_DAMAGE_TILE_SAMPLE_COUNT = 32U;
inline constexpr std::size_t CURVE_PREVIEW_DAMAGE_TILE_COUNT =
    (CURVE_PREVIEW_MAX_SAMPLE_COUNT +
     CURVE_PREVIEW_DAMAGE_TILE_SAMPLE_COUNT - 1U) /
    CURVE_PREVIEW_DAMAGE_TILE_SAMPLE_COUNT;

struct CurvePreviewDamageTile {
    uint16_t firstSample = CURVE_PREVIEW_NORMALIZED_MAX;
    uint16_t lastSample = 0U;
    uint16_t minimumValue = CURVE_PREVIEW_NORMALIZED_MAX;
    uint16_t maximumValue = 0U;

    [[nodiscard]] constexpr bool dirty() const {
        return firstSample != CURVE_PREVIEW_NORMALIZED_MAX;
    }

    void include(std::size_t sampleIndex, uint16_t value) {
        const auto index = static_cast<uint16_t>(sampleIndex);
        if (!dirty()) {
            firstSample = index;
            lastSample = index;
            minimumValue = value;
            maximumValue = value;
            return;
        }
        firstSample = std::min(firstSample, index);
        lastSample = std::max(lastSample, index);
        minimumValue = std::min(minimumValue, value);
        maximumValue = std::max(maximumValue, value);
    }
};

struct CurvePreviewDamage {
    std::array<
        CurvePreviewDamageTile,
        CURVE_PREVIEW_DAMAGE_TILE_COUNT
    > curveTiles{};
    /**
     * Base/impact rails and their filled delta band share one plane. Keeping
     * it separate from the foreground curve avoids turning two narrow,
     * vertically distant edits into one almost full-height rectangle.
     */
    std::array<
        CurvePreviewDamageTile,
        CURVE_PREVIEW_DAMAGE_TILE_COUNT
    > impactTiles{};
    uint16_t sampleCount = 0U;
    uint16_t changedSampleCount = 0U;

    void clear() {
        curveTiles = {};
        impactTiles = {};
        sampleCount = 0U;
        changedSampleCount = 0U;
    }

    void reset(std::size_t count) {
        clear();
        sampleCount = static_cast<uint16_t>(count);
    }

    void includeCurve(std::size_t sampleIndex, uint16_t value) {
        if (sampleIndex >= sampleCount) return;
        const auto tileIndex =
            sampleIndex / CURVE_PREVIEW_DAMAGE_TILE_SAMPLE_COUNT;
        if (tileIndex >= curveTiles.size()) return;
        curveTiles[tileIndex].include(sampleIndex, value);
    }

    void includeImpact(std::size_t sampleIndex, uint16_t value) {
        if (sampleIndex >= sampleCount) return;
        const auto tileIndex =
            sampleIndex / CURVE_PREVIEW_DAMAGE_TILE_SAMPLE_COUNT;
        if (tileIndex >= impactTiles.size()) return;
        impactTiles[tileIndex].include(sampleIndex, value);
    }

    [[nodiscard]] std::size_t dirtyTileCount() const {
        const auto countDirty = [](const auto& tiles) {
            return static_cast<std::size_t>(std::count_if(
                tiles.begin(),
                tiles.end(),
                [](const CurvePreviewDamageTile& tile) {
                    return tile.dirty();
                }
            ));
        };
        return countDirty(curveTiles) + countDirty(impactTiles);
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

[[nodiscard]] constexpr CurvePreviewRect curvePreviewDamageRect(
    const CurvePreviewDamageTile& tile,
    std::size_t sampleCount,
    int32_t originX,
    int32_t originY,
    int32_t width,
    int32_t height,
    int32_t margin
) {
    if (!tile.dirty() || sampleCount < 2U || width < 2 || height < 2 ||
        margin < 0) {
        return {
            .x1 = originX,
            .y1 = originY,
            .x2 = originX - 1,
            .y2 = originY - 1,
        };
    }
    const int32_t firstX = curvePreviewCoordinate(
        curvePreviewPositionQ16(tile.firstSample, sampleCount),
        originX,
        width
    );
    const int32_t lastX = curvePreviewCoordinate(
        curvePreviewPositionQ16(tile.lastSample, sampleCount),
        originX,
        width
    );
    const int32_t top = curvePreviewY(
        tile.maximumValue,
        originY,
        height
    );
    const int32_t bottom = curvePreviewY(
        tile.minimumValue,
        originY,
        height
    );
    return {
        .x1 = std::max(originX, firstX - margin),
        .y1 = std::max(originY, top - margin),
        .x2 = std::min(originX + width - 1, lastX + margin),
        .y2 = std::min(originY + height - 1, bottom + margin),
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

    /**
     * Re-sample retained geometry in place and report the old/new raster
     * envelope of every changed segment. The caller must only use this when
     * width and provider identity are stable; false clears geometry exactly
     * like rebuild() after a rejected sample.
     */
    [[nodiscard]] bool rebuildWithDamage(
        int32_t width,
        int32_t height,
        CurvePreviewSampleProvider provider,
        void* context,
        bool includeBaseAndImpact,
        CurvePreviewDamage& damage
    ) {
        const std::size_t count = curvePreviewSampleCountForWidth(width);
        if (count < 2U || count != sampleCount || height < 2 ||
            provider == nullptr) {
            damage.clear();
            return false;
        }

        damage.reset(count);
        CurvePreviewSample previousOld{};
        CurvePreviewSample previousNew{};
        bool previousCurveChanged = false;
        bool previousBaseChanged = false;
        bool previousImpactChanged = false;

        for (std::size_t index = 0U; index < count; ++index) {
            CurvePreviewSample oldSample{
                .curve = curve[index],
                .base = base[index],
                .impact = impact[index],
                .discontinuityBefore =
                    index > 0U && discontinuities[index],
            };
            CurvePreviewSample newSample{};
            const uint16_t position =
                curvePreviewPositionQ16(index, count);
            if (!provider(context, position, newSample)) {
                clear();
                damage.clear();
                return false;
            }
            if (index == 0U) newSample.discontinuityBefore = false;

            const bool discontinuityChanged =
                oldSample.discontinuityBefore !=
                newSample.discontinuityBefore;
            const bool curveChanged =
                oldSample.curve != newSample.curve ||
                discontinuityChanged;
            const bool baseChanged = includeBaseAndImpact &&
                oldSample.base != newSample.base;
            const bool impactChanged = includeBaseAndImpact &&
                oldSample.impact != newSample.impact;
            const bool changed =
                curveChanged || baseChanged || impactChanged;
            if (changed) ++damage.changedSampleCount;

            curve[index] = newSample.curve;
            base[index] = newSample.base;
            impact[index] = newSample.impact;
            discontinuities[index] =
                index > 0U && newSample.discontinuityBefore;

            if (index == 0U) {
                if (curveChanged) {
                    damage.includeCurve(index, oldSample.curve);
                    damage.includeCurve(index, newSample.curve);
                }
                if (baseChanged) {
                    damage.includeImpact(index, oldSample.base);
                    damage.includeImpact(index, newSample.base);
                }
                if (impactChanged) {
                    damage.includeImpact(index, oldSample.impact);
                    damage.includeImpact(index, newSample.impact);
                }
            } else {
                // A changed endpoint affects both adjacent line segments.
                // Track each visual plane independently: an unchanged Base
                // must not expand an amplitude edit down to the Base rail.
                if (previousCurveChanged || curveChanged) {
                    damage.includeCurve(index - 1U, previousOld.curve);
                    damage.includeCurve(index - 1U, previousNew.curve);
                    damage.includeCurve(index, oldSample.curve);
                    damage.includeCurve(index, newSample.curve);
                }
                if (previousBaseChanged || baseChanged) {
                    damage.includeImpact(index - 1U, previousOld.base);
                    damage.includeImpact(index - 1U, previousNew.base);
                    damage.includeImpact(index, oldSample.base);
                    damage.includeImpact(index, newSample.base);
                }
                if (previousImpactChanged || impactChanged) {
                    damage.includeImpact(index - 1U, previousOld.impact);
                    damage.includeImpact(index - 1U, previousNew.impact);
                    damage.includeImpact(index, oldSample.impact);
                    damage.includeImpact(index, newSample.impact);
                }
            }

            previousOld = oldSample;
            previousNew = newSample;
            previousCurveChanged = curveChanged;
            previousBaseChanged = baseChanged;
            previousImpactChanged = impactChanged;
        }
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
