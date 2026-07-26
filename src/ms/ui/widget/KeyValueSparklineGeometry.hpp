#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ms::ui {

inline constexpr int KEY_VALUE_SPARKLINE_MAX_WIDTH = 110;
inline constexpr int KEY_VALUE_SPARKLINE_HEIGHT = 18;
inline constexpr std::size_t KEY_VALUE_SPARKLINE_DRAW_CHUNK = 16U;

struct KeyValueSparklineColumnRange {
    std::size_t begin = 0U;
    std::size_t end = 0U;

    [[nodiscard]] constexpr bool empty() const { return begin >= end; }
    [[nodiscard]] constexpr std::size_t size() const {
        return end > begin ? end - begin : 0U;
    }
};

/** Exact normalized coordinate for one physical column. */
[[nodiscard]] constexpr uint16_t keyValueSparklinePositionQ16(
    std::size_t column,
    std::size_t width
) {
    if (width <= 1U) return 0U;
    const std::size_t bounded = std::min(column, width - 1U);
    return static_cast<uint16_t>(
        (static_cast<uint64_t>(bounded) * 65535ULL) /
        static_cast<uint64_t>(width - 1U)
    );
}

/** Pixel coordinate for a normalized position over an inclusive extent. */
[[nodiscard]] constexpr int keyValueSparklineCoordinate(
    uint16_t positionQ16,
    int extent
) {
    if (extent <= 1) return 0;
    return static_cast<int>(
        (static_cast<uint32_t>(positionQ16) *
             static_cast<uint32_t>(extent - 1) +
         32767U) /
        65535U
    );
}

/**
 * Columns intersecting an absolute clip, plus one neighbour on either side
 * so adjacent line segments remain continuous.
 */
[[nodiscard]] constexpr KeyValueSparklineColumnRange
keyValueSparklineColumnsForClip(
    int surfaceX,
    int width,
    int clipX1,
    int clipX2
) {
    if (width <= 0 || clipX2 < surfaceX ||
        clipX1 > surfaceX + width - 1) {
        return {};
    }
    const int visibleBegin = std::max(0, clipX1 - surfaceX);
    const int visibleEnd = std::min(width - 1, clipX2 - surfaceX);
    const int begin = std::max(0, visibleBegin - 1);
    const int endInclusive = std::min(width - 1, visibleEnd + 1);
    return {
        static_cast<std::size_t>(begin),
        static_cast<std::size_t>(endInclusive + 1),
    };
}

}  // namespace ms::ui
