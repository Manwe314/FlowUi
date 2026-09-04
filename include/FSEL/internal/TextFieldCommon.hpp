#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "managers/FlowUiElementBuilder.hpp"
#include "managers/InputFieldManager.hpp"

namespace FlowUi::FSEL {

enum class TextFieldSyncPolicy : uint8_t {
	Live = 0,
	OnCommit,
	ReadOnly,
};

struct TextFieldActions {
	ActionCall onChanged{};
	ActionCall onCommit{};
	ActionCall onSubmit{};
	ActionCall onFocus{};
	ActionCall onBlur{};
	ActionCall onUndoRequested{};
	ActionCall onRedoRequested{};
};

struct TextFieldStateOverrides {
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> textColor = std::nullopt;
	std::optional<Clay_Color> placeholderColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
};

struct TextFieldCaretOverrides {
	std::optional<InputCaretShape> shape = std::nullopt;
	std::optional<float> thicknessPx = std::nullopt;
	std::optional<float> blockWidthPx = std::nullopt;
	std::optional<float> heightOverflowTopPx = std::nullopt;
	std::optional<float> heightOverflowBottomPx = std::nullopt;
	std::optional<Clay_Color> color = std::nullopt;
	std::optional<Clay_Color> selectionBoxColor = std::nullopt;
	std::optional<Clay_Color> selectedTextColor = std::nullopt;
	std::optional<double> blinkPeriodSeconds = std::nullopt;
	std::optional<double> blinkVisibleSeconds = std::nullopt;
};

namespace detail::text_field {

struct State {
	bool wasFocused = false;
	bool editSessionDirty = false;
};

struct ActionNotifications {
	bool changed = false;
	bool committed = false;
	bool submitted = false;
	bool focused = false;
	bool blurred = false;
	bool undoRequested = false;
	bool redoRequested = false;
};

struct BuildResult {
	FieldQueryResult field{};
	ActionNotifications notifications{};
};

inline FSELTextFieldStateTheme applyOverrides(
	FSELTextFieldStateTheme appearance,
	const TextFieldStateOverrides& overrides) {
	appearance.backgroundColor = overrides.backgroundColor.value_or(
		appearance.backgroundColor);
	appearance.textColor = overrides.textColor.value_or(appearance.textColor);
	appearance.placeholderColor = overrides.placeholderColor.value_or(
		appearance.placeholderColor);
	appearance.borderColor = overrides.borderColor.value_or(
		appearance.borderColor);
	return appearance;
}

template <typename Parameters>
FSELTextFieldStateTheme resolveAppearance(
	const FSELTextFieldTheme& theme,
	const Parameters& params,
	bool enabled,
	bool focused,
	bool hovered) {
	if (!enabled) {
		return applyOverrides(theme.disabled, params.disabledOverrides);
	}
	if (!params.valid) {
		return applyOverrides(theme.invalid, params.invalidOverrides);
	}
	if (focused) {
		return applyOverrides(theme.focused, params.focusedOverrides);
	}
	if (params.readOnly || params.syncPolicy == TextFieldSyncPolicy::ReadOnly) {
		return applyOverrides(theme.readOnly, params.readOnlyOverrides);
	}
	if (hovered) {
		return applyOverrides(theme.hovered, params.hoveredOverrides);
	}
	return applyOverrides(theme.idle, params.idleOverrides);
}

inline InputFieldOverlayStyle resolveOverlayStyle(
	const FSELTextFieldTheme& theme,
	const TextFieldCaretOverrides& overrides) {
	InputFieldOverlayStyle style = theme.overlayStyle;
	style.caretShape = overrides.shape.value_or(style.caretShape);
	style.caretThicknessPx = overrides.thicknessPx.value_or(
		style.caretThicknessPx);
	style.caretBlockWidthPx = overrides.blockWidthPx.value_or(
		style.caretBlockWidthPx);
	style.caretHeightOverflowTopPx = overrides.heightOverflowTopPx.value_or(
		style.caretHeightOverflowTopPx);
	style.caretHeightOverflowBottomPx =
		overrides.heightOverflowBottomPx.value_or(
			style.caretHeightOverflowBottomPx);
	style.caretColor = overrides.color.value_or(style.caretColor);
	style.selectionBoxColor = overrides.selectionBoxColor.value_or(
		style.selectionBoxColor);
	style.selectedTextColor = overrides.selectedTextColor.value_or(
		style.selectedTextColor);
	style.caretBlinkPeriodSeconds = overrides.blinkPeriodSeconds.value_or(
		style.caretBlinkPeriodSeconds);
	style.caretBlinkVisibleSeconds = overrides.blinkVisibleSeconds.value_or(
		style.caretBlinkVisibleSeconds);
	return style;
}

inline bool textEquals(const FieldTextView& fieldText, std::string_view value) {
	if (fieldText.sizeBytes() != value.size()) {
		return false;
	}
	if (const auto contiguous = fieldText.contiguous()) {
		return *contiguous == value;
	}

	size_t offset = 0;
	bool equal = true;
	fieldText.forEachChunk(
		TextRange{0, fieldText.sizeBytes()},
		[&](std::string_view chunk) {
			if (!equal || offset + chunk.size() > value.size() ||
				value.substr(offset, chunk.size()) != chunk) {
				equal = false;
				return;
			}
			offset += chunk.size();
		});
	return equal && offset == value.size();
}

inline bool hasCommand(
	const FieldQueryResult& field,
	FieldCommandRequest command) {
	return std::ranges::find(field.commandRequests, command) !=
		field.commandRequests.end();
}

template <typename Context, typename Parameters>
BuildResult requestAndSynchronize(
	Context& context,
	const Parameters& params,
	TextFieldMode mode,
	Clay_ElementId textElementId,
	Clay_ElementId contentElementId,
	TextLayoutDescriptor layout,
	InputFieldOverlayStyle overlayStyle,
	bool softWrap,
	bool clearFocusOnSubmit) {
	auto& manager = context.uiManager.inputFields();
	auto& state = context.state();
	ActionNotifications notifications{};
	const bool enabled = params.enabled && params.value != nullptr;
	const bool editable = enabled && !params.readOnly &&
		params.syncPolicy != TextFieldSyncPolicy::ReadOnly;

	if (!enabled && state.wasFocused) {
		manager.requestCaret(context.id, CaretRequestKind::ClearAll);
	}

	FieldRequest request{
		.initialText = params.value ? std::string_view(*params.value) : std::string_view{},
		.config = FieldConfig{
			.mode = mode,
			.readOnly = !editable,
			.allowNewline = mode == TextFieldMode::MultiLine,
			.softWrap = mode == TextFieldMode::MultiLine && softWrap,
			.allowArrowNavigation = true,
			.maxBytes = params.maxBytes,
			.transactionDetail = params.transactionDetail,
		},
		.layout = layout,
		.overlayStyle = overlayStyle,
		.textElementId = enabled ? textElementId : Clay_ElementId{},
		.contentElementId = enabled ? contentElementId : Clay_ElementId{},
	};
	FieldQueryResult field = manager.requestField(context.id, request);

	const bool changedThisFrame = !field.transactions.empty();
	if (changedThisFrame) {
		state.editSessionDirty = true;
		if (params.value && params.syncPolicy == TextFieldSyncPolicy::Live) {
			*params.value = field.text.copy();
		}
		notifications.changed = true;
	}

	const bool focused = enabled && field.hasPrimaryCaret;
	if (!state.wasFocused && focused) {
		notifications.focused = true;
	}
	if (state.wasFocused && !focused) {
		if (params.value && state.editSessionDirty &&
			params.syncPolicy == TextFieldSyncPolicy::OnCommit) {
			*params.value = field.text.copy();
		}
		if (state.editSessionDirty) {
			notifications.committed = true;
		}
		notifications.blurred = true;
		state.editSessionDirty = false;
	}

	const bool submitted = hasCommand(field, FieldCommandRequest::Submit);
	notifications.undoRequested = hasCommand(field, FieldCommandRequest::Undo);
	notifications.redoRequested = hasCommand(field, FieldCommandRequest::Redo);
	if (submitted) {
		if (params.value && params.syncPolicy == TextFieldSyncPolicy::OnCommit) {
			*params.value = field.text.copy();
		}
		if (state.editSessionDirty) {
			notifications.committed = true;
			state.editSessionDirty = false;
		}
		notifications.submitted = true;
		if (clearFocusOnSubmit) {
			manager.requestCaret(context.id, CaretRequestKind::ClearAll);
			notifications.blurred = true;
			field.hasPrimaryCaret = false;
		}
	}

	const bool mayAcceptExternalValue = params.value && !changedThisFrame &&
		(params.syncPolicy != TextFieldSyncPolicy::OnCommit || !focused) &&
		!state.editSessionDirty;
	if (mayAcceptExternalValue && !textEquals(field.text, *params.value)) {
		(void)manager.replaceText(context.id, *params.value, focused);
		field = manager.requestField(context.id, request);
	}

	state.wasFocused = enabled && field.hasPrimaryCaret &&
		!(submitted && clearFocusOnSubmit);
	return BuildResult{
		.field = field,
		.notifications = notifications,
	};
}

template <typename Context, typename Parameters>
void invokeActionsAfterBuild(
	Context& context,
	const Parameters& params,
	const ActionNotifications& notifications) {
	if (notifications.changed) {
		(void)context.uiManager.invoke(params.actions.onChanged);
	}
	if (notifications.focused) {
		(void)context.uiManager.invoke(params.actions.onFocus);
	}
	if (notifications.committed) {
		(void)context.uiManager.invoke(params.actions.onCommit);
	}
	if (notifications.submitted) {
		(void)context.uiManager.invoke(params.actions.onSubmit);
	}
	if (notifications.blurred) {
		(void)context.uiManager.invoke(params.actions.onBlur);
	}
	if (notifications.undoRequested) {
		(void)context.uiManager.invoke(params.actions.onUndoRequested);
	}
	if (notifications.redoRequested) {
		(void)context.uiManager.invoke(params.actions.onRedoRequested);
	}
}

template <typename Parameters>
Clay_Sizing resolveSizing(
	const Parameters& params,
	const FSELTextFieldTheme& theme) {
	return params.sizing.value_or(Clay_Sizing{
		.width = CLAY_SIZING_FIXED(std::max(theme.width, 1.0f)),
		.height = CLAY_SIZING_FIXED(std::max(theme.height, 1.0f)),
	});
}

inline Clay_Dimensions resolveViewport(
	Clay_ElementId contentElementId,
	const FSELTextFieldTheme& theme,
	const Clay_Padding& padding,
	std::optional<float> widthOverride,
	std::optional<float> heightOverride) {
	Clay_Dimensions result{
		std::max(1.0f, theme.width - padding.left - padding.right),
		std::max(1.0f, theme.height - padding.top - padding.bottom),
	};
	const Clay_ElementData previousContent = Clay_GetElementData(contentElementId);
	if (previousContent.found) {
		result.width = std::max(previousContent.boundingBox.width, 1.0f);
		result.height = std::max(previousContent.boundingBox.height, 1.0f);
	}
	result.width = std::max(widthOverride.value_or(result.width), 1.0f);
	result.height = std::max(heightOverride.value_or(result.height), 1.0f);
	return result;
}

inline Clay_ElementDeclaration makeRootDeclaration(
	Clay_Sizing sizing,
	const Clay_Padding& padding,
	const Clay_BorderWidth& borderWidth,
	const Clay_CornerRadius& cornerRadius,
	const FSELTextFieldStateTheme& appearance) {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = sizing;
	declaration.layout.padding = padding;
	declaration.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
	declaration.backgroundColor = appearance.backgroundColor;
	declaration.cornerRadius = cornerRadius;
	declaration.clip = {.horizontal = true, .vertical = true};
	declaration.border = {
		.color = appearance.borderColor,
		.width = borderWidth,
	};
	return declaration;
}

inline Clay_ElementDeclaration makeContentDeclaration() {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	declaration.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
	declaration.clip = {.horizontal = true, .vertical = true};
	return declaration;
}

inline Clay_ElementDeclaration makePlaceholderDeclaration(int16_t floatingZIndex = 0) {
	Clay_ElementDeclaration declaration{};
	declaration.floating.attachPoints = {
		.element = CLAY_ATTACH_POINT_LEFT_TOP,
		.parent = CLAY_ATTACH_POINT_LEFT_TOP,
	};
	declaration.floating.pointerCaptureMode =
		CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
	declaration.floating.zIndex = floatingZIndex;
	declaration.floating.attachTo = CLAY_ATTACH_TO_PARENT;
	declaration.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
	return declaration;
}

} // namespace detail::text_field
} // namespace FlowUi::FSEL
