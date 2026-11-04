#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

#include <array>
#include <charconv>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

namespace FlowUi::devSystems::agentic {
FlowUiError agentic_error(AgenticError error) noexcept {
	switch (error) {
	case AgenticError::invalid_request:
		return makeError(ErrorCode::AgenticInvalidRequest, ErrorSite::AgenticCommand);
	case AgenticError::unavailable:
		return makeError(ErrorCode::AgenticUnavailable, ErrorSite::AgenticCommand);
	case AgenticError::busy:
		return makeError(ErrorCode::AgenticBusy, ErrorSite::AgenticCapture);
	case AgenticError::stale_generation:
		return makeError(ErrorCode::AgenticStaleGeneration, ErrorSite::AgenticSnapshot);
	case AgenticError::invalid_scope:
		return makeError(ErrorCode::AgenticInvalidScope, ErrorSite::AgenticCommand);
	case AgenticError::budget_exhausted:
		return makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot);
	case AgenticError::incomplete:
		return makeError(ErrorCode::AgenticIncompleteCapture, ErrorSite::AgenticCapture);
	case AgenticError::publication_failed:
		return makeError(ErrorCode::AgenticPublicationFailed, ErrorSite::AgenticPublish);
	case AgenticError::incompatible:
		return makeError(ErrorCode::AgenticIncompatibleComparison, ErrorSite::AgenticCompare);
	case AgenticError::canceled:
		return makeError(ErrorCode::AgenticCanceled, ErrorSite::AgenticCapture);
	case AgenticError::timeout:
		return makeError(ErrorCode::AgenticTimeout, ErrorSite::AgenticCapture);
	case AgenticError::integrity_failed:
		return makeError(ErrorCode::AgenticIntegrityFailed, ErrorSite::AgenticPublish);
	}
	return makeError(ErrorCode::InternalInvariantBroken, ErrorSite::AgenticCommand);
}
bool valid_identifier(std::string_view identifier) noexcept {
	if (identifier.empty() || identifier.size() > 128 || identifier == "." || identifier == "..")
		return false;
	for (const auto character : identifier)
		if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			  (character >= '0' && character <= '9') || character == '_' || character == '-' ||
			  character == '.'))
			return false;
	return true;
}
Result<std::string> make_identifier(std::string_view prefix) noexcept {
	try {
		if (!valid_identifier(prefix) || prefix.size() > 64)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		std::random_device random;
		constexpr char hex[] = "0123456789abcdef";
		std::string output(prefix);
		output.push_back('_');
		output.reserve(prefix.size() + 33);
		for (unsigned word_index = 0; word_index < 4; ++word_index) {
			const auto word = static_cast<std::uint32_t>(random());
			for (unsigned digit_index = 0; digit_index < 8; ++digit_index)
				output.push_back(hex[(word >> (28u - digit_index * 4u)) & 0xfu]);
		}
		return output;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticConfigure));
	}
}
std::uint64_t monotonic_nanoseconds() noexcept {
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
										  std::chrono::steady_clock::now().time_since_epoch())
										  .count());
}
std::string utc_timestamp() {
	const auto now = std::chrono::system_clock::now();
	const auto seconds = std::chrono::system_clock::to_time_t(now);
	std::tm utc{};
#ifdef _WIN32
	gmtime_s(&utc, &seconds);
#else
	gmtime_r(&seconds, &utc);
#endif
	const auto micros =
		std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() %
		1000000;
	std::ostringstream output;
	output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(6)
		   << micros << 'Z';
	return output.str();
}
std::string_view name(ExecutionMode value) noexcept {
	constexpr std::array values{"offline", "cpu_scenario", "layout_scenario", "offscreen_vulkan",
								"windowed"};
	return values[static_cast<unsigned>(value) < values.size() ? static_cast<unsigned>(value) : 0];
}
std::string_view name(CaptureProfile value) noexcept {
	constexpr std::array values{"performance_diagnose",	 "performance_verify",
								"diagnostics_reproduce", "visual_inspect",
								"memory_diagnose",		 "startup_diagnose"};
	return values[static_cast<unsigned>(value) < values.size() ? static_cast<unsigned>(value) : 0];
}
std::string_view name(SectionStatus value) noexcept {
	constexpr std::array values{"complete", "partial",	   "not_requested",
								"disabled", "unsupported", "failed"};
	return values[static_cast<unsigned>(value) < values.size() ? static_cast<unsigned>(value) : 2];
}
std::string_view name(SectionRequirement value) noexcept {
	constexpr std::array values{"omit", "optional", "required"};
	return values[static_cast<unsigned>(value) < values.size() ? static_cast<unsigned>(value) : 0];
}
std::string_view name(JobPhase value) noexcept {
	constexpr std::array values{"queued",	  "preparing",	   "warming_up", "recording",
								"finalizing", "materializing", "publishing", "sealed",
								"canceling",  "canceled",	   "failed"};
	return values[static_cast<unsigned>(value) < values.size() ? static_cast<unsigned>(value) : 11];
}
Json to_json(const SectionCoverage& coverage) {
	auto ranges = [](const auto& values) {
		Json result = Json::make_array();
		result.array.reserve(values.size());
		for (const auto& range : values)
			result.array.emplace_back(Json::make_object(
				{{"first_app_tick", decimal(range.first_app_tick)},
				 {"end_app_tick_exclusive", decimal(range.end_app_tick_exclusive)}}));
		return result;
	};
	Json reasons = Json::make_array();
	for (const auto& reason : coverage.reasons)
		reasons.array.emplace_back(reason);
	return Json::make_object({{"status", name(coverage.status)},
							  {"covered_ranges", ranges(coverage.covered_ranges)},
							  {"gaps", ranges(coverage.gaps)},
							  {"reasons", std::move(reasons)},
							  {"producer_records", decimal(coverage.producer_records)},
							  {"retained_records", decimal(coverage.retained_records)},
							  {"dropped_records", decimal(coverage.dropped_records)},
							  {"evicted_records", decimal(coverage.evicted_records)},
							  {"pending_records", decimal(coverage.pending_records)},
							  {"generation", decimal(coverage.generation)},
							  {"finalization_watermark", decimal(coverage.finalization_watermark)},
							  {"uncertain", coverage.uncertain}});
}
Json to_json(const AgenticLimits& limits) {
	return Json::make_object({{"maximum_snapshot_bytes", decimal(limits.maximum_snapshot_bytes)},
							  {"maximum_artifact_bytes", decimal(limits.maximum_artifact_bytes)},
							  {"maximum_disk_bytes", decimal(limits.maximum_disk_bytes)},
							  {"maximum_ticks", decimal(limits.maximum_ticks)},
							  {"maximum_records", decimal(limits.maximum_records)},
							  {"maximum_images", decimal(limits.maximum_images)},
							  {"maximum_image_bytes", decimal(limits.maximum_image_bytes)},
							  {"finalization_timeout_ns", decimal(limits.finalization_timeout_ns)},
							  {"maximum_queued_jobs", Json::number(limits.maximum_queued_jobs)}});
}
Json to_json(const AgenticCapabilities& capabilities) {
	Json modes = Json::make_array();
	for (const auto mode : capabilities.modes)
		modes.array.emplace_back(name(mode));
	Json profiles = Json::make_array();
	for (const auto profile : capabilities.profiles)
		profiles.array.emplace_back(name(profile));
	Json limitations = Json::make_array();
	for (const auto& limitation : capabilities.limitations)
		limitations.array.emplace_back(limitation);
	return Json::make_object({{"schema", "FlowUi.Agentic.Capabilities"},
							  {"schema_version", agentic_schema_version},
							  {"run_id", capabilities.run_id},
							  {"application_id", capabilities.application_id},
							  {"enabled", capabilities.enabled},
							  {"modes", std::move(modes)},
							  {"profiles", std::move(profiles)},
							  {"runtime_diagnostics", capabilities.runtime_diagnostics},
							  {"memory", capabilities.memory},
							  {"visual", capabilities.visual},
							  {"trees", capabilities.trees},
							  {"themes", capabilities.themes},
							  {"local_attachment", capabilities.local_attachment},
							  {"timing_level", Json::number(capabilities.timing_level)},
							  {"memory_level", Json::number(capabilities.memory_level)},
							  {"error_level", Json::number(capabilities.error_level)},
							  {"details", capabilities.details},
							  {"limits", to_json(capabilities.limits)},
							  {"limitations", std::move(limitations)}});
}
Json to_json(const AgenticJobStatus& status) {
	return Json::make_object({{"job_id", status.job_id},
							  {"capture_id", status.capture_id},
							  {"manifest_path", status.manifest_path.generic_string()},
							  {"phase", name(status.phase)},
							  {"reason", status.reason},
							  {"measured_ticks", decimal(status.measured_ticks)},
							  {"required_ticks", decimal(status.required_ticks)},
							  {"measured_intervals", decimal(status.measured_intervals)},
							  {"required_intervals", decimal(status.required_intervals)},
							  {"cancellation_requested", status.cancellation_requested}});
}
} // namespace FlowUi::devSystems::agentic
