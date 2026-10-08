#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/SplitterHandle.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevContiguousTimelineStrip.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevDrillDownTimelineCard.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include <algorithm>
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
namespace FlowUi::devSystems::interface_elements {
void DevPerformanceWorkbench::buildElement(BuildContext& context) {
	if (!context.params.interfaceState || !context.params.app)
		return;
	auto& state = context.params.interfaceState->performance_timeline;
	auto& selection = context.params.interfaceState->performance_selection;
	auto& reporting = context.params.app->devMonitoring().timingReporting();
	const auto status = reporting.status();
	auto capture = reporting.read_capture();
	if (capture.generation != state.capture_generation)
		state.pending = {};
	apply_timeline_command(state, selection);
	const bool scope_changed = state.selection.selected_scope != selection.selected_scope ||
							   state.selection.selected_zone != selection.selected_zone ||
							   state.selection.selector_mode != selection.selector_mode ||
							   state.selection.hardware_domain != selection.hardware_domain ||
							   state.selection.category_mask != selection.category_mask;
	const bool generation_changed = capture.generation != state.capture_generation;
	if (capture.generation && (generation_changed || scope_changed)) {
		++state.snapshot_revision;
		state.snapshot = extract_timeline(capture, selection);
		state.capture_generation = capture.generation;
		state.capture_event_ns =
			context.params.app->devMonitoring().performance_capture().status().event_end_ns;
		if (generation_changed) {
			const auto live_windows = context.params.app->devWindowSnapshot();
			auto& captured_windows = context.params.interfaceState->capture_windows;
			for (const auto segment : {capture.first, capture.second})
				for (const auto& tick : segment)
					for (const auto& window : tick.windows) {
						if (!window.occupied ||
							std::ranges::any_of(captured_windows, [&](const auto& known) {
								return known.id == window.window;
							}))
							continue;
						const auto live =
							std::ranges::find(live_windows, window.window, &DevWindowInfo::id);
						captured_windows.emplace_back(
							live != live_windows.end()
								? *live
								: DevWindowInfo{window.window,
												"Window " + std::to_string(window.window)});
					}
		}
		reset_timeline_minimap_scale(state, selection);
		if (generation_changed) {
			state.cards.clear();
			state.pending = {};
			state.inspected_sample = timeline_no_parent;
			state.origin_ns = state.snapshot.start_ns;
			state.selected_frame =
				state.snapshot.frames.empty() ? 0 : state.snapshot.frames.size() - 1;
			state.visible_start_ns = state.snapshot.end_ns;
			clamp_timeline_view(state);
		}
	}
	state.selection = selection;
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth0Keel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	const DevTimelineParameters parameters{&state, &selection, 0, context.clayID("canvas"),
										   context.params.column_clip};
	if (!capture.generation) {
		root.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
		CLAY(context.clayID(), root) {
			Clay_TextElementConfig message{};
			message.fontSize = 12;
			message.textColor = interface_theme::kTextSecondary;
			message.wrapMode = CLAY_TEXT_WRAP_WORDS;
			message.textAlignment = CLAY_TEXT_ALIGN_CENTER;
			CLAY_TEXT(context.uiManager.toClayString("Capture a snapshot to investigate."),
					  CLAY_TEXT_CONFIG(message));
		}
		return;
	}
	CLAY(context.clayID(), root) {
		const auto& progress = context.params.app->devMonitoring().performance_capture().status();
		auto snapshot_header = timeline_ui::row(0);
		snapshot_header.layout.sizing.height = CLAY_SIZING_FIT(0);
		snapshot_header.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		snapshot_header.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
		CLAY(context.clayID("snapshot-header"), snapshot_header) {
			timeline_ui::text(
				context, "Snapshot #" + std::to_string(capture.generation) + " · " +
							 std::to_string(status.retainedTickCount) + " ticks · " +
							 timeline_ui::milliseconds(progress.end_ns >= progress.start_ns
														   ? progress.end_ns - progress.start_ns
														   : 0));
		}
		if (status.capture_overwritten_ticks)
			timeline_ui::text(context, "Capture wrapped: oldest " +
										   std::to_string(status.capture_overwritten_ticks) +
										   " ticks overwritten; showing newest retained interval.");
		if (progress.event_end_ns)
			timeline_ui::text(
				context, "Budget event: " + timeline_ui::milliseconds(progress.event_duration_ns) +
							 " at AppTick #" + std::to_string(progress.event_app_tick) +
							 (progress.event_app_tick < status.oldestRetainedAppTick
								  ? " (outside retained history)"
								  : ""));
		if (status.capture_dropped_records || status.capture_ingestion_failures)
			timeline_ui::text(context,
							  "Capture quality: " + std::to_string(status.capture_dropped_records) +
								  " dropped producer records; " +
								  std::to_string(status.capture_ingestion_failures) +
								  " ingestion failures.");
		if (status.capture_gpu_failures)
			timeline_ui::text(
				context, "GPU capture quality: " + std::to_string(status.capture_gpu_failures) +
							 " query failures/unavailable results.");
		if (progress.stop_reason == PerformanceCaptureStopReason::Failure)
			timeline_ui::text(context, "Capture stopped: budget window became unavailable.");
		if (progress.incomplete)
			timeline_ui::text(
				context,
				"Incomplete capture: pending measurements exceeded finalization deadline.");
		context.uiManager.createElement(kDevWorkbenchHeader, LocalElementName{"header"})
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
		if (state.track_controls_open) {
			auto track_panel = root;
			track_panel.layout.sizing.height = CLAY_SIZING_FIXED(160);
			track_panel.clip = {.horizontal = true, .vertical = true};
			const auto track_scroll = Clay_GetScrollContainerData(context.clayID("track-settings"));
			if (track_scroll.found && track_scroll.scrollPosition)
				track_panel.clip.childOffset = *track_scroll.scrollPosition;
			CLAY(context.clayID("track-settings"), track_panel) {
				auto preferences = state.track_preferences;
				for (auto& preference : preferences)
					preference.hidden = false;
				const auto lanes =
					timeline_major_lanes(state.snapshot, state.selection, preferences);
				std::vector<size_t> row_indices;
				row_indices.reserve(lanes.size());
				for (size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
					const auto key = timeline_track_key(state.snapshot, lanes[lane_index]);
					if (std::ranges::none_of(row_indices, [&](size_t existing) {
							return timeline_track_key(state.snapshot, lanes[existing]) == key;
						}))
						row_indices.emplace_back(lane_index);
				}
				const auto control = [&](uint64_t control_key, std::string label,
										 TimelineAction action, std::vector<size_t> members) {
					context.uiManager
						.createElement(kDevTimelineButton, Keyed("track-control", control_key))
						.setParameters(DevTimelineButtonParameters{
							{std::move(members), action, 0, 0, state.snapshot_revision},
							std::move(label),
							&state})
						.setDevInternalCapture(true)
						.draw();
				};
				for (size_t row_index = 0; row_index < row_indices.size(); ++row_index) {
					const auto& lane = lanes[row_indices[row_index]];
					const auto root_index = lane.blocks.front();
					const auto key = timeline_track_key(state.snapshot, lane);
					const auto preference = std::ranges::find(state.track_preferences, key,
															  &TimelineTrackPreference::key);
					const bool hidden =
						preference != state.track_preferences.end() && preference->hidden;
					const bool pinned =
						preference != state.track_preferences.end() && preference->pinned;
					timeline_ui::text(
						context,
						(lane.window ? "Window " + std::to_string(lane.window) + " / " : "") +
							std::string(lane.domain == TimingSampleDomain::Cpu ? "CPU "
																			   : "GPU queue ") +
							std::to_string(lane.track));
					const auto buttons = timeline_ui::row(24);
					CLAY(Clay_GetElementIdWithIndex(CLAY_STRING("performance.track-controls"),
													uint32_t(root_index)),
						 buttons) {
						control(root_index * 4, hidden ? "Show" : "Hide", TimelineAction::TrackHide,
								{root_index});
						control(root_index * 4 + 1, pinned ? "Unpin" : "Pin",
								TimelineAction::TrackPin, {root_index});
						if (row_index)
							control(root_index * 4 + 2, "Up", TimelineAction::TrackMove,
									{root_index, lanes[row_indices[row_index - 1]].blocks.front()});
						if (row_index + 1 < row_indices.size())
							control(root_index * 4 + 3, "Down", TimelineAction::TrackMove,
									{root_index, lanes[row_indices[row_index + 1]].blocks.front()});
					}
				}
			}
		}
		Clay_ElementDeclaration canvas{};
		canvas.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		canvas.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		const auto canvas_bounds = Clay_GetElementData(context.clayID("canvas"));
		const float available_height = canvas_bounds.found ? canvas_bounds.boundingBox.height : 600;
		state.major_height =
			std::clamp(state.major_height, 80.0f, std::max(80.0f, available_height - 84));

		CLAY(context.clayID("canvas"), canvas) {
			auto major = canvas;
			major.layout.sizing.height = CLAY_SIZING_FIXED(state.major_height);

			CLAY(context.clayID("major-pane"), major) {

				context.uiManager
					.createElement(kDevContiguousTimelineStrip, LocalElementName{"macro"})
					.setParameters(DevTimelineParameters{&state, &selection, 0,
														 context.clayID("major-pane"),
														 context.params.column_clip})
					.setDevInternalCapture(true)
					.draw();
			}
			context.uiManager
				.createElement(FSEL::kSplitterHandle, LocalElementName{"timeline-divider"})
				.setParameters(FSEL::SplitterHandleParameters{
					.axis = FSEL::SplitterAxis::Vertical,
					.targetExtent = &state.major_height,
					.minExtent = 80,
					.maxExtent = std::max(80.0f, available_height - 84),
					.backgroundColor = interface_theme::kTextMuted,
					.hoverColor = interface_theme::kAccentSeaGlass})
				.setDevInternalCapture(true)
				.draw();
			auto minor = canvas;

			CLAY(context.clayID("minor-pane"), minor) {
				context.uiManager
					.createElement(kDevDrillDownTimelineCard, LocalElementName{"minor"})
					.setParameters(DevTimelineParameters{
						&state, &selection, state.cards.empty() ? 0 : state.cards.size() - 1,
						context.clayID("minor-pane"), context.params.column_clip})
					.setDevInternalCapture(true)
					.draw();
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
