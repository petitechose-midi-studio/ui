#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <ms/ui/component/VirtualListOverlay.hpp>
#include <ms/ui/font/CoreFonts.hpp>
#include <ms/ui/widget/VirtualListSelectorOverlay.hpp>
#include <ms/ui/widget/VirtualListKeyValueOverlay.hpp>
#include <ms/ui/widget/MenuListView.hpp>
#include <ms/ui/widget/TextOverflow.hpp>

// Exercise the real component with LVGL's built-in font, without loading assets.
CoreFonts fonts;

static uint64_t pixelHash(const std::array<uint16_t, 320 * 240>& pixels) {
    uint64_t hash = 14695981039346656037ULL;
    for (auto pixel : pixels) {
        hash = (hash ^ (pixel & 255U)) * 1099511628211ULL;
        hash = (hash ^ (pixel >> 8U)) * 1099511628211ULL;
    }
    return hash;
}

static lv_obj_t* findVisibleText(lv_obj_t* root, const char* text) {
    if (!lv_obj_is_visible(root)) return nullptr;
    if (lv_obj_check_type(root, &lv_label_class) &&
        std::strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (auto* label = findVisibleText(lv_obj_get_child(root, i), text)) return label;
    }
    return nullptr;
}

static unsigned countObjects(lv_obj_t* root) {
    unsigned count = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        count += countObjects(lv_obj_get_child(root, i));
    }
    return count;
}

static void checkOptionalSelectorValues(lv_obj_t* parent, lv_display_t* display,
                                      const std::array<uint16_t, 320 * 240>& pixels) {
    ms::ui::VirtualListSelectorOverlay selector(parent);
    const char* names[] = {"Zero", "One", "Two", "Three", "Four", "Five"};
    const char* values[] = {"10", "20", "30", "40", "50", "60"};
    ms::ui::VirtualListSelectorOverlayProps props{
        .items = names, .itemCount = 6, .selectedIndex = 2, .visible = true};
    selector.render(props);
    lv_refr_now(display);
    const unsigned withoutValues = countObjects(selector.getElement());
    for (bool showValues : {true, false, true}) {
        props.values = showValues ? values : nullptr;
        selector.render(props);
        lv_refr_now(display);
        // Optional columns are created once, on demand, and retained for reuse.
        assert(countObjects(selector.getElement()) == withoutValues + 5);
        auto* value = findVisibleText(selector.getElement(), "30");
        assert((value != nullptr) == showValues);
        if (value) {
            assert(lv_color_eq(lv_obj_get_style_text_color(value, LV_PART_MAIN),
                lv_color_hex(ms::ui::DEFAULT_LIST_VISUAL_TOKENS.secondaryTextColor)));
            assert(lv_obj_get_style_text_opa(value, LV_PART_MAIN) == LV_OPA_COVER);
        }
        const auto incremental = pixels;
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(display);
        assert(incremental == pixels);
    }
    std::puts("selector optional values: five lazy labels, retained style and pixels");
}

static void checkRetainedText() {
    // Equivalence with the former temporary-buffer algorithm, including
    // clipped prefixes, shortened values, null, and repeated assignments.
    for (size_t capacity = 1; capacity <= 48; ++capacity) {
        std::array<char, 49> actual{}, expected{};
        actual[capacity] = expected[capacity] = '!';
        for (size_t length = 0; length <= 96; ++length) {
            std::array<char, 97> source{};
            source.fill('A');
            source[length] = '\0';
            for (const char* input : {source.data(), source.data(), static_cast<char*>(nullptr)}) {
                std::array<char, 49> next{};
                std::strncpy(next.data(), input ? input : "", capacity - 1);
                const bool changed = std::strncmp(expected.data(), next.data(), capacity) != 0;
                std::memcpy(expected.data(), next.data(), capacity);
                assert(ms::ui::text::copyTruncatedIfChanged(actual.data(), capacity, input) == changed);
                assert(actual == expected);
            }
        }
    }
    assert(!ms::ui::text::copyTruncatedIfChanged(nullptr, 0, "ignored"));
    std::array<char, 0> empty{};
    assert(!ms::ui::text::copyTruncatedIfChanged(empty, "ignored"));
    std::array<char, 8> retained{}, pointerCache{};
    for (const char* input : {"Long clipped text", "Long clipped suffix", "Short", "", "Again"}) {
        assert(ms::ui::text::copyTruncatedIfChanged(retained, input) ==
            ms::ui::text::copyTruncatedIfChanged(pointerCache.data(), pointerCache.size(), input));
        assert(retained == pointerCache);
        // Feedback may pass its own retained text back to the presenter.
        assert(!ms::ui::text::copyTruncatedIfChanged(retained, retained.data()));
    }
    std::puts("retained text: 13968 bounded assignments match the reference");
}

static void checkKeyValueTransitions(lv_obj_t* parent, lv_display_t* display) {
    ms::ui::VirtualListKeyValueOverlay overlay(parent);
    std::array<ms::ui::KeyValueRow, 16> rows{};
    for (auto& row : rows) row = {.key = "Fixed", .value = "42"};
    const auto provider = [](void* context, int index, ms::ui::KeyValueRowBuffer& out) {
        ++*static_cast<unsigned*>(context);
        std::snprintf(out.key.data(), out.key.size(), "Source %d", index);
    };
    for (int virtualCount : {0, 1, 5, 16, 17, 4096}) {
        for (int fixedCount : {0, 1, 16}) {
            unsigned calls = 0;
            overlay.render({.rowProvider = provider, .rowProviderContext = &calls,
                .rowCount = virtualCount, .selectedIndex = virtualCount - 1,
                .visible = true, .dataRevision = 1});
            assert(calls <= 15); // Only visible rows, even for thousands of sources.
            // The same retained overlay serves picker and small detail panels.
            overlay.render({.rows = rows.data(), .rowCount = fixedCount,
                .visible = true, .dataRevision = 1});
            lv_refr_now(display);
            assert((findVisibleText(overlay.getElement(), "Fixed") != nullptr) == (fixedCount > 0));
            // Regrowing a cached row must not reuse stale text, even at the same revision.
            rows[15].value = "Updated";
            overlay.render({.rows = rows.data(), .rowCount = 16, .selectedIndex = 15,
                .visible = true, .dataRevision = 2});
            lv_refr_now(display);
            assert(findVisibleText(overlay.getElement(), "Updated"));
            rows[15].value = "42";
            overlay.render({.visible = false});
        }
    }
    std::puts("key-value provider/fixed transitions: 18 passed");
}

static void checkMenuValuePresentation(lv_obj_t* parent, lv_display_t* display, bool reference) {
    unsigned glyphQueries = 0;
    lv_font_t countedFont = *LV_FONT_DEFAULT;
    countedFont.user_data = &glyphQueries;
    countedFont.get_glyph_dsc = [](const lv_font_t* font, lv_font_glyph_dsc_t* out,
                                  uint32_t letter, uint32_t next) {
        ++*static_cast<unsigned*>(font->user_data);
        return (LV_FONT_DEFAULT)->get_glyph_dsc(LV_FONT_DEFAULT, out, letter, next);
    };
    fonts.inter_14_semibold = &countedFont;
    {
        ms::ui::MenuListView menu(parent);
        ms::ui::MenuRow row{.label = "Depth", .value = "42", .tone = ms::ui::MenuRowTone::Warning};
        menu.render({.rows = &row, .rowCount = 1});
        lv_refr_now(display);
        glyphQueries = 0;
        char expectedText[48]{};
        ms::ui::text::formatEllipsized(expectedText, sizeof(expectedText), "42", &countedFont, 106);
        const unsigned onePresentation = glyphQueries;
        glyphQueries = 0;
        row.label = "Amount"; // Same value: count formatting, not changed label rasterization.
        menu.render({.rows = &row, .rowCount = 1});
        std::printf("menu unchanged value glyph queries=%u\n", glyphQueries);
        if (!reference) assert(glyphQueries == onePresentation);
        lv_refr_now(display);
        auto* value = findVisibleText(menu.getElement(), "42");
        assert(value);
        const auto color = lv_obj_get_style_text_color(value, LV_PART_MAIN);
        row.valueAutoScroll = true; // Create the scroller after the style is already retained.
        menu.render({.rows = &row, .rowCount = 1});
        lv_refr_now(display);
        value = findVisibleText(menu.getElement(), "42");
        assert(value);
        std::printf("menu late scroller preserves color=%d\n",
            lv_color_eq(lv_obj_get_style_text_color(value, LV_PART_MAIN), color));
        if (!reference) assert(lv_color_eq(lv_obj_get_style_text_color(value, LV_PART_MAIN), color));
        row.valueAutoScroll = false;
        row.value = "64";
        menu.render({.rows = &row, .rowCount = 1});
        lv_refr_now(display);
        assert(findVisibleText(menu.getElement(), "64"));
        std::array<ms::ui::MenuRow, 6> rows{};
        for (auto& item : rows) item = row;
        rows[4] = {.label = "Disabled", .value = "7", .enabled = false};
        rows[5] = {.label = "Last", .value = "8"};
        for (int count : {6, 1, 0, 6}) {
            menu.render({.rows = rows.data(), .rowCount = count, .selectedIndex = count - 1,
                .dataRevision = 1});
            lv_refr_now(display);
            assert((findVisibleText(menu.getElement(), "Last") != nullptr) == (count == 6));
        }
        for (int selected : {3, 4, 3}) {
            menu.render({.rows = rows.data(), .rowCount = 6, .selectedIndex = selected,
                .dataRevision = 1});
            lv_refr_now(display);
            auto* disabled = findVisibleText(menu.getElement(), "7");
            assert(disabled);
            assert(lv_obj_get_style_text_opa(disabled, LV_PART_MAIN) ==
                (selected == 4 ? LV_OPA_COVER : LV_OPA_50));
        }
    }
    fonts.inter_14_semibold = const_cast<lv_font_t*>(LV_FONT_DEFAULT);
}

static void checkSparklineDamage(lv_obj_t* parent, lv_display_t* display,
                                const std::array<uint16_t, 320 * 240>& pixels) {
    for (const uint16_t width : {0U, 1U, 2U, 58U, 110U, 320U, 65535U}) {
        for (uint32_t column = 0; column <= UINT16_MAX; ++column) {
            const uint16_t expected = width < 2U ? 0U : static_cast<uint16_t>(
                (std::min<uint64_t>(column, width - 1U) * 65535ULL) / (width - 1U));
            assert(ms::ui::keyValueSparklinePositionQ16(column, width) == expected);
        }
    }
    ms::ui::VirtualListKeyValueOverlay overlay(parent);
    ms::ui::KeyValueSparklineMarker marker{};
    ms::ui::KeyValueRow row{.key = "Source", .sparkline = {
        .context = &marker, .identity = 1, .geometryRevision = 1,
        .enabled = true, .centerLine = true,
        .sampleProvider = [](const ms::ui::KeyValueSparkline&, uint16_t position,
                             uint16_t previous, bool hasPrevious,
                             ms::ui::KeyValueSparklineSample& out) {
            out.valueQ16 = position < 32768U ? 16000U : 48000U;
            out.discontinuityBefore = hasPrevious && previous < 32768U && position >= 32768U;
            return true;
        },
        .markerProvider = [](const ms::ui::KeyValueSparkline& descriptor, uint32_t,
                             ms::ui::KeyValueSparklineMarker& out) {
            out = *static_cast<const ms::ui::KeyValueSparklineMarker*>(descriptor.context);
            return true;
        },
    }};
    for (bool compact : {false, true}) {
        overlay.render({.rows = &row, .rowCount = 1, .compactFacts = compact,
                        .visible = true, .dataRevision = 1});
        lv_refr_now(display);
        for (const uint16_t position : {0U, 1000U, 1001U, 32000U, 32768U, 65535U}) {
            for (bool visible : {true, false, true}) {
                marker = {.positionQ16 = position, .valueQ16 = position, .visible = visible};
                lv_tick_inc(4);
                lv_timer_handler();
                lv_refr_now(display);
                const auto incremental = pixels;
                lv_obj_invalidate(lv_screen_active());
                lv_refr_now(display);
                assert(incremental == pixels);
                std::printf("sparkline compact=%d position=%u visible=%d rgb565=%016llx\n",
                    compact, position, visible, static_cast<unsigned long long>(pixelHash(pixels)));
            }
        }
    }
}

static void checkHiddenSparklineMarkers(lv_obj_t* parent, lv_display_t* display) {
    ms::ui::VirtualListKeyValueOverlay overlay(parent);
    unsigned calls = 0;
    std::array<ms::ui::KeyValueRow, 5> rows{};
    for (auto& row : rows) row = {.key = "Source", .sparkline = {
        .context = &calls, .identity = 1, .geometryRevision = 1, .enabled = true,
        .sampleProvider = [](const ms::ui::KeyValueSparkline&, uint16_t position,
                             uint16_t, bool, ms::ui::KeyValueSparklineSample& out) {
            out.valueQ16 = position;
            return true;
        },
        .markerProvider = [](const ms::ui::KeyValueSparkline& descriptor, uint32_t,
                             ms::ui::KeyValueSparklineMarker& out) {
            ++*static_cast<unsigned*>(const_cast<void*>(descriptor.context));
            out = {};
            return true;
        },
    }};
    const auto service = [&]() {
        calls = 0;
        lv_tick_inc(4);
        lv_timer_handler();
        return calls;
    };
    overlay.render({.rows = rows.data(), .rowCount = 5, .visible = true, .dataRevision = 1});
    lv_refr_now(display);
    assert(service() == 5);
    overlay.render({.rows = rows.data(), .rowCount = 1, .visible = true, .dataRevision = 2});
    lv_refr_now(display);
    const auto singleRowCalls = service();
    std::printf("sparkline markers after shrinking to one row: %u\n", singleRowCalls);
    std::fflush(stdout);
    assert(singleRowCalls == 1);
    for (auto* hidden : {overlay.getElement(), parent}) {
        lv_obj_add_flag(hidden, LV_OBJ_FLAG_HIDDEN);
        assert(service() == 0);
        lv_obj_remove_flag(hidden, LV_OBJ_FLAG_HIDDEN);
        assert(service() == 1); // No explicit render: the parent's reveal is enough.
    }
}

int main(int argc, char** argv) {
    checkRetainedText();
    const bool requireHiddenBinding = argc < 2 || std::strcmp(argv[1], "--reference") != 0;
    lv_init();
    auto* display = lv_display_create(320, 240);
    std::array<uint16_t, 320 * 240> pixels{};
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels.data(), nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) {
        lv_display_flush_ready(d);
    });
    auto* screen = lv_screen_active();
    auto* parent = lv_obj_create(screen);
    lv_obj_remove_style_all(parent);
    lv_obj_set_size(parent, 320, 210);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x17334b), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(parent);
    {
        ms::ui::VirtualListOverlay overlay(parent);
        auto* list = overlay.list();
        std::array<lv_obj_t*, 5> labels{};
        bool opening = false;
        unsigned openingBinds = 0;
        list->onBindSlot([&](oc::ui::lvgl::widget::VirtualSlot& slot, int index, bool) {
            if (opening) {
                ++openingBinds;
                if (requireHiddenBinding) assert(!lv_obj_is_visible(slot.container));
            }
            auto*& label = labels[static_cast<size_t>(index - list->getWindowStart())];
            if (!label) label = lv_label_create(slot.container);
            lv_label_set_text_fmt(label, "Value %d", index);
        });
        list->prepare();
        list->setTotalCount(8);
        for (int pass = 0; pass < 4; ++pass) {
            overlay.hide();
            // Include resized parents, reopened content, and the registry's
            // early reveal of a retained root before the presenter runs.
            lv_obj_set_size(parent, pass == 2 ? 280 : 320, pass == 2 ? 190 : 210);
            overlay.setTitle(pass == 1 ? "A longer title for a reopened list" : "Modulation");
            overlay.setMeta(pass == 2 ? "2/8" : "1/8");
            overlay.setBreadcrumb(pass == 3 ? "Track / Macro / Source" : "");
            list->setSelectedIndex(pass);
            if (pass != 0) lv_obj_clear_flag(overlay.getElement(), LV_OBJ_FLAG_HIDDEN);
            opening = true;
            const unsigned before = openingBinds;
            overlay.show();
            opening = false;
            assert(openingBinds > before);
            assert(overlay.isVisible());
            lv_refr_now(display);
            assert(lv_obj_is_visible(overlay.getElement()));
            const auto firstFrame = pixels;
            // A settled second frame must not repair missing cursor/layout pixels.
            lv_obj_invalidate(screen);
            lv_refr_now(display);
            assert(firstFrame == pixels);
            std::printf("pass=%d rgb565=%016llx\n", pass, static_cast<unsigned long long>(pixelHash(pixels)));
            const unsigned settled = openingBinds;
            overlay.show();
            assert(openingBinds == settled); // Idempotent show.
        }
    }
    {
        ms::ui::VirtualListSelectorOverlay selector(parent);
        const char* names[] = {"Macros", "Clips", "Modulation", "Project", "Device", "Extra"};
        const char* values[] = {"1", "2", "3", "4", "5", "6"};
        for (int pass = 0; pass < 4; ++pass) {
            selector.render({.visible = false});
            lv_obj_set_size(parent, pass == 2 ? 280 : 320, pass == 2 ? 190 : 210);
            // The presentation registry reveals the retained root first.
            lv_obj_clear_flag(selector.getElement(), LV_OBJ_FLAG_HIDDEN);
            selector.render({.title = "Views", .items = names,
                .values = pass == 2 ? values : nullptr,
                .itemCount = 6, .selectedIndex = pass,
                .showIndexColumn = pass == 3, .visible = true});
            lv_refr_now(display);
            const auto firstFrame = pixels;
            lv_obj_invalidate(screen);
            lv_refr_now(display);
            assert(firstFrame == pixels);
            std::printf("selector=%d rgb565=%016llx\n", pass, static_cast<unsigned long long>(pixelHash(pixels)));
        }
    }
    {
        fonts.inter_12_medium = fonts.inter_14_semibold = fonts.list_item_label =
            const_cast<lv_font_t*>(LV_FONT_DEFAULT);
        ms::ui::MenuListView menu(parent);
        ms::ui::MenuRow rows[] = {{.label = "Name", .value = "A long project name"},
            {.label = "Tempo", .value = "120.0"}, {.label = "Scale", .value = "C minor"}};
        for (int pass = 0; pass < 4; ++pass) {
            menu.hide();
            lv_obj_set_size(parent, pass == 2 ? 280 : 320, pass == 2 ? 190 : 210);
            menu.render({.title = "Project", .meta = "Settings", .rows = rows,
                .rowCount = 3, .selectedIndex = pass % 3,
                .headerLayout = pass % 2 ? ms::ui::MenuListHeaderLayout::Stacked
                                        : ms::ui::MenuListHeaderLayout::Horizontal});
            menu.show();
            lv_refr_now(display);
            const auto firstFrame = pixels;
            lv_obj_invalidate(screen);
            lv_refr_now(display);
            assert(firstFrame == pixels);
            std::printf("menu=%d rgb565=%016llx\n", pass, static_cast<unsigned long long>(pixelHash(pixels)));
        }
    }
    checkKeyValueTransitions(parent, display);
    checkMenuValuePresentation(parent, display, !requireHiddenBinding);
    checkSparklineDamage(parent, display, pixels);
    checkHiddenSparklineMarkers(parent, display);
    checkOptionalSelectorValues(parent, display, pixels);
    {
        ms::ui::VirtualListSelectorOverlay selector(parent);
        const char* names[] = {"Zero", "One", "Two", "Three", "Four", "Five"};
        const char* icons[] = {"A", "B", "C", "D", "E", "F"};
        const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff};
        for (int selected : {2, 3, 2}) {
            selector.render({.items = names, .icons = icons, .iconColors = colors,
                .iconFont = LV_FONT_DEFAULT, .itemCount = 6, .selectedIndex = selected,
                .visible = true, .dataRevision = 1});
            lv_refr_now(display);
            unsigned visible = 0;
            for (size_t i = 0; i < 6; ++i) {
                auto* icon = findVisibleText(selector.getElement(), icons[i]);
                if (!icon) continue;
                ++visible;
                assert(lv_color_eq(lv_obj_get_style_text_color(icon, LV_PART_MAIN), lv_color_hex(colors[i])));
            }
            assert(visible == 5);
        }
        std::puts("selector recycled rows preserve semantic colors");
    }
    lv_display_delete(display);
    lv_deinit();
}
