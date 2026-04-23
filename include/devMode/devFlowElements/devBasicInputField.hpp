#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devBasicInputFieldParams {
	std::string fieldId = "";
	std::string initialText = "";
	bool readOnly = false;
	bool allowNewline = false;
	bool allowArrowNavigation = true;
	size_t maxBytes = std::numeric_limits<size_t>::max();
	std::function<void(std::string_view)> onTextChangedCallback = nullptr;

	Clay_Padding padding = CLAY_PADDING_ALL(10);
	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(220),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_Color backgroundColor = FlowUi::Flow_Color("#cfcfcfff");
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	bool clipHorizontal = false;
	bool clipVertical = false;
	Clay_ChildAlignment childTextAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_TextElementConfigWrapMode textWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
	uint16_t fontId = 0;
	uint16_t fontSize = 14;
	Clay_Color textColor = FlowUi::Flow_Color("#000000ff");
};

struct devBasicInputFieldState {
	bool hasLastObservedText = false;
	std::string lastObservedText{};
};

using DevBasicInputFieldDef = FlowUi::ElementDefinition<
	devBasicInputFieldParams,
	devBasicInputFieldState,
	void,
	FLOW_DEF_ID("DevBasicInputField"),
	true>;

inline const DevBasicInputFieldDef kDevBasicInputField = {
	+[](DevBasicInputFieldDef::InteractionContext& context) {
		context.uiManager.requestCursor(FlowUi::CursorType::IBeam);
	},
	+[](DevBasicInputFieldDef::InteractionContext& context) {
		const std::string_view fieldId =
			context.params.fieldId.empty()
			? context.elementID
			: std::string_view(context.params.fieldId);
		context.uiManager.inputFields().requestCaret(
			fieldId,
			FlowUi::InputFieldManager::CaretRequestKind::SetPrimary);
	},
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevBasicInputFieldDef::BuildContext& context) {
		devBasicInputFieldState& state = DevBasicInputFieldDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const std::string_view fieldId =
			context.params.fieldId.empty()
			? context.elementID
			: std::string_view(context.params.fieldId);

		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		const Clay_ElementId contentId = context.uiManager.toClayEID(context.createChildElementId("content-box"));
		const std::string textElementPath = context.createChildElementId("text");
		const Clay_ElementId textId = context.uiManager.toClayEID(textElementPath);

		const FlowUi::InputFieldManager::FieldQueryResult result =
			context.uiManager.inputFields().requestField({
				.fieldId = fieldId,
				.initialText = context.params.initialText,
				.config = FlowUi::InputFieldManager::FieldConfig{
					.readOnly = context.params.readOnly,
					.allowNewline = context.params.allowNewline,
					.allowArrowNavigation = context.params.allowArrowNavigation,
					.maxBytes = context.params.maxBytes,
				},
				.textElementId = textId,
				.contentElementId = contentId,
			});

		const std::string resultText(result.text);
		if (!state.hasLastObservedText)
		{
			state.hasLastObservedText = true;
			state.lastObservedText = resultText;
		}
		else if (state.lastObservedText != resultText)
		{
			state.lastObservedText = resultText;
			if (context.params.onTextChangedCallback != nullptr)
			{
				context.params.onTextChangedCallback(resultText);
			}
		}

		Clay_LayoutConfig rootLayout{};
		rootLayout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		rootLayout.sizing = context.params.sizing;
		rootLayout.padding = context.params.padding;
		rootLayout.childAlignment = context.params.childTextAlignment;

		Clay_ElementDeclaration root{};
		root.layout = rootLayout;
		root.backgroundColor = context.params.backgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.clip = {
			.horizontal = context.params.clipHorizontal,
			.vertical = context.params.clipVertical,
		};
		root.border = {.color = context.params.borderColor, .width = context.params.borderWidth};

		Clay_ElementDeclaration content{};
		content.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height =
				context.params.allowNewline
				? CLAY_SIZING_FIT(0)
				: CLAY_SIZING_FIXED(
					std::max(1.0f, static_cast<float>(context.params.fontSize))),
		};
		content.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		content.layout.childAlignment = context.params.childTextAlignment;
		content.backgroundColor = FlowUi::Flow_Color("#00000000");

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = context.params.textWrapMode;
		textConfig.textAlignment = context.params.textAlignment;
		textConfig.fontId = context.params.fontId;

		CLAY(rootId, root){
			CLAY(contentId, content){
				CLAY(textId, {}){
					CLAY_TEXT(
						context.uiManager.toClayString(result.text),
						CLAY_TEXT_CONFIG(textConfig));
				};
			};
		};
	},
};
