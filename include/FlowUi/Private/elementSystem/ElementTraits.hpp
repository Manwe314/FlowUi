#pragma once

#include "FlowUi/Public/elementAuthoring/ElementStatePolicy.hpp"
#include "FlowUi/Public/elementAuthoring/ElementTypes.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "clay.h"
#include <array>
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

template <typename Payload>
concept ObjectCapabilityAlias = !std::is_void_v<Payload> && std::is_object_v<Payload>;

template <typename Payload>
concept PartsCapabilityAlias = !std::is_void_v<Payload> && std::is_class_v<Payload>;

template <typename Element>
[[nodiscard]] consteval bool hasValidCapabilityAliases() noexcept {
	using ElementType = UnqualifiedElement<Element>;
	if constexpr (DeclaresParameters<ElementType>) {
		if constexpr (!ObjectCapabilityAlias<typename ElementType::Parameters>)
			return false;
	}
	if constexpr (DeclaresState<ElementType>) {
		if constexpr (!ObjectCapabilityAlias<typename ElementType::State>)
			return false;
	}
	if constexpr (DeclaresResources<ElementType>) {
		if constexpr (!ObjectCapabilityAlias<typename ElementType::Resources>)
			return false;
	}
	if constexpr (DeclaresParts<ElementType>) {
		if constexpr (!PartsCapabilityAlias<typename ElementType::Parts>)
			return false;
	}
	return true;
}

template <typename Element>
concept ValidCapabilityAliases = hasValidCapabilityAliases<Element>();

template <typename Element>
[[nodiscard]] consteval auto parametersTypeIdentity() noexcept;

template <typename T>
concept CompleteType = requires { sizeof(T); };

template <typename Payload>
concept DefaultConstructiblePayload =
	CompleteType<Payload> && std::is_default_constructible_v<Payload>;

template <typename Payload>
concept NothrowDestructiblePayload =
	CompleteType<Payload> && std::is_nothrow_destructible_v<Payload>;

template <typename Payload>
concept ConstructibleResourcePayload = CompleteType<Payload> &&
	(std::is_constructible_v<Payload, App&> || std::is_default_constructible_v<Payload>);

template <typename Element>
[[nodiscard]] consteval bool hasValidCapabilityPayloads() noexcept {
	using ElementType = UnqualifiedElement<Element>;
	using Parameters = typename decltype(parametersTypeIdentity<ElementType>())::type;
	if constexpr (!DefaultConstructiblePayload<Parameters>)
		return false;

	if constexpr (DeclaresState<ElementType>) {
		using State = typename ElementType::State;
		if constexpr (!DefaultConstructiblePayload<State> || !NothrowDestructiblePayload<State>)
			return false;
	}
	if constexpr (DeclaresResources<ElementType>) {
		using Resources = typename ElementType::Resources;
		if constexpr (!ConstructibleResourcePayload<Resources> ||
					  !NothrowDestructiblePayload<Resources>)
			return false;
	}
	if constexpr (DeclaresParts<ElementType>) {
		using Parts = typename ElementType::Parts;
		if constexpr (!CompleteType<Parts> || !PartsCapabilityAlias<Parts>)
			return false;
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
concept ValidStatePolicyMetadata =
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
	});

template <typename Element>
concept ValidDevInternalMetadata =
	(!requires { UnqualifiedElement<Element>::isDevInternal; } || requires {
		{ UnqualifiedElement<Element>::isDevInternal } -> std::convertible_to<bool>;
		typename std::bool_constant<static_cast<bool>(
			UnqualifiedElement<Element>::isDevInternal)>;
	});

template <typename Element>
concept ValidDebugNameMetadata =
	(!requires { UnqualifiedElement<Element>::debugName; } || requires {
		{ UnqualifiedElement<Element>::debugName } -> std::convertible_to<std::string_view>;
		typename std::integral_constant<
			std::size_t,
			std::string_view(UnqualifiedElement<Element>::debugName).size()>;
	});

template <typename Element>
concept ValidOptionalMetadata = ValidStatePolicyMetadata<Element> &&
	ValidDevInternalMetadata<Element> && ValidDebugNameMetadata<Element>;

template <typename Element>
concept HasValidDefinitionId = requires {
	requires std::same_as<
		std::remove_cv_t<decltype(UnqualifiedElement<Element>::definitionId)>,
		FlowDefinitionID>;
	typename std::integral_constant<uint64_t, UnqualifiedElement<Element>::definitionId.value>;
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


// A structural value places a constant debug name in the instantiation trace.
// Its contents are optional: metadata with constexpr size but runtime characters
// remains valid under the query-only contract.
template <auto DiagnosticName>
struct ConstantDiagnosticName {};

template <typename Element>
[[nodiscard]] consteval auto copy_diagnostic_name() noexcept {
	constexpr std::string_view diagnostic_name = debugName<Element>();
	std::array<char, diagnostic_name.size() + 1> characters{};
	for (std::size_t character_index = 0; character_index < diagnostic_name.size();
		 ++character_index)
		characters[character_index] = diagnostic_name[character_index];
	return characters;
}

template <typename Element>
[[nodiscard]] consteval auto element_diagnostic_name() noexcept {
	if constexpr (ValidDebugNameMetadata<Element>) {
		if constexpr (requires { typename ConstantDiagnosticName<copy_diagnostic_name<Element>()>; })
			return copy_diagnostic_name<Element>();
		else
			return std::array<char, 1>{};
	} else {
		return std::array<char, 1>{};
	}
}

template <typename Element, auto DiagnosticName>
[[nodiscard]] consteval bool validate_flow_element_contract() noexcept {
	using ElementType = UnqualifiedElement<Element>;
	static_assert(HasValidDefinitionId<ElementType>,
		"FlowUi FE001: definitionId must be a nonzero constexpr FlowUi::FlowDefinitionID. "
		"Declare static constexpr auto definitionId = FlowUi::DefinitionID(\"app/element\");");

	if constexpr (DeclaresParameters<ElementType>) {
		using Parameters = typename ElementType::Parameters;
		static_assert(ObjectCapabilityAlias<Parameters>,
			"FlowUi FE002: Parameters must name an object payload. Omit unused aliases; do not "
			"use void or a reference.");
		if constexpr (ObjectCapabilityAlias<Parameters>) {
			static_assert(CompleteType<Parameters>,
				"FlowUi FE003: Parameters must be complete. Define the payload before "
				"validating the element.");
			if constexpr (CompleteType<Parameters>)
				static_assert(DefaultConstructiblePayload<Parameters>,
					"FlowUi FE005: Parameters must be default constructible. Provide "
					"Parameters() or default member initializers.");
		}
	}
	if constexpr (DeclaresState<ElementType>) {
		using State = typename ElementType::State;
		static_assert(ObjectCapabilityAlias<State>,
			"FlowUi FE006: State must name an object payload. Omit State for stateless "
			"elements; do not use void or a reference.");
		if constexpr (ObjectCapabilityAlias<State>) {
			static_assert(CompleteType<State>,
				"FlowUi FE007: State must be complete. Define the payload before validating the "
				"element.");
			if constexpr (CompleteType<State>) {
				static_assert(DefaultConstructiblePayload<State>,
					"FlowUi FE004: State must be default constructible. Provide State() or "
					"default member initializers.");
				if constexpr (DefaultConstructiblePayload<State>)
					static_assert(NothrowDestructiblePayload<State>,
						"FlowUi FE008: State destruction must be noexcept. Handle release "
						"failures outside the destructor.");
			}
		}
	}
	if constexpr (DeclaresResources<ElementType>) {
		using Resources = typename ElementType::Resources;
		static_assert(ObjectCapabilityAlias<Resources>,
			"FlowUi FE009: Resources must name an object payload. Omit unused Resources; do not "
			"use void or a reference.");
		if constexpr (ObjectCapabilityAlias<Resources>) {
			static_assert(CompleteType<Resources>,
				"FlowUi FE010: Resources must be complete. Define the payload before validating "
				"the element.");
			if constexpr (CompleteType<Resources>) {
				static_assert(ConstructibleResourcePayload<Resources>,
					"FlowUi FE013: Resources must be default constructible or constructible "
					"from FlowUi::App&. Provide a supported constructor.");
				if constexpr (ConstructibleResourcePayload<Resources>)
					static_assert(NothrowDestructiblePayload<Resources>,
						"FlowUi FE014: Resources destruction must be noexcept. Handle release "
						"failures outside the destructor.");
			}
		}
	}
	if constexpr (DeclaresParts<ElementType>) {
		using Parts = typename ElementType::Parts;
		static_assert(PartsCapabilityAlias<Parts>,
			"FlowUi FE015: Parts must name a class payload. Omit unused Parts; do not use void "
			"or a reference.");
		if constexpr (PartsCapabilityAlias<Parts>)
			static_assert(CompleteType<Parts>,
				"FlowUi FE016: Parts must be complete. Define the part declarations before "
				"validating the element.");
	}
	static_assert(ValidStatePolicyMetadata<ElementType>,
		"FlowUi FE017: statePolicy must be a constexpr value convertible to "
		"FlowUi::ElementStatePolicy.");
	static_assert(ValidDebugNameMetadata<ElementType>,
		"FlowUi FE018: debugName must be a constexpr value convertible to std::string_view.");
	static_assert(ValidDevInternalMetadata<ElementType>,
		"FlowUi FE019: isDevInternal must be a constexpr value convertible to bool.");
	static_assert(!NamesBuildElementHook<ElementType> || HasBuildElementHook<ElementType>,
		"FlowUi FE011: buildElement must be callable as static void "
		"buildElement(FlowUi::ElementBuildContext<Element>&).");
	static_assert(!NamesConstructElementHook<ElementType> || HasConstructElementHook<ElementType>,
		"FlowUi FE012: constructElement must be callable as static Clay_ElementDeclaration "
		"constructElement(FlowUi::ElementBuildContext<Element>&).");
	static_assert(!NamesOnHoveredHook<ElementType> || HasOnHoveredHook<ElementType>,
		"FlowUi FE020: onHovered must be callable as static void "
		"onHovered(FlowUi::ElementInteractionContext<Element>&).");
	static_assert(!NamesOnPressedHook<ElementType> || HasOnPressedHook<ElementType>,
		"FlowUi FE021: onPressed must be callable as static void "
		"onPressed(FlowUi::ElementInteractionContext<Element>&).");
	static_assert(!NamesOnHeldHook<ElementType> || HasOnHeldHook<ElementType>,
		"FlowUi FE022: onHeld must be callable as static void "
		"onHeld(FlowUi::ElementInteractionContext<Element>&).");
	static_assert(!NamesOnReleasedHook<ElementType> || HasOnReleasedHook<ElementType>,
		"FlowUi FE023: onReleased must be callable as static void "
		"onReleased(FlowUi::ElementInteractionContext<Element>&).");
	static_assert(!NamesRunLogicHook<ElementType> || HasRunLogicHook<ElementType>,
		"FlowUi FE024: runLogic must be callable as static void "
		"runLogic(FlowUi::ElementInteractionContext<Element>&).");
	static_assert(CompleteType<ElementType>,
		"FlowUi FE025: the element definition tag must be complete, empty, trivially default "
		"constructible and trivially copyable. Move instance members into State.");
	if constexpr (CompleteType<ElementType>)
		static_assert(EmptyElementTag<ElementType>,
			"FlowUi FE025: the element definition tag must be empty, trivially default "
			"constructible and trivially copyable. Move instance members into State.");
	if constexpr (!NamesBuildElementHook<ElementType> && !NamesConstructElementHook<ElementType>)
		static_assert(HasBuildElementHook<ElementType> || HasConstructElementHook<ElementType>,
			"FlowUi FE026: provide static void "
			"buildElement(FlowUi::ElementBuildContext<Element>&) or static "
			"Clay_ElementDeclaration constructElement(FlowUi::ElementBuildContext<Element>&).");
	return FinalFlowElementContract<ElementType>;
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
