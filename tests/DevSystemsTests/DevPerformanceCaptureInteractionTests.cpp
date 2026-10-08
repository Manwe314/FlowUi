#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelector.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <string_view>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
void frame(App& app) {
	assert(app.beginFrame());
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	CLAY(CLAY_ID("capture-application"), root) {
		CLAY_TEXT(CLAY_STRING("Application work"), CLAY_TEXT_CONFIG({.fontSize = 12}));
	}
	assert(app.endFrame());
	assert(app.drawFrame());
}
[[nodiscard]] WindowId interface_window(App& app) {
	for (const auto& window : app.devWindowSnapshot())
		if (window.title == "FlowUi Developer Interface")
			return window.id;
	return InvalidWindowId;
}
void verify_startup_capture(bool open_early) {
	AppConfig startup_config;
	startup_config.dev.enabled = true;
	startup_config.dev.capture_timing_on_startup = true;
	startup_config.dev.excludeInternalDevElementsFromCapture = false;
	startup_config.dev.monitoring.timing.gpuTimingEnabled = false;
	startup_config.dev.monitoring.timingReporting.retainedAppTickCapacity = 4;
	startup_config.dev.monitoring.timingReporting.minimumFramesInFlightMultiplier = 1;
	startup_config.vk.enableValidation = true;
	startup_config.window.title = "Startup timing capture verification";
	auto startup_app = makeApplication(startup_config);
	auto& startup_capture = startup_app.devMonitoring().performance_capture();
	assert(startup_capture.startup_capture());
	frame(startup_app);
	frame(startup_app);
	assert(startup_capture.status().phase == PerformanceCapturePhase::Recording);
	if (open_early)
		startup_app.request_dev_interface_toggle();
	for (int frame_count = 0;
		 frame_count < 100 && startup_capture.status().phase != PerformanceCapturePhase::Sealed;
		 ++frame_count)
		frame(startup_app);
	assert(startup_capture.status().phase == PerformanceCapturePhase::Sealed);
	const auto retained = startup_app.devMonitoring().timingReporting().status();
	assert(retained.capture_overwritten_ticks == 0 && retained.oldestRetainedAppTick == 1);
	if (!open_early) {
		assert(interface_window(startup_app) == InvalidWindowId);
		assert(retained.capture_admitted_ticks == retained.effectiveCapacity);
		assert(startup_capture.status().stop_reason ==
			   PerformanceCaptureStopReason::CapacityReached);
		startup_app.request_dev_interface_toggle();
	} else {
		assert(retained.capture_admitted_ticks == 2);
		assert(startup_capture.status().stop_reason == PerformanceCaptureStopReason::ManualOpen);
	}
	frame(startup_app);
	const auto startup_window = interface_window(startup_app);
	assert(startup_window != InvalidWindowId);
	const auto& startup_tree = startup_app.ui(startup_window).devTreeSnapshot();
	assert(std::ranges::any_of(startup_tree.flow.nodes, [](const auto& node) {
		return node.definition == DevPerformanceWorkbench::definitionId;
	}));
	auto view = startup_app.devMonitoring().timingReporting().read_capture();
	assert(view.generation == 1 && !view.first.empty());
	assert(!view.first.front().windows.empty());
	std::cout << "Startup snapshot survives " << (open_early ? "early opening" : "ring capacity")
			  << '\n';
}
} // namespace
int main(int argument_count, char** arguments) {
	if (argument_count == 2) {
		const std::string_view scenario(arguments[1]);
		assert(scenario == "startup-full" || scenario == "startup-early");
		verify_startup_capture(scenario == "startup-early");
		return 0;
	}
	AppConfig config;
	config.dev.enabled = true;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.window.title = "Capture lifecycle integration";
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	app.request_dev_interface_toggle();
	frame(app);
	const auto window = interface_window(app);
	assert(window != InvalidWindowId);
	const auto& tree = app.ui(window).devTreeSnapshot();
	FlowElementID selector_id{};
	for (const auto& node : tree.flow.nodes)
		if (node.definition == DevInspectSelector::definitionId)
			selector_id.value = node.instance.value;
	assert(selector_id);
	auto* selector_state = app.elements().getStatePointer(kDevInspectSelector, window, selector_id);
	assert(selector_state);
	const auto selector_address = selector_state;
	const auto frame_before_close = tree.frameNumber;
	app.request_dev_interface_toggle();
	frame(app);
	assert(interface_window(app) == window);
	for (int frame_index = 0; frame_index < 7; ++frame_index)
		frame(app);
	assert(app.ui(window).devTreeSnapshot().frameNumber == frame_before_close);
	assert(app.elements().getStatePointer(kDevInspectSelector, window, selector_id) ==
		   selector_address);
	app.request_dev_interface_toggle();
	frame(app);
	assert(app.ui(window).devTreeSnapshot().frameNumber > frame_before_close);
	app.devTooling().inspect_interaction().toggle_surface(tooling::DevOverlayModeFlags::Typography);
	const auto saved_overlay_flags = app.devTooling().inspect_interaction().mode_flags();
	app.devTooling().inspect_interaction().toggle_primary_pick();
	assert(app.devTooling().inspect_interaction().picking());
	PerformanceCaptureSettings settings;
	settings.end_mode = PerformanceCaptureEndMode::Manual;
	assert(app.request_dev_performance_capture(settings));
	frame(app); // suspension tick
	auto& capture = app.devMonitoring().performance_capture();
	assert(capture.status().phase == PerformanceCapturePhase::Armed);
	const auto suspension_frame = app.ui(window).devTreeSnapshot().frameNumber;
	frame(app); // first clean tick
	assert(capture.status().phase == PerformanceCapturePhase::Recording);
	assert(capture.interface_work_suspended() && app.devTooling().inspect_interaction().picking());
	for (int frame_index = 0; frame_index < 5; ++frame_index)
		frame(app);
	assert(app.ui(window).devTreeSnapshot().frameNumber == suspension_frame);
	app.request_dev_interface_toggle();
	for (int frame_index = 0;
		 frame_index < 100 && capture.status().phase != PerformanceCapturePhase::Sealed;
		 ++frame_index)
		frame(app);
	assert(capture.status().phase == PerformanceCapturePhase::Sealed);
	assert(interface_window(app) == window);
	frame(app);
	bool performance_visible = false;
	for (const auto& node : app.ui(window).devTreeSnapshot().flow.nodes)
		if (node.definition == DevPerformanceWorkbench::definitionId)
			performance_visible = true;
	assert(performance_visible);
	assert(!capture.interface_work_suspended());
	assert(app.devTooling().inspect_interaction().mode_flags() == saved_overlay_flags);
	assert(!app.devTooling()
				.inspect_interaction()
				.picking()); // normal navigation to Performance cancels a pick
	app.devTooling().inspect_interaction().cancel_pick();
	assert(app.elements().getStatePointer(kDevInspectSelector, window, selector_id) ==
		   selector_address);
	{
		auto view = app.devMonitoring().timingReporting().read_capture();
		assert(view.generation == 1);
		for (const auto segment : {view.first, view.second})
			for (const auto& tick : segment) {
				assert(tick.appTick < capture.status().end_app_tick_exclusive);
				for (const auto& captured_window : tick.windows)
					if (captured_window.occupied)
						assert(captured_window.window != window);
			}
	}
	settings.start_mode = PerformanceCaptureStartMode::Shortcut;
	assert(app.request_dev_performance_capture(settings));
	frame(app);
	assert(capture.status().phase == PerformanceCapturePhase::Armed);
	app.request_dev_interface_toggle();
	frame(app);
	assert(capture.status().phase == PerformanceCapturePhase::Sealed &&
		   capture.status().generation == 1);
	std::cout << "Capture UI suspension, return, and session persistence checks passed\n";
}
