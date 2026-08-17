#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "FlowUi/PublicStructs.hpp"
#include "internal/InputFieldRenderOverrides.hpp"
#include "internal/InputFieldKey.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/Text/TextStorage.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"
#include "managers/structs/InputStructs.hpp"

namespace FlowUi {
namespace detail::manager_storage {

struct InputCaretState {
	size_t anchorByteOffset = 0;
	size_t headByteOffset = 0;
	float preferredX = 0.0f;
	bool hasPreferredX = false;
};

struct InputCaretFallbackMetrics {
	bool valid = false;
	float height = 0.0f;
};

struct InputOwnedReplacementReport {
	TextRange range{};
	size_t removedByteCount = 0;
	size_t insertedByteCount = 0;
	std::string removedText{};
	std::string insertedText{};
};

struct InputOwnedTransaction {
	uint64_t sequence = 0;
	uint64_t revisionBefore = 0;
	uint64_t revisionAfter = 0;
	EditOrigin origin = EditOrigin::Programmatic;
	std::vector<InputOwnedReplacementReport> replacements{};
	std::vector<TextReplacementReport> replacementViews{};
	std::vector<TextSelection> selectionsBefore{};
	std::vector<TextSelection> selectionsAfter{};
};

struct InputPendingCommand {
	input_field::InputFieldKey fieldId{};
	TextCommand command = TextCommand::SelectAll;
	std::string payload{};
	bool extendSelection = false;
};

struct InputWrapCacheEntry {
	uint64_t revision = 0;
	TextRange hardLineRange{};
	TextLayoutDescriptor layout{};
	std::vector<TextRange> visualRanges{};
	size_t visualLineStart = 0;
	bool hasVisualLineStart = false;
};

struct InputSubmittedTextSpan {
	TextRange logicalRange{};
	Clay_ElementId textElementId{};
	uint32_t visualLineIndex = 0;
};

struct InputFieldState {
	text::FieldStorage storage{text::SingleLineStorage{}};
	bool initialized = false;
	FieldConfig config{};
	TextLayoutDescriptor layout{};
	InputFieldOverlayStyle overlayStyle{};
	std::vector<InputCaretState> carets{};
	Clay_ElementId textElementId{};
	Clay_ElementId contentElementId{};
	InputCaretFallbackMetrics fallbackMetrics{};
	uint64_t lastTouchedEpoch = 0;
	uint64_t commandsAppliedEpoch = 0;
	uint64_t revision = 0;
	uint64_t handleValue = 0;
	Clay_Vector2 scrollOffset{};
	bool caretRevealPending = true;
	float maximumScrollX = 0.0f;
	float maximumScrollY = 0.0f;
	bool modeChangeRejected = false;
	std::vector<std::string> visibleLineStrings{};
	std::vector<VisibleTextLine> visibleLines{};
	std::vector<InputSubmittedTextSpan> submittedTextSpans{};
	std::unordered_map<size_t, InputWrapCacheEntry> wrapCacheByHardLine{};
	std::vector<InputOwnedTransaction> frameTransactions{};
	std::vector<FieldEditTransaction> frameTransactionViews{};
	std::vector<FieldCommandRequest> frameCommandRequests{};
};

struct InputSelectionRange {
	size_t start = 0;
	size_t end = 0;
};

struct InputKeyRepeatState {
	bool wasDown = false;
	double repeatCountdownSeconds = 0.0;
};

struct InputPointerDragState {
	bool active = false;
	input_field::InputFieldKey fieldId{};
	size_t anchorByteOffset = 0;
};

struct InputFieldManagerState {
	FrameInput currentInput{};
	FrameInput previousInput{};
	std::unordered_map<
		input_field::InputFieldKey,
		InputFieldState,
		input_field::InputFieldKeyHash> fieldsById{};
	input_field::InputFieldKey primaryFieldId{};
	InputManagerConfig config{};
	FontFrameView fontView{};
	float pointsToPixelsScale = 96.0f / 72.0f;
	InputKeyRepeatState leftKeyRepeat{};
	InputKeyRepeatState rightKeyRepeat{};
	InputKeyRepeatState upKeyRepeat{};
	InputKeyRepeatState downKeyRepeat{};
	InputKeyRepeatState homeKeyRepeat{};
	InputKeyRepeatState endKeyRepeat{};
	InputKeyRepeatState backspaceKeyRepeat{};
	InputKeyRepeatState deleteKeyRepeat{};
	InputPointerDragState pointerDrag{};
	double caretBlinkElapsedSeconds = 0.0;
	bool caretBlinkResetPending = true;
	bool emitCaretsThisFrame = true;
	uint64_t currentTouchEpoch = 0;
	bool dirty = false;
	InputFieldFrameOverrides frameOverrides{};
	std::vector<InputPendingCommand> pendingCommands{};
	std::string selectedTextScratch{};
	uint64_t nextTransactionSequence = 1;
	uint64_t nextFieldHandle = 1;
	std::function<void(std::string_view)> setClipboardText{};
	std::function<std::string()> getClipboardText{};
};

} // namespace detail::manager_storage
} // namespace FlowUi
