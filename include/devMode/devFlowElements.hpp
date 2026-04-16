#pragma once

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <FlowUi/Flow.hpp>
#include "devMode/devIcons.hpp"
#include "devMode/devJson.hpp"


struct devDynamicSeparatorParams {
	enum class Orientation : uint8_t {
		Vertical,
		Horizontal,
	};

	Orientation orientation = Orientation::Vertical;
	bool reverseDrag = false;
	int width = 4;
	int height = 4;
	Clay_Color color = FlowUi::Flow_Color("#5e646eff");
	Clay_Color hoverColor = FlowUi::Flow_Color("#7a828fff");
	Clay_Color activeColor = FlowUi::Flow_Color("#9aa2aeff");

	int minValue = 0;
	int maxValue = 100000;
	std::function<int()> getValue = nullptr;
	std::function<void(int)> setValue = nullptr;
};

struct devDynamicSeparatorState {
	bool isPressed = false;
	bool isDragging = false;
	float pressMouseAxis = 0.0f;
	int pressValue = 0;
	int localValue = 0;
};

using DevDynamicSeparatorDef = FlowUi::ElementDefinition<
	devDynamicSeparatorParams,
	devDynamicSeparatorState,
	void,
	FLOW_DEF_ID("DevDynamicSeparator"),
	true>;

inline const DevDynamicSeparatorDef kDevDynamicSeparator = {
	+[](DevDynamicSeparatorDef::InteractionContext& context) {
		devDynamicSeparatorState& state = DevDynamicSeparatorDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		state.isPressed = true;
		state.isDragging = true;
		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		state.pressMouseAxis =
			(context.params.orientation == devDynamicSeparatorParams::Orientation::Horizontal)
			? input.mouseY
			: input.mouseX;

		int minValue = context.params.minValue;
		int maxValue = context.params.maxValue;
		if (maxValue < minValue)
		{
			maxValue = minValue;
		}

		int baseValue = state.localValue;
		if (context.params.getValue != nullptr)
		{
			baseValue = context.params.getValue();
		}
		if (baseValue < minValue)
		{
			baseValue = minValue;
		}
		else if (baseValue > maxValue)
		{
			baseValue = maxValue;
		}
		state.pressValue = baseValue;
	},
	nullptr,
	nullptr,
	+[](DevDynamicSeparatorDef::InteractionContext& context) {
		devDynamicSeparatorState& state = DevDynamicSeparatorDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		state.isPressed = false;
		state.isDragging = false;
	},
	+[](DevDynamicSeparatorDef::InteractionContext& context) {
		devDynamicSeparatorState& state = DevDynamicSeparatorDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		if (!input.mouseDown[0])
		{
			state.isPressed = false;
			state.isDragging = false;
			return;
		}
		if (!state.isDragging)
		{
			return;
		}

		int minValue = context.params.minValue;
		int maxValue = context.params.maxValue;
		if (maxValue < minValue)
		{
			maxValue = minValue;
		}

		const float currentAxis =
			(context.params.orientation == devDynamicSeparatorParams::Orientation::Horizontal)
			? input.mouseY
			: input.mouseX;
		const float deltaAxis = currentAxis - state.pressMouseAxis;
		const int deltaPixels = static_cast<int>(std::lround(deltaAxis));
		const int signedDelta = context.params.reverseDrag ? -deltaPixels : deltaPixels;
		int nextValue = state.pressValue + signedDelta;
		if (nextValue < minValue)
		{
			nextValue = minValue;
		}
		else if (nextValue > maxValue)
		{
			nextValue = maxValue;
		}

		state.localValue = nextValue;
		if (context.params.setValue != nullptr)
		{
			context.params.setValue(nextValue);
		}
	},
	nullptr,
	+[](DevDynamicSeparatorDef::BuildContext& context) {
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		const devDynamicSeparatorState* state = DevDynamicSeparatorDef::tryGetStateConst(FlowUi::toFlowId(context.elementID));
		const FlowUi::InteractionSnapshot& previousInteraction = context.uiManager.getPreviousFramesInteraction();

		int width = context.params.width;
		int height = context.params.height;
		if (width < 1)
		{
			width = 1;
		}
		if (height < 1)
		{
			height = 1;
		}

		Clay_Color separatorColor = context.params.color;
		if (state && state->isPressed)
		{
			separatorColor = context.params.activeColor;
		}
		else if (previousInteraction.isHovered(rootId))
		{
			separatorColor = context.params.hoverColor;
		}

		Clay_LayoutConfig rootLayout{};
		if (context.params.orientation == devDynamicSeparatorParams::Orientation::Horizontal)
		{
			rootLayout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(static_cast<float>(height)),
			};
		}
		else
		{
			rootLayout.sizing = {
				.width = CLAY_SIZING_FIXED(static_cast<float>(width)),
				.height = CLAY_SIZING_GROW(0),
			};
		}

		Clay_ElementDeclaration root{};
		root.id = rootId;
		root.layout = rootLayout;
		root.backgroundColor = separatorColor;
		root.cornerRadius = CLAY_CORNER_RADIUS(0);
		root.border = {.color = FlowUi::Flow_Color("#00000000"), .width = Clay_BorderWidth{0, 0, 0, 0, 0}};

		CLAY(root){};
	},
};


struct devBasicButtonParams;
using DevBasicButtonDef = FlowUi::ElementDefinition<
	devBasicButtonParams,
	void,
	void,
	FLOW_DEF_ID("DevBasicButton"),
	true>;
using DevBasicButtonInteractionContext = DevBasicButtonDef::InteractionContext;

struct devBasicButtonParams {
	enum class ContentMode : uint8_t {
		None,
		TextOnly,
		IconOnly,
		IconThenText,
		TextThenIcon,
	};

	std::string text = "";
	FlowUi::TextureRef icon = FlowUi::TextureRef{};
	std::function<void(DevBasicButtonInteractionContext)> onHoveredCallback = nullptr;
	std::function<void(DevBasicButtonInteractionContext)> onPressedCallback = nullptr;
	ContentMode contentMode = ContentMode::None;

	Clay_Padding padding = CLAY_PADDING_ALL(10);
	Clay_Sizing sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)};
	Clay_Color backgroundColor = FlowUi::Flow_Color("#cfcfcfff");
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_LayoutDirection childLayoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ChildAlignment childAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	uint16_t childGap = 8;

	Clay_TextElementConfigWrapMode textWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
	uint16_t fontId = 0;
	uint16_t fontSize = 16;
	Clay_Color textColor = FlowUi::Flow_Color("#000000ff");

	Clay_Sizing iconContainerSizing = Clay_Sizing{.width = CLAY_SIZING_FIXED(18), .height = CLAY_SIZING_FIXED(18)};
	Clay_Padding iconContainerPadding = CLAY_PADDING_ALL(0);
	Clay_LayoutDirection iconContainerChildLayoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ChildAlignment iconContainerChildAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	uint16_t iconContainerChildGap = 0;
	Clay_Color iconContainerBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color iconContainerBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth iconContainerBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};

	Clay_Sizing iconSizing = Clay_Sizing{.width = CLAY_SIZING_PERCENT(1.0f), .height = CLAY_SIZING_PERCENT(1.0f)};
	Clay_Color iconTintColor = FlowUi::Flow_Color("#00000000");
};

inline const DevBasicButtonDef kDevBasicButton = {
	+[](DevBasicButtonDef::InteractionContext& context) {
		if (context.params.onHoveredCallback != nullptr)
		{
			context.params.onHoveredCallback(context);
		}
	},
	+[](DevBasicButtonDef::InteractionContext& context) {
		if (context.params.onPressedCallback != nullptr)
		{
			context.params.onPressedCallback(context);
		}
	},
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevBasicButtonDef::BuildContext& context) {
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);

		const devBasicButtonParams::ContentMode contentMode = context.params.contentMode;
		const bool needsText =
			contentMode == devBasicButtonParams::ContentMode::TextOnly ||
			contentMode == devBasicButtonParams::ContentMode::IconThenText ||
			contentMode == devBasicButtonParams::ContentMode::TextThenIcon;
		const bool needsIcon =
			contentMode == devBasicButtonParams::ContentMode::IconOnly ||
			contentMode == devBasicButtonParams::ContentMode::IconThenText ||
			contentMode == devBasicButtonParams::ContentMode::TextThenIcon;

		Clay_LayoutConfig rootLayout{};
		rootLayout.layoutDirection = context.params.childLayoutDirection;
		rootLayout.sizing = context.params.sizing;
		rootLayout.padding = context.params.padding;
		rootLayout.childAlignment = context.params.childAlignment;
		rootLayout.childGap = context.params.childGap;

		Clay_ElementDeclaration root{};
		root.id = rootId;
		root.layout = rootLayout;
		root.backgroundColor = context.params.backgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.border = {.color = context.params.borderColor, .width = context.params.borderWidth};

		Clay_TextElementConfig textConfig{};
		Clay_ElementId textId{};
		if (needsText)
		{
			const std::string textPath = context.createChildElementId("text");
			textId = context.uiManager.toClayEID(textPath);
			textConfig.textColor = context.params.textColor;
			textConfig.fontSize = context.params.fontSize;
			textConfig.wrapMode = context.params.textWrapMode;
			textConfig.textAlignment = context.params.textAlignment;
			textConfig.fontId = context.params.fontId;
		}

		Clay_ElementDeclaration iconContainer{};
		Clay_ElementDeclaration iconElement{};
		if (needsIcon)
		{
			const std::string iconContainerPath = context.createChildElementId("icon-container");
			const std::string iconPath = context.createChildElementId("icon");
			const Clay_ElementId iconContainerId = context.uiManager.toClayEID(iconContainerPath);
			const Clay_ElementId iconId = context.uiManager.toClayEID(iconPath);

			Clay_LayoutConfig iconContainerLayout{};
			iconContainerLayout.layoutDirection = context.params.iconContainerChildLayoutDirection;
			iconContainerLayout.sizing = context.params.iconContainerSizing;
			iconContainerLayout.padding = context.params.iconContainerPadding;
			iconContainerLayout.childAlignment = context.params.iconContainerChildAlignment;
			iconContainerLayout.childGap = context.params.iconContainerChildGap;

			iconContainer.id = iconContainerId;
			iconContainer.layout = iconContainerLayout;
			iconContainer.backgroundColor = context.params.iconContainerBackgroundColor;
			iconContainer.border = {.color = context.params.iconContainerBorderColor, .width = context.params.iconContainerBorderWidth};

			Clay_LayoutConfig iconLayout{};
			iconLayout.sizing = context.params.iconSizing;

			iconElement.id = iconId;
			iconElement.layout = iconLayout;
			iconElement.backgroundColor = context.params.iconTintColor;
			iconElement.image = {
				.imageData = context.uiManager.storeTexture(context.params.icon),
			};
		}

		auto drawTextChild = [&]() {
			CLAY({.id = textId}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.text),
					CLAY_TEXT_CONFIG(textConfig)
				);
			};
		};

		auto drawIconChild = [&]() {
			CLAY(iconContainer){
				CLAY(iconElement){};
			};
		};

		CLAY(root){
			switch (contentMode)
			{
			case devBasicButtonParams::ContentMode::None:
				break;
			case devBasicButtonParams::ContentMode::TextOnly:
				drawTextChild();
				break;
			case devBasicButtonParams::ContentMode::IconOnly:
				drawIconChild();
				break;
			case devBasicButtonParams::ContentMode::IconThenText:
				drawIconChild();
				drawTextChild();
				break;
			case devBasicButtonParams::ContentMode::TextThenIcon:
				drawTextChild();
				drawIconChild();
				break;
			}
		};
	},
};

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
		(void)context;
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
		root.id = rootId;
		root.layout = rootLayout;
		root.backgroundColor = context.params.backgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.clip = {.horizontal = true, .vertical = true};
		root.border = {.color = context.params.borderColor, .width = context.params.borderWidth};

		Clay_ElementDeclaration content{};
		content.id = contentId;
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

		CLAY(root){
			CLAY(content){
				CLAY({.id = textId}){
					CLAY_TEXT(
						context.uiManager.toClayString(result.text),
						CLAY_TEXT_CONFIG(textConfig));
				};
			};
		};
	},
};

struct devBasicToggleParams;
struct devBasicToggleState {
	bool initialized = false;
	bool isEnabled = false;
};

using DevBasicToggleDef = FlowUi::ElementDefinition<
	devBasicToggleParams,
	devBasicToggleState,
	void,
	FLOW_DEF_ID("DevBasicToggle"),
	true>;
using DevBasicToggleInteractionContext = DevBasicToggleDef::InteractionContext;

struct devBasicToggleParams {
	bool defaultEnabled = false;
	std::string text = "";
	std::function<void(DevBasicToggleInteractionContext, bool)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(26),
		.height = CLAY_SIZING_FIXED(26),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(2);
	Clay_Color enabledBackgroundColor = FlowUi::Flow_Color("#4b8c5aff");
	Clay_Color disabledBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);
	Clay_ChildAlignment childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_TextElementConfigWrapMode textWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_CENTER;
	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
};

inline const DevBasicToggleDef kDevBasicToggle = {
	nullptr,
	+[](DevBasicToggleDef::InteractionContext& context) {
		devBasicToggleState& state = DevBasicToggleDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		if (!state.initialized)
		{
			state.isEnabled = context.params.defaultEnabled;
			state.initialized = true;
		}
		state.isEnabled = !state.isEnabled;
		if (context.params.onValueChangedCallback != nullptr)
		{
			context.params.onValueChangedCallback(context, state.isEnabled);
		}
	},
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevBasicToggleDef::BuildContext& context) {
		devBasicToggleState& state = DevBasicToggleDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		if (!state.initialized)
		{
			state.isEnabled = context.params.defaultEnabled;
			state.initialized = true;
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.padding;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.childAlignment = context.params.childAlignment;
		root.backgroundColor =
			state.isEnabled
			? context.params.enabledBackgroundColor
			: context.params.disabledBackgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.border = {.color = context.params.borderColor, .width = context.params.borderWidth};

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = context.params.textWrapMode;
		textConfig.textAlignment = context.params.textAlignment;

		CLAY(root){
			if (!context.params.text.empty())
			{
				CLAY({
					.id = context.uiManager.toClayEID(context.createChildElementId("text")),
					.layout = {
						.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_GROW(0),
						},
					},
				}){
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.text),
						CLAY_TEXT_CONFIG(textConfig));
				};
			}
		};
	},
};


struct devHeaderParams {
	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	Clay_LayoutDirection childLayoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ChildAlignment childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	uint16_t childGap = 8;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	std::string titleText = "FlowUi Dev tool";
	uint16_t titleFontId = 0;
	uint16_t titleFontSize = 12;
	Clay_Color titleTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_TextElementConfigWrapMode titleWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment titleTextAlignment = CLAY_TEXT_ALIGN_LEFT;

	std::string exportButtonText = "Export";
	uint16_t exportButtonFontId = 0;
	uint16_t exportButtonFontSize = 12;
	Clay_Color exportButtonTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_Color exportButtonBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color exportButtonHoverBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color exportButtonBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth exportButtonBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	Clay_CornerRadius exportButtonCornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Padding exportButtonPadding = CLAY_PADDING_ALL(8);
	uint16_t exportButtonChildGap = 6;
	Clay_Sizing exportButtonIconContainerSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(14),
		.height = CLAY_SIZING_FIXED(14),
	};
	Clay_Sizing exportButtonIconSizing = Clay_Sizing{
		.width = CLAY_SIZING_PERCENT(1.0f),
		.height = CLAY_SIZING_PERCENT(1.0f),
	};
	Clay_Color exportButtonIconTintColor = FlowUi::Flow_Color("#00000000");
};

struct devHeaderResources {
	bool exportIconPrepared = false;
	FlowUi::TextureRef exportIcon = FlowUi::TextureRef{};
};

using DevHeaderDef = FlowUi::ElementDefinition<
	devHeaderParams,
	void,
	devHeaderResources,
	FLOW_DEF_ID("DevHeader"),
	true>;

inline const DevHeaderDef kDevHeader = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHeaderDef::BuildContext& context) {
		FlowUi::TextureRef exportIcon{};
		if (DevHeaderDef::resources.has_value()) {
			const devHeaderResources& resources = *DevHeaderDef::resources;
			if (resources.exportIconPrepared) {
				exportIcon = resources.exportIcon;
			}
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.padding;
		root.layout.layoutDirection = context.params.childLayoutDirection;
		root.layout.childAlignment = context.params.childAlignment;
		root.layout.childGap = context.params.childGap;
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig titleTextConfig{};
		titleTextConfig.textColor = context.params.titleTextColor;
		titleTextConfig.fontId = context.params.titleFontId;
		titleTextConfig.fontSize = context.params.titleFontSize;
		titleTextConfig.wrapMode = context.params.titleWrapMode;
		titleTextConfig.textAlignment = context.params.titleTextAlignment;

		const Clay_ElementId titleId = context.uiManager.toClayEID(context.createChildElementId("title"));
		const Clay_ElementId spacerId = context.uiManager.toClayEID(context.createChildElementId("spacer"));
		const std::string exportButtonId = context.createChildElementId("export-button");

		CLAY(root){
			CLAY({.id = titleId}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.titleText),
					CLAY_TEXT_CONFIG(titleTextConfig));
			};

			CLAY({
				.id = spacerId,
				.layout = {
					.sizing = {
						.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIT(0),
					},
				},
			}){};

			const Clay_Color hoverBackgroundColor = context.params.exportButtonHoverBackgroundColor;
			context.uiManager
				.createElement(kDevBasicButton, exportButtonId)
				.setParameters(devBasicButtonParams{
					.text = context.params.exportButtonText,
					.icon = exportIcon,
					.onHoveredCallback = [hoverBackgroundColor](DevBasicButtonInteractionContext buttonContext) {
						buttonContext.params.backgroundColor = hoverBackgroundColor;
					},
					.onPressedCallback = [](DevBasicButtonInteractionContext buttonContext) {
						(void)FlowUi::devMode::exportOverridesAsJson(buttonContext.uiManager);
					},
					.contentMode = devBasicButtonParams::ContentMode::IconThenText,
					.padding = context.params.exportButtonPadding,
					.sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
					.backgroundColor = context.params.exportButtonBackgroundColor,
					.cornerRadius = context.params.exportButtonCornerRadius,
					.borderColor = context.params.exportButtonBorderColor,
					.borderWidth = context.params.exportButtonBorderWidth,
					.childLayoutDirection = CLAY_LEFT_TO_RIGHT,
					.childAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
					.childGap = context.params.exportButtonChildGap,
					.textWrapMode = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_LEFT,
					.fontId = context.params.exportButtonFontId,
					.fontSize = context.params.exportButtonFontSize,
					.textColor = context.params.exportButtonTextColor,
					.iconContainerSizing = context.params.exportButtonIconContainerSizing,
					.iconContainerPadding = CLAY_PADDING_ALL(0),
					.iconContainerChildLayoutDirection = CLAY_LEFT_TO_RIGHT,
					.iconContainerChildAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
					.iconContainerChildGap = 0,
					.iconContainerBackgroundColor = FlowUi::Flow_Color("#00000000"),
					.iconContainerBorderColor = FlowUi::Flow_Color("#00000000"),
					.iconContainerBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
					.iconSizing = context.params.exportButtonIconSizing,
					.iconTintColor = context.params.exportButtonIconTintColor,
				})
				.draw();
		};
	},
};

struct devPanelContentParams {
	int defaultHierarchyWidthPx = 280;
	int minHierarchyWidthPx = 180;
	int maxHierarchyWidthPx = 640;
	int separatorThicknessPx = 6;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color hierarchyBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color propertiesBackgroundColor = FlowUi::Flow_Color("#00000000");
};

struct devPanelContentState {
	bool isViewingInstances = true;
	int hierarchyWidthPx = 0;
	bool hierarchyWidthInitialized = false;
	std::string selectedElementId = "";
};

using DevPanelContentDef = FlowUi::ElementDefinition<
	devPanelContentParams,
	devPanelContentState,
	void,
	FLOW_DEF_ID("DevPanelContent"),
	true>;

inline devPanelContentState* findSingleDevPanelContentState() {
	constexpr std::string_view kSingleDevPanelContentElementId = "flowui/dev/debug-view/main-view/content";

	devPanelContentState* state =
		DevPanelContentDef::tryGetState(FlowUi::toFlowId(kSingleDevPanelContentElementId));
	if (state != nullptr)
	{
		return state;
	}
	if (!DevPanelContentDef::statePool.empty())
	{
		return &DevPanelContentDef::statePool.front().second;
	}
	return nullptr;
}

inline const devPanelContentState* findSingleDevPanelContentStateConst() {
	constexpr std::string_view kSingleDevPanelContentElementId = "flowui/dev/debug-view/main-view/content";

	const devPanelContentState* state =
		DevPanelContentDef::tryGetStateConst(FlowUi::toFlowId(kSingleDevPanelContentElementId));
	if (state != nullptr)
	{
		return state;
	}
	if (!DevPanelContentDef::statePool.empty())
	{
		return &DevPanelContentDef::statePool.front().second;
	}
	return nullptr;
}

enum class devPropertiesSelectionKind : uint8_t {
	None = 0,
	Instance = 1,
	Definition = 2,
};

enum class devPropertiesStructScope : uint8_t {
	Parameters = 0,
	State = 1,
	Resources = 2,
};

struct devPropertiesSelectionNode {
	devPropertiesSelectionKind kind = devPropertiesSelectionKind::None;
	devPropertiesStructScope structScope = devPropertiesStructScope::Parameters;
	uint64_t definitionId = 0u;
	uint64_t definitionTypeHash = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};
	std::string definitionDisplayName{};
	std::string definitionTypeToken{};
	std::string authoredInstanceKey{};
	std::string authoredDefinitionKey{};
	uint64_t sourceLocationHash = 0u;
};

inline bool isDevPropertiesSelectionNull(const devPropertiesSelectionNode& selection) {
	return selection.kind == devPropertiesSelectionKind::None || selection.definitionId == 0u;
}

inline devPropertiesSelectionNode makeDevPropertiesSelectionFromInstanceNode(
	const FlowUi::devMode::ElementTreePlaceholder::FlatNode& node) {
	devPropertiesSelectionNode selection{};
	selection.kind = devPropertiesSelectionKind::Instance;
	selection.structScope = devPropertiesStructScope::Parameters;
	selection.definitionId = node.definitionId;
	selection.definitionTypeHash = node.definitionTypeHash;
	selection.elementId = node.elementId;
	selection.flowId = (node.flowId != 0u) ? node.flowId : FlowUi::toFlowId(selection.elementId);
	selection.definitionDisplayName = node.definitionDisplayName;
	selection.definitionTypeToken = node.definitionTypeToken;
	selection.authoredInstanceKey = node.authoredInstanceKey;
	selection.authoredDefinitionKey = node.authoredDefinitionKey;
	selection.sourceLocationHash = node.sourceLocationHash;
	return selection;
}

inline devPropertiesSelectionNode makeDevPropertiesSelectionFromDefinitionDescriptor(
	const FlowUi::devMode::ElementDescriptor& descriptor,
	devPropertiesStructScope structScope) {
	devPropertiesSelectionNode selection{};
	selection.kind = devPropertiesSelectionKind::Definition;
	selection.structScope = structScope;
	selection.definitionId = descriptor.definitionId;
	selection.definitionTypeHash = descriptor.definitionTypeHash;
	selection.definitionDisplayName = descriptor.definitionName;
	selection.definitionTypeToken = descriptor.definitionTypeToken;
	selection.authoredDefinitionKey = descriptor.definitionName;
	return selection;
}

inline bool setSelectedDevPropertiesNode(const devPropertiesSelectionNode& selection);


struct devHierarchyHeadParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t childGap = 8;

	std::string titleText = "Hierarchy";
	uint16_t titleFontId = 0;
	uint16_t titleFontSize = 12;
	Clay_Color titleTextColor = FlowUi::Flow_Color("#ffffffff");

	std::string instancesButtonText = "Instances";
	std::string definitionsButtonText = "Definitions";
	Clay_Color modeButtonActiveBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color modeButtonInactiveBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color modeButtonTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_Color modeButtonBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth modeButtonBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	Clay_CornerRadius modeButtonCornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Padding modeButtonPadding = CLAY_PADDING_ALL(8);
	uint16_t modeButtonsRowGap = 6;
	uint16_t modeButtonFontId = 0;
	uint16_t modeButtonFontSize = 12;
};

using DevHierarchyHeadDef = FlowUi::ElementDefinition<
	devHierarchyHeadParams,
	void,
	void,
	FLOW_DEF_ID("DevHierarchyHead"),
	true>;

inline const DevHierarchyHeadDef kDevHierarchyHead = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHierarchyHeadDef::BuildContext& context) {
		bool isViewingInstances = true;
		const devPanelContentState* state = findSingleDevPanelContentStateConst();
		if (state != nullptr)
		{
			isViewingInstances = state->isViewingInstances;
		}

		const Clay_Color instancesButtonColor =
			isViewingInstances
			? context.params.modeButtonActiveBackgroundColor
			: context.params.modeButtonInactiveBackgroundColor;
		const Clay_Color definitionsButtonColor =
			isViewingInstances
			? context.params.modeButtonInactiveBackgroundColor
			: context.params.modeButtonActiveBackgroundColor;

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig titleTextConfig{};
		titleTextConfig.textColor = context.params.titleTextColor;
		titleTextConfig.fontId = context.params.titleFontId;
		titleTextConfig.fontSize = context.params.titleFontSize;
		titleTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		titleTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		Clay_ElementDeclaration modeButtonsRow{};
		modeButtonsRow.id = context.uiManager.toClayEID(context.createChildElementId("mode-buttons-row"));
		modeButtonsRow.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		modeButtonsRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		modeButtonsRow.layout.childGap = context.params.modeButtonsRowGap;
		modeButtonsRow.backgroundColor = FlowUi::Flow_Color("#00000000");

		CLAY(root){
			CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("title"))}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.titleText),
					CLAY_TEXT_CONFIG(titleTextConfig));
			};

			CLAY(modeButtonsRow){
				context.uiManager
					.createElement(kDevBasicButton, context.createChildElementId("instances-button"))
					.setParameters(devBasicButtonParams{
						.text = context.params.instancesButtonText,
						.onPressedCallback = [](DevBasicButtonInteractionContext) {
							devPanelContentState* panelState = findSingleDevPanelContentState();
							if (panelState != nullptr)
							{
								panelState->isViewingInstances = true;
							}
						},
						.contentMode = devBasicButtonParams::ContentMode::TextOnly,
						.padding = context.params.modeButtonPadding,
						.sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
						.backgroundColor = instancesButtonColor,
						.cornerRadius = context.params.modeButtonCornerRadius,
						.borderColor = context.params.modeButtonBorderColor,
						.borderWidth = context.params.modeButtonBorderWidth,
						.textWrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_LEFT,
						.fontId = context.params.modeButtonFontId,
						.fontSize = context.params.modeButtonFontSize,
						.textColor = context.params.modeButtonTextColor,
					})
					.draw();

				context.uiManager
					.createElement(kDevBasicButton, context.createChildElementId("definitions-button"))
					.setParameters(devBasicButtonParams{
						.text = context.params.definitionsButtonText,
						.onPressedCallback = [](DevBasicButtonInteractionContext) {
							devPanelContentState* panelState = findSingleDevPanelContentState();
							if (panelState != nullptr)
							{
								panelState->isViewingInstances = false;
							}
						},
						.contentMode = devBasicButtonParams::ContentMode::TextOnly,
						.padding = context.params.modeButtonPadding,
						.sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
						.backgroundColor = definitionsButtonColor,
						.cornerRadius = context.params.modeButtonCornerRadius,
						.borderColor = context.params.modeButtonBorderColor,
						.borderWidth = context.params.modeButtonBorderWidth,
						.textWrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_LEFT,
						.fontId = context.params.modeButtonFontId,
						.fontSize = context.params.modeButtonFontSize,
						.textColor = context.params.modeButtonTextColor,
					})
					.draw();
			};
		};
	},
};


struct devHierarchyContentParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	int panelWidthPx = 280;
	int minRowContentWidthPx = 180;
	int firstIndentDepthCount = 6;
	int firstIndentStepPx = 20;
	int secondIndentDepthCount = 5;
	int secondIndentStepPx = 13;
	int thirdIndentDepthCount = 4;
	int thirdIndentStepPx = 8;
	int arrowSlotWidthPx = 18;
	Clay_Color rowBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color selectedRowBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color disclosureButtonBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding disclosureButtonPadding = CLAY_PADDING_ALL(2);
	Clay_Sizing disclosureIconContainerSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(12),
		.height = CLAY_SIZING_FIXED(12),
	};
	std::string emptyInstancesText = "No instances captured";
	std::string emptyDefinitionsText = "No definitions registered";
	std::string parametersTypeText = "parameters";
	std::string stateTypeText = "state";
	std::string resourcesTypeText = "resources";
};

struct hierarchyEntryUiState {
	bool expanded = true;
};

struct devHierarchyContentState {
	std::vector<hierarchyEntryUiState> entryStates{};
	std::vector<uint64_t> entryKeys{};
	std::vector<hierarchyEntryUiState> definitionEntryStates{};
	std::vector<uint64_t> definitionEntryKeys{};
};

struct devHierarchyContentResources {
	bool disclosureIconsPrepared = false;
	FlowUi::TextureRef downArrowIcon = FlowUi::TextureRef{};
	FlowUi::TextureRef rightArrowIcon = FlowUi::TextureRef{};
};

using DevHierarchyContentDef = FlowUi::ElementDefinition<
	devHierarchyContentParams,
	devHierarchyContentState,
	devHierarchyContentResources,
	FLOW_DEF_ID("DevHierarchyContent"),
	true>;

inline devHierarchyContentState* findSingleDevHierarchyContentState() {
	constexpr std::string_view kSingleDevHierarchyContentElementId =
		"flowui/dev/debug-view/main-view/content/hierarchy/content";

	devHierarchyContentState* state =
		DevHierarchyContentDef::tryGetState(FlowUi::toFlowId(kSingleDevHierarchyContentElementId));
	if (state != nullptr)
	{
		return state;
	}
	if (!DevHierarchyContentDef::statePool.empty())
	{
		return &DevHierarchyContentDef::statePool.front().second;
	}
	return nullptr;
}

inline uint64_t makeHierarchyNodeUiKey(const FlowUi::devMode::ElementTreePlaceholder::FlatNode& node) {
	uint64_t key = (node.flowId != 0u) ? node.flowId : FlowUi::toFlowId(node.elementId);
	key ^= (node.definitionId + 0x9e3779b97f4a7c15ull + (key << 6u) + (key >> 2u));
	return (key == 0u) ? 1u : key;
}

inline std::string hierarchyLeafSegment(std::string_view elementId) {
	const std::size_t lastSlash = elementId.find_last_of('/');
	if (lastSlash == std::string_view::npos || (lastSlash + 1u) >= elementId.size())
	{
		return std::string(elementId);
	}
	return std::string(elementId.substr(lastSlash + 1u));
}

inline int computeHierarchyIndentPx(uint32_t depth, const devHierarchyContentParams& params) {
	const int safeDepth = static_cast<int>(depth);
	const int firstBandDepthCount = std::max(0, params.firstIndentDepthCount);
	const int secondBandDepthCount = std::max(0, params.secondIndentDepthCount);
	const int thirdBandDepthCount = std::max(0, params.thirdIndentDepthCount);
	const int firstStep = std::max(0, params.firstIndentStepPx);
	const int secondStep = std::max(0, params.secondIndentStepPx);
	const int thirdStep = std::max(0, params.thirdIndentStepPx);

	const int firstBandDepth = std::min(safeDepth, firstBandDepthCount);
	const int secondBandDepth = std::min(
		std::max(safeDepth - firstBandDepthCount, 0),
		secondBandDepthCount);
	const int thirdBandDepth = std::min(
		std::max(safeDepth - firstBandDepthCount - secondBandDepthCount, 0),
		thirdBandDepthCount);
	int indentPx =
		firstBandDepth * firstStep +
		secondBandDepth * secondStep +
		thirdBandDepth * thirdStep;

	const int maxIndentPx = std::max(0, params.panelWidthPx - params.minRowContentWidthPx);
	if (indentPx > maxIndentPx)
	{
		indentPx = maxIndentPx;
	}
	return indentPx;
}

inline void syncHierarchyEntryStateToFlatNodes(
	const std::vector<FlowUi::devMode::ElementTreePlaceholder::FlatNode>& flatNodes,
	devHierarchyContentState& state) {
	std::unordered_map<uint64_t, bool> previousExpandedByKey{};
	previousExpandedByKey.reserve(state.entryKeys.size());

	for (std::size_t i = 0; i < state.entryKeys.size() && i < state.entryStates.size(); ++i)
	{
		previousExpandedByKey[state.entryKeys[i]] = state.entryStates[i].expanded;
	}

	state.entryKeys.resize(flatNodes.size());
	state.entryStates.assign(flatNodes.size(), hierarchyEntryUiState{});

	for (std::size_t i = 0; i < flatNodes.size(); ++i)
	{
		const uint64_t key = makeHierarchyNodeUiKey(flatNodes[i]);
		state.entryKeys[i] = key;

		const auto previousIt = previousExpandedByKey.find(key);
		if (previousIt != previousExpandedByKey.end())
		{
			state.entryStates[i].expanded = previousIt->second;
		}
	}
}

inline uint64_t makeDefinitionEntryUiKey(const FlowUi::devMode::ElementDescriptor& descriptor) {
	uint64_t key = descriptor.definitionId;
	if (key == 0u)
	{
		const std::string_view stableText =
			!descriptor.definitionName.empty()
			? std::string_view(descriptor.definitionName)
			: std::string_view(descriptor.definitionTypeToken);
		key = FlowUi::devMode::hashString64(stableText);
	}
	return (key == 0u) ? 1u : key;
}

inline void syncDefinitionEntryStateToRegistry(
	const std::vector<FlowUi::devMode::ElementDescriptor>& definitions,
	devHierarchyContentState& state) {
	std::unordered_map<uint64_t, bool> previousExpandedByKey{};
	previousExpandedByKey.reserve(state.definitionEntryKeys.size());

	for (std::size_t i = 0; i < state.definitionEntryKeys.size() && i < state.definitionEntryStates.size(); ++i)
	{
		previousExpandedByKey[state.definitionEntryKeys[i]] = state.definitionEntryStates[i].expanded;
	}

	state.definitionEntryKeys.resize(definitions.size());
	state.definitionEntryStates.assign(definitions.size(), hierarchyEntryUiState{});

	for (std::size_t i = 0; i < definitions.size(); ++i)
	{
		const uint64_t key = makeDefinitionEntryUiKey(definitions[i]);
		state.definitionEntryKeys[i] = key;
		state.definitionEntryStates[i].expanded = false;

		const auto previousIt = previousExpandedByKey.find(key);
		if (previousIt != previousExpandedByKey.end())
		{
			state.definitionEntryStates[i].expanded = previousIt->second;
		}
	}
}

inline const DevHierarchyContentDef kDevHierarchyContent = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHierarchyContentDef::BuildContext& context) {
		devHierarchyContentState& state = DevHierarchyContentDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const FlowUi::TextureRef downArrowIcon =
			(DevHierarchyContentDef::resources.has_value() && DevHierarchyContentDef::resources->disclosureIconsPrepared)
			? DevHierarchyContentDef::resources->downArrowIcon
			: FlowUi::TextureRef{};
		const FlowUi::TextureRef rightArrowIcon =
			(DevHierarchyContentDef::resources.has_value() && DevHierarchyContentDef::resources->disclosureIconsPrepared)
			? DevHierarchyContentDef::resources->rightArrowIcon
			: FlowUi::TextureRef{};

		devPanelContentState* panelState = findSingleDevPanelContentState();
		const bool isViewingInstances = (panelState == nullptr) ? true : panelState->isViewingInstances;

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;
		root.clip = {
			.horizontal = false,
			.vertical = true,
			.childOffset = Clay_GetScrollOffset(),
		};

		Clay_TextElementConfig textConfigBase{};
		textConfigBase.textColor = context.params.textColor;
		textConfigBase.fontId = context.params.fontId;
		textConfigBase.fontSize = context.params.fontSize;
		textConfigBase.wrapMode = CLAY_TEXT_WRAP_NONE;
		textConfigBase.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		CLAY(root){
			if (!isViewingInstances)
			{
				const FlowUi::devMode::DevRegistry& registry = FlowUi::devMode::DevRegistry::instance();
				const auto& definitions = registry.getElements();
				syncDefinitionEntryStateToRegistry(definitions, state);

				if (definitions.empty())
				{
					Clay_TextElementConfig emptyTextConfig = textConfigBase;
					emptyTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.emptyDefinitionsText),
						CLAY_TEXT_CONFIG(emptyTextConfig));
				}
				else
				{
					for (std::size_t i = 0; i < definitions.size(); ++i)
					{
						const auto& descriptor = definitions[i];
						const std::string definitionName =
							!descriptor.definitionName.empty()
							? descriptor.definitionName
							: (!descriptor.definitionTypeToken.empty() ? descriptor.definitionTypeToken : "UnknownDefinition");

						const uint64_t entryKey =
							(i < state.definitionEntryKeys.size())
							? state.definitionEntryKeys[i]
							: makeDefinitionEntryUiKey(descriptor);
						const bool isExpanded =
							(i < state.definitionEntryStates.size())
							? state.definitionEntryStates[i].expanded
							: false;

						const bool hasStatePseudoChild =
							descriptor.stateStructTypeHash != FlowUi::devMode::typeHash<FlowUi::NoElementState>();
						const bool hasResourcesPseudoChild =
							descriptor.resourcesStructTypeHash != FlowUi::devMode::typeHash<FlowUi::NoElementResources>();

						const std::string definitionSelectionId = definitionName;
						const devPropertiesSelectionNode definitionSelectionNode =
							makeDevPropertiesSelectionFromDefinitionDescriptor(
								descriptor,
								devPropertiesStructScope::Parameters);
						const bool isDefinitionSelected =
							panelState != nullptr &&
							panelState->selectedElementId == definitionSelectionId;

						Clay_ElementDeclaration definitionRow{};
						definitionRow.id = context.uiManager.toClayEID(
							context.createChildElementId("definition-row-" + std::to_string(i)));
						definitionRow.layout.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_FIT(0),
						};
						definitionRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
						definitionRow.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
						definitionRow.layout.childGap = 4;
						definitionRow.backgroundColor = FlowUi::Flow_Color("#00000000");

						CLAY(definitionRow){
							context.uiManager
								.createElement(kDevBasicButton, context.createChildElementId("definition-row-" + std::to_string(i) + "/expand"))
								.setParameters(devBasicButtonParams{
									.icon = isExpanded ? downArrowIcon : rightArrowIcon,
									.onPressedCallback = [entryKey](DevBasicButtonInteractionContext) {
										devHierarchyContentState* contentState = findSingleDevHierarchyContentState();
										if (contentState == nullptr)
										{
											return;
										}
										for (std::size_t j = 0; j < contentState->definitionEntryKeys.size() && j < contentState->definitionEntryStates.size(); ++j)
										{
											if (contentState->definitionEntryKeys[j] == entryKey)
											{
												contentState->definitionEntryStates[j].expanded = !contentState->definitionEntryStates[j].expanded;
												break;
											}
										}
									},
									.contentMode = devBasicButtonParams::ContentMode::IconOnly,
									.padding = context.params.disclosureButtonPadding,
									.sizing = Clay_Sizing{
										.width = CLAY_SIZING_FIXED(static_cast<float>(std::max(1, context.params.arrowSlotWidthPx))),
										.height = CLAY_SIZING_FIT(0),
									},
									.backgroundColor = context.params.disclosureButtonBackgroundColor,
									.borderColor = FlowUi::Flow_Color("#00000000"),
									.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
									.iconContainerSizing = context.params.disclosureIconContainerSizing,
								})
								.draw();

								context.uiManager
									.createElement(kDevBasicButton, context.createChildElementId("definition-row-" + std::to_string(i) + "/select"))
									.setParameters(devBasicButtonParams{
										.text = definitionName,
										.onPressedCallback = [definitionSelectionId, definitionSelectionNode](DevBasicButtonInteractionContext) {
											devPanelContentState* latestPanelState = findSingleDevPanelContentState();
											if (latestPanelState != nullptr)
											{
												latestPanelState->selectedElementId = definitionSelectionId;
											}
											(void)setSelectedDevPropertiesNode(definitionSelectionNode);
										},
										.contentMode = devBasicButtonParams::ContentMode::TextOnly,
									.padding = CLAY_PADDING_ALL(4),
									.sizing = Clay_Sizing{
										.width = CLAY_SIZING_GROW(0),
										.height = CLAY_SIZING_FIT(0),
									},
									.backgroundColor =
										isDefinitionSelected
										? context.params.selectedRowBackgroundColor
										: context.params.rowBackgroundColor,
									.borderColor = FlowUi::Flow_Color("#00000000"),
									.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
									.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
									.textWrapMode = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
									.fontId = context.params.fontId,
									.fontSize = context.params.fontSize,
									.textColor = context.params.textColor,
								})
								.draw();
						};

						if (!isExpanded)
						{
							continue;
						}

							const auto drawPseudoChild = [&](
								std::string_view typeLabel,
								std::string_view localChildId,
								devPropertiesStructScope structScope) {
								const std::string childSelectionId =
									definitionSelectionId + "/" + std::string(localChildId);
								const devPropertiesSelectionNode childSelectionNode =
									makeDevPropertiesSelectionFromDefinitionDescriptor(descriptor, structScope);
								const bool isChildSelected =
									panelState != nullptr &&
									panelState->selectedElementId == childSelectionId;

							Clay_ElementDeclaration pseudoRow{};
							pseudoRow.id = context.uiManager.toClayEID(
								context.createChildElementId(
									"definition-row-" + std::to_string(i) + "/" + std::string(localChildId)));
							pseudoRow.layout.sizing = {
								.width = CLAY_SIZING_GROW(0),
								.height = CLAY_SIZING_FIT(0),
							};
							pseudoRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
							pseudoRow.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
							pseudoRow.layout.childGap = 4;
							pseudoRow.backgroundColor = FlowUi::Flow_Color("#00000000");

							CLAY(pseudoRow){
								CLAY({
									.id = context.uiManager.toClayEID(
										context.createChildElementId(
											"definition-row-" + std::to_string(i) + "/" + std::string(localChildId) + "/expand-spacer")),
									.layout = {
										.sizing = {
											.width = CLAY_SIZING_FIXED(static_cast<float>(std::max(1, context.params.arrowSlotWidthPx))),
											.height = CLAY_SIZING_FIT(0),
										},
									},
								}){};
								CLAY({
									.id = context.uiManager.toClayEID(
										context.createChildElementId(
											"definition-row-" + std::to_string(i) + "/" + std::string(localChildId) + "/inset-spacer")),
									.layout = {
										.sizing = {
											.width = CLAY_SIZING_FIXED(20.0f),
											.height = CLAY_SIZING_FIT(0),
										},
									},
								}){};

									context.uiManager
										.createElement(kDevBasicButton, context.createChildElementId(
											"definition-row-" + std::to_string(i) + "/" + std::string(localChildId) + "/select"))
										.setParameters(devBasicButtonParams{
											.text = std::string(typeLabel),
											.onPressedCallback = [childSelectionId, childSelectionNode](DevBasicButtonInteractionContext) {
												devPanelContentState* latestPanelState = findSingleDevPanelContentState();
												if (latestPanelState != nullptr)
												{
													latestPanelState->selectedElementId = childSelectionId;
												}
												(void)setSelectedDevPropertiesNode(childSelectionNode);
											},
											.contentMode = devBasicButtonParams::ContentMode::TextOnly,
										.padding = CLAY_PADDING_ALL(4),
										.sizing = Clay_Sizing{
											.width = CLAY_SIZING_GROW(0),
											.height = CLAY_SIZING_FIT(0),
										},
										.backgroundColor =
											isChildSelected
											? context.params.selectedRowBackgroundColor
											: context.params.rowBackgroundColor,
										.borderColor = FlowUi::Flow_Color("#00000000"),
										.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
										.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
										.textWrapMode = CLAY_TEXT_WRAP_NONE,
										.textAlignment = CLAY_TEXT_ALIGN_LEFT,
										.fontId = context.params.fontId,
										.fontSize = context.params.fontSize,
										.textColor = context.params.textColor,
									})
									.draw();
							};
						};

							drawPseudoChild(
								context.params.parametersTypeText,
								"parameters",
								devPropertiesStructScope::Parameters);
							if (hasStatePseudoChild)
							{
								drawPseudoChild(
									context.params.stateTypeText,
									"state",
									devPropertiesStructScope::State);
							}
							if (hasResourcesPseudoChild)
							{
								drawPseudoChild(
									context.params.resourcesTypeText,
									"resources",
									devPropertiesStructScope::Resources);
							}
					}
				}
			}
			else
			{
				const auto& flatNodes = context.uiManager.devRuntime().elementTreePlaceholder().flatNodes;
				syncHierarchyEntryStateToFlatNodes(flatNodes, state);
				if (flatNodes.empty())
				{
					Clay_TextElementConfig emptyTextConfig = textConfigBase;
					emptyTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.emptyInstancesText),
						CLAY_TEXT_CONFIG(emptyTextConfig));
				}
				else
				{
					int latestCollapsedDepth = -1;

					for (std::size_t i = 0; i < flatNodes.size(); ++i)
					{
						const auto& node = flatNodes[i];
						if (latestCollapsedDepth >= 0)
						{
							if (static_cast<int>(node.depth) > latestCollapsedDepth)
							{
								continue;
							}
							latestCollapsedDepth = -1;
						}

						if (node.kind != FlowUi::devMode::ElementTreePlaceholder::ElementKind::FlowElement)
						{
							continue;
						}

						const bool hasChildren =
							(i + 1u) < flatNodes.size() &&
							flatNodes[i + 1u].depth > node.depth;
						const bool isExpanded = (i < state.entryStates.size()) ? state.entryStates[i].expanded : true;
						if (hasChildren && !isExpanded)
						{
							latestCollapsedDepth = static_cast<int>(node.depth);
						}

						const int indentPx = computeHierarchyIndentPx(node.depth, context.params);
							const uint64_t entryKey = (i < state.entryKeys.size()) ? state.entryKeys[i] : makeHierarchyNodeUiKey(node);
							const std::string elementId = node.elementId;
							const devPropertiesSelectionNode instanceSelectionNode =
								makeDevPropertiesSelectionFromInstanceNode(node);
							const std::string definitionLabel =
								!node.definitionDisplayName.empty() ? node.definitionDisplayName : "Unknown";
							const std::string rowText = definitionLabel + " / " + hierarchyLeafSegment(elementId);
						const bool isSelected = panelState != nullptr && panelState->selectedElementId == elementId;

						Clay_ElementDeclaration row{};
						row.id = context.uiManager.toClayEID(context.createChildElementId("row-" + std::to_string(i)));
						row.layout.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_FIT(0),
						};
						row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
						row.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
						row.layout.childGap = 4;
						row.backgroundColor = FlowUi::Flow_Color("#00000000");

						CLAY(row){
							CLAY({
								.id = context.uiManager.toClayEID(context.createChildElementId("row-" + std::to_string(i) + "/indent")),
								.layout = {
									.sizing = {
										.width = CLAY_SIZING_FIXED(static_cast<float>(indentPx)),
										.height = CLAY_SIZING_FIT(0),
									},
								},
							}){};

							if (hasChildren)
							{
								context.uiManager
									.createElement(kDevBasicButton, context.createChildElementId("row-" + std::to_string(i) + "/expand"))
									.setParameters(devBasicButtonParams{
										.icon = isExpanded ? downArrowIcon : rightArrowIcon,
										.onPressedCallback = [entryKey](DevBasicButtonInteractionContext) {
											devHierarchyContentState* contentState = findSingleDevHierarchyContentState();
											if (contentState == nullptr)
											{
												return;
											}
											for (std::size_t j = 0; j < contentState->entryKeys.size() && j < contentState->entryStates.size(); ++j)
											{
												if (contentState->entryKeys[j] == entryKey)
												{
													contentState->entryStates[j].expanded = !contentState->entryStates[j].expanded;
													break;
												}
											}
										},
										.contentMode = devBasicButtonParams::ContentMode::IconOnly,
										.padding = context.params.disclosureButtonPadding,
										.sizing = Clay_Sizing{
											.width = CLAY_SIZING_FIXED(static_cast<float>(std::max(1, context.params.arrowSlotWidthPx))),
											.height = CLAY_SIZING_FIT(0),
										},
										.backgroundColor = context.params.disclosureButtonBackgroundColor,
										.borderColor = FlowUi::Flow_Color("#00000000"),
										.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
										.iconContainerSizing = context.params.disclosureIconContainerSizing,
									})
									.draw();
							}
							else
							{
								CLAY({
									.id = context.uiManager.toClayEID(context.createChildElementId("row-" + std::to_string(i) + "/expand-spacer")),
									.layout = {
										.sizing = {
											.width = CLAY_SIZING_FIXED(static_cast<float>(std::max(1, context.params.arrowSlotWidthPx))),
											.height = CLAY_SIZING_FIT(0),
										},
									},
								}){};
							}

								context.uiManager
									.createElement(kDevBasicButton, context.createChildElementId("row-" + std::to_string(i) + "/select"))
									.setParameters(devBasicButtonParams{
										.text = rowText,
										.onPressedCallback = [elementId, instanceSelectionNode](DevBasicButtonInteractionContext) {
											devPanelContentState* latestPanelState = findSingleDevPanelContentState();
											if (latestPanelState != nullptr)
											{
												latestPanelState->selectedElementId = elementId;
											}
											(void)setSelectedDevPropertiesNode(instanceSelectionNode);
										},
										.contentMode = devBasicButtonParams::ContentMode::TextOnly,
									.padding = CLAY_PADDING_ALL(4),
									.sizing = Clay_Sizing{
										.width = CLAY_SIZING_GROW(0),
										.height = CLAY_SIZING_FIT(0),
									},
									.backgroundColor = isSelected ? context.params.selectedRowBackgroundColor : context.params.rowBackgroundColor,
									.borderColor = FlowUi::Flow_Color("#00000000"),
									.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
									.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
									.textWrapMode = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
									.fontId = context.params.fontId,
									.fontSize = context.params.fontSize,
									.textColor = context.params.textColor,
								})
								.draw();
						};
					}
				}
			}
		};
	},
};

#if FLOW_UI_DEV_MODE
namespace FlowUi::devMode {

inline void initializeDevFlowElementResourcesFromApp(App& app) {
	if (!DevHeaderDef::resources.has_value()) {
		DevHeaderDef::resources.emplace();
	}
	if (!DevHierarchyContentDef::resources.has_value()) {
		DevHierarchyContentDef::resources.emplace();
	}

	devHeaderResources& headerResources = *DevHeaderDef::resources;
	devHierarchyContentResources& hierarchyResources = *DevHierarchyContentDef::resources;

#if FLOWUI_INCLUDE_SVG_MANAGER
	constexpr std::string_view kExportIconKey = "flowui/dev/header/export";
	constexpr std::string_view kDownArrowIconKey = "flowui/dev/hierarchy/arrow-down";
	constexpr std::string_view kRightArrowIconKey = "flowui/dev/hierarchy/arrow-right";

	(void)app.icons().registerSvg(kExportIconKey, ::kExport);
	(void)app.icons().registerSvg(kDownArrowIconKey, ::kDownArrow);
	(void)app.icons().registerSvg(kRightArrowIconKey, ::kRightArrow);

	headerResources.exportIcon = app.icons().textureRef(kExportIconKey);
	hierarchyResources.downArrowIcon = app.icons().textureRef(kDownArrowIconKey);
	hierarchyResources.rightArrowIcon = app.icons().textureRef(kRightArrowIconKey);
#endif

	headerResources.exportIconPrepared = true;
	hierarchyResources.disclosureIconsPrepared = true;
}

} // namespace FlowUi::devMode
#endif


struct devHierarchyParams {
	int width = 280;
	int minRowContentWidthPx = 180;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color headBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color contentBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color modeButtonActiveBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color modeButtonInactiveBackgroundColor = FlowUi::Flow_Color("#2f323aff");
};

using DevHierarchyDef = FlowUi::ElementDefinition<
	devHierarchyParams,
	void,
	void,
	FLOW_DEF_ID("DevHierarchy"),
	true>;

inline const DevHierarchyDef kDevHierarchy = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHierarchyDef::BuildContext& context) {
		int width = context.params.width;
		if (width < 0)
		{
			width = 0;
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_FIXED(static_cast<float>(width)),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
			context.uiManager
				.createElement(kDevHierarchyHead, context.createChildElementId("head"))
				.setParameters(devHierarchyHeadParams{
					.backgroundColor = context.params.headBackgroundColor,
					.modeButtonActiveBackgroundColor = context.params.modeButtonActiveBackgroundColor,
					.modeButtonInactiveBackgroundColor = context.params.modeButtonInactiveBackgroundColor,
				})
				.draw();

			context.uiManager
				.createElement(kDevHierarchyContent, context.createChildElementId("content"))
				.setParameters(devHierarchyContentParams{
					.backgroundColor = context.params.contentBackgroundColor,
					.panelWidthPx = width,
					.minRowContentWidthPx = context.params.minRowContentWidthPx,
				})
				.draw();
		};
	},
};

inline bool devFieldTypeIsBool(uint64_t fieldTypeHash) {
	return fieldTypeHash == FlowUi::devMode::typeHash<bool>();
}

inline bool devFieldTypeIsString(uint64_t fieldTypeHash) {
	return fieldTypeHash == FlowUi::devMode::typeHash<std::string>();
}

inline bool devFieldTypeIsIntegral(uint64_t fieldTypeHash) {
	return
		fieldTypeHash == FlowUi::devMode::typeHash<int8_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<int16_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<int32_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<int64_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint8_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint16_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint32_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint64_t>();
}

inline bool devFieldTypeIsFloating(uint64_t fieldTypeHash) {
	return
		fieldTypeHash == FlowUi::devMode::typeHash<float>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<double>();
}

inline uint64_t normalizedSelectionFlowId(const devPropertiesSelectionNode& selection) {
	if (selection.flowId != 0u)
	{
		return selection.flowId;
	}
	if (!selection.elementId.empty())
	{
		return FlowUi::toFlowId(selection.elementId);
	}
	return 0u;
}

inline std::string trimDevInputText(std::string_view text) {
	std::size_t begin = 0u;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
	{
		++begin;
	}
	std::size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1u])) != 0)
	{
		--end;
	}
	return std::string(text.substr(begin, end - begin));
}

inline bool tryParseInt64FromText(std::string_view text, int64_t& outValue) {
	const std::string trimmed = trimDevInputText(text);
	if (trimmed.empty())
	{
		return false;
	}

	errno = 0;
	char* end = nullptr;
	const long long parsed = std::strtoll(trimmed.c_str(), &end, 10);
	if (end == nullptr || *end != '\0' || errno == ERANGE)
	{
		return false;
	}

	if (
		parsed < static_cast<long long>(std::numeric_limits<int64_t>::min()) ||
		parsed > static_cast<long long>(std::numeric_limits<int64_t>::max()))
	{
		return false;
	}

	outValue = static_cast<int64_t>(parsed);
	return true;
}

inline bool tryParseDoubleFromText(std::string_view text, double& outValue) {
	const std::string trimmed = trimDevInputText(text);
	if (trimmed.empty())
	{
		return false;
	}

	errno = 0;
	char* end = nullptr;
	const double parsed = std::strtod(trimmed.c_str(), &end);
	if (end == nullptr || *end != '\0' || errno == ERANGE || !std::isfinite(parsed))
	{
		return false;
	}

	outValue = parsed;
	return true;
}

inline bool tryCoerceDevValueToBool(const FlowUi::devMode::DevValue& value, bool& outValue) {
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue;
		return true;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		outValue = (*intValue != 0);
		return true;
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		outValue = (*doubleValue != 0.0);
		return true;
	}
	return false;
}

inline bool tryCoerceDevValueToInt64(const FlowUi::devMode::DevValue& value, int64_t& outValue) {
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		outValue = *intValue;
		return true;
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue ? int64_t{1} : int64_t{0};
		return true;
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		if (!std::isfinite(*doubleValue) || std::trunc(*doubleValue) != *doubleValue)
		{
			return false;
		}
		if (
			*doubleValue < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
			*doubleValue > static_cast<double>(std::numeric_limits<int64_t>::max()))
		{
			return false;
		}
		outValue = static_cast<int64_t>(*doubleValue);
		return true;
	}
	return false;
}

inline bool tryCoerceDevValueToDouble(const FlowUi::devMode::DevValue& value, double& outValue) {
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		if (!std::isfinite(*doubleValue))
		{
			return false;
		}
		outValue = *doubleValue;
		return true;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		outValue = static_cast<double>(*intValue);
		return true;
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue ? 1.0 : 0.0;
		return true;
	}
	return false;
}

inline bool devValuesEquivalentForEditableField(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& lhs,
	const FlowUi::devMode::DevValue& rhs) {
	if (devFieldTypeIsString(fieldTypeHash))
	{
		const std::string* lhsText = std::get_if<std::string>(&lhs);
		const std::string* rhsText = std::get_if<std::string>(&rhs);
		return lhsText != nullptr && rhsText != nullptr && *lhsText == *rhsText;
	}

	if (devFieldTypeIsBool(fieldTypeHash))
	{
		bool lhsBool = false;
		bool rhsBool = false;
		return
			tryCoerceDevValueToBool(lhs, lhsBool) &&
			tryCoerceDevValueToBool(rhs, rhsBool) &&
			lhsBool == rhsBool;
	}

	if (devFieldTypeIsIntegral(fieldTypeHash))
	{
		int64_t lhsInt = 0;
		int64_t rhsInt = 0;
		return
			tryCoerceDevValueToInt64(lhs, lhsInt) &&
			tryCoerceDevValueToInt64(rhs, rhsInt) &&
			lhsInt == rhsInt;
	}

	if (devFieldTypeIsFloating(fieldTypeHash))
	{
		double lhsDouble = 0.0;
		double rhsDouble = 0.0;
		if (!tryCoerceDevValueToDouble(lhs, lhsDouble) || !tryCoerceDevValueToDouble(rhs, rhsDouble))
		{
			return false;
		}
		const double diff = std::fabs(lhsDouble - rhsDouble);
		const double scale = std::max(1.0, std::max(std::fabs(lhsDouble), std::fabs(rhsDouble)));
		return diff <= (1e-9 * scale);
	}

	return lhs == rhs;
}

inline std::optional<FlowUi::devMode::DevValue> findFallbackDefinitionParamValueFromLastSeenInstances(
	const FlowUi::devMode::DevRuntime& runtime,
	uint64_t definitionId,
	uint64_t fieldHash) {
	bool found = false;
	uint64_t bestFlowId = std::numeric_limits<uint64_t>::max();
	std::string bestElementId{};
	FlowUi::devMode::DevValue bestValue{};

	for (const auto& [scopeKey, snapshot] : runtime.lastSeenParamsByInstance())
	{
		if (scopeKey.definitionId != definitionId)
		{
			continue;
		}
		const auto it = snapshot.valuesByFieldHash.find(fieldHash);
		if (it == snapshot.valuesByFieldHash.end())
		{
			continue;
		}

		if (
			!found ||
			scopeKey.flowId < bestFlowId ||
			(scopeKey.flowId == bestFlowId && scopeKey.elementId < bestElementId))
		{
			found = true;
			bestFlowId = scopeKey.flowId;
			bestElementId = scopeKey.elementId;
			bestValue = it->second;
		}
	}

	if (!found)
	{
		return std::nullopt;
	}
	return bestValue;
}

inline std::string devValueToEditableText(const FlowUi::devMode::DevValue& value) {
	if (const std::string* textValue = std::get_if<std::string>(&value))
	{
		return *textValue;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		return std::to_string(*intValue);
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		std::ostringstream stream{};
		stream.precision(16);
		stream << *doubleValue;
		return stream.str();
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		return *boolValue ? "true" : "false";
	}
	return {};
}

inline std::optional<FlowUi::devMode::DevValue> parseEditableTextToDevValue(
	uint64_t fieldTypeHash,
	std::string_view text) {
	if (devFieldTypeIsString(fieldTypeHash))
	{
		return FlowUi::devMode::DevValue{std::string(text)};
	}
	if (devFieldTypeIsIntegral(fieldTypeHash))
	{
		int64_t parsed = 0;
		if (!tryParseInt64FromText(text, parsed))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{parsed};
	}
	if (devFieldTypeIsFloating(fieldTypeHash))
	{
		double parsed = 0.0;
		if (!tryParseDoubleFromText(text, parsed))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{parsed};
	}
	return std::nullopt;
}

inline std::optional<FlowUi::devMode::DevValue> findCurrentEditableFieldValue(
	FlowUi::devMode::DevRuntime& runtime,
	const devPropertiesSelectionNode& selection,
	uint64_t fieldHash) {
	using FlowUi::devMode::DevValue;

	switch (selection.kind)
	{
	case devPropertiesSelectionKind::None:
		return std::nullopt;
	case devPropertiesSelectionKind::Instance:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
		{
			if (const DevValue* value = runtime.findInstanceParamOverride(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId,
				fieldHash))
			{
				return *value;
			}
			if (const DevValue* value = runtime.findDefinitionParamOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenParams(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		case devPropertiesStructScope::State:
		{
			if (const DevValue* value = runtime.findStateOverride(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId,
				fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenState(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		case devPropertiesStructScope::Resources:
		{
			if (const DevValue* value = runtime.findResourceOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenResources(selection.definitionId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		}
		break;
	case devPropertiesSelectionKind::Definition:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
		{
			if (const DevValue* value = runtime.findDefinitionParamOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const std::optional<DevValue> fallback =
				findFallbackDefinitionParamValueFromLastSeenInstances(
					runtime,
					selection.definitionId,
					fieldHash);
				fallback.has_value())
			{
				return *fallback;
			}
			return std::nullopt;
		}
		case devPropertiesStructScope::State:
			return std::nullopt;
		case devPropertiesStructScope::Resources:
		{
			if (const DevValue* value = runtime.findResourceOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenResources(selection.definitionId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		}
		break;
	}

	return std::nullopt;
}

inline void setEditableFieldOverride(
	FlowUi::devMode::DevRuntime& runtime,
	const devPropertiesSelectionNode& selection,
	uint64_t fieldHash,
	const FlowUi::devMode::DevValue& value) {
	switch (selection.kind)
	{
	case devPropertiesSelectionKind::None:
		return;
	case devPropertiesSelectionKind::Instance:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
				runtime.setInstanceParamOverride(
					selection.definitionId,
					normalizedSelectionFlowId(selection),
					selection.elementId,
					fieldHash,
					value);
			return;
		case devPropertiesStructScope::State:
				runtime.setStateOverride(
					selection.definitionId,
					normalizedSelectionFlowId(selection),
					selection.elementId,
					fieldHash,
					value);
			return;
		case devPropertiesStructScope::Resources:
			runtime.setResourceOverride(selection.definitionId, fieldHash, value);
			return;
		}
		return;
	case devPropertiesSelectionKind::Definition:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
			runtime.setDefinitionParamOverride(selection.definitionId, fieldHash, value);
			return;
		case devPropertiesStructScope::State:
			return;
		case devPropertiesStructScope::Resources:
			runtime.setResourceOverride(selection.definitionId, fieldHash, value);
			return;
		}
		return;
	}
}

inline std::string makeDevPropertiesFieldIdentity(
	const devPropertiesSelectionNode& selection,
	uint64_t fieldHash) {
	const std::string scopeText = std::to_string(static_cast<int>(selection.structScope));
	return
		std::to_string(selection.definitionId) +
		"/" +
		std::to_string(normalizedSelectionFlowId(selection)) +
		"/" +
		selection.elementId +
		"/" +
		scopeText +
		"/" +
		std::to_string(fieldHash);
}


struct devPropertiesHeadParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t childGap = 6;

	std::string titleText = "Properties";
	uint16_t titleFontId = 0;
	uint16_t titleFontSize = 12;
	Clay_Color titleTextColor = FlowUi::Flow_Color("#ffffffff");

	std::string selectedElementIdText = "placeholder";
	uint16_t selectedElementIdFontId = 0;
	uint16_t selectedElementIdFontSize = 12;
	Clay_Color selectedElementIdTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevPropertiesHeadDef = FlowUi::ElementDefinition<
	devPropertiesHeadParams,
	void,
	void,
	FLOW_DEF_ID("DevPropertiesHead"),
	true>;

inline const DevPropertiesHeadDef kDevPropertiesHead = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesHeadDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig titleTextConfig{};
		titleTextConfig.textColor = context.params.titleTextColor;
		titleTextConfig.fontId = context.params.titleFontId;
		titleTextConfig.fontSize = context.params.titleFontSize;
		titleTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		titleTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		Clay_TextElementConfig selectedTextConfig{};
		selectedTextConfig.textColor = context.params.selectedElementIdTextColor;
		selectedTextConfig.fontId = context.params.selectedElementIdFontId;
		selectedTextConfig.fontSize = context.params.selectedElementIdFontSize;
		selectedTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		selectedTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		CLAY(root){
			CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("title"))}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.titleText),
					CLAY_TEXT_CONFIG(titleTextConfig));
			};

			CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("selected-id"))}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.selectedElementIdText),
					CLAY_TEXT_CONFIG(selectedTextConfig));
			};
		};
	},
};


struct devPropertiesContentParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t childGap = 8;
	uint16_t rowGap = 6;
	uint16_t rowChildGap = 8;
	Clay_Sizing valueEditorSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(220),
		.height = CLAY_SIZING_FIT(0),
	};
	std::string noSelectionText = "Select an Element or a Definition from the Hirearchy";
	std::string missingSelectionText = "Selected target is unavailable.";
	std::string unsupportedScopeText = "This scope cannot be overridden with current runtime keys.";
	std::string noEditableFieldsText = "No editable fields are registered for the selected target.";
	std::string unsupportedFieldTypeText = "<unsupported type>";
	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
};

struct devPropertiesContentState {
	devPropertiesSelectionNode selectedNode{};
};

using DevPropertiesContentDef = FlowUi::ElementDefinition<
	devPropertiesContentParams,
	devPropertiesContentState,
	void,
	FLOW_DEF_ID("DevPropertiesContent"),
	true>;

inline devPropertiesContentState* findSingleDevPropertiesContentState() {
	constexpr std::string_view kSingleDevPropertiesContentElementId =
		"flowui/dev/debug-view/main-view/content/properties/content";

	devPropertiesContentState* state =
		DevPropertiesContentDef::tryGetState(FlowUi::toFlowId(kSingleDevPropertiesContentElementId));
	if (state != nullptr)
	{
		return state;
	}
	if (!DevPropertiesContentDef::statePool.empty())
	{
		return &DevPropertiesContentDef::statePool.front().second;
	}
	return nullptr;
}

inline bool setSelectedDevPropertiesNode(const devPropertiesSelectionNode& selection) {
	devPropertiesContentState* state = findSingleDevPropertiesContentState();
	if (state == nullptr)
	{
		return false;
	}
	state->selectedNode = selection;
	return true;
}

inline const DevPropertiesContentDef kDevPropertiesContent = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesContentDef::BuildContext& context) {
		devPropertiesContentState& state = DevPropertiesContentDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const devPropertiesSelectionNode& selection = state.selectedNode;

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = context.params.childGap;
		root.layout.padding = context.params.padding;
		root.backgroundColor = context.params.backgroundColor;
		root.clip = {
			.horizontal = false,
			.vertical = true,
			.childOffset = Clay_GetScrollOffset(),
		};

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		textConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

			CLAY(root){
				if (isDevPropertiesSelectionNull(selection))
				{
					Clay_TextElementConfig noSelectionTextConfig = textConfig;
				noSelectionTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.noSelectionText),
					CLAY_TEXT_CONFIG(noSelectionTextConfig));
				}
				else
				{
					FlowUi::devMode::DevRuntime& runtime = context.uiManager.devRuntime();
					const FlowUi::devMode::DevRegistry& registry = FlowUi::devMode::DevRegistry::instance();
					const FlowUi::devMode::ElementDescriptor* descriptor =
						registry.findElementByDefinitionId(selection.definitionId);

				uint64_t structTypeHash = 0u;
				if (descriptor != nullptr)
				{
					switch (selection.structScope)
					{
					case devPropertiesStructScope::Parameters:
						structTypeHash = descriptor->paramsStructTypeHash;
						break;
					case devPropertiesStructScope::State:
						structTypeHash = descriptor->stateStructTypeHash;
						break;
					case devPropertiesStructScope::Resources:
						structTypeHash = descriptor->resourcesStructTypeHash;
						break;
					}
				}

					const FlowUi::devMode::StructDescriptor* structure =
						(structTypeHash == 0u) ? nullptr : registry.findStructByTypeHash(structTypeHash);

					const bool unsupportedScope =
						selection.kind == devPropertiesSelectionKind::Definition &&
						selection.structScope == devPropertiesStructScope::State;

					if (unsupportedScope)
					{
						Clay_TextElementConfig unsupportedScopeTextConfig = textConfig;
						unsupportedScopeTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
						CLAY_TEXT(
							context.uiManager.toClayString(context.params.unsupportedScopeText),
							CLAY_TEXT_CONFIG(unsupportedScopeTextConfig));
					}
					else if (descriptor == nullptr || structure == nullptr)
					{
						Clay_TextElementConfig missingSelectionTextConfig = textConfig;
						missingSelectionTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.missingSelectionText),
						CLAY_TEXT_CONFIG(missingSelectionTextConfig));
				}
				else if (structure->fields.empty())
				{
					Clay_TextElementConfig noFieldsTextConfig = textConfig;
					noFieldsTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.noEditableFieldsText),
						CLAY_TEXT_CONFIG(noFieldsTextConfig));
					}
					else
					{
						Clay_ElementDeclaration fieldsColumn{};
						fieldsColumn.id = context.uiManager.toClayEID(context.createChildElementId("rows"));
						fieldsColumn.layout.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_FIT(0),
						};
						fieldsColumn.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
						fieldsColumn.layout.childGap = context.params.rowGap;
						fieldsColumn.backgroundColor = FlowUi::Flow_Color("#00000000");

						CLAY(fieldsColumn){
							for (std::size_t i = 0; i < structure->fields.size(); ++i)
							{
								const FlowUi::devMode::FieldDescriptor& field = structure->fields[i];
								const std::string fieldIdentity = makeDevPropertiesFieldIdentity(selection, field.fieldHash);
								const std::optional<FlowUi::devMode::DevValue> fieldValue =
									findCurrentEditableFieldValue(runtime, selection, field.fieldHash);
								const bool isBoolField = devFieldTypeIsBool(field.fieldTypeHash);
								const bool isInputField =
									devFieldTypeIsString(field.fieldTypeHash) ||
									devFieldTypeIsIntegral(field.fieldTypeHash) ||
									devFieldTypeIsFloating(field.fieldTypeHash);

								bool toggleDefaultEnabled = false;
								if (fieldValue.has_value())
								{
									(void)tryCoerceDevValueToBool(*fieldValue, toggleDefaultEnabled);
								}
								const std::string inputInitialText =
									fieldValue.has_value()
									? devValueToEditableText(*fieldValue)
									: std::string{};

								Clay_ElementDeclaration row{};
								row.id = context.uiManager.toClayEID(
									context.createChildElementId("row-" + std::to_string(i)));
								row.layout.sizing = {
									.width = CLAY_SIZING_GROW(0),
									.height = CLAY_SIZING_FIT(0),
								};
								row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
								row.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
								row.layout.childGap = context.params.rowChildGap;
								row.backgroundColor = FlowUi::Flow_Color("#00000000");

								CLAY(row){
									CLAY({
										.id = context.uiManager.toClayEID(
											context.createChildElementId("row-" + std::to_string(i) + "/label")),
										.layout = {
											.sizing = {
												.width = CLAY_SIZING_GROW(0),
												.height = CLAY_SIZING_FIT(0),
											},
										},
									}){
										CLAY_TEXT(
											context.uiManager.toClayString(field.name),
											CLAY_TEXT_CONFIG(textConfig));
									};

									if (isBoolField)
									{
										context.uiManager
											.createElement(
												kDevBasicToggle,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/toggle/" + fieldIdentity))
											.setParameters(devBasicToggleParams{
												.defaultEnabled = toggleDefaultEnabled,
												.onValueChangedCallback = [
													uiManager = &context.uiManager,
													selection,
													fieldHash = field.fieldHash
												](DevBasicToggleInteractionContext, bool isEnabled) {
													setEditableFieldOverride(
														uiManager->devRuntime(),
														selection,
														fieldHash,
														FlowUi::devMode::DevValue{isEnabled});
												},
												.sizing = context.params.valueEditorSizing,
											})
											.draw();
									}
									else if (isInputField)
									{
										const std::string inputFieldId =
											"flowui/dev/input/" + fieldIdentity;
										context.uiManager
											.createElement(
												kDevBasicInputField,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/input/" + fieldIdentity))
											.setParameters(devBasicInputFieldParams{
												.fieldId = inputFieldId,
												.initialText = inputInitialText,
												.onTextChangedCallback = [
													uiManager = &context.uiManager,
													selection,
													fieldHash = field.fieldHash,
													fieldTypeHash = field.fieldTypeHash,
													initialText = inputInitialText
												](std::string_view text) {
													const std::optional<FlowUi::devMode::DevValue> parsed =
														parseEditableTextToDevValue(fieldTypeHash, text);
													if (!parsed.has_value())
													{
														return;
													}
													FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
													const std::optional<FlowUi::devMode::DevValue> currentValue =
														findCurrentEditableFieldValue(runtime, selection, fieldHash);
													if (currentValue.has_value())
													{
														if (devValuesEquivalentForEditableField(
															fieldTypeHash,
															*currentValue,
															*parsed))
														{
															return;
														}
													}
													else if (text == std::string_view(initialText))
													{
														return;
													}
													setEditableFieldOverride(
														runtime,
														selection,
														fieldHash,
														*parsed);
												},
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else
									{
										CLAY({
											.id = context.uiManager.toClayEID(
												context.createChildElementId("row-" + std::to_string(i) + "/unsupported")),
											.layout = {
												.sizing = {
													.width = context.params.valueEditorSizing.width,
													.height = CLAY_SIZING_FIT(0),
												},
											},
										}){
											CLAY_TEXT(
												context.uiManager.toClayString(context.params.unsupportedFieldTypeText),
												CLAY_TEXT_CONFIG(textConfig));
										};
									}
								};
							}
						};
					}
				}
			};
	},
};


struct devPropertiesParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color headBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color contentBackgroundColor = FlowUi::Flow_Color("#00000000");
	std::string selectedElementIdText = "placeholder";
};

using DevPropertiesDef = FlowUi::ElementDefinition<
	devPropertiesParams,
	void,
	void,
	FLOW_DEF_ID("DevProperties"),
	true>;

inline const DevPropertiesDef kDevProperties = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
			context.uiManager
				.createElement(kDevPropertiesHead, context.createChildElementId("head"))
				.setParameters(devPropertiesHeadParams{
					.backgroundColor = context.params.headBackgroundColor,
					.selectedElementIdText = context.params.selectedElementIdText,
				})
				.draw();

			context.uiManager
				.createElement(kDevPropertiesContent, context.createChildElementId("content"))
				.setParameters(devPropertiesContentParams{
					.backgroundColor = context.params.contentBackgroundColor,
				})
				.draw();
		};
	},
};

inline const DevPanelContentDef kDevPanelContent = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPanelContentDef::BuildContext& context) {
		devPanelContentState& state = DevPanelContentDef::getOrCreateState(FlowUi::toFlowId(context.elementID));

		int minHierarchyWidth = context.params.minHierarchyWidthPx;
		if (minHierarchyWidth < 0)
		{
			minHierarchyWidth = 0;
		}

		int maxHierarchyWidth = context.params.maxHierarchyWidthPx;
		if (maxHierarchyWidth < minHierarchyWidth)
		{
			maxHierarchyWidth = minHierarchyWidth;
		}

		if (!state.hierarchyWidthInitialized)
		{
			state.hierarchyWidthPx = context.params.defaultHierarchyWidthPx;
			if (state.hierarchyWidthPx < minHierarchyWidth)
			{
				state.hierarchyWidthPx = minHierarchyWidth;
			}
			else if (state.hierarchyWidthPx > maxHierarchyWidth)
			{
				state.hierarchyWidthPx = maxHierarchyWidth;
			}
			state.hierarchyWidthInitialized = true;
		}
		else
		{
			if (state.hierarchyWidthPx < minHierarchyWidth)
			{
				state.hierarchyWidthPx = minHierarchyWidth;
			}
			else if (state.hierarchyWidthPx > maxHierarchyWidth)
			{
				state.hierarchyWidthPx = maxHierarchyWidth;
			}
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
			context.uiManager
				.createElement(kDevHierarchy, context.createChildElementId("hierarchy"))
				.setParameters(devHierarchyParams{
					.width = state.hierarchyWidthPx,
					.backgroundColor = context.params.hierarchyBackgroundColor,
				})
				.draw();

			devDynamicSeparatorParams separatorParams{};
			separatorParams.orientation = devDynamicSeparatorParams::Orientation::Vertical;
			separatorParams.reverseDrag = false;
			separatorParams.width = (context.params.separatorThicknessPx < 1) ? 1 : context.params.separatorThicknessPx;
			separatorParams.height = 1;
			separatorParams.minValue = minHierarchyWidth;
			separatorParams.maxValue = maxHierarchyWidth;
			separatorParams.getValue = [&state]() {
				return state.hierarchyWidthPx;
			};
			separatorParams.setValue = [&state, minHierarchyWidth, maxHierarchyWidth](int nextWidth) {
				if (nextWidth < minHierarchyWidth)
				{
					nextWidth = minHierarchyWidth;
				}
				else if (nextWidth > maxHierarchyWidth)
				{
					nextWidth = maxHierarchyWidth;
				}
				state.hierarchyWidthPx = nextWidth;
			};

			context.uiManager
				.createElement(kDevDynamicSeparator, context.createChildElementId("separator"))
				.setParameters(separatorParams)
				.draw();

				context.uiManager
					.createElement(kDevProperties, context.createChildElementId("properties"))
					.setParameters(devPropertiesParams{
						.backgroundColor = context.params.propertiesBackgroundColor,
						.selectedElementIdText =
							state.selectedElementId.empty()
							? std::string("placeholder")
							: state.selectedElementId,
					})
					.draw();
		};
	},
};


struct devFooterParams {
	int height = 36;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
};

using DevFooterDef = FlowUi::ElementDefinition<
	devFooterParams,
	void,
	void,
	FLOW_DEF_ID("DevFooter"),
	true>;

inline const DevFooterDef kDevFooter = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevFooterDef::BuildContext& context) {
		int height = context.params.height;
		if (height < 0)
		{
			height = 0;
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(static_cast<float>(height)),
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){};
	},
};


struct mainDevViewParams {
	int width = 420;
	int footerHeight = 36;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#1f2127ff");
};

using MainDevViewDef = FlowUi::ElementDefinition<
	mainDevViewParams,
	void,
	void,
	FLOW_DEF_ID("MainDevView"),
	true>;

inline const MainDevViewDef kMainDevView = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](MainDevViewDef::BuildContext& context) {
		int width = context.params.width;
		if (width < 0)
		{
			width = 0;
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_FIXED(static_cast<float>(width)),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
			context.uiManager
				.createElement(kDevHeader, context.createChildElementId("header"))
				.setParameters(devHeaderParams{})
				.draw();

			context.uiManager
				.createElement(kDevPanelContent, context.createChildElementId("content"))
				.setParameters(devPanelContentParams{})
				.draw();

			context.uiManager
				.createElement(kDevFooter, context.createChildElementId("footer"))
				.setParameters(devFooterParams{
					.height = context.params.footerHeight,
				})
				.draw();
		};
	},
};
