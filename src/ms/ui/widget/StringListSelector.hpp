#pragma once

/**
 * @file StringListSelector.hpp
 * @brief Stateless string list selector (render(props))
 */

#include <string>
#include <vector>

#include "BaseSelector.hpp"

namespace ms::ui {

struct StringListSelectorProps {
    const std::vector<std::string>* items = nullptr;
    int selectedIndex = 0;
    bool visible = false;
};

/**
 * @brief Convenience wrapper around BaseSelector/ListOverlay
 *
 * Avoids rebuilding the LVGL list when only the selected index changes.
 */
class StringListSelector : public BaseSelector {
public:
    explicit StringListSelector(lv_obj_t* parent);
    ~StringListSelector() override = default;

    void render(const StringListSelectorProps& props);

    /// Force next render() to rebuild items.
    void invalidateItems();

private:
    const std::vector<std::string>* items_ref_ = nullptr;
    size_t items_size_ = 0;
};

}  // namespace ms::ui
