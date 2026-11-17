#pragma once

#include <cstdint>
#include <vector>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "clay.h"

namespace FlowUi {

/**
 * @brief Snapshot of per-element interaction state for one completed frame.
 *
 * InteractionSnapshot records the Clay elements that were hovered, pressed,
 * held, or released when FlowUi finalized a frame. Element callbacks receive
 * the previous completed snapshot so they can query stable interaction results
 * while constructing the next frame. References obtained from UiManager borrow its
 * snapshot and should not be retained as immutable history across later frames.
 * Each observer accepts Clay_ElementId, FlowElementID, GlobalFlowID and
 * FlowElementPartID directly and returns a plain bool without a Result wrapper.
 *
 * Press, hold, and release currently describe the primary pointer button over
 * hovered elements. The same element can appear in more than one list if the
 * underlying interaction state makes that true for the snapshot.

 *
 * @par Example
 * @code{.cpp}
 * const bool hovered = snapshot.isHovered(clayId);
 * const bool pressed = snapshot.isPressed(localId);
 * const bool held = snapshot.isHeld(globalId);
 * const bool released = snapshot.isReleased(partId);
 * @endcode
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

	// Compare the stored Clay identifier values rather than object addresses.
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
	[[nodiscard]] bool isHovered(Clay_ElementId id) const noexcept { return contains(hoveredElementIds, id.id); }
	/** Return whether the folded Flow identity appears in the hovered list of this snapshot. */
	[[nodiscard]] bool isHovered(FlowElementID id) const noexcept {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the hovered list of this snapshot. */
	[[nodiscard]] bool isHovered(GlobalFlowID id) const noexcept {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the hovered list of this snapshot. */
	[[nodiscard]] bool isHovered(FlowElementPartID id) const noexcept {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}

	/**
	 * @brief Return whether the element was pressed in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in pressedElementIds.
	 * @retval false The element does not appear in pressedElementIds.
	 */
	[[nodiscard]] bool isPressed(Clay_ElementId id) const noexcept { return contains(pressedElementIds, id.id); }
	/** Return whether the folded Flow identity appears in the pressed list of this snapshot. */
	[[nodiscard]] bool isPressed(FlowElementID id) const noexcept {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the pressed list of this snapshot. */
	[[nodiscard]] bool isPressed(GlobalFlowID id) const noexcept {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the pressed list of this snapshot. */
	[[nodiscard]] bool isPressed(FlowElementPartID id) const noexcept {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}

	/**
	 * @brief Return whether the element was held in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in heldElementIds.
	 * @retval false The element does not appear in heldElementIds.
	 */
	[[nodiscard]] bool isHeld(Clay_ElementId id) const noexcept { return contains(heldElementIds, id.id); }
	/** Return whether the folded Flow identity appears in the held list of this snapshot. */
	[[nodiscard]] bool isHeld(FlowElementID id) const noexcept {
		return contains(heldElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the held list of this snapshot. */
	[[nodiscard]] bool isHeld(GlobalFlowID id) const noexcept {
		return contains(heldElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the held list of this snapshot. */
	[[nodiscard]] bool isHeld(FlowElementPartID id) const noexcept {
		return contains(heldElementIds, FlowIDToClayID(id));
	}

	/**
	 * @brief Return whether the element was released in this snapshot.
	 * @param id Element id to query.
	 * @retval true The element appears in releasedElementIds.
	 * @retval false The element does not appear in releasedElementIds.
	 */
	[[nodiscard]] bool isReleased(Clay_ElementId id) const noexcept { return contains(releasedElementIds, id.id); }
	/** Return whether the folded Flow identity appears in the released list of this snapshot. */
	[[nodiscard]] bool isReleased(FlowElementID id) const noexcept {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the released list of this snapshot. */
	[[nodiscard]] bool isReleased(GlobalFlowID id) const noexcept {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the released list of this snapshot. */
	[[nodiscard]] bool isReleased(FlowElementPartID id) const noexcept {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
};


} // namespace FlowUi
