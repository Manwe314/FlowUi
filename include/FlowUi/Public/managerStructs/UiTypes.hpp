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
	bool isHovered(Clay_ElementId id) const { return contains(hoveredElementIds, id.id); }
	/** Return whether the folded Flow identity appears in the hovered list of this snapshot. */
bool isHovered(FlowElementID id) const {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the hovered list of this snapshot. */
bool isHovered(GlobalFlowID id) const {
		return contains(hoveredElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the hovered list of this snapshot. */
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
	/** Return whether the folded Flow identity appears in the pressed list of this snapshot. */
bool isPressed(FlowElementID id) const {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the pressed list of this snapshot. */
bool isPressed(GlobalFlowID id) const {
		return contains(pressedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the pressed list of this snapshot. */
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
	/** Return whether the folded Flow identity appears in the held list of this snapshot. */
bool isHeld(FlowElementID id) const {
		return contains(heldElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the held list of this snapshot. */
bool isHeld(GlobalFlowID id) const {
		return contains(heldElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the held list of this snapshot. */
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
	/** Return whether the folded Flow identity appears in the released list of this snapshot. */
bool isReleased(FlowElementID id) const {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the released list of this snapshot. */
bool isReleased(GlobalFlowID id) const {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
	/** Return whether the folded Flow identity appears in the released list of this snapshot. */
bool isReleased(FlowElementPartID id) const {
		return contains(releasedElementIds, FlowIDToClayID(id));
	}
};


} // namespace FlowUi
