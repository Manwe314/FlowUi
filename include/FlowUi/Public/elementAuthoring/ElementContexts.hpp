#pragma once

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
	 * Bind a build context to the active invocation and builder-owned parameters.
	 * ElementBuilder creates this context before calling a build hook.
	 * @param invocation Active runtime invocation that owns the state lease.
	 * @param parameters Mutable parameters shared by callbacks in this invocation.
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
	 * Borrow mutable state cached for this invocation; do not retain the reference.
	 * @tparam E This context's element type, which must declare State.
	 * @return Per-window state for this element identity.
	 * @throws FlowUiException If managed state is unavailable.
	 */
	template <typename E = ElementType>
		requires HasState<E>
	[[nodiscard]] StateOf<E>& state() {
		return invocation_.template state<E>();
	}

	/**
	 * Borrow immutable cached state for the duration of this invocation.
	 * @tparam E This context's element type, which must declare State.
	 * @return Per-window state for this element identity.
	 * @throws FlowUiException If managed state is unavailable.
	 */
	template <typename E = ElementType>
		requires HasState<E>
	[[nodiscard]] const StateOf<E>& state() const {
		return std::as_const(invocation_).template state<E>();
	}

	/**
	 * Lazily initialize and borrow this definition's immutable app-wide resources.
	 * @tparam E This context's element type, which must declare Resources.
	 * @return Resources shared by every instance of this definition in the app.
	 * @throws FlowUiException If resource registration or initialization fails.
	 */
	template <typename E = ElementType>
		requires HasResources<E>
	[[nodiscard]] const ResourcesOf<E>& resources() const {
		return invocation_.template resources<E>();
	}

#if FLOW_UI_DEV_MODE
	/** Return the active window frame's development timing recorder, if any. */
	[[nodiscard]] devSystems::DevTimingRecorder* devTimingRecorder() const noexcept;
#endif

	/** Return this element's numeric Clay root ID. */
	[[nodiscard]] Clay_ElementId clayID() const;
	/** Resolve a named Clay/semantic child under this element. */
	[[nodiscard]] FlowElementID childID(LocalElementName name) const;
	/** Resolve a runtime name under this element; the name text is hashed, not retained. */
	[[nodiscard]] FlowElementID childID(RuntimeElementName name) const;
	/** Resolve an indexed or keyed child under this element. */
	[[nodiscard]] FlowElementID childID(IndexedElementName name) const;
	/** Resolve and bridge a named child in one call. */
	[[nodiscard]] Clay_ElementId clayID(LocalElementName name) const;
	/** Resolve a runtime-named child and return its Clay bridge identity. */
	[[nodiscard]] Clay_ElementId clayID(RuntimeElementName name) const;
	/** Resolve an indexed or keyed child and return its Clay bridge identity. */
	[[nodiscard]] Clay_ElementId clayID(IndexedElementName name) const;
	/** Bind one definition-time part declaration to this concrete owner instance. */
	[[nodiscard]] FlowElementPartID part(FlowElementPart declaration) const;
	/** Create a child element directly at one semantic part address. */
	template <FlowElement PartElement>
	[[nodiscard]] ElementBuilder<PartElement> createPart(
		const PartElement& element,
		FlowElementPart declaration) const;

	/** Create a local positional sequence. State follows position after reordering. */
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
	 * Bind an interaction context to the active invocation and prior snapshot.
	 * ElementBuilder creates this context before calling interaction hooks.
	 * @param invocation Active runtime invocation that owns the state lease.
	 * @param parameters Mutable parameters shared by callbacks in this invocation.
	 * @param interaction Previous completed frame's interaction snapshot.
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

	/** Return the state resolved once and cached for this element invocation. */
	template <typename E = ElementType>
		requires HasState<E>
	[[nodiscard]] StateOf<E>& state() {
		return invocation_.template state<E>();
	}

	/** Return immutable cached state from a const context. */
	template <typename E = ElementType>
		requires HasState<E>
	[[nodiscard]] const StateOf<E>& state() const {
		return std::as_const(invocation_).template state<E>();
	}

	/** Lazily resolve and return this definition's immutable app-wide resources. */
	template <typename E = ElementType>
		requires HasResources<E>
	[[nodiscard]] const ResourcesOf<E>& resources() const {
		return invocation_.template resources<E>();
	}

	/** Return this element's numeric Clay root ID. */
	[[nodiscard]] Clay_ElementId clayID() const;
	/** Resolve a named Clay/semantic child under this element. */
	[[nodiscard]] FlowElementID childID(LocalElementName name) const;
	/** Resolve a runtime name under this element; the name text is hashed, not retained. */
	[[nodiscard]] FlowElementID childID(RuntimeElementName name) const;
	/** Resolve an indexed or keyed child under this element. */
	[[nodiscard]] FlowElementID childID(IndexedElementName name) const;
	/** Resolve and bridge a named child in one call. */
	[[nodiscard]] Clay_ElementId clayID(LocalElementName name) const;
	/** Resolve a runtime-named child and return its Clay bridge identity. */
	[[nodiscard]] Clay_ElementId clayID(RuntimeElementName name) const;
	/** Resolve an indexed or keyed child and return its Clay bridge identity. */
	[[nodiscard]] Clay_ElementId clayID(IndexedElementName name) const;
	/** Bind one definition-time part declaration to this concrete owner instance. */
	[[nodiscard]] FlowElementPartID part(FlowElementPart declaration) const;
	/** Create a child element directly at one semantic part address. */
	template <FlowElement PartElement>
	[[nodiscard]] ElementBuilder<PartElement> createPart(
		const PartElement& element,
		FlowElementPart declaration) const;

	/** Create a local positional sequence. State follows position after reordering. */
	[[nodiscard]] constexpr IndexedElementIDSequence indexedIDs(
		LocalElementName baseName,
		uint64_t firstIndex = 0) const noexcept {
		return IndexedIDs(baseName, firstIndex);
	}

	/**
	 * Invoke an app or local UI action with this window, element and hook as source.
	 * @param call Action and borrowed payload to invoke synchronously.
	 * @return Whether the action ran, was unavailable, or failed validation.
	 */
	ActionInvocationStatus invoke(ActionCall call);
	/**
	 * Query an action's current enabled state without invoking it.
	 * @param call Action and payload to validate.
	 * @return Current action availability and its reported reason.
	 */
	[[nodiscard]] ActionAvailability actionAvailability(ActionCall call) const;

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
 * @brief Combine two ElementDrawOptions flags.
 *
 * @param first_options First option value.
 * @param second_options Second option value.
 * @return Option value containing both flags.
 */
[[nodiscard]] constexpr ElementDrawOptions operator|(ElementDrawOptions first_options,
													 ElementDrawOptions second_options) noexcept {
	return static_cast<ElementDrawOptions>(static_cast<std::uint8_t>(first_options) |
										   static_cast<std::uint8_t>(second_options));
}

/**
 * @brief Return whether an ElementDrawOptions flag is set.
 *
 * @param value Combined option value to inspect.
 * @param flag Flag to test.
 * @retval true value contains flag.
 * @retval false value does not contain flag.
 */
[[nodiscard]] constexpr bool elementDrawOptionsHas(ElementDrawOptions value,
												   ElementDrawOptions flag) noexcept {
	return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

/** @} */

} // namespace FlowUi
