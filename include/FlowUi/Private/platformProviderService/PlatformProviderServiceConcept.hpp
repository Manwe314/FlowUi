#pragma once
#include "FlowUi/Private/platformProviderService/EventBindingConcept.hpp"
#include "FlowUi/Private/platformProviderService/PlatformInputConcept.hpp"
#include "FlowUi/Private/platformProviderService/PlatformRuntimeConcept.hpp"
#include "FlowUi/Private/platformProviderService/PlatformWindowConcept.hpp"
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <vulkan/vulkan.h>

namespace FlowUi::detail::platform {
/** Complete callable coordinator; aliases alone cannot satisfy the phase-1 Vulkan contract. */
template <class Service>
concept PlatformProviderServiceConcept =
	PlatformRuntimeConcept<typename Service::PlatformRuntime> &&
	PlatformWindowConcept<typename Service::PlatformWindow, typename Service::PlatformRuntime> &&
	PlatformWindowInputConcept<typename Service::PlatformWindowInput,
							   typename Service::PlatformWindow> &&
	EventBindingConcept<typename Service::EventBinding, typename Service::PlatformWindow,
						typename Service::PlatformWindowInput> &&
	std::is_nothrow_default_constructible_v<Service> && std::is_nothrow_destructible_v<Service> &&
	std::is_nothrow_default_constructible_v<typename Service::WindowState> &&
	std::is_nothrow_destructible_v<typename Service::WindowState> &&
	requires(Service& service, const Service& observed, typename Service::WindowState& state,
			 const typename Service::WindowState& observedState,
			 const PlatformConfig& platformConfig, const WindowConfig& windowConfig,
			 const WindowInputConfig& inputConfig, WindowId target, InputSink& sink,
			 VkInstance instance, std::string_view text, CursorType cursor, bool flag) {
		requires Service::contractRevision == platformContractRevision;
		{ service.initialize(platformConfig) } -> std::same_as<Status>;
		{ service.pollEvents() } -> std::same_as<Status>;
		{ service.shutdown() } noexcept -> std::same_as<void>;
		{ observed.ready() } noexcept -> std::same_as<bool>;
		{ observed.windowReady(observedState) } noexcept -> std::same_as<bool>;
		{ observed.validateWindowOwner(observedState) } noexcept -> std::same_as<Status>;
		{ service.initializeWindow(state, target, windowConfig, sink) } -> std::same_as<Status>;
		{ service.refreshInput(state) } -> std::same_as<Status>;
		{ service.quiesceWindow(state) } noexcept -> std::same_as<void>;
		{ service.shutdownWindow(state) } noexcept -> std::same_as<void>;
		{ observed.requiredInstanceExtensions() } -> std::same_as<Result<std::vector<const char*>>>;
		{ service.createSurface(state, instance) } -> std::same_as<Result<VkSurfaceKHR>>;
		{ observed.shouldClose(observedState) } noexcept -> std::same_as<bool>;
		{ service.setShouldClose(state, flag) } noexcept -> std::same_as<void>;
		{
			observed.windowOperationCapabilities(observedState)
		} noexcept -> std::same_as<WindowOperationCapabilities>;
		{ service.requestWindowClose(state) } noexcept -> std::same_as<Status>;
		{ service.minimizeWindow(state) } noexcept -> std::same_as<Status>;
		{ service.toggleWindowMaximized(state) } noexcept -> std::same_as<Status>;
		{ service.toggleWindowFullscreen(state) } noexcept -> std::same_as<Status>;
		{ observed.windowExtent(observedState) } noexcept -> std::same_as<VkExtent2D>;
		{ observed.framebufferExtent(observedState) } noexcept -> std::same_as<VkExtent2D>;
		{ service.setTitle(state, text) } -> std::same_as<Status>;
		{ observed.nativeHandle(observedState) } noexcept -> std::same_as<void*>;
		{ service.setInputConfig(state, inputConfig) } -> std::same_as<Status>;
		{ observed.inputConfig(observedState) } noexcept -> std::same_as<WindowInputConfig>;
		{ observed.supportsRawMouseMotion(observedState) } noexcept -> std::same_as<bool>;
		{ service.setClipboardText(state, text) } -> std::same_as<Status>;
		{ observed.clipboardText(observedState) } -> std::same_as<Result<std::string>>;
		{ observed.hasClipboardAccess(observedState) } noexcept -> std::same_as<bool>;
		{ service.setCursor(state, cursor) } -> std::same_as<Status>;
		{ service.setVisible(state, flag) } noexcept -> std::same_as<Status>;
		{ service.requestFocus(state) } noexcept -> std::same_as<Status>;
		{ observed.isFocused(observedState) } noexcept -> std::same_as<bool>;
		{ observed.isVisible(observedState) } noexcept -> std::same_as<bool>;
		{ observed.isMinimized(observedState) } noexcept -> std::same_as<bool>;
	};
} // namespace FlowUi::detail::platform
