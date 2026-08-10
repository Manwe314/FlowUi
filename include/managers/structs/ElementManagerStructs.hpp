#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>

#include "FlowUi/App.hpp"
#include "internal/ManagerStorage/ElementRegistration.hpp"
#include "internal/TypeOperations.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"

namespace FlowUi {

namespace detail {

// transitional: temporary normalization accepts the current ElementDefinition aliases until the planned concept-based element type replaces that five-parameter specialization.
template <typename Element>
consteval auto elementParametersTypeIdentity() {
	using E = std::remove_cvref_t<Element>;
	if constexpr (requires { typename E::ParametersType; }) {
		return std::type_identity<typename E::ParametersType>{};
	} else if constexpr (requires { typename E::Parameters; }) {
		if constexpr (std::is_void_v<typename E::Parameters>) {
			return std::type_identity<NoElementParameters>{};
		} else {
			return std::type_identity<typename E::Parameters>{};
		}
	} else {
		return std::type_identity<NoElementParameters>{};
	}
}

// transitional: temporary normalization accepts the current ElementDefinition state flags and aliases until element concepts expose State directly.
template <typename Element>
consteval auto elementStateTypeIdentity() {
	using E = std::remove_cvref_t<Element>;
	if constexpr (requires { E::hasState; typename E::StateType; }) {
		if constexpr (E::hasState) {
			return std::type_identity<typename E::StateType>{};
		} else {
			return std::type_identity<NoElementState>{};
		}
	} else if constexpr (requires { typename E::State; }) {
		if constexpr (std::is_void_v<typename E::State>) {
			return std::type_identity<NoElementState>{};
		} else {
			return std::type_identity<typename E::State>{};
		}
	} else {
		return std::type_identity<NoElementState>{};
	}
}

// transitional: temporary normalization accepts the current ElementDefinition resource flags and aliases until element concepts expose Resources directly.
template <typename Element>
consteval auto elementResourcesTypeIdentity() {
	using E = std::remove_cvref_t<Element>;
	if constexpr (requires { E::hasResources; typename E::ResourcesType; }) {
		if constexpr (E::hasResources) {
			return std::type_identity<typename E::ResourcesType>{};
		} else {
			return std::type_identity<NoElementResources>{};
		}
	} else if constexpr (requires { typename E::Resources; }) {
		if constexpr (std::is_void_v<typename E::Resources>) {
			return std::type_identity<NoElementResources>{};
		} else {
			return std::type_identity<typename E::Resources>{};
		}
	} else {
		return std::type_identity<NoElementResources>{};
	}
}

template <typename Element>
consteval std::string_view elementDebugName() noexcept {
	using E = std::remove_cvref_t<Element>;
	if constexpr (requires { { E::debugName } -> std::convertible_to<std::string_view>; }) {
		return std::string_view(E::debugName);
	} else {
		return detail::typeToken<E>();
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

template <typename Element>
using ParametersOf = typename decltype(detail::elementParametersTypeIdentity<Element>())::type;

template <typename Element>
using StateOf = typename decltype(detail::elementStateTypeIdentity<Element>())::type;

template <typename Element>
using ResourcesOf = typename decltype(detail::elementResourcesTypeIdentity<Element>())::type;

template <typename Element>
inline constexpr bool HasState = !std::same_as<StateOf<Element>, NoElementState>;

template <typename Element>
inline constexpr bool HasResources = !std::same_as<ResourcesOf<Element>, NoElementResources>;

template <typename Element>
concept FlowElement = requires {
	{ std::remove_cvref_t<Element>::definitionId } -> std::convertible_to<FlowDefinitionId>;
};

template <FlowElement Element>
consteval detail::element::ElementRegistrationDescriptor makeElementDescriptor() {
	using E = std::remove_cvref_t<Element>;
	using Parameters = ParametersOf<E>;
	using State = StateOf<E>;
	using Resources = ResourcesOf<E>;

	detail::element::ElementRegistrationDescriptor descriptor{
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
		descriptor.stateTypeHash = detail::typeHash<State>();
		descriptor.stateSize = sizeof(State);
		descriptor.stateAlignment = alignof(State);
		descriptor.stateOperations = detail::makeDefaultTypeOperations<State>();
		descriptor.stateTypeName = detail::typeToken<State>();
	}
	if constexpr (HasResources<E>) {
		descriptor.resourcesTypeHash = detail::typeHash<Resources>();
		descriptor.resourcesSize = sizeof(Resources);
		descriptor.resourcesAlignment = alignof(Resources);
		descriptor.resourceOperations = detail::makeResourceTypeOperations<Resources>();
		descriptor.resourcesTypeName = detail::typeToken<Resources>();
	}
	return descriptor;
}

template <FlowElement Element>
inline constexpr detail::element::ElementRegistrationDescriptor elementDescriptor = makeElementDescriptor<Element>();

} // namespace FlowUi
