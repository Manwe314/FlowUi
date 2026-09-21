#include "devSystems/devInterface/Inspect/Workbench/DevClayDataDump.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

Clay_TextElementConfig textStyle(Clay_Color color, uint16_t size, bool wrap = false) {
	Clay_TextElementConfig text{};
	text.textColor = color;
	text.fontSize = size;
	text.wrapMode = wrap ? CLAY_TEXT_WRAP_WORDS : CLAY_TEXT_WRAP_NONE;
	text.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return text;
}

const tooling::DevClayNode* findClayNode(
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

std::string formatSizingType(Clay__SizingType type) {
	switch (type) {
		case CLAY__SIZING_TYPE_FIT: return "Fit";
		case CLAY__SIZING_TYPE_GROW: return "Grow";
		case CLAY__SIZING_TYPE_PERCENT: return "Percent";
		case CLAY__SIZING_TYPE_FIXED: return "Fixed";
	}
	return "Unknown";
}

std::string formatSizingAxis(const Clay_SizingAxis& axis) {
	char buffer[64]{};
	switch (axis.type) {
		case CLAY__SIZING_TYPE_FIT:
			if (axis.size.minMax.min > 0.0f || axis.size.minMax.max > 0.0f) {
				std::snprintf(
					buffer, sizeof(buffer), "Fit (min: %.1f, max: %.1f)", axis.size.minMax.min,
					axis.size.minMax.max);
			} else {
				std::snprintf(buffer, sizeof(buffer), "Fit");
			}
			break;
		case CLAY__SIZING_TYPE_GROW:
			if (axis.size.minMax.min > 0.0f || axis.size.minMax.max > 0.0f) {
				std::snprintf(
					buffer, sizeof(buffer), "Grow (min: %.1f, max: %.1f)", axis.size.minMax.min,
					axis.size.minMax.max);
			} else {
				std::snprintf(buffer, sizeof(buffer), "Grow");
			}
			break;
		case CLAY__SIZING_TYPE_PERCENT:
			std::snprintf(buffer, sizeof(buffer), "Percent (%.1f%%)", axis.size.percent * 100.0f);
			break;
		case CLAY__SIZING_TYPE_FIXED:
			std::snprintf(buffer, sizeof(buffer), "Fixed (%.1f px)", axis.size.minMax.min);
			break;
	}
	return buffer;
}

std::string formatColor(Clay_Color color) {
	char buffer[48]{};
	std::snprintf(
		buffer, sizeof(buffer), "RGBA(%u, %u, %u, %u) #%02X%02X%02X%02X",
		static_cast<uint32_t>(color.r), static_cast<uint32_t>(color.g),
		static_cast<uint32_t>(color.b), static_cast<uint32_t>(color.a),
		static_cast<uint32_t>(color.r), static_cast<uint32_t>(color.g),
		static_cast<uint32_t>(color.b), static_cast<uint32_t>(color.a));
	return buffer;
}

std::string formatWrapMode(Clay_TextElementConfigWrapMode mode) {
	switch (mode) {
		case CLAY_TEXT_WRAP_WORDS: return "Words";
		case CLAY_TEXT_WRAP_NEWLINES: return "Newlines";
		case CLAY_TEXT_WRAP_NONE: return "None";
	}
	return "Unknown";
}

std::string formatAlignment(Clay_LayoutAlignmentX alignX, Clay_LayoutAlignmentY alignY) {
	const char* const xLabel = (alignX == CLAY_ALIGN_X_LEFT)	 ? "Left"
							   : (alignX == CLAY_ALIGN_X_CENTER) ? "Center"
																 : "Right";
	const char* const yLabel = (alignY == CLAY_ALIGN_Y_TOP)		 ? "Top"
							   : (alignY == CLAY_ALIGN_Y_CENTER) ? "Center"
																 : "Bottom";
	char buffer[48]{};
	std::snprintf(buffer, sizeof(buffer), "X: %s, Y: %s", xLabel, yLabel);
	return buffer;
}

void drawSectionHeader(
	DevClayDataDump::BuildContext& context,
	std::string_view sectionTitle,
	uint32_t index) {
	const IndexedElementName headerName = Indexed(LocalElementName{"sec-hdr"}, index);

	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(22),
	};
	header.layout.padding = Clay_Padding{6, 6, 0, 0};
	header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	header.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	header.backgroundColor = interface_theme::kDepth2Ink;
	header.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 1, 0, 0},
	};

	CLAY(context.clayID(headerName), header) {
		CLAY_TEXT(
			context.uiManager.toClayString(sectionTitle),
			CLAY_TEXT_CONFIG(textStyle(interface_theme::kAccentCurrent, 9)));
	}
}

void drawPropertyRow(
	DevClayDataDump::BuildContext& context,
	std::string_view propertyName,
	std::string_view propertyValue,
	bool isDefault,
	uint32_t rowIndex) {
	const IndexedElementName rowName = Indexed(LocalElementName{"prop-row"}, rowIndex);
	const IndexedElementName nameBox = Indexed(LocalElementName{"prop-name"}, rowIndex);
	const IndexedElementName valBox = Indexed(LocalElementName{"prop-val"}, rowIndex);
	const IndexedElementName badgeBox = Indexed(LocalElementName{"prop-badge"}, rowIndex);

	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	row.layout.padding = Clay_Padding{8, 8, 4, 4};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childGap = 8;
	row.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	row.backgroundColor = (rowIndex % 2 == 0) ? interface_theme::kDepth1Panel
											  : interface_theme::kDepth0Keel;
	row.border = {
		.color = interface_theme::kBorderSubtle,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};

	CLAY(context.clayID(rowName), row) {
		// Property Name
		Clay_ElementDeclaration nameContainer{};
		nameContainer.layout.sizing = {
			.width = CLAY_SIZING_FIXED(160),
			.height = CLAY_SIZING_FIT(0),
		};
		CLAY(context.clayID(nameBox), nameContainer) {
			CLAY_TEXT(
				context.uiManager.toClayString(propertyName),
				CLAY_TEXT_CONFIG(textStyle(interface_theme::kTextSecondary, 9)));
		}

		// Property Value
		Clay_ElementDeclaration valueContainer{};
		valueContainer.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		CLAY(context.clayID(valBox), valueContainer) {
			const Clay_Color valueColor = isDefault ? interface_theme::kTextMuted
													: interface_theme::kTextCanvas;
			CLAY_TEXT(
				context.uiManager.toClayString(propertyValue),
				CLAY_TEXT_CONFIG(textStyle(valueColor, 9, true)));
		}

		// Status Badge ([SET] or [DEFAULT])
		Clay_ElementDeclaration badgeContainer{};
		badgeContainer.layout.sizing = {
			.width = CLAY_SIZING_FIT(0),
			.height = CLAY_SIZING_FIXED(16),
		};
		badgeContainer.layout.padding = Clay_Padding{4, 4, 0, 0};
		badgeContainer.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
		badgeContainer.backgroundColor = isDefault ? interface_theme::kDepth2Ink
												   : interface_theme::kDepth3Elevated;
		badgeContainer.cornerRadius = CLAY_CORNER_RADIUS(2);
		badgeContainer.border = {
			.color = isDefault ? interface_theme::kBorderPrimary
							   : interface_theme::kAccentCurrent,
			.width = Clay_BorderWidth{1, 1, 1, 1, 0},
		};

		CLAY(context.clayID(badgeBox), badgeContainer) {
			const Clay_Color badgeColor = isDefault ? interface_theme::kTextMuted
													: interface_theme::kAccentCurrent;
			CLAY_TEXT(
				context.uiManager.toClayString(isDefault ? "DEFAULT" : "SET"),
				CLAY_TEXT_CONFIG(textStyle(badgeColor, 8)));
		}
	}
}

} // namespace

void DevClayDataDump::buildElement(BuildContext& context) {
	DevInterfaceState* const interfaceState = context.params.inspect.interfaceState;
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevClayNode* selectedClayNode = nullptr;

	if (interfaceState && context.params.inspect.app &&
		context.params.inspect.app->hasWindow(interfaceState->selectedWindowId)) {
		snapshot = &context.params.inspect.app->ui(interfaceState->selectedWindowId).devTreeSnapshot();
		if (snapshot && context.params.selectionKey != 0u) {
			selectedClayNode = findClayNode(*snapshot, context.params.selectionKey);
		}
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.cornerRadius = CLAY_CORNER_RADIUS(4);
	root.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	root.clip = {.horizontal = true, .vertical = true};

	CLAY(context.clayID(), root) {
		// Header Bar
		Clay_ElementDeclaration header{};
		header.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(32),
		};
		header.layout.padding = Clay_Padding{10, 10, 0, 0};
		header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		header.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
		header.backgroundColor = interface_theme::kDepth2Ink;
		header.border = {
			.color = interface_theme::kBorderPrimary,
			.width = Clay_BorderWidth{0, 0, 0, 1, 0},
		};

		CLAY(context.clayID("dump-header"), header) {
			CLAY_TEXT(
				context.uiManager.toClayString("Clay_ElementDeclaration Struct Dump"),
				CLAY_TEXT_CONFIG(textStyle(interface_theme::kTextCanvas, 10)));
		}

		if (!selectedClayNode || !snapshot) {
			Clay_ElementDeclaration empty{};
			empty.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			empty.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("empty"), empty) {
				CLAY_TEXT(
					context.uiManager.toClayString("No Clay element selected"),
					CLAY_TEXT_CONFIG(textStyle(interface_theme::kTextMuted, 11)));
			}
			return;
		}

		// Scrollable Body
		const Clay_ElementId scrollId = context.clayID("data-dump-scroll");
		const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(scrollId);

		Clay_ElementDeclaration scrollRegion{};
		scrollRegion.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		scrollRegion.clip = {
			.horizontal = false,
			.vertical = true,
			.childOffset = scroll.found && scroll.scrollPosition
							   ? *scroll.scrollPosition
							   : Clay_Vector2{},
		};
		scrollRegion.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;

		CLAY(scrollId, scrollRegion) {
			Clay_ElementDeclaration contentStack{};
			contentStack.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			contentStack.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;

			CLAY(context.clayID("dump-content-stack"), contentStack) {
				const auto& decl = selectedClayNode->declaration;
				uint32_t sectionCounter = 0;
				uint32_t rowCounter = 0;

				// 1. Sizing
				drawSectionHeader(context, "LAYOUT SIZING", ++sectionCounter);
				drawPropertyRow(
					context, "layout.sizing.width", formatSizingAxis(decl.layout.sizing.width),
					decl.layout.sizing.width.type == CLAY__SIZING_TYPE_FIT &&
						decl.layout.sizing.width.size.minMax.min == 0.0f &&
						decl.layout.sizing.width.size.minMax.max == 0.0f,
					++rowCounter);
				drawPropertyRow(
					context, "layout.sizing.height", formatSizingAxis(decl.layout.sizing.height),
					decl.layout.sizing.height.type == CLAY__SIZING_TYPE_FIT &&
						decl.layout.sizing.height.size.minMax.min == 0.0f &&
						decl.layout.sizing.height.size.minMax.max == 0.0f,
					++rowCounter);

				// 2. Alignment & Flow
				drawSectionHeader(context, "LAYOUT DIRECTION & ALIGNMENT", ++sectionCounter);
				drawPropertyRow(
					context, "layout.layoutDirection",
					decl.layout.layoutDirection == CLAY_TOP_TO_BOTTOM ? "TopToBottom" : "LeftToRight",
					decl.layout.layoutDirection == CLAY_LEFT_TO_RIGHT, ++rowCounter);
				drawPropertyRow(
					context, "layout.childAlignment",
					formatAlignment(decl.layout.childAlignment.x, decl.layout.childAlignment.y),
					decl.layout.childAlignment.x == CLAY_ALIGN_X_LEFT &&
						decl.layout.childAlignment.y == CLAY_ALIGN_Y_TOP,
					++rowCounter);
				drawPropertyRow(
					context, "layout.childGap", std::to_string(decl.layout.childGap) + " px",
					decl.layout.childGap == 0, ++rowCounter);

				// 3. Padding
				drawSectionHeader(context, "PADDING", ++sectionCounter);
				char paddingBuffer[64]{};
				std::snprintf(
					paddingBuffer, sizeof(paddingBuffer), "T: %u, R: %u, B: %u, L: %u",
					decl.layout.padding.top, decl.layout.padding.right,
					decl.layout.padding.bottom, decl.layout.padding.left);
				const bool paddingIsDefault = (decl.layout.padding.top == 0 &&
											   decl.layout.padding.right == 0 &&
											   decl.layout.padding.bottom == 0 &&
											   decl.layout.padding.left == 0);
				drawPropertyRow(
					context, "layout.padding", paddingBuffer, paddingIsDefault, ++rowCounter);

				// 4. Background & Colors
				drawSectionHeader(context, "COLORS & BACKGROUND", ++sectionCounter);
				drawPropertyRow(
					context, "backgroundColor", formatColor(decl.backgroundColor),
					decl.backgroundColor.a == 0, ++rowCounter);
				drawPropertyRow(
					context, "overlayColor", formatColor(decl.overlayColor),
					decl.overlayColor.a == 0, ++rowCounter);

				// 5. Corner Radius
				drawSectionHeader(context, "CORNER RADIUS", ++sectionCounter);
				char radiusBuffer[64]{};
				std::snprintf(
					radiusBuffer, sizeof(radiusBuffer), "TL: %.1f, TR: %.1f, BL: %.1f, BR: %.1f",
					decl.cornerRadius.topLeft, decl.cornerRadius.topRight,
					decl.cornerRadius.bottomLeft, decl.cornerRadius.bottomRight);
				const bool radiusIsDefault = (decl.cornerRadius.topLeft == 0.0f &&
											  decl.cornerRadius.topRight == 0.0f &&
											  decl.cornerRadius.bottomLeft == 0.0f &&
											  decl.cornerRadius.bottomRight == 0.0f);
				drawPropertyRow(
					context, "cornerRadius", radiusBuffer, radiusIsDefault, ++rowCounter);

				// 6. Borders
				drawSectionHeader(context, "BORDER CONFIGURATION", ++sectionCounter);
				char borderBuffer[80]{};
				std::snprintf(
					borderBuffer, sizeof(borderBuffer), "L: %u, R: %u, T: %u, B: %u, Between: %u",
					decl.border.width.left, decl.border.width.right,
					decl.border.width.top, decl.border.width.bottom,
					decl.border.width.betweenChildren);
				const bool borderIsDefault = (decl.border.width.left == 0 &&
											  decl.border.width.right == 0 &&
											  decl.border.width.top == 0 &&
											  decl.border.width.bottom == 0 &&
											  decl.border.width.betweenChildren == 0);
				drawPropertyRow(
					context, "border.width", borderBuffer, borderIsDefault, ++rowCounter);
				drawPropertyRow(
					context, "border.color", formatColor(decl.border.color),
					decl.border.color.a == 0, ++rowCounter);

				// 7. Clipping & Floating
				drawSectionHeader(context, "CLIPPING & FLOATING", ++sectionCounter);
				char clipBuffer[48]{};
				std::snprintf(
					clipBuffer, sizeof(clipBuffer), "H: %s, V: %s",
					decl.clip.horizontal ? "true" : "false",
					decl.clip.vertical ? "true" : "false");
				drawPropertyRow(
					context, "clip", clipBuffer,
					!decl.clip.horizontal && !decl.clip.vertical, ++rowCounter);

				char floatBuffer[64]{};
				std::snprintf(
					floatBuffer, sizeof(floatBuffer), "attachTo: %u, zIndex: %d",
					static_cast<uint32_t>(decl.floating.attachTo),
					static_cast<int>(decl.floating.zIndex));
				drawPropertyRow(
					context, "floating", floatBuffer,
					decl.floating.attachTo == CLAY_ATTACH_TO_NONE, ++rowCounter);

				// 8. Text Configuration (if Text node)
				if (tooling::hasFlag(selectedClayNode->flags, tooling::DevClayNodeFlag::Text)) {
					drawSectionHeader(context, "TEXT CONFIGURATION", ++sectionCounter);
					drawPropertyRow(
						context, "textConfig.fontSize",
						std::to_string(selectedClayNode->textConfig.fontSize), false,
						++rowCounter);
					drawPropertyRow(
						context, "textConfig.textColor",
						formatColor(selectedClayNode->textConfig.textColor), false,
						++rowCounter);
					drawPropertyRow(
						context, "textConfig.lineHeight",
						std::to_string(selectedClayNode->textConfig.lineHeight),
						selectedClayNode->textConfig.lineHeight == 0, ++rowCounter);
					drawPropertyRow(
						context, "textConfig.letterSpacing",
						std::to_string(selectedClayNode->textConfig.letterSpacing),
						selectedClayNode->textConfig.letterSpacing == 0, ++rowCounter);
					drawPropertyRow(
						context, "textConfig.wrapMode",
						formatWrapMode(selectedClayNode->textConfig.wrapMode),
						selectedClayNode->textConfig.wrapMode == CLAY_TEXT_WRAP_WORDS,
						++rowCounter);

					std::string_view textContent = snapshot->string(selectedClayNode->text);
					drawPropertyRow(
						context, "text.content",
						textContent.empty() ? "(empty)" : textContent,
						textContent.empty(), ++rowCounter);
				}

				// 9. Computed Metrics
				drawSectionHeader(context, "COMPUTED LAYOUT METRICS", ++sectionCounter);
				char boundsBuffer[64]{};
				std::snprintf(
					boundsBuffer, sizeof(boundsBuffer), "X: %.1f, Y: %.1f, W: %.1f, H: %.1f",
					selectedClayNode->bounds.x, selectedClayNode->bounds.y,
					selectedClayNode->bounds.width, selectedClayNode->bounds.height);
				drawPropertyRow(context, "computed.bounds", boundsBuffer, false, ++rowCounter);

				char dimsBuffer[48]{};
				std::snprintf(
					dimsBuffer, sizeof(dimsBuffer), "%.1f x %.1f px",
					selectedClayNode->dimensions.width, selectedClayNode->dimensions.height);
				drawPropertyRow(context, "computed.dimensions", dimsBuffer, false, ++rowCounter);

				if (selectedClayNode->clipClayId != 0) {
					drawPropertyRow(
						context, "computed.clipClayId",
						"#" + std::to_string(selectedClayNode->clipClayId), false, ++rowCounter);
				}
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
