#include "FlowUi/Public/Flow.hpp"
#include "TestHarness.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionRuntime.hpp"
#include "devSystems/devAgenticWorkflow/AgenticWorkflow.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <thread>

using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace {
struct TestDirectory {
	std::filesystem::path root;
	TestDirectory() {
		const auto identity = make_identifier("verification_service_test");
		FLOWUI_CHECK(identity);
		root = std::filesystem::temp_directory_path() / *identity;
		FLOWUI_CHECK(std::filesystem::create_directory(root));
	}
	~TestDirectory() {
		std::error_code error;
		std::filesystem::remove_all(root, error);
	}
};
struct ScenarioCounters {
	std::uint64_t setup_calls = 0;
	std::uint64_t cleanup_calls = 0;
};
struct NestedScenarioState {
	std::string child_job;
	bool cleanup_observed_released_input = false;
};
enum class ScenarioBehavior { pass, failing_step, pending_setup, failing_cleanup };

void frame(App& application) {
	FLOWUI_CHECK(application.beginFrame());
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.backgroundColor = {20, 40, 60, 255};
	CLAY(CLAY_ID("verification_service_fixture"), root) {}
	FLOWUI_CHECK(application.endFrame());
	FLOWUI_CHECK(application.drawFrame());
}
[[nodiscard]] Json command(AgenticWorkflowService& service, std::string_view operation,
						   Json arguments) {
	const auto response = service.submit_command(AgenticCommandRequest{
		std::move(arguments), "verification-service-test", std::string(operation)});
	FLOWUI_CHECK(response);
	if (response->find("exit_code")->uint64_or() != 0)
		std::cerr << write_json(*response) << '\n';
	FLOWUI_CHECK(response->find("exit_code")->uint64_or() == 0);
	return *response;
}
[[nodiscard]] Json request(std::string_view scenario_id, std::string_view key) {
	return Json::make_object(
		{{"schema", "FlowUi.Agentic.VerificationRequest"},
		 {"schema_version", "1.0"},
		 {"suite_id", "verification.service.contract"},
		 {"idempotency_key", key},
		 {"wait_timeout_ms", Json::number(0)},
		 {"execution_timeout_ns", decimal(10'000'000'000ull)},
		 {"report", Json::make_object({{"detail", "all"}})},
		 {"cases", Json::make_array({Json::make_object(
					   {{"case_id", "semantic_case"},
						{"workload", Json::make_object({{"kind", "registered_scenario"},
														{"scenario_id", scenario_id},
														{"revision", "1"},
														{"fixture_id", "service.fixture"}})}})})}});
}
[[nodiscard]] Json run(AgenticWorkflowService& service, const Json& suite_request) {
	return command(service, "verification.run", Json::make_object({{"request", suite_request}}));
}
[[nodiscard]] Json finish(App& application, std::string_view identity) {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (std::chrono::steady_clock::now() < deadline) {
		frame(application);
		const auto response = command(application.agentic_workflow(), "verification.report",
									  Json::make_object({{"job_id", identity}}));
		const auto* report = response.find("result");
		FLOWUI_CHECK(report);
		if (report->find("verdict")) {
			FLOWUI_CHECK(validate_agentic_document(*report, "FlowUi.Agentic.VerificationReport"));
			return *report;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	FLOWUI_CHECK(false);
	return Json{};
}
void register_scenario(AgenticWorkflowService& service, std::string_view identifier,
					   ScenarioBehavior behavior, ScenarioCounters& counters,
					   const std::atomic<bool>* setup_gate = nullptr) {
	AgenticScenario scenario;
	scenario.descriptor.scenario_id = identifier;
	scenario.descriptor.revision = "1";
	scenario.descriptor.digest = sha256(identifier);
	scenario.descriptor.fixture_id = "service.fixture";
	scenario.descriptor.deadline_ns =
		behavior == ScenarioBehavior::pending_setup ? 50'000'000ull : 5'000'000'000ull;
	scenario.setup = [behavior, &counters, setup_gate](
						 const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		if (!context.application->hasWindow(context.application->mainWindowId()))
			return unexpectedError(agentic_error(AgenticError::unavailable));
		if (setup_gate && !setup_gate->load())
			return AgenticScenarioProgress::pending;
		++counters.setup_calls;
		return behavior == ScenarioBehavior::pending_setup ? AgenticScenarioProgress::pending
														   : AgenticScenarioProgress::complete;
	};
	scenario.step =
		[behavior](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		if (behavior == ScenarioBehavior::failing_step) {
			AgenticScenarioAction assertion;
			assertion.name = "step_semantic_mismatch";
			assertion.assertion = true;
			assertion.passed = false;
			assertion.state_revision = context.app_tick;
			const auto recorded = context.record_action(assertion);
			if (!recorded)
				return unexpectedError(recorded.error());
			return unexpectedError(agentic_error(AgenticError::incomplete));
		}
		return context.application->hasWindow(context.application->mainWindowId())
				   ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
				   : unexpectedError(agentic_error(AgenticError::unavailable));
	};
	scenario.verify = [](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		AgenticScenarioAction assertion;
		assertion.name = "fixture_semantic_state";
		assertion.assertion = true;
		assertion.passed = context.application->hasWindow(context.application->mainWindowId());
		assertion.state_revision = context.app_tick;
		const auto recorded = context.record_action(assertion);
		return recorded ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
						: unexpectedError(recorded.error());
	};
	scenario.cleanup =
		[behavior,
		 &counters](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		++counters.cleanup_calls;
		if (behavior == ScenarioBehavior::failing_cleanup ||
			!context.application->hasWindow(context.application->mainWindowId()))
			return unexpectedError(agentic_error(AgenticError::unavailable));
		return AgenticScenarioProgress::complete;
	};
	FLOWUI_CHECK(service.register_scenario(std::move(scenario)));
}

void register_nested_input_scenario(AgenticWorkflowService& service, NestedScenarioState& state) {
	AgenticScenario scenario;
	scenario.descriptor.scenario_id = "service.nested_input";
	scenario.descriptor.revision = "1";
	scenario.descriptor.digest = sha256("nested-held-input-cancellation-v1");
	scenario.descriptor.fixture_id = "service.fixture";
	scenario.descriptor.deadline_ns = 30'000'000'000ull;
	scenario.setup =
		[&state](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		if (!context.application->hasWindow(context.application->mainWindowId()))
			return unexpectedError(agentic_error(AgenticError::unavailable));
		state.child_job.clear();
		state.cleanup_observed_released_input = false;
		return AgenticScenarioProgress::complete;
	};
	scenario.step =
		[&state](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		auto& workflow = context.application->agentic_workflow();
		if (state.child_job.empty()) {
			AgenticInteractionPlan plan;
			plan.window_id = context.application->mainWindowId();
			plan.mode = AgenticInteractionMode::common_controls;
			plan.deadline_ns = 30'000'000'000ull;
			plan.limits.maximum_frames = 128;
			plan.limits.maximum_observations = 256;
			plan.limits.maximum_artifact_bytes = 1024u * 1024u;
			plan.steps.reserve(3);
			AgenticInteractionStep down;
			down.step_id = "hold-pointer-down";
			down.operation = AgenticInteractionOperation::pointer_down;
			down.target.emplace();
			down.target->point = {40, 30};
			plan.steps.emplace_back(std::move(down));
			AgenticInteractionStep hold;
			hold.step_id = "hold-until-canceled";
			hold.operation = AgenticInteractionOperation::wait;
			hold.minimum_duration_ns = 20'000'000'000ull;
			hold.deadline_ns = 30'000'000'000ull;
			plan.steps.emplace_back(std::move(hold));
			AgenticInteractionStep up;
			up.step_id = "release-after-wait";
			up.operation = AgenticInteractionOperation::pointer_up;
			plan.steps.emplace_back(std::move(up));
			const auto runtime = workflow.interaction_runtime();
			if (!runtime)
				return unexpectedError(runtime.error());
			const auto started = (*runtime)->start_plan(to_json(plan), {}, context.job_id);
			if (!started)
				return unexpectedError(started.error());
			state.child_job = *started;
		}
		const auto child = workflow.query_interaction(state.child_job);
		if (!child)
			return unexpectedError(child.error());
		return child->execution_succeeded ? AgenticScenarioProgress::complete
										  : AgenticScenarioProgress::pending;
	};
	scenario.verify =
		[&state](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		const auto child =
			context.application->agentic_workflow().query_interaction(state.child_job);
		if (!child)
			return unexpectedError(child.error());
		AgenticScenarioAction assertion;
		assertion.name = "nested_input_completed";
		assertion.assertion = true;
		assertion.passed = child->execution_succeeded && child->cleanup_complete;
		const auto recorded = context.record_action(assertion);
		return recorded ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
						: unexpectedError(recorded.error());
	};
	scenario.cleanup =
		[&state](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		const auto child =
			context.application->agentic_workflow().query_interaction(state.child_job);
		if (!child)
			return unexpectedError(child.error());
		if (!child->cleanup_complete)
			return AgenticScenarioProgress::pending;
		state.cleanup_observed_released_input =
			!context.application->ui().getCurrentFrameInput().mouseDown[0];
		return state.cleanup_observed_released_input
				   ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
				   : unexpectedError(agentic_error(AgenticError::incomplete));
	};
	FLOWUI_CHECK(service.register_scenario(std::move(scenario)));
}

void verify_service(const std::filesystem::path& store) {
	AppConfig configuration;
	configuration.window.title = "FlowUi verification suite service contracts";
	configuration.window.width = 160;
	configuration.window.height = 120;
	configuration.vk.enableValidation = false;
	configuration.vk.enableDebugUtils = false;
	configuration.errors.observer.mode = ErrorReportingMode::SinkOnly;
	configuration.dev.enabled = true;
	configuration.agentic_workflow.enabled = true;
	configuration.agentic_workflow.visual_capture_enabled = true;
	configuration.agentic_workflow.input_control_enabled = true;
	configuration.agentic_workflow.store_path = store;
	configuration.agentic_workflow.application_id = "flowui.tests.verification";
	// An eligible frame cannot fit: requested checkpoints must report unavailable evidence.
	configuration.agentic_workflow.limits.maximum_image_bytes = 1;
	std::array<ScenarioCounters, 4> counters;
	std::atomic<bool> setup_gate{false};
	NestedScenarioState nested_state;
	auto application = makeApplication(configuration);
	auto& service = application.agentic_workflow();
	register_scenario(service, "service.pass", ScenarioBehavior::pass, counters[0], &setup_gate);
	register_scenario(service, "service.step_failure", ScenarioBehavior::failing_step, counters[1]);
	register_scenario(service, "service.pending", ScenarioBehavior::pending_setup, counters[2]);
	register_scenario(service, "service.cleanup_failure", ScenarioBehavior::failing_cleanup,
					  counters[3]);
	register_nested_input_scenario(service, nested_state);
	frame(application);
	const auto duplicate_request = request("service.pass", "same-admission-once");
	std::array<std::future<Result<Json>>, 2> duplicates;
	for (auto& duplicate : duplicates)
		duplicate = std::async(std::launch::async, [&] {
			return service.submit_command(
				AgenticCommandRequest{Json::make_object({{"request", duplicate_request}}),
									  "concurrent-retry", "verification.run"});
		});
	const auto admission_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while ((duplicates[0].wait_for(std::chrono::milliseconds(0)) != std::future_status::ready ||
			duplicates[1].wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) &&
		   std::chrono::steady_clock::now() < admission_deadline) {
		frame(application);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto first = duplicates[0].get();
	const auto second = duplicates[1].get();
	FLOWUI_CHECK(first && second && first->find("exit_code")->uint64_or() == 0 &&
				 second->find("exit_code")->uint64_or() == 0);
	FLOWUI_CHECK(first->find("operation_status")->string_or() == "accepted");
	FLOWUI_CHECK(first->find("job_id")->string_or() == second->find("job_id")->string_or());
	setup_gate.store(true);
	const auto baseline = finish(application, first->find("job_id")->string_or());
	FLOWUI_CHECK(baseline.find("verdict")->string_or() == "pass" && counters[0].setup_calls == 1 &&
				 counters[0].cleanup_calls == 1);
	const auto failing = run(service, request("service.step_failure", "step-assertion-fails"));
	const auto failure_report = finish(application, failing.find("job_id")->string_or());
	FLOWUI_CHECK(failure_report.find("verdict")->string_or() == "fail");
	FLOWUI_CHECK(failure_report.find("coverage")->find("failed")->uint64_or() == 1);
	FLOWUI_CHECK(counters[1].cleanup_calls == 1);
	const auto pending = run(service, request("service.pending", "descriptor-deadline"));
	const auto timeout_report = finish(application, pending.find("job_id")->string_or());
	FLOWUI_CHECK(timeout_report.find("execution_status")->string_or() == "timeout");
	FLOWUI_CHECK(timeout_report.find("verdict")->string_or() == "inconclusive");
	FLOWUI_CHECK(counters[2].cleanup_calls == 1);

	for (const bool required : {false, true}) {
		auto checkpoint_request =
			request("service.pass", required ? "required-missing-image" : "optional-missing-image");
		checkpoint_request.set("evidence",
							   Json::make_object({{"policy", "checkpoints_and_failure"}}));
		checkpoint_request.find("cases")->array.front().set(
			"checkpoints", Json::make_array({Json::make_object(
							   {{"checkpoint_id", "unavailable_frame"},
								{"trigger", "case_verified"},
								{"evidence", "image"},
								{"required", required},
								{"window_id", decimal(application.mainWindowId())}})}));
		const auto accepted = run(service, checkpoint_request);
		const auto report = finish(application, accepted.find("job_id")->string_or());
		FLOWUI_CHECK(report.find("verdict")->string_or() == (required ? "inconclusive" : "pass"));
		FLOWUI_CHECK(report.find("coverage")->find("required")->uint64_or() == (required ? 2 : 1));
		FLOWUI_CHECK(report.find("coverage")->find("unavailable")->uint64_or() ==
					 (required ? 1 : 0));
	}
	const auto nested = run(service, request("service.nested_input", "cancel-nested-input"));
	bool held_input_delivered = false;
	const auto input_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (std::chrono::steady_clock::now() < input_deadline) {
		frame(application);
		if (!nested_state.child_job.empty() &&
			application.ui().getCurrentFrameInput().mouseDown[0]) {
			held_input_delivered = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	FLOWUI_CHECK(held_input_delivered);
	const auto child_before_cancel = service.query_interaction(nested_state.child_job);
	FLOWUI_CHECK(child_before_cancel && child_before_cancel->coverage.consumed > 0 &&
				 !child_before_cancel->cleanup_complete);
	const auto cancellation = command(service, "verification.cancel",
									  Json::make_object({{"job_id", *nested.find("job_id")}}));
	FLOWUI_CHECK(cancellation.find("result")->find("cancellation_requested")->bool_or());
	const auto canceled_report = finish(application, nested.find("job_id")->string_or());
	FLOWUI_CHECK(canceled_report.find("execution_status")->string_or() == "canceled");
	FLOWUI_CHECK(canceled_report.find("cleanup")->find("completed")->bool_or() &&
				 canceled_report.find("cleanup")->find("fixture_usable")->bool_or());
	FLOWUI_CHECK(nested_state.cleanup_observed_released_input);
	const auto child_after_cancel = service.query_interaction(nested_state.child_job);
	FLOWUI_CHECK(child_after_cancel && child_after_cancel->cleanup_complete &&
				 !application.ui().getCurrentFrameInput().mouseDown[0]);
	const auto recovered = run(service, request("service.pass", "after-nested-cancellation"));
	FLOWUI_CHECK(
		finish(application, recovered.find("job_id")->string_or()).find("verdict")->string_or() ==
		"pass");
	for (std::size_t repetition = 0; repetition < 34; ++repetition) {
		const auto retained =
			run(service, request("service.pass", "terminal-history-" + std::to_string(repetition)));
		FLOWUI_CHECK(finish(application, retained.find("job_id")->string_or())
						 .find("verdict")
						 ->string_or() == "pass");
	}
	const auto expired_response = service.submit_command(
		AgenticCommandRequest{Json::make_object({{"request", duplicate_request}}),
							  "expired-suite-retry", "verification.run"});
	FLOWUI_CHECK(expired_response);
	const auto& expired_retry = *expired_response;
	FLOWUI_CHECK(expired_retry.find("exit_code")->uint64_or() == 6);
	FLOWUI_CHECK(expired_retry.find("error")->find("code")->string_or() == "result_expired");
	FLOWUI_CHECK(expired_retry.find("job_id")->string_or() == first->find("job_id")->string_or());
	FLOWUI_CHECK(expired_retry.find("next")->find("operation")->string_or() ==
				 "verification.report");
	const auto* recovery_arguments = expired_retry.find("next")->find("arguments");
	FLOWUI_CHECK(recovery_arguments && recovery_arguments->find("job_id")->string_or() ==
										   first->find("job_id")->string_or());
	AgenticCommandContext recovery_context;
	recovery_context.store_root = store;
	recovery_context.live_handler = [&](const AgenticCommandRequest& query) {
		return service.submit_command(query);
	};
	const auto recovered_report = execute_agentic_command(
		recovery_context, AgenticCommandRequest{*recovery_arguments, "expired-report-recovery",
												"verification.report"});
	FLOWUI_CHECK(recovered_report.exit_code == 0);
	FLOWUI_CHECK(recovered_report.envelope.find("result")->find("job_id")->string_or() ==
				 first->find("job_id")->string_or());
	FLOWUI_CHECK(recovered_report.envelope.find("result")->find("verdict")->string_or() == "pass");
	const auto dirty = run(service, request("service.cleanup_failure", "dirty-fixture"));
	const auto dirty_report = finish(application, dirty.find("job_id")->string_or());
	FLOWUI_CHECK(dirty_report.find("verdict")->string_or() == "inconclusive");
	FLOWUI_CHECK(!dirty_report.find("cleanup")->find("fixture_usable")->bool_or());
	const auto rejected = service.submit_command(AgenticCommandRequest{
		Json::make_object({{"request", request("service.pass", "after-dirty-fixture")}}),
		"dirty-fixture-rejection", "verification.run"});
	FLOWUI_CHECK(rejected && rejected->find("exit_code")->uint64_or() != 0 &&
				 rejected->find("error")->find("code")->string_or() == "dirty_fixture");
}
} // namespace

int main() {
#if !defined(_WIN32)
	if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY"))
		return 77;
#endif
	TestDirectory directory;
	FlowUi::test::Runner runner;
	runner.run(
		"verification assertions, deadlines, duplicate admission, checkpoint coverage and cleanup",
		[&] { verify_service(directory.root); });
	return runner.finish();
}
