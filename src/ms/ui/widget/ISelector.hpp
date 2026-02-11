#pragma once

/**
 * @file ISelector.hpp
 * @brief Interface for modal list selector overlays
 *
 * Defines the contract for selectors that display a scrollable list
 * of items with keyboard/encoder navigation. Used for:
 * - Page selection (RemoteControlsPageSelector)
 * - Device selection (DeviceSelector)
 * - Track selection (TrackSelector)
 *
 * @see BaseSelector for the default implementation
 * @see ListOverlay for the underlying UI component
 */

#include <string>

#include <oc/ui/lvgl/IComponent.hpp>

namespace ms::ui {

/**
 * @brief Interface for list selector components
 *
 * Provides navigation, selection and visibility control.
 */
class ISelector : public oc::ui::lvgl::IComponent {
public:
    ~ISelector() override = default;

    virtual void setTitle(const std::string& title) = 0;
    virtual void setSelectedIndex(int index) = 0;
    virtual int getSelectedIndex() const = 0;
    virtual int getItemCount() const = 0;
};

}  // namespace ms::ui
