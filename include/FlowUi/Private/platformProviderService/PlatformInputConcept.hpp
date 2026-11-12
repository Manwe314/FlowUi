#pragma once
#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
namespace FlowUi::detail::platform {
/** Typed native input, synchronous clipboard and cursor resource contract. */
template <class Input, class Window>
concept PlatformWindowInputConcept =
	std::is_nothrow_default_constructible_v<Input> && std::is_nothrow_destructible_v<Input> &&
	requires(Input& input, const Input& observed, Window& window, const WindowInputConfig& config,
			 std::string_view text, CursorType cursor) {
		{ input.initialize(window, config) } -> std::same_as<Status>;
		{ input.shutdown() } noexcept -> std::same_as<void>;
		{ observed.ready() } noexcept -> std::same_as<bool>;
		{ observed.window() } noexcept -> std::same_as<const Window*>;
		{ input.refresh() } -> std::same_as<Status>;
		{ input.reset() } noexcept -> std::same_as<void>;
		{ input.setInputConfig(config) } -> std::same_as<Status>;
		{ observed.inputConfig() } noexcept -> std::same_as<WindowInputConfig>;
		{ observed.supportsRawMouseMotion() } noexcept -> std::same_as<bool>;
		{ input.setCursor(cursor) } -> std::same_as<Status>;
		{ input.setClipboardText(text) } -> std::same_as<Status>;
		{ observed.clipboardText() } -> std::same_as<Result<std::string>>;
		{ observed.hasClipboardAccess() } noexcept -> std::same_as<bool>;
	};
} // namespace FlowUi::detail::platform
