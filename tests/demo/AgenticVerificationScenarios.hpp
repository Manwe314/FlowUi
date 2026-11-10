#pragma once

#if FLOW_UI_AGENTIC_WORKFLOW && FLOW_UI_DEV_MODE

/* Resettable developer-window fixture. The declared precondition is a closed interface. */
struct DemoAgenticVerificationHarness {
	std::string click_job_id;
	std::string cleanup_job_id;
	std::uint64_t opening_frame = 0;
	WindowId interface_window = InvalidWindowId;
	bool opening_requested = false;
	bool closing_requested = false;
};

/* Resolve the developer window through application metadata, without assuming an identity. */
[[nodiscard]] WindowId demo_agentic_interface_window(const App& application) {
	for (const auto& window : application.devWindowSnapshot())
		if (window.title == "FlowUi Developer Interface")
			return window.id;
	return InvalidWindowId;
}

/* Compile a real pointer click against a unique completed developer-header element. */
[[nodiscard]] Result<agentic::Json> demo_agentic_interface_click(App& application, WindowId window,
																 std::string_view suffix) noexcept {
	try {
		const auto& tree = application.ui(window).devTreeSnapshot();
		std::uint64_t instance_id = 0;
		for (const auto& node : tree.flow.nodes) {
			if (node.definition != FSEL::RadioChoice::definitionId ||
				!tree.string(node.debugName).ends_with(suffix))
				continue;
			if (instance_id)
				return unexpectedError(
					agentic::agentic_error(agentic::AgenticError::invalid_request));
			instance_id = node.instance.value;
		}
		if (!instance_id)
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
		return agentic::Json::make_object(
			{{"mode", "common_controls"},
			 {"window_id", agentic::decimal(window)},
			 {"deadline_ns", "10000000000"},
			 {"limits", agentic::Json::make_object({{"maximum_frames", "64"},
													{"maximum_spans", "64"},
													{"maximum_steps", "8"},
													{"maximum_observations", "128"},
													{"maximum_artifact_bytes", "1048576"}})},
			 {"steps", agentic::Json::make_array({agentic::Json::make_object(
						   {{"operation", "click"},
							{"target", agentic::Json::make_object(
										   {{"flow_instance_id", agentic::decimal(instance_id)}})},
							{"expect", agentic::Json::make_array(
										   {"target_hovered", "target_pressed", "target_released",
											"layout_completed"})}})})}});
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticScenario));
	}
}

/* Exercise developer navigation through pointer routing and restore the closed fixture. */
[[nodiscard]] Status
register_demo_agentic_verification_scenario(App& application,
											DemoAgenticVerificationHarness& harness) noexcept {
	try {
		agentic::AgenticScenario scenario;
		scenario.descriptor.scenario_id = "demo.dev_interface.catalogue";
		scenario.descriptor.revision = "1";
		scenario.descriptor.fixture_id = "developer-interface-catalogue-v1";
		scenario.descriptor.digest =
			"sha256:" +
			agentic::sha256("demo.dev_interface.catalogue:developer-interface-catalogue-v1:seed17");
		scenario.descriptor.description =
			"Open the closed developer interface, click Catalogue, verify completed catalogue "
			"surfaces, then click Inspect and close the interface.";
		scenario.descriptor.seed = 17u;
		scenario.descriptor.deadline_ns = 30'000'000'000ull;
		scenario.descriptor.mutating = true;
		scenario.descriptor.assertions = agentic::Json::make_array(
			{agentic::Json::make_object({{"id", "catalogue_pointer_routing"}, {"expected", true}}),
			 agentic::Json::make_object(
				 {{"id", "catalogue_completed_layout"}, {"expected", true}})});
		scenario.setup = [&harness](const agentic::AgenticScenarioContext& context) {
			return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
				if (!context.application || context.seed != 17u)
					return unexpectedError(
						agentic::agentic_error(agentic::AgenticError::invalid_request));
				auto& application = *context.application;
				if (!harness.opening_requested) {
					harness = DemoAgenticVerificationHarness{};
					harness.interface_window = demo_agentic_interface_window(application);
					if (harness.interface_window != InvalidWindowId)
						harness.opening_frame =
							application.ui(harness.interface_window).dev_completed_window_frame();
					harness.opening_requested = true;
					application.request_dev_interface_toggle();
					return agentic::AgenticScenarioProgress::pending;
				}
				harness.interface_window = demo_agentic_interface_window(application);
				if (harness.interface_window == InvalidWindowId ||
					application.ui(harness.interface_window).dev_completed_window_frame() <=
						harness.opening_frame)
					return agentic::AgenticScenarioProgress::pending;
				return agentic::AgenticScenarioProgress::complete;
			});
		};
		scenario.step = [&harness](const agentic::AgenticScenarioContext& context) {
			return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
				auto& application = *context.application;
				if (harness.click_job_id.empty()) {
					auto plan = demo_agentic_interface_click(application, harness.interface_window,
															 "/tab-catalogue");
					if (!plan)
						return unexpectedError(plan.error());
					auto started = application.agentic_workflow().start_interaction(*plan);
					if (!started)
						return unexpectedError(started.error());
					harness.click_job_id = *started;
				}
				const auto result =
					application.agentic_workflow().query_interaction(harness.click_job_id);
				if (!result)
					return unexpectedError(result.error());
				if (result->phase == agentic::AgenticInteractionPhase::failed ||
					result->phase == agentic::AgenticInteractionPhase::canceled)
					return unexpectedError(
						agentic::agentic_error(agentic::AgenticError::incomplete));
				return result->phase == agentic::AgenticInteractionPhase::completed
						   ? agentic::AgenticScenarioProgress::complete
						   : agentic::AgenticScenarioProgress::pending;
			});
		};
		scenario.verify = [&harness](const agentic::AgenticScenarioContext& context) {
			return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
				auto& application = *context.application;
				const auto result =
					application.agentic_workflow().query_interaction(harness.click_job_id);
				if (!result)
					return unexpectedError(result.error());
				const auto& tree = application.ui(harness.interface_window).devTreeSnapshot();
				const auto contains_definition = [&](FlowDefinitionID identity) {
					return std::ranges::any_of(tree.flow.nodes, [&](const auto& node) {
						return node.definition == identity;
					});
				};
				const bool complete_layout =
					contains_definition(DefinitionID("flowui.dev_interface.catalogue.selector")) &&
					contains_definition(DefinitionID("flowui.dev_interface.catalogue.workbench"));
				const auto capabilities = application.agentic_workflow().capabilities();
				if (!capabilities)
					return unexpectedError(capabilities.error());
				for (const auto& [identity, passed] :
					 {std::pair{"catalogue_pointer_routing",
								result->execution_succeeded &&
									result->assertion_verdict ==
										agentic::AgenticInteractionVerdict::passed},
					  std::pair{"catalogue_completed_layout", complete_layout}}) {
					agentic::AgenticScenarioAction assertion;
					assertion.name = identity;
					assertion.target = "Developer Interface Catalogue";
					assertion.assertion = true;
					assertion.passed = passed;
					assertion.state_revision = tree.generation;
					assertion.observed_frame =
						agentic::EvidenceFrameKey{capabilities->run_id, tree.app_tick,
												  tree.frameNumber, harness.interface_window};
					assertion.arguments =
						agentic::Json::make_object({{"expected", true}, {"observed", passed}});
					requireStatus(context.record_action(assertion));
				}
				return agentic::AgenticScenarioProgress::complete;
			});
		};
		scenario.cleanup = [&harness](const agentic::AgenticScenarioContext& context) {
			return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
				if (!harness.opening_requested)
					return agentic::AgenticScenarioProgress::complete;
				auto& application = *context.application;
				if (harness.interface_window == InvalidWindowId) {
					harness = DemoAgenticVerificationHarness{};
					return agentic::AgenticScenarioProgress::complete;
				}
				if (harness.cleanup_job_id.empty()) {
					auto plan = demo_agentic_interface_click(application, harness.interface_window,
															 "/tab-inspect");
					if (!plan)
						return unexpectedError(plan.error());
					auto started = application.agentic_workflow().start_interaction(*plan);
					if (!started)
						return unexpectedError(started.error());
					harness.cleanup_job_id = *started;
				}
				const auto result =
					application.agentic_workflow().query_interaction(harness.cleanup_job_id);
				if (!result)
					return unexpectedError(result.error());
				if (result->phase == agentic::AgenticInteractionPhase::failed ||
					result->phase == agentic::AgenticInteractionPhase::canceled)
					return unexpectedError(
						agentic::agentic_error(agentic::AgenticError::incomplete));
				if (result->phase != agentic::AgenticInteractionPhase::completed)
					return agentic::AgenticScenarioProgress::pending;
				if (!harness.closing_requested) {
					application.request_dev_interface_toggle();
					harness.closing_requested = true;
					return agentic::AgenticScenarioProgress::pending;
				}
				harness = DemoAgenticVerificationHarness{};
				return agentic::AgenticScenarioProgress::complete;
			});
		};
		return application.agentic_workflow().register_scenario(std::move(scenario));
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticScenario));
	}
}

#endif
