#pragma once
#include "FlowUi/Public/configurationStructs/WindowConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/managerStructs/WindowOperationTypes.hpp"
#include <concepts>
#include <string_view>
#include <type_traits>
#include <vulkan/vulkan.h>
namespace FlowUi::detail::platform {
/** Typed native window acquisition and phase-1 Vulkan presentation contract. */
template <class Window, class Runtime>
concept PlatformWindowConcept =
	std::is_nothrow_default_constructible_v<Window> && std::is_nothrow_destructible_v<Window> &&
	requires(Window& window, const Window& observed, Runtime& runtime, const WindowConfig& config,
			 std::string_view text, VkInstance instance, bool flag) {
		{ window.initialize(runtime, config) } -> std::same_as<Status>;
		{ window.shutdown() } noexcept -> std::same_as<void>;
		{ observed.ready() } noexcept -> std::same_as<bool>;
		{ observed.validate() } noexcept -> std::same_as<Status>;
		{ observed.owner() } noexcept -> std::same_as<const Runtime*>;
		{ observed.shouldClose() } noexcept -> std::same_as<bool>;
		{ window.setShouldClose(flag) } noexcept -> std::same_as<void>;
		{
			observed.windowOperationCapabilities()
		} noexcept -> std::same_as<WindowOperationCapabilities>;
		{ window.requestWindowClose() } noexcept -> std::same_as<Status>;
		{ window.minimizeWindow() } noexcept -> std::same_as<Status>;
		{ window.toggleWindowMaximized() } noexcept -> std::same_as<Status>;
		{ window.toggleWindowFullscreen() } noexcept -> std::same_as<Status>;
		{ observed.windowExtent() } noexcept -> std::same_as<VkExtent2D>;
		{ observed.framebufferExtent() } noexcept -> std::same_as<VkExtent2D>;
		{ window.setTitle(text) } -> std::same_as<Status>;
		{ window.setVisible(flag) } noexcept -> std::same_as<Status>;
		{ window.requestFocus() } noexcept -> std::same_as<Status>;
		{ observed.isFocused() } noexcept -> std::same_as<bool>;
		{ observed.isVisible() } noexcept -> std::same_as<bool>;
		{ observed.isMinimized() } noexcept -> std::same_as<bool>;
		{ window.createSurface(instance) } -> std::same_as<Result<VkSurfaceKHR>>;
	};
} // namespace FlowUi::detail::platform
