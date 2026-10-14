#include "devSystems/devInterface/Memory/Inspector/DevMemoryInspector.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/Button.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryCapacityTool.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryUi.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
namespace FlowUi::devSystems::interface_elements {
namespace {
using namespace memory_ui;
constexpr auto toggle_disclosure =
	UiAction("flowui.memory.inspector.disclosure", [](bool& expanded) { expanded = !expanded; });
void disclosure(DevMemoryInspector::BuildContext& context, LocalElementName name,
				std::string_view label, bool& expanded) {
	FSEL::ButtonParameters parameters{};
	const auto owned_label = std::string(expanded ? "-  " : "+  ") + std::string(label);
	parameters.text = owned_label;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(28)};
	parameters.padding = Clay_Padding{12, 12, 8, 8};
	parameters.labelFontSize = 12;
	parameters.labelWrapMode = CLAY_TEXT_WRAP_WORDS;
	parameters.labelAlignment = CLAY_TEXT_ALIGN_LEFT;
	parameters.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(0);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.idleOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
#if FLOWUI_INCLUDE_ICON_MANAGER
	auto& icons = context.params.app->icons();
	interface_icons::registerDevInterfaceIcons(icons);
	parameters.icon =
		icons.textureRef(expanded ? interface_icons::kCollapseKey : interface_icons::kExpandKey);
	parameters.contentMode = FSEL::ButtonContentMode::IconThenText;
	parameters.iconSize = 12;
	parameters.text = label;
#endif
	parameters.onActivate =
		ActionCall{context.params.app->actions().uiActions().make(toggle_disclosure, expanded)};
	context.uiManager.createElement(FSEL::kButton, name)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}
[[nodiscard]] const MemoryWorkbenchTrack*
inspected_track(const DevMemoryWorkbenchState& state) noexcept {
	if (state.focus) {
		const auto found =
			std::ranges::find(state.tracks, state.focus, &MemoryWorkbenchTrack::identity);
		return found == state.tracks.end() ? nullptr : &*found;
	}
	return state.tracks.empty() ? nullptr : &state.tracks.front();
}
void reading(DevMemoryInspector::BuildContext& context, std::string_view label, uint64_t value,
			 bool supported, bool bytes, bool decimal) {
	auto line = row(22);
	line.layout.sizing.height = CLAY_SIZING_FIT(22);
	CLAY(context.clayID(Keyed("reading", uint64_t(std::hash<std::string_view>{}(label)))), line) {
		text(context, label, interface_theme::kTextMuted, 10);
		text(context,
			 supported ? bytes
							 ? memory_bytes(value, decimal) + "  (" + std::to_string(value) + " B)"
							 : std::to_string(value)
					   : "Not reported",
			 interface_theme::kTextCanvas, 11);
	}
}
constexpr auto capacity_evidence_choices = std::to_array<FSEL::ComboBoxOption>(
	{{1, "Overall runtime high-water"}, {0, "Recorded snippet"}});
constexpr auto capacity_strategies =
	std::to_array<FSEL::ComboBoxOption>({{0, "Cover observed peak"},
										 {1, "Smaller startup: percentile"},
										 {2, "Keep current"},
										 {3, "Custom value"},
										 {4, "Use library default"}});
constexpr auto capacity_factors =
	std::to_array<FSEL::ComboBoxOption>({{0, "Keep current storage factor"},
										 {1, "Auto-fit shared storage factor"},
										 {2, "Manual storage factor"}});
constexpr auto capacity_margin_modes = std::to_array<FSEL::ComboBoxOption>(
	{{0, "Percentage headroom"}, {1, "Absolute native-unit headroom"}});
constexpr auto capacity_weightings = std::to_array<FSEL::ComboBoxOption>(
	{{1, "Elapsed observation time"}, {0, "Equal sampled intervals"}});
[[nodiscard]] bool capacity_inputs_valid(DevMemoryInspector::BuildContext& context,
										 const MemoryCapacityToolState& tool) {
	const auto valid = [&](const auto& input, LocalElementName name) {
		using Input = std::remove_cvref_t<decltype(input)>;
		const auto element =
			::FlowUi::detail::element_id::resolveLocal(context.id, Input::definitionId, name.token);
		const auto* field = context.uiManager.elements().getStatePointerConst(
			input, context.uiManager.windowId(), element);
		return !field || field->textStatus == FSEL::NumericTextStatus::Complete;
	};
	if (tool.strategy == 0 && tool.editor.planner.margin_mode == 0 &&
		!valid(kDoubleInput, "capacity-headroom"))
		return false;
	if (tool.strategy == 0 && tool.editor.planner.margin_mode == 1 &&
		!valid(kCapacityInput, "capacity-primary-absolute"))
		return false;
	if (tool.strategy == 1 && !valid(kDoubleInput, "capacity-percentile"))
		return false;
	if (tool.strategy == 3 && !valid(kCapacityInput, "capacity-manual"))
		return false;
	if (tool.advanced) {
		if (!valid(kCapacityInput, "capacity-budget"))
			return false;
		if (tool.editor.planner.margin_mode == 1 && tool.strategy != 0 &&
			!valid(kCapacityInput, "capacity-absolute-margin"))
			return false;
		if (tool.editor.evidence_mode == 0) {
			if (!valid(kCapacityInput, "capacity-range-begin") ||
				!valid(kCapacityInput, "capacity-range-end"))
				return false;
			const auto* definition = FlowUi::detail::find_initial_capacity(tool.selected_target);
			if (definition && !definition->storage_member.empty()) {
				if (((tool.strategy != 2 && tool.strategy != 4) &&
					 !valid(kCapacityInput, "capacity-growths")) ||
					!valid(kCapacityInput, "capacity-next"))
					return false;
				if (tool.editor.planner.factor_mode == 2 &&
					!valid(kDoubleInput, "capacity-factor-value"))
					return false;
				if (tool.editor.planner.factor_mode == 1 &&
					!valid(kDoubleInput, "capacity-factor-limit"))
					return false;
			}
		}
	}
	return true;
}
void reset_capacity_inputs(DevMemoryInspector::BuildContext& context) {
	const auto reset = [&](const auto& input, LocalElementName name) {
		using Input = std::remove_cvref_t<decltype(input)>;
		const auto element =
			::FlowUi::detail::element_id::resolveLocal(context.id, Input::definitionId, name.token);
		if (auto* field = context.uiManager.elements().getStatePointer(
				input, context.uiManager.windowId(), element)) {
			field->editSession = {};
			field->wasFocused = false;
			field->forceTextSynchronization = true;
			field->textStatus = FSEL::NumericTextStatus::Complete;
			context.uiManager.inputFields().requestCaret(element, CaretRequestKind::ClearAll);
		}
	};
	for (const auto name :
		 {LocalElementName{"capacity-headroom"}, LocalElementName{"capacity-percentile"},
		  LocalElementName{"capacity-factor-value"}, LocalElementName{"capacity-factor-limit"}})
		reset(kDoubleInput, name);
	for (const auto name :
		 {LocalElementName{"capacity-manual"}, LocalElementName{"capacity-absolute-margin"},
		  LocalElementName{"capacity-primary-absolute"}, LocalElementName{"capacity-budget"},
		  LocalElementName{"capacity-range-begin"}, LocalElementName{"capacity-range-end"},
		  LocalElementName{"capacity-growths"}, LocalElementName{"capacity-next"}})
		reset(kCapacityInput, name);
}
void update_capacity_tool(DevMemoryInspector::BuildContext& context, DevMemoryReporting& reporting,
						  uint64_t action) {
	auto& state = context.params.interfaceState->memory_workbench;
	auto& analysis = state.analysis;
	auto& tool = analysis.capacity_tool;
	const auto selected_scope =
		memory_capacity_scope(context.params.interfaceState->memory_selection.selected_node);
	if (tool.scope != selected_scope) {
		reset_capacity_inputs(context);
		select_memory_capacity_scope(analysis, selected_scope);
	}
	if (action == 401 || action == 406 || action == 407 || action == 409 || action == 410 ||
		action == 412)
		reset_capacity_inputs(context);
	if (tool.strategy != tool.previous_strategy) {
		tool.previous_strategy = tool.strategy;
		auto settings = MemoryCapacityRecipe{}.planner;
		settings.initial_mode = tool.strategy == 4	 ? 4
								: tool.strategy == 1 ? 1
								: tool.strategy == 2 ? 0
								: tool.strategy == 3 ? 3
													 : 2;
		settings.percentile = 50;
		settings.maximum_growths = 2;
		settings.margin_percent = tool.strategy == 0 ? 5 : 0;
		tool.editor.planner = settings;
	}
	if (tool.editor.evidence_mode == 0 && !tool.editor.dataset && analysis.dataset)
		tool.editor.dataset = analysis.dataset;
	const auto live = reporting.read_live();
	if (tool.rules.empty() && analysis.staged.empty())
		analysis.shared_factor = live.storage_growth_factor;
	tool.runtime_growth = live.allow_runtime_growth;
	tool.exceeded_locks = 0;
	for (const auto& draft : analysis.staged) {
		const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
		if (!definition)
			continue;
		const auto& peak = live.capacity_high_water[static_cast<size_t>(definition->target)];
		if ((definition->unit == FlowUi::detail::InitialCapacityUnit::Bytes
				 ? peak.active_bytes
				 : peak.active_entries) > draft.plan.initial &&
			std::ranges::any_of(tool.rules, [&](const auto& rule) {
				return rule.locked && std::ranges::find(rule.locked_targets, draft.target.id) !=
										  rule.locked_targets.end();
			}))
			++tool.exceeded_locks;
	}
	if (tool.editor != tool.evaluated_editor || memory_now_ns() >= tool.last_preview_ns || action) {
		preview_memory_capacities(analysis, live);
		tool.evaluated_editor = tool.editor;
		tool.last_preview_ns = memory_now_ns() + 250'000'000;
	}
	if ((action == 400 || action == 411) && !capacity_inputs_valid(context, tool)) {
		tool.message = "Finish valid numeric edits before preparing settings.";
		action = 0;
	}
	if (action == 400) {
		const auto result = prepare_memory_capacities(analysis);
		if (result)
			preview_memory_capacities(analysis, live);
	}
	if (action == 401)
		undo_memory_capacity_edit(analysis);
	if (action == 402) {
		tool.editor.planner.margin_percent = 0;
		tool.editor.planner.absolute_margin = 0;
		tool.editor.planner.margin_mode = 0;
		tool.last_preview_ns = 0;
	}
	if (action == 403) {
		tool.show_targets = !tool.show_targets;
	}
	if (action == 404) {
		tool.advanced = !tool.advanced;
	}
	if (action == 405) {
		tool.show_ledger = !tool.show_ledger;
	}
	if (action == 406) {
		select_memory_capacity_scope(analysis, selected_scope);
	}
	if (action == 407) {
		const auto proposal =
			std::ranges::find(tool.proposals, tool.selected_target,
							  [](const auto& value) { return value.draft.target.id; });
		if (proposal != tool.proposals.end() && !proposal->locked) {
			select_memory_capacity_scope(analysis, selected_scope);
			tool.editor_target = tool.selected_target;
			if (proposal->available) {
				tool.editor.planner = proposal->draft.recipe;
				tool.editor.evidence_mode = proposal->draft.maximum_only ? 1 : 0;
				tool.editor.dataset = proposal->draft.export_evidence.dataset_id;
				tool.strategy = tool.editor.planner.initial_mode == 4	? 4
								: tool.editor.planner.initial_mode == 1 ? 1
								: tool.editor.planner.initial_mode == 0 ? 2
								: tool.editor.planner.initial_mode == 3 ? 3
																		: 0;
				tool.previous_strategy = tool.strategy;
			}
			const auto pending = std::ranges::find(tool.pending_editors, tool.editor_target,
												   &MemoryCapacityRule::target);
			if (pending != tool.pending_editors.end())
				tool.editor = pending->recipe;
			tool.loaded_editor = tool.editor;
			tool.strategy = tool.editor.planner.initial_mode == 4	? 4
							: tool.editor.planner.initial_mode == 1 ? 1
							: tool.editor.planner.initial_mode == 0 ? 2
							: tool.editor.planner.initial_mode == 3 ? 3
																	: 0;
			tool.previous_strategy = tool.strategy;
			tool.message = "Editing a target specialization; prepare to keep it independent of "
						   "parent policies.";
		}
	}
	if (action == 411) {
		const auto prepared = prepare_memory_capacities(analysis);
		if (prepared) {
			const auto updated = std::ranges::find_if(tool.rules, [&](const auto& rule) {
				return tool.editor_target ? rule.target == tool.editor_target
										  : rule.scope == tool.scope && !rule.target;
			});
			if (updated != tool.rules.end()) {
				const auto locked =
					lock_memory_capacity_rule(analysis, size_t(updated - tool.rules.begin()));
				if (!locked)
					tool.message = "Prepared, but locking failed.";
				else if (tool.undo.size() > 1)
					tool.undo.pop_back(); // One UI operation, one undo snapshot.
			}
		}
	}
	if (action == 415 && tool.selected_target)
		if (const auto* definition = FlowUi::detail::find_initial_capacity(tool.selected_target))
			context.params.app->setClipboardText(definition->key);
	if (action == 413) {
		auto& memory = context.params.app->devMonitoring().memory();
		auto config = memory.config();
		config.level = MemoryMonitoringLevel::SubsystemCapacity;
		memory.setConfig(config);
		tool.message = "Detailed demand monitoring enabled. Coverage begins now; earlier transient "
					   "peaks may be unavailable.";
		tool.last_preview_ns = 0;
	}
	if (action == 412 && tool.selected_target) {
		const auto* definition = FlowUi::detail::find_initial_capacity(tool.selected_target);
		if (definition) {
			select_memory_capacity_scope(analysis, selected_scope);
			tool.editor_target = definition->id();
			tool.strategy = tool.previous_strategy = 4;
			tool.editor.planner.initial_mode = 4;
			tool.editor.planner.manual_initial = definition->default_value;
			tool.editor.planner.margin_percent = 0;
			tool.editor.planner.absolute_margin = 0;
		}
	}
	if (action == 408) {
		const auto found = std::ranges::find(tool.rules, tool.selected_rule, [](const auto& rule) {
			return rule.target ? rule.target : rule.scope + 1;
		});
		if (found != tool.rules.end()) {
			const auto result =
				lock_memory_capacity_rule(analysis, size_t(found - tool.rules.begin()));
			if (!result && tool.message.empty())
				tool.message = "Rule transition could not be completed.";
		}
	}
	if (action == 409) {
		const auto found = std::ranges::find(tool.rules, tool.selected_rule, [](const auto& rule) {
			return rule.target ? rule.target : rule.scope + 1;
		});
		if (found != tool.rules.end()) {
			const auto result =
				revert_memory_capacity_rule(analysis, size_t(found - tool.rules.begin()), live);
			if (!result && tool.message.empty())
				tool.message = "Rule transition could not be completed.";
		}
	}
	if (action == 410) {
		const auto found = std::ranges::find(tool.rules, tool.selected_rule, [](const auto& rule) {
			return rule.target ? rule.target : rule.scope + 1;
		});
		if (found != tool.rules.end()) {
			context.params.interfaceState->memory_selection.selected_node = found->scope;
			select_memory_capacity_scope(analysis, found->scope);
			tool.editor_target = found->target;
			tool.editor = found->recipe;
			const auto pending =
				std::ranges::find_if(tool.pending_editors, [&](const auto& editor) {
					return editor.scope == found->scope && editor.target == found->target;
				});
			if (pending != tool.pending_editors.end())
				tool.editor = pending->recipe;
			tool.loaded_editor = tool.editor;
			tool.strategy = tool.editor.planner.initial_mode == 4	? 4
							: tool.editor.planner.initial_mode == 1 ? 1
							: tool.editor.planner.initial_mode == 0 ? 2
							: tool.editor.planner.initial_mode == 3 ? 3
																	: 0;
			tool.previous_strategy = tool.strategy;
		}
	}
	if (action >= 1000 && action < 1000 + FlowUi::detail::initial_capacity_definitions.size()) {
		tool.selected_target = FlowUi::detail::initial_capacity_definitions[action - 1000].id();
		tool.show_targets = true;
	}
	if (action)
		preview_memory_capacities(analysis, live);
	tool.target_options.clear();
	tool.target_options.reserve(tool.proposals.size());
	for (const auto& proposal : tool.proposals)
		tool.target_options.emplace_back(
			FSEL::ComboBoxOption{proposal.draft.target.id, proposal.label});
	if (std::ranges::find(tool.target_options, tool.selected_target,
						  &FSEL::ComboBoxOption::value) == tool.target_options.end())
		tool.selected_target = tool.target_options.empty() ? 0 : tool.target_options.front().value;
}
[[nodiscard]] Clay_ElementDeclaration capacity_stage() noexcept {
	auto declaration = column(8);
	declaration.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	declaration.layout.padding = {10, 10, 10, 10};
	declaration.backgroundColor = interface_theme::kDepth2Ink;
	declaration.border = {.color = interface_theme::kAccentSeaGlass, .width = {2, 0, 0, 0, 0}};
	declaration.cornerRadius = CLAY_CORNER_RADIUS(4);
	return declaration;
}
void stage_connector(DevMemoryInspector::BuildContext& context, uint64_t index) {
	auto connector = column();
	connector.layout.sizing = {.width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(9)};
	connector.backgroundColor = interface_theme::kAccentSeaGlass;
	CLAY(context.clayID(Keyed("capacity-connector", index)), connector) {}
}
void capacity_comparison(DevMemoryInspector::BuildContext& context,
						 const MemoryCapacityProposal& proposal) {
	const std::array values{proposal.current, proposal.draft.plan.observed_maximum,
							proposal.draft.plan.initial};
	constexpr std::array names{"Current startup hint", "Measured active maximum",
							   "Proposed initial reserve"};
	const auto scale = std::max<uint64_t>(1, std::ranges::max(values));
	for (size_t index = 0; index < values.size(); ++index) {
		text(context, names[index], interface_theme::kTextMuted, 9);
		auto track = row(5);
		track.layout.childGap = 0;
		track.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(context.clayID(Keyed("capacity-comparison", index)), track) {
			auto bar = column();
			bar.layout.sizing = {
				.width = values[index] ? CLAY_SIZING_PERCENT(float(double(values[index]) / scale))
									   : CLAY_SIZING_FIXED(1),
				.height = CLAY_SIZING_FIXED(5)};
			bar.backgroundColor = index == 0   ? interface_theme::kTextMuted
								  : index == 1 ? interface_theme::kStatusAmber
											   : interface_theme::kAccentSeaGlass;
			CLAY(context.clayID(Keyed("capacity-comparison-bar", index)), bar) {}
		}
	}
}
void build_capacity_tool(DevMemoryInspector::BuildContext& context,
						 DevMemoryWorkbenchState& state) {
	auto& analysis = state.analysis;
	auto& tool = analysis.capacity_tool;
	auto panel = column(0);
	panel.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	panel.layout.padding = {12, 12, 8, 12};
	const auto selected =
		std::ranges::find(tool.proposals, tool.selected_target,
						  [](const auto& value) { return value.draft.target.id; });
	size_t eligible = 0, protected_count = 0, locked_count = 0, unavailable = 0;
	for (const auto& proposal : tool.proposals) {
		if (proposal.locked)
			++locked_count;
		else if (proposal.protected_rule)
			++protected_count;
		else if (!proposal.available)
			++unavailable;
		else
			++eligible;
	}
	CLAY(context.clayID("capacity-tool"), panel) {
		text(context, "Capacity tuning", interface_theme::kTextCanvas, 14);
		text(context,
			 "Application draft · " + std::string(memory_capacity_scope_name(tool.scope)) +
				 (tool.editor_target ? " · Target specialization" : " · Scope policy"),
			 interface_theme::kAccentSeaGlass, 11);
		text(context, "Five steps to prepare startup values. Live allocations stay unchanged.",
			 interface_theme::kTextMuted, 10);
		CLAY(context.clayID("capacity-stage-evidence"), capacity_stage()) {
			text(context, "1  Evidence", interface_theme::kTextCanvas, 12);
			if (context.params.app->devMonitoring().memory().config().level <
				MemoryMonitoringLevel::SubsystemCapacity) {
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
				button(context, state, 413, "Enable detailed demand monitoring");
#else
				text(context,
					 "Container demand requires a build with memory monitoring level 2 or above.",
					 interface_theme::kStatusAmber, 10);
#endif
			}
			choice(context, "capacity-evidence", "Choose the measured workload",
				   tool.editor.evidence_mode, capacity_evidence_choices);
			if (tool.editor.evidence_mode == 1) {
				text(context, "Active-demand maxima over this application run; independent of "
							  "retained history.");
				text(context,
					 "Highest measured demand. Sampling and late monitoring can miss peaks.",
					 interface_theme::kStatusAmber, 10);
			} else {
				choice(context, "capacity-recording",
					   "Saved recording (snapshots cannot supply percentiles)", tool.editor.dataset,
					   analysis.dataset_options);
				text(context,
					 "Record or save a snippet using Memory's context controls. Plot zoom does not "
					 "change this evidence.",
					 interface_theme::kTextMuted, 10);
			}
		}
		stage_connector(context, 0);
		CLAY(context.clayID("capacity-stage-strategy"), capacity_stage()) {
			text(context, "2  Scope strategy", interface_theme::kTextCanvas, 12);
			const auto existing = std::ranges::find_if(tool.rules, [&](const auto& rule) {
				return tool.editor_target ? rule.target == tool.editor_target
										  : rule.scope == tool.scope && !rule.target;
			});
			text(context,
				 existing != tool.rules.end()
					 ? "Explicit policy · preparing updates keeps specific descendants"
					 : "Inherited policy · prepare to specialize this scope",
				 interface_theme::kTextMuted, 10);
			if (existing != tool.rules.end() && existing->recipe != tool.editor)
				text(context, "Edited preview · prepared values stay unchanged until updated.",
					 interface_theme::kAccentSeaGlass, 10);
			const auto origin =
				memory_capacity_policy_origin(analysis, tool.scope, tool.editor_target);
			if (existing == tool.rules.end())
				text(context,
					 origin.empty()
						 ? "Using the tool's default strategy"
						 : "Policy inherited from " +
							   std::string(memory_capacity_scope_name(origin.front().scope)),
					 interface_theme::kAccentSeaGlass, 10);
			choice(context, "capacity-strategy", "Choose the memory / growth tradeoff",
				   tool.strategy, capacity_strategies);
			if (tool.strategy == 0) {
				if (tool.editor.planner.margin_mode == 0)
					number(context, "capacity-headroom", "Headroom above PMax (%)",
						   tool.editor.planner.margin_percent, 0.0, 100.0);
				else
					number(context, "capacity-primary-absolute",
						   "Headroom above PMax (native units)",
						   tool.editor.planner.absolute_margin, uint64_t{0}, UINT64_MAX);
				button(context, state, 402, "Exact PMax · 0% headroom");
			} else if (tool.strategy == 1) {
				number(context, "capacity-percentile", "Typical-use percentile (P50, P70, P95...)",
					   tool.editor.planner.percentile, 1.0, 100.0);
				if (tool.editor.evidence_mode == 1)
					text(context,
						 "Choose a recorded snippet: one maximum cannot provide a percentile.",
						 interface_theme::kStatusAmber);
			}
			if (tool.editor_target)
				button(context, state, 406, "Back to scope policy");
		}
		stage_connector(context, 1);
		CLAY(context.clayID("capacity-stage-result"), capacity_stage()) {
			text(context, "3  Result", interface_theme::kTextCanvas, 12);
			text(context, std::to_string(eligible) + " eligible · " +
							  std::to_string(protected_count) + " specific · " +
							  std::to_string(locked_count) + " locked · " +
							  std::to_string(unavailable) + " unavailable");
			if (selected != tool.proposals.end()) {
				const auto& proposal = *selected;
				text(context, proposal.label, interface_theme::kTextCanvas, 11);
				text(context,
					 memory_target_value(proposal.current, proposal.draft.target.unit,
										 state.decimal) +
						 " -> " +
						 (proposal.available
							  ? memory_target_value(proposal.draft.plan.initial,
													proposal.draft.target.unit, state.decimal)
							  : "Keep current · needs evidence"),
					 interface_theme::kAccentSeaGlass, 13);
				if (proposal.available) {
					capacity_comparison(context, proposal);
					text(context,
						 "Exact proposed value: " + std::to_string(proposal.draft.plan.initial) +
							 (proposal.draft.target.unit == MemoryCapacityUnit::Bytes
								  ? " bytes"
								  : " entries"));
					text(context,
						 "Measured maximum: " +
							 memory_target_value(proposal.draft.plan.observed_maximum,
												 proposal.draft.target.unit, state.decimal));
					const auto* definition =
						FlowUi::detail::find_initial_capacity(proposal.draft.target.id);
					text(context,
						 "Applied to " + std::string(definition->scope) + " · " +
							 (proposal.locked			? "Locked"
							  : proposal.protected_rule ? "Specific policy preserved"
														: "Preview"),
						 interface_theme::kTextMuted, 10);
					if (!proposal.draft.maximum_only) {
						text(context,
							 "Initial reserve covers " +
								 std::to_string(int(proposal.draft.plan.initial_coverage * 100)) +
								 "% of weighted measured observations.",
							 interface_theme::kTextMuted, 10);
						if (!definition->storage_member.empty())
							text(context,
								 "Modeled final capacity: " +
									 memory_target_value(proposal.draft.plan.final_capacity,
														 proposal.draft.target.unit,
														 state.decimal) +
									 " · factor " + std::to_string(proposal.draft.plan.factor),
								 interface_theme::kTextMuted, 10);
					}
					if (proposal.draft.maximum_only)
						text(
							context,
							"Maximum-only evidence: no growth chronology or event-count guarantee.",
							interface_theme::kTextMuted, 10);
					else if (definition->storage_member.empty())
						text(
							context,
							"Backend-managed container growth; storage factor does not control it.",
							interface_theme::kTextMuted, 10);
					else
						text(context,
							 "Sampled scalar replay: " +
								 std::to_string(proposal.draft.plan.maximum_instance_growths) +
								 " growths per modeled owner; allocator verification required.",
							 interface_theme::kTextMuted, 10);
					if (proposal.exceeded)
						text(context,
							 "New runtime peak exceeds locked reserve. Unlock and re-evaluate.",
							 interface_theme::kStatusAmber);
				} else
					text(context, proposal.message, interface_theme::kStatusAmber);
			} else
				text(context, "No bakeable capacities in this scope. Choose Overview, a manager or "
							  "CPU storage.");
			text(context,
				 tool.runtime_growth
					 ? "Runtime storage growth enabled; error policies may further restrict it."
					 : "Runtime storage growth disabled.",
				 interface_theme::kTextMuted, 10);
			text(context,
				 "Values are per configured owner; entry costs and physical totals are not "
				 "inferred from reserve hints.",
				 interface_theme::kTextMuted, 10);
		}
		stage_connector(context, 2);
		CLAY(context.clayID("capacity-stage-refine"), capacity_stage()) {
			text(context, "4  Refine capacities", interface_theme::kTextCanvas, 12);
			if (!tool.target_options.empty())
				choice(context, "capacity-target", "Inspect an affected capacity",
					   tool.selected_target, tool.target_options);
			button(context, state, 403,
				   tool.show_targets ? "Hide grouped capacities" : "Show grouped capacities");
			if (tool.show_targets) {
				uint64_t previous_owner = UINT64_MAX;
				for (const auto& proposal : tool.proposals) {
					const auto* definition =
						FlowUi::detail::find_initial_capacity(proposal.draft.target.id);
					const auto owner = memory_capacity_owner(*definition);
					if (owner != previous_owner) {
						text(context, memory_capacity_scope_name(owner),
							 interface_theme::kAccentSeaGlass, 11);
						previous_owner = owner;
					}
					button(context, state, 1000 + uint32_t(definition->target),
						   proposal.label +
							   (proposal.locked			  ? " · Locked"
								: proposal.protected_rule ? " · Specific"
								: !proposal.available
									? " · Needs evidence"
									: " · " + std::to_string(proposal.draft.plan.initial)));
				}
			}
			button(context, state, 412, "Use library default for selected capacity",
				   selected != tool.proposals.end() && !selected->locked);
			button(context, state, 407, "Customize selected capacity",
				   selected != tool.proposals.end() && !selected->locked);
			button(context, state, 404,
				   tool.advanced ? "Hide advanced controls" : "Advanced tuning");
			if (tool.advanced) {
				const auto* definition =
					selected == tool.proposals.end()
						? nullptr
						: FlowUi::detail::find_initial_capacity(selected->draft.target.id);
				if (definition) {
					button(context, state, 415, "Copy canonical configuration key");
					text(context, definition->key, interface_theme::kTextMuted, 10);
					text(context, "Library default " + std::to_string(definition->default_value) +
									  " · alignment " + std::to_string(definition->alignment));
				}
				choice(context, "capacity-margin-mode", "Headroom basis",
					   tool.editor.planner.margin_mode, capacity_margin_modes);
				if (tool.editor.planner.margin_mode == 1)
					number(context, "capacity-absolute-margin", "Additional native bytes / entries",
						   tool.editor.planner.absolute_margin, uint64_t{0}, UINT64_MAX);
				if (tool.strategy == 3)
					number(context, "capacity-manual",
						   "Exact native capacity (zero resolves to default)",
						   tool.editor.planner.manual_initial, uint64_t{0},
						   definition ? definition->maximum : UINT64_MAX);
				if (tool.editor.evidence_mode == 0) {
					choice(context, "capacity-weighting", "Distribution weighting",
						   tool.editor.planner.weighting, capacity_weightings);
					number(context, "capacity-range-begin",
						   "Snippet start timestamp ns (0 = entire snippet)", tool.editor.begin_ns,
						   uint64_t{0}, UINT64_MAX);
					number(context, "capacity-range-end",
						   "Snippet end timestamp ns (0 = entire snippet)", tool.editor.end_ns,
						   uint64_t{0}, UINT64_MAX);
					if (definition && !definition->storage_member.empty()) {
						choice(context, "capacity-factor", "Storage overflow policy",
							   tool.editor.planner.factor_mode, capacity_factors);
						if (tool.strategy != 2 && tool.strategy != 4)
							number(context, "capacity-growths", "Maximum modeled growths per owner",
								   tool.editor.planner.maximum_growths, uint64_t{0}, uint64_t{64});
						if (tool.editor.planner.factor_mode == 2)
							number(context, "capacity-factor-value", "Shared storage growth factor",
								   tool.editor.planner.factor, 1.1, 64.0);
						if (tool.editor.planner.factor_mode == 1)
							number(context, "capacity-factor-limit", "Factor search upper bound",
								   tool.editor.planner.maximum_factor, 1.1, 64.0);
						number(context, "capacity-next",
							   "First overflow target (0 = unconstrained)",
							   tool.editor.planner.next_target, uint64_t{0}, UINT64_MAX);
					}
				}
				number(context, "capacity-budget",
					   (definition && !definition->storage_member.empty() &&
								tool.editor.evidence_mode == 0
							? "Per-owner modeled final budget (0 = unconstrained)"
							: "Per-owner initial reserve budget (0 = unconstrained)"),
					   tool.editor.planner.final_budget, uint64_t{0}, UINT64_MAX);
				text(context,
					 "Keep current preserves resolved startup hints. Library defaults and custom "
					 "values use native registered units.",
					 interface_theme::kTextMuted, 10);
			}
		}
		stage_connector(context, 3);
		CLAY(context.clayID("capacity-stage-prepare"), capacity_stage()) {
			text(context, "5  Prepare settings", interface_theme::kTextCanvas, 12);
			button(context, state, 400,
				   (tool.editor_target
						? "Prepare selected capacity"
						: "Add / update " + std::string(memory_capacity_scope_name(tool.scope)) +
							  " draft"),
				   eligible != 0 && capacity_inputs_valid(context, tool));
			button(context, state, 411, "Prepare and lock exact values",
				   eligible != 0 && capacity_inputs_valid(context, tool));
			if (!capacity_inputs_valid(context, tool))
				text(context, "Correct incomplete or out-of-range numeric text before preparing.",
					 interface_theme::kStatusAmber, 10);
			text(context,
				 std::to_string(analysis.staged.size()) +
					 " prepared capacity values. Final review and bake belong in Changes.");
			text(context,
				 "Changes integration is planned; prepared values do not modify startup or live "
				 "allocations.",
				 interface_theme::kTextMuted, 10);
			if (!tool.message.empty())
				text(context, tool.message, interface_theme::kStatusAmber, 10);
		}
	}
	auto ledger = column(8);
	ledger.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	ledger.layout.padding = {12, 12, 10, 12};
	CLAY(context.clayID("scope-settings"), ledger) {
		text(context, "Scope settings", interface_theme::kTextCanvas, 14);
		text(context, "Specific policies survive parent edits. Locks freeze prepared exact values.",
			 interface_theme::kTextMuted, 10);
		button(context, state, 405, tool.show_ledger ? "Hide set scopes" : "All set scopes");
		if (tool.exceeded_locks)
			text(context,
				 std::to_string(tool.exceeded_locks) +
					 " locked capacities exceeded by new runtime peaks.",
				 interface_theme::kStatusAmber);
		if (tool.show_ledger) {
			tool.rule_options.clear();
			tool.rule_options.reserve(tool.rules.size());
			tool.rule_labels.clear();
			tool.rule_labels.reserve(tool.rules.size());
			for (const auto& rule : tool.rules) {
				const auto* target_definition = FlowUi::detail::find_initial_capacity(rule.target);
				tool.rule_labels.emplace_back(
					std::string(memory_capacity_scope_name(rule.scope)) +
					(target_definition ? " / " + memory_capacity_label(*target_definition) : ""));
				tool.rule_options.emplace_back(FSEL::ComboBoxOption{
					rule.target ? rule.target : rule.scope + 1, tool.rule_labels.back()});
				const auto policy =
					rule.recipe.planner.initial_mode == 1
						? "P" + std::to_string(int(rule.recipe.planner.percentile))
					: rule.recipe.planner.initial_mode == 2
						? "PMax + " + std::to_string(int(rule.recipe.planner.margin_percent)) + "%"
					: rule.recipe.planner.initial_mode == 4 ? "Library default"
															: "Current / custom";
				text(context,
					 std::string(memory_capacity_scope_name(rule.scope)) +
						 (rule.target ? " · Target override" : "") + " · " + policy +
						 (rule.recipe.evidence_mode == 1 ? " · Overall high-water"
														 : " · Recording") +
						 (rule.locked ? " · Locked" : " · Prepared"),
					 interface_theme::kTextCanvas, 11);
			}
			if (!tool.rule_options.empty()) {
				if (std::ranges::find(tool.rule_options, tool.selected_rule,
									  &FSEL::ComboBoxOption::value) == tool.rule_options.end())
					tool.selected_rule = tool.rule_options.front().value;
				choice(context, "capacity-rule", "Choose a prepared scope or target rule",
					   tool.selected_rule, tool.rule_options);
				button(context, state, 410, "Edit selected rule");
				button(context, state, 408, "Lock / unlock prepared values");
				button(context, state, 409, "Revert selected rule to inherited");
			} else
				text(context, "No explicit scopes prepared yet.");
			button(context, state, 401, "Undo last authoring edit", !tool.undo.empty());
		}
	}
}
} // namespace
void DevMemoryInspector::buildElement(BuildContext& context) {
	if (!context.params.app || !context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->memory_workbench;
	auto& analysis = state.analysis;
	auto& reporting = context.params.app->devMonitoring().memoryReporting();
	const auto* track = inspected_track(state);
	const auto action = analysis.ui_action >= 110 ? std::exchange(analysis.ui_action, 0) : 0;
	if (action == 137 && track)
		context.params.app->setClipboardText(track->title + "\n" +
											 memory_track_reading(*track, state.decimal));
	if (action == 134 && track) {
		state.focus = track->parent_identity;
		state.next_history_ns = 0;
	}
	update_capacity_tool(context, reporting, action);
	auto root = column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth1Panel;
	auto content = column(8);
	content.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	content.layout.padding = {12, 12, 10, 12};
	auto section = column(0);
	section.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.clip = {.vertical = true};
	const auto scroll = Clay_GetScrollContainerData(context.clayID());
	if (scroll.found && scroll.scrollPosition)
		root.clip.childOffset = *scroll.scrollPosition;
	CLAY(context.clayID(), root) {
		const auto* saved = selected_memory_dataset(analysis);
		CLAY(context.clayID("subject-content"), content) {
			text(context, track ? track->title : "Memory overview", interface_theme::kTextCanvas,
				 14);
			text(context,
				 saved					? saved->name
				 : state.range_begin_ns ? "Selected range · retained samples"
				 : state.selected_ns	? "At selected time · retained samples"
										: "Live measurements",
				 interface_theme::kAccentSeaGlass);
			if (saved && !saved->complete)
				text(context, "Partial coverage: some observations were not retained.",
					 interface_theme::kStatusAmber, 10);
			text(context,
				 track && !track->window_scoped
					 ? "Application-wide · window modifier does not apply"
				 : state.window ? "Window " + std::to_string(state.window)
								: "All attributed windows",
				 interface_theme::kTextMuted, 10);
			if (!track)
				text(context, "Choose a scope or track. CPU, GPU and process views overlap and "
							  "cannot be added.");
			else {
				disclosure(context, LocalElementName{"scope-diagnostics"}, "Scope diagnostics",
						   analysis.capacity_tool.show_diagnostics);
				if (analysis.capacity_tool.show_diagnostics) {
					if (!track->available)
						text(context,
							 "No supported observation for this subject/time. Live data is not "
							 "substituted.",
							 interface_theme::kStatusAmber);
					else {
						text(context,
							 track->value.logicalLiveBytes ? "Measured reading"
														   : "Idle / measured zero",
							 interface_theme::kTextCanvas, 12);
						const auto fields = track->value.value_fields;
						reading(context, "Active", track->value.logicalLiveBytes,
								fields & uint16_t(MemoryValueField::LogicalLive), true,
								state.decimal);
						reading(context, "Backing", track->value.backingAllocatedBytes,
								fields & uint16_t(MemoryValueField::Backing), true, state.decimal);
						reading(context, "Reusable", track->value.reusableBytes,
								fields & uint16_t(MemoryValueField::Reusable), true, state.decimal);
						reading(context, "Retired", track->value.retiredBytes,
								fields & uint16_t(MemoryValueField::Retired), true, state.decimal);
						reading(context, "Objects", track->value.objectCount,
								fields & uint16_t(MemoryValueField::Objects), false, state.decimal);
						reading(context, "Capacity count", track->value.capacityCount,
								fields & uint16_t(MemoryValueField::Capacity), false,
								state.decimal);
					}
					button(context, state, 137, "Copy reading", track->available);
					text(context, "Accounting", interface_theme::kTextCanvas, 12);
					text(context, track->note.empty()
									  ? "Independent accounting view; group totals may overlap."
									  : track->note);
					if (track->included)
						text(context,
							 "Included/overlapping child: do not add this reading to its parent.",
							 interface_theme::kStatusAmber, 10);
					if (track->parent_identity)
						button(context, state, 134, "Inspect parent");
				}
			}
		}
		if (track && analysis.capacity_tool.show_diagnostics)
			CLAY(context.clayID("measurement-section"), section) {
				disclosure(context, LocalElementName{"measurement-header"}, "Measurement Details",
						   analysis.show_quality);
				if (analysis.show_quality) {
					CLAY(context.clayID("measurement-content"), content) {
						text(context, track->estimated ? "Estimated container/resource footprint"
													   : "Producer-reported measurement");
						text(context, "Report tick " + std::to_string(track->sampled_tick) + " · " +
										  std::to_string(track->keys.size()) +
										  " attributed producer(s)");
						text(context,
							 "History contains sampled/held source values. Allocator reset "
							 "boundaries and interval burst peaks are not retained yet.");
						const auto status = saved ? saved->live.status : reporting.status();
						text(context, "Retained segments " +
										  std::to_string(status.segments.retainedCount) +
										  " · overwritten " +
										  std::to_string(status.segments.overwriteCount));
					}
				}
			}
		if (analysis.baseline_dataset && track) {
			CLAY(context.clayID("baseline-content"), content) {
				const auto baseline = std::ranges::find(
					analysis.datasets, analysis.baseline_dataset, &MemoryDataset::identity);
				if (baseline != analysis.datasets.end()) {
					uint64_t active = 0;
					bool found = false;
					for (const auto& source : baseline->live.sources)
						if (std::ranges::find(track->keys, source.key) != track->keys.end()) {
							active += source.value.logicalLiveBytes;
							found = true;
						}
					text(context, found ? "Baseline active: " + memory_bytes(active, state.decimal)
										: "Baseline subject unavailable");
				}
			}
		}
		build_capacity_tool(context, state);
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
