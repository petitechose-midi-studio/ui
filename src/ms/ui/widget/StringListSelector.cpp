#include "StringListSelector.hpp"

#include <algorithm>

namespace ms::ui {

StringListSelector::StringListSelector(lv_obj_t* parent)
    : BaseSelector(parent) {}

void StringListSelector::invalidateItems() {
    items_ref_ = nullptr;
    items_size_ = 0;
}

void StringListSelector::render(const StringListSelectorProps& props) {
    if (!props.visible) {
        hide();
        return;
    }

    if (!isVisible()) {
        show();
    }

    const int selected = props.selectedIndex;

    if (!props.items || props.items->empty()) {
        overlay().setItems({});
        overlay().setSelectedIndex(0);
        items_ref_ = nullptr;
        items_size_ = 0;
        return;
    }

    // Only rebuild list when items change.
    if (props.items != items_ref_ || props.items->size() != items_size_) {
        overlay().setItems(*props.items);
        items_ref_ = props.items;
        items_size_ = props.items->size();
    }

    const int maxIndex = std::max(0, static_cast<int>(items_size_) - 1);
    overlay().setSelectedIndex(std::clamp(selected, 0, maxIndex));
}

}  // namespace ms::ui
