#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/RadioChoice.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbench.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchControls.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
int main() {
	assert(memory_bytes(1024, false) == "1.00 KiB");
	assert(memory_bytes(1000, true) == "1.00 kB");
	assert(memory_axis_max(0) == 1);
	assert(memory_axis_max(UINT64_MAX) == UINT64_MAX);
	AppConfig config{};
	config.window.title = "Memory workbench verification";
	config.window.width = 1100;
	config.window.height = 800;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	constexpr auto workbench_id = Global<kDevMemoryWorkbench>("memory.workbench.verification");
	const auto control_id = [&](uint64_t action) {
		return ::FlowUi::detail::element_id::resolveLocal(
			FlowElementID{.value = workbench_id.value},
			action == 1 ? FSEL::RadioChoice::definitionId : DevMemoryControl::definitionId,
			Keyed(action == 1					  ? LocalElementName{"units"}
				  : action >= 100 && action < 300 ? LocalElementName{"memory-action"}
												  : LocalElementName{"memory-control"},
				  action)
				.token);
	};
	uint64_t drawn_frames = 0;
	const auto draw = [&](uint64_t action = 0, int event = 0, float width = 900,
						  bool plot_gesture = false, FlowElementID child_control = {},
						  float pointer_fraction = .5f, float pointer_vertical = .5f,
						  float scroll = 0, int mouse_button = 0, bool shift = false,
						  int keyboard_key = -1) {
		++drawn_frames;
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.mouseX = -1;
		input.mouseY = -1;
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseDown[size_t(mouse_button)] = event == 1;
		input.keyDown = {};
		if (keyboard_key >= 0)
			input.keyDown[size_t(keyboard_key)] = true;
		input.scrollY = scroll;
		input.shift = shift;
		if (plot_gesture) {
			const auto image_id = ::FlowUi::detail::element_id::resolveLocal(
				FlowElementID{.value = workbench_id.value}, DevMemoryWorkbench::definitionId,
				LocalElementName{"history-image"}.token);
			const auto bounds = Clay_GetElementData(app.ui().toClayEID(image_id)).boundingBox;
			input.mouseX = bounds.x + 68 + (bounds.width - 76) * pointer_fraction;
			input.mouseY = bounds.y + bounds.height * pointer_vertical;
		}
		if (action) {
			const auto element = app.ui().toClayEID(control_id(action));
			const auto identity = element.id;
			const auto button_bounds = Clay_GetElementData(element).boundingBox;
			input.mouseX = button_bounds.x + button_bounds.width / 2;
			input.mouseY = button_bounds.y + button_bounds.height / 2;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		if (child_control.value) {
			const auto element = app.ui().toClayEID(child_control);
			const auto identity = element.id;
			const auto child_bounds = Clay_GetElementData(element).boundingBox;
			input.mouseX = child_bounds.x + child_bounds.width / 2;
			input.mouseY = child_bounds.y + child_bounds.height / 2;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		Clay_ElementDeclaration root{};
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(730)};
		root.clip = {.horizontal = true, .vertical = true};

		Clay_ElementDeclaration background{};
		background.layout.sizing = {.width = CLAY_SIZING_FIXED(1100),
									.height = CLAY_SIZING_FIXED(800)};
		background.backgroundColor = Flow_Color("#071A22");
		CLAY(CLAY_ID("memory-full-window"), background) {
			CLAY(CLAY_ID("memory-workbench-root"), root) {
				app.ui()
					.createElement(kDevMemoryWorkbench, workbench_id)
					.setParameters({&app, &session})
					.draw();
			}
		}
		assert(app.endFrame());
		const auto workbench_bounds =
			Clay_GetElementData(app.ui().toClayEID(FlowElementID{.value = workbench_id.value}))
				.boundingBox;
		assert(workbench_bounds.width <= width + 1);
		assert(app.drawFrame());
	};
	const auto click = [&](uint64_t action) {
		draw(action, 1);
		draw(action, 2);
		draw();
	};
	draw();
	draw();
	assert(!session.memory_workbench.tracks.empty());
	const auto workbench_scroll =
		Clay_GetScrollContainerData(app.ui().toClayEID(FlowElementID{.value = workbench_id.value}));
	assert(workbench_scroll.found);
	assert(workbench_scroll.contentDimensions.height >
		   workbench_scroll.scrollContainerDimensions.height);
	const auto cpu_scope = uint64_t(DevMemoryNodeId::CpuStorage);
	const auto track_token = cpu_scope | (uint64_t{1} << 63);
	const auto track_id = control_id(track_token);
	const auto track_bounds = Clay_GetElementData(app.ui().toClayEID(track_id)).boundingBox;
	assert(track_bounds.width > 800 && track_bounds.height >= 58);
	click(track_token);
	assert(session.memory_workbench.focus == cpu_scope);
	assert(session.memory_selection.selected_node == 0);
	// A nested navigation button must not also trigger the enclosing track.
	session.memory_workbench.focus = 0;
	const auto scope_control = ::FlowUi::detail::element_id::resolveLocal(
		track_id, DevMemoryControl::definitionId,
		Keyed("memory-control", track_token ^ (uint64_t{9} << 56)).token);
	draw(track_token, 1, 900, false, scope_control);
	draw(track_token, 2, 900, false, scope_control);
	assert(session.memory_workbench.focus == 0);
	draw();
	assert(session.memory_selection.selected_node == cpu_scope);
	assert(session.memory_workbench.focus == 0);
	for (const auto& node : memory_selector_nodes) {
		session.memory_selection.selected_node = uint64_t(node.id);
		draw();
		draw();
		assert(!session.memory_workbench.tracks.empty());
	}
	session.memory_selection.selected_node = uint64_t(DevMemoryNodeId::PersistentPool);
	draw();
	draw();
	assert(session.memory_workbench.tracks.front().title == "Persistent Pool");
	draw(0, 0, 420);
	draw(0, 0, 420);
	click(1);
	assert(session.memory_workbench.decimal);
	click(2);
	assert(!session.memory_workbench.following);
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	assert(session.memory_workbench.held_data);
	const auto held = session.memory_workbench.held_data->segments;
	const auto consumed_tick = app.devMonitoring().memoryReporting().status().lastConsumedAppTick;
	draw();
	draw();
	assert(app.devMonitoring().memoryReporting().status().lastConsumedAppTick > consumed_tick);
	assert(session.memory_workbench.held_data->segments.size() == held.size());
	for (size_t index = 0; index < held.size(); ++index)
		assert(session.memory_workbench.held_data->segments[index].endNs == held[index].endNs);
	// Wheel zoom and navigator scrubbing work without a mode or modifier.
	const auto original_duration =
		session.memory_workbench.view_end_ns - session.memory_workbench.view_begin_ns;
	draw(0, 0, 900, true, {}, .5f, .5f, 1);
	assert(session.memory_workbench.view_end_ns - session.memory_workbench.view_begin_ns <
		   original_duration);
	const auto zoomed_duration =
		session.memory_workbench.view_end_ns - session.memory_workbench.view_begin_ns;
	draw(0, 1, 900, true, {}, .3f, .98f);
	draw(0, 1, 900, true, {}, .6f, .98f);
	draw(0, 0, 900, true, {}, .6f, .98f);
	assert(session.memory_workbench.view_end_ns - session.memory_workbench.view_begin_ns ==
		   zoomed_duration);
	click(2);
	assert(session.memory_workbench.following);
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	draw(0, 1, 900, true);
	draw(0, 0, 900, true);
	assert(!session.memory_workbench.following);
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	// Direct range selection, followed by an independently configured three-lane view.
	draw(0, 1, 900, true, {}, .2f);
	draw(0, 1, 900, true, {}, .8f);
	draw(0, 0, 900, true, {}, .8f);
	assert(session.memory_workbench.range_end_ns > session.memory_workbench.range_begin_ns);
	assert(session.memory_workbench.selected_ns == 0);
	click(204);
	assert(session.memory_workbench.history_settings_open);
	std::cout << "History review frame=" << drawn_frames - 1 << " phase=one-lane-settings\n";
	click(205);
	click(205);
	assert(session.memory_workbench.lane_count == 3);
	click(205);
	assert(session.memory_workbench.lane_count == 3);
	session.memory_workbench.lane_settings[1].scope = uint64_t(DevMemoryNodeId::InputFields);
	const auto count_radio = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = workbench_id.value}, FSEL::RadioChoice::definitionId,
		Keyed("lane-metric", 3).token);
	draw(0, 1, 900, false, count_radio);
	draw(0, 2, 900, false, count_radio);
	assert(session.memory_workbench.lane_settings[1].metric == 1);
	session.memory_workbench.lane_settings[2].scope = uint64_t(DevMemoryNodeId::ProcessMemory);
	draw();
	draw();
	assert(session.memory_workbench.lanes.size() == 3);
	assert(session.memory_workbench.lanes[1].metric == 1);
	std::cout << "History review frame=" << drawn_frames - 1 << " phase=three-lane-settings\n";
	click(211);
	assert(session.memory_workbench.lane_count == 2);
	assert(session.memory_workbench.lane_settings[1].scope ==
		   uint64_t(DevMemoryNodeId::ProcessMemory));
	click(204);
	assert(!session.memory_workbench.history_settings_open);
	std::cout << "History review frame=" << drawn_frames - 1 << " phase=two-lanes\n";
	// Keyboard commands use the focused graph and preserve live collection.
	draw(0, 0, 900, true, {}, .5f, .5f, 0, 0, false, GLFW_KEY_ESCAPE);
	assert(session.memory_workbench.selected_ns == 0 && session.memory_workbench.range_end_ns == 0);
	draw(0, 0, 900, true, {}, .5f, .5f, 0, 0, false, GLFW_KEY_HOME);
	draw();
	assert(session.memory_workbench.view_begin_ns == session.memory_workbench.retained_begin_ns);
	assert(session.memory_workbench.view_end_ns == session.memory_workbench.retained_end_ns);
	const auto fitted_duration =
		session.memory_workbench.view_end_ns - session.memory_workbench.view_begin_ns;
	draw(0, 0, 900, true, {}, .5f, .5f, 0, 0, false, GLFW_KEY_EQUAL);
	assert(session.memory_workbench.view_end_ns - session.memory_workbench.view_begin_ns <
		   fitted_duration);
	const auto hide = [&] {
		assert(app.beginFrame());
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	hide();
	hide();
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	assert(session.memory_workbench.following);
	assert(!session.memory_workbench.held_data);
	draw(0, 0, 420);
	draw(0, 0, 420);
	std::cout << "Memory workbench catalogue, controls, independent held history and real Vulkan "
				 "frames verified\n";
}
