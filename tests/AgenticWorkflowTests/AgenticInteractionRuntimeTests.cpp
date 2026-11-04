#include "FlowUi/Flow.hpp"
#include "TestHarness.hpp"
#include "devSystems/devAgenticWorkflow/AgenticWorkflow.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace {
struct Fixture {
	std::vector<FrameInput> inputs;
	std::uint64_t presses = 0, releases = 0, held = 0;
	void frame(App& application) {
		FLOWUI_CHECK(application.beginFrame());
		const auto& previous = application.ui().getPreviousFramesInteraction();
		const auto target = CLAY_ID("interaction_raw_target");
		if (previous.isPressed(target))
			++presses;
		if (previous.isReleased(target))
			++releases;
		if (previous.isHeld(target))
			++held;
		inputs.emplace_back(application.ui().getCurrentFrameInput());
		Clay_ElementDeclaration declaration{};
		declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		declaration.backgroundColor = {20, 40, 60, 255};
		CLAY(CLAY_ID("interaction_raw_target"), declaration) {}
		FLOWUI_CHECK(application.endFrame());
		FLOWUI_CHECK(application.drawFrame());
	}
};
Json command(AgenticWorkflowService& service, std::string operation, Json arguments = {}) {
	if (arguments.kind == Json::Kind::null)
		arguments = Json::make_object();
	auto response = service.submit_command(
		AgenticCommandRequest{std::move(arguments), "interaction-test", std::move(operation)});
	FLOWUI_CHECK(response);
	if (response->find("exit_code") && response->find("exit_code")->uint64_or() != 0)
		std::cerr << write_json(*response) << '\n';
	FLOWUI_CHECK(!response->find("exit_code") || response->find("exit_code")->uint64_or() == 0);
	FLOWUI_CHECK(response->find("result"));
	return *response->find("result");
}
std::string start(AgenticWorkflowService& service, const AgenticInteractionPlan& plan,
				  std::string_view key) {
	const auto response =
		command(service, "interaction.start",
				Json::make_object({{"plan", to_json(plan, true)}, {"idempotency_key", key}}));
	FLOWUI_CHECK(response.find("job_id"));
	return std::string(response.find("job_id")->string_or());
}
Json finish(App& application, Fixture& fixture, std::string_view job_id) {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (std::chrono::steady_clock::now() < deadline) {
		fixture.frame(application);
		const auto status = command(application.agentic_workflow(), "job.status",
									Json::make_object({{"job_id", job_id}}));
		const auto phase = status.find("phase");
		if (phase && phase->string_or() == "failed") {
			std::cerr << write_json(status) << '\n';
			FLOWUI_CHECK(false);
		}
		if (phase && phase->string_or() == "sealed") {
			const auto result = command(application.agentic_workflow(), "interaction.query",
										Json::make_object({{"job_id", job_id}}));
			FLOWUI_CHECK(result.find("cleanup_complete") &&
						 result.find("cleanup_complete")->bool_or());
			const auto path = status.find("manifest_path");
			FLOWUI_CHECK(path);
			const auto manifest = read_manifest(path->string_or());
			FLOWUI_CHECK(manifest);
			FLOWUI_CHECK(read_artifact_json(*manifest, "interaction_results"));
			return result;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	FLOWUI_CHECK(false);
	return {};
}
/** Drive the existing backend callback, retaining its ordinary InputQueue assessment semantics. */
void assess_native_key(App& application, int key, bool down) {
	auto* window = static_cast<GLFWwindow*>(application.nativeWindowHandle());
	FLOWUI_CHECK(window);
	const auto callback = glfwSetKeyCallback(window, nullptr);
	FLOWUI_CHECK(callback);
	glfwSetKeyCallback(window, callback);
	callback(window, key, 0, down ? GLFW_PRESS : GLFW_RELEASE, 0);
}
void verify_handoff_across_jobs(App& application, Fixture& fixture) {
	auto& service = application.agentic_workflow();
	auto runtime = service.interaction_runtime();
	FLOWUI_CHECK(runtime);
	AgenticInteractionPlan neutral_plan;
	neutral_plan.window_id = application.mainWindowId();
	AgenticInteractionSpan span;
	span.count = 4;
	neutral_plan.spans.emplace_back(span);
	assess_native_key(application, GLFW_KEY_A, true);
	const auto initial_job = start(service, neutral_plan, "handoff-initial-input");
	finish(application, fixture, initial_job);
	FLOWUI_CHECK((*runtime)->controls_window(application.mainWindowId()));
	FLOWUI_CHECK(!fixture.inputs.back().keyDown[GLFW_KEY_A]);
	const auto next_job = start(service, neutral_plan, "handoff-queued-input");
	// A queued capture has not selected any row: probing this native path creates no receipt.
	FrameInput assessed;
	assessed.keyDown[GLFW_KEY_A] = true;
	EvidenceFrameKey probe;
	probe.window_id = application.mainWindowId();
	probe.app_tick = application.ui().dev_completed_app_tick();
	probe.window_frame = application.ui().dev_completed_window_frame() + 1;
	FLOWUI_CHECK((*runtime)->select_input(probe, assessed, 1));
	FLOWUI_CHECK(!assessed.keyDown[GLFW_KEY_A]);
	finish(application, fixture, next_job);
	const auto recording =
		command(service, "interaction.record.start",
				Json::make_object({{"window_id", decimal(application.mainWindowId())},
								   {"max_frames", "4"},
								   {"deadline_ns", "5000000000"},
								   {"idempotency_key", "handoff-recorder"}}));
	const auto recorded_job = std::string(recording.find("job_id")->string_or());
	finish(application, fixture, recorded_job);
	const auto recorded =
		command(service, "interaction.query",
				Json::make_object(
					{{"job_id", recorded_job}, {"kind", "inputs"}, {"limit", Json::number(100)}}));
	FLOWUI_CHECK(recorded.find("records") && !recorded.find("records")->array.empty());
	for (const auto& record : recorded.find("records")->array) {
		const auto* effective = record.find("effective_input");
		FLOWUI_CHECK(effective);
		const auto* keys = effective->find("keys_down");
		FLOWUI_CHECK(keys);
		FLOWUI_CHECK(std::none_of(keys->array.begin(), keys->array.end(),
								  [](const Json& key) { return key.uint64_or() == GLFW_KEY_A; }));
	}
	assess_native_key(application, GLFW_KEY_A, false);
	fixture.frame(application);
	FLOWUI_CHECK(!(*runtime)->controls_window(application.mainWindowId()));
	assess_native_key(application, GLFW_KEY_A, true);
	fixture.frame(application);
	FLOWUI_CHECK(fixture.inputs.back().keyDown[GLFW_KEY_A]);
	assess_native_key(application, GLFW_KEY_A, false);
	fixture.frame(application);
}
void run() {
	const auto identifier = make_identifier("interaction_native_tests");
	FLOWUI_CHECK(identifier);
	const auto directory = std::filesystem::temp_directory_path() / *identifier;
	struct RemoveDirectory {
		std::filesystem::path path;
		~RemoveDirectory() {
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
	} cleanup{directory};
	AppConfig configuration;
	configuration.window.title = "FlowUi interaction replay verification";
	configuration.window.width = 200;
	configuration.window.height = 120;
	configuration.vk.enableValidation = false;
	configuration.vk.enableDebugUtils = false;
	configuration.dev.enabled = true;
	configuration.errors.observer.mode = ErrorReportingMode::SinkOnly;
	configuration.agentic_workflow.enabled = true;
	configuration.agentic_workflow.store_path = directory;
	configuration.agentic_workflow.application_id = "flowui.interaction.tests";
	configuration.agentic_workflow.input_control_enabled = true;
	configuration.agentic_workflow.action_control_enabled = true;
	std::string scenario_child;
	auto application = makeApplication(configuration);
	Fixture fixture;
	fixture.inputs.reserve(1024);
	auto& service = application.agentic_workflow();
	application.request_dev_interface_toggle();
	const auto malformed = service.submit_command(AgenticCommandRequest{
		Json::make_object({{"window_id", "bad"}}), "malformed-input", "interaction.targets"});
	FLOWUI_CHECK(malformed && malformed->find("exit_code")->uint64_or() == 2);
	for (int frame = 0; frame < 4; ++frame)
		fixture.frame(application);
	application.devTooling().inspect_interaction().toggle_primary_pick();
	AgenticInteractionPlan raw;
	raw.window_id = application.mainWindowId();
	raw.fixture_id = "raw-clay-root";
	raw.persistence = AgenticInteractionPersistence::retain_text;
	raw.persistence_declared = true;
	raw.record_observations = true;
	AgenticInteractionSpan initial;
	initial.count = 3;
	initial.input.pointer_x = 40;
	initial.input.pointer_y = 30;
	initial.input.pointer_inside = true;
	raw.spans.emplace_back(initial);
	auto down = initial;
	down.offset = 3;
	down.count = 2;
	down.input.buttons_down = {0};
	down.input.keys_down = {65, 341};
	down.input.text = {U'A', U'\U0001F642'};
	raw.spans.emplace_back(down);
	auto up = initial;
	up.offset = 5;
	up.count = 1;
	raw.spans.emplace_back(up);
	const auto job = start(service, raw, "raw-click-key-text");
	FLOWUI_CHECK(start(service, raw, "raw-click-key-text") == job);
	FLOWUI_CHECK(service.query_job(job));
	FLOWUI_CHECK(!service.request_capture({}));
	const auto raw_result = finish(application, fixture, job);
	FLOWUI_CHECK(raw_result.find("execution_succeeded")->bool_or());
	auto foreign_scope = raw;
	foreign_scope.run_id = "run_other_process";
	FLOWUI_CHECK(!service.start_interaction(to_json(foreign_scope, true), "raw-click-key-text"));
	FLOWUI_CHECK(service.query_interaction(job)->phase == AgenticInteractionPhase::completed);
	FLOWUI_CHECK(application.devTooling().inspect_interaction().picking());
	application.devTooling().inspect_interaction().cancel_pick();
	application.request_dev_interface_toggle();
	fixture.frame(application);
	FLOWUI_CHECK(fixture.presses >= 1 && fixture.releases >= 1 && fixture.held >= 1);
	bool text_observed = false, hold_transients_cleared = false;
	for (const auto& input : fixture.inputs) {
		if (input.text.size() == 2) {
			FLOWUI_CHECK(input.ctrl && input.keyDown[65] && input.mouseDown[0]);
			text_observed = true;
		}
		if (input.ctrl && input.text.empty() && input.mouseDown[0])
			hold_transients_cleared = true;
	}
	FLOWUI_CHECK(text_observed && hold_transients_cleared);
	const auto inputs = command(
		service, "interaction.query",
		Json::make_object({{"job_id", job}, {"kind", "inputs"}, {"limit", Json::number(100)}}));
	FLOWUI_CHECK(inputs.find("records") && inputs.find("records")->array.size() >= 6);
	AgenticInteractionPlan common;
	common.window_id = application.mainWindowId();
	common.mode = AgenticInteractionMode::common_controls;
	AgenticInteractionStep click;
	click.step_id = "raw-clay-click";
	click.operation = AgenticInteractionOperation::click;
	click.target = AgenticInteractionTarget{};
	click.target->kind = AgenticTargetKind::clay_name;
	click.target->clay_name = "interaction_raw_target";
	click.recipe.hold_frames = 2;
	click.expect = AgenticInteractionExpectation::target_hovered |
				   AgenticInteractionExpectation::target_pressed |
				   AgenticInteractionExpectation::target_held |
				   AgenticInteractionExpectation::target_released;
	common.steps.emplace_back(click);
	const auto common_job = start(service, common, "mechanical-click");
	const auto common_result = finish(application, fixture, common_job);
	FLOWUI_CHECK(common_result.find("execution_succeeded")->bool_or());
	FLOWUI_CHECK(common_result.find("assertion_verdict")->string_or() == "passed");
	auto separate_controls = common;
	separate_controls.steps.clear();
	for (const auto& [operation, expectation] :
		 std::array{std::pair{AgenticInteractionOperation::pointer_move,
							  AgenticInteractionExpectation::target_hovered},
					std::pair{AgenticInteractionOperation::pointer_down,
							  AgenticInteractionExpectation::target_pressed},
					std::pair{AgenticInteractionOperation::pointer_hold,
							  AgenticInteractionExpectation::target_held},
					std::pair{AgenticInteractionOperation::pointer_up,
							  AgenticInteractionExpectation::target_released}}) {
		AgenticInteractionStep step;
		step.operation = operation;
		step.target = click.target;
		step.expect = expectation;
		step.frames = 2;
		separate_controls.steps.emplace_back(std::move(step));
	}
	const auto separate_job = start(service, separate_controls, "separate-mechanical-controls");
	FLOWUI_CHECK(finish(application, fixture, separate_job).find("execution_succeeded")->bool_or());
	AgenticInteractionPlan dragging = common;
	dragging.steps.clear();
	AgenticInteractionStep drag;
	drag.operation = AgenticInteractionOperation::drag;
	drag.target = click.target;
	drag.path = {{60, 40}, {100, 60}};
	drag.frames = 4;
	drag.expect = AgenticInteractionExpectation::target_pressed |
				  AgenticInteractionExpectation::target_held |
				  AgenticInteractionExpectation::target_released;
	dragging.steps.emplace_back(drag);
	const auto drag_job = start(service, dragging, "resolved-target-drag-path");
	FLOWUI_CHECK(finish(application, fixture, drag_job).find("execution_succeeded")->bool_or());
	FLOWUI_CHECK(std::ranges::any_of(fixture.inputs, [](const auto& input) {
		return input.mouseDown[0] && input.mouseX == 80 && input.mouseY == 50;
	}));
	AgenticInteractionPlan missing_destination;
	missing_destination.window_id = application.mainWindowId();
	missing_destination.mode = AgenticInteractionMode::common_controls;
	AgenticInteractionStep retained_down;
	retained_down.operation = AgenticInteractionOperation::pointer_down;
	retained_down.target = AgenticInteractionTarget{};
	retained_down.target->point = {40, 30};
	missing_destination.steps.emplace_back(retained_down);
	AgenticInteractionStep waiting_drag;
	waiting_drag.operation = AgenticInteractionOperation::drag;
	waiting_drag.target = AgenticInteractionTarget{};
	waiting_drag.target->point = {80, 70};
	waiting_drag.destination = AgenticInteractionTarget{};
	waiting_drag.destination->kind = AgenticTargetKind::custom_region;
	waiting_drag.destination->custom_id = "missing-drag-destination";
	waiting_drag.deadline_ns = 100'000'000;
	missing_destination.steps.emplace_back(std::move(waiting_drag));
	const auto waiting_begin = fixture.inputs.size();
	const auto waiting_job = start(service, missing_destination, "missing-drag-destination");
	FLOWUI_CHECK(!finish(application, fixture, waiting_job).find("execution_succeeded")->bool_or());
	bool waiting_held = false;
	for (std::size_t frame = waiting_begin; frame < fixture.inputs.size(); ++frame)
		if (fixture.inputs[frame].mouseDown[0]) {
			waiting_held = true;
			FLOWUI_CHECK(fixture.inputs[frame].mouseX == 40 && fixture.inputs[frame].mouseY == 30);
		}
	FLOWUI_CHECK(waiting_held);
	const auto archived_inputs = command(service, "interaction.query",
										 Json::make_object({{"job_id", job}, {"kind", "inputs"}}));
	FLOWUI_CHECK(!archived_inputs.find("records")->array.empty());
	const auto filtered_inputs =
		command(service, "interaction.query",
				Json::make_object({{"job_id", job},
								   {"kind", "inputs"},
								   {"window_id", decimal(application.mainWindowId())},
								   {"offset", "3"}}));
	FLOWUI_CHECK(filtered_inputs.find("records")->array.size() == 1);
	FLOWUI_CHECK(filtered_inputs.find("records")->array.front().find("offset")->uint64_or() == 3);
	auto exact = raw;
	exact.clock = AgenticInteractionClock::exact_window_frames;
	exact.baseline = AgenticInteractionBaseline::require_neutral;
	exact.run_id = service.capabilities()->run_id;
	exact.first_window_frame = application.ui().dev_completed_window_frame() + 4;
	const auto exact_job = start(service, exact, "exact-window-frames");
	const auto exact_result = finish(application, fixture, exact_job);
	FLOWUI_CHECK(exact_result.find("execution_succeeded")->bool_or());
	FLOWUI_CHECK(exact_result.find("activation_frame")->find("window_frame")->uint64_or() ==
				 exact.first_window_frame);
	const auto expired = service.submit_command(AgenticCommandRequest{
		Json::make_object({{"plan", to_json(exact, true)}, {"idempotency_key", "expired-address"}}),
		"expired-address", "interaction.start"});
	FLOWUI_CHECK(expired && expired->find("exit_code")->uint64_or() == 6);
	auto unobserved = raw;
	unobserved.record_observations = false;
	const auto unobserved_job = start(service, unobserved, "raw-without-observation-copy");
	FLOWUI_CHECK(
		finish(application, fixture, unobserved_job).find("execution_succeeded")->bool_or());
	auto volatile_plan = raw;
	volatile_plan.persistence = AgenticInteractionPersistence::volatile_plan;
	const auto volatile_job = start(service, volatile_plan, "volatile-input-payload");
	const auto volatile_result = finish(application, fixture, volatile_job);
	FLOWUI_CHECK(volatile_result.find("coverage")->find("retained_inputs")->uint64_or() == 0);
	AgenticCaptureRequest cutoff_capture;
	cutoff_capture.limits = service.capabilities()->limits;
	cutoff_capture.performance = SectionRequirement::omit;
	cutoff_capture.runtime_diagnostics = SectionRequirement::omit;
	cutoff_capture.measurement_ticks = 3;
	cutoff_capture.duration_ns = 10'000'000'000;
	auto long_plan = raw;
	long_plan.spans.clear();
	auto long_hold = initial;
	long_hold.count = 100;
	long_hold.input.buttons_down = {0};
	long_plan.spans.emplace_back(long_hold);
	const auto cutoff = command(service, "interaction.start",
								Json::make_object({{"plan", to_json(long_plan, true)},
												   {"capture", to_json(cutoff_capture)},
												   {"idempotency_key", "short-capture-cutoff"}}));
	const auto cutoff_job = std::string(cutoff.find("job_id")->string_or());
	const auto cutoff_result = finish(application, fixture, cutoff_job);
	FLOWUI_CHECK(!cutoff_result.find("execution_succeeded")->bool_or());
	FLOWUI_CHECK(cutoff_result.find("cleanup_complete")->bool_or());
	FLOWUI_CHECK(!fixture.inputs.back().mouseDown[0]);
	auto canceled = raw;
	canceled.spans.clear();
	down.offset = 0;
	down.count = 100;
	down.input.text.clear();
	canceled.spans.emplace_back(down);
	const auto canceled_job = start(service, canceled, "cancel-held");
	for (int frame = 0; frame < 5; ++frame)
		fixture.frame(application);
	command(service, "job.cancel", Json::make_object({{"job_id", canceled_job}}));
	const auto canceled_result = finish(application, fixture, canceled_job);
	FLOWUI_CHECK(canceled_result.find("phase")->string_or() == "canceled");
	FLOWUI_CHECK(!fixture.inputs.back().mouseDown[0] && !fixture.inputs.back().ctrl);
	auto recorded = command(service, "interaction.record.start",
							Json::make_object({{"window_id", decimal(application.mainWindowId())},
											   {"max_frames", "5"},
											   {"include_text", false},
											   {"idempotency_key", "record-five"}}));
	const auto recording_job = std::string(recorded.find("job_id")->string_or());
	const auto recording_result = finish(application, fixture, recording_job);
	FLOWUI_CHECK(recording_result.find("execution_succeeded")->bool_or());
	const auto recorded_plan =
		command(service, "interaction.query",
				Json::make_object({{"job_id", recording_job}, {"kind", "plan"}}));
	FLOWUI_CHECK(interaction_plan_from_json(recorded_plan));
	std::uint64_t action_calls = 0;
	constexpr auto action_id = AppAction("interaction.test.action");
	const auto bound = application.actions().appActions().bind(
		action_id, [](std::uint64_t* calls) { ++*calls; }, &action_calls);
	FLOWUI_CHECK(bound);
	const auto runtime = service.interaction_runtime();
	FLOWUI_CHECK(runtime);
	AgenticActionDeclaration declaration;
	declaration.action_id = "fixture.action";
	declaration.revision = 1;
	FLOWUI_CHECK((*runtime)->actions().register_bound_action(std::move(declaration), *bound));
	auto action = command(service, "action.invoke",
						  Json::make_object({{"action_id", "fixture.action"},
											 {"revision", "1"},
											 {"arguments", Json::make_object()},
											 {"max_bytes", Json::number(1024)},
											 {"idempotency_key", "one-action"}}));
	const auto action_job = std::string(action.find("job_id")->string_or());
	const auto action_result = finish(application, fixture, action_job);
	FLOWUI_CHECK(action_result.find("execution_succeeded")->bool_or() && action_calls == 1);
	command(service, "action.invoke",
			Json::make_object({{"action_id", "fixture.action"},
							   {"revision", "1"},
							   {"arguments", Json::make_object()},
							   {"idempotency_key", "one-action"}}));
	FLOWUI_CHECK(action_calls == 1);
	const auto unknown_action =
		command(service, "action.invoke",
				Json::make_object({{"action_id", "fixture.missing"},
								   {"revision", "1"},
								   {"arguments", Json::make_object()},
								   {"idempotency_key", "missing-action-evidence"}}));
	const auto rejected_action =
		finish(application, fixture, unknown_action.find("job_id")->string_or());
	FLOWUI_CHECK(!rejected_action.find("execution_succeeded")->bool_or());
	FLOWUI_CHECK(rejected_action.find("action")->find("action_id")->string_or() ==
				 "fixture.missing");
	FLOWUI_CHECK(!rejected_action.find("action")->find("dispatched")->bool_or());
	AgenticScenario scenario;
	scenario.descriptor.scenario_id = "fixture.input_setup";
	scenario.descriptor.revision = "1";
	scenario.descriptor.digest = sha256("raw-input-setup-before-capture-v1");
	scenario.descriptor.fixture_id = "raw-clay-root";
	scenario.setup = [source = to_json(common, true), &scenario_child](
						 const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		auto& service = context.application->agentic_workflow();
		if (scenario_child.empty()) {
			auto child = service.start_interaction(source, "scenario-setup-input");
			if (!child)
				return unexpectedError(child.error());
			scenario_child = *child;
		}
		const auto result = service.query_interaction(scenario_child);
		if (!result)
			return unexpectedError(result.error());
		return result->phase == AgenticInteractionPhase::completed
				   ? AgenticScenarioProgress::complete
				   : AgenticScenarioProgress::pending;
	};
	scenario.step = [](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		return context.application->hasWindow(context.application->mainWindowId())
				   ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
				   : unexpectedError(agentic_error(AgenticError::unavailable));
	};
	scenario.verify =
		[&scenario_child](
			const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		const auto result =
			context.application->agentic_workflow().query_interaction(scenario_child);
		if (!result || !result->execution_succeeded)
			return unexpectedError(agentic_error(AgenticError::incomplete));
		return AgenticScenarioProgress::complete;
	};
	scenario.cleanup =
		[&scenario_child](const AgenticScenarioContext&) -> Result<AgenticScenarioProgress> {
		scenario_child.clear();
		return AgenticScenarioProgress::complete;
	};
	FLOWUI_CHECK(service.register_scenario(std::move(scenario)));
	AgenticCaptureRequest scenario_capture;
	scenario_capture.limits = service.capabilities()->limits;
	scenario_capture.performance = SectionRequirement::omit;
	scenario_capture.runtime_diagnostics = SectionRequirement::omit;
	scenario_capture.content.include_interaction_labels = true;
	scenario_capture.scenario_id = "fixture.input_setup";
	scenario_capture.measurement_ticks = 5;
	scenario_capture.duration_ns = 10'000'000'000;
	for (int repetition = 0; repetition < 2; ++repetition) {
		const auto scenario_job = service.request_capture(scenario_capture);
		FLOWUI_CHECK(scenario_job);
		const auto scenario_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		bool scenario_sealed = false;
		while (std::chrono::steady_clock::now() < scenario_deadline) {
			fixture.frame(application);
			const auto status = service.query_job(*scenario_job);
			FLOWUI_CHECK(status && status->phase != JobPhase::failed);
			if (status->phase == JobPhase::sealed) {
				const auto manifest = read_manifest(status->manifest_path);
				FLOWUI_CHECK(manifest && read_artifact_json(*manifest, "interaction_results"));
				const auto observations = read_artifact(*manifest, "interaction_observations");
				FLOWUI_CHECK(observations);
				const std::string_view observed_text(
					reinterpret_cast<const char*>(observations->data()), observations->size());
				FLOWUI_CHECK(observed_text.find("interaction_raw_target") !=
							 std::string_view::npos);
				scenario_sealed = true;
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (!scenario_sealed) {
			const auto status = service.query_job(*scenario_job);
			if (status)
				std::cerr << "Scenario parent: " << write_json(to_json(*status)) << '\n';
			const auto child = service.query_interaction(scenario_child);
			if (child)
				std::cerr << "Scenario child: " << write_json(to_json(*child)) << '\n';
		}
		FLOWUI_CHECK(scenario_sealed && scenario_child.empty());
	}
	verify_handoff_across_jobs(application, fixture);
}
} // namespace
int main() {
#if !defined(_WIN32)
	if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY"))
		return 77;
#endif
	FlowUi::test::Runner runner;
	runner.run("input replacement, raw Clay controls, cancellation, recording and action evidence",
			   run);
	return runner.finish();
}
