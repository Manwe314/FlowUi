#include "devSystems/devInterface/Permanents/Elements/DevContentHeader.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

#include "FSEL/Button.hpp"
#include "FSEL/ComboBox.hpp"
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/ActionManager.hpp"
#include "managers/UiManager.hpp"

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
inline constexpr LocalElementName kInspectScope{"inspect-scope"};
inline constexpr LocalElementName kPickElement{"pick-element"};
inline constexpr LocalElementName kFrameSelector{"frame-selector"};
inline constexpr LocalElementName kCpuReportingLevel{"cpu-reporting-level"};
inline constexpr LocalElementName kCapturedState{"captured-state"};
inline constexpr LocalElementName kCaptureState{"capture-state"};
inline constexpr LocalElementName kBakeReview{"bake-review"};
inline constexpr LocalElementName kBakeChanges{"bake-changes"};
inline constexpr LocalElementName kRefreshCatalogue{"refresh-catalogue"};

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

constexpr auto kBakeActiveChanges = UiAction(
	"flowui.dev_interface.bake-active-changes",
	[](App& app, DevInterfaceState& state) {
		const uint32_t requestedCount = state.unbakedChangeCount;
		const tooling::DevCommandResult result = app.devTooling().bakeActiveEdits();
		const tooling::DevBakeStatusSnapshot after = app.devTooling().queryBakeStatus();
		state.unbakedChangeCount = static_cast<uint32_t>(std::min<std::size_t>(
			after.activeLiveOverrideCount,
			std::numeric_limits<uint32_t>::max()));

		if (result.status == tooling::DevCommandStatus::NothingToBake) {
			state.lastActionMessage = "No changes to bake";
			return;
		}
		if (result.applied) {
			char message[80]{};
			std::snprintf(
				message, sizeof(message), "Baked %u change%s",
				requestedCount, requestedCount == 1u ? "" : "s");
			state.lastActionMessage = message;
			return;
		}
		state.lastActionMessage = "Bake failed; review bake diagnostics";
	});

constexpr auto kApplyCpuReportingLevel = UiAction(
	"flowui.dev_interface.apply-cpu-reporting-level",
	[](App& app, DevInterfaceState& state) {
		state.cpuReportingLevel = std::min<uint64_t>(
			state.cpuReportingLevel, FLOWUI_DEV_TIMING_LEVEL);
		DevTimingConfig config = app.devMonitoring().timing().config();
		config.cpuLevel = static_cast<CpuTimingLevel>(state.cpuReportingLevel);
		app.devMonitoring().timing().setConfig(config);
		state.lastActionMessage = "CPU reporting detail changed";
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
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = tabValue(tab.tab);
	parameters.selectedValue = &state.activeTab;
	parameters.style.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_GROW(0),
	};
	parameters.style.padding = Clay_Padding{14, 14, 0, 0};
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
		12);
	CLAY_TEXT(
		context.uiManager.toClayString(tab.label),
		CLAY_TEXT_CONFIG(labelStyle));
	context.uiManager.drawConstructed();
}

void drawTabs(
	DevContentHeader::BuildContext& context,
	DevInterfaceState& state) {
	Clay_ElementDeclaration strip{};
	strip.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_GROW(0),
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

void drawControl(
	DevContentHeader::BuildContext& context,
	LocalElementName id,
	std::string_view label,
	ActionCall action = {},
	bool enabled = true,
	bool emphasized = false) {
	context.uiManager.createElement(FSEL::kButton, id)
		.setParameters(controlParameters(label, action, enabled, emphasized))
		.setDevInternalCapture(true)
		.draw();
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

void drawCpuReportingSelector(
	DevContentHeader::BuildContext& context,
	App* app,
	DevInterfaceState& state) {
	ActionCall changedAction{};
	if (app) {
		changedAction = ActionCall{
			app->actions().uiActions().make(kApplyCpuReportingLevel, *app, state)};
	}

	FSEL::ComboBoxParameters parameters{};
	parameters.options = std::span<const FSEL::ComboBoxOption>{
		kCpuReportingOptions.data(),
		static_cast<std::size_t>(FLOWUI_DEV_TIMING_LEVEL) + 1u};
	parameters.selectedValue = &state.cpuReportingLevel;
	parameters.enabled = app != nullptr;
	parameters.onChanged = changedAction;
	parameters.sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(160),
		.height = CLAY_SIZING_FIXED(28),
	};
	parameters.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
	parameters.fontSize = 12;

	context.uiManager.createElement(FSEL::kComboBox, kCpuReportingLevel)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
}

void drawContextualControls(
	DevContentHeader::BuildContext& context,
	App* app,
	DevInterfaceState& state,
	DevInterfaceTab activeTab,
	std::size_t bakeableChangeCount) {
	switch (activeTab) {
	case DevInterfaceTab::Inspect:
		drawControl(context, kInspectScope, "Inspection Scope  ▾");
		drawControl(context, kPickElement, "Pick Element");
		break;
	case DevInterfaceTab::Performance: {
		drawControl(context, kFrameSelector, "Frame: Latest  ▾");
		drawCpuReportingSelector(context, app, state);
		break;
	}
	case DevInterfaceTab::Memory:
		drawReportingText(context, kCapturedState, "Captured State: None");
		drawControl(context, kCaptureState, "Capture State");
		break;
	case DevInterfaceTab::Diagnostics:
		break;
	case DevInterfaceTab::Changes: {
		drawControl(context, kBakeReview, "Bake Review");
		char bakeLabel[80]{};
		std::snprintf(
			bakeLabel, sizeof(bakeLabel), "Bake %zu Change%s",
			bakeableChangeCount, bakeableChangeCount == 1u ? "" : "s");
		ActionCall bakeAction{};
		if (app) {
			bakeAction = ActionCall{
				app->actions().uiActions().make(kBakeActiveChanges, *app, state)};
		}
		drawControl(
			context, kBakeChanges, bakeLabel, bakeAction,
			app && bakeableChangeCount > 0u, true);
		break;
	}
	case DevInterfaceTab::Catalogue:
		drawControl(context, kRefreshCatalogue, "Refresh");
		break;
	}
}

} // namespace

void DevContentHeader::buildElement(BuildContext& context) {
	DevInterfaceState* const state = context.params.interfaceState;
	if (!state) return;

	const DevInterfaceTab activeTab = normalizedTab(*state);
	std::size_t bakeableChangeCount = 0u;
	if (context.params.app) {
		state->cpuReportingLevel = std::min<uint64_t>(
			static_cast<uint64_t>(
				context.params.app->devMonitoring().timing().config().cpuLevel),
			static_cast<uint64_t>(FLOWUI_DEV_TIMING_LEVEL));
		const tooling::DevBakeStatusSnapshot bakeStatus =
			context.params.app->devTooling().queryBakeStatus();
		bakeableChangeCount = bakeStatus.bakeableOverrideCount;
		state->unbakedChangeCount = static_cast<uint32_t>(std::min<std::size_t>(
			bakeStatus.activeLiveOverrideCount,
			std::numeric_limits<uint32_t>::max()));
	}

	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(kContentHeaderHeight),
	};
	header.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	header.backgroundColor = interface_theme::kDepth2Ink;
	header.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};

	CLAY(context.clayID(), header) {
		drawTabs(context, *state);
		drawInsetSeparator(context);

		Clay_ElementDeclaration controls{};
		controls.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		controls.layout.padding = Clay_Padding{10, 10, 0, 0};
		controls.layout.childGap = 6;
		controls.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		controls.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		CLAY(context.clayID(kContextualControls), controls) {
			drawContextualControls(
				context, context.params.app, *state, activeTab,
				bakeableChangeCount);
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
