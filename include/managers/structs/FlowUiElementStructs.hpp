#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "FlowUi/App.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "managers/structs/ActionManagerStructs.hpp"
#include "clay.h"
#include "managers/structs/FlowUiElementConcepts.hpp"

namespace FlowUi {

#if FLOW_UI_DEV_MODE
namespace devSystems { class DevTimingRecorder; }
#endif

/** @addtogroup flowui_element_system
 * @{
 */

/**
 * @brief Snapshot of per-element interaction state for one completed frame.
 *
 * InteractionSnapshot records the Clay elements that were hovered, pressed,
 * held, or released when FlowUi finalized a frame. Element callbacks receive
 * the previous completed snapshot so they can query stable interaction results
 * while constructing the next frame.
 *
 * Press, hold, and release currently describe the primary pointer button over
 * hovered elements. The same element can appear in more than one list if the
 * underlying interaction state makes that true for the snapshot.
 */
struct InteractionSnapshot {
	/**
	 * @brief Elements hovered during the snapshot.
	 *
	 * Filled from Clay's pointer-over element list at frame end.
	 */
	std::vector<uint32_t> hoveredElementIds;

	/**
	 * @brief Elements pressed during the snapshot.
	 *
	 * Filled when the primary pointer button transitions from up to down while
	 * over the element.
	 */
	std::vector<uint32_t> pressedElementIds;

	/**
	 * @brief Elements held during the snapshot.
	 *
	 * Filled while the primary pointer button remains down over the element
	 * after the initial press frame.
	 */
	std::vector<uint32_t> heldElementIds;

	/**
	 * @brief Elements released during the snapshot.
	 *
	 * Filled when the primary pointer button transitions from down to up while
	 * over the element.
	 */
	std::vector<uint32_t> releasedElementIds;

	/**
	 * @brief Return whether a list contains an element with the same Clay id.
	 *
	 * Comparison uses the underlying Clay id value, not object address.
	 *
	 * @param list Interaction list to search.
	 * @param id Element id to test.
	 * @retval true A matching element id exists in the list.
	 * @retval false No matching element id exists in the list.
	 */
private:
	static bool contains(const std::vector<uint32_t>& list, uint32_t id)
	{
		for (const uint32_t item : list) {
			if (item == id) {
				return true;
			}
		}
		return false;
	}

public:

	/**
	 * @brief Return whether the element was hovered in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in hoveredElementIds.
	 * @retval false The element does not appear in hoveredElementIds.
	 */
	bool isHovered(Clay_ElementId id) const { return contains(hoveredElementIds, id.id); }
	bool isHovered(FlowElementID id) const {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}
	bool isHovered(GlobalFlowID id) const {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}
	bool isHovered(FlowElementPartID id) const {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}

	/**
	 * @brief Return whether the element was pressed in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in pressedElementIds.
	 * @retval false The element does not appear in pressedElementIds.
	 */
	bool isPressed(Clay_ElementId id) const { return contains(pressedElementIds, id.id); }
	bool isPressed(FlowElementID id) const {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}
	bool isPressed(GlobalFlowID id) const {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}
	bool isPressed(FlowElementPartID id) const {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}

	/**
	 * @brief Return whether the element was held in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in heldElementIds.
	 * @retval false The element does not appear in heldElementIds.
	 */
	bool isHeld(Clay_ElementId id) const { return contains(heldElementIds, id.id); }
	bool isHeld(FlowElementID id) const {
		return contains(heldElementIds, FlowIDToClayID(id));
	}
	bool isHeld(GlobalFlowID id) const {
		return contains(heldElementIds, FlowIDToClayID(id));
	}
	bool isHeld(FlowElementPartID id) const {
		return contains(heldElementIds, FlowIDToClayID(id));
	}

	/**
	 * @brief Return whether the element was released in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in releasedElementIds.
	 * @retval false The element does not appear in releasedElementIds.
	 */
	bool isReleased(Clay_ElementId id) const { return contains(releasedElementIds, id.id); }
	bool isReleased(FlowElementID id) const {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
	bool isReleased(GlobalFlowID id) const {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
	bool isReleased(FlowElementPartID id) const {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
};

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
 * enabled event and logic callbacks, therefore it retains changes made to parameters 
 * From Previous callbacks.
 * 
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
	 * invocation. 
	 */
	ParametersType& params;

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

#if FLOW_UI_DEV_MODE
	/** Return the active window frame's development timing recorder, if any. */
	[[nodiscard]] devSystems::DevTimingRecorder* devTimingRecorder() const noexcept;
#endif

	/** Return this element's numeric Clay root ID. */
	[[nodiscard]] Clay_ElementId clayID() const;
	/** Resolve a named Clay/semantic child under this element. */
	[[nodiscard]] FlowElementID childID(LocalElementName name) const;
	[[nodiscard]] FlowElementID childID(RuntimeElementName name) const;
	[[nodiscard]] FlowElementID childID(IndexedElementName name) const;
	/** Resolve and bridge a named child in one call. */
	[[nodiscard]] Clay_ElementId clayID(LocalElementName name) const;
	[[nodiscard]] Clay_ElementId clayID(RuntimeElementName name) const;
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
	[[nodiscard]] FlowElementID childID(RuntimeElementName name) const;
	[[nodiscard]] FlowElementID childID(IndexedElementName name) const;
	/** Resolve and bridge a named child in one call. */
	[[nodiscard]] Clay_ElementId clayID(LocalElementName name) const;
	[[nodiscard]] Clay_ElementId clayID(RuntimeElementName name) const;
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

	/** Invoke an app or local UI action from this element interaction hook. */
	ActionInvocationStatus invoke(ActionCall call);
	/** Query whether an action is currently available. */
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
enum class ElementDrawOptions : uint32_t
{
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
	SkipLogicCallback  = 1u << 1,

	/**
	 * @brief Skip buildElement during ElementBuilder::draw().
	 *
	 * This flag is checked by draw(). ElementBuilder::construct() always calls
	 * constructElement after enabled event and logic callbacks.
	 */
	SkipBuildCallback  = 1u << 2,
};

/**
 * @brief Combine two ElementDrawOptions flags.
 *
 * @param a First option value.
 * @param b Second option value.
 * @return Option value containing both flags.
 */
inline ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)
{
	return static_cast<ElementDrawOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/**
 * @brief Return whether an ElementDrawOptions flag is set.
 *
 * @param value Combined option value to inspect.
 * @param flag Flag to test.
 * @retval true value contains flag.
 * @retval false value does not contain flag.
 */
inline bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)
{
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

/** @} */

} // namespace FlowUi
