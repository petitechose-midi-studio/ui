#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <ms/ui/component/VirtualListOverlay.hpp>
#include <ms/ui/font/CoreFonts.hpp>
#include <ms/ui/widget/VirtualListSelectorOverlay.hpp>
#include <ms/ui/widget/VirtualListKeyValueOverlay.hpp>
#include <ms/ui/widget/MenuListView.hpp>

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

int main(int argc, char** argv) {
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
