#include "devSystems/devAgenticWorkflow/AgenticAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <regex>
#include <set>

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] const Json* path(const Json& value,
							   std::initializer_list<std::string_view> names) noexcept {
	const Json* current = &value;
	for (const auto key : names) {
		current = current->find(key);
		if (!current)
			return nullptr;
	}
	return current;
}
[[nodiscard]] std::uint64_t unsigned_field(const Json& value, std::string_view key) noexcept {
	const auto* found = value.find(key);
	return found ? found->uint64_or() : 0;
}
[[nodiscard]] bool boolean_field(const Json& value, std::string_view key) noexcept {
	const auto* found = value.find(key);
	return found && found->bool_or();
}
[[nodiscard]] std::string text_field(const Json& value, std::string_view key) {
	const auto* found = value.find(key);
	return found ? std::string(found->string_or()) : std::string{};
}
[[nodiscard]] Json distribution(const std::vector<std::uint64_t>& values) {
	if (values.empty())
		return Json::make_object({{"count", decimal(0)}});
	auto sorted = values;
	std::sort(sorted.begin(), sorted.end());
	const auto rank =
		static_cast<std::size_t>(std::ceil(static_cast<double>(sorted.size()) * 0.95)) - 1u;
	long double total = 0;
	for (const auto value : sorted)
		total += value;
	return Json::make_object({{"count", decimal(sorted.size())},
							  {"minimum_ns", decimal(sorted.front())},
							  {"maximum_ns", decimal(sorted.back())},
							  {"mean_ns", Json(static_cast<double>(total / sorted.size()))},
							  {"p95_ns", decimal(sorted[rank])},
							  {"p95_definition", "nearest_rank_ceil_0.95N"}});
}
[[nodiscard]] bool satisfied(double value, std::string_view comparison, double threshold) noexcept {
	if (comparison == "gt")
		return value > threshold;
	if (comparison == "ge")
		return value >= threshold;
	if (comparison == "lt")
		return value < threshold;
	if (comparison == "le")
		return value <= threshold;
	return false;
}
[[nodiscard]] bool unknown_identity(const Json* value) noexcept {
	return !value || value->kind == Json::Kind::null ||
		   (value->kind == Json::Kind::string &&
			(value->text.empty() || value->text == "unknown")) ||
		   (value->kind == Json::Kind::object && value->object.empty()) ||
		   (value->kind == Json::Kind::array && value->array.empty());
}
[[nodiscard]] bool same_field(const Json& first, const Json& second,
							  std::initializer_list<std::string_view> selectors, bool& unknown) {
	const auto* left = path(first, selectors);
	const auto* right = path(second, selectors);
	if (unknown_identity(left) || unknown_identity(right)) {
		unknown = true;
		return true;
	}
	return write_json(*left) == write_json(*right);
}
[[nodiscard]] bool complete_required_coverage(const AgenticManifest& manifest,
											  std::vector<std::string>& reasons) {
	bool valid = !manifest.partial;
	if (manifest.partial)
		reasons.emplace_back("capture_partial");
	const auto* coverage = manifest.document.find("coverage");
	const auto* timing = coverage ? coverage->find("performance") : nullptr;
	if (!timing || timing->string_or() != "complete") {
		reasons.emplace_back("performance_coverage_incomplete");
		valid = false;
	}
	auto quality = read_artifact_json(manifest, "capture_quality");
	if (!quality) {
		reasons.emplace_back("quality_unavailable");
		return false;
	}
	const auto* sections = quality->find("sections");
	if (sections)
		for (const auto& section : sections->object) {
			if (section.first != "performance")
				continue;
			if (boolean_field(section.second, "uncertain") ||
				unsigned_field(section.second, "dropped_records") ||
				unsigned_field(section.second, "evicted_records") ||
				unsigned_field(section.second, "pending_records")) {
				valid = false;
				reasons.emplace_back("performance_loss_or_uncertain_coverage");
			}
			if (const auto* gaps = section.second.find("gaps"); gaps && !gaps->array.empty()) {
				valid = false;
				reasons.emplace_back("performance_coverage_gaps");
			}
		}
	return valid;
}
} // namespace
Result<Json> analyze_performance(std::span<const AgenticFrameRecord> frames,
								 std::optional<std::uint64_t> window_id) noexcept {
	try {
		std::map<std::pair<std::string, std::uint64_t>, std::vector<const AgenticFrameRecord*>>
			by_window;
		for (const auto& frame : frames)
			if ((!window_id || frame.key.window_id == *window_id) && !frame.internal_window)
				by_window[{frame.key.run_id, frame.key.window_id}].emplace_back(&frame);
		Json windows = Json::make_array();
		windows.array.reserve(by_window.size());
		for (auto& entry : by_window) {
			auto& selected = entry.second;
			std::sort(selected.begin(), selected.end(), [](const auto* first, const auto* second) {
				if (first->begin_ns != second->begin_ns)
					return first->begin_ns < second->begin_ns;
				return first->key.window_frame < second->key.window_frame;
			});
			std::vector<std::uint64_t> intervals;
			std::vector<std::uint64_t> cpu;
			std::vector<std::uint64_t> gpu;
			intervals.reserve(selected.size());
			cpu.reserve(selected.size());
			gpu.reserve(selected.size());
			Json segments = Json::make_array();
			std::uint64_t elapsed = 0;
			std::uint64_t excluded = 0;
			std::uint64_t discontinuities = 0;
			std::uint64_t observed = 0;
			std::uint64_t segment_intervals = 0;
			std::uint64_t segment_begin = 0;
			std::uint64_t segment_end = 0;
			const AgenticFrameRecord* previous = nullptr;
			auto finish_segment = [&]() {
				if (segment_intervals)
					segments.array.emplace_back(
						Json::make_object({{"begin_ns", decimal(segment_begin)},
										   {"end_ns", decimal(segment_end)},
										   {"interval_count", decimal(segment_intervals)},
										   {"elapsed_ns", decimal(segment_end - segment_begin)}}));
				segment_intervals = 0;
				previous = nullptr;
			};
			for (const auto* frame : selected) {
				if (frame->presented)
					++observed;
				if (!frame->presented || !frame->submitted || frame->canceled ||
					frame->interrupted || any(frame->observers) || frame->present_ns == 0) {
					++excluded;
					finish_segment();
					continue;
				}
				cpu.emplace_back(frame->cpu_duration_ns);
				if (frame->gpu_duration_available)
					gpu.emplace_back(frame->gpu_duration_ns);
				if (previous && (frame->key.window_frame != previous->key.window_frame + 1 ||
								 frame->present_ns <= previous->present_ns ||
								 frame->key.app_tick < previous->key.app_tick)) {
					++discontinuities;
					finish_segment();
				}
				if (previous) {
					const auto interval = frame->present_ns - previous->present_ns;
					if (interval > std::numeric_limits<std::uint64_t>::max() - elapsed)
						return unexpectedError(agentic_error(AgenticError::budget_exhausted));
					intervals.emplace_back(interval);
					elapsed += interval;
					++segment_intervals;
					segment_end = frame->present_ns;
				} else {
					segment_begin = frame->present_ns;
					segment_end = frame->present_ns;
				}
				previous = frame;
			}
			finish_segment();
			const auto interval_distribution = distribution(intervals);
			const auto* percentile = interval_distribution.find("p95_ns");
			const double fps =
				elapsed ? static_cast<double>(static_cast<long double>(intervals.size()) *
											  1000000000.0L / elapsed)
						: 0.0;
			windows.array.emplace_back(Json::make_object(
				{{"run_id", entry.first.first},
				 {"window_id", decimal(entry.first.second)},
				 {"observed_fps", Json(fps)},
				 {"valid_intervals", decimal(intervals.size())},
				 {"elapsed_ns", decimal(elapsed)},
				 {"p95_interval_ns", percentile ? *percentile : Json{}},
				 {"observed_present_calls", decimal(observed)},
				 {"observed_frames", decimal(selected.size())},
				 {"excluded_frames", decimal(excluded)},
				 {"discontinuities", decimal(discontinuities)},
				 {"segments", std::move(segments)},
				 {"interval_distribution", interval_distribution},
				 {"cpu_frame_duration", distribution(cpu)},
				 {"gpu_duration", distribution(gpu)},
				 {"formula", "sum_segment_interval_count * 1e9 / sum_segment_elapsed_ns"}}));
		}
		return Json::make_object(
			{{"schema", "FlowUi.Agentic.Performance"},
			 {"schema_version", "1.0"},
			 {"algorithm_version", "cadence_segments_1"},
			 {"windows", std::move(windows)},
			 {"limitations",
			  Json::make_array({"present_call_observation_is_not_physical_display_time",
								"adjacent_intervals_are_not_independent_experiments"})}});
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCompare));
	}
}
Result<std::vector<AgenticFrameRecord>> read_frames(const AgenticManifest& manifest) noexcept {
	try {
		auto bytes = read_artifact(manifest, "frame_relations", 256u * 1024u * 1024u);
		if (!bytes)
			return unexpectedError(bytes.error());
		const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
		std::vector<AgenticFrameRecord> output;
		output.reserve(std::min<std::size_t>(text.size() / 300u, 1000000));
		std::size_t begin = 0;
		while (begin < text.size()) {
			auto end = text.find('\n', begin);
			if (end == std::string_view::npos)
				end = text.size();
			const auto line = text.substr(begin, end - begin);
			begin = end + 1;
			if (line.empty())
				continue;
			auto record = parse_json(line);
			if (!record)
				return unexpectedError(record.error());
			auto valid = validate_agentic_document(*record, "FlowUi.Agentic.Frame");
			if (!valid)
				return unexpectedError(valid.error());
			AgenticFrameRecord frame;
			frame.key.run_id = text_field(*record, "run_id");
			if (frame.key.run_id != manifest.run_id)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			frame.key.app_tick = unsigned_field(*record, "app_tick");
			frame.key.window_id = unsigned_field(*record, "window_id");
			frame.key.window_frame = unsigned_field(*record, "window_frame");
			frame.begin_ns = unsigned_field(*record, "begin_ns");
			frame.end_ns = unsigned_field(*record, "end_ns");
			frame.submit_ns = unsigned_field(*record, "submit_ns");
			frame.present_ns = unsigned_field(*record, "present_ns");
			frame.cpu_duration_ns = unsigned_field(*record, "cpu_duration_ns");
			frame.gpu_duration_ns = unsigned_field(*record, "gpu_duration_ns");
			frame.gpu_duration_available = record->find("gpu_duration_ns") != nullptr;
			frame.tree_generation = unsigned_field(*record, "tree_generation");
			frame.frame_slot = static_cast<std::uint32_t>(unsigned_field(*record, "frame_slot"));
			frame.width = static_cast<std::uint32_t>(unsigned_field(*record, "width"));
			frame.height = static_cast<std::uint32_t>(unsigned_field(*record, "height"));
			frame.observers =
				static_cast<ObserverActivity>(unsigned_field(*record, "observer_activity"));
			frame.submitted = boolean_field(*record, "submitted");
			frame.presented = boolean_field(*record, "presented");
			frame.canceled = boolean_field(*record, "canceled");
			frame.internal_window = boolean_field(*record, "internal_window");
			frame.interrupted = boolean_field(*record, "interrupted");
			if (const auto* submission = record->find("submission")) {
				frame.device_id = unsigned_field(*submission, "device_id");
				frame.queue_id = unsigned_field(*submission, "queue_id");
				frame.submission_serial = unsigned_field(*submission, "serial");
			}
			output.emplace_back(std::move(frame));
			if (output.size() > 1000000)
				return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		}
		return output;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
Result<Json> check_objective(std::span<const AgenticManifest> passes,
							 const AgenticObjective& objective) noexcept {
	try {
		if (objective.metric != "window_present_call_cadence_fps" ||
			!std::isfinite(objective.threshold) || objective.threshold <= 0 ||
			(objective.comparison != "gt" && objective.comparison != "ge" &&
			 objective.comparison != "lt" && objective.comparison != "le") ||
			objective.minimum_passes == 0)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		Json results = Json::make_array();
		Json all_reasons = Json::make_array();
		bool inconclusive = passes.size() < objective.minimum_passes;
		bool passed = true;
		if (inconclusive)
			all_reasons.array.emplace_back("insufficient_independent_passes");
		std::set<std::string> identifiers;
		std::vector<double> pass_fps;
		pass_fps.reserve(passes.size());
		for (const auto& manifest : passes) {
			std::vector<std::string> reasons;
			bool valid = complete_required_coverage(manifest, reasons);
			const auto* mode = path(manifest.document, {"measurement", "mode"});
			const auto* profile = path(manifest.document, {"measurement", "profile"});
			const auto* windows = path(manifest.document, {"measurement", "window_ids"});
			if (!mode || mode->string_or() != "windowed") {
				reasons.emplace_back("requires_native_windowed_measurement");
				valid = false;
			}
			if (!profile || profile->string_or() != "performance_verify") {
				reasons.emplace_back("requires_performance_verify_profile");
				valid = false;
			}
			if (!identifiers.insert(manifest.capture_id).second) {
				reasons.emplace_back("duplicate_pass_identity");
				valid = false;
			}
			if (!windows || windows->kind != Json::Kind::array || windows->array.size() != 1) {
				reasons.emplace_back("requires_one_selected_window");
				valid = false;
			}
			for (const auto key : {"build_profile_digest", "hardware_digest", "settings_digest",
								   "instrumentation_digest", "resource_digest",
								   "measurement_instrumentation", "capability_flags"}) {
				const auto* identity = path(manifest.document, {"provenance", key});
				if (unknown_identity(identity)) {
					valid = false;
					reasons.emplace_back(std::string("provenance_unknown:") + key);
				}
			}
			const auto* scenario = path(manifest.document, {"measurement", "scenario_id"});
			const auto* fixture = path(manifest.document, {"measurement", "scenario_digest"});
			if (!scenario || scenario->string_or().empty() || !fixture ||
				fixture->string_or().empty()) {
				valid = false;
				reasons.emplace_back("reproducible_scenario_identity_unavailable");
			}
			Json metric;
			auto frames = read_frames(manifest);
			if (!frames) {
				reasons.emplace_back("frame_evidence_unavailable");
				valid = false;
			} else {
				auto analysis = analyze_performance(
					*frames, windows && windows->array.size() == 1
								 ? std::optional<std::uint64_t>(windows->array.front().uint64_or())
								 : std::nullopt);
				if (!analysis || !analysis->find("windows") ||
					analysis->find("windows")->array.size() != 1) {
					reasons.emplace_back("selected_window_evidence_unavailable");
					valid = false;
				} else {
					metric = analysis->find("windows")->array.front();
					if (unsigned_field(metric, "valid_intervals") <
						objective.minimum_valid_intervals) {
						reasons.emplace_back("insufficient_valid_intervals");
						valid = false;
					}
					if (unsigned_field(metric, "elapsed_ns") < objective.minimum_duration_ns) {
						reasons.emplace_back("insufficient_measured_duration");
						valid = false;
					}
					if (unsigned_field(metric, "discontinuities") != 0) {
						reasons.emplace_back("frame_identity_or_timestamp_discontinuity");
						valid = false;
					}
					if (unsigned_field(metric, "excluded_frames") != 0) {
						reasons.emplace_back("observer_or_interruption_exclusions");
						valid = false;
					}
					if (const auto* segments = metric.find("segments");
						!segments || segments->array.size() != 1) {
						reasons.emplace_back("measurement_has_discontinuous_segments");
						valid = false;
					}
				}
			}
			if (&manifest != &passes.front()) {
				bool unknown = false;
				const auto& first = passes.front().document;
				for (const auto key : {"scenario_id", "scenario_digest", "fixture_id", "seed"})
					if (!same_field(first, manifest.document, {"measurement", key}, unknown)) {
						valid = false;
						reasons.emplace_back("repeated_pass_workload_differs");
					}
				for (const auto key : {"build_profile_digest", "hardware_digest", "settings_digest",
									   "instrumentation_digest", "resource_digest",
									   "measurement_instrumentation", "capability_flags"})
					if (!same_field(first, manifest.document, {"provenance", key}, unknown)) {
						valid = false;
						reasons.emplace_back("repeated_pass_provenance_differs");
					}
				if (unknown) {
					valid = false;
					reasons.emplace_back("repeated_pass_provenance_unknown");
				}
			}
			const double fps =
				metric.find("observed_fps") ? metric.find("observed_fps")->number_or() : 0;
			bool pass = valid && satisfied(fps, objective.comparison, objective.threshold);
			if (objective.maximum_p95_interval_ns &&
				unsigned_field(metric, "p95_interval_ns") > *objective.maximum_p95_interval_ns)
				pass = false;
			Json encoded_reasons = Json::make_array();
			for (const auto& reason : reasons)
				encoded_reasons.array.emplace_back(reason);
			results.array.emplace_back(
				Json::make_object({{"capture_id", manifest.capture_id},
								   {"manifest_sha256", manifest.digest},
								   {"verdict", !valid ? "inconclusive"
											   : pass ? "pass"
													  : "fail"},
								   {"metrics", metric},
								   {"reasons", std::move(encoded_reasons)}}));
			inconclusive = inconclusive || !valid;
			passed = passed && pass;
			if (valid)
				pass_fps.emplace_back(fps);
		}
		Json spread = Json::make_object();
		if (!pass_fps.empty()) {
			auto bounds = std::minmax_element(pass_fps.begin(), pass_fps.end());
			spread.set("minimum_fps", Json(*bounds.first));
			spread.set("maximum_fps", Json(*bounds.second));
			spread.set("spread_fps", Json(*bounds.second - *bounds.first));
		}
		return Json::make_object(
			{{"schema", "FlowUi.Agentic.ObjectiveResult"},
			 {"schema_version", "1.0"},
			 {"verdict", inconclusive ? "inconclusive"
						 : passed	  ? "pass"
									  : "fail"},
			 {"objective", to_json(objective)},
			 {"passes", std::move(results)},
			 {"reasons", std::move(all_reasons)},
			 {"run_to_run", std::move(spread)},
			 {"uncertainty", "independent_pass_distribution; no per-frame confidence claim"}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::incompatible));
	}
}
Result<Json> compare_captures(const AgenticManifest& baseline, const AgenticManifest& candidate,
							  const std::optional<AgenticObjective>& objective) noexcept {
	try {
		Json reasons = Json::make_array();
		bool incompatible = false;
		bool unknown = false;
		for (const auto key : {"mode", "scenario_id", "scenario_revision", "scenario_digest",
							   "fixture_id", "seed", "window_ids", "profile"})
			if (!same_field(baseline.document, candidate.document, {"measurement", key}, unknown)) {
				incompatible = true;
				reasons.array.emplace_back(std::string("measurement_differs:") + key);
			}
		for (const auto key : {"build_profile_digest", "hardware_digest", "settings_digest",
							   "instrumentation_digest", "resource_digest",
							   "measurement_instrumentation", "capability_flags"})
			if (!same_field(baseline.document, candidate.document, {"provenance", key}, unknown)) {
				incompatible = true;
				reasons.array.emplace_back(std::string("provenance_differs:") + key);
			}
		if (unknown)
			reasons.array.emplace_back("required_comparability_provenance_unknown");
		auto baseline_frames = read_frames(baseline);
		auto candidate_frames = read_frames(candidate);
		Json before;
		Json after;
		if (baseline_frames) {
			auto analysis = analyze_performance(*baseline_frames);
			if (analysis)
				before = std::move(*analysis);
		}
		if (candidate_frames) {
			auto analysis = analyze_performance(*candidate_frames);
			if (analysis)
				after = std::move(*analysis);
		}
		std::vector<std::string> quality_reasons;
		bool complete = complete_required_coverage(baseline, quality_reasons) &&
						complete_required_coverage(candidate, quality_reasons);
		for (const auto& reason : quality_reasons)
			reasons.array.emplace_back(reason);
		Json result = Json::make_object(
			{{"schema", "FlowUi.Agentic.Comparison"},
			 {"schema_version", "1.0"},
			 {"algorithm_version", "comparable_cadence_1"},
			 {"baseline_capture_id", baseline.capture_id},
			 {"candidate_capture_id", candidate.capture_id},
			 {"baseline_manifest_sha256", baseline.digest},
			 {"candidate_manifest_sha256", candidate.digest},
			 {"compatibility", incompatible ? "incompatible"
							   : unknown || !complete || !baseline_frames || !candidate_frames
								   ? "inconclusive"
								   : "compatible"},
			 {"reasons", std::move(reasons)},
			 {"baseline", std::move(before)},
			 {"candidate", std::move(after)},
			 {"visual_review", "not_reviewed"},
			 {"diagnostic_regressions", "query_diagnostic_diff_for_exercised_scenario"}});
		if (objective) {
			const std::array<AgenticManifest, 1> pass{candidate};
			auto evaluated = check_objective(pass, *objective);
			if (!evaluated)
				return unexpectedError(evaluated.error());
			result.set("objective", std::move(*evaluated));
		}
		return result;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::incompatible));
	}
}
Result<Json> ingest_build_diagnostics(std::string_view log, std::string_view producer,
									  std::string_view build_id,
									  std::uint64_t maximum_bytes) noexcept {
	try {
		if (producer.empty() || producer.size() > 128 || !valid_identifier(build_id) ||
			log.size() > maximum_bytes)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		const std::regex gcc_pattern(
			R"(^(.+):([0-9]+):([0-9]+):[ \t]*(fatal error|error|warning|note):[ \t]*(.*)$)");
		const std::regex gcc_line_pattern(
			R"(^(.+):([0-9]+):[ \t]*(fatal error|error|warning|note):[ \t]*(.*)$)");
		const std::regex msvc_pattern(
			R"(^(.+)\(([0-9]+)(?:,([0-9]+))?\):[ \t]*(fatal error|error|warning|note)[ \t]*([A-Za-z]+[0-9]+)?:[ \t]*(.*)$)");
		Json diagnostics = Json::make_array();
		std::size_t begin = 0;
		std::uint64_t line_number = 0;
		std::uint64_t unparsed_lines = 0;
		while (begin < log.size()) {
			auto end = log.find('\n', begin);
			if (end == std::string_view::npos)
				end = log.size();
			const std::string line(log.substr(begin, end - begin));
			begin = end + 1;
			++line_number;
			std::smatch matched;
			std::string file;
			std::string severity;
			std::string message;
			std::string code;
			std::string source_line;
			std::string column;
			if (std::regex_match(line, matched, gcc_pattern)) {
				file = matched[1];
				source_line = matched[2];
				column = matched[3];
				severity = matched[4];
				message = matched[5];
			} else if (std::regex_match(line, matched, gcc_line_pattern)) {
				file = matched[1];
				source_line = matched[2];
				severity = matched[3];
				message = matched[4];
			} else if (std::regex_match(line, matched, msvc_pattern)) {
				file = matched[1];
				source_line = matched[2];
				column = matched[3];
				severity = matched[4];
				code = matched[5];
				message = matched[6];
			} else {
				if (!line.empty())
					++unparsed_lines;
				continue;
			}
			const auto code_begin = message.rfind(" [-");
			if (code.empty() && code_begin != std::string::npos && message.back() == ']') {
				code = message.substr(code_begin + 2, message.size() - code_begin - 3);
				message.resize(code_begin);
			}
			Json location = Json::make_object({{"file", file}, {"line", source_line}});
			if (!column.empty())
				location.set("column", column);
			Json issue =
				Json::make_object({{"domain", "build"},
								   {"producer", producer},
								   {"parser", "compiler_text_v1"},
								   {"build_id", build_id},
								   {"severity", severity == "fatal error" ? "fatal" : severity},
								   {"code", code},
								   {"location", std::move(location)},
								   {"message", message},
								   {"normalization_confidence", "recognized_compiler_syntax"},
								   {"raw_log", Json::make_object({{"artifact_id", "build_log"},
																  {"line", decimal(line_number)}})},
								   {"related_notes", Json::make_array()}});
			if (severity == "note" && !diagnostics.array.empty())
				diagnostics.array.back()
					.find("related_notes")
					->array.emplace_back(std::move(issue));
			else {
				issue.set("fingerprint", sha256(std::string(producer) + '\n' + file + '\n' + code +
												'\n' + message));
				diagnostics.array.emplace_back(std::move(issue));
			}
		}
		return Json::make_object(
			{{"schema", "FlowUi.Agentic.BuildDiagnostics"},
			 {"schema_version", "1.0"},
			 {"domain", "build"},
			 {"producer", producer},
			 {"build_id", build_id},
			 {"parser", "compiler_text_v1"},
			 {"diagnostics", std::move(diagnostics)},
			 {"raw_log_bytes", decimal(log.size())},
			 {"unparsed_lines", decimal(unparsed_lines)},
			 {"coverage", unparsed_lines ? "partial_normalization_raw_log_retained"
										 : "recognized_records_only"}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	}
}
Result<Json> diff_diagnostics(const Json& baseline, const Json& candidate,
							  bool same_exercised_scenario) noexcept {
	try {
		auto issue_array = [](const Json& value) -> const Json* {
			if (value.kind == Json::Kind::array)
				return &value;
			for (const auto key : {"diagnostics", "occurrences", "records"})
				if (const auto* records = value.find(key);
					records && records->kind == Json::Kind::array)
					return records;
			return nullptr;
		};
		const auto* before = issue_array(baseline);
		const auto* after = issue_array(candidate);
		if (!before || !after)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		auto identity = [](const Json& issue) {
			if (const auto* fingerprint = issue.find("fingerprint"))
				return std::string(fingerprint->string_or());
			if (const auto* error = issue.find("error")) {
				const auto encoded = [&](std::string_view key) {
					const auto* value = error->find(key);
					return value ? write_json(*value) : std::string("unknown");
				};
				return sha256("runtime\n" + encoded("domain") + '\n' + encoded("name") + '\n' +
							  encoded("site_name") + '\n' + encoded("code") + '\n' +
							  encoded("nativeCode"));
			}
			const auto encoded = [&](std::string_view key) {
				const auto* value = issue.find(key);
				return value ? write_json(*value) : std::string("unknown");
			};
			return sha256(encoded("domain") + '\n' + encoded("code") + '\n' + encoded("site") +
						  '\n' + encoded("message"));
		};
		std::map<std::string, Json> first;
		std::map<std::string, Json> second;
		for (const auto& issue : before->array)
			first.emplace(identity(issue), issue);
		for (const auto& issue : after->array)
			second.emplace(identity(issue), issue);
		Json groups = Json::make_array();
		for (const auto& issue : first)
			groups.array.emplace_back(Json::make_object(
				{{"fingerprint", issue.first},
				 {"state", second.contains(issue.first) ? "still_observed"
						   : same_exercised_scenario	? "resolved_in_exercised_scope"
														: "not_re_exercised"},
				 {"baseline", issue.second},
				 {"candidate", second.contains(issue.first) ? second.at(issue.first) : Json{}}}));
		for (const auto& issue : second)
			if (!first.contains(issue.first))
				groups.array.emplace_back(Json::make_object({{"fingerprint", issue.first},
															 {"state", "newly_observed"},
															 {"candidate", issue.second}}));
		return Json::make_object(
			{{"schema", "FlowUi.Agentic.DiagnosticsDiff"},
			 {"schema_version", "1.0"},
			 {"same_exercised_scenario", same_exercised_scenario},
			 {"groups", std::move(groups)},
			 {"limitation",
			  "absence_of_retained_occurrences_does_not_prove_all_paths_error_free"}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::incompatible));
	}
}
} // namespace FlowUi::devSystems::agentic
