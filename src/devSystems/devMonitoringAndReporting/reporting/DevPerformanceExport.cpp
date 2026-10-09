#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceExport.hpp"
#if FLOW_UI_DEV_MODE
#include "internal/Resources/ExecutableDirectory.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <type_traits>
namespace FlowUi::devSystems {
namespace {
constexpr std::array<std::string_view, 132> columns{
	"schema_version",
	"record_kind",
	"capture_generation",
	"app_tick",
	"window_id",
	"frame_number",
	"record_index",
	"scope",
	"start_ns",
	"duration_ns",
	"direct_child_ns",
	"invocation_id",
	"parent_invocation_id",
	"zone_type_id",
	"primary_entity_id",
	"secondary_entity_id",
	"track",
	"entity_kind",
	"depth",
	"quality_flags",
	"exclusive_ns",
	"start_tick",
	"duration_ticks",
	"timestamp_period_ns",
	"timestamp_valid_bits",
	"cpu_aligned_start_ns",
	"calibration_maximum_deviation_ns",
	"submission_serial",
	"device_identity",
	"queue_identity",
	"calibration_id",
	"zone_index",
	"parent_zone_index",
	"queue_family_index",
	"begin_stage",
	"end_stage",
	"zone_name",
	"zone_category",
	"zone_role",
	"zone_minimum_cpu_level",
	"source_file",
	"source_function",
	"source_line",
	"source_column",
	"aggregate_app_tick",
	"aggregate_invocation_count",
	"aggregate_total_inclusive_ns",
	"aggregate_maximum_inclusive_ns",
	"aggregate_canceled_invocation_count",
	"aggregate_definition_id",
	"config_balanced_element_retention_threshold_ns",
	"config_cpu_level",
	"config_enabled_category_mask",
	"config_gpu_query_capacity_per_frame",
	"config_producer_record_capacity",
	"config_gpu_timing_enabled",
	"config_selected_element_instance",
	"config_selected_element_definition",
	"cpu_quality_recorded_zones",
	"cpu_quality_suppressed_zones",
	"cpu_quality_dropped_records",
	"cpu_quality_stack_overflows",
	"cpu_quality_misnested_zones",
	"cpu_quality_incomplete_zones",
	"cpu_quality_clock_anomalies",
	"cpu_quality_descriptor_collisions",
	"cpu_quality_timing_overhead_ns",
	"gpu_quality_resolved_submissions",
	"gpu_quality_recorded_zones",
	"gpu_quality_truncated_zones",
	"gpu_quality_unavailable_queries",
	"gpu_quality_query_pool_failures",
	"gpu_quality_query_read_failures",
	"gpu_quality_dropped_records",
	"gpu_quality_supported",
	"gpu_quality_synchronization2_available",
	"gpu_quality_calibrated",
	"gpu_quality_timestamp_valid_bits",
	"gpu_quality_timestamp_period_ns",
	"reporting_configured_capacity",
	"reporting_effective_capacity",
	"reporting_rolling_sample_capacity",
	"reporting_maximum_frames_in_flight",
	"reporting_retained_tick_count",
	"reporting_oldest_retained_app_tick",
	"reporting_newest_retained_app_tick",
	"reporting_total_published_ticks",
	"reporting_evicted_ticks",
	"reporting_late_records_after_eviction",
	"reporting_ingestion_failures",
	"reporting_mutation_sequence",
	"reporting_capture_generation",
	"reporting_capture_admitted_ticks",
	"reporting_capture_overwritten_ticks",
	"reporting_capture_first_app_tick",
	"reporting_capture_end_app_tick_exclusive",
	"reporting_not_retained_by_policy",
	"reporting_late_after_seal",
	"reporting_capture_dropped_records",
	"reporting_capture_ingestion_failures",
	"reporting_capture_gpu_failures",
	"reporting_capture_recording",
	"reporting_capture_sealed",
	"reporting_has_retained_ticks",
	"capture_start_ns",
	"capture_end_ns",
	"capture_event_end_ns",
	"capture_event_duration_ns",
	"capture_event_app_tick",
	"capture_first_app_tick",
	"capture_end_app_tick_exclusive",
	"capture_pending_measurements",
	"capture_phase",
	"capture_stop_reason",
	"capture_incomplete",
	"capture_settings_duration_ns",
	"capture_settings_budget_ns",
	"capture_settings_post_event_ns",
	"capture_settings_budget_window",
	"capture_settings_start_mode",
	"capture_settings_end_mode",
	"capture_settings_shortcut_key",
	"capture_settings_shortcut_ctrl",
	"capture_settings_shortcut_shift",
	"capture_settings_shortcut_alt",
	"capture_settings_shortcut_super",
	"mutation_sequence",
	"boundary_start_ns",
	"boundary_end_ns",
	"boundary_open",
	"tick_revision",
	"occupied",
};
enum class Column : size_t {
	schema_version,
	record_kind,
	capture_generation,
	app_tick,
	window_id,
	frame_number,
	record_index,
	scope,
	start_ns,
	duration_ns,
	direct_child_ns,
	invocation_id,
	parent_invocation_id,
	zone_type_id,
	primary_entity_id,
	secondary_entity_id,
	track,
	entity_kind,
	depth,
	quality_flags,
	exclusive_ns,
	start_tick,
	duration_ticks,
	timestamp_period_ns,
	timestamp_valid_bits,
	cpu_aligned_start_ns,
	calibration_maximum_deviation_ns,
	submission_serial,
	device_identity,
	queue_identity,
	calibration_id,
	zone_index,
	parent_zone_index,
	queue_family_index,
	begin_stage,
	end_stage,
	zone_name,
	zone_category,
	zone_role,
	zone_minimum_cpu_level,
	source_file,
	source_function,
	source_line,
	source_column,
	aggregate_app_tick,
	aggregate_invocation_count,
	aggregate_total_inclusive_ns,
	aggregate_maximum_inclusive_ns,
	aggregate_canceled_invocation_count,
	aggregate_definition_id,
	config_balanced_element_retention_threshold_ns,
	config_cpu_level,
	config_enabled_category_mask,
	config_gpu_query_capacity_per_frame,
	config_producer_record_capacity,
	config_gpu_timing_enabled,
	config_selected_element_instance,
	config_selected_element_definition,
	cpu_quality_recorded_zones,
	cpu_quality_suppressed_zones,
	cpu_quality_dropped_records,
	cpu_quality_stack_overflows,
	cpu_quality_misnested_zones,
	cpu_quality_incomplete_zones,
	cpu_quality_clock_anomalies,
	cpu_quality_descriptor_collisions,
	cpu_quality_timing_overhead_ns,
	gpu_quality_resolved_submissions,
	gpu_quality_recorded_zones,
	gpu_quality_truncated_zones,
	gpu_quality_unavailable_queries,
	gpu_quality_query_pool_failures,
	gpu_quality_query_read_failures,
	gpu_quality_dropped_records,
	gpu_quality_supported,
	gpu_quality_synchronization2_available,
	gpu_quality_calibrated,
	gpu_quality_timestamp_valid_bits,
	gpu_quality_timestamp_period_ns,
	reporting_configured_capacity,
	reporting_effective_capacity,
	reporting_rolling_sample_capacity,
	reporting_maximum_frames_in_flight,
	reporting_retained_tick_count,
	reporting_oldest_retained_app_tick,
	reporting_newest_retained_app_tick,
	reporting_total_published_ticks,
	reporting_evicted_ticks,
	reporting_late_records_after_eviction,
	reporting_ingestion_failures,
	reporting_mutation_sequence,
	reporting_capture_generation,
	reporting_capture_admitted_ticks,
	reporting_capture_overwritten_ticks,
	reporting_capture_first_app_tick,
	reporting_capture_end_app_tick_exclusive,
	reporting_not_retained_by_policy,
	reporting_late_after_seal,
	reporting_capture_dropped_records,
	reporting_capture_ingestion_failures,
	reporting_capture_gpu_failures,
	reporting_capture_recording,
	reporting_capture_sealed,
	reporting_has_retained_ticks,
	capture_start_ns,
	capture_end_ns,
	capture_event_end_ns,
	capture_event_duration_ns,
	capture_event_app_tick,
	capture_first_app_tick,
	capture_end_app_tick_exclusive,
	capture_pending_measurements,
	capture_phase,
	capture_stop_reason,
	capture_incomplete,
	capture_settings_duration_ns,
	capture_settings_budget_ns,
	capture_settings_post_event_ns,
	capture_settings_budget_window,
	capture_settings_start_mode,
	capture_settings_end_mode,
	capture_settings_shortcut_key,
	capture_settings_shortcut_ctrl,
	capture_settings_shortcut_shift,
	capture_settings_shortcut_alt,
	capture_settings_shortcut_super,
	mutation_sequence,
	boundary_start_ns,
	boundary_end_ns,
	boundary_open,
	tick_revision,
	occupied,
};
void quoted(std::ostream& output, std::string_view value) {
	output.put('"');
	for (const char character : value) {
		if (character == '"')
			output.put('"');
		output.put(character);
	}
	output.put('"');
}
struct CsvRow {
	std::array<std::string, columns.size()> values{};
	void begin(std::string_view kind, uint64_t generation, uint64_t tick = 0, uint64_t window = 0,
			   uint64_t frame = 0, size_t index = 0) {
		for (auto& value : values)
			value.clear();
		field(Column::schema_version, 1);
		field(Column::record_kind, kind);
		field(Column::capture_generation, generation);
		field(Column::app_tick, tick);
		field(Column::window_id, window);
		field(Column::frame_number, frame);
		field(Column::record_index, index);
	}
	void field(Column index, std::string_view value) { values[static_cast<size_t>(index)] = value; }
	template <class Value>
	void field(Column index, Value value) {
		if constexpr (std::is_enum_v<Value>)
			field(index, static_cast<std::underlying_type_t<Value>>(value));
		else if constexpr (std::is_floating_point_v<Value>) {
			std::ostringstream formatted;
			formatted.imbue(std::locale::classic());
			formatted << std::setprecision(std::numeric_limits<Value>::max_digits10) << value;
			values[static_cast<size_t>(index)] = formatted.str();
		} else
			values[static_cast<size_t>(index)] = std::to_string(value);
	}
	void emit(std::ostream& output) const {
		for (size_t index = 0; index < values.size(); ++index) {
			if (index)
				output.put(',');
			quoted(output, values[index]);
		}
		output << "\r\n";
	}
};
void write_cpu(CsvRow& row, const CpuTimingRecord& sample) {
	row.field(Column::start_ns, sample.startNs);
	row.field(Column::duration_ns, sample.durationNs);
	row.field(Column::direct_child_ns, sample.directChildNs);
	row.field(Column::invocation_id, sample.invocationId);
	row.field(Column::parent_invocation_id, sample.parentInvocationId);
	row.field(Column::zone_type_id, sample.typeId);
	row.field(Column::app_tick, sample.appTick);
	row.field(Column::primary_entity_id, sample.primaryEntityId);
	row.field(Column::secondary_entity_id, sample.secondaryEntityId);
	row.field(Column::track, sample.track);
	row.field(Column::entity_kind, sample.entityKind);
	row.field(Column::depth, sample.depth);
	row.field(Column::quality_flags, sample.flags);
	row.field(Column::window_id, sample.frame.window);
	row.field(Column::frame_number, sample.frame.frameNumber);
	row.field(Column::exclusive_ns, sample.exclusiveNs());
}
void write_gpu(CsvRow& row, const GpuTimingRecord& sample) {
	row.field(Column::start_tick, sample.startTick);
	row.field(Column::duration_ticks, sample.durationTicks);
	row.field(Column::duration_ns, sample.durationNs);
	row.field(Column::timestamp_period_ns, sample.timestamp_period_ns);
	row.field(Column::timestamp_valid_bits, sample.timestamp_valid_bits);
	row.field(Column::cpu_aligned_start_ns, sample.cpuAlignedStartNs);
	row.field(Column::calibration_maximum_deviation_ns, sample.calibrationMaximumDeviationNs);
	row.field(Column::submission_serial, sample.submissionSerial);
	row.field(Column::device_identity, sample.device_identity);
	row.field(Column::queue_identity, sample.queue_identity);
	row.field(Column::zone_type_id, sample.typeId);
	row.field(Column::app_tick, sample.appTick);
	row.field(Column::primary_entity_id, sample.primaryEntityId);
	row.field(Column::secondary_entity_id, sample.secondaryEntityId);
	row.field(Column::calibration_id, sample.calibrationId);
	row.field(Column::zone_index, sample.zone_index);
	row.field(Column::parent_zone_index, sample.parentZoneIndex);
	row.field(Column::queue_family_index, sample.queueFamilyIndex);
	row.field(Column::begin_stage, sample.beginStage);
	row.field(Column::end_stage, sample.endStage);
	row.field(Column::entity_kind, sample.entityKind);
	row.field(Column::depth, sample.depth);
	row.field(Column::quality_flags, sample.flags);
	row.field(Column::window_id, sample.frame.window);
	row.field(Column::frame_number, sample.frame.frameNumber);
}
void write_descriptor(CsvRow& row, const TimingZoneDescriptor& descriptor) {
	row.field(Column::zone_type_id, descriptor.typeId);
	row.field(Column::zone_name, descriptor.name);
	row.field(Column::zone_category, descriptor.category);
	row.field(Column::zone_role, descriptor.role);
	row.field(Column::zone_minimum_cpu_level, descriptor.minimumCpuLevel);
	row.field(Column::source_file, descriptor.source.file);
	row.field(Column::source_function, descriptor.source.function);
	row.field(Column::source_line, descriptor.source.line);
	row.field(Column::source_column, descriptor.source.column);
}
void write_aggregate(CsvRow& row, const ElementDefinitionTimingAggregate& aggregate) {
	row.field(Column::aggregate_app_tick, aggregate.appTick);
	row.field(Column::aggregate_invocation_count, aggregate.invocationCount);
	row.field(Column::aggregate_total_inclusive_ns, aggregate.totalInclusiveNs);
	row.field(Column::aggregate_maximum_inclusive_ns, aggregate.maximumInclusiveNs);
	row.field(Column::aggregate_canceled_invocation_count, aggregate.canceledInvocationCount);
	row.field(Column::window_id, aggregate.frame.window);
	row.field(Column::frame_number, aggregate.frame.frameNumber);
	row.field(Column::aggregate_definition_id, aggregate.definition.value);
}
void write_config(CsvRow& row, const DevTimingConfig& config) {
	row.field(Column::config_balanced_element_retention_threshold_ns,
			  config.balancedElementRetentionThresholdNs);
	row.field(Column::config_cpu_level, config.cpuLevel);
	row.field(Column::config_enabled_category_mask, config.enabledCategoryMask);
	row.field(Column::config_gpu_query_capacity_per_frame, config.gpuQueryCapacityPerFrame);
	row.field(Column::config_producer_record_capacity, config.producerRecordCapacity);
	row.field(Column::config_gpu_timing_enabled, config.gpuTimingEnabled);
	row.field(Column::config_selected_element_instance, config.selectedElementInstance.value);
	row.field(Column::config_selected_element_definition, config.selectedElementDefinition.value);
}
void write_cpu_quality(CsvRow& row, const TimingQualitySnapshot& quality) {
	row.field(Column::cpu_quality_recorded_zones, quality.recordedZones);
	row.field(Column::cpu_quality_suppressed_zones, quality.suppressedZones);
	row.field(Column::cpu_quality_dropped_records, quality.droppedRecords);
	row.field(Column::cpu_quality_stack_overflows, quality.stackOverflows);
	row.field(Column::cpu_quality_misnested_zones, quality.misnestedZones);
	row.field(Column::cpu_quality_incomplete_zones, quality.incompleteZones);
	row.field(Column::cpu_quality_clock_anomalies, quality.clockAnomalies);
	row.field(Column::cpu_quality_descriptor_collisions, quality.descriptorCollisions);
	row.field(Column::cpu_quality_timing_overhead_ns, quality.timingOverheadNs);
}
void write_gpu_quality(CsvRow& row, const GpuTimingQualitySnapshot& quality) {
	row.field(Column::gpu_quality_resolved_submissions, quality.resolvedSubmissions);
	row.field(Column::gpu_quality_recorded_zones, quality.recordedZones);
	row.field(Column::gpu_quality_truncated_zones, quality.truncatedZones);
	row.field(Column::gpu_quality_unavailable_queries, quality.unavailableQueries);
	row.field(Column::gpu_quality_query_pool_failures, quality.queryPoolFailures);
	row.field(Column::gpu_quality_query_read_failures, quality.queryReadFailures);
	row.field(Column::gpu_quality_dropped_records, quality.droppedRecords);
	row.field(Column::gpu_quality_supported, quality.supported);
	row.field(Column::gpu_quality_synchronization2_available, quality.synchronization2Available);
	row.field(Column::gpu_quality_calibrated, quality.calibrated);
	row.field(Column::gpu_quality_timestamp_valid_bits, quality.timestampValidBits);
	row.field(Column::gpu_quality_timestamp_period_ns, quality.timestampPeriodNs);
}
void write_reporting(CsvRow& row, const TimingReportingStatus& status) {
	row.field(Column::reporting_configured_capacity, status.configuredCapacity);
	row.field(Column::reporting_effective_capacity, status.effectiveCapacity);
	row.field(Column::reporting_rolling_sample_capacity, status.rollingSampleCapacity);
	row.field(Column::reporting_maximum_frames_in_flight, status.maximumFramesInFlight);
	row.field(Column::reporting_retained_tick_count, status.retainedTickCount);
	row.field(Column::reporting_oldest_retained_app_tick, status.oldestRetainedAppTick);
	row.field(Column::reporting_newest_retained_app_tick, status.newestRetainedAppTick);
	row.field(Column::reporting_total_published_ticks, status.totalPublishedTicks);
	row.field(Column::reporting_evicted_ticks, status.evictedTicks);
	row.field(Column::reporting_late_records_after_eviction, status.lateRecordsAfterEviction);
	row.field(Column::reporting_ingestion_failures, status.ingestionFailures);
	row.field(Column::reporting_mutation_sequence, status.mutationSequence);
	row.field(Column::reporting_capture_generation, status.capture_generation);
	row.field(Column::reporting_capture_admitted_ticks, status.capture_admitted_ticks);
	row.field(Column::reporting_capture_overwritten_ticks, status.capture_overwritten_ticks);
	row.field(Column::reporting_capture_first_app_tick, status.capture_first_app_tick);
	row.field(Column::reporting_capture_end_app_tick_exclusive,
			  status.capture_end_app_tick_exclusive);
	row.field(Column::reporting_not_retained_by_policy, status.not_retained_by_policy);
	row.field(Column::reporting_late_after_seal, status.late_after_seal);
	row.field(Column::reporting_capture_dropped_records, status.capture_dropped_records);
	row.field(Column::reporting_capture_ingestion_failures, status.capture_ingestion_failures);
	row.field(Column::reporting_capture_gpu_failures, status.capture_gpu_failures);
	row.field(Column::reporting_capture_recording, status.capture_recording);
	row.field(Column::reporting_capture_sealed, status.capture_sealed);
	row.field(Column::reporting_has_retained_ticks, status.hasRetainedTicks);
}
void write_capture(CsvRow& row, const PerformanceCaptureStatus& status) {
	row.field(Column::capture_generation, status.generation);
	row.field(Column::capture_start_ns, status.start_ns);
	row.field(Column::capture_end_ns, status.end_ns);
	row.field(Column::capture_event_end_ns, status.event_end_ns);
	row.field(Column::capture_event_duration_ns, status.event_duration_ns);
	row.field(Column::capture_event_app_tick, status.event_app_tick);
	row.field(Column::capture_first_app_tick, status.first_app_tick);
	row.field(Column::capture_end_app_tick_exclusive, status.end_app_tick_exclusive);
	row.field(Column::capture_pending_measurements, status.pending_measurements);
	row.field(Column::capture_phase, status.phase);
	row.field(Column::capture_stop_reason, status.stop_reason);
	row.field(Column::capture_incomplete, status.incomplete);
}
void write_settings(CsvRow& row, const PerformanceCaptureSettings& settings) {
	row.field(Column::capture_settings_duration_ns, settings.duration_ns);
	row.field(Column::capture_settings_budget_ns, settings.budget_ns);
	row.field(Column::capture_settings_post_event_ns, settings.post_event_ns);
	row.field(Column::capture_settings_budget_window, settings.budget_window);
	row.field(Column::capture_settings_start_mode, settings.start_mode);
	row.field(Column::capture_settings_end_mode, settings.end_mode);
	row.field(Column::capture_settings_shortcut_key, settings.start_chord.key);
	row.field(Column::capture_settings_shortcut_ctrl, settings.start_chord.ctrl);
	row.field(Column::capture_settings_shortcut_shift, settings.start_chord.shift);
	row.field(Column::capture_settings_shortcut_alt, settings.start_chord.alt);
	row.field(Column::capture_settings_shortcut_super, settings.start_chord.super);
}

void sample_descriptor(CsvRow& row, const TimingCaptureSnapshot& snapshot, uint64_t type_id) {
	const auto descriptor =
		std::ranges::find(snapshot.descriptors, type_id, &TimingZoneDescriptor::typeId);
	if (descriptor != snapshot.descriptors.end())
		write_descriptor(row, *descriptor);
}
[[nodiscard]] FlowUiError export_error() noexcept {
	return makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile);
}
} // namespace
Status write_performance_capture_csv(std::ostream& output, const TimingCaptureSnapshot& snapshot,
									 const TimingReportingStatus& reporting_status,
									 const PerformanceCaptureStatus& capture_status) noexcept {
	try {
		for (size_t index = 0; index < columns.size(); ++index) {
			if (index)
				output.put(',');
			quoted(output, columns[index]);
		}
		output << "\r\n";
		CsvRow row;
		const auto generation = reporting_status.capture_generation;
		row.begin("capture", generation);
		write_reporting(row, reporting_status);
		write_capture(row, capture_status);
		write_settings(row, capture_status.settings);
		write_cpu_quality(row, reporting_status.quality);
		row.field(Column::mutation_sequence, snapshot.mutation_sequence);
		row.emit(output);
		for (size_t descriptor_index = 0; descriptor_index < snapshot.descriptors.size();
			 ++descriptor_index) {
			row.begin("zone_descriptor", generation, 0, 0, 0, descriptor_index);
			write_descriptor(row, snapshot.descriptors[descriptor_index]);
			row.emit(output);
		}
		for (const auto& tick : snapshot.reports) {
			row.begin("app_tick", generation, tick.appTick);
			row.field(Column::boundary_start_ns, tick.boundary_start_ns);
			row.field(Column::boundary_end_ns, tick.boundary_end_ns);
			row.field(Column::boundary_open, tick.boundary_open);
			row.field(Column::tick_revision, tick.revision);
			row.field(Column::occupied, tick.occupied);
			write_config(row, tick.captureConfig);
			write_cpu_quality(row, tick.cpuQuality);
			write_gpu_quality(row, tick.gpuQuality);
			row.emit(output);
			for (size_t sample_index = 0; sample_index < tick.applicationCpuZones.size();
				 ++sample_index) {
				const auto& sample = tick.applicationCpuZones[sample_index];
				row.begin("cpu_sample", generation, tick.appTick, 0, 0, sample_index);
				row.field(Column::scope, std::string_view{"application"});
				write_cpu(row, sample);
				sample_descriptor(row, snapshot, sample.typeId);
				row.emit(output);
			}
			for (const auto& window : tick.windows) {
				if (!window.occupied)
					continue;
				row.begin("window", generation, tick.appTick, window.window);
				row.field(Column::occupied, window.occupied);
				row.emit(output);
				for (const auto& frame : window.frames) {
					if (!frame.occupied)
						continue;
					row.begin("frame", generation, tick.appTick, frame.key.window,
							  frame.key.frameNumber);
					row.field(Column::occupied, frame.occupied);
					row.emit(output);
					for (size_t sample_index = 0; sample_index < frame.cpuZones.size();
						 ++sample_index) {
						const auto& sample = frame.cpuZones[sample_index];
						row.begin("cpu_sample", generation, tick.appTick, frame.key.window,
								  frame.key.frameNumber, sample_index);
						row.field(Column::scope, std::string_view{"frame"});
						write_cpu(row, sample);
						sample_descriptor(row, snapshot, sample.typeId);
						row.emit(output);
					}
					for (size_t sample_index = 0; sample_index < frame.gpuZones.size();
						 ++sample_index) {
						const auto& sample = frame.gpuZones[sample_index];
						row.begin("gpu_sample", generation, tick.appTick, frame.key.window,
								  frame.key.frameNumber, sample_index);
						write_gpu(row, sample);
						sample_descriptor(row, snapshot, sample.typeId);
						row.emit(output);
					}
					for (size_t aggregate_index = 0;
						 aggregate_index < frame.elementDefinitions.size(); ++aggregate_index) {
						const auto& aggregate = frame.elementDefinitions[aggregate_index];
						row.begin("element_aggregate", generation, tick.appTick, frame.key.window,
								  frame.key.frameNumber, aggregate_index);
						write_aggregate(row, aggregate);
						row.emit(output);
					}
				}
			}
		}
		if (!output)
			return unexpectedError(export_error());
		return {};
	} catch (...) {
		return unexpectedError(export_error());
	}
}
Result<std::filesystem::path>
export_performance_capture_csv(DevTimingReporting& reporting, uint64_t expected_generation,
							   const PerformanceCaptureStatus& capture_status,
							   const std::filesystem::path& directory) noexcept {
	std::filesystem::path temporary;
	try {
		const auto reporting_status = reporting.status();
		if (!expected_generation || !reporting_status.capture_sealed ||
			reporting_status.capture_generation != expected_generation ||
			!reporting_status.hasRetainedTicks)
			return unexpectedError(export_error());
		TimingCaptureSnapshot snapshot;
		{
			auto view = reporting.read_capture();
			if (view.generation != expected_generation)
				return unexpectedError(export_error());
			snapshot.mutation_sequence = reporting_status.mutationSequence;
			snapshot.descriptors.assign(view.descriptors.begin(), view.descriptors.end());
			snapshot.reports.reserve(view.first.size() + view.second.size());
			for (const auto segment : {view.first, view.second})
				for (const auto& report : segment)
					snapshot.reports.emplace_back(report);
		}
		const auto export_directory =
			directory.empty() ? ::FlowUi::detail::executable_directory() : directory;
		if (export_directory.empty())
			return unexpectedError(export_error());
		const auto now = std::chrono::system_clock::now();
		const auto day = std::chrono::floor<std::chrono::days>(now);
		const std::chrono::year_month_day date{day};
		const std::chrono::hh_mm_ss time{
			std::chrono::duration_cast<std::chrono::microseconds>(now - day)};
		std::ostringstream filename;
		filename.imbue(std::locale::classic());
		filename << "performance-capture-" << expected_generation << '-' << int(date.year())
				 << std::setfill('0') << std::setw(2) << unsigned(date.month()) << std::setw(2)
				 << unsigned(date.day()) << '-' << std::setw(2) << time.hours().count()
				 << std::setw(2) << time.minutes().count() << std::setw(2) << time.seconds().count()
				 << '-' << std::setw(6) << time.subseconds().count() << "-UTC.csv";
		const auto destination = export_directory / filename.str();
		temporary = destination;
		temporary += ".tmp";
		if (std::filesystem::exists(destination) || std::filesystem::exists(temporary))
			return unexpectedError(export_error());
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if (!output)
			return unexpectedError(export_error());
		const auto written =
			write_performance_capture_csv(output, snapshot, reporting_status, capture_status);
		output.close();
		if (!written || !output) {
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return unexpectedError(export_error());
		}
		std::filesystem::rename(temporary, destination);
		return destination;
	} catch (...) {
		std::error_code ignored;
		if (!temporary.empty())
			std::filesystem::remove(temporary, ignored);
		return unexpectedError(export_error());
	}
}
} // namespace FlowUi::devSystems
#endif
