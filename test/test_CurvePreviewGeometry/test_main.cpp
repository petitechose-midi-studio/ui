#include <cassert>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <ms/ui/widget/CurvePreviewGeometry.hpp>
#include <ms/ui/widget/KeyValueSparklineGeometry.hpp>

namespace {

struct SampleContext {
    std::size_t calls = 0U;
    std::size_t rejectAt = static_cast<std::size_t>(-1);
    uint16_t previousPosition = 0U;
};

struct RollingContext {
    std::array<uint16_t, 8> values{};
    std::size_t calls = 0U;
};

bool sampleRolling(
    void* rawContext,
    uint16_t positionQ16,
    ms::ui::CurvePreviewSample& out
) {
    auto& context = *static_cast<RollingContext*>(rawContext);
    ++context.calls;
    const std::size_t index = std::min<std::size_t>(
        context.values.size() - 1U,
        (static_cast<uint32_t>(positionQ16) *
             (context.values.size() - 1U) +
         32767U) /
            65535U
    );
    out.curve = context.values[index];
    out.base = context.values[index];
    out.impact = context.values[index];
    return true;
}

bool sampleRamp(
    void* rawContext,
    uint16_t positionQ16,
    ms::ui::CurvePreviewSample& out
) {
    auto& context = *static_cast<SampleContext*>(rawContext);
    const std::size_t call = context.calls++;
    if (call == context.rejectAt) return false;
    out.curve = positionQ16;
    out.base = 16384U;
    out.impact = static_cast<uint16_t>(65535U - positionQ16);
    out.discontinuityBefore = call > 0U &&
        context.previousPosition < 32768U && positionQ16 >= 32768U;
    context.previousPosition = positionQ16;
    return true;
}

void testWidthDerivedDensity() {
    using ms::ui::curvePreviewSampleCountForWidth;
    assert(curvePreviewSampleCountForWidth(0) == 0U);
    assert(curvePreviewSampleCountForWidth(1) == 0U);
    assert(curvePreviewSampleCountForWidth(2) == 2U);
    assert(curvePreviewSampleCountForWidth(180) == 180U);
    assert(curvePreviewSampleCountForWidth(304) == 304U);
    assert(curvePreviewSampleCountForWidth(320) == 320U);
    assert(curvePreviewSampleCountForWidth(1000) == 320U);
    std::cout << "[PASS] width-derived density is one column per native pixel\n";
}

void testNormalizedMappingAndGuides() {
    using namespace ms::ui;
    assert(curvePreviewPositionQ16(0U, 304U) == 0U);
    assert(curvePreviewPositionQ16(303U, 304U) == 65535U);
    assert(curvePreviewCoordinate(0U, 8, 304) == 8);
    assert(curvePreviewCoordinate(65535U, 8, 304) == 311);
    for (std::size_t index = 0U; index < 304U; ++index) {
        assert(curvePreviewCoordinate(
            curvePreviewPositionQ16(index, 304U),
            8,
            304
        ) == static_cast<int32_t>(8U + index));
    }
    assert(curvePreviewY(0U, 20, 92) == 111);
    assert(curvePreviewY(65535U, 20, 92) == 20);
    const int32_t center = curvePreviewY(32768U, 20, 92);
    assert(center == 65 || center == 66);
    assert(curvePreviewY(16384U, 20, 92) > center);
    std::cout << "[PASS] endpoints and center/rest guide mapping are exact\n";
}

void testGeometryAndDiscontinuity() {
    ms::ui::CurvePreviewGeometry geometry{};
    SampleContext context{};
    assert(geometry.rebuild(304, 92, sampleRamp, &context));
    assert(geometry.sampleCount == 304U);
    assert(context.calls == 304U);
    assert(geometry.curve.front() == 0U);
    assert(geometry.curve[303] == 65535U);
    assert(geometry.base.front() == 16384U);
    assert(geometry.impact.front() == 65535U);
    assert(geometry.impact[303] == 0U);
    assert(geometry.discontinuities.count() == 1U);
    std::size_t transition = geometry.sampleCount;
    for (std::size_t index = 1U; index < geometry.sampleCount; ++index) {
        if (geometry.discontinuities.test(index)) {
            transition = index;
            break;
        }
    }
    assert(transition > 0U && transition < geometry.sampleCount);
    assert(geometry.curve[transition - 1U] < 32768U);
    assert(geometry.curve[transition] >= 32768U);
    std::cout << "[PASS] compact geometry preserves explicit discontinuities\n";
}

void testRejectedSamplingClearsGeometry() {
    ms::ui::CurvePreviewGeometry geometry{};
    SampleContext accepted{};
    assert(geometry.rebuild(304, 92, sampleRamp, &accepted));
    SampleContext rejected{.rejectAt = 5U};
    assert(!geometry.rebuild(304, 92, sampleRamp, &rejected));
    assert(geometry.sampleCount == 0U);
    assert(geometry.discontinuities.none());
    assert(!geometry.rebuild(1, 92, sampleRamp, &accepted));
    assert(!geometry.rebuild(304, 1, sampleRamp, &accepted));
    assert(!geometry.rebuild(304, 92, nullptr, nullptr));
    std::cout << "[PASS] invalid or rejected sampling cannot leave stale geometry\n";
}

void testRollingGeometryTouchesOnlyExposedColumns() {
    ms::ui::CurvePreviewGeometry geometry{};
    RollingContext context{};
    for (std::size_t index = 0U; index < context.values.size(); ++index) {
        context.values[index] = static_cast<uint16_t>(index * 100U);
    }
    assert(geometry.rebuild(8, 8, sampleRolling, &context));
    assert(context.calls == 8U);

    for (std::size_t index = 0U; index < context.values.size(); ++index) {
        context.values[index] = static_cast<uint16_t>((index + 1U) * 100U);
    }
    context.calls = 0U;
    assert(geometry.advance(1U, sampleRolling, &context));
    assert(context.calls == 1U);
    for (std::size_t index = 0U; index < context.values.size(); ++index) {
        assert(geometry.curve[index] == context.values[index]);
    }

    context.values.back() = 4242U;
    context.calls = 0U;
    assert(geometry.patchLast(sampleRolling, &context));
    assert(context.calls == 1U);
    assert(geometry.curve[7] == 4242U);
    assert(!geometry.advance(0U, sampleRolling, &context));
    assert(!geometry.advance(8U, sampleRolling, &context));
    std::cout << "[PASS] rolling geometry samples only changed columns\n";
}

void testMarkerRectanglesStayClipped() {
    using ms::ui::curvePreviewMarkerRect;
    const auto low = curvePreviewMarkerRect(8, 20, 304, 92, 0U, 0U, 3);
    assert(low.valid());
    assert(low.x1 == 8 && low.x2 == 11);
    assert(low.y1 == 108 && low.y2 == 111);
    const auto high = curvePreviewMarkerRect(
        8,
        20,
        304,
        92,
        65535U,
        65535U,
        3
    );
    assert(high.valid());
    assert(high.x1 == 308 && high.x2 == 311);
    assert(high.y1 == 20 && high.y2 == 23);
    const auto subPixelA = curvePreviewMarkerRect(
        8, 20, 304, 92, 10000U, 20000U, 3
    );
    const auto subPixelB = curvePreviewMarkerRect(
        8, 20, 304, 92, 10001U, 20001U, 3
    );
    assert(subPixelA.x1 == subPixelB.x1 && subPixelA.x2 == subPixelB.x2);
    assert(subPixelA.y1 == subPixelB.y1 && subPixelA.y2 == subPixelB.y2);
    assert(!curvePreviewMarkerRect(0, 0, 0, 10, 0U, 0U, 2).valid());
    std::cout << "[PASS] marker invalidation rectangles are exact and clipped\n";
}

void testClipDerivedSampleRange() {
    using ms::ui::curvePreviewSampleRangeForClip;
    const auto full = curvePreviewSampleRangeForClip(
        8, 304, 304U, 8, 311
    );
    assert(full.begin == 0U && full.end == 304U);

    const auto marker = curvePreviewSampleRangeForClip(
        8, 304, 304U, 108, 114
    );
    assert(marker.begin == 99U);
    assert(marker.end == 108U);
    assert(marker.size() == 9U);

    const auto scaled = curvePreviewSampleRangeForClip(
        0, 1000, 320U, 500, 505
    );
    assert(!scaled.empty());
    assert(scaled.size() <= 6U);
    assert(scaled.begin > 0U && scaled.end < 320U);

    assert(curvePreviewSampleRangeForClip(
        8, 304, 304U, -20, 7
    ).empty());
    assert(curvePreviewSampleRangeForClip(
        8, 304, 304U, 312, 400
    ).empty());
    std::cout << "[PASS] draw sample range follows the LVGL clip with neighbours\n";
}

void testKeyValueSparklinePixelContract() {
    using namespace ms::ui;
    for (const std::size_t width : {58U, 110U}) {
        assert(keyValueSparklinePositionQ16(0U, width) == 0U);
        assert(keyValueSparklinePositionQ16(width - 1U, width) == 65535U);
        for (std::size_t column = 0U; column < width; ++column) {
            assert(keyValueSparklineCoordinate(
                keyValueSparklinePositionQ16(column, width),
                static_cast<int>(width)
            ) == static_cast<int>(column));
        }
    }
    const auto compactMarker = keyValueSparklineColumnsForClip(
        20,
        58,
        43,
        47
    );
    assert(compactMarker.begin == 22U);
    assert(compactMarker.end == 29U);
    assert(compactMarker.size() == 7U);
    const auto wideMarker = keyValueSparklineColumnsForClip(
        5,
        110,
        60,
        62
    );
    assert(wideMarker.begin == 54U);
    assert(wideMarker.end == 59U);
    assert(wideMarker.size() == 5U);
    assert(keyValueSparklineColumnsForClip(20, 58, 0, 19).empty());
    assert(keyValueSparklineColumnsForClip(20, 58, 78, 100).empty());
    static_assert(KEY_VALUE_SPARKLINE_DRAW_CHUNK <= 16U);
    std::cout << "[PASS] key/value sparklines sample one point per visible column\n";
}

}  // namespace

int main() {
    static_assert(sizeof(ms::ui::CurvePreviewGeometry) <= 2048U);
    testWidthDerivedDensity();
    testNormalizedMappingAndGuides();
    testGeometryAndDiscontinuity();
    testRejectedSamplingClearsGeometry();
    testRollingGeometryTouchesOnlyExposedColumns();
    testMarkerRectanglesStayClipped();
    testClipDerivedSampleRange();
    testKeyValueSparklinePixelContract();
    std::cout << "All CurvePreviewGeometry tests passed (size="
              << sizeof(ms::ui::CurvePreviewGeometry) << " B)\n";
    return 0;
}
