#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>
#include <utility>

#include "FSEL/RadioChoice.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr float kTitleHeight = 36.0f;
inline constexpr float kControlsHeight = 44.0f;
inline constexpr float kSearchHeight = 46.0f;
inline constexpr float kFooterHeight = 60.0f;
inline constexpr float kControlHeight = 28.0f;
inline constexpr float kNodeHeight = 25.0f;

inline constexpr LocalElementName kForestToggle{"forest-toggle"};
inline constexpr LocalElementName kFlowChoice{"flow-choice"};
inline constexpr LocalElementName kClayChoice{"clay-choice"};
inline constexpr LocalElementName kDefinitionFilter{"definition-filter"};
inline constexpr LocalElementName kSearchInput{"search-input"};
inline constexpr LocalElementName kForestContent{"content"};
inline constexpr LocalElementName kNodeRow{"node"};
inline constexpr LocalElementName kNodeDisclosure{"disclosure"};
inline constexpr LocalElementName kNodeIndicator{"selection-indicator"};
inline constexpr LocalElementName kNodeDepth{"depth"};
inline constexpr LocalElementName kNodeLabel{"label"};
inline constexpr LocalElementName kNodeSpacer{"spacer"};
inline constexpr LocalElementName kNodeStatus{"status"};

std::string_view localDebugName(std::string_view name) {
	const std::size_t separator = name.find_last_of('/');
	return separator == std::string_view::npos ? name : name.substr(separator + 1u);
}

Clay_TextElementConfig textConfig(Clay_Color color, uint16_t size) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontSize = size;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	config.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return config;
}

Clay_ElementDeclaration fixedSection(
	float height,
	Clay_Color background,
	Clay_Padding padding = {}) {
	Clay_ElementDeclaration section{};
	section.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(height),
	};
	section.layout.padding = padding;
	section.backgroundColor = background;
	section.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};
	return section;
}

FSEL::SelectableSurfaceStyle forestChoiceStyle() {
	FSEL::SelectableSurfaceStyle style{};
	style.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	style.padding = Clay_Padding{7, 7, 0, 0};
	style.childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	style.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	style.cornerRadius = CLAY_CORNER_RADIUS(2);
	style.idleOverrides.backgroundColor = interface_theme::kDepth0Keel;
	style.idleOverrides.borderColor = interface_theme::kDepth0Keel;
	style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	style.hoveredOverrides.borderColor = interface_theme::kHoverSurface;
	style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
	style.selectedOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;
	style.disabledOverrides.backgroundColor = interface_theme::kDepth0Keel;
	style.disabledOverrides.borderColor = interface_theme::kDepth0Keel;
	style.selectedDisabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	style.selectedDisabledOverrides.borderColor = interface_theme::kBorderPrimary;
	return style;
}

void drawForestChoice(
	DevInterfaceSelectorControls::BuildContext& context,
	LocalElementName id,
	std::string_view label,
	uint64_t value,
	bool enabled) {
	const bool selected = context.params.selectedForest &&
		*context.params.selectedForest == value;
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = context.params.selectedForest;
	parameters.enabled = enabled;
	parameters.style = forestChoiceStyle();

	context.uiManager.createElement(FSEL::kRadioChoice, id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.construct();
	const Clay_Color labelColor = !enabled
		? interface_theme::kTextMuted
		: selected ? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	const Clay_TextElementConfig labelStyle = textConfig(labelColor, 11);
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(labelStyle));
	context.uiManager.drawConstructed();
}

float nodeInset(uint32_t depth) noexcept {
	static constexpr float insets[] = {3, 11, 19, 27, 31, 35, 39, 43, 44, 45};
	return insets[std::min<std::size_t>(depth, std::size(insets) - 1u)];
}

uint64_t stableNodeKey(uint64_t value, uint64_t salt) noexcept {
	value ^= salt + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
	value ^= value >> 30u;
	value *= 0xbf58476d1ce4e5b9ull;
	value ^= value >> 27u;
	value *= 0x94d049bb133111ebull;
	value ^= value >> 31u;
	return value == 0u ? 1u : value;
}

bool containsInsensitive(std::string_view text, std::string_view query) {
	if (query.empty()) return true;
	if (query.size() > text.size()) return false;
	return std::search(
		text.begin(), text.end(), query.begin(), query.end(),
		[](char left, char right) {
			return std::tolower(static_cast<unsigned char>(left)) ==
				std::tolower(static_cast<unsigned char>(right));
		}) != text.end();
}

template <typename Index, typename Node, typename ParentFn>
std::vector<uint8_t> visibleNodes(
	std::span<const Node> nodes,
	bool filterActive,
	const auto& directMatch,
	ParentFn parentOf,
	Index invalid) {
	std::vector<uint8_t> visible(nodes.size(), filterActive ? 0u : 1u);
	if (!filterActive) return visible;
	for (std::size_t i = 0; i < nodes.size(); ++i) {
		const Index index = static_cast<Index>(i);
		if (!directMatch(index)) continue;
		for (Index current = index;
			current != invalid && current < nodes.size();
			current = parentOf(nodes[current])) {
			visible[current] = 1u;
		}
	}
	return visible;
}

void drawEmptyForestMessage(
	DevInterfaceSelectorForest::BuildContext& context,
	std::string_view message) {
	Clay_ElementDeclaration empty{};
	empty.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(42),
	};
	empty.layout.padding = Clay_Padding{10, 10, 12, 12};
	empty.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	const Clay_TextElementConfig style = textConfig(interface_theme::kTextMuted, 11);
	CLAY(context.clayID("empty"), empty) {
		CLAY_TEXT(context.uiManager.toClayString(message), CLAY_TEXT_CONFIG(style));
	}
}

} // namespace

void DevInterfaceSelectorTitle::buildElement(BuildContext& context) {
	Clay_ElementDeclaration title = fixedSection(
		kTitleHeight,
		interface_theme::kDepth2Ink,
		Clay_Padding{12, 12, 0, 0});
	title.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};

	const Clay_TextElementConfig titleStyle = textConfig(interface_theme::kTextCanvas, 14);
	CLAY(context.clayID(), title) {
		CLAY_TEXT(context.uiManager.toClayString("Ui Tree"), CLAY_TEXT_CONFIG(titleStyle));
	}
}

void DevInterfaceSelectorControls::buildElement(BuildContext& context) {
	Clay_ElementDeclaration controls = fixedSection(
		kControlsHeight,
		interface_theme::kDepth1Panel,
		Clay_Padding{8, 8, 7, 7});
	controls.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	controls.layout.childGap = 8;
	controls.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};

	CLAY(context.clayID(), controls) {
		Clay_ElementDeclaration toggle{};
		toggle.layout.sizing = {
			.width = CLAY_SIZING_FIXED(108),
			.height = CLAY_SIZING_FIXED(kControlHeight),
		};
		toggle.layout.padding = Clay_Padding{2, 2, 2, 2};
		toggle.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		toggle.backgroundColor = interface_theme::kDepth0Keel;
		toggle.cornerRadius = CLAY_CORNER_RADIUS(3);
		toggle.border = {
			.color = interface_theme::kBorderVisible,
			.width = Clay_BorderWidth{1, 1, 1, 1, 0},
		};

		CLAY(context.clayID(kForestToggle), toggle) {
			drawForestChoice(
				context, kFlowChoice, "Flow", kDevInterfaceFlowForest, true);
			drawForestChoice(
				context, kClayChoice, "Clay", kDevInterfaceClayForest,
				context.params.clayForestAvailable);
		}

		FSEL::ComboBoxParameters definitionFilter{};
		definitionFilter.options = context.params.definitionOptions;
		definitionFilter.selectedValue = context.params.selectedDefinition;
		definitionFilter.placeholder = "All";
		definitionFilter.enabled = context.params.selectedDefinition &&
			!context.params.definitionOptions.empty();
		definitionFilter.sizing = Clay_Sizing{
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(kControlHeight),
		};
		definitionFilter.popupWidthPolicy =
			FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
		definitionFilter.popupMaxHeight = 260.0f;
		definitionFilter.optionHeight = 28.0f;
		definitionFilter.fontSize = 11;

		context.uiManager.createElement(FSEL::kComboBox, kDefinitionFilter)
			.setParameters(std::move(definitionFilter))
			.setDevInternalCapture(true)
			.draw();
	}
}

void DevInterfaceSelectorSearch::buildElement(BuildContext& context) {
	Clay_ElementDeclaration search = fixedSection(
		kSearchHeight,
		interface_theme::kDepth1Panel,
		Clay_Padding{8, 8, 8, 8});
	search.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};

	CLAY(context.clayID(), search) {
		FSEL::TextInputParameters input{};
		input.value = context.params.query;
		input.placeholder = "Filter nodes...";
		input.enabled = context.params.query != nullptr;
		input.sizing = Clay_Sizing{
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(30),
		};
		input.padding = Clay_Padding{8, 8, 5, 5};
		input.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
		input.cornerRadius = CLAY_CORNER_RADIUS(3);
		input.fontSize = 12;
		input.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
		input.idleOverrides.textColor = interface_theme::kTextCanvas;
		input.idleOverrides.placeholderColor = interface_theme::kTextMuted;
		input.idleOverrides.borderColor = interface_theme::kBorderVisible;
		input.hoveredOverrides.backgroundColor = interface_theme::kDepth3Elevated;
		input.hoveredOverrides.textColor = interface_theme::kTextCanvas;
		input.hoveredOverrides.placeholderColor = interface_theme::kTextSecondary;
		input.hoveredOverrides.borderColor = interface_theme::kTextMuted;
		input.focusedOverrides.backgroundColor = interface_theme::kDepth3Elevated;
		input.focusedOverrides.textColor = interface_theme::kTextCanvas;
		input.focusedOverrides.placeholderColor = interface_theme::kTextMuted;
		input.focusedOverrides.borderColor = interface_theme::kAccentCurrent;
		input.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
		input.disabledOverrides.textColor = interface_theme::kTextMuted;
		input.disabledOverrides.placeholderColor = interface_theme::kTextMuted;
		input.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
		input.caret.color = interface_theme::kAccentCurrent;
		input.caret.selectionBoxColor = interface_theme::kSelectedRow;

		context.uiManager.createElement(FSEL::kTextInput, kSearchInput)
			.setParameters(std::move(input))
			.setDevInternalCapture(true)
			.draw();
	}
}

void DevNode::onHovered(InteractionContext& context) {
	context.uiManager.requestCursor(CursorType::PointingHand, 4);
}

void DevNode::onPressed(InteractionContext& context) {
	DevNodeState& state = context.state();
	state.isArmed = true;
	state.mouseUpObserved = false;
}

void DevNode::onReleased(InteractionContext& context) {
	DevNodeState& nodeState = context.state();
	if (!nodeState.isArmed) return;
	nodeState.isArmed = false;
	nodeState.mouseUpObserved = false;

	if (context.params.hasChildren && context.previousInteraction.isReleased(
		context.clayID(kNodeDisclosure))) {
		nodeState.isExpanded = !nodeState.isExpanded;
		return;
	}
	DevInterfaceState* state = context.params.interfaceState;
	if (!state || context.params.selectionKey == 0u) return;
	const bool selected = state->inspectSelectedNodeKind == context.params.kind &&
		state->inspectSelectedNodeKey == context.params.selectionKey;
	if (selected) {
		state->inspectSelectedNodeKind = 0u;
		state->inspectSelectedNodeKey = 0u;
		state->selectedElementId = {};
		return;
	}
	state->inspectSelectedNodeKind = context.params.kind;
	state->inspectSelectedNodeKey = context.params.selectionKey;
	state->selectedElementId = context.params.kind == kDevInterfaceFlowNodeKind
		? FlowElementID{.value = context.params.selectionKey}
		: FlowElementID{};
}

DevNodeResources::DevNodeResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	if (icons.contains(interface_icons::kTreeExpandedKey)) {
		expandedIcon = icons.textureRef(interface_icons::kTreeExpandedKey);
	}
	if (icons.contains(interface_icons::kTreeCollapsedKey)) {
		collapsedIcon = icons.textureRef(interface_icons::kTreeCollapsedKey);
	}
#else
	(void)app;
#endif
}

void DevNode::runLogic(InteractionContext& context) {
	DevNodeState& state = context.state();
	if (!state.isArmed) return;
	if (context.uiManager.getCurrentFrameInput().mouseDown[0]) {
		if (state.mouseUpObserved) {
			state.isArmed = false;
			state.mouseUpObserved = false;
		}
		return;
	}
	if (state.mouseUpObserved) {
		state.isArmed = false;
		state.mouseUpObserved = false;
	} else {
		state.mouseUpObserved = true;
	}
}

void DevNode::buildElement(BuildContext& context) {
	if (context.params.expandedOutput) {
		*context.params.expandedOutput = context.state().isExpanded;
	}
	const DevInterfaceState* interfaceState = context.params.interfaceState;
	const bool selected = interfaceState &&
		interfaceState->inspectSelectedNodeKind == context.params.kind &&
		interfaceState->inspectSelectedNodeKey == context.params.selectionKey;
	const bool hovered = context.uiManager.getPreviousFramesInteraction()
		.isHovered(context.clayID());
	const Clay_Color rowColor = selected ? interface_theme::kSelectedRow
		: hovered ? interface_theme::kHoverSurface : interface_theme::kDepth0Keel;

	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(kNodeHeight),
	};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	row.backgroundColor = rowColor;

	CLAY(context.clayID(), row) {
		Clay_ElementDeclaration indicator{};
		indicator.layout.sizing = {
			.width = CLAY_SIZING_FIXED(3),
			.height = CLAY_SIZING_GROW(0),
		};
		indicator.backgroundColor = selected ? interface_theme::kAccentCurrent : rowColor;
		CLAY(context.clayID(kNodeIndicator), indicator);

		Clay_ElementDeclaration depth{};
		depth.layout.sizing = {
			.width = CLAY_SIZING_FIXED(nodeInset(context.params.depth)),
			.height = CLAY_SIZING_GROW(0),
		};
		CLAY(context.clayID(kNodeDepth), depth);

		if (context.params.hasChildren) {
			Clay_ElementDeclaration disclosure{};
			disclosure.layout.sizing = {
				.width = CLAY_SIZING_FIXED(14),
				.height = CLAY_SIZING_GROW(0),
			};
			disclosure.layout.childAlignment = {
				CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID(kNodeDisclosure), disclosure) {
				TextureRef icon = context.state().isExpanded
					? context.resources().expandedIcon
					: context.resources().collapsedIcon;
				if (icon.handle) {
					icon.tintEnabled = true;
					Clay_ElementDeclaration image{};
					image.layout.sizing = {
						.width = CLAY_SIZING_FIXED(12),
						.height = CLAY_SIZING_FIXED(12),
					};
					image.backgroundColor = interface_theme::kTextSecondary;
					image.image = {.imageData = context.uiManager.imageData(icon)};
					CLAY(context.clayID("disclosure-icon"), image);
				} else {
					const Clay_TextElementConfig fallback = textConfig(
						interface_theme::kTextSecondary, 10);
					CLAY_TEXT(
						context.uiManager.toClayString(
							context.state().isExpanded ? "v" : ">"),
						CLAY_TEXT_CONFIG(fallback));
				}
			}
		} else {
			Clay_ElementDeclaration disclosureSpacer{};
			disclosureSpacer.layout.sizing = {
				.width = CLAY_SIZING_FIXED(14),
				.height = CLAY_SIZING_GROW(0),
			};
			CLAY(context.clayID(kNodeDisclosure), disclosureSpacer);
		}

		Clay_ElementDeclaration label{};
		label.layout.sizing = {
			.width = CLAY_SIZING_FIT(0),
			.height = CLAY_SIZING_GROW(0),
		};
		label.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
		label.clip.horizontal = true;
		label.clip.scrollInputDisabled = true;
		const Clay_TextElementConfig labelStyle = textConfig(
			(selected || hovered) ? interface_theme::kTextCanvas
				: interface_theme::kTextSecondary,
			11);
		CLAY(context.clayID(kNodeLabel), label) {
			CLAY_TEXT(
				context.uiManager.toClayString(context.params.debugName),
				CLAY_TEXT_CONFIG(labelStyle));
		}

		Clay_ElementDeclaration spacer{};
		spacer.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		CLAY(context.clayID(kNodeSpacer), spacer);

		Clay_ElementDeclaration status{};
		status.layout.sizing = {
			.width = CLAY_SIZING_FIXED(16),
			.height = CLAY_SIZING_GROW(0),
		};
		status.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
		CLAY(context.clayID(kNodeStatus), status) {
			if (context.params.hasChanges) {
				Clay_ElementDeclaration dot{};
				dot.layout.sizing = {
					.width = CLAY_SIZING_FIXED(6),
					.height = CLAY_SIZING_FIXED(6),
				};
				dot.backgroundColor = interface_theme::kAccentSignalCoral;
				dot.cornerRadius = CLAY_CORNER_RADIUS(3);
				CLAY(context.clayID("changes"), dot);
			}
		}
	}
}

void DevInterfaceSelectorForest::buildElement(BuildContext& context) {
	Clay_ElementDeclaration forest{};
	forest.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	forest.backgroundColor = interface_theme::kDepth0Keel;
	forest.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};
	forest.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(context.clayID());
	forest.clip = {
		.vertical = true,
		.childOffset = scroll.found && scroll.scrollPosition
			? *scroll.scrollPosition : Clay_Vector2{},
	};

	CLAY(context.clayID(), forest) {
		Clay_ElementDeclaration content{};
		content.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		content.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(context.clayID(kForestContent), content) {
			DevInterfaceState* state = context.params.interfaceState;
			App* app = context.params.app;
			const tooling::DevTreeSnapshot* snapshotPointer =
				state && app && app->hasWindow(state->selectedWindowId)
					? &app->ui(state->selectedWindowId).devTreeSnapshot() : nullptr;
			if (!snapshotPointer) {
				drawEmptyForestMessage(context, "No captured UI tree");
			} else {
			const tooling::DevTreeSnapshot& snapshot = *snapshotPointer;
			const bool filterActive = state->inspectDefinitionFilter !=
				kDevInterfaceAllDefinitions || !state->searchQuery.empty();
			bool drewNode = false;

			if (state->inspectForest == kDevInterfaceFlowForest) {
				const auto matches = [&](tooling::DevFlowNodeIndex index) {
					const tooling::DevFlowNode& node = snapshot.flow.nodes[index];
					if (state->inspectDefinitionFilter != kDevInterfaceAllDefinitions &&
						node.definition.value != state->inspectDefinitionFilter) return false;
					std::string_view name = snapshot.string(node.debugName);
					if (name.empty()) name = snapshot.string(node.definitionName);
					return containsInsensitive(name, state->searchQuery);
				};
				const std::vector<uint8_t> visible = visibleNodes<tooling::DevFlowNodeIndex>(
					std::span<const tooling::DevFlowNode>(snapshot.flow.nodes), filterActive, matches,
					[](const tooling::DevFlowNode& node) { return node.parent; },
					tooling::InvalidFlowNode);
				auto drawBranch = [&](auto&& self, tooling::DevFlowNodeIndex index) -> void {
					for (tooling::DevFlowNodeIndex current = index;
						current != tooling::InvalidFlowNode && current < snapshot.flow.nodes.size();
						current = snapshot.flow.nodes[current].nextSibling) {
						if (!visible[current]) continue;
						const tooling::DevFlowNode& node = snapshot.flow.nodes[current];
						std::string_view name = snapshot.string(node.debugName);
						if (name.empty()) name = snapshot.string(node.definitionName);
						if (name.empty()) name = "Unnamed Flow Element";
						name = localDebugName(name);
						const uint64_t rowKey = stableNodeKey(node.instance.value, kDevInterfaceFlowNodeKind);
						bool rowExpanded = true;
						context.uiManager.createElement(kDevNode, Keyed(kNodeRow, rowKey))
							.setParameters(DevNodeParameters{
								.interfaceState = state,
								.expandedOutput = &rowExpanded,
								.kind = kDevInterfaceFlowNodeKind,
								.selectionKey = node.instance.value,
								.depth = node.depth,
								.debugName = name,
								.hasChildren = node.firstChild != tooling::InvalidFlowNode,
							})
							.setDevInternalCapture(true)
							.draw();
						drewNode = true;
						if (node.firstChild != tooling::InvalidFlowNode && rowExpanded) {
							self(self, node.firstChild);
						}
					}
				};
				for (tooling::DevFlowNodeIndex root : snapshot.flow.roots) drawBranch(drawBranch, root);
			}
#if FLOW_UI_DEV_CAPTURE_CLAY
			else {
				const auto matches = [&](tooling::DevClayNodeIndex index) {
					const tooling::DevClayNode& node = snapshot.clay.nodes[index];
					if (state->inspectDefinitionFilter != kDevInterfaceAllDefinitions) {
						if (node.directFlowOwner == tooling::InvalidFlowNode ||
							node.directFlowOwner >= snapshot.flow.nodes.size() ||
							snapshot.flow.nodes[node.directFlowOwner].definition.value !=
								state->inspectDefinitionFilter) return false;
					}
					return containsInsensitive(snapshot.string(node.idString), state->searchQuery);
				};
				const std::vector<uint8_t> visible = visibleNodes<tooling::DevClayNodeIndex>(
					std::span<const tooling::DevClayNode>(snapshot.clay.nodes), filterActive, matches,
					[](const tooling::DevClayNode& node) { return node.parent; },
					tooling::InvalidClayNode);
				auto drawBranch = [&](auto&& self, tooling::DevClayNodeIndex index) -> void {
					for (tooling::DevClayNodeIndex current = index;
						current != tooling::InvalidClayNode && current < snapshot.clay.nodes.size();
						current = snapshot.clay.nodes[current].nextSibling) {
						if (!visible[current]) continue;
						const tooling::DevClayNode& node = snapshot.clay.nodes[current];
						char fallback[40]{};
						std::string_view name = snapshot.string(node.idString);
						if (name.empty()) {
							std::snprintf(fallback, sizeof(fallback), "Clay #%u", node.clayId);
							name = fallback;
						}
						const uint64_t selectionKey = stableNodeKey(
							(static_cast<uint64_t>(node.rootIndex) << 32u) | current,
							node.clayId);
						const uint64_t rowKey = stableNodeKey(selectionKey, kDevInterfaceClayNodeKind);
						bool rowExpanded = true;
						context.uiManager.createElement(kDevNode, Keyed(kNodeRow, rowKey))
							.setParameters(DevNodeParameters{
								.interfaceState = state,
								.expandedOutput = &rowExpanded,
								.kind = kDevInterfaceClayNodeKind,
								.selectionKey = selectionKey,
								.depth = node.depthWithinRoot,
								.debugName = name,
								.hasChildren = node.firstChild != tooling::InvalidClayNode,
							})
							.setDevInternalCapture(true)
							.draw();
						drewNode = true;
						if (node.firstChild != tooling::InvalidClayNode && rowExpanded) {
							self(self, node.firstChild);
						}
					}
				};
				for (const tooling::DevClayRoot& root : snapshot.clay.roots) {
					drawBranch(drawBranch, root.node);
				}
			}
#endif
			if (!drewNode) drawEmptyForestMessage(context, "No nodes match the filters");
			}
		}
	}
}

void DevInterfaceSelectorFooter::buildElement(BuildContext& context) {
	Clay_ElementDeclaration footer = fixedSection(
		kFooterHeight,
		interface_theme::kDepth2Ink,
		Clay_Padding{10, 10, 6, 6});
	footer.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	footer.layout.childGap = 2;
	footer.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	// The forest surface directly above owns the single separating hairline.
	footer.border.width = Clay_BorderWidth{0, 0, 0, 0, 0};

	char flowText[48]{};
	char clayText[48]{};
	char generationText[64]{};
	std::snprintf(
		flowText, sizeof(flowText), "Flow Nodes: %u", context.params.flowNodeCount);
	if (context.params.clayNodeCountKnown) {
		std::snprintf(
			clayText, sizeof(clayText), "Clay Nodes: %u", context.params.clayNodeCount);
	} else {
		std::snprintf(clayText, sizeof(clayText), "Clay Nodes: unavailable");
	}
	std::snprintf(
		generationText, sizeof(generationText), "Forest Generation: %llu",
		static_cast<unsigned long long>(context.params.forestGeneration));

	const Clay_TextElementConfig reportStyle = textConfig(interface_theme::kTextMuted, 10);
	CLAY(context.clayID(), footer) {
		CLAY_TEXT(context.uiManager.toClayString(flowText), CLAY_TEXT_CONFIG(reportStyle));
		CLAY_TEXT(context.uiManager.toClayString(clayText), CLAY_TEXT_CONFIG(reportStyle));
		CLAY_TEXT(context.uiManager.toClayString(generationText), CLAY_TEXT_CONFIG(reportStyle));
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
