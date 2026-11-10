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

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"

namespace FlowUi {

/** @addtogroup flowui_input_field_manager
 * @{
 */

/**
 * Half-open UTF-8 byte interval [startByte, endByte) in a field document. Editing ranges must be
 * ordered, in bounds, and at codepoint boundaries.
 */
struct TextRange {
	/** Inclusive document byte offset. */
	size_t startByte = 0;
	/** Exclusive document byte offset. */
	size_t endByte = 0;
};

/** Text storage and newline policy for a field: contiguous single-line text or chunked multiline text. */
enum class TextFieldMode : uint8_t {
	SingleLine = 0, /**< Reject line endings and use contiguous storage. */
	MultiLine /**< Normalize newlines and use chunked document storage. */
};

/**
 * Synchronous visitor for a borrowed contiguous text chunk. Copy bytes during the call to retain them;
 * do not mutate the field during iteration.
 */
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
	/** Construct an empty detached view. */
	FieldTextView() noexcept = default;

	/** Return the byte count, or zero for a detached view. */
	[[nodiscard]] size_t sizeBytes() const noexcept {
		return context_ && size_ ? size_(context_) : 0u;
	}
	/** Return whether the viewed field has no text bytes. */
	[[nodiscard]] bool empty() const noexcept { return sizeBytes() == 0u; }
	/** Borrow contiguous storage when available; returns nullopt for detached or noncontiguous storage. */
	[[nodiscard]] std::optional<std::string_view> contiguous() const noexcept {
		return context_ && contiguous_ ? contiguous_(context_) : std::nullopt;
	}
	/** Copy the full document into caller-owned contiguous storage. */
	[[nodiscard]] std::string copy() const {
		return copy(TextRange{0, sizeBytes()});
	}
	/**
	 * Copy a byte interval into caller-owned storage; bounds are clamped to the document and the
	 * detached view yields an empty string.
	 */
	[[nodiscard]] std::string copy(TextRange range) const {
		return context_ && copy_ ? copy_(context_, range) : std::string{};
	}
	/**
	 * Visit the chunks overlapping range synchronously after clamping its bounds; an empty range or
	 * detached view makes no calls.
	 */
	void forEachChunk(TextRange range, const TextChunkVisitor& visitor) const {
		if (context_ && forEachChunk_) forEachChunk_(context_, range, visitor);
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
	/** Opaque field slot identity; zero marks a detached token. */
	uint64_t value = 0;
	/** Frame epoch that must match the current submission frame. */
	uint64_t frameEpoch = 0;

	/** Return whether the token has a field slot; this does not validate its epoch. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
	/** Compare both field identity and frame epoch. */
	friend constexpr bool operator==(FieldHandle, FieldHandle) noexcept = default;
};

/** Font selection and logical viewport geometry for editable text layout. */
struct TextLayoutDescriptor {
	/** Resolved font face ID used for layout and glyph metrics. */
	FontId fontId = 0;
	/** Font size in the same point-based units as Clay text declarations. */
	uint16_t fontSize = 14;
	/** Additional character spacing matching the Clay text declaration. */
	uint16_t letterSpacing = 0;
	/** Visible content width in layout pixels; used for wrapping. */
	float viewportWidth = 0.0f;
	/** Visible content height in layout pixels. */
	float viewportHeight = 0.0f;
	/** Tab expansion width measured in space characters. */
	uint8_t tabWidth = 4;
	/** Z-index assigned to manager-materialized floating text lines. */
	int16_t floatingZIndex = 0;
};

/**
 * One visible text slice borrowed from the current field query. Text and declarations are frame-scoped
 * and invalidated by field mutation.
 */
struct VisibleTextLine {
	/** Borrowed UTF-8 bytes of this visible line slice. */
	std::string_view text{};
	/** Document interval represented by the visible slice or submitted text node. */
	TextRange logicalRange{};
	/** Zero-based visual line index after wrapping. */
	uint32_t visualLineIndex = 0;
	/** Layout declaration for emitting this slice. */
	Clay_ElementDeclaration declaration{};
};

/** Map a rendered Clay text node to its half-open logical byte range for hit testing and caret geometry. */
struct FieldTextSpanSubmission {
	/** Document interval represented by the visible slice or submitted text node. */
	TextRange logicalRange{};
	/** Clay text node corresponding to the supplied logical interval. */
	Clay_ElementId textElementId{};
	/** Zero-based visual line index after wrapping. */
	uint32_t visualLineIndex = 0;
};

/** Oriented byte selection: anchor is fixed, head is the caret. Equal offsets represent an insertion point. */
struct TextSelection {
	/** Fixed endpoint as a UTF-8 byte offset. */
	size_t anchorByte = 0;
	/** Active caret endpoint as a UTF-8 byte offset. */
	size_t headByte = 0;
	/** Preferred horizontal layout coordinate for vertical caret navigation. */
	float preferredX = 0.0f;
};

/**
 * Report detail: Summary retains counts/ranges; Reversible additionally retains removed and inserted
 * bytes for external undo.
 */
enum class TransactionReportDetail : uint8_t {
	Summary = 0, /**< Ranges and byte counts only. */
	Reversible /**< Include removed and inserted byte payloads. */
};

/** Source attribution for a successful text edit transaction. */
enum class EditOrigin : uint8_t {
	TypedInput = 0, /**< Typed Unicode text input. */
	Paste, /**< Insert clipboard text. */
	Cut, /**< Remove selected text and copy it to the clipboard. */
	Delete, /**< Delete text without clipboard transfer. */
	Programmatic, /**< Caller-requested edit. */
	ExternalUndo, /**< Application-provided undo edit. */
	ExternalRedo, /**< Application-provided redo edit. */
};

/**
 * Atomic edit outcome. Applied changes the revision; NoChange and rejection outcomes leave the
 * document unchanged.
 */
enum class EditResult : uint8_t {
	Applied = 0, /**< The atomic edit changed stored text. */
	NoChange, /**< The edit produced no text change. */
	RejectedNoField, /**< The requested field does not exist. */
	RejectedNoFocus, /**< The command requires a focused field. */
	RejectedReadOnly, /**< The field rejects mutations. */
	RejectedInvalidRange, /**< A byte range is invalid or not on a UTF-8 boundary. */
	RejectedSizeLimit, /**< The resulting text exceeds maxBytes. */
	RejectedNewline, /**< Single-line mode rejects inserted newlines. */
	RejectedInvalidUtf8, /**< Inserted bytes are not valid UTF-8. */
};

/**
 * Requested replacement of one old-document byte interval. Inserted bytes are borrowed only for the
 * editing call.
 */
struct TextReplacement {
	/** Half-open interval in the document before this replacement. */
	TextRange oldRange{};
	/** UTF-8 replacement bytes; ownership remains with the caller or report storage. */
	std::string_view insertedText{};
};

/**
 * One applied replacement, addressed in the pre-edit document. Text views are populated for reversible
 * reports and remain frame-scoped.
 */
struct TextReplacementReport {
	/** Half-open interval in the document before this replacement. */
	TextRange oldRange{};
	/** Number of normalized UTF-8 bytes inserted. */
	size_t insertedByteCount = 0;
	/** Number of UTF-8 bytes removed. */
	size_t removedByteCount = 0;
	/** Removed bytes in reversible reports; empty in summary reports. */
	std::string_view removedText{};
	/** UTF-8 replacement bytes; ownership remains with the caller or report storage. */
	std::string_view insertedText{};
};

/**
 * Successful atomic field edit and before/after selections. All spans and text views are manager-
 * owned, frame-scoped reports; copy them to retain undo history.
 */
struct FieldEditTransaction {
	/** Monotonic transaction sequence assigned by the manager. */
	uint64_t sequence = 0;
	/** Field revision immediately before the atomic edit. */
	uint64_t revisionBefore = 0;
	/** Field revision immediately after the atomic edit. */
	uint64_t revisionAfter = 0;
	/** Source attribution for this edit. */
	EditOrigin origin = EditOrigin::Programmatic;
	/** Applied replacements addressed against revisionBefore. */
	std::span<const TextReplacementReport> replacements{};
	/** Borrowed selections before applying replacements. */
	std::span<const TextSelection> selectionsBefore{};
	/** Borrowed selections after applying replacements. */
	std::span<const TextSelection> selectionsAfter{};
};

/**
 * Command requested by field interaction; the application supplies undo/redo history and handles
 * submit/cancel.
 */
enum class FieldCommandRequest : uint8_t {
	Undo = 0, /**< Request application undo. */
	Redo, /**< Request application redo. */
	Submit, /**< Request submission. */
	Cancel /**< Request cancellation. */
};

/** Visual form used when the manager emits a field's caret overlay. */
enum class InputCaretShape : uint8_t {
	Bar = 0, /**< Vertical insertion bar. */
	Block, /**< Character-width block. */
	Underline /**< Horizontal underline. */
};

/**
 * Per-field caret and selection presentation.
 *
 * FieldRequest may provide this whole resolved style. Omitting it preserves the
 * window-wide InputManagerConfig defaults and the classic blinking bar caret.
 */
struct InputFieldOverlayStyle {
	/** Caret geometry used for field overlay rendering. */
	InputCaretShape caretShape = InputCaretShape::Bar;
	/** Bar width or underline thickness in layout pixels. */
	float caretThicknessPx = 2.0f;
	/** Fallback block-caret width in layout pixels. */
	float caretBlockWidthPx = 8.0f;
	/** Extra caret height above the glyph bounds in layout pixels. */
	float caretHeightOverflowTopPx = 1.0f;
	/** Extra caret height below the glyph bounds in layout pixels. */
	float caretHeightOverflowBottomPx = 1.0f;
	/** Caret RGBA color in Clay's 0..255 component range. */
	Clay_Color caretColor = {255.0f, 255.0f, 255.0f, 255.0f};
	/** Selection rectangle RGBA color in the 0..255 range. */
	Clay_Color selectionBoxColor = {66.0f, 133.0f, 244.0f, 150.0f};
	/** Selected glyph RGBA color in the 0..255 range. */
	Clay_Color selectedTextColor = {255.0f, 255.0f, 255.0f, 255.0f};
	/** Full caret blink-cycle duration in seconds. */
	double caretBlinkPeriodSeconds = 1.0;
	/** Visible portion of each blink cycle in seconds. */
	double caretBlinkVisibleSeconds = 0.5;
};

/**
 * Built-in clipboard, selection, navigation, and undo/redo-request commands dispatched by
 * InputFieldManager.
 */
enum class TextCommand : uint8_t {
	SelectAll = 0, /**< Select the whole document. */
	Copy, /**< Copy selected text. */
	Cut, /**< Remove selected text and copy it to the clipboard. */
	Paste, /**< Insert clipboard text. */
	RequestUndo, /**< Publish an Undo command request. */
	RequestRedo, /**< Publish a Redo command request. */
	MoveWordLeft, /**< Move carets to preceding word boundaries. */
	MoveWordRight, /**< Move carets to following word boundaries. */
	MoveDocumentStart, /**< Move carets to document start. */
	MoveDocumentEnd, /**< Move carets to document end. */
	DeleteWordBackward, /**< Delete to preceding word boundaries. */
	DeleteWordForward, /**< Delete to following word boundaries. */
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

	/**
	 * @brief Maximum stored UTF-8 byte count.
	 *
	 * Insertions that would make the field text exceed this byte limit are
	 * ignored. The limit is measured in bytes, not Unicode codepoints or visible
	 * glyphs.
	 */
	size_t maxBytes = std::numeric_limits<size_t>::max();
	/**
	 * @brief Selects compact single-line or chunked multiline behavior.
	 *
	 * Multiline mode normalizes inserted line endings to newline characters.
	 * Single-line mode rejects edits containing line endings.
	 */
	TextFieldMode mode = TextFieldMode::SingleLine;

	/** @brief Amount of byte content retained in frame-scoped edit reports. */
	TransactionReportDetail transactionDetail = TransactionReportDetail::Summary;

	/**
	 * @brief Whether the field rejects text edits.
	 *
	 * When true, the field can still be registered and queried, but keyboard
	 * text input, delete, backspace, and programmatic insertion are ignored.
	 * When false, the focused field may edit its stored text subject to the
	 * other FieldConfig options.
	 */
	bool readOnly = false;

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
 * detection from this result. Arrays and their nested views are valid only
 * during the current frame and may be invalidated by further field mutations;
 * copy any data that must survive.
 */
struct FieldQueryResult {

	/**
	 * @brief Current text for the field.
	 *
	 * The view points into manager-owned storage and is valid until that field
	 * state changes or is removed. Use contiguous() for compact fields and
	 * copy()/forEachChunk() for multiline documents.
	 */
	FieldTextView text{};
	/** @brief Frame-scoped token for submitTextSpan(). */
	FieldHandle field{};

	/** Visible single-line or multiline slices for this viewport. */
	std::span<const VisibleTextLine> visibleLines{};

	/** @brief Successful edits published for this field during the current frame. */
	std::span<const FieldEditTransaction> transactions{};

	/** @brief Undo, redo, submit, or cancel requests from this frame. */
	std::span<const FieldCommandRequest> commandRequests{};

	/** @brief Monotonic revision incremented once per successful atomic edit. */
	uint64_t revision = 0;

	/** Current manager-owned scroll position in layout pixels. */
	Clay_Vector2 scrollOffset{};

	/** @brief Actual retained mode after applying the requested mode migration. */
	TextFieldMode mode = TextFieldMode::SingleLine;

	/** True when a multiline-to-single-line request was rejected due to newlines. */
	bool modeChangeRejected = false;

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
