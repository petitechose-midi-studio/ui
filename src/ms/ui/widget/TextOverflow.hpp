#pragma once

#include <cstddef>

#include <lvgl.h>

namespace ms::ui::text {

/** UTF-8 typographic ellipsis used by every compact text surface. */
inline constexpr const char ELLIPSIS[] = "\xE2\x80\xA6";

/**
 * Copy text into a fixed buffer and append one typographic ellipsis when it
 * exceeds the supplied pixel width. No allocation is performed and UTF-8
 * code points are never split.
 *
 * @return true when the source had to be shortened.
 */
bool formatEllipsized(char* output, std::size_t outputSize, const char* source,
                      const lv_font_t* font, lv_coord_t maxWidth);

}  // namespace ms::ui::text
