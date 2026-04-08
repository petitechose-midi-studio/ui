#include "BaseSelector.hpp"

#include <config/PlatformCompat.hpp>

namespace ms::ui {

FLASHMEM BaseSelector::BaseSelector(lv_obj_t* parent) : parent_(parent), overlay_(parent) {}

FLASHMEM BaseSelector::~BaseSelector() = default;

FLASHMEM void BaseSelector::setTitle(const std::string& title) { overlay_.setTitle(title); }

FLASHMEM void BaseSelector::setSelectedIndex(int index) { overlay_.setSelectedIndex(index); }

FLASHMEM int BaseSelector::getSelectedIndex() const { return overlay_.getSelectedIndex(); }

FLASHMEM int BaseSelector::getItemCount() const { return overlay_.getItemCount(); }

FLASHMEM void BaseSelector::show() { overlay_.show(); }

FLASHMEM void BaseSelector::hide() { overlay_.hide(); }

FLASHMEM bool BaseSelector::isVisible() const { return overlay_.isVisible(); }

FLASHMEM lv_obj_t* BaseSelector::getElement() const { return overlay_.getElement(); }

}  // namespace ms::ui
