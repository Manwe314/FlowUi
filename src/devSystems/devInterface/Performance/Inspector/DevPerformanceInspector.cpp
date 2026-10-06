#include "devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Performance/Workbench/DevTimelineControls.hpp"
namespace FlowUi::devSystems::interface_elements {
void DevPerformanceInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->performance_timeline;
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.padding = {14, 14, 14, 14};
	root.layout.childGap = 8;
	root.clip.vertical = true;
	const auto scroll = Clay_GetScrollContainerData(context.clayID());
	if (scroll.found && scroll.scrollPosition)
		root.clip.childOffset = *scroll.scrollPosition;
	CLAY(context.clayID(), root) {
		Clay_TextElementConfig text{};
		text.fontSize = 12;
		text.textColor = interface_theme::kTextCanvas;
		const auto line = [&](const std::string& value) {
			CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(text));
		};
		if (state.inspected_sample >= state.snapshot.blocks.size()) {
			line("Select a timing sample to inspect.");
		} else {
			const auto& block = state.snapshot.blocks[state.inspected_sample];
			line(block.label);
			if (block.synthetic_tick)
				line("Synthetic AppTick grouping");
			else
				line(std::string(block.domain == TimingSampleDomain::Cpu ? "CPU thread "
																		 : "GPU queue family ") +
					 std::to_string(block.track));
			line("Category: " + std::string(performance_category_names[size_t(block.category)]) +
				 " / role " + std::to_string(uint32_t(block.role)));
			if (block.cpu_clock_aligned)
				line("Start: " + std::to_string(block.start_ns) + " ns\nEnd: " +
					 std::to_string(timeline_end(block.start_ns, block.duration_ns)) + " ns");
			if (!block.cpu_clock_aligned)
				line("CPU absolute time unavailable; GPU local time.\nDevice start: " +
					 std::to_string(block.device_start_ticks) + " ticks");
			line("Duration: " + timeline_ui::milliseconds(block.duration_ns));
			if (block.domain == TimingSampleDomain::Cpu && !block.synthetic_tick) {
				line("Recorded exclusive: " + timeline_ui::milliseconds(block.exclusive_ns));
				line("Elapsed time minus recorded synchronous direct-child time; includes waits "
					 "and uninstrumented work.");
			}
			if (!block.hierarchy_note.empty())
				line(block.hierarchy_note + " / recorded parent " +
					 (block.domain == TimingSampleDomain::Cpu
						  ? std::to_string(block.parent_invocation_id)
						  : std::to_string(block.parent_zone_index)));
			std::string quality = block.synthetic_tick		  ? "Derived grouping"
								  : (block.quality_flags & 1) ? "Completed"
															  : "Completion unknown";
			if (block.domain == TimingSampleDomain::Cpu && !block.synthetic_tick) {
				if (block.quality_flags & timingRecordFlags(TimingRecordFlag::Canceled))
					quality += " / canceled";
				if (block.quality_flags & timingRecordFlags(TimingRecordFlag::Incomplete))
					quality += " / incomplete";
				if (block.quality_flags & timingRecordFlags(TimingRecordFlag::ClockAnomaly))
					quality += " / clock anomaly";
				if (block.quality_flags & timingRecordFlags(TimingRecordFlag::DetailTruncated))
					quality += " / detail truncated";
				if (block.quality_flags & timingRecordFlags(TimingRecordFlag::OutOfDate))
					quality += " / out of date";
			} else if (block.domain == TimingSampleDomain::Gpu) {
				if (!block.cpu_clock_aligned)
					quality += " / GPU local clock";
				if (block.quality_flags &
					gpuTimingRecordFlags(GpuTimingRecordFlag::DetailTruncated))
					quality += " / detail truncated";
			}
			line("Quality: " + quality + " (flags " + std::to_string(block.quality_flags) + ")");
			line("Entity: " + std::to_string(block.entity.primaryId) + " / " +
				 std::to_string(block.entity.secondaryId));
			if (!block.source_file.empty())
				line(block.source_file + ":" + std::to_string(block.source_line) + "\n" +
					 block.source_function);
			if (block.domain == TimingSampleDomain::Gpu) {
				line("Submission: " + std::to_string(block.submission_serial) + " / zone " +
					 std::to_string(block.zone_index));
				line("Device tick period: " + std::to_string(block.timestamp_period_ns) + " ns / " +
					 std::to_string(block.timestamp_valid_bits) + " valid bits");
				line("Device: " + std::to_string(block.device_identity) + " / queue instance " +
					 std::to_string(block.queue_identity));
				line("Calibration: " + std::to_string(block.calibration_id) + " / deviation " +
					 std::to_string(block.calibration_deviation_ns) + " ns");
				line("Timestamp stages: " + std::to_string(block.begin_stage) + " -> " +
					 std::to_string(block.end_stage));
			}
			line("Descriptor: " + std::to_string(block.type_id) +
				 "\nInvocation: " + std::to_string(block.invocation_id));
			line("Window: " + std::to_string(block.frame.window) + " / frame " +
				 std::to_string(block.frame.frameNumber) +
				 "\nAppTick: " + std::to_string(block.app_tick));
			if (!state.cards.empty() && state.cards.back().roots.size() == 1) {
				const auto& focus = state.snapshot.blocks[state.cards.back().roots.front()];
				const bool comparable = (block.cpu_clock_aligned && focus.cpu_clock_aligned) ||
										(!block.cpu_clock_aligned && !focus.cpu_clock_aligned &&
										 block.device_identity == focus.device_identity &&
										 block.queue_identity == focus.queue_identity &&
										 block.submission_serial == focus.submission_serial &&
										 block.frame == focus.frame);
				if (comparable) {
					const auto relative = [&](uint64_t timestamp) {
						return std::string(timestamp < focus.start_ns ? "-" : "+") +
							   timeline_ui::milliseconds(timestamp < focus.start_ns
															 ? focus.start_ns - timestamp
															 : timestamp - focus.start_ns);
					};
					const auto end = timeline_end(block.start_ns, block.duration_ns);
					const auto visible_start = std::max(block.start_ns, focus.start_ns);
					const auto visible_end =
						std::min(end, timeline_end(focus.start_ns, focus.duration_ns));
					line("Root-relative: " + relative(block.start_ns) + " to " + relative(end));
					line("Within root: " +
						 timeline_ui::milliseconds(
							 visible_end > visible_start ? visible_end - visible_start : 0));
				}
			}
			context.uiManager.createElement(kDevTimelineButton, LocalElementName{"focus"})
				.setParameters(DevTimelineButtonParameters{{{state.inspected_sample},
															TimelineAction::Open,
															state.cards.size(),
															0,
															state.snapshot_revision},
														   "Focus in minor",
														   &state})
				.setDevInternalCapture(true)
				.draw();
			if (block.cpu_clock_aligned)
				context.uiManager.createElement(kDevTimelineButton, LocalElementName{"reveal"})
					.setParameters(DevTimelineButtonParameters{
						{{}, TimelineAction::FitSelected, 0, 0, state.snapshot_revision},
						"Reveal in major",
						&state})
					.setDevInternalCapture(true)
					.draw();
			if (block.parent < state.snapshot.blocks.size()) {
				line(std::string(block.domain == TimingSampleDomain::Cpu
									 ? "Synchronous parent: "
									 : "Instrumented GPU parent: ") +
					 state.snapshot.blocks[block.parent].label);
				context.uiManager.createElement(kDevTimelineButton, LocalElementName{"parent"})
					.setParameters(DevTimelineButtonParameters{
						{{block.parent}, TimelineAction::Inspect, 0, 0, state.snapshot_revision},
						"Inspect parent",
						&state})
					.setDevInternalCapture(true)
					.draw();
			}
			if (state.inspected_sample + 1 < state.snapshot.child_offsets.size()) {
				const auto first_child = state.snapshot.child_offsets[state.inspected_sample];
				const auto child_end = state.snapshot.child_offsets[state.inspected_sample + 1];
				if (child_end > first_child)
					line("Recorded children: " + std::to_string(child_end - first_child));
				for (size_t child_position = first_child;
					 child_position < std::min(child_end, first_child + 16); ++child_position) {
					const auto child_index = state.snapshot.children[child_position];
					context.uiManager.createElement(kDevTimelineButton, Keyed("child", child_index))
						.setParameters(DevTimelineButtonParameters{
							{{child_index}, TimelineAction::Inspect, 0, 0, state.snapshot_revision},
							state.snapshot.blocks[child_index].label,
							&state})
						.setDevInternalCapture(true)
						.draw();
				}
			}
		}
		context.uiManager.createElement(kDevTimelineButton, LocalElementName{"tracks"})
			.setParameters(DevTimelineButtonParameters{
				{{}, TimelineAction::TrackControls},
				state.track_controls_open ? "Close major tracks" : "Major tracks",
				&state})
			.setDevInternalCapture(true)
			.draw();
		if (state.track_controls_open) {
			auto preferences = state.track_preferences;
			for (auto& preference : preferences)
				preference.hidden = false;
			const auto lanes = timeline_major_lanes(state.snapshot, state.selection, preferences);
			std::vector<size_t> row_indices;
			row_indices.reserve(lanes.size());
			for (size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
				const auto key = timeline_track_key(state.snapshot, lanes[lane_index]);
				if (std::ranges::none_of(row_indices, [&](size_t existing) {
						return timeline_track_key(state.snapshot, lanes[existing]) == key;
					}))
					row_indices.emplace_back(lane_index);
			}
			const auto control = [&](uint64_t control_key, std::string label, TimelineAction action,
									 std::vector<size_t> members) {
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
				const auto preference =
					std::ranges::find(state.track_preferences, key, &TimelineTrackPreference::key);
				const bool hidden =
					preference != state.track_preferences.end() && preference->hidden;
				const bool pinned =
					preference != state.track_preferences.end() && preference->pinned;
				line((lane.window ? "Window " + std::to_string(lane.window) + " / " : "") +
					 std::string(lane.domain == TimingSampleDomain::Cpu ? "CPU " : "GPU queue ") +
					 std::to_string(lane.track));
				const auto buttons = timeline_ui::row(24);
				CLAY(Clay_GetElementIdWithIndex(CLAY_STRING("performance.track-controls"),
												uint32_t(root_index)),
					 buttons) {
					control(root_index * 4, hidden ? "Show" : "Hide", TimelineAction::TrackHide,
							{root_index});
					control(root_index * 4 + 1, pinned ? "Unpin" : "Pin", TimelineAction::TrackPin,
							{root_index});
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
}
} // namespace FlowUi::devSystems::interface_elements
#endif
