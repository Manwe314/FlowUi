#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <clay.h>

#include "managers/structs/FontManagerStructs.hpp"

namespace FlowUi {

/** @addtogroup flowui_input_field_manager
 * @{
 */

struct TextRange {
	size_t startByte = 0;
	size_t endByte = 0;
};

enum class TextFieldMode : uint8_t { SingleLine = 0, MultiLine };

using TextChunkVisitor = std::function<void(std::string_view)>;

/**
 * @brief Non-owning access to manager-owned single-line or chunked text.
 *
 * The view is invalidated by the next mutation of its field. Single-line text
 * is guaranteed contiguous. Multiline callers explicitly copy a range or
 * visit its chunks, avoiding an accidental full-document flatten per frame.
 */
class FieldTextView {
public:
	FieldTextView() noexcept = default;

	[[nodiscard]] size_t sizeBytes() const noexcept {
		return context_ && size_ ? size_(context_) : 0u;
	}
	[[nodiscard]] bool empty() const noexcept { return sizeBytes() == 0u; }
	[[nodiscard]] std::optional<std::string_view> contiguous() const noexcept {
		return context_ && contiguous_ ? contiguous_(context_) : std::nullopt;
	}
	[[nodiscard]] std::string copy() const {
		return copy(TextRange{0, sizeBytes()});
	}
	[[nodiscard]] std::string copy(TextRange range) const {
		return context_ && copy_ ? copy_(context_, range) : std::string{};
	}
	void forEachChunk(TextRange range, const TextChunkVisitor& visitor) const {
		if (context_ && forEachChunk_) forEachChunk_(context_, range, visitor);
	}

	/** Compatibility conversion for compact single-line fields. */
	[[nodiscard]] operator std::string_view() const noexcept {
		return contiguous().value_or(std::string_view{});
	}

private:
	friend class InputFieldManager;
	using SizeFunction = size_t (*)(const void*) noexcept;
	using ContiguousFunction = std::optional<std::string_view> (*)(const void*) noexcept;
	using CopyFunction = std::string (*)(const void*, TextRange);
	using ForEachChunkFunction = void (*)(const void*, TextRange, const TextChunkVisitor&);

	FieldTextView(
		const void* context,
		SizeFunction size,
		ContiguousFunction contiguous,
		CopyFunction copy,
		ForEachChunkFunction forEachChunk) noexcept
		: context_(context), size_(size), contiguous_(contiguous), copy_(copy),
		  forEachChunk_(forEachChunk) {}

	const void* context_ = nullptr;
	SizeFunction size_ = nullptr;
	ContiguousFunction contiguous_ = nullptr;
	CopyFunction copy_ = nullptr;
	ForEachChunkFunction forEachChunk_ = nullptr;
};

/** Frame-scoped opaque token used when submitting visible multiline spans. */
struct FieldHandle {
	uint64_t value = 0;
	uint64_t frameEpoch = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
	friend constexpr bool operator==(FieldHandle, FieldHandle) noexcept = default;
};

struct TextLayoutDescriptor {
	FontId fontId = 0;
	uint16_t fontSize = 14;
	uint16_t letterSpacing = 0;
	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;
	uint8_t tabWidth = 4;
};

struct VisibleTextLine {
	std::string_view text{};
	TextRange logicalRange{};
	uint32_t visualLineIndex = 0;
	Clay_ElementDeclaration declaration{};
};

struct FieldTextSpanSubmission {
	TextRange logicalRange{};
	Clay_ElementId textElementId{};
	uint32_t visualLineIndex = 0;
};

struct TextSelection {
	size_t anchorByte = 0;
	size_t headByte = 0;
	float preferredX = 0.0f;
};

enum class TransactionReportDetail : uint8_t { Summary = 0, Reversible };

enum class EditOrigin : uint8_t {
	TypedInput = 0, Paste, Cut, Delete, Programmatic, ExternalUndo, ExternalRedo,
};

enum class EditResult : uint8_t {
	Applied = 0, NoChange, RejectedNoField, RejectedNoFocus, RejectedReadOnly,
	RejectedInvalidRange, RejectedSizeLimit, RejectedNewline, RejectedInvalidUtf8,
};

struct TextReplacement {
	TextRange oldRange{};
	std::string_view insertedText{};
};

struct TextReplacementReport {
	TextRange oldRange{};
	size_t insertedByteCount = 0;
	size_t removedByteCount = 0;
	std::string_view removedText{};
	std::string_view insertedText{};
};

struct FieldEditTransaction {
	uint64_t sequence = 0;
	uint64_t revisionBefore = 0;
	uint64_t revisionAfter = 0;
	EditOrigin origin = EditOrigin::Programmatic;
	std::span<const TextReplacementReport> replacements{};
	std::span<const TextSelection> selectionsBefore{};
	std::span<const TextSelection> selectionsAfter{};
};

enum class FieldCommandRequest : uint8_t { Undo = 0, Redo, Submit, Cancel };

/** Visual form used when the manager emits a field's caret overlay. */
enum class InputCaretShape : uint8_t { Bar = 0, Block, Underline };

/**
 * Per-field caret and selection presentation.
 *
 * FieldRequest may provide this whole resolved style. Omitting it preserves the
 * window-wide InputManagerConfig defaults and the classic blinking bar caret.
 */
struct InputFieldOverlayStyle {
	InputCaretShape caretShape = InputCaretShape::Bar;
	float caretThicknessPx = 2.0f;
	float caretBlockWidthPx = 8.0f;
	float caretHeightOverflowTopPx = 1.0f;
	float caretHeightOverflowBottomPx = 1.0f;
	Clay_Color caretColor = {255.0f, 255.0f, 255.0f, 255.0f};
	Clay_Color selectionBoxColor = {66.0f, 133.0f, 244.0f, 150.0f};
	Clay_Color selectedTextColor = {255.0f, 255.0f, 255.0f, 255.0f};
	double caretBlinkPeriodSeconds = 1.0;
	double caretBlinkVisibleSeconds = 0.5;
};

enum class TextCommand : uint8_t {
	SelectAll = 0, Copy, Cut, Paste, RequestUndo, RequestRedo,
	MoveWordLeft, MoveWordRight, MoveDocumentStart, MoveDocumentEnd,
	DeleteWordBackward, DeleteWordForward,
};

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
 * element that draws the Clay nodes and by FieldRequest::overlayStyle (falling
 * back to InputManagerConfig) for caret and selection presentation.
 */
struct FieldConfig {
	/** @brief Selects compact single-line or chunked multiline behavior. */
	TextFieldMode mode = TextFieldMode::SingleLine;

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
	 * Compatibility spelling for requesting MultiLine mode. New code should set
	 * mode directly. A multiline mode always enables normalized '\n' insertion;
	 * a single-line mode always rejects newline edits.
	 */
	bool allowNewline = false;

	/** @brief Wrap multiline hard lines into manager-owned visual slices. */
	bool softWrap = false;

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

	/** @brief Amount of byte content retained in frame-scoped edit reports. */
	TransactionReportDetail transactionDetail = TransactionReportDetail::Summary;
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
	 * @brief Initial text used when the field state is first created.
	 *
	 * The value is copied only when the explicit ID passed to requestField() has
	 * no existing managed state. Later requests with the same ID preserve the
	 * current edited text.
	 */
	std::string_view initialText{};

	/** @brief Field behavior configuration for this frame. */
	FieldConfig config{};

	/** @brief Font and viewport data used for editable-line materialization. */
	TextLayoutDescriptor layout{};

	/** Optional field-local caret and selection presentation. */
	std::optional<InputFieldOverlayStyle> overlayStyle = std::nullopt;

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

	/**
	 * @brief Auxiliary element surfaces whose pointer presses retain this field's focus.
	 *
	 * Composite inputs use these surfaces for controls such as numeric increment
	 * and decrement buttons. Pressing one of the submitted elements does not move
	 * the caret, begin text selection, or clear the field's existing focus. The
	 * manager copies the IDs during requestField(), so this span only needs to
	 * remain valid for the duration of that call.
	 */
	std::span<const Clay_ElementId> focusRetentionElementIds{};
};

/**
 * @brief Result returned from an input field request.
 *
 * FieldQueryResult describes the current manager-owned state for the field that
 * was just requested. Element code typically renders text and runs change
 * detection from this result.
 */
struct FieldQueryResult {
	/** @brief Frame-scoped token for submitTextSpan(). */
	FieldHandle field{};

	/**
	 * @brief Current text for the field.
	 *
	 * The view points into manager-owned storage and is valid until that field
	 * state changes or is removed. Use contiguous() for compact fields and
	 * copy()/forEachChunk() for multiline documents.
	 */
	FieldTextView text{};

	/** @brief Actual retained mode after applying the requested mode migration. */
	TextFieldMode mode = TextFieldMode::SingleLine;

	/** True when a multiline-to-single-line request was rejected due to newlines. */
	bool modeChangeRejected = false;

	/** Visible single-line or multiline slices for this viewport. */
	std::span<const VisibleTextLine> visibleLines{};

	/** Current manager-owned scroll position in layout pixels. */
	Clay_Vector2 scrollOffset{};

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

	/** @brief Monotonic revision incremented once per successful atomic edit. */
	uint64_t revision = 0;

	/** @brief Successful edits published for this field during the current frame. */
	std::span<const FieldEditTransaction> transactions{};

	/** @brief Undo, redo, submit, or cancel requests from this frame. */
	std::span<const FieldCommandRequest> commandRequests{};
};

/** @} */

} // namespace FlowUi
