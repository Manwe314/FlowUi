#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceExport.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
namespace {
[[nodiscard]] std::vector<std::vector<std::string>> parse_csv(std::string_view csv) {
	std::vector<std::vector<std::string>> rows;
	std::vector<std::string> fields;
	std::string field;
	bool quoted = false;
	for (size_t position = 0; position < csv.size(); ++position) {
		const char character = csv[position];
		if (character == '"') {
			if (quoted && position + 1 < csv.size() && csv[position + 1] == '"') {
				field += '"';
				++position;
			} else
				quoted = !quoted;
		} else if (!quoted && (character == ',' || character == '\r')) {
			fields.emplace_back(std::move(field));
			field.clear();
			if (character == '\r') {
				assert(position + 1 < csv.size() && csv[++position] == '\n');
				rows.emplace_back(std::move(fields));
				fields.clear();
			}
		} else
			field += character;
	}
	assert(!quoted && field.empty() && fields.empty());
	return rows;
}
} // namespace
int main() {
	TimingCaptureSnapshot snapshot;
	snapshot.mutation_sequence = 123;
	snapshot.descriptors.emplace_back(
		TimingZoneDescriptor{.typeId = 42,
							 .name = "Zone, \"quoted\"\nsecond line",
							 .category = TimingCategory::User,
							 .source = {.file = "source,with,commas.cpp",
										.function = "function\"name",
										.line = 71,
										.column = 8}});
	snapshot.reports.resize(1);
	auto& tick = snapshot.reports.front();
	tick.appTick = 9;
	tick.revision = 7;
	tick.boundary_start_ns = 100;
	tick.boundary_end_ns = 900;
	tick.occupied = true;
	tick.cpuQuality.droppedRecords = 3;
	tick.gpuQuality.timestampPeriodNs = .42;
	tick.captureConfig.balancedElementRetentionThresholdNs = 51;
	tick.applicationCpuZones.emplace_back(
		CpuTimingRecord{.startNs = 101,
						.durationNs = 20,
						.directChildNs = 5,
						.invocationId = UINT64_MAX,
						.parentInvocationId = 18,
						.typeId = 42,
						.appTick = 9,
						.primaryEntityId = 555,
						.secondaryEntityId = 666,
						.track = 11,
						.depth = 2,
						.flags = timingRecordFlags(TimingRecordFlag::DetailTruncated)});
	tick.windows.resize(1);
	auto& window = tick.windows.front();
	window.window = 4;
	window.occupied = true;
	window.frames.resize(2);
	auto& frame = window.frames.front();
	frame.key = {4, 6};
	frame.occupied = true;
	frame.cpuZones.emplace_back(tick.applicationCpuZones.front());
	frame.cpuZones.back().frame = frame.key;
	frame.gpuZones.emplace_back(GpuTimingRecord{.startTick = UINT64_MAX - 1,
												.durationTicks = 55,
												.durationNs = 23,
												.timestamp_period_ns = .42,
												.timestamp_valid_bits = 48,
												.cpuAlignedStartNs = 101,
												.calibrationMaximumDeviationNs = 3,
												.submissionSerial = 17,
												.device_identity = 88,
												.queue_identity = 99,
												.typeId = 42,
												.frame = frame.key,
												.appTick = 9,
												.primaryEntityId = 12,
												.secondaryEntityId = 13,
												.calibrationId = 19,
												.zone_index = 3,
												.parentZoneIndex = 1,
												.queueFamilyIndex = 2,
												.depth = 2});
	frame.elementDefinitions.emplace_back(
		ElementDefinitionTimingAggregate{.definition = DefinitionID("export.test"),
										 .frame = frame.key,
										 .appTick = 9,
										 .invocationCount = 8,
										 .totalInclusiveNs = 40,
										 .maximumInclusiveNs = 10,
										 .canceledInvocationCount = 2});
	window.frames.back().key = {4, 7};
	window.frames.back().occupied = true;
	TimingReportingStatus reporting_status;
	reporting_status.capture_generation = 2;
	reporting_status.capture_sealed = true;
	PerformanceCaptureStatus capture_status;
	capture_status.generation = 2;
	capture_status.phase = PerformanceCapturePhase::Sealed;
	capture_status.event_duration_ns = 77;
	std::ostringstream output;
	assert(write_performance_capture_csv(output, snapshot, reporting_status, capture_status));
	const auto rows = parse_csv(output.str());
	assert(rows.size() == 11);
	for (const auto& row : rows)
		assert(row.size() == rows.front().size());
	const auto value = [&](size_t row, std::string_view column) -> const std::string& {
		const auto found = std::ranges::find(rows.front(), column);
		assert(found != rows.front().end());
		return rows[row][size_t(found - rows.front().begin())];
	};
	assert(value(1, "schema_version") == "1" && value(1, "capture_generation") == "2");
	assert(value(1, "mutation_sequence") == "123" && value(1, "capture_event_duration_ns") == "77");
	assert(value(2, "zone_name") == snapshot.descriptors.front().name);
	assert(value(2, "source_file") == "source,with,commas.cpp");
	assert(value(2, "source_function") == "function\"name" && value(2, "source_column") == "8");
	assert(value(3, "cpu_quality_dropped_records") == "3");
	assert(value(3, "config_balanced_element_retention_threshold_ns") == "51");
	assert(value(4, "invocation_id") == "18446744073709551615");
	assert(value(4, "parent_invocation_id") == "18" && value(4, "exclusive_ns") == "15");
	assert(value(4, "scope") == "application" && value(7, "scope") == "frame");
	assert(value(8, "start_tick") == "18446744073709551614");
	assert(value(8, "device_identity") == "88" && value(8, "queue_identity") == "99");
	assert(value(8, "parent_zone_index") == "1" && value(8, "calibration_id") == "19");
	assert(std::stod(value(8, "timestamp_period_ns")) == .42);
	assert(value(9, "aggregate_total_inclusive_ns") == "40");
	assert(value(10, "record_kind") == "frame" && value(10, "frame_number") == "7");
	std::ostringstream failed_output;
	failed_output.setstate(std::ios::badbit);
	assert(
		!write_performance_capture_csv(failed_output, snapshot, reporting_status, capture_status));

	DevTiming timing;
	DevGpuTiming gpu(timing);
	DevTimingReporting reporting(timing, gpu);
	auto reporting_config = reporting.config();
	reporting_config.retainedAppTickCapacity = 2;
	reporting_config.minimumFramesInFlightMultiplier = 1;
	reporting.setConfig(reporting_config);
	assert(!export_performance_capture_csv(reporting, 0, capture_status));
	assert(reporting.begin_capture(10) == 1);
	reporting.admit_tick(10, 100);
	reporting.note_tick_boundary(11, 200);
	assert(!export_performance_capture_csv(reporting, 1, capture_status));
	reporting.admit_tick(11, 200);
	reporting.note_tick_boundary(12, 300);
	reporting.admit_tick(12, 300);
	reporting.note_tick_boundary(13, 400);
	reporting.stop_capture(13);
	reporting.seal_capture();
	const auto directory =
		std::filesystem::temp_directory_path() / "flowui-performance-export-test";
	std::filesystem::create_directories(directory);
	assert(!export_performance_capture_csv(reporting, 2, capture_status, directory));
	assert(!export_performance_capture_csv(reporting, 1, capture_status, directory / "missing"));
	capture_status.generation = 1;
	const auto saved = export_performance_capture_csv(reporting, 1, capture_status, directory);
	assert(saved && saved->parent_path() == directory && saved->extension() == ".csv");
	assert(saved->filename().string().starts_with("performance-capture-1-"));
	std::ifstream file(*saved, std::ios::binary);
	std::ostringstream contents;
	contents << file.rdbuf();
	const auto retained_rows = parse_csv(contents.str());
	const auto tick_column = std::ranges::find(retained_rows.front(), "app_tick");
	const auto kind_column = std::ranges::find(retained_rows.front(), "record_kind");
	assert(tick_column != retained_rows.front().end() &&
		   kind_column != retained_rows.front().end());
	std::vector<std::string> retained_ticks;
	for (size_t row_index = 1; row_index < retained_rows.size(); ++row_index)
		if (retained_rows[row_index][size_t(kind_column - retained_rows.front().begin())] ==
			"app_tick")
			retained_ticks.emplace_back(
				retained_rows[row_index][size_t(tick_column - retained_rows.front().begin())]);
	assert((retained_ticks == std::vector<std::string>{"11", "12"}));
	assert(!std::filesystem::exists(saved->string() + ".tmp"));
	std::filesystem::remove(*saved);
	std::filesystem::remove(directory);
}
