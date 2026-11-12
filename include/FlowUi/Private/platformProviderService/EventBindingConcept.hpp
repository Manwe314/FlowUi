#pragma once
#include "FlowUi/Public/input/InputSink.hpp"
#include <concepts>
#include <type_traits>
namespace FlowUi::detail::platform {
/** Exact typed connection between native window/input endpoints and a stable FlowUi sink. */
template <class Binding, class Window, class Input>
concept EventBindingConcept =
	std::is_nothrow_default_constructible_v<Binding> && std::is_nothrow_destructible_v<Binding> &&
	requires(Binding& binding, const Binding& observed, Window& window, Input& input,
			 InputSink& sink, WindowId target) {
		{ binding.attach(window, input, sink, target) } -> std::same_as<Status>;
		{ binding.detach() } noexcept -> std::same_as<void>;
		{ observed.attached() } noexcept -> std::same_as<bool>;
	};
} // namespace FlowUi::detail::platform
