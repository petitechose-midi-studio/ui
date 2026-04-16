#include "StringListSelector.hpp"

#include <algorithm>

#include <config/PlatformCompat.hpp>

namespace ms::ui {

StringListSelector::StringListSelector(lv_obj_t* parent)
    : BaseSelector(parent) {}

FLASHMEM void StringListSelector::invalidateItems() {
    items_ref_ = nullptr;
    items_size_ = 0;
}

FLASHMEM void StringListSelector::render(const StringListSelectorProps& props) {
    if (!props.visible) {
        hide();
        return;
    }

    if (!isVisible()) {
        show();
    }

    const int selected = props.selectedIndex;

    if (!props.items || props.itemCount == 0) {
        overlay().setItems(nullptr, 0);
        overlay().setSelectedIndex(0);
        items_ref_ = nullptr;
        items_size_ = 0;
        return;
    }

    // Only rebuild list when items change.
    if (props.items != items_ref_ || props.itemCount != items_size_) {
        overlay().setItems(props.items, props.itemCount);
        items_ref_ = props.items;
        items_size_ = props.itemCount;
    }

    const int maxIndex = std::max(0, static_cast<int>(items_size_) - 1);
    overlay().setSelectedIndex(std::clamp(selected, 0, maxIndex));
}

}  // namespace ms::ui
