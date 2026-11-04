#include "devSystems/devAgenticWorkflow/AgenticCapture.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] bool decode_uint(const Json& input, std::string_view key,
							   std::uint64_t& output) noexcept {
	if (const auto* value = input.find(key)) {
		const auto parsed = parse_decimal(*value);
		if (!parsed)
			return false;
		output = *parsed;
	}
	return true;
}
[[nodiscard]] bool decode_requirement(const Json& input, std::string_view key,
									  SectionRequirement& output) noexcept {
	if (const auto* value = input.find(key)) {
		if (value->string_or() == "omit")
			output = SectionRequirement::omit;
		else if (value->string_or() == "optional")
			output = SectionRequirement::optional;
		else if (value->string_or() == "required")
			output = SectionRequirement::required;
		else
			return false;
	}
	return true;
}
} // namespace
Result<AgenticCapturePlan> plan_capture(const AgenticCaptureRequest& request,
										const AgenticCapabilities& capabilities) noexcept {
	try {
		if (request.duration_ns == 0 || request.limits.maximum_ticks == 0 ||
			request.limits.maximum_records == 0 || request.limits.maximum_snapshot_bytes == 0 ||
			request.limits.finalization_timeout_ns == 0 ||
			(request.duration_ns > 3600000000000ull) || request.idempotency_key.size() > 256)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (request.objective &&
			(!std::isfinite(request.objective->threshold) || request.objective->threshold <= 0 ||
			 request.objective->minimum_passes == 0 || request.objective->minimum_passes > 1000 ||
			 (request.objective->comparison != "gt" && request.objective->comparison != "ge" &&
			  request.objective->comparison != "lt" && request.objective->comparison != "le")))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (request.visual != SectionRequirement::omit && !request.capture_window &&
			!request.capture_viewports)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (request.visual != SectionRequirement::omit &&
			(request.limits.maximum_images == 0 || request.limits.maximum_images > 1024 ||
			 request.limits.maximum_image_bytes == 0))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		AgenticCapturePlan plan;
		plan.request = request;
		if (request.profile == CaptureProfile::startup_diagnose &&
			(request.warmup_ticks != 0 || request.scenario_id.empty()))
			plan.rejected_reasons.emplace_back(
				"startup_requires_registered_cold_initialization_scenario_without_warmup");
		if (request.objective) {
			if (request.objective->minimum_duration_ns > 3599000000000ull)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			plan.request.duration_ns = std::max<std::uint64_t>(
				request.duration_ns, request.objective->minimum_duration_ns + 1000000000ull);
			plan.limitations.emplace_back("objective_present_span_margin_1s");
		}
		plan.reserved_ticks = request.limits.maximum_ticks;
		if (request.maximum_expected_tick_cadence == 0 ||
			request.maximum_expected_tick_cadence > 1000000)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		const auto estimated_ticks =
			request.measurement_ticks != 0
				? request.measurement_ticks
				: (plan.request.duration_ns * request.maximum_expected_tick_cadence +
				   999999999ull) /
					  1000000000ull;
		if (estimated_ticks > request.limits.maximum_ticks)
			plan.rejected_reasons.emplace_back(
				"duration_at_declared_maximum_cadence_exceeds_retention");
		if (!capabilities.enabled)
			plan.rejected_reasons.emplace_back("service_dormant");
		if (std::find(capabilities.modes.begin(), capabilities.modes.end(), request.mode) ==
			capabilities.modes.end())
			plan.rejected_reasons.emplace_back("execution_mode_unavailable");
		if (std::find(capabilities.profiles.begin(), capabilities.profiles.end(),
					  request.profile) == capabilities.profiles.end())
			plan.rejected_reasons.emplace_back("profile_unavailable");
		auto check_section = [&](SectionRequirement requirement, bool available,
								 std::string_view reason) {
			if (requirement == SectionRequirement::omit || available)
				return;
			if (requirement == SectionRequirement::required && !request.allow_degraded)
				plan.rejected_reasons.emplace_back(reason);
			else
				plan.limitations.emplace_back(reason);
		};
		check_section(request.performance, capabilities.enabled, "timing_disabled");
		if (request.profile == CaptureProfile::visual_inspect &&
			request.performance != SectionRequirement::omit)
			plan.limitations.emplace_back(
				"combined_visual_diagnostic_timing_suspends_development_interface");
		check_section(request.runtime_diagnostics, capabilities.runtime_diagnostics,
					  "runtime_diagnostics_disabled");
		check_section(request.memory, capabilities.memory, "memory_disabled");
		check_section(request.visual, capabilities.visual, "visual_capture_unavailable");
		if (request.profile == CaptureProfile::performance_verify &&
			request.visual != SectionRequirement::omit)
			plan.rejected_reasons.emplace_back("verification_forbids_visual_collection");
		if (request.objective && request.objective->metric == "window_present_call_cadence_fps" &&
			(request.mode != ExecutionMode::windowed || request.window_ids.size() != 1))
			plan.rejected_reasons.emplace_back("window_cadence_requires_one_native_window");
		if (request.objective && request.objective->metric == "display_present_cadence_fps")
			plan.rejected_reasons.emplace_back("display_presentation_timing_unavailable");
		if (request.limits.maximum_ticks > capabilities.limits.maximum_ticks ||
			request.limits.maximum_records > capabilities.limits.maximum_records ||
			request.limits.maximum_snapshot_bytes > capabilities.limits.maximum_snapshot_bytes ||
			request.limits.maximum_images > capabilities.limits.maximum_images ||
			request.limits.maximum_image_bytes > capabilities.limits.maximum_image_bytes ||
			request.limits.maximum_artifact_bytes > capabilities.limits.maximum_artifact_bytes)
			plan.rejected_reasons.emplace_back("requested_limits_exceed_available_reservation");
		if (request.measurement_ticks > request.limits.maximum_ticks ||
			request.warmup_ticks > request.limits.maximum_ticks)
			plan.rejected_reasons.emplace_back("requested_tick_interval_exceeds_retention");
		const std::uint64_t window_count = std::max<std::uint64_t>(request.window_ids.size(), 1);
		if (request.limits.maximum_ticks >
			std::numeric_limits<std::uint64_t>::max() / (256u * window_count))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		plan.estimated_snapshot_bytes = request.limits.maximum_ticks * 256u * window_count;
		if (request.runtime_diagnostics != SectionRequirement::omit)
			plan.estimated_snapshot_bytes +=
				std::min<std::uint64_t>(request.limits.maximum_records, 65536) * 256u;
		if (request.memory != SectionRequirement::omit)
			plan.estimated_snapshot_bytes +=
				std::min<std::uint64_t>(request.limits.maximum_records, 65536) * 128u;
		if (request.visual != SectionRequirement::omit)
			plan.estimated_snapshot_bytes += request.limits.maximum_image_bytes;
		if (plan.estimated_snapshot_bytes > request.limits.maximum_snapshot_bytes)
			plan.rejected_reasons.emplace_back("estimated_snapshot_exceeds_byte_budget");
		plan.limitations.emplace_back("compositor_display_times_unavailable");
		plan.feasible = plan.rejected_reasons.empty();
		return plan;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCapture));
	}
}
Result<AgenticCaptureRequest> capture_request_from_json(const Json& input) noexcept {
	try {
		if (input.kind != Json::Kind::object)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		AgenticCaptureRequest request;
		if (const auto* mode = input.find("mode")) {
			bool found = false;
			for (const auto candidate : {ExecutionMode::offline, ExecutionMode::cpu_scenario,
										 ExecutionMode::layout_scenario,
										 ExecutionMode::offscreen_vulkan, ExecutionMode::windowed})
				if (mode->string_or() == name(candidate)) {
					request.mode = candidate;
					found = true;
				}
			if (!found)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		}
		if (const auto* profile = input.find("profile")) {
			bool found = false;
			for (const auto candidate :
				 {CaptureProfile::performance_diagnose, CaptureProfile::performance_verify,
				  CaptureProfile::diagnostics_reproduce, CaptureProfile::visual_inspect,
				  CaptureProfile::memory_diagnose, CaptureProfile::startup_diagnose})
				if (profile->string_or() == name(candidate)) {
					request.profile = candidate;
					found = true;
				}
			if (!found)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		}
		if (request.profile == CaptureProfile::visual_inspect) {
			request.performance = SectionRequirement::omit;
			request.visual = SectionRequirement::required;
		}
		if (request.profile == CaptureProfile::memory_diagnose)
			request.memory = SectionRequirement::required;
		if (request.profile == CaptureProfile::diagnostics_reproduce)
			request.runtime_diagnostics = SectionRequirement::required;
		if (const auto* windows = input.find("window_ids")) {
			if (windows->kind != Json::Kind::array || windows->array.size() > 64)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			for (const auto& value : windows->array) {
				auto parsed = parse_decimal(value);
				if (!parsed || *parsed == 0)
					return unexpectedError(agentic_error(AgenticError::invalid_request));
				if (std::find(request.window_ids.begin(), request.window_ids.end(), *parsed) !=
					request.window_ids.end())
					return unexpectedError(agentic_error(AgenticError::invalid_request));
				request.window_ids.emplace_back(*parsed);
			}
		}
		if (!decode_uint(input, "duration_ns", request.duration_ns) ||
			!decode_uint(input, "warmup_ticks", request.warmup_ticks) ||
			!decode_uint(input, "measurement_ticks", request.measurement_ticks) ||
			!decode_uint(input, "seed", request.seed))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (const auto* measure = input.find("measurement"))
			if (!decode_uint(*measure, "duration_ns", request.duration_ns) ||
				!decode_uint(*measure, "ticks", request.measurement_ticks))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (const auto* warmup = input.find("warmup"))
			if (!decode_uint(*warmup, "minimum_ticks", request.warmup_ticks))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (const auto* sections = input.find("sections"))
			if (!decode_requirement(*sections, "performance", request.performance) ||
				!decode_requirement(*sections, "runtime_diagnostics",
									request.runtime_diagnostics) ||
				!decode_requirement(*sections, "memory", request.memory) ||
				!decode_requirement(*sections, "visual", request.visual))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (const auto* value = input.find("maximum_expected_tick_cadence")) {
			if (value->kind != Json::Kind::number || value->uint64_or(0) == 0 ||
				value->uint64_or(1000001) > 1000000)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			request.maximum_expected_tick_cadence = static_cast<std::uint32_t>(value->uint64_or());
		}
		if (const auto* scenario = input.find("scenario")) {
			if (const auto* value = scenario->find("id"))
				request.scenario_id = value->string_or();
			if (const auto* value = scenario->find("revision"))
				request.scenario_revision = value->string_or();
			if (const auto* value = scenario->find("digest"))
				request.scenario_digest = value->string_or();
			if (const auto* value = scenario->find("fixture_id"))
				request.fixture_id = value->string_or();
			if (!decode_uint(*scenario, "seed", request.seed))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		}
		if (const auto* value = input.find("scenario_id"))
			request.scenario_id = value->string_or();
		if (const auto* value = input.find("idempotency_key"))
			request.idempotency_key = value->string_or();
		if (const auto* value = input.find("allow_degraded")) {
			if (value->kind != Json::Kind::boolean)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			request.allow_degraded = value->boolean;
		}
		if (const auto* sources = input.find("visual_sources")) {
			if (sources->kind != Json::Kind::object)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			for (const auto& field :
				 {std::pair<std::string_view, bool*>{"window", &request.capture_window},
				  {"viewports", &request.capture_viewports}})
				if (const auto* value = sources->find(field.first)) {
					if (value->kind != Json::Kind::boolean)
						return unexpectedError(agentic_error(AgenticError::invalid_request));
					*field.second = value->boolean;
				}
		}
		if (const auto* content = input.find("content")) {
			if (content->kind != Json::Kind::object)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			const std::array<std::pair<std::string_view, bool*>, 6> fields{
				{{"include_tree_text", &request.content.include_tree_text},
				 {"include_native_messages", &request.content.include_native_messages},
				 {"include_stacks", &request.content.include_stacks},
				 {"include_source_paths", &request.content.include_source_paths},
				 {"include_interaction_labels", &request.content.include_interaction_labels},
				 {"include_interaction_outcomes", &request.content.include_interaction_outcomes}}};
			for (const auto& field : fields)
				if (const auto* value = content->find(field.first)) {
					if (value->kind != Json::Kind::boolean)
						return unexpectedError(agentic_error(AgenticError::invalid_request));
					*field.second = value->boolean;
				}
		}
		if (const auto* limits = input.find("limits")) {
			if (!decode_uint(*limits, "maximum_ticks", request.limits.maximum_ticks) ||
				!decode_uint(*limits, "maximum_records", request.limits.maximum_records) ||
				!decode_uint(*limits, "maximum_snapshot_bytes",
							 request.limits.maximum_snapshot_bytes) ||
				!decode_uint(*limits, "maximum_artifact_bytes",
							 request.limits.maximum_artifact_bytes) ||
				!decode_uint(*limits, "maximum_disk_bytes", request.limits.maximum_disk_bytes) ||
				!decode_uint(*limits, "maximum_images", request.limits.maximum_images) ||
				!decode_uint(*limits, "maximum_image_bytes", request.limits.maximum_image_bytes) ||
				!decode_uint(*limits, "finalization_timeout_ns",
							 request.limits.finalization_timeout_ns))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		}
		if (const auto* value = input.find("objective")) {
			if (value->kind != Json::Kind::object)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			AgenticObjective objective;
			if (const auto* member = value->find("metric"))
				objective.metric = member->string_or();
			if (const auto* member = value->find("operator"))
				objective.comparison = member->string_or();
			if (const auto* member = value->find("threshold"))
				objective.threshold = member->number_or(std::numeric_limits<double>::quiet_NaN());
			if (!std::isfinite(objective.threshold) || objective.threshold <= 0 ||
				!decode_uint(*value, "minimum_valid_intervals",
							 objective.minimum_valid_intervals) ||
				!decode_uint(*value, "minimum_duration_ns", objective.minimum_duration_ns))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			if (const auto* member = value->find("minimum_passes")) {
				if (member->kind != Json::Kind::number || member->uint64_or(0) == 0 ||
					member->uint64_or(1001) > 1000)
					return unexpectedError(agentic_error(AgenticError::invalid_request));
				objective.minimum_passes = static_cast<std::uint32_t>(member->uint64_or());
			}
			if (const auto* member = value->find("maximum_p95_interval_ns")) {
				const auto maximum = member->number_or(-1);
				if (maximum <= 0)
					return unexpectedError(agentic_error(AgenticError::invalid_request));
				objective.maximum_p95_interval_ns = maximum;
			}
			request.objective = std::move(objective);
		}
		return request;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}
Json to_json(const AgenticObjective& objective) {
	auto output =
		Json::make_object({{"metric", objective.metric},
						   {"operator", objective.comparison},
						   {"threshold", Json(objective.threshold)},
						   {"minimum_valid_intervals", decimal(objective.minimum_valid_intervals)},
						   {"minimum_duration_ns", decimal(objective.minimum_duration_ns)},
						   {"minimum_passes", Json::number(objective.minimum_passes)}});
	if (objective.maximum_p95_interval_ns)
		output.set("maximum_p95_interval_ns", Json(*objective.maximum_p95_interval_ns));
	return output;
}
Json to_json(const AgenticCaptureRequest& request) {
	Json windows = Json::make_array();
	for (const auto window : request.window_ids)
		windows.array.emplace_back(decimal(window));
	Json output = Json::make_object(
		{{"schema", "FlowUi.Agentic.CaptureRequest"},
		 {"schema_version", agentic_schema_version},
		 {"mode", name(request.mode)},
		 {"profile", name(request.profile)},
		 {"window_ids", std::move(windows)},
		 {"scenario", Json::make_object({{"id", request.scenario_id},
										 {"revision", request.scenario_revision},
										 {"digest", request.scenario_digest},
										 {"fixture_id", request.fixture_id},
										 {"seed", decimal(request.seed)}})},
		 {"duration_ns", decimal(request.duration_ns)},
		 {"maximum_expected_tick_cadence", Json::number(request.maximum_expected_tick_cadence)},
		 {"warmup_ticks", decimal(request.warmup_ticks)},
		 {"measurement_ticks", decimal(request.measurement_ticks)},
		 {"sections", Json::make_object({{"performance", name(request.performance)},
										 {"runtime_diagnostics", name(request.runtime_diagnostics)},
										 {"memory", name(request.memory)},
										 {"visual", name(request.visual)}})},
		 {"limits", to_json(request.limits)},
		 {"content",
		  Json::make_object(
			  {{"include_tree_text", request.content.include_tree_text},
			   {"include_native_messages", request.content.include_native_messages},
			   {"include_stacks", request.content.include_stacks},
			   {"include_source_paths", request.content.include_source_paths},
			   {"include_interaction_labels", request.content.include_interaction_labels},
			   {"include_interaction_outcomes", request.content.include_interaction_outcomes}})},
		 {"visual_sources", Json::make_object({{"window", request.capture_window},
											   {"viewports", request.capture_viewports}})},
		 {"allow_degraded", request.allow_degraded},
		 {"idempotency_key", request.idempotency_key}});
	if (request.objective)
		output.set("objective", to_json(*request.objective));
	return output;
}
Json to_json(const AgenticCapturePlan& plan) {
	Json limitations = Json::make_array();
	for (const auto& value : plan.limitations)
		limitations.array.emplace_back(value);
	Json reasons = Json::make_array();
	for (const auto& value : plan.rejected_reasons)
		reasons.array.emplace_back(value);
	return Json::make_object({{"request", to_json(plan.request)},
							  {"feasible", plan.feasible},
							  {"estimated_snapshot_bytes", decimal(plan.estimated_snapshot_bytes)},
							  {"reserved_ticks", decimal(plan.reserved_ticks)},
							  {"limitations", std::move(limitations)},
							  {"rejected_reasons", std::move(reasons)}});
}
} // namespace FlowUi::devSystems::agentic
