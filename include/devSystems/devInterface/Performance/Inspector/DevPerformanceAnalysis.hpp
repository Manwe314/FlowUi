#pragma once
#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <tuple>

namespace FlowUi::devSystems::interface_elements {
/** Display refresh gate. A changed data context refreshes immediately; ordinary frames count down.
 */
struct PerformanceRefreshGate {
	uint32_t frames_remaining = 0;
	/** Return true on the first frame, a context change, or every 240 subsequent builds. */
	[[nodiscard]] bool advance(bool context_changed) noexcept {
		if (context_changed || frames_remaining == 0 || --frames_remaining == 0) {
			frames_remaining = 240;
			return true;
		}
		return false;
	}
};
/** Owned measurement identity; no borrowed capture data survives the read lease. */
struct PerformanceAnalysisSample {
	uint64_t duration_ns = 0, exclusive_ns = 0, type_id = 0, invocation_id = 0;
	uint64_t app_tick = 0, track = 0, device = 0, queue = 0, submission = 0;
	WindowFrameKey frame{};
	uint32_t zone_index = UINT32_MAX;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	bool self_valid = false;
	/** Comparison populations never mix CPU threads or GPU devices/queues. */
	[[nodiscard]] auto population() const noexcept {
		return std::tuple{domain, type_id, frame.window, track, device, queue};
	}
	/** Match a freshly leased timeline record by stable semantic identity. */
	[[nodiscard]] bool matches(const TimelineBlockSlice& block) const noexcept {
		return !block.synthetic_tick && domain == block.domain && type_id == block.type_id &&
			   app_tick == block.app_tick && frame == block.frame && track == block.track &&
			   (domain == TimingSampleDomain::Cpu
					? invocation_id == block.invocation_id
					: device == block.device_identity && queue == block.queue_identity &&
						  submission == block.submission_serial && zone_index == block.zone_index);
	}
};
/** One comparable population, with separately labeled inclusive and recorded self totals. */
struct PerformanceAnalysisZone {
	std::string label{};
	std::vector<uint64_t> durations{};
	PerformanceAnalysisSample largest{};
	uint64_t total_ns = 0, self_ns = 0, self_count = 0;
	size_t first_sample = 0, sample_end = 0;
	TimingCategory category = TimingCategory::User;
	TimingZoneRole role = TimingZoneRole::Work;
};
/** Bounded retained timing analysis, independent of investigation filters. */
struct PerformanceAnalysis {
	std::vector<PerformanceAnalysisSample> samples{};
	std::vector<PerformanceAnalysisZone> zones{};
	std::vector<uint64_t> cadence{}, sorted_cadence{};
	uint64_t generation = 0, first_tick = 0, last_tick = 0, coverage_ns = 0;
	uint64_t complete_ticks = 0, excluded_intervals = 0, excluded_samples = 0;
	uint64_t dropped_records = 0;
};
/** Nearest-rank percentile. Empty distributions have no measurement. */
[[nodiscard]] inline uint64_t performance_percentile(std::span<const uint64_t> sorted,
													 double percentile) noexcept {
	if (sorted.empty())
		return 0;
	const auto rank = size_t(std::ceil(std::clamp(percentile, 0.0, 1.0) * sorted.size()));
	return sorted[std::min(sorted.size() - 1, rank ? rank - 1 : 0)];
}
/** Duration text preserves sub-millisecond optimization opportunities. */
[[nodiscard]] inline std::string performance_duration(double duration_ns) {
	if (duration_ns > 0 && duration_ns < 1)
		return "<0.001 us";
	char buffer[64]{};
	std::snprintf(buffer, sizeof(buffer), "%.3f %s", duration_ns / (duration_ns >= 1e6 ? 1e6 : 1e3),
				  duration_ns >= 1e6 ? "ms" : "us");
	return buffer;
}
/** Completed CPU records without cancellation or clock errors are duration-eligible. */
[[nodiscard]] inline bool performance_sample_valid(const TimelineBlockSlice& block) noexcept {
	if (block.synthetic_tick || block.category == TimingCategory::DevTool ||
		block.role == TimingZoneRole::DevToolWork)
		return false;
	const auto flags = block.details().quality_flags;
	if (block.domain == TimingSampleDomain::Gpu)
		return (flags & gpuTimingRecordFlags(GpuTimingRecordFlag::Completed)) != 0;
	constexpr auto rejected = timingRecordFlags(TimingRecordFlag::Canceled) |
							  timingRecordFlags(TimingRecordFlag::Incomplete) |
							  timingRecordFlags(TimingRecordFlag::ClockAnomaly) |
							  timingRecordFlags(TimingRecordFlag::OutOfDate) |
							  timingRecordFlags(TimingRecordFlag::Exception);
	return (flags & timingRecordFlags(TimingRecordFlag::Completed)) && !(flags & rejected);
}
/** Extract owned aggregates while a capture is leased. Range is complete retained application
 * ticks. */
[[nodiscard]] inline PerformanceAnalysis analyze_performance(const TimingCaptureReadView& capture,
															 uint64_t tick_limit,
															 WindowId cadence_window) {
	PerformanceAnalysis result;
	result.generation = capture.generation;
	std::vector<const TimingAppTickReport*> ticks;
	ticks.reserve(capture.first.size() + capture.second.size());
	for (const auto segment : {capture.first, capture.second})
		for (const auto& tick : segment)
			if (tick.occupied)
				ticks.emplace_back(&tick);
	const size_t first_position =
		tick_limit && ticks.size() > tick_limit ? ticks.size() - tick_limit : 0;
	if (first_position == ticks.size())
		return result;
	result.first_tick = ticks[first_position]->appTick;
	result.last_tick = ticks.back()->appTick;
	result.cadence.reserve(ticks.size() - first_position);
	for (size_t tick_position = first_position; tick_position < ticks.size(); ++tick_position) {
		const auto& tick = *ticks[tick_position];
		const bool complete = !tick.boundary_open && tick.boundary_end_ns > tick.boundary_start_ns;
		if (complete) {
			++result.complete_ticks;
			result.coverage_ns += tick.boundary_end_ns - tick.boundary_start_ns;
			if (!cadence_window)
				result.cadence.emplace_back(tick.boundary_end_ns - tick.boundary_start_ns);
		} else
			++result.excluded_intervals;
		result.dropped_records += tick.cpuQuality.droppedRecords + tick.gpuQuality.droppedRecords;
		if (cadence_window)
			for (const auto& window : tick.windows)
				if (window.occupied && window.window == cadence_window)
					for (const auto& frame : window.frames)
						if (frame.occupied)
							for (const auto& sample : frame.cpuZones)
								if (sample.typeId == timing_zones::kWindowFrameTotal.typeId &&
									(sample.flags &
									 timingRecordFlags(TimingRecordFlag::Completed)) &&
									!(sample.flags &
									  (timingRecordFlags(TimingRecordFlag::Incomplete) |
									   timingRecordFlags(TimingRecordFlag::ClockAnomaly) |
									   timingRecordFlags(TimingRecordFlag::Canceled))))
									result.cadence.emplace_back(sample.durationNs);
	}
	result.sorted_cadence = result.cadence;
	std::ranges::sort(result.sorted_cadence);
	DevPerformanceSelection all_application;
	const auto snapshot = extract_timeline(capture, all_application);
	result.samples.reserve(snapshot.blocks.size());
	for (const auto& block : snapshot.blocks) {
		if (block.synthetic_tick || block.app_tick < result.first_tick ||
			block.app_tick > result.last_tick || block.category == TimingCategory::DevTool ||
			block.role == TimingZoneRole::DevToolWork)
			continue;
		if (!performance_sample_valid(block)) {
			++result.excluded_samples;
			continue;
		}
		const auto details = block.details();
		const bool self_valid =
			block.domain == TimingSampleDomain::Cpu && block.hierarchy_note.empty() &&
			!(details.quality_flags & timingRecordFlags(TimingRecordFlag::DetailTruncated)) &&
			!block.cpu_sample.empty() &&
			block.cpu_sample.front().directChildNs <= block.duration_ns;
		result.samples.emplace_back(PerformanceAnalysisSample{
			block.duration_ns, details.exclusive_ns, block.type_id, block.invocation_id,
			block.app_tick, block.track, block.device_identity, block.queue_identity,
			block.submission_serial, block.frame, block.zone_index, block.domain, self_valid});
	}
	std::ranges::sort(result.samples, [](const auto& left, const auto& right) {
		return std::tuple{left.population(), left.app_tick, left.invocation_id, left.submission,
						  left.zone_index} < std::tuple{right.population(), right.app_tick,
														right.invocation_id, right.submission,
														right.zone_index};
	});
	result.zones.reserve(std::min(result.samples.size(), capture.descriptors.size() * 2));
	for (size_t sample_position = 0; sample_position < result.samples.size();) {
		const auto& first = result.samples[sample_position];
		PerformanceAnalysisZone zone;
		zone.first_sample = sample_position;
		zone.largest = first;
		const auto descriptor =
			std::ranges::find(capture.descriptors, first.type_id, &TimingZoneDescriptor::typeId);
		if (descriptor != capture.descriptors.end()) {
			zone.label = performance_zone_label(descriptor->name);
			zone.category = descriptor->category;
			zone.role = descriptor->role;
		} else
			zone.label = "Unknown zone";
		size_t population_end = sample_position;
		while (population_end < result.samples.size() &&
			   result.samples[population_end].population() == first.population())
			++population_end;
		zone.durations.reserve(population_end - sample_position);
		for (; sample_position < population_end; ++sample_position) {
			const auto& sample = result.samples[sample_position];
			zone.durations.emplace_back(sample.duration_ns);
			zone.total_ns += sample.duration_ns;
			if (sample.self_valid) {
				zone.self_ns += sample.exclusive_ns;
				++zone.self_count;
			}
			if (sample.duration_ns > zone.largest.duration_ns)
				zone.largest = sample;
		}
		zone.sample_end = sample_position;
		std::ranges::sort(zone.durations);
		result.zones.emplace_back(std::move(zone));
	}
	return result;
}
} // namespace FlowUi::devSystems::interface_elements
#endif
