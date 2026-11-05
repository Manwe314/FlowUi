#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticVerification.hpp"

#include <cstdlib>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace {
void require(bool condition, std::string_view message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
[[nodiscard]] Json request() {
	return *parse_json(
		R"({"schema":"FlowUi.Agentic.VerificationRequest","schema_version":"1.0","suite_id":"fsel.smoke","cases":[{"case_id":"click","workload":{"kind":"interaction_plan","plan":{"window_id":"1","mode":"common_controls","steps":[{"step_id":"check","operation":"assert","expect":["layout_completed"]}]}}}]})");
}
void normalization_contracts() {
	auto source = request();
	require(bool(validate_agentic_document(source, "FlowUi.Agentic.VerificationRequest")),
			"verification request schema rejected source");
	const auto decoded = interaction_plan_from_json(
		*source.find("cases")->array.front().find("workload")->find("plan"));
	require(bool(decoded), "embedded verification plan did not decode");
	require(bool(compile_interaction_plan(*decoded)), "embedded verification plan did not compile");
	auto normalized = normalize_verification_request(source);
	require(bool(normalized), "finite verification plan rejected");
	require(normalized->find("execution_timeout_ns")->uint64_or() == 30'000'000'000ull,
			"default execution deadline changed");
	require(normalized->find("evidence")->find("policy")->string_or() == "assertions_only",
			"assertions-only suite silently collects visual evidence");
	require(normalized->find("limits")->find("maximum_images")->uint64_or() == 0,
			"assertions-only default reserves readbacks");
	auto round_trip = normalize_verification_request(*normalized);
	require(round_trip && write_json(*round_trip) == write_json(*normalized),
			"normalization is not stable");
	const auto digest = verification_request_digest(*normalized);
	normalized->set("wait_timeout_ms", Json::number(0));
	normalized->set("instance", "another_instance");
	normalized->set("idempotency_key", "another_attempt");
	normalized->find("report")->set("detail", "all");
	require(digest == verification_request_digest(*normalized),
			"retry or response preferences changed execution identity");
	normalized->set("failure_policy", "stop_suite");
	require(digest != verification_request_digest(*normalized),
			"different execution policy retained execution identity");

	for (const auto field : {"shell", "executable", "script"}) {
		auto invalid = source;
		invalid.set(field, "untrusted");
		require(!normalize_verification_request(invalid), "unknown execution field accepted");
	}
	auto invalid = source;
	invalid.set("schema_version", "2.0");
	require(!normalize_verification_request(invalid), "unsupported request version accepted");
	invalid = source;
	invalid.set("execution_timeout_ns", "18446744073709551616");
	require(!normalize_verification_request(invalid), "overflowed unsigned duration accepted");
	invalid.set("execution_timeout_ns", Json::number(30));
	require(!normalize_verification_request(invalid), "numeric unsigned duration accepted");
	invalid = source;
	invalid.set("wait_timeout_ms", Json::number(20001));
	require(!normalize_verification_request(invalid), "unbounded command wait accepted");
	invalid = source;
	invalid.find("cases")->array.emplace_back(invalid.find("cases")->array.front());
	require(!normalize_verification_request(invalid), "duplicate case identity accepted");
	invalid = source;
	invalid.find("cases")->array.front().find("workload")->find("plan")->set("script", "x");
	require(!normalize_verification_request(invalid), "unknown embedded plan field accepted");
	auto custom_request = source;
	custom_request.find("cases")
		->array.front()
		.find("workload")
		->set(
			"plan",
			*parse_json(
				R"({"window_id":"1","mode":"common_controls","steps":[{"operation":"click","target":{"custom_id":"fixture.entry"},"expect":["target_released","text_focused"]}]})"));
	require(bool(normalize_verification_request(custom_request)),
			"structural planning rejected runtime-resolved custom targets or text focus");
}
void checkpoint_contracts() {
	auto source = request();
	source.find("cases")->array.front().set(
		"checkpoints",
		*parse_json(
			R"([{"checkpoint_id":"shown","trigger":"case_verified","evidence":"image_and_tree"}])"));
	require(!normalize_verification_request(source), "assertions-only checkpoint accepted");
	source.set("evidence", Json::make_object({{"policy", "checkpoints_and_failure"}}));
	auto normalized = normalize_verification_request(source);
	require(bool(normalized), "bounded visual checkpoint rejected");
	require(normalized->find("cases")
				->array.front()
				.find("checkpoints")
				->array.front()
				.find("required")
				->bool_or(),
			"checkpoint silently became optional");
	source.set("limits", Json::make_object({{"maximum_images", decimal(0)}}));
	require(!normalize_verification_request(source), "required checkpoint exceeded image budget");
}
void coverage_contracts() {
	auto cases = *parse_json(
		R"([{"assertions":[{"passed":true},{"passed":false},{"available":false},{"skipped":true},{"required":false,"passed":false}],"checkpoints":[{"passed":true}]}])");
	auto coverage = verification_assertion_coverage(cases);
	require(coverage.find("required")->uint64_or() == 5 &&
				coverage.find("evaluated")->uint64_or() == 3 &&
				coverage.find("failed")->uint64_or() == 1 &&
				coverage.find("unavailable")->uint64_or() == 1,
			"required assertion/checkpoint coverage is inaccurate");
	require(verification_report_verdict(coverage, false, false) == "fail",
			"interrupted execution hid a definite assertion failure");
	cases = *parse_json(R"([{"assertions":[{"passed":true}]}])");
	coverage = verification_assertion_coverage(cases);
	require(verification_report_verdict(coverage, true, true) == "pass",
			"completed assertions did not pass");
	require(verification_report_verdict(coverage, true, false) == "inconclusive",
			"missing cleanup passed");
	require(verification_report_verdict(verification_assertion_coverage(Json::make_array()), true,
										true) == "inconclusive",
			"mere input delivery passed without assertions");
	Json compact_case = Json::make_object({{"coverage", coverage},
										   {"assertions", Json::make_array()},
										   {"checkpoints", Json::make_array()}});
	const auto compact_coverage = verification_assertion_coverage(Json::make_array({compact_case}));
	require(write_json(compact_coverage) == write_json(coverage) &&
				verification_report_verdict(compact_coverage, true, true) == "pass",
			"compacting case details changed preserved assertion coverage");
}
void report_contracts() {
	Json report =
		Json::make_object({{"job_id", "job_one"},
						   {"verdict", "fail"},
						   {"coverage", Json::make_object({{"failed", decimal(1)}})},
						   {"cases", Json::make_array({Json::make_object({{"case_id", "one"}}),
													   Json::make_object({{"case_id", "two"}})})}});
	auto page = verification_report_page(report, Json::make_object({{"limit", Json::number(1)}}));
	require(page && page->find("cases")->array.size() == 1 &&
				page->find("verdict")->string_or() == "fail" &&
				!page->find("cursor")->string_or().empty(),
			"bounded report lost verdict or detail cursor");
	const auto arguments = Json::make_object({{"cursor", *page->find("cursor")}});
	page = verification_report_page(report, arguments);
	require(page && page->find("cases")->array.front().find("case_id")->string_or() == "two",
			"report continuation lost next case");
	report.set("verdict", "pass");
	require(!verification_report_page(report, arguments), "cursor accepted a different report");
	report.find("cases")->array.front().set(
		"assertions", Json::make_array({Json::make_object({{"passed", true}}),
										Json::make_object({{"passed", false},
														   {"expected", std::string(2048, 'x')}}),
										Json::make_object({{"available", false}})}));
	const auto all = verification_report_page(report, Json::make_object());
	require(all && all->find("cases")->array.front().find("assertions")->array.size() == 3,
			"all detail hid assertion records");
	const auto& first_assertion =
		all->find("cases")->array.front().find("assertions")->array.front();
	require(!first_assertion.find("passed")->bool_or() &&
				first_assertion.find("expected")->find("truncated")->bool_or(),
			"report did not prioritize failures or bound large expected values");
	const auto failures = verification_report_page(report, Json::make_object(), "failures");
	require(failures &&
				failures->find("cases")->array.front().find("assertions")->array.size() == 2,
			"failure detail retained successful assertions or hid unavailable checks");
	const auto summary = verification_report_page(report, Json::make_object(), "summary");
	require(summary && summary->find("cases")->array.front().find("assertions")->array.empty() &&
				summary->find("cases")->array.front().find("assertion_count")->uint64_or() == 3,
			"summary detail lost assertion counts");
	require(summary->find("report_digest")->string_or() == all->find("report_digest")->string_or(),
			"response projection changed canonical report identity");
}
} // namespace
int main() {
	normalization_contracts();
	checkpoint_contracts();
	coverage_contracts();
	report_contracts();
	return 0;
}
