#include "devSystems/devInterface/Memory/Analysis/DevMemoryContextControls.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Memory/Analysis/DevMemoryUi.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "managers/UiManager.hpp"
#include <GLFW/glfw3.h>
#include <utility>
namespace FlowUi::devSystems::interface_elements {
void DevMemoryContextControls::buildElement(BuildContext& context) {
	if (!context.params.app || !context.params.interfaceState)
		return;
	using namespace memory_ui;
	auto& workbench = context.params.interfaceState->memory_workbench;
	auto& state = workbench.analysis;
	auto& reporting = context.params.app->devMonitoring().memoryReporting();
	const auto action =
		state.ui_action >= 100 && state.ui_action < 110 ? std::exchange(state.ui_action, 0) : 0;
	if (action >= 100 && action <= 104)
		state.pending_action = action - 99;
	if (action == 105 || action == 106)
		state.show_recording_settings = !state.show_recording_settings;
	if (action == 107)
		for (auto& saved : state.datasets)
			if (saved.identity == state.dataset)
				saved.name = state.capture_name;
	if (action == 108)
		state.include_recent = !state.include_recent;
	if (action == 109)
		state.record_shortcut = !state.record_shortcut;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	if (state.record_shortcut && input.ctrl && input.shift && input.keyDown[GLFW_KEY_M] &&
		!previous.keyDown[GLFW_KEY_M])
		state.pending_action = 1;
	update_memory_session(state, reporting);
	auto root = row(28);
	root.layout.sizing.width = CLAY_SIZING_FIT(0);
	CLAY(context.clayID(), root) {
		FSEL::ComboBoxParameters dataset{};
		dataset.options = state.dataset_options;
		dataset.selectedValue = &state.dataset;
		dataset.sizing =
			Clay_Sizing{.width = CLAY_SIZING_FIXED(155), .height = CLAY_SIZING_FIXED(28)};
		dataset.fontSize = 11;
		context.uiManager.createElement(FSEL::kComboBox, "memory-dataset")
			.setParameters(dataset)
			.setDevInternalCapture(true)
			.draw();
		button(context, workbench, 100, state.recording ? "Stop & save" : "Record",
			   reporting.status().runtimeLevel != MemoryMonitoringLevel::Disabled);
		button(context, workbench, 106, "…");
		if (state.show_recording_settings) {
			auto panel = column(8);
			panel.layout.sizing.width = CLAY_SIZING_FIXED(300);
			panel.layout.padding = {12, 12, 12, 12};
			panel.backgroundColor = interface_theme::kDepth3Elevated;
			panel.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
			panel.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			panel.floating.attachPoints = {CLAY_ATTACH_POINT_RIGHT_TOP,
										   CLAY_ATTACH_POINT_RIGHT_BOTTOM};
			panel.floating.offset.y = 4;
			panel.floating.zIndex = 90;
			CLAY(context.clayID("record-settings"), panel) {
				text(context, "Memory datasets", interface_theme::kTextCanvas, 13);
				FSEL::TextInputParameters name{};
				name.value = &state.capture_name;
				name.maxBytes = 80;
				name.sizing =
					Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
				context.uiManager.createElement(FSEL::kTextInput, "capture-name")
					.setParameters(name)
					.setDevInternalCapture(true)
					.draw();
				number(context, "record-duration", "Duration seconds (0 = manual)",
					   state.recording_seconds, 0.0, 3600.0);
				button(context, workbench, 108,
					   state.include_recent ? "Pre-start history: 5 s" : "Pre-start history: off");
				button(context, workbench, 109,
					   state.record_shortcut ? "Ctrl+Shift+M: enabled" : "Ctrl+Shift+M: disabled");
				button(context, workbench, 101, "Capture current state");
				button(context, workbench, 102, "Save recent 30 seconds");
				if (state.dataset) {
					button(context, workbench, 107, "Rename selected dataset");
					button(context, workbench, 104, "Use selected as baseline");
					button(context, workbench, 103, "Remove selected dataset");
				}
				if (state.recording)
					text(context, "Recording " +
									  std::to_string((memory_now_ns() - state.recording_begin_ns) /
													 1'000'000'000) +
									  " s");
				text(context, state.message, interface_theme::kTextSecondary, 10);
				text(context, "Four datasets / 8192 records each. Partial coverage is explicit.",
					 interface_theme::kTextMuted, 10);
				button(context, workbench, 105, "Close");
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
