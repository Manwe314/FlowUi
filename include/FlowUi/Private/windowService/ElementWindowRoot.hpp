#pragma once

#include <utility>

#include "FlowUi/Public/Color.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace FlowUi {

namespace detail {

template <typename DrawElement>
void drawWindowRoot(UiManager& ui, DrawElement&& drawElement) {
	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = Flow_Color("#18181b");

	CLAY(ui.toClaySID("window/root-container"), root) {
		std::forward<DrawElement>(drawElement)();
	}
}

} // namespace detail

} // namespace FlowUi
