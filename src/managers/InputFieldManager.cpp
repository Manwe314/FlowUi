#include "managers/InputFieldManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <GLFW/glfw3.h>

#include "internal/TextLayoutEngine.hpp"
#include "managers/FontManager.hpp"

namespace {

constexpr float kBoundsEpsilon = 0.5f;
constexpr double kCaretBlinkPeriodSeconds = 1.0;
constexpr double kCaretBlinkVisibleSeconds = 0.5;
constexpr double kKeyRepeatInitialDelaySeconds = 0.35;
constexpr double kKeyRepeatIntervalSeconds = 0.06;

bool boundsContains(const Clay_BoundingBox& outer, const Clay_BoundingBox& inner) {
	return (inner.x + kBoundsEpsilon) >= outer.x &&
		(inner.y + kBoundsEpsilon) >= outer.y &&
		(inner.x + inner.width) <= (outer.x + outer.width + kBoundsEpsilon) &&
		(inner.y + inner.height) <= (outer.y + outer.height + kBoundsEpsilon);
}

bool boundsContainsPoint(const Clay_BoundingBox& bounds, float x, float y) {
	return x >= bounds.x && x <= (bounds.x + bounds.width) &&
		y >= bounds.y && y <= (bounds.y + bounds.height);
}

bool appendUtf8Codepoint(std::string& out, char32_t codepoint) {
	if (codepoint > 0x10FFFFu) {
		return false;
	}
	if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) {
		return false;
	}

	if (codepoint <= 0x7Fu) {
		out.push_back(static_cast<char>(codepoint));
		return true;
	}
	if (codepoint <= 0x7FFu) {
		out.push_back(static_cast<char>(0xC0u | ((codepoint >> 6u) & 0x1Fu)));
		out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
		return true;
	}
	if (codepoint <= 0xFFFFu) {
		out.push_back(static_cast<char>(0xE0u | ((codepoint >> 12u) & 0x0Fu)));
		out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
		return true;
	}

	out.push_back(static_cast<char>(0xF0u | ((codepoint >> 18u) & 0x07u)));
	out.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
	out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
	out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
	return true;
}

} // namespace

namespace FlowUi {

void InputFieldManager::setConfig(const InputManagerConfig& config) {
	config_ = config;
}

void InputFieldManager::setFontManager(const ::FontManager* fontManager, float pointsToPixelsScale) {
	fontManager_ = fontManager;
	pointsToPixelsScale_ = std::max(pointsToPixelsScale, 1.0e-6f);
}

void InputFieldManager::markCaretBlinkReset() {
	caretBlinkResetPending_ = true;
	emitCaretsThisFrame_ = true;
}

void InputFieldManager::updateCaretBlinkVisibility(bool hasAnyActiveCaret) {
	if (!hasAnyActiveCaret) {
		caretBlinkElapsedSeconds_ = 0.0;
		caretBlinkResetPending_ = false;
		emitCaretsThisFrame_ = true;
		return;
	}

	if (caretBlinkResetPending_) {
		caretBlinkElapsedSeconds_ = 0.0;
		caretBlinkResetPending_ = false;
		emitCaretsThisFrame_ = true;
		return;
	}

	const double dt = std::max(0.0, currentInput_.dt);
	caretBlinkElapsedSeconds_ += dt;

	const double period = std::max(kCaretBlinkPeriodSeconds, 1.0e-6);
	const double visibleDuration = std::clamp(kCaretBlinkVisibleSeconds, 0.0, period);
	const double phase = std::fmod(caretBlinkElapsedSeconds_, period);
	emitCaretsThisFrame_ = phase < visibleDuration;
}

bool InputFieldManager::shouldTriggerActionWithRepeat(int key, KeyRepeatState& state) {
	const bool isDown = keyDown(currentInput_, key);
	if (!isDown) {
		state.wasDown = false;
		state.repeatCountdownSeconds = 0.0;
		return false;
	}

	if (!state.wasDown) {
		state.wasDown = true;
		state.repeatCountdownSeconds = std::max(kKeyRepeatInitialDelaySeconds, 0.0);
		return true;
	}

	const double dt = std::max(0.0, currentInput_.dt);
	state.repeatCountdownSeconds -= dt;
	if (state.repeatCountdownSeconds > 0.0) {
		return false;
	}

	state.repeatCountdownSeconds += std::max(kKeyRepeatIntervalSeconds, 1.0e-6);
	if (state.repeatCountdownSeconds <= 0.0) {
		state.repeatCountdownSeconds = std::max(kKeyRepeatIntervalSeconds, 1.0e-6);
	}
	return true;
}

void InputFieldManager::beginFrame(const FrameInput& currentInput, const FrameInput& previousInput) {
	previousInput_ = previousInput;
	currentInput_ = currentInput;

	for (auto& [_, field] : fieldsById_) {
		field.touchedThisFrame = false;
	}

	applyKeyboardEdits();
}

Clay_RenderCommandArray InputFieldManager::endFrame(const Clay_RenderCommandArray& renderCommands) {
	frameOverrides_.rects.clear();
	frameOverrides_.textColorOverrides.clear();

	const bool isPrimaryPointerDown = currentInput_.mouseDown[0];
	const bool wasPrimaryPointerDown = previousInput_.mouseDown[0];
	const bool pointerPressed = isPrimaryPointerDown && !wasPrimaryPointerDown;
	const bool pointerReleased = !isPrimaryPointerDown && wasPrimaryPointerDown;
	if (pointerReleased) {
		pointerDrag_ = PointerDragState{};
	}

	if (!primaryFieldId_.empty()) {
		const auto primaryIt = fieldsById_.find(primaryFieldId_);
		if (primaryIt == fieldsById_.end() || !primaryIt->second.touchedThisFrame) {
			for (auto& [_, field] : fieldsById_) {
				field.carets.clear();
			}
			primaryFieldId_.clear();
		}
	}

	for (auto& [_, field] : fieldsById_) {
		if (!field.touchedThisFrame) {
			continue;
		}
		clampCaretsToText(field);
	}

	struct RuntimeTextSpan {
		int32_t commandIndex = -1;
		Clay_RenderCommand command{};
		size_t startByteOffset = 0u;
		size_t endByteOffset = 0u;
	};

	struct RuntimeFieldState {
		std::string fieldId{};
		FieldState* field = nullptr;
		size_t cursor = 0u;
		bool hasContentBounds = false;
		Clay_BoundingBox contentBounds{};
		bool hasTextBounds = false;
		Clay_BoundingBox textBounds{};
		std::vector<RuntimeTextSpan> textSpans{};
		std::vector<SelectionRange> mergedSelections{};
		std::vector<bool> caretDrawn{};
		bool hasTextCommand = false;
		Clay_RenderCommand lastTextCommand{};
		int32_t lastTextCommandIndex = -1;
	};

	std::vector<RuntimeFieldState> runtimes;
	runtimes.reserve(fieldsById_.size());
	for (auto& [fieldId, field] : fieldsById_) {
		if (!field.touchedThisFrame) {
			continue;
		}

		RuntimeFieldState runtime{};
		runtime.fieldId = fieldId;
		runtime.field = &field;

		if (elementIdIsValid(field.textElementId)) {
			const Clay_ElementData textElementData = Clay_GetElementData(field.textElementId);
			if (textElementData.found) {
				runtime.hasTextBounds = true;
				runtime.textBounds = textElementData.boundingBox;
			}
		}

		if (elementIdIsValid(field.contentElementId)) {
			const Clay_ElementData contentElementData = Clay_GetElementData(field.contentElementId);
			if (contentElementData.found) {
				runtime.hasContentBounds = true;
				runtime.contentBounds = contentElementData.boundingBox;
			}
		}

		runtimes.push_back(std::move(runtime));
	}

	auto findRuntimeByFieldId = [&](std::string_view fieldId) -> RuntimeFieldState* {
		for (RuntimeFieldState& runtime : runtimes) {
			if (runtime.fieldId == fieldId) {
				return &runtime;
			}
		}
		return nullptr;
	};

	if (runtimes.empty()) {
		if (pointerPressed) {
			for (auto& [_, field] : fieldsById_) {
				field.carets.clear();
			}
			primaryFieldId_.clear();
			pointerDrag_ = PointerDragState{};
		}
		updateCaretBlinkVisibility(false);
		return renderCommands;
	}

	if (renderCommands.length > 0 && renderCommands.internalArray) {
		for (int32_t i = 0; i < renderCommands.length; ++i) {
			const Clay_RenderCommand& command = renderCommands.internalArray[i];
			if (command.commandType != CLAY_RENDER_COMMAND_TYPE_TEXT) {
				continue;
			}

			const Clay_TextRenderData& textData = command.renderData.text;
			const Clay_StringSlice sourceSlice = textData.stringContents;
			const int commandByteLength = std::max(0, sourceSlice.length);
			const std::string_view commandTextView(
				sourceSlice.chars ? sourceSlice.chars : "",
				static_cast<size_t>(commandByteLength));

			RuntimeFieldState* matchedRuntime = nullptr;
			for (RuntimeFieldState& runtime : runtimes) {
				if (!runtime.field || !runtime.hasTextBounds) {
					continue;
				}
				if (!boundsContains(runtime.textBounds, command.boundingBox)) {
					continue;
				}

				size_t resolved = std::min(runtime.cursor, runtime.field->text.size());
				if (!commandTextView.empty() &&
					!findSliceOffsetFromCursor(runtime.field->text, commandTextView, runtime.cursor, resolved)) {
					continue;
				}

				matchedRuntime = &runtime;
				break;
			}

			if (!matchedRuntime) {
				continue;
			}

			FieldState& field = *matchedRuntime->field;
			size_t commandStartInField = std::min(matchedRuntime->cursor, field.text.size());
			if (!commandTextView.empty()) {
				size_t resolved = commandStartInField;
				if (findSliceOffsetFromCursor(field.text, commandTextView, matchedRuntime->cursor, resolved)) {
					commandStartInField = resolved;
				}
			}
			const size_t commandEndInField = std::min(
				field.text.size(),
				commandStartInField + commandTextView.size());

			matchedRuntime->cursor = commandEndInField;
			matchedRuntime->hasTextCommand = true;
			matchedRuntime->lastTextCommand = command;
			matchedRuntime->lastTextCommandIndex = i;
			matchedRuntime->textSpans.push_back(RuntimeTextSpan{
				.commandIndex = i,
				.command = command,
				.startByteOffset = commandStartInField,
				.endByteOffset = commandEndInField,
			});
		}
	}

	auto resolvePointerOffsetInRuntime = [&](const RuntimeFieldState& runtime, float mouseX, float mouseY) -> size_t {
		if (!runtime.field) {
			return 0u;
		}
		const FieldState& field = *runtime.field;
		if (runtime.textSpans.empty()) {
			if (field.text.empty()) {
				return 0u;
			}
			if (runtime.hasContentBounds && mouseX > (runtime.contentBounds.x + runtime.contentBounds.width * 0.5f)) {
				return field.text.size();
			}
			return 0u;
		}

		const RuntimeTextSpan* bestSpan = nullptr;
		float bestScore = std::numeric_limits<float>::max();
		for (const RuntimeTextSpan& span : runtime.textSpans) {
			const Clay_BoundingBox& bounds = span.command.boundingBox;
			const float dy = (mouseY < bounds.y)
				? (bounds.y - mouseY)
				: ((mouseY > (bounds.y + bounds.height)) ? (mouseY - (bounds.y + bounds.height)) : 0.0f);
			const float dx = (mouseX < bounds.x)
				? (bounds.x - mouseX)
				: ((mouseX > (bounds.x + bounds.width)) ? (mouseX - (bounds.x + bounds.width)) : 0.0f);
			const float score = dy * 10000.0f + dx;
			if (!bestSpan || score < bestScore ||
				(std::abs(score - bestScore) <= 1.0e-5f && span.commandIndex > bestSpan->commandIndex)) {
				bestSpan = &span;
				bestScore = score;
			}
		}

		if (!bestSpan) {
			return 0u;
		}

		const Clay_TextRenderData& textData = bestSpan->command.renderData.text;
		const Clay_StringSlice sourceSlice = textData.stringContents;
		const int commandByteLength = std::max(0, sourceSlice.length);
		const std::string_view commandTextView(
			sourceSlice.chars ? sourceSlice.chars : "",
			static_cast<size_t>(commandByteLength));
		const float commandX = bestSpan->command.boundingBox.x;
		float commandAdvance = measureTextSlice(sourceSlice, textData);
		if (commandAdvance <= 0.0f) {
			commandAdvance = std::max(0.0f, bestSpan->command.boundingBox.width);
		}

		if (mouseX <= commandX) {
			return clampUtf8Boundary(field.text, bestSpan->startByteOffset);
		}
		if (mouseX >= (commandX + commandAdvance)) {
			return clampUtf8Boundary(field.text, bestSpan->endByteOffset);
		}

		size_t localOffset = 0u;
		float previousX = commandX;
		while (localOffset < commandTextView.size()) {
			const size_t nextOffset = nextUtf8Codepoint(commandTextView, localOffset);
			const Clay_StringSlice nextPrefix = subSlice(sourceSlice, 0, static_cast<int>(nextOffset));
			const float nextX = commandX + measureTextSlice(nextPrefix, textData);
			const float midpoint = 0.5f * (previousX + nextX);
			if (mouseX < midpoint) {
				const size_t resolved = bestSpan->startByteOffset + localOffset;
				return clampUtf8Boundary(field.text, std::min(resolved, bestSpan->endByteOffset));
			}
			localOffset = nextOffset;
			previousX = nextX;
		}

		return clampUtf8Boundary(field.text, bestSpan->endByteOffset);
	};

	if (pointerPressed) {
		RuntimeFieldState* targetRuntime = nullptr;
		float bestArea = std::numeric_limits<float>::max();
		int32_t bestZ = std::numeric_limits<int32_t>::min();
		for (RuntimeFieldState& runtime : runtimes) {
			Clay_BoundingBox hitBounds{};
			bool hasHitBounds = false;
			if (runtime.hasContentBounds) {
				hitBounds = runtime.contentBounds;
				hasHitBounds = true;
			} else if (runtime.hasTextBounds) {
				hitBounds = runtime.textBounds;
				hasHitBounds = true;
			}

			if (!hasHitBounds || !boundsContainsPoint(hitBounds, currentInput_.mouseX, currentInput_.mouseY)) {
				continue;
			}

			const float area = std::max(0.0f, hitBounds.width) * std::max(0.0f, hitBounds.height);
			const int32_t z = runtime.lastTextCommandIndex;
			if (!targetRuntime || area < bestArea || (std::abs(area - bestArea) <= 1.0e-5f && z > bestZ)) {
				targetRuntime = &runtime;
				bestArea = area;
				bestZ = z;
			}
		}

		if (!targetRuntime || !targetRuntime->field) {
			for (auto& [_, field] : fieldsById_) {
				field.carets.clear();
			}
			primaryFieldId_.clear();
			pointerDrag_ = PointerDragState{};
		} else {
			FieldState& field = *targetRuntime->field;
			const size_t hitOffset = resolvePointerOffsetInRuntime(*targetRuntime, currentInput_.mouseX, currentInput_.mouseY);
			for (auto& [_, candidate] : fieldsById_) {
				candidate.carets.clear();
			}
			field.carets = { CaretState{hitOffset, hitOffset} };
			primaryFieldId_ = targetRuntime->fieldId;
			pointerDrag_.active = true;
			pointerDrag_.fieldId = targetRuntime->fieldId;
			pointerDrag_.anchorByteOffset = hitOffset;
			markCaretBlinkReset();
		}
	}

	if (isPrimaryPointerDown && pointerDrag_.active) {
		RuntimeFieldState* dragRuntime = findRuntimeByFieldId(pointerDrag_.fieldId);
		if (!dragRuntime || !dragRuntime->field) {
			pointerDrag_ = PointerDragState{};
		} else {
			FieldState& dragField = *dragRuntime->field;
			const size_t clampedAnchor = clampUtf8Boundary(dragField.text, pointerDrag_.anchorByteOffset);
			const size_t hitOffset = resolvePointerOffsetInRuntime(*dragRuntime, currentInput_.mouseX, currentInput_.mouseY);
			if (dragField.carets.empty()) {
				dragField.carets.push_back(CaretState{clampedAnchor, hitOffset});
				markCaretBlinkReset();
			} else {
				CaretState& caret = dragField.carets.front();
				if (caret.anchorByteOffset != clampedAnchor || caret.headByteOffset != hitOffset) {
					caret.anchorByteOffset = clampedAnchor;
					caret.headByteOffset = hitOffset;
					markCaretBlinkReset();
				}
				if (dragField.carets.size() > 1u) {
					dragField.carets.resize(1u);
				}
			}
			primaryFieldId_ = pointerDrag_.fieldId;
			clampCaretsToText(dragField);
		}
	}

	bool needsOverrides = false;
	bool hasAnyVisibleCaret = false;
	for (RuntimeFieldState& runtime : runtimes) {
		if (!runtime.field) {
			continue;
		}
		clampCaretsToText(*runtime.field);
		runtime.mergedSelections = mergedSelectionRanges(*runtime.field);
		runtime.caretDrawn.assign(runtime.field->carets.size(), false);

		const bool fieldShowsCaret = !runtime.field->config.readOnly && !runtime.field->carets.empty();
		needsOverrides = needsOverrides || fieldShowsCaret || !runtime.mergedSelections.empty();
		hasAnyVisibleCaret = hasAnyVisibleCaret || fieldShowsCaret;
	}

	updateCaretBlinkVisibility(hasAnyVisibleCaret);
	if (!needsOverrides) {
		return renderCommands;
	}

	frameOverrides_.rects.reserve(64u);
	frameOverrides_.textColorOverrides.reserve(32u);
	const int32_t maxInsertionIndex = std::max<int32_t>(0, renderCommands.length);

	auto pushRectOverride = [&](int32_t insertBeforeCommandIndex, float x, float y, float w, float h, const Clay_Color& color) {
		if (w <= 0.0f || h <= 0.0f) {
			return;
		}
		InputFieldRectOverride rectOverride{};
		rectOverride.insertBeforeCommandIndex = std::clamp(insertBeforeCommandIndex, 0, maxInsertionIndex);
		rectOverride.boundingBox = Clay_BoundingBox{
			.x = x,
			.y = y,
			.width = w,
			.height = h,
		};
		rectOverride.color = color;
		frameOverrides_.rects.push_back(rectOverride);
	};

	auto pushCaretRect = [&](int32_t insertBeforeCommandIndex, float caretX, float y, float h) {
		pushRectOverride(
			insertBeforeCommandIndex,
			caretX,
			y,
			std::max(0.0f, config_.caretWidthPx),
			h,
			config_.caretColor);
	};

	auto pushTextColorOverride = [&](int32_t commandIndex, const std::vector<SelectionRange>& localSelections, int commandByteLength) {
		if (localSelections.empty() || commandByteLength <= 0) {
			return;
		}

		InputFieldTextColorOverride textOverride{};
		textOverride.commandIndex = std::clamp(commandIndex, 0, std::max(0, renderCommands.length - 1));
		textOverride.color = config_.highlightedTextColor;
		textOverride.ranges.reserve(localSelections.size());

		for (const SelectionRange& localSelection : localSelections) {
			const size_t start = std::min<size_t>(localSelection.start, static_cast<size_t>(commandByteLength));
			const size_t end = std::min<size_t>(localSelection.end, static_cast<size_t>(commandByteLength));
			if (end <= start) {
				continue;
			}
			textOverride.ranges.push_back(InputFieldTextColorRangeOverride{
				.startByteOffset = start,
				.endByteOffset = end,
			});
		}

		if (textOverride.ranges.empty()) {
			return;
		}

		frameOverrides_.textColorOverrides.push_back(std::move(textOverride));
	};

	for (RuntimeFieldState& runtime : runtimes) {
		if (!runtime.field) {
			continue;
		}

		FieldState& field = *runtime.field;
		for (const RuntimeTextSpan& span : runtime.textSpans) {
			const Clay_RenderCommand& command = span.command;
			const Clay_TextRenderData& textData = command.renderData.text;
			const Clay_StringSlice sourceSlice = textData.stringContents;
			const int commandByteLength = std::max(0, sourceSlice.length);

			std::vector<SelectionRange> localSelections;
			for (const SelectionRange& range : runtime.mergedSelections) {
				const size_t intersectStart = std::max(range.start, span.startByteOffset);
				const size_t intersectEnd = std::min(range.end, span.endByteOffset);
				if (intersectStart >= intersectEnd) {
					continue;
				}
				localSelections.push_back(SelectionRange{
					.start = intersectStart - span.startByteOffset,
					.end = intersectEnd - span.startByteOffset,
				});
			}

			auto emitHighlightRect = [&](int localStart, int localEnd) {
				if (localEnd <= localStart) {
					return;
				}
				const Clay_StringSlice prefixSlice = subSlice(sourceSlice, 0, localStart);
				const Clay_StringSlice selectionSlice = subSlice(sourceSlice, localStart, localEnd - localStart);

				const float x0 = command.boundingBox.x + measureTextSlice(prefixSlice, textData);
				const float selectionWidth = measureTextSlice(selectionSlice, textData);
				if (selectionWidth <= 0.0f) {
					return;
				}

				pushRectOverride(
					span.commandIndex,
					x0,
					command.boundingBox.y,
					selectionWidth,
					command.boundingBox.height,
					config_.highlightBoxColor);
			};

			if (!localSelections.empty() && commandByteLength > 0) {
				pushTextColorOverride(span.commandIndex, localSelections, commandByteLength);
				for (const SelectionRange& localSelection : localSelections) {
					const int selStart = static_cast<int>(std::min<size_t>(localSelection.start, static_cast<size_t>(commandByteLength)));
					const int selEnd = static_cast<int>(std::min<size_t>(localSelection.end, static_cast<size_t>(commandByteLength)));
					if (selEnd <= selStart) {
						continue;
					}
					emitHighlightRect(selStart, selEnd);
				}
			}

			if (emitCaretsThisFrame_ && !field.config.readOnly) {
				const float caretY = command.boundingBox.y - config_.caretHeightOverflowTopPx;
				const float caretH = command.boundingBox.height + config_.caretHeightOverflowTopPx + config_.caretHeightOverflowBottomPx;
				for (size_t caretIndex = 0; caretIndex < field.carets.size(); ++caretIndex) {
					if (caretIndex >= runtime.caretDrawn.size() || runtime.caretDrawn[caretIndex]) {
						continue;
					}

					const size_t caretOffset = clampUtf8Boundary(field.text, field.carets[caretIndex].headByteOffset);
					const bool caretInsideCommand =
						(caretOffset >= span.startByteOffset && caretOffset < span.endByteOffset) ||
						(caretOffset == span.endByteOffset && caretOffset == field.text.size());
					if (!caretInsideCommand) {
						continue;
					}

					const size_t localCaret = std::min(caretOffset - span.startByteOffset, static_cast<size_t>(commandByteLength));
					const Clay_StringSlice caretPrefix = subSlice(sourceSlice, 0, static_cast<int>(localCaret));
					const float caretX = command.boundingBox.x + measureTextSlice(caretPrefix, textData);
					pushCaretRect(span.commandIndex + 1, caretX, caretY, caretH);
					runtime.caretDrawn[caretIndex] = true;
				}
			}
		}
	}

	if (!emitCaretsThisFrame_) {
		std::stable_sort(
			frameOverrides_.rects.begin(),
			frameOverrides_.rects.end(),
			[](const InputFieldRectOverride& a, const InputFieldRectOverride& b) {
				return a.insertBeforeCommandIndex < b.insertBeforeCommandIndex;
			});
		std::stable_sort(
			frameOverrides_.textColorOverrides.begin(),
			frameOverrides_.textColorOverrides.end(),
			[](const InputFieldTextColorOverride& a, const InputFieldTextColorOverride& b) {
				return a.commandIndex < b.commandIndex;
			});
		return renderCommands;
	}

	for (RuntimeFieldState& runtime : runtimes) {
		if (!runtime.field || runtime.field->config.readOnly) {
			continue;
		}

		FieldState& field = *runtime.field;
		for (size_t caretIndex = 0; caretIndex < field.carets.size(); ++caretIndex) {
			if (caretIndex < runtime.caretDrawn.size() && runtime.caretDrawn[caretIndex]) {
				continue;
			}

			if (runtime.hasTextCommand && runtime.lastTextCommand.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
				const Clay_RenderCommand& referenceCommand = runtime.lastTextCommand;
				const Clay_TextRenderData& textData = referenceCommand.renderData.text;
				const float textWidth = measureTextSlice(textData.stringContents, textData);
				const float caretY = referenceCommand.boundingBox.y - config_.caretHeightOverflowTopPx;
				const float caretH = referenceCommand.boundingBox.height + config_.caretHeightOverflowTopPx + config_.caretHeightOverflowBottomPx;
				pushCaretRect(runtime.lastTextCommandIndex + 1, referenceCommand.boundingBox.x + textWidth, caretY, caretH);
				continue;
			}

			if (runtime.hasContentBounds) {
				const float caretY = runtime.contentBounds.y - config_.caretHeightOverflowTopPx;
				const float caretH = runtime.contentBounds.height + config_.caretHeightOverflowTopPx + config_.caretHeightOverflowBottomPx;
				pushCaretRect(maxInsertionIndex, runtime.contentBounds.x, caretY, caretH);
			}
		}
	}

	std::stable_sort(
		frameOverrides_.rects.begin(),
		frameOverrides_.rects.end(),
		[](const InputFieldRectOverride& a, const InputFieldRectOverride& b) {
			return a.insertBeforeCommandIndex < b.insertBeforeCommandIndex;
		});
	std::stable_sort(
		frameOverrides_.textColorOverrides.begin(),
		frameOverrides_.textColorOverrides.end(),
		[](const InputFieldTextColorOverride& a, const InputFieldTextColorOverride& b) {
			return a.commandIndex < b.commandIndex;
		});

	return renderCommands;
}

InputFieldManager::FieldQueryResult InputFieldManager::requestField(const FieldRequest& request) {
	if (request.fieldId.empty()) {
		return {};
	}

	const std::string key(request.fieldId);
	auto [it, inserted] = fieldsById_.try_emplace(key);
	FieldState& field = it->second;
	if (inserted) {
		field.text = std::string(request.initialText);
	}
	field.config = request.config;
	field.textElementId = request.textElementId;
	field.contentElementId = request.contentElementId;
	field.touchedThisFrame = true;
	clampCaretsToText(field);

	FieldQueryResult result{};
	result.text = field.text;
	result.hasPrimaryCaret = !field.carets.empty() && primaryFieldId_ == key;
	for (const CaretState& caret : field.carets) {
		if (caretHasSelection(caret)) {
			result.hasSelection = true;
			break;
		}
	}
	return result;
}

bool InputFieldManager::hasPrimaryFieldFocus() const {
	if (primaryFieldId_.empty()) {
		return false;
	}
	const auto it = fieldsById_.find(primaryFieldId_);
	if (it == fieldsById_.end()) {
		return false;
	}
	return !it->second.carets.empty();
}

std::string_view InputFieldManager::getSelectedText() const {
	if (primaryFieldId_.empty()) {
		return {};
	}
	const auto it = fieldsById_.find(primaryFieldId_);
	if (it == fieldsById_.end()) {
		return {};
	}

	const FieldState& field = it->second;
	size_t selectedStart = field.text.size();
	size_t selectedEnd = field.text.size();
	bool foundSelection = false;
	for (const CaretState& caret : field.carets) {
		if (!caretHasSelection(caret)) {
			continue;
		}
		const size_t start = caretSelectionStart(caret);
		const size_t end = caretSelectionEnd(caret);
		if (!foundSelection || start < selectedStart) {
			selectedStart = start;
			selectedEnd = end;
			foundSelection = true;
		}
	}

	if (!foundSelection || selectedEnd <= selectedStart) {
		return {};
	}
	const std::string_view textView(field.text);
	return textView.substr(selectedStart, selectedEnd - selectedStart);
}

bool InputFieldManager::insertTextAtPrimaryCaret(std::string_view utf8Text) {
	if (utf8Text.empty() || primaryFieldId_.empty()) {
		return false;
	}

	const auto it = fieldsById_.find(primaryFieldId_);
	if (it == fieldsById_.end()) {
		return false;
	}

	FieldState& field = it->second;
	if (field.config.readOnly || field.carets.empty()) {
		return false;
	}

	const std::string beforeText = field.text;
	const std::vector<CaretState> beforeCarets = field.carets;
	applyTextInsertion(field, utf8Text);

	bool caretsChanged = beforeCarets.size() != field.carets.size();
	if (!caretsChanged) {
		for (size_t i = 0; i < beforeCarets.size(); ++i) {
			if (beforeCarets[i].anchorByteOffset != field.carets[i].anchorByteOffset ||
				beforeCarets[i].headByteOffset != field.carets[i].headByteOffset) {
				caretsChanged = true;
				break;
			}
		}
	}

	const bool changed = (field.text != beforeText) || caretsChanged;
	if (changed) {
		markCaretBlinkReset();
	}
	return changed;
}

void InputFieldManager::requestCaret(std::string_view fieldId, CaretRequestKind kind) {
	if (kind == CaretRequestKind::ClearAll) {
		for (auto& [_, field] : fieldsById_) {
			field.carets.clear();
		}
		primaryFieldId_.clear();
		pointerDrag_ = PointerDragState{};
		markCaretBlinkReset();
		return;
	}

	if (fieldId.empty()) {
		return;
	}

	const std::string key(fieldId);
	auto& field = fieldsById_[key];
	clampCaretsToText(field);

	if (kind == CaretRequestKind::SetPrimary) {
		const bool primaryWasSameField = (primaryFieldId_ == key);
		const bool hadExistingCaret = !field.carets.empty();
		CaretState preservedCaret{};
		if (hadExistingCaret) {
			preservedCaret = field.carets.front();
		}

		for (auto& [_, entry] : fieldsById_) {
			entry.carets.clear();
		}
		if (hadExistingCaret) {
			field.carets = { preservedCaret };
		} else {
			const size_t offset = field.text.size();
			field.carets = { CaretState{offset, offset} };
		}
		primaryFieldId_ = key;

		// Preserve ongoing pointer drag when SetPrimary targets the same field.
		if (pointerDrag_.active && pointerDrag_.fieldId != key) {
			pointerDrag_ = PointerDragState{};
		}

		if (!primaryWasSameField || !hadExistingCaret) {
			markCaretBlinkReset();
		}
		return;
	}

	size_t offset = field.text.size();
	if (!field.carets.empty()) {
		offset = field.carets.back().headByteOffset;
	}
	field.carets.push_back(CaretState{offset, offset});
	if (primaryFieldId_.empty()) {
		primaryFieldId_ = key;
	}
	pointerDrag_ = PointerDragState{};
	markCaretBlinkReset();
}

bool InputFieldManager::removeField(std::string_view fieldId) {
	if (fieldId.empty()) {
		return false;
	}

	const std::string key(fieldId);
	const auto it = fieldsById_.find(key);
	if (it == fieldsById_.end()) {
		return false;
	}
	fieldsById_.erase(it);

	if (primaryFieldId_ == key) {
		primaryFieldId_.clear();
		for (const auto& [candidateId, field] : fieldsById_) {
			if (!field.carets.empty()) {
				primaryFieldId_ = candidateId;
				break;
			}
		}
	}
	if (pointerDrag_.fieldId == key) {
		pointerDrag_ = PointerDragState{};
	}
	return true;
}

void InputFieldManager::clear() {
	fieldsById_.clear();
	primaryFieldId_.clear();
	previousInput_ = FrameInput{};
	currentInput_ = FrameInput{};
	leftKeyRepeat_ = KeyRepeatState{};
	rightKeyRepeat_ = KeyRepeatState{};
	backspaceKeyRepeat_ = KeyRepeatState{};
	deleteKeyRepeat_ = KeyRepeatState{};
	pointerDrag_ = PointerDragState{};
	caretBlinkElapsedSeconds_ = 0.0;
	caretBlinkResetPending_ = true;
	emitCaretsThisFrame_ = true;
	frameOverrides_.rects.clear();
	frameOverrides_.textColorOverrides.clear();
}

void InputFieldManager::applyKeyboardEdits() {
	bool hasAnyCarets = false;
	for (const auto& [_, field] : fieldsById_) {
		if (!field.carets.empty()) {
			hasAnyCarets = true;
			break;
		}
	}

	bool leftPressed = false;
	bool rightPressed = false;
	bool backspacePressed = false;
	bool deletePressed = false;
	if (hasAnyCarets) {
		leftPressed = shouldTriggerActionWithRepeat(GLFW_KEY_LEFT, leftKeyRepeat_);
		rightPressed = shouldTriggerActionWithRepeat(GLFW_KEY_RIGHT, rightKeyRepeat_);
		backspacePressed = shouldTriggerActionWithRepeat(GLFW_KEY_BACKSPACE, backspaceKeyRepeat_);
		deletePressed = shouldTriggerActionWithRepeat(GLFW_KEY_DELETE, deleteKeyRepeat_);
	} else {
		leftKeyRepeat_ = KeyRepeatState{};
		rightKeyRepeat_ = KeyRepeatState{};
		backspaceKeyRepeat_ = KeyRepeatState{};
		deleteKeyRepeat_ = KeyRepeatState{};
	}

	const bool selecting = currentInput_.shift;
	bool shouldResetCaretBlink = false;

	for (auto& [_, field] : fieldsById_) {
		if (field.carets.empty()) {
			continue;
		}
		clampCaretsToText(field);

		if (field.config.allowArrowNavigation) {
			if (leftPressed) {
				moveCaretsHorizontal(field, -1, selecting);
				shouldResetCaretBlink = true;
			}
			if (rightPressed) {
				moveCaretsHorizontal(field, +1, selecting);
				shouldResetCaretBlink = true;
			}
		}

		if (field.config.readOnly) {
			continue;
		}

		if (backspacePressed) {
			applyDelete(field, true);
			shouldResetCaretBlink = true;
		}
		if (deletePressed) {
			applyDelete(field, false);
			shouldResetCaretBlink = true;
		}

		const std::string textInput = encodeTextInput(currentInput_, field.config.allowNewline);
		if (!textInput.empty()) {
			applyTextInsertion(field, textInput);
			shouldResetCaretBlink = true;
		}
	}

	if (shouldResetCaretBlink) {
		markCaretBlinkReset();
	}
}

void InputFieldManager::applyTextInsertion(FieldState& field, std::string_view utf8Text) {
	if (utf8Text.empty() || field.carets.empty()) {
		return;
	}
	clampCaretsToText(field);

	std::vector<size_t> caretOrder(field.carets.size());
	for (size_t i = 0; i < caretOrder.size(); ++i) {
		caretOrder[i] = i;
	}

	std::sort(caretOrder.begin(), caretOrder.end(), [&field](size_t a, size_t b) {
		const size_t ah = field.carets[a].headByteOffset;
		const size_t bh = field.carets[b].headByteOffset;
		if (ah != bh) {
			return ah > bh;
		}
		return a > b;
	});

	for (size_t caretIndex : caretOrder) {
		if (caretIndex >= field.carets.size()) {
			continue;
		}

		CaretState& caret = field.carets[caretIndex];
		if (caretHasSelection(caret)) {
			eraseRange(field, caretSelectionStart(caret), caretSelectionEnd(caret), caretIndex);
		}

		if (field.text.size() + utf8Text.size() > field.config.maxBytes) {
			continue;
		}

		const size_t insertOffset = std::min(caret.headByteOffset, field.text.size());
		field.text.insert(insertOffset, utf8Text);

		for (size_t i = 0; i < field.carets.size(); ++i) {
			CaretState& candidate = field.carets[i];
			if (i == caretIndex) {
				candidate.anchorByteOffset = insertOffset + utf8Text.size();
				candidate.headByteOffset = insertOffset + utf8Text.size();
				continue;
			}

			if (candidate.anchorByteOffset >= insertOffset) {
				candidate.anchorByteOffset += utf8Text.size();
			}
			if (candidate.headByteOffset >= insertOffset) {
				candidate.headByteOffset += utf8Text.size();
			}
		}
	}
}

void InputFieldManager::applyDelete(FieldState& field, bool backspace) {
	if (field.carets.empty()) {
		return;
	}
	clampCaretsToText(field);

	std::vector<size_t> caretOrder(field.carets.size());
	for (size_t i = 0; i < caretOrder.size(); ++i) {
		caretOrder[i] = i;
	}

	std::sort(caretOrder.begin(), caretOrder.end(), [&field](size_t a, size_t b) {
		const size_t ae = caretSelectionEnd(field.carets[a]);
		const size_t be = caretSelectionEnd(field.carets[b]);
		if (ae != be) {
			return ae > be;
		}
		return a > b;
	});

	for (size_t caretIndex : caretOrder) {
		if (caretIndex >= field.carets.size()) {
			continue;
		}

		const CaretState caret = field.carets[caretIndex];
		size_t eraseStart = 0u;
		size_t eraseEnd = 0u;

		if (caretHasSelection(caret)) {
			eraseStart = caretSelectionStart(caret);
			eraseEnd = caretSelectionEnd(caret);
		} else if (backspace) {
			eraseEnd = caret.headByteOffset;
			eraseStart = prevUtf8Codepoint(field.text, eraseEnd);
		} else {
			eraseStart = caret.headByteOffset;
			eraseEnd = nextUtf8Codepoint(field.text, eraseStart);
		}

		if (eraseStart >= eraseEnd) {
			continue;
		}
		eraseRange(field, eraseStart, eraseEnd, caretIndex);
	}
}

void InputFieldManager::moveCaretsHorizontal(FieldState& field, int direction, bool selecting) {
	if (field.carets.empty()) {
		return;
	}
	clampCaretsToText(field);

	for (CaretState& caret : field.carets) {
		if (!selecting && caretHasSelection(caret)) {
			const size_t collapsed = (direction < 0) ? caretSelectionStart(caret) : caretSelectionEnd(caret);
			caret.anchorByteOffset = collapsed;
			caret.headByteOffset = collapsed;
			continue;
		}

		const size_t currentHead = caret.headByteOffset;
		const size_t target = (direction < 0)
			? prevUtf8Codepoint(field.text, currentHead)
			: nextUtf8Codepoint(field.text, currentHead);

		if (selecting) {
			caret.headByteOffset = target;
		} else {
			caret.anchorByteOffset = target;
			caret.headByteOffset = target;
		}
	}
}

void InputFieldManager::clampCaretsToText(FieldState& field) const {
	const std::string_view textView(field.text);
	for (CaretState& caret : field.carets) {
		caret.anchorByteOffset = clampUtf8Boundary(textView, caret.anchorByteOffset);
		caret.headByteOffset = clampUtf8Boundary(textView, caret.headByteOffset);
	}
}

void InputFieldManager::eraseRange(FieldState& field, size_t start, size_t end, size_t sourceCaretIndex) {
	const size_t clampedStart = std::min(start, field.text.size());
	const size_t clampedEnd = std::min(end, field.text.size());
	if (clampedStart >= clampedEnd) {
		return;
	}

	const size_t removedBytes = clampedEnd - clampedStart;
	field.text.erase(clampedStart, removedBytes);

	const auto adjustOffset = [clampedStart, clampedEnd, removedBytes](size_t offset) -> size_t {
		if (offset <= clampedStart) {
			return offset;
		}
		if (offset >= clampedEnd) {
			return offset - removedBytes;
		}
		return clampedStart;
	};

	for (CaretState& caret : field.carets) {
		caret.anchorByteOffset = adjustOffset(caret.anchorByteOffset);
		caret.headByteOffset = adjustOffset(caret.headByteOffset);
	}

	if (sourceCaretIndex < field.carets.size()) {
		field.carets[sourceCaretIndex].anchorByteOffset = clampedStart;
		field.carets[sourceCaretIndex].headByteOffset = clampedStart;
	}
}

size_t InputFieldManager::clampUtf8Boundary(std::string_view text, size_t offset) {
	size_t clamped = std::min(offset, text.size());
	while (clamped > 0 && clamped < text.size() && isUtf8ContinuationByte(text[clamped])) {
		--clamped;
	}
	return clamped;
}

size_t InputFieldManager::nextUtf8Codepoint(std::string_view text, size_t offset) {
	size_t cursor = clampUtf8Boundary(text, offset);
	if (cursor >= text.size()) {
		return text.size();
	}
	++cursor;
	while (cursor < text.size() && isUtf8ContinuationByte(text[cursor])) {
		++cursor;
	}
	return cursor;
}

size_t InputFieldManager::prevUtf8Codepoint(std::string_view text, size_t offset) {
	size_t cursor = clampUtf8Boundary(text, offset);
	if (cursor == 0u) {
		return 0u;
	}
	--cursor;
	while (cursor > 0u && isUtf8ContinuationByte(text[cursor])) {
		--cursor;
	}
	return cursor;
}

bool InputFieldManager::isUtf8ContinuationByte(char c) {
	return (static_cast<unsigned char>(c) & 0xC0u) == 0x80u;
}

bool InputFieldManager::keyPressedThisFrame(const FrameInput& currentInput, const FrameInput& previousInput, int key) {
	return keyDown(currentInput, key) && !keyDown(previousInput, key);
}

bool InputFieldManager::keyDown(const FrameInput& input, int key) {
	if (key < 0 || key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) {
		return false;
	}
	return input.keyDown[static_cast<size_t>(key)];
}

bool InputFieldManager::caretHasSelection(const CaretState& caret) {
	return caret.anchorByteOffset != caret.headByteOffset;
}

size_t InputFieldManager::caretSelectionStart(const CaretState& caret) {
	return std::min(caret.anchorByteOffset, caret.headByteOffset);
}

size_t InputFieldManager::caretSelectionEnd(const CaretState& caret) {
	return std::max(caret.anchorByteOffset, caret.headByteOffset);
}

std::string InputFieldManager::encodeTextInput(const FrameInput& input, bool allowNewline) {
	std::string utf8Text;
	for (char32_t codepoint : input.text) {
		if (codepoint == U'\r' || codepoint == U'\n') {
			if (allowNewline) {
				utf8Text.push_back('\n');
			}
			continue;
		}

		if (codepoint < 32u || codepoint == 127u) {
			continue;
		}
		appendUtf8Codepoint(utf8Text, codepoint);
	}
	return utf8Text;
}

bool InputFieldManager::elementIdIsValid(const Clay_ElementId& id) {
	return id.id != 0u;
}

float InputFieldManager::measureTextSlice(const Clay_StringSlice& text, const Clay_TextRenderData& textData) const {
	if (!text.chars || text.length <= 0) {
		return 0.0f;
	}

	const FontManager::FontFaceData* fontFace = FlowUi::detail::ResolveFontFace(fontManager_, textData.fontId);
	if (!fontFace) {
		const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, textData.fontSize)) * pointsToPixelsScale_;
		return static_cast<float>(text.length) * fallbackEmPixels * 0.5f;
	}

	const FlowUi::detail::TextLayoutResult layoutResult = FlowUi::detail::LayoutTextLine(
		FlowUi::detail::TextLayoutRequest{
			.text = text,
			.fontFace = fontFace,
			.pointsToPixelsScale = pointsToPixelsScale_,
			.fontSize = textData.fontSize,
			.letterSpacing = textData.letterSpacing,
			.lineOriginX = 0.0f,
			.lineOriginY = 0.0f,
			.emitGlyphQuads = false,
		},
		[](const FlowUi::detail::TextLayoutGlyphQuad&) {});

	if (!layoutResult.success) {
		return 0.0f;
	}
	return std::max(layoutResult.measuredAdvance, layoutResult.measuredWidth);
}

Clay_StringSlice InputFieldManager::subSlice(const Clay_StringSlice& source, int start, int length) {
	Clay_StringSlice out{};
	if (!source.chars || source.length <= 0) {
		return out;
	}

	const int clampedStart = std::clamp(start, 0, source.length);
	const int clampedLength = std::clamp(length, 0, source.length - clampedStart);
	out.chars = source.chars + clampedStart;
	out.length = clampedLength;
	out.baseChars = source.baseChars;
	return out;
}

bool InputFieldManager::findSliceOffsetFromCursor(
	std::string_view fullText,
	std::string_view needle,
	size_t cursor,
	size_t& outOffset) {
	const size_t clampedCursor = std::min(cursor, fullText.size());
	if (needle.empty()) {
		outOffset = clampedCursor;
		return true;
	}

	if (clampedCursor + needle.size() <= fullText.size() &&
		std::memcmp(fullText.data() + clampedCursor, needle.data(), needle.size()) == 0) {
		outOffset = clampedCursor;
		return true;
	}

	const size_t found = fullText.find(needle, clampedCursor);
	if (found != std::string_view::npos) {
		outOffset = found;
		return true;
	}

	return false;
}

std::vector<InputFieldManager::SelectionRange> InputFieldManager::mergedSelectionRanges(const FieldState& field) const {
	std::vector<SelectionRange> ranges;
	ranges.reserve(field.carets.size());

	for (const CaretState& caret : field.carets) {
		if (!caretHasSelection(caret)) {
			continue;
		}
		ranges.push_back(SelectionRange{
			.start = caretSelectionStart(caret),
			.end = caretSelectionEnd(caret),
		});
	}

	if (ranges.empty()) {
		return ranges;
	}

	std::sort(ranges.begin(), ranges.end(), [](const SelectionRange& a, const SelectionRange& b) {
		if (a.start != b.start) {
			return a.start < b.start;
		}
		return a.end < b.end;
	});

	std::vector<SelectionRange> merged;
	merged.reserve(ranges.size());
	merged.push_back(ranges.front());
	for (size_t i = 1; i < ranges.size(); ++i) {
		SelectionRange& tail = merged.back();
		if (ranges[i].start <= tail.end) {
			tail.end = std::max(tail.end, ranges[i].end);
		} else {
			merged.push_back(ranges[i]);
		}
	}
	return merged;
}

} // namespace FlowUi
