#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevContiguousTimelineStrip.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevDrillDownTimelineCard.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineLayout.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#include "managers/ElementManager.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include <algorithm>
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
				FlowElementID control = {}, int pointer_event = 0, float pointer_x = -10,
				float pointer_y = -10, bool middle = false, float wheel = 0, bool focused = true,
				int key_code = 0) {
	require(app.beginFrame(), "begin frame");
	auto& manager = app.ui();
	auto& interaction = const_cast<InteractionSnapshot&>(manager.getPreviousFramesInteraction());
	interaction = {};
	auto& input = const_cast<FrameInput&>(manager.getCurrentFrameInput());
	input.mouseDown[0] = pointer_event == 1 || pointer_event == 3;
	input.mouseX = pointer_x;
	input.mouseY = pointer_y;
	input.mouseDown[2] = middle;
	input.scrollY = wheel;
	input.windowFocused = focused;
	input.keyDown = {};
	if (key_code)
		input.keyDown[size_t(key_code)] = true;
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
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevTimelineState state;
	DevPerformanceSelection selection;
	selection.selected_scope = {1, DevPerformanceScopeKind::Window};
	auto& snapshot = state.snapshot;
	snapshot.start_ns = 1'000'000'000;
	snapshot.end_ns = 1'064'000'000;
	snapshot.labels.reserve(64);
	std::array<CpuTimingRecord, 8> measured_samples{};
	snapshot.frames.reserve(4);
	snapshot.frame_metrics.reserve(4);
	snapshot.blocks.reserve(12);
	for (size_t frame_index = 0; frame_index < 4; ++frame_index) {
		const uint64_t start = snapshot.start_ns + frame_index * 16'000'000;
		TimelineBlockSlice frame;
		snapshot.labels.emplace_back("Frame #" + std::to_string(100 + frame_index));
		frame.label = snapshot.labels.back();
		frame.start_ns = start;
		frame.duration_ns = 16'000'000;
		frame.frame = {1, 100 + frame_index};
		frame.app_tick = frame_index + 1;
		snapshot.frames.emplace_back(frame);
		snapshot.frame_metrics.emplace_back(frame.duration_ns);
		TimelineBlockSlice parent = frame;
		parent.label = "User build";
		parent.duration_ns = 10'000'000;
		measured_samples[frame_index * 2].durationNs = 2'000'000;
		parent.cpu_sample = {&measured_samples[frame_index * 2], 1};
		parent.category = TimingCategory::Frame;
		parent.track = 1;
		snapshot.blocks.emplace_back(parent);
		TimelineBlockSlice child = parent;
		child.label = "Workbench container";
		child.start_ns += 2'000'000;
		child.duration_ns = 8'000'000;
		measured_samples[frame_index * 2 + 1].durationNs = 8'000'000;
		child.cpu_sample = {&measured_samples[frame_index * 2 + 1], 1};
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
	draw_frame(app, state, selection);
	const auto cached_stats = app.ui().timeline_controller()->stats();
	require(cached_stats.surfaces == 2 && cached_stats.layout_builds == 0 &&
				cached_stats.base_builds == 0,
			"Paused geometry and glyph batches are reused across frame slots");
	std::cout << "Cached baseline: " << cached_stats.surfaces << " surfaces, "
			  << cached_stats.instances << " instances, " << cached_stats.runs << " runs, "
			  << cached_stats.uploaded_bytes << " upload bytes\n";
	const auto header_bounds = bounds(app, FlowElementID{.value = header_id.value});
	const auto macro_bounds = bounds(app, FlowElementID{.value = macro_id.value});
	require(header_bounds.height == 154 && macro_bounds.y >= header_bounds.y + 154,
			"Header stays above the timeline");
	draw_frame(app, state, selection, {}, 0, -10, -10, false, 0, true, 32);
	require(state.zoom == 4, "Space does not change sealed investigation mode");
	click(app, state, selection, control_id(header_id, 11));
	require(state.zoom == 2, "2x zoom button callback");
	click(app, state, selection, control_id(macro_id, 1));
	require(state.zoom == 1 && state.active_depth == 1,
			"Major Fit history replaces depth controls");
	// The viewport has no per-sample Flow IDs. Click the submitted image coordinates.
	const auto macro_layout =
		build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, macro_bounds.width);
	const auto sample = std::ranges::find_if(macro_layout.items, [](const auto& item) {
		return item.command.action == TimelineAction::Open;
	});
	require(sample != macro_layout.items.end(), "Macro has sample geometry");
	const float sample_x = macro_bounds.x + sample->bounds.x + sample->bounds.width / 2;
	const float sample_y = macro_bounds.y + 28 + sample->bounds.y + sample->bounds.height / 2;
	draw_frame(app, state, selection, {}, 1, sample_x, sample_y);
	draw_frame(app, state, selection, {}, 2, sample_x, sample_y);
	draw_frame(app, state, selection);
	require(state.cards.size() == 1, "Sample click opens a drill-down card");
	draw_frame(app, state, selection);
	const auto card_bounds = bounds(app, FlowElementID{.value = card_id.value});
	require(card_bounds.width > 1100 && card_bounds.height > 80,
			"Drill-down card fills the timeline width");
	const auto root = state.cards[0].roots.front();
	require(snapshot.blocks[root].label == "User build",
			"Drill target identity survives callbacks");
	const auto card_layout =
		build_timeline_layout(state, selection, TimelineSurfaceKind::Card, 0, card_bounds.width);
	const auto child = std::ranges::find_if(card_layout.items, [](const auto& item) {
		return item.command.action == TimelineAction::Inspect &&
			   item.command.members.front() % 3 == 1;
	});
	require(child != card_layout.items.end(), "Card has child geometry");
	require(std::abs(child->bounds.x - (150 + (card_bounds.width - 150) * .2f)) < 2,
			"Child starts at its exact 20 percent offset");
	require(std::abs(child->bounds.width - (card_bounds.width - 150) * .8f) < 2,
			"Child occupies its exact 80 percent duration");
	require(state.cards[0].active_depth == 256, "Minor starts with recorded hierarchy visible");
	const float child_x = card_bounds.x + child->bounds.x + child->bounds.width / 2;
	const float child_y = card_bounds.y + 54 + child->bounds.y + child->bounds.height / 2;
	draw_frame(app, state, selection, {}, 1, child_x, child_y);
	draw_frame(app, state, selection, {}, 2, child_x, child_y);
	draw_frame(app, state, selection);

	require(state.cards.size() == 1 && state.inspected_sample == root + 1,
			"Child click inspects without changing minor focus");
	draw_frame(app, state, selection, {}, 0, -10, -10, false, 0, true, 265);
	draw_frame(app, state, selection);
	require(state.inspected_sample == root && state.cards.size() == 1,
			"Keyboard Up inspects the root without hover");
	draw_frame(app, state, selection, {}, 0, -10, -10, false, 0, true, 264);
	draw_frame(app, state, selection);
	require(state.inspected_sample == root + 1 && state.cards.size() == 1,
			"Keyboard Down inspects a child without changing focus");
	click(app, state, selection, control_id(card_id, 40));
	require(state.cards.size() == 1, "Breadcrumb prunes descendants");
	click(app, state, selection, control_id(card_id, 3));
	require(state.cards.empty(), "Close prunes card chain");
	click(app, state, selection, control_id(header_id, 20));
	require(selection.hardware_domain == 1, "Hardware domain callback");

	// Middle-button capture continues outside the body and never opens a sample.
	state.zoom = 4;
	clamp_timeline_view(state);
	state.visible_start_ns = state.snapshot.start_ns + 10000000;
	draw_frame(app, state, selection);
	const auto before_pan = state.visible_start_ns;
	draw_frame(app, state, selection, {}, 0, 400, sample_y, true);
	draw_frame(app, state, selection, {}, 0, -100, sample_y, true);
	require(state.visible_start_ns > before_pan, "Middle drag pans outside viewport bounds");
	draw_frame(app, state, selection, {}, 0, -100, sample_y);
	require(state.cards.empty(), "Middle drag never opens samples");
	const auto zoom_before = state.zoom;
	draw_frame(app, state, selection, {}, 0, 600, sample_y, false, .25f);
	require(state.zoom > zoom_before && state.zoom < zoom_before * 1.2,
			"Fractional wheel zoom is bounded");
	// Scope replacement and focus loss invalidate a pressed sample.
	draw_frame(app, state, selection, {}, 1, 600, sample_y);
	++state.snapshot_revision;
	draw_frame(app, state, selection, {}, 2, 600, sample_y);
	require(state.pending.action == TimelineAction::None && state.cards.empty(),
			"Stale revision cannot activate");
	draw_frame(app, state, selection, {}, 1, 600, sample_y);
	draw_frame(app, state, selection, {}, 2, 600, sample_y, false, 0, false);
	require(state.pending.action == TimelineAction::None && state.cards.empty(),
			"Focus loss cancels activation");
	const float minimap_y = header_bounds.y + 32 + 30;
	draw_frame(app, state, selection, {}, 1, 600, minimap_y);
	draw_frame(app, state, selection, {}, 1, 900, minimap_y);
	const auto scrubbed_start = state.visible_start_ns;
	draw_frame(app, state, selection, {}, 2, 900, minimap_y);
	require(state.visible_start_ns == scrubbed_start &&
				state.pending.action == TimelineAction::None,
			"Minimap drag has no competing release activation");
	// Clay geometry remains bounded when retained samples and frames increase tenfold.
	const auto original_snapshot = state.snapshot;
	size_t sparse_clay_count = 0;
	for (const size_t sample_count : {1000u, 10000u}) {
		state.snapshot.blocks.clear();
		state.snapshot.frames.clear();
		state.snapshot.frame_metrics.clear();
		state.snapshot.blocks.reserve(sample_count);
		state.snapshot.frames.reserve(sample_count);
		state.snapshot.frame_metrics.reserve(sample_count);
		state.snapshot.start_ns = 1000000000;
		state.snapshot.end_ns = 1064000000;
		for (size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
			TimelineBlockSlice sample{.label = "Dense sample",
									  .start_ns =
										  1000000000 + sample_index * 64000000 / sample_count,
									  .duration_ns = 64000000 / sample_count,
									  .track = 1};
			state.snapshot.blocks.emplace_back(sample);
			state.snapshot.frames.emplace_back(sample);
			state.snapshot.frame_metrics.emplace_back(sample.duration_ns);
		}
		++state.snapshot_revision;
		state.zoom = 1;
		clamp_timeline_view(state);
		draw_frame(app, state, selection);
		draw_frame(app, state, selection);
		draw_frame(app, state, selection);
		const auto count = app.ui().devTreeSnapshot().clay.nodes.size();
		if (!sparse_clay_count)
			sparse_clay_count = count;
		require(count == sparse_clay_count, "Clay element count is independent of sample count");
		std::cout << sample_count << " retained samples: " << count << " Clay nodes\n";
	}
	state.snapshot = original_snapshot;
	++state.snapshot_revision;
	clamp_timeline_view(state);
	// Exercise the actual orchestrator: retained snapshots and pinned/scrolling regions.
	constexpr auto workbench_id = Global<kDevPerformanceWorkbench>("timeline.test.workbench");
	DevInterfaceState interface_state;
	interface_state.performance_selection = selection;
	const auto draw_workbench = [&](float wheel = 0, FlowElementID control = {},
									int pointer_event = 0, float pointer_y = -10) {
		require(app.beginFrame(), "begin Workbench frame");
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.mouseX = wheel || control ? 600 : -10;
		input.mouseY = control ? pointer_y : wheel ? 220 : -10;
		input.scrollY = wheel;
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseDown[0] = pointer_event == 1 || pointer_event == 3;
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		if (control) {
			const auto control_clay = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(control_clay);
			if (pointer_event == 1)
				interaction.pressedElementIds.emplace_back(control_clay);
			if (pointer_event == 2)
				interaction.releasedElementIds.emplace_back(control_clay);
		}
		Clay_ElementDeclaration workbench_row{};
		workbench_row.backgroundColor = interface_theme::kDepth0Keel;
		workbench_row.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		CLAY(CLAY_ID("timeline-workbench-test"), workbench_row) {
			app.ui()
				.createElement(kDevPerformanceWorkbench, workbench_id)
				.setParameters({&app, &interface_state})
				.draw();
			Clay_ElementDeclaration inspector_panel{};
			inspector_panel.backgroundColor = interface_theme::kDepth1Panel;
			inspector_panel.layout.sizing = {.width = CLAY_SIZING_FIXED(280),
											 .height = CLAY_SIZING_GROW(0)};
			CLAY(CLAY_ID("timeline-inspector-test"), inspector_panel) {
				app.ui()
					.createElement(kDevPerformanceInspector,
								   Global<kDevPerformanceInspector>("timeline.test.inspector"))
					.setParameters({&app, &interface_state})
					.draw();
			}
		}
		require(app.endFrame(), "end Workbench frame");
		require(app.drawFrame(), "render Workbench frame");
	};
	auto& reporting = app.devMonitoring().timingReporting();
	require(reporting.begin_capture(1) == 1, "seed sealed generation");
	reporting.admit_tick(1, 100);
	reporting.note_tick_boundary(2, 200);
	reporting.stop_capture(2);
	reporting.seal_capture();
	draw_workbench();
	auto* retained = &interface_state.performance_timeline;
	require(retained, "Workbench owns timeline state");
	retained->snapshot = snapshot;
	retained->capture_generation = 1;
	retained->selection = interface_state.performance_selection;
	++retained->snapshot_revision;
	retained->origin_ns = snapshot.start_ns;
	retained->cards.assign(7, TimelineCard{{0}, 256});
	retained->inspected_sample = 1;
	retained->selected_frame = 0;
	retained->minimap_follow_selection = true;
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
	require(pinned_before.y == pinned_after.y && pinned_after.height == 154,
			"Pinned header remains fixed while canvas reveals cards");
	require(app.ui().timeline_controller()->stats().surfaces == 3,
			"Navigation history renders exactly minimap, major and one minor viewport");
	require(retained && retained->snapshot.frames.front().frame.frameNumber == 100 &&
				retained->cards.size() == 7,
			"Paused snapshot survives new live reports");
	// Deep hierarchy scrolling keeps the toolbar and ruler on screen and restores Back state.
	const auto shallow_snapshot = retained->snapshot;
	const auto shallow_cards = retained->cards;
	size_t ancestor_index = 1;
	retained->snapshot.blocks.reserve(retained->snapshot.blocks.size() + 20);
	for (size_t depth_index = 0; depth_index < 20; ++depth_index) {
		auto nested = retained->snapshot.blocks[1];
		retained->snapshot.labels.emplace_back("Nested zone " + std::to_string(depth_index + 1));
		nested.label = retained->snapshot.labels.back();
		nested.duration_ns = 1000000;
		nested.parent = ancestor_index;
		ancestor_index = retained->snapshot.blocks.size();
		retained->snapshot.blocks.emplace_back(std::move(nested));
	}
	++retained->snapshot_revision;
	draw_workbench();
	draw_workbench();
	draw_workbench();
	const auto minor_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = workbench_id.value}, DevDrillDownTimelineCard::definitionId,
		LocalElementName{"minor"}.token);
	const auto plot_id = ::FlowUi::detail::element_id::resolveLocal(
		minor_id, DevDrillDownTimelineCard::definitionId, LocalElementName{"plot-scroll"}.token);
	auto plot_scroll = Clay_GetScrollContainerData(app.ui().toClayEID(plot_id));
	require(plot_scroll.found &&
				plot_scroll.contentDimensions.height > plot_scroll.scrollContainerDimensions.height,
			"Deep minor hierarchy has an independent scroll container");
	const auto minor_before_scroll = bounds(app, minor_id);
	const auto major_start_before_scroll = retained->visible_start_ns;
	plot_scroll.scrollPosition->y = -200;
	draw_workbench();
	draw_workbench();
	require(bounds(app, minor_id).y == minor_before_scroll.y &&
				retained->cards.back().scroll_y == -200,
			"Minor toolbar remains fixed while hierarchy content scrolls");
	retained->pending = {
		{1}, TimelineAction::Open, retained->cards.size(), 0, retained->snapshot_revision};
	draw_workbench();
	draw_workbench();
	require(retained->cards.back().scroll_y == 0,
			"New minor focus begins with its own scroll position");
	retained->pending = {{}, TimelineAction::Close, retained->cards.size() - 1};
	draw_workbench();
	draw_workbench();
	require(retained->cards.back().scroll_y == -200 &&
				retained->visible_start_ns == major_start_before_scroll,
			"Back restores hierarchy scroll and preserves the major viewport");
	retained->snapshot = shallow_snapshot;
	retained->cards = shallow_cards;
	++retained->snapshot_revision;
	retained->restore_minor_scroll = true;
	draw_workbench();
	draw_workbench();
	draw_workbench();
	const auto divider_id = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{.value = workbench_id.value}, FSEL::SplitterHandle::definitionId,
		LocalElementName{"timeline-divider"}.token);
	const auto divider_bounds = bounds(app, divider_id);
	require(divider_bounds.width > 500 && divider_bounds.height > 0,
			"Horizontal FSEL divider fills workbench width");
	const auto previous_major_height = retained->major_height;
	draw_workbench(0, divider_id, 1, divider_bounds.y + 2);
	draw_workbench(0, divider_id, 3, divider_bounds.y + 42);
	draw_workbench(0, divider_id, 2, divider_bounds.y + 42);
	require(retained->major_height > previous_major_height + 30,
			"FSEL drag resizes the major and minor panes");
	retained->cards.clear();
	retained->reveal_frames = 0;
	draw_workbench();
	draw_workbench();
	draw_workbench();
	const auto live_zoom = retained->zoom;
	draw_workbench(1);
	require(retained->zoom > live_zoom &&
				retained->snapshot.frames.front().frame.frameNumber == 100,
			"Wheel navigation preserves the sealed snapshot");
	std::cout << "Timeline rendering and interaction checks passed\n";
}
