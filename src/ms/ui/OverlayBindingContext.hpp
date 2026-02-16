#pragma once

/**
 * @file OverlayBindingContext.hpp
 * @brief Context struct for handlers that manage overlay-based selection UI
 *
 * Groups overlay-related dependencies that always travel together to reduce
 * handler constructor parameter count.
 */

#include <lvgl.h>

#include <oc/context/OverlayManager.hpp>

namespace ms::ui {

/**
 * @brief Context for handlers that manage overlay-based selection UI
 *
 * Groups overlay-related dependencies that always travel together:
 * - OverlayManager for show/hide management
 * - scopeElement for scoped input bindings when handler wants LVGL-scoped authority
 * - overlayElement for positioning/rendering the overlay
 *
 * Usage in handlers:
 * @code
 * class MyOverlayHandler {
 * public:
 *     MyOverlayHandler(OverlayBindingContext<MyOverlayType>& ctx)
 *         : overlayCtx_(ctx) {
 *         ctx.controller.show(MyOverlayType::SELECTOR);
 *         // Use ctx.scopeElement for scoped bindings
 *         // Use ctx.overlayElement for UI positioning
 *     }
 * private:
 *     OverlayBindingContext<MyOverlayType>& overlayCtx_;
 * };
 * @endcode
 */
template <typename OverlayEnumT>
struct OverlayBindingContext {
    oc::context::OverlayManager<OverlayEnumT>& controller;
    lv_obj_t* scopeElement;    ///< Optional LVGL scope element used by handlers that bind with oc::ui::lvgl::scope(...)
    lv_obj_t* overlayElement;  ///< Element for overlay UI positioning
};

}  // namespace ms::ui
