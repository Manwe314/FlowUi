#include "devSystems/devInterface/Permanents/Elements/DevContentHeader.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceCapturePolicy.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceExport.hpp"
#include "FlowUi/Public/Resources.hpp"
#include <limits>
#include <string>
#include <utility>

#include "FSEL/Button.hpp"
#include "FSEL/ComboBox.hpp"
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Changes/DevChangesControls.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsControls.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryContextControls.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInspectSelection.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevActionButton.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInspectOverlayMenu.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

constexpr float kContentHeaderHeight = 40.0f;
inline constexpr LocalElementName kTabStrip{"tabs"};
inline constexpr LocalElementName kInspectTab{"tab-inspect"};
inline constexpr LocalElementName kPerformanceTab{"tab-performance"};
inline constexpr LocalElementName kMemoryTab{"tab-memory"};
inline constexpr LocalElementName kDiagnosticsTab{"tab-diagnostics"};
inline constexpr LocalElementName kChangesTab{"tab-changes"};
inline constexpr LocalElementName kCatalogueTab{"tab-catalogue"};
inline constexpr LocalElementName kSectionSeparatorInset{"section-separator-inset"};
inline constexpr LocalElementName kSectionSeparatorLine{"section-separator-line"};
inline constexpr LocalElementName kContextualControls{"contextual-controls"};
inline constexpr LocalElementName kFrameSelector{"frame-selector"};
inline constexpr LocalElementName kCpuReportingLevel{"cpu-reporting-level"};
inline constexpr LocalElementName kWindowTarget{"window-target"};
inline constexpr LocalElementName kCapturedState{"captured-state"};
inline constexpr LocalElementName kCaptureState{"capture-state"};

struct TabSpec {
	DevInterfaceTab tab;
	LocalElementName id;
	std::string_view label;
};

inline constexpr std::array<TabSpec, 6> kTabs{{
	{DevInterfaceTab::Inspect, kInspectTab, "Inspect"},
	{DevInterfaceTab::Performance, kPerformanceTab, "Performance"},
	{DevInterfaceTab::Memory, kMemoryTab, "Memory"},
	{DevInterfaceTab::Diagnostics, kDiagnosticsTab, "Diagnostics"},
	{DevInterfaceTab::Changes, kChangesTab, "Changes"},
	{DevInterfaceTab::Catalogue, kCatalogueTab, "Catalogue"},
}};

constexpr auto kExportCapture =
	UiAction("flowui.dev_interface.performance.export", [](App& app, DevInterfaceState& state) {
		const auto generation = state.performance_timeline.capture_generation;
		if (generation && state.performance_export_generation == generation)
			return;
		const auto result =
			export_performance_capture_csv(app.devMonitoring().timingReporting(), generation,
										   app.devMonitoring().performance_capture().status());
		if (result) {
			state.performance_export_generation = generation;
			state.performance_export_path = path_to_utf8(*result);
			state.lastActionMessage = "Exported capture to " + state.performance_export_path;
		} else {
			state.lastActionMessage =
				"Export failed: the capture must be sealed and the executable directory writable.";
		}
	});

constexpr auto kMainCapture = UiAction(
	"flowui.dev_interface.performance.main-capture", [](App& app, DevInterfaceState& state) {
		const auto validation_error = performance_capture_policy_error(state, &app);
		if (!validation_error.empty()) {
			state.capture_error = validation_error;
			return;
		}
		PerformanceCaptureSettings settings;
		settings.start_mode = static_cast<PerformanceCaptureStartMode>(state.capture_start_mode);
		settings.end_mode = static_cast<PerformanceCaptureEndMode>(state.capture_end_mode);
		settings.duration_ns = static_cast<uint64_t>(double(state.capture_duration_seconds) * 1e9);
		settings.budget_ns = static_cast<uint64_t>(double(state.capture_budget_ms) * 1e6);
		settings.post_event_ns = static_cast<uint64_t>(double(state.capture_tail_seconds) * 1e9);
		settings.budget_window = state.capture_budget_window;
		settings.start_chord.key = static_cast<int>(state.capture_key);
		settings.start_chord.ctrl = (state.capture_modifiers & 1) != 0;
		settings.start_chord.shift = (state.capture_modifiers & 2) != 0;
		settings.start_chord.alt = (state.capture_modifiers & 4) != 0;
		settings.start_chord.super = (state.capture_modifiers & 8) != 0;
		auto requested = app.request_dev_performance_capture(settings);
		state.capture_error =
			requested ? "" : "Capture unavailable: check shortcut conflict and settings.";
		if (requested) {
			state.pending_capture_windows = app.devWindowSnapshot();
			state.lastActionMessage = "Main Capture queued";
		}
	});

constexpr auto kBackToDiagnostics =
	UiAction("flowui.dev_interface.diagnostics.back", [](DevInterfaceState& state) {
		state.diagnostics.pending = DevDiagnosticsCommand::Back;
	});

constexpr auto kBackToChanges =
	UiAction("flowui.dev_interface.changes.back", [](DevInterfaceState& state) {
		state.changes.pending = DevChangesCommand::Back;
	});

constexpr auto kApplyCpuReportingLevel = UiAction(
	"flowui.dev_interface.apply-cpu-reporting-level", [](App& app, DevInterfaceState& state) {
		state.cpuReportingLevel =
			std::min<uint64_t>(state.cpuReportingLevel, FLOWUI_DEV_TIMING_LEVEL);
		DevTimingConfig config = app.devMonitoring().timing().config();
		config.cpuLevel = static_cast<CpuTimingLevel>(state.cpuReportingLevel);
		app.devMonitoring().timing().setConfig(config);
		state.performance_selection.cpu_detail_level = config.cpuLevel;
		state.lastActionMessage = "CPU timeline and recording detail changed";
	});

inline constexpr std::array<FSEL::ComboBoxOption, 4> kCpuReportingOptions{{
	{.value = 0u, .text = "CPU: Frame Only"},
	{.value = 1u, .text = "CPU: Summary"},
	{.value = 2u, .text = "CPU: Balanced"},
	{.value = 3u, .text = "CPU: Deep"},
}};

uint64_t tabValue(DevInterfaceTab tab) {
	return static_cast<uint64_t>(tab);
}

DevInterfaceTab normalizedTab(DevInterfaceState& state) {
	if (state.activeTab > tabValue(DevInterfaceTab::Catalogue)) {
		state.activeTab = tabValue(DevInterfaceTab::Inspect);
	}
	return static_cast<DevInterfaceTab>(state.activeTab);
}

Clay_TextElementConfig textConfig(Clay_Color color, uint16_t fontSize) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontId = 0;
	config.fontSize = fontSize;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	config.textAlignment = CLAY_TEXT_ALIGN_CENTER;
	return config;
}

void drawTab(
	DevContentHeader::BuildContext& context,
	DevInterfaceState& state,
	const TabSpec& tab) {
	const bool selected = state.activeTab == tabValue(tab.tab);
	const bool compact = Clay_GetLayoutDimensions().width < 600.0f;
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = tabValue(tab.tab);
	parameters.selectedValue = &state.activeTab;
	parameters.style.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_GROW(0),
	};
	parameters.style.padding = compact ? Clay_Padding{6, 6, 0, 0} : Clay_Padding{14, 14, 0, 0};
	parameters.style.childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	parameters.style.borderWidth = Clay_BorderWidth{0, 0, 0, 2, 0};
	parameters.style.cornerRadius = CLAY_CORNER_RADIUS(0);
	parameters.style.idleOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.style.idleOverrides.borderColor = interface_theme::kDepth2Ink;
	parameters.style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.style.hoveredOverrides.borderColor = interface_theme::kHoverSurface;
	parameters.style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.style.selectedOverrides.backgroundColor = interface_theme::kDepth1Panel;
	parameters.style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;

	context.uiManager.createElement(FSEL::kRadioChoice, tab.id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.construct();
	const Clay_TextElementConfig labelStyle = textConfig(
		selected ? interface_theme::kTextCanvas : interface_theme::kTextSecondary,
		compact ? 10 : 12);
	std::string_view label = tab.label;
	if (compact && tab.tab == DevInterfaceTab::Performance) label = "Perf";
	if (compact && tab.tab == DevInterfaceTab::Diagnostics) label = "Issues";
	if (compact && tab.tab == DevInterfaceTab::Catalogue) label = "Assets";
	CLAY_TEXT(
		context.uiManager.toClayString(label),
		CLAY_TEXT_CONFIG(labelStyle));
	context.uiManager.drawConstructed();
}

void drawTabs(
	DevContentHeader::BuildContext& context,
	DevInterfaceState& state) {
	Clay_ElementDeclaration strip{};
	strip.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(kContentHeaderHeight),
	};
	strip.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	strip.backgroundColor = interface_theme::kDepth2Ink;

	CLAY(context.clayID(kTabStrip), strip) {
		for (const TabSpec& tab : kTabs) drawTab(context, state, tab);
	}
}

void drawInsetSeparator(DevContentHeader::BuildContext& context) {
	Clay_ElementDeclaration inset{};
	inset.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1),
		.height = CLAY_SIZING_GROW(0),
	};
	inset.layout.padding = Clay_Padding{0, 0, 8, 8};

	Clay_ElementDeclaration line{};
	line.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1),
		.height = CLAY_SIZING_GROW(0),
	};
	line.backgroundColor = interface_theme::kBorderVisible;

	CLAY(context.clayID(kSectionSeparatorInset), inset) {
		CLAY(context.clayID(kSectionSeparatorLine), line);
	}
}

FSEL::ButtonParameters controlParameters(
	std::string_view label,
	ActionCall action = {},
	bool enabled = true,
	bool emphasized = false) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = enabled;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.text = label;
	parameters.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(28),
	};
	parameters.padding = Clay_Padding{9, 9, 5, 5};
	parameters.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.idleOverrides.backgroundColor = emphasized
		? interface_theme::kSelectedRow : interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = emphasized
		? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	parameters.idleOverrides.borderColor = emphasized
		? interface_theme::kAccentCurrent : interface_theme::kBorderVisible;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.pressedOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.borderColor = interface_theme::kAccentSeaGlass;
	parameters.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
	parameters.labelFontSize = 12;
	return parameters;
}

void drawControl(DevContentHeader::BuildContext& context, LocalElementName id,
				 std::string_view label, ActionCall action = {}, bool enabled = true,
				 bool emphasized = false, [[maybe_unused]] std::string_view icon_key = {}) {
	auto parameters = controlParameters(label, action, enabled, emphasized);
	TextureRef icon{};
#if FLOWUI_INCLUDE_ICON_MANAGER
	if (context.params.app && !icon_key.empty()) {
		auto& icons = context.params.app->icons();
		if (!icons.contains(icon_key))
			interface_icons::registerDevInterfaceIcons(icons);
		if (icons.contains(icon_key))
			icon = icons.textureRef(icon_key);
	}
#endif
	action_button::set_icon(parameters, icon);
	context.uiManager.createElement(FSEL::kButton, id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
	if (icon.handle)
		action_button::tooltip(context, id, label);
}

void drawReportingText(
	DevContentHeader::BuildContext& context,
	LocalElementName id,
	std::string_view text) {
	Clay_ElementDeclaration label{};
	label.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(28),
	};
	label.layout.padding = Clay_Padding{8, 8, 0, 0};
	label.layout.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	label.backgroundColor = interface_theme::kDepth2Ink;
	label.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	label.cornerRadius = CLAY_CORNER_RADIUS(3);
	const Clay_TextElementConfig style = textConfig(interface_theme::kTextSecondary, 12);
	CLAY(context.clayID(id), label) {
		CLAY_TEXT(context.uiManager.toClayString(text), CLAY_TEXT_CONFIG(style));
	}
}

void drawCpuReportingSelector(DevContentHeader::BuildContext& context, App* app,
							  DevInterfaceState& state) {
	ActionCall changedAction{};
	if (app) {
		changedAction =
			ActionCall{app->actions().uiActions().make(kApplyCpuReportingLevel, *app, state)};
	}

	FSEL::ComboBoxParameters parameters{};
	parameters.options = std::span<const FSEL::ComboBoxOption>{
		kCpuReportingOptions.data(), static_cast<std::size_t>(FLOWUI_DEV_TIMING_LEVEL) + 1u};
	parameters.selectedValue = &state.cpuReportingLevel;
	parameters.enabled = app != nullptr;
	parameters.onChanged = changedAction;
	parameters.sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(200),
		.height = CLAY_SIZING_FIXED(28),
	};
	parameters.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
	parameters.fontSize = 12;

	context.uiManager.createElement(FSEL::kComboBox, kCpuReportingLevel)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
	constexpr std::array<std::string_view, 4> compiled_labels{"Build: Frame Only", "Build: Summary",
															  "Build: Balanced", "Build: Deep"};
	drawReportingText(context, LocalElementName{"cpu-compiled-ceiling"},
					  compiled_labels[FLOWUI_DEV_TIMING_LEVEL]);
	action_button::tooltip(
		context, kCpuReportingLevel,
		"CPU detail filters the displayed snapshot and sets future recording detail. "
		"Build is the maximum compiled CPU level. Recorded CPU shows the capture's original "
		"level. Balanced retains slow or selected element invocations; GPU detail is separate.",
		FSEL::ComboBox::definitionId);
}

constexpr auto kChangeInspectWindow =
	UiAction("flowui.dev_interface.inspect.change-window", [](App& app, DevInterfaceState& state) {
		set_inspect_window(app, state, state.selectedWindowId);
	});

void drawWindowTarget(DevContentHeader::BuildContext& context, App* app, DevInterfaceState& state) {
	const auto windows = app ? app->devWindowSnapshot() : std::vector<DevWindowInfo>{};
	std::vector<std::string> labels;
	std::vector<FSEL::ComboBoxOption> options;
	labels.reserve(windows.size());
	options.reserve(windows.size());
	for (const auto& window : windows) {
		if (window.id == state.interface_window_id)
			continue;
		labels.emplace_back(window.title.empty() ? "Window " + std::to_string(window.id)
												 : window.title);
		options.emplace_back(FSEL::ComboBoxOption{.value = window.id, .text = labels.back()});
	}
	FSEL::ComboBoxParameters parameters{};
	parameters.options = options;
	parameters.selectedValue = &state.selectedWindowId;
	parameters.onChanged =
		app ? ActionCall{app->actions().uiActions().make(kChangeInspectWindow, *app, state)}
			: ActionCall{};
	parameters.enabled = app && !options.empty();
	parameters.placeholder = "No application windows";
	parameters.sizing =
		Clay_Sizing{CLAY_SIZING_FIXED(Clay_GetLayoutDimensions().width < 600.0f ? 150.0f : 210.0f),
					CLAY_SIZING_FIXED(28)};
	parameters.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
	parameters.fontSize = 12;
	context.uiManager.createElement(FSEL::kComboBox, kWindowTarget)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
	action_button::tooltip(context, kWindowTarget,
						   "Application window inspected by the tree, properties and overlays. "
						   "Catalogue and Changes reuse this window context.",
						   FSEL::ComboBox::definitionId);
}

void drawContextualControls(DevContentHeader::BuildContext& context, App* app,
							DevInterfaceState& state, DevInterfaceTab activeTab) {
	switch (activeTab) {
	case DevInterfaceTab::Inspect:
		drawWindowTarget(context, app, state);
		context.uiManager
			.createElement(kDevInspectOverlayMenu, LocalElementName{"inspect-overlay-controls"})
			.setParameters(DevInspectOverlayMenuParameters{.app = app})
			.draw();
		break;
	case DevInterfaceTab::Performance: {
		drawControl(context, kFrameSelector, "Main Capture",
					app ? ActionCall{app->actions().uiActions().make(kMainCapture, *app, state)}
						: ActionCall{},
					app != nullptr, true);
		const auto status =
			app ? app->devMonitoring().timingReporting().status() : TimingReportingStatus{};
		const auto generation = state.performance_timeline.capture_generation;
		const bool exported = generation && state.performance_export_generation == generation;
		const bool can_export = app && status.capture_sealed && status.hasRetainedTicks &&
								generation == status.capture_generation;
		drawControl(context, LocalElementName{"export-capture"}, exported ? "Exported" : "Export",
					app ? ActionCall{app->actions().uiActions().make(kExportCapture, *app, state)}
						: ActionCall{},
					can_export && !exported, false, interface_icons::kExportKey);
		drawCpuReportingSelector(context, app, state);
		break;
	}
	case DevInterfaceTab::Memory:
		context.uiManager.createElement(kDevMemoryContextControls, LocalElementName{"memory-context"})
			.setParameters({app, &state}).setDevInternalCapture(true).draw();
		break;
	case DevInterfaceTab::Diagnostics:
		context.uiManager
			.createElement(kDevDiagnosticsControls, LocalElementName{"diagnostics-controls"})
			.setParameters({app, &state})
			.setDevInternalCapture(true)
			.draw();
		break;
	case DevInterfaceTab::Changes:
		context.uiManager.createElement(kDevChangesControls, LocalElementName{"changes-controls"})
			.setParameters({app, &state}).setDevInternalCapture(true).draw();
		break;
	case DevInterfaceTab::Catalogue:
		break;
	}
}

} // namespace

void DevContentHeader::buildElement(BuildContext& context) {
	DevInterfaceState* const state = context.params.interfaceState;
	if (!state) return;

	const DevInterfaceTab activeTab = normalizedTab(*state);
	if (context.params.app) {
		state->cpuReportingLevel = std::min<uint64_t>(
			static_cast<uint64_t>(context.params.app->devMonitoring().timing().config().cpuLevel),
			static_cast<uint64_t>(FLOWUI_DEV_TIMING_LEVEL));
		state->performance_selection.cpu_detail_level =
			static_cast<CpuTimingLevel>(state->cpuReportingLevel);
	}

	Clay_ElementDeclaration header{};
	const bool stacked = Clay_GetLayoutDimensions().width < 1280.0f;
	header.layout.sizing = {
		.width = CLAY_SIZING_PERCENT(1),
		.height = CLAY_SIZING_FIXED(stacked ? 2.0f * kContentHeaderHeight : kContentHeaderHeight),
	};
	header.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	header.layout.layoutDirection = stacked ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT;
	header.backgroundColor = interface_theme::kDepth2Ink;
	header.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};

	CLAY(context.clayID(), header) {
		drawTabs(context, *state);
		if (!stacked)
			drawInsetSeparator(context);

		Clay_ElementDeclaration controls{};
		controls.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(kContentHeaderHeight),
		};
		controls.layout.padding = Clay_Padding{10, 10, 0, 0};
		controls.layout.childGap = 6;
		controls.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		controls.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		CLAY(context.clayID(kContextualControls), controls) {
			if (state->diagnostics.return_available &&
				(activeTab == DevInterfaceTab::Performance ||
				 activeTab == DevInterfaceTab::Memory) &&
				context.params.app) {
				drawControl(context, LocalElementName{"back-to-diagnostics"}, "Back to Diagnostics",
							ActionCall{context.params.app->actions().uiActions().make(
								kBackToDiagnostics, *state)},
							true, false, interface_icons::kBackKey);
			}
			if (state->changes.return_available && activeTab != DevInterfaceTab::Changes &&
				context.params.app) {
				drawControl(context, LocalElementName{"back-to-changes"}, "Back to Changes",
							ActionCall{context.params.app->actions().uiActions().make(
								kBackToChanges, *state)},
							true, false, interface_icons::kBackKey);
			}
			drawContextualControls(context, context.params.app, *state, activeTab);
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
