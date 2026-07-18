#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <ms/ui/widget/CurvePreviewGeometry.hpp>

namespace {

struct SampleContext {
    std::size_t calls = 0U;
    std::size_t rejectAt = static_cast<std::size_t>(-1);
    uint16_t previousPosition = 0U;
};

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

}  // namespace

int main() {
    static_assert(sizeof(ms::ui::CurvePreviewGeometry) <= 2048U);
    testWidthDerivedDensity();
    testNormalizedMappingAndGuides();
    testGeometryAndDiscontinuity();
    testRejectedSamplingClearsGeometry();
    testMarkerRectanglesStayClipped();
    std::cout << "All CurvePreviewGeometry tests passed (size="
              << sizeof(ms::ui::CurvePreviewGeometry) << " B)\n";
    return 0;
}
