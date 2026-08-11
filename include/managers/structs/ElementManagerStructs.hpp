#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FlowUi/App.hpp"
#include "internal/ManagerStorage/ElementRegistration.hpp"
#include "internal/TypeOperations.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"

namespace FlowUi {

namespace detail {

template <typename Element>
consteval std::string_view elementDebugName() noexcept {
	using E = std::remove_cvref_t<Element>;
	if constexpr (element::debugName<E>().empty()) {
		return detail::typeToken<E>();
	} else {
		return element::debugName<E>();
	}
}

template <typename T>
consteval element::ElementTypeOperations makeDefaultTypeOperations() {
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
consteval element::ElementTypeOperations makeResourceTypeOperations() {
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

} // namespace detail

/** @brief Zero-data compile-time catalog of element definition types. */
template <FlowElement... Elements>
class ElementSet {};

/**
 * @brief Deduce a reusable element-type catalog from definition objects.
 *
 * The arguments participate only in type deduction; ElementSet stores no
 * object addresses or runtime data.
 */
template <typename... Elements>
	requires ((FlowElement<std::remove_cvref_t<Elements>>) && ...)
[[nodiscard]] constexpr auto elementSet(const Elements&...) noexcept {
	return ElementSet<std::remove_cvref_t<Elements>...>{};
}

namespace detail::element {

template <FlowElement Element>
consteval ElementRegistrationDescriptor makeElementDescriptor() {
	using E = std::remove_cvref_t<Element>;
	using Parameters = ParametersOf<E>;

	ElementRegistrationDescriptor descriptor{
		.definitionId = static_cast<FlowDefinitionId>(E::definitionId),
		.definitionTypeHash = detail::typeHash<E>(),
		.parametersTypeHash = detail::typeHash<Parameters>(),
		.parametersSize = sizeof(Parameters),
		.parametersAlignment = alignof(Parameters),
		.hasState = HasState<E>,
		.hasResources = HasResources<E>,
		.debugName = detail::elementDebugName<E>(),
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

} // namespace detail::element

} // namespace FlowUi
