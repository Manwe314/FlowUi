#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <clay.h>

namespace FlowUi {

/** @addtogroup flowui_input_field_manager
 * @{
 */

/**
 * @brief Requested caret operation for an input field.
 *
 * Caret requests are usually issued from element interaction callbacks, for
 * example when a custom input element is pressed and should become the active
 * text-editing target.
 */
enum class CaretRequestKind : uint8_t {
	/**
	 * @brief Make the requested field the primary focused input field.
	 *
	 * Existing carets in other fields are cleared. If the target field already
	 * has a caret, its first caret is preserved; otherwise a caret is placed at
	 * the end of the field text.
	 */
	SetPrimary = 0,

	/**
	 * @brief Add another caret to the requested field.
	 *
	 * The new caret is placed at the last existing caret position, or at the end
	 * of the field text when the field has no caret. If no primary field is set,
	 * the requested field becomes primary.
	 */
	Add = 1,

	/**
	 * @brief Clear all carets and remove primary input focus.
	 *
	 * This affects every managed input field and cancels any active pointer
	 * selection drag.
	 */
	ClearAll = 2,
};

/**
 * @brief Behavior configuration for a requested input field.
 *
 * FieldConfig is submitted with FieldRequest every frame the field is present.
 * The values control editing behavior only; visual styling is provided by the
 * element that draws the Clay nodes and by InputManagerConfig for caret and
 * selection colors.
 */
struct FieldConfig {
	/**
	 * @brief Whether the field rejects text edits.
	 *
	 * When true, the field can still be registered and queried, but keyboard
	 * text input, delete, backspace, and programmatic insertion are ignored.
	 * When false, the focused field may edit its stored text subject to the
	 * other FieldConfig options.
	 */
	bool readOnly = false;

	/**
	 * @brief Whether newline characters may be inserted.
	 *
	 * When true, carriage return and newline input are normalized to '\n' and
	 * inserted. When false, newline input is discarded, making the field behave
	 * like a single-line editor.
	 */
	bool allowNewline = false;

	/**
	 * @brief Whether arrow keys move the caret within the field.
	 *
	 * When true, left and right arrow keys move carets, and Shift extends or
	 * contracts the selection. When false, FlowUi leaves arrow key handling to
	 * surrounding UI or application shortcuts.
	 */
	bool allowArrowNavigation = true;

	/**
	 * @brief Maximum stored UTF-8 byte count.
	 *
	 * Insertions that would make the field text exceed this byte limit are
	 * ignored. The limit is measured in bytes, not Unicode codepoints or visible
	 * glyphs.
	 */
	size_t maxBytes = std::numeric_limits<size_t>::max();
};

/**
 * @brief Input field request submitted by UI code.
 *
 * A custom input element calls InputFieldManager::requestField while building
 * its Clay nodes. The request marks the field as present for the current frame,
 * updates its behavior config and element ids, and returns the manager-owned
 * text state through FieldQueryResult.
 */
struct FieldRequest {
	/**
	 * @brief Stable field id.
	 *
	 * This id keys the persistent text and caret state. Use a value that remains
	 * stable across frames for the same logical field.
	 */
	std::string_view fieldId{};

	/**
	 * @brief Initial text used when the field state is first created.
	 *
	 * The value is copied only when fieldId has no existing managed state.
	 * Later requests with the same fieldId preserve the current edited text.
	 */
	std::string_view initialText{};

	/** @brief Field behavior configuration for this frame. */
	FieldConfig config{};

	/**
	 * @brief Clay text element id for measuring caret and selection positions.
	 *
	 * The id should refer to the Clay element that emits the field text.
	 */
	Clay_ElementId textElementId{};

	/**
	 * @brief Clay content element id for pointer hit testing.
	 *
	 * The id should refer to the clickable/editable field area. When available,
	 * FlowUi uses it to place the caret in empty space and to resolve pointer
	 * focus.
	 */
	Clay_ElementId contentElementId{};
};

/**
 * @brief Result returned from an input field request.
 *
 * FieldQueryResult describes the current manager-owned state for the field that
 * was just requested. Element code typically renders text and runs change
 * detection from this result.
 */
struct FieldQueryResult {
	/**
	 * @brief Current text for the field.
	 *
	 * The view points into manager-owned storage and is valid until that field
	 * state changes or is removed.
	 */
	std::string_view text{};

	/**
	 * @brief Whether this field owns the primary caret.
	 *
	 * True when the field is the active text-editing target and has at least one
	 * caret.
	 */
	bool hasPrimaryCaret = false;

	/**
	 * @brief Whether this field has an active selection.
	 *
	 * True when any caret in the field has different anchor and head offsets.
	 */
	bool hasSelection = false;
};

/** @} */

} // namespace FlowUi
