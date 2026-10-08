#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/Button.hpp"
#include "FSEL/NumberInput.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
constexpr auto inspector_id = Global<kDevPerformanceInspector>("inspector.visual.test");
void require(bool condition, const char* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
[[nodiscard]] FlowElementID
child_id(LocalElementName name,
		 FlowDefinitionID definition = DevPerformanceInspector::definitionId) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(FlowElementID{.value = inspector_id.value},
													  definition, name.token);
}
[[nodiscard]] Clay_BoundingBox find_bounds(const App& app, uint32_t clay_id) {
	for (const auto& node : app.ui().devTreeSnapshot().clay.nodes)
		if (node.clayId == clay_id)
			return node.bounds;
	return {};
}
void check_bounds(App& app, float width) {
	const auto& snapshot = app.ui().devTreeSnapshot();
	const auto root_id = app.ui().toClayEID(FlowElementID{.value = inspector_id.value}).id;
	const auto panel = find_bounds(app, root_id);
	require(std::abs(panel.width - width) < 1 && panel.x + panel.width <= 1400.5f,
			"Inspector stays inside its fixed column and screen");
	for (const auto& node : snapshot.clay.nodes) {
		auto owner = node.directFlowOwner;
		bool inspector_child = false;
		while (owner != tooling::InvalidFlowNode && owner < snapshot.flow.nodes.size()) {
			if (snapshot.flow.nodes[owner].instance.value == inspector_id.value) {
				inspector_child = true;
				break;
			}
			owner = snapshot.flow.nodes[owner].parent;
		}
		if (!inspector_child ||
			tooling::hasFlag(node.flags, tooling::DevClayNodeFlag::BoundsUnavailable))
			continue;
		if (node.bounds.x < panel.x - 1 ||
			node.bounds.x + node.bounds.width > panel.x + panel.width + 1) {
			std::cerr << "Overflow at " << width << " px: " << snapshot.string(node.idString)
					  << " text=" << snapshot.string(node.text) << " x=" << node.bounds.x
					  << " width=" << node.bounds.width << " panel=" << panel.x << ','
					  << panel.width << '\n';
			require(false, "Inspector child does not overflow horizontally");
		}
	}
}

void check_numeric_center(App& app) {
	const auto input_id =
		child_id(LocalElementName{"capture-duration"}, FSEL::NumberInput<float>::definitionId);
	const auto content_id = FlowIDToClayID(
		PartID(FSEL::kNumberInputFloat, input_id, FSEL::NumberInput<float>::Parts::content));
	const auto text_id = FlowIDToClayID(
		PartID(FSEL::kNumberInputFloat, input_id, FSEL::NumberInput<float>::Parts::text));
	const auto content = find_bounds(app, content_id);
	const auto value = find_bounds(app, text_id);
	require(content.width > 0 && value.width > 0, "Numeric field emits centered text");
	require(std::abs((content.x + content.width / 2) - (value.x + value.width / 2)) < 2,
			"Numeric text is horizontally centered in its editable field");
	require(std::abs((content.y + content.height / 2) - (value.y + value.height / 2)) < 2,
			"Numeric text is vertically centered in its editable field");
}
void preview_marker(const App& app, float width, const char* phase) {
	std::cout << "Inspector preview: frame=" << app.ui().devTreeSnapshot().frameNumber
			  << " width=" << int(width) << " phase=" << phase << '\n';
}

} // namespace
int main() {
	AppConfig config;
	config.window.title = "Performance Inspector visual verification";
	config.window.width = 1400;
	config.window.height = 900;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	session.capture_windows.emplace_back(
		DevWindowInfo{1, "Example application with a deliberately long descriptive window title"});
	std::array descriptors{
		makeTimingDescriptor(
			TimingCategory::User, TimingZoneRole::Work,
			"application.prepare_complex_document_layout",
			TimingSourceLocation{
				"project/components/layout/DocumentLayoutPreparation.cpp",
				"application::DocumentLayoutPreparation::rebuild_all_visible_document_sections",
				184}),
		makeTimingDescriptor(TimingCategory::Layout, TimingZoneRole::Work,
							 "application.layout_document_section"),
		timing_zones::kWindowFrameTotal,
		makeTimingDescriptor(TimingCategory::Gpu, TimingZoneRole::GpuWork,
							 "application.render_complex_document_sections")};
	std::vector<TimingAppTickReport> reports(120);
	for (size_t tick_position = 0; tick_position < reports.size(); ++tick_position) {
		auto& tick = reports[tick_position];
		tick.appTick = tick_position + 1;
		tick.occupied = true;
		tick.boundary_start_ns = 1000000000 + tick_position * 8000000;
		tick.boundary_end_ns = tick.boundary_start_ns + 8000000 + (tick_position % 9) * 100000;
		tick.windows.resize(1);
		auto& window = tick.windows.front();
		window.window = 1;
		window.occupied = true;
		window.frames.resize(1);
		auto& frame = window.frames.front();
		frame.key = {1, tick.appTick};
		frame.occupied = true;
		frame.cpuZones.reserve(10);
		frame.cpuZones.emplace_back(CpuTimingRecord{.startNs = tick.boundary_start_ns,
													.durationNs = 3800000,
													.directChildNs = 2400000,
													.invocationId = tick.appTick * 10,
													.typeId = descriptors[0].typeId,
													.frame = frame.key,
													.appTick = tick.appTick,
													.track = 1});
		for (uint64_t child_position = 0; child_position < 8; ++child_position)
			frame.cpuZones.emplace_back(
				CpuTimingRecord{.startNs = tick.boundary_start_ns + 10000 + child_position * 350000,
								.durationNs = 300000,
								.invocationId = tick.appTick * 10 + child_position + 1,
								.parentInvocationId = tick.appTick * 10,
								.typeId = descriptors[1].typeId,
								.frame = frame.key,
								.appTick = tick.appTick,
								.track = 1});

		frame.gpuZones.reserve(2);
		frame.gpuZones.emplace_back(
			GpuTimingRecord{.durationNs = 850000,
							.timestamp_period_ns = .42,
							.timestamp_valid_bits = 48,
							.cpuAlignedStartNs = tick.boundary_start_ns + 100000,
							.submissionSerial = tick.appTick,
							.device_identity = 987654321012345678ULL,
							.queue_identity = 123456789012345678ULL,
							.typeId = descriptors[3].typeId,
							.frame = frame.key,
							.appTick = tick.appTick,
							.calibrationId = 8888888888888888ULL,
							.zone_index = 0,
							.queueFamilyIndex = 2,
							.flags = gpuTimingRecordFlags(GpuTimingRecordFlag::Completed)});
		frame.gpuZones.emplace_back(GpuTimingRecord{.startTick = 777777777777777777ULL,
													.durationTicks = 2000000,
													.durationNs = 840000,
													.timestamp_period_ns = .42,
													.timestamp_valid_bits = 48,
													.submissionSerial = tick.appTick,
													.device_identity = 987654321012345678ULL,
													.queue_identity = 123456789012345679ULL,
													.typeId = descriptors[3].typeId,
													.frame = frame.key,
													.appTick = tick.appTick,
													.zone_index = 0,
													.queueFamilyIndex = 2});
	}
	TimingCaptureReadView fixture;
	fixture.first = reports;
	fixture.descriptors = descriptors;
	fixture.generation = 1;
	float width = 240;
	const auto draw = [&](FlowElementID control = {}, int event = 0, bool numeric = false) {
		require(bool(app.beginFrame()), "begin inspector frame");
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseX = -10;
		input.mouseY = -10;
		if (control) {
			const auto clay_id = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(clay_id);
			if (event == 1) {
				interaction.pressedElementIds.emplace_back(clay_id);
				input.mouseDown[0] = true;
			}
			if (event == 2)
				interaction.releasedElementIds.emplace_back(clay_id);
			if (numeric) {
				const auto bounds = find_bounds(app, clay_id);
				input.mouseX = bounds.x + bounds.width / 2;
				input.mouseY = bounds.y + bounds.height / 2;
			}
		}
		Clay_ElementDeclaration root{};
		root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		root.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(CLAY_ID("inspector-verification"), root) {
			Clay_ElementDeclaration workbench_column{};
			workbench_column.layout.sizing = {.width = CLAY_SIZING_FIXED(1400 - width),
											  .height = CLAY_SIZING_GROW(0)};
			workbench_column.clip.horizontal = true;
			workbench_column.clip.scrollInputDisabled = true;
			CLAY(CLAY_ID("inspector-workbench-column"), workbench_column) {
				app.ui()
					.createElement(kDevPerformanceWorkbench,
								   Global<kDevPerformanceWorkbench>("inspector.visual.workbench"))
					.setParameters({&app, &session, CLAY_ID("inspector-workbench-column")})
					.draw();
			}
			Clay_ElementDeclaration panel{};
			panel.layout.sizing = {.width = CLAY_SIZING_FIXED(width),
								   .height = CLAY_SIZING_GROW(0)};
			CLAY(CLAY_ID("inspector-verification-column"), panel) {
				app.ui()
					.createElement(kDevPerformanceInspector, inspector_id)
					.setParameters({&app, &session})
					.draw();
			}
		}
		require(bool(app.endFrame()), "end inspector frame");
		require(bool(app.drawFrame()), "render inspector frame");
	};
	for (int frame_count = 0; frame_count < 4; ++frame_count)
		draw();
	preview_marker(app, width, "empty");
	auto& reporting = app.devMonitoring().timingReporting();
	require(reporting.begin_capture(1) == 1, "seed retained generation");
	reporting.admit_tick(1, 1000000000);
	reporting.note_tick_boundary(2, 1008000000);
	reporting.stop_capture(2);
	reporting.seal_capture();
	draw();
	auto* state = app.elements().getStatePointer(kDevPerformanceInspector, MainWindowId,
												 FlowElementID{.value = inspector_id.value});
	require(state != nullptr, "inspector state exists");
	state->analysis = analyze_performance(fixture, 0, 0);
	state->analysis_target_ms = 4;
	state->refresh_gate.frames_remaining = 240;
	state->cached_range = state->analysis_range;
	state->cached_source = state->analysis_source;
	auto& timeline = session.performance_timeline;
	timeline.snapshot = extract_timeline(fixture, {});
	timeline.capture_generation = 1;
	++timeline.snapshot_revision;
	timeline.selection = session.performance_selection;
	timeline.origin_ns = timeline.snapshot.start_ns;
	timeline.inspected_sample = 0;
	timeline.cards = {TimelineCard{{0}, 256}};
	for (const float test_width : {240.0f, 560.0f}) {
		width = test_width;
		state->capture_expanded = true;
		state->rolling_expanded = true;
		state->selected_expanded = true;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		check_numeric_center(app);
		preview_marker(app, width, "capture");
		session.capture_start_mode = 1;
		session.capture_end_mode = 1;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, "budget");
		session.capture_start_mode = 0;
		session.capture_end_mode = 2;
		state->capture_expanded = false;
		state->rolling_expanded = true;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, "rolling");
		state->rolling_expanded = false;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, "selected");
		state->technical_details = true;
		state->all_children = true;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, "technical");

		const auto gpu = std::ranges::find_if(timeline.snapshot.blocks, [](const auto& block) {
			return block.domain == TimingSampleDomain::Gpu && !block.cpu_clock_aligned;
		});
		require(gpu != timeline.snapshot.blocks.end(), "Local-clock GPU fixture exists");
		timeline.inspected_sample = size_t(gpu - timeline.snapshot.blocks.begin());
		state->technical_details = false;
		state->all_children = false;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, "gpu");
		state->technical_details = true;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		auto gpu_scroll = Clay_GetScrollContainerData(
			Clay_ElementId{.id = FlowIDToClayID(child_id(LocalElementName{"scroll-body"}))});
		require(gpu_scroll.found && gpu_scroll.scrollPosition,
				"Inspector owns an independent vertical scroll");
		gpu_scroll.scrollPosition->y = -600;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, "gpu_technical");
		timeline.inspected_sample = 0;
		gpu_scroll.scrollPosition->y = 0;
		state->technical_details = false;
		state->all_children = false;
	}
	require(state->analysis.samples.size() == 1320,
			"Rolling values remain held while frames count down");
	// Disclosure and navigation are actual release-driven interactions.
	const auto rolling_header =
		child_id(LocalElementName{"rolling-header"}, FSEL::Button::definitionId);
	draw(rolling_header, 1);
	draw(rolling_header, 2);
	require(state->rolling_expanded, "Rolling Stats header expands through interaction");
	state->rolling_expanded = false;
	timeline.inspected_sample = 1;
	draw();
	const auto focus =
		child_id(LocalElementName{"focus"}, DevPerformanceInspectorAction::definitionId);
	const auto previous_cards = timeline.cards.size();
	draw(focus, 1);
	draw(focus, 2);
	draw();
	require(timeline.cards.size() == previous_cards + 1, "Selected-zone focus opens a minor card");
	state->refresh_gate.frames_remaining = 1;
	draw();
	require(state->analysis.samples.empty() && state->analysis.complete_ticks == 1,
			"Displayed analysis refreshes when its countdown expires");
	state->analysis_range = 120;
	draw();
	require(state->refresh_gate.frames_remaining == 240,
			"Changing analysis context refreshes immediately");
	// Render zero-duration and over-budget bars beside one another, in both selector modes.
	timeline.snapshot.frames.clear();
	timeline.snapshot.frame_metrics.clear();
	const std::array<uint64_t, 6> minimap_metrics{0, 4'000'000, 16'000'000, 33'300'000,
												34'000'000, 40'000'000};
	timeline.snapshot.frames.reserve(minimap_metrics.size());
	timeline.snapshot.frame_metrics.reserve(minimap_metrics.size());
	for (size_t frame_index = 0; frame_index < minimap_metrics.size(); ++frame_index) {
		auto frame_sample = timeline.snapshot.blocks.front();
		frame_sample.start_ns = timeline.snapshot.start_ns + frame_index * 40'000'000;
		frame_sample.duration_ns = minimap_metrics[frame_index];
		timeline.snapshot.frames.emplace_back(frame_sample);
		timeline.snapshot.frame_metrics.emplace_back(minimap_metrics[frame_index]);
	}
	timeline.minimap_zoom = 1;
	timeline.minimap_maximum_ns = 40'000'000;
	timeline.minimap_follow_selection = false;
	timeline.selected_frame = 0;
	timeline.cards.clear();
	for (const int selector_mode : {1, 0}) {
		session.performance_selection.selector_mode = selector_mode;
		session.performance_selection.selected_zone = timeline.snapshot.blocks.front().type_id;
		timeline.selection = session.performance_selection;
		++timeline.snapshot_revision;
		for (int frame_count = 0; frame_count < 4; ++frame_count)
			draw();
		check_bounds(app, width);
		preview_marker(app, width, selector_mode ? "minimap_zone" : "minimap_frames");
	}
	std::cout << "Inspector 240/560 px containment, wrapping, centered inputs, refresh gate and "
				 "navigation passed\n";
}
