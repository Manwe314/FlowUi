#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "FlowUi/App.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "clay.h"

namespace FlowUi {

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
	std::vector<Clay_ElementId> hoveredElementIds;

	/**
	 * @brief Elements pressed during the snapshot.
	 *
	 * Filled when the primary pointer button transitions from up to down while
	 * over the element.
	 */
	std::vector<Clay_ElementId> pressedElementIds;

	/**
	 * @brief Elements held during the snapshot.
	 *
	 * Filled while the primary pointer button remains down over the element
	 * after the initial press frame.
	 */
	std::vector<Clay_ElementId> heldElementIds;

	/**
	 * @brief Elements released during the snapshot.
	 *
	 * Filled when the primary pointer button transitions from down to up while
	 * over the element.
	 */
	std::vector<Clay_ElementId> releasedElementIds;

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
	static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)
	{
		for (const auto& item : list) {
			if (item.id == id.id) {
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Return whether the element was hovered in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in hoveredElementIds.
	 * @retval false The element does not appear in hoveredElementIds.
	 */
	bool isHovered(Clay_ElementId id) const { return contains(hoveredElementIds, id); }

	/**
	 * @brief Return whether the element was pressed in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in pressedElementIds.
	 * @retval false The element does not appear in pressedElementIds.
	 */
	bool isPressed(Clay_ElementId id) const { return contains(pressedElementIds, id); }

	/**
	 * @brief Return whether the element was held in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in heldElementIds.
	 * @retval false The element does not appear in heldElementIds.
	 */
	bool isHeld(Clay_ElementId id) const { return contains(heldElementIds, id); }

	/**
	 * @brief Return whether the element was released in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in releasedElementIds.
	 * @retval false The element does not appear in releasedElementIds.
	 */
	bool isReleased(Clay_ElementId id) const { return contains(releasedElementIds, id); }
};

/** @brief Empty parameter type used by elements with no parameters. */
struct NoElementParameters {};

/** @brief Empty state type used by elements with no state. */
struct NoElementState {};

/** @brief Empty resource type used by elements with no resources. */
struct NoElementResources {};

class UiManager;

template <typename Parameters = NoElementParameters>
struct ElementBuildContext;

template <typename Parameters = NoElementParameters>
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
template <typename Parameters>
struct ElementBuildContext
{
	/**
	 * @brief Parameter type exposed through params.
	 *
	 * void parameter definitions resolve to NoElementParameters so build
	 * callbacks always receive a valid params reference.
	 */
	using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

	/**
	 * @brief UI manager that owns the active frame.
	 *
	 * Build callbacks use this reference for frame-scoped FlowUi services, such
	 * as converting Flow element id strings to Clay_ElementId values.
	 */
	UiManager& uiManager;

	/**
	 * @brief Flow element id string for this element instance.
	 *
	 * This is the id held by the ElementBuilder passed with createElement.
	 * @note This Id should be used for root clay element in this Flow Element.
	 */
	std::string_view elementID;

	/**
	 * @brief Parameters for this element instance.
	 *
	 * The reference points to the builder's current parameter storage for this
	 * invocation. 
	 */
	ParametersType& params;

	/**
	 * @brief Create a nested element id for child elements.
	 *
	 * The returned id is formed by appending "/" and localChildId to elementID.
	 * Use it when child Clay nodes or child Flow elements need stable ids scoped
	 * to the current element instance.
	 *
	 * @param localChildId Local id segment or relative child path.
	 * @return Combined child id string.
	 */
	std::string createChildElementId(std::string_view localChildId) const
	{
		return std::string(elementID) + "/" + std::string(localChildId);
	}
};

/**
 * @brief Context passed to event and logic callbacks.
 *
 * ElementInteractionContext provides the element instance data available while
 * processing onHovered, onPressed, onHeld, onReleased, and runLogic callbacks.
 * Event callbacks are invoked from the previous completed frame interaction
 * snapshot. Logic callbacks receive the same context type when enabled.
 */
template <typename Parameters>
struct ElementInteractionContext
{
	/**
	 * @brief Parameter type exposed through params.
	 *
	 * void parameter definitions resolve to NoElementParameters so interaction
	 * callbacks always receive a valid params reference.
	 */
	using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

	/**
	 * @brief UI manager that owns the active frame.
	 *
	 * Interaction callbacks use this reference for frame-scoped FlowUi services
	 * and for accessing manager state needed by the callback.
	 */
	UiManager& uiManager;

	/**
	 * @brief Flow element id string for this element instance.
	 *
	 * This is the id held by the ElementBuilder passed with createElement.
	 */
	std::string_view elementID;

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
	 * @brief Create a nested element id for child elements.
	 *
	 * The returned id is formed by appending "/" and localChildId to elementID.
	 * Use it when child Clay nodes or child Flow elements need stable ids scoped
	 * to the current element instance.
	 *
	 * @param localChildId Local id segment or relative child path.
	 * @return Combined child id string.
	 */
	std::string createChildElementId(std::string_view localChildId) const
	{
		return std::string(elementID) + "/" + std::string(localChildId);
	}
};

/**
 * @brief Typed definition for a FlowUi element.
 *
 * ElementDefinition binds an element's parameter, state, and resource structs
 * to the callbacks executed by ElementBuilder. Store an ElementDefinition in a const variable
 * to use with ElementBuilder.
 * 
 * Example:
 * @code{.cpp}
 * struct exampleParameters {
 * 		int i = 0;
 * };
 * using exampleElementDefinition = FlowUi::ElementDefinition< 
 * 											exampleParameters,
 * 														 void,
 * 														 void,
 * 														 FLOW_DEF_ID("Example")>;
 * inline const exampleElementDefinition kExampleElement = {
 * // function pointers set here.
 * };
 * @endcode
 *
 * @tparam Parameters User-defined parameter struct copied into each builder
 * instance. Use void or omit the argument for elements with no parameters.
 * @tparam State User-defined per-element-instance state struct keyed by Flow
 * element id. Use void for stateless elements.
 * @tparam Resources User-defined shared resource struct stored once per
 * definition specialization. Use void for elements with no shared resources.
 * @tparam DefinitionId Compile-time id for this element definition.
 * @tparam IsDevInternal Marks this definition as internal to FlowUi dev tooling.
 *
 * @see @ref md_docs_2concepts_2element__system "Element System"
 * @see @ref md_docs_2tutorials_2custom__elements "Custom Elements"
 * @see @ref md_docs_2tutorials_2developer__mode "Developer Mode"
 */
template <
	typename Parameters = NoElementParameters,
	typename State = void,
	typename Resources = void,
	uint64_t DefinitionId = 0,
	bool IsDevInternal = false>
struct ElementDefinition
{
	/** @brief Parameter struct used by this element definition. */
	using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

	/** @brief State struct used by this element definition. */
	using StateType = std::conditional_t<std::is_void_v<State>, NoElementState, State>;

	/** @brief Resources struct used by this element definition. */
	using ResourcesType = std::conditional_t<std::is_void_v<Resources>, NoElementResources, Resources>;

	/** @brief Build callback context type. */
	using BuildContext = ElementBuildContext<Parameters>;

	/** @brief Interaction callback context type. */
	using InteractionContext = ElementInteractionContext<Parameters>;

	/** @brief State pool entry type. */
	using StatePoolEntry = std::pair<uint64_t, StateType>;

	/**
	 * @brief Compile-time id for this element definition.
	 *
	 * The id belongs to the element definition type, not to one drawn element
	 * instance. Element instances are keyed separately by their Flow element id.
	 */
	static constexpr uint64_t definitionId = DefinitionId;

	/**
	 * @brief Whether this definition is internal to FlowUi dev tooling.
	 *
	 * Set the fifth ElementDefinition template argument to true to mark the
	 * definition as dev-internal:
	 *
	 * @code{.cpp}
	 * using DevOnlyDefinition = FlowUi::ElementDefinition<
	 *     DevOnlyParams,
	 *     DevOnlyState,
	 *     void,
	 *     FLOW_DEF_ID("flowui_dev_only"),
	 *     true>;
	 * @endcode
	 *
	 * Dev-internal definitions are passed to the dev capture runtime as internal
	 * tooling.
	 * @note Normal user elements should leave this as false. Normally users wont ever need to set this to true. 
	 */
	static constexpr bool isDevInternal = IsDevInternal;

	/** @brief Whether this definition has a user-defined state struct. */
	static constexpr bool hasState = !std::is_void_v<State>;

	/** @brief Whether this definition has a user-defined resources struct. */
	static constexpr bool hasResources = !std::is_void_v<Resources>;

	/**
	 * @brief Shared resources container for this definition specialization.
	 *
	 * Resources are stored once per ElementDefinition type. getResources()
	 * initializes this optional on first use.
	 */
	static inline std::optional<ResourcesType> resources{};

	/**
	 * @brief State storage for element instances of this definition.
	 *
	 * Each entry is keyed by a Flow element id, usually produced with toFlowId()
	 * from the element id string passed to createElement().
	 */
	static inline std::vector<StatePoolEntry> statePool{};

	/**
	 * @brief Get or lazily create this definition's shared resources.
	 *
	 * The resources instance is created once and stored in resources. Construction
	 * prefers ResourcesType(App&), then ResourcesType(UiManager&), then a default
	 * constructor.
	 *
	 * @param app Active FlowUi application used for resource construction.
	 * @return Mutable resources instance for this definition specialization.
	 *
	 * @warning This function is only available when the Resources template argument
	 * is not void; otherwise the static_assert fails at compile time.
	 *
	 * @code{.cpp}
	 * struct ButtonResources {
	 *     explicit ButtonResources(FlowUi::App& app) {
	 *         (void)app;
	 *     }
	 * };
	 *
	 * using ButtonDefinition = FlowUi::ElementDefinition<
	 *     ButtonParams,
	 *     void,
	 *     ButtonResources,
	 *     FLOW_DEF_ID("button")>;
	 *
	 * void initializeResources(FlowUi::App& app) {
	 *     ButtonResources& resources = ButtonDefinition::getResources(app);
	 *     (void)resources;
	 * }
	 * @endcode
	 * 
	 * @note returned Resources is Mustable but normally this struct would be used for app lifetime constant values. rarely it might be useful to mutate resources at runtime.
	 */
	static ResourcesType& getResources(App& app)
	{
		static_assert(hasResources, "FlowUi: getResources is only available when ElementDefinition Resources template argument is not void.");
		if (!resources.has_value()) {
			if constexpr (std::is_constructible_v<ResourcesType, App&>) {
				resources.emplace(app);
			} else if constexpr (std::is_constructible_v<ResourcesType, UiManager&>) {
				resources.emplace(app.ui());
			} else {
				resources.emplace();
			}
		}
		return *resources;
	}

	/**
	 * @brief Get or create mutable state for an element instance.
	 *
	 * If statePool already contains elementFlowId, the existing state is
	 * returned. Otherwise a default-constructed StateType is added and returned.
	 *
	 * @param elementFlowId Flow id for the element instance.
	 * @return Mutable state associated with elementFlowId.
	 *
	 * @warning This function is only available when the State template argument is
	 * not void; otherwise the static_assert fails at compile time.
	 *
	 * use this in element callbacks
	 * @code{.cpp}
	 * +[](ButtonDefinition::InteractionContext& context) {
	 *     ButtonState& state =
	 *         ButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
	 *     state.clickCount += 1;
	 * }
	 * @endcode
	 * 
	 * This function can also be used to get data outside element callbacks:
	 * @code{.cpp}
	 * int grabNumber() {
	 *    exampleState& state = ExampleElementDefinition::getOrCreateState(FlowUi::toFlowId("Example"));
	 * 	  return state.exampleNumber;
	 * }
	 * @endcode
	 */
	static StateType& getOrCreateState(uint64_t elementFlowId)
	{
		static_assert(hasState, "FlowUi: getOrCreateState is only available when ElementDefinition State template argument is not void.");
		for (StatePoolEntry& entry : statePool) {
			if (entry.first == elementFlowId) {
				return entry.second;
			}
		}
		statePool.emplace_back(elementFlowId, StateType{});
		return statePool.back().second;
	}

	/**
	 * @brief Return mutable state for an element instance if it exists.
	 *
	 * This lookup does not create a new state entry.
	 *
	 * @param elementFlowId Flow id for the element instance.
	 * @return Pointer to mutable state, or nullptr when no entry exists.
	 *
	 * @warning This function is only available when the State template argument is
	 * not void; otherwise the static_assert fails at compile time.
	 *
	 * @code{.cpp}
	 * if (ButtonState* state = ButtonDefinition::tryGetState(FLOW_ID("button-1"))) {
	 *     state->isActive = false;
	 * }
	 * @endcode
	 */
	static StateType* tryGetState(uint64_t elementFlowId)
	{
		static_assert(hasState, "FlowUi: tryGetState is only available when ElementDefinition State template argument is not void.");
		for (StatePoolEntry& entry : statePool) {
			if (entry.first == elementFlowId) {
				return &entry.second;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Return immutable state for an element instance if it exists.
	 *
	 * This lookup does not create a new state entry.
	 *
	 * @param elementFlowId Flow id for the element instance.
	 * @return Pointer to immutable state, or nullptr when no entry exists.
	 *
	 * @warning This function is only available when the State template argument is
	 * not void; otherwise the static_assert fails at compile time.
	 *
	 * @code{.cpp}
	 * const ButtonState* state = ButtonDefinition::tryGetStateConst(FLOW_ID("button-1"));
	 * const bool active = state && state->isActive;
	 * @endcode
	 */
	static const StateType* tryGetStateConst(uint64_t elementFlowId)
	{
		static_assert(hasState, "FlowUi: tryGetStateConst is only available when ElementDefinition State template argument is not void.");
		for (const StatePoolEntry& entry : statePool) {
			if (entry.first == elementFlowId) {
				return &entry.second;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Erase state for an element instance.
	 *
	 * The matching state entry is removed from statePool when present.
	 *
	 * @param elementFlowId Flow id for the element instance.
	 * @retval true State existed and was erased.
	 * @retval false No state entry existed for elementFlowId.
	 *
	 * @warning This function is only available when the State template argument is
	 * not void; otherwise the static_assert fails at compile time.
	 *
	 * @code{.cpp}
	 * const bool removed = ButtonDefinition::eraseState(FLOW_ID("button-1"));
	 * (void)removed;
	 * @endcode
	 * 
	 * @note For now there is no automatic grabage collection for state structs. use this function to keep memory use low.
	 */
	static bool eraseState(uint64_t elementFlowId)
	{
		static_assert(hasState, "FlowUi: eraseState is only available when ElementDefinition State template argument is not void.");
		for (std::size_t i = 0; i < statePool.size(); ++i) {
			if (statePool[i].first == elementFlowId) {
				statePool[i] = std::move(statePool.back());
				statePool.pop_back();
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Callback invoked when the root element was hovered.
	 *
	 * ElementBuilder calls this before runLogic and before constructElement or
	 * buildElement when event callbacks are enabled and previousInteraction
	 * contains the root Clay id.
	 *
	 * @code{.cpp}
	 * +[](ButtonDefinition::InteractionContext& context) {
	 *     context.params.backgroundColor = FlowUi::Flow_Color("#fff3e8ff");
	 * }
	 * @endcode
	 */
	void (*onHovered)(InteractionContext&) = nullptr;

	/**
	 * @brief Callback invoked when the root element was pressed.
	 *
	 * ElementBuilder calls this after onHovered and before onHeld, onReleased,
	 * runLogic, and constructElement or buildElement when previousInteraction
	 * contains a press for the root Clay id.
	 *
	 * @code{.cpp}
	 * +[](ButtonDefinition::InteractionContext& context) {
	 *     ButtonState& state =
	 *         ButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
	 *     state.clickCount += 1;
	 * }
	 * @endcode
	 */
	void (*onPressed)(InteractionContext&) = nullptr;

	/**
	 * @brief Callback invoked when the root element was held.
	 *
	 * ElementBuilder calls this after onPressed and before onReleased, runLogic,
	 * and constructElement or buildElement when previousInteraction contains a
	 * hold for the root Clay id.
	 *
	 * @code{.cpp}
	 * +[](SliderDefinition::InteractionContext& context) {
	 *     SliderState& state =
	 *         SliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
	 *     state.dragging = true;
	 * }
	 * @endcode
	 */
	void (*onHeld)(InteractionContext&) = nullptr;

	/**
	 * @brief Callback invoked when the root element was released.
	 *
	 * ElementBuilder calls this after onHeld and before runLogic and
	 * constructElement or buildElement when previousInteraction contains a
	 * release for the root Clay id.
	 *
	 * @code{.cpp}
	 * +[](SliderDefinition::InteractionContext& context) {
	 *     SliderState& state =
	 *         SliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
	 *     state.dragging = false;
	 * }
	 * @endcode
	 */
	void (*onReleased)(InteractionContext&) = nullptr;

	/**
	 * @brief Per-frame logic callback invoked before constructElement or buildElement.
	 *
	 * ElementBuilder calls this after event callbacks when logic callbacks are
	 * enabled.
	 * unlike event callbacks, runLogic will allways be called if enabled from Element builder.
	 * unlike build or Construct callbacks Logic is passed InteractionContext.
	 * 
	 * use runLogic to perform additional functional work even query for interaction events on other elements
	 *
	 * @code{.cpp}
	 * +[](SliderDefinition::InteractionContext& context) {
	 *     const Clay_ElementId toggleID = context.params.toggleElementId;
	 *     if (context.previousInteraction.isPressed(toggleID)) {
	 *         SliderState& state =
	 *             SliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
	 *         state.isEnabled = true;
	 *     }
	 * }
	 * @endcode
	 */
	void (*runLogic)(InteractionContext&) = nullptr;

	/**
	 * @brief Callback that returns the root Clay declaration for construct() flows.
	 *
	 * ElementBuilder::construct() calls this after event and logic callbacks.
	 * The returned declaration is applied to the root element opened by the
	 * builder using the builder's element id. This callback returns data; it is
	 * not the draw() path that emits an arbitrary Clay subtree.
	 * 
	 * Use this callback for when you want to make open ended Flow Elements that need manual closing
	 *
	 * @code{.cpp}
	 * +[](PanelDefinition::BuildContext& context) -> Clay_ElementDeclaration {
	 *     Clay_ElementDeclaration root{};
	 *     root.layout.sizing = context.params.sizing;
	 *     root.backgroundColor = context.params.backgroundColor;
	 *     return root;
	 * }
	 * @endcode
	 */
	Clay_ElementDeclaration (*constructElement)(BuildContext&) = nullptr;

	/**
	 * @brief Callback that emits Clay UI for draw() flows.
	 *
	 * ElementBuilder::draw() calls this after event and logic callbacks when
	 * build callbacks are enabled. The callback owns the Clay emission for the
	 * element and may emit the root node and any child nodes or child Flow
	 * elements.
	 *
	 * @code{.cpp}
	 * +[](ButtonDefinition::BuildContext& context) {
	 *     Clay_ElementDeclaration root{};
	 *     root.id = context.uiManager.toClayEID(context.elementID);
	 *     root.layout.sizing = context.params.sizing;
	 *
	 *     CLAY(root) {}
	 * }
	 * @endcode
	 */
	void (*buildElement)(BuildContext&) = nullptr;
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
