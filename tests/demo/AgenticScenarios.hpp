#pragma once

// Included beside the demo's private Demo/DemoState definitions.
#if FLOW_UI_AGENTIC_WORKFLOW

namespace agentic = FlowUi::devSystems::agentic;

/* Application-owned scenario state; it outlives the App and its registered callbacks. */
struct DemoAgenticHarness {
	std::optional<DemoState> previous_state;
	DemoParameters previous_parameters;
	std::string previous_variant;
	FlowElementID state_id{};
	std::uint64_t measured_steps = 0;
};

enum class DemoAgenticFixture : std::uint8_t { gallery, writing, theme_matrix };

template <typename Function>
[[nodiscard]] Result<agentic::AgenticScenarioProgress>
run_demo_scenario_step(Function&& function) noexcept {
	try {
		return function();
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticScenario));
	}
}

[[nodiscard]] DemoState* scenario_demo_state(App& application, DemoAgenticHarness& harness) {
	if (!harness.state_id) {
		const auto& tree = application.ui().devTreeSnapshot();
		for (const auto& node : tree.flow.nodes)
			if (node.definition == Demo::definitionId) {
				harness.state_id.value = node.instance.value;
				break;
			}
	}
	return harness.state_id ? application.elements().getStatePointer(
								  kDemo, application.mainWindowId(), harness.state_id)
							: nullptr;
}

[[nodiscard]] std::string active_demo_theme(App& application) {
	struct ActiveVariant {
		std::string name;
		bool failed = false;
	} active;
	application.themes().visitDevCatalogueThemes(
		&active, [](void* owner, const ThemeManager::DevThemePayloadView& theme) noexcept {
			auto& selected = *static_cast<ActiveVariant*>(owner);
			if (theme.active && theme.type == FlowUi::detail::typeHash<FSELTheme>()) {
				try {
					selected.name = theme.variant;
				} catch (...) {
					selected.failed = true;
					return false;
				}
			}
			return true;
		});
	if (active.failed)
		throw FlowUiException(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticScenario));
	return active.name;
}

[[nodiscard]] agentic::AgenticScenario make_demo_scenario(DemoAgenticFixture fixture,
														  DemoParameters& parameters,
														  DemoAgenticHarness& harness) {
	agentic::AgenticScenario scenario;
	auto& descriptor = scenario.descriptor;
	descriptor.scenario_id = fixture == DemoAgenticFixture::gallery ? "demo.gallery.steady"
							 : fixture == DemoAgenticFixture::writing
								 ? "demo.writing.large_document"
								 : "demo.theme_matrix";
	descriptor.revision = "1";
	descriptor.fixture_id = fixture == DemoAgenticFixture::gallery ? "gallery-default-v1"
							: fixture == DemoAgenticFixture::writing
								? "writing-128-lines-v1"
								: "gallery-writing-two-theme-v1";
	descriptor.digest = "sha256:" + agentic::sha256(descriptor.scenario_id + ":" +
													descriptor.fixture_id + ":seed17");
	descriptor.description = fixture == DemoAgenticFixture::theme_matrix
								 ? "Visual state matrix: Gallery/default, Writing/default, "
								   "Gallery/ocean, Writing/ocean; at least four measured frames."
								 : "Deterministic demo workload with setup/readiness, measured "
								   "steps, assertions and state restoration.";
	descriptor.seed = 17u;
	descriptor.deadline_ns = 120000000000ull;
	descriptor.mutating = true;
	descriptor.surfaces = agentic::Json::make_array(
		{agentic::Json::make_object(
			 {{"label", "Gallery"},
			  {"flow_definition_id", agentic::decimal(Demo::definitionId.value)},
			  {"source", "tests/demo/main.cpp"},
			  {"symbol", "drawGallery"}}),
		 agentic::Json::make_object({{"label", "Writing Studio"},
									 {"source", "tests/demo/main.cpp"},
									 {"symbol", "drawWritingStudio"}})});
	descriptor.assertions = agentic::Json::make_array(
		{agentic::Json::make_object({{"id", "fixture_seed"}, {"expected", "17"}}),
		 agentic::Json::make_object({{"id", "state_retained"}, {"expected", true}}),
		 agentic::Json::make_object(
			 {{"id", "minimum_state_steps"},
			  {"expected",
			   agentic::Json::number(fixture == DemoAgenticFixture::theme_matrix ? 4u : 1u)}})});
	scenario.setup = [&, fixture](const agentic::AgenticScenarioContext& context) {
		return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
			if (!context.application || context.seed != 17u)
				return unexpectedError(
					makeError(ErrorCode::AgenticInvalidRequest, ErrorSite::AgenticScenario));
			auto* state = scenario_demo_state(*context.application, harness);
			if (!state)
				return agentic::AgenticScenarioProgress::pending;
			if (!harness.previous_state) {
				harness.previous_state = *state;
				harness.previous_parameters = parameters;
				harness.previous_variant = active_demo_theme(*context.application);
				harness.measured_steps = 0;
				*state = DemoState{};
				state->page = fixture == DemoAgenticFixture::writing ? DemoPage::WritingStudio
																	 : DemoPage::Gallery;
				if (fixture != DemoAgenticFixture::gallery) {
					state->documentTitle = "Agentic deterministic writing fixture";
					state->document.clear();
					state->document.reserve(16384u);
					for (unsigned line = 0; line < 128u; ++line)
						state->document += "Record " + std::to_string(line) +
										   ": seed 17; stable text, wrapping and editor layout.\n";
				}
				requireStatus(
					context.application->themes().setActiveVariant<FSELTheme>("agentic-default"));
			}
			return agentic::AgenticScenarioProgress::complete;
		});
	};
	scenario.step = [&, fixture](const agentic::AgenticScenarioContext& context) {
		return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
			auto* state =
				context.application ? scenario_demo_state(*context.application, harness) : nullptr;
			if (!state || !harness.previous_state)
				return unexpectedError(
					makeError(ErrorCode::AgenticStaleGeneration, ErrorSite::AgenticScenario));
			if (fixture == DemoAgenticFixture::theme_matrix) {
				const std::uint64_t matrix_state = harness.measured_steps % 4u;
				state->page = matrix_state % 2u ? DemoPage::WritingStudio : DemoPage::Gallery;
				state->surfaceSelected = matrix_state >= 2u;
				state->checkboxChecked = matrix_state >= 2u;
				state->editorSoftWrap = matrix_state < 2u;
				parameters.backgroundColor =
					matrix_state < 2u ? Flow_Color("#0c111bff") : Flow_Color("#081b26ff");
				requireStatus(context.application->themes().setActiveVariant<FSELTheme>(
					matrix_state < 2u ? "agentic-default" : "agentic-ocean"));
			}
            agentic::AgenticScenarioAction observed;
            observed.name = fixture == DemoAgenticFixture::theme_matrix ? "select_theme_state" : "observe_steady_fixture";
            observed.target = state->page == DemoPage::WritingStudio ? "Writing Studio" : "Gallery";
            observed.arguments = agentic::Json::make_object({{"theme", active_demo_theme(*context.application)},
                {"step", agentic::decimal(harness.measured_steps)}, {"selected", state->surfaceSelected},
                {"soft_wrap", state->editorSoftWrap}});
            observed.state_revision = context.application->themes().devRevision();
            requireStatus(context.record_action(observed));
			++harness.measured_steps;
			return agentic::AgenticScenarioProgress::complete;
		});
	};
	scenario.verify = [&, fixture](const agentic::AgenticScenarioContext& context) {
		return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
			const auto* state =
				context.application ? scenario_demo_state(*context.application, harness) : nullptr;
			const auto record_assertion = [&](std::string_view identity, agentic::Json expected,
											  agentic::Json observed, bool passed) {
				agentic::AgenticScenarioAction assertion;
				assertion.name = identity;
				assertion.target = "DemoState";
				assertion.assertion = true;
				assertion.passed = passed;
				assertion.arguments = agentic::Json::make_object(
					{{"expected", std::move(expected)}, {"observed", std::move(observed)}});
				if (context.application) {
					const auto& tree = context.application->ui().devTreeSnapshot();
					assertion.state_revision = tree.generation;
				}
				requireStatus(context.record_action(assertion));
			};
			const auto minimum_steps = fixture == DemoAgenticFixture::theme_matrix ? 4u : 1u;
			const bool retained = state && harness.previous_state.has_value();
			const bool valid_fixture =
				state &&
				(fixture != DemoAgenticFixture::gallery || state->page == DemoPage::Gallery) &&
				(fixture != DemoAgenticFixture::writing ||
				 (state->page == DemoPage::WritingStudio &&
				  std::count(state->document.begin(), state->document.end(), '\n') == 128));
			record_assertion("fixture_seed", "17", agentic::decimal(context.seed),
							 context.seed == 17u);
			record_assertion("state_retained", true, retained && valid_fixture,
							 retained && valid_fixture);
			record_assertion("minimum_state_steps", agentic::Json::number(minimum_steps),
							 agentic::Json::number(harness.measured_steps),
							 harness.measured_steps >= minimum_steps);
			if (!retained || !valid_fixture || harness.measured_steps < minimum_steps)
				return unexpectedError(
					makeError(ErrorCode::AgenticIncompleteCapture, ErrorSite::AgenticScenario));
			return agentic::AgenticScenarioProgress::complete;
		});
	};
	scenario.cleanup = [&](const agentic::AgenticScenarioContext& context) {
		return run_demo_scenario_step([&]() -> Result<agentic::AgenticScenarioProgress> {
			if (harness.previous_state && context.application) {
				auto* state = scenario_demo_state(*context.application, harness);
				if (!state)
					return unexpectedError(
						makeError(ErrorCode::AgenticStaleGeneration, ErrorSite::AgenticScenario));
				*state = std::move(*harness.previous_state);
				parameters = harness.previous_parameters;
				if (!harness.previous_variant.empty())
					requireStatus(context.application->themes().setActiveVariant<FSELTheme>(
						harness.previous_variant));
				harness.previous_state.reset();
			}
			return agentic::AgenticScenarioProgress::complete;
		});
	};
	return scenario;
}

[[nodiscard]] Status register_demo_agentic_scenarios(App& application, DemoParameters& parameters,
													 DemoAgenticHarness& harness) noexcept {
	try {
		const FSELTheme original = application.themes().getActiveTheme<FSELTheme>();
		Status registered = application.themes().registerTheme("agentic-default", original, false);
		if (!registered)
			return registered;
		auto ocean = original;
		ocean.buttonTheme.idle.backgroundColor = Flow_Color("#12374aff");
		ocean.buttonTheme.hovered.backgroundColor = Flow_Color("#18526aff");
		ocean.buttonTheme.pressed.backgroundColor = Flow_Color("#087c96ff");
		ocean.textInputTheme.focused.borderColor = Flow_Color("#44d6e8ff");
		ocean.selectableSurfaceTheme.selected.backgroundColor = Flow_Color("#124e65ff");
		registered = application.themes().registerTheme("agentic-ocean", ocean, false);
		if (!registered)
			return registered;
		for (const auto fixture : {DemoAgenticFixture::gallery, DemoAgenticFixture::writing,
								   DemoAgenticFixture::theme_matrix}) {
			registered = application.agentic_workflow().register_scenario(
				make_demo_scenario(fixture, parameters, harness));
			if (!registered)
				return registered;
		}
		return {};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticScenario));
	}
}

#endif
