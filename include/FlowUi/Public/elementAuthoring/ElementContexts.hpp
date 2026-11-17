#pragma once
#include "FlowUi/Private/ApiBoundary.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "FlowUi/Public/elementAuthoring/ElementConcepts.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"
#include "FlowUi/Public/managerStructs/UiTypes.hpp"
#include "clay.h"

namespace FlowUi {

#if FLOW_UI_DEV_MODE
namespace devSystems { class DevTimingRecorder; }
#endif

/** @addtogroup flowui_element_system
 * @{
 */


class UiManager;

template <FlowElement Element>
class ElementBuilder;

namespace detail::element {
template <typename Element>
class ElementInvocation;
}

template <typename Element>
struct ElementBuildContext;

template <typename Element>
struct ElementInteractionContext;

/**
 * @brief Context passed to constructElement and buildElement callbacks.
 *
 * ElementBuildContext provides the data needed while an element emits Clay UI
 * for the current frame. It is created after the builder has processed any
 * enabled event and logic callbacks, so params includes changes made by earlier
 * callbacks in the same invocation. The context and its borrowed references
 * are valid only for the callback; do not retain them across frames.
 */
template <typename Element>
struct ElementBuildContext
{
	/** @brief Complete element definition type associated with this callback. */
	using ElementType = std::remove_cvref_t<Element>;

	/**
	 * @brief Parameter type exposed through params.
	 *
	 * Elements that omit Parameters resolve to NoElementParameters so build
	 * callbacks always receive a valid params reference.
	 */
	using ParametersType = ParametersOf<ElementType>;

	/**
	 * @brief Framework-created callback context bound to an active invocation and its parameter draft. Application elements receive this by reference; do not retain it.
	 *
	 * @see docs/api/elements.md
	 */
	ElementBuildContext(
		detail::element::ElementInvocation<ElementType>& invocation,
		ParametersType& parameters) noexcept
		: uiManager(invocation.uiManager()),
		  id(invocation.elementId()),
		  params(parameters),
		  invocation_(invocation) {}

	/**
	 * @brief UI manager that owns the active frame.
	 *
	 * Build callbacks use this reference for frame-scoped FlowUi services, such
	 * as bridging strong Flow IDs to Clay_ElementId values.
	 */
	UiManager& uiManager;

	/** Strong numeric identity of this Flow element instance. */
	FlowElementID id{};

	/**
	 * @brief Parameters for this element instance.
	 *
	 * The reference points to the builder's current parameter storage for this
	 * invocation. Mutations affect subsequent callbacks in this invocation only.
	 */
	ParametersType& params;

	/**
	 * @brief Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.
	 *
	 * The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.
	 *
	 * @see docs/api/elements.md
	 */
	template <typename E = ElementType>
		requires HasState<E> && std::same_as<E, ElementType>
	[[nodiscard]] StateOf<E>& state() try {
		return invocation_.template state<E>();
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::UiManagerInvokeElement);
		invocation_.uiManager().recordFrameError(error);
		throw;
	}

	/**
	 * @brief Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.
	 *
	 * The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.
	 *
	 * @see docs/api/elements.md
	 */
	template <typename E = ElementType>
		requires HasState<E> && std::same_as<E, ElementType>
	[[nodiscard]] const StateOf<E>& state() const try {
		return std::as_const(invocation_).template state<E>();
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::UiManagerInvokeElement);
		invocation_.uiManager().recordFrameError(error);
		throw;
	}

	/**
	 * @brief Lazily initialize and return this exact definition immutable app-wide resources by const reference. Available only with HasResources.
	 *
	 * The reference borrows manager-owned app-wide data for this callback. Resource-constructor and allocation exceptions propagate and latch frame failure; cancel the failed frame. Resources are shared across all instances of this definition.
	 *
	 * @see docs/api/elements.md
	 */
	template <typename E = ElementType>
		requires HasResources<E> && std::same_as<E, ElementType>
	[[nodiscard]] const ResourcesOf<E>& resources() const try {
		return invocation_.template resources<E>();
	} catch (...) {
		invocation_.uiManager().recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerInvokeElement));
		throw;
	}

#if FLOW_UI_DEV_MODE
	/** Return the active window frame's development timing recorder, if any. */
	[[nodiscard]] devSystems::DevTimingRecorder* devTimingRecorder() const noexcept;
#endif

	/**
	 * @brief Return this element root Clay ID directly. Library failures throw FlowUiException and latch frame failure; abandon the failed frame.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * const Clay_ElementId id = context.clayID();
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Clay_ElementId clayID() const;
	/**
	 * @brief Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed, not retained.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.childID("label");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementID> childID(LocalElementName name) const noexcept;
	/**
	 * @brief Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed, not retained.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.childID(FlowUi::RuntimeName("label"));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementID> childID(RuntimeElementName name) const noexcept;
	/**
	 * @brief Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed, not retained.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.childID(FlowUi::Indexed("row", 0));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementID> childID(IndexedElementName name) const noexcept;
	/**
	 * @brief Resolve and bridge a named child through a checked Result. Invalid child names can be rejected locally; bridge/storage failures latch the frame and require cancellation.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.clayID("label");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const Clay_ElementId id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<Clay_ElementId> clayID(LocalElementName name) const noexcept;
	/**
	 * @brief Resolve and bridge a named child through a checked Result. Invalid child names can be rejected locally; bridge/storage failures latch the frame and require cancellation.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.clayID(FlowUi::RuntimeName("label"));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const Clay_ElementId id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<Clay_ElementId> clayID(RuntimeElementName name) const noexcept;
	/**
	 * @brief Resolve and bridge a named child through a checked Result. Invalid child names can be rejected locally; bridge/storage failures latch the frame and require cancellation.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.clayID(FlowUi::Indexed("row", 0));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const Clay_ElementId id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<Clay_ElementId> clayID(IndexedElementName name) const noexcept;
	/**
	 * @brief Bind a definition-time semantic part declaration to this concrete owner and return its FlowElementPartID.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * constexpr auto declaration = FlowUi::Part("label");
	 * auto result = context.part(declaration);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementPartID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementPartID> part(FlowElementPart declaration) const noexcept;
	/**
	 * @brief Create a direct child builder at a semantic part address. Validation and parameter-construction exceptions propagate and fail the frame.
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement PartElement>
	[[nodiscard]] ElementBuilder<PartElement> createPart(
		const PartElement& element,
		FlowElementPart declaration) const;

	/**
	 * @brief Create a positional child-name sequence. Names follow position after reordering; use Keyed names for stable data identity.
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] constexpr IndexedElementIDSequence indexedIDs(
		LocalElementName baseName,
		uint64_t firstIndex = 0) const noexcept {
		return IndexedIDs(baseName, firstIndex);
	}

private:
	detail::element::ElementInvocation<ElementType>& invocation_;
};

/**
 * @brief Context passed to event and logic callbacks.
 *
 * ElementInteractionContext provides the element instance data available while
 * processing onHovered, onPressed, onHeld, onReleased, and runLogic callbacks.
 * Event callbacks are invoked from the previous completed frame interaction
 * snapshot. Logic callbacks receive the same context type when enabled.
 * The context and its borrowed references are valid only for the callback.
 */
template <typename Element>
struct ElementInteractionContext
{
	/** @brief Complete element definition type associated with this callback. */
	using ElementType = std::remove_cvref_t<Element>;

	/**
	 * @brief Parameter type exposed through params.
	 *
	 * Elements that omit Parameters resolve to NoElementParameters so interaction
	 * callbacks always receive a valid params reference.
	 */
	using ParametersType = ParametersOf<ElementType>;

	/**
	 * @brief Framework-created callback context with the previous completed interaction snapshot. Application hooks receive this by reference; do not retain it.
	 *
	 * @see docs/api/elements.md
	 */
	ElementInteractionContext(
		detail::element::ElementInvocation<ElementType>& invocation,
		ParametersType& parameters,
		const InteractionSnapshot& interaction) noexcept
		: uiManager(invocation.uiManager()),
		  id(invocation.elementId()),
		  params(parameters),
		  previousInteraction(interaction),
		  invocation_(invocation) {}

	/**
	 * @brief UI manager that owns the active frame.
	 *
	 * Interaction callbacks use this reference for frame-scoped FlowUi services
	 * and for accessing manager state needed by the callback.
	 */
	UiManager& uiManager;

	/** Strong numeric identity of this Flow element instance. */
	FlowElementID id{};

	/**
	 * @brief Parameters for this element instance.
	 *
	 * The reference points to the builder's current parameter storage for this
	 * invocation. Changes made here are retained by later callbacks in the same
	 * builder flow.
	 */
	ParametersType& params;

	/**
	 * @brief Interaction state captured from the previous completed frame.
	 *
	 * This snapshot stores the hovered, pressed, held, and released Clay element
	 * ids used to decide event callback execution and to query child element
	 * interaction state from logic callbacks.
	 */
	const InteractionSnapshot& previousInteraction;

	/**
	 * @brief Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.
	 *
	 * The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.
	 *
	 * @see docs/api/elements.md
	 */
	template <typename E = ElementType>
		requires HasState<E> && std::same_as<E, ElementType>
	[[nodiscard]] StateOf<E>& state() try {
		return invocation_.template state<E>();
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::UiManagerInvokeElement);
		invocation_.uiManager().recordFrameError(error);
		throw;
	}

	/**
	 * @brief Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.
	 *
	 * The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.
	 *
	 * @see docs/api/elements.md
	 */
	template <typename E = ElementType>
		requires HasState<E> && std::same_as<E, ElementType>
	[[nodiscard]] const StateOf<E>& state() const try {
		return std::as_const(invocation_).template state<E>();
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::UiManagerInvokeElement);
		invocation_.uiManager().recordFrameError(error);
		throw;
	}

	/**
	 * @brief Lazily initialize and return this exact definition immutable app-wide resources by const reference. Available only with HasResources.
	 *
	 * The reference borrows manager-owned app-wide data for this callback. Resource-constructor and allocation exceptions propagate and latch frame failure; cancel the failed frame. Resources are shared across all instances of this definition.
	 *
	 * @see docs/api/elements.md
	 */
	template <typename E = ElementType>
		requires HasResources<E> && std::same_as<E, ElementType>
	[[nodiscard]] const ResourcesOf<E>& resources() const try {
		return invocation_.template resources<E>();
	} catch (...) {
		invocation_.uiManager().recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerInvokeElement));
		throw;
	}

	/**
	 * @brief Return this element root Clay ID directly. Library failures throw FlowUiException and latch frame failure; abandon the failed frame.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * const Clay_ElementId id = context.clayID();
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Clay_ElementId clayID() const;
	/**
	 * @brief Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed, not retained.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.childID("label");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementID> childID(LocalElementName name) const noexcept;
	/**
	 * @brief Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed, not retained.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.childID(FlowUi::RuntimeName("label"));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementID> childID(RuntimeElementName name) const noexcept;
	/**
	 * @brief Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed, not retained.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.childID(FlowUi::Indexed("row", 0));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementID> childID(IndexedElementName name) const noexcept;
	/**
	 * @brief Resolve and bridge a named child through a checked Result. Invalid child names can be rejected locally; bridge/storage failures latch the frame and require cancellation.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.clayID("label");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const Clay_ElementId id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<Clay_ElementId> clayID(LocalElementName name) const noexcept;
	/**
	 * @brief Resolve and bridge a named child through a checked Result. Invalid child names can be rejected locally; bridge/storage failures latch the frame and require cancellation.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.clayID(FlowUi::RuntimeName("label"));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const Clay_ElementId id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<Clay_ElementId> clayID(RuntimeElementName name) const noexcept;
	/**
	 * @brief Resolve and bridge a named child through a checked Result. Invalid child names can be rejected locally; bridge/storage failures latch the frame and require cancellation.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.clayID(FlowUi::Indexed("row", 0));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const Clay_ElementId id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<Clay_ElementId> clayID(IndexedElementName name) const noexcept;
	/**
	 * @brief Bind a definition-time semantic part declaration to this concrete owner and return its FlowElementPartID.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * constexpr auto declaration = FlowUi::Part("label");
	 * auto result = context.part(declaration);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowElementPartID id = *result;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<FlowElementPartID> part(FlowElementPart declaration) const noexcept;
	/**
	 * @brief Create a direct child builder at a semantic part address. Validation and parameter-construction exceptions propagate and fail the frame.
	 *
	 * @see docs/api/elements.md
	 */
	template <FlowElement PartElement>
	[[nodiscard]] ElementBuilder<PartElement> createPart(
		const PartElement& element,
		FlowElementPart declaration) const;

	/**
	 * @brief Create a positional child-name sequence. Names follow position after reordering; use Keyed names for stable data identity.
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] constexpr IndexedElementIDSequence indexedIDs(
		LocalElementName baseName,
		uint64_t firstIndex = 0) const noexcept {
		return IndexedIDs(baseName, firstIndex);
	}

	/**
	 * @brief Invoke this action synchronously with this window/element/hook as source. Return a direct ActionInvocationStatus; callback exceptions propagate unchanged.
	 *
	 * @see docs/api/elements.md

	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
	 * const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
	 * @endcode
	 */
	[[nodiscard]] ActionInvocationStatus invoke(ActionCall call);
	/**
	 * @brief Return checked effective action availability without invoking the call. Unavailable/empty/unbound calls have enabled=false; structured resolution failures remain distinct errors.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * auto result = context.actionAvailability(call);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool enabled = result->enabled;
	 * @endcode
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] Result<ActionAvailability> actionAvailability(ActionCall call) const noexcept;

private:
	friend class ElementBuilder<ElementType>;
	void setActionInvocationSourceKind(ActionInvocationSourceKind kind) noexcept {
		actionInvocationSourceKind_ = kind;
	}
	ActionInvocationSourceKind actionInvocationSourceKind_ =
		ActionInvocationSourceKind::ElementLogic;
	detail::element::ElementInvocation<ElementType>& invocation_;
};

/**
 * @brief Flags controlling which callbacks ElementBuilder executes.
 *
 * ElementDrawOptions is passed to ElementBuilder::construct() and
 * ElementBuilder::draw(). Flags can be combined with operator| to skip selected
 * callback phases for a single builder call.
 *
 * @code{.cpp}
 * ui.createElement(kButton, "preview")
 *     .draw(FlowUi::ElementDrawOptions::SkipEventCallbacks |
 *           FlowUi::ElementDrawOptions::SkipLogicCallback);
 * @endcode
 */
enum class ElementDrawOptions : std::uint8_t {
	/** @brief Execute all callbacks normally for the selected builder flow. */
	Default = 0,

	/**
	 * @brief Skip onHovered, onPressed, onHeld, and onReleased.
	 *
	 * runLogic and the constructElement or buildElement phase still execute
	 * unless their own skip flags are set.
	 */
	SkipEventCallbacks = 1u << 0,

	/**
	 * @brief Skip runLogic.
	 *
	 * Event callbacks and the constructElement or buildElement phase still
	 * execute unless their own skip flags are set.
	 */
	SkipLogicCallback = 1u << 1,

	/**
	 * @brief Skip buildElement during ElementBuilder::draw().
	 *
	 * This flag is checked by draw(). ElementBuilder::construct() always calls
	 * constructElement after enabled event and logic callbacks.
	 */
	SkipBuildCallback = 1u << 2,
};

/**
 * @brief Combine callback skip flags without throwing. SkipBuildCallback affects draw(), while construct() still calls constructElement().
 *
 * @see docs/api/elements.md
 */
[[nodiscard]] constexpr ElementDrawOptions operator|(ElementDrawOptions first_options,
													 ElementDrawOptions second_options) noexcept {
	return static_cast<ElementDrawOptions>(static_cast<std::uint8_t>(first_options) |
										   static_cast<std::uint8_t>(second_options));
}

/**
 * @brief Return whether value and flag share any set bit; Default (zero) is never reported as set.
 *
 * @see docs/api/elements.md
 */
[[nodiscard]] constexpr bool elementDrawOptionsHas(ElementDrawOptions value,
												   ElementDrawOptions flag) noexcept {
	return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

/** @} */

} // namespace FlowUi
