#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "FlowUi/PublicStructs.hpp"
#include "window/Inputs.hpp"

struct FontManager;

namespace FlowUi {

struct InputFieldRectOverride {
	int32_t insertBeforeCommandIndex = 0;
	Clay_BoundingBox boundingBox{};
	Clay_Color color = Clay_Color{0.0f, 0.0f, 0.0f, 0.0f};
};

struct InputFieldTextColorRangeOverride {
	size_t startByteOffset = 0u;
	size_t endByteOffset = 0u;
};

struct InputFieldTextColorOverride {
	int32_t commandIndex = 0;
	Clay_Color color = Clay_Color{0.0f, 0.0f, 0.0f, 0.0f};
	std::vector<InputFieldTextColorRangeOverride> ranges{};
};

struct InputFieldFrameOverrides {
	std::vector<InputFieldRectOverride> rects{};
	std::vector<InputFieldTextColorOverride> textColorOverrides{};
};

class InputFieldManager {
public:
	enum class CaretRequestKind : uint8_t {
		SetPrimary = 0,
		Add = 1,
		ClearAll = 2,
	};

	struct FieldConfig {
		bool readOnly = false;
		bool allowNewline = false;
		bool allowArrowNavigation = true;
		size_t maxBytes = std::numeric_limits<size_t>::max();
	};

	struct FieldRequest {
		std::string_view fieldId{};
		std::string_view initialText{};
		FieldConfig config{};
		Clay_ElementId textElementId{};
		Clay_ElementId contentElementId{};
	};

	struct FieldQueryResult {
		std::string_view text{};
		bool hasPrimaryCaret = false;
		bool hasSelection = false;
	};

	void setConfig(const UiConfig::InputManagerConfig& config);
	void setFontManager(const ::FontManager* fontManager, float pointsToPixelsScale);
	void beginFrame(const FrameInput& currentInput, const FrameInput& previousInput);
	Clay_RenderCommandArray endFrame(const Clay_RenderCommandArray& renderCommands);
	const InputFieldFrameOverrides& frameOverrides() const { return frameOverrides_; }
	FieldQueryResult requestField(const FieldRequest& request);
	void requestCaret(std::string_view fieldId, CaretRequestKind kind);
	bool removeField(std::string_view fieldId);
	void clear();
	bool hasPrimaryFieldFocus() const;
	std::string_view getSelectedText() const;
	bool insertTextAtPrimaryCaret(std::string_view utf8Text);

private:
	struct CaretState {
		size_t anchorByteOffset = 0u;
		size_t headByteOffset = 0u;
	};

	struct FieldState {
		std::string text{};
		FieldConfig config{};
		std::vector<CaretState> carets{};
		Clay_ElementId textElementId{};
		Clay_ElementId contentElementId{};
		bool touchedThisFrame = false;
	};

	struct SelectionRange {
		size_t start = 0u;
		size_t end = 0u;
	};

	struct KeyRepeatState {
		bool wasDown = false;
		double repeatCountdownSeconds = 0.0;
	};

	struct PointerDragState {
		bool active = false;
		std::string fieldId{};
		size_t anchorByteOffset = 0u;
	};

	void applyKeyboardEdits();
	void markCaretBlinkReset();
	void updateCaretBlinkVisibility(bool hasAnyActiveCaret);
	bool shouldTriggerActionWithRepeat(int key, KeyRepeatState& state);
	void applyTextInsertion(FieldState& field, std::string_view utf8Text);
	void applyDelete(FieldState& field, bool backspace);
	void moveCaretsHorizontal(FieldState& field, int direction, bool selecting);
	void clampCaretsToText(FieldState& field) const;
	void eraseRange(FieldState& field, size_t start, size_t end, size_t sourceCaretIndex);

	static size_t clampUtf8Boundary(std::string_view text, size_t offset);
	static size_t nextUtf8Codepoint(std::string_view text, size_t offset);
	static size_t prevUtf8Codepoint(std::string_view text, size_t offset);
	static bool isUtf8ContinuationByte(char c);
	static bool keyPressedThisFrame(const FrameInput& currentInput, const FrameInput& previousInput, int key);
	static bool keyDown(const FrameInput& input, int key);
	static bool caretHasSelection(const CaretState& caret);
	static size_t caretSelectionStart(const CaretState& caret);
	static size_t caretSelectionEnd(const CaretState& caret);
	static std::string encodeTextInput(const FrameInput& input, bool allowNewline);
	static bool elementIdIsValid(const Clay_ElementId& id);

	float measureTextSlice(const Clay_StringSlice& text, const Clay_TextRenderData& textData) const;
	static Clay_StringSlice subSlice(const Clay_StringSlice& source, int start, int length);
	static bool findSliceOffsetFromCursor(
		std::string_view fullText,
		std::string_view needle,
		size_t cursor,
		size_t& outOffset);
	std::vector<SelectionRange> mergedSelectionRanges(const FieldState& field) const;

private:
	FrameInput currentInput_{};
	FrameInput previousInput_{};
	std::unordered_map<std::string, FieldState> fieldsById_{};
	std::string primaryFieldId_{};
	UiConfig::InputManagerConfig config_{};
	const ::FontManager* fontManager_ = nullptr;
	float pointsToPixelsScale_ = 96.0f / 72.0f;
	KeyRepeatState leftKeyRepeat_{};
	KeyRepeatState rightKeyRepeat_{};
	KeyRepeatState backspaceKeyRepeat_{};
	KeyRepeatState deleteKeyRepeat_{};
	PointerDragState pointerDrag_{};
	double caretBlinkElapsedSeconds_ = 0.0;
	bool caretBlinkResetPending_ = true;
	bool emitCaretsThisFrame_ = true;
	InputFieldFrameOverrides frameOverrides_{};
};

} // namespace FlowUi
