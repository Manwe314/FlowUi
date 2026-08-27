#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "FlowUi/App.hpp"
#include "managers/FlowUiElementBuilder.hpp"
#include "managers/UiManager.hpp"

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

template <FlowElement Element>
Result<WindowId> App::createWindow(
	const WindowConfigOverrides& overrides,
	const Element& element,
	ParametersOf<Element> params,
	LocalElementName localName) {
	return createWindow(
		overrides,
		[element, params = std::move(params), localName](UiManager& ui, WindowId) mutable {
			detail::drawWindowRoot(ui, [&] {
				ui.createElement(element, localName).setParameters(params).draw();
			});
		});
}

template <FlowElement Element>
Result<WindowId> App::createWindow(
	const WindowConfigOverrides& overrides,
	const Element& element,
	std::function<void(ElementBuilder<Element>&, WindowId)> configurator,
	LocalElementName localName) {
	if (!configurator) {
		return unexpectedError(makeError(
			ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	return createWindow(
		overrides,
		[element, configurator = std::move(configurator), localName](UiManager& ui, WindowId id) {
			detail::drawWindowRoot(ui, [&] {
				auto builder = ui.createElement(element, localName);
				configurator(builder, id);
				builder.draw();
			});
		});
}

template <FlowElement Element>
Result<WindowId> App::createWindowWithState(
	const WindowConfigOverrides& overrides,
	const Element& element,
	std::shared_ptr<ParametersOf<Element>> sharedParams,
	LocalElementName localName) {
	if (!sharedParams) {
		return unexpectedError(makeError(
			ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	return createWindow(
		overrides,
		[element, sharedParams = std::move(sharedParams), localName](UiManager& ui, WindowId) {
			detail::drawWindowRoot(ui, [&] {
				ui.createElement(element, localName).setParameters(*sharedParams).draw();
			});
		});
}

} // namespace FlowUi
