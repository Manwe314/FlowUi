/** @file
 * @brief Template definitions for App's managed single-element windows; public contracts are declared in App.hpp.
 */
#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "FlowUi/Private/windowService/ElementWindowRoot.hpp"
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace FlowUi {

template <FlowElement Element>
Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides, const Element& element,
								   ParametersOf<Element> params, LocalElementName localName) {
	return createWindow(overrides, [element, params = std::move(params),
									localName](UiManager& ui, WindowId) mutable {
		detail::drawWindowRoot(
			ui, [&] { ui.createElement(element, localName).setParameters(params).draw(); });
	});
}

template <FlowElement Element>
Result<WindowId>
App::createWindow(const WindowConfigOverrides& overrides, const Element& element,
				  std::function<void(ElementBuilder<Element>&, WindowId)> configurator,
				  LocalElementName localName) {
	if (!configurator) {
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	return createWindow(overrides, [element, configurator = std::move(configurator),
									localName](UiManager& ui, WindowId id) {
		detail::drawWindowRoot(ui, [&] {
			auto builder = ui.createElement(element, localName);
			configurator(builder, id);
			builder.draw();
		});
	});
}

template <FlowElement Element>
Result<WindowId> App::createWindowWithState(const WindowConfigOverrides& overrides,
											const Element& element,
											std::shared_ptr<ParametersOf<Element>> sharedParams,
											LocalElementName localName) {
	if (!sharedParams) {
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	return createWindow(overrides, [element, sharedParams = std::move(sharedParams),
									localName](UiManager& ui, WindowId) {
		detail::drawWindowRoot(
			ui, [&] { ui.createElement(element, localName).setParameters(*sharedParams).draw(); });
	});
}

} // namespace FlowUi
