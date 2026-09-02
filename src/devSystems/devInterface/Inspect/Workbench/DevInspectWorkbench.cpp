#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cstdio>
#include <string>

#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr LocalElementName kHeader{"header"};
inline constexpr LocalElementName kSubtitle{"subtitle"};
inline constexpr LocalElementName kContent{"content"};
inline constexpr LocalElementName kHeaderName{"instance-name"};
inline constexpr LocalElementName kHeaderSpacer{"spacer"};
inline constexpr LocalElementName kConstructedStamp{"constructed"};
inline constexpr LocalElementName kDrawnStamp{"drawn"};
inline constexpr LocalElementName kClayCount{"clay-count"};
inline constexpr LocalElementName kOverrideCount{"override-count"};

Clay_TextElementConfig textConfig(Clay_Color color, uint16_t size) {
	Clay_TextElementConfig text{};
	text.textColor = color;
	text.fontSize = size;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	text.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return text;
}

std::string_view localInstanceName(std::string_view name) {
	const std::size_t separator = name.find_last_of('/');
	return separator == std::string_view::npos ? name : name.substr(separator + 1u);
}

std::string readableInstancePath(std::string_view name) {
	std::string path{};
	path.reserve(name.size() + 8u);
	for (const char character : name) {
		if (character == '/') path += " > ";
		else path.push_back(character);
	}
	return path;
}

void drawStateStamp(
	DevInspectWorkbenchHeader::BuildContext& context,
	LocalElementName id,
	std::string_view label,
	bool active,
	Clay_Color activeColor) {
	Clay_ElementDeclaration stamp{};
	stamp.layout.sizing = {
		.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(22)};
	stamp.layout.padding = Clay_Padding{6, 6, 0, 0};
	stamp.layout.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	stamp.backgroundColor = active
		? interface_theme::kDepth3Elevated : interface_theme::kDepth2Ink;
	stamp.cornerRadius = CLAY_CORNER_RADIUS(3);
	stamp.border = {
		.color = active ? activeColor : interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	CLAY(context.clayID(id), stamp) {
		CLAY_TEXT(context.uiManager.toClayString(label),
			CLAY_TEXT_CONFIG(textConfig(
				active ? activeColor : interface_theme::kTextMuted, 8)));
	}
}

void drawTelemetry(
	DevInspectWorkbenchHeader::BuildContext& context,
	LocalElementName id,
	std::string_view label,
	uint32_t value,
	Clay_Color valueColor) {
	Clay_ElementDeclaration telemetry{};
	telemetry.layout.sizing = {
		.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(22)};
	telemetry.layout.padding = Clay_Padding{6, 6, 0, 0};
	telemetry.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	telemetry.layout.childGap = 4;
	telemetry.layout.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	telemetry.backgroundColor = interface_theme::kDepth2Ink;
	telemetry.cornerRadius = CLAY_CORNER_RADIUS(3);
	telemetry.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	CLAY(context.clayID(id), telemetry) {
		CLAY_TEXT(context.uiManager.toClayString(label),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
		const std::string count = std::to_string(value);
		CLAY_TEXT(context.uiManager.toClayString(count),
			CLAY_TEXT_CONFIG(textConfig(valueColor, 9)));
	}
}

} // namespace

void DevInspectWorkbenchHeader::buildElement(BuildContext& context) {
	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(44)};
	header.layout.padding = Clay_Padding{14, 14, 0, 0};
	header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	header.layout.childGap = 7;
	header.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	header.backgroundColor = interface_theme::kDepth1Panel;
	header.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};
	header.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), header) {
		Clay_ElementDeclaration nameArea{};
		nameArea.layout.sizing = {
			.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)};
		nameArea.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		nameArea.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(kHeaderName), nameArea) {
			CLAY_TEXT(context.uiManager.toClayString(
				context.params.instanceName.empty()
					? std::string_view{"No element selected"}
					: context.params.instanceName),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 14)));
		}
		CLAY(context.clayID(kHeaderSpacer), {
			.layout = {.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}}) {}
		drawStateStamp(context, kConstructedStamp, "CONSTRUCTED",
			context.params.constructed, interface_theme::kStatusGreen);
		drawStateStamp(context, kDrawnStamp, "DRAWN",
			context.params.drawn, interface_theme::kAccentCurrent);
		drawTelemetry(context, kClayCount, "CLAY", context.params.directClayNodeCount,
			interface_theme::kTextSecondary);
		drawTelemetry(context, kOverrideCount, "OVERRIDES", context.params.overrideCount,
			context.params.overrideCount == 0u
				? interface_theme::kTextMuted : interface_theme::kAccentSignalCoral);
	}
}

void DevInspectWorkbenchSubtitle::buildElement(BuildContext& context) {
	Clay_ElementDeclaration subtitle{};
	subtitle.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	subtitle.layout.padding = Clay_Padding{14, 14, 7, 7};
	subtitle.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	subtitle.layout.childGap = 3;
	subtitle.backgroundColor = interface_theme::kDepth2Ink;
	subtitle.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};
	subtitle.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), subtitle) {
		CLAY_TEXT(context.uiManager.toClayString(
			context.params.definitionName.empty()
				? std::string_view{"Definition unavailable"}
				: context.params.definitionName),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextSecondary, 11)));
		CLAY_TEXT(context.uiManager.toClayString(
			context.params.instancePath.empty()
				? std::string_view{"Select an element to inspect its instance path"}
				: context.params.instancePath),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 9)));
	}
}

void DevInspectWorkbenchContent::buildElement(BuildContext& context) {
	Clay_ElementDeclaration content{};
	content.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	content.layout.padding = Clay_Padding{14, 14, 14, 14};
	content.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), content) {
		CLAY_TEXT(context.uiManager.toClayString("Workbench content"),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 12)));
	}
}

void DevInspectWorkbench::buildElement(BuildContext& context) {
	DevInterfaceState* state = context.params.interfaceState;
	App* app = context.params.app;
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevFlowNode* selected = nullptr;
	if (state && state->selectedElementId && app && app->hasWindow(state->selectedWindowId)) {
		snapshot = &app->ui(state->selectedWindowId).devTreeSnapshot();
		const auto found = std::ranges::find_if(snapshot->flow.nodes,
			[state](const tooling::DevFlowNode& node) {
				return node.instance.value == state->selectedElementId.value;
			});
		if (found != snapshot->flow.nodes.end()) selected = &*found;
	}

	char instanceFallback[24]{};
	char definitionFallback[24]{};
	std::string_view fullInstanceName{};
	std::string_view definitionName{};
	bool constructed = false;
	bool drawn = false;
	uint32_t directClayNodes = 0u;
	uint32_t overrides = 0u;
	if (selected && snapshot) {
		fullInstanceName = snapshot->string(selected->debugName);
		definitionName = snapshot->string(selected->definitionName);
		if (fullInstanceName.empty()) {
			std::snprintf(instanceFallback, sizeof(instanceFallback), "0x%016llX",
				static_cast<unsigned long long>(selected->instance.value));
			fullInstanceName = instanceFallback;
		}
		if (definitionName.empty()) {
			std::snprintf(definitionFallback, sizeof(definitionFallback), "0x%016llX",
				static_cast<unsigned long long>(selected->definition.value));
			definitionName = definitionFallback;
		}
		constructed = tooling::hasFlag(
			selected->flags, tooling::DevFlowNodeFlag::Constructed);
		drawn = tooling::hasFlag(selected->flags, tooling::DevFlowNodeFlag::Drawn);
#if FLOW_UI_DEV_CAPTURE_CLAY
		directClayNodes = selected->directClayCount;
#endif
		for (const tooling::DevOverrideApply::Record& record :
			app->devTooling().overrides().appliedOverrides().records()) {
			if (record.layer == tooling::DevOverrideLayer::EphemeralPreview ||
				record.target.definition != selected->definition) continue;
			if (record.target.scope == tooling::DevOverrideScope::Definition ||
				(record.target.window == state->selectedWindowId &&
				 record.target.instance == selected->instance)) ++overrides;
		}
	}
	const std::string path = readableInstancePath(fullInstanceName);

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), root) {
		context.uiManager.createElement(kDevInspectWorkbenchHeader, kHeader)
			.setParameters(DevInspectWorkbenchHeaderParameters{
				.instanceName = localInstanceName(fullInstanceName),
				.constructed = constructed,
				.drawn = drawn,
				.directClayNodeCount = directClayNodes,
				.overrideCount = overrides,
			}).setDevInternalCapture(true).draw();
		context.uiManager.createElement(kDevInspectWorkbenchSubtitle, kSubtitle)
			.setParameters(DevInspectWorkbenchSubtitleParameters{
				.definitionName = definitionName,
				.instancePath = path,
			}).setDevInternalCapture(true).draw();
		context.uiManager.createElement(kDevInspectWorkbenchContent, kContent)
			.setDevInternalCapture(true).draw();
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
