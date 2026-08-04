#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "internal/InputFieldRenderOverrides.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"
#include "managers/structs/InputStructs.hpp"

namespace FlowUi {
namespace detail::manager_storage {

struct InputCaretState {
	size_t anchorByteOffset = 0;
	size_t headByteOffset = 0;
};

struct InputCaretFallbackMetrics {
	bool valid = false;
	float height = 0.0f;
};

struct InputFieldState {
	std::string text{};
	FieldConfig config{};
	std::vector<InputCaretState> carets{};
	Clay_ElementId textElementId{};
	Clay_ElementId contentElementId{};
	InputCaretFallbackMetrics fallbackMetrics{};
	uint64_t lastTouchedEpoch = 0;
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
	std::string fieldId{};
	size_t anchorByteOffset = 0;
};

struct InputFieldManagerState {
	FrameInput currentInput{};
	FrameInput previousInput{};
	std::unordered_map<std::string, InputFieldState> fieldsById{};
	std::string primaryFieldId{};
	InputManagerConfig config{};
	FontFrameView fontView{};
	float pointsToPixelsScale = 96.0f / 72.0f;
	InputKeyRepeatState leftKeyRepeat{};
	InputKeyRepeatState rightKeyRepeat{};
	InputKeyRepeatState backspaceKeyRepeat{};
	InputKeyRepeatState deleteKeyRepeat{};
	InputPointerDragState pointerDrag{};
	double caretBlinkElapsedSeconds = 0.0;
	bool caretBlinkResetPending = true;
	bool emitCaretsThisFrame = true;
	uint64_t currentTouchEpoch = 0;
	bool dirty = false;
	InputFieldFrameOverrides frameOverrides{};
};

} // namespace detail::manager_storage
} // namespace FlowUi
