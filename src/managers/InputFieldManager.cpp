#include "managers/InputFieldManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

#include <GLFW/glfw3.h>

#include "internal/Text/TextLayoutService.hpp"
#include "internal/Text/TextLineBreaker.hpp"
#include "internal/Text/TextStorage.hpp"
#include "internal/ManagerStorage/InputFieldManagerState.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"

namespace {

constexpr float kBoundsEpsilon = 0.5f;
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

FlowUi::InputFieldOverlayStyle defaultOverlayStyle(
	const FlowUi::InputManagerConfig& config) {
	return {
		.caretShape = FlowUi::InputCaretShape::Bar,
		.caretThicknessPx = config.caretWidthPx,
		.caretBlockWidthPx = std::max(config.caretWidthPx, 8.0f),
		.caretHeightOverflowTopPx = config.caretHeightOverflowTopPx,
		.caretHeightOverflowBottomPx = config.caretHeightOverflowBottomPx,
		.caretColor = config.caretColor,
		.selectionBoxColor = config.highlightBoxColor,
		.selectedTextColor = config.highlightedTextColor,
	};
}

FlowUi::InputFieldOverlayStyle normalizeOverlayStyle(
	FlowUi::InputFieldOverlayStyle style) {
	style.caretThicknessPx = std::max(style.caretThicknessPx, 0.0f);
	style.caretBlockWidthPx = std::max(style.caretBlockWidthPx, 0.0f);
	style.caretHeightOverflowTopPx = std::max(
		style.caretHeightOverflowTopPx, 0.0f);
	style.caretHeightOverflowBottomPx = std::max(
		style.caretHeightOverflowBottomPx, 0.0f);
	style.caretBlinkPeriodSeconds = std::max(
		style.caretBlinkPeriodSeconds, 1.0e-6);
	style.caretBlinkVisibleSeconds = std::clamp(
		style.caretBlinkVisibleSeconds,
		0.0,
		style.caretBlinkPeriodSeconds);
	return style;
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

bool containsNewline(std::string_view text) {
	return text.find('\n') != std::string_view::npos || text.find('\r') != std::string_view::npos;
}

bool validUtf8(std::string_view text) {
	for (size_t i = 0; i < text.size();) {
		const auto first = static_cast<unsigned char>(text[i]);
		size_t count = 0;
		uint32_t value = 0;
		if (first <= 0x7Fu) { count = 1; value = first; }
		else if ((first & 0xE0u) == 0xC0u) { count = 2; value = first & 0x1Fu; }
		else if ((first & 0xF0u) == 0xE0u) { count = 3; value = first & 0x0Fu; }
		else if ((first & 0xF8u) == 0xF0u) { count = 4; value = first & 0x07u; }
		else return false;
		if (i + count > text.size()) return false;
		for (size_t j = 1; j < count; ++j) {
			const auto continuation = static_cast<unsigned char>(text[i + j]);
			if ((continuation & 0xC0u) != 0x80u) return false;
			value = (value << 6u) | (continuation & 0x3Fu);
		}
		if ((count == 2 && value < 0x80u) || (count == 3 && value < 0x800u) ||
			(count == 4 && value < 0x10000u) || value > 0x10FFFFu ||
			(value >= 0xD800u && value <= 0xDFFFu)) return false;
		i += count;
	}
	return true;
}

std::string normalizedNewlines(std::string_view text) {
	std::string result;
	result.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] != '\r') result.push_back(text[i]);
		else {
			result.push_back('\n');
			if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
		}
	}
	return result;
}

int wordClass(unsigned char value) {
	if (std::isspace(value)) return 0;
	if (std::isalnum(value) || value == '_') return 1;
	return 2;
}

size_t fieldTextSize(const void* context) noexcept {
	return FlowUi::detail::text::byteCount(
		*static_cast<const FlowUi::detail::text::FieldStorage*>(context));
}

std::optional<std::string_view> fieldTextContiguous(const void* context) noexcept {
	return FlowUi::detail::text::contiguous(
		*static_cast<const FlowUi::detail::text::FieldStorage*>(context));
}

std::string fieldTextCopy(const void* context, FlowUi::TextRange range) {
	return FlowUi::detail::text::copy(
		*static_cast<const FlowUi::detail::text::FieldStorage*>(context), range);
}

void fieldTextForEachChunk(
	const void* context,
	FlowUi::TextRange range,
	const FlowUi::TextChunkVisitor& visitor) {
	FlowUi::detail::text::forEachChunk(
		*static_cast<const FlowUi::detail::text::FieldStorage*>(context), range, visitor);
}

bool sameLayoutDescriptor(
	const FlowUi::TextLayoutDescriptor& a,
	const FlowUi::TextLayoutDescriptor& b) noexcept {
	return a.fontId == b.fontId && a.fontSize == b.fontSize &&
		a.letterSpacing == b.letterSpacing && a.viewportWidth == b.viewportWidth &&
		a.viewportHeight == b.viewportHeight && a.tabWidth == b.tabWidth;
}

} // namespace

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void InputFieldManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!state_) return;
	try {
		devSystems::DevContainerMemoryAccumulator memory{};
		devSystems::DevContainerMemoryAccumulator textPayload{};
		memory.addNodeContainer(state_->fieldsById);
		memory.add(state_->pendingCommands);
		memory.add(state_->selectedTextScratch);
		for (const auto& [_, field] : state_->fieldsById) {
			textPayload.liveBytes += detail::text::byteCount(field.storage);
			std::visit([&](const auto& storage) {
				using Storage = std::decay_t<decltype(storage)>;
				if constexpr (std::is_same_v<Storage, detail::text::SingleLineStorage>) {
					memory.add(storage.text);
					textPayload.add(storage.text);
				} else {
					memory.add(storage.chunks);
					memory.add(storage.lineStarts);
					for (const auto& chunk : storage.chunks) memory.add(chunk);
					for (const auto& chunk : storage.chunks) textPayload.add(chunk);
				}
			}, field.storage);
			memory.add(field.carets);
			memory.add(field.focusRetentionElementIds);
			memory.add(field.visibleLineStrings);
			for (const auto& line : field.visibleLineStrings) memory.add(line);
			memory.add(field.visibleLines);
			memory.add(field.submittedTextSpans);
			memory.addNodeContainer(field.wrapCacheByHardLine);
			for (const auto& [__, wrap] : field.wrapCacheByHardLine) memory.add(wrap.visualRanges);
			memory.add(field.frameTransactions);
			memory.add(field.frameTransactionViews);
			memory.add(field.frameCommandRequests);
		}
		devSystems::appendManagerSample(
			sink, devSystems::memory_sources::kInputFields.id, memory, window_);
		devSystems::appendManagerSample(
			sink, devSystems::memory_sources::kInputTextPayload.id, textPayload, window_);
	} catch (...) {}
}
#endif

namespace manager_storage = detail::manager_storage;
namespace key_storage = detail::managerStorage;
namespace storage = detail::storage;
namespace field_key = detail::input_field;
namespace text_storage = detail::text;

void InputFieldManager::init(
	storage::IStorageSystem& storageSystem,
	WindowId window,
	const InputManagerConfig& config,
	float pointsToPixelsScale,
	detail::text::TextLayoutService& textLayoutService) {
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
	textLayoutService_ = &textLayoutService;
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
	textLayoutService_ = nullptr;
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

void InputFieldManager::updateCaretBlinkVisibility(
	bool hasAnyActiveCaret,
	const InputFieldOverlayStyle* style) {
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

	const InputFieldOverlayStyle fallback = defaultOverlayStyle(state_->config);
	const InputFieldOverlayStyle& resolved = style ? *style : fallback;
	const double period = std::max(resolved.caretBlinkPeriodSeconds, 1.0e-6);
	const double visibleDuration = std::clamp(
		resolved.caretBlinkVisibleSeconds,
		0.0,
		period);
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

void InputFieldManager::beginFrame(
	const FrameInput& currentInput,
	const FrameInput& previousInput,
	uint32_t suppressedPrimaryPressClayId) {
	state_->previousInput = previousInput;
	state_->currentInput = currentInput;
	state_->suppressedPrimaryPressClayId = suppressedPrimaryPressClayId;
	if (state_->currentTouchEpoch == std::numeric_limits<uint64_t>::max()) {
		for (auto& [_, field] : state_->fieldsById) field.lastTouchedEpoch = 0;
		state_->currentTouchEpoch = 1;
	} else {
		++state_->currentTouchEpoch;
	}

	state_->pendingCommands.clear();
	for (auto& [_, field] : state_->fieldsById) {
		field.frameTransactions.clear();
		field.frameTransactionViews.clear();
		field.frameCommandRequests.clear();
		field.submittedTextSpans.clear();
	}
	if (!hasPrimaryFieldFocus()) {
		state_->leftKeyRepeat = KeyRepeatState{};
		state_->rightKeyRepeat = KeyRepeatState{};
		state_->upKeyRepeat = KeyRepeatState{};
		state_->downKeyRepeat = KeyRepeatState{};
		state_->homeKeyRepeat = KeyRepeatState{};
		state_->endKeyRepeat = KeyRepeatState{};
		state_->backspaceKeyRepeat = KeyRepeatState{};
		state_->deleteKeyRepeat = KeyRepeatState{};
	}
	state_->dirty = true;
}

void InputFieldManager::setClipboardAccess(
	std::function<void(std::string_view)> setClipboardText,
	std::function<std::string()> getClipboardText) {
	state_->setClipboardText = std::move(setClipboardText);
	state_->getClipboardText = std::move(getClipboardText);
}

Clay_RenderCommandArray InputFieldManager::endFrame(const Clay_RenderCommandArray& renderCommands) {
	const auto publishMutation = [this] {
		if (!state_->dirty) return;
		storage_->noteManagerMutation(window_);
		state_->dirty = false;
	};
	state_->frameOverrides.rects.clear();
	state_->frameOverrides.textColorOverrides.clear();
	state_->frameOverrides.textLayoutOverrides.clear();

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
	struct RuntimeSubmittedSpan {
		TextRange logicalRange{};
		uint32_t visualLineIndex = 0;
		Clay_BoundingBox bounds{};
	};

	struct RuntimeFieldState {
		field_key::InputFieldKey fieldId{};
		FieldState* field = nullptr;
		size_t cursor = 0u;
		bool hasContentBounds = false;
		Clay_BoundingBox contentBounds{};
		bool hasTextBounds = false;
		Clay_BoundingBox textBounds{};
		std::vector<RuntimeSubmittedSpan> submittedSpans{};
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

		for (const manager_storage::InputSubmittedTextSpan& submission : field.submittedTextSpans) {
			if (!elementIdIsValid(submission.textElementId)) continue;
			const Clay_ElementData elementData = Clay_GetElementData(submission.textElementId);
			if (!elementData.found) continue;
			runtime.submittedSpans.push_back(RuntimeSubmittedSpan{
				.logicalRange = submission.logicalRange,
				.visualLineIndex = submission.visualLineIndex,
				.bounds = elementData.boundingBox,
			});
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

	for (RuntimeFieldState& runtime : runtimes) {
		if (!runtime.field || !runtime.hasContentBounds ||
			text_storage::storageMode(runtime.field->storage) != TextFieldMode::MultiLine ||
			!boundsContainsPoint(
				runtime.contentBounds,
				state_->currentInput.mouseX,
				state_->currentInput.mouseY)) {
			continue;
		}
		FieldState& field = *runtime.field;
		const float previousX = field.scrollOffset.x;
		const float previousY = field.scrollOffset.y;
		if (!field.config.softWrap) {
			field.scrollOffset.x = std::clamp(
				field.scrollOffset.x - state_->currentInput.scrollX,
				0.0f,
				field.maximumScrollX);
		}
		field.scrollOffset.y = std::clamp(
			field.scrollOffset.y - state_->currentInput.scrollY,
			0.0f,
			field.maximumScrollY);
		if (field.scrollOffset.x != previousX || field.scrollOffset.y != previousY) {
			state_->dirty = true;
		}
	}

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
			const RuntimeSubmittedSpan* matchedSubmission = nullptr;
			for (RuntimeFieldState& runtime : runtimes) {
				if (!runtime.field) continue;
				for (const RuntimeSubmittedSpan& submission : runtime.submittedSpans) {
					if (boundsContains(submission.bounds, command.boundingBox)) {
						matchedRuntime = &runtime;
						matchedSubmission = &submission;
						break;
					}
				}
				if (matchedRuntime) break;
				if (text_storage::storageMode(runtime.field->storage) != TextFieldMode::SingleLine ||
					!runtime.hasTextBounds) continue;
				if (!boundsContains(runtime.textBounds, command.boundingBox)) {
					continue;
				}

				const std::string_view fullText = text_storage::contiguous(runtime.field->storage).value_or(std::string_view{});
				size_t resolved = std::min(runtime.cursor, fullText.size());
				if (!commandTextView.empty() &&
					!findSliceOffsetFromCursor(fullText, commandTextView, runtime.cursor, resolved)) {
					continue;
				}

				matchedRuntime = &runtime;
				break;
			}

			if (!matchedRuntime) {
				continue;
			}

			FieldState& field = *matchedRuntime->field;
			const size_t fieldSize = text_storage::byteCount(field.storage);
			size_t commandStartInField = matchedSubmission
				? std::min(matchedSubmission->logicalRange.startByte, fieldSize)
				: std::min(matchedRuntime->cursor, fieldSize);
			if (!matchedSubmission && !commandTextView.empty()) {
				size_t resolved = commandStartInField;
				const std::string_view fullText = text_storage::contiguous(field.storage).value_or(std::string_view{});
				if (findSliceOffsetFromCursor(fullText, commandTextView, matchedRuntime->cursor, resolved)) {
					commandStartInField = resolved;
				}
			}
			const size_t commandEndInField = matchedSubmission
				? std::min(matchedSubmission->logicalRange.endByte, fieldSize)
				: std::min(fieldSize, commandStartInField + commandTextView.size());

			if (!matchedSubmission) matchedRuntime->cursor = commandEndInField;
			matchedRuntime->hasTextCommand = true;
			matchedRuntime->lastTextCommand = command;
			matchedRuntime->lastTextCommandIndex = i;
			matchedRuntime->textSpans.push_back(RuntimeTextSpan{
				.commandIndex = i,
				.command = command,
				.startByteOffset = commandStartInField,
				.endByteOffset = commandEndInField,
			});
			state_->frameOverrides.textLayoutOverrides.push_back(
				detail::InputFieldTextLayoutOverride{
					.commandIndex = i,
					.tabWidth = field.layout.tabWidth,
				});
		}
	}

	auto resolvePointerOffsetInRuntime = [&](const RuntimeFieldState& runtime, float mouseX, float mouseY) -> size_t {
		if (!runtime.field) {
			return 0u;
		}
		const FieldState& field = *runtime.field;
		if (runtime.textSpans.empty()) {
			if (text_storage::empty(field.storage)) {
				return 0u;
			}
			if (runtime.hasContentBounds && mouseX > (runtime.contentBounds.x + runtime.contentBounds.width * 0.5f)) {
				return text_storage::byteCount(field.storage);
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
		const float localX = mouseX - bestSpan->command.boundingBox.x;
		const detail::text::TextLayoutResult& layout = textLayoutService_->layout(
			detail::text::TextLayoutRequest{
				.text = commandTextView,
				.fontView = &state_->fontView,
				.fontId = static_cast<FontId>(textData.fontId),
				.pointsToPixelsScale = state_->pointsToPixelsScale,
				.fontSize = textData.fontSize,
				.letterSpacing = textData.letterSpacing,
				.tabWidth = field.layout.tabWidth,
				.includeGlyphGeometry = false,
			});
		size_t localOffset = localX <= 0.0f ? 0u : commandTextView.size();
		if (layout.success && !layout.caretStops.empty()) {
			localOffset = layout.caretStops.back().byteOffset;
			for (size_t i = 0; i + 1u < layout.caretStops.size(); ++i) {
				const float midpoint = 0.5f * (layout.caretStops[i].x + layout.caretStops[i + 1u].x);
				if (localX < midpoint) {
					localOffset = layout.caretStops[i].byteOffset;
					break;
				}
			}
		}
		const size_t resolved = std::min(
			bestSpan->startByteOffset + localOffset,
			bestSpan->endByteOffset);
		return text_storage::clampUtf8Boundary(field.storage, resolved);
	};

	if (pointerPressed) {
		RuntimeFieldState* targetRuntime = nullptr;
		bool hitSuppressedAnchor = false;
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
			if (state_->suppressedPrimaryPressClayId != 0 &&
				runtime.fieldId.domain == detail::input_field::InputFieldKeyDomain::Element &&
				FlowIDToClayID(FlowElementID{.value = runtime.fieldId.value}) ==
					state_->suppressedPrimaryPressClayId) {
				hitSuppressedAnchor = true;
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

		bool retainPrimaryFocus = hitSuppressedAnchor;
		if (!targetRuntime && state_->primaryFieldId) {
			const auto primaryIt = state_->fieldsById.find(state_->primaryFieldId);
			if (primaryIt != state_->fieldsById.end()) {
				for (Clay_ElementId retentionId :
					primaryIt->second.focusRetentionElementIds) {
					if (!elementIdIsValid(retentionId)) {
						continue;
					}
					const Clay_ElementData retentionData =
						Clay_GetElementData(retentionId);
					if (retentionData.found && boundsContainsPoint(
						retentionData.boundingBox,
						state_->currentInput.mouseX,
						state_->currentInput.mouseY)) {
						retainPrimaryFocus = true;
						break;
					}
				}
			}
		}

		if ((!targetRuntime || !targetRuntime->field) && !retainPrimaryFocus) {
			for (auto& [_, field] : state_->fieldsById) {
				field.carets.clear();
			}
			state_->primaryFieldId = {};
			state_->pointerDrag = PointerDragState{};
		} else if (targetRuntime && targetRuntime->field) {
			FieldState& field = *targetRuntime->field;
			const size_t hitOffset = resolvePointerOffsetInRuntime(*targetRuntime, state_->currentInput.mouseX, state_->currentInput.mouseY);
			for (auto& [_, candidate] : state_->fieldsById) {
				candidate.carets.clear();
			}
			field.carets = { CaretState{hitOffset, hitOffset} };
			field.caretRevealPending = true;
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
			const size_t clampedAnchor = text_storage::clampUtf8Boundary(
				dragField.storage, state_->pointerDrag.anchorByteOffset);
			const size_t hitOffset = resolvePointerOffsetInRuntime(*dragRuntime, state_->currentInput.mouseX, state_->currentInput.mouseY);
			if (dragField.carets.empty()) {
				dragField.carets.push_back(CaretState{clampedAnchor, hitOffset});
				markCaretBlinkReset();
			} else {
				CaretState& caret = dragField.carets.front();
				if (caret.anchorByteOffset != clampedAnchor || caret.headByteOffset != hitOffset) {
					caret.anchorByteOffset = clampedAnchor;
					caret.headByteOffset = hitOffset;
					dragField.caretRevealPending = true;
					markCaretBlinkReset();
				}
				if (dragField.carets.size() > 1u) {
					dragField.carets.resize(1u);
				}
			}
			state_->primaryFieldId = state_->pointerDrag.fieldId;
			clampCaretsToText(dragField);
			if (dragRuntime->hasContentBounds &&
				text_storage::storageMode(dragField.storage) == TextFieldMode::MultiLine) {
				const float dt = static_cast<float>(std::max(0.0, state_->currentInput.dt));
				const float verticalSpeed = std::max(60.0f, dragRuntime->contentBounds.height * 3.0f);
				if (state_->currentInput.mouseY < dragRuntime->contentBounds.y) {
					dragField.scrollOffset.y = std::max(0.0f, dragField.scrollOffset.y - verticalSpeed * dt);
				} else if (state_->currentInput.mouseY > dragRuntime->contentBounds.y + dragRuntime->contentBounds.height) {
					dragField.scrollOffset.y = std::min(
						dragField.maximumScrollY,
						dragField.scrollOffset.y + verticalSpeed * dt);
				}
				if (!dragField.config.softWrap) {
					const float horizontalSpeed = std::max(60.0f, dragRuntime->contentBounds.width * 3.0f);
					if (state_->currentInput.mouseX < dragRuntime->contentBounds.x) {
						dragField.scrollOffset.x = std::max(0.0f, dragField.scrollOffset.x - horizontalSpeed * dt);
					} else if (state_->currentInput.mouseX > dragRuntime->contentBounds.x + dragRuntime->contentBounds.width) {
						dragField.scrollOffset.x = std::min(
							dragField.maximumScrollX,
							dragField.scrollOffset.x + horizontalSpeed * dt);
					}
				}
			}
		}
	}

	bool needsOverrides = false;
	bool hasAnyVisibleCaret = false;
	const InputFieldOverlayStyle* blinkStyle = nullptr;
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
		if (fieldShowsCaret && runtime.fieldId == state_->primaryFieldId) {
			blinkStyle = &runtime.field->overlayStyle;
		}
	}

	updateCaretBlinkVisibility(hasAnyVisibleCaret, blinkStyle);
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

	auto pushCaretRect = [&](const InputFieldOverlayStyle& style,
		int32_t insertBeforeCommandIndex, float caretX, float y, float h) {
		const float thickness = std::max(0.0f, style.caretThicknessPx);
		const float blockWidth = std::max(0.0f, style.caretBlockWidthPx);
		switch (style.caretShape) {
		case InputCaretShape::Bar:
			pushRectOverride(
				insertBeforeCommandIndex, caretX, y, thickness, h,
				style.caretColor);
			break;
		case InputCaretShape::Block:
			pushRectOverride(
				insertBeforeCommandIndex, caretX, y, blockWidth, h,
				style.caretColor);
			break;
		case InputCaretShape::Underline:
			pushRectOverride(
				insertBeforeCommandIndex, caretX, y + std::max(0.0f, h - thickness),
				blockWidth, thickness, style.caretColor);
			break;
		}
	};

	auto pushTextColorOverride = [&](int32_t commandIndex,
		const std::vector<SelectionRange>& localSelections,
		int commandByteLength,
		const Clay_Color& selectedTextColor) {
		if (localSelections.empty() || commandByteLength <= 0) {
			return;
		}

		detail::InputFieldTextColorOverride textOverride{};
		textOverride.commandIndex = std::clamp(commandIndex, 0, std::max(0, renderCommands.length - 1));
		textOverride.color = selectedTextColor;
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
		const InputFieldOverlayStyle& overlayStyle = field.overlayStyle;
		for (const RuntimeTextSpan& span : runtime.textSpans) {
			const Clay_RenderCommand& command = span.command;
			const Clay_TextRenderData& textData = command.renderData.text;
			const Clay_StringSlice sourceSlice = textData.stringContents;
			const int commandByteLength = std::max(0, sourceSlice.length);
			const std::string_view commandText(
				sourceSlice.chars ? sourceSlice.chars : "",
				static_cast<size_t>(commandByteLength));
			const detail::text::TextLayoutResult& spanLayout = textLayoutService_->layout(
				detail::text::TextLayoutRequest{
					.text = commandText,
					.fontView = &state_->fontView,
					.fontId = static_cast<FontId>(textData.fontId),
					.pointsToPixelsScale = state_->pointsToPixelsScale,
					.fontSize = textData.fontSize,
					.letterSpacing = textData.letterSpacing,
					.tabWidth = field.layout.tabWidth,
					.includeGlyphGeometry = false,
				});
			const auto xAtByte = [&](size_t localByte) {
				if (!spanLayout.success || spanLayout.caretStops.empty()) return 0.0f;
				localByte = std::min(localByte, commandText.size());
				float x = 0.0f;
				for (const detail::text::TextCaretStop& stop : spanLayout.caretStops) {
					if (stop.byteOffset > localByte) break;
					x = stop.x;
					if (stop.byteOffset == localByte) break;
				}
				return x;
			};
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

			auto emitHighlightRect = [&](size_t localStart, size_t localEnd) {
				if (localEnd <= localStart) {
					return;
				}
				const float startX = xAtByte(localStart);
				const float endX = xAtByte(localEnd);
				const float x0 = command.boundingBox.x + std::min(startX, endX);
				const float selectionWidth = std::abs(endX - startX);
				if (selectionWidth <= 0.0f) {
					return;
				}

				pushRectOverride(
					span.commandIndex,
					x0,
					command.boundingBox.y,
					selectionWidth,
					command.boundingBox.height,
					overlayStyle.selectionBoxColor);
			};

			if (!localSelections.empty() && commandByteLength > 0) {
				pushTextColorOverride(
					span.commandIndex,
					localSelections,
					commandByteLength,
					overlayStyle.selectedTextColor);
				for (const SelectionRange& localSelection : localSelections) {
					const size_t selStart = std::min<size_t>(localSelection.start, static_cast<size_t>(commandByteLength));
					const size_t selEnd = std::min<size_t>(localSelection.end, static_cast<size_t>(commandByteLength));
					if (selEnd <= selStart) {
						continue;
					}
					emitHighlightRect(selStart, selEnd);
				}
			}

			if (state_->emitCaretsThisFrame && !field.config.readOnly) {
				const float caretY = command.boundingBox.y - overlayStyle.caretHeightOverflowTopPx;
				const float caretH = command.boundingBox.height +
					overlayStyle.caretHeightOverflowTopPx +
					overlayStyle.caretHeightOverflowBottomPx;
				for (size_t caretIndex = 0; caretIndex < field.carets.size(); ++caretIndex) {
					if (caretIndex >= runtime.caretDrawn.size() || runtime.caretDrawn[caretIndex]) {
						continue;
					}

					const size_t fieldSize = text_storage::byteCount(field.storage);
					const size_t caretOffset = text_storage::clampUtf8Boundary(
						field.storage, field.carets[caretIndex].headByteOffset);
					const bool spanEndsHardLine = span.endByteOffset == fieldSize ||
						text_storage::byteAt(field.storage, span.endByteOffset) == '\n';
					const bool caretInsideCommand =
						(caretOffset >= span.startByteOffset && caretOffset < span.endByteOffset) ||
						(caretOffset == span.endByteOffset && spanEndsHardLine);
					if (!caretInsideCommand) {
						continue;
					}

					const size_t localCaret = std::min(caretOffset - span.startByteOffset, static_cast<size_t>(commandByteLength));
					const float caretX = command.boundingBox.x + xAtByte(localCaret);
					pushCaretRect(
						overlayStyle,
						span.commandIndex + 1,
						caretX,
						caretY,
						caretH);
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
		const InputFieldOverlayStyle& overlayStyle = field.overlayStyle;
		for (size_t caretIndex = 0; caretIndex < field.carets.size(); ++caretIndex) {
			if (caretIndex < runtime.caretDrawn.size() && runtime.caretDrawn[caretIndex]) {
				continue;
			}

			if (text_storage::storageMode(field.storage) == TextFieldMode::MultiLine) {
				const size_t caretOffset = field.carets[caretIndex].headByteOffset;
				const auto submitted = std::find_if(
					runtime.submittedSpans.begin(), runtime.submittedSpans.end(),
					[&](const RuntimeSubmittedSpan& span) {
						return span.logicalRange.startByte == span.logicalRange.endByte &&
							caretOffset == span.logicalRange.startByte;
					});
				if (submitted != runtime.submittedSpans.end()) {
					const float caretY = submitted->bounds.y - overlayStyle.caretHeightOverflowTopPx;
					const float caretH = std::max(submitted->bounds.height, fieldLineHeight(field)) +
						overlayStyle.caretHeightOverflowTopPx +
						overlayStyle.caretHeightOverflowBottomPx;
					pushCaretRect(
						overlayStyle,
						maxInsertionIndex,
						submitted->bounds.x,
						caretY,
						caretH);
				}
				continue;
			}

			if (runtime.hasTextCommand && runtime.lastTextCommand.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
				const Clay_RenderCommand& referenceCommand = runtime.lastTextCommand;
				const Clay_TextRenderData& textData = referenceCommand.renderData.text;
				const float textWidth = measureTextSlice(textData.stringContents, textData);
				const float caretY = referenceCommand.boundingBox.y - overlayStyle.caretHeightOverflowTopPx;
				const float caretH = referenceCommand.boundingBox.height +
					overlayStyle.caretHeightOverflowTopPx +
					overlayStyle.caretHeightOverflowBottomPx;
				pushCaretRect(
					overlayStyle,
					runtime.lastTextCommandIndex + 1,
					referenceCommand.boundingBox.x + textWidth,
					caretY,
					caretH);
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
				const float caretY = fallbackBounds.y - overlayStyle.caretHeightOverflowTopPx;
				const float caretH = fallbackHeight +
					overlayStyle.caretHeightOverflowTopPx +
					overlayStyle.caretHeightOverflowBottomPx;
				pushCaretRect(
					overlayStyle,
					maxInsertionIndex,
					fallbackBounds.x,
					caretY,
					caretH);
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
	FieldConfig normalizedConfig = request.config;
	if (normalizedConfig.allowNewline) normalizedConfig.mode = TextFieldMode::MultiLine;
	normalizedConfig.allowNewline = normalizedConfig.mode == TextFieldMode::MultiLine;
	if (inserted || !field.initialized) {
		field.modeChangeRejected = normalizedConfig.mode == TextFieldMode::SingleLine &&
			containsNewline(request.initialText);
		if (field.modeChangeRejected) {
			normalizedConfig.mode = TextFieldMode::MultiLine;
			normalizedConfig.allowNewline = true;
		}
		field.storage = text_storage::makeFieldStorage(normalizedConfig.mode, request.initialText);
		field.handleValue = state_->nextFieldHandle++;
		if (field.handleValue == 0) field.handleValue = state_->nextFieldHandle++;
		field.initialized = true;
	} else if (text_storage::storageMode(field.storage) != normalizedConfig.mode) {
		field.modeChangeRejected = !text_storage::migrate(field.storage, normalizedConfig.mode);
		if (field.modeChangeRejected) {
			normalizedConfig.mode = text_storage::storageMode(field.storage);
			normalizedConfig.allowNewline = normalizedConfig.mode == TextFieldMode::MultiLine;
		} else {
			field.wrapCacheByHardLine.clear();
			field.scrollOffset = {};
		}
	} else {
		field.modeChangeRejected = false;
	}
	if (!sameLayoutDescriptor(field.layout, request.layout)) field.wrapCacheByHardLine.clear();
	field.config = normalizedConfig;
	field.layout = request.layout;
	field.overlayStyle = normalizeOverlayStyle(request.overlayStyle.value_or(
		defaultOverlayStyle(state_->config)));
	field.textElementId = request.textElementId;
	field.contentElementId = request.contentElementId;
	field.focusRetentionElementIds.assign(
		request.focusRetentionElementIds.begin(),
		request.focusRetentionElementIds.end());
	field.lastTouchedEpoch = state_->currentTouchEpoch;
	clampCaretsToText(field);
	if (state_->primaryFieldId == fieldId && !field.carets.empty() &&
		field.commandsAppliedEpoch != state_->currentTouchEpoch) {
		field.commandsAppliedEpoch = state_->currentTouchEpoch;
		applyPendingCommands(fieldId, field);
		applyCapturedEdits(fieldId, field);
	}
	if (state_->primaryFieldId == fieldId) revealPrimaryCaret(field);
	materializeVisibleLines(field);
	refreshTransactionViews(field);

	FieldQueryResult result{};
	result.field = FieldHandle{field.handleValue, state_->currentTouchEpoch};
	result.text = FieldTextView{
		&field.storage,
		fieldTextSize,
		fieldTextContiguous,
		fieldTextCopy,
		fieldTextForEachChunk,
	};
	result.mode = text_storage::storageMode(field.storage);
	result.modeChangeRejected = field.modeChangeRejected;
	result.visibleLines = field.visibleLines;
	result.scrollOffset = field.scrollOffset;
	result.hasPrimaryCaret =
		!field.carets.empty() && state_->primaryFieldId == fieldId;
	result.revision = field.revision;
	result.transactions = field.frameTransactionViews;
	result.commandRequests = field.frameCommandRequests;
	for (const CaretState& caret : field.carets) {
		if (caretHasSelection(caret)) {
			result.hasSelection = true;
			break;
		}
	}
	return result;
}

bool InputFieldManager::submitTextSpan(
	FieldHandle handle,
	const FieldTextSpanSubmission& span) {
	if (!handle || handle.frameEpoch != state_->currentTouchEpoch ||
		!elementIdIsValid(span.textElementId)) return false;
	for (auto& [_, field] : state_->fieldsById) {
		if (field.handleValue != handle.value || field.lastTouchedEpoch != handle.frameEpoch) continue;
		const size_t size = text_storage::byteCount(field.storage);
		if (span.logicalRange.startByte > span.logicalRange.endByte ||
			span.logicalRange.endByte > size ||
			text_storage::clampUtf8Boundary(field.storage, span.logicalRange.startByte) != span.logicalRange.startByte ||
			text_storage::clampUtf8Boundary(field.storage, span.logicalRange.endByte) != span.logicalRange.endByte) {
			return false;
		}
		const bool isVisible = std::ranges::any_of(field.visibleLines, [&](const VisibleTextLine& line) {
			return line.logicalRange.startByte == span.logicalRange.startByte &&
				line.logicalRange.endByte == span.logicalRange.endByte &&
				line.visualLineIndex == span.visualLineIndex;
		});
		if (!isVisible) return false;
		field.submittedTextSpans.push_back(manager_storage::InputSubmittedTextSpan{
			.logicalRange = span.logicalRange,
			.textElementId = span.textElementId,
			.visualLineIndex = span.visualLineIndex,
		});
		return true;
	}
	return false;
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

bool InputFieldManager::canEditPrimaryField() const {
	if (!state_->primaryFieldId) return false;
	const auto it = state_->fieldsById.find(state_->primaryFieldId);
	return it != state_->fieldsById.end() && !it->second.carets.empty() && !it->second.config.readOnly;
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
	const size_t fieldSize = text_storage::byteCount(field.storage);
	size_t selectedStart = fieldSize;
	size_t selectedEnd = fieldSize;
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
	if (const auto textView = text_storage::contiguous(field.storage)) {
		return textView->substr(selectedStart, selectedEnd - selectedStart);
	}
	state_->selectedTextScratch = text_storage::copy(
		field.storage, TextRange{selectedStart, selectedEnd});
	return state_->selectedTextScratch;
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

	const uint64_t beforeRevision = field.revision;
	const std::vector<CaretState> beforeCarets = field.carets;
	applyTextInsertion(field, utf8Text, EditOrigin::Programmatic);

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

	const bool changed = field.revision != beforeRevision || caretsChanged;
	if (changed) {
		markCaretBlinkReset();
	}
	return changed;
}

bool InputFieldManager::enqueueCommand(TextCommand command, std::string_view payload) {
	if (!state_->primaryFieldId || !hasPrimaryFieldFocus()) return false;
	state_->pendingCommands.push_back(manager_storage::InputPendingCommand{
		.fieldId = state_->primaryFieldId,
		.command = command,
		.payload = std::string(payload),
		.extendSelection = state_->currentInput.shift,
	});
	return true;
}

EditResult InputFieldManager::applyEdits(
	FlowElementID fieldId,
	std::span<const TextReplacement> edits,
	EditOrigin origin) {
	return applyEditsByKey(field_key::toInputFieldKey(fieldId), edits, origin, false, true);
}

EditResult InputFieldManager::applyEdits(
	GlobalFlowID fieldId,
	std::span<const TextReplacement> edits,
	EditOrigin origin) {
	return applyEditsByKey(field_key::toInputFieldKey(fieldId), edits, origin, false, true);
}

EditResult InputFieldManager::applyEdits(
	FlowElementPartID fieldId,
	std::span<const TextReplacement> edits,
	EditOrigin origin) {
	return applyEditsByKey(field_key::toInputFieldKey(fieldId), edits, origin, false, true);
}

EditResult InputFieldManager::applyEdits(
	ResourceKey key,
	std::span<const TextReplacement> edits,
	EditOrigin origin) {
	return applyEditsByKey(normalizeFieldKey(key), edits, origin, false, true);
}

EditResult InputFieldManager::applyEditsByKey(
	field_key::InputFieldKey fieldId,
	std::span<const TextReplacement> edits,
	EditOrigin origin,
	bool requireFocus,
	bool enforceReadOnly) {
	if (!fieldId) return EditResult::RejectedNoField;
	const auto it = state_->fieldsById.find(fieldId);
	if (it == state_->fieldsById.end()) return EditResult::RejectedNoField;
	if (requireFocus && state_->primaryFieldId != fieldId) return EditResult::RejectedNoFocus;
	const EditResult result = commitEdits(it->second, edits, origin, enforceReadOnly);
	if (result == EditResult::Applied) {
		markCaretBlinkReset();
		state_->dirty = true;
	}
	return result;
}

EditResult InputFieldManager::commitEdits(
	FieldState& field,
	std::span<const TextReplacement> edits,
	EditOrigin origin,
	bool enforceReadOnly) {
	if (edits.empty()) return EditResult::NoChange;
	if (enforceReadOnly && field.config.readOnly) return EditResult::RejectedReadOnly;

	struct PreparedEdit { TextRange range{}; std::string inserted{}; };
	std::vector<PreparedEdit> prepared;
	prepared.reserve(edits.size());
	const size_t originalSize = text_storage::byteCount(field.storage);
	for (const TextReplacement& edit : edits) {
		if (edit.oldRange.startByte > edit.oldRange.endByte ||
			edit.oldRange.endByte > originalSize ||
			text_storage::clampUtf8Boundary(field.storage, edit.oldRange.startByte) != edit.oldRange.startByte ||
			text_storage::clampUtf8Boundary(field.storage, edit.oldRange.endByte) != edit.oldRange.endByte) {
			return EditResult::RejectedInvalidRange;
		}
		if (!field.config.allowNewline && containsNewline(edit.insertedText)) {
			return EditResult::RejectedNewline;
		}
		if (!validUtf8(edit.insertedText)) return EditResult::RejectedInvalidUtf8;
		prepared.push_back(PreparedEdit{
			edit.oldRange,
			field.config.allowNewline ? normalizedNewlines(edit.insertedText) : std::string(edit.insertedText),
		});
	}
	std::stable_sort(prepared.begin(), prepared.end(), [](const PreparedEdit& a, const PreparedEdit& b) {
		return a.range.startByte < b.range.startByte;
	});

	size_t removedBytes = 0;
	size_t insertedBytes = 0;
	bool hasChange = false;
	for (size_t i = 0; i < prepared.size(); ++i) {
		if (i > 0 && (prepared[i - 1].range.endByte > prepared[i].range.startByte ||
			(prepared[i - 1].range.endByte == prepared[i].range.startByte &&
				(prepared[i - 1].range.startByte == prepared[i - 1].range.endByte ||
				 prepared[i].range.startByte == prepared[i].range.endByte)))) {
			return EditResult::RejectedInvalidRange;
		}
		const size_t removed = prepared[i].range.endByte - prepared[i].range.startByte;
		if (removedBytes > std::numeric_limits<size_t>::max() - removed ||
			insertedBytes > std::numeric_limits<size_t>::max() - prepared[i].inserted.size()) {
			return EditResult::RejectedSizeLimit;
		}
		removedBytes += removed;
		insertedBytes += prepared[i].inserted.size();
		hasChange = hasChange ||
			text_storage::copy(field.storage, prepared[i].range) != prepared[i].inserted;
	}
	if (!hasChange) return EditResult::NoChange;
	if (insertedBytes > field.config.maxBytes ||
		originalSize - removedBytes > field.config.maxBytes - insertedBytes) {
		return EditResult::RejectedSizeLimit;
	}

	manager_storage::InputOwnedTransaction transaction{};
	transaction.sequence = state_->nextTransactionSequence++;
	transaction.revisionBefore = field.revision;
	transaction.revisionAfter = field.revision + 1;
	transaction.origin = origin;
	for (const CaretState& caret : field.carets) {
		transaction.selectionsBefore.push_back(TextSelection{
			caret.anchorByteOffset,
			caret.headByteOffset,
			caret.hasPreferredX ? caret.preferredX : 0.0f,
		});
	}
	transaction.selectionsAfter.reserve(field.carets.size());
	transaction.replacements.reserve(prepared.size());
	transaction.replacementViews.reserve(prepared.size());
	for (const PreparedEdit& edit : prepared) {
		const size_t removed = edit.range.endByte - edit.range.startByte;
		auto& report = transaction.replacements.emplace_back();
		report.range = edit.range;
		report.removedByteCount = removed;
		report.insertedByteCount = edit.inserted.size();
		if (field.config.transactionDetail == TransactionReportDetail::Reversible) {
			report.removedText = text_storage::copy(field.storage, edit.range);
			report.insertedText = edit.inserted;
		}
	}
	field.frameTransactions.reserve(field.frameTransactions.size() + 1);
	field.frameTransactionViews.reserve(field.frameTransactions.size() + 1);

	const auto remapOffset = [&prepared](size_t original) {
		size_t mapped = original;
		for (const PreparedEdit& edit : prepared) {
			const size_t removed = edit.range.endByte - edit.range.startByte;
			if (original < edit.range.startByte) break;
			if (original <= edit.range.endByte) {
				return mapped - (original - edit.range.startByte) + edit.inserted.size();
			}
			if (edit.inserted.size() >= removed) mapped += edit.inserted.size() - removed;
			else mapped -= removed - edit.inserted.size();
		}
		return mapped;
	};
	const size_t firstChangedLine = text_storage::lineFromByte(
		field.storage, prepared.front().range.startByte);
	for (auto it = prepared.rbegin(); it != prepared.rend(); ++it) {
		text_storage::replace(field.storage, it->range, it->inserted);
	}
	for (auto cache = field.wrapCacheByHardLine.begin(); cache != field.wrapCacheByHardLine.end();) {
		if (cache->first >= firstChangedLine) cache = field.wrapCacheByHardLine.erase(cache);
		else ++cache;
	}
	for (CaretState& caret : field.carets) {
		caret.anchorByteOffset = remapOffset(caret.anchorByteOffset);
		caret.headByteOffset = remapOffset(caret.headByteOffset);
		caret.hasPreferredX = false;
		transaction.selectionsAfter.push_back(TextSelection{
			caret.anchorByteOffset,
			caret.headByteOffset,
			caret.hasPreferredX ? caret.preferredX : 0.0f,
		});
	}
	field.revision = transaction.revisionAfter;
	field.caretRevealPending = true;
	field.frameTransactions.push_back(std::move(transaction));
	refreshTransactionViews(field);
	return EditResult::Applied;
}

void InputFieldManager::refreshTransactionViews(FieldState& field) {
	field.frameTransactionViews.clear();
	field.frameTransactionViews.reserve(field.frameTransactions.size());
	for (auto& transaction : field.frameTransactions) {
		transaction.replacementViews.clear();
		transaction.replacementViews.reserve(transaction.replacements.size());
		for (const auto& replacement : transaction.replacements) {
			transaction.replacementViews.push_back(TextReplacementReport{
				.oldRange = replacement.range,
				.insertedByteCount = replacement.insertedByteCount,
				.removedByteCount = replacement.removedByteCount,
				.removedText = replacement.removedText,
				.insertedText = replacement.insertedText,
			});
		}
		field.frameTransactionViews.push_back(FieldEditTransaction{
			.sequence = transaction.sequence,
			.revisionBefore = transaction.revisionBefore,
			.revisionAfter = transaction.revisionAfter,
			.origin = transaction.origin,
			.replacements = transaction.replacementViews,
			.selectionsBefore = transaction.selectionsBefore,
			.selectionsAfter = transaction.selectionsAfter,
		});
	}
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
			const size_t offset = text_storage::byteCount(field.storage);
			field.carets = { CaretState{offset, offset} };
		}
		state_->primaryFieldId = fieldId;

		// Preserve ongoing pointer drag when SetPrimary targets the same field.
		if (state_->pointerDrag.active && state_->pointerDrag.fieldId != fieldId) {
			state_->pointerDrag = PointerDragState{};
		}

		if (!primaryWasSameField || !hadExistingCaret) {
			field.caretRevealPending = true;
			markCaretBlinkReset();
		}
		return;
	}

	size_t offset = text_storage::byteCount(field.storage);
	if (!field.carets.empty()) {
		offset = field.carets.back().headByteOffset;
	}
	field.carets.push_back(CaretState{offset, offset});
	field.caretRevealPending = true;
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
	if (it == state_->fieldsById.end() || text_storage::equals(it->second.storage, text)) {
		return false;
	}

	FieldState& field = it->second;
	const std::string currentText = text_storage::copy(
		field.storage,
		TextRange{0, text_storage::byteCount(field.storage)});
	size_t commonPrefix = 0;
	const size_t maximumPrefix = std::min(currentText.size(), text.size());
	while (commonPrefix < maximumPrefix &&
		currentText[commonPrefix] == text[commonPrefix]) {
		++commonPrefix;
	}
	while (commonPrefix > 0 && commonPrefix < currentText.size() &&
		(static_cast<unsigned char>(currentText[commonPrefix]) & 0xc0u) == 0x80u) {
		--commonPrefix;
	}

	size_t commonSuffix = 0;
	while (commonSuffix < currentText.size() - commonPrefix &&
		commonSuffix < text.size() - commonPrefix &&
		currentText[currentText.size() - 1u - commonSuffix] ==
			text[text.size() - 1u - commonSuffix]) {
		++commonSuffix;
	}
	while (commonSuffix > 0 &&
		currentText.size() - commonSuffix < currentText.size() &&
		(static_cast<unsigned char>(
			currentText[currentText.size() - commonSuffix]) & 0xc0u) == 0x80u) {
		--commonSuffix;
	}

	const size_t currentEnd = currentText.size() - commonSuffix;
	const size_t replacementEnd = text.size() - commonSuffix;
	const TextReplacement replacement{
		.oldRange = TextRange{commonPrefix, currentEnd},
		.insertedText = text.substr(
			commonPrefix,
			replacementEnd - commonPrefix),
	};
	if (commitEdits(field, std::span<const TextReplacement>(&replacement, 1), EditOrigin::Programmatic, false) != EditResult::Applied) {
		return false;
	}
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
	if (!field.frameTransactions.empty()) {
		auto& after = field.frameTransactions.back().selectionsAfter;
		after.clear();
		for (const CaretState& caret : field.carets) {
			after.push_back(TextSelection{
				caret.anchorByteOffset,
				caret.headByteOffset,
				caret.hasPreferredX ? caret.preferredX : 0.0f,
			});
		}
		refreshTransactionViews(field);
	}

	markCaretBlinkReset();
	state_->dirty = true;
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
	state_->upKeyRepeat = KeyRepeatState{};
	state_->downKeyRepeat = KeyRepeatState{};
	state_->homeKeyRepeat = KeyRepeatState{};
	state_->endKeyRepeat = KeyRepeatState{};
	state_->backspaceKeyRepeat = KeyRepeatState{};
	state_->deleteKeyRepeat = KeyRepeatState{};
	state_->pointerDrag = PointerDragState{};
	state_->caretBlinkElapsedSeconds = 0.0;
	state_->caretBlinkResetPending = true;
	state_->emitCaretsThisFrame = true;
	state_->frameOverrides.rects.clear();
	state_->frameOverrides.textColorOverrides.clear();
	state_->frameOverrides.textLayoutOverrides.clear();
	state_->pendingCommands.clear();
	state_->selectedTextScratch.clear();
}

void InputFieldManager::applyCapturedEdits(field_key::InputFieldKey fieldId, FieldState& field) {
	if (state_->primaryFieldId != fieldId || field.carets.empty()) return;
	if (keyPressedThisFrame(
		state_->currentInput,
		state_->previousInput,
		GLFW_KEY_ESCAPE)) {
		field.frameCommandRequests.push_back(FieldCommandRequest::Cancel);
		return;
	}
	const bool modified = state_->currentInput.ctrl || state_->currentInput.super || state_->currentInput.alt;
	const bool selecting = state_->currentInput.shift;
	bool caretMoved = false;
	if (modified) {
		state_->leftKeyRepeat = KeyRepeatState{};
		state_->rightKeyRepeat = KeyRepeatState{};
		state_->upKeyRepeat = KeyRepeatState{};
		state_->downKeyRepeat = KeyRepeatState{};
		state_->homeKeyRepeat = KeyRepeatState{};
		state_->endKeyRepeat = KeyRepeatState{};
		state_->backspaceKeyRepeat = KeyRepeatState{};
		state_->deleteKeyRepeat = KeyRepeatState{};
	}
	if (!modified && field.config.allowArrowNavigation) {
		if (shouldTriggerActionWithRepeat(GLFW_KEY_LEFT, state_->leftKeyRepeat)) {
			moveCaretsHorizontal(field, -1, selecting);
			caretMoved = true;
		}
		if (shouldTriggerActionWithRepeat(GLFW_KEY_RIGHT, state_->rightKeyRepeat)) {
			moveCaretsHorizontal(field, +1, selecting);
			caretMoved = true;
		}
		if (text_storage::storageMode(field.storage) == TextFieldMode::MultiLine) {
			if (shouldTriggerActionWithRepeat(GLFW_KEY_UP, state_->upKeyRepeat)) {
				moveCaretsVertical(field, -1, selecting);
				caretMoved = true;
			}
			if (shouldTriggerActionWithRepeat(GLFW_KEY_DOWN, state_->downKeyRepeat)) {
				moveCaretsVertical(field, +1, selecting);
				caretMoved = true;
			}
		}
		if (shouldTriggerActionWithRepeat(GLFW_KEY_HOME, state_->homeKeyRepeat)) {
			moveCaretsToLineBoundary(field, false, selecting);
			caretMoved = true;
		}
		if (shouldTriggerActionWithRepeat(GLFW_KEY_END, state_->endKeyRepeat)) {
			moveCaretsToLineBoundary(field, true, selecting);
			caretMoved = true;
		}
	}
	if (!modified && !field.config.readOnly) {
		if (shouldTriggerActionWithRepeat(GLFW_KEY_BACKSPACE, state_->backspaceKeyRepeat)) applyDelete(field, true);
		if (shouldTriggerActionWithRepeat(GLFW_KEY_DELETE, state_->deleteKeyRepeat)) applyDelete(field, false);
		std::string textInput = encodeTextInput(state_->currentInput, field.config.allowNewline);
		const bool enterPressed =
			keyPressedThisFrame(state_->currentInput, state_->previousInput, GLFW_KEY_ENTER) ||
			keyPressedThisFrame(state_->currentInput, state_->previousInput, GLFW_KEY_KP_ENTER);
		if (enterPressed) {
			if (text_storage::storageMode(field.storage) == TextFieldMode::SingleLine) {
				field.frameCommandRequests.push_back(FieldCommandRequest::Submit);
			} else if (textInput.find('\n') == std::string::npos) {
				textInput.push_back('\n');
			}
		}
		if (!textInput.empty()) applyTextInsertion(field, textInput);
	}
	if (caretMoved) markCaretBlinkReset();
}

void InputFieldManager::applyPendingCommands(field_key::InputFieldKey fieldId, FieldState& field) {
	const auto clipboardSelection = [this, &field] {
		std::string result;
		for (const SelectionRange& range : mergedSelectionRanges(field)) {
			if (!result.empty()) result.push_back('\n');
			result += text_storage::copy(field.storage, TextRange{range.start, range.end});
		}
		return result;
	};
	for (const auto& pending : state_->pendingCommands) {
		if (pending.fieldId != fieldId) continue;
		switch (pending.command) {
		case TextCommand::SelectAll:
			field.carets = {CaretState{0, text_storage::byteCount(field.storage)}};
			field.caretRevealPending = true;
			markCaretBlinkReset();
			break;
		case TextCommand::Copy: {
			const std::string selected = clipboardSelection();
			if (!selected.empty() && state_->setClipboardText) state_->setClipboardText(selected);
			break;
		}
		case TextCommand::Cut: {
			if (field.config.readOnly || !state_->setClipboardText) break;
			const std::string selected = clipboardSelection();
			if (selected.empty()) break;
			std::vector<TextReplacement> cuts;
			for (const SelectionRange& range : mergedSelectionRanges(field)) {
				cuts.push_back(TextReplacement{TextRange{range.start, range.end}, {}});
			}
			if (commitEdits(field, cuts, EditOrigin::Cut, true) == EditResult::Applied) {
				state_->setClipboardText(selected);
			}
			break;
		}
		case TextCommand::Paste: {
			const std::string pasted = !pending.payload.empty()
				? pending.payload
				: (state_->getClipboardText ? state_->getClipboardText() : std::string{});
			if (!pasted.empty() && !field.config.readOnly) {
				applyTextInsertion(field, pasted, EditOrigin::Paste);
			}
			break;
		}
		case TextCommand::RequestUndo:
			field.frameCommandRequests.push_back(FieldCommandRequest::Undo);
			break;
		case TextCommand::RequestRedo:
			field.frameCommandRequests.push_back(FieldCommandRequest::Redo);
			break;
		case TextCommand::MoveWordLeft:
			if (field.config.allowArrowNavigation) moveCaretsByWord(field, -1, pending.extendSelection);
			break;
		case TextCommand::MoveWordRight:
			if (field.config.allowArrowNavigation) moveCaretsByWord(field, +1, pending.extendSelection);
			break;
		case TextCommand::MoveDocumentStart:
		case TextCommand::MoveDocumentEnd: {
			if (!field.config.allowArrowNavigation) break;
			const size_t target = pending.command == TextCommand::MoveDocumentStart
				? 0
				: text_storage::byteCount(field.storage);
			for (CaretState& caret : field.carets) {
				if (pending.extendSelection) caret.headByteOffset = target;
				else caret.anchorByteOffset = caret.headByteOffset = target;
				caret.hasPreferredX = false;
			}
			field.caretRevealPending = true;
			markCaretBlinkReset();
			break;
		}
		case TextCommand::DeleteWordBackward:
			if (!field.config.readOnly) applyWordDelete(field, true);
			break;
		case TextCommand::DeleteWordForward:
			if (!field.config.readOnly) applyWordDelete(field, false);
			break;
		}
	}
	refreshTransactionViews(field);
}

void InputFieldManager::applyTextInsertion(FieldState& field, std::string_view utf8Text, EditOrigin origin) {
	if (utf8Text.empty() || field.carets.empty()) {
		return;
	}
	clampCaretsToText(field);
	std::vector<TextReplacement> edits;
	for (const CaretState& caret : field.carets) {
		edits.push_back(TextReplacement{
			.oldRange = TextRange{caretSelectionStart(caret), caretSelectionEnd(caret)},
			.insertedText = utf8Text,
		});
	}
	std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) {
		if (a.oldRange.startByte != b.oldRange.startByte) return a.oldRange.startByte < b.oldRange.startByte;
		return a.oldRange.endByte < b.oldRange.endByte;
	});
	std::vector<TextReplacement> normalized;
	for (const TextReplacement& edit : edits) {
		if (!normalized.empty() && edit.oldRange.startByte <= normalized.back().oldRange.endByte) {
			normalized.back().oldRange.endByte = std::max(
				normalized.back().oldRange.endByte,
				edit.oldRange.endByte);
		} else {
			normalized.push_back(edit);
		}
	}
	(void)commitEdits(field, normalized, origin, true);
}

void InputFieldManager::applyDelete(FieldState& field, bool backspace, EditOrigin origin) {
	if (field.carets.empty()) {
		return;
	}
	clampCaretsToText(field);

	std::vector<TextReplacement> edits;
	for (const CaretState& caret : field.carets) {
		size_t eraseStart = 0u;
		size_t eraseEnd = 0u;

		if (caretHasSelection(caret)) {
			eraseStart = caretSelectionStart(caret);
			eraseEnd = caretSelectionEnd(caret);
		} else if (backspace) {
			eraseEnd = caret.headByteOffset;
			eraseStart = text_storage::previousUtf8Codepoint(field.storage, eraseEnd);
		} else {
			eraseStart = caret.headByteOffset;
			eraseEnd = text_storage::nextUtf8Codepoint(field.storage, eraseStart);
		}

		if (eraseStart < eraseEnd) edits.push_back(TextReplacement{TextRange{eraseStart, eraseEnd}, {}});
	}
	std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) { return a.oldRange.startByte < b.oldRange.startByte; });
	std::vector<TextReplacement> merged;
	for (const auto& edit : edits) {
		if (!merged.empty() && edit.oldRange.startByte <= merged.back().oldRange.endByte) {
			merged.back().oldRange.endByte = std::max(merged.back().oldRange.endByte, edit.oldRange.endByte);
		} else merged.push_back(edit);
	}
	(void)commitEdits(field, merged, origin, true);
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
			caret.hasPreferredX = false;
			continue;
		}

		const size_t currentHead = caret.headByteOffset;
		const size_t target = (direction < 0)
			? text_storage::previousUtf8Codepoint(field.storage, currentHead)
			: text_storage::nextUtf8Codepoint(field.storage, currentHead);

		if (selecting) {
			caret.headByteOffset = target;
		} else {
			caret.anchorByteOffset = target;
			caret.headByteOffset = target;
		}
		caret.hasPreferredX = false;
	}
	field.caretRevealPending = true;
}

void InputFieldManager::moveCaretsByWord(FieldState& field, int direction, bool selecting) {
	clampCaretsToText(field);
	for (CaretState& caret : field.carets) {
		size_t target = caret.headByteOffset;
		if (!selecting && caretHasSelection(caret)) {
			target = direction < 0 ? caretSelectionStart(caret) : caretSelectionEnd(caret);
		} else if (direction < 0) {
			while (target > 0) {
				const size_t previous = text_storage::previousUtf8Codepoint(field.storage, target);
				if (wordClass(static_cast<unsigned char>(text_storage::byteAt(field.storage, previous))) != 0) break;
				target = previous;
			}
			if (target > 0) {
				const size_t previous = text_storage::previousUtf8Codepoint(field.storage, target);
				const int klass = wordClass(static_cast<unsigned char>(text_storage::byteAt(field.storage, previous)));
				while (target > 0) {
					const size_t candidate = text_storage::previousUtf8Codepoint(field.storage, target);
					if (wordClass(static_cast<unsigned char>(text_storage::byteAt(field.storage, candidate))) != klass) break;
					target = candidate;
				}
			}
		} else {
			const size_t size = text_storage::byteCount(field.storage);
			if (target < size) {
				const int klass = wordClass(static_cast<unsigned char>(text_storage::byteAt(field.storage, target)));
				while (target < size && wordClass(static_cast<unsigned char>(text_storage::byteAt(field.storage, target))) == klass) {
					target = text_storage::nextUtf8Codepoint(field.storage, target);
				}
			}
			while (target < size && wordClass(static_cast<unsigned char>(text_storage::byteAt(field.storage, target))) == 0) {
				target = text_storage::nextUtf8Codepoint(field.storage, target);
			}
		}
		if (selecting) caret.headByteOffset = target;
		else caret.anchorByteOffset = caret.headByteOffset = target;
		caret.hasPreferredX = false;
	}
	markCaretBlinkReset();
	field.caretRevealPending = true;
}

void InputFieldManager::applyWordDelete(FieldState& field, bool backspace) {
	const std::vector<CaretState> original = field.carets;
	moveCaretsByWord(field, backspace ? -1 : +1, true);
	std::vector<TextReplacement> edits;
	for (size_t i = 0; i < field.carets.size(); ++i) {
		if (caretHasSelection(original[i])) {
			edits.push_back(TextReplacement{TextRange{caretSelectionStart(original[i]), caretSelectionEnd(original[i])}, {}});
		} else {
			edits.push_back(TextReplacement{TextRange{
				std::min(original[i].headByteOffset, field.carets[i].headByteOffset),
				std::max(original[i].headByteOffset, field.carets[i].headByteOffset)}, {}});
		}
	}
	field.carets = original;
	std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) { return a.oldRange.startByte < b.oldRange.startByte; });
	std::vector<TextReplacement> merged;
	for (const auto& edit : edits) {
		if (edit.oldRange.startByte == edit.oldRange.endByte) continue;
		if (!merged.empty() && edit.oldRange.startByte <= merged.back().oldRange.endByte) {
			merged.back().oldRange.endByte = std::max(merged.back().oldRange.endByte, edit.oldRange.endByte);
		} else merged.push_back(edit);
	}
	(void)commitEdits(field, merged, EditOrigin::Delete, true);
}

float InputFieldManager::fieldLineHeight(const FieldState& field) const {
	const detail::text::TextLayoutResult& layout = textLayoutService_->layout(
		detail::text::TextLayoutRequest{
			.text = "Mg",
			.fontView = &state_->fontView,
			.fontId = static_cast<FontId>(field.layout.fontId),
			.pointsToPixelsScale = state_->pointsToPixelsScale,
			.fontSize = field.layout.fontSize,
			.letterSpacing = field.layout.letterSpacing,
			.tabWidth = field.layout.tabWidth,
			.includeGlyphGeometry = false,
		});
	if (layout.success && layout.lineHeight > 0.0f) return layout.lineHeight;
	return std::max(
		1.0f,
		static_cast<float>(std::max<uint16_t>(1u, field.layout.fontSize)) *
			state_->pointsToPixelsScale);
}

float InputFieldManager::caretXInRange(
	const FieldState& field,
	TextRange range,
	size_t byteOffset) const {
	const std::string lineText = text_storage::copy(field.storage, range);
	const detail::text::TextLayoutResult& layout = textLayoutService_->layout(
		detail::text::TextLayoutRequest{
			.text = lineText,
			.fontView = &state_->fontView,
			.fontId = static_cast<FontId>(field.layout.fontId),
			.pointsToPixelsScale = state_->pointsToPixelsScale,
			.fontSize = field.layout.fontSize,
			.letterSpacing = field.layout.letterSpacing,
			.tabWidth = field.layout.tabWidth,
			.includeGlyphGeometry = false,
		});
	if (!layout.success || layout.caretStops.empty()) return 0.0f;
	const size_t local = std::min(byteOffset, range.endByte) - range.startByte;
	float x = 0.0f;
	for (const detail::text::TextCaretStop& stop : layout.caretStops) {
		if (stop.byteOffset > local) break;
		x = stop.x;
		if (stop.byteOffset == local) break;
	}
	return x;
}

std::vector<TextRange> InputFieldManager::visualRangesForHardLine(
	FieldState& field,
	size_t hardLineIndex) {
	const TextRange hardRange = text_storage::lineRange(field.storage, hardLineIndex);
	if (!field.config.softWrap || field.layout.viewportWidth <= 0.0f) return {hardRange};
	auto cached = field.wrapCacheByHardLine.find(hardLineIndex);
	if (cached != field.wrapCacheByHardLine.end() &&
		cached->second.revision == field.revision &&
		cached->second.hardLineRange.startByte == hardRange.startByte &&
		cached->second.hardLineRange.endByte == hardRange.endByte &&
		sameLayoutDescriptor(cached->second.layout, field.layout)) {
		return cached->second.visualRanges;
	}

	const std::string hardText = text_storage::copy(field.storage, hardRange);
	const detail::text::TextLayoutResult& layout = textLayoutService_->layout(
		detail::text::TextLayoutRequest{
			.text = hardText,
			.fontView = &state_->fontView,
			.fontId = static_cast<FontId>(field.layout.fontId),
			.pointsToPixelsScale = state_->pointsToPixelsScale,
			.fontSize = field.layout.fontSize,
			.letterSpacing = field.layout.letterSpacing,
			.tabWidth = field.layout.tabWidth,
			.includeGlyphGeometry = false,
		});
	std::vector<TextRange> ranges = detail::text::breakVisualLines(
		hardText, layout, field.layout.viewportWidth);
	for (TextRange& range : ranges) {
		range.startByte += hardRange.startByte;
		range.endByte += hardRange.startByte;
	}
	auto& entry = field.wrapCacheByHardLine[hardLineIndex];
	entry.revision = field.revision;
	entry.hardLineRange = hardRange;
	entry.layout = field.layout;
	entry.visualRanges = ranges;
	return ranges;
}

void InputFieldManager::moveCaretsVertical(
	FieldState& field,
	int direction,
	bool selecting) {
	if (direction == 0 || field.carets.empty() ||
		text_storage::storageMode(field.storage) != TextFieldMode::MultiLine) return;
	clampCaretsToText(field);
	const size_t hardLineCount = text_storage::lineCount(field.storage);
	for (CaretState& caret : field.carets) {
		const size_t hardLine = text_storage::lineFromByte(field.storage, caret.headByteOffset);
		std::vector<TextRange> ranges = visualRangesForHardLine(field, hardLine);
		size_t visualInHardLine = 0;
		for (size_t i = 0; i < ranges.size(); ++i) {
			if (caret.headByteOffset >= ranges[i].startByte &&
				(caret.headByteOffset < ranges[i].endByte || i + 1u == ranges.size())) {
				visualInHardLine = i;
				break;
			}
		}
		const TextRange currentRange = ranges[visualInHardLine];
		if (!caret.hasPreferredX) {
			caret.preferredX = caretXInRange(field, currentRange, caret.headByteOffset);
			caret.hasPreferredX = true;
		}

		TextRange targetRange = currentRange;
		if (direction < 0) {
			if (visualInHardLine > 0) targetRange = ranges[visualInHardLine - 1u];
			else if (hardLine > 0) {
				ranges = visualRangesForHardLine(field, hardLine - 1u);
				targetRange = ranges.back();
			}
		} else {
			if (visualInHardLine + 1u < ranges.size()) targetRange = ranges[visualInHardLine + 1u];
			else if (hardLine + 1u < hardLineCount) {
				ranges = visualRangesForHardLine(field, hardLine + 1u);
				targetRange = ranges.front();
			}
		}

		const std::string targetText = text_storage::copy(field.storage, targetRange);
		const detail::text::TextLayoutResult& targetLayout = textLayoutService_->layout(
			detail::text::TextLayoutRequest{
				.text = targetText,
				.fontView = &state_->fontView,
				.fontId = static_cast<FontId>(field.layout.fontId),
				.pointsToPixelsScale = state_->pointsToPixelsScale,
				.fontSize = field.layout.fontSize,
				.letterSpacing = field.layout.letterSpacing,
				.tabWidth = field.layout.tabWidth,
				.includeGlyphGeometry = false,
			});
		size_t target = targetRange.startByte;
		float bestDistance = std::numeric_limits<float>::max();
		for (const detail::text::TextCaretStop& stop : targetLayout.caretStops) {
			const float distance = std::abs(stop.x - caret.preferredX);
			if (distance < bestDistance) {
				bestDistance = distance;
				target = targetRange.startByte + stop.byteOffset;
			}
		}
		if (selecting) caret.headByteOffset = target;
		else caret.anchorByteOffset = caret.headByteOffset = target;
	}
	field.caretRevealPending = true;
}

void InputFieldManager::moveCaretsToLineBoundary(
	FieldState& field,
	bool toEnd,
	bool selecting) {
	clampCaretsToText(field);
	for (CaretState& caret : field.carets) {
		const size_t hardLine = text_storage::lineFromByte(field.storage, caret.headByteOffset);
		const std::vector<TextRange> ranges = visualRangesForHardLine(field, hardLine);
		TextRange targetRange = ranges.front();
		for (size_t i = 0; i < ranges.size(); ++i) {
			if (caret.headByteOffset >= ranges[i].startByte &&
				(caret.headByteOffset < ranges[i].endByte || i + 1u == ranges.size())) {
				targetRange = ranges[i];
				break;
			}
		}
		const size_t target = toEnd ? targetRange.endByte : targetRange.startByte;
		if (selecting) caret.headByteOffset = target;
		else caret.anchorByteOffset = caret.headByteOffset = target;
		caret.hasPreferredX = false;
	}
	field.caretRevealPending = true;
}

void InputFieldManager::revealPrimaryCaret(FieldState& field) {
	if (!field.caretRevealPending || field.carets.empty() ||
		field.layout.viewportHeight <= 0.0f) return;
	field.caretRevealPending = false;
	const float lineHeight = fieldLineHeight(field);
	const size_t hardLine = text_storage::lineFromByte(
		field.storage, field.carets.front().headByteOffset);
	size_t visualLine = hardLine;
	TextRange caretRange = text_storage::lineRange(field.storage, hardLine);
	if (field.config.softWrap) {
		visualLine = 0;
		size_t firstLineToCount = 0;
		for (const auto& [cachedLine, entry] : field.wrapCacheByHardLine) {
			if (cachedLine <= hardLine && entry.hasVisualLineStart &&
				cachedLine >= firstLineToCount) {
				firstLineToCount = cachedLine;
				visualLine = entry.visualLineStart;
			}
		}
		for (size_t line = firstLineToCount; line < hardLine; ++line) {
			visualLine += visualRangesForHardLine(field, line).size();
		}
		const std::vector<TextRange> ranges = visualRangesForHardLine(field, hardLine);
		for (size_t i = 0; i < ranges.size(); ++i) {
			if (field.carets.front().headByteOffset >= ranges[i].startByte &&
				(field.carets.front().headByteOffset < ranges[i].endByte || i + 1u == ranges.size())) {
				visualLine += i;
				caretRange = ranges[i];
				break;
			}
		}
	}
	const float caretTop = static_cast<float>(visualLine) * lineHeight;
	if (caretTop < field.scrollOffset.y) field.scrollOffset.y = caretTop;
	else if (caretTop + lineHeight > field.scrollOffset.y + field.layout.viewportHeight) {
		field.scrollOffset.y = caretTop + lineHeight - field.layout.viewportHeight;
	}
	field.scrollOffset.y = std::max(0.0f, field.scrollOffset.y);

	if (!field.config.softWrap && field.layout.viewportWidth > 0.0f) {
		const float caretX = caretXInRange(field, caretRange, field.carets.front().headByteOffset);
		if (caretX < field.scrollOffset.x) field.scrollOffset.x = caretX;
		else if (caretX > field.scrollOffset.x + field.layout.viewportWidth) {
			field.scrollOffset.x = caretX - field.layout.viewportWidth;
		}
		field.scrollOffset.x = std::max(0.0f, field.scrollOffset.x);
	} else {
		field.scrollOffset.x = 0.0f;
	}
}

void InputFieldManager::materializeVisibleLines(FieldState& field) {
	field.visibleLineStrings.clear();
	field.visibleLines.clear();
	const float lineHeight = fieldLineHeight(field);
	const float viewportHeight = field.layout.viewportHeight > 0.0f
		? field.layout.viewportHeight
		: lineHeight;
	const size_t overscan = 2;
	const size_t firstVisible = static_cast<size_t>(std::floor(
		std::max(0.0f, field.scrollOffset.y) / lineHeight));
	const size_t visibleCount = std::max<size_t>(
		1u, static_cast<size_t>(std::ceil(viewportHeight / lineHeight)));
	const size_t firstWanted = firstVisible > overscan ? firstVisible - overscan : 0u;
	const size_t lastWanted = firstVisible + visibleCount + overscan;

	std::vector<std::pair<TextRange, size_t>> materializedRanges;
	const size_t hardLineCount = text_storage::lineCount(field.storage);
	size_t totalVisualLines = hardLineCount;
	if (field.config.softWrap && text_storage::storageMode(field.storage) == TextFieldMode::MultiLine) {
		totalVisualLines = 0;
		size_t firstHardLine = 0;
		for (const auto& [cachedLine, entry] : field.wrapCacheByHardLine) {
			if (entry.hasVisualLineStart && entry.visualLineStart <= firstWanted &&
				entry.visualLineStart >= totalVisualLines) {
				firstHardLine = cachedLine;
				totalVisualLines = entry.visualLineStart;
			}
		}
		for (size_t hardLine = firstHardLine; hardLine < hardLineCount; ++hardLine) {
			const std::vector<TextRange> ranges = visualRangesForHardLine(field, hardLine);
			auto& cacheEntry = field.wrapCacheByHardLine[hardLine];
			cacheEntry.visualLineStart = totalVisualLines;
			cacheEntry.hasVisualLineStart = true;
			for (const TextRange range : ranges) {
				if (totalVisualLines >= firstWanted && totalVisualLines < lastWanted) {
					materializedRanges.emplace_back(range, totalVisualLines);
				}
				++totalVisualLines;
			}
			if (totalVisualLines >= lastWanted) {
				totalVisualLines += hardLineCount - hardLine - 1u;
				break;
			}
		}
	} else {
		const size_t end = std::min(hardLineCount, lastWanted);
		for (size_t line = std::min(firstWanted, hardLineCount); line < end; ++line) {
			materializedRanges.emplace_back(text_storage::lineRange(field.storage, line), line);
		}
	}

	field.visibleLineStrings.resize(materializedRanges.size());
	for (size_t i = 0; i < materializedRanges.size(); ++i) {
		field.visibleLineStrings[i] = text_storage::copy(
			field.storage, materializedRanges[i].first);
	}
	field.visibleLines.reserve(materializedRanges.size());
	float maximumVisibleAdvance = 0.0f;
	for (size_t i = 0; i < materializedRanges.size(); ++i) {
		const auto [range, visualIndex] = materializedRanges[i];
		Clay_ElementDeclaration declaration{};
		declaration.layout.sizing.height = CLAY_SIZING_FIXED(lineHeight);
		declaration.floating.offset = Clay_Vector2{
			-field.scrollOffset.x,
			static_cast<float>(visualIndex) * lineHeight - field.scrollOffset.y,
		};
		declaration.floating.attachPoints = Clay_FloatingAttachPoints{
			CLAY_ATTACH_POINT_LEFT_TOP,
			CLAY_ATTACH_POINT_LEFT_TOP,
		};
		declaration.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		declaration.floating.attachTo = CLAY_ATTACH_TO_PARENT;
		declaration.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
		field.visibleLines.push_back(VisibleTextLine{
			.text = field.visibleLineStrings[i],
			.logicalRange = range,
			.visualLineIndex = static_cast<uint32_t>(std::min<size_t>(
				visualIndex, std::numeric_limits<uint32_t>::max())),
			.declaration = declaration,
		});
		const detail::text::TextLayoutResult& layout = textLayoutService_->layout(
			detail::text::TextLayoutRequest{
				.text = field.visibleLineStrings[i],
				.fontView = &state_->fontView,
				.fontId = static_cast<FontId>(field.layout.fontId),
				.pointsToPixelsScale = state_->pointsToPixelsScale,
				.fontSize = field.layout.fontSize,
				.letterSpacing = field.layout.letterSpacing,
				.tabWidth = field.layout.tabWidth,
				.includeGlyphGeometry = false,
			});
		maximumVisibleAdvance = std::max(maximumVisibleAdvance, layout.measuredAdvance);
	}
	field.maximumScrollX = field.config.softWrap
		? 0.0f
		: std::max(field.maximumScrollX, maximumVisibleAdvance - field.layout.viewportWidth);
	field.maximumScrollY = std::max(
		0.0f, static_cast<float>(totalVisualLines) * lineHeight - viewportHeight);
	field.scrollOffset.x = std::clamp(field.scrollOffset.x, 0.0f, field.maximumScrollX);
	field.scrollOffset.y = std::clamp(field.scrollOffset.y, 0.0f, field.maximumScrollY);
}

void InputFieldManager::clampCaretsToText(FieldState& field) const {
	for (CaretState& caret : field.carets) {
		caret.anchorByteOffset = text_storage::clampUtf8Boundary(field.storage, caret.anchorByteOffset);
		caret.headByteOffset = text_storage::clampUtf8Boundary(field.storage, caret.headByteOffset);
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

	const FlowUi::Font::FontFaceData* fontFace = detail::text::resolveFontFace(&state_->fontView, textData.fontId);
	if (!fontFace) {
		const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, textData.fontSize)) * state_->pointsToPixelsScale;
		return static_cast<float>(text.length) * fallbackEmPixels * 0.5f;
	}

	const detail::text::TextLayoutResult& layoutResult = textLayoutService_->layout(
		detail::text::TextLayoutRequest{
			.text = std::string_view(text.chars, static_cast<size_t>(text.length)),
			.fontView = &state_->fontView,
			.fontId = static_cast<FontId>(textData.fontId),
			.pointsToPixelsScale = state_->pointsToPixelsScale,
			.fontSize = textData.fontSize,
			.letterSpacing = textData.letterSpacing,
			.includeGlyphGeometry = false,
		});

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
