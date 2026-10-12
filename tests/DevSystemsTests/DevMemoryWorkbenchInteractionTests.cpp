#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/RadioChoice.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbench.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchControls.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
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
			Keyed(action == 1 ? LocalElementName{"units"} : LocalElementName{"memory-control"},
				  action)
				.token);
	};
	const auto draw = [&](uint64_t action = 0, int event = 0, float width = 900,
						  bool plot_gesture = false, FlowElementID child_control = {}) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.mouseX = -1;
		input.mouseY = -1;
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseDown[0] = event == 1;
		if (plot_gesture) {
			const auto image_id = ::FlowUi::detail::element_id::resolveLocal(
				FlowElementID{.value = workbench_id.value}, DevMemoryWorkbench::definitionId,
				LocalElementName{"history-image"}.token);
			const auto bounds = Clay_GetElementData(app.ui().toClayEID(image_id)).boundingBox;
			input.mouseX = bounds.x + bounds.width / 2;
			input.mouseY = bounds.y + bounds.height / 2;
		}
		if (action) {
			const auto identity = app.ui().toClayEID(control_id(action)).id;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		if (child_control.value) {
			const auto identity = app.ui().toClayEID(child_control).id;
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
	click(5);
	assert(!session.memory_workbench.following);
	click(2);
	assert(session.memory_workbench.following);
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	draw(0, 1, 900, true);
	draw(0, 0, 900, true);
	assert(!session.memory_workbench.following);
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	const auto hide = [&] {
		assert(app.beginFrame());
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	hide();
	hide();
	assert(app.devMonitoring().memoryReporting().status().freeze_owner_count == 0);
	assert(session.memory_workbench.following);
	draw(0, 0, 420);
	draw(0, 0, 420);
	std::cout << "Memory workbench catalogue, controls, independent held history and real Vulkan "
				 "frames verified\n";
}
