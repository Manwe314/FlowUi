#include "FlowUi/Flow.hpp"
#include "devSystems/devAgenticWorkflow/AgenticWorkflow.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argument_count, char** arguments) {
	using namespace FlowUi;
	using namespace FlowUi::devSystems::agentic;
	static_assert(FLOW_UI_AGENTIC_WORKFLOW == 1);
	if (argument_count != 2)
		return 2;
	std::uint64_t measured_steps = 0;
	AppConfig configuration;
	configuration.window.title = "Installed FlowUi agentic consumer";
	configuration.window.width = 160;
	configuration.window.height = 120;
	configuration.vk.enableValidation = false;
	configuration.vk.enableDebugUtils = false;
	configuration.resources.roots.emplace_back(FLOWUI_CONSUMER_RESOURCES);
	configuration.resources.use_build_directory = false;
	configuration.agentic_workflow.enabled = true;
	configuration.agentic_workflow.input_control_enabled = true;
	configuration.agentic_workflow.store_path = arguments[1];
	configuration.agentic_workflow.application_id = "flowui.agentic.installed_consumer";
	configuration.agentic_workflow.application_revision = "consumer-fixture-v2";
	configuration.agentic_workflow.provenance.set(
		"application_build_profile",
		Json::make_object({{"compiler", FLOWUI_CONSUMER_COMPILER},
						   {"configuration", FLOWUI_CONSUMER_BUILD_TYPE},
						   {"flags", FLOWUI_CONSUMER_FLAGS}}));
	try {
		auto application = makeApplication(configuration);
		AgenticScenario scenario;
		scenario.descriptor.scenario_id = "consumer.solid_frame";
		scenario.descriptor.revision = "1";
		scenario.descriptor.digest = "sha256:" + sha256("solid-frame-160x120-rgb-20-40-60-v1");
		scenario.descriptor.fixture_id = "solid-frame-160x120";
		scenario.descriptor.seed = 17;
		scenario.setup =
			[](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
			return context.application->hasWindow(context.application->mainWindowId())
					   ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
					   : unexpectedError(
							 makeError(ErrorCode::AgenticUnavailable, ErrorSite::AgenticScenario));
		};
		scenario.step =
			[&](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
			if (context.seed != 17)
				return unexpectedError(
					makeError(ErrorCode::AgenticInvalidRequest, ErrorSite::AgenticScenario));
			++measured_steps;
			return AgenticScenarioProgress::complete;
		};
		scenario.verify = [&](const AgenticScenarioContext&) -> Result<AgenticScenarioProgress> {
			return measured_steps
					   ? Result<AgenticScenarioProgress>(AgenticScenarioProgress::complete)
					   : unexpectedError(makeError(ErrorCode::AgenticIncompleteCapture,
												   ErrorSite::AgenticScenario));
		};
		scenario.cleanup =
			[&](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
			if (!context.application->hasWindow(context.application->mainWindowId()))
				return unexpectedError(
					makeError(ErrorCode::AgenticUnavailable, ErrorSite::AgenticScenario));
			measured_steps = 0;
			return AgenticScenarioProgress::complete;
		};
		auto registered = application.agentic_workflow().register_scenario(std::move(scenario));
		if (!registered)
			return 3;
		auto capabilities = application.agentic_workflow().capabilities();
		if (!capabilities)
			return 3;
		AgenticCaptureRequest request;
		request.window_ids = {application.mainWindowId()};
		request.scenario_id = "consumer.solid_frame";
		request.runtime_diagnostics = SectionRequirement::omit;
		request.limits = capabilities->limits;
		request.measurement_ticks = 8;
		request.warmup_ticks = 2;
		request.duration_ns = 1'000'000'000;
		AgenticInteractionPlan input_plan;
		input_plan.window_id = application.mainWindowId();
		AgenticInteractionSpan neutral_span;
		neutral_span.count = 2;
		input_plan.spans.emplace_back(std::move(neutral_span));
		auto compiled = compile_interaction_plan(input_plan);
		if (!compiled || compiled->frame_count != 2)
			return 4;
		auto job = application.agentic_workflow().start_interaction(to_json(input_plan),
																	"installed-neutral-input");
		if (!job)
			return 4;
		bool interaction_verified = false;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		while (std::chrono::steady_clock::now() < deadline) {
			if (!application.beginFrame())
				return 4;
			Clay_ElementDeclaration background{};
			background.backgroundColor = {20, 40, 60, 255};
			background.layout.sizing = {.width = CLAY_SIZING_GROW(0),
										.height = CLAY_SIZING_GROW(0)};
			CLAY(CLAY_ID("installed-consumer-root"), background) {}
			if (!application.endFrame() || !application.drawFrame())
				return 4;
			auto status = application.agentic_workflow().query_job(*job);
			if (!status)
				return 4;
			if (status->phase == JobPhase::failed) {
				std::cerr << status->reason << '\n';
				return 4;
			}
			if (status->phase == JobPhase::sealed) {
				if (!interaction_verified) {
					const auto result = application.agentic_workflow().query_interaction(*job);
					if (!result || !result->execution_succeeded || !result->cleanup_complete ||
						result->coverage.consumed != 2)
						return 6;
					job = application.agentic_workflow().request_capture(request);
					if (!job)
						return 4;
					interaction_verified = true;
					continue;
				}
				auto manifest = read_manifest(status->manifest_path);
				if (!manifest || manifest->partial)
					return 6;
				std::cout << manifest->capture_id << '\n';
				return 0;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return 5;
	} catch (const std::exception& exception) {
		std::cerr << exception.what() << '\n';
		return 4;
	}
}
