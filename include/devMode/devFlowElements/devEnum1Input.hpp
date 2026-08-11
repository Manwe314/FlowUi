#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicButton.hpp"

struct devEnum1InputOption {
	std::string value = "";
	std::string label = "";
};

struct devEnum1InputParams {
	std::string hintText = "type";
	std::string selectedValue = "";
	std::string selectedLabel = "";
	std::vector<devEnum1InputOption> options{};
	std::string emptyValueText = "<enum>";
	std::string emptyOptionsText = "<no options>";
	std::function<void(std::string_view)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(220),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 6;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");

	Clay_Sizing triggerSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding triggerPadding = CLAY_PADDING_ALL(8);
	Clay_Color triggerBackgroundColor = FlowUi::Flow_Color("#252932ff");
	Clay_Color triggerHoverBackgroundColor = FlowUi::Flow_Color("#2f3540ff");
	Clay_Color triggerBorderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth triggerBorderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius triggerCornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Sizing triggerIconContainerSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(12),
		.height = CLAY_SIZING_FIXED(12),
	};
	uint16_t triggerChildGap = 6;

	int16_t outsideDismissZIndex = 200;
	int16_t dropdownZIndex = 201;
	uint16_t dropdownGapPx = 2;
	Clay_Color dropdownBackgroundColor = FlowUi::Flow_Color("#252932ff");
	Clay_Color dropdownBorderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth dropdownBorderWidth = Clay_BorderWidth{1, 1, 1, 1, 1};
	Clay_CornerRadius dropdownCornerRadius = CLAY_CORNER_RADIUS(6);

	Clay_Padding optionPadding = CLAY_PADDING_ALL(8);
	Clay_Color optionTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_Color optionHoverBackgroundColor = FlowUi::Flow_Color("#3f4452ff");
};

struct devEnum1InputState {
	bool isExpanded = false;
};

struct devEnum1InputResources {
	bool disclosureIconsPrepared = false;
	FlowUi::TextureRef downArrowIcon = FlowUi::TextureRef{};
	FlowUi::TextureRef upArrowIcon = FlowUi::TextureRef{};

	explicit devEnum1InputResources(FlowUi::App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		constexpr std::string_view downKey = "flowui/dev/enum1/arrow-down";
		constexpr std::string_view upKey = "flowui/dev/enum1/arrow-up";
		(void)app.icons().registerSvg(downKey, ::kDownArrow);
		(void)app.icons().registerSvg(upKey, ::kUpArrow);
		downArrowIcon = app.icons().textureRef(downKey);
		upArrowIcon = app.icons().textureRef(upKey);
#else
		(void)app;
#endif
		disclosureIconsPrepared = true;
	}
};

using DevEnum1InputDef = FlowUi::ElementDefinition<
	devEnum1InputParams,
	devEnum1InputState,
	devEnum1InputResources,
	FLOW_DEF_ID("DevEnum1Input"),
	true>;

inline const DevEnum1InputDef kDevEnum1Input = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevEnum1InputDef::BuildContext& context) {
		devEnum1InputState& state = context.state();
		const uint64_t elementFlowId = FlowUi::toFlowId(context.elementID);

		const devEnum1InputResources& resources = context.resources();
		const FlowUi::TextureRef downArrowIcon = resources.disclosureIconsPrepared
			? resources.downArrowIcon
			: FlowUi::TextureRef{};
		const FlowUi::TextureRef upArrowIcon = resources.disclosureIconsPrepared
			? resources.upArrowIcon
			: FlowUi::TextureRef{};

		const FlowUi::TextureRef disclosureIcon = state.isExpanded ? downArrowIcon : upArrowIcon;
		std::string valueText = context.params.selectedLabel;
		if (valueText.empty())
		{
			valueText = context.params.selectedValue;
		}
		if (valueText.empty())
		{
			valueText = context.params.emptyValueText;
		}
		const std::string hintWithColon =
			(context.params.hintText.empty() ? std::string("type") : context.params.hintText) + ":";

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig hintTextConfig{};
		hintTextConfig.textColor = context.params.hintTextColor;
		hintTextConfig.fontId = context.params.fontId;
		hintTextConfig.fontSize = context.params.fontSize;
		hintTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		hintTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		CLAY(rootId, root){
			CLAY(context.uiManager.toClayEID(context.createChildElementId("hint")), {}){
				CLAY_TEXT(
					context.uiManager.toClayString(hintWithColon),
					CLAY_TEXT_CONFIG(hintTextConfig));
			};

			if (state.isExpanded)
			{
				Clay_ElementDeclaration dismissLayer{};
				const Clay_ElementId dismissLayerId = context.uiManager.toClayEID(context.createChildElementId("dismiss-layer"));
				dismissLayer.layout.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0),
				};
				dismissLayer.backgroundColor = FlowUi::Flow_Color("#00000000");
				dismissLayer.floating = {
					.zIndex = context.params.outsideDismissZIndex,
					.attachPoints = {
						.element = CLAY_ATTACH_POINT_LEFT_TOP,
						.parent = CLAY_ATTACH_POINT_LEFT_TOP,
					},
					.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
					.attachTo = CLAY_ATTACH_TO_ROOT,
				};

				CLAY(dismissLayerId, dismissLayer){
					context.uiManager
						.createElement(kDevBasicButton, context.createChildElementId("dismiss-layer/button"))
						.setParameters(devBasicButtonParams{
							.onPressedCallback = [elementFlowId](DevBasicButtonInteractionContext buttonContext) {
								devEnum1InputState* latestState = buttonContext.uiManager.elements().modifyState(
									kDevEnum1Input, buttonContext.uiManager.windowId(), elementFlowId);
								if (latestState != nullptr)
								{
									latestState->isExpanded = false;
								}
							},
							.contentMode = devBasicButtonParams::ContentMode::None,
							.padding = CLAY_PADDING_ALL(0),
							.sizing = {
								.width = CLAY_SIZING_GROW(0),
								.height = CLAY_SIZING_GROW(0),
							},
							.backgroundColor = FlowUi::Flow_Color("#00000000"),
							.borderColor = FlowUi::Flow_Color("#00000000"),
							.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
						})
						.draw();
				};
			}

			Clay_ElementDeclaration triggerAnchor{};
			const Clay_ElementId triggerAnchorId = context.uiManager.toClayEID(context.createChildElementId("trigger-anchor"));
			triggerAnchor.layout.sizing = {
				.width = CLAY_SIZING_FIT(0),
				.height = CLAY_SIZING_FIT(0),
			};
			triggerAnchor.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			triggerAnchor.layout.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_CENTER,
			};
			triggerAnchor.backgroundColor = FlowUi::Flow_Color("#00000000");

			CLAY(triggerAnchorId, triggerAnchor){
				context.uiManager
					.createElement(kDevBasicButton, context.createChildElementId("trigger"))
					.setParameters(devBasicButtonParams{
						.text = valueText,
						.icon = disclosureIcon,
						.onHoveredCallback = [hoverColor = context.params.triggerHoverBackgroundColor](
							DevBasicButtonInteractionContext buttonContext) {
							buttonContext.params.backgroundColor = hoverColor;
						},
						.onPressedCallback = [elementFlowId](DevBasicButtonInteractionContext buttonContext) {
							devEnum1InputState* latestState = buttonContext.uiManager.elements().modifyState(
								kDevEnum1Input, buttonContext.uiManager.windowId(), elementFlowId);
							if (latestState == nullptr)
							{
								return;
							}
							latestState->isExpanded = !latestState->isExpanded;
						},
						.contentMode = devBasicButtonParams::ContentMode::TextThenIcon,
						.padding = context.params.triggerPadding,
						.sizing = context.params.triggerSizing,
						.backgroundColor = context.params.triggerBackgroundColor,
						.cornerRadius = context.params.triggerCornerRadius,
						.borderColor = context.params.triggerBorderColor,
						.borderWidth = context.params.triggerBorderWidth,
						.childLayoutDirection = CLAY_LEFT_TO_RIGHT,
						.childAlignment = {
							.x = CLAY_ALIGN_X_LEFT,
							.y = CLAY_ALIGN_Y_CENTER,
						},
						.childGap = context.params.triggerChildGap,
						.textWrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_LEFT,
						.fontId = context.params.fontId,
						.fontSize = context.params.fontSize,
						.textColor = context.params.valueTextColor,
						.iconContainerSizing = context.params.triggerIconContainerSizing,
						.iconContainerPadding = CLAY_PADDING_ALL(0),
						.iconContainerChildLayoutDirection = CLAY_LEFT_TO_RIGHT,
						.iconContainerChildAlignment = {
							.x = CLAY_ALIGN_X_CENTER,
							.y = CLAY_ALIGN_Y_CENTER,
						},
						.iconContainerChildGap = 0,
						.iconContainerBackgroundColor = FlowUi::Flow_Color("#00000000"),
						.iconContainerBorderColor = FlowUi::Flow_Color("#00000000"),
						.iconContainerBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
						.iconSizing = {
							.width = CLAY_SIZING_PERCENT(1.0f),
							.height = CLAY_SIZING_PERCENT(1.0f),
						},
						.iconTintColor = FlowUi::Flow_Color("#00000000"),
					})
					.draw();

				if (state.isExpanded)
				{
				Clay_ElementDeclaration dropdown{};
				const Clay_ElementId dropdownId = context.uiManager.toClayEID(context.createChildElementId("dropdown"));
				dropdown.layout.sizing = {
					.width = CLAY_SIZING_PERCENT(1.0f),
					.height = CLAY_SIZING_FIT(0),
				};
				dropdown.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
				dropdown.backgroundColor = context.params.dropdownBackgroundColor;
				dropdown.cornerRadius = context.params.dropdownCornerRadius;
				dropdown.border = {
					.color = context.params.dropdownBorderColor,
					.width = context.params.dropdownBorderWidth,
				};
				dropdown.floating = {
					.offset = {
						.x = 0.0f,
						.y = static_cast<float>(context.params.dropdownGapPx),
					},
					.zIndex = context.params.dropdownZIndex,
					.attachPoints = {
						.element = CLAY_ATTACH_POINT_CENTER_TOP,
						.parent = CLAY_ATTACH_POINT_CENTER_BOTTOM,
					},
					.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
					.attachTo = CLAY_ATTACH_TO_PARENT,
				};

				CLAY(dropdownId, dropdown){
					if (context.params.options.empty())
					{
						Clay_TextElementConfig emptyTextConfig{};
						emptyTextConfig.textColor = context.params.optionTextColor;
						emptyTextConfig.fontId = context.params.fontId;
						emptyTextConfig.fontSize = context.params.fontSize;
						emptyTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
						emptyTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

						CLAY(context.uiManager.toClayEID(context.createChildElementId("dropdown/empty")), {
							.layout = {
								.sizing = {
									.width = CLAY_SIZING_GROW(0),
									.height = CLAY_SIZING_FIT(0),
								},
								.padding = context.params.optionPadding,
							},
						}){
							CLAY_TEXT(
								context.uiManager.toClayString(context.params.emptyOptionsText),
								CLAY_TEXT_CONFIG(emptyTextConfig));
						};
					}
					else
					{
						for (std::size_t i = 0; i < context.params.options.size(); ++i)
						{
							const devEnum1InputOption option = context.params.options[i];
							const std::string optionLabel =
								option.label.empty()
								? option.value
								: option.label;
							context.uiManager
								.createElement(
									kDevBasicButton,
									context.createChildElementId(
										"dropdown/option-" + std::to_string(i)))
								.setParameters(devBasicButtonParams{
									.text = optionLabel,
									.onHoveredCallback = [hoverColor = context.params.optionHoverBackgroundColor](
										DevBasicButtonInteractionContext buttonContext) {
										buttonContext.params.backgroundColor = hoverColor;
									},
									.onPressedCallback = [
										elementFlowId,
										onValueChanged = context.params.onValueChangedCallback,
										selectedValue = context.params.selectedValue,
										optionValue = option.value
									](DevBasicButtonInteractionContext buttonContext) {
										devEnum1InputState* latestState = buttonContext.uiManager.elements().modifyState(
											kDevEnum1Input, buttonContext.uiManager.windowId(), elementFlowId);
										if (latestState != nullptr)
										{
											latestState->isExpanded = false;
										}
										if (
											onValueChanged != nullptr &&
											optionValue != selectedValue)
										{
											onValueChanged(optionValue);
										}
									},
									.contentMode = devBasicButtonParams::ContentMode::TextOnly,
									.padding = context.params.optionPadding,
									.sizing = {
										.width = CLAY_SIZING_GROW(0),
										.height = CLAY_SIZING_FIT(0),
									},
									.backgroundColor = FlowUi::Flow_Color("#00000000"),
									.borderColor = FlowUi::Flow_Color("#00000000"),
									.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
									.childAlignment = {
										.x = CLAY_ALIGN_X_LEFT,
										.y = CLAY_ALIGN_Y_CENTER,
									},
									.textWrapMode = CLAY_TEXT_WRAP_WORDS,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT,
									.fontId = context.params.fontId,
									.fontSize = context.params.fontSize,
									.textColor = context.params.optionTextColor,
								})
								.draw();
						}
					}
				};
				}
			}
		};
	},
};
