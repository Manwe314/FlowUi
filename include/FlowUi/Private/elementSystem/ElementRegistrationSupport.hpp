#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FlowUi/Private/TypeOperations.hpp"
#include "FlowUi/Private/elementSystem/ElementRegistration.hpp"
#include "FlowUi/Public/elementAuthoring/ElementContexts.hpp"

/* Internal typed construction adapters for registered element definitions. */
namespace FlowUi::detail {

template <typename Element>
[[nodiscard]] consteval std::string_view elementDebugName() noexcept {
	using E = std::remove_cvref_t<Element>;
	if constexpr (element::debugName<E>().empty()) {
		return detail::typeToken<E>();
	} else {
		return element::debugName<E>();
	}
}

template <typename T>
[[nodiscard]] consteval element::ElementTypeOperations makeDefaultTypeOperations() noexcept {
	static_assert(std::is_default_constructible_v<T>,
		"FlowUi element state must be default constructible.");
	static_assert(std::is_nothrow_destructible_v<T>,
		"FlowUi element state/resource destructors must be noexcept.");
	return element::ElementTypeOperations{
		.defaultConstruct = +[](void* destination) { ::new (destination) T(); },
		.constructWithApp = nullptr,
		.destroy = +[](void* object) noexcept { static_cast<T*>(object)->~T(); },
	};
}

template <typename T>
[[nodiscard]] consteval element::ElementTypeOperations makeResourceTypeOperations() noexcept {
	static_assert(
		std::is_constructible_v<T, App&> || std::is_default_constructible_v<T>,
		"FlowUi element resources must be constructible from App& or default constructible.");
	static_assert(std::is_nothrow_destructible_v<T>,
		"FlowUi element state/resource destructors must be noexcept.");
	element::ElementTypeOperations operations{};
	if constexpr (std::is_default_constructible_v<T>) {
		operations.defaultConstruct = +[](void* destination) { ::new (destination) T(); };
	}
	if constexpr (std::is_constructible_v<T, App&>) {
		operations.constructWithApp = +[](void* destination, App& app) {
			::new (destination) T(app);
		};
	}
	operations.destroy = +[](void* object) noexcept { static_cast<T*>(object)->~T(); };
	return operations;
}

} // namespace FlowUi::detail

namespace FlowUi::detail::element {

template <FlowElement Element>
[[nodiscard]] consteval ElementRegistrationDescriptor makeElementDescriptor() noexcept {
	using E = std::remove_cvref_t<Element>;
	using Parameters = ParametersOf<E>;
	// Evaluate before aggregate initialization for GCC 12 immediate-function handling.
	constexpr std::string_view element_debug_name = detail::elementDebugName<E>();

	ElementRegistrationDescriptor descriptor{
		.definitionId = E::definitionId,
		.definitionTypeHash = detail::typeHash<E>(),
		.parametersTypeHash = detail::typeHash<Parameters>(),
		.parametersSize = sizeof(Parameters),
		.parametersAlignment = alignof(Parameters),
		.hasState = HasState<E>,
		.hasResources = HasResources<E>,
		.debugName = element_debug_name,
		.definitionTypeName = detail::typeToken<E>(),
		.parametersTypeName = detail::typeToken<Parameters>(),
	};

	if constexpr (HasState<E>) {
		using State = StateOf<E>;
		descriptor.stateTypeHash = detail::typeHash<State>();
		descriptor.stateSize = sizeof(State);
		descriptor.stateAlignment = alignof(State);
		descriptor.stateOperations = detail::makeDefaultTypeOperations<State>();
		descriptor.stateTypeName = detail::typeToken<State>();
	}
	if constexpr (HasResources<E>) {
		using Resources = ResourcesOf<E>;
		descriptor.resourcesTypeHash = detail::typeHash<Resources>();
		descriptor.resourcesSize = sizeof(Resources);
		descriptor.resourcesAlignment = alignof(Resources);
		descriptor.resourceOperations = detail::makeResourceTypeOperations<Resources>();
		descriptor.resourcesTypeName = detail::typeToken<Resources>();
	}
	return descriptor;
}

template <FlowElement Element>
inline constexpr ElementRegistrationDescriptor elementDescriptor = makeElementDescriptor<Element>();

} // namespace FlowUi::detail::element
