#include "devSystems/devInterface/Memory/Analysis/DevMemoryCapacityTool.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace FlowUi::devSystems::interface_elements {
namespace {
using Scope = DevMemoryNodeId;
[[nodiscard]] uint64_t identity(Scope scope) noexcept {
	return uint64_t(scope);
}
[[nodiscard]] uint64_t parent_scope(uint64_t scope) noexcept {
	const auto* node = memory_node(scope);
	return node ? memory_capacity_scope(uint64_t(node->parent)) : 0;
}
[[nodiscard]] Status failed() noexcept {
	return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
}
[[nodiscard]] const MemoryCapacityRule* resolved_rule(std::span<const MemoryCapacityRule> rules,
													  uint64_t owner, uint64_t target) noexcept {
	const auto specific = std::ranges::find(rules, target, &MemoryCapacityRule::target);
	if (specific != rules.end() && target)
		return &*specific;
	for (auto scope = owner;; scope = parent_scope(scope)) {
		const auto found = std::ranges::find_if(
			rules, [scope](const auto& rule) { return rule.scope == scope && rule.target == 0; });
		if (found != rules.end())
			return &*found;
		if (!scope)
			break;
	}
	return nullptr;
}
[[nodiscard]] bool locked_target(std::span<const MemoryCapacityRule> rules,
								 uint64_t target) noexcept {
	return std::ranges::any_of(rules, [target](const auto& rule) {
		return rule.locked &&
			   std::ranges::find(rule.locked_targets, target) != rule.locked_targets.end();
	});
}
void save_undo(MemoryAnalysisState& state) {
	auto& tool = state.capacity_tool;
	if (tool.undo.size() == 12)
		tool.undo.erase(tool.undo.begin());
	tool.undo.emplace_back(
		MemoryCapacityUndo{tool.pending_editors, tool.rules, state.staged, state.shared_factor});
}
[[nodiscard]] MemoryTuningTargetDescriptor
descriptor_for(const FlowUi::detail::InitialCapacityDefinition& definition) {
	MemoryTuningTargetDescriptor result;
	result.configKey = std::string(definition.key);
	result.id = result.source = definition.id();
	result.maximum = definition.maximum;
	result.alignment = definition.alignment;
	result.productionDefault = definition.default_value;
	result.unit = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
					  ? MemoryCapacityUnit::Bytes
					  : MemoryCapacityUnit::Entries;
	result.metric = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
						? MemoryTuningMetric::LogicalLiveBytes
						: MemoryTuningMetric::ObjectCount;
	result.applyPolicy = MemoryApplyPolicy::RestartRequired;
	static const auto reporting_targets = memory_sources::tuningTargets();
	for (const auto& descriptor : reporting_targets)
		if (!definition.storage_member.empty() &&
			descriptor.configKey == "storage." + std::string(definition.storage_member))
			result.source = descriptor.source;
	return result;
}
[[nodiscard]] Result<MemorySettingDraft>
evaluate(const FlowUi::detail::InitialCapacityDefinition& definition,
		 const MemoryCapacityRecipe& recipe, const MemoryAnalysisState& state,
		 const MemoryLiveRead& live) {
	MemorySettingDraft draft;
	draft.target = descriptor_for(definition);
	draft.recipe = recipe.planner;
	draft.maximum_only = recipe.evidence_mode == 1;
	draft.percentile = recipe.planner.percentile;
	draft.maximum_growths = (recipe.planner.initial_mode == 0 || recipe.planner.initial_mode == 4)
								? UINT64_MAX
								: recipe.planner.maximum_growths;
	draft.final_budget = recipe.planner.final_budget;
	draft.export_evidence.kind = draft.maximum_only ? "maximum" : "recording";
	draft.export_evidence.source_name = "Overall runtime high-water";
	draft.provenance = draft.export_evidence.source_name;
	uint64_t maximum = 0;
	if (draft.maximum_only) {
		const auto& peak = live.capacity_high_water[static_cast<size_t>(definition.target)];
		const auto field = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
							   ? MemoryValueField::LogicalLive
							   : MemoryValueField::Objects;
		if (!(peak.valid_fields & uint16_t(field)) || recipe.planner.initial_mode == 1)
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
		maximum = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
					  ? peak.active_bytes
					  : peak.active_entries;
		draft.evidence_revision = peak.revision;
		draft.export_evidence.begin_ns = peak.first_measured_ns;
		draft.export_evidence.end_ns =
			std::max(peak.first_measured_ns, live.status.last_consumed_ns);
		// Provenance point only: never replay this maximum as a chronological workload.
		draft.evidence.emplace_back(MemoryDemandObservation{maximum, 1, peak.peak_ns, 0, 0});
		draft.complete = false; // Sampled peaks and late monitoring do not prove full-run coverage.
	} else {
		const auto dataset =
			std::ranges::find(state.datasets, recipe.dataset, &MemoryDataset::identity);
		if (dataset == state.datasets.end() || dataset->snapshot)
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
		draft.export_evidence.source_name = draft.provenance = dataset->name;
		draft.export_evidence.dataset_id = dataset->identity;
		draft.export_evidence.sample_every_ticks = dataset->sample_every_ticks;
		draft.export_evidence.begin_ns =
			recipe.begin_ns ? std::max(recipe.begin_ns, dataset->begin_ns) : dataset->begin_ns;
		draft.export_evidence.end_ns =
			recipe.end_ns ? std::min(recipe.end_ns, dataset->end_ns) : dataset->end_ns;
		draft.export_evidence.begin_tick = UINT64_MAX;
		draft.evidence.reserve(std::min<size_t>(4096, dataset->segments.size()));
		draft.complete = dataset->complete;
		for (const auto& segment : dataset->segments) {
			if (segment.value.source != draft.target.source)
				continue;
			// Registered container aggregate is the maximum across owners, not their sum.
			if (draft.target.source == definition.id() && segment.value.window != InvalidWindowId)
				continue;
			const auto begin = std::max(segment.beginNs, draft.export_evidence.begin_ns);
			const auto end = std::min(segment.endNs, draft.export_evidence.end_ns);
			if (end <= begin)
				continue;
			const auto field = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
								   ? MemoryValueField::LogicalLive
								   : MemoryValueField::Objects;
			if (!(segment.value.value_fields & uint16_t(field)))
				continue;
			if (draft.evidence.size() == 4096) {
				draft.complete = false;
				continue;
			}
			const auto demand = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
									? segment.value.logicalLiveBytes
									: segment.value.objectCount;
			maximum = std::max(maximum, demand);
			draft.evidence.emplace_back(
				MemoryDemandObservation{demand, recipe.planner.weighting ? end - begin : 1, begin,
										segment.value.window, 0});
			draft.export_evidence.begin_tick =
				std::min(draft.export_evidence.begin_tick, segment.beginTick);
			draft.export_evidence.end_tick_exclusive =
				std::max(draft.export_evidence.end_tick_exclusive, segment.endTickExclusive);
		}
		if (draft.evidence.empty())
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
	}
	auto settings = recipe.planner;
	if (settings.initial_mode == 0 || settings.initial_mode == 4)
		settings.maximum_growths = UINT64_MAX;
	draft.selection_policy = settings.initial_mode == 1	  ? "percentile"
							 : settings.initial_mode == 2 ? "observed_maximum"
														  : "manual";
	if (draft.maximum_only) {
		uint64_t basis = settings.initial_mode == 4 ? definition.default_value
						 : settings.initial_mode == 0
							 ? live.initial_capacities.get(definition.target)
						 : settings.initial_mode == 3 ? settings.manual_initial
													  : maximum;
		const auto proposal =
			settings.initial_mode == 0 ? static_cast<long double>(basis)
			: settings.margin_mode == 1
				? static_cast<long double>(basis) + settings.absolute_margin
				: std::ceil(static_cast<long double>(basis) +
							static_cast<long double>(basis) * settings.margin_percent / 100);
		if (!std::isfinite(proposal) || proposal < 0 || proposal > definition.maximum)
			return unexpectedError(makeError(ErrorCode::ArithmeticOverflow, ErrorSite::None));
		basis = uint64_t(proposal);
		const auto remainder = basis % definition.alignment;
		if (remainder && basis > definition.maximum - (definition.alignment - remainder))
			return unexpectedError(makeError(ErrorCode::ArithmeticOverflow, ErrorSite::None));
		basis += remainder ? definition.alignment - remainder : 0;
		// Zero is a default sentinel, never an explicit zero reserve.
		if (!basis)
			basis = settings.initial_mode == 2 ? definition.alignment : definition.default_value;
		if (!FlowUi::detail::valid_initial_capacity(definition, basis) ||
			(settings.final_budget && basis > settings.final_budget))
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
		draft.plan.initial = draft.plan.final_capacity = basis;
		draft.plan.observed_maximum = maximum;
		draft.plan.factor = state.shared_factor;
		draft.plan.uses_default =
			settings.initial_mode == 4 ||
			(settings.initial_mode == 0 &&
			 !live.initial_capacities.values[static_cast<size_t>(definition.target)]);
	} else {
		settings.preset = 3;
		settings.maximum_only = false;
		if (settings.initial_mode == 0 || settings.initial_mode == 4) {
			const auto basis = settings.initial_mode == 4
								   ? definition.default_value
								   : live.initial_capacities.get(definition.target);
			settings.initial_mode = 3;
			settings.manual_initial = basis;
			settings.margin_percent = 0;
			settings.absolute_margin = 0;
		}
		if (definition.storage_member.empty()) {
			settings.maximum_growths = UINT64_MAX;
			settings.factor_mode = 0;
			settings.factor = live.storage_growth_factor;
			settings.next_target = 0;
			settings.final_budget =
				0; // Container final backing is unknown; budget constrains startup only.
		} else if (settings.factor_mode == 0)
			settings.factor = live.storage_growth_factor;
		auto planning_target = draft.target;
		if (settings.initial_mode == 1 || settings.initial_mode == 2)
			planning_target.minimum = definition.alignment;
		if (definition.storage_member.empty())
			planning_target.maximum = UINT64_MAX;
		const auto plan = calculate_memory_plan(draft.evidence, planning_target, settings);
		if (!plan)
			return unexpectedError(plan.error());
		draft.plan = *plan;
		if (!FlowUi::detail::valid_initial_capacity(definition, draft.plan.initial))
			return unexpectedError(makeError(ErrorCode::ArithmeticOverflow, ErrorSite::None));
		if (definition.storage_member.empty() && recipe.planner.final_budget &&
			draft.plan.initial > recipe.planner.final_budget)
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
		draft.plan.uses_default =
			recipe.planner.initial_mode == 4 ||
			(recipe.planner.initial_mode == 0 &&
			 !live.initial_capacities.values[static_cast<size_t>(definition.target)]);
		if (!definition.storage_member.empty() && !live.allow_runtime_growth &&
			draft.plan.growth_count)
			return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::None));
		if (definition.storage_member.empty()) {
			// Scalar geometry is only used to calculate coverage, not to claim STL growth.
			draft.plan.steps.clear();
			draft.plan.growth_count = draft.plan.maximum_instance_growths = 0;
			draft.plan.final_capacity = draft.plan.initial;
		}
	}
	if (draft.plan.initial >= maximum && draft.selection_policy == "observed_maximum")
		draft.headroom = draft.plan.initial - maximum;
	if (draft.selection_policy == "percentile" &&
		draft.plan.initial >= draft.plan.percentile_demand)
		draft.headroom = draft.plan.initial - draft.plan.percentile_demand;
	draft.export_evidence.complete = draft.complete;
	return draft;
}
/** Fit one shared storage factor across this preview and all previously prepared storage peers. */
void reconcile_storage_factor(MemoryAnalysisState& state) {
	auto& proposals = state.capacity_tool.proposals;
	bool auto_requested = false;
	double upper_factor = 64;
	std::vector<MemorySettingDraft> peers;
	peers.reserve(state.staged.size() + proposals.size());
	for (const auto& draft : state.staged)
		if (const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
			definition && !definition->storage_member.empty())
			peers.emplace_back(draft);
	for (const auto& proposal : proposals) {
		const auto* definition = FlowUi::detail::find_initial_capacity(proposal.draft.target.id);
		if (!proposal.available || proposal.locked || proposal.protected_rule ||
			definition->storage_member.empty())
			continue;
		std::erase_if(
			peers, [&](const auto& draft) { return draft.target.id == proposal.draft.target.id; });
		peers.emplace_back(proposal.draft);
		if (!proposal.draft.maximum_only && proposal.draft.recipe.factor_mode == 1) {
			auto_requested = true;
			upper_factor = std::min(upper_factor, proposal.draft.recipe.maximum_factor);
		}
	}
	if (!auto_requested)
		return;
	std::optional<double> best;
	long double best_final = std::numeric_limits<long double>::infinity();
	const auto consider = [&](double factor) {
		if (factor < 1.1 || factor > upper_factor)
			return;
		long double final_total = 0;
		for (const auto& draft : peers) {
			if (locked_target(state.capacity_tool.rules, draft.target.id) &&
				std::abs(draft.plan.factor - factor) > .000001)
				return;
			if (draft.recipe.factor_mode == 2 && std::abs(draft.recipe.factor - factor) > .000001)
				return;
			if (draft.maximum_only) {
				final_total += draft.plan.initial;
				continue;
			}
			const auto replay =
				replay_memory_plan(draft.evidence, draft.target, draft.plan.initial, factor);
			if (!replay || replay->maximum_instance_growths > draft.maximum_growths ||
				(draft.final_budget && replay->final_capacity > draft.final_budget) ||
				(draft.recipe.next_target && replay->ladder[1] < draft.recipe.next_target))
				return;
			final_total += replay->final_capacity;
		}
		if (final_total < best_final) {
			best = factor;
			best_final = final_total;
		}
	};
	consider(state.shared_factor);
	for (const auto& draft : peers)
		consider(draft.plan.factor);
	for (size_t index = 0; index <= 256; ++index)
		consider(1.1 + (upper_factor - 1.1) * index / 256);
	for (auto& proposal : proposals) {
		const auto* definition = FlowUi::detail::find_initial_capacity(proposal.draft.target.id);
		if (!proposal.available || proposal.locked || proposal.protected_rule ||
			definition->storage_member.empty())
			continue;
		if (!best) {
			proposal.available = false;
			proposal.message =
				"No common storage factor satisfies prepared peers, locks and growth constraints";
		} else if (proposal.draft.maximum_only)
			proposal.draft.plan.factor = *best;
		else {
			const auto replay = replay_memory_plan(proposal.draft.evidence, proposal.draft.target,
												   proposal.draft.plan.initial, *best);
			if (replay) {
				const auto percentile = proposal.draft.plan.percentile_demand;
				proposal.draft.plan = *replay;
				proposal.draft.plan.percentile_demand = percentile;
			}
		}
	}
}

} // namespace
uint64_t memory_capacity_scope(uint64_t selection) noexcept {
	if (selection == identity(Scope::SharedManagers))
		return identity(Scope::Managers);
	if (selection == identity(Scope::SharedCpuStorage))
		return identity(Scope::CpuStorage);
	if (selection >= memory_heap_node_base)
		return identity(Scope::GpuResources);
	return selection;
}
uint64_t
memory_capacity_owner(const FlowUi::detail::InitialCapacityDefinition& definition) noexcept {
	const auto key = definition.key;
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_persistent_bytes)
		return identity(Scope::PersistentPool);
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_strings_bytes)
		return identity(Scope::StringPool);
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_frame_bytes)
		return identity(Scope::FrameArenas);
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_worker_bytes)
		return identity(Scope::WorkerArenas);
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_decode_bytes)
		return identity(Scope::DecodeScratch);
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_staging_bytes)
		return identity(Scope::UploadStaging);
	if (definition.target == FlowUi::detail::InitialCapacityTarget::storage_instance_bytes)
		return identity(Scope::FramePayload);
	constexpr std::array groups{std::pair{"input_fields", Scope::InputFields},
								std::pair{"popups", Scope::Popups},
								std::pair{"shortcuts", Scope::Shortcuts},
								std::pair{"actions", Scope::Actions},
								std::pair{"fonts", Scope::Fonts},
								std::pair{"icons", Scope::Icons},
								std::pair{"viewports", Scope::Viewports},
								std::pair{"themes", Scope::Themes},
								std::pair{"elements", Scope::Elements},
								std::pair{"ui", Scope::UiLayout},
								std::pair{"development", Scope::TreeCapture},
								std::pair{"storage", Scope::StorageMetadata},
								std::pair{"allocator", Scope::StorageMetadata},
								std::pair{"app", Scope::Managers}};
	const auto suffix = key.substr(std::string_view("flowui.tuning.").size());
	for (const auto& [prefix, scope] : groups)
		if (suffix.starts_with(prefix) && suffix.size() > std::string_view(prefix).size() &&
			suffix[std::string_view(prefix).size()] == '.')
			return identity(scope);
	return 0;
}
bool memory_capacity_contains(uint64_t scope, uint64_t owner) noexcept {
	if (!scope)
		return true;
	for (;;) {
		if (scope == owner)
			return true;
		if (!owner)
			return false;
		owner = parent_scope(owner);
	}
}
std::string_view memory_capacity_scope_name(uint64_t scope) noexcept {
	const auto* node = memory_node(scope);
	return node ? node->label : "Overview";
}
std::string memory_capacity_label(const FlowUi::detail::InitialCapacityDefinition& definition) {
	if (!definition.storage_member.empty()) {
		constexpr std::array names{"Persistent CPU",		"String pool",	  "Frame arena",
								   "Worker arena",			"Decode scratch", "Upload staging",
								   "Frame instance payload"};
		return names[static_cast<size_t>(definition.target)];
	}
	auto label = std::string(definition.key.substr(std::string_view("flowui.tuning.").size()));
	const auto first = label.find('.');
	if (first != std::string::npos)
		label.erase(0, first + 1);
	for (auto& character : label)
		if (character == '_' || character == '.')
			character = ' ';
	return label;
}
std::span<const MemoryCapacityRule> memory_capacity_policy_origin(const MemoryAnalysisState& state,
																  uint64_t scope,
																  uint64_t target) noexcept {
	const auto* definition = FlowUi::detail::find_initial_capacity(target);
	const auto* rule = resolved_rule(
		state.capacity_tool.rules, definition ? memory_capacity_owner(*definition) : scope, target);
	return rule ? std::span<const MemoryCapacityRule>{rule, 1}
				: std::span<const MemoryCapacityRule>{};
}
void select_memory_capacity_scope(MemoryAnalysisState& state, uint64_t scope) {
	auto& tool = state.capacity_tool;
	if (tool.scope != UINT64_MAX && tool.editor != tool.loaded_editor) {
		const auto* editor_definition = FlowUi::detail::find_initial_capacity(tool.editor_target);
		const auto editor_scope =
			editor_definition ? memory_capacity_owner(*editor_definition) : tool.scope;
		std::erase_if(tool.pending_editors, [&](const auto& editor) {
			return editor.scope == editor_scope && editor.target == tool.editor_target;
		});
		tool.pending_editors.reserve(FlowUi::detail::initial_capacity_definitions.size() +
									 memory_selector_nodes.size());
		tool.pending_editors.emplace_back(
			MemoryCapacityRule{{}, tool.editor, editor_scope, tool.editor_target, false});
	}
	tool.scope = memory_capacity_scope(scope);
	tool.editor_target = 0;
	const auto* rule = resolved_rule(tool.rules, tool.scope, 0);
	tool.editor = rule ? rule->recipe : MemoryCapacityRecipe{};
	const auto pending = std::ranges::find_if(tool.pending_editors, [&](const auto& editor) {
		return editor.scope == tool.scope && !editor.target;
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
	tool.last_preview_ns = 0;
}
void preview_memory_capacities(MemoryAnalysisState& state, const MemoryLiveRead& live) {
	if (state.capacity_tool.rules.empty() && state.staged.empty())
		state.shared_factor = live.storage_growth_factor;
	auto& tool = state.capacity_tool;
	auto rules = tool.rules;
	const auto* editor_definition = FlowUi::detail::find_initial_capacity(tool.editor_target);
	const auto editor_scope =
		editor_definition ? memory_capacity_owner(*editor_definition) : tool.scope;
	std::erase_if(rules, [&](const auto& rule) {
		return tool.editor_target ? rule.target == tool.editor_target
								  : rule.scope == tool.scope && !rule.target;
	});
	rules.emplace_back(
		MemoryCapacityRule{{}, tool.editor, editor_scope, tool.editor_target, false});
	tool.proposals.clear();
	tool.proposals.reserve(FlowUi::detail::initial_capacity_definitions.size());
	for (const auto& definition : FlowUi::detail::initial_capacity_definitions) {
		const auto owner = memory_capacity_owner(definition);
		if (!memory_capacity_contains(tool.scope, owner) ||
			(tool.editor_target && tool.editor_target != definition.id()))
			continue;
		MemoryCapacityProposal proposal;
		proposal.label = memory_capacity_label(definition);
		proposal.current = live.initial_capacities.get(definition.target);
		const auto* rule = resolved_rule(rules, owner, definition.id());
		proposal.protected_rule = rule && (rule->target != tool.editor_target ||
										   (!tool.editor_target && rule->scope != tool.scope));
		proposal.locked = locked_target(tool.rules, definition.id());
		const auto prepared = std::ranges::find(state.staged, definition.id(),
												[](const auto& item) { return item.target.id; });
		if ((proposal.locked || proposal.protected_rule) && prepared != state.staged.end()) {
			proposal.draft = *prepared;
			proposal.available = true;
		} else {
			const auto result =
				evaluate(definition, rule ? rule->recipe : tool.editor, state, live);
			if (result) {
				proposal.draft = *result;
				proposal.available = true;
			} else {
				proposal.draft.target = descriptor_for(definition);
				proposal.message = rule && rule->recipe.evidence_mode == 1 &&
										   rule->recipe.planner.initial_mode == 1
									   ? "Percentile needs a recorded demand distribution"
									   : "No applicable evidence or constraints cannot be met";
			}
		}
		if (rule && !proposal.protected_rule && !proposal.locked) {
			proposal.draft.origin_scope = rule->scope;
			proposal.draft.origin_target = rule->target;
		}
		const auto& peak = live.capacity_high_water[static_cast<size_t>(definition.target)];
		const auto maximum = definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes
								 ? peak.active_bytes
								 : peak.active_entries;
		proposal.exceeded = proposal.locked && maximum > proposal.draft.plan.initial;
		tool.proposals.emplace_back(std::move(proposal));
	}
	reconcile_storage_factor(state);
}
Status prepare_memory_capacities(MemoryAnalysisState& state) {
	auto& tool = state.capacity_tool;
	const auto* editing_definition = FlowUi::detail::find_initial_capacity(tool.editor_target);
	const auto editing_scope =
		editing_definition ? memory_capacity_owner(*editing_definition) : tool.scope;
	if (std::ranges::any_of(tool.rules, [&](const auto& rule) {
			return rule.locked &&
				   ((!rule.target && memory_capacity_contains(rule.scope, editing_scope)) ||
					(rule.target && rule.target == tool.editor_target));
		})) {
		tool.message = "Unlock this scope or its ancestor before preparing updates.";
		return failed();
	}
	std::vector<MemorySettingDraft> prepared = state.staged;
	size_t changed = 0;
	auto factor = state.shared_factor;
	bool factor_selected = false;
	for (const auto& proposal : tool.proposals) {
		if (!proposal.available || proposal.protected_rule || proposal.locked)
			continue;
		const auto* definition = FlowUi::detail::find_initial_capacity(proposal.draft.target.id);
		if (!definition->storage_member.empty() && !proposal.draft.maximum_only) {
			if (factor_selected && std::abs(factor - proposal.draft.plan.factor) > .000001) {
				tool.message = "Storage targets need different factors. Keep a common factor or "
							   "refine constraints.";
				return failed();
			}
			factor = proposal.draft.plan.factor;
			factor_selected = true;
		}
		std::erase_if(prepared,
					  [&](const auto& item) { return item.target.id == proposal.draft.target.id; });
		prepared.emplace_back(proposal.draft);
		++changed;
	}
	if (!changed) {
		tool.message = "Nothing eligible: check evidence, specific overrides and locks.";
		return failed();
	}
	if (std::abs(factor - state.shared_factor) > .000001) {
		for (auto& draft : prepared) {
			const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
			if (definition->storage_member.empty())
				continue;
			if (locked_target(tool.rules, draft.target.id)) {
				tool.message =
					"Shared factor change affects a locked storage target. Unlock it first.";
				return failed();
			}
			if (!draft.maximum_only && draft.recipe.factor_mode == 2 &&
				std::abs(draft.recipe.factor - factor) > .000001) {
				tool.message = "Shared factor conflicts with an explicit manual factor. Edit that "
							   "scope policy first.";
				return failed();
			}
			if (draft.maximum_only) {
				draft.plan.factor = factor;
				continue;
			}
			const auto replay =
				replay_memory_plan(draft.evidence, draft.target, draft.plan.initial, factor);
			if (!replay || replay->maximum_instance_growths > draft.maximum_growths ||
				(draft.final_budget && replay->final_capacity > draft.final_budget) ||
				(draft.recipe.next_target && replay->ladder[1] < draft.recipe.next_target)) {
				tool.message = "Shared factor conflicts with " + draft.target.configKey;
				return failed();
			}
			draft.plan = *replay;
		}
	}
	for (auto& draft : prepared)
		if (const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
			definition && !definition->storage_member.empty())
			draft.plan.factor = factor;
	save_undo(state);
	const auto* editor_definition = FlowUi::detail::find_initial_capacity(tool.editor_target);
	const auto editor_scope =
		editor_definition ? memory_capacity_owner(*editor_definition) : tool.scope;
	std::erase_if(tool.rules, [&](const auto& rule) {
		return tool.editor_target ? rule.target == tool.editor_target
								  : rule.scope == tool.scope && !rule.target;
	});
	tool.rules.emplace_back(
		MemoryCapacityRule{{}, tool.editor, editor_scope, tool.editor_target, false});
	std::erase_if(tool.pending_editors, [&](const auto& editor) {
		return editor.scope == editor_scope && editor.target == tool.editor_target;
	});
	tool.loaded_editor = tool.editor;
	state.staged = std::move(prepared);
	state.shared_factor = factor;
	++state.staged_revision;
	tool.message = "Prepared " + std::to_string(changed) +
				   " capacities; specific descendants and locks preserved.";
	return {};
}
Status lock_memory_capacity_rule(MemoryAnalysisState& state, size_t rule_index) {
	auto& tool = state.capacity_tool;
	if (rule_index >= tool.rules.size())
		return failed();
	const auto& selected = tool.rules[rule_index];
	for (size_t index = 0; index < tool.rules.size(); ++index)
		if (index != rule_index && tool.rules[index].locked && !tool.rules[index].target &&
			memory_capacity_contains(tool.rules[index].scope, selected.scope)) {
			tool.message = "Unlock the ancestor scope first.";
			return failed();
		}
	save_undo(state);
	auto& rule = tool.rules[rule_index];
	if (!rule.locked) {
		rule.locked_targets.clear();
		rule.locked_targets.reserve(state.staged.size());
		for (const auto& draft : state.staged) {
			const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
			if (memory_capacity_contains(rule.scope, memory_capacity_owner(*definition)) &&
				(!rule.target || rule.target == draft.target.id))
				rule.locked_targets.emplace_back(draft.target.id);
		}
		if (rule.locked_targets.empty()) {
			tool.undo.pop_back();
			tool.message = "Prepare settings before locking.";
			return failed();
		}
	}
	rule.locked = !rule.locked;
	tool.message = rule.locked ? "Prepared exact values locked; new peaks will be reported."
							   : "Unlocked; prepared values stay unchanged until updated.";
	tool.last_preview_ns = 0;
	return {};
}
Status revert_memory_capacity_rule(MemoryAnalysisState& state, size_t rule_index,
								   const MemoryLiveRead& live) {
	auto& tool = state.capacity_tool;
	if (rule_index >= tool.rules.size())
		return failed();
	const auto rule = tool.rules[rule_index];
	if (rule.locked || std::ranges::any_of(tool.rules, [&](const auto& ancestor) {
			return ancestor.locked && !ancestor.target &&
				   memory_capacity_contains(ancestor.scope, rule.scope);
		})) {
		tool.message = "Unlock the rule and its ancestors before reverting.";
		return failed();
	}
	auto rules = tool.rules;
	rules.erase(rules.begin() + static_cast<ptrdiff_t>(rule_index));
	auto prepared = state.staged;
	for (auto& draft : prepared) {
		if (draft.origin_scope != rule.scope || draft.origin_target != rule.target)
			continue;
		if (locked_target(tool.rules, draft.target.id)) {
			tool.message = "Unlock affected descendants before reverting their inherited values.";
			return failed();
		}
		const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
		const auto* inherited =
			resolved_rule(rules, memory_capacity_owner(*definition), definition->id());
		const auto result = evaluate(
			*definition, inherited ? inherited->recipe : MemoryCapacityRecipe{}, state, live);
		if (!result) {
			tool.message = "Inherited policy cannot be evaluated. Preserve the rule and choose "
						   "valid evidence.";
			return failed();
		}
		if (!definition->storage_member.empty() && !result->maximum_only) {
			const auto replay = replay_memory_plan(result->evidence, result->target,
												   result->plan.initial, state.shared_factor);
			if (!replay || replay->maximum_instance_growths > result->maximum_growths ||
				(result->final_budget && replay->final_capacity > result->final_budget) ||
				(result->recipe.next_target && replay->ladder[1] < result->recipe.next_target)) {
				tool.message = "Inherited storage policy conflicts with the prepared shared "
							   "factor. Refine the parent policy first.";
				return failed();
			}
		}
		draft = *result;
		if (!definition->storage_member.empty()) {
			if (!draft.maximum_only) {
				const auto replay = replay_memory_plan(draft.evidence, draft.target,
													   draft.plan.initial, state.shared_factor);
				if (replay)
					draft.plan = *replay;
			}
			draft.plan.factor = state.shared_factor;
		}
		draft.origin_scope = inherited ? inherited->scope : 0;
		draft.origin_target = inherited ? inherited->target : 0;
	}
	save_undo(state);
	tool.rules = std::move(rules);
	state.staged = std::move(prepared);
	++state.staged_revision;
	select_memory_capacity_scope(state, rule.scope);
	tool.message =
		"Specialization removed; inherited values prepared. Undo restores the prior rule.";
	return {};
}
void undo_memory_capacity_edit(MemoryAnalysisState& state) {
	auto& tool = state.capacity_tool;
	if (tool.undo.empty())
		return;
	auto previous = std::move(tool.undo.back());
	tool.undo.pop_back();
	tool.pending_editors = std::move(previous.pending_editors);
	tool.rules = std::move(previous.rules);
	state.staged = std::move(previous.staged);
	state.shared_factor = previous.shared_factor;
	++state.staged_revision;
	tool.editor = tool.loaded_editor;
	select_memory_capacity_scope(state, tool.scope);
	tool.message = "Restored the previous authoring state.";
}
} // namespace FlowUi::devSystems::interface_elements
#endif
