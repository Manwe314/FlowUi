#pragma once

#include <array>

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicButton.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"

struct devNineSplitParams {
	std::string fieldIdPrefix = "";
	std::vector<uint8_t> numericSlots{};
	std::array<double, 9> slotValues{};
	devNumericInputValueKind valueKind = devNumericInputValueKind::UnsignedInt;
	double minValue = 0.0;
	double maxValue = static_cast<double>(std::numeric_limits<uint16_t>::max());
	double floatRatePerPixel = 0.01;
	double integerRatePerPixel = 1.0;
	std::function<void(const std::array<double, 9>&)> onValuesChangedCallback = nullptr;

	bool linkDefaultEnabled = true;
	Clay_Color linkEnabledBackgroundColor = FlowUi::Flow_Color("#4b8c5aff");
	Clay_Color linkDisabledBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color linkBorderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth linkBorderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius linkCornerRadius = CLAY_CORNER_RADIUS(10);
	Clay_Padding linkPadding = CLAY_PADDING_ALL(2);
	Clay_Sizing linkSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(18),
		.height = CLAY_SIZING_FIXED(18),
	};
	Clay_Sizing linkIconContainerSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(10),
		.height = CLAY_SIZING_FIXED(10),
	};
	Clay_Sizing linkIconSizing = Clay_Sizing{
		.width = CLAY_SIZING_PERCENT(1.0f),
		.height = CLAY_SIZING_PERCENT(1.0f),
	};
	Clay_Color linkIconTintColor = FlowUi::Flow_Color("#00000000");

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t rowGap = 4;
	uint16_t columnGap = 4;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	Clay_Sizing slotSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(50),
		.height = CLAY_SIZING_FIXED(50),
	};
	Clay_Padding slotPadding = CLAY_PADDING_ALL(2);
	uint16_t cornerInnerPadding = 2;
	uint16_t cornerOuterPadding = 5;
	Clay_Color slotBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color slotBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth slotBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	Clay_CornerRadius slotCornerRadius = CLAY_CORNER_RADIUS(6);

	Clay_Sizing numericSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(46),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding numericInputPadding = CLAY_PADDING_ALL(5);
	Clay_Color numericInputBackgroundColor = FlowUi::Flow_Color("#252932ff");
	Clay_Color numericInputBorderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth numericInputBorderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius numericInputCornerRadius = CLAY_CORNER_RADIUS(6);

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

struct devNineSplitState {
	bool initialized = false;
	bool linkEnabled = true;
	std::array<bool, 9> hasNumeric{};
	std::array<double, 9> slotValues{};
	std::array<int, 9> cachedIntValues{};
	std::array<std::string, 9> slotInputElementIds{};
};

struct devNineSplitResources {
	bool linkIconPrepared = false;
	FlowUi::TextureRef linkIcon = FlowUi::TextureRef{};

	explicit devNineSplitResources(FlowUi::App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		constexpr std::string_view key = "flowui/dev/nine-split/link";
		(void)app.icons().registerSvg(key, ::kLink);
		linkIcon = app.icons().textureRef(key);
#else
		(void)app;
#endif
		linkIconPrepared = true;
	}
};

using DevNineSplitDef = FlowUi::ElementDefinition<
	devNineSplitParams,
	devNineSplitState,
	devNineSplitResources,
	FLOW_DEF_ID("DevNineSplit"),
	true>;

inline const DevNineSplitDef kDevNineSplit = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevNineSplitDef::BuildContext& context) {
		devNineSplitState& state = context.state();
		const uint64_t elementFlowId = FlowUi::toFlowId(context.elementID);

		if (!state.initialized)
		{
			state.linkEnabled = context.params.linkDefaultEnabled;
			state.initialized = true;
		}

		state.hasNumeric.fill(false);
		state.slotInputElementIds.fill(std::string{});
		for (uint8_t slotId : context.params.numericSlots)
		{
			if (slotId < 9u && slotId != 4u)
			{
				state.hasNumeric[slotId] = true;
			}
		}

		for (uint8_t slotId = 0u; slotId < 9u; ++slotId)
		{
			if (!state.hasNumeric[slotId])
			{
				continue;
			}
			state.slotInputElementIds[slotId] =
				context.createChildElementId("slot-input-" + std::to_string(slotId));
		}

		for (uint8_t slotId = 0u; slotId < 9u; ++slotId)
		{
			if (state.hasNumeric[slotId])
			{
				state.slotValues[slotId] = context.params.slotValues[slotId];
				if (context.params.valueKind == devNumericInputValueKind::Floating)
				{
					state.cachedIntValues[slotId] =
						static_cast<int>(std::lround(context.params.slotValues[slotId]));
				}
				else
				{
					state.cachedIntValues[slotId] =
						static_cast<int>(std::llround(context.params.slotValues[slotId]));
				}
			}
			else
			{
				state.slotValues[slotId] = 0.0;
				state.cachedIntValues[slotId] = std::numeric_limits<int>::min();
			}
		}

		const devNineSplitResources& resources = context.resources();
		const FlowUi::TextureRef linkIcon = resources.linkIconPrepared
			? resources.linkIcon
			: FlowUi::TextureRef{};

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.rowGap;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_TOP,
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(rootId, root){
			for (int row = 0; row < 3; ++row)
			{
				Clay_ElementDeclaration rowDeclaration{};
				const Clay_ElementId rowDeclarationId = context.uiManager.toClayEID(
					context.createChildElementId("row-" + std::to_string(row)));
				rowDeclaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				rowDeclaration.layout.childGap = context.params.columnGap;
				rowDeclaration.backgroundColor = FlowUi::Flow_Color("#00000000");

				CLAY(rowDeclarationId, rowDeclaration){
					for (int col = 0; col < 3; ++col)
					{
						const uint8_t slotId = static_cast<uint8_t>(row * 3 + col);
						Clay_ElementDeclaration slotDeclaration{};
						const Clay_ElementId slotDeclarationId = context.uiManager.toClayEID(
							context.createChildElementId("slot-" + std::to_string(slotId)));
						slotDeclaration.layout.sizing = context.params.slotSizing;
						slotDeclaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
						Clay_Padding resolvedSlotPadding = context.params.slotPadding;
						if (state.hasNumeric[slotId])
						{
							if (slotId == 0u || slotId == 2u || slotId == 6u || slotId == 8u)
							{
								resolvedSlotPadding.left = context.params.cornerInnerPadding;
								resolvedSlotPadding.right = context.params.cornerInnerPadding;
								resolvedSlotPadding.top = context.params.cornerInnerPadding;
								resolvedSlotPadding.bottom = context.params.cornerInnerPadding;

								if (slotId == 0u)
								{
									resolvedSlotPadding.left = context.params.cornerOuterPadding;
									resolvedSlotPadding.top = context.params.cornerOuterPadding;
								}
								else if (slotId == 2u)
								{
									resolvedSlotPadding.right = context.params.cornerOuterPadding;
									resolvedSlotPadding.top = context.params.cornerOuterPadding;
								}
								else if (slotId == 6u)
								{
									resolvedSlotPadding.left = context.params.cornerOuterPadding;
									resolvedSlotPadding.bottom = context.params.cornerOuterPadding;
								}
								else
								{
									resolvedSlotPadding.right = context.params.cornerOuterPadding;
									resolvedSlotPadding.bottom = context.params.cornerOuterPadding;
								}
							}
						}
						slotDeclaration.layout.padding = resolvedSlotPadding;
						slotDeclaration.layout.childAlignment = {
							.x = CLAY_ALIGN_X_CENTER,
							.y = CLAY_ALIGN_Y_CENTER,
						};
						slotDeclaration.backgroundColor = context.params.slotBackgroundColor;
						slotDeclaration.border = {
							.color = context.params.slotBorderColor,
							.width = context.params.slotBorderWidth,
						};
						slotDeclaration.cornerRadius = context.params.slotCornerRadius;

						CLAY(slotDeclarationId, slotDeclaration){
							if (slotId == 4u)
							{
								context.uiManager
									.createElement(kDevBasicButton, context.createChildElementId("center-link"))
									.setParameters(devBasicButtonParams{
										.icon = linkIcon,
										.onPressedCallback = [elementFlowId](DevBasicButtonInteractionContext buttonContext) {
											devNineSplitState* latestState = buttonContext.uiManager.elements().modifyState(
												kDevNineSplit, buttonContext.uiManager.windowId(), elementFlowId);
											if (latestState != nullptr)
											{
												latestState->linkEnabled = !latestState->linkEnabled;
											}
										},
										.contentMode = devBasicButtonParams::ContentMode::IconOnly,
										.padding = context.params.linkPadding,
										.sizing = context.params.linkSizing,
										.backgroundColor =
											state.linkEnabled
											? context.params.linkEnabledBackgroundColor
											: context.params.linkDisabledBackgroundColor,
										.cornerRadius = context.params.linkCornerRadius,
										.borderColor = context.params.linkBorderColor,
										.borderWidth = context.params.linkBorderWidth,
										.childLayoutDirection = CLAY_LEFT_TO_RIGHT,
										.childAlignment = {
											.x = CLAY_ALIGN_X_CENTER,
											.y = CLAY_ALIGN_Y_CENTER,
										},
										.iconContainerSizing = context.params.linkIconContainerSizing,
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
										.iconSizing = context.params.linkIconSizing,
										.iconTintColor = context.params.linkIconTintColor,
									})
									.draw();
							}
							else if (state.hasNumeric[slotId])
							{
								const std::string& slotInputElementId = state.slotInputElementIds[slotId];
								context.uiManager
									.createElement(
										kDevNumericInput,
										slotInputElementId)
									.setParameters(devNumericInputParams{
										.fieldId = context.params.fieldIdPrefix.empty()
											? context.createChildElementId("slot-field-" + std::to_string(slotId))
											: context.params.fieldIdPrefix + "/slot-" + std::to_string(slotId),
										.initialText = devNumericValueToText(
											context.params.valueKind,
											state.slotValues[slotId]),
										.hintText = "",
										.showHint = false,
										.valueKind = context.params.valueKind,
										.minValue = context.params.minValue,
										.maxValue = context.params.maxValue,
										.floatRatePerPixel = context.params.floatRatePerPixel,
										.integerRatePerPixel = context.params.integerRatePerPixel,
										.onValueChangedCallback = [
											uiManager = &context.uiManager,
											elementFlowId,
											slotId,
											valueKind = context.params.valueKind,
											onValuesChanged = context.params.onValuesChangedCallback
										](double changedValue) {
											devNineSplitState* latestState = uiManager->elements().modifyState(
												kDevNineSplit, uiManager->windowId(), elementFlowId);
											if (latestState == nullptr)
											{
												return;
											}

											if (latestState->linkEnabled)
											{
												for (uint8_t i = 0u; i < 9u; ++i)
												{
													if (!latestState->hasNumeric[i])
													{
														continue;
													}
													latestState->slotValues[i] = changedValue;
												}
											}
											else if (latestState->hasNumeric[slotId])
											{
												latestState->slotValues[slotId] = changedValue;
											}

											for (uint8_t i = 0u; i < 9u; ++i)
											{
												if (!latestState->hasNumeric[i])
												{
													latestState->cachedIntValues[i] = std::numeric_limits<int>::min();
													continue;
												}

												if (valueKind == devNumericInputValueKind::Floating)
												{
													latestState->cachedIntValues[i] =
														static_cast<int>(std::lround(latestState->slotValues[i]));
												}
												else
												{
													latestState->cachedIntValues[i] =
														static_cast<int>(std::llround(latestState->slotValues[i]));
												}
											}

											const auto syncNumericInputState =
												[&](uint8_t syncedSlotId, bool forceFieldReset) {
												const std::string& syncedElementId =
													latestState->slotInputElementIds[syncedSlotId];
												if (syncedElementId.empty())
												{
													return;
												}

												devNumericInputState* numericState =
													uiManager->elements().modifyState(
														kDevNumericInput,
														uiManager->windowId(),
														FlowUi::toFlowId(syncedElementId));
												if (numericState == nullptr)
												{
													return;
												}

												const std::string nextText = devNumericValueToText(
													valueKind,
													latestState->slotValues[syncedSlotId]);
												const bool textChanged = numericState->normalizedText != nextText;
												numericState->hasValue = true;
												numericState->value = latestState->slotValues[syncedSlotId];
												numericState->normalizedText = nextText;
												if (forceFieldReset && textChanged)
												{
													numericState->pendingFieldReset = true;
												}
											};

											if (latestState->linkEnabled)
											{
												for (uint8_t i = 0u; i < 9u; ++i)
												{
													if (!latestState->hasNumeric[i])
													{
														continue;
													}
													syncNumericInputState(i, i != slotId);
												}
											}
											else
											{
												syncNumericInputState(slotId, false);
											}

											if (onValuesChanged != nullptr)
											{
												onValuesChanged(latestState->slotValues);
											}
										},
										.sizing = context.params.numericSizing,
										.padding = CLAY_PADDING_ALL(0),
										.childGap = 0,
										.inputSizing = {
											.width = CLAY_SIZING_GROW(0),
											.height = CLAY_SIZING_FIT(0),
										},
										.inputPadding = context.params.numericInputPadding,
										.inputBackgroundColor = context.params.numericInputBackgroundColor,
										.inputBorderColor = context.params.numericInputBorderColor,
										.inputBorderWidth = context.params.numericInputBorderWidth,
										.inputCornerRadius = context.params.numericInputCornerRadius,
										.fontId = context.params.fontId,
										.fontSize = context.params.fontSize,
										.valueTextColor = context.params.valueTextColor,
									})
									.draw();
							}
						};
					}
				};
			}
		};
	},
};
