#pragma once

#include "FlowUi/Public/elementAuthoring/ElementStatePolicy.hpp"
#include "FlowUi/Public/elementAuthoring/ElementTypes.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "clay.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

/* Internal detection and validation for the supported element authoring contracts. */
namespace FlowUi::detail::element {

template <typename Element>
using UnqualifiedElement = std::remove_cvref_t<Element>;

template <typename Element>
concept DeclaresParameters = requires {
	typename UnqualifiedElement<Element>::Parameters;
};

template <typename Element>
concept DeclaresState = requires {
	typename UnqualifiedElement<Element>::State;
};

template <typename Element>
concept DeclaresResources = requires {
	typename UnqualifiedElement<Element>::Resources;
};

template <typename Element>
concept DeclaresParts = requires {
	typename UnqualifiedElement<Element>::Parts;
};

template <typename Element>
[[nodiscard]] consteval bool hasValidCapabilityAliases() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (DeclaresParameters<E>) {
		if constexpr (
			std::is_void_v<typename E::Parameters> ||
			!std::is_object_v<typename E::Parameters>) {
			return false;
		}
	}
	if constexpr (DeclaresState<E>) {
		if constexpr (
			std::is_void_v<typename E::State> ||
			!std::is_object_v<typename E::State>) {
			return false;
		}
	}
	if constexpr (DeclaresResources<E>) {
		if constexpr (
			std::is_void_v<typename E::Resources> ||
			!std::is_object_v<typename E::Resources>) {
			return false;
		}
	}
	if constexpr (DeclaresParts<E>) {
		if constexpr (
			std::is_void_v<typename E::Parts> ||
			!std::is_class_v<typename E::Parts>) {
			return false;
		}
	}
	return true;
}

template <typename Element>
concept ValidCapabilityAliases = hasValidCapabilityAliases<Element>();

template <typename Element>
[[nodiscard]] consteval auto parametersTypeIdentity() noexcept;

template <typename T>
concept CompleteType = requires { sizeof(T); };

template <typename Element>
[[nodiscard]] consteval bool hasValidCapabilityPayloads() noexcept {
	using E = UnqualifiedElement<Element>;
	using Parameters = typename decltype(parametersTypeIdentity<E>())::type;
	if constexpr (!CompleteType<Parameters>) {
		return false;
	} else if constexpr (!std::is_default_constructible_v<Parameters>) {
		return false;
	}

	if constexpr (DeclaresState<E>) {
		using State = typename E::State;
		if constexpr (!CompleteType<State>) {
			return false;
		} else if constexpr (
			!std::is_default_constructible_v<State> ||
			!std::is_nothrow_destructible_v<State>) {
			return false;
		}
	}

	if constexpr (DeclaresResources<E>) {
		using Resources = typename E::Resources;
		if constexpr (!CompleteType<Resources>) {
			return false;
		} else if constexpr (
			(!std::is_constructible_v<Resources, App&> &&
				!std::is_default_constructible_v<Resources>) ||
			!std::is_nothrow_destructible_v<Resources>) {
			return false;
		}
	}

	if constexpr (DeclaresParts<E>) {
		using Parts = typename E::Parts;
		if constexpr (!CompleteType<Parts> || !std::is_class_v<Parts>) {
			return false;
		}
	}
	return true;
}

template <typename Element>
concept ValidCapabilityPayloads = hasValidCapabilityPayloads<Element>();

template <typename Element>
[[nodiscard]] consteval auto parametersTypeIdentity() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (DeclaresParameters<E>) {
		return std::type_identity<typename E::Parameters>{};
	} else {
		return std::type_identity<NoElementParameters>{};
	}
}

template <typename Element>
[[nodiscard]] consteval auto stateTypeIdentity() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (DeclaresState<E>) {
		static_assert(!std::is_void_v<typename E::State>,
			"FlowUi: omit the State alias for a stateless element; do not alias it to void.");
		return std::type_identity<typename E::State>{};
	} else {
		static_assert(DeclaresState<E>,
			"FlowUi: StateOf<Element> requires an element that declares State.");
	}
}

template <typename Element>
[[nodiscard]] consteval auto resourcesTypeIdentity() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (DeclaresResources<E>) {
		static_assert(!std::is_void_v<typename E::Resources>,
			"FlowUi: omit the Resources alias for a resource-free element; do not alias it to void.");
		return std::type_identity<typename E::Resources>{};
	} else {
		static_assert(DeclaresResources<E>,
			"FlowUi: ResourcesOf<Element> requires an element that declares Resources.");
	}
}

template <typename Element>
concept HasBuildElementHook = requires(ElementBuildContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::buildElement(context) } -> std::same_as<void>;
};

template <typename Element>
concept HasConstructElementHook = requires(ElementBuildContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::constructElement(context) } ->
		std::same_as<Clay_ElementDeclaration>;
};

template <typename Element>
concept HasOnHoveredHook = requires(ElementInteractionContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::onHovered(context) } -> std::same_as<void>;
};

template <typename Element>
concept HasOnPressedHook = requires(ElementInteractionContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::onPressed(context) } -> std::same_as<void>;
};

template <typename Element>
concept HasOnHeldHook = requires(ElementInteractionContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::onHeld(context) } -> std::same_as<void>;
};

template <typename Element>
concept HasOnReleasedHook = requires(ElementInteractionContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::onReleased(context) } -> std::same_as<void>;
};

template <typename Element>
concept HasRunLogicHook = requires(ElementInteractionContext<UnqualifiedElement<Element>>& context) {
	{ UnqualifiedElement<Element>::runLogic(context) } -> std::same_as<void>;
};

template <typename Element>
concept NamesOnHoveredHook = requires { &UnqualifiedElement<Element>::onHovered; };

template <typename Element>
concept NamesOnPressedHook = requires { &UnqualifiedElement<Element>::onPressed; };

template <typename Element>
concept NamesOnHeldHook = requires { &UnqualifiedElement<Element>::onHeld; };

template <typename Element>
concept NamesOnReleasedHook = requires { &UnqualifiedElement<Element>::onReleased; };

template <typename Element>
concept NamesRunLogicHook = requires { &UnqualifiedElement<Element>::runLogic; };

template <typename Element>
concept NamesBuildElementHook = requires { &UnqualifiedElement<Element>::buildElement; };

template <typename Element>
concept NamesConstructElementHook = requires { &UnqualifiedElement<Element>::constructElement; };

template <typename Element>
concept ValidNamedHooks =
	(!NamesOnHoveredHook<Element> || HasOnHoveredHook<Element>) &&
	(!NamesOnPressedHook<Element> || HasOnPressedHook<Element>) &&
	(!NamesOnHeldHook<Element> || HasOnHeldHook<Element>) &&
	(!NamesOnReleasedHook<Element> || HasOnReleasedHook<Element>) &&
	(!NamesRunLogicHook<Element> || HasRunLogicHook<Element>) &&
	(!NamesBuildElementHook<Element> || HasBuildElementHook<Element>) &&
	(!NamesConstructElementHook<Element> || HasConstructElementHook<Element>);

template <typename Element>
concept ValidOptionalMetadata =
	(!requires { UnqualifiedElement<Element>::statePolicy; } || requires {
		{ UnqualifiedElement<Element>::statePolicy } ->
			std::convertible_to<ElementStatePolicy>;
		typename std::integral_constant<
			ElementStateRetention,
			static_cast<ElementStatePolicy>(
				UnqualifiedElement<Element>::statePolicy).retention>;
		typename std::integral_constant<
			uint32_t,
			static_cast<ElementStatePolicy>(
				UnqualifiedElement<Element>::statePolicy).graceFrames>;
	}) &&
	(!requires { UnqualifiedElement<Element>::isDevInternal; } || requires {
		{ UnqualifiedElement<Element>::isDevInternal } -> std::convertible_to<bool>;
		typename std::bool_constant<static_cast<bool>(
			UnqualifiedElement<Element>::isDevInternal)>;
	}) &&
	(!requires { UnqualifiedElement<Element>::debugName; } || requires {
		{ UnqualifiedElement<Element>::debugName } -> std::convertible_to<std::string_view>;
		typename std::integral_constant<
			std::size_t,
			std::string_view(UnqualifiedElement<Element>::debugName).size()>;
	});

template <typename Element>
concept HasValidDefinitionId = requires {
	requires std::same_as<
		std::remove_cv_t<decltype(UnqualifiedElement<Element>::definitionId)>,
		FlowDefinitionID>;
	requires (UnqualifiedElement<Element>::definitionId.value != 0);
};

template <typename Element>
concept EmptyElementTag =
	std::is_empty_v<UnqualifiedElement<Element>> &&
	std::is_trivially_default_constructible_v<UnqualifiedElement<Element>> &&
	std::is_trivially_copyable_v<UnqualifiedElement<Element>>;

template <typename Element>
concept FinalFlowElementContract =
	HasValidDefinitionId<Element> &&
	ValidCapabilityAliases<Element> &&
	ValidCapabilityPayloads<Element> &&
	ValidOptionalMetadata<Element> &&
	ValidNamedHooks<Element> &&
	EmptyElementTag<Element> &&
	(HasBuildElementHook<Element> || HasConstructElementHook<Element>);

template <typename Element>
[[nodiscard]] consteval ElementStatePolicy statePolicy() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (requires {
		{ E::statePolicy } -> std::convertible_to<ElementStatePolicy>;
	}) {
		return static_cast<ElementStatePolicy>(E::statePolicy);
	} else {
		return ElementStatePolicy::transient();
	}
}

template <typename Element>
[[nodiscard]] consteval bool isDevInternal() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (requires {
		{ E::isDevInternal } -> std::convertible_to<bool>;
	}) {
		return static_cast<bool>(E::isDevInternal);
	} else {
		return false;
	}
}

template <typename Element>
[[nodiscard]] consteval std::string_view debugName() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (requires {
		{ E::debugName } -> std::convertible_to<std::string_view>;
	}) {
		return std::string_view(E::debugName);
	} else {
		return {};
	}
}


template <typename Element>
[[nodiscard]] consteval bool hasStateCapability() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (DeclaresState<E>) {
		return true;
	} else {
		return false;
	}
}

template <typename Element>
[[nodiscard]] consteval bool hasResourcesCapability() noexcept {
	using E = UnqualifiedElement<Element>;
	if constexpr (DeclaresResources<E>) {
		return true;
	} else {
		return false;
	}
}

template <typename Element>
[[nodiscard]] consteval bool hasPartsCapability() noexcept {
	using E = UnqualifiedElement<Element>;
	return DeclaresParts<E>;
}

} // namespace FlowUi::detail::element
