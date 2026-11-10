#include "devSystems/devInterface/Changes/DevChangesModel.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/App.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemorySession.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Private/IdentityHash.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include <algorithm>
#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace FlowUi::devSystems {
namespace {
using namespace tooling;
[[nodiscard]] std::string_view runtime_failure_reason(DevCommandStatus status) noexcept {
	switch (status) {
	case DevCommandStatus::FieldReadOnly:
		return "The field is read-only.";
	case DevCommandStatus::TypeMismatch:
		return "The value type changed; refresh and review the typed value.";
	case DevCommandStatus::ConstraintRejected:
		return "The value does not satisfy the field constraints.";
	case DevCommandStatus::SchemaUnavailable:
		return "The runtime schema is unavailable.";
	case DevCommandStatus::SchemaMismatch:
		return "The runtime schema changed; refresh this review.";
	case DevCommandStatus::FieldUnavailable:
		return "The field no longer resolves; refresh and inspect its source identity.";
	case DevCommandStatus::InvalidTarget:
		return "The target or window is unavailable; select an observed target.";
	case DevCommandStatus::ThemeVariantUnavailable:
		return "The theme variant is no longer registered.";
	case DevCommandStatus::QueueFull:
		return "The override command queue is full; try again.";
	case DevCommandStatus::ValueCopyFailed:
		return "The typed value could not be copied; refresh and retry.";
	case DevCommandStatus::EmptyTransaction:
		return "The runtime transaction contains no commands.";
	case DevCommandStatus::NothingToBake:
		return "No live change is available to apply.";
	case DevCommandStatus::BakeManifestInvalid:
		return "The active manifest is invalid; review its validation details.";
	case DevCommandStatus::BakeWriteFailed:
		return "The active manifest could not be written.";
	case DevCommandStatus::InternalFailure:
		return "The override engine could not complete the command.";
	case DevCommandStatus::Applied:
		return "The command was acknowledged without applying its value.";
	}
	return "The override engine returned an unknown command status.";
}
[[nodiscard]] uint64_t identity(std::string_view text) noexcept {
	uint64_t result = 14695981039346656037ull;
	for (const unsigned char character : text)
		result = (result ^ character) * 1099511628211ull;
	return result ? result : 1;
}
[[nodiscard]] std::string lowercase(std::string_view text) {
	std::string result(text);
	for (auto& character : result)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	return result;
}
[[nodiscard]] std::string_view domain_name(DevChangesDomain domain) noexcept {
	if (domain == DevChangesDomain::Themes)
		return "Themes";
	if (domain == DevChangesDomain::Memory)
		return "Memory";
	return "Elements";
}
[[nodiscard]] bool pending(const DevChangesReviewRow& row) noexcept {
	return row.operation == DevChangesOperation::Add ||
		   row.operation == DevChangesOperation::Update ||
		   row.operation == DevChangesOperation::Remove;
}
[[nodiscard]] bool row_matches(const DevChangesState& state, const DevChangesReviewRow& row) {
	if (state.domain && state.domain != static_cast<uint64_t>(row.domain) + 1)
		return false;
	if (state.status == 1 && !pending(row))
		return false;
	if (state.status == 2 && !row.has_disk)
		return false;
	if (state.status == 3 && row.validity == DevChangesValidity::Ready)
		return false;
	if (state.status == 4 && row.decision != DevChangesDecision::OmitProposal)
		return false;
	if (state.search.empty())
		return true;
	std::string haystack = row.target_name + " " + row.field_path + " " + row.key + " " +
						   row.reason + " " + row.origin + " " +
						   std::string(changes_operation_name(row.operation)) + " " +
						   std::string(changes_validity_name(row.validity)) + " " +
						   std::string(changes_decision_name(row.decision));
	if (row.operation == DevChangesOperation::Add)
		haystack += " New";
	if (row.operation == DevChangesOperation::Remove)
		haystack += " Removal";
	if (const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry)
		haystack +=
			" " + entry->ownerCppType + " " + entry->themeVariant + " " + entry->instanceDebugLabel;
	return lowercase(haystack).find(lowercase(state.search)) != std::string::npos;
}
[[nodiscard]] DevChangesReviewRequest current_request(const DevInterfaceState& interface_state) {
	const auto& state = interface_state.changes;
	DevChangesReviewRequest request{};
	request.decisions = state.decisions;
	if (state.initialized)
		request.opened_manifest = state.review.request.opened_manifest
									  ? state.review.request.opened_manifest
									  : std::optional<DevBakeManifest>{state.review.disk_manifest};
	const auto& analysis = interface_state.memory_workbench.analysis;
	request.memory_revision = analysis.staged_revision;
	if (!analysis.staged.empty()) {
		FlowUi::detail::MemoryCapacityManifest prepared{};
		prepared.targets.reserve(analysis.staged.size());
		bool has_storage = false;
		for (const auto& draft : analysis.staged) {
			const auto* definition = FlowUi::detail::find_initial_capacity(draft.target.id);
			FlowUi::detail::MemoryCapacitySelection selection{};
			selection.key = definition ? std::string(definition->key) : draft.target.configKey;
			selection.policy = draft.selection_policy;
			selection.value = (draft.recipe.initial_mode == 4 ||
							   (draft.recipe.initial_mode == 3 && !draft.recipe.manual_initial &&
								(!draft.recipe.margin_mode || !draft.recipe.absolute_margin)))
								  ? 0
								  : draft.plan.initial;
			selection.observed_maximum = draft.plan.observed_maximum;
			selection.headroom = draft.headroom;
			selection.percentile = draft.percentile;
			selection.evidence = draft.export_evidence;
			selection.evidence.acknowledge_incomplete = state.acknowledge_incomplete;
			if (draft.synthetic)
				selection.evidence.kind = "synthetic";
			else if (draft.evidence.empty())
				selection.evidence.kind = "unavailable";
			prepared.targets.emplace_back(std::move(selection));
			has_storage = has_storage || (definition && !definition->storage_member.empty());
		}
		if (has_storage) {
			prepared.growth_factor = static_cast<float>(analysis.shared_factor);
			prepared.allow_runtime_growth = analysis.capacity_tool.runtime_growth;
		}
		request.prepared_memory = std::move(prepared);
	}
	if (state.policies.enabled || state.policies.growth_factor || state.policies.runtime_growth) {
		if (!request.prepared_memory) {
			request.prepared_memory.emplace();
			request.prepared_memory->enabled =
				state.review.disk_manifest.memoryCapacity
					? state.review.disk_manifest.memoryCapacity->enabled
					: true;
		}
		if (state.policies.enabled)
			request.prepared_memory->enabled = *state.policies.enabled;
		if (state.policies.growth_factor)
			request.prepared_memory->growth_factor = state.policies.growth_factor;
		if (state.policies.runtime_growth)
			request.prepared_memory->allow_runtime_growth = state.policies.runtime_growth;
	}
	request.clear_memory_section = state.policies.clear_section;
	request.clear_growth_factor = state.policies.clear_growth_factor;
	request.clear_runtime_growth = state.policies.clear_runtime_growth;
	uint64_t memory_revision = analysis.staged_revision ^ 14695981039346656037ull;
	auto mix_number = [&](uint64_t value) {
		for (int byte_index = 0; byte_index < 8; ++byte_index) {
			memory_revision = (memory_revision ^ (value & 255)) * 1099511628211ull;
			value >>= 8;
		}
	};
	auto mix_text = [&](std::string_view value) {
		mix_number(value.size());
		for (const unsigned char character : value)
			memory_revision = (memory_revision ^ character) * 1099511628211ull;
	};
	mix_number(request.clear_memory_section);
	mix_number(request.clear_growth_factor);
	mix_number(request.clear_runtime_growth);
	mix_number(request.prepared_memory.has_value());
	if (const auto& prepared = request.prepared_memory) {
		mix_number(prepared->enabled);
		mix_number(prepared->growth_factor.has_value());
		if (prepared->growth_factor)
			mix_number(std::bit_cast<uint32_t>(*prepared->growth_factor));
		mix_number(prepared->allow_runtime_growth.has_value());
		if (prepared->allow_runtime_growth)
			mix_number(*prepared->allow_runtime_growth);
		mix_number(prepared->targets.size());
		for (const auto& selection : prepared->targets) {
			mix_text(selection.key);
			mix_text(selection.policy);
			mix_number(selection.value);
			mix_number(selection.observed_maximum);
			mix_number(selection.headroom);
			mix_number(std::bit_cast<uint64_t>(selection.percentile));
			const auto& evidence = selection.evidence;
			mix_text(evidence.kind);
			mix_text(evidence.source_name);
			mix_number(evidence.dataset_id);
			mix_number(evidence.begin_ns);
			mix_number(evidence.end_ns);
			mix_number(evidence.begin_tick);
			mix_number(evidence.end_tick_exclusive);
			mix_number(evidence.sample_every_ticks);
			mix_number(evidence.complete);
			mix_number(evidence.acknowledge_incomplete);
		}
	}
	request.memory_revision = memory_revision;
	return request;
}
void refresh(App& app, DevInterfaceState& interface_state) {
	auto& state = interface_state.changes;
	state.memory_error.clear();
	auto refreshed = app.devTooling().bakePipeline().query_review(current_request(interface_state));
	if (!refreshed.valid_disk && state.initialized && !state.review.disk_revision.empty()) {
		if (state.review.valid_disk)
			++state.review.counts.errors;
		state.review.can_commit = false;
		state.review.valid_disk = false;
		state.review.error = std::move(refreshed.error);
		state.stale = true;
		state.message_is_error = true;
		++state.refresh_count;
		return;
	}
	state.review = std::move(refreshed);
	state.memory_observations.clear();
	if (std::ranges::any_of(state.review.rows, [](const auto& row) {
			return row.disk_memory || row.proposed_memory;
		})) {
		const auto live = app.devMonitoring().memoryReporting().read_live();
		const auto targets = app.devMonitoring().memoryReporting().tuningTargetSnapshot();
		state.memory_observations.reserve(state.review.counts.domains[2]);
		for (const auto& row : state.review.rows) {
			if (!row.disk_memory && !row.proposed_memory)
				continue;
			const auto* definition =
				FlowUi::detail::find_initial_capacity(memory_capacity_target_id(row.field_path));
			if (!definition)
				continue;
			DevChangesMemoryObservation observation{};
			observation.key = row.key;
			observation.observed_ns = live.status.last_consumed_ns;
			observation.observed_tick = live.status.lastConsumedAppTick;
			const bool bytes = definition->unit == FlowUi::detail::InitialCapacityUnit::Bytes;
			const std::string units = bytes ? " bytes" : " entries";
			observation.startup =
				std::to_string(live.initial_capacities.get(definition->target)) + units;
			const auto& peak =
				live.capacity_high_water[static_cast<std::size_t>(definition->target)];
			if (peak.valid_fields & static_cast<uint16_t>(bytes ? MemoryValueField::LogicalLive
																: MemoryValueField::Objects)) {
				observation.demand =
					std::to_string(bytes ? peak.active_bytes : peak.active_entries) + units;
				observation.peak_ns = peak.peak_ns;
			}
			const auto target =
				std::ranges::find(targets, definition->id(), &MemoryTuningTargetDescriptor::id);
			if (target != targets.end()) {
				std::size_t count = 0;
				for (const auto& source : live.sources)
					if (source.key.source == target->source &&
						(source.value.value_fields &
						 static_cast<uint16_t>(bytes ? MemoryValueField::Backing
													 : MemoryValueField::Capacity))) {
						if (count++ >= 8) {
							observation.allocations += "Additional sources available in Memory.\n";
							break;
						}
						observation.allocations +=
							"Window " + std::to_string(source.key.window) + ": " +
							std::to_string(bytes ? source.value.backingAllocatedBytes
												 : source.value.capacityCount) +
							units + " · sampled tick " + std::to_string(source.lastSampledTick) +
							"\n";
					}
			}
			state.memory_observations.emplace_back(std::move(observation));
		}
	}
	if (!state.memory_error.empty())
		state.review.can_commit = false;
	state.stale = false;
	state.initialized = true;
	++state.refresh_count;
	rebuild_changes_model(state);
	interface_state.unbakedChangeCount = static_cast<uint32_t>(
		std::min<std::size_t>(state.review.counts.pending(), std::numeric_limits<uint32_t>::max()));
}
void navigate(App& app, DevInterfaceState& interface_state) {
	auto& state = interface_state.changes;
	const auto* selected = selected_changes_row(state);
	if (!selected) {
		state.message = "Select an individual change to open its authoring surface.";
		return;
	}
	if (selected->domain == DevChangesDomain::Memory) {
		interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Memory);
		auto& analysis = interface_state.memory_workbench.analysis;
		const auto owner = changes_memory_owner(
			selected->key.starts_with("memory:policy.") ? selected->key : selected->field_path);
		for (const auto& node : memory_selector_nodes)
			if (lowercase(node.label) == lowercase(owner))
				interface_state.memory_selection.selected_node = static_cast<uint64_t>(node.id);
		if (owner == "Storage" || owner == "Shared policy")
			interface_state.memory_selection.selected_node =
				static_cast<uint64_t>(DevMemoryNodeId::SharedCpuStorage);
		if (owner == "Ui")
			interface_state.memory_selection.selected_node =
				static_cast<uint64_t>(DevMemoryNodeId::UiLayout);
		if (owner == "Field")
			interface_state.memory_selection.selected_node =
				static_cast<uint64_t>(DevMemoryNodeId::TextPayload);
		for (const auto& target : analysis.targets) {
			if (target.id == memory_capacity_target_id(selected->field_path) ||
				target.configKey == selected->field_path) {
				analysis.target = target.id;
				analysis.capacity_tool.selected_target = target.id;
				break;
			}
		}
		for (const auto& draft : analysis.staged)
			if (draft.target.id == memory_capacity_target_id(selected->field_path)) {
				analysis.capacity_tool.scope = draft.origin_scope;
				if (memory_node(draft.origin_scope))
					interface_state.memory_selection.selected_node = draft.origin_scope;
				break;
			}
		state.message = "Opened Memory; prepared initial hints never resize running allocations.";
	} else {
		const auto& entry =
			selected->proposed_entry ? selected->proposed_entry : selected->disk_entry;
		if (!entry) {
			state.message = "This unresolved proposal has no stable authoring target.";
			return;
		}
		interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Inspect);
		if (selected->domain == DevChangesDomain::Themes) {
			interface_state.inspectForest = interface_elements::kDevInterfaceThemeForest;
			interface_state.inspect_theme_type = entry->themeType;
			interface_state.inspect_theme_variant =
				entry->themeVariant == "*" ? "" : entry->themeVariant;
			interface_state.inspect_theme_all_variants = entry->themeVariant == "*";
			interface_state.inspect_theme_group = entry->fieldId;
			interface_state.inspect_theme_tab = 1;
			interface_state.inspect_theme_key = interface_elements::stableNodeKey(
				entry->themeType,
				entry->themeVariant == "*"
					? 0x5448454d45ull
					: ::FlowUi::detail::identity_hash::hashBytes(entry->themeVariant));
		} else {
			interface_state.inspectForest = interface_elements::kDevInterfaceFlowForest;
			interface_state.inspectDefinitionFilter = entry->definition.value;
			interface_state.inspect_theme_type = 0;
			interface_state.selectedElementId = {};
			interface_state.inspectSelectedNodeKind = 0;
			interface_state.inspectSelectedNodeKey = 0;
			for (const auto& window : app.devWindowSnapshot()) {
				const auto& tree = app.ui(window.id).devTreeSnapshot();
				const auto found = std::ranges::find_if(tree.flow.nodes, [&](const auto& node) {
					return node.definition == entry->definition &&
						   (entry->targetScope == DevOverrideScope::Definition ||
							node.instance == entry->instanceKey);
				});
				if (found != tree.flow.nodes.end()) {
					interface_state.selectedWindowId = window.id;
					interface_state.selectedElementId = FlowElementID{found->instance.value};
					interface_state.inspectSelectedNodeKind =
						interface_elements::kDevInterfaceFlowNodeKind;
					interface_state.inspectSelectedNodeKey = found->instance.value;
					interface_state.inspect_reveal_frames = 3;
					break;
				}
			}
		}
		state.message =
			"Opened Inspect for the already baked target; Changes selection is retained.";
	}
	state.return_available = true;
}
} // namespace

std::string_view changes_decision_name(tooling::DevChangesDecision decision) noexcept {
	switch (decision) {
	case DevChangesDecision::IncludeProposal:
		return "Include proposal";
	case DevChangesDecision::OmitProposal:
		return "Omitted";
	case DevChangesDecision::RetainDisk:
		return "Keep already baked value";
	case DevChangesDecision::Remove:
		return "Remove from next bake";
	}
	return "Unknown decision";
}
std::string changes_summary(const DevChangesReviewCounts& counts) {
	return std::to_string(counts.additions) + " added · " + std::to_string(counts.updates) +
		   " updated · " + std::to_string(counts.removals) + " removed · " +
		   std::to_string(counts.retained) + " kept · " + std::to_string(counts.omitted) +
		   " omitted";
}
std::string changes_memory_owner(std::string_view key) {
	if (key.starts_with("policy.") || key.starts_with("memory:policy."))
		return "Shared policy";
	if (key.starts_with("flowui.tuning."))
		key.remove_prefix(14);
	const auto separator = key.find('.');
	std::string owner(key.substr(0, separator));
	for (auto& character : owner)
		if (character == '_')
			character = ' ';
	if (!owner.empty())
		owner.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(owner.front())));
	return owner.empty() ? "Unresolved already baked targets" : owner;
}
const DevChangesNode* selected_changes_node(const DevChangesState& state) noexcept {
	const auto found = std::ranges::find(state.nodes, state.selected_key, &DevChangesNode::key);
	return found == state.nodes.end() ? nullptr : &*found;
}
const DevChangesReviewRow* selected_changes_row(const DevChangesState& state) noexcept {
	const auto* node = selected_changes_node(state);
	return node && node->leaf && !node->rows.empty() ? &state.review.rows[node->rows.front()]
													 : nullptr;
}
void rebuild_changes_model(DevChangesState& state) {
	const bool filters_changed = state.search != state.applied_search ||
								 state.domain != state.applied_domain ||
								 state.status != state.applied_status;
	auto previous_nodes = std::move(state.nodes);
	state.nodes.clear();
	state.nodes.reserve(state.review.rows.size() * 4 + 3);
	state.matching_rows.clear();
	state.matching_rows.reserve(state.review.rows.size());
	auto add_node = [&](std::string key, std::string parent, std::string label, bool leaf,
						std::size_t row_index) {
		auto found = std::ranges::find(state.nodes, key, &DevChangesNode::key);
		if (found == state.nodes.end()) {
			DevChangesNode node{};
			node.key = std::move(key);
			node.parent = std::move(parent);
			node.label = std::move(label);
			node.leaf = leaf;
			node.identity = identity(node.key);
			const auto previous = std::ranges::find(previous_nodes, node.key, &DevChangesNode::key);
			if (previous != previous_nodes.end())
				node.expanded = previous->expanded;
			state.nodes.emplace_back(std::move(node));
			found = state.nodes.end() - 1;
		}
		found->rows.emplace_back(row_index);
		found->matches = found->matches || row_matches(state, state.review.rows[row_index]);
		return found->key;
	};
	for (std::size_t row_index = 0; row_index < state.review.rows.size(); ++row_index) {
		const auto& row = state.review.rows[row_index];
		if (row_matches(state, row))
			state.matching_rows.emplace_back(row_index);
		const std::string root_key = "group:" + std::string(domain_name(row.domain));
		std::string parent =
			add_node(root_key, {}, std::string(domain_name(row.domain)), false, row_index);
		const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
		std::string owner =
			row.domain == DevChangesDomain::Memory
				? changes_memory_owner(row.key.starts_with("memory:policy.") ? row.key
																			 : row.field_path)
				: row.target_name;
		if (owner.empty())
			owner = "Unresolved already baked targets";
		const std::string owner_key = root_key + ":" +
									  (entry ? std::to_string(row.domain == DevChangesDomain::Themes
																  ? entry->themeType
																  : entry->definition.value)
											 : owner);
		parent = add_node(owner_key, parent, owner, false, row_index);
		if (entry && row.domain == DevChangesDomain::Elements &&
			entry->targetScope == DevOverrideScope::ExactInstance) {
			parent = add_node(owner_key + ":instances", parent, "Instances", false, row_index);
			const auto instance = std::to_string(entry->instanceKey.value);
			parent = add_node(
				owner_key + ":instance:" + instance, parent,
				(entry->instanceDebugLabel.empty() ? "Instance" : entry->instanceDebugLabel) +
					" · " + instance,
				false, row_index);
		} else if (entry && row.domain == DevChangesDomain::Themes && entry->themeVariant != "*") {
			parent = add_node(owner_key + ":variant:" + entry->themeVariant, parent,
							  entry->themeVariant.empty() ? "Default variant" : entry->themeVariant,
							  false, row_index);
		}
		std::string label = row.field_path.empty() ? row.target_name : row.field_path;
		if (row.domain == DevChangesDomain::Memory && label.starts_with("flowui.tuning."))
			label.erase(0, 14);
		if (entry && row.domain == DevChangesDomain::Themes && entry->themeVariant == "*")
			label += " · All variants";
		add_node(row.key, parent, std::move(label), true, row_index);
	}
	state.visible_nodes.clear();
	state.visible_nodes.reserve(state.nodes.size());
	auto append_children = [&](auto&& self, std::string_view parent, uint16_t depth) -> void {
		std::vector<std::size_t> children;
		children.reserve(state.nodes.size());
		for (std::size_t node_index = 0; node_index < state.nodes.size(); ++node_index)
			if (state.nodes[node_index].parent == parent && state.nodes[node_index].matches)
				children.emplace_back(node_index);
		std::ranges::sort(children, [&](auto left, auto right) {
			const auto& left_node = state.nodes[left];
			const auto& right_node = state.nodes[right];
			return left_node.leaf != right_node.leaf ? left_node.leaf
													 : left_node.label < right_node.label;
		});
		for (const auto node_index : children) {
			auto& node = state.nodes[node_index];
			const bool hide_domain = state.domain && node.parent.empty();
			node.depth = depth;
			if (!hide_domain)
				state.visible_nodes.emplace_back(node_index);
			if (!node.leaf && (node.expanded || !state.search.empty() || hide_domain))
				self(self, node.key, depth + (hide_domain ? 0 : 1));
		}
	};
	append_children(append_children, {}, 0);
	const auto selected = std::ranges::find(state.nodes, state.selected_key, &DevChangesNode::key);
	state.selected_outside_filter =
		!state.selected_key.empty() && (selected == state.nodes.end() || !selected->matches);
	if (selected == state.nodes.end() && !state.selected_key.empty() && !state.message_is_error &&
		std::ranges::find(previous_nodes, state.selected_key, &DevChangesNode::key) !=
			previous_nodes.end()) {
		state.message =
			"The selected target is no longer present in this review: " + state.selected_key +
			". Its context is retained; choose a group or Overview to continue.";
	}
	if (!state.queue_scope_key.empty() && !state.message_is_error &&
		std::ranges::find(state.nodes, state.queue_scope_key, &DevChangesNode::key) ==
			state.nodes.end() &&
		std::ranges::find(previous_nodes, state.queue_scope_key, &DevChangesNode::key) !=
			previous_nodes.end())
		state.message =
			"The review scope is no longer available: " + state.queue_scope_key +
			". Choose a group or Overview to continue; existing decisions are retained.";
	if (filters_changed && selected != state.nodes.end() && selected->leaf && selected->matches) {
		const auto rows = changes_queue_rows(state);
		const auto position = std::ranges::find(rows, selected->rows.front());
		if (position != rows.end()) {
			const auto page = static_cast<uint64_t>(position - rows.begin()) / 40;
			if (page != state.workbench_page) {
				state.workbench_page = page;
				state.workbench_page_changed = true;
			}
		}
	}
	state.applied_search = state.search;
	state.applied_domain = state.domain;
	state.applied_status = state.status;
	state.model_dirty = false;
	if (std::exchange(state.reveal_selection_pending, false)) {
		changes_select_key(state, state.selected_key);
		state.model_dirty = false;
	}
	++state.model_revision;
}
std::vector<std::size_t> changes_queue_rows(const DevChangesState& state) {
	std::vector<std::size_t> rows;
	rows.reserve(state.matching_rows.size());
	const auto scope = std::ranges::find(state.nodes, state.queue_scope_key, &DevChangesNode::key);
	if (!state.queue_scope_key.empty() && scope == state.nodes.end())
		return rows;
	for (const auto row_index : state.matching_rows)
		if (scope == state.nodes.end() ||
			std::ranges::find(scope->rows, row_index) != scope->rows.end())
			rows.emplace_back(row_index);
	return rows;
}
void changes_select_key(DevChangesState& state, std::string_view key) {
	state.selected_key = key;
	state.view = 0;
	state.group_decisions_open = false;
	++state.selection_revision;
	const auto selected = std::ranges::find(state.nodes, state.selected_key, &DevChangesNode::key);
	if (selected == state.nodes.end()) {
		state.selected_outside_filter = !state.selected_key.empty();
		state.model_dirty = true;
		return;
	}
	state.focused_identity = selected->identity;
	if (!selected->leaf) {
		if (state.queue_scope_key != selected->key) {
			state.queue_scope_key = selected->key;
			state.workbench_page = 0;
			state.workbench_page_changed = true;
		}
	} else {
		const auto scope =
			std::ranges::find(state.nodes, state.queue_scope_key, &DevChangesNode::key);
		if (!state.queue_scope_key.empty() &&
			(scope == state.nodes.end() ||
			 std::ranges::find(scope->rows, selected->rows.front()) == scope->rows.end())) {
			state.queue_scope_key = selected->parent;
			if (!state.message_is_error)
				state.message = "Queue scope changed to reveal the selected target.";
		}
		const auto rows = changes_queue_rows(state);
		const auto found = std::ranges::find(rows, selected->rows.front());
		if (found != rows.end()) {
			const auto page = static_cast<uint64_t>(found - rows.begin()) / 40;
			if (page != state.workbench_page) {
				state.workbench_page = page;
				state.workbench_page_changed = true;
			}
		}
	}
	state.selected_outside_filter = !selected->matches;
	state.model_dirty = true;
}
void queue_changes_scope_decision(DevChangesState& state, std::string_view scope_key,
								  DevChangesDecision decision) {
	state.pending_keys.clear();
	state.pending_skipped = 0;
	state.pending = DevChangesCommand::None;
	const auto scope = std::ranges::find(state.nodes, scope_key, &DevChangesNode::key);
	const auto* selected = scope == state.nodes.end() ? nullptr : &*scope;
	if (!scope_key.empty() && !selected) {
		state.message = "This decision scope is no longer available. Refresh the review.";
		state.message_is_error = true;
		return;
	}
	state.pending_keys.reserve(selected ? selected->rows.size() : state.review.rows.size());
	auto include = [&](const DevChangesReviewRow& row) {
		if (decision == DevChangesDecision::IncludeProposal) {
			if (!row.has_proposal)
				return;
			if (row.blocking && !(selected && selected->leaf)) {
				++state.pending_skipped;
				return;
			}
		}
		if (decision == DevChangesDecision::OmitProposal && !row.has_proposal)
			return;
		if ((decision == DevChangesDecision::Remove ||
			 decision == DevChangesDecision::RetainDisk) &&
			!row.has_disk)
			return;
		state.pending_keys.emplace_back(row.key);
	};
	if (selected)
		for (const auto row_index : selected->rows)
			include(state.review.rows[row_index]);
	else
		for (const auto& row : state.review.rows)
			include(row);
	state.pending_decision = decision;
	state.pending = DevChangesCommand::Decide;
	const auto action = decision == DevChangesDecision::IncludeProposal ? "Use proposal"
						: decision == DevChangesDecision::OmitProposal	? "Excluded"
						: decision == DevChangesDecision::RetainDisk
							? "Kept already baked value"
							: "Removed already baked assignment";
	state.feedback_label =
		std::string(action) + " · " +
		(selected && selected->leaf ? state.review.rows[selected->rows.front()].target_name + "." +
										  state.review.rows[selected->rows.front()].field_path
		 : selected					? selected->label
									: "Whole plan");
}
void queue_changes_decision(DevChangesState& state, DevChangesDecision decision) {
	const auto* selected = selected_changes_node(state);
	queue_changes_scope_decision(
		state, selected ? std::string_view(selected->key) : std::string_view{}, decision);
}
void service_changes(App& app, DevInterfaceState& interface_state, bool visible) {
	auto& state = interface_state.changes;
	const bool review_visible =
		visible && interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Changes);
	const bool entered = review_visible && !state.was_visible;
	state.was_visible = review_visible;
	auto command = std::exchange(state.pending, DevChangesCommand::None);
	if (command == DevChangesCommand::StagePolicy && state.pending_policy == 1 &&
		(!state.policy_growth_valid || !std::isfinite(state.policy_growth_draft) ||
		 state.policy_growth_draft <= 1)) {
		command = DevChangesCommand::None;
		state.message = "Complete a finite growth factor greater than one before staging; the "
						"draft is retained.";
		state.message_is_error = true;
	}
	const auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
											   std::chrono::steady_clock::now().time_since_epoch())
											   .count());
	if (!state.initialized) {
		refresh(app, interface_state);
		state.last_check_ns = now;
	}
	if (entered && command == DevChangesCommand::None) {
		refresh(app, interface_state);
		state.last_check_ns = now;
	}
	const bool decision_requires_refresh =
		(command == DevChangesCommand::Decide || command == DevChangesCommand::StagePolicy ||
		 command == DevChangesCommand::Undo || command == DevChangesCommand::Redo) &&
		(state.stale ||
		 current_request(interface_state).memory_revision != state.review.memory_revision);
	auto& overrides = app.devTooling().overrides();
	if (state.runtime_transaction) {
		std::size_t acknowledged = 0;
		bool accepted = true;
		std::string_view failure_reason{};
		for (const auto& result : overrides.commandResults())
			if (result.transaction == state.runtime_transaction) {
				++acknowledged;
				accepted = accepted && result.applied;
				if (!result.applied && failure_reason.empty())
					failure_reason = runtime_failure_reason(result.status);
			}
		const auto expected = state.runtime_mode == 1 ? state.pending_runtime.inverse.size()
													  : state.pending_runtime.forward.size();
		if (acknowledged >= expected) {
			state.runtime_transaction = 0;
			if (accepted) {
				if (state.runtime_mode == 1) {
					state.runtime_redo.emplace_back(std::move(state.runtime_undo.back()));
					state.runtime_undo.pop_back();
				} else if (state.runtime_mode == 2) {
					state.runtime_undo.emplace_back(std::move(state.runtime_redo.back()));
					state.runtime_redo.pop_back();
				} else {
					state.runtime_undo.emplace_back(std::move(state.pending_runtime));
					state.runtime_redo.clear();
				}
				state.runtime_message = "Live command acknowledged for " +
										state.runtime_target_label +
										". Manifest decisions are unchanged.";
				state.runtime_message_is_error = false;
				refresh(app, interface_state);
			} else {
				state.runtime_message = "Live command rejected for " + state.runtime_target_label +
										": " + std::string(failure_reason) +
										" Draft, manifest decisions and undo history are retained.";
				state.runtime_message_is_error = true;
			}
		}
	}
	if (command >= DevChangesCommand::ApplyProposal && command <= DevChangesCommand::RedoLive &&
		!state.runtime_transaction) {
		state.runtime_mode = command == DevChangesCommand::UndoLive	  ? 1
							 : command == DevChangesCommand::RedoLive ? 2
																	  : 0;
		if (state.runtime_mode) {
			state.runtime_target_label = state.runtime_mode == 1 ? "the previous live edit (Undo)"
																 : "the recorded live edit (Redo)";
			const auto& history = state.runtime_mode == 1 ? state.runtime_undo : state.runtime_redo;
			if (!history.empty())
				state.pending_runtime = history.back();
			else
				state.pending_runtime = {};
		} else if (const auto* row = selected_changes_row(state)) {
			state.runtime_target_label = row->target_name + "." + row->field_path;
			const auto schema = app.devTooling().schemas().view();
			if (schema) {
				const auto action = command == DevChangesCommand::ApplySaved
										? DevChangesRuntimeAction::ApplySaved
									: command == DevChangesCommand::DiscardLive
										? DevChangesRuntimeAction::DiscardLive
									: command == DevChangesCommand::ResetAuthored
										? DevChangesRuntimeAction::ResetAuthored
										: DevChangesRuntimeAction::ApplyProposal;
				state.pending_runtime =
					prepare_changes_runtime_edit(*schema, overrides, app.themes(), *row,
												 interface_state.selectedWindowId, action);
			} else {
				state.pending_runtime = {};
				state.pending_runtime.error = "Runtime schema unavailable.";
			}
		} else {
			state.pending_runtime = {};
			state.pending_runtime.error = "Select an individual element or theme change.";
		}
		if (state.pending_runtime.available) {
			DevChangeSet change{};
			change.transaction = interface_state.nextEditTransaction++;
			change.commands = state.runtime_mode == 1 ? state.pending_runtime.inverse
													  : state.pending_runtime.forward;
			if (overrides.submit(change)) {
				state.runtime_transaction = change.transaction;
				state.runtime_message = "Live command queued for " + state.runtime_target_label +
										"; waiting for application acknowledgement.";
				state.runtime_message_is_error = false;
			} else {
				state.runtime_message =
					"Live edit queue is full; the draft is retained. Try again.";
				state.runtime_message_is_error = true;
			}
		} else {
			state.runtime_message = state.pending_runtime.error.empty()
										? "No live edit to restore."
										: state.pending_runtime.error;
			state.runtime_message_is_error = true;
		}
	}
	if (command == DevChangesCommand::Refresh) {
		refresh(app, interface_state);
		state.message =
			state.review.valid_disk ? "Review refreshed from disk, live edits and prepared memory."
			: state.review.disk_revision.empty()
				? "Refresh failed. " + state.review.error
				: "Refresh failed; the last valid review remains visible. " + state.review.error;
		state.message_is_error = !state.review.valid_disk || !state.memory_error.empty();
	}
	if (command == DevChangesCommand::Decide && !state.pending_keys.empty()) {
		std::size_t affected_targets = state.pending_keys.size();
		const bool removes_memory_section =
			state.pending_decision == DevChangesDecision::Remove &&
			std::ranges::find(state.pending_keys, "memory:policy.enabled") !=
				state.pending_keys.end();
		if (removes_memory_section)
			for (const auto& row : state.review.rows)
				if (row.domain == DevChangesDomain::Memory && row.has_disk &&
					std::ranges::find(state.pending_keys, row.key) == state.pending_keys.end())
					++affected_targets;
		state.undo.emplace_back(DevChangesDecisionSnapshot{state.decisions, state.policies});
		if (state.undo.size() > 100)
			state.undo.erase(state.undo.begin());
		state.redo.clear();
		for (const auto& key : state.pending_keys) {
			const auto found =
				std::ranges::find(state.decisions, key, &DevChangesDecisionEntry::key);
			if (found == state.decisions.end())
				state.decisions.emplace_back(DevChangesDecisionEntry{key, state.pending_decision,
																	 state.review.disk_revision});
			else {
				found->decision = state.pending_decision;
				found->disk_revision = state.review.disk_revision;
			}
		}
		state.message = state.feedback_label + " · " + std::to_string(affected_targets) +
						" targets in next bake. Live behavior is unchanged.";
		if (removes_memory_section)
			state.message += " The entire already baked memory section is removed.";
		if (state.pending_skipped)
			state.message += " Skipped " + std::to_string(state.pending_skipped) +
							 " blocked proposals; resolve their issues before including them.";
		state.message_is_error = false;
		refresh(app, interface_state);
	}
	if (command == DevChangesCommand::StagePolicy) {
		state.undo.emplace_back(DevChangesDecisionSnapshot{state.decisions, state.policies});
		if (state.undo.size() > 100)
			state.undo.erase(state.undo.begin());
		state.redo.clear();
		std::string policy_key = "memory:policy.enabled";
		if (state.pending_policy <= 2)
			state.policies.clear_section = false;
		if (state.pending_policy == 0) {
			state.policies.enabled = state.policy_enabled_choice != 0;
			state.policies.clear_section = false;
		}
		if (state.pending_policy == 1) {
			state.policies.growth_factor = static_cast<float>(state.policy_growth_draft);
			state.policies.clear_growth_factor = false;
			policy_key = "memory:policy.growth_factor";
		}
		if (state.pending_policy == 2) {
			state.policies.runtime_growth = state.policy_runtime_choice != 0;
			state.policies.clear_runtime_growth = false;
			policy_key = "memory:policy.allow_runtime_growth";
		}
		if (state.pending_policy == 3) {
			state.policies.growth_factor.reset();
			state.policies.clear_growth_factor = true;
			policy_key = "memory:policy.growth_factor";
		}
		if (state.pending_policy == 4) {
			state.policies.runtime_growth.reset();
			state.policies.clear_runtime_growth = true;
			policy_key = "memory:policy.allow_runtime_growth";
		}
		if (state.pending_policy == 5)
			state.policies.clear_section = true;
		const auto found =
			std::ranges::find(state.decisions, policy_key, &DevChangesDecisionEntry::key);
		if (found == state.decisions.end())
			state.decisions.emplace_back(DevChangesDecisionEntry{
				policy_key, DevChangesDecision::IncludeProposal, state.review.disk_revision});
		else {
			found->decision = DevChangesDecision::IncludeProposal;
			found->disk_revision = state.review.disk_revision;
		}
		refresh(app, interface_state);
		state.view = 0;
		changes_select_key(state, policy_key);
		state.message = "Shared policy staged for the next manifest. Review the resulting "
						"operation before baking.";
		state.message_is_error = false;
	}
	if (command == DevChangesCommand::Undo || command == DevChangesCommand::Redo) {
		auto& source = command == DevChangesCommand::Undo ? state.undo : state.redo;
		auto& destination = command == DevChangesCommand::Undo ? state.redo : state.undo;
		if (!source.empty()) {
			destination.emplace_back(
				DevChangesDecisionSnapshot{std::move(state.decisions), state.policies});
			state.decisions = std::move(source.back().decisions);
			state.policies = source.back().policies;
			source.pop_back();
			refresh(app, interface_state);
			state.message = "Manifest decision restored; live behavior is unchanged.";
			state.message_is_error = false;
		}
	}
	if (command == DevChangesCommand::Bake) {
		if (state.stale || !state.review.can_commit || !state.memory_error.empty()) {
			state.message =
				"Bake unavailable: refresh this review and resolve its blocking issues.";
			state.message_is_error = true;
		} else {
			const auto counts = state.review.counts;
			const auto result = app.devTooling().bakePipeline().commit_review(
				state.review, current_request(interface_state).memory_revision);
			if (result.applied) {
				state.initialized = false;
				refresh(app, interface_state);
				state.message = "Manifest updated. Rebuild the application to use it. " +
								changes_summary(counts);
				state.message_is_error = false;
				interface_state.lastActionMessage = state.message;
			} else {
				state.stale = true;
				state.message =
					"Bake rejected: the reviewed revision changed or the file could not be "
					"written. Refresh to review the current disk state; pending work is retained.";
				for (const auto& diagnostic :
					 app.devTooling().bakePipeline().queryStatus().bakeDiagnostics)
					if (!diagnostic.message.empty())
						state.message += " " + diagnostic.message;
				state.message_is_error = true;
			}
		}
	}
	if (decision_requires_refresh) {
		state.stale = true;
		state.message = "Decision retained. Prepared inputs changed outside this review; "
						"explicitly refresh before baking.";
		state.message_is_error = false;
	}
	if (command == DevChangesCommand::Edit)
		navigate(app, interface_state);
	if (command == DevChangesCommand::Back) {
		interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Changes);
		state.return_available = false;
	}
	if (visible && now - state.last_check_ns >= 1'000'000'000ull) {
		state.last_check_ns = now;
		const auto current = app.devTooling().bakePipeline().query_review(state.review.request);
		if (current.disk_revision != state.review.disk_revision ||
			current.schema_generation != state.review.schema_generation ||
			current.proposal_revision != state.review.proposal_revision ||
			current_request(interface_state).memory_revision != state.review.memory_revision) {
			if (review_visible) {
				state.stale = true;
				if (!state.message_is_error)
					state.message = "Sources changed since this review. Refresh before baking.";
			} else
				refresh(app, interface_state);
		}
	}
	if (state.model_dirty || state.search != state.applied_search ||
		state.domain != state.applied_domain || state.status != state.applied_status)
		rebuild_changes_model(state);
}
} // namespace FlowUi::devSystems
#endif
