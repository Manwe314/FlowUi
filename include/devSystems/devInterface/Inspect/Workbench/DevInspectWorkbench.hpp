#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "devSystems/devTooling/overlay/DevOverlayTypes.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems { class DevPreviewViewPortRenderer; }

namespace FlowUi::devSystems::interface_elements {

struct DevInspectWorkbenchHeaderParameters {
	std::string_view instanceName{};
	bool constructed = false;
	bool drawn = false;
	uint32_t directClayNodeCount = 0u;
	uint32_t overrideCount = 0u;
};

struct DevInspectWorkbenchHeader {
	using Parameters = DevInspectWorkbenchHeaderParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbenchHeader>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.header");
	static constexpr std::string_view debugName = "Inspect Workbench Header";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInspectWorkbenchSubtitleParameters {
	std::string_view definitionName{};
	std::string_view instancePath{};
};

struct DevInspectWorkbenchSubtitle {
	using Parameters = DevInspectWorkbenchSubtitleParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbenchSubtitle>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.subtitle");
	static constexpr std::string_view debugName = "Inspect Workbench Subtitle";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

enum class DevWorkbenchSection : uint8_t {
	Preview,
	OverviewAndPerformance,
	ChangesAndDiagnostics,
	ClaySubTree,
};

struct DevPreviewCamera {
	float panX = 0.0f;
	float panY = 0.0f;
	float zoomScale = 1.0f;
	float minZoom = 0.1f;
	float maxZoom = 32.0f;
};

enum class DevPreviewToolMode : uint8_t {
	PanInspect,
	Ruler,
};

struct DevRulerState {
	bool pointAValid = false;
	bool pointBValid = false;
	float pointAX = 0.0f;
	float pointAY = 0.0f;
	float pointBX = 0.0f;
	float pointBY = 0.0f;
};

struct DevPreviewState {
	DevPreviewCamera camera{};
	tooling::DevOverlayModeFlags sidecarFlags =
		tooling::DevOverlayModeFlags::Default;
	DevPreviewToolMode activeTool = DevPreviewToolMode::PanInspect;
	DevRulerState ruler{};
	float zoomPercent = 100.0f;
	bool isDraggingPan = false;
	float dragStartMouseX = 0.0f;
	float dragStartMouseY = 0.0f;
	float dragStartPanX = 0.0f;
	float dragStartPanY = 0.0f;
	float canvasWidth = 0.0f;
	float canvasHeight = 0.0f;
	float elementWidth = 0.0f;
	float elementHeight = 0.0f;
	uint64_t lastSelectedNodeKey = 0u;
	bool pendingAutoFit = true;
};

struct PreviewSelection {
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevFlowNode* flow = nullptr;
	tooling::DevFlowNodeIndex flowIndex = tooling::InvalidFlowNode;
#if FLOW_UI_DEV_CAPTURE_CLAY
	const tooling::DevClayNode* clay = nullptr;
#endif

	[[nodiscard]] explicit operator bool() const noexcept {
#if FLOW_UI_DEV_CAPTURE_CLAY
		return snapshot && flow && clay;
#else
		return false;
#endif
	}
};

enum class DevPreviewControlCommand : uint8_t {
	ZoomOut,
	ZoomIn,
	Fit,
	Reset,
	ToggleBoxModel,
	ToggleRulers,
	ToggleTypography,
	ToggleHierarchy,
	ToggleClip,
	ToggleDiagnostics,
	SelectPan,
	SelectRuler,
};

struct DevPreviewControlParameters {
	DevPreviewState* preview = nullptr;
	DevPreviewControlCommand command = DevPreviewControlCommand::Fit;
	std::string_view label{};
	TextureRef icon{};
	bool active = false;
};

struct DevPreviewControl {
	using Parameters = DevPreviewControlParameters;
	using BuildContext = ElementBuildContext<DevPreviewControl>;
	using InteractionContext = ElementInteractionContext<DevPreviewControl>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.preview.control");
	static constexpr std::string_view debugName = "Preview Control";
	static constexpr bool isDevInternal = true;

	static void onHovered(InteractionContext& context);
	static void onPressed(InteractionContext& context);
	static void buildElement(BuildContext& context);
};

struct DevPreviewCanvasParameters {
	DevInspectContentParameters inspect{};
	DevPreviewState* preview = nullptr;
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	DevPreviewViewPortRenderer* viewportRenderer = nullptr;
#endif
};

struct DevPreviewCanvas {
	using Parameters = DevPreviewCanvasParameters;
	using BuildContext = ElementBuildContext<DevPreviewCanvas>;
	using InteractionContext = ElementInteractionContext<DevPreviewCanvas>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.preview.canvas");
	static constexpr std::string_view debugName = "Preview Canvas";
	static constexpr bool isDevInternal = true;

	static void onHovered(InteractionContext& context);
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};

struct DevPreview {
	using Parameters = DevInspectContentParameters;
	using State = DevPreviewState;
	struct Resources {
		TextureRef panIcon{};
		TextureRef resetIcon{};
		TextureRef rulerIcon{};
#if FLOWUI_PUBLIC_VULKAN_INTEROP
		std::shared_ptr<DevPreviewViewPortRenderer> viewportRenderer{};
#endif

		Resources() = default;
		explicit Resources(App& app);
	};
	using BuildContext = ElementBuildContext<DevPreview>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.preview");
	static constexpr std::string_view debugName = "Preview";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

struct DevOverviewAndPerformance {
	using Parameters = DevInspectContentParameters;
	struct State {
		std::array<double, 60> frameTimeMs{};
		std::size_t frameSampleCount = 0u;
		std::size_t nextFrameSample = 0u;
		double frameTimeEmaMs = 0.0;
		uint64_t lastFrameNumber = 0u;
		uint64_t selectedPercentile = 95u;
	};
	using BuildContext = ElementBuildContext<DevOverviewAndPerformance>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.overview_and_performance");
	static constexpr std::string_view debugName = "Overview And Performance";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

/** Severity used by the selected-element issues console. */
enum class DevResolvedIssueSeverity : uint8_t {
	Error,
	Warning,
	Advice,
};

/** Read-only summary of the winning override for one field. */
struct DevMinimalChangeItem {
	devMode::DevFieldId fieldId = 0u;
	std::string fieldPath{};
	std::string valueSummary{};
	tooling::DevOverrideScope scope = tooling::DevOverrideScope::Definition;
};

/** Diagnostic or error that was resolved to the selected element. */
struct DevResolvedElementIssue {
	uint64_t issueId = 0u;
	std::string title{};
	std::string message{};
	std::string source{};
	std::string target{};
	std::string suggestedAction{};
	tooling::DevOverrideScope scope = tooling::DevOverrideScope::ExactInstance;
	DevResolvedIssueSeverity severity = DevResolvedIssueSeverity::Warning;
};

/** Snapshot consumed by the Changes & Issues workbench card. */
struct DevChangeAndIssuePanelData {
	std::vector<DevMinimalChangeItem> activeChanges{};
	std::vector<DevResolvedElementIssue> resolvedIssues{};
};

struct DevChangesAndDiagnostics {
	using Parameters = DevInspectContentParameters;
	struct State {
		// 0 = All, 1 = Errors, 2 = Warnings, 3 = Advice.
		uint64_t selectedIssueFilter = 0u;
	};
	using BuildContext = ElementBuildContext<DevChangesAndDiagnostics>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.changes_and_diagnostics");
	static constexpr std::string_view debugName = "Changes And Diagnostics";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

struct DevClaySubTree {
	using Parameters = DevInspectContentParameters;
	using BuildContext = ElementBuildContext<DevClaySubTree>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.clay_sub_tree");
	static constexpr std::string_view debugName = "Clay Sub Tree";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevWorkbenchCardParameters {
	DevInspectContentParameters inspect{};
	DevWorkbenchSection section = DevWorkbenchSection::Preview;
};

struct DevWorkbenchCard {
	using Parameters = DevWorkbenchCardParameters;
	using BuildContext = ElementBuildContext<DevWorkbenchCard>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.card");
	static constexpr std::string_view debugName = "Workbench Card";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInspectWorkbenchContentParameters {
	DevInspectContentParameters inspect{};
	bool hasSelectedFlowElement = false;
};

struct DevInspectWorkbenchContent {
	using Parameters = DevInspectWorkbenchContentParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbenchContent>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.content");
	static constexpr std::string_view debugName = "Inspect Workbench Content";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInspectWorkbench {
	using Parameters = DevInspectContentParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench");
	static constexpr std::string_view debugName = "Inspect Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInspectWorkbenchHeader kDevInspectWorkbenchHeader{};
inline constexpr DevInspectWorkbenchSubtitle kDevInspectWorkbenchSubtitle{};
inline constexpr DevPreviewControl kDevPreviewControl{};
inline constexpr DevPreviewCanvas kDevPreviewCanvas{};
inline constexpr DevPreview kDevPreview{};
inline constexpr DevOverviewAndPerformance kDevOverviewAndPerformance{};
inline constexpr DevChangesAndDiagnostics kDevChangesAndDiagnostics{};
inline constexpr DevClaySubTree kDevClaySubTree{};
inline constexpr DevWorkbenchCard kDevWorkbenchCard{};
inline constexpr DevInspectWorkbenchContent kDevInspectWorkbenchContent{};
inline constexpr DevInspectWorkbench kDevInspectWorkbench{};
static_assert(DrawableFlowElement<DevInspectWorkbenchHeader>);
static_assert(DrawableFlowElement<DevInspectWorkbenchSubtitle>);
static_assert(DrawableFlowElement<DevPreviewControl>);
static_assert(DrawableFlowElement<DevPreviewCanvas>);
static_assert(DrawableFlowElement<DevPreview>);
static_assert(DrawableFlowElement<DevOverviewAndPerformance>);
static_assert(DrawableFlowElement<DevChangesAndDiagnostics>);
static_assert(DrawableFlowElement<DevClaySubTree>);
static_assert(DrawableFlowElement<DevWorkbenchCard>);
static_assert(DrawableFlowElement<DevInspectWorkbenchContent>);
static_assert(DrawableFlowElement<DevInspectWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
