#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>

namespace FlowUi::devSystems::interface_elements {
uint64_t timeline_end(uint64_t start_ns, uint64_t duration_ns) noexcept {
	return start_ns + std::min(duration_ns, UINT64_MAX - start_ns);
}
TimelineSnapshot extract_timeline(std::span<const TimingAppTickReport> reports,
								  std::span<const TimingZoneDescriptor> descriptors,
								  const DevPerformanceSelection& selection) {
	TimelineSnapshot result;
	result.window_milestones = selection.selected_scope.kind == DevPerformanceScopeKind::Window &&
							   selection.selected_scope.id != 0;
	size_t record_count = 0;
	for (const auto& report : reports) {
		record_count += report.applicationCpuZones.size();
		for (const auto& window : report.windows)
			for (const auto& frame : window.frames)
				record_count += frame.cpuZones.size() + frame.gpuZones.size();
	}
	result.blocks.reserve(record_count);
	result.frames.reserve(reports.size());
	std::unordered_map<uint64_t, const TimingZoneDescriptor*> descriptor_map;
	descriptor_map.reserve(descriptors.size());
	for (const auto& descriptor : descriptors)
		descriptor_map.emplace(descriptor.typeId, &descriptor);
	std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> parent_keys;
	parent_keys.reserve(record_count);
	std::map<std::tuple<uint64_t, uint64_t, uint64_t>, size_t> identities;
	const auto label_block = [&](TimelineBlockSlice& block) {
		if (const auto found = descriptor_map.find(block.type_id); found != descriptor_map.end()) {
			block.label = performance_zone_label(found->second->name);
			block.category = found->second->category;
		} else
			block.label = "Zone " + std::to_string(block.type_id);
		block.selected = selection.selector_mode == 1 && selection.selected_zone != 0 &&
						 selection.selected_zone == block.type_id;
	};
	const auto add_cpu = [&](const CpuTimingRecord& record) {
		if (selection.selected_scope.id &&
			(selection.selected_scope.kind == DevPerformanceScopeKind::Window
				 ? record.frame.window != selection.selected_scope.id
				 : record.track != selection.selected_scope.id))
			return;
		TimelineBlockSlice block;
		block.start_ns = record.startNs;
		block.duration_ns = record.durationNs;
		block.exclusive_ns = record.exclusiveNs();
		block.invocation_id = record.invocationId;
		block.type_id = record.typeId;
		block.app_tick = record.appTick;
		block.frame = record.frame;
		block.track = record.track;
		label_block(block);
		identities.emplace(std::tuple{record.appTick, uint64_t(record.track), record.invocationId},
						   result.blocks.size());
		parent_keys.emplace_back(record.appTick, record.track, record.parentInvocationId);
		result.blocks.emplace_back(std::move(block));
	};
	for (const auto& report : reports) {
		uint64_t tick_start = UINT64_MAX, tick_end = 0;
		const auto note_time = [&](const CpuTimingRecord& record) {
			tick_start = std::min(tick_start, record.startNs);
			tick_end = std::max(tick_end, timeline_end(record.startNs, record.durationNs));
		};
		for (const auto& record : report.applicationCpuZones) {
			note_time(record);
			add_cpu(record);
		}
		for (const auto& window : report.windows) {
			for (const auto& frame : window.frames) {
				for (const auto& record : frame.cpuZones) {
					note_time(record);
					add_cpu(record);
					if (record.typeId == timing_zones::kWindowFrameTotal.typeId &&
						(selection.selected_scope.kind == DevPerformanceScopeKind::Thread ||
						 selection.selected_scope.id == record.frame.window)) {
						TimelineBlockSlice block;
						block.start_ns = record.startNs;
						block.duration_ns = record.durationNs;
						block.frame = record.frame;
						block.app_tick = report.appTick;
						block.label = "Window " + std::to_string(record.frame.window) +
									  " · Frame #" + std::to_string(record.frame.frameNumber);
						result.frames.emplace_back(std::move(block));
					}
				}
				if (selection.selected_scope.kind == DevPerformanceScopeKind::Window &&
					selection.selected_scope.id && selection.selected_scope.id != window.window)
					continue;
				std::map<std::pair<uint64_t, size_t>, size_t> gpu_indices;
				std::map<uint64_t, size_t> submission_indices;
				std::vector<std::pair<size_t, size_t>> gpu_parents;
				gpu_parents.reserve(frame.gpuZones.size());
				const size_t gpu_begin = result.blocks.size();
				for (size_t record_index = 0; record_index < frame.gpuZones.size();
					 ++record_index) {
					const auto& record = frame.gpuZones[record_index];
					const auto submission_index = submission_indices[record.submissionSerial]++;
					if ((record.flags & gpuTimingRecordFlags(GpuTimingRecordFlag::Uncalibrated)) !=
							0 ||
						record.cpuAlignedStartNs == 0) {
						++result.uncalibrated_gpu_count;
						continue;
					}
					TimelineBlockSlice block;
					block.start_ns = record.cpuAlignedStartNs;
					block.duration_ns = record.durationNs;
					block.exclusive_ns = record.durationNs;
					block.type_id = record.typeId;
					block.frame = record.frame;
					block.app_tick = record.appTick;
					block.track = record.queueFamilyIndex;
					block.domain = TimingSampleDomain::Gpu;
					label_block(block);
					gpu_indices.emplace(std::pair{record.submissionSerial, submission_index},
										result.blocks.size());
					gpu_parents.emplace_back(record_index, result.blocks.size());
					parent_keys.emplace_back(0, 0, 0);
					result.blocks.emplace_back(std::move(block));
				}
				for (const auto& [record_index, block_index] : gpu_parents) {
					const auto& record = frame.gpuZones[record_index];
					const auto parent =
						gpu_indices.find({record.submissionSerial, record.parentZoneIndex});
					if (parent != gpu_indices.end() && parent->second != block_index)
						result.blocks[block_index].parent = parent->second;
				}
				for (size_t block_index = gpu_begin; block_index < result.blocks.size();
					 ++block_index) {
					const auto& child = result.blocks[block_index];
					if (child.parent != timeline_no_parent) {
						auto& parent = result.blocks[child.parent];
						const auto overlap_start = std::max(parent.start_ns, child.start_ns);
						const auto overlap_end =
							std::min(timeline_end(parent.start_ns, parent.duration_ns),
									 timeline_end(child.start_ns, child.duration_ns));
						const auto overlap =
							overlap_end > overlap_start ? overlap_end - overlap_start : 0;
						parent.exclusive_ns -= std::min(parent.exclusive_ns, overlap);
					}
				}
			}
		}
		if (selection.selected_scope.kind == DevPerformanceScopeKind::Window &&
			selection.selected_scope.id == 0 && tick_start != UINT64_MAX) {
			TimelineBlockSlice block;
			block.start_ns = tick_start;
			block.duration_ns = tick_end - tick_start;
			block.app_tick = report.appTick;
			block.label = "AppTick #" + std::to_string(report.appTick);
			result.frames.emplace_back(std::move(block));
		}
	}
	for (size_t block_index = 0; block_index < result.blocks.size(); ++block_index) {
		auto& block = result.blocks[block_index];
		if (block.domain != TimingSampleDomain::Cpu)
			continue;
		const auto parent = identities.find(parent_keys[block_index]);
		if (std::get<2>(parent_keys[block_index]) && parent != identities.end() &&
			parent->second != block_index)
			block.parent = parent->second;
	}
	std::ranges::sort(result.frames, {}, &TimelineBlockSlice::start_ns);
	result.start_ns = UINT64_MAX;
	for (const auto& block : result.blocks) {
		result.start_ns = std::min(result.start_ns, block.start_ns);
		result.end_ns = std::max(result.end_ns, timeline_end(block.start_ns, block.duration_ns));
	}
	for (const auto& frame : result.frames) {
		result.start_ns = std::min(result.start_ns, frame.start_ns);
		result.end_ns = std::max(result.end_ns, timeline_end(frame.start_ns, frame.duration_ns));
	}
	if (result.start_ns == UINT64_MAX)
		result.start_ns = 0;
	result.frame_metrics.reserve(result.frames.size());
	for (const auto& frame : result.frames) {
		uint64_t metric = frame.duration_ns;
		if (selection.selector_mode == 1 && selection.selected_zone) {
			metric = 0;
			for (const auto& block : result.blocks)
				if (block.selected && block.app_tick == frame.app_tick &&
					(!frame.frame.window || block.frame == frame.frame) &&
					(selection.hardware_domain == 0 ||
					 (selection.hardware_domain == 1) == (block.domain == TimingSampleDomain::Cpu)))
					metric = timeline_end(metric, block.duration_ns);
		}
		result.frame_metrics.emplace_back(metric);
	}
	auto sorted_metrics = result.frame_metrics;
	std::ranges::sort(sorted_metrics);
	if (!sorted_metrics.empty())
		result.percentile_ns = sorted_metrics[(sorted_metrics.size() - 1) * 95 / 100];
	return result;
}
std::vector<TimelineTrackLane> timeline_lanes(const TimelineSnapshot& snapshot, uint32_t depth,
											  std::span<const size_t> roots) {
	std::map<std::tuple<TimingSampleDomain, uint64_t, uint32_t>, std::vector<size_t>> groups;
	for (size_t block_index = 0; block_index < snapshot.blocks.size(); ++block_index) {
		const auto& block = snapshot.blocks[block_index];
		if (roots.empty() && snapshot.window_milestones &&
			block.type_id == timing_zones::kWindowFrameTotal.typeId)
			continue;
		size_t parent = block.parent;
		uint32_t level = 0;
		bool included = roots.empty();
		if (!roots.empty() && std::ranges::find(roots, block_index) != roots.end())
			continue;
		size_t guard = 0;
		while (parent != timeline_no_parent && parent < snapshot.blocks.size() &&
			   guard++ < snapshot.blocks.size()) {
			if (!roots.empty() && std::ranges::find(roots, parent) != roots.end()) {
				included = true;
				break;
			}
			if (!(roots.empty() && snapshot.window_milestones &&
				  snapshot.blocks[parent].type_id == timing_zones::kWindowFrameTotal.typeId))
				++level;
			parent = snapshot.blocks[parent].parent;
		}
		if (guard >= snapshot.blocks.size() || !included || (level >= depth && !block.selected))
			continue;
		groups[{block.domain, block.track, level}].emplace_back(block_index);
	}
	std::vector<TimelineTrackLane> lanes;
	lanes.reserve(groups.size());
	for (auto& [key, blocks] : groups) {
		std::ranges::sort(blocks, [&](size_t left, size_t right) {
			return snapshot.blocks[left].start_ns < snapshot.blocks[right].start_ns;
		});
		lanes.emplace_back(TimelineTrackLane{std::move(blocks), std::get<1>(key), std::get<2>(key),
											 std::get<0>(key)});
	}
	return lanes;
}
std::vector<TimelineCluster> cluster_timeline(const TimelineSnapshot& snapshot,
											  std::span<const size_t> blocks, uint64_t start_ns,
											  uint64_t duration_ns, float width) {
	std::vector<TimelineCluster> clusters;
	clusters.reserve(blocks.size());
	const double scale = std::max(0.0f, width) / double(std::max(uint64_t{1}, duration_ns));
	bool previous_small = false;
	for (const size_t block_index : blocks) {
		if (block_index >= snapshot.blocks.size())
			continue;
		const auto& block = snapshot.blocks[block_index];
		const auto end_ns = timeline_end(block.start_ns, block.duration_ns);
		if (end_ns <= start_ns || block.start_ns >= timeline_end(start_ns, duration_ns))
			continue;
		const bool small = block.duration_ns < 50'000 || block.duration_ns * scale < 6;
		bool merge = false;
		if (!clusters.empty() && small && previous_small && !block.selected) {
			auto& previous = clusters.back();
			const auto& previous_block = snapshot.blocks[previous.members.back()];
			const auto previous_end = timeline_end(previous.start_ns, previous.duration_ns);
			merge = !previous_block.selected && previous_block.parent == block.parent &&
					previous_block.category == block.category && block.start_ns >= previous_end &&
					(block.start_ns - previous_end) * scale < 1;
			if (merge) {
				previous.members.emplace_back(block_index);
				previous.duration_ns = end_ns - previous.start_ns;
			}
		}
		if (!merge)
			clusters.emplace_back(
				TimelineCluster{{block_index}, block.start_ns, block.duration_ns});
		previous_small = small;
	}
	return clusters;
}
void clamp_timeline_view(DevTimelineState& state) noexcept {
	const uint64_t range = std::max(uint64_t{1}, state.snapshot.end_ns - state.snapshot.start_ns);
	state.zoom = std::clamp(std::isfinite(state.zoom) ? state.zoom : 1.0, 1.0, 1000.0);
	state.visible_duration_ns = std::max(uint64_t{1}, static_cast<uint64_t>(range / state.zoom));
	const uint64_t latest = state.snapshot.end_ns >= state.visible_duration_ns
								? state.snapshot.end_ns - state.visible_duration_ns
								: state.snapshot.start_ns;
	state.visible_start_ns = std::clamp(state.visible_start_ns, state.snapshot.start_ns,
										std::max(state.snapshot.start_ns, latest));
}
void center_timeline(DevTimelineState& state, uint64_t timestamp) noexcept {
	state.visible_start_ns =
		timestamp > state.visible_duration_ns / 2 ? timestamp - state.visible_duration_ns / 2 : 0;
	clamp_timeline_view(state);
}
void apply_timeline_command(DevTimelineState& state, DevPerformanceSelection& selection) {
	auto command = std::move(state.pending);
	state.pending = {};
	const auto frame_count = state.snapshot.frames.size();
	if (frame_count)
		state.selected_frame = std::min(state.selected_frame, frame_count - 1);
	switch (command.action) {
	case TimelineAction::Pause:
		state.paused = !state.paused;
		if (!state.paused)
			state.cards.clear();
		break;
	case TimelineAction::Freeze:
		state.auto_freeze = !state.auto_freeze;
		break;
	case TimelineAction::Zoom:
		state.zoom = command.value;
		break;
	case TimelineAction::Domain:
		selection.hardware_domain = command.index;
		break;
	case TimelineAction::Previous:
	case TimelineAction::Next:
	case TimelineAction::Spike:
	case TimelineAction::Center:
		if (!frame_count)
			break;
		state.paused = true;
		if (command.action == TimelineAction::Previous && state.selected_frame)
			--state.selected_frame;
		if (command.action == TimelineAction::Next)
			state.selected_frame = std::min(state.selected_frame + 1, frame_count - 1);
		if (command.action == TimelineAction::Center)
			state.selected_frame = std::min(command.index, frame_count - 1);
		if (command.action == TimelineAction::Spike) {
			for (size_t offset = 1; offset <= frame_count; ++offset) {
				const auto candidate = (state.selected_frame + offset) % frame_count;
				const auto metric = state.snapshot.frame_metrics[candidate];
				if (metric > 16'666'667 || metric > state.snapshot.percentile_ns) {
					state.selected_frame = candidate;
					break;
				}
			}
		}
		center_timeline(state, state.snapshot.frames[state.selected_frame].start_ns);
		state.cards.clear();
		break;
	case TimelineAction::Open:
		if (command.members.empty() || std::ranges::any_of(command.members, [&](size_t index) {
				return index >= state.snapshot.blocks.size();
			}))
			break;
		state.paused = true;
		state.cards.resize(std::min(command.index, state.cards.size()));
		state.cards.emplace_back(TimelineCard{std::move(command.members), 1});
		state.reveal_frames = 12;
		break;
	case TimelineAction::Close:
	case TimelineAction::Breadcrumb:
		state.cards.resize(std::min(command.index, state.cards.size()));
		break;
	case TimelineAction::Depth: {
		auto* depth = command.index == timeline_no_parent ? &state.active_depth
					  : command.index < state.cards.size()
						  ? &state.cards[command.index].active_depth
						  : nullptr;
		if (depth)
			*depth = static_cast<uint32_t>(std::clamp(int(*depth) + int(command.value), 1, 64));
		break;
	}
	case TimelineAction::None:
		break;
	}
	clamp_timeline_view(state);
}
} // namespace FlowUi::devSystems::interface_elements
#endif
