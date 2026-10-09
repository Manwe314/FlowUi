#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>

namespace FlowUi::devSystems::interface_elements {
TimelineSampleDetails TimelineBlockSlice::details() const noexcept {
	TimelineSampleDetails result;
	if (!descriptor.empty()) {
		result.source_file = descriptor.front().source.file;
		result.source_function = descriptor.front().source.function;
		result.source_line = descriptor.front().source.line;
	}
	if (!cpu_sample.empty()) {
		const auto& sample = cpu_sample.front();
		result.exclusive_ns = sample.exclusiveNs();
		result.parent_invocation_id = sample.parentInvocationId;
		result.entity = {sample.entityKind, sample.primaryEntityId, sample.secondaryEntityId};
		result.quality_flags = sample.flags;
	} else if (!gpu_sample.empty()) {
		const auto& sample = gpu_sample.front();
		result.entity = {sample.entityKind, sample.primaryEntityId, sample.secondaryEntityId};
		result.quality_flags = sample.flags;
		result.device_start_ticks = sample.startTick;
		result.device_duration_ticks = sample.durationTicks;
		result.timestamp_period_ns = sample.timestamp_period_ns;
		result.timestamp_valid_bits = sample.timestamp_valid_bits;
		result.calibration_id = sample.calibrationId;
		result.calibration_deviation_ns = sample.calibrationMaximumDeviationNs;
		result.begin_stage = sample.beginStage;
		result.end_stage = sample.endStage;
	}
	return result;
}
TimelineSnapshot::TimelineSnapshot(const TimelineSnapshot& other) {
	*this = other;
}
TimelineSnapshot& TimelineSnapshot::operator=(const TimelineSnapshot& other) {
	if (this == &other)
		return *this;
	blocks = other.blocks;
	frames = other.frames;
	ticks = other.ticks;
	labels.reserve(other.labels.capacity());
	labels = other.labels;
	frame_metrics = other.frame_metrics;
	tick_samples = other.tick_samples;
	child_offsets = other.child_offsets;
	children = other.children;
	start_ns = other.start_ns;
	end_ns = other.end_ns;
	percentile_ns = other.percentile_ns;
	uncalibrated_gpu_count = other.uncalibrated_gpu_count;
	window_milestones = other.window_milestones;
	tick_cadence = other.tick_cadence;
	for (auto* sequence : {&blocks, &frames, &ticks})
		for (auto& block : *sequence)
			for (size_t label_index = 0; label_index < other.labels.size(); ++label_index)
				if (block.label.data() == other.labels[label_index].data()) {
					block.label = labels[label_index];
					break;
				}
	return *this;
}
namespace {
struct ReportSegments {
	std::span<const TimingAppTickReport> first{}, second{};
	struct Iterator {
		const ReportSegments* owner;
		size_t offset;
		[[nodiscard]] const TimingAppTickReport& operator*() const noexcept {
			return offset < owner->first.size() ? owner->first[offset]
												: owner->second[offset - owner->first.size()];
		}
		Iterator& operator++() noexcept {
			++offset;
			return *this;
		}
		[[nodiscard]] bool operator!=(const Iterator& other) const noexcept {
			return offset != other.offset;
		}
	};
	[[nodiscard]] size_t size() const noexcept { return first.size() + second.size(); }
	[[nodiscard]] Iterator begin() const noexcept { return {this, 0}; }
	[[nodiscard]] Iterator end() const noexcept { return {this, size()}; }
};
} // namespace
uint64_t timeline_end(uint64_t start_ns, uint64_t duration_ns) noexcept {
	return start_ns + std::min(duration_ns, UINT64_MAX - start_ns);
}
static TimelineSnapshot extract_timeline_segments(ReportSegments reports,
												  std::span<const TimingZoneDescriptor> descriptors,
												  const DevPerformanceSelection& selection) {
	TimelineSnapshot result;
	std::vector<const GpuTimingRecord*> unaligned_records;
	result.window_milestones = true;
	size_t record_count = 0;
	for (const auto& report : reports) {
		record_count += report.applicationCpuZones.size();
		for (const auto& window : report.windows)
			if (window.occupied)
				for (const auto& frame : window.frames)
					if (frame.occupied)
						record_count += frame.cpuZones.size() + frame.gpuZones.size();
	}
	result.blocks.reserve(record_count + reports.size());
	result.labels.reserve(record_count * 2 + reports.size() * 3 + descriptors.size());
	const auto own_label = [&](std::string value) -> std::string_view {
		result.labels.emplace_back(std::move(value));
		return result.labels.back();
	};
	std::unordered_map<uint64_t, std::string_view> labels;
	labels.reserve(descriptors.size());
	for (const auto& descriptor : descriptors)
		labels.emplace(descriptor.typeId, own_label(performance_zone_label(descriptor.name)));
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
			block.label = labels.at(block.type_id);
			block.descriptor = {found->second, 1};
			block.category = found->second->category;
			block.role = found->second->role;
		} else {
			auto label_iterator = labels.find(block.type_id);
			if (label_iterator == labels.end())
				label_iterator =
					labels
						.emplace(block.type_id, own_label("Zone " + std::to_string(block.type_id)))
						.first;
			block.label = label_iterator->second;
		}
		block.selected = selection.selector_mode == 1 && selection.selected_zone != 0 &&
						 selection.selected_zone == block.type_id;
	};
	const auto add_cpu = [&](const CpuTimingRecord& record) {
		TimelineBlockSlice block;
		block.cpu_sample = {&record, 1};
		block.scope_visible =
			!selection.selected_scope.id ||
			(selection.selected_scope.kind == DevPerformanceScopeKind::Window
				 ? !record.frame.window || record.frame.window == selection.selected_scope.id
				 : record.track == selection.selected_scope.id);
		block.start_ns = record.startNs;
		block.duration_ns = record.durationNs;
		block.invocation_id = record.invocationId;
		block.type_id = record.typeId;
		block.app_tick = record.appTick;
		block.frame = record.frame;
		block.track = record.track;
		label_block(block);
		identities.emplace(std::tuple{uint64_t{0}, uint64_t(record.track), record.invocationId},
						   result.blocks.size());
		parent_keys.emplace_back(0, record.track, record.parentInvocationId);
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
			if (!window.occupied)
				continue;
			for (const auto& frame : window.frames) {
				if (!frame.occupied)
					continue;
				for (const auto& record : frame.cpuZones) {
					note_time(record);
					add_cpu(record);
					if (record.typeId == timing_zones::kWindowFrameTotal.typeId &&
						(!selection.selected_scope.id ||
						 (selection.selected_scope.kind == DevPerformanceScopeKind::Window
							  ? selection.selected_scope.id == record.frame.window
							  : selection.selected_scope.id == record.track))) {
						TimelineBlockSlice block;
						block.start_ns = record.startNs;
						block.duration_ns = record.durationNs;
						block.frame = record.frame;
						block.app_tick = report.appTick;
						block.label =
							own_label("Window " + std::to_string(record.frame.window) +
									  " · Frame #" + std::to_string(record.frame.frameNumber));
						result.frames.emplace_back(std::move(block));
					}
				}
				std::map<std::tuple<uint64_t, uint64_t, uint64_t, size_t>, size_t> gpu_indices;
				std::map<uint64_t, size_t> submission_indices;
				std::vector<std::pair<size_t, size_t>> gpu_parents;
				gpu_parents.reserve(frame.gpuZones.size());
				for (size_t record_index = 0; record_index < frame.gpuZones.size();
					 ++record_index) {
					const auto& record = frame.gpuZones[record_index];
					const auto submission_index = submission_indices[record.submissionSerial]++;
					if ((record.flags & gpuTimingRecordFlags(GpuTimingRecordFlag::Uncalibrated)) !=
							0 ||
						record.cpuAlignedStartNs == 0) {
						++result.uncalibrated_gpu_count;
						unaligned_records.emplace_back(&record);
						continue;
					}
					TimelineBlockSlice block;
					block.gpu_sample = {&record, 1};
					block.start_ns = record.cpuAlignedStartNs;
					block.duration_ns = record.durationNs;
					block.type_id = record.typeId;
					block.frame = record.frame;
					block.app_tick = record.appTick;
					block.track = record.queueFamilyIndex;
					block.domain = TimingSampleDomain::Gpu;
					block.submission_serial = record.submissionSerial;
					block.device_identity = record.device_identity;
					block.queue_identity = record.queue_identity;
					block.zone_index = record.zone_index;
					block.parent_zone_index = record.parentZoneIndex;
					block.scope_visible =
						selection.selected_scope.kind != DevPerformanceScopeKind::Thread &&
						(!selection.selected_scope.id ||
						 selection.selected_scope.id == record.frame.window);
					label_block(block);
					gpu_indices.emplace(std::tuple{record.device_identity, record.queue_identity,
												   record.submissionSerial,
												   record.zone_index == UINT32_MAX
													   ? submission_index
													   : size_t(record.zone_index)},
										result.blocks.size());
					gpu_parents.emplace_back(record_index, result.blocks.size());
					parent_keys.emplace_back(0, 0, 0);
					result.blocks.emplace_back(std::move(block));
				}
				for (const auto& [record_index, block_index] : gpu_parents) {
					const auto& record = frame.gpuZones[record_index];
					const auto parent =
						gpu_indices.find({record.device_identity, record.queue_identity,
										  record.submissionSerial, record.parentZoneIndex});
					if (parent != gpu_indices.end() && parent->second != block_index)
						result.blocks[block_index].parent = parent->second;
					else if (record.parentZoneIndex != UINT32_MAX)
						result.blocks[block_index].hierarchy_note =
							"Instrumented GPU parent unavailable";
				}
			}
		}
		if (report.boundary_start_ns && report.boundary_end_ns >= report.boundary_start_ns) {
			tick_start = report.boundary_start_ns;
			tick_end = report.boundary_end_ns;
			result.tick_cadence = true;
		}
		if (tick_start != UINT64_MAX) {
			TimelineBlockSlice tick;
			tick.start_ns = tick_start;
			tick.duration_ns = tick_end >= tick_start ? tick_end - tick_start : 0;
			tick.app_tick = report.appTick;
			tick.measured_cadence =
				report.boundary_start_ns && report.boundary_end_ns >= report.boundary_start_ns;
			tick.label = own_label("AppTick #" + std::to_string(report.appTick) +
								   (report.boundary_open ? " (open)" : ""));
			result.ticks.emplace_back(tick);
		}
	}
	for (size_t block_index = 0; block_index < result.blocks.size(); ++block_index) {
		auto& block = result.blocks[block_index];
		if (block.domain != TimingSampleDomain::Cpu)
			continue;
		const auto parent = identities.find(parent_keys[block_index]);
		if (std::get<2>(parent_keys[block_index]) && parent != identities.end() &&
			parent->second != block_index)
			if (const auto& ancestor = result.blocks[parent->second];
				ancestor.track == block.track && ancestor.start_ns <= block.start_ns &&
				timeline_end(block.start_ns, block.duration_ns) <=
					timeline_end(ancestor.start_ns, ancestor.duration_ns))
				block.parent = parent->second;
			else
				block.hierarchy_note = "Invalid synchronous containment";
	}
	for (auto& block : result.blocks)
		if (block.domain == TimingSampleDomain::Cpu && block.details().parent_invocation_id &&
			block.parent == timeline_no_parent && block.hierarchy_note.empty())
			block.hierarchy_note = "Parent unavailable or invalid containment";
	std::vector<uint8_t> ancestry_state(result.blocks.size(), 0);
	std::vector<size_t> ancestry_path;
	ancestry_path.reserve(result.blocks.size());
	for (size_t block_index = 0; block_index < result.blocks.size(); ++block_index) {
		ancestry_path.clear();
		size_t ancestor = block_index;
		while (ancestor < result.blocks.size() && ancestry_state[ancestor] == 0) {
			ancestry_state[ancestor] = 1;
			ancestry_path.emplace_back(ancestor);
			ancestor = result.blocks[ancestor].parent;
		}
		if (ancestor < result.blocks.size() && ancestry_state[ancestor] == 1) {
			result.blocks[ancestor].parent = timeline_no_parent;
			result.blocks[ancestor].hierarchy_note = "Invalid parent cycle";
		}
		for (const auto visited : ancestry_path)
			ancestry_state[visited] = 2;
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
	const size_t aligned_count = result.blocks.size();
	using SubmissionKey = std::tuple<uint64_t, uint64_t, uint64_t, WindowFrameKey>;
	std::map<SubmissionKey, std::pair<uint64_t, uint32_t>> device_origins;
	std::map<SubmissionKey, long double> earliest_offsets;
	std::map<std::pair<SubmissionKey, uint32_t>, size_t> local_identities;
	const auto submission_key = [](const GpuTimingRecord& record) {
		return SubmissionKey{record.device_identity, record.queue_identity, record.submissionSerial,
							 record.frame};
	};
	for (size_t record_index = 0; record_index < unaligned_records.size(); ++record_index) {
		const auto& record = *unaligned_records[record_index];
		auto [origin, inserted] = device_origins.emplace(
			submission_key(record), std::pair{record.startTick, record.zone_index});
		if (!inserted && record.zone_index < origin->second.second)
			origin->second = {record.startTick, record.zone_index};
		if (record.zone_index != UINT32_MAX)
			local_identities.emplace(std::pair{submission_key(record), record.zone_index},
									 aligned_count + record_index);
	}
	const auto local_offset = [&](const GpuTimingRecord& record) {
		const auto reference = device_origins.at(submission_key(record)).first;
		const auto bits = std::clamp(record.timestamp_valid_bits, 1u, 64u);
		const uint64_t mask = bits == 64 ? UINT64_MAX : (uint64_t{1} << bits) - 1;
		const uint64_t forward = (record.startTick - reference) & mask;
		const long double period =
			record.timestamp_period_ns > 0 ? record.timestamp_period_ns
			: record.durationTicks
				? static_cast<long double>(record.durationNs) / record.durationTicks
				: 0;
		return forward <= mask / 2
				   ? static_cast<long double>(forward) * period
				   : -static_cast<long double>((reference - record.startTick) & mask) * period;
	};
	for (const auto* record_pointer : unaligned_records) {
		const auto& record = *record_pointer;
		auto [offset, inserted] =
			earliest_offsets.emplace(submission_key(record), local_offset(record));
		if (!inserted)
			offset->second = std::min(offset->second, local_offset(record));
	}
	for (const auto* record_pointer : unaligned_records) {
		const auto& record = *record_pointer;
		TimelineBlockSlice block;
		block.gpu_sample = {&record, 1};
		block.cpu_clock_aligned = false;
		block.duration_ns = record.durationNs;
		block.type_id = record.typeId;
		block.frame = record.frame;
		block.app_tick = record.appTick;
		block.track = record.queueFamilyIndex;
		block.domain = TimingSampleDomain::Gpu;
		block.submission_serial = record.submissionSerial;
		block.device_identity = record.device_identity;
		block.queue_identity = record.queue_identity;
		block.zone_index = record.zone_index;
		block.parent_zone_index = record.parentZoneIndex;
		block.scope_visible =
			selection.selected_scope.kind != DevPerformanceScopeKind::Thread &&
			(!selection.selected_scope.id || selection.selected_scope.id == record.frame.window);
		label_block(block);
		// Device-local origin is the earliest retained timestamp in this submission.
		const auto local_start =
			std::max(0.0L, local_offset(record) - earliest_offsets.at(submission_key(record)));
		block.start_ns = local_start >= static_cast<long double>(UINT64_MAX)
							 ? UINT64_MAX
							 : static_cast<uint64_t>(local_start);
		result.blocks.emplace_back(std::move(block));
	}
	for (size_t record_index = 0; record_index < unaligned_records.size(); ++record_index) {
		const auto& record = *unaligned_records[record_index];
		const auto parent = local_identities.find({submission_key(record), record.parentZoneIndex});
		if (parent != local_identities.end() && parent->second != aligned_count + record_index)
			result.blocks[aligned_count + record_index].parent = parent->second;
		else if (record.parentZoneIndex != UINT32_MAX)
			result.blocks[aligned_count + record_index].hierarchy_note =
				"Instrumented GPU parent unavailable";
	}
	result.tick_samples.reserve(result.ticks.size());
	for (const auto& tick : result.ticks) {
		result.tick_samples.emplace_back(result.blocks.size());
		result.blocks.emplace_back(tick);
		auto& sample = result.blocks.back();
		sample.synthetic_tick = true;
		sample.invocation_id = tick.app_tick;
		sample.category = TimingCategory::Frame;
		sample.hierarchy_note =
			sample.measured_cadence
				? "Synthetic tick cadence grouping; related work is associated, not contained"
				: "Synthetic recorded CPU envelope; related work is associated, not contained";
	}
	std::map<std::pair<uint64_t, uint64_t>, uint64_t> matching_frame_metrics;
	if (selection.selector_mode == 1 && selection.selected_zone)
		for (const auto& block : result.blocks)
			if (block.scope_visible && block.selected &&
				(selection.hardware_domain == 0 ||
				 (selection.hardware_domain == 1) == (block.domain == TimingSampleDomain::Cpu))) {
				auto& metric =
					matching_frame_metrics[{block.frame.window, block.frame.frameNumber}];
				metric = timeline_end(metric, block.duration_ns);
			}
	result.frame_metrics.reserve(result.frames.size());
	for (const auto& frame : result.frames)
		result.frame_metrics.emplace_back(
			selection.selector_mode == 1 && selection.selected_zone
				? matching_frame_metrics[{frame.frame.window, frame.frame.frameNumber}]
				: frame.duration_ns);
	result.child_offsets.assign(result.blocks.size() + 1, 0);
	for (const auto& block : result.blocks)
		if (block.parent < result.blocks.size())
			++result.child_offsets[block.parent + 1];
	for (size_t offset = 1; offset < result.child_offsets.size(); ++offset)
		result.child_offsets[offset] += result.child_offsets[offset - 1];
	result.children.resize(result.child_offsets.back());
	auto child_positions = result.child_offsets;
	for (size_t block_index = 0; block_index < result.blocks.size(); ++block_index)
		if (const auto parent = result.blocks[block_index].parent; parent < result.blocks.size())
			result.children[child_positions[parent]++] = block_index;
	auto sorted_metrics = result.frame_metrics;
	std::ranges::sort(sorted_metrics);
	if (!sorted_metrics.empty())
		result.percentile_ns = sorted_metrics[(sorted_metrics.size() - 1) * 95 / 100];
	return result;
}
void reset_timeline_minimap_scale(DevTimelineState& state,
								  const DevPerformanceSelection& selection) noexcept {
	state.minimap_maximum_ns = 33'333'334;
	if (selection.selector_mode == 1 && selection.selected_zone) {
		long double total = 0;
		size_t sample_count = 0;
		for (const auto metric : state.snapshot.frame_metrics)
			if (metric) {
				total += metric;
				++sample_count;
			}
		if (sample_count)
			state.minimap_maximum_ns = uint64_t(
				std::clamp(2 * total / sample_count, 1.0L, static_cast<long double>(UINT64_MAX)));
	} else {
		for (const auto metric : state.snapshot.frame_metrics)
			state.minimap_maximum_ns = std::max(state.minimap_maximum_ns, metric);
	}
}
TimelineSnapshot extract_timeline(std::span<const TimingAppTickReport> reports,
								  std::span<const TimingZoneDescriptor> descriptors,
								  const DevPerformanceSelection& selection) {
	return extract_timeline_segments({reports, {}}, descriptors, selection);
}
TimelineSnapshot extract_timeline(const TimingCaptureReadView& capture,
								  const DevPerformanceSelection& selection) {
	return extract_timeline_segments({capture.first, capture.second}, capture.descriptors,
									 selection);
}
std::vector<TimelineTrackLane> timeline_lanes(const TimelineSnapshot& snapshot, uint32_t depth,
											  std::span<const size_t> roots) {
	std::map<std::tuple<TimingSampleDomain, uint64_t, uint32_t>, std::vector<size_t>> groups;
	for (size_t block_index = 0; block_index < snapshot.blocks.size(); ++block_index) {
		const auto& block = snapshot.blocks[block_index];
		if (block.synthetic_tick)
			continue;
		if (roots.empty() && !block.cpu_clock_aligned)
			continue;
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
std::vector<TimelineTrackLane>
timeline_major_lanes(const TimelineSnapshot& snapshot, const DevPerformanceSelection& selection,
					 std::span<const TimelineTrackPreference> preferences) {
	std::map<std::tuple<TimingSampleDomain, uint64_t, uint64_t, uint64_t>, std::vector<size_t>>
		groups;
	for (size_t block_index = 0; block_index < snapshot.blocks.size(); ++block_index) {
		const auto& block = snapshot.blocks[block_index];
		bool candidate =
			block.parent == timeline_no_parent &&
			(block.domain != TimingSampleDomain::Cpu || !block.details().parent_invocation_id);
		if (selection.selector_mode == 1 && selection.selected_zone) {
			candidate = block.selected;
			size_t parent = block.parent, guard = 0;
			while (parent < snapshot.blocks.size() && guard++ < snapshot.blocks.size()) {
				if (snapshot.blocks[parent].selected)
					candidate = false;
				parent = snapshot.blocks[parent].parent;
			}
			if (guard >= snapshot.blocks.size())
				candidate = false;
		} else if (block.domain == TimingSampleDomain::Cpu &&
				   selection.selected_scope.kind == DevPerformanceScopeKind::Window) {
			candidate = block.type_id == timing_zones::kWindowFrameTotal.typeId || candidate;
		}
		const bool all_windows = selection.selected_scope.id == 0 &&
								 selection.selected_scope.kind == DevPerformanceScopeKind::Window &&
								 !(selection.selector_mode == 1 && selection.selected_zone);
		if (all_windows && block.domain == TimingSampleDomain::Cpu)
			candidate = block.synthetic_tick;
		if (candidate && block.scope_visible && block.cpu_clock_aligned &&
			(all_windows || !block.synthetic_tick))
			groups[{block.domain,
					block.domain == TimingSampleDomain::Gpu && block.queue_identity
						? block.queue_identity
						: block.track,
					(block.domain == TimingSampleDomain::Gpu ||
							 selection.selected_scope.kind == DevPerformanceScopeKind::Thread
						 ? 0
						 : block.frame.window),
					block.domain == TimingSampleDomain::Gpu ? block.device_identity : 0}]
				.emplace_back(block_index);
	}
	std::vector<TimelineTrackLane> lanes;
	lanes.reserve(groups.size());
	for (auto& [key, blocks] : groups) {
		std::ranges::sort(blocks, [&](size_t left, size_t right) {
			return std::tie(snapshot.blocks[left].start_ns, left) <
				   std::tie(snapshot.blocks[right].start_ns, right);
		});
		std::vector<uint64_t> ends;
		const size_t first_lane = lanes.size();
		for (const auto block_index : blocks) {
			const auto& block = snapshot.blocks[block_index];
			size_t sibling = 0;
			while (sibling < ends.size() && ends[sibling] > block.start_ns)
				++sibling;
			if (sibling == ends.size()) {
				ends.emplace_back(0);
				lanes.emplace_back(TimelineTrackLane{
					{}, std::get<1>(key), uint32_t(sibling), std::get<0>(key), std::get<2>(key)});
			}
			lanes[first_lane + sibling].blocks.emplace_back(block_index);
			ends[sibling] = timeline_end(block.start_ns, block.duration_ns);
		}
	}
	// Window frame totals lead the default overview; application work stays separate.
	std::ranges::stable_sort(lanes, [&](const auto& left, const auto& right) {
		const auto rank = [&](const auto& lane) {
			const auto& block = snapshot.blocks[lane.blocks.front()];
			return block.type_id == timing_zones::kWindowFrameTotal.typeId ? 0
				   : block.domain == TimingSampleDomain::Cpu			   ? 1
																		   : 2;
		};
		return rank(left) < rank(right);
	});
	const auto preference_index = [&](const TimelineTrackLane& lane) {
		const auto key = timeline_track_key(snapshot, lane);
		const auto found = std::ranges::find(preferences, key, &TimelineTrackPreference::key);
		return found == preferences.end() ? preferences.size()
										  : size_t(found - preferences.begin());
	};
	std::erase_if(lanes, [&](const auto& lane) {
		const auto index = preference_index(lane);
		return index < preferences.size() && preferences[index].hidden;
	});
	std::stable_sort(lanes.begin(), lanes.end(), [&](const auto& first, const auto& second) {
		const auto first_index = preference_index(first), second_index = preference_index(second);
		const bool first_pinned =
			first_index < preferences.size() && preferences[first_index].pinned;
		const bool second_pinned =
			second_index < preferences.size() && preferences[second_index].pinned;
		if (first_pinned != second_pinned)
			return first_pinned;
		return first_index < second_index;
	});
	return lanes;
}
TimelineTrackKey timeline_track_key(const TimelineSnapshot& snapshot,
									const TimelineTrackLane& lane) noexcept {
	const auto device = lane.domain == TimingSampleDomain::Gpu && !lane.blocks.empty()
							? snapshot.blocks[lane.blocks.front()].device_identity
							: 0;
	return TimelineTrackKey{device, lane.track, lane.window, lane.domain};
}
std::vector<TimelineCluster> cluster_timeline(const TimelineSnapshot& snapshot,
											  std::span<const size_t> blocks, uint64_t start_ns,
											  uint64_t duration_ns, float width,
											  size_t inspected_sample,
											  std::span<const size_t> protected_roots) {
	std::vector<TimelineCluster> clusters;
	clusters.reserve(blocks.size());
	const double scale = std::max(0.0f, width) / double(std::max(uint64_t{1}, duration_ns));
	bool previous_small = false;
	for (const size_t block_index : blocks) {
		if (block_index >= snapshot.blocks.size())
			continue;
		const auto& block = snapshot.blocks[block_index];
		const auto end_ns = timeline_end(block.start_ns, block.duration_ns);
		if ((block.duration_ns ? end_ns <= start_ns : block.start_ns < start_ns) ||
			block.start_ns >= timeline_end(start_ns, duration_ns))
			continue;
		const bool small = block.duration_ns * scale < 8;
		bool merge = false;
		if (!clusters.empty() && small && previous_small && !block.selected &&
			block_index != inspected_sample &&
			std::ranges::find(protected_roots, block_index) == protected_roots.end()) {
			auto& previous = clusters.back();
			const auto& previous_block = snapshot.blocks[previous.members.back()];
			const auto previous_end = timeline_end(previous.start_ns, previous.duration_ns);
			merge = previous_block.selected == block.selected &&
					(!block.selected || previous_block.type_id == block.type_id) &&
					previous.members.back() != inspected_sample &&
					std::ranges::find(protected_roots, previous.members.back()) ==
						protected_roots.end() &&
					previous_block.parent == block.parent &&
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
	state.visible_duration_ns = std::max(
		uint64_t{1}, (state.zoom == 1 ? range : static_cast<uint64_t>(range / state.zoom)));
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
	if (command.revision && command.revision != state.snapshot_revision)
		return;
	const auto frame_count = state.snapshot.frames.size();
	if (frame_count)
		state.selected_frame = std::min(state.selected_frame, frame_count - 1);
	switch (command.action) {
	case TimelineAction::Zoom:
		state.zoom = command.value;
		break;
	case TimelineAction::Domain:
		selection.hardware_domain = command.index;
		break;
	case TimelineAction::Previous:
	case TimelineAction::Next:
	case TimelineAction::Spike:
	case TimelineAction::Center: {
		if (!frame_count)
			break;
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
		state.minimap_follow_selection = true;
		center_timeline(state, state.snapshot.frames[state.selected_frame].start_ns);
		state.cards.clear();
		const auto& frame = state.snapshot.frames[state.selected_frame];
		for (size_t block_index = 0; block_index < state.snapshot.blocks.size(); ++block_index) {
			const auto& block = state.snapshot.blocks[block_index];
			if (block.scope_visible && block.domain == TimingSampleDomain::Cpu &&
				block.app_tick == frame.app_tick &&
				(!frame.frame.window ? block.synthetic_tick : !block.synthetic_tick) &&
				(block.type_id == timing_zones::kWindowFrameTotal.typeId ||
				 block.parent == timeline_no_parent) &&
				(!frame.frame.window || frame.frame == block.frame)) {
				state.cards.emplace_back(
					TimelineCard{{block_index}, 256, state.next_surface_identity++});
				state.inspected_sample = block_index;
				break;
			}
		}
		break;
	}
	case TimelineAction::Open:
		if (command.members.empty() || std::ranges::any_of(command.members, [&](size_t index) {
				return index >= state.snapshot.blocks.size();
			}))
			break;
		state.inspected_sample = command.members.front();
		if (command.index && !state.cards.empty() && state.cards.back().roots == command.members)
			break;
		state.cards.resize(std::min(command.index, state.cards.size()));
		state.cards.emplace_back(
			TimelineCard{std::move(command.members), 256, state.next_surface_identity++});
		if (!command.index) {
			state.minimap_follow_selection = true;
			const auto& root = state.snapshot.blocks[state.inspected_sample];
			for (size_t frame_index = 0; frame_index < state.snapshot.frames.size();
				 ++frame_index) {
				const auto& frame = state.snapshot.frames[frame_index];
				if (frame.app_tick == root.app_tick &&
					(!frame.frame.window || frame.frame == root.frame)) {
					state.selected_frame = frame_index;
					break;
				}
			}
		}
		state.restore_minor_scroll = true;
		break;
	case TimelineAction::Close:
	case TimelineAction::Breadcrumb:
		state.cards.resize(std::min(command.index, state.cards.size()));
		state.restore_minor_scroll = true;
		break;
	case TimelineAction::Depth: {
		auto* depth = command.index == timeline_no_parent ? &state.active_depth
					  : command.index < state.cards.size()
						  ? &state.cards[command.index].active_depth
						  : nullptr;
		if (depth)
			*depth = static_cast<uint32_t>(std::clamp(int(*depth) + int(command.value), 1, 256));
		break;
	}
	case TimelineAction::Inspect:
		if (command.members.size() == 1 && command.members.front() < state.snapshot.blocks.size())
			state.inspected_sample = command.members.front();
		break;
	case TimelineAction::TrackControls:
		state.track_controls_open = !state.track_controls_open;
		break;
	case TimelineAction::TrackHide:
	case TimelineAction::TrackPin:
	case TimelineAction::TrackMove: {
		if (command.members.empty() || command.members.front() >= state.snapshot.blocks.size())
			break;
		const auto lanes = timeline_major_lanes(state.snapshot, state.selection);
		state.track_preferences.reserve(state.track_preferences.size() + lanes.size() + 1);
		if (state.snapshot.blocks[command.members.front()].synthetic_tick &&
			std::ranges::none_of(state.track_preferences,
								 [](const auto& preference) { return preference.key.contextual; }))
			state.track_preferences.emplace_back(
				TimelineTrackPreference{TimelineTrackKey{0, 0, 0, TimingSampleDomain::Cpu, true}});
		for (const auto& lane : lanes) {
			const auto key = timeline_track_key(state.snapshot, lane);
			if (std::ranges::find(state.track_preferences, key, &TimelineTrackPreference::key) ==
				state.track_preferences.end())
				state.track_preferences.emplace_back(TimelineTrackPreference{key});
		}
		const auto key_for_sample = [&](size_t sample_index) {
			const auto& block = state.snapshot.blocks[sample_index];
			return TimelineTrackKey{
				block.domain == TimingSampleDomain::Gpu ? block.device_identity : 0,
				block.domain == TimingSampleDomain::Gpu && block.queue_identity
					? block.queue_identity
					: block.track,
				block.domain == TimingSampleDomain::Gpu ||
						state.selection.selected_scope.kind == DevPerformanceScopeKind::Thread
					? 0
					: block.frame.window,
				block.domain, block.synthetic_tick};
		};
		auto preference =
			std::ranges::find(state.track_preferences, key_for_sample(command.members.front()),
							  &TimelineTrackPreference::key);
		if (preference == state.track_preferences.end())
			break;
		if (command.action == TimelineAction::TrackHide)
			preference->hidden = !preference->hidden;
		else if (command.action == TimelineAction::TrackPin)
			preference->pinned = !preference->pinned;
		else if (command.members.size() > 1 && command.members[1] < state.snapshot.blocks.size()) {
			const auto neighbor =
				std::ranges::find(state.track_preferences, key_for_sample(command.members[1]),
								  &TimelineTrackPreference::key);
			if (neighbor != state.track_preferences.end())
				std::iter_swap(preference, neighbor);
		}
		++state.track_preferences_revision;
		break;
	}
	case TimelineAction::MinimapZoom:
		state.minimap_zoom =
			std::clamp(std::isfinite(command.value) ? command.value : 1.0, 1.0, 1000.0);
		break;
	case TimelineAction::FitSelected:
		if (!command.members.empty() && command.members.front() < state.snapshot.blocks.size())
			state.inspected_sample = command.members.front();
		if (state.inspected_sample < state.snapshot.blocks.size() &&
			state.snapshot.blocks[state.inspected_sample].cpu_clock_aligned) {
			const auto& block = state.snapshot.blocks[state.inspected_sample];
			const TimelineTrackKey key{
				block.domain == TimingSampleDomain::Gpu ? block.device_identity : 0,
				block.domain == TimingSampleDomain::Gpu && block.queue_identity
					? block.queue_identity
					: block.track,
				block.domain == TimingSampleDomain::Gpu ||
						state.selection.selected_scope.kind == DevPerformanceScopeKind::Thread
					? 0
					: block.frame.window,
				block.domain, block.synthetic_tick};
			const auto preference =
				std::ranges::find(state.track_preferences, key, &TimelineTrackPreference::key);
			if (preference != state.track_preferences.end() && preference->hidden) {
				preference->hidden = false;
				++state.track_preferences_revision;
			}
			const auto history_duration =
				std::max(uint64_t{1}, state.snapshot.end_ns - state.snapshot.start_ns);
			state.zoom = double(history_duration) / std::max(1.0, double(block.duration_ns) * 1.2);
			clamp_timeline_view(state);
			center_timeline(state, timeline_end(block.start_ns, block.duration_ns / 2));
		}
		break;
	case TimelineAction::None:
		break;
	}
	clamp_timeline_view(state);
}
} // namespace FlowUi::devSystems::interface_elements
#endif
