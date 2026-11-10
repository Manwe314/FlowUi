#pragma once

// Included beside the demo's private application and scenario state definitions.
#if FLOW_UI_AGENTIC_WORKFLOW

/* One stable application alias; its owned registration changes only with identity/geometry. */
struct DemoAgenticInteractionAlias {
	std::string_view target_id, flow_suffix, alternate_flow_suffix, label;
	FlowDefinitionID definition{};
	std::optional<agentic::AgenticResolvedInteractionTarget> previous;
	std::uint64_t revision = 0;
};
/* Borrowed callbacks are destroyed with App before these application-owned resources. */
struct DemoAgenticInteractionHarness {
	std::array<DemoAgenticInteractionAlias, 5> aliases{
		{{"demo.navigate", "/gallery/navigate", "/writing/navigate",
		  "Navigate between Gallery and Writing Studio", FlowUi::FSEL::Button::definitionId},
		 {"demo.reset_control", "/gallery/reset", "/writing/reset", "Reset demo control",
		  FlowUi::FSEL::Button::definitionId},
		 {"demo.primary_button",
		  "/gallery/left/buttons/built",
		  {},
		  "Gallery built button",
		  FlowUi::FSEL::Button::definitionId},
		 {"demo.quick_note",
		  "/gallery/left/text/input",
		  {},
		  "Gallery quick note text field",
		  FlowUi::FSEL::TextInput::definitionId},
		 {"demo.editor_body",
		  "/writing/document/body",
		  {},
		  "Writing Studio document body",
		  FlowUi::FSEL::TextArea::definitionId}}};
	bool input_enabled = false, actions_enabled = false, reset_available = false;
};

inline constexpr auto kAgenticResetDemo = AppAction("flowui.demo.agentic.reset");

[[nodiscard]] agentic::Json demo_agentic_state_result(App& application, const DemoState& state) {
	return agentic::Json::make_object(
		{{"page", state.page == DemoPage::Gallery ? "gallery" : "writing"},
		 {"theme", active_demo_theme(application)},
		 {"editor_soft_wrap", state.editorSoftWrap},
		 {"window_id", agentic::decimal(application.mainWindowId())},
		 {"completed_window_frame",
		  agentic::decimal(application.ui().dev_completed_window_frame())}});
}

void reset_demo_agentic_state(App* application, DemoAgenticHarness* harness) {
	auto* state = scenario_demo_state(*application, *harness);
	if (!state)
		throw FlowUiException(agentic::agentic_error(agentic::AgenticError::unavailable));
	*state = DemoState{};
}

/* Explicit application exports, independent from development catalogue discovery. */
[[nodiscard]] Status
register_demo_agentic_interactions(App& application, DemoAgenticHarness& scenario_harness,
								   DemoAgenticInteractionHarness& interaction_harness,
								   bool input_enabled, bool actions_enabled) noexcept {
	try {
		interaction_harness.input_enabled = input_enabled;
		interaction_harness.actions_enabled = actions_enabled;
		if (!input_enabled && !actions_enabled)
			return {};
		auto runtime = application.agentic_workflow().interaction_runtime();
		if (!runtime)
			return unexpectedError(runtime.error());
		if (!actions_enabled)
			return {};
		auto bound = application.actions().appActions().bind(
			kAgenticResetDemo, &reset_demo_agentic_state, &application, &scenario_harness);
		if (!bound)
			return unexpectedError(bound.error());
		if (!application.actions().appActions().setAvailability(kAgenticResetDemo,
																{.enabled = false}))
			return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
		agentic::AgenticActionDeclaration reset;
		reset.action_id = "demo.reset";
		reset.description = "Reset the real demo state using a parameterless bound App action. "
							"Completion establishes dispatch, without claiming a rendered frame.";
		auto registered = (*runtime)->actions().register_bound_action(std::move(reset), *bound);
		if (!registered)
			return registered;

		agentic::AgenticActionDeclaration configure;
		configure.action_id = "demo.configure";
		configure.description =
			"Select the actual Gallery/Writing page, reflected theme variant "
			"or editor soft-wrap setting; acknowledge a subsequent completed layout.";
		configure.asynchronous = true;
		configure.argument_schema = agentic::Json::make_object(
			{{"type", "object"},
			 {"additionalProperties", false},
			 {"minProperties", agentic::Json::number(1)},
			 {"properties",
			  agentic::Json::make_object(
				  {{"page", agentic::Json::make_object(
								{{"type", "string"},
								 {"enum", agentic::Json::make_array({"gallery", "writing"})}})},
				   {"theme", agentic::Json::make_object(
								 {{"type", "string"},
								  {"enum", agentic::Json::make_array(
											   {"agentic-default", "agentic-ocean"})}})},
				   {"editor_soft_wrap", agentic::Json::make_object({{"type", "boolean"}})}})}});
		configure.result_schema = agentic::Json::make_object(
			{{"type", "object"},
			 {"additionalProperties", false},
			 {"required", agentic::Json::make_array({"page", "theme", "editor_soft_wrap",
													 "window_id", "completed_window_frame"})},
			 {"properties",
			  agentic::Json::make_object(
				  {{"page", agentic::Json::make_object({{"type", "string"}})},
				   {"theme", agentic::Json::make_object({{"type", "string"}})},
				   {"editor_soft_wrap", agentic::Json::make_object({{"type", "boolean"}})},
				   {"window_id",
					agentic::Json::make_object({{"type", "string"}, {"pattern", "^[0-9]+$"}})},
				   {"completed_window_frame",
					agentic::Json::make_object({{"type", "string"}, {"pattern", "^[0-9]+$"}})}})}});
		agentic::AgenticActionAdapter adapter;
		adapter.decode_arguments = [](const agentic::Json& arguments) -> Result<agentic::Json> {
			if (arguments.kind != agentic::Json::Kind::object || arguments.object.empty() ||
				arguments.object.size() > 3)
				return unexpectedError(
					agentic::agentic_error(agentic::AgenticError::invalid_request));
			for (const auto& [key, value] : arguments.object) {
				const bool valid_page =
					key == "page" && value.kind == agentic::Json::Kind::string &&
					(value.string_or() == "gallery" || value.string_or() == "writing");
				const bool valid_theme = key == "theme" &&
										 value.kind == agentic::Json::Kind::string &&
										 (value.string_or() == "agentic-default" ||
										  value.string_or() == "agentic-ocean");
				if (!valid_page && !valid_theme &&
					!(key == "editor_soft_wrap" && value.kind == agentic::Json::Kind::boolean))
					return unexpectedError(
						agentic::agentic_error(agentic::AgenticError::invalid_request));
			}
			return arguments;
		};
		adapter.available = [&scenario_harness](const App& app) -> Result<bool> {
			return scenario_harness.state_id &&
				   app.elements().getStatePointerConst(kDemo, app.mainWindowId(),
													   scenario_harness.state_id) != nullptr;
		};
		adapter.invoke =
			[&scenario_harness](
				App& app, const agentic::AgenticActionInvocation&,
				const agentic::Json& arguments) -> Result<agentic::AgenticActionReceipt> {
			auto* state = scenario_demo_state(app, scenario_harness);
			if (!state)
				return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
			if (const auto* theme = arguments.find("theme")) {
				const auto changed = app.themes().setActiveVariant<FSELTheme>(theme->string_or());
				if (!changed)
					return unexpectedError(changed.error());
			}
			if (const auto* page = arguments.find("page"))
				state->page =
					page->string_or() == "gallery" ? DemoPage::Gallery : DemoPage::WritingStudio;
			if (const auto* wrap = arguments.find("editor_soft_wrap"))
				state->editorSoftWrap = wrap->boolean;
			agentic::AgenticActionReceipt receipt;
			receipt.completion_token = demo_agentic_state_result(app, *state);
			return receipt;
		};
		adapter.poll_completion = [&scenario_harness](App& app,
													  const agentic::AgenticActionReceipt& previous)
			-> Result<agentic::AgenticActionReceipt> {
			const auto* state = scenario_demo_state(app, scenario_harness);
			if (!state)
				return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
			auto receipt = previous;
			const auto& expected = previous.completion_token;
			const auto* prior_frame = expected.find("completed_window_frame");
			if (!prior_frame || app.ui().dev_completed_window_frame() <= prior_frame->uint64_or())
				return receipt;
			receipt.result = demo_agentic_state_result(app, *state);
			receipt.completed = true;
			receipt.completion_succeeded =
				receipt.result.find("page")->string_or() == expected.find("page")->string_or() &&
				receipt.result.find("theme")->string_or() == expected.find("theme")->string_or() &&
				receipt.result.find("editor_soft_wrap")->bool_or() ==
					expected.find("editor_soft_wrap")->bool_or();
			if (!receipt.completion_succeeded)
				receipt.reason = "demo_state_changed_before_completed_layout_acknowledgement";
			return receipt;
		};
		return (*runtime)->actions().register_action(std::move(configure), std::move(adapter));
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
	}
}

[[nodiscard]] bool
same_demo_agentic_geometry(const agentic::AgenticResolvedInteractionTarget& previous,
						   const agentic::AgenticResolvedInteractionTarget& current) noexcept {
	const auto same_region = [](const auto& first, const auto& second) {
		return first.x == second.x && first.y == second.y && first.width == second.width &&
			   first.height == second.height;
	};
	return previous.clay_id == current.clay_id &&
		   previous.flow_instance_id == current.flow_instance_id &&
		   same_region(previous.bounds, current.bounds) &&
		   same_region(previous.visible_region, current.visible_region) &&
		   previous.z_index == current.z_index && previous.paint_order == current.paint_order &&
		   previous.floating == current.floating;
}

/* Publish after drawFrame releases the sealed storage lease; read genuine completed Clay facts. */
[[nodiscard]] Status
publish_demo_agentic_targets(App& application, DemoAgenticHarness& scenario_harness,
							 DemoAgenticInteractionHarness& interaction_harness,
							 float ui_scale) noexcept {
	try {
		if (!interaction_harness.input_enabled && !interaction_harness.actions_enabled)
			return {};
		auto* state = scenario_demo_state(application, scenario_harness);
		if (interaction_harness.actions_enabled &&
			interaction_harness.reset_available != (state != nullptr)) {
			if (!application.actions().appActions().setAvailability(kAgenticResetDemo,
																	{.enabled = state != nullptr}))
				return unexpectedError(agentic::agentic_error(agentic::AgenticError::unavailable));
			interaction_harness.reset_available = state != nullptr;
		}
		if (!interaction_harness.input_enabled)
			return {};
		auto runtime = application.agentic_workflow().interaction_runtime();
		if (!runtime)
			return unexpectedError(runtime.error());
		auto& targets = (*runtime)->targets();
		for (auto& alias : interaction_harness.aliases) {
			agentic::AgenticInteractionTarget selector;
			selector.kind = agentic::AgenticTargetKind::flow_instance;
			selector.window_id = application.mainWindowId();
			const auto& tree = application.ui().devTreeSnapshot();
			bool ambiguous = false;
			for (const auto& node : tree.flow.nodes) {
				if (node.definition != alias.definition ||
					FlowUi::devSystems::tooling::hasFlag(
						node.flags, FlowUi::devSystems::tooling::DevFlowNodeFlag::InternalDev))
					continue;
				const auto path = tree.string(node.debugName);
				const bool matches_primary = path.ends_with(alias.flow_suffix);
				const bool matches_alternate = !alias.alternate_flow_suffix.empty() &&
											   path.ends_with(alias.alternate_flow_suffix);
				if (!matches_primary && !matches_alternate)
					continue;
				if (selector.flow_instance_id) {
					ambiguous = true;
					break;
				}
				selector.flow_instance_id = node.instance.value;
			}
			// Optional aliases disappear on absent/ambiguous layouts; never select a first match.
			if (ambiguous)
				selector.flow_instance_id = 0;
			auto current =
				targets.resolve(application, application.mainWindowId(), selector, ui_scale);
			if (!current) {
				if (alias.previous) {
					const auto removed = targets.unregister_target(alias.target_id, alias.revision);
					if (!removed)
						return removed;
					alias.previous.reset();
				}
				continue;
			}
			if (alias.previous && same_demo_agentic_geometry(*alias.previous, *current))
				continue;
			if (alias.revision == UINT64_MAX)
				return unexpectedError(
					agentic::agentic_error(agentic::AgenticError::budget_exhausted));
			agentic::AgenticCustomInteractionTarget owned;
			owned.target_id = alias.target_id;
			owned.label = alias.label;
			owned.bounds = current->bounds;
			owned.clips.emplace_back(current->visible_region);
			owned.window_id = application.mainWindowId();
			owned.app_tick = current->app_tick;
			owned.window_frame = current->window_frame;
			owned.revision = ++alias.revision;
			owned.paint_order = current->paint_order;
			owned.z_index = current->z_index;
			owned.floating = current->floating;
			const auto clay_id = static_cast<std::uint32_t>(current->clay_id);
			owned.observe = [&application, &scenario_harness,
							 clay_id](const agentic::AgenticResolvedInteractionTarget&)
				-> Result<agentic::AgenticCustomInteractionObservation> {
				const auto& actual = application.ui().dev_completed_interaction();
				const auto member = [clay_id](const auto& values) {
					return std::find(values.begin(), values.end(), clay_id) != values.end();
				};
				agentic::AgenticCustomInteractionObservation observed;
				observed.hovered = member(actual.hoveredElementIds);
				observed.pressed = member(actual.pressedElementIds);
				observed.held = member(actual.heldElementIds);
				observed.released = member(actual.releasedElementIds);
				if (const auto* model = scenario_demo_state(application, scenario_harness))
					observed.outcome = agentic::Json::make_object(
						{{"page", model->page == DemoPage::Gallery ? "gallery" : "writing"},
						 {"button_activations", agentic::decimal(model->buttonActivations)},
						 {"document_changes", agentic::decimal(model->documentChanges)},
						 {"source_clay_id", agentic::decimal(clay_id)}});
				return observed;
			};
			const auto registered = targets.register_target(std::move(owned));
			if (!registered)
				return registered;
			alias.previous = std::move(*current);
		}
		return {};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(agentic::agentic_error(agentic::AgenticError::invalid_request));
	}
}

#endif
