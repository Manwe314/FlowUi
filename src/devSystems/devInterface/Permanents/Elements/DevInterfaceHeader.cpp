#include "devSystems/devInterface/Permanents/Elements/DevInterfaceHeader.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cstdio>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "FSEL/Button.hpp"
#include "FSEL/ComboBox.hpp"
#include "devMode/performanceDiagnostics.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

constexpr float kHeaderHeight = 44.0f;
inline constexpr LocalElementName kTitleSeparatorInset{"title-separator-inset"};
inline constexpr LocalElementName kTitleSeparatorLine{"title-separator-line"};
inline constexpr LocalElementName kWindowSelector{"window-selector"};
inline constexpr LocalElementName kActionSpacer{"action-spacer"};
inline constexpr LocalElementName kHeaderActions{"header-actions"};
inline constexpr LocalElementName kUndoAction{"undo"};
inline constexpr LocalElementName kHistoryAction{"history"};
inline constexpr LocalElementName kRedoAction{"redo"};
inline constexpr LocalElementName kConfigurationsAction{"configurations"};

bool submitHistoryCommands(
	App& app,
	DevInterfaceState& state,
	const std::vector<tooling::DevOverrideCommand>& commands) {
	if (commands.empty()) return true;
	return app.devTooling().overrides().submit(tooling::DevChangeSet{
		.transaction = state.nextEditTransaction++,
		.commands = commands,
	});
}

constexpr auto kUndoEditorTransaction = UiAction(
	"flowui.dev_interface.editor.undo",
	[](App& app, DevInterfaceState& state) {
		if (state.editUndoStack.empty()) return;
		DevInterfaceEditTransaction transaction = state.editUndoStack.back();
		if (!submitHistoryCommands(app, state, transaction.inverse)) return;
		state.editUndoStack.pop_back();
		if (transaction.changesClipboard) {
			state.editorClipboard = transaction.clipboardBefore;
		}
		state.lastActionMessage = "Undo: " + transaction.description;
		state.editRedoStack.push_back(std::move(transaction));
	});

constexpr auto kRedoEditorTransaction = UiAction(
	"flowui.dev_interface.editor.redo",
	[](App& app, DevInterfaceState& state) {
		if (state.editRedoStack.empty()) return;
		DevInterfaceEditTransaction transaction = state.editRedoStack.back();
		if (!submitHistoryCommands(app, state, transaction.forward)) return;
		state.editRedoStack.pop_back();
		if (transaction.changesClipboard) {
			state.editorClipboard = transaction.clipboardAfter;
		}
		state.lastActionMessage = "Redo: " + transaction.description;
		state.editUndoStack.push_back(std::move(transaction));
	});

Clay_TextElementConfig textConfig(Clay_Color color, uint16_t fontSize) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontId = 0;
	config.fontSize = fontSize;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	config.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return config;
}

std::string fallbackWindowName(WindowId id) {
	char buffer[48]{};
	std::snprintf(buffer, sizeof(buffer), "Window %llu",
		static_cast<unsigned long long>(id));
	return buffer;
}

std::string truncateWindowTitle(std::string_view title) {
	constexpr std::size_t kMaximumDisplayBytes = 24u;
	constexpr std::size_t kEllipsisBytes = 3u;
	if (title.size() <= kMaximumDisplayBytes) return std::string{title};

	std::size_t end = kMaximumDisplayBytes - kEllipsisBytes;
	while (end > 0u &&
		(static_cast<unsigned char>(title[end]) & 0xC0u) == 0x80u) {
		--end;
	}
	std::string result{title.substr(0u, end)};
	result.append("...");
	return result;
}

std::string formatBytes(uint64_t bytes) {
	constexpr double kMegabyte = 1'000'000.0;
	constexpr double kGigabyte = 1'000.0 * kMegabyte;
	char buffer[48]{};
	if (static_cast<double>(bytes) >= kGigabyte) {
		std::snprintf(buffer, sizeof(buffer), "%.2f GB", static_cast<double>(bytes) / kGigabyte);
	} else {
		std::snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / kMegabyte);
	}
	return buffer;
}

uint64_t currentMemoryBytes(const App& app) {
	const auto environment = app.devMonitoring().memoryReporting().environmentSnapshot();
	if (environment && environment->process.residentBytes > 0u) {
		return environment->process.residentBytes;
	}

	uint64_t current = 0u;
	for (const auto& source : app.devMonitoring().memoryReporting().currentSourceSnapshot()) {
		current += source.value.backingAllocatedBytes;
	}
	return current;
}

void drawInsetSeparator(DevInterfaceHeader::BuildContext& context) {
	Clay_ElementDeclaration inset{};
	inset.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1),
		.height = CLAY_SIZING_GROW(0),
	};
	inset.layout.padding = Clay_Padding{0, 0, 10, 10};

	Clay_ElementDeclaration line{};
	line.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1),
		.height = CLAY_SIZING_GROW(0),
	};
	line.backgroundColor = interface_theme::kBorderPrimary;

	CLAY(context.clayID(kTitleSeparatorInset), inset) {
		CLAY(context.clayID(kTitleSeparatorLine), line);
	}
}

void drawActionButton(
	DevInterfaceHeader::BuildContext& context,
	LocalElementName id,
	std::string_view label,
	ActionCall action = {},
	bool enabled = true,
	TextureRef icon = {}) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = enabled;
	parameters.contentMode = icon.handle
		? FSEL::ButtonContentMode::IconOnly : FSEL::ButtonContentMode::TextOnly;
	parameters.text = label;
	parameters.icon = icon;
	parameters.tintIcon = true;
	parameters.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(28),
	};
	parameters.padding = Clay_Padding{9, 9, 5, 5};
	parameters.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.iconColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.iconColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.pressedOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.iconColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.iconColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
	parameters.labelFontSize = 12;
	parameters.iconSize = 14.0f;

	context.uiManager.createElement(FSEL::kButton, id)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
}

void drawHeaderContents(
	DevInterfaceHeader::BuildContext& context,
	App* app,
	DevInterfaceState* state,
	const std::vector<DevWindowInfo>& windows,
	std::span<const FSEL::ComboBoxOption> windowOptions) {
	const Clay_TextElementConfig titleStyle = textConfig(interface_theme::kTextCanvas, 15);
	CLAY_TEXT(context.uiManager.toClayString("FlowUi DevInterface"), CLAY_TEXT_CONFIG(titleStyle));

	drawInsetSeparator(context);

	auto combo = context.uiManager.createElement(
		FSEL::kComboBox, kWindowSelector);
	FSEL::ComboBoxParameters comboParameters{};
	comboParameters.options = windowOptions;
	comboParameters.selectedValue = state ? &state->selectedWindowId : nullptr;
	comboParameters.placeholder = windows.empty() ? "No application windows" : "Select window";
	comboParameters.enabled = state && !windows.empty();
	comboParameters.sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(210),
		.height = CLAY_SIZING_FIXED(28),
	};
	comboParameters.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
	comboParameters.fontSize = 12;
	combo.setParameters(std::move(comboParameters));
	combo.setDevInternalCapture(true).draw();

	uint32_t framesInFlight = 0u;
	double rollingFps = 0.0;
	if (app && state && state->selectedWindowId != InvalidWindowId) {
		const auto selected = std::ranges::find(windows, state->selectedWindowId, &DevWindowInfo::id);
		if (selected != windows.end()) framesInFlight = selected->framesInFlight;
		if (app->hasWindow(state->selectedWindowId)) {
			rollingFps = app->ui(state->selectedWindowId).performanceDiagnostics().rolling().fps;
		}
	}

	char telemetryBuffer[160]{};
	const std::string memoryText = app ? formatBytes(currentMemoryBytes(*app)) : std::string{"Unavailable"};
	std::snprintf(
		telemetryBuffer, sizeof(telemetryBuffer), "Max Frames: %u   FPS: %.0f   Memory: %s",
		framesInFlight, rollingFps, memoryText.c_str());
	const Clay_TextElementConfig telemetryStyle = textConfig(interface_theme::kTextSecondary, 12);
	CLAY_TEXT(context.uiManager.toClayString(telemetryBuffer), CLAY_TEXT_CONFIG(telemetryStyle));

	Clay_ElementDeclaration spacer{};
	spacer.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(1),
	};
	CLAY(context.clayID(kActionSpacer), spacer);

	Clay_ElementDeclaration actions{};
	actions.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_GROW(0),
	};
	actions.layout.childGap = 6;
	actions.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	actions.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(context.clayID(kHeaderActions), actions) {
		ActionCall undo{};
		ActionCall redo{};
		if (app && state) {
			undo = ActionCall{app->actions().uiActions().make(
				kUndoEditorTransaction, *app, *state)};
			redo = ActionCall{app->actions().uiActions().make(
				kRedoEditorTransaction, *app, *state)};
		}
		drawActionButton(
			context, kUndoAction, "Undo", undo,
			state && !state->editUndoStack.empty(),
			context.resources().undoIcon);
		drawActionButton(context, kHistoryAction, "History");
		drawActionButton(
			context, kRedoAction, "Redo", redo,
			state && !state->editRedoStack.empty(),
			context.resources().redoIcon);
		drawActionButton(context, kConfigurationsAction, "Configurations");
	}
}

} // namespace

DevInterfaceHeaderResources::DevInterfaceHeaderResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	if (icons.contains(interface_icons::kUndoKey)) {
		undoIcon = icons.textureRef(interface_icons::kUndoKey);
	}
	if (icons.contains(interface_icons::kRedoKey)) {
		redoIcon = icons.textureRef(interface_icons::kRedoKey);
	}
#else
	(void)app;
#endif
}

void DevInterfaceHeader::buildElement(BuildContext& context) {
	App* const app = context.params.app;
	DevInterfaceState* const state = context.params.interfaceState;

	std::vector<DevWindowInfo> windows = app ? app->devWindowSnapshot() : std::vector<DevWindowInfo>{};
	std::erase_if(windows, [&](const DevWindowInfo& window) {
		return window.id == context.params.interfaceWindowId;
	});

	if (state) {
		const bool selectedWindowAvailable = std::ranges::any_of(
			windows, [state](const DevWindowInfo& window) {
				return window.id == state->selectedWindowId;
			});
		if (!selectedWindowAvailable) {
			state->selectedWindowId = windows.empty() ? InvalidWindowId : windows.front().id;
		}
	}

	std::vector<std::string> windowLabels;
	windowLabels.reserve(windows.size());
	for (const DevWindowInfo& window : windows) {
		windowLabels.push_back(window.title.empty()
			? fallbackWindowName(window.id)
			: truncateWindowTitle(window.title));
	}
	std::vector<FSEL::ComboBoxOption> windowOptions;
	windowOptions.reserve(windows.size());
	for (std::size_t index = 0; index < windows.size(); ++index) {
		windowOptions.push_back(FSEL::ComboBoxOption{
			.value = windows[index].id,
			.text = windowLabels[index],
		});
	}

	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(kHeaderHeight),
	};
	header.layout.padding = Clay_Padding{16, 16, 0, 0};
	header.layout.childGap = 10;
	header.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	header.backgroundColor = interface_theme::kDepth1Panel;
	header.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};

	CLAY(context.clayID(), header) {
		drawHeaderContents(context, app, state, windows, windowOptions);
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
