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

/** @brief Context passed to an element build callback. */
template <typename Parameters>
struct ElementBuildContext
{
	/** @brief Resolved parameter type. */
	using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

	/** @brief UI manager for the active frame. */
	UiManager& uiManager;
	/** @brief Element id string for this instance. */
	std::string_view elementID;
	/** @brief Parameters for this element instance. */
	ParametersType& params;

	/** @brief Create a child element id by appending a local child id. */
	std::string createChildElementId(std::string_view localChildId) const
	{
		return std::string(elementID) + "/" + std::string(localChildId);
	}
};

/** @brief Context passed to element interaction and logic callbacks. */
template <typename Parameters>
struct ElementInteractionContext
{
	/** @brief Resolved parameter type. */
	using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

	/** @brief UI manager for the active frame. */
	UiManager& uiManager;
	/** @brief Element id string for this instance. */
	std::string_view elementID;
	/** @brief Parameters for this element instance. */
	ParametersType& params;
	/** @brief Previous frame interaction snapshot. */
	const InteractionSnapshot& previousInteraction;

	/** @brief Create a child element id by appending a local child id. */
	std::string createChildElementId(std::string_view localChildId) const
	{
		return std::string(elementID) + "/" + std::string(localChildId);
	}
};

/** @brief Typed element definition used by UiManager::createElement(). */
template <
	typename Parameters = NoElementParameters,
	typename State = void,
	typename Resources = void,
	uint64_t DefinitionId = 0,
	bool IsDevInternal = false>
struct ElementDefinition
{
	/** @brief Resolved parameter type. */
	using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;
	/** @brief Resolved state type. */
	using StateType = std::conditional_t<std::is_void_v<State>, NoElementState, State>;
	/** @brief Resolved resources type. */
	using ResourcesType = std::conditional_t<std::is_void_v<Resources>, NoElementResources, Resources>;
	/** @brief Build callback context type. */
	using BuildContext = ElementBuildContext<Parameters>;
	/** @brief Interaction callback context type. */
	using InteractionContext = ElementInteractionContext<Parameters>;
	/** @brief State pool entry type. */
	using StatePoolEntry = std::pair<uint64_t, StateType>;

	/** @brief Compile-time definition id. */
	static constexpr uint64_t definitionId = DefinitionId;
	/** @brief Whether this definition is internal to dev tooling. */
	static constexpr bool isDevInternal = IsDevInternal;
	/** @brief Whether this definition has state. */
	static constexpr bool hasState = !std::is_void_v<State>;
	/** @brief Whether this definition has resources. */
	static constexpr bool hasResources = !std::is_void_v<Resources>;
	/** @brief Lazy resources instance for this definition specialization. */
	static inline std::optional<ResourcesType> resources{};
	/** @brief State pool keyed by Flow element id. */
	static inline std::vector<StatePoolEntry> statePool{};

	/** @brief Get or lazily create resources for this definition specialization. */
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

	/** @brief Get or create state for an element flow id. */
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

	/** @brief Return mutable state for an element flow id, or nullptr if missing. */
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

	/** @brief Return immutable state for an element flow id, or nullptr if missing. */
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

	/** @brief Erase state for an element flow id. */
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

	/** @brief Callback invoked when the element is hovered. */
	void (*onHovered)(InteractionContext&) = nullptr;
	/** @brief Callback invoked when the element is pressed. */
	void (*onPressed)(InteractionContext&) = nullptr;
	/** @brief Callback invoked when the element is held. */
	void (*onHeld)(InteractionContext&) = nullptr;
	/** @brief Callback invoked when the element is released. */
	void (*onReleased)(InteractionContext&) = nullptr;

	/** @brief Callback invoked for element logic before build. */
	void (*runLogic)(InteractionContext&) = nullptr;

	/** @brief Callback that returns a Clay declaration for construct() flows. */
	Clay_ElementDeclaration (*constructElement)(BuildContext&) = nullptr;
	/** @brief Callback that emits Clay UI for draw() flows. */
	void (*buildElement)(BuildContext&) = nullptr;
};

/** @brief Options controlling which callbacks ElementBuilder executes. */
enum class ElementDrawOptions : uint32_t
{
	/** @brief Execute the default callback set. */
	Default = 0,
	/** @brief Skip hover/press/held/release callbacks. */
	SkipEventCallbacks = 1u << 0,
	/** @brief Skip the logic callback. */
	SkipLogicCallback  = 1u << 1,
	/** @brief Skip the build callback. */
	SkipBuildCallback  = 1u << 2,
};

/** @brief Combine element draw option flags. */
inline ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)
{
	return static_cast<ElementDrawOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/** @brief Return true if a draw option flag is set. */
inline bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)
{
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

/** @} */

} // namespace FlowUi
