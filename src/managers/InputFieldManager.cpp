#include "managers/InputFieldManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <GLFW/glfw3.h>

#include "internal/TextLayoutEngine.hpp"
#include "internal/ManagerStorage/InputFieldManagerState.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"

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

namespace manager_storage = detail::manager_storage;
namespace key_storage = detail::managerStorage;
namespace storage = detail::storage;
namespace field_key = detail::input_field;

void InputFieldManager::init(
	storage::IStorageSystem& storageSystem,
	WindowId window,
	const InputManagerConfig& config,
	float pointsToPixelsScale) {
	if (storage_) throw std::logic_error("InputFieldManager is already initialized.");
	const storage::StringId name = storageSystem.intern("flowui.input.root");
	const storage::ResourceKey key{storage::ResourceDomain::Input, name, window};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::InputFieldManagerState>(
		storageSystem, key, storage::ResourceKind::InputField, name);
	storage_ = &storageSystem;
	window_ = window;
	stateHandle_ = handle.packed();
	state_ = manager_storage::state<manager_storage::InputFieldManagerState>(
		storage_, handle, storage::ResourceKind::InputField);
	if (!state_) {
		destroy();
		throw std::runtime_error("InputFieldManager storage record publication failed.");
	}
	state_->config = config;
	state_->pointsToPixelsScale = std::max(pointsToPixelsScale, 1.0e-6f);
}

void InputFieldManager::destroy() noexcept {
	if (storage_) {
		try {
			const storage::StringId name = storage_->intern("flowui.input.root");
			(void)storage_->removeManagerRecord(
				storage::ResourceKey{storage::ResourceDomain::Input, name, window_},
				storage::ResourceKind::InputField);
		} catch (...) {
		}
	}
	state_ = nullptr;
	stateHandle_ = 0;
	window_ = InvalidWindowId;
	storage_ = nullptr;
}

const detail::InputFieldFrameOverrides& InputFieldManager::frameOverrides() const {
	if (!state_) throw std::logic_error("InputFieldManager is not attached to window storage.");
	return state_->frameOverrides;
}

void InputFieldManager::setConfig(const InputManagerConfig& config) {
	state_->config = config;
}

void InputFieldManager::setFontFrameView(
	const manager_storage::FontFrameView& fontView,
	float pointsToPixelsScale) {
	state_->fontView = fontView;
	state_->pointsToPixelsScale = std::max(pointsToPixelsScale, 1.0e-6f);
}

void InputFieldManager::markCaretBlinkReset() {
	state_->caretBlinkResetPending = true;
	state_->emitCaretsThisFrame = true;
}

void InputFieldManager::updateCaretBlinkVisibility(bool hasAnyActiveCaret) {
	if (!hasAnyActiveCaret) {
		state_->caretBlinkElapsedSeconds = 0.0;
		state_->caretBlinkResetPending = false;
		state_->emitCaretsThisFrame = true;
		return;
	}

	if (state_->caretBlinkResetPending) {
		state_->caretBlinkElapsedSeconds = 0.0;
		state_->caretBlinkResetPending = false;
		state_->emitCaretsThisFrame = true;
		return;
	}

	const double dt = std::max(0.0, state_->currentInput.dt);
	state_->caretBlinkElapsedSeconds += dt;

	const double period = std::max(kCaretBlinkPeriodSeconds, 1.0e-6);
	const double visibleDuration = std::clamp(kCaretBlinkVisibleSeconds, 0.0, period);
	const double phase = std::fmod(state_->caretBlinkElapsedSeconds, period);
	state_->emitCaretsThisFrame = phase < visibleDuration;
}

bool InputFieldManager::shouldTriggerActionWithRepeat(int key, KeyRepeatState& state) {
	const bool isDown = keyDown(state_->currentInput, key);
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

	const double dt = std::max(0.0, state_->currentInput.dt);
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
	state_->previousInput = previousInput;
	state_->currentInput = currentInput;
	if (state_->currentTouchEpoch == std::numeric_limits<uint64_t>::max()) {
		for (auto& [_, field] : state_->fieldsById) field.lastTouchedEpoch = 0;
		state_->currentTouchEpoch = 1;
	} else {
		++state_->currentTouchEpoch;
	}

	applyKeyboardEdits();
	state_->dirty = true;
}

Clay_RenderCommandArray InputFieldManager::endFrame(const Clay_RenderCommandArray& renderCommands) {
	const auto publishMutation = [this] {
		if (!state_->dirty) return;
		storage_->noteManagerMutation(window_);
		state_->dirty = false;
	};
	state_->frameOverrides.rects.clear();
	state_->frameOverrides.textColorOverrides.clear();

	const bool isPrimaryPointerDown = state_->currentInput.mouseDown[0];
	const bool wasPrimaryPointerDown = state_->previousInput.mouseDown[0];
	const bool pointerPressed = isPrimaryPointerDown && !wasPrimaryPointerDown;
	const bool pointerReleased = !isPrimaryPointerDown && wasPrimaryPointerDown;
	if (pointerReleased) {
		state_->pointerDrag = PointerDragState{};
	}

	if (state_->primaryFieldId) {
		const auto primaryIt = state_->fieldsById.find(state_->primaryFieldId);
		if (primaryIt == state_->fieldsById.end() ||
			primaryIt->second.lastTouchedEpoch != state_->currentTouchEpoch) {
			for (auto& [_, field] : state_->fieldsById) {
				field.carets.clear();
			}
			state_->primaryFieldId = {};
		}
	}

	for (auto& [_, field] : state_->fieldsById) {
		if (field.lastTouchedEpoch != state_->currentTouchEpoch) {
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
		field_key::InputFieldKey fieldId{};
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
	runtimes.reserve(state_->fieldsById.size());
	for (auto& [fieldId, field] : state_->fieldsById) {
		if (field.lastTouchedEpoch != state_->currentTouchEpoch) {
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

	auto findRuntimeByFieldId = [&](field_key::InputFieldKey fieldId) -> RuntimeFieldState* {
		for (RuntimeFieldState& runtime : runtimes) {
			if (runtime.fieldId == fieldId) {
				return &runtime;
			}
		}
		return nullptr;
	};

	if (runtimes.empty()) {
		if (pointerPressed) {
			for (auto& [_, field] : state_->fieldsById) {
				field.carets.clear();
			}
			state_->primaryFieldId = {};
			state_->pointerDrag = PointerDragState{};
		}
		updateCaretBlinkVisibility(false);
			publishMutation();
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

			if (!hasHitBounds || !boundsContainsPoint(hitBounds, state_->currentInput.mouseX, state_->currentInput.mouseY)) {
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
			for (auto& [_, field] : state_->fieldsById) {
				field.carets.clear();
			}
			state_->primaryFieldId = {};
			state_->pointerDrag = PointerDragState{};
		} else {
			FieldState& field = *targetRuntime->field;
			const size_t hitOffset = resolvePointerOffsetInRuntime(*targetRuntime, state_->currentInput.mouseX, state_->currentInput.mouseY);
			for (auto& [_, candidate] : state_->fieldsById) {
				candidate.carets.clear();
			}
			field.carets = { CaretState{hitOffset, hitOffset} };
			state_->primaryFieldId = targetRuntime->fieldId;
			state_->pointerDrag.active = true;
			state_->pointerDrag.fieldId = targetRuntime->fieldId;
			state_->pointerDrag.anchorByteOffset = hitOffset;
			markCaretBlinkReset();
		}
	}

	if (isPrimaryPointerDown && state_->pointerDrag.active) {
		RuntimeFieldState* dragRuntime = findRuntimeByFieldId(state_->pointerDrag.fieldId);
		if (!dragRuntime || !dragRuntime->field) {
			state_->pointerDrag = PointerDragState{};
		} else {
			FieldState& dragField = *dragRuntime->field;
			const size_t clampedAnchor = clampUtf8Boundary(dragField.text, state_->pointerDrag.anchorByteOffset);
			const size_t hitOffset = resolvePointerOffsetInRuntime(*dragRuntime, state_->currentInput.mouseX, state_->currentInput.mouseY);
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
			state_->primaryFieldId = state_->pointerDrag.fieldId;
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
		publishMutation();
		return renderCommands;
	}

	state_->frameOverrides.rects.reserve(64u);
	state_->frameOverrides.textColorOverrides.reserve(32u);
	const int32_t maxInsertionIndex = std::max<int32_t>(0, renderCommands.length);

	auto pushRectOverride = [&](int32_t insertBeforeCommandIndex, float x, float y, float w, float h, const Clay_Color& color) {
		if (w <= 0.0f || h <= 0.0f) {
			return;
		}
		detail::InputFieldRectOverride rectOverride{};
		rectOverride.insertBeforeCommandIndex = std::clamp(insertBeforeCommandIndex, 0, maxInsertionIndex);
		rectOverride.boundingBox = Clay_BoundingBox{
			.x = x,
			.y = y,
			.width = w,
			.height = h,
		};
		rectOverride.color = color;
		state_->frameOverrides.rects.push_back(rectOverride);
	};

	auto pushCaretRect = [&](int32_t insertBeforeCommandIndex, float caretX, float y, float h) {
		pushRectOverride(
			insertBeforeCommandIndex,
			caretX,
			y,
			std::max(0.0f, state_->config.caretWidthPx),
			h,
			state_->config.caretColor);
	};

	auto pushTextColorOverride = [&](int32_t commandIndex, const std::vector<SelectionRange>& localSelections, int commandByteLength) {
		if (localSelections.empty() || commandByteLength <= 0) {
			return;
		}

		detail::InputFieldTextColorOverride textOverride{};
		textOverride.commandIndex = std::clamp(commandIndex, 0, std::max(0, renderCommands.length - 1));
		textOverride.color = state_->config.highlightedTextColor;
		textOverride.ranges.reserve(localSelections.size());

		for (const SelectionRange& localSelection : localSelections) {
			const size_t start = std::min<size_t>(localSelection.start, static_cast<size_t>(commandByteLength));
			const size_t end = std::min<size_t>(localSelection.end, static_cast<size_t>(commandByteLength));
			if (end <= start) {
				continue;
			}
			textOverride.ranges.push_back(detail::InputFieldTextColorRangeOverride{
				.startByteOffset = start,
				.endByteOffset = end,
			});
		}

		if (textOverride.ranges.empty()) {
			return;
		}

		state_->frameOverrides.textColorOverrides.push_back(std::move(textOverride));
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
			if (command.boundingBox.height > 0.0f) {
				field.fallbackMetrics.valid = true;
				field.fallbackMetrics.height = command.boundingBox.height;
			}

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
					state_->config.highlightBoxColor);
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

			if (state_->emitCaretsThisFrame && !field.config.readOnly) {
				const float caretY = command.boundingBox.y - state_->config.caretHeightOverflowTopPx;
				const float caretH = command.boundingBox.height + state_->config.caretHeightOverflowTopPx + state_->config.caretHeightOverflowBottomPx;
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

	if (!state_->emitCaretsThisFrame) {
		std::stable_sort(
			state_->frameOverrides.rects.begin(),
			state_->frameOverrides.rects.end(),
			[](const detail::InputFieldRectOverride& a, const detail::InputFieldRectOverride& b) {
				return a.insertBeforeCommandIndex < b.insertBeforeCommandIndex;
			});
		std::stable_sort(
			state_->frameOverrides.textColorOverrides.begin(),
			state_->frameOverrides.textColorOverrides.end(),
			[](const detail::InputFieldTextColorOverride& a, const detail::InputFieldTextColorOverride& b) {
				return a.commandIndex < b.commandIndex;
			});
		publishMutation();
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
				const float caretY = referenceCommand.boundingBox.y - state_->config.caretHeightOverflowTopPx;
				const float caretH = referenceCommand.boundingBox.height + state_->config.caretHeightOverflowTopPx + state_->config.caretHeightOverflowBottomPx;
				pushCaretRect(runtime.lastTextCommandIndex + 1, referenceCommand.boundingBox.x + textWidth, caretY, caretH);
				continue;
			}

			const bool hasFallbackBounds = runtime.hasTextBounds || runtime.hasContentBounds;
			if (hasFallbackBounds) {
				const Clay_BoundingBox fallbackBounds =
					runtime.hasTextBounds ? runtime.textBounds : runtime.contentBounds;
				float fallbackHeight = fallbackBounds.height;
				if (field.fallbackMetrics.valid && field.fallbackMetrics.height > 0.0f) {
					fallbackHeight = field.fallbackMetrics.height;
				} else if (fallbackHeight <= 0.0f && runtime.hasContentBounds) {
					fallbackHeight = runtime.contentBounds.height;
				}
				const float caretY = fallbackBounds.y - state_->config.caretHeightOverflowTopPx;
				const float caretH = fallbackHeight + state_->config.caretHeightOverflowTopPx + state_->config.caretHeightOverflowBottomPx;
				pushCaretRect(maxInsertionIndex, fallbackBounds.x, caretY, caretH);
			}
		}
	}

	std::stable_sort(
		state_->frameOverrides.rects.begin(),
		state_->frameOverrides.rects.end(),
		[](const detail::InputFieldRectOverride& a, const detail::InputFieldRectOverride& b) {
			return a.insertBeforeCommandIndex < b.insertBeforeCommandIndex;
		});
	std::stable_sort(
		state_->frameOverrides.textColorOverrides.begin(),
		state_->frameOverrides.textColorOverrides.end(),
		[](const detail::InputFieldTextColorOverride& a, const detail::InputFieldTextColorOverride& b) {
			return a.commandIndex < b.commandIndex;
		});

	publishMutation();
	return renderCommands;
}

field_key::InputFieldKey InputFieldManager::normalizeFieldKey(ResourceKey key) const {
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		*storage_, key, ResourceDomain::InputField,
		key_storage::ResourceScope::WindowLocal, window_);
	return field_key::resourceInputFieldKey(normalized.name);
}

FieldQueryResult InputFieldManager::requestField(
	FlowElementID fieldId,
	const FieldRequest& request) {
	return requestFieldByKey(field_key::toInputFieldKey(fieldId), request);
}

FieldQueryResult InputFieldManager::requestField(
	GlobalFlowID fieldId,
	const FieldRequest& request) {
	return requestFieldByKey(field_key::toInputFieldKey(fieldId), request);
}

FieldQueryResult InputFieldManager::requestField(
	FlowElementPartID fieldId,
	const FieldRequest& request) {
	return requestFieldByKey(field_key::toInputFieldKey(fieldId), request);
}

FieldQueryResult InputFieldManager::requestField(
	ResourceKey key,
	const FieldRequest& request) {
	return requestFieldByKey(normalizeFieldKey(key), request);
}

FieldQueryResult InputFieldManager::requestFieldByKey(
	field_key::InputFieldKey fieldId,
	const FieldRequest& request) {
	if (!fieldId) {
		return {};
	}

	auto [it, inserted] = state_->fieldsById.try_emplace(fieldId);
	FieldState& field = it->second;
	if (inserted) {
		field.text = std::string(request.initialText);
	}
	field.config = request.config;
	field.textElementId = request.textElementId;
	field.contentElementId = request.contentElementId;
	field.lastTouchedEpoch = state_->currentTouchEpoch;
	clampCaretsToText(field);

	FieldQueryResult result{};
	result.text = field.text;
	result.hasPrimaryCaret =
		!field.carets.empty() && state_->primaryFieldId == fieldId;
	for (const CaretState& caret : field.carets) {
		if (caretHasSelection(caret)) {
			result.hasSelection = true;
			break;
		}
	}
	return result;
}

void InputFieldManager::requestCaret(ResourceKey key, CaretRequestKind kind) {
	requestCaretByKey(normalizeFieldKey(key), kind);
}

void InputFieldManager::requestCaret(FlowElementID fieldId, CaretRequestKind kind) {
	requestCaretByKey(field_key::toInputFieldKey(fieldId), kind);
}

void InputFieldManager::requestCaret(GlobalFlowID fieldId, CaretRequestKind kind) {
	requestCaretByKey(field_key::toInputFieldKey(fieldId), kind);
}

void InputFieldManager::requestCaret(FlowElementPartID fieldId, CaretRequestKind kind) {
	requestCaretByKey(field_key::toInputFieldKey(fieldId), kind);
}

bool InputFieldManager::hasPrimaryFieldFocus() const {
	if (!state_->primaryFieldId) {
		return false;
	}
	const auto it = state_->fieldsById.find(state_->primaryFieldId);
	if (it == state_->fieldsById.end()) {
		return false;
	}
	return !it->second.carets.empty();
}

std::string_view InputFieldManager::getSelectedText() const {
	if (!state_->primaryFieldId) {
		return {};
	}
	const auto it = state_->fieldsById.find(state_->primaryFieldId);
	if (it == state_->fieldsById.end()) {
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
	if (utf8Text.empty() || !state_->primaryFieldId) {
		return false;
	}

	const auto it = state_->fieldsById.find(state_->primaryFieldId);
	if (it == state_->fieldsById.end()) {
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

void InputFieldManager::requestCaretByKey(
	field_key::InputFieldKey fieldId,
	CaretRequestKind kind) {
	if (kind == CaretRequestKind::ClearAll) {
		for (auto& [_, field] : state_->fieldsById) {
			field.carets.clear();
		}
		state_->primaryFieldId = {};
		state_->pointerDrag = PointerDragState{};
		markCaretBlinkReset();
		return;
	}

	if (!fieldId) {
		return;
	}

	auto& field = state_->fieldsById[fieldId];
	clampCaretsToText(field);

	if (kind == CaretRequestKind::SetPrimary) {
		const bool primaryWasSameField = (state_->primaryFieldId == fieldId);
		const bool hadExistingCaret = !field.carets.empty();
		CaretState preservedCaret{};
		if (hadExistingCaret) {
			preservedCaret = field.carets.front();
		}

		for (auto& [_, entry] : state_->fieldsById) {
			entry.carets.clear();
		}
		if (hadExistingCaret) {
			field.carets = { preservedCaret };
		} else {
			const size_t offset = field.text.size();
			field.carets = { CaretState{offset, offset} };
		}
		state_->primaryFieldId = fieldId;

		// Preserve ongoing pointer drag when SetPrimary targets the same field.
		if (state_->pointerDrag.active && state_->pointerDrag.fieldId != fieldId) {
			state_->pointerDrag = PointerDragState{};
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
	if (!state_->primaryFieldId) {
		state_->primaryFieldId = fieldId;
	}
	state_->pointerDrag = PointerDragState{};
	markCaretBlinkReset();
}

bool InputFieldManager::removeField(FlowElementID fieldId) {
	return removeFieldByKey(field_key::toInputFieldKey(fieldId));
}

bool InputFieldManager::removeField(GlobalFlowID fieldId) {
	return removeFieldByKey(field_key::toInputFieldKey(fieldId));
}

bool InputFieldManager::removeField(FlowElementPartID fieldId) {
	return removeFieldByKey(field_key::toInputFieldKey(fieldId));
}

bool InputFieldManager::removeFieldByKey(field_key::InputFieldKey fieldId) {
	if (!fieldId) {
		return false;
	}

	const auto it = state_->fieldsById.find(fieldId);
	if (it == state_->fieldsById.end()) {
		return false;
	}
	state_->fieldsById.erase(it);

	if (state_->primaryFieldId == fieldId) {
		state_->primaryFieldId = {};
		for (const auto& [candidateId, field] : state_->fieldsById) {
			if (!field.carets.empty()) {
				state_->primaryFieldId = candidateId;
				break;
			}
		}
	}
	if (state_->pointerDrag.fieldId == fieldId) {
		state_->pointerDrag = PointerDragState{};
	}
	return true;
}

bool InputFieldManager::removeField(ResourceKey key) {
	return removeFieldByKey(normalizeFieldKey(key));
}

bool InputFieldManager::replaceText(
	FlowElementID fieldId,
	std::string_view text,
	bool preserveCaret) {
	return replaceTextByKey(field_key::toInputFieldKey(fieldId), text, preserveCaret);
}

bool InputFieldManager::replaceText(
	GlobalFlowID fieldId,
	std::string_view text,
	bool preserveCaret) {
	return replaceTextByKey(field_key::toInputFieldKey(fieldId), text, preserveCaret);
}

bool InputFieldManager::replaceText(
	FlowElementPartID fieldId,
	std::string_view text,
	bool preserveCaret) {
	return replaceTextByKey(field_key::toInputFieldKey(fieldId), text, preserveCaret);
}

bool InputFieldManager::replaceTextByKey(
	field_key::InputFieldKey fieldId,
	std::string_view text,
	bool preserveCaret) {
	if (!fieldId) {
		return false;
	}

	const auto it = state_->fieldsById.find(fieldId);
	if (it == state_->fieldsById.end() || it->second.text == text) {
		return false;
	}

	FieldState& field = it->second;
	field.text = std::string(text);
	if (preserveCaret) {
		clampCaretsToText(field);
	} else {
		field.carets.clear();
		if (state_->primaryFieldId == fieldId) {
			state_->primaryFieldId = {};
			for (const auto& [candidateId, candidate] : state_->fieldsById) {
				if (!candidate.carets.empty()) {
					state_->primaryFieldId = candidateId;
					break;
				}
			}
		}
		if (state_->pointerDrag.fieldId == fieldId) {
			state_->pointerDrag = PointerDragState{};
		}
	}

	markCaretBlinkReset();
	return true;
}

bool InputFieldManager::replaceText(ResourceKey key, std::string_view text, bool preserveCaret) {
	return replaceTextByKey(normalizeFieldKey(key), text, preserveCaret);
}

void InputFieldManager::clear() {
	state_->fieldsById.clear();
	state_->primaryFieldId = {};
	state_->previousInput = FrameInput{};
	state_->currentInput = FrameInput{};
	state_->leftKeyRepeat = KeyRepeatState{};
	state_->rightKeyRepeat = KeyRepeatState{};
	state_->backspaceKeyRepeat = KeyRepeatState{};
	state_->deleteKeyRepeat = KeyRepeatState{};
	state_->pointerDrag = PointerDragState{};
	state_->caretBlinkElapsedSeconds = 0.0;
	state_->caretBlinkResetPending = true;
	state_->emitCaretsThisFrame = true;
	state_->frameOverrides.rects.clear();
	state_->frameOverrides.textColorOverrides.clear();
}

void InputFieldManager::applyKeyboardEdits() {
	bool hasAnyCarets = false;
	for (const auto& [_, field] : state_->fieldsById) {
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
		leftPressed = shouldTriggerActionWithRepeat(GLFW_KEY_LEFT, state_->leftKeyRepeat);
		rightPressed = shouldTriggerActionWithRepeat(GLFW_KEY_RIGHT, state_->rightKeyRepeat);
		backspacePressed = shouldTriggerActionWithRepeat(GLFW_KEY_BACKSPACE, state_->backspaceKeyRepeat);
		deletePressed = shouldTriggerActionWithRepeat(GLFW_KEY_DELETE, state_->deleteKeyRepeat);
	} else {
		state_->leftKeyRepeat = KeyRepeatState{};
		state_->rightKeyRepeat = KeyRepeatState{};
		state_->backspaceKeyRepeat = KeyRepeatState{};
		state_->deleteKeyRepeat = KeyRepeatState{};
	}

	const bool selecting = state_->currentInput.shift;
	bool shouldResetCaretBlink = false;

	for (auto& [_, field] : state_->fieldsById) {
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

		const std::string textInput = encodeTextInput(state_->currentInput, field.config.allowNewline);
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

	const FlowUi::Font::FontFaceData* fontFace = FlowUi::detail::ResolveFontFace(&state_->fontView, textData.fontId);
	if (!fontFace) {
		const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, textData.fontSize)) * state_->pointsToPixelsScale;
		return static_cast<float>(text.length) * fallbackEmPixels * 0.5f;
	}

	const FlowUi::detail::TextLayoutResult layoutResult = FlowUi::detail::LayoutTextLine(
		FlowUi::detail::TextLayoutRequest{
			.text = text,
			.fontFace = fontFace,
			.pointsToPixelsScale = state_->pointsToPixelsScale,
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
