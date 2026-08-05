# Canonical production source inventory for MIDI Studio UI consumers.
set(MS_UI_SOURCE_PATHS
    src/ms/ui/ViewContainer.cpp
    src/ms/ui/component/LayoutOverlay.cpp
    src/ms/ui/component/LayoutView.cpp
    src/ms/ui/component/VirtualListOverlay.cpp
    src/ms/ui/font/CoreFonts.cpp
    src/ms/ui/widget/BaseSelector.cpp
    src/ms/ui/widget/CurvePreviewWidget.cpp
    src/ms/ui/widget/ListOverlay.cpp
    src/ms/ui/widget/MenuListView.cpp
    src/ms/ui/widget/StringListSelector.cpp
    src/ms/ui/widget/VirtualListKeyValueOverlay.cpp
    src/ms/ui/widget/VirtualListSelectorOverlay.cpp
)

set(MS_UI_SOURCES ${MS_UI_SOURCE_PATHS})
list(TRANSFORM MS_UI_SOURCES
    PREPEND "${CMAKE_CURRENT_LIST_DIR}/../")
