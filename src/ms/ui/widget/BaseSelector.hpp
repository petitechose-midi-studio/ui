#pragma once

/**
 * @file BaseSelector.hpp
 * @brief Base implementation for modal list selector overlays
 *
 * Provides common functionality for all selector types:
 * - ListOverlay management (show/hide, title, selection)
 * - ISelector interface implementation
 * - Protected access to overlay for subclass customization
 *
 * Subclasses (RemoteControlsPageSelector) add domain-specific
 * item rendering and data binding.
 *
 * @see ISelector for the interface contract
 * @see ListOverlay for the underlying UI component
 */

#include "ISelector.hpp"
#include "ListOverlay.hpp"

namespace ms::ui {

/**
 * @brief Base implementation for list selectors
 *
 * Handles ListOverlay, navigation, and optional footer.
 * Subclasses implement item-specific rendering.
 */
class BaseSelector : public ISelector {
public:
    explicit BaseSelector(lv_obj_t* parent);
    ~BaseSelector() override;

    // Non-copyable, non-movable
    BaseSelector(const BaseSelector&) = delete;
    BaseSelector& operator=(const BaseSelector&) = delete;

    void setTitle(const std::string& title) override;
    void setSelectedIndex(int index) override;
    int getSelectedIndex() const override;
    int getItemCount() const override;

    void show() override;
    void hide() override;
    bool isVisible() const override;
    lv_obj_t* getElement() const override;

protected:
    ListOverlay& overlay() { return overlay_; }
    const ListOverlay& overlay() const { return overlay_; }

    lv_obj_t* parent_ = nullptr;
    ListOverlay overlay_;
};

}  // namespace ms::ui
