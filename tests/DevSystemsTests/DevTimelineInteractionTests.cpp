#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevContiguousTimelineStrip.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevDrillDownTimelineCard.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#include "managers/ElementManager.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
template <class Value>
void require(const Value& value, const char* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
constexpr auto header_id = Global<kDevWorkbenchHeader>("timeline.test.header");
constexpr auto macro_id = Global<kDevContiguousTimelineStrip>("timeline.test.macro");
constexpr auto card_id = Global<kDevDrillDownTimelineCard>("timeline.test.card");
[[nodiscard]] FlowElementID control_id(GlobalFlowID owner, uint64_t index) {
	return ::FlowUi::detail::element_id::resolveLocal(FlowElementID{.value = owner.value},
													  DevTimelineButton::definitionId,
													  Keyed("control", index).token);
}
[[nodiscard]] Clay_BoundingBox bounds(App& app, FlowElementID element) {
	const auto clay_id = app.ui().toClayEID(element).id;
	for (const auto& node : app.ui().devTreeSnapshot().clay.nodes)
		if (node.clayId == clay_id)
			return node.bounds;
	return {};
}
void draw_frame(App& app, DevTimelineState& state, DevPerformanceSelection& selection,
				FlowElementID control = {}, int pointer_event = 0) {
	require(app.beginFrame(), "begin frame");
	auto& manager = app.ui();
	auto& interaction = const_cast<InteractionSnapshot&>(manager.getPreviousFramesInteraction());
	interaction = {};
	auto& input = const_cast<FrameInput&>(manager.getCurrentFrameInput());
	input.mouseDown[0] = pointer_event == 1;
	input.mouseX = -10;
	input.mouseY = -10;
	if (control) {
		const auto clay_id = manager.toClayEID(control).id;
		interaction.hoveredElementIds.emplace_back(clay_id);
		if (pointer_event == 1)
			interaction.pressedElementIds.emplace_back(clay_id);
		if (pointer_event == 2)
			interaction.releasedElementIds.emplace_back(clay_id);
	}
	apply_timeline_command(state, selection);
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 12;
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(CLAY_ID("timeline-test-root"), root) {
		manager.createElement(kDevWorkbenchHeader, header_id)
			.setParameters({&state, &selection})
			.draw();
		manager.createElement(kDevContiguousTimelineStrip, macro_id)
			.setParameters({&state, &selection})
			.draw();
		if (!state.cards.empty())
			manager.createElement(kDevDrillDownTimelineCard, card_id)
				.setParameters({&state, &selection, 0})
				.draw();
	}
	require(app.endFrame(), "end frame");
	require(app.drawFrame(), "render frame");
}
void click(App& app, DevTimelineState& state, DevPerformanceSelection& selection,
		   FlowElementID control) {
	draw_frame(app, state, selection, control, 1);
	draw_frame(app, state, selection, control, 2);
	draw_frame(app, state, selection);
}
} // namespace
int main() {
	AppConfig config{};
	config.window.title = "Timeline integration verification";
	config.window.width = 1200;
	config.window.height = 800;
	config.vk.enableValidation = false;
	auto app = makeApplication(config);
	DevTimelineState state;
	DevPerformanceSelection selection;
	selection.selected_scope = {1, DevPerformanceScopeKind::Window};
	auto& snapshot = state.snapshot;
	snapshot.start_ns = 1'000'000'000;
	snapshot.end_ns = 1'064'000'000;
	snapshot.frames.reserve(4);
	snapshot.frame_metrics.reserve(4);
	snapshot.blocks.reserve(12);
	for (size_t frame_index = 0; frame_index < 4; ++frame_index) {
		const uint64_t start = snapshot.start_ns + frame_index * 16'000'000;
		TimelineBlockSlice frame;
		frame.label = "Frame #" + std::to_string(100 + frame_index);
		frame.start_ns = start;
		frame.duration_ns = 16'000'000;
		frame.frame = {1, 100 + frame_index};
		frame.app_tick = frame_index + 1;
		snapshot.frames.emplace_back(frame);
		snapshot.frame_metrics.emplace_back(frame.duration_ns);
		TimelineBlockSlice parent = frame;
		parent.label = "User build";
		parent.duration_ns = 10'000'000;
		parent.exclusive_ns = 2'000'000;
		parent.category = TimingCategory::Frame;
		parent.track = 1;
		snapshot.blocks.emplace_back(parent);
		TimelineBlockSlice child = parent;
		child.label = "Workbench container";
		child.start_ns += 2'000'000;
		child.duration_ns = 8'000'000;
		child.exclusive_ns = 8'000'000;
		child.parent = snapshot.blocks.size() - 1;
		child.category = TimingCategory::Element;
		snapshot.blocks.emplace_back(child);
		TimelineBlockSlice gpu = parent;
		gpu.label = "UI GPU pass";
		gpu.start_ns += 10'000'000;
		gpu.duration_ns = 4'000'000;
		gpu.domain = TimingSampleDomain::Gpu;
		gpu.track = 0;
		snapshot.blocks.emplace_back(gpu);
	}
	clamp_timeline_view(state);
	draw_frame(app, state, selection);
	draw_frame(app, state, selection);
	const auto header_bounds = bounds(app, FlowElementID{.value = header_id.value});
	const auto macro_bounds = bounds(app, FlowElementID{.value = macro_id.value});
	require(header_bounds.height == 106 && macro_bounds.y >= header_bounds.y + 106,
			"Header stays above the timeline");
	click(app, state, selection, control_id(header_id, 1));
	require(state.paused, "Pause button callback");
	click(app, state, selection, control_id(header_id, 11));
	require(state.zoom == 2, "2x zoom button callback");
	click(app, state, selection, control_id(macro_id, 1));
	require(state.active_depth == 2, "In-place macro depth callback");
	// Select a real timeline sample using its stable Flow ID.
	const auto lane_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = macro_id.value}, DevContiguousTimelineStrip::definitionId,
		Indexed("lane", 0).token);
	const auto sample_key = uint64_t(app.ui().toClayEID(lane_id).id) << 32;
	const auto sample_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = macro_id.value}, DevTimelineButton::definitionId,
		Keyed("sample", sample_key).token);
	click(app, state, selection, sample_id);
	require(state.cards.size() == 1, "Sample click opens a drill-down card");
	draw_frame(app, state, selection);
	const auto card_bounds = bounds(app, FlowElementID{.value = card_id.value});
	require(card_bounds.width > 1100 && card_bounds.height > 80,
			"Drill-down card fills the timeline width");
	const auto root = state.cards[0].roots.front();
	require(snapshot.blocks[root].label == "User build",
			"Drill target identity survives callbacks");
	const auto card_lane = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = card_id.value}, DevDrillDownTimelineCard::definitionId,
		Indexed("lane", 0).token);
	const auto child_key = uint64_t(app.ui().toClayEID(card_lane).id) << 32;
	const auto child_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = card_id.value}, DevTimelineButton::definitionId,
		Keyed("sample", child_key).token);
	const auto child_bounds = bounds(app, child_id);
	require(std::abs(child_bounds.x - card_bounds.x - card_bounds.width * .2f) < 2,
			"Child starts at its exact 20 percent offset");
	require(std::abs(child_bounds.width - card_bounds.width * .8f) < 2,
			"Child occupies its exact 80 percent duration");
	click(app, state, selection, control_id(card_id, 1));
	require(state.cards[0].active_depth == 2, "Card depth callback");
	click(app, state, selection, child_id);
	require(state.cards.size() == 2, "Child sample chains a card");
	click(app, state, selection, control_id(header_id, 40));
	require(state.cards.size() == 1, "Breadcrumb prunes descendants");
	click(app, state, selection, control_id(card_id, 3));
	require(state.cards.empty(), "Close prunes card chain");
	click(app, state, selection, control_id(header_id, 20));
	require(selection.hardware_domain == 1, "Hardware domain callback");
	click(app, state, selection, control_id(header_id, 1));
	require(!state.paused, "Play callback");
	// Exercise the actual orchestrator: retained snapshots and pinned/scrolling regions.
	constexpr auto workbench_id = Global<kDevPerformanceWorkbench>("timeline.test.workbench");
	DevInterfaceState interface_state;
	interface_state.performance_selection = selection;
	const auto draw_workbench = [&] {
		require(app.beginFrame(), "begin Workbench frame");
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.mouseX = -10;
		input.mouseY = -10;
		input.mouseDown = {};
		app.ui()
			.createElement(kDevPerformanceWorkbench, workbench_id)
			.setParameters({&app, &interface_state})
			.draw();
		require(app.endFrame(), "end Workbench frame");
		require(app.drawFrame(), "render Workbench frame");
	};
	draw_workbench();
	auto* retained = app.ui().elements().getStatePointer(
		kDevPerformanceWorkbench, app.ui().windowId(), FlowElementID{.value = workbench_id.value});
	require(retained, "Workbench owns timeline state");
	retained->snapshot = snapshot;
	retained->origin_ns = snapshot.start_ns;
	retained->paused = true;
	retained->cards.assign(7, TimelineCard{{0}, 1});
	retained->reveal_frames = 12;
	clamp_timeline_view(*retained);
	draw_workbench();
	const auto pinned_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = workbench_id.value}, DevWorkbenchHeader::definitionId,
		LocalElementName{"header"}.token);
	const auto pinned_before = bounds(app, pinned_id);
	for (int frame_index = 0; frame_index < 14; ++frame_index)
		draw_workbench();
	const auto pinned_after = bounds(app, pinned_id);
	require(pinned_before.y == pinned_after.y && pinned_after.height == 106,
			"Pinned header remains fixed while canvas reveals cards");
	const auto canvas_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = workbench_id.value}, DevPerformanceWorkbench::definitionId,
		LocalElementName{"canvas"}.token);
	const auto scroll = Clay_GetScrollContainerData(app.ui().toClayEID(canvas_id));
	require(scroll.found && scroll.scrollPosition->y < -100, "Canvas scrolls to appended cards");
	retained = app.ui().elements().getStatePointer(kDevPerformanceWorkbench, app.ui().windowId(),
												   FlowElementID{.value = workbench_id.value});
	require(retained && retained->snapshot.frames.front().frame.frameNumber == 100 &&
				retained->cards.size() == 7,
			"Paused snapshot survives new live reports");
	std::cout << "Timeline rendering and interaction checks passed\n";
}
