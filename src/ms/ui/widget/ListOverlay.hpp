#pragma once

/**
 * @file ListOverlay.hpp
 * @brief Generic modal overlay with scrollable list selection
 *
 * Pure UI component displaying a centered modal with:
 * - Title header
 * - Scrollable list of string items
 * - Visual selection highlighting
 *
 * Stateless and callback-free - data is pushed via setters.
 * Used as the underlying widget for BaseSelector and RemoteControlsPageSelector.
 *
 * @see BaseSelector for the ISelector wrapper
 * @see DeviceSelector for a more complex virtualized variant
 */

#include <memory>
#include <string>
#include <vector>

#include <lvgl.h>

#include <oc/ui/lvgl/IComponent.hpp>
#include <oc/ui/lvgl/theme/BaseTheme.hpp>
#include <oc/ui/lvgl/widget/Label.hpp>

namespace ms::ui {

/**
 * @brief Pure UI widget for modal list overlay with selection
 *
 * Displays a centered modal overlay containing a scrollable list of items.
 * Supports visual selection highlighting via index.
 *
 * PURE UI - No logic, no callbacks, only setters/getters.
 *
 * Usage:
 *   ListOverlay overlay(parent);
 *   overlay.setTitle("Select Page");
 *   overlay.setItems({"Page 1", "Page 2", "Page 3"});
 *   overlay.setSelectedIndex(0);
 *   overlay.show();
 */
class ListOverlay : public oc::ui::lvgl::IComponent {
public:
    explicit ListOverlay(lv_obj_t* parent);
    ~ListOverlay();

    // Non-copyable, non-movable
    ListOverlay(const ListOverlay&) = delete;
    ListOverlay& operator=(const ListOverlay&) = delete;

    void setTitle(const std::string& title);
    void setItems(const std::vector<std::string>& items);
    void setSelectedIndex(int index);

    /**
     * @brief Append new items to the list without destroying existing ones
     *
     * Optimized for windowed loading where items are added incrementally.
     * Only creates new LVGL objects for the appended items.
     *
     * @param items Full list including existing + new items
     * @return Number of new items appended (0 if full rebuild was needed)
     */
    size_t appendItemsIfPossible(const std::vector<std::string>& items);

    void show() override;
    void hide() override;
    bool isVisible() const override;

    int getSelectedIndex() const;
    int getItemCount() const;

    lv_obj_t* getButton(size_t index) const;
    oc::ui::lvgl::Label* getLabel(size_t index) const;
    void setItemFont(size_t index, const lv_font_t* font);
    void removeLabel(size_t index);

    lv_obj_t* getElement() const override { return overlay_; }
    lv_obj_t* getContainer() const { return container_; }

private:
    void createOverlay();
    void createTitleLabel();
    void createList();
    void populateList();

    void updateHighlight();
    void scrollToSelected(bool animate = true);

    void destroyList();
    void cleanup();

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    lv_obj_t* container_ = nullptr;
    std::unique_ptr<oc::ui::lvgl::Label> title_label_;
    lv_obj_t* list_ = nullptr;

    std::vector<lv_obj_t*> buttons_;
    std::vector<std::unique_ptr<oc::ui::lvgl::Label>> labels_;
    std::vector<std::string> items_;
    std::string title_;
    int selected_index_ = 0;
    int previous_index_ = -1;  // Track previous selection for optimized highlight updates
    bool visible_ = false;
    bool ui_created_ = false;
};

}  // namespace ms::ui
