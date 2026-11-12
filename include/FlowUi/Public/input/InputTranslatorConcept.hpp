#pragma once

#include "FlowUi/Public/input/InputSink.hpp"
#include "FlowUi/Public/input/InputTranslatorContext.hpp"
#include <concepts>
#include <type_traits>

namespace FlowUi {

/** Polling device integration. initialize rolls back on failure; shutdown stops all producers.
 * resetTarget releases held/captured SDK state for a removed or suspended route.
 */
template <class Translator>
concept InputTranslatorConcept =
	std::is_nothrow_default_constructible_v<Translator> &&
	std::is_nothrow_destructible_v<Translator> &&
	requires(Translator& translator, const InputTranslatorContext& context, InputSink& sink,
			 WindowId window) {
		{ translator.initialize(context) } -> std::same_as<Status>;
		{ translator.poll(context, sink) } -> std::same_as<Status>;
		{ translator.resetTarget(window, sink) } noexcept -> std::same_as<void>;
		{ translator.shutdown() } noexcept -> std::same_as<void>;
	};

} // namespace FlowUi
