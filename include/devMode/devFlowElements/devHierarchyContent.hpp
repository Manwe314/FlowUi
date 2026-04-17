#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicButton.hpp"
#include "devMode/devFlowElements/devPanelContentShared.hpp"
#include "devMode/devFlowElements/devPropertiesSelection.hpp"
#include "devMode/devFlowElements/devPropertiesContentBase.hpp"

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
