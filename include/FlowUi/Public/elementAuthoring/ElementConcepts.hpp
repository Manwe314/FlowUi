#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "FlowUi/Private/elementSystem/ElementTraits.hpp"
#include "FlowUi/Public/elementAuthoring/ElementStatePolicy.hpp"
#include "FlowUi/Public/elementAuthoring/ElementTypes.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "clay.h"

namespace FlowUi {

/** Parameter payload declared by Element, or NoElementParameters when omitted. */
template <typename Element>
using ParametersOf = typename decltype(
	detail::element::parametersTypeIdentity<Element>())::type;


/** Whether Element declares a nested State type for per-window instance storage. */
template <typename Element>
concept HasState = detail::element::hasStateCapability<Element>();

/** Whether Element declares a nested Resources type for immutable app-wide storage. */
template <typename Element>
concept HasResources = detail::element::hasResourcesCapability<Element>();

/** Element definition that declares semantic part metadata through nested Parts. */
template <typename Element>
concept HasParts = detail::element::hasPartsCapability<Element>();

/** State payload declared by Element; available only for HasState definitions. */
template <typename Element>
	requires HasState<Element>
using StateOf = typename decltype(
	detail::element::stateTypeIdentity<Element>())::type;

/** Resource payload declared by Element; available only for HasResources definitions. */
template <typename Element>
	requires HasResources<Element>
using ResourcesOf = typename decltype(
	detail::element::resourcesTypeIdentity<Element>())::type;

/**
 * Compile-time contract for an empty element definition tag with a nonzero
 * constexpr FlowDefinitionID and at least one build or construct hook.
 *
 * Optional Parameters must be default constructible. State must also be nothrow
 * destructible; Resources must be default constructible or constructible from
 * App&, and nothrow destructible. Parts must be a complete class type. Named
 * hooks must accept their corresponding context and return void, except
 * constructElement(), which returns Clay_ElementDeclaration. Optional
 * statePolicy, debugName and isDevInternal metadata must be constexpr.
 */
template <typename Element>
concept FlowElement = detail::element::FinalFlowElementContract<Element>;

/** Flow element that supports ElementBuilder::draw(). */
template <typename Element>
concept DrawableFlowElement =
	FlowElement<Element> && detail::element::HasBuildElementHook<Element>;

/** Flow element that supports ElementBuilder::construct(). */
template <typename Element>
concept ConstructibleFlowElement =
	FlowElement<Element> && detail::element::HasConstructElementHook<Element>;

} // namespace FlowUi
