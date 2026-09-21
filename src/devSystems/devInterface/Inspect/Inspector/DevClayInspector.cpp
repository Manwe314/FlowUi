#include "devSystems/devInterface/Inspect/Inspector/DevClayInspector.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdio>
#include <string>
#include <string_view>

#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

Clay_TextElementConfig clayTextConfig(Clay_Color color, uint16_t size, bool bold = false) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontSize = size;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	config.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	(void)bold;
	return config;
}

const tooling::DevClayNode* findClayNodeBySelectionKey(
	const tooling::DevTreeSnapshot& snapshot,
	uint64_t selectionKey) noexcept {
#if FLOW_UI_DEV_CAPTURE_CLAY
	for (uint32_t nodeIndex = 0; nodeIndex < snapshot.clay.nodes.size(); ++nodeIndex) {
		const tooling::DevClayNode& node = snapshot.clay.nodes[nodeIndex];
		if (stableNodeKey((static_cast<uint64_t>(node.rootIndex) << 32u) | nodeIndex, node.clayId) ==
			selectionKey) {
			return &node;
		}
	}
#else
	(void)snapshot;
	(void)selectionKey;
#endif
	return nullptr;
}

} // namespace

void DevClayInspector::buildElement(BuildContext& context) {
	DevInterfaceState* const interface_state = context.params.interfaceState;
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevClayNode* selected_clay_node = nullptr;

	if (interface_state && context.params.app &&
		context.params.app->hasWindow(interface_state->selectedWindowId)) {
		snapshot = &context.params.app->ui(interface_state->selectedWindowId).devTreeSnapshot();
		if (snapshot && context.params.selectionKey != 0u) {
			selected_clay_node = findClayNodeBySelectionKey(*snapshot, context.params.selectionKey);
		}
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_PERCENT(1.0f),
		.height = CLAY_SIZING_PERCENT(1.0f),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;

	CLAY(context.clayID(), root) {
		// Header Bar
		Clay_ElementDeclaration header{};
		header.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(36),
		};
		header.layout.padding = Clay_Padding{12, 12, 0, 0};
		header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		header.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
		header.border = {
			.color = interface_theme::kBorderPrimary,
			.width = Clay_BorderWidth{0, 0, 0, 1, 0},
		};
		header.backgroundColor = interface_theme::kDepth2Ink;

		CLAY(context.clayID("inspector-header"), header) {
			CLAY_TEXT(
				context.uiManager.toClayString("Clay Element Inspector"),
				CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextCanvas, 12)));
		}

		if (!selected_clay_node || !snapshot) {
			Clay_ElementDeclaration empty_container{};
			empty_container.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			empty_container.layout.padding = Clay_Padding{18, 18, 18, 18};
			empty_container.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};

			CLAY(context.clayID("empty-selection"), empty_container) {
				CLAY_TEXT(
					context.uiManager.toClayString("No Clay element selected"),
					CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextMuted, 12)));
			}
			return;
		}

		// Scrollable Content Area
		Clay_ElementDeclaration content_column{};
		content_column.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		content_column.layout.padding = Clay_Padding{12, 12, 12, 12};
		content_column.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		content_column.layout.childGap = 12;

		CLAY(context.clayID("inspector-content"), content_column) {
			// Card 1: Clay Element Identity
			Clay_ElementDeclaration identity_card{};
			identity_card.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			identity_card.layout.padding = Clay_Padding{10, 10, 10, 10};
			identity_card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			identity_card.layout.childGap = 6;
			identity_card.backgroundColor = interface_theme::kDepth2Ink;
			identity_card.cornerRadius = CLAY_CORNER_RADIUS(4);
			identity_card.border = {
				.color = interface_theme::kBorderPrimary,
				.width = Clay_BorderWidth{1, 1, 1, 1, 0},
			};

			CLAY(context.clayID("identity-card"), identity_card) {
				CLAY_TEXT(
					context.uiManager.toClayString("IDENTITY"),
					CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextMuted, 9)));

				std::string_view identifier_name = snapshot->string(selected_clay_node->idString);
				if (identifier_name.empty()) {
					identifier_name = "(anonymous layout element)";
				}
				CLAY_TEXT(
					context.uiManager.toClayString(identifier_name),
					CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextCanvas, 12)));

				char id_string_buffer[32]{};
				std::snprintf(
					id_string_buffer, sizeof(id_string_buffer), "Clay ID: #%u (0x%08X)",
					selected_clay_node->clayId, selected_clay_node->clayId);
				CLAY_TEXT(
					context.uiManager.toClayString(id_string_buffer),
					CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kAccentSeaGlass, 10)));

				// Active Node Flags
				std::string flags_summary{};
				if (tooling::hasFlag(selected_clay_node->flags, tooling::DevClayNodeFlag::Text)) {
					flags_summary += "[Text] ";
				}
				if (tooling::hasFlag(selected_clay_node->flags, tooling::DevClayNodeFlag::Floating)) {
					flags_summary += "[Floating] ";
				}
				if (tooling::hasFlag(
						selected_clay_node->flags, tooling::DevClayNodeFlag::SyntheticAfterBuild)) {
					flags_summary += "[Synthetic] ";
				}
				if (tooling::hasFlag(
						selected_clay_node->flags, tooling::DevClayNodeFlag::BoundsUnavailable)) {
					flags_summary += "[Bounds Unavailable] ";
				}
				if (!flags_summary.empty()) {
					CLAY_TEXT(
						context.uiManager.toClayString(flags_summary),
						CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kAccentSignalBlue, 9)));
				}
			}

			// Card 2: Owning Flow Element
			Clay_ElementDeclaration owner_card{};
			owner_card.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			owner_card.layout.padding = Clay_Padding{10, 10, 10, 10};
			owner_card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			owner_card.layout.childGap = 6;
			owner_card.backgroundColor = interface_theme::kDepth2Ink;
			owner_card.cornerRadius = CLAY_CORNER_RADIUS(4);
			owner_card.border = {
				.color = interface_theme::kBorderPrimary,
				.width = Clay_BorderWidth{1, 1, 1, 1, 0},
			};

			CLAY(context.clayID("owner-card"), owner_card) {
				CLAY_TEXT(
					context.uiManager.toClayString("OWNING FLOW ELEMENT"),
					CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextMuted, 9)));

				if (selected_clay_node->directFlowOwner != tooling::InvalidFlowNode &&
					selected_clay_node->directFlowOwner < snapshot->flow.nodes.size()) {
					const tooling::DevFlowNode& owning_flow =
						snapshot->flow.nodes[selected_clay_node->directFlowOwner];
					std::string_view flow_name = snapshot->string(owning_flow.debugName);
					if (flow_name.empty()) {
						flow_name = snapshot->string(owning_flow.definitionName);
					}
					if (flow_name.empty()) {
						flow_name = "(unnamed Flow element)";
					}

					CLAY_TEXT(
						context.uiManager.toClayString(flow_name),
						CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kAccentCurrent, 11)));

					std::string_view definition_name = snapshot->string(owning_flow.definitionName);
					if (!definition_name.empty()) {
						CLAY_TEXT(
							context.uiManager.toClayString(definition_name),
							CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextSecondary, 10)));
					}

					char instance_buffer[32]{};
					std::snprintf(
						instance_buffer, sizeof(instance_buffer), "Instance: 0x%016llX",
						static_cast<unsigned long long>(owning_flow.instance.value));
					CLAY_TEXT(
						context.uiManager.toClayString(instance_buffer),
						CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextMuted, 9)));
				} else {
					CLAY_TEXT(
						context.uiManager.toClayString("Unowned / Root Clay Element"),
						CLAY_TEXT_CONFIG(clayTextConfig(interface_theme::kTextSecondary, 11)));
				}
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
