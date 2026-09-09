#include "CoreFonts.hpp"

#include <config/PlatformCompat.hpp>

// Some applications may embed ms-ui while also linking another library that
// already provides the CoreFonts global symbols (fonts registry + entries).
// Define MS_UI_EXTERNAL_CORE_FONTS in those builds to avoid ODR violations.
#ifndef MS_UI_EXTERNAL_CORE_FONTS

// Font binary data (stored in flash via PROGMEM)
#include "data/interdisplay_medium_12.c.inc"
#include "data/interdisplay_bold_13.c.inc"
#include "data/interdisplay_medium_13.c.inc"
#include "data/interdisplay_medium_14.c.inc"
#include "data/interdisplay_regular_14.c.inc"
#include "data/interdisplay_semibold_14.c.inc"

// =============================================================================
// Global Instances
// =============================================================================

CoreFonts fonts;

const oc::ui::lvgl::font::Entry CORE_FONT_ENTRIES[] = {
    // Generic fonts - 12px
    {&fonts.inter_12_medium, interdisplay_medium_12_bin,
     interdisplay_medium_12_bin_len, "Medium12", false},

    // Generic fonts - 13px
    {&fonts.inter_13_medium, interdisplay_medium_13_bin,
     interdisplay_medium_13_bin_len, "Medium13", false},
    {&fonts.inter_13_bold, interdisplay_bold_13_bin,
     interdisplay_bold_13_bin_len, "Bold13", false},

    // Generic fonts - 14px
    {&fonts.inter_14_regular, interdisplay_regular_14_bin,
     interdisplay_regular_14_bin_len, "Regular", false},
    {&fonts.inter_14_medium, interdisplay_medium_14_bin,
     interdisplay_medium_14_bin_len, "Medium", false},
    {&fonts.inter_14_semibold, interdisplay_semibold_14_bin,
     interdisplay_semibold_14_bin_len, "SemiBold", false},
};

const size_t CORE_FONT_COUNT = sizeof(CORE_FONT_ENTRIES) / sizeof(CORE_FONT_ENTRIES[0]);

void linkCoreFontAliases() {
    fonts.parameter_label = fonts.inter_14_regular;
    fonts.parameter_value_label = fonts.inter_14_medium;
    fonts.tempo_label = fonts.inter_14_semibold;
    fonts.list_item_label = fonts.inter_14_semibold;
}

#endif  // MS_UI_EXTERNAL_CORE_FONTS
