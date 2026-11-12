#pragma once
#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"
#include "FlowUi/Private/utility/EnumFlags.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Private/inputService/InputFieldRenderOverrides.hpp"
#include "FlowUi/Private/inputService/InputFieldKey.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/textLayoutService/TextStorage.hpp"
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"

namespace FlowUi {
namespace detail::platform {
class WindowPlatformServices;
}
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
	std::string payload{};
	input_field::InputFieldKey fieldId{};
	TextCommand command = TextCommand::SelectAll;
	bool extendSelection = false;
};

struct InputWrapCacheEntry {
	std::vector<TextRange> visualRanges{};
	TextRange hardLineRange{};
	uint64_t revision = 0;
	size_t visualLineStart = 0;
	TextLayoutDescriptor layout{};
	bool hasVisualLineStart = false;
};

struct InputSubmittedTextSpan {
	TextRange logicalRange{};
	Clay_ElementId textElementId{};
	uint32_t visualLineIndex = 0;
};

enum class InputFieldFlag : uint8_t {
	initialized = 1u << 0,
	caret_reveal_pending = 1u << 1,
	mode_change_rejected = 1u << 2,
};

struct InputFieldState {
	InputFieldOverlayStyle overlayStyle{};
	text::FieldStorage storage{text::SingleLineStorage{}};
	std::unordered_map<size_t, InputWrapCacheEntry> wrapCacheByHardLine{};
	Clay_ElementId textElementId{};
	Clay_ElementId contentElementId{};
	FieldConfig config{};
	std::vector<InputCaretState> carets{};
	std::vector<Clay_ElementId> focusRetentionElementIds{};
	std::vector<std::string> visibleLineStrings{};
	std::vector<VisibleTextLine> visibleLines{};
	std::vector<InputSubmittedTextSpan> submittedTextSpans{};
	std::vector<InputOwnedTransaction> frameTransactions{};
	std::vector<FieldEditTransaction> frameTransactionViews{};
	std::vector<FieldCommandRequest> frameCommandRequests{};
	uint64_t lastTouchedEpoch = 0;
	uint64_t commandsAppliedEpoch = 0;
	uint64_t revision = 0;
	uint64_t handleValue = 0;
	TextLayoutDescriptor layout{};
	InputCaretFallbackMetrics fallbackMetrics{};
	Clay_Vector2 scrollOffset{};
	float maximumScrollX = 0.0f;
	float maximumScrollY = 0.0f;
	EnumFlags<InputFieldFlag> flags{InputFieldFlag::caret_reveal_pending};
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

enum class InputFieldManagerFlag : uint8_t {
	caret_blink_reset_pending = 1u << 0,
	emit_carets_this_frame = 1u << 1,
	dirty = 1u << 2,
};

struct InputFieldManagerState {
	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	FrameInput currentInput{};
	FrameInput previousInput{};
	FontFrameView fontView{};
	InputFieldFrameOverrides frameOverrides{};
	std::unordered_map<
		input_field::InputFieldKey,
		InputFieldState,
		input_field::InputFieldKeyHash> fieldsById{};
	InputPointerDragState pointerDrag{};
	std::string selectedTextScratch{};
	platform::WindowPlatformServices* platformServices = nullptr;
	std::vector<InputPendingCommand> pendingCommands{};
	input_field::InputFieldKey primaryFieldId{};
	InputKeyRepeatState leftKeyRepeat{};
	InputKeyRepeatState rightKeyRepeat{};
	InputKeyRepeatState upKeyRepeat{};
	InputKeyRepeatState downKeyRepeat{};
	InputKeyRepeatState homeKeyRepeat{};
	InputKeyRepeatState endKeyRepeat{};
	InputKeyRepeatState backspaceKeyRepeat{};
	InputKeyRepeatState deleteKeyRepeat{};
	double caretBlinkElapsedSeconds = 0.0;
	uint64_t currentTouchEpoch = 0;
	uint64_t nextTransactionSequence = 1;
	uint64_t nextFieldHandle = 1;
	InputManagerConfig config{};
	uint32_t suppressedPrimaryPressClayId = 0;
	float pointsToPixelsScale = 96.0f / 72.0f;
	EnumFlags<InputFieldManagerFlag> flags{
		InputFieldManagerFlag::caret_blink_reset_pending,
		InputFieldManagerFlag::emit_carets_this_frame};
};

} // namespace detail::manager_storage
} // namespace FlowUi
