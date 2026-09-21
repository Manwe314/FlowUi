#include "devSystems/devInterface/Inspect/Workbench/DevClayCodeGenerator.hpp"

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

Clay_TextElementConfig codeTextStyle(Clay_Color color, uint16_t size) {
	Clay_TextElementConfig text{};
	text.textColor = color;
	text.fontSize = size;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	text.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return text;
}

const tooling::DevClayNode* findClayNodeForCode(
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

std::string formatSizingSnippet(const Clay_SizingAxis& axis) {
	char buffer[64]{};
	switch (axis.type) {
		case CLAY__SIZING_TYPE_FIT:
			if (axis.size.minMax.min > 0.0f || axis.size.minMax.max > 0.0f) {
				std::snprintf(
					buffer, sizeof(buffer), "CLAY_SIZING_FIT(%.1ff, %.1ff)", axis.size.minMax.min,
					axis.size.minMax.max);
			} else {
				std::snprintf(buffer, sizeof(buffer), "CLAY_SIZING_FIT(0)");
			}
			break;
		case CLAY__SIZING_TYPE_GROW:
			if (axis.size.minMax.min > 0.0f || axis.size.minMax.max > 0.0f) {
				std::snprintf(
					buffer, sizeof(buffer), "CLAY_SIZING_GROW(%.1ff, %.1ff)", axis.size.minMax.min,
					axis.size.minMax.max);
			} else {
				std::snprintf(buffer, sizeof(buffer), "CLAY_SIZING_GROW(0)");
			}
			break;
		case CLAY__SIZING_TYPE_PERCENT:
			std::snprintf(buffer, sizeof(buffer), "CLAY_SIZING_PERCENT(%.2ff)", axis.size.percent);
			break;
		case CLAY__SIZING_TYPE_FIXED:
			std::snprintf(buffer, sizeof(buffer), "CLAY_SIZING_FIXED(%.1ff)", axis.size.minMax.min);
			break;
	}
	return buffer;
}

std::string generateClayCppCode(const tooling::DevClayNode& node) {
	std::string code = "Clay_ElementDeclaration element{};\n";
	const auto& decl = node.declaration;

	// Sizing
	const auto& width = decl.layout.sizing.width;
	const auto& height = decl.layout.sizing.height;
	const bool widthCustom = (width.type != CLAY__SIZING_TYPE_FIT || width.size.minMax.min > 0.0f ||
							  width.size.minMax.max > 0.0f);
	const bool heightCustom = (height.type != CLAY__SIZING_TYPE_FIT ||
							   height.size.minMax.min > 0.0f || height.size.minMax.max > 0.0f);

	if (widthCustom || heightCustom) {
		code += "element.layout.sizing = {\n";
		code += "\t.width = " + formatSizingSnippet(width) + ",\n";
		code += "\t.height = " + formatSizingSnippet(height) + ",\n";
		code += "};\n";
	}

	// Direction
	if (decl.layout.layoutDirection == CLAY_TOP_TO_BOTTOM) {
		code += "element.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;\n";
	}

	// Child Alignment
	if (decl.layout.childAlignment.x != CLAY_ALIGN_X_LEFT ||
		decl.layout.childAlignment.y != CLAY_ALIGN_Y_TOP) {
		const char* const xAlign = (decl.layout.childAlignment.x == CLAY_ALIGN_X_CENTER)  ? "CLAY_ALIGN_X_CENTER"
								   : (decl.layout.childAlignment.x == CLAY_ALIGN_X_RIGHT) ? "CLAY_ALIGN_X_RIGHT"
																						  : "CLAY_ALIGN_X_LEFT";
		const char* const yAlign = (decl.layout.childAlignment.y == CLAY_ALIGN_Y_CENTER)   ? "CLAY_ALIGN_Y_CENTER"
								   : (decl.layout.childAlignment.y == CLAY_ALIGN_Y_BOTTOM) ? "CLAY_ALIGN_Y_BOTTOM"
																						   : "CLAY_ALIGN_Y_TOP";
		code += "element.layout.childAlignment = {\n";
		code += std::string("\t.x = ") + xAlign + ",\n";
		code += std::string("\t.y = ") + yAlign + ",\n";
		code += "};\n";
	}

	// Child Gap
	if (decl.layout.childGap > 0) {
		code += "element.layout.childGap = " + std::to_string(decl.layout.childGap) + ";\n";
	}

	// Padding
	const auto& pad = decl.layout.padding;
	if (pad.left > 0 || pad.right > 0 || pad.top > 0 || pad.bottom > 0) {
		char padBuf[128]{};
		std::snprintf(
			padBuf, sizeof(padBuf), "element.layout.padding = Clay_Padding{%u, %u, %u, %u};\n",
			pad.left, pad.right, pad.top, pad.bottom);
		code += padBuf;
	}

	// Background Color
	if (decl.backgroundColor.a > 0) {
		char colorBuf[64]{};
		std::snprintf(
			colorBuf, sizeof(colorBuf), "element.backgroundColor = Clay_Color{%u, %u, %u, %u};\n",
			static_cast<uint32_t>(decl.backgroundColor.r),
			static_cast<uint32_t>(decl.backgroundColor.g),
			static_cast<uint32_t>(decl.backgroundColor.b),
			static_cast<uint32_t>(decl.backgroundColor.a));
		code += colorBuf;
	}

	// Corner Radius
	const auto& cr = decl.cornerRadius;
	if (cr.topLeft > 0.0f || cr.topRight > 0.0f || cr.bottomLeft > 0.0f || cr.bottomRight > 0.0f) {
		if (cr.topLeft == cr.topRight && cr.topLeft == cr.bottomLeft &&
			cr.topLeft == cr.bottomRight) {
			code += "element.cornerRadius = CLAY_CORNER_RADIUS(" +
					std::to_string(static_cast<uint16_t>(cr.topLeft)) + ");\n";
		} else {
			char radBuf[80]{};
			std::snprintf(
				radBuf, sizeof(radBuf),
				"element.cornerRadius = Clay_CornerRadius{%.1ff, %.1ff, %.1ff, %.1ff};\n",
				cr.topLeft, cr.topRight, cr.bottomLeft, cr.bottomRight);
			code += radBuf;
		}
	}

	// Borders
	const auto& b = decl.border;
	if (b.width.left > 0 || b.width.right > 0 || b.width.top > 0 || b.width.bottom > 0 ||
		b.width.betweenChildren > 0) {
		code += "element.border = {\n";
		char bColorBuf[64]{};
		std::snprintf(
			bColorBuf, sizeof(bColorBuf), "\t.color = Clay_Color{%u, %u, %u, %u},\n",
			static_cast<uint32_t>(b.color.r), static_cast<uint32_t>(b.color.g),
			static_cast<uint32_t>(b.color.b), static_cast<uint32_t>(b.color.a));
		code += bColorBuf;
		char bWidthBuf[80]{};
		std::snprintf(
			bWidthBuf, sizeof(bWidthBuf),
			"\t.width = Clay_BorderWidth{%u, %u, %u, %u, %u},\n", b.width.left,
			b.width.right, b.width.top, b.width.bottom, b.width.betweenChildren);
		code += bWidthBuf;
		code += "};\n";
	}

	// Clip
	if (decl.clip.horizontal || decl.clip.vertical) {
		code += "element.clip = {\n";
		code += std::string("\t.horizontal = ") + (decl.clip.horizontal ? "true" : "false") + ",\n";
		code += std::string("\t.vertical = ") + (decl.clip.vertical ? "true" : "false") + ",\n";
		code += "};\n";
	}

	// Text Configuration
	if (tooling::hasFlag(node.flags, tooling::DevClayNodeFlag::Text)) {
		code += "\nClay_TextElementConfig textConfig{};\n";
		char txtColor[64]{};
		std::snprintf(
			txtColor, sizeof(txtColor), "textConfig.textColor = Clay_Color{%u, %u, %u, %u};\n",
			static_cast<uint32_t>(node.textConfig.textColor.r),
			static_cast<uint32_t>(node.textConfig.textColor.g),
			static_cast<uint32_t>(node.textConfig.textColor.b),
			static_cast<uint32_t>(node.textConfig.textColor.a));
		code += txtColor;
		code += "textConfig.fontSize = " + std::to_string(node.textConfig.fontSize) + ";\n";
		if (node.textConfig.wrapMode == CLAY_TEXT_WRAP_NONE) {
			code += "textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;\n";
		} else if (node.textConfig.wrapMode == CLAY_TEXT_WRAP_NEWLINES) {
			code += "textConfig.wrapMode = CLAY_TEXT_WRAP_NEWLINES;\n";
		}
	}

	return code;
}

} // namespace

void DevClayCodeGenerator::onPressed(InteractionContext& context) {
	if (!context.previousInteraction.isPressed(context.clayID("copy-btn"))) return;

	DevInterfaceState* const interfaceState = context.params.inspect.interfaceState;
	if (!interfaceState || !context.params.inspect.app ||
		!context.params.inspect.app->hasWindow(interfaceState->selectedWindowId)) {
		return;
	}

	const tooling::DevTreeSnapshot& snapshot =
		context.params.inspect.app->ui(interfaceState->selectedWindowId).devTreeSnapshot();
	const tooling::DevClayNode* const node = findClayNodeForCode(snapshot, context.params.selectionKey);
	if (!node) return;

	const std::string code = generateClayCppCode(*node);
	context.uiManager.setClipboardText(code);
	context.state().copiedRecently = true;
}

void DevClayCodeGenerator::buildElement(BuildContext& context) {
	DevInterfaceState* const interfaceState = context.params.inspect.interfaceState;
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevClayNode* selectedClayNode = nullptr;

	if (interfaceState && context.params.inspect.app &&
		context.params.inspect.app->hasWindow(interfaceState->selectedWindowId)) {
		snapshot = &context.params.inspect.app->ui(interfaceState->selectedWindowId).devTreeSnapshot();
		if (snapshot && context.params.selectionKey != 0u) {
			selectedClayNode = findClayNodeForCode(*snapshot, context.params.selectionKey);
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
		// Header Bar: "Element Code" on Left, "Copy" Button on Right
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

		CLAY(context.clayID("code-gen-header"), header) {
			CLAY_TEXT(
				context.uiManager.toClayString("Element Code"),
				CLAY_TEXT_CONFIG(codeTextStyle(interface_theme::kTextCanvas, 10)));

			// Spacer
			Clay_ElementDeclaration spacer{};
			spacer.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			CLAY(context.clayID("spacer"), spacer) {}

			// Copy Button
			const bool isHovered = context.uiManager.getPreviousFramesInteraction().isHovered(
				context.clayID("copy-btn"));
			const bool copiedRecently = context.state().copiedRecently;

			Clay_ElementDeclaration copyBtn{};
			copyBtn.layout.sizing = {
				.width = CLAY_SIZING_FIT(0),
				.height = CLAY_SIZING_FIXED(22),
			};
			copyBtn.layout.padding = Clay_Padding{8, 8, 0, 0};
			copyBtn.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			copyBtn.backgroundColor = copiedRecently ? interface_theme::kDepth3Elevated
									  : isHovered	 ? interface_theme::kHoverSurface
													 : interface_theme::kDepth3Elevated;
			copyBtn.cornerRadius = CLAY_CORNER_RADIUS(3);
			copyBtn.border = {
				.color = copiedRecently ? interface_theme::kStatusGreen
										: interface_theme::kBorderVisible,
				.width = Clay_BorderWidth{1, 1, 1, 1, 0},
			};

			CLAY(context.clayID("copy-btn"), copyBtn) {
				const Clay_Color btnTextColor = copiedRecently ? interface_theme::kStatusGreen
																: interface_theme::kTextCanvas;
				CLAY_TEXT(
					context.uiManager.toClayString(copiedRecently ? "Copied!" : "Copy"),
					CLAY_TEXT_CONFIG(codeTextStyle(btnTextColor, 9)));
			}
		}

		if (!selectedClayNode) {
			Clay_ElementDeclaration empty{};
			empty.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			empty.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("empty-code"), empty) {
				CLAY_TEXT(
					context.uiManager.toClayString("Select a Clay element to view generated code"),
					CLAY_TEXT_CONFIG(codeTextStyle(interface_theme::kTextMuted, 11)));
			}
			return;
		}

		// Scrollable Code Display
		const Clay_ElementId scrollId = context.clayID("code-scroll");
		const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(scrollId);

		Clay_ElementDeclaration scrollRegion{};
		scrollRegion.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		scrollRegion.clip = {
			.horizontal = true,
			.vertical = true,
			.childOffset = scroll.found && scroll.scrollPosition
							   ? *scroll.scrollPosition
							   : Clay_Vector2{},
		};
		scrollRegion.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		scrollRegion.backgroundColor = interface_theme::kDepth0Keel;

		CLAY(scrollId, scrollRegion) {
			Clay_ElementDeclaration codeBody{};
			codeBody.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			codeBody.layout.padding = Clay_Padding{12, 12, 10, 10};
			codeBody.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;

			CLAY(context.clayID("code-body"), codeBody) {
				const std::string generatedCode = generateClayCppCode(*selectedClayNode);
				CLAY_TEXT(
					context.uiManager.toClayString(generatedCode),
					CLAY_TEXT_CONFIG(codeTextStyle(interface_theme::kAccentSeaGlass, 9)));
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
