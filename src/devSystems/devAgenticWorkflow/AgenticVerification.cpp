#include "devSystems/devAgenticWorkflow/AgenticVerification.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"

#include <algorithm>
#include <charconv>
#include <limits>

namespace FlowUi::devSystems::agentic {
namespace {
constexpr std::uint64_t maximum_duration_ns = 600'000'000'000ull;
constexpr std::uint64_t maximum_snapshot_bytes = 128u * 1024u * 1024u;
constexpr std::uint64_t maximum_retained_bytes = 2ull * 1024u * 1024u * 1024u;

void default_field(Json& object, std::string_view key, Json value) {
	if (!object.find(key))
		object.set(std::string(key), std::move(value));
}
[[nodiscard]] std::uint64_t count(const Json& object, std::string_view key) noexcept {
	const auto* value = object.find(key);
	return value ? value->uint64_or() : 0;
}
void erase_field(Json& object, std::string_view key) {
	std::erase_if(object.object, [key](const auto& field) { return field.first == key; });
}
[[nodiscard]] bool decimal_limit(const Json& limits, std::string_view key, std::uint64_t minimum,
								 std::uint64_t maximum) noexcept {
	const auto* value = limits.find(key);
	if (!value)
		return false;
	const auto decoded = parse_decimal(*value);
	return decoded && *decoded >= minimum && *decoded <= maximum;
}
[[nodiscard]] FlowUiError invalid_request() noexcept {
	return agentic_error(AgenticError::invalid_request);
}
[[nodiscard]] bool assertion_passed(const Json& assertion) noexcept {
	const auto* passed = assertion.find("passed");
	const auto* available = assertion.find("available");
	return passed && passed->bool_or() && (!available || available->bool_or());
}
[[nodiscard]] Json project_case_report(const Json& source, std::string_view detail) {
	Json projected = source;
	auto* assertions = projected.find("assertions");
	if (!assertions || assertions->kind != Json::Kind::array)
		return projected;
	const auto assertion_count = assertions->array.size();
	if (detail == "summary")
		assertions->array.clear();
	else if (detail == "failures")
		std::erase_if(assertions->array, assertion_passed);
	std::stable_sort(assertions->array.begin(), assertions->array.end(),
					 [](const auto& first, const auto& second) {
						 return !assertion_passed(first) && assertion_passed(second);
					 });
	for (auto& assertion : assertions->array)
		for (const auto key : {"expected", "observed"}) {
			auto* value = assertion.find(key);
			if (!value)
				continue;
			const auto encoded = write_json(*value);
			if (encoded.size() <= 1024)
				continue;
			std::size_t preview_bytes = 512;
			while (preview_bytes > 0 &&
				   (static_cast<unsigned char>(encoded[preview_bytes]) & 0xc0u) == 0x80u)
				--preview_bytes;
			*value = Json::make_object({{"truncated", true},
										{"serialized_bytes", decimal(encoded.size())},
										{"sha256", sha256(encoded)},
										{"json_preview", encoded.substr(0, preview_bytes)}});
		}
	projected.set("assertion_count", decimal(assertion_count));
	projected.set("assertion_details", detail);
	return projected;
}
} // namespace

Result<Json> normalize_verification_request(const Json& request) noexcept {
	try {
		if (!validate_agentic_document(request, "FlowUi.Agentic.VerificationRequest"))
			return unexpectedError(invalid_request());
		Json normalized = request;
		default_field(normalized, "suite_revision", "1");
		default_field(normalized, "idempotency_key", "");
		default_field(normalized, "execution_timeout_ns", decimal(30'000'000'000ull));
		default_field(normalized, "wait_timeout_ms", Json::number(20000));
		default_field(normalized, "failure_policy", "stop_case");
		const auto duration = parse_decimal(*normalized.find("execution_timeout_ns"));
		if (!duration || *duration == 0 || *duration > maximum_duration_ns)
			return unexpectedError(invalid_request());

		default_field(normalized, "evidence", Json::make_object());
		auto& evidence = *normalized.find("evidence");
		default_field(evidence, "policy", "assertions_only");
		for (const auto key : {"include_tree_text", "include_stacks", "include_native_messages",
							   "include_interaction_outcomes"})
			default_field(evidence, key, false);
		const bool assertions_only = evidence.find("policy")->string_or() == "assertions_only";

		default_field(normalized, "limits", Json::make_object());
		auto& limits = *normalized.find("limits");
		default_field(limits, "maximum_cases", Json::number(16));
		default_field(limits, "maximum_images", decimal(assertions_only ? 0 : 4));
		default_field(limits, "maximum_snapshot_bytes", decimal(32u * 1024u * 1024u));
		default_field(limits, "maximum_retained_bytes", decimal(64u * 1024u * 1024u));
		default_field(limits, "maximum_report_bytes", decimal(65536));
		if (!decimal_limit(limits, "maximum_images", 0, 64) ||
			!decimal_limit(limits, "maximum_snapshot_bytes", 4096, maximum_snapshot_bytes) ||
			!decimal_limit(limits, "maximum_retained_bytes", 4096, maximum_retained_bytes) ||
			!decimal_limit(limits, "maximum_report_bytes", 4096, 1024u * 1024u))
			return unexpectedError(invalid_request());

		default_field(normalized, "report", Json::make_object());
		auto& report = *normalized.find("report");
		default_field(report, "detail", "failures");
		default_field(report, "include_contact_sheet", false);

		auto& cases = normalized.find("cases")->array;
		if (cases.size() > limits.find("maximum_cases")->uint64_or())
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		std::uint64_t required_image_count = 0;
		for (std::size_t case_index = 0; case_index < cases.size(); ++case_index) {
			auto& case_request = cases[case_index];
			const auto case_id = case_request.find("case_id")->string_or();
			for (std::size_t previous_index = 0; previous_index < case_index; ++previous_index)
				if (cases[previous_index].find("case_id")->string_or() == case_id)
					return unexpectedError(invalid_request());
			auto& workload = *case_request.find("workload");
			if (workload.find("kind")->string_or() == "registered_scenario") {
				default_field(workload, "seed", decimal(0));
			} else {
				auto decoded = interaction_plan_from_json(*workload.find("plan"));
				if (!decoded)
					return unexpectedError(decoded.error());
				AgenticInteractionCapabilities planning_capabilities;
				planning_capabilities.custom_targets = true;
				planning_capabilities.text_focus_observations = true;
				auto compiled = compile_interaction_plan(*decoded, planning_capabilities);
				if (!compiled)
					return unexpectedError(compiled.error());
				workload.set("plan", to_json(compiled->plan, true));
			}
			default_field(case_request, "checkpoints", Json::make_array());
			auto& checkpoints = case_request.find("checkpoints")->array;
			if (assertions_only && !checkpoints.empty())
				return unexpectedError(invalid_request());
			for (std::size_t checkpoint_index = 0; checkpoint_index < checkpoints.size();
				 ++checkpoint_index) {
				auto& checkpoint = checkpoints[checkpoint_index];
				default_field(checkpoint, "required", true);
				const auto checkpoint_id = checkpoint.find("checkpoint_id")->string_or();
				for (std::size_t previous_index = 0; previous_index < checkpoint_index;
					 ++previous_index)
					if (checkpoints[previous_index].find("checkpoint_id")->string_or() ==
						checkpoint_id)
						return unexpectedError(invalid_request());
				const auto* window = checkpoint.find("window_id");
				if (window && window->uint64_or() == 0)
					return unexpectedError(invalid_request());
				if (checkpoint.find("required")->bool_or() &&
					checkpoint.find("evidence")->string_or() != "tree")
					++required_image_count;
			}
		}
		if (required_image_count > count(limits, "maximum_images"))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		return normalized;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}

std::string verification_request_digest(const Json& normalized_request) {
	Json execution = normalized_request;
	for (const auto key :
		 {"instance", "idempotency_key", "wait_timeout_ms", "report", "require_pass"})
		erase_field(execution, key);
	if (auto* cases = execution.find("cases"))
		for (auto& case_request : cases->array)
			if (auto* workload = case_request.find("workload"))
				if (auto* plan = workload->find("plan")) {
					erase_field(*plan, "run_id");
					erase_field(*plan, "idempotency_key");
				}
	return sha256(write_json(execution));
}

Result<Json> verification_report_page(const Json& report, const Json& arguments,
									  std::string_view detail) noexcept {
	try {
		const auto* cases = report.find("cases");
		if (!cases || cases->kind != Json::Kind::array ||
			(detail != "all" && detail != "summary" && detail != "failures"))
			return unexpectedError(invalid_request());
		const auto* requested_limit = arguments.find("limit");
		const auto* requested_bytes = arguments.find("max_bytes");
		const auto limit = requested_limit ? requested_limit->uint64_or() : 16;
		const auto maximum_bytes = requested_bytes ? requested_bytes->uint64_or() : 65536;
		if (limit == 0 || limit > 64 || maximum_bytes < 4096 || maximum_bytes > 1024u * 1024u)
			return unexpectedError(invalid_request());
		const auto digest = sha256(write_json(report));
		const auto prefix = digest + ':';
		std::uint64_t offset = 0;
		if (const auto* cursor = arguments.find("cursor"); cursor && !cursor->string_or().empty()) {
			const auto text = cursor->string_or();
			if (!text.starts_with(prefix))
				return unexpectedError(agentic_error(AgenticError::stale_generation));
			const auto suffix = text.substr(prefix.size());
			const auto parsed =
				std::from_chars(suffix.data(), suffix.data() + suffix.size(), offset);
			if (suffix.empty() || parsed.ec != std::errc{} ||
				parsed.ptr != suffix.data() + suffix.size() || offset >= cases->array.size())
				return unexpectedError(invalid_request());
		}
		Json page = report;
		page.set("cases", Json::make_array());
		page.set("report_digest", digest);
		page.set("detail_selector", Json::make_object({{"report_digest", digest}}));
		page.set("truncated", false);
		page.set("cursor", "");
		if (write_json(page).size() + 256 > maximum_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		auto* selected = page.find("cases");
		for (; offset < cases->array.size() && selected->array.size() < limit; ++offset) {
			selected->array.emplace_back(
				project_case_report(cases->array[static_cast<std::size_t>(offset)], detail));
			if (write_json(page).size() + 256 > maximum_bytes) {
				selected->array.pop_back();
				if (selected->array.empty())
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				break;
			}
		}
		if (offset < cases->array.size()) {
			page.set("truncated", true);
			page.set("cursor", prefix + std::to_string(offset));
		}
		return page;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}

Json verification_assertion_coverage(const Json& case_reports) {
	std::uint64_t required = 0;
	std::uint64_t evaluated = 0;
	std::uint64_t passed = 0;
	std::uint64_t failed = 0;
	std::uint64_t skipped = 0;
	std::uint64_t unavailable = 0;
	bool aggregation_overflow = false;
	const auto add_count = [&](std::uint64_t& aggregate, std::uint64_t amount) {
		if (amount > std::numeric_limits<std::uint64_t>::max() - aggregate) {
			aggregate = std::numeric_limits<std::uint64_t>::max();
			aggregation_overflow = true;
		} else
			aggregate += amount;
	};
	for (const auto& case_report : case_reports.array) {
		if (const auto* retained = case_report.find("coverage");
			retained && retained->kind == Json::Kind::object) {
			add_count(required, count(*retained, "required"));
			add_count(evaluated, count(*retained, "evaluated"));
			add_count(passed, count(*retained, "passed"));
			add_count(failed, count(*retained, "failed"));
			add_count(skipped, count(*retained, "skipped"));
			add_count(unavailable, count(*retained, "unavailable"));
			continue;
		}
		for (const auto key : {"assertions", "checkpoints"}) {
			const auto* records = case_report.find(key);
			if (!records)
				continue;
			for (const auto& record : records->array) {
				if (const auto* requirement = record.find("required");
					requirement && !requirement->bool_or())
					continue;
				add_count(required, 1);
				if (const auto* observation = record.find("skipped");
					observation && observation->bool_or()) {
					add_count(skipped, 1);
					continue;
				}
				const auto* available = record.find("available");
				const auto* outcome = record.find("passed");
				if ((available && !available->bool_or()) || !outcome ||
					outcome->kind != Json::Kind::boolean) {
					add_count(unavailable, 1);
					continue;
				}
				add_count(evaluated, 1);
				if (outcome->bool_or())
					add_count(passed, 1);
				else
					add_count(failed, 1);
			}
		}
	}
	if (aggregation_overflow)
		unavailable = std::numeric_limits<std::uint64_t>::max();
	return Json::make_object({{"required", decimal(required)},
							  {"evaluated", decimal(evaluated)},
							  {"passed", decimal(passed)},
							  {"failed", decimal(failed)},
							  {"skipped", decimal(skipped)},
							  {"unavailable", decimal(unavailable)}});
}

std::string_view verification_report_verdict(const Json& coverage, bool execution_complete,
											 bool cleanup_complete) noexcept {
	if (count(coverage, "failed") != 0)
		return "fail";
	if (!execution_complete || !cleanup_complete || count(coverage, "required") == 0 ||
		count(coverage, "required") != count(coverage, "passed") ||
		count(coverage, "skipped") != 0 || count(coverage, "unavailable") != 0)
		return "inconclusive";
	return "pass";
}
} // namespace FlowUi::devSystems::agentic
#endif
