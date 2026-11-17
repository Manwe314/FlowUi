/** @file
 * @brief Template definitions for App's managed single-element windows; public contracts are declared in App.hpp.
 */
#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "FlowUi/Private/windowService/ElementWindowRoot.hpp"
#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace FlowUi {

/**
 * @brief Define a checked managed element window retaining copied parameters.
 * @return A new window identity or the original creation/retention error.
 * @note Runs on the platform thread outside an active frame. Deferred ordinary
 * builder/configurator exceptions are contained by managed frame dispatch;
 * captured references must remain valid until their last callback finishes.
 * @see App::createWindow
 */
template <FlowElement Element>
Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides, const Element& element,
								   const ParametersOf<Element>& params, LocalElementName localName) noexcept try {
	return createWindow(overrides, [element, params,
									localName](UiManager& ui, WindowId) mutable {
		detail::drawWindowRoot(
			ui, [&] {
			auto builder = ui.createElement(element, localName);
			(void)builder.setParameters(params);
			builder.draw();
		});
	});
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

/**
 * @brief Define a checked managed element window with default-constructed parameters.
 * @return A new window identity or the original creation/retention error.
 * @note Runs on the platform thread outside an active frame. Deferred ordinary
 * builder/configurator exceptions are contained by managed frame dispatch;
 * captured references must remain valid until their last callback finishes.
 * @see App::createWindow
 */
template <FlowElement Element>
Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides,
	const Element& element) noexcept try {
	const ParametersOf<Element> parameters{};
	return createWindow(overrides, element, parameters);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

/**
 * @brief Define a checked managed element window with an owned per-frame configurator.
 * @return A new window identity or the original creation/retention error.
 * @note Runs on the platform thread outside an active frame. Deferred ordinary
 * builder/configurator exceptions are contained by managed frame dispatch;
 * captured references must remain valid until their last callback finishes.
 * @see App::createWindow
 */
template <FlowElement Element>
Result<WindowId>
App::createWindow(const WindowConfigOverrides& overrides, const Element& element,
				  const std::function<void(ElementBuilder<Element>&, WindowId)>& configurator,
				  LocalElementName localName) noexcept try {
	if (!configurator) {
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	return createWindow(overrides, [element, configurator,
									localName](UiManager& ui, WindowId id) {
		detail::drawWindowRoot(ui, [&] {
			auto builder = ui.createElement(element, localName);
			configurator(builder, id);
			builder.draw();
		});
	});
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

/**
 * @brief Define a checked managed element window retaining shared parameter ownership.
 * @return A new window identity or the original creation/retention error.
 * @note Runs on the platform thread outside an active frame. Deferred ordinary
 * builder/configurator exceptions are contained by managed frame dispatch;
 * captured references must remain valid until their last callback finishes.
 * @see App::createWindow
 */
template <FlowElement Element>
Result<WindowId> App::createWindowWithState(const WindowConfigOverrides& overrides,
											const Element& element,
											std::shared_ptr<ParametersOf<Element>> sharedParams,
											LocalElementName localName) noexcept try {
	if (!sharedParams) {
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	return createWindow(overrides, [element, sharedParams = std::move(sharedParams),
									localName](UiManager& ui, WindowId) {
		detail::drawWindowRoot(
			ui, [&] {
			auto builder = ui.createElement(element, localName);
			(void)builder.setParameters(*sharedParams);
			builder.draw();
		});
	});
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

} // namespace FlowUi
