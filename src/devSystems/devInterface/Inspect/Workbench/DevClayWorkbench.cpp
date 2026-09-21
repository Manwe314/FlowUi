#include "devSystems/devInterface/Inspect/Workbench/DevClayWorkbench.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdio>
#include <string>
#include <string_view>

#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevClayCodeGenerator.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevClayDataDump.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr uint16_t kWorkbenchPadding = 14;
inline constexpr uint16_t kWorkbenchGap = 12;

Clay_TextElementConfig headerTextStyle(Clay_Color color, uint16_t size) {
	Clay_TextElementConfig text{};
	text.textColor = color;
	text.fontSize = size;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	text.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return text;
}

const tooling::DevClayNode* findClayNodeForWorkbench(
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

void DevClayWorkbench::buildElement(BuildContext& context) {
	DevInterfaceState* const state = context.params.inspect.interfaceState;
	App* const app = context.params.inspect.app;
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevClayNode* selectedClayNode = nullptr;

	if (state && app && app->hasWindow(state->selectedWindowId)) {
		snapshot = &app->ui(state->selectedWindowId).devTreeSnapshot();
		if (snapshot && context.params.selectionKey != 0u) {
			selectedClayNode = findClayNodeForWorkbench(*snapshot, context.params.selectionKey);
		}
	}

	std::string nodeTitle = "Clay Element";
	std::string nodeSubtitle = "Select an element";
	if (selectedClayNode && snapshot) {
		std::string_view name = snapshot->string(selectedClayNode->idString);
		if (!name.empty()) {
			nodeTitle = std::string(name);
		} else {
			char idFallback[32]{};
			std::snprintf(idFallback, sizeof(idFallback), "Clay #%u", selectedClayNode->clayId);
			nodeTitle = idFallback;
		}

		char subBuffer[96]{};
		std::snprintf(
			subBuffer, sizeof(subBuffer), "Clay ID: #%u  |  Bounds: (%.1f, %.1f) %.1f x %.1f px",
			selectedClayNode->clayId, selectedClayNode->bounds.x, selectedClayNode->bounds.y,
			selectedClayNode->bounds.width, selectedClayNode->bounds.height);
		nodeSubtitle = subBuffer;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_PERCENT(1.0f),
		.height = CLAY_SIZING_PERCENT(1.0f),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth0Keel;

	CLAY(context.clayID(), root) {
		// Header Bar
		Clay_ElementDeclaration headerBar{};
		headerBar.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(48),
		};
		headerBar.layout.padding = Clay_Padding{kWorkbenchPadding, kWorkbenchPadding, 6, 6};
		headerBar.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		headerBar.layout.childGap = 2;
		headerBar.backgroundColor = interface_theme::kDepth1Panel;
		headerBar.border = {
			.color = interface_theme::kBorderPrimary,
			.width = Clay_BorderWidth{0, 0, 0, 1, 0},
		};

		CLAY(context.clayID("clay-wb-header"), headerBar) {
			CLAY_TEXT(
				context.uiManager.toClayString(nodeTitle),
				CLAY_TEXT_CONFIG(headerTextStyle(interface_theme::kTextCanvas, 13)));
			CLAY_TEXT(
				context.uiManager.toClayString(nodeSubtitle),
				CLAY_TEXT_CONFIG(headerTextStyle(interface_theme::kTextSecondary, 9)));
		}

		// Workbench 2-Row Body
		Clay_ElementDeclaration body{};
		body.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		body.layout.padding = Clay_Padding{
			kWorkbenchPadding, kWorkbenchPadding, kWorkbenchPadding, kWorkbenchPadding};
		body.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		body.layout.childGap = kWorkbenchGap;
		body.backgroundColor = interface_theme::kDepth0Keel;

		CLAY(context.clayID("clay-wb-body"), body) {
			// Row 1 (Top Row): Left = ViewPort Preview, Right = Full Struct Dump
			Clay_ElementDeclaration topRow{};
			topRow.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_PERCENT(0.5f),
			};
			topRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			topRow.layout.childGap = kWorkbenchGap;

			CLAY(context.clayID("top-row"), topRow) {
				// Top-Left: Reused ViewPort Element in a card container
				Clay_ElementDeclaration previewCard{};
				previewCard.layout.sizing = {
					.width = CLAY_SIZING_PERCENT(0.5f),
					.height = CLAY_SIZING_PERCENT(1.0f),
				};
				previewCard.backgroundColor = interface_theme::kDepth1Panel;
				previewCard.cornerRadius = CLAY_CORNER_RADIUS(4);
				previewCard.border = {
					.color = interface_theme::kBorderPrimary,
					.width = Clay_BorderWidth{1, 1, 1, 1, 0},
				};
				previewCard.clip = {.horizontal = true, .vertical = true};

				CLAY(context.clayID("preview-card"), previewCard) {
					context.uiManager.createElement(kDevPreview, "clay-preview")
						.setParameters(context.params.inspect)
						.setDevInternalCapture(true)
						.draw();
				}

				// Top-Right: Full Scrollable Struct Dump
				Clay_ElementDeclaration dumpContainer{};
				dumpContainer.layout.sizing = {
					.width = CLAY_SIZING_PERCENT(0.5f),
					.height = CLAY_SIZING_PERCENT(1.0f),
				};
				CLAY(context.clayID("dump-card-wrapper"), dumpContainer) {
					context.uiManager.createElement(kDevClayDataDump, "clay-data-dump")
						.setParameters(DevClayDataDumpParameters{
							.inspect = context.params.inspect,
							.selectionKey = context.params.selectionKey,
						})
						.setDevInternalCapture(true)
						.draw();
				}
			}

			// Row 2 (Bottom Row): Single Spanning Element (Generated C++ Code with Copy Button)
			Clay_ElementDeclaration bottomRow{};
			bottomRow.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_PERCENT(0.5f),
			};
			bottomRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;

			CLAY(context.clayID("bottom-row"), bottomRow) {
				context.uiManager.createElement(kDevClayCodeGenerator, "clay-code-gen")
					.setParameters(DevClayCodeGeneratorParameters{
						.inspect = context.params.inspect,
						.selectionKey = context.params.selectionKey,
					})
					.setDevInternalCapture(true)
					.draw();
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
