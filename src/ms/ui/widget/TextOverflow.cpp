#include "TextOverflow.hpp"

#include <algorithm>
#include <cstring>

#include <config/PlatformCompat.hpp>

namespace ms::ui::text {
namespace {

FLASHMEM lv_coord_t textWidth(const char* text, const lv_font_t* font) {
    if (!text || !font) return 0;
    lv_point_t size{};
    lv_text_get_size(
        &size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return static_cast<lv_coord_t>(size.x);
}

FLASHMEM std::size_t previousUtf8Boundary(const char* text, std::size_t end) {
    if (!text || end == 0U) return 0U;
    std::size_t cursor = end - 1U;
    while (cursor > 0U &&
           (static_cast<unsigned char>(text[cursor]) & 0xC0U) == 0x80U) {
        --cursor;
    }
    return cursor;
}

FLASHMEM void copyPrefixWithEllipsis(char* output, std::size_t outputSize,
                                     const char* source, std::size_t prefixBytes) {
    if (!output || outputSize == 0U) return;
    constexpr std::size_t ellipsisBytes = sizeof(ELLIPSIS) - 1U;
    const std::size_t safePrefix = std::min(
        prefixBytes, outputSize > ellipsisBytes ? outputSize - ellipsisBytes - 1U : 0U);
    if (safePrefix > 0U) std::memcpy(output, source, safePrefix);
    std::memcpy(output + safePrefix, ELLIPSIS, ellipsisBytes);
    output[safePrefix + ellipsisBytes] = '\0';
}

}  // namespace

FLASHMEM bool formatEllipsized(char* output, std::size_t outputSize,
                               const char* source, const lv_font_t* font,
                               lv_coord_t maxWidth) {
    if (!output || outputSize == 0U) return false;
    output[0] = '\0';
    source = source ? source : "";

    const std::size_t sourceBytes = std::strlen(source);
    const bool fitsBuffer = sourceBytes < outputSize;
    const bool fitsWidth =
        font == nullptr || maxWidth <= 0 || textWidth(source, font) <= maxWidth;
    if (fitsBuffer && fitsWidth) {
        std::memcpy(output, source, sourceBytes + 1U);
        return false;
    }

    constexpr std::size_t ellipsisBytes = sizeof(ELLIPSIS) - 1U;
    if (outputSize <= ellipsisBytes) {
        output[0] = '\0';
        return sourceBytes > 0U;
    }

    std::size_t prefixBytes = std::min(sourceBytes, outputSize - ellipsisBytes - 1U);
    while (prefixBytes > 0U && prefixBytes < sourceBytes &&
           (static_cast<unsigned char>(source[prefixBytes]) & 0xC0U) == 0x80U) {
        --prefixBytes;
    }
    copyPrefixWithEllipsis(output, outputSize, source, prefixBytes);
    while (prefixBytes > 0U && font && maxWidth > 0 &&
           textWidth(output, font) > maxWidth) {
        prefixBytes = previousUtf8Boundary(source, prefixBytes);
        copyPrefixWithEllipsis(output, outputSize, source, prefixBytes);
    }
    return true;
}

}  // namespace ms::ui::text
