#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "FSEL/NumberInput.hpp"
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"
#if FLOWUI_PUBLIC_VULKAN_INTEROP
#include "devSystems/devInterface/Inspect/Workbench/DevPreviewViewPortRendering.hpp"
#include "managers/ViewPortManager.hpp"
#endif

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
inline constexpr LocalElementName kTopRow{"top-row"};
inline constexpr LocalElementName kBottomRow{"bottom-row"};
inline constexpr LocalElementName kPreview{"preview"};
inline constexpr LocalElementName kOverviewAndPerformance{"overview-and-performance"};
inline constexpr LocalElementName kChangesAndDiagnostics{"changes-and-diagnostics"};
inline constexpr LocalElementName kClaySubTree{"clay-sub-tree"};
inline constexpr LocalElementName kPreviewToolbar{"toolbar"};
inline constexpr LocalElementName kPreviewZoom{"zoom"};
inline constexpr LocalElementName kPreviewZoomReadout{"zoom-readout"};
inline constexpr LocalElementName kPreviewZoomSuffix{"zoom-suffix"};
inline constexpr LocalElementName kPreviewCameraGroup{"camera-group"};
inline constexpr LocalElementName kPreviewSidecarGroup{"sidecar-group"};
inline constexpr LocalElementName kPreviewToolGroup{"tool-group"};
inline constexpr LocalElementName kPreviewSeparator{"separator"};
inline constexpr LocalElementName kPreviewCanvas{"canvas"};
inline constexpr LocalElementName kPreviewViewportImage{"viewport-image"};
inline constexpr LocalElementName kPreviewPrimitive{"primitive"};
inline constexpr LocalElementName kPreviewGridLine{"grid-line"};
inline constexpr LocalElementName kPreviewOverlay{"overlay"};
inline constexpr LocalElementName kPreviewRuler{"ruler"};
inline constexpr LocalElementName kPreviewControl{"control"};
inline constexpr LocalElementName kOverviewHeader{"overview-header"};
inline constexpr LocalElementName kOverviewBody{"overview-body"};
inline constexpr LocalElementName kOverviewIdentity{"identity"};
inline constexpr LocalElementName kOverviewStructure{"structure"};
inline constexpr LocalElementName kOverviewPerformance{"performance"};
inline constexpr LocalElementName kOverviewMemory{"memory"};
inline constexpr LocalElementName kOverviewMetric{"metric"};
inline constexpr LocalElementName kOverviewColumn{"column"};
inline constexpr LocalElementName kOverviewPercentile{"percentile"};
inline constexpr LocalElementName kChangesHeader{"changes-header"};
inline constexpr LocalElementName kChangesBody{"changes-body"};
inline constexpr LocalElementName kChangesSummary{"changes-summary"};
inline constexpr LocalElementName kChangesRow{"change-row"};
inline constexpr LocalElementName kIssuesConsole{"issues-console"};
inline constexpr LocalElementName kIssueFilter{"issue-filter"};
inline constexpr LocalElementName kIssueCard{"issue-card"};
inline constexpr LocalElementName kIssueText{"issue-text"};

inline constexpr std::string_view kPreviewViewportKey =
	"flowui.dev_inspect.preview";

inline constexpr uint16_t kWorkbenchPadding = 14;
inline constexpr uint16_t kWorkbenchGap = 12;

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

template <typename Context>
void drawStub(Context& context, std::string_view label) {
	Clay_ElementDeclaration stub{};
	stub.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	stub.layout.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	stub.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), stub) {
		CLAY_TEXT(context.uiManager.toClayString(label),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 11)));
	}
}

void drawWorkbenchCard(
	DevInspectWorkbenchContent::BuildContext& context,
	LocalElementName id,
	DevWorkbenchSection section) {
	context.uiManager.createElement(kDevWorkbenchCard, id)
		.setParameters(DevWorkbenchCardParameters{
			.inspect = context.params.inspect,
			.section = section,
		})
		.setDevInternalCapture(true)
		.draw();
}

void synchronizeZoom(DevPreviewState& state) {
	if (!std::isfinite(state.zoomPercent)) state.zoomPercent = 100.0;
	state.zoomPercent = std::clamp(
		state.zoomPercent,
		state.camera.minZoom * 100.0f,
		state.camera.maxZoom * 100.0f);
	state.camera.zoomScale = static_cast<float>(state.zoomPercent * 0.01);
}

void setZoom(DevPreviewState& state, float zoom) {
	state.camera.zoomScale = std::clamp(
		zoom, state.camera.minZoom, state.camera.maxZoom);
	state.zoomPercent = state.camera.zoomScale * 100.0f;
}

void resetPreviewCamera(DevPreviewState& state) {
	setZoom(state, 1.0f);
	state.camera.panX = state.elementWidth * 0.5f;
	state.camera.panY = state.elementHeight * 0.5f;
}

void fitPreviewCamera(DevPreviewState& state) {
	constexpr float margin = 24.0f;
	const float availableWidth = std::max(10.0f, state.canvasWidth - margin * 2.0f);
	const float availableHeight = std::max(10.0f, state.canvasHeight - margin * 2.0f);
	const float scaleX = availableWidth / std::max(1.0f, state.elementWidth);
	const float scaleY = availableHeight / std::max(1.0f, state.elementHeight);
	setZoom(state, std::min(scaleX, scaleY));
	state.camera.panX = state.elementWidth * 0.5f;
	state.camera.panY = state.elementHeight * 0.5f;
}

void clearRuler(DevPreviewState& state) {
	state.ruler = {};
}

tooling::DevOverlayModeFlags toggledFlag(
	tooling::DevOverlayModeFlags flags,
	tooling::DevOverlayModeFlags flag) {
	return static_cast<tooling::DevOverlayModeFlags>(
		static_cast<uint32_t>(flags) ^ static_cast<uint32_t>(flag));
}



PreviewSelection resolvePreviewSelection(const DevInspectContentParameters& params) {
	PreviewSelection result{};
	if (!params.app || !params.interfaceState ||
		!params.interfaceState->selectedElementId ||
		!params.app->hasWindow(params.interfaceState->selectedWindowId)) return result;
	result.snapshot = &params.app->ui(
		params.interfaceState->selectedWindowId).devTreeSnapshot();
	const auto found = std::ranges::find_if(
		result.snapshot->flow.nodes,
		[params](const tooling::DevFlowNode& node) {
			return node.instance.value ==
				params.interfaceState->selectedElementId.value;
		});
	if (found == result.snapshot->flow.nodes.end()) return {};
	result.flow = &*found;
	result.flowIndex = static_cast<tooling::DevFlowNodeIndex>(
		std::distance(result.snapshot->flow.nodes.begin(), found));
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (found->clayRoot < result.snapshot->clay.nodes.size()) {
		result.clay = &result.snapshot->clay.nodes[found->clayRoot];
	}
#endif
	return result;
}

Clay_Vector2 worldToCanvas(
	const DevPreviewState& state, float worldX, float worldY) {
	return Clay_Vector2{
		state.canvasWidth * 0.5f +
			(worldX - state.camera.panX) * state.camera.zoomScale,
		state.canvasHeight * 0.5f +
			(worldY - state.camera.panY) * state.camera.zoomScale,
	};
}

Clay_Vector2 canvasPointerToWorld(
	const DevPreviewState& state, float canvasX, float canvasY) {
	const float zoom = std::max(state.camera.zoomScale, 1.0e-6f);
	return Clay_Vector2{
		state.camera.panX + (canvasX - state.canvasWidth * 0.5f) / zoom,
		state.camera.panY + (canvasY - state.canvasHeight * 0.5f) / zoom,
	};
}

Clay_FloatingElementConfig previewFloating(float x, float y, int16_t zIndex) {
	Clay_FloatingElementConfig floating{};
	floating.offset = {x, y};
	floating.zIndex = zIndex;
	floating.attachPoints = {
		.element = CLAY_ATTACH_POINT_LEFT_TOP,
		.parent = CLAY_ATTACH_POINT_LEFT_TOP,
	};
	floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
	floating.attachTo = CLAY_ATTACH_TO_PARENT;
	floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
	return floating;
}

uint16_t scaledU16(float value) {
	return static_cast<uint16_t>(std::clamp(
		std::lround(value), 0l,
		static_cast<long>(std::numeric_limits<uint16_t>::max())));
}

void drawPreviewControl(
	DevPreview::BuildContext& context,
	IndexedElementName id,
	DevPreviewState& state,
	DevPreviewControlCommand command,
	std::string_view label,
	bool active = false,
	TextureRef icon = {}) {
	context.uiManager.createElement(kDevPreviewControl, id)
		.setParameters(DevPreviewControlParameters{
			.preview = &state,
			.command = command,
			.label = label,
			.icon = icon,
			.active = active,
		})
		.setDevInternalCapture(true)
		.draw();
}

void drawPreviewToolbarSeparator(
	DevPreview::BuildContext& context,
	IndexedElementName id) {
	Clay_ElementDeclaration line{};
	line.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1), .height = CLAY_SIZING_FIXED(14)};
	line.backgroundColor = interface_theme::kBorderVisible;
	CLAY(context.clayID(id), line) {}
}

Clay_ElementDeclaration previewToolbarGroup() {
	Clay_ElementDeclaration group{};
	group.layout.sizing = {
		.width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_GROW(0)};
	group.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	group.layout.childGap = 3;
	group.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	group.clip = {.horizontal = true, .vertical = true};
	return group;
}

struct OverviewTelemetry {
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevFlowNode* flow = nullptr;
	tooling::DevFlowNodeIndex flowIndex = tooling::InvalidFlowNode;
#if FLOW_UI_DEV_CAPTURE_CLAY
	const tooling::DevClayNode* clay = nullptr;
#endif
	std::string definition{};
	std::string definitionId{};
	std::string flowNode{};
	std::string clayNode{"Unavailable"};
	std::string source{};
	std::string path{};
	std::string flowChildren{};
	std::string clayChildren{"Unavailable"};
	std::string flowSubtree{};
	std::string claySubtree{"Unavailable"};
	std::string depth{};
	std::string paint{"Unavailable"};
	std::string position{"Unavailable"};
	std::string dimensions{"Unavailable"};
	std::string layout{"Unavailable"};
	std::string padding{"Unavailable"};
	std::string sizing{"Unavailable"};
	std::string clip{"Unavailable"};
	std::string flags{};
	std::string currentFrame{"Unavailable"};
	std::string smoothFrame{"Unavailable"};
	std::string frameRange{"Unavailable"};
	std::string jitter{"Unavailable"};
	std::string frameContextSummary{"Frame context unavailable"};
	std::string timingLevelBadge{"LEVEL 2/2 BALANCED"};
	std::string instanceTiming{"Unavailable"};
	std::string subtreeTiming{"Unavailable"};
	std::string definitionTiming{"Unavailable"};
	std::string definitionPeak{"Unavailable"};
	std::string instanceMemory{};
	std::string stringMemory{};
	std::string schemaMemory{"Unavailable"};
	std::string engineMemory{"Unavailable"};
	std::string residentMemory{"Unavailable"};
	std::string gpuMemory{"Unavailable"};
	uint64_t invocationCount = 0u;
	bool timingAvailable = false;
};

std::string formatHex(uint64_t value) {
	char buffer[24]{};
	std::snprintf(buffer, sizeof(buffer), "0x%016llX",
		static_cast<unsigned long long>(value));
	return buffer;
}

std::string formatCount(uint64_t value) {
	return std::to_string(value);
}

std::string formatBytes(uint64_t bytes) {
	char buffer[32]{};
	if (bytes >= 1'000'000'000u) {
		std::snprintf(buffer, sizeof(buffer), "%.2f GB",
			static_cast<double>(bytes) / 1'000'000'000.0);
	} else if (bytes >= 1'000'000u) {
		std::snprintf(buffer, sizeof(buffer), "%.1f MB",
			static_cast<double>(bytes) / 1'000'000.0);
	} else if (bytes >= 1'000u) {
		std::snprintf(buffer, sizeof(buffer), "%.1f KB",
			static_cast<double>(bytes) / 1'000.0);
	} else {
		std::snprintf(buffer, sizeof(buffer), "%llu B",
			static_cast<unsigned long long>(bytes));
	}
	return buffer;
}

std::string formatDuration(uint64_t nanoseconds) {
	char buffer[32]{};
	if (nanoseconds >= 1'000'000u) {
		std::snprintf(buffer, sizeof(buffer), "%.3f ms",
			static_cast<double>(nanoseconds) / 1'000'000.0);
	} else {
		std::snprintf(buffer, sizeof(buffer), "%.1f us",
			static_cast<double>(nanoseconds) / 1'000.0);
	}
	return buffer;
}

std::string formatFrameTime(double milliseconds, bool withFps) {
	char buffer[48]{};
	if (withFps && milliseconds > 0.0) {
		std::snprintf(buffer, sizeof(buffer), "%.2f ms  /  %.1f FPS",
			milliseconds, 1'000.0 / milliseconds);
	} else {
		std::snprintf(buffer, sizeof(buffer), "%.2f ms", milliseconds);
	}
	return buffer;
}

std::string basename(std::string_view path) {
	const std::size_t separator = path.find_last_of("/\\");
	return std::string(separator == std::string_view::npos
		? path : path.substr(separator + 1u));
}

std::string compactMiddle(std::string_view value, std::size_t maximumBytes) {
	if (value.size() <= maximumBytes || maximumBytes < 7u) return std::string(value);
	std::size_t prefixBytes = (maximumBytes - 3u) / 2u;
	const std::size_t suffixBytes = maximumBytes - 3u - prefixBytes;
	while (prefixBytes > 0u &&
		(static_cast<unsigned char>(value[prefixBytes]) & 0xC0u) == 0x80u) {
		--prefixBytes;
	}
	std::size_t suffixBegin = value.size() - suffixBytes;
	while (suffixBegin < value.size() &&
		(static_cast<unsigned char>(value[suffixBegin]) & 0xC0u) == 0x80u) {
		++suffixBegin;
	}
	std::string result{};
	result.reserve(maximumBytes);
	result.append(value.substr(0u, prefixBytes));
	result += "...";
	result.append(value.substr(suffixBegin));
	return result;
}

uint32_t directFlowChildCount(
	const tooling::DevTreeSnapshot& snapshot,
	const tooling::DevFlowNode& node) noexcept {
	uint32_t count = 0u;
	auto child = node.firstChild;
	while (child < snapshot.flow.nodes.size()) {
		++count;
		child = snapshot.flow.nodes[child].nextSibling;
	}
	return count;
}

#if FLOW_UI_DEV_CAPTURE_CLAY
uint32_t directClayChildCount(
	const tooling::DevTreeSnapshot& snapshot,
	const tooling::DevClayNode& node) noexcept {
	uint32_t count = 0u;
	auto child = node.firstChild;
	while (child < snapshot.clay.nodes.size()) {
		++count;
		child = snapshot.clay.nodes[child].nextSibling;
	}
	return count;
}
#endif

std::string ancestryPath(
	const tooling::DevTreeSnapshot& snapshot,
	tooling::DevFlowNodeIndex flowIndex) {
	std::vector<std::string_view> names{};
	tooling::DevFlowNodeIndex index = flowIndex;
	while (index < snapshot.flow.nodes.size()) {
		const tooling::DevFlowNode& node = snapshot.flow.nodes[index];
		std::string_view name = snapshot.string(node.debugName);
		if (name.empty()) name = snapshot.string(node.definitionName);
		names.push_back(localInstanceName(name.empty() ? std::string_view{"?"} : name));
		index = node.parent;
	}
	std::string path{};
	for (auto iterator = names.rbegin(); iterator != names.rend(); ++iterator) {
		if (!path.empty()) path += " > ";
		path.append(*iterator);
	}
	return path;
}

std::string sizingName(Clay__SizingType type) {
	switch (type) {
		case CLAY__SIZING_TYPE_FIT: return "Fit";
		case CLAY__SIZING_TYPE_GROW: return "Grow";
		case CLAY__SIZING_TYPE_PERCENT: return "Percent";
		case CLAY__SIZING_TYPE_FIXED: return "Fixed";
	}
	return "Unknown";
}

uint64_t percentileValue(std::vector<uint64_t> samples, uint64_t percentile) {
	if (samples.empty()) return 0u;
	std::sort(samples.begin(), samples.end());
	if (percentile >= 100u) return samples.back();
	const double rank = std::ceil(
		(static_cast<double>(percentile) / 100.0) *
		static_cast<double>(samples.size()));
	const std::size_t index = static_cast<std::size_t>(std::max(1.0, rank)) - 1u;
	return samples[std::min(index, samples.size() - 1u)];
}

void updateFrameStatistics(
	DevOverviewAndPerformance::State& state,
	uint64_t frameNumber,
	double frameTimeMs) noexcept {
	if (frameNumber == 0u || frameNumber == state.lastFrameNumber || frameTimeMs <= 0.0) {
		return;
	}
	state.lastFrameNumber = frameNumber;
	state.frameTimeEmaMs = state.frameSampleCount == 0u
		? frameTimeMs : 0.10 * frameTimeMs + 0.90 * state.frameTimeEmaMs;
	state.frameTimeMs[state.nextFrameSample] = frameTimeMs;
	state.nextFrameSample = (state.nextFrameSample + 1u) % state.frameTimeMs.size();
	state.frameSampleCount = std::min(
		state.frameSampleCount + 1u, state.frameTimeMs.size());
}

void populateFrameStatistics(
	OverviewTelemetry& telemetry,
	const DevOverviewAndPerformance::State& state,
	double currentFrameMs) {
	if (state.frameSampleCount == 0u) return;
	const auto begin = state.frameTimeMs.begin();
	const auto end = begin + static_cast<std::ptrdiff_t>(state.frameSampleCount);
	const double average = std::accumulate(begin, end, 0.0) /
		static_cast<double>(state.frameSampleCount);
	double variance = 0.0;
	for (auto iterator = begin; iterator != end; ++iterator) {
		const double delta = *iterator - average;
		variance += delta * delta;
	}
	variance /= static_cast<double>(state.frameSampleCount);
	const auto [minimum, maximum] = std::minmax_element(begin, end);
	telemetry.currentFrame = formatFrameTime(currentFrameMs, true);
	telemetry.smoothFrame = formatFrameTime(state.frameTimeEmaMs, true);
	char range[48]{};
	std::snprintf(range, sizeof(range), "%.2f / %.2f ms", *minimum, *maximum);
	telemetry.frameRange = range;
	char jitter[32]{};
	std::snprintf(jitter, sizeof(jitter), "+/- %.2f ms", std::sqrt(variance));
	telemetry.jitter = jitter;

	char frameContext[128]{};
	std::snprintf(frameContext, sizeof(frameContext),
		"Frame context: %s  (EMA %s %s)",
		telemetry.currentFrame.c_str(),
		telemetry.smoothFrame.c_str(),
		telemetry.jitter.c_str());
	telemetry.frameContextSummary = frameContext;
}

uint64_t definitionSchemaBytes(const App& app, FlowDefinitionID definition) {
	const devMode::DevSchemaView schema = app.devTooling().schemas().view();
	if (!schema) return 0u;
	const devMode::DevElementSchema* element = schema->findElement(definition);
	if (!element) return 0u;
	uint64_t bytes = sizeof(devMode::DevElementSchema);
	const std::array roles{
		element->definitionType, element->parametersType,
		element->stateType, element->resourcesType};
	for (const devMode::DevTypeIndex typeIndex : roles) {
		const devMode::DevTypeSchema* type = schema->type(typeIndex);
		if (!type) continue;
		bytes += sizeof(devMode::DevTypeSchema);
		bytes += static_cast<uint64_t>(schema->fieldsOf(typeIndex).size()) *
			sizeof(devMode::DevFieldSchema);
	}
	return bytes;
}

OverviewTelemetry collectOverviewTelemetry(
	const DevInspectContentParameters& parameters,
	DevOverviewAndPerformance::State& state) {
	OverviewTelemetry telemetry{};
	const PreviewSelection selection = resolvePreviewSelection(parameters);
	if (!selection.snapshot || !selection.flow) return telemetry;
	telemetry.snapshot = selection.snapshot;
	telemetry.flow = selection.flow;
	telemetry.flowIndex = selection.flowIndex;
#if FLOW_UI_DEV_CAPTURE_CLAY
	telemetry.clay = selection.clay;
#endif
	const tooling::DevFlowNode& flow = *selection.flow;
	const tooling::DevTreeSnapshot& snapshot = *selection.snapshot;
	telemetry.definition = compactMiddle(snapshot.string(flow.definitionName), 30u);
	if (telemetry.definition.empty()) telemetry.definition = "Unnamed definition";
	telemetry.definitionId = formatHex(flow.definition.value);
	telemetry.flowNode = "#" + formatCount(selection.flowIndex);
	telemetry.path = compactMiddle(ancestryPath(snapshot, selection.flowIndex), 56u);
	telemetry.flowChildren = formatCount(directFlowChildCount(snapshot, flow));
	telemetry.flowSubtree = formatCount(
		flow.subtreeEnd >= selection.flowIndex
			? flow.subtreeEnd - selection.flowIndex : 0u);
	telemetry.depth = formatCount(flow.depth);
	std::string sourceFile = basename(snapshot.string(flow.sourceFile));
	if (sourceFile.empty()) sourceFile = "Unknown source";
	const std::string_view sourceFunction = snapshot.string(flow.sourceFunction);
	char source[256]{};
	std::snprintf(source, sizeof(source), "%s:%u:%u  %.*s",
		sourceFile.empty() ? "Unknown source" : sourceFile.c_str(),
		flow.sourceLine, flow.sourceColumn, static_cast<int>(sourceFunction.size()),
		sourceFunction.empty() ? "" : sourceFunction.data());
	telemetry.source = compactMiddle(source, 56u);
	std::vector<std::string_view> activeFlags{};
	if (tooling::hasFlag(flow.flags, tooling::DevFlowNodeFlag::Constructed)) {
		activeFlags.emplace_back("Constructed");
	}
	if (tooling::hasFlag(flow.flags, tooling::DevFlowNodeFlag::Drawn)) {
		activeFlags.emplace_back("Drawn");
	}
	if (tooling::hasFlag(flow.flags, tooling::DevFlowNodeFlag::InternalDev)) {
		activeFlags.emplace_back("Internal");
	}
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (tooling::hasFlag(flow.flags, tooling::DevFlowNodeFlag::MissingClayRoot)) {
		activeFlags.emplace_back("Missing Clay");
	}
	if (tooling::hasFlag(flow.flags, tooling::DevFlowNodeFlag::EscapedClayEmission)) {
		activeFlags.emplace_back("Escaped emission");
	}
#endif
	for (std::string_view flag : activeFlags) {
		if (!telemetry.flags.empty()) telemetry.flags += "  ";
		telemetry.flags += "[";
		telemetry.flags.append(flag);
		telemetry.flags += "]";
	}
	if (telemetry.flags.empty()) telemetry.flags = "No active flags";

	uint64_t stringBytes = 0u;
	const uint32_t flowEnd = std::min<uint32_t>(
		flow.subtreeEnd, static_cast<uint32_t>(snapshot.flow.nodes.size()));
	for (uint32_t index = selection.flowIndex; index < flowEnd; ++index) {
		const tooling::DevFlowNode& node = snapshot.flow.nodes[index];
		stringBytes += node.debugName.length + node.definitionName.length +
			node.definitionTypeToken.length + node.sourceFile.length +
			node.sourceFunction.length;
	}
	uint64_t structuralBytes = sizeof(tooling::DevFlowNode);
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (selection.clay) {
		const tooling::DevClayNode& clay = *selection.clay;
		telemetry.clayNode = "#" + formatCount(flow.clayRoot) + " / Clay " +
			formatCount(clay.clayId);
		telemetry.clayChildren = formatCount(directClayChildCount(snapshot, clay));
		const uint32_t clayCount = clay.subtreeEnd >= flow.clayRoot
			? clay.subtreeEnd - flow.clayRoot : 0u;
		telemetry.claySubtree = formatCount(clayCount);
		telemetry.depth += " Flow / " + formatCount(clay.depthWithinRoot) + " Clay";
		structuralBytes += sizeof(tooling::DevClayNode);
		const uint32_t clayEnd = std::min<uint32_t>(
			clay.subtreeEnd, static_cast<uint32_t>(snapshot.clay.nodes.size()));
		for (uint32_t index = flow.clayRoot; index < clayEnd; ++index) {
			stringBytes += snapshot.clay.nodes[index].idString.length +
				snapshot.clay.nodes[index].text.length;
		}
		char value[160]{};
		std::snprintf(value, sizeof(value), "X %.1f  Y %.1f px",
			clay.bounds.x, clay.bounds.y);
		telemetry.position = value;
		std::snprintf(value, sizeof(value), "%.1f x %.1f px",
			clay.bounds.width, clay.bounds.height);
		telemetry.dimensions = value;
		std::snprintf(value, sizeof(value), "%s  /  gap %u px",
			clay.declaration.layout.layoutDirection == CLAY_LEFT_TO_RIGHT
				? "Row (L -> R)" : "Column (T -> B)",
			clay.declaration.layout.childGap);
		telemetry.layout = value;
		const Clay_Padding& padding = clay.declaration.layout.padding;
		std::snprintf(value, sizeof(value), "T%u  R%u  B%u  L%u",
			padding.top, padding.right, padding.bottom, padding.left);
		telemetry.padding = value;
		telemetry.sizing = sizingName(clay.declaration.layout.sizing.width.type) +
			" x " + sizingName(clay.declaration.layout.sizing.height.type);
		const float overflowX = std::max(
			0.0f, clay.unwrappedTextDimensions.width - clay.bounds.width);
		const float overflowY = std::max(
			0.0f, clay.unwrappedTextDimensions.height - clay.bounds.height);
		std::snprintf(value, sizeof(value), "Clay %u  /  overflow %.1f, %.1f px",
			clay.clipClayId, overflowX, overflowY);
		telemetry.clip = value;
		if (tooling::hasFlag(clay.flags, tooling::DevClayNodeFlag::BoundsUnavailable)) {
			telemetry.flags += "  [Bounds unavailable]";
		}
		if (clay.rootIndex < snapshot.clay.roots.size()) {
			const tooling::DevClayRoot& root = snapshot.clay.roots[clay.rootIndex];
			telemetry.paint = "z " + std::to_string(root.zIndex) +
				"  /  paint " + formatCount(root.paintOrder);
		}
	}
#endif
	telemetry.instanceMemory = formatBytes(structuralBytes);
	telemetry.stringMemory = formatBytes(stringBytes);
	if (!parameters.app) return telemetry;
	const uint64_t schemaBytes = definitionSchemaBytes(*parameters.app, flow.definition);
	if (schemaBytes > 0u) telemetry.schemaMemory = formatBytes(schemaBytes);

	const DevTimingReporting& reporting =
		parameters.app->devMonitoring().timingReporting();
	const TimingReportingStatus timingStatus = reporting.status();
	std::vector<uint64_t> invokeSamples{};
	std::vector<uint64_t> subtreeSamples{};
	std::vector<uint64_t> definitionAverageSamples{};
	std::vector<uint64_t> definitionPeakSamples{};
	invokeSamples.reserve(60u);
	subtreeSamples.reserve(60u);
	definitionAverageSamples.reserve(60u);
	definitionPeakSamples.reserve(60u);
	double currentFrameMs = 0.0;
	uint64_t newestFrameNumber = 0u;
	if (timingStatus.hasRetainedTicks) {
		const AppTickId firstTick = timingStatus.newestRetainedAppTick > 59u
			? timingStatus.newestRetainedAppTick - 59u : 0u;
		const std::vector<TimingAppTickReport> reports =
			reporting.appTickRange(firstTick, 60u);
		for (const TimingAppTickReport& report : reports) {
			for (const TimingWindowReport& window : report.windows) {
				if (!window.occupied || !parameters.interfaceState ||
					window.window != parameters.interfaceState->selectedWindowId) continue;
				for (const TimingFrameReport& frame : window.frames) {
					if (!frame.occupied) continue;
					for (const CpuTimingRecord& record : frame.cpuZones) {
						if (record.typeId == timing_zones::kWindowFrameTotal.typeId &&
							frame.key.frameNumber >= newestFrameNumber) {
							newestFrameNumber = frame.key.frameNumber;
							currentFrameMs = static_cast<double>(record.durationNs) / 1'000'000.0;
						}
						if (record.entityKind != TimingEntityKind::ElementInstance ||
							record.primaryEntityId != flow.instance.value) continue;
						if (record.typeId == timing_zones::kElementInvoke.typeId) {
							invokeSamples.push_back(record.durationNs);
						} else if (record.typeId ==
							timing_zones::kElementConstructedSubtree.typeId) {
							subtreeSamples.push_back(record.durationNs);
						}
					}
					for (const ElementDefinitionTimingAggregate& aggregate :
						frame.elementDefinitions) {
						if (aggregate.definition != flow.definition ||
							aggregate.invocationCount == 0u) continue;
						definitionAverageSamples.push_back(
							aggregate.totalInclusiveNs / aggregate.invocationCount);
						definitionPeakSamples.push_back(aggregate.maximumInclusiveNs);
						telemetry.invocationCount += aggregate.invocationCount;
					}
				}
			}
		}
	}
	updateFrameStatistics(state, newestFrameNumber, currentFrameMs);
	populateFrameStatistics(telemetry, state, currentFrameMs);

#ifndef FLOWUI_DEV_TIMING_LEVEL
#define FLOWUI_DEV_TIMING_LEVEL 2
#endif
	const uint32_t compiledLevel = FLOWUI_DEV_TIMING_LEVEL;
	uint32_t activeLevel = compiledLevel;
	if (parameters.app) {
		const DevTimingConfig config = parameters.app->devMonitoring().timing().config();
		activeLevel = std::min<uint32_t>(compiledLevel, static_cast<uint32_t>(config.cpuLevel));
	}
	std::string levelName = "BALANCED";
	if (activeLevel == 0u) levelName = "FRAME ONLY";
	else if (activeLevel == 1u) levelName = "SUMMARY";
	else if (activeLevel == 2u) levelName = "BALANCED";
	else if (activeLevel >= 3u) levelName = "DEEP";

	char badge[64]{};
	std::snprintf(badge, sizeof(badge), "LEVEL %u/%u %s", activeLevel, compiledLevel, levelName.c_str());
	telemetry.timingLevelBadge = badge;
	const uint64_t percentile = state.selectedPercentile;
	if (!invokeSamples.empty()) {
		telemetry.instanceTiming = formatDuration(
			percentileValue(invokeSamples, percentile));
		telemetry.timingAvailable = true;
	}
	if (!subtreeSamples.empty()) {
		telemetry.subtreeTiming = formatDuration(
			percentileValue(subtreeSamples, percentile));
	}
	if (!definitionAverageSamples.empty()) {
		telemetry.definitionTiming = formatDuration(
			percentileValue(definitionAverageSamples, percentile));
	}
	if (!definitionPeakSamples.empty()) {
		telemetry.definitionPeak = formatDuration(
			percentileValue(definitionPeakSamples, percentile));
	}

	const auto environment =
		parameters.app->devMonitoring().memoryReporting().environmentSnapshot();
	if (environment) {
		telemetry.engineMemory = formatBytes(
			environment->safelyAttributableFlowUiCpuBytes);
		if (environment->process.residentBytes > 0u) {
			telemetry.residentMemory = formatBytes(environment->process.residentBytes);
		}
		if (environment->gpu.available) {
			uint64_t gpuBytes = 0u;
			for (const GpuHeapMemorySample& heap : environment->gpu.heaps) {
				gpuBytes += heap.allocatorUsageBytes > 0u
					? heap.allocatorUsageBytes : heap.allocationBytes;
			}
			telemetry.gpuMemory = formatBytes(gpuBytes);
		}
	}
	return telemetry;
}

Clay_ElementDeclaration overviewPanel() {
	Clay_ElementDeclaration panel{};
	panel.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	panel.layout.padding = Clay_Padding{9, 9, 7, 7};
	panel.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	panel.layout.childGap = 5;
	panel.backgroundColor = interface_theme::kDepth2Ink;
	panel.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	return panel;
}

template <typename Context>
void drawOverviewMetric(
	Context& context,
	IndexedElementName id,
	std::string_view label,
	std::string_view value,
	Clay_Color valueColor = interface_theme::kTextCanvas,
	Clay_SizingAxis widthSizing = CLAY_SIZING_GROW(0)) {
	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = widthSizing, .height = CLAY_SIZING_FIXED(14)};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childGap = 6;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	row.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(id), row) {
		CLAY_TEXT(context.uiManager.toClayString(label),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextSecondary, 8)));
		Clay_ElementDeclaration valueBox{};
		valueBox.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		valueBox.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		valueBox.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID("val-box"), valueBox) {
			CLAY_TEXT(context.uiManager.toClayString(value),
				CLAY_TEXT_CONFIG(textConfig(valueColor, 9)));
		}
	}
}

template <typename Context>
void drawOverviewPanelTitle(Context& context, std::string_view title) {
	CLAY_TEXT(context.uiManager.toClayString(title),
		CLAY_TEXT_CONFIG(textConfig(interface_theme::kAccentSeaGlass, 8)));
}

void drawPercentileChoice(
	DevOverviewAndPerformance::BuildContext& context,
	IndexedElementName id,
	DevOverviewAndPerformance::State& state,
	uint64_t value,
	std::string_view label,
	Clay_SizingAxis widthSizing = CLAY_SIZING_GROW(0)) {
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = &state.selectedPercentile;
	parameters.style.sizing = {
		.width = widthSizing, .height = CLAY_SIZING_FIXED(20)};
	parameters.style.padding = Clay_Padding{4, 4, 0, 0};
	parameters.style.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	parameters.style.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.style.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.style.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.style.idleOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.style.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.style.selectedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;
	context.uiManager.createElement(FSEL::kRadioChoice, id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.construct();
	CLAY_TEXT(context.uiManager.toClayString(label),
		CLAY_TEXT_CONFIG(textConfig(
			state.selectedPercentile == value
				? interface_theme::kTextCanvas : interface_theme::kTextSecondary,
			8)));
	context.uiManager.drawConstructed();
}

const devMode::DevFieldSchema* overrideField(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevFieldIndex index) noexcept {
	if (!index || index.value > schema.fields.size()) return nullptr;
	return &schema.fields[index.value - 1u];
}

std::string schemaName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return "Unknown";
	std::string_view name = schema.string(type->displayName);
	if (name.empty()) name = schema.string(type->cppTypeName);
	return std::string(name.empty() ? std::string_view{"Unknown"} : name);
}

std::string overrideFieldPath(
	const devMode::DevSchemaGeneration& schema,
	const tooling::DevOverrideApply::Record& record) {
	std::string result{};
	for (const devMode::DevFieldId pathId : record.field.nestedPath) {
		const auto found = std::ranges::find_if(
			schema.fields,
			[pathId](const devMode::DevFieldSchema& field) {
				return field.id == pathId;
			});
		if (found == schema.fields.end()) continue;
		std::string_view name = schema.string(found->displayName);
		if (name.empty()) name = schema.string(found->name);
		if (!result.empty()) result += ".";
		result.append(name);
	}
	if (const devMode::DevFieldSchema* field = overrideField(schema, record.fieldIndex)) {
		std::string_view name = schema.string(field->displayName);
		if (name.empty()) name = schema.string(field->name);
		if (!result.empty()) result += ".";
		result.append(name);
	}
	return result.empty() ? "Unknown field" : compactMiddle(result, 34u);
}

std::string overrideValueSummary(
	const devMode::DevSchemaGeneration& schema,
	const tooling::DevOverrideApply::Record& record) {
	const devMode::DevFieldSchema* field = overrideField(schema, record.fieldIndex);
	const devMode::DevTypeSchema* type = field ? schema.type(field->valueType) : nullptr;
	const void* value = record.value.data();
	if (!type || !value) return "Value unavailable";
	const devMode::DevTypeOps* operations = field->valueType.value < schema.typeOperations.size()
		? schema.typeOperations[field->valueType.value] : nullptr;
	const std::string typeName = schemaName(schema, type);
	char buffer[128]{};
	if (typeName == "Clay_Color") {
		const auto& color = *static_cast<const Clay_Color*>(value);
		std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
			static_cast<unsigned>(std::lround(std::clamp(color.r, 0.0f, 255.0f))),
			static_cast<unsigned>(std::lround(std::clamp(color.g, 0.0f, 255.0f))),
			static_cast<unsigned>(std::lround(std::clamp(color.b, 0.0f, 255.0f))),
			static_cast<unsigned>(std::lround(std::clamp(color.a, 0.0f, 255.0f))));
		return buffer;
	}
	if (typeName == "Clay_Padding") {
		const auto& padding = *static_cast<const Clay_Padding*>(value);
		std::snprintf(buffer, sizeof(buffer), "T%u R%u B%u L%u",
			padding.top, padding.right, padding.bottom, padding.left);
		return buffer;
	}
	if (type->kind == devMode::DevTypeKind::Boolean && operations &&
		operations->numericValue) {
		long double numeric = 0.0L;
		if (operations->numericValue(value, numeric)) return numeric == 0.0L ? "Off" : "On";
	}
	if ((type->kind == devMode::DevTypeKind::SignedInteger ||
		type->kind == devMode::DevTypeKind::UnsignedInteger ||
		type->kind == devMode::DevTypeKind::FloatingPoint) && operations &&
		operations->numericValue) {
		long double numeric = 0.0L;
		if (operations->numericValue(value, numeric)) {
			std::snprintf(buffer, sizeof(buffer), "%.6Lg", numeric);
			return buffer;
		}
	}
	if (type->kind == devMode::DevTypeKind::Enumeration && operations &&
		operations->numericValue) {
		long double numeric = 0.0L;
		if (operations->numericValue(value, numeric)) {
			const uint64_t bits = static_cast<uint64_t>(numeric);
			const uint32_t begin = type->enumeration.values.first;
			const uint32_t count = type->enumeration.values.count;
			if (begin <= schema.enumValues.size() && count <= schema.enumValues.size() - begin) {
				for (uint32_t index = 0u; index < count; ++index) {
					const devMode::DevEnumValueSchema& option = schema.enumValues[begin + index];
					if (option.bits == bits) return std::string(schema.string(option.name));
				}
			}
			return std::to_string(bits);
		}
	}
	if (type->kind == devMode::DevTypeKind::Text && operations && operations->textView) {
		return compactMiddle(operations->textView(value), 36u);
	}
	if (type->kind == devMode::DevTypeKind::Sequence && operations &&
		operations->sequenceSize) {
		return std::to_string(operations->sequenceSize(value)) + " items";
	}
	if (type->kind == devMode::DevTypeKind::Object) {
		return std::to_string(type->fields.count) + " fields";
	}
	return compactMiddle(typeName + " / " + std::to_string(type->size) + " B", 36u);
}

struct TreeIssueDescription {
	std::string_view title{};
	std::string_view message{};
	std::string_view action{};
	DevResolvedIssueSeverity severity = DevResolvedIssueSeverity::Warning;
};

TreeIssueDescription describeTreeIssue(tooling::DevTreeDiagnosticCode code) noexcept {
	switch (code) {
		case tooling::DevTreeDiagnosticCode::FlowCaptureUnbalanced:
			return {"FlowCaptureUnbalanced",
				"The element capture scope did not close in a balanced order.",
				"Check constructed-element open and draw ordering around this element.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::FlowNodeCapacityExceeded:
			return {"FlowNodeCapacityExceeded",
				"The Flow tree capture reached its configured node capacity.",
				"Increase the development tree node capacity or reduce emitted elements.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::StringCapacityExceeded:
			return {"StringCapacityExceeded",
				"The tree capture could not retain all diagnostic strings.",
				"Increase the development capture string capacity.",
				DevResolvedIssueSeverity::Warning};
#if FLOW_UI_DEV_CAPTURE_CLAY
		case tooling::DevTreeDiagnosticCode::FlowElementMissingClayRoot:
			return {"MissingClayRoot",
				"The Flow element did not emit a matching Clay root.",
				"Emit one root using the element build context Clay ID.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::FlowElementDuplicateClayRoot:
			return {"DuplicateClayRoot",
				"More than one Clay node claimed the element root identity.",
				"Ensure the element context Clay ID is emitted exactly once.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::FlowClayDebugNameMismatch:
			return {"ClayDebugNameMismatch",
				"The captured Clay root name differs from the Flow element name.",
				"Use the build context root ID instead of a separately composed ID.",
				DevResolvedIssueSeverity::Warning};
		case tooling::DevTreeDiagnosticCode::FlowElementEmittedClayOutsideRoot:
			return {"EscapedClayEmission",
				"The element emitted Clay nodes outside its declared root.",
				"Move all Clay emission inside the element root container.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::FlowChildClayParentMismatch:
			return {"ClayParentMismatch",
				"A child element's Clay root is attached to an unexpected parent.",
				"Draw the child within the intended parent Clay scope.",
				DevResolvedIssueSeverity::Warning};
		case tooling::DevTreeDiagnosticCode::ClayBridgeIdCollision:
			return {"ClayBridgeIdCollision",
				"Multiple captured nodes share a Clay bridge identity.",
				"Assign stable unique IDs to sibling Clay elements.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::ClayAttachmentParentMissing:
			return {"ClayAttachmentParentMissing",
				"A floating Clay root references an attachment parent that is absent.",
				"Emit the attachment parent before the floating child.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::ClayClipNodeMissing:
			return {"ClayClipNodeMissing",
				"The selected subtree references a clip node that is absent.",
				"Declare clipping on a retained parent before child emission.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::ClayBridgeTraversalFailed:
			return {"ClayBridgeTraversalFailed",
				"The captured Clay hierarchy could not be traversed completely.",
				"Inspect duplicate IDs and parent-child emission order.",
				DevResolvedIssueSeverity::Error};
		case tooling::DevTreeDiagnosticCode::ClayNodeCapacityExceeded:
			return {"ClayNodeCapacityExceeded",
				"The Clay tree capture reached its configured node capacity.",
				"Increase Clay capture capacity or reduce the emitted subtree.",
				DevResolvedIssueSeverity::Error};
#endif
	}
	return {"TreeDiagnostic", "The tree capture reported an unknown issue.",
		"Inspect the global diagnostics report for additional evidence.",
		DevResolvedIssueSeverity::Warning};
}

bool diagnosticMatchesSelection(
	const tooling::DevTreeSnapshot& snapshot,
	const tooling::DevTreeDiagnostic& diagnostic,
	tooling::DevFlowNodeIndex flowIndex) noexcept {
	if (diagnostic.flow == flowIndex) return true;
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (flowIndex >= snapshot.flow.nodes.size()) return false;
	const tooling::DevClayNodeIndex root = snapshot.flow.nodes[flowIndex].clayRoot;
	if (root >= snapshot.clay.nodes.size() ||
		diagnostic.clay >= snapshot.clay.nodes.size()) return false;
	return diagnostic.clay >= root &&
		diagnostic.clay < snapshot.clay.nodes[root].subtreeEnd;
#else
	(void)snapshot;
	return false;
#endif
}

std::optional<tooling::DevOverrideScope> resolveOccurrenceScope(
	const App& app,
	const DevErrorOccurrence& occurrence,
	FlowDefinitionID definition,
	uint64_t instance,
	WindowId window) {
	const ErrorSubjectKind subjectKind = occurrence.error.subjectKind();
	if (subjectKind == ErrorSubjectKind::ElementInstance &&
		occurrence.error.subject == instance) {
		return tooling::DevOverrideScope::ExactInstance;
	}
	if (subjectKind == ErrorSubjectKind::ElementDefinition &&
		occurrence.error.subject == definition.value) {
		return tooling::DevOverrideScope::Definition;
	}
	if (occurrence.timing.state != DevErrorCorrelationState::Available ||
		occurrence.timing.containingInvocation == 0u ||
		occurrence.timing.frame.window != window) return std::nullopt;
	const auto report = app.devMonitoring().timingReporting().appTickReport(
		occurrence.timing.appTick);
	if (!report) return std::nullopt;
	for (const TimingWindowReport& windowReport : report->windows) {
		if (!windowReport.occupied || windowReport.window != window) continue;
		for (const TimingFrameReport& frame : windowReport.frames) {
			for (const CpuTimingRecord& record : frame.cpuZones) {
				if (record.invocationId != occurrence.timing.containingInvocation ||
					record.entityKind != TimingEntityKind::ElementInstance) continue;
				if (record.primaryEntityId == instance) {
					return tooling::DevOverrideScope::ExactInstance;
				}
				if (record.secondaryEntityId == definition.value) {
					return tooling::DevOverrideScope::Definition;
				}
			}
		}
	}
	return std::nullopt;
}

DevResolvedIssueSeverity errorSeverity(const FlowUiError& error) noexcept {
	return error.descriptor().impact == ErrorImpact::Degraded ||
		error.descriptor().impact == ErrorImpact::None
		? DevResolvedIssueSeverity::Warning : DevResolvedIssueSeverity::Error;
}

DevChangeAndIssuePanelData queryChangeAndIssueData(
	const DevInspectContentParameters& parameters) {
	DevChangeAndIssuePanelData result{};
	const PreviewSelection selection = resolvePreviewSelection(parameters);
	if (!selection.snapshot || !selection.flow || !parameters.app ||
		!parameters.interfaceState) return result;
	const tooling::DevTreeSnapshot& snapshot = *selection.snapshot;
	const tooling::DevFlowNode& flow = *selection.flow;
	const WindowId window = parameters.interfaceState->selectedWindowId;
	const auto& apply = parameters.app->devTooling().overrides().appliedOverrides();
	const devMode::DevSchemaView schema = parameters.app->devTooling().schemas().view();
	if (schema) {
		for (const tooling::DevOverrideApply::Record& record : apply.records()) {
			if (!record.schemaValid || !record.value ||
				record.target.definition != flow.definition) continue;
			if (record.target.scope == tooling::DevOverrideScope::ExactInstance &&
				(record.target.window != window || record.target.instance != flow.instance)) continue;
			tooling::DevOverrideLayer winning{};
			if (!apply.winningLayer(
					flow.definition, window, flow.instance, record.field.field, winning) ||
				winning != record.layer) continue;
			const devMode::DevFieldSchema* field = overrideField(*schema, record.fieldIndex);
			result.activeChanges.push_back(DevMinimalChangeItem{
				.fieldId = field ? field->id : record.field.field,
				.fieldPath = overrideFieldPath(*schema, record),
				.valueSummary = overrideValueSummary(*schema, record),
				.scope = record.target.scope,
			});
		}
	}
	std::ranges::sort(result.activeChanges,
		[](const DevMinimalChangeItem& left, const DevMinimalChangeItem& right) {
			if (left.scope != right.scope) {
				return left.scope == tooling::DevOverrideScope::ExactInstance;
			}
			return left.fieldPath < right.fieldPath;
		});

	std::string sourceFile = basename(snapshot.string(flow.sourceFile));
	if (sourceFile.empty()) sourceFile = "Unknown source";
	for (const tooling::DevTreeDiagnostic& diagnostic : snapshot.diagnostics) {
		if (!diagnosticMatchesSelection(snapshot, diagnostic, selection.flowIndex)) continue;
		const TreeIssueDescription description = describeTreeIssue(diagnostic.code);
		result.resolvedIssues.push_back(DevResolvedElementIssue{
			.issueId = 0x100000000ull + static_cast<uint64_t>(diagnostic.code),
			.title = std::string(description.title),
			.message = std::string(description.message),
			.source = sourceFile + ":" + std::to_string(flow.sourceLine),
			.target = "Flow #" + std::to_string(selection.flowIndex),
			.suggestedAction = std::string(description.action),
			.scope = tooling::DevOverrideScope::ExactInstance,
			.severity = description.severity,
		});
	}
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (selection.clay) {
		const tooling::DevClayNode& clay = *selection.clay;
		const std::string target = "Clay #" + std::to_string(flow.clayRoot);
		if (clay.bounds.width <= 0.0f || clay.bounds.height <= 0.0f) {
			result.resolvedIssues.push_back(DevResolvedElementIssue{
				.issueId = 0x200000001ull,
				.title = "ZeroSolvedBounds",
				.message = "The selected element solved to a zero width or height.",
				.source = sourceFile + ":" + std::to_string(flow.sourceLine),
				.target = target,
				.suggestedAction = "Inspect parent sizing constraints and GROW/FIT relationships.",
				.scope = tooling::DevOverrideScope::ExactInstance,
				.severity = DevResolvedIssueSeverity::Warning,
			});
		}
		const float overflowX = std::max(
			0.0f, clay.unwrappedTextDimensions.width - clay.bounds.width);
		const float overflowY = std::max(
			0.0f, clay.unwrappedTextDimensions.height - clay.bounds.height);
		if (overflowX > 0.5f || overflowY > 0.5f) {
			char message[128]{};
			std::snprintf(message, sizeof(message),
				"Unwrapped text exceeds solved bounds by %.1f x %.1f px.",
				overflowX, overflowY);
			result.resolvedIssues.push_back(DevResolvedElementIssue{
				.issueId = 0x200000002ull,
				.title = "TextLayoutOverflow",
				.message = message,
				.source = sourceFile + ":" + std::to_string(flow.sourceLine),
				.target = target,
				.suggestedAction = "Adjust wrapping, sizing constraints, or the nearest clip container.",
				.scope = tooling::DevOverrideScope::ExactInstance,
				.severity = DevResolvedIssueSeverity::Warning,
			});
		}
	}
#endif

	for (const DevErrorOccurrence& occurrence :
		parameters.app->devMonitoring().errorReporting().occurrenceSnapshot()) {
		const std::optional<tooling::DevOverrideScope> scope = resolveOccurrenceScope(
			*parameters.app, occurrence, flow.definition, flow.instance.value, window);
		if (!scope) continue;
		const ErrorDescriptor descriptor = occurrence.error.descriptor();
		std::string action = "Inspect the related subsystem report and captured evidence.";
		if (!occurrence.advice.empty() && !occurrence.advice.front().suggestedAction.empty()) {
			action = std::string(occurrence.advice.front().suggestedAction);
		}
		result.resolvedIssues.push_back(DevResolvedElementIssue{
			.issueId = occurrence.id,
			.title = std::string(descriptor.name),
			.message = std::string(descriptor.message),
			.source = std::string(errorSiteName(occurrence.error.site)) +
				" / tick " + std::to_string(occurrence.lastAppTick),
			.target = *scope == tooling::DevOverrideScope::ExactInstance
				? "Instance " + formatHex(flow.instance.value)
				: "Definition " + formatHex(flow.definition.value),
			.suggestedAction = std::move(action),
			.scope = *scope,
			.severity = errorSeverity(occurrence.error),
		});
	}
	std::ranges::stable_sort(result.resolvedIssues,
		[](const DevResolvedElementIssue& left, const DevResolvedElementIssue& right) {
			return left.severity < right.severity;
		});
	return result;
}

Clay_Color issueColor(DevResolvedIssueSeverity severity) noexcept {
	switch (severity) {
		case DevResolvedIssueSeverity::Error: return interface_theme::kStatusRed;
		case DevResolvedIssueSeverity::Warning: return interface_theme::kStatusAmber;
		case DevResolvedIssueSeverity::Advice: return interface_theme::kAccentSignalBlue;
	}
	return interface_theme::kTextMuted;
}

std::string_view issueLabel(DevResolvedIssueSeverity severity) noexcept {
	switch (severity) {
		case DevResolvedIssueSeverity::Error: return "[!] ERROR";
		case DevResolvedIssueSeverity::Warning: return "[*] WARNING";
		case DevResolvedIssueSeverity::Advice: return "[?] ADVICE";
	}
	return "ISSUE";
}

bool issueMatchesFilter(
	const DevResolvedElementIssue& issue,
	uint64_t filter) noexcept {
	return filter == 0u ||
		(filter == 1u && issue.severity == DevResolvedIssueSeverity::Error) ||
		(filter == 2u && issue.severity == DevResolvedIssueSeverity::Warning) ||
		(filter == 3u && issue.severity == DevResolvedIssueSeverity::Advice);
}

void drawIssueFilterChoice(
	DevChangesAndDiagnostics::BuildContext& context,
	IndexedElementName id,
	DevChangesAndDiagnostics::State& state,
	uint64_t value,
	std::string_view label,
	uint32_t count) {
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = &state.selectedIssueFilter;
	parameters.style.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(20)};
	parameters.style.padding = Clay_Padding{4, 4, 0, 0};
	parameters.style.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	parameters.style.contentGap = 3;
	parameters.style.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.style.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.style.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.style.idleOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.style.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.style.selectedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;
	context.uiManager.createElement(FSEL::kRadioChoice, id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.construct();
	const bool selected = state.selectedIssueFilter == value;
	CLAY_TEXT(context.uiManager.toClayString(label),
		CLAY_TEXT_CONFIG(textConfig(
			selected ? interface_theme::kTextCanvas : interface_theme::kTextSecondary, 8)));
	CLAY_TEXT(context.uiManager.toClayString(std::to_string(count)),
		CLAY_TEXT_CONFIG(textConfig(
			selected ? interface_theme::kAccentSeaGlass : interface_theme::kTextMuted, 8)));
	context.uiManager.drawConstructed();
}

void drawIssueCard(
	DevChangesAndDiagnostics::BuildContext& context,
	IndexedElementName id,
	const DevResolvedElementIssue& issue) {
	const Clay_Color accent = issueColor(issue.severity);
	Clay_ElementDeclaration card{};
	card.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(72)};
	card.layout.padding = Clay_Padding{8, 8, 6, 6};
	card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	card.layout.childGap = 3;
	card.backgroundColor = interface_theme::kDepth2Ink;
	card.border = {
		.color = accent,
		.width = Clay_BorderWidth{2, 1, 1, 1, 0},
	};
	card.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(id), card) {
		Clay_ElementDeclaration header{};
		header.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(12)};
		header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		header.layout.childGap = 5;
		IndexedElementIDSequence textIds{kIssueText};
		CLAY(context.clayID(textIds.next()), header) {
			CLAY_TEXT(context.uiManager.toClayString(issueLabel(issue.severity)),
				CLAY_TEXT_CONFIG(textConfig(accent, 8)));
			CLAY_TEXT(context.uiManager.toClayString(compactMiddle(issue.title, 34u)),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 9)));
			CLAY(context.clayID(textIds.next()), {
				.layout = {.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0)}}}) {}
			CLAY_TEXT(context.uiManager.toClayString(
				issue.scope == tooling::DevOverrideScope::ExactInstance ? "[INST]" : "[DEF]"),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
		}
		const std::string metadata = compactMiddle(issue.source, 34u) + "  /  " +
			compactMiddle(issue.target, 30u);
		CLAY_TEXT(context.uiManager.toClayString(metadata),
			CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
		Clay_TextElementConfig messageStyle = textConfig(interface_theme::kTextSecondary, 8);
		messageStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
		CLAY_TEXT(context.uiManager.toClayString(compactMiddle(issue.message, 100u)),
			CLAY_TEXT_CONFIG(messageStyle));
		const std::string action = "Fix: " + compactMiddle(issue.suggestedAction, 96u);
		Clay_TextElementConfig actionStyle = textConfig(interface_theme::kAccentSeaGlass, 8);
		actionStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
		CLAY_TEXT(context.uiManager.toClayString(action), CLAY_TEXT_CONFIG(actionStyle));
	}
}

} // namespace

DevPreview::Resources::Resources(App& app) {
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	viewportRenderer = std::make_shared<DevPreviewViewPortRenderer>();
#endif
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	auto resolve = [&icons](std::string_view key) -> TextureRef {
		return icons.contains(key) ? icons.textureRef(key) : TextureRef{};
	};
	panIcon = resolve(interface_icons::kPreviewPanKey);
	resetIcon = resolve(interface_icons::kPreviewResetKey);
	rulerIcon = resolve(interface_icons::kPreviewRulerKey);
#else
	(void)app;
#endif
}

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
			.width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_GROW(0)};
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

void DevPreviewControl::onHovered(InteractionContext& context) {
	if (context.params.preview) {
		context.uiManager.requestCursor(CursorType::PointingHand, 20);
	}
}

void DevPreviewControl::onPressed(InteractionContext& context) {
	DevPreviewState* state = context.params.preview;
	if (!state) return;
	switch (context.params.command) {
		case DevPreviewControlCommand::ZoomOut:
			setZoom(*state, state->camera.zoomScale - 0.05f);
			break;
		case DevPreviewControlCommand::ZoomIn:
			setZoom(*state, state->camera.zoomScale + 0.05f);
			break;
		case DevPreviewControlCommand::Fit:
			fitPreviewCamera(*state);
			break;
		case DevPreviewControlCommand::Reset:
			resetPreviewCamera(*state);
			break;
		case DevPreviewControlCommand::ToggleBoxModel:
			state->sidecarFlags = toggledFlag(
				state->sidecarFlags, tooling::DevOverlayModeFlags::BoxModel);
			break;
		case DevPreviewControlCommand::ToggleRulers:
			state->sidecarFlags = toggledFlag(
				state->sidecarFlags, tooling::DevOverlayModeFlags::RulersAndDistance);
			break;
		case DevPreviewControlCommand::ToggleTypography:
			state->sidecarFlags = toggledFlag(
				state->sidecarFlags, tooling::DevOverlayModeFlags::Typography);
			break;
		case DevPreviewControlCommand::ToggleHierarchy:
			state->sidecarFlags = toggledFlag(
				state->sidecarFlags, tooling::DevOverlayModeFlags::TreeHierarchy);
			break;
		case DevPreviewControlCommand::ToggleClip:
			state->sidecarFlags = toggledFlag(
				state->sidecarFlags, tooling::DevOverlayModeFlags::ScissorAndClip);
			break;
		case DevPreviewControlCommand::ToggleDiagnostics:
			state->sidecarFlags = toggledFlag(
				state->sidecarFlags, tooling::DevOverlayModeFlags::RenderRunDiagnostics);
			break;
		case DevPreviewControlCommand::SelectPan:
			state->activeTool = DevPreviewToolMode::PanInspect;
			state->isDraggingPan = false;
			clearRuler(*state);
			break;
		case DevPreviewControlCommand::SelectRuler:
			state->activeTool = DevPreviewToolMode::Ruler;
			state->isDraggingPan = false;
			clearRuler(*state);
			break;
	}
}

void DevPreviewControl::buildElement(BuildContext& context) {
	const bool hovered = context.uiManager.getPreviousFramesInteraction()
		.isHovered(context.clayID());
	Clay_ElementDeclaration control{};
	control.layout.sizing = {
		.width = context.params.icon.handle
			? CLAY_SIZING_FIXED(24) : CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(22)};
	control.layout.padding = Clay_Padding{5, 5, 0, 0};
	control.layout.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	control.backgroundColor = context.params.active
		? interface_theme::kSelectedRow
		: hovered ? interface_theme::kHoverSurface : interface_theme::kDepth2Ink;
	control.cornerRadius = CLAY_CORNER_RADIUS(3);
	control.border = {
		.color = context.params.active
			? interface_theme::kAccentCurrent : interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	CLAY(context.clayID(), control) {
		const Clay_Color foreground = context.params.active
			? interface_theme::kAccentSeaGlass
			: interface_theme::kTextSecondary;
		if (context.params.icon.handle) {
			TextureRef icon = context.params.icon;
			icon.tintEnabled = true;
			Clay_ElementDeclaration image{};
			image.layout.sizing = {
				.width = CLAY_SIZING_FIXED(12), .height = CLAY_SIZING_FIXED(12)};
			image.backgroundColor = foreground;
			image.image.imageData = context.uiManager.imageData(icon);
			CLAY(context.clayID("icon"), image) {}
		} else {
			CLAY_TEXT(context.uiManager.toClayString(context.params.label),
				CLAY_TEXT_CONFIG(textConfig(foreground, 8)));
		}
	}
}

void DevPreviewCanvas::onHovered(InteractionContext& context) {
	DevPreviewState* state = context.params.preview;
	if (!state) return;
	context.uiManager.requestCursor(
		state->activeTool == DevPreviewToolMode::Ruler
			? CursorType::Crosshair
			: state->isDraggingPan ? CursorType::Grabbing : CursorType::Grab,
		25);
	const FrameInput& input = context.uiManager.getCurrentFrameInput();
	const Clay_ElementData canvas = Clay_GetElementData(context.clayID());
	if (canvas.found) {
		state->canvasWidth = std::max(0.0f, canvas.boundingBox.width);
		state->canvasHeight = std::max(0.0f, canvas.boundingBox.height);
	}
	if (input.scrollY != 0.0f && canvas.found) {
		const float localX = input.mouseX - canvas.boundingBox.x;
		const float localY = input.mouseY - canvas.boundingBox.y;
		const Clay_Vector2 anchoredWorld = canvasPointerToWorld(*state, localX, localY);
		// App input converts one conventional wheel notch to roughly 20 layout
		// units. Normalize that transport-scale delta back to a 10 percentage-point
		// zoom step while preserving smaller high-resolution trackpad deltas.
		constexpr float kLayoutScrollUnitsPerStep = 20.0f;
		constexpr float kZoomScalePerStep = 0.10f;
		const float zoomDelta = std::clamp(
			input.scrollY / kLayoutScrollUnitsPerStep, -4.0f, 4.0f) *
			kZoomScalePerStep;
		setZoom(*state, state->camera.zoomScale + zoomDelta);
		state->camera.panX = anchoredWorld.x -
			(localX - state->canvasWidth * 0.5f) / state->camera.zoomScale;
		state->camera.panY = anchoredWorld.y -
			(localY - state->canvasHeight * 0.5f) / state->camera.zoomScale;
	}
	if (state->activeTool == DevPreviewToolMode::Ruler &&
		state->ruler.pointAValid && !state->ruler.pointBValid && canvas.found) {
		const Clay_Vector2 world = canvasPointerToWorld(
			*state,
			input.mouseX - canvas.boundingBox.x,
			input.mouseY - canvas.boundingBox.y);
		state->ruler.pointBX = world.x;
		state->ruler.pointBY = world.y;
	}
}

void DevPreviewCanvas::onPressed(InteractionContext& context) {
	DevPreviewState* state = context.params.preview;
	if (!state) return;
	const FrameInput& input = context.uiManager.getCurrentFrameInput();
	const Clay_ElementData canvas = Clay_GetElementData(context.clayID());
	if (!canvas.found) return;
	if (state->activeTool == DevPreviewToolMode::PanInspect) {
		state->isDraggingPan = true;
		state->dragStartMouseX = input.mouseX;
		state->dragStartMouseY = input.mouseY;
		state->dragStartPanX = state->camera.panX;
		state->dragStartPanY = state->camera.panY;
		return;
	}
	const Clay_Vector2 world = canvasPointerToWorld(
		*state,
		input.mouseX - canvas.boundingBox.x,
		input.mouseY - canvas.boundingBox.y);
	if (!state->ruler.pointAValid || state->ruler.pointBValid) {
		state->ruler = {
			.pointAValid = true,
			.pointBValid = false,
			.pointAX = world.x,
			.pointAY = world.y,
			.pointBX = world.x,
			.pointBY = world.y,
		};
	} else {
		state->ruler.pointBX = world.x;
		state->ruler.pointBY = world.y;
		state->ruler.pointBValid = true;
	}
}

void DevPreviewCanvas::onReleased(InteractionContext& context) {
	DevPreviewState* state = context.params.preview;
	if (!state) return;
	const FrameInput& input = context.uiManager.getCurrentFrameInput();
	const bool wasClick = state->isDraggingPan &&
		std::hypot(input.mouseX - state->dragStartMouseX,
			input.mouseY - state->dragStartMouseY) < 3.0f;
	state->isDraggingPan = false;
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (!wasClick || state->activeTool != DevPreviewToolMode::PanInspect) return;
	const PreviewSelection selection = resolvePreviewSelection(context.params.inspect);
	const Clay_ElementData canvas = Clay_GetElementData(context.clayID());
	if (!selection || !canvas.found) return;
	const Clay_Vector2 world = canvasPointerToWorld(
		*state,
		input.mouseX - canvas.boundingBox.x,
		input.mouseY - canvas.boundingBox.y);
	const float sourceX = selection.clay->bounds.x + world.x;
	const float sourceY = selection.clay->bounds.y + world.y;
	tooling::DevFlowNodeIndex hit = selection.flowIndex;
	const uint32_t end = std::min<uint32_t>(
		selection.flow->subtreeEnd, selection.snapshot->flow.nodes.size());
	for (tooling::DevFlowNodeIndex index = selection.flowIndex; index < end; ++index) {
		const tooling::DevFlowNode& flow = selection.snapshot->flow.nodes[index];
		if (flow.clayRoot >= selection.snapshot->clay.nodes.size()) continue;
		const Clay_BoundingBox bounds = selection.snapshot->clay.nodes[flow.clayRoot].bounds;
		if (sourceX >= bounds.x && sourceX <= bounds.x + bounds.width &&
			sourceY >= bounds.y && sourceY <= bounds.y + bounds.height &&
			flow.depth >= selection.snapshot->flow.nodes[hit].depth) hit = index;
	}
	if (DevInterfaceState* interfaceState = context.params.inspect.interfaceState) {
		const tooling::DevFlowNode& selected = selection.snapshot->flow.nodes[hit];
		interfaceState->inspectSelectedNodeKind = kDevInterfaceFlowNodeKind;
		interfaceState->inspectSelectedNodeKey = selected.instance.value;
		interfaceState->selectedElementId = FlowElementID{.value = selected.instance.value};
	}
#else
	(void)wasClick;
#endif
}

void DevPreviewCanvas::runLogic(InteractionContext& context) {
	DevPreviewState* state = context.params.preview;
	if (!state || !state->isDraggingPan) return;
	const FrameInput& input = context.uiManager.getCurrentFrameInput();
	if (!input.mouseDown[0]) {
		state->isDraggingPan = false;
		return;
	}
	const float zoom = std::max(state->camera.zoomScale, 1.0e-6f);
	state->camera.panX = state->dragStartPanX -
		(input.mouseX - state->dragStartMouseX) / zoom;
	state->camera.panY = state->dragStartPanY -
		(input.mouseY - state->dragStartMouseY) / zoom;
}

namespace {

void drawPreviewRect(
	DevPreviewCanvas::BuildContext& context,
	const DevPreviewState& state,
	IndexedElementIDSequence& ids,
	float x,
	float y,
	float width,
	float height,
	Clay_Color fill,
	Clay_Color border = {},
	Clay_BorderWidth borderWidth = {},
	Clay_CornerRadius radius = {},
	int16_t zIndex = 0) {
	if (width <= 0.0f || height <= 0.0f) return;
	if (state.canvasWidth <= 0.0f || state.canvasHeight <= 0.0f) return;
	if (x + width <= 0.0f || x >= state.canvasWidth ||
		y + height <= 0.0f || y >= state.canvasHeight) {
		return;
	}

	const float clippedX = std::max(0.0f, x);
	const float clippedY = std::max(0.0f, y);
	const float clippedRight = std::min(state.canvasWidth, x + width);
	const float clippedBottom = std::min(state.canvasHeight, y + height);
	const float clippedW = clippedRight - clippedX;
	const float clippedH = clippedBottom - clippedY;
	if (clippedW <= 0.0f || clippedH <= 0.0f) return;

	Clay_ElementDeclaration primitive{};
	primitive.layout.sizing = {
		.width = CLAY_SIZING_FIXED(clippedW),
		.height = CLAY_SIZING_FIXED(clippedH),
	};
	primitive.backgroundColor = fill;
	primitive.cornerRadius = radius;
	primitive.border = {.color = border, .width = borderWidth};
	primitive.floating = previewFloating(clippedX, clippedY, zIndex);
	CLAY(context.clayID(ids.next()), primitive) {}
}

void drawPreviewLabel(
	DevPreviewCanvas::BuildContext& context,
	const DevPreviewState& state,
	IndexedElementIDSequence& ids,
	float x,
	float y,
	std::string_view label,
	Clay_Color color = interface_theme::kTextCanvas,
	Clay_Color background = interface_theme::kDepth3Elevated) {
	if (state.canvasWidth <= 0.0f || state.canvasHeight <= 0.0f) return;
	const float width = std::max(34.0f, static_cast<float>(label.size()) * 5.4f + 8.0f);
	const float clampedX = std::clamp(x, 0.0f, std::max(0.0f, state.canvasWidth - width));
	const float clampedY = std::clamp(y, 0.0f, std::max(0.0f, state.canvasHeight - 17.0f));

	Clay_ElementDeclaration badge{};
	badge.layout.sizing = {
		.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(17)};
	badge.layout.padding = Clay_Padding{4, 4, 0, 0};
	badge.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	badge.backgroundColor = background;
	badge.cornerRadius = CLAY_CORNER_RADIUS(3);
	badge.border = {
		.color = interface_theme::kBorderVisible,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	badge.floating = previewFloating(clampedX, clampedY, 0);
	CLAY(context.clayID(ids.next()), badge) {
		CLAY_TEXT(context.uiManager.toClayString(label),
			CLAY_TEXT_CONFIG(textConfig(color, 8)));
	}
}

void drawPreviewGrid(
	DevPreviewCanvas::BuildContext& context,
	DevPreviewState& state,
	IndexedElementIDSequence& ids) {
	if (state.canvasWidth <= 0.0f || state.canvasHeight <= 0.0f) return;
	float minorStep = 10.0f;
	float majorStep = 50.0f;
	if (minorStep * state.camera.zoomScale < 6.0f) minorStep = 50.0f;
	if (minorStep * state.camera.zoomScale < 6.0f) minorStep = 200.0f;
	if (majorStep < minorStep) majorStep = minorStep * 4.0f;
	const Clay_Vector2 worldMin = canvasPointerToWorld(state, 0.0f, 0.0f);
	const Clay_Vector2 worldMax = canvasPointerToWorld(
		state, state.canvasWidth, state.canvasHeight);
	const int firstX = static_cast<int>(std::floor(worldMin.x / minorStep));
	const int lastX = static_cast<int>(std::ceil(worldMax.x / minorStep));
	const int firstY = static_cast<int>(std::floor(worldMin.y / minorStep));
	const int lastY = static_cast<int>(std::ceil(worldMax.y / minorStep));
	const int maxLines = 160;
	for (int grid = firstX, count = 0; grid <= lastX && count < maxLines; ++grid, ++count) {
		const float world = static_cast<float>(grid) * minorStep;
		const float x = worldToCanvas(state, world, 0.0f).x;
		if (x < 0.0f || x > state.canvasWidth) continue;
		const bool axis = std::abs(world) < 0.01f;
		const bool major = std::fmod(std::abs(world), majorStep) < 0.01f;
		const Clay_Color color = axis
			? interface_theme::kAccentSignalBlue
			: major ? Flow_Color("#3F4D55A0") : Flow_Color("#26343B70");
		drawPreviewRect(context, state, ids, x, 0.0f, axis ? 1.5f : 1.0f,
			state.canvasHeight, color, {}, {}, {}, 0);
	}
	for (int grid = firstY, count = 0; grid <= lastY && count < maxLines; ++grid, ++count) {
		const float world = static_cast<float>(grid) * minorStep;
		const float y = worldToCanvas(state, 0.0f, world).y;
		if (y < 0.0f || y > state.canvasHeight) continue;
		const bool axis = std::abs(world) < 0.01f;
		const bool major = std::fmod(std::abs(world), majorStep) < 0.01f;
		const Clay_Color color = axis
			? interface_theme::kAccentSignalBlue
			: major ? Flow_Color("#3F4D55A0") : Flow_Color("#26343B70");
		drawPreviewRect(context, state, ids, 0.0f, y, state.canvasWidth,
			axis ? 1.5f : 1.0f, color, {}, {}, {}, 0);
	}
}

#if FLOW_UI_DEV_CAPTURE_CLAY
void drawCapturedSubtree(
	DevPreviewCanvas::BuildContext& context,
	const PreviewSelection& selection,
	DevPreviewState& state,
	IndexedElementIDSequence& ids) {
	const Clay_BoundingBox origin = selection.clay->bounds;
	const std::span<const tooling::DevClayNode> nodes =
		tooling::fullClaySubtree(*selection.snapshot, selection.flowIndex);
	for (const tooling::DevClayNode& node : nodes) {
		const float worldX = node.bounds.x - origin.x;
		const float worldY = node.bounds.y - origin.y;
		const Clay_Vector2 position = worldToCanvas(state, worldX, worldY);
		const float width = std::max(0.0f, node.bounds.width * state.camera.zoomScale);
		const float height = std::max(0.0f, node.bounds.height * state.camera.zoomScale);
		Clay_ElementDeclaration primitive{};
		primitive.layout.sizing = {
			.width = CLAY_SIZING_FIXED(width),
			.height = CLAY_SIZING_FIXED(height),
		};
		primitive.backgroundColor = node.declaration.backgroundColor;
		primitive.cornerRadius = Clay_CornerRadius{
			node.declaration.cornerRadius.topLeft * state.camera.zoomScale,
			node.declaration.cornerRadius.topRight * state.camera.zoomScale,
			node.declaration.cornerRadius.bottomLeft * state.camera.zoomScale,
			node.declaration.cornerRadius.bottomRight * state.camera.zoomScale,
		};
		primitive.border = {
			.color = node.declaration.border.color,
			.width = Clay_BorderWidth{
				scaledU16(node.declaration.border.width.left * state.camera.zoomScale),
				scaledU16(node.declaration.border.width.right * state.camera.zoomScale),
				scaledU16(node.declaration.border.width.top * state.camera.zoomScale),
				scaledU16(node.declaration.border.width.bottom * state.camera.zoomScale),
				0},
		};
		primitive.clip = {.horizontal = true, .vertical = true};
		primitive.floating = previewFloating(position.x, position.y, 0);
		if (node.pointerPresence.imageData && primitive.backgroundColor.a <= 0.0f) {
			primitive.backgroundColor = Flow_Color("#263D4A");
		}
		CLAY(context.clayID(ids.next()), primitive) {
			if (tooling::hasFlag(node.flags, tooling::DevClayNodeFlag::Text)) {
				Clay_TextElementConfig captured = node.textConfig;
				captured.fontSize = scaledU16(
					std::max(1.0f, node.textConfig.fontSize * state.camera.zoomScale));
				captured.letterSpacing = scaledU16(
					node.textConfig.letterSpacing * state.camera.zoomScale);
				CLAY_TEXT(context.uiManager.toClayString(selection.snapshot->string(node.text)),
					CLAY_TEXT_CONFIG(captured));
			} else if (node.pointerPresence.imageData) {
				CLAY_TEXT(context.uiManager.toClayString("IMAGE"),
					CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
			}
		}
	}
}

void drawPreviewSidecars(
	DevPreviewCanvas::BuildContext& context,
	const PreviewSelection& selection,
	DevPreviewState& state,
	IndexedElementIDSequence& ids) {
	const Clay_BoundingBox origin = selection.clay->bounds;
	const std::span<const tooling::DevClayNode> nodes =
		tooling::fullClaySubtree(*selection.snapshot, selection.flowIndex);
	const float zoom = state.camera.zoomScale;
	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::TreeHierarchy)) {
		for (const tooling::DevClayNode& node : nodes) {
			const Clay_Vector2 p = worldToCanvas(
				state, node.bounds.x - origin.x, node.bounds.y - origin.y);
			drawPreviewRect(context, state, ids, p.x, p.y,
				node.bounds.width * zoom, node.bounds.height * zoom, {},
				interface_theme::kAccentSeaGlass,
				Clay_BorderWidth{1, 1, 1, 1, 0}, {}, 0);
		}
	}
	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::BoxModel)) {
		const Clay_Vector2 p = worldToCanvas(state, 0.0f, 0.0f);
		const Clay_Padding padding = selection.clay->declaration.layout.padding;
		drawPreviewRect(context, state, ids, p.x, p.y,
			selection.clay->bounds.width * zoom,
			selection.clay->bounds.height * zoom,
			Flow_Color("#27AE6020"), interface_theme::kStatusAmber,
			Clay_BorderWidth{2, 2, 2, 2, 0}, {}, 0);
		const float innerX = p.x + padding.left * zoom;
		const float innerY = p.y + padding.top * zoom;
		drawPreviewRect(context, state, ids, innerX, innerY,
			std::max(0.0f, (selection.clay->bounds.width - padding.left - padding.right) * zoom),
			std::max(0.0f, (selection.clay->bounds.height - padding.top - padding.bottom) * zoom),
			Flow_Color("#2F80ED20"), interface_theme::kAccentSignalBlue,
			Clay_BorderWidth{1, 1, 1, 1, 0}, {}, 0);
	}
	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::Typography)) {
		for (const tooling::DevClayNode& node : nodes) {
			if (!tooling::hasFlag(node.flags, tooling::DevClayNodeFlag::Text)) continue;
			const Clay_Vector2 p = worldToCanvas(
				state, node.bounds.x - origin.x, node.bounds.y - origin.y);
			const float baseline = p.y +
				std::min(node.bounds.height, node.textConfig.fontSize * 0.8f) * zoom;
			drawPreviewRect(context, state, ids, p.x, baseline,
				node.bounds.width * zoom, 1.0f, Flow_Color("#56CCF2FF"), {}, {}, {}, 0);
		}
	}
	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::ScissorAndClip)) {
		for (const tooling::DevClayNode& node : nodes) {
			if (!node.declaration.clip.horizontal && !node.declaration.clip.vertical) continue;
			const Clay_Vector2 p = worldToCanvas(
				state, node.bounds.x - origin.x, node.bounds.y - origin.y);
			drawPreviewRect(context, state, ids, p.x, p.y,
				node.bounds.width * zoom, node.bounds.height * zoom, {},
				interface_theme::kStatusRed,
				Clay_BorderWidth{2, 2, 2, 2, 0}, {}, 0);
		}
	}
	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::RulersAndDistance)) {
		const Clay_Vector2 p = worldToCanvas(state, 0.0f, 0.0f);
		const float right = p.x + selection.clay->bounds.width * zoom;
		const float bottom = p.y + selection.clay->bounds.height * zoom;
		drawPreviewRect(context, state, ids, p.x, 0.0f, 1.0f, state.canvasHeight,
			Flow_Color("#56CCF280"), {}, {}, {}, 0);
		drawPreviewRect(context, state, ids, right, 0.0f, 1.0f, state.canvasHeight,
			Flow_Color("#56CCF280"), {}, {}, {}, 0);
		drawPreviewRect(context, state, ids, 0.0f, p.y, state.canvasWidth, 1.0f,
			Flow_Color("#56CCF280"), {}, {}, {}, 0);
		drawPreviewRect(context, state, ids, 0.0f, bottom, state.canvasWidth, 1.0f,
			Flow_Color("#56CCF280"), {}, {}, {}, 0);
		char dimensions[64]{};
		std::snprintf(dimensions, sizeof(dimensions), "%.1f x %.1f px",
			selection.clay->bounds.width, selection.clay->bounds.height);
		drawPreviewLabel(context, state, ids, p.x + 4.0f, p.y + 4.0f, dimensions,
			interface_theme::kAccentSeaGlass);
	}
	if (tooling::hasFlag(state.sidecarFlags,
		tooling::DevOverlayModeFlags::RenderRunDiagnostics)) {
		for (const tooling::DevClayNode& node : nodes) {
			const Clay_Vector2 p = worldToCanvas(
				state, node.bounds.x - origin.x, node.bounds.y - origin.y);
			drawPreviewRect(context, state, ids, p.x, p.y,
				node.bounds.width * zoom, node.bounds.height * zoom,
				Flow_Color("#00FF0012"), {}, {}, {}, 0);
		}
	}
}
#endif

void drawRulerTool(
	DevPreviewCanvas::BuildContext& context,
	DevPreviewState& state,
	IndexedElementIDSequence& ids) {
	if (!state.ruler.pointAValid) return;
	const Clay_Vector2 a = worldToCanvas(
		state, state.ruler.pointAX, state.ruler.pointAY);
	const Clay_Vector2 b = worldToCanvas(
		state, state.ruler.pointBX, state.ruler.pointBY);
	const float left = std::min(a.x, b.x);
	const float top = std::min(a.y, b.y);
	drawPreviewRect(context, state, ids, left, a.y, std::max(1.5f, std::abs(b.x - a.x)),
		1.5f, interface_theme::kAccentSeaGlass, {}, {}, {}, 0);
	drawPreviewRect(context, state, ids, b.x, top, 1.5f,
		std::max(1.5f, std::abs(b.y - a.y)),
		interface_theme::kAccentSeaGlass, {}, {}, {}, 0);
	drawPreviewRect(context, state, ids, a.x - 3.0f, a.y - 3.0f, 6.0f, 6.0f,
		interface_theme::kAccentSeaGlass, {}, {}, CLAY_CORNER_RADIUS(3), 0);
	drawPreviewRect(context, state, ids, b.x - 3.0f, b.y - 3.0f, 6.0f, 6.0f,
		interface_theme::kAccentSeaGlass, {}, {}, CLAY_CORNER_RADIUS(3), 0);
	const float dx = state.ruler.pointBX - state.ruler.pointAX;
	const float dy = state.ruler.pointBY - state.ruler.pointAY;
	char measurement[80]{};
	std::snprintf(measurement, sizeof(measurement),
		"%.1f px  dx %.1f  dy %.1f", std::hypot(dx, dy), dx, dy);
	drawPreviewLabel(context, state, ids, (a.x + b.x) * 0.5f,
		(a.y + b.y) * 0.5f - 20.0f, measurement,
		interface_theme::kAccentSeaGlass);
}

} // namespace

void DevPreviewCanvas::buildElement(BuildContext& context) {
	DevPreviewState* state = context.params.preview;
	Clay_ElementDeclaration canvas{};
	canvas.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	canvas.backgroundColor = Flow_Color("#0B1217");
	canvas.clip = {.horizontal = true, .vertical = true};
	if (!state) {
		CLAY(context.clayID(), canvas) {}
		return;
	}
	CLAY(context.clayID(), canvas) {
		const Clay_ElementData previous = Clay_GetElementData(context.clayID());
		if (previous.found) {
			state->canvasWidth = std::max(0.0f, previous.boundingBox.width);
			state->canvasHeight = std::max(0.0f, previous.boundingBox.height);
		}
		if (state->pendingAutoFit && state->canvasWidth > 0.0f &&
			state->canvasHeight > 0.0f) {
			fitPreviewCamera(*state);
			state->pendingAutoFit = false;
		}

#if FLOWUI_PUBLIC_VULKAN_INTEROP
		TextureRef texture{};
		if (context.params.inspect.app) {
			ViewPortManager& viewPorts = context.params.inspect.app->viewPorts(
				context.uiManager.windowId());
			if (!viewPorts.contains(kPreviewViewportKey)) {
				(void)viewPorts.create(kPreviewViewportKey, ViewPortCreateInfo{
					.clearColor = {0.043f, 0.071f, 0.090f, 1.0f},
					.clearEveryFrame = true,
				});
				if (ViewPort* viewport = viewPorts.getViewPort(kPreviewViewportKey)) {
					viewport->setRenderCallback([inspectParams = context.params.inspect, state](const ViewPortRenderContext& ctx) {
						const PreviewSelection selection = resolvePreviewSelection(inspectParams);
						const WindowId selectedWin = inspectParams.interfaceState ? inspectParams.interfaceState->selectedWindowId : InvalidWindowId;
						const DevUiReplaySource replay = (inspectParams.app && selectedWin != InvalidWindowId)
							? inspectParams.app->devUiReplaySource(selectedWin)
							: DevUiReplaySource{};
						recordDevPreviewViewPort(ctx, *state, selection, replay);
					});
				}
			}
			texture = viewPorts.getTexture(kPreviewViewportKey);
		}
		Clay_ElementDeclaration image{};
		image.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		texture.fitMode = TextureFitMode::Stretch;
		image.image.imageData = context.uiManager.imageData(texture);
		CLAY(context.clayID(kPreviewViewportImage), image) {}
#else
		Clay_ElementDeclaration fallback{};
		fallback.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		fallback.layout.padding = Clay_Padding{18, 18, 18, 18};
		fallback.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		fallback.layout.childGap = 5;
		fallback.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
		CLAY(context.clayID("interop-unavailable"), fallback) {
			CLAY_TEXT(context.uiManager.toClayString(
				"Unavailable: compile Vulkan interop"),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kStatusAmber, 11)));
			CLAY_TEXT(context.uiManager.toClayString(
				"Enable -DFLOWUI_PUBLIC_VULKAN_INTEROP=ON"),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
		}
#endif
	}
}

void DevPreview::buildElement(BuildContext& context) {
	DevPreviewState& state = context.state();
	const PreviewSelection selection = resolvePreviewSelection(context.params);
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (selection.clay) {
		state.elementWidth = std::max(0.0f, selection.clay->bounds.width);
		state.elementHeight = std::max(0.0f, selection.clay->bounds.height);
	}
#endif
	if (selection.flow && state.lastSelectedNodeKey != selection.flow->instance.value) {
		state.lastSelectedNodeKey = selection.flow->instance.value;
		clearRuler(state);
		state.pendingAutoFit = true;
	}
	synchronizeZoom(state);

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), root) {
		Clay_ElementDeclaration toolbar{};
		toolbar.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(36)};
		toolbar.layout.padding = Clay_Padding{6, 6, 6, 6};
		toolbar.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		toolbar.layout.childGap = 3;
		toolbar.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		toolbar.backgroundColor = interface_theme::kDepth1Panel;
		toolbar.border = {
			.color = interface_theme::kBorderPrimary,
			.width = Clay_BorderWidth{0, 0, 0, 1, 0},
		};
		toolbar.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(kPreviewToolbar), toolbar) {
			IndexedElementIDSequence controls{kPreviewControl};
			IndexedElementIDSequence separators{kPreviewSeparator};
			const Clay_ElementDeclaration group = previewToolbarGroup();
			CLAY(context.clayID(kPreviewCameraGroup), group) {
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ZoomOut, "-");
				Clay_ElementDeclaration readout{};
				readout.layout.sizing = {
					.width = CLAY_SIZING_FIXED(52), .height = CLAY_SIZING_FIXED(22)};
				readout.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				readout.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				readout.backgroundColor = interface_theme::kDepth2Ink;
				readout.cornerRadius = CLAY_CORNER_RADIUS(3);
				readout.clip = {
					.horizontal = true, .vertical = true, .scrollInputDisabled = true};
				CLAY(context.clayID(kPreviewZoomReadout), readout) {
					context.uiManager.createElement(FSEL::kNumberInputFloat, kPreviewZoom)
						.setParameters(FSEL::NumberInputParameters<float>{
							.value = &state.zoomPercent,
							.minimum = state.camera.minZoom * 100.0f,
							.maximum = state.camera.maxZoom * 100.0f,
							.step = 5.0f,
							.stepButtons = FSEL::NumberInputStepButtons::None,
							.maxBytes = 8,
							.placeholder = "100",
							.sizing = Clay_Sizing{
								.width = CLAY_SIZING_FIXED(40),
								.height = CLAY_SIZING_FIXED(22)},
							.viewportWidth = 32.0f,
							.viewportHeight = 20.0f,
							.padding = Clay_Padding{4, 2, 0, 0},
							})
						.setDevInternalCapture(true).draw();
					Clay_ElementDeclaration suffix{};
					suffix.layout.sizing = {
						.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)};
					suffix.layout.childAlignment = {
						.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
					CLAY(context.clayID(kPreviewZoomSuffix), suffix) {
						CLAY_TEXT(context.uiManager.toClayString("%"),
							CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
					}
				}
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ZoomIn, "+");
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::Fit, "Fit");
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::Reset, "1:1", false,
					context.resources().resetIcon);
			}

			drawPreviewToolbarSeparator(context, separators.next());
			CLAY(context.clayID(kPreviewSidecarGroup), group) {
				const auto enabled = [&state](tooling::DevOverlayModeFlags flag) {
					return tooling::hasFlag(state.sidecarFlags, flag);
				};
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ToggleBoxModel, "Box",
					enabled(tooling::DevOverlayModeFlags::BoxModel));
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ToggleRulers, "Rules",
					enabled(tooling::DevOverlayModeFlags::RulersAndDistance));
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ToggleTypography, "Type",
					enabled(tooling::DevOverlayModeFlags::Typography));
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ToggleHierarchy, "Tree",
					enabled(tooling::DevOverlayModeFlags::TreeHierarchy));
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ToggleClip, "Clip",
					enabled(tooling::DevOverlayModeFlags::ScissorAndClip));
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::ToggleDiagnostics, "Diag",
					enabled(tooling::DevOverlayModeFlags::RenderRunDiagnostics));
			}

			drawPreviewToolbarSeparator(context, separators.next());
			CLAY(context.clayID(kPreviewToolGroup), group) {
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::SelectPan, "Pan",
					state.activeTool == DevPreviewToolMode::PanInspect,
					context.resources().panIcon);
				drawPreviewControl(context, controls.next(), state,
					DevPreviewControlCommand::SelectRuler, "Ruler",
					state.activeTool == DevPreviewToolMode::Ruler,
					context.resources().rulerIcon);
			}
		}
		synchronizeZoom(state);
		context.uiManager.createElement(kDevPreviewCanvas, kPreviewCanvas)
			.setParameters(DevPreviewCanvasParameters{
				.inspect = context.params,
				.preview = &state,
#if FLOWUI_PUBLIC_VULKAN_INTEROP
				.viewportRenderer = context.resources().viewportRenderer.get(),
#endif
			})
			.setDevInternalCapture(true).draw();
	}
}

void DevOverviewAndPerformance::buildElement(BuildContext& context) {
	State& state = context.state();
	if (state.selectedPercentile != 50u && state.selectedPercentile != 90u &&
		state.selectedPercentile != 95u && state.selectedPercentile != 99u &&
		state.selectedPercentile != 100u) {
		state.selectedPercentile = 95u;
	}
	const OverviewTelemetry telemetry = collectOverviewTelemetry(context.params, state);
	if (!telemetry.flow) {
		drawStub(context, "Selected element telemetry is unavailable");
		return;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), root) {
		Clay_ElementDeclaration header{};
		header.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(34)};
		header.layout.padding = Clay_Padding{10, 10, 0, 0};
		header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		header.layout.childGap = 8;
		header.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		header.backgroundColor = interface_theme::kDepth2Ink;
		header.border = {
			.color = interface_theme::kBorderPrimary,
			.width = Clay_BorderWidth{0, 0, 0, 1, 0},
		};
		CLAY(context.clayID(kOverviewHeader), header) {
			CLAY_TEXT(context.uiManager.toClayString("OVERVIEW & PERFORMANCE"),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 10)));
			CLAY(context.clayID("header-spacer"), {
				.layout = {.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0)}}}) {}
			CLAY_TEXT(context.uiManager.toClayString(
				telemetry.timingAvailable ? "● LIVE SAMPLES" : "○ CAPTURE LIMITED"),
				CLAY_TEXT_CONFIG(textConfig(
					telemetry.timingAvailable
						? interface_theme::kStatusGreen : interface_theme::kStatusAmber,
					8)));
		}

		Clay_ElementDeclaration body{};
		body.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		body.layout.padding = Clay_Padding{8, 8, 8, 8};
		body.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		body.layout.childGap = 7;
		body.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(kOverviewBody), body) {
			IndexedElementIDSequence identityMetrics{kOverviewMetric};
			CLAY(context.clayID(kOverviewIdentity), overviewPanel()) {
				drawOverviewPanelTitle(context, "ELEMENT IDENTITY & ANCESTRY");
				drawOverviewMetric(context, identityMetrics.next(), "Definition",
					telemetry.definition + "  /  " + telemetry.definitionId,
					interface_theme::kAccentSeaGlass);
				drawOverviewMetric(context, identityMetrics.next(), "Nodes",
					"Flow " + telemetry.flowNode + "  /  " + telemetry.clayNode);
				drawOverviewMetric(context, identityMetrics.next(), "Source", telemetry.source);
				drawOverviewMetric(context, identityMetrics.next(), "Path", telemetry.path,
					interface_theme::kTextSecondary);
			}

			CLAY(context.clayID(kOverviewStructure), overviewPanel()) {
				drawOverviewPanelTitle(context, "TREE STRUCTURE & SOLVED GEOMETRY");
				Clay_ElementDeclaration columns{};
				columns.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				columns.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				columns.layout.childGap = 10;
				IndexedElementIDSequence columnIds{kOverviewColumn};
				IndexedElementIDSequence metrics{kOverviewMetric};
				CLAY(context.clayID("structure-columns"), columns) {
					Clay_ElementDeclaration column{};
					column.layout.sizing = {
						.width = CLAY_SIZING_PERCENT(0.5f), .height = CLAY_SIZING_FIT(0)};
					column.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
					column.layout.childGap = 2;
					column.clip = {.horizontal = true, .vertical = true};
					CLAY(context.clayID(columnIds.next()), column) {
						drawOverviewMetric(context, metrics.next(), "Flow children",
							telemetry.flowChildren + "  / subtree " + telemetry.flowSubtree);
						drawOverviewMetric(context, metrics.next(), "Clay children",
							telemetry.clayChildren + "  / subtree " + telemetry.claySubtree);
						drawOverviewMetric(context, metrics.next(), "Depth", telemetry.depth);
						drawOverviewMetric(context, metrics.next(), "Z / paint", telemetry.paint);
						drawOverviewMetric(context, metrics.next(), "Flags", telemetry.flags,
							interface_theme::kStatusGreen);
					}
					CLAY(context.clayID(columnIds.next()), column) {
						drawOverviewMetric(context, metrics.next(), "Position", telemetry.position);
						drawOverviewMetric(context, metrics.next(), "Dimensions", telemetry.dimensions);
						drawOverviewMetric(context, metrics.next(), "Layout", telemetry.layout);
						drawOverviewMetric(context, metrics.next(), "Padding", telemetry.padding);
						drawOverviewMetric(context, metrics.next(), "Sizing / clip",
							telemetry.sizing + "  /  " + telemetry.clip);
					}
				}
			}

			CLAY(context.clayID(kOverviewPerformance), overviewPanel()) {
				Clay_ElementDeclaration perfHeader{};
				perfHeader.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				perfHeader.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				perfHeader.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				perfHeader.clip = {.horizontal = true, .vertical = true};

				CLAY(context.clayID("perf-header-row"), perfHeader) {
					drawOverviewPanelTitle(context, "ELEMENT & DEFINITION TIMING");
					CLAY(context.clayID("perf-header-spacer"), {
						.layout = {.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_GROW(0)}}}) {}
					CLAY_TEXT(context.uiManager.toClayString(telemetry.timingLevelBadge),
						CLAY_TEXT_CONFIG(textConfig(interface_theme::kAccentCurrent, 8)));
				}

				CLAY_TEXT(context.uiManager.toClayString(telemetry.frameContextSummary),
					CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextSecondary, 8)));

				Clay_ElementDeclaration selector{};
				selector.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(20)};
				selector.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				selector.layout.childGap = 3;
				selector.clip = {.horizontal = true, .vertical = true};
				IndexedElementIDSequence choices{kOverviewPercentile};
				const Clay_SizingAxis choice5Sizing = CLAY_SIZING_PERCENT(0.2f);
				CLAY(context.clayID("percentile-selector"), selector) {
					drawPercentileChoice(context, choices.next(), state, 50u, "P50", choice5Sizing);
					drawPercentileChoice(context, choices.next(), state, 90u, "P90", choice5Sizing);
					drawPercentileChoice(context, choices.next(), state, 95u, "P95", choice5Sizing);
					drawPercentileChoice(context, choices.next(), state, 99u, "P99", choice5Sizing);
					drawPercentileChoice(context, choices.next(), state, 100u, "MAX", choice5Sizing);
				}

				IndexedElementIDSequence metrics{kOverviewMetric};
				const std::string percentileLabel = state.selectedPercentile == 100u
					? "Max" : "P" + std::to_string(state.selectedPercentile);

				Clay_ElementDeclaration instanceRow{};
				instanceRow.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				instanceRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				instanceRow.layout.childGap = 8;
				instanceRow.clip = {.horizontal = true, .vertical = true};
				const Clay_SizingAxis col2Sizing = CLAY_SIZING_PERCENT(0.5f);

				CLAY(context.clayID("instance-hero-row"), instanceRow) {
					drawOverviewMetric(context, metrics.next(), "Instance " + percentileLabel + " (Self)",
						telemetry.instanceTiming, interface_theme::kAccentSeaGlass, col2Sizing);
					drawOverviewMetric(context, metrics.next(), "Subtree " + percentileLabel + " (Total)",
						telemetry.subtreeTiming, interface_theme::kTextCanvas, col2Sizing);
				}

				Clay_ElementDeclaration definitionRow = instanceRow;
				const Clay_SizingAxis col3Sizing = CLAY_SIZING_PERCENT(1.0f / 3.0f);

				CLAY(context.clayID("definition-grid-row"), definitionRow) {
					drawOverviewMetric(context, metrics.next(), "Definition " + percentileLabel + " Avg",
						telemetry.definitionTiming, interface_theme::kTextCanvas, col3Sizing);
					drawOverviewMetric(context, metrics.next(), "Definition Peak",
						telemetry.definitionPeak, interface_theme::kTextCanvas, col3Sizing);
					drawOverviewMetric(context, metrics.next(), "Calls in window",
						std::to_string(telemetry.invocationCount) + " calls",
						interface_theme::kTextCanvas, col3Sizing);
				}
			}

			CLAY(context.clayID(kOverviewMemory), overviewPanel()) {
				drawOverviewPanelTitle(context, "MEMORY ATTRIBUTION");
				Clay_ElementDeclaration memoryRow{};
				memoryRow.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				memoryRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				memoryRow.layout.childGap = 8;
				memoryRow.clip = {.horizontal = true, .vertical = true};
				IndexedElementIDSequence metrics{kOverviewMetric};
				const Clay_SizingAxis col3Sizing = CLAY_SIZING_PERCENT(1.0f / 3.0f);
				CLAY(context.clayID("memory-row-local"), memoryRow) {
					drawOverviewMetric(context, metrics.next(), "Instance structures",
						telemetry.instanceMemory, interface_theme::kTextCanvas, col3Sizing);
					drawOverviewMetric(context, metrics.next(), "Subtree strings",
						telemetry.stringMemory, interface_theme::kTextCanvas, col3Sizing);
					drawOverviewMetric(context, metrics.next(), "Definition schema",
						telemetry.schemaMemory, interface_theme::kTextCanvas, col3Sizing);
				}
				CLAY(context.clayID("memory-row-system"), memoryRow) {
					drawOverviewMetric(context, metrics.next(), "FlowUi CPU live",
						telemetry.engineMemory, interface_theme::kAccentSeaGlass, col3Sizing);
					drawOverviewMetric(context, metrics.next(), "Process RSS",
						telemetry.residentMemory, interface_theme::kTextCanvas, col3Sizing);
					drawOverviewMetric(context, metrics.next(), "GPU allocator",
						telemetry.gpuMemory, interface_theme::kTextCanvas, col3Sizing);
				}
			}
		}
	}
}

void DevChangesAndDiagnostics::buildElement(BuildContext& context) {
	State& state = context.state();
	if (state.selectedIssueFilter > 3u) state.selectedIssueFilter = 0u;
	const DevChangeAndIssuePanelData data = queryChangeAndIssueData(context.params);
	const uint32_t instanceChanges = static_cast<uint32_t>(std::ranges::count_if(
		data.activeChanges,
		[](const DevMinimalChangeItem& item) {
			return item.scope == tooling::DevOverrideScope::ExactInstance;
		}));
	const uint32_t definitionChanges =
		static_cast<uint32_t>(data.activeChanges.size()) - instanceChanges;
	const auto issueCount = [&data](DevResolvedIssueSeverity severity) {
		return static_cast<uint32_t>(std::ranges::count_if(
			data.resolvedIssues,
			[severity](const DevResolvedElementIssue& issue) {
				return issue.severity == severity;
			}));
	};
	const uint32_t errorCount = issueCount(DevResolvedIssueSeverity::Error);
	const uint32_t warningCount = issueCount(DevResolvedIssueSeverity::Warning);
	const uint32_t adviceCount = issueCount(DevResolvedIssueSeverity::Advice);
	std::vector<const DevResolvedElementIssue*> visibleIssues{};
	visibleIssues.reserve(data.resolvedIssues.size());
	for (const DevResolvedElementIssue& issue : data.resolvedIssues) {
		if (issueMatchesFilter(issue, state.selectedIssueFilter)) {
			visibleIssues.push_back(&issue);
		}
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), root) {
		Clay_ElementDeclaration header{};
		header.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(34)};
		header.layout.padding = Clay_Padding{10, 10, 0, 0};
		header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		header.layout.childGap = 8;
		header.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		header.backgroundColor = interface_theme::kDepth2Ink;
		header.border = {
			.color = interface_theme::kBorderPrimary,
			.width = Clay_BorderWidth{0, 0, 0, 1, 0},
		};
		CLAY(context.clayID(kChangesHeader), header) {
			CLAY_TEXT(context.uiManager.toClayString("CHANGES & ISSUES"),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 10)));
			CLAY(context.clayID("changes-header-spacer"), {
				.layout = {.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0)}}}) {}
			const std::string status = data.resolvedIssues.empty()
				? "● NO ELEMENT ISSUES"
				: std::to_string(data.resolvedIssues.size()) + " ISSUE" +
					(data.resolvedIssues.size() == 1u ? "" : "S");
			CLAY_TEXT(context.uiManager.toClayString(status),
				CLAY_TEXT_CONFIG(textConfig(
					data.resolvedIssues.empty()
						? interface_theme::kStatusGreen : interface_theme::kStatusAmber,
					8)));
		}

		Clay_ElementDeclaration body{};
		body.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		body.layout.padding = Clay_Padding{8, 8, 8, 8};
		body.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		body.layout.childGap = 7;
		body.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(kChangesBody), body) {
			Clay_ElementDeclaration changes{};
			changes.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(124)};
			changes.layout.padding = Clay_Padding{8, 8, 7, 7};
			changes.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			changes.layout.childGap = 3;
			changes.backgroundColor = interface_theme::kDepth2Ink;
			changes.border = {
				.color = interface_theme::kBorderPrimary,
				.width = Clay_BorderWidth{1, 1, 1, 1, 0},
			};
			changes.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID(kChangesSummary), changes) {
				Clay_ElementDeclaration summaryRow{};
				summaryRow.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(14)};
				summaryRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				summaryRow.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				CLAY(context.clayID("change-summary-title"), summaryRow) {
					CLAY_TEXT(context.uiManager.toClayString("ACTIVE CHANGES · READ ONLY"),
						CLAY_TEXT_CONFIG(textConfig(interface_theme::kAccentSeaGlass, 8)));
					CLAY(context.clayID("change-summary-spacer"), {
						.layout = {.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_GROW(0)}}}) {}
					const std::string totals = std::to_string(data.activeChanges.size()) +
						" total  ·  " + std::to_string(instanceChanges) + " inst  ·  " +
						std::to_string(definitionChanges) + " def";
					CLAY_TEXT(context.uiManager.toClayString(totals),
						CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
				}

				Clay_ElementDeclaration tableRow{};
				tableRow.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(19)};
				tableRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				tableRow.layout.childGap = 6;
				tableRow.layout.padding = Clay_Padding{5, 5, 0, 0};
				tableRow.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				tableRow.backgroundColor = interface_theme::kDepth3Elevated;
				tableRow.clip = {.horizontal = true, .vertical = true};
				CLAY(context.clayID("change-table-header"), tableRow) {
					CLAY_TEXT(context.uiManager.toClayString("FIELD PATH"),
						CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
					CLAY(context.clayID("field-header-space"), {
						.layout = {.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_GROW(0)}}}) {}
					CLAY_TEXT(context.uiManager.toClayString("WINNING VALUE        SCOPE"),
						CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
				}
				if (data.activeChanges.empty()) {
					Clay_ElementDeclaration empty = tableRow;
					empty.backgroundColor = interface_theme::kDepth2Ink;
					empty.layout.childAlignment = {
						.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
					CLAY(context.clayID("no-active-changes"), empty) {
						CLAY_TEXT(context.uiManager.toClayString(
							"No active overrides affect this element"),
							CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
					}
				} else {
					IndexedElementIDSequence rows{kChangesRow};
					const std::size_t visibleChangeCount =
						std::min<std::size_t>(3u, data.activeChanges.size());
					for (std::size_t index = 0u; index < visibleChangeCount; ++index) {
						const DevMinimalChangeItem& item = data.activeChanges[index];
						Clay_ElementDeclaration row = tableRow;
						row.backgroundColor = index % 2u == 0u
							? interface_theme::kDepth1Panel : interface_theme::kDepth2Ink;
						CLAY(context.clayID(rows.next()), row) {
							CLAY_TEXT(context.uiManager.toClayString(item.fieldPath),
								CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 8)));
							CLAY(context.clayID(rows.next()), {
								.layout = {.sizing = {
									.width = CLAY_SIZING_GROW(0),
									.height = CLAY_SIZING_GROW(0)}}}) {}
							CLAY_TEXT(context.uiManager.toClayString(
								compactMiddle(item.valueSummary, 28u)),
								CLAY_TEXT_CONFIG(textConfig(interface_theme::kAccentSeaGlass, 8)));
							CLAY_TEXT(context.uiManager.toClayString(
								item.scope == tooling::DevOverrideScope::ExactInstance
									? "[INST]" : "[DEF]"),
								CLAY_TEXT_CONFIG(textConfig(
									item.scope == tooling::DevOverrideScope::ExactInstance
										? interface_theme::kAccentSignalCoral
										: interface_theme::kAccentSeaGlass,
									8)));
						}
					}
				}
			}

			Clay_ElementDeclaration issues{};
			issues.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			issues.layout.padding = Clay_Padding{8, 8, 7, 7};
			issues.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			issues.layout.childGap = 5;
			issues.backgroundColor = interface_theme::kDepth1Panel;
			issues.border = {
				.color = interface_theme::kBorderPrimary,
				.width = Clay_BorderWidth{1, 1, 1, 1, 0},
			};
			issues.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID(kIssuesConsole), issues) {
				CLAY_TEXT(context.uiManager.toClayString("DIAGNOSTICS & ISSUES CONSOLE"),
					CLAY_TEXT_CONFIG(textConfig(interface_theme::kAccentSeaGlass, 8)));
				Clay_ElementDeclaration filters{};
				filters.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(20)};
				filters.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				filters.layout.childGap = 3;
				IndexedElementIDSequence filterIds{kIssueFilter};
				CLAY(context.clayID("issue-filters"), filters) {
					drawIssueFilterChoice(context, filterIds.next(), state, 0u, "ALL",
						static_cast<uint32_t>(data.resolvedIssues.size()));
					drawIssueFilterChoice(context, filterIds.next(), state, 1u, "ERRORS",
						errorCount);
					drawIssueFilterChoice(context, filterIds.next(), state, 2u, "WARNINGS",
						warningCount);
					drawIssueFilterChoice(context, filterIds.next(), state, 3u, "ADVICE",
						adviceCount);
				}
				if (visibleIssues.empty()) {
					Clay_ElementDeclaration empty{};
					empty.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
					empty.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
					empty.layout.childGap = 4;
					empty.layout.childAlignment = {
						.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
					CLAY(context.clayID("no-element-issues"), empty) {
						CLAY_TEXT(context.uiManager.toClayString("● No matching issues"),
							CLAY_TEXT_CONFIG(textConfig(interface_theme::kStatusGreen, 10)));
						CLAY_TEXT(context.uiManager.toClayString(
							"Global diagnostics are intentionally filtered out"),
							CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
					}
				} else {
					IndexedElementIDSequence cards{kIssueCard};
					const std::size_t visibleCardCount =
						std::min<std::size_t>(2u, visibleIssues.size());
					for (std::size_t index = 0u; index < visibleCardCount; ++index) {
						drawIssueCard(context, cards.next(), *visibleIssues[index]);
					}
					if (visibleIssues.size() > visibleCardCount) {
						const std::string remaining = "+ " +
							std::to_string(visibleIssues.size() - visibleCardCount) +
							" more matching issues in the Diagnostics workspace";
						CLAY_TEXT(context.uiManager.toClayString(remaining),
							CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
					}
				}
			}
		}
	}
}

void DevClaySubTree::buildElement(BuildContext& context) {
	drawStub(context, "Clay Sub Tree");
}

void DevWorkbenchCard::buildElement(BuildContext& context) {
	Clay_ElementDeclaration card{};
	card.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	card.backgroundColor = interface_theme::kDepth1Panel;
	card.cornerRadius = CLAY_CORNER_RADIUS(5);
	card.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	card.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), card) {
		switch (context.params.section) {
			case DevWorkbenchSection::Preview:
				context.uiManager.createElement(kDevPreview, kPreview)
					.setParameters(context.params.inspect)
					.setDevInternalCapture(true).draw();
				break;
			case DevWorkbenchSection::OverviewAndPerformance:
				context.uiManager.createElement(
					kDevOverviewAndPerformance, kOverviewAndPerformance)
					.setParameters(context.params.inspect)
					.setDevInternalCapture(true).draw();
				break;
			case DevWorkbenchSection::ChangesAndDiagnostics:
				context.uiManager.createElement(
					kDevChangesAndDiagnostics, kChangesAndDiagnostics)
					.setParameters(context.params.inspect)
					.setDevInternalCapture(true).draw();
				break;
			case DevWorkbenchSection::ClaySubTree:
				context.uiManager.createElement(kDevClaySubTree, kClaySubTree)
					.setParameters(context.params.inspect)
					.setDevInternalCapture(true).draw();
				break;
		}
	}
}

void DevInspectWorkbenchContent::buildElement(BuildContext& context) {
	Clay_ElementDeclaration content{};
	content.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	content.layout.padding = Clay_Padding{
		kWorkbenchPadding, kWorkbenchPadding, kWorkbenchPadding, kWorkbenchPadding};
	content.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	content.layout.childGap = kWorkbenchGap;
	content.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), content) {
		if (!context.params.hasSelectedFlowElement) {
			Clay_ElementDeclaration empty{};
			empty.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			empty.layout.childAlignment = {
				.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("empty-selection"), empty) {
				CLAY_TEXT(context.uiManager.toClayString(
					"Select a Flow element to open its workbench"),
					CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 12)));
			}
		} else {
			Clay_ElementDeclaration row{};
			row.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			row.layout.childGap = kWorkbenchGap;

			CLAY(context.clayID(kTopRow), row) {
				drawWorkbenchCard(context, kPreview, DevWorkbenchSection::Preview);
				drawWorkbenchCard(context, kOverviewAndPerformance,
					DevWorkbenchSection::OverviewAndPerformance);
			}
			CLAY(context.clayID(kBottomRow), row) {
				drawWorkbenchCard(context, kChangesAndDiagnostics,
					DevWorkbenchSection::ChangesAndDiagnostics);
				drawWorkbenchCard(context, kClaySubTree, DevWorkbenchSection::ClaySubTree);
			}
		}
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
			.setParameters(DevInspectWorkbenchContentParameters{
				.inspect = context.params,
				.hasSelectedFlowElement = selected != nullptr,
			})
			.setDevInternalCapture(true).draw();
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
