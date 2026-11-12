#pragma once
#include "FlowUi/Private/platformProviderService/PlatformTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include <concepts>
#include <type_traits>
#include <vector>
namespace FlowUi::detail::platform {
/** Native runtime acquisition, single-pump and owner-thread contract. */
template <class Runtime>
concept PlatformRuntimeConcept =
	std::is_nothrow_default_constructible_v<Runtime> && std::is_nothrow_destructible_v<Runtime> &&
	requires(Runtime& runtime, const Runtime& observed, const PlatformConfig& config) {
		{ runtime.initialize(config) } -> std::same_as<Status>;
		{ runtime.pollEvents() } -> std::same_as<Status>;
		{ runtime.shutdown() } noexcept -> std::same_as<void>;
		{ observed.validate() } noexcept -> std::same_as<Status>;
		{ observed.ready() } noexcept -> std::same_as<bool>;
		{ observed.requiredInstanceExtensions() } -> std::same_as<Result<std::vector<const char*>>>;
	};
} // namespace FlowUi::detail::platform
