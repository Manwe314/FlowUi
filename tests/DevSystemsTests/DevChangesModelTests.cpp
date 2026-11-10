#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Changes/DevChangesModel.hpp"
#include "devSystems/devTooling/bake/DevChangesRuntime.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::tooling;
namespace changes_test {
struct Nested {
	float opacity = 0.25f;
};
struct Parameters {
	Nested nested{};
	float amount = 1;
};
struct Element {
	using Parameters = changes_test::Parameters;
	static constexpr FlowDefinitionID definitionId = DefinitionID("changes.runtime.fixture");
	static void buildElement(ElementBuildContext<Element>&) {}
};
FLOWUI_DEV_SCHEMA(Nested, FLOWUI_DEV_FIELD(Nested, opacity))
FLOWUI_DEV_SCHEMA(Parameters, FLOWUI_DEV_FIELD(Parameters, nested),
				  FLOWUI_DEV_FIELD(Parameters, amount,
								   devSystems::tooling::DevFieldOptions{}.numericRange(0, 10)))

void verify_runtime_intents() {
	devSystems::tooling::DevSchemaRegistry registry;
	DevOverrideEngine engine(registry);
	ThemeManager themes;
	registry.ensureElement<Element>();
	assert(registry.publishPendingAtSafePoint());
	const auto schema = registry.view();
	const auto* element = schema->findElement(Element::definitionId);
	assert(element);
	const auto fields = schema->fieldsOf(element->parametersType);
	const auto field = std::ranges::find_if(
		fields, [&](const auto& candidate) { return schema->string(candidate.name) == "amount"; });
	assert(field != fields.end());
	DevOverrideCommand set;
	set.element = {.definition = Element::definitionId, .scope = DevOverrideScope::Definition};
	set.field = {schema->type(element->parametersType)->id, field->id};
	assert(engine.copyValue(3.0f, set.value) ==
		   devSystems::tooling::DevValueOperationStatus::Success);
	assert(engine.submit({.transaction = 1, .commands = {set}}));
	engine.commitAtSafePoint(themes);
	assert(engine.commandResults().front().applied);
	DevBakeEntry saved;
	saved.definition = Element::definitionId;
	saved.fieldId = field->id;
	saved.fieldPath = "amount";
	saved.valueType = schema->type(field->valueType)->id;
	saved.valueJson = "6";
	DevChangesReviewRow row;
	row.disk_entry = saved;
	row.proposed_entry = saved;
	row.has_disk = row.has_proposal = true;
	auto edit = prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
											 DevChangesRuntimeAction::ApplySaved);
	assert(edit.available && edit.forward.size() == 1 && edit.inverse.size() == 1);
	assert(edit.forward.front().preserve_baked_assignment);
	assert(engine.submit({.transaction = 2, .commands = edit.forward}));
	engine.commitAtSafePoint(themes);
	Parameters parameters;
	engine.applyElement(Element::definitionId, 7, {.value = 11}, &parameters);
	assert(parameters.amount == 6);
	assert(engine.submit({.transaction = 3, .commands = edit.inverse}));
	engine.commitAtSafePoint(themes);
	parameters = {};
	engine.applyElement(Element::definitionId, 7, {.value = 11}, &parameters);
	assert(parameters.amount == 3);
	edit = prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										DevChangesRuntimeAction::DiscardLive);
	assert(edit.available &&
		   edit.forward.front().kind == DevOverrideCommandKind::ClearElementField);
	assert(engine.submit({.transaction = 4, .commands = edit.forward}));
	engine.commitAtSafePoint(themes);
	assert(engine.appliedOverrides().records().empty());
	assert(engine.appliedOverrides().bakeTombstones().empty());
	parameters = {};
	engine.applyElement(Element::definitionId, 7, {.value = 11}, &parameters);
	assert(parameters.amount == 1);
	// A saved value requires an actual typed baseline; no effective value is invented.
	assert(!prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										 DevChangesRuntimeAction::ApplySaved)
				.available);
	assert(engine.submit({.transaction = 5, .commands = edit.inverse}));
	engine.commitAtSafePoint(themes);
	row.disk_entry->valueJson = "99";
	edit = prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										DevChangesRuntimeAction::ApplySaved);
	assert(edit.available);
	assert(engine.submit({.transaction = 6, .commands = edit.forward}));
	engine.commitAtSafePoint(themes);
	assert(!engine.commandResults().front().applied);
	assert(engine.commandResults().front().status == DevCommandStatus::ConstraintRejected);
	row.disk_entry->valueJson = "6 trailing";
	assert(!prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										 DevChangesRuntimeAction::ApplySaved)
				.available);
	row.disk_entry->valueType = 0;
	row.proposed_entry.reset();
	assert(!prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										 DevChangesRuntimeAction::ApplySaved)
				.available);
	// Composite materialization assigns reflected members into a constructed typed clone.
	const auto nested = std::ranges::find_if(
		fields, [&](const auto& candidate) { return schema->string(candidate.name) == "nested"; });
	assert(nested != fields.end());
	set.field.field = nested->id;
	assert(engine.copyValue(Nested{}, set.value) ==
		   devSystems::tooling::DevValueOperationStatus::Success);
	assert(engine.submit({.transaction = 7, .commands = {set}}));
	engine.commitAtSafePoint(themes);
	saved.fieldId = nested->id;
	saved.fieldPath = "nested";
	saved.valueType = schema->type(nested->valueType)->id;
	saved.valueJson = "{\"opacity\":0.75}";
	row.disk_entry = saved;
	edit = prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										DevChangesRuntimeAction::ApplySaved);
	assert(edit.available);
	assert(engine.submit({.transaction = 8, .commands = edit.forward}));
	engine.commitAtSafePoint(themes);
	parameters = {};
	engine.applyElement(Element::definitionId, 7, {.value = 11}, &parameters);
	assert(parameters.nested.opacity == 0.75f);
	row.disk_entry->valueJson = "{\"opacity\":0.75,\"opacity\":0.5}";
	assert(!prepare_changes_runtime_edit(*schema, engine, themes, row, 7,
										 DevChangesRuntimeAction::ApplySaved)
				.available);
}

void verify_model_identity_and_scope() {
	DevChangesState state;
	state.review.rows.reserve(4);
	for (std::size_t index = 0; index < 4; ++index) {
		DevChangesReviewRow row;
		row.key = "fixture:" + std::to_string(index);
		row.target_name = "Button";
		row.field_path = "layout.padding" + std::to_string(index);
		row.has_proposal = true;
		row.operation = DevChangesOperation::Add;
		DevBakeEntry entry;
		entry.ownerCppType = "FixtureParameters";
		entry.fieldPath = row.field_path;
		if (index == 1) {
			entry.targetScope = DevOverrideScope::ExactInstance;
			entry.instanceKey.value = 42;
			entry.instanceDebugLabel = "toolbar.save";
		}
		if (index == 2) {
			row.domain = DevChangesDomain::Themes;
			entry.targetKind = DevBakeTargetKind::Theme;
			entry.themeVariant = "*";
		}
		if (index == 3) {
			row.domain = DevChangesDomain::Memory;
			row.field_path = "flowui.tuning.storage.initial_frame_bytes";
		}
		row.proposed_entry = entry;
		state.review.rows.emplace_back(std::move(row));
	}
	state.review.counts.additions = 4;
	rebuild_changes_model(state);
	assert(state.nodes.size() > state.review.rows.size());
	state.selected_key = "fixture:1";
	assert(selected_changes_row(state) == &state.review.rows[1]);
	const auto identity = selected_changes_node(state)->identity;
	state.search = "ALL VARIANTS";
	rebuild_changes_model(state);
	assert(state.selected_outside_filter && state.selected_key == "fixture:1");
	assert(selected_changes_node(state)->identity == identity);
	assert(state.review.counts.pending() == 4);
	// Search never narrows a whole-manifest group decision.
	state.selected_key.clear();
	queue_changes_decision(state, DevChangesDecision::OmitProposal);
	assert(state.pending_keys.size() == 4 && state.pending == DevChangesCommand::Decide);
	state.selected_key = "fixture:1";
	queue_changes_decision(state, DevChangesDecision::OmitProposal);
	assert(state.pending_keys.size() == 1 && state.pending_keys.front() == "fixture:1");
	state.search.clear();
	state.domain = 1;
	rebuild_changes_model(state);
	assert(!state.selected_outside_filter && selected_changes_node(state)->identity == identity);
	for (const auto node_index : state.visible_nodes)
		assert(state.nodes[node_index].key != "group:Elements");
	const auto parent_key = selected_changes_node(state)->parent;
	const auto parent = std::ranges::find(state.nodes, parent_key, &DevChangesNode::key);
	assert(parent != state.nodes.end());
	parent->expanded = false;
	rebuild_changes_model(state);
	assert(!state.selected_outside_filter && selected_changes_row(state));
	assert(changes_memory_owner("flowui.tuning.input_fields.registry_entries") == "Input fields");
}

void verify_queue_projection_and_explicit_scope() {
	DevChangesState state;
	state.review.rows.reserve(85);
	for (std::size_t index = 0; index < 85; ++index) {
		DevChangesReviewRow row;
		row.key = "queue:" + std::to_string(index);
		row.target_name = "Toolbar";
		row.field_path = "field_" + std::to_string(index);
		row.has_proposal = true;
		row.operation = DevChangesOperation::Add;
		state.review.rows.emplace_back(std::move(row));
	}
	rebuild_changes_model(state);
	assert(state.matching_rows.size() == 85 && changes_queue_rows(state).size() == 85);
	changes_select_key(state, "group:Elements");
	const auto group = std::ranges::find(state.nodes, "group:Elements", &DevChangesNode::key);
	assert(group != state.nodes.end());
	group->expanded = false;
	rebuild_changes_model(state);
	assert(changes_queue_rows(state).size() == 85 && state.visible_nodes.size() == 1);
	changes_select_key(state, "queue:45");
	assert(state.workbench_page == 1 && state.workbench_page_changed);
	state.workbench_page_changed = false;
	state.workbench_scroll = -300;
	changes_select_key(state, "queue:46");
	assert(state.workbench_page == 1 && !state.workbench_page_changed &&
		   state.workbench_scroll == -300);
	state.search = "queue:45";
	rebuild_changes_model(state);
	assert(state.selected_outside_filter &&
		   changes_queue_rows(state) == std::vector<std::size_t>{45});
	state.review.rows.front().blocking = true;
	queue_changes_scope_decision(state, "group:Elements", DevChangesDecision::IncludeProposal);
	assert(state.pending_keys.size() == 84 && state.pending_skipped == 1);
	queue_changes_decision(state, DevChangesDecision::OmitProposal);
	assert(state.pending_keys == std::vector<std::string>{"queue:46"});
	queue_changes_scope_decision(state, "missing-group", DevChangesDecision::OmitProposal);
	assert(state.pending_keys.empty() && state.pending == DevChangesCommand::None);
	assert(state.message_is_error);
	state.search.clear();
	state.reveal_selection_pending = true;
	rebuild_changes_model(state);
	assert(!state.selected_outside_filter && state.workbench_page == 1 && !state.model_dirty);
	state.review.rows.back().domain = DevChangesDomain::Themes;
	rebuild_changes_model(state);
	const auto previous_scope = state.queue_scope_key;
	changes_select_key(state, "queue:84");
	assert(state.queue_scope_key != previous_scope && state.selected_key == "queue:84");
	assert(changes_queue_rows(state) == std::vector<std::size_t>{84});
	state.queue_scope_key.clear();
	for (std::size_t index = 0; index < state.review.rows.size(); ++index)
		state.review.rows[index].has_disk = index >= 10;
	rebuild_changes_model(state);
	changes_select_key(state, "queue:80");
	assert(state.workbench_page == 2);
	state.view = 1;
	state.status = 2;
	rebuild_changes_model(state);
	assert(state.workbench_page == 1 && state.view == 1 && !state.selected_outside_filter);
	state.queue_scope_key = "group:Elements";
	state.message = "Rejected write remains visible";
	state.message_is_error = true;
	const auto selection_revision = state.selection_revision;
	changes_select_key(state, "queue:84");
	changes_select_key(state, "queue:84");
	assert(state.selection_revision == selection_revision + 2);
	assert(state.message == "Rejected write remains visible" && state.message_is_error);
	state.message_is_error = false;
	state.review.rows.pop_back();
	rebuild_changes_model(state);
	assert(state.selected_key == "queue:84" && state.selected_outside_filter);
	assert(state.message.find("no longer") != std::string::npos &&
		   changes_queue_rows(state).empty());
}
} // namespace changes_test
int main() {
	changes_test::verify_runtime_intents();
	changes_test::verify_model_identity_and_scope();
	changes_test::verify_queue_projection_and_explicit_scope();
	std::cout << "Changes model identity, filter scope and typed runtime intents verified\n";
}
