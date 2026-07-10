#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#include "devMode/devFlowElements/common.hpp"
#include "devMode/performanceDiagnostics.hpp"

namespace {

inline std::string devFooterFormatDouble(double value, int precision = 1) {
	char buffer[64]{};
	if (precision <= 0) {
		std::snprintf(buffer, sizeof(buffer), "%.0f", value);
	} else if (precision == 2) {
		std::snprintf(buffer, sizeof(buffer), "%.2f", value);
	} else {
		std::snprintf(buffer, sizeof(buffer), "%.1f", value);
	}
	return std::string(buffer);
}

inline std::string devFooterFormatUInt(uint64_t value) {
	char buffer[64]{};
	std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
	return std::string(buffer);
}

inline std::string devFooterMetricText(std::string_view hint, std::string_view value) {
	std::string text{};
	text.reserve(hint.size() + value.size() + 3u);
	text.append(hint);
	text.append(": ");
	text.append(value);
	return text;
}

inline float devFooterEstimateChipWidth(
	std::string_view text,
	uint16_t fontSize,
	const Clay_Padding& padding) {
	const float glyphWidthHint = std::max(5.0f, static_cast<float>(std::max<uint16_t>(1u, fontSize)) * 0.62f);
	const float textWidth = static_cast<float>(text.size()) * glyphWidthHint;
	return std::ceil(textWidth + static_cast<float>(padding.left + padding.right) + 2.0f);
}

} // namespace

struct devFooterParams {
	int height = 36;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = Clay_Padding{6, 6, 4, 4};
	uint16_t childGap = 6;
	Clay_Color chipBackgroundColor = FlowUi::Flow_Color("#171a20ff");
	Clay_CornerRadius chipCornerRadius = CLAY_CORNER_RADIUS(5);
	Clay_Padding chipPadding = Clay_Padding{7, 7, 3, 3};
	Clay_Color separatorColor = FlowUi::Flow_Color("#5e646eff");
	uint16_t separatorWidth = 1;
	uint16_t separatorHeight = 18;
	uint16_t fontId = 0;
	uint16_t fontSize = 10;
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
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
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(static_cast<float>(height)),
		};
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.childAlignment = Clay_ChildAlignment{
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = context.params.backgroundColor;
		root.clip = {
			.horizontal = true,
			.vertical = false,
			.childOffset = devScrollOffsetForElementId(context.uiManager, context.elementID),
		};

		const FlowUi::devMode::RollingDiagnostics& rolling =
			context.uiManager.performanceDiagnostics().rolling();
		const FlowUi::devMode::FrameDiagnostics& latest = rolling.latest;

		const std::array<std::string, 16> metricTexts = {
			devFooterMetricText("FPS", devFooterFormatDouble(rolling.fps, 0)),
			devFooterMetricText("AvgMs", devFooterFormatDouble(rolling.avgFrameMs)),
			devFooterMetricText("p95Ms", devFooterFormatDouble(rolling.p95FrameMs)),
			devFooterMetricText("MaxMs", devFooterFormatDouble(rolling.maxFrameMs)),
			devFooterMetricText("UiRecordMs", devFooterFormatDouble(rolling.avgUiRecordMs)),
			devFooterMetricText("ViewPortMs", devFooterFormatDouble(rolling.avgViewportRecordMs)),
			devFooterMetricText("FenceMs", devFooterFormatDouble(rolling.avgFenceWaitMs)),
			devFooterMetricText("PresentMs", devFooterFormatDouble(rolling.avgPresentMs)),
			devFooterMetricText("Cmds", devFooterFormatUInt(static_cast<uint64_t>(latest.clayCommandCount))),
			devFooterMetricText("Inst", devFooterFormatUInt(latest.uiInstanceCount)),
			devFooterMetricText("Runs", devFooterFormatUInt(latest.uiRunCount)),
			devFooterMetricText("Glyphs", devFooterFormatUInt(latest.textGlyphCount)),
			devFooterMetricText("Images", devFooterFormatUInt(latest.imageCommandCount)),
			devFooterMetricText("VP", devFooterFormatUInt(latest.referencedViewportCount)),
			devFooterMetricText("VPResize", devFooterFormatUInt(latest.resizedViewportCount)),
			devFooterMetricText("VPpx", devFooterFormatUInt(latest.viewportPixelArea)),
		};

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		textConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		CLAY(rootId, root){
			Clay_ElementDeclaration row{};
			row.layout.sizing = {
				.width = CLAY_SIZING_FIT(0),
				.height = CLAY_SIZING_GROW(0),
			};
			row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			row.layout.childGap = context.params.childGap;
			row.layout.childAlignment = Clay_ChildAlignment{
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_CENTER,
			};

			CLAY(context.uiManager.toClayEID(context.createChildElementId("scroll-row")), row){
				for (std::size_t i = 0u; i < metricTexts.size(); ++i) {
					if (i > 0u) {
						Clay_ElementDeclaration separator{};
						separator.layout.sizing = {
							.width = CLAY_SIZING_FIXED(static_cast<float>(context.params.separatorWidth)),
							.height = CLAY_SIZING_FIXED(static_cast<float>(context.params.separatorHeight)),
						};
						separator.backgroundColor = context.params.separatorColor;
						separator.cornerRadius = CLAY_CORNER_RADIUS(0);

						CLAY(context.uiManager.toClayEID(context.createChildElementId("separator-" + std::to_string(i))), separator){};
					}

					const float chipMinWidth = devFooterEstimateChipWidth(
						metricTexts[i],
						context.params.fontSize,
						context.params.chipPadding);

					Clay_ElementDeclaration chip{};
					chip.layout.sizing = {
						.width = CLAY_SIZING_FIT(chipMinWidth, 100000.0f),
						.height = CLAY_SIZING_FIT(0),
					};
					chip.layout.padding = context.params.chipPadding;
					chip.layout.childAlignment = Clay_ChildAlignment{
						.x = CLAY_ALIGN_X_CENTER,
						.y = CLAY_ALIGN_Y_CENTER,
					};
					chip.backgroundColor = context.params.chipBackgroundColor;
					chip.cornerRadius = context.params.chipCornerRadius;
					chip.border = {
						.color = FlowUi::Flow_Color("#00000000"),
						.width = Clay_BorderWidth{0, 0, 0, 0, 0},
					};

					CLAY(context.uiManager.toClayEID(context.createChildElementId("metric-" + std::to_string(i))), chip){
						CLAY_TEXT(
							context.uiManager.toClayString(metricTexts[i]),
							CLAY_TEXT_CONFIG(textConfig));
					};
				}
			};
		};
	},
};
