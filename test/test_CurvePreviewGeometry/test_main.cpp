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
    assert(curvePreviewSampleCountForWidth(180) == 61U);
    assert(curvePreviewSampleCountForWidth(304) == 64U);
    assert(curvePreviewSampleCountForWidth(1000) == 64U);
    std::cout << "[PASS] width-derived density is bounded at 64 samples\n";
}

void testNormalizedMappingAndGuides() {
    using namespace ms::ui;
    assert(curvePreviewPositionQ16(0U, 64U) == 0U);
    assert(curvePreviewPositionQ16(63U, 64U) == 65535U);
    assert(curvePreviewCoordinate(0U, 8, 304) == 8);
    assert(curvePreviewCoordinate(65535U, 8, 304) == 311);
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
    assert(geometry.sampleCount == 64U);
    assert(context.calls == 64U);
    assert(geometry.curve.front() == 0U);
    assert(geometry.curve[63] == 65535U);
    assert(geometry.base.front() == 16384U);
    assert(geometry.impact.front() == 65535U);
    assert(geometry.impact[63] == 0U);
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
    assert(!curvePreviewMarkerRect(0, 0, 0, 10, 0U, 0U, 2).valid());
    std::cout << "[PASS] marker invalidation rectangles are exact and clipped\n";
}

}  // namespace

int main() {
    static_assert(sizeof(ms::ui::CurvePreviewGeometry) <= 400U);
    testWidthDerivedDensity();
    testNormalizedMappingAndGuides();
    testGeometryAndDiscontinuity();
    testRejectedSamplingClearsGeometry();
    testMarkerRectanglesStayClipped();
    std::cout << "All CurvePreviewGeometry tests passed (size="
              << sizeof(ms::ui::CurvePreviewGeometry) << " B)\n";
    return 0;
}
