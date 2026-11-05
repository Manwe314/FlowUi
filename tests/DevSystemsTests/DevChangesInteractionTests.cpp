#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devTooling/bake/DevBakeInternals.hpp"
#include "FSEL/TextInput.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Catalogue/Inspector/DevCatalogueInspector.hpp"
#include "devSystems/devInterface/Catalogue/Selector/DevCatalogueSelector.hpp"
#include "devSystems/devInterface/Catalogue/Workbench/DevCatalogueWorkbench.hpp"
#include "devSystems/devInterface/Changes/DevChangesControls.hpp"
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "devSystems/devInterface/Changes/Inspector/DevChangesInspector.hpp"
#include "devSystems/devInterface/Changes/Selector/DevChangesSelector.hpp"
#include "devSystems/devInterface/Changes/Workbench/DevChangesWorkbench.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelector.hpp"
#include "devSystems/devInterface/Memory/Selector/DevMemorySelector.hpp"
#include "devSystems/devInterface/Performance/Selector/DevPerformanceSelector.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::tooling;
using namespace FlowUi::devSystems::interface_elements;
namespace {
constexpr auto selector_id = Global<kDevChangesSelector>("changes.test.selector");
constexpr auto workbench_id = Global<kDevChangesWorkbench>("changes.test.workbench");
constexpr auto inspector_id = Global<kDevChangesInspector>("changes.test.inspector");
constexpr auto controls_id = Global<kDevChangesControls>("changes.test.controls");
template <class Identity>
[[nodiscard]] FlowElementID local(FlowElementID parent, FlowDefinitionID definition,
								  const Identity& identity) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(parent, definition, identity.token);
}
void check_bounds(App& app, FlowElementID surface) {
	const auto& snapshot = app.ui().devTreeSnapshot();
	const auto panel = Clay_GetElementData(app.ui().toClayEID(surface));
	assert(panel.found && panel.boundingBox.width > 0);
	assert(panel.boundingBox.y >= 0 && panel.boundingBox.y + panel.boundingBox.height <= 901);
	const auto raw_scroll = app.ui().toClayEID(local(FlowElementID{workbench_id.value},
													 DevChangesWorkbench::definitionId,
													 LocalElementName{"raw-scroll"}));
	for (const auto& node : snapshot.clay.nodes) {
		auto owner = node.directFlowOwner;
		bool descendant = false;
		while (owner != InvalidFlowNode && owner < snapshot.flow.nodes.size()) {
			if (snapshot.flow.nodes[owner].instance.value == surface.value) {
				descendant = true;
				break;
			}
			owner = snapshot.flow.nodes[owner].parent;
		}
		if (!descendant || hasFlag(node.flags, DevClayNodeFlag::BoundsUnavailable))
			continue;
		bool in_raw_viewport = false;
		for (auto parent = node.parent;
			 parent != InvalidClayNode && parent < snapshot.clay.nodes.size();
			 parent = snapshot.clay.nodes[parent].parent)
			in_raw_viewport =
				in_raw_viewport || snapshot.clay.nodes[parent].clayId == raw_scroll.id;
		if (in_raw_viewport)
			continue;
		if (node.bounds.x < panel.boundingBox.x - 1 ||
			node.bounds.x + node.bounds.width > panel.boundingBox.x + panel.boundingBox.width + 1) {
			std::cerr << "Changes horizontal overflow: " << snapshot.string(node.idString)
					  << " text=" << snapshot.string(node.text) << " x=" << node.bounds.x
					  << " width=" << node.bounds.width << " panel=" << panel.boundingBox.x << ','
					  << panel.boundingBox.width << '\n';
			assert(false && "Changes content fits the resizable panel");
		}
	}
}
void check_assigned_width(App& app, FlowElementID surface, float assigned_width) {
	const auto panel = Clay_GetElementData(app.ui().toClayEID(surface));
	assert(panel.found);
	if (std::abs(panel.boundingBox.width - assigned_width) > 1)
		std::cerr << "Changes panel assigned width=" << assigned_width
				  << " actual root width=" << panel.boundingBox.width << '\n';
	assert(std::abs(panel.boundingBox.width - assigned_width) <= 1);
}
void check_centered_search(App& app, bool placeholder = false) {
	const auto search = local(FlowElementID{selector_id.value}, DevSelectorSearch::definitionId,
							  LocalElementName{"search"});
	const auto input =
		local(search, FSEL::TextInput::definitionId, LocalElementName{"search-input"});
	const auto content = Clay_GetElementData(Clay_ElementId{
		.id = FlowIDToClayID(PartID(FSEL::kTextInput, input, FSEL::TextInput::Parts::content))});
	const auto text = Clay_GetElementData(
		Clay_ElementId{.id = FlowIDToClayID(PartID(FSEL::kTextInput, input,
												   placeholder ? FSEL::TextInput::Parts::placeholder
															   : FSEL::TextInput::Parts::text))});
	assert(content.found && text.found && text.boundingBox.width > 0);
	assert(std::abs(content.boundingBox.x + content.boundingBox.width / 2 - text.boundingBox.x -
					text.boundingBox.width / 2) < 2);
	assert(std::abs(content.boundingBox.y + content.boundingBox.height / 2 - text.boundingBox.y -
					text.boundingBox.height / 2) < 2);
}
void check_centered_policy(App& app) {
	static constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.changes.policy.factor")>
		growth_input{};
	const auto identity =
		local(FlowElementID{workbench_id.value}, decltype(growth_input)::definitionId,
			  LocalElementName{"policy-growth-input"});
	const auto content = Clay_GetElementData(
		Clay_ElementId{.id = FlowIDToClayID(PartID(growth_input, identity,
												   decltype(growth_input)::Parts::content))});
	const auto text = Clay_GetElementData(Clay_ElementId{
		.id = FlowIDToClayID(PartID(growth_input, identity, decltype(growth_input)::Parts::text))});
	assert(content.found && text.found && text.boundingBox.width > 0);
	assert(std::abs(content.boundingBox.x + content.boundingBox.width / 2 - text.boundingBox.x -
					text.boundingBox.width / 2) < 2);
	assert(std::abs(content.boundingBox.y + content.boundingBox.height / 2 - text.boundingBox.y -
					text.boundingBox.height / 2) < 2);
}
void rebuild_fixture_output(DevChangesState& state) {
	auto& review = state.review;
	review.candidate = {};
	review.disk_manifest = {};
	review.candidate.manifestVersion = review.disk_manifest.manifestVersion = 2;
	review.candidate.entries.reserve(review.rows.size());
	review.disk_manifest.entries.reserve(review.rows.size());
	review.counts = {};
	for (const auto& row : review.rows) {
		++review.counts.domains[static_cast<std::size_t>(row.domain)];
		switch (row.operation) {
		case DevChangesOperation::Add:
			++review.counts.additions;
			break;
		case DevChangesOperation::Update:
			++review.counts.updates;
			break;
		case DevChangesOperation::Remove:
			++review.counts.removals;
			break;
		default:
			review.counts.retained += row.has_disk;
			break;
		}
		if (row.has_proposal && (row.decision == DevChangesDecision::OmitProposal ||
								 row.decision == DevChangesDecision::RetainDisk))
			++review.counts.omitted;
		if (row.blocking && row.validity == DevChangesValidity::Conflict)
			++review.counts.conflicts;
		else if (row.blocking)
			++review.counts.errors;
		else if (row.validity != DevChangesValidity::Ready)
			++review.counts.warnings;
		if (row.has_proposal) {
			if (row.proposed_memory)
				++review.counts.prepared_memory;
			else if (row.domain != DevChangesDomain::Memory)
				++review.counts.live_edits;
		}
		if (row.has_disk && row.disk_entry)
			review.disk_manifest.entries.emplace_back(*row.disk_entry);
		if (row.has_disk && row.disk_memory) {
			if (!review.disk_manifest.memoryCapacity)
				review.disk_manifest.memoryCapacity.emplace();
			review.disk_manifest.memoryCapacity->targets.emplace_back(*row.disk_memory);
		}
		if (row.operation == DevChangesOperation::Remove)
			continue;
		const bool use_proposal =
			row.has_proposal && row.decision == DevChangesDecision::IncludeProposal;
		const auto& entry = use_proposal ? row.proposed_entry : row.disk_entry;
		if (entry)
			review.candidate.entries.emplace_back(*entry);
		const auto& memory = use_proposal ? row.proposed_memory : row.disk_memory;
		if (memory) {
			if (!review.candidate.memoryCapacity)
				review.candidate.memoryCapacity.emplace();
			review.candidate.memoryCapacity->targets.emplace_back(*memory);
		}
		if (row.key.starts_with("memory:policy.")) {
			if (!review.candidate.memoryCapacity)
				review.candidate.memoryCapacity.emplace();
			if (!review.disk_manifest.memoryCapacity)
				review.disk_manifest.memoryCapacity.emplace();
			if (row.field_path == "enabled") {
				review.candidate.memoryCapacity->enabled = row.next_value == "true";
				review.disk_manifest.memoryCapacity->enabled = row.on_disk == "true";
			} else if (row.field_path == "growth_factor") {
				review.candidate.memoryCapacity->growth_factor = 2.0f;
				review.disk_manifest.memoryCapacity->growth_factor = 1.5f;
			} else if (row.field_path == "allow_runtime_growth") {
				review.candidate.memoryCapacity->allow_runtime_growth = row.next_value == "true";
				review.disk_manifest.memoryCapacity->allow_runtime_growth = row.on_disk == "true";
			}
		}
	}
	review.can_commit = review.valid_disk && review.error.empty() && review.counts.errors == 0 &&
						review.counts.conflicts == 0 && review.counts.pending() > 0;
	review.candidate_json = bake_detail::encode_manifest(review.candidate);
	rebuild_changes_model(state);
}
void seed_review(DevChangesState& state) {
	state.initialized = true;
	state.review.valid_disk = true;
	state.review.manifest_path =
		".flowui/changes/a_manifest_with_a_long_application_name.flowchanges";
	state.review.rows.reserve(80);
	for (uint64_t index = 0; index < 80; ++index) {
		DevChangesReviewRow row;
		row.key = "element:definition:1234567890:field:" + std::to_string(index);
		row.target_name = "Application toolbar";
		row.field_path = "layout.padding.field_" + std::to_string(index);
		row.on_disk = index == 3 ? "true" : index == 4 ? R"("saved")" : "12";
		row.proposal = row.next_value = index == 2	 ? "0"
										: index == 3 ? "false"
										: index == 4 ? R"("")"
													 : "24";
		if (index == 5) {
			row.on_disk = R"({"left":12,"right":12,"top":12,"bottom":12})";
			row.proposal = row.next_value = R"({"left":24,"right":24,"top":24,"bottom":24})";
		}
		row.origin = "Prior manifest and live session";
		row.has_disk = row.has_proposal = true;
		row.operation = DevChangesOperation::Update;
		row.reason = "Ready: reflected identity, writable field and initializer validated.";
		DevBakeEntry entry;
		entry.definition = DefinitionID("changes.application.fixture");
		entry.fieldId = index + 1;
		entry.fieldPath = row.field_path;
		entry.ownerCppType =
			"application::components::ToolbarParametersWithLongButValidSourceIdentity";
		entry.sourceHeader =
			"project/components/toolbar/AnIntentionallyLongSourceHeaderWithoutWhitespace.hpp";
		entry.cppValue =
			index == 5 ? "Clay_Padding{.left=24,.right=24,.top=24,.bottom=24}" : row.proposal;
		entry.valueJson = row.proposal;
		entry.valueTypeName = index == 3   ? "bool"
							  : index == 4 ? "std::string"
							  : index == 5 ? "Clay_Padding"
										   : "uint16_t";
		entry.provenance.note =
			"Retained UTF-8 source metadata: fenêtre 🧭. Keep exact bytes when copying raw output.";
		entry.provenance.timestamp = "2026-11-02T12:00:00Z";
		if (index == 6 || index == 7) {
			entry.targetScope = DevOverrideScope::ExactInstance;
			entry.instanceKey = {.value = 9001};
			entry.instanceDebugLabel = "Toolbar instance";
		}
		if (index >= 30 && index < 55) {
			row.domain = DevChangesDomain::Themes;
			entry.targetKind = DevBakeTargetKind::Theme;
			entry.themeType = 3456;
			entry.themeVariant = index % 2 ? "dark" : "*";
			row.key = "theme:" + std::to_string(index);
			row.target_name = "FSEL theme";
		} else if (index >= 55) {
			row.domain = DevChangesDomain::Memory;
			const auto& definition = ::FlowUi::detail::initial_capacity_definitions[index - 55];
			row.field_path = definition.key;
			row.key = changes_memory_key(row.field_path);
			row.target_name = "Prepared startup capacity";
			FlowUi::detail::MemoryCapacitySelection selection;
			selection.key = row.field_path;
			const auto saved = std::max(definition.default_value, definition.alignment);
			selection.value = index == 55 ? 0 : saved + definition.alignment;
			selection.observed_maximum = index == 55 ? 0 : saved;
			selection.headroom = index == 55 ? 0 : definition.alignment;
			row.on_disk = std::to_string(saved);
			row.proposal = row.next_value = std::to_string(selection.value);
			selection.evidence.source_name =
				"Measured workload: navigation and large document editing";
			selection.evidence.begin_ns = 1000;
			selection.evidence.end_ns = 5000;
			selection.evidence.begin_tick = 1;
			selection.evidence.end_tick_exclusive = 300;
			row.proposed_memory = selection;
			selection.value = saved;
			selection.observed_maximum = selection.headroom = 0;
			row.disk_memory = selection;
		}
		if (row.domain != DevChangesDomain::Memory) {
			row.proposed_entry = entry;
			entry.valueJson = row.on_disk;
			entry.cppValue =
				index == 5 ? "Clay_Padding{.left=12,.right=12,.top=12,.bottom=12}" : row.on_disk;
			row.disk_entry = entry;
		}
		if (index == 1 || index == 11) {
			row.operation = DevChangesOperation::Add;
			row.has_disk = false;
			row.disk_entry.reset();
			row.on_disk.clear();
		}
		if (index == 10 || index == 13) {
			row.operation = DevChangesOperation::Remove;
			row.proposal_removes = true;
			row.proposed_entry.reset();
			row.next_value.clear();
		}
		if (index >= 11 && index <= 13) {
			row.blocking = true;
			row.validity = DevChangesValidity::Invalid;
			row.reason = "The selected change has invalid initializer metadata.";
		}
		state.review.rows.emplace_back(std::move(row));
	}
	rebuild_fixture_output(state);
}
void verify_safe_point_service(App& app) {
	const auto directory =
		std::filesystem::temp_directory_path() /
		("flowui-changes-interface-" +
		 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	const auto manifest = directory / "active.flowchanges";
	app.devTooling().bakePipeline().setManifestPath(manifest);
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Changes);
	auto& state = session.changes;
	auto& analysis = session.memory_workbench.analysis;
	analysis.staged.reserve(2);
	for (const auto key : {"flowui.tuning.input_fields.registry_entries",
						   "flowui.tuning.input_fields.rect_override_entries"}) {
		MemorySettingDraft draft;
		draft.target.id = ::FlowUi::memory_capacity_target_id(key);
		draft.target.configKey = key;
		draft.plan.initial = 128;
		draft.plan.observed_maximum = 112;
		draft.selection_policy = "manual";
		draft.recipe.initial_mode = 3;
		draft.recipe.manual_initial = 128;
		draft.export_evidence.kind = "maximum";
		draft.export_evidence.complete = true;
		draft.evidence.emplace_back(MemoryDemandObservation{112, 1, 1, 7, 0});
		analysis.staged.emplace_back(std::move(draft));
	}
	analysis.staged.back().synthetic = true;
	analysis.staged_revision = 7;
	service_changes(app, session, true);
	assert(state.review.valid_disk && !state.review.can_commit && state.review.counts.errors > 0);
	state.selected_key = changes_memory_key(analysis.staged.back().target.configKey);
	assert(selected_changes_row(state) && selected_changes_row(state)->blocking);
	queue_changes_decision(state, DevChangesDecision::OmitProposal);
	service_changes(app, session, true);
	assert(state.review.can_commit && state.review.counts.omitted == 1);
	assert(!selected_changes_row(state)->has_disk &&
		   selected_changes_row(state)->next_value.empty());
	assert(state.review.candidate.memoryCapacity->targets.size() == 1 &&
		   analysis.staged.size() == 2);
	state.pending = DevChangesCommand::Undo;
	service_changes(app, session, true);
	assert(!state.review.can_commit && state.review.counts.errors > 0);
	state.pending = DevChangesCommand::Redo;
	service_changes(app, session, true);
	assert(state.review.can_commit);
	state.search = "nothing-visible";
	state.domain = 1;
	rebuild_changes_model(state);
	assert(state.visible_nodes.empty());
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(std::filesystem::exists(manifest));
	assert(!state.message_is_error && state.message.starts_with("Manifest updated.") &&
		   state.message.find("Rebuild") != std::string::npos);
	assert(state.review.counts.pending() == 0 && state.review.rebuild_required);
	assert(analysis.staged.size() == 2 && session.unbakedChangeCount == 0);
	std::string valid_manifest;
	{
		std::ifstream input(manifest, std::ios::binary);
		valid_manifest.assign(std::istreambuf_iterator<char>(input), {});
	}
	const auto retained_row_count = state.review.rows.size();
	{
		std::ofstream output(manifest, std::ios::trunc);
		output << '{';
	}
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(!state.review.valid_disk && !state.review.can_commit && state.message_is_error);
	assert(state.review.rows.size() == retained_row_count &&
		   !state.review.disk_manifest.memoryCapacity->targets.empty());
	{
		std::ofstream output(manifest, std::ios::trunc);
		output << valid_manifest;
	}
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(state.review.valid_disk && state.review.counts.pending() == 0);
	analysis.staged.front().plan.initial = 256;
	++analysis.staged_revision;
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(state.review.can_commit);
	state.selected_key = changes_memory_key(analysis.staged.front().target.configKey);
	queue_changes_decision(state, DevChangesDecision::OmitProposal);
	service_changes(app, session, true);
	assert(selected_changes_row(state)->decision == DevChangesDecision::OmitProposal);
	assert(selected_changes_row(state)->next_value == "128" &&
		   analysis.staged.front().plan.initial == 256);
	queue_changes_decision(state, DevChangesDecision::RetainDisk);
	service_changes(app, session, true);
	assert(selected_changes_row(state)->decision == DevChangesDecision::RetainDisk);
	assert(selected_changes_row(state)->next_value == "128");
	state.pending = DevChangesCommand::Undo;
	service_changes(app, session, true);
	assert(selected_changes_row(state)->decision == DevChangesDecision::OmitProposal);
	state.pending = DevChangesCommand::Undo;
	service_changes(app, session, true);
	assert(selected_changes_row(state)->decision == DevChangesDecision::IncludeProposal);
	assert(selected_changes_row(state)->next_value == "256");
	queue_changes_decision(state, DevChangesDecision::Remove);
	service_changes(app, session, true);
	assert(selected_changes_row(state)->operation == DevChangesOperation::Remove);
	state.pending = DevChangesCommand::Undo;
	service_changes(app, session, true);
	assert(selected_changes_row(state)->next_value == "256");
	auto external_manifest = valid_manifest;
	const auto saved_value = external_manifest.find("\"value\":\"128\"");
	assert(saved_value != std::string::npos);
	external_manifest.replace(saved_value, std::string_view("\"value\":\"128\"").size(),
							  "\"value\":\"192\"");
	{
		std::ofstream output(manifest, std::ios::trunc);
		output << external_manifest;
	}
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(selected_changes_row(state)->validity == DevChangesValidity::Conflict);
	assert(selected_changes_row(state)->decision == DevChangesDecision::IncludeProposal);
	queue_changes_decision(state, DevChangesDecision::IncludeProposal);
	service_changes(app, session, true);
	assert(selected_changes_row(state)->validity == DevChangesValidity::Ready);
	assert(selected_changes_row(state)->next_value == "256" && state.review.can_commit);
	{
		std::ofstream output(manifest, std::ios::app);
		output << '\n';
	}
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(state.message_is_error && state.stale);
	assert(analysis.staged.front().plan.initial == 256);
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	// The reviewed disk revision changed again, so confirm against that exact revision.
	assert(selected_changes_row(state)->validity == DevChangesValidity::Conflict);
	queue_changes_decision(state, DevChangesDecision::IncludeProposal);
	service_changes(app, session, true);
	assert(state.review.can_commit);
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(!state.message_is_error && state.review.counts.pending() == 0);
	state.selected_key = changes_memory_key(analysis.staged.front().target.configKey);
	queue_changes_decision(state, DevChangesDecision::Remove);
	service_changes(app, session, true);
	assert(state.review.can_commit && state.review.counts.removals == 1);
	state.pending = DevChangesCommand::Bake;
	service_changes(app, session, true);
	assert(!state.message_is_error && state.review.counts.pending() == 0);
	assert(analysis.staged.front().plan.initial == 256);
	const auto manifest_history_size = state.undo.size();
	state.pending = DevChangesCommand::ApplyProposal;
	service_changes(app, session, true);
	assert(!state.runtime_transaction && state.runtime_message_is_error &&
		   !state.runtime_message.empty());
	assert(state.undo.size() == manifest_history_size &&
		   analysis.staged.front().plan.initial == 256);
	state.selector_scroll = -17;
	state.workbench_scroll = -23;
	state.inspector_scroll = -31;
	state.pending = DevChangesCommand::Refresh;
	service_changes(app, session, true);
	assert(state.selector_scroll == -17 && state.workbench_scroll == -23 &&
		   state.inspector_scroll == -31);
	std::filesystem::remove_all(directory);
}
void preview_selector_references(App& app, DevInterfaceState& session) {
	if (!std::getenv("FLOWUI_CHANGES_PREVIEW"))
		return;
	DevInterfaceState references;
	for (int frame = 0; frame < 3; ++frame) {
		assert(app.beginFrame());
		const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction()) = {};
		auto root = changes_ui::row(900);
		root.layout.sizing.width = CLAY_SIZING_FIXED(1400);
		root.layout.childGap = 0;
		uint32_t column_index = 0;
		const auto column = [&](const auto& element, const auto& identity, std::string_view title,
								DevInterfaceState& state) {
			auto panel = changes_ui::column(0);
			panel.layout.sizing = {.width = CLAY_SIZING_FIXED(350),
								   .height = CLAY_SIZING_FIXED(900)};
			CLAY(CLAY_IDI("selector-reference", column_index++), panel) {
				auto heading = changes_ui::row(32);
				heading.layout.padding.left = 12;
				heading.backgroundColor = interface_theme::kDepth3Elevated;
				CLAY(CLAY_IDI("selector-reference-title", column_index), heading) {
					Clay_TextElementConfig text{};
					text.fontSize = 12;
					text.textColor = interface_theme::kTextCanvas;
					CLAY_TEXT(app.ui().toClayString(title), CLAY_TEXT_CONFIG(text));
				}
				auto content = changes_ui::column(0);
				content.layout.sizing.height = CLAY_SIZING_FIXED(868);
				content.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_IDI("selector-reference-body", column_index), content)
				app.ui().createElement(element, identity).setParameters({&app, &state}).draw();
			}
		};
		CLAY(CLAY_ID("selector-references"), root) {
			column(kDevInspectSelector, Global<kDevInspectSelector>("changes.reference.inspect"),
				   "INSPECT", references);
			column(kDevPerformanceSelector,
				   Global<kDevPerformanceSelector>("changes.reference.performance"), "PERFORMANCE",
				   references);
			column(kDevMemorySelector, Global<kDevMemorySelector>("changes.reference.memory"),
				   "MEMORY", references);
			column(kDevChangesSelector, selector_id, "CHANGES", session);
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	}
	std::cout << "Changes preview=selector-comparison" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(700));
}

void verify_catalogue_panel_scrolling(App& app) {
	constexpr auto catalogue_selector = Global<kDevCatalogueSelector>("catalogue.scroll.selector");
	constexpr auto catalogue_workbench =
		Global<kDevCatalogueWorkbench>("catalogue.scroll.workbench");
	constexpr auto catalogue_inspector =
		Global<kDevCatalogueInspector>("catalogue.scroll.inspector");
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Catalogue);
	auto& catalogue = session.catalogue;
	catalogue.domain = static_cast<uint64_t>(DevCatalogueDomain::Actions);
	catalogue.assets.reserve(60);
	for (uint64_t asset_index = 0; asset_index < 60; ++asset_index) {
		catalogue.assets.emplace_back(
			DevCatalogueAsset{.name = "Scroll action " + std::to_string(asset_index),
							  .group = "Application actions",
							  .status = "Registered",
							  .identity = asset_index + 1,
							  .domain = DevCatalogueDomain::Actions});
	}
	for (int paragraph_index = 0; paragraph_index < 40; ++paragraph_index)
		catalogue.domain_status[catalogue.domain] += "Observed application action inventory. ";
	rebuild_catalogue_filter(catalogue);
	const auto draw = [&](bool visible = true, FlowElementID control = {}, int event = 0) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.windowFocused = true;
		input.mouseDown[0] = event == 1;
		input.keyDown = {};
		if (control.value) {
			const auto identity = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		auto root = changes_ui::row(360);
		root.layout.sizing.width = CLAY_SIZING_FIXED(1400);
		root.layout.childGap = 0;
		CLAY(CLAY_ID("catalogue-scroll-test"), root) {
			const auto panel = [&](const auto& element, const auto& identity, uint32_t panel_index,
								   float width) {
				auto declaration = changes_ui::column(0);
				declaration.layout.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(360)};
				declaration.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_IDI("catalogue-scroll-panel", panel_index), declaration) {
					if (visible)
						app.ui()
							.createElement(element, identity)
							.setParameters({&app, &session})
							.draw();
				}
			};
			panel(kDevCatalogueSelector, catalogue_selector, 0, 280);
			panel(kDevCatalogueWorkbench, catalogue_workbench, 1, 760);
			panel(kDevCatalogueInspector, catalogue_inspector, 2, 360);
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto scroll_identity = [&](const auto& owner, FlowDefinitionID definition,
									 const auto& identity) {
		return app.ui().toClayEID(local(FlowElementID{owner.value}, definition, identity));
	};
	const std::array viewports{
		scroll_identity(catalogue_selector, DevCatalogueSelector::definitionId,
						LocalElementName{"domains"}),
		scroll_identity(catalogue_selector, DevCatalogueSelector::definitionId,
						Keyed("browser", catalogue.domain)),
		scroll_identity(catalogue_workbench, DevCatalogueWorkbench::definitionId,
						LocalElementName{"gallery-scroll"}),
		scroll_identity(catalogue_inspector, DevCatalogueInspector::definitionId,
						LocalElementName{"scroll-body"})};
	draw();
	draw();
	draw(false);
	draw(false);
	draw();
	draw();
	draw();
	if (std::getenv("FLOWUI_CHANGES_PREVIEW")) {
		std::cout << "Changes preview=catalogue-overview" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(700));
	}
	auto* window = static_cast<GLFWwindow*>(app.nativeWindowHandle());
	glfwFocusWindow(window);
	for (const auto identity : viewports) {
		const auto bounds = Clay_GetElementData(identity);
		const auto scroll = Clay_GetScrollContainerData(identity);
		assert(bounds.found && scroll.found && scroll.scrollPosition);
		if (scroll.contentDimensions.height <= scroll.scrollContainerDimensions.height + 1)
			std::cerr << "Catalogue scroll region " << identity.id
					  << " content height=" << scroll.contentDimensions.height
					  << " viewport height=" << scroll.scrollContainerDimensions.height << '\n';
		assert(scroll.contentDimensions.height > scroll.scrollContainerDimensions.height + 1);
		glfwSetCursorPos(window, bounds.boundingBox.x + bounds.boundingBox.width / 2,
						 bounds.boundingBox.y + bounds.boundingBox.height / 2);
		draw();
		draw();
		std::array<float, viewports.size()> before{};
		for (std::size_t viewport_index = 0; viewport_index < viewports.size(); ++viewport_index)
			before[viewport_index] =
				Clay_GetScrollContainerData(viewports[viewport_index]).scrollPosition->y;
		const auto callback = glfwSetScrollCallback(window, nullptr);
		assert(callback);
		glfwSetScrollCallback(window, callback);
		callback(window, 0, -1);
		draw();
		for (std::size_t viewport_index = 0; viewport_index < viewports.size(); ++viewport_index) {
			const auto after = Clay_GetScrollContainerData(viewports[viewport_index]);
			if (viewports[viewport_index].id == identity.id)
				assert(after.scrollPosition->y < before[viewport_index] - 1);
			else
				assert(after.scrollPosition->y == before[viewport_index]);
		}
	}
	const auto click = [&](FlowElementID control) {
		draw(true, control, 1);
		draw(true, control, 2);
		draw();
	};
	const auto overview_header =
		local(FlowElementID{catalogue_inspector.value}, diagnostics_ui::Control::definitionId,
			  Keyed("section-header", LocalElementName{"overview-section"}.token));
	Clay_GetScrollContainerData(viewports.back()).scrollPosition->y = 0;
	draw();
	const auto expanded_height =
		Clay_GetScrollContainerData(viewports.back()).contentDimensions.height;
	click(overview_header);
	const auto collapsed_height =
		Clay_GetScrollContainerData(viewports.back()).contentDimensions.height;
	assert(collapsed_height < expanded_height);
	assert(app.ui().shortcuts().focusedElement().id == app.ui().toClayEID(overview_header).id);
	click(overview_header);
	assert(Clay_GetScrollContainerData(viewports.back()).contentDimensions.height >
		   collapsed_height);
	select_catalogue_asset(catalogue, catalogue.assets.front().identity);
	draw();
	assert(catalogue.selected_identity != 0);
	if (std::getenv("FLOWUI_CHANGES_PREVIEW")) {
		std::cout << "Changes preview=catalogue-selection" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(700));
	}
	const auto domain_choice = [&](DevCatalogueDomain domain) {
		return local(FlowElementID{catalogue_selector.value}, FSEL::RadioChoice::definitionId,
					 Keyed("domain", static_cast<uint64_t>(domain)));
	};
	click(domain_choice(DevCatalogueDomain::Images));
	assert(catalogue.domain == static_cast<uint64_t>(DevCatalogueDomain::Images));
	assert(catalogue.selected_identity == 0 && catalogue.visible_assets.empty());
	click(domain_choice(DevCatalogueDomain::Actions));
	assert(catalogue.domain == static_cast<uint64_t>(DevCatalogueDomain::Actions));
	assert(catalogue.selected_identity == 0 && catalogue.visible_assets.size() == 60);
}
} // namespace

int main() {
	AppConfig config;
	config.window.title = "Changes complete surface verification";
	config.window.width = 1400;
	config.window.height = 900;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session;
	session.activeTab = static_cast<uint64_t>(DevInterfaceTab::Changes);
	auto& state = session.changes;
	seed_review(state);
	float selector_width = 280, inspector_width = 360;
	bool draw_changes = true;
	const auto draw = [&](FlowElementID control = {}, int event = 0, int key = -1,
						  Clay_Vector2 pointer = {-1, -1}, FlowElementID extra = {},
						  bool shift = false) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.windowFocused = true;
		input.mouseDown = {};
		input.keyDown = {};
		if (pointer.x >= 0 && pointer.y >= 0) {
			input.mouseX = pointer.x;
			input.mouseY = pointer.y;
		}
		input.mouseDown[0] = event == 1;
		if (key >= 0)
			input.keyDown[key] = true;
		input.keyDown[GLFW_KEY_LEFT_SHIFT] = shift;
		for (const auto target : {control, extra}) {
			if (!target.value)
				continue;
			const auto identity = app.ui().toClayEID(target).id;
			interaction.hoveredElementIds.emplace_back(identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(identity);
		}
		auto root = changes_ui::column(0);
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(1400), .height = CLAY_SIZING_FIXED(900)};
		root.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(CLAY_ID("changes-test"), root) {
			if (draw_changes)
				app.ui()
					.createElement(kDevChangesControls, controls_id)
					.setParameters({&app, &session})
					.draw();
			auto body = changes_ui::row();
			body.layout.childGap = 0;
			body.layout.sizing.height = CLAY_SIZING_GROW(0);
			CLAY(CLAY_ID("changes-test-columns"), body) {
				auto panel = changes_ui::column(0);
				panel.layout.sizing = {.width = CLAY_SIZING_FIXED(selector_width),
									   .height = CLAY_SIZING_GROW(0)};
				// Match the clipped columns used by DevContentAreas in the developer window.
				panel.clip = {.horizontal = true, .vertical = true};
				CLAY(CLAY_ID("changes-test-selector"), panel)
				if (draw_changes)
					app.ui()
						.createElement(kDevChangesSelector, selector_id)
						.setParameters({&app, &session})
						.draw();
				panel.layout.sizing.width =
					CLAY_SIZING_FIXED(1400 - selector_width - inspector_width);
				CLAY(CLAY_ID("changes-test-workbench"), panel)
				if (draw_changes)
					app.ui()
						.createElement(kDevChangesWorkbench, workbench_id)
						.setParameters({&app, &session})
						.draw();
				panel.layout.sizing.width = CLAY_SIZING_FIXED(inspector_width);
				CLAY(CLAY_ID("changes-test-inspector"), panel)
				if (draw_changes)
					app.ui()
						.createElement(kDevChangesInspector, inspector_id)
						.setParameters({&app, &session})
						.draw();
			}
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto preview = [&](std::string name) {
		if (std::getenv("FLOWUI_CHANGES_PREVIEW")) {
			std::cout << "Changes preview=" << name << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(700));
		}
	};
	const auto viewport = [&](FlowElementID parent, FlowDefinitionID definition,
							  LocalElementName name) {
		return app.ui().toClayEID(local(parent, definition, name));
	};
	const auto selector_scroll =
		viewport(FlowElementID{selector_id.value}, DevChangesSelector::definitionId, "scroll");
	const auto workbench_scroll =
		viewport(FlowElementID{workbench_id.value}, DevChangesWorkbench::definitionId, "scroll");
	const auto inspector_scroll = viewport(FlowElementID{inspector_id.value},
										   DevChangesInspector::definitionId, "scroll-body");
	const auto wheel = [&](Clay_ElementId identity, float delta) {
		const auto bounds = Clay_GetElementData(identity);
		const auto before = Clay_GetScrollContainerData(identity);
		assert(bounds.found && before.found && before.scrollPosition);
		if (before.contentDimensions.height <= before.scrollContainerDimensions.height + 1)
			std::cerr << "Changes scroll region " << identity.id
					  << " content height=" << before.contentDimensions.height
					  << " viewport height=" << before.scrollContainerDimensions.height << '\n';
		assert(before.contentDimensions.height > before.scrollContainerDimensions.height + 1);
		auto* window = static_cast<GLFWwindow*>(app.nativeWindowHandle());
		glfwFocusWindow(window);
		glfwSetCursorPos(window, bounds.boundingBox.x + bounds.boundingBox.width / 2,
						 bounds.boundingBox.y + bounds.boundingBox.height / 2);
		draw();
		draw();
		const auto old_position = Clay_GetScrollContainerData(identity).scrollPosition->y;
		const auto callback = glfwSetScrollCallback(window, nullptr);
		assert(callback);
		glfwSetScrollCallback(window, callback);
		callback(window, 0, delta);
		draw();
		const auto after = Clay_GetScrollContainerData(identity);
		if (delta < 0)
			assert(after.scrollPosition->y < old_position - 1);
		else
			assert(after.scrollPosition->y > old_position + 1);
	};

	draw();
	draw();
	const auto has_text = [&](std::string_view description) {
		const auto& snapshot = app.ui().devTreeSnapshot();
		return std::ranges::any_of(snapshot.clay.nodes, [&](const auto& node) {
			return snapshot.string(node.text).find(description) != std::string_view::npos;
		});
	};
	const auto surface_has_text = [&](FlowElementID surface, std::string_view description) {
		const auto& snapshot = app.ui().devTreeSnapshot();
		for (const auto& node : snapshot.clay.nodes) {
			if (snapshot.string(node.text).find(description) == std::string_view::npos)
				continue;
			for (auto owner = node.directFlowOwner;
				 owner != InvalidFlowNode && owner < snapshot.flow.nodes.size();
				 owner = snapshot.flow.nodes[owner].parent)
				if (snapshot.flow.nodes[owner].instance.value == surface.value)
					return true;
		}
		return false;
	};
	const auto has_element = [&](FlowElementID identity) {
		const auto clay_id = app.ui().toClayEID(identity).id;
		return std::ranges::any_of(app.ui().devTreeSnapshot().clay.nodes,
								   [&](const auto& node) { return node.clayId == clay_id; });
	};
	const auto workbench_control = [&](const auto& identity) {
		return local(FlowElementID{workbench_id.value}, diagnostics_ui::Control::definitionId,
					 identity);
	};
	const auto inspector_control = [&](const auto& identity) {
		return local(FlowElementID{inspector_id.value}, diagnostics_ui::Control::definitionId,
					 identity);
	};
	const auto ensure_visible = [&](FlowElementID control, Clay_ElementId scroll_id) {
		if (!scroll_id.id)
			return;
		const auto bounds = Clay_GetElementData(app.ui().toClayEID(control));
		const auto visible = Clay_GetElementData(scroll_id);
		const auto scroll = Clay_GetScrollContainerData(scroll_id);
		assert(bounds.found && visible.found && scroll.found && scroll.scrollPosition);
		if (bounds.boundingBox.y < visible.boundingBox.y)
			scroll.scrollPosition->y += visible.boundingBox.y - bounds.boundingBox.y;
		else if (bounds.boundingBox.y + bounds.boundingBox.height >
				 visible.boundingBox.y + visible.boundingBox.height)
			scroll.scrollPosition->y -= bounds.boundingBox.y + bounds.boundingBox.height -
										visible.boundingBox.y - visible.boundingBox.height;
		draw();
		draw();
	};
	const auto click = [&](FlowElementID control, Clay_ElementId scroll_id = {}) {
		ensure_visible(control, scroll_id);
		assert(has_element(control));
		draw(control, 1);
		draw(control, 2);
		draw();
	};
	const auto reset_overview = [&] {
		state.selected_key.clear();
		state.queue_scope_key.clear();
		state.search.clear();
		state.domain = state.status = state.view = state.workbench_page = 0;
		state.workbench_page_changed = true;
		rebuild_changes_model(state);
	};
	const auto rendered_queue_count = [&] {
		return std::ranges::count_if(app.ui().devTreeSnapshot().flow.nodes, [](const auto& node) {
			return node.definition == DefinitionID("flowui.dev_interface.changes.queue-row");
		});
	};
	const auto expanded_count = [&] {
		std::size_t count = 0;
		for (const auto& node : state.nodes)
			if (node.leaf && has_element(local(FlowElementID{workbench_id.value},
											   DevChangesWorkbench::definitionId,
											   Keyed("expanded-review", node.identity))))
				++count;
		return count;
	};
	assert(changes_ui::value("", true) == R"("")");
	assert(changes_ui::value("", false) == "Absent");
	assert(changes_ui::value("0", true) == "0" && changes_ui::value("false", true) == "false");
	assert(state.review.counts.pending() == 80);
	assert(state.review.candidate.entries.size() == 53);
	assert(state.review.candidate.memoryCapacity->targets.size() == 25);
	assert(rendered_queue_count() == 40 && expanded_count() == 0);
	assert(surface_has_text(FlowElementID{inspector_id.value},
							"Select a change to see its scope and evidence."));
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "For next bake"));
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "Stage factor"));
	check_centered_search(app, true);
	preview("overview");
	preview_selector_references(app, session);
	draw();
	draw();

	// Shared matching is independent of collapse and group actions keep their explicit scope.
	changes_select_key(state, "group:Elements");
	state.search = "field_0";
	rebuild_changes_model(state);
	assert(changes_queue_rows(state).size() == 1);
	queue_changes_scope_decision(state, "group:Elements", DevChangesDecision::OmitProposal);
	assert(state.pending_keys.size() == 30);
	queue_changes_scope_decision(state, "group:Elements", DevChangesDecision::IncludeProposal);
	assert(state.pending_keys.size() == 27 && state.pending_skipped == 3);
	queue_changes_scope_decision(state, "group:Elements", DevChangesDecision::RetainDisk);
	assert(state.pending_keys.size() == 28);
	state.pending = DevChangesCommand::None;
	draw();
	draw();
	const auto group_decisions = workbench_control(LocalElementName{"group-decisions"});
	click(group_decisions, workbench_scroll);
	assert(state.group_decisions_open && has_text("29 hidden by filters"));
	preview("group-decisions-hidden-targets");
	draw({}, 0, GLFW_KEY_ESCAPE);
	draw();
	assert(!state.group_decisions_open);
	assert(app.ui().shortcuts().focusedElement().id == app.ui().toClayEID(group_decisions).id);
	click(group_decisions, workbench_scroll);
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(state.pending == DevChangesCommand::None &&
		   app.ui().shortcuts().focusedElement().id ==
			   app.ui().toClayEID(workbench_control(LocalElementName{"batch-include"})).id);
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(app.ui().shortcuts().focusedElement().id ==
		   app.ui().toClayEID(workbench_control(LocalElementName{"batch-omit"})).id);
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	assert(state.pending == DevChangesCommand::Decide && state.pending_keys.size() == 30);
	state.pending = DevChangesCommand::None;
	reset_overview();
	draw();
	draw();

	// The existing header opens human-readable Output; raw bytes remain optional.
	const auto review_trigger =
		local(FlowElementID{controls_id.value}, diagnostics_ui::Control::definitionId,
			  LocalElementName{"review-trigger"});
	click(review_trigger);
	assert(state.view == 1 && !state.raw_manifest_open);
	assert(has_text("Whole-plan output") && !has_text("read-only exact serialization"));
	assert(!has_element(local(FlowElementID{workbench_id.value}, DevChangesWorkbench::definitionId,
							  LocalElementName{"raw-scroll"})));
	click(workbench_control(LocalElementName{"copy-manifest"}), workbench_scroll);
	assert(app.clipboardText() == state.review.candidate_json);
	preview("output-summary");
	click(workbench_control(LocalElementName{"raw-manifest"}), workbench_scroll);
	assert(state.raw_manifest_open && state.review.candidate_json.size() > 8192);
	const auto raw_scroll = viewport(FlowElementID{workbench_id.value},
									 DevChangesWorkbench::definitionId, "raw-scroll");
	ensure_visible(local(FlowElementID{workbench_id.value}, DevChangesWorkbench::definitionId,
						 LocalElementName{"raw-scroll"}),
				   workbench_scroll);
	const auto outside_raw = Clay_GetScrollContainerData(workbench_scroll).scrollPosition->y;
	wheel(raw_scroll, -4);
	assert(Clay_GetScrollContainerData(workbench_scroll).scrollPosition->y == outside_raw);
	wheel(raw_scroll, 4);
	click(workbench_control(LocalElementName{"next-candidate"}), workbench_scroll);
	const auto* output_state = app.ui().elements().getStatePointerConst(
		kDevChangesWorkbench, app.ui().windowId(), FlowElementID{workbench_id.value});
	assert(output_state && output_state->raw_page == 1 && state.workbench_page == 0);
	click(workbench_control(LocalElementName{"copy-manifest"}), workbench_scroll);
	assert(app.clipboardText() == state.review.candidate_json);
	check_bounds(app, FlowElementID{workbench_id.value});
	preview("output-raw-paged");
	click(workbench_control(LocalElementName{"raw-manifest"}), workbench_scroll);
	reset_overview();
	draw();
	draw();

	// The selector's compact visual and independent disclosure behavior remain unchanged.
	const auto selector_node = [&](uint64_t identity) {
		return local(FlowElementID{selector_id.value}, DevChangesSelectorNode::definitionId,
					 Keyed("node", identity));
	};
	std::vector<uint64_t> group_identities;
	group_identities.reserve(state.nodes.size());
	for (const auto& node : state.nodes)
		if (!node.leaf)
			group_identities.emplace_back(node.identity);
	for (const auto identity : group_identities) {
		const auto group = selector_node(identity);
		const auto disclosure =
			local(group, DevChangesSelectorNode::definitionId, LocalElementName{"disclosure"});
		const auto find_group = [&]() -> const DevChangesNode& {
			const auto found = std::ranges::find(state.nodes, identity, &DevChangesNode::identity);
			assert(found != state.nodes.end());
			return *found;
		};
		const auto key = find_group().key;
		const auto visible = std::ranges::find_if(state.visible_nodes, [&](auto index) {
			return state.nodes[index].identity == identity;
		});
		assert(visible != state.visible_nodes.end());
		Clay_GetScrollContainerData(selector_scroll).scrollPosition->y =
			-static_cast<float>(visible - state.visible_nodes.begin()) *
			DevChangesSelectorNode::row_height;
		draw();
		draw();
		assert(find_group().expanded);
		draw(group, 1);
		draw(group, 2);
		draw();
		assert(find_group().expanded && state.selected_key == key);
		if (key == "group:Elements")
			preview("selector-group-selection");
		const auto current_page = state.workbench_page;
		draw(group, 1, -1, {-1, -1}, disclosure);
		draw(group, 2, -1, {-1, -1}, disclosure);
		draw();
		assert(!find_group().expanded && state.selected_key == key &&
			   state.workbench_page == current_page);
		draw(group, 1);
		draw(group, 2);
		draw();
		assert(!find_group().expanded && state.selected_key == key);
		draw(group, 1, -1, {-1, -1}, disclosure);
		draw(group, 2, -1, {-1, -1}, disclosure);
		draw();
		assert(find_group().expanded && state.selected_key == key);
	}
	state.selected_key.clear();
	state.queue_scope_key.clear();
	state.workbench_page = 0;
	Clay_GetScrollContainerData(selector_scroll).scrollPosition->y = 0;
	draw();
	draw();
	assert(!has_text("Select to review this group"));
	preview("selector-operation-colors");
	const auto root_identity = state.nodes[state.visible_nodes.front()].identity;
	const auto root_node = selector_node(root_identity);
	const auto root_disclosure =
		local(root_node, DevChangesSelectorNode::definitionId, LocalElementName{"disclosure"});
	state.selected_key = state.review.rows.front().key;
	draw(root_node, 1, -1, {-1, -1}, root_disclosure);
	draw(root_node, 2);
	draw();
	assert(state.selected_key == state.review.rows.front().key &&
		   state.nodes[state.visible_nodes.front()].expanded);
	draw(root_node, 1);
	draw(root_node, 2, -1, {-1, -1}, root_disclosure);
	draw();
	assert(state.selected_key == state.review.rows.front().key &&
		   state.nodes[state.visible_nodes.front()].expanded);
	state.selected_key.clear();
	state.queue_scope_key.clear();
	state.keyboard_active = true;
	state.focused_identity = state.nodes[state.visible_nodes.front()].identity;
	app.ui().shortcuts().setFocusedElement(
		app.ui().toClayEID(selector_node(state.focused_identity)));
	const auto next_identity = state.nodes[state.visible_nodes[1]].identity;
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(state.focused_identity == next_identity);
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	assert(selected_changes_node(state) && selected_changes_node(state)->identity == next_identity);
	state.selected_key.clear();
	state.queue_scope_key.clear();
	const auto memory_radio = local(FlowElementID{selector_id.value},
									FSEL::RadioChoice::definitionId, Keyed("domain", 3));
	draw(memory_radio, 1);
	draw(memory_radio, 2);
	draw();
	assert(state.domain == 3 && state.review.counts.pending() == 80);
	state.domain = 0;
	rebuild_changes_model(state);
	const auto leaf_node =
		std::ranges::find(state.nodes, state.review.rows.front().key, &DevChangesNode::key);
	assert(leaf_node != state.nodes.end());
	const auto leaf_control =
		local(FlowElementID{selector_id.value}, DevChangesSelectorNode::definitionId,
			  Keyed("node", leaf_node->identity));
	draw();
	draw(leaf_control, 1);
	draw(leaf_control, 2);
	draw();
	assert(selected_changes_row(state) == &state.review.rows.front());
	const auto& tree = app.ui().devTreeSnapshot();
	const auto selected_clay_id = app.ui().toClayEID(leaf_control).id;
	const auto selected_row =
		std::ranges::find(tree.clay.nodes, selected_clay_id, &DevClayNode::clayId);
	assert(selected_row != tree.clay.nodes.end());
	assert(selected_row->declaration.backgroundColor.a == 0);
	assert(selected_row->declaration.border.width.left == 3);
	assert(selected_row->declaration.border.width.bottom == 0);
	preview("selector-leaf-selection");
	state.selected_key.clear();
	state.queue_scope_key.clear();
	for (const auto width : {220.0f, 280.0f, 360.0f, 450.0f}) {
		selector_width = width;
		state.search = "layout";
		draw();
		draw();
		check_centered_search(app);
		check_bounds(app, FlowElementID{selector_id.value});
		check_bounds(app, FlowElementID{workbench_id.value});
		check_assigned_width(app, FlowElementID{workbench_id.value},
							 1400 - selector_width - inspector_width);
		const auto selector_bounds =
			Clay_GetElementData(app.ui().toClayEID(FlowElementID{selector_id.value}));
		if (std::abs(selector_bounds.boundingBox.width - selector_width) > 1)
			std::cerr << "Changes selector assigned width=" << selector_width
					  << " actual root width=" << selector_bounds.boundingBox.width << '\n';
		assert(std::abs(selector_bounds.boundingBox.width - selector_width) <= 1);
		const auto search = Clay_GetElementData(
			app.ui().toClayEID(local(FlowElementID{selector_id.value},
									 DevSelectorSearch::definitionId, LocalElementName{"search"})));
		const auto status = Clay_GetElementData(
			app.ui().toClayEID(local(FlowElementID{selector_id.value}, FSEL::ComboBox::definitionId,
									 LocalElementName{"status"})));
		assert(search.found && status.found);
		assert(std::abs(status.boundingBox.y - search.boundingBox.y) <= 1);
		assert(std::abs(status.boundingBox.height - search.boundingBox.height) <= 1);
		assert(search.boundingBox.x + search.boundingBox.width < status.boundingBox.x);
		preview("selector-width-" + std::to_string(static_cast<int>(width)));
	}
	state.search.clear();
	selector_width = 220;
	for (uint64_t status = 0; status < 5; ++status) {
		state.status = status;
		draw();
		draw();
		check_bounds(app, FlowElementID{selector_id.value});
	}
	state.status = 0;
	selector_width = 280;
	draw();
	draw();

	// All three panes retain their own wheel offset in the actual clipped shell columns.
	wheel(selector_scroll, -4);
	const auto selector_position = Clay_GetScrollContainerData(selector_scroll).scrollPosition->y;
	wheel(workbench_scroll, -4);
	assert(Clay_GetScrollContainerData(selector_scroll).scrollPosition->y == selector_position);
	wheel(workbench_scroll, 4);
	wheel(selector_scroll, 4);

	// A selected leaf expands in the persistent queue; choosing a neighbor closes the old review.
	reset_overview();
	changes_select_key(state, state.review.rows.front().key);
	draw();
	draw();
	assert(rendered_queue_count() == 40 && expanded_count() == 1);
	const auto first_identity = selected_changes_node(state)->identity;
	const auto neighbor_node =
		std::ranges::find(state.nodes, state.review.rows[1].key, &DevChangesNode::key);
	assert(neighbor_node != state.nodes.end());
	const auto neighbor = local(FlowElementID{workbench_id.value},
								DefinitionID("flowui.dev_interface.changes.queue-row"),
								Keyed("review-row", neighbor_node->identity));
	const auto first_queue_row = local(FlowElementID{workbench_id.value},
									   DefinitionID("flowui.dev_interface.changes.queue-row"),
									   Keyed("review-row", first_identity));
	ensure_visible(first_queue_row, workbench_scroll);
	app.ui().shortcuts().setFocusedElement(app.ui().toClayEID(first_queue_row));
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(app.ui().shortcuts().focusedElement().id == app.ui().toClayEID(neighbor).id);
	assert(selected_changes_row(state) == &state.review.rows.front() &&
		   state.pending == DevChangesCommand::None);
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	assert(selected_changes_row(state) == &state.review.rows[1]);
	assert(rendered_queue_count() == 40 && expanded_count() == 1);
	assert(!has_element(local(FlowElementID{workbench_id.value}, DevChangesWorkbench::definitionId,
							  Keyed("expanded-review", first_identity))));
	preview("persistent-queue");

	// The decision menu queues canonical intent and never fabricates a serviced success.
	const auto decision = local(FlowElementID{workbench_id.value}, FSEL::ComboBox::definitionId,
								LocalElementName{"next-bake-decision"});
	ensure_visible(decision, workbench_scroll);
	app.ui().shortcuts().setFocusedElement(app.ui().toClayEID(decision));
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	const auto* decision_state = app.ui().elements().getStatePointerConst(
		kDevChangesWorkbench, app.ui().windowId(), FlowElementID{workbench_id.value});
	assert(decision_state && decision_state->decision_open);
	assert(
		app.ui().shortcuts().focusedElement().id ==
		app.ui().toClayEID(local(decision, FSEL::ComboBox::definitionId, Keyed("option", 0))).id);
	const auto exclude = local(decision, FSEL::ComboBox::definitionId, Keyed("option", 1));
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(state.pending == DevChangesCommand::None &&
		   app.ui().shortcuts().focusedElement().id == app.ui().toClayEID(exclude).id);
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	assert(state.pending == DevChangesCommand::Decide);
	assert(state.pending_decision == DevChangesDecision::OmitProposal);
	assert(state.pending_keys == std::vector<std::string>{state.review.rows[1].key});
	assert(state.review.rows[1].decision == DevChangesDecision::IncludeProposal);
	assert(state.review.rows[1].next_value == "24");
	state.pending = DevChangesCommand::None;
	app.ui().shortcuts().setFocusedElement(app.ui().toClayEID(decision));
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	draw({}, 0, GLFW_KEY_ESCAPE);
	draw();
	decision_state = app.ui().elements().getStatePointerConst(
		kDevChangesWorkbench, app.ui().windowId(), FlowElementID{workbench_id.value});
	assert(decision_state && !decision_state->decision_open &&
		   state.pending == DevChangesCommand::None);
	assert(app.ui().shortcuts().focusedElement().id == app.ui().toClayEID(decision).id);
	state.review.rows[0].validity = DevChangesValidity::Conflict;
	state.review.rows[0].blocking = true;
	state.review.rows[0].reason = "Saved value changed since this review.";
	rebuild_fixture_output(state);
	changes_select_key(state, state.review.rows[0].key);
	draw();
	draw();
	assert(state.review.rows[0].decision == DevChangesDecision::IncludeProposal);
	click(decision, workbench_scroll);
	click(local(decision, FSEL::ComboBox::definitionId, Keyed("option", 0)));
	assert(state.pending == DevChangesCommand::Decide &&
		   state.pending_decision == DevChangesDecision::IncludeProposal &&
		   state.pending_keys.size() == 1);
	state.pending = DevChangesCommand::None;
	state.review.rows[0].validity = DevChangesValidity::Ready;
	state.review.rows[0].blocking = false;
	state.review.rows[0].reason.clear();
	rebuild_fixture_output(state);
	for (const auto index : {2u, 3u, 4u}) {
		changes_select_key(state, state.review.rows[index].key);
		draw();
		draw();
		assert(expanded_count() == 1);
		assert(surface_has_text(FlowElementID{workbench_id.value},
								state.review.rows[index].next_value));
	}
	preview("zero-false-empty-values");

	// Global feedback stays in a fixed workbench area and never duplicates into Context.
	state.message = "Manifest updated. Rebuild the application to use it.";
	state.message_is_error = false;
	draw();
	if (!surface_has_text(FlowElementID{workbench_id.value}, "Manifest updated.")) {
		const auto& snapshot = app.ui().devTreeSnapshot();
		const auto feedback_id = viewport(FlowElementID{workbench_id.value},
										  DevChangesWorkbench::definitionId, "feedback")
									 .id;
		std::cerr << "First feedback render: message=" << state.message << '\n';
		for (const auto& node : snapshot.clay.nodes) {
			bool in_feedback = node.clayId == feedback_id;
			for (auto parent = node.parent;
				 parent != InvalidClayNode && parent < snapshot.clay.nodes.size();
				 parent = snapshot.clay.nodes[parent].parent)
				in_feedback = in_feedback || snapshot.clay.nodes[parent].clayId == feedback_id;
			if (!in_feedback)
				continue;
			std::cerr << "Feedback node=" << snapshot.string(node.idString)
					  << " width=" << node.bounds.width << " min-width=" << node.minDimensions.width
					  << " text=";
			for (const char character : snapshot.string(node.text))
				if (character == '\n')
					std::cerr << "\\n";
				else
					std::cerr << character;
			std::cerr << '\n';
		}
	}
	preview("feedback-new-message");
	assert(surface_has_text(FlowElementID{workbench_id.value}, "Manifest updated."));
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "Manifest updated."));
	const auto feedback =
		viewport(FlowElementID{workbench_id.value}, DevChangesWorkbench::definitionId, "feedback");
	const auto feedback_before = Clay_GetElementData(feedback);
	wheel(workbench_scroll, -4);
	const auto feedback_after = Clay_GetElementData(feedback);
	assert(feedback_before.found && feedback_after.found &&
		   std::abs(feedback_before.boundingBox.y - feedback_after.boundingBox.y) < 1);
	wheel(workbench_scroll, 4);
	state.message.clear();

	// A filtered selection remains visibly pinned, with explicit reveal and clear actions.
	state.search = "no_matching_change";
	draw();
	draw();
	assert(state.selected_outside_filter && changes_queue_rows(state).empty());
	assert(has_text("Selected change outside current filters") && expanded_count() == 1);
	assert(state.review.counts.pending() == 80);
	preview("selection-outside-filter");
	click(workbench_control(LocalElementName{"clear-selection"}), workbench_scroll);
	assert(state.selected_key.empty() && state.search == "no_matching_change");
	changes_select_key(state, state.review.rows.front().key);
	draw();
	draw();
	click(workbench_control(LocalElementName{"reveal-selection"}), workbench_scroll);
	assert(state.search.empty() && state.domain == 0 && state.status == 0);
	assert(!state.selected_outside_filter && !changes_queue_rows(state).empty());

	// Default Context is a short read; full metadata is explicit and remains exactly copyable.
	inspector_width = 320;
	state.inspector_expanded = {true, true, true, false, false};
	draw();
	draw();
	assert(surface_has_text(FlowElementID{inspector_id.value}, "Scope"));
	assert(surface_has_text(FlowElementID{inspector_id.value}, "Source"));
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "Saved →"));
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "For next bake"));
	const auto default_context = Clay_GetScrollContainerData(inspector_scroll);
	assert(default_context.found && default_context.contentDimensions.height <=
										default_context.scrollContainerDimensions.height + 1);
	preview("context-default-320");
	click(inspector_control(Keyed("section", 4)), inspector_scroll);
	const auto copy_identity = inspector_control(
		Keyed("copy-detail", ::FlowUi::detail::identity_hash::hashBytes("Canonical identity")));
	click(copy_identity, inspector_scroll);
	assert(app.clipboardText() == state.review.rows.front().key);
	const auto copy_clay_id = app.ui().toClayEID(copy_identity);
	const auto registered_copy =
		std::ranges::find(state.focus_targets, copy_clay_id.id,
						  [](const auto& target) { return target.identity.id; });
	assert(registered_copy != state.focus_targets.end());
	const auto registered_index =
		static_cast<std::size_t>(registered_copy - state.focus_targets.begin());
	const auto next_target =
		state.focus_targets[(registered_index + 1) % state.focus_targets.size()].identity;
	const auto selected_before_tab = state.selected_key;
	const auto pending_before_tab = state.pending;
	draw({}, 0, GLFW_KEY_TAB);
	draw();
	assert(app.ui().shortcuts().focusedElement().id == next_target.id);
	draw({}, 0, GLFW_KEY_TAB, {-1, -1}, {}, true);
	draw();
	assert(app.ui().shortcuts().focusedElement().id == copy_clay_id.id);
	app.ui().setClipboardText("Keyboard copy must replace this marker.");
	draw({}, 0, GLFW_KEY_ENTER);
	draw();
	assert(app.clipboardText() == state.review.rows.front().key);
	assert(state.selected_key == selected_before_tab && state.pending == pending_before_tab);
	const auto expand_identity = inspector_control(
		Keyed("expand-detail", ::FlowUi::detail::identity_hash::hashBytes("Source header")));
	click(expand_identity, inspector_scroll);
	for (const auto width : {280.0f, 320.0f, 360.0f, 450.0f}) {
		inspector_width = width;
		draw();
		draw();
		check_assigned_width(app, FlowElementID{inspector_id.value}, width);
		check_bounds(app, FlowElementID{inspector_id.value});
		check_bounds(app, FlowElementID{workbench_id.value});
		preview("inspector-" + std::to_string(static_cast<int>(width)));
	}
	Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y = 0;
	draw();
	wheel(inspector_scroll, -4);
	const auto inspector_position = Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y;
	wheel(workbench_scroll, -4);
	assert(Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y == inspector_position);
	wheel(workbench_scroll, 4);
	wheel(inspector_scroll, 4);
	wheel(selector_scroll, -4);
	wheel(workbench_scroll, -4);
	wheel(inspector_scroll, -4);
	const std::array<float, 3> retained_offsets{
		Clay_GetScrollContainerData(selector_scroll).scrollPosition->y,
		Clay_GetScrollContainerData(workbench_scroll).scrollPosition->y,
		Clay_GetScrollContainerData(inspector_scroll).scrollPosition->y};
	const std::array<Clay_ElementId, 3> restored_viewports{selector_scroll, workbench_scroll,
														   inspector_scroll};
	draw_changes = false;
	for (int frame = 0; frame < 8; ++frame)
		draw();
	for (const auto identity : restored_viewports)
		assert(!Clay_GetScrollContainerData(identity).found);
	draw_changes = true;
	draw();
	draw();
	draw();
	for (std::size_t index = 0; index < restored_viewports.size(); ++index) {
		const auto restored = Clay_GetScrollContainerData(restored_viewports[index]);
		assert(restored.found && restored.scrollPosition);
		if (std::abs(restored.scrollPosition->y - retained_offsets[index]) > 1)
			std::cerr << "Scroll restore pane=" << index << " saved=" << retained_offsets[index]
					  << " restored=" << restored.scrollPosition->y
					  << " retained=" << state.selector_scroll << ',' << state.workbench_scroll
					  << ',' << state.inspector_scroll
					  << " viewport=" << restored.scrollContainerDimensions.height
					  << " content=" << restored.contentDimensions.height << '\n';
		if (std::abs(restored.scrollPosition->y - retained_offsets[index]) > 1) {
			std::cerr << "Scroll restoration index=" << index
					  << " before=" << retained_offsets[index]
					  << " after=" << restored.scrollPosition->y
					  << " shared=" << state.selector_scroll << ',' << state.workbench_scroll << ','
					  << state.inspector_scroll << " content=" << restored.contentDimensions.height
					  << " viewport=" << restored.scrollContainerDimensions.height << '\n';
			const auto* local = app.ui().elements().getStatePointerConst(
				kDevChangesWorkbench, app.ui().windowId(), FlowElementID{workbench_id.value});
			if (local)
				std::cerr << "WB view=" << local->previous_view
						  << " selected=" << local->previous_selection
						  << " reveal=" << local->reveal_pending
						  << " page-change=" << state.workbench_page_changed << '\n';
		}
		assert(std::abs(restored.scrollPosition->y - retained_offsets[index]) <= 1);
	}
	preview("scroll-restored-after-tab-return");

	// Structured comparisons remain usable on both sides of the responsive stacking boundary.
	changes_select_key(state, state.review.rows[5].key);
	selector_width = 450;
	for (const auto width : {500.0f, 560.0f, 600.0f}) {
		inspector_width = 1400 - selector_width - width;
		draw();
		draw();
		check_assigned_width(app, FlowElementID{workbench_id.value}, width);
		check_bounds(app, FlowElementID{workbench_id.value});
		check_bounds(app, FlowElementID{inspector_id.value});
		assert(expanded_count() == 1);
		preview("workbench-width-" + std::to_string(static_cast<int>(width)));
	}
	selector_width = 280;
	inspector_width = 320;

	// Runtime feedback and history are local, and disabled commands explain their exact reason.
	state.inspector_expanded[3] = true;
	state.inspector_expanded[4] = false;
	draw();
	draw();
	const auto apply_live = inspector_control(LocalElementName{"apply-live-menu"});
	click(apply_live, inspector_scroll);
	const auto* context_state = app.ui().elements().getStatePointerConst(
		kDevChangesInspector, app.ui().windowId(), FlowElementID{inspector_id.value});
	assert(context_state && context_state->live_menu_open);
	assert(surface_has_text(FlowElementID{inspector_id.value}, "running application"));
	const auto pending_before_focus = state.pending;
	draw({}, 0, GLFW_KEY_DOWN);
	draw();
	assert(state.pending == pending_before_focus);
	assert(app.ui().shortcuts().focusedElement().id ==
		   app.ui().toClayEID(inspector_control(Keyed("live-action", 0))).id);
	state.message = "Manifest decision remains pending.";
	state.runtime_message = "Live edit rejected: fixture reason.";
	state.runtime_message_is_error = true;
	draw();
	assert(
		surface_has_text(FlowElementID{inspector_id.value}, "Live edit rejected: fixture reason."));
	assert(!surface_has_text(FlowElementID{workbench_id.value},
							 "Live edit rejected: fixture reason."));
	assert(
		!surface_has_text(FlowElementID{inspector_id.value}, "Manifest decision remains pending."));
	state.runtime_transaction = 9001;
	draw();
	assert(surface_has_text(FlowElementID{inspector_id.value}, "acknowledgement"));
	preview("live-tools-pending");
	state.runtime_transaction = 0;
	draw({}, 0, GLFW_KEY_ESCAPE);
	draw();
	context_state = app.ui().elements().getStatePointerConst(
		kDevChangesInspector, app.ui().windowId(), FlowElementID{inspector_id.value});
	assert(context_state && !context_state->live_menu_open);
	assert(app.ui().shortcuts().focusedElement().id == app.ui().toClayEID(apply_live).id);
	state.runtime_message.clear();
	state.message.clear();

	// Capacity evidence is distinct from live observations, and acknowledgement is session-wide.
	auto& memory_row = state.review.rows[55];
	const auto memory_key = memory_row.key;
	memory_row.proposed_memory->evidence.complete = false;
	state.review.rows[56].proposed_memory->evidence.complete = false;
	memory_row.blocking = state.review.rows[56].blocking = true;
	memory_row.validity = state.review.rows[56].validity = DevChangesValidity::Invalid;
	memory_row.reason = state.review.rows[56].reason = "Incomplete prepared memory evidence.";
	state.memory_observations.emplace_back(
		DevChangesMemoryObservation{.key = memory_key,
									.startup = "4194304 bytes",
									.demand = "2621440 bytes",
									.allocations = "Persistent storage: 8388608 bytes",
									.observed_ns = 9000,
									.observed_tick = 40,
									.peak_ns = 8500});
	rebuild_fixture_output(state);
	changes_select_key(state, memory_key);
	state.search = memory_row.field_path;
	draw();
	draw();
	assert(surface_has_text(FlowElementID{workbench_id.value}, "Use registered default"));
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "Live tools"));
	assert(surface_has_text(FlowElementID{workbench_id.value}, "2 prepared memory proposals"));
	assert(surface_has_text(FlowElementID{workbench_id.value}, "1 hidden by filters"));
	const auto acknowledge = workbench_control(LocalElementName{"acknowledge-memory"});
	click(acknowledge, workbench_scroll);
	assert(state.acknowledge_incomplete && state.pending == DevChangesCommand::Refresh);
	state.pending = DevChangesCommand::None;
	click(acknowledge, workbench_scroll);
	assert(!state.acknowledge_incomplete && state.pending == DevChangesCommand::Refresh);
	state.pending = DevChangesCommand::None;
	click(inspector_control(LocalElementName{"evidence-details"}), inspector_scroll);
	assert(surface_has_text(FlowElementID{inspector_id.value}, "PREPARED EVIDENCE"));
	assert(surface_has_text(FlowElementID{inspector_id.value}, "LIVE OBSERVATIONS"));
	preview("memory-evidence");
	state.search.clear();

	// Shared startup drafts live in an explicitly opened workbench editor.
	changes_select_key(state, "group:Memory");
	draw();
	draw();
	click(workbench_control(LocalElementName{"edit-policy"}), workbench_scroll);
	assert(state.view == 2);
	check_centered_policy(app);
	static constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.changes.policy.factor")>
		growth_input{};
	const auto growth_identity =
		local(FlowElementID{workbench_id.value}, decltype(growth_input)::definitionId,
			  LocalElementName{"policy-growth-input"});
	const auto set_growth = local(FlowElementID{workbench_id.value},
								  FSEL::RadioChoice::definitionId, Keyed("growth-intent", 1));
	click(set_growth, workbench_scroll);
	app.ui().inputFields().requestCaret(growth_identity, CaretRequestKind::SetPrimary);
	assert(app.ui().inputFields().replaceText(growth_identity, "-", true));
	draw();
	draw();
	const auto* invalid_growth = app.ui().elements().getStatePointerConst(
		growth_input, app.ui().windowId(), growth_identity);
	assert(invalid_growth && invalid_growth->textStatus != FSEL::NumericTextStatus::Complete);
	click(workbench_control(LocalElementName{"stage-growth"}), workbench_scroll);
	assert(state.pending == DevChangesCommand::None);
	assert(app.ui().inputFields().replaceText(growth_identity, "1.5", true));
	app.ui().inputFields().requestCaret(growth_identity, CaretRequestKind::ClearAll);
	draw();
	draw();
	check_centered_policy(app);
	assert(!surface_has_text(FlowElementID{inspector_id.value}, "Stage factor"));
	preview("startup-policy-editor");
	click(workbench_control(LocalElementName{"close-policy"}), workbench_scroll);
	assert(state.view == 0);

	// Output includes capacity-only plans and shared policy settings, regardless of display
	// filters.
	state.review.rows.erase(state.review.rows.begin(), state.review.rows.begin() + 55);
	for (auto& row : state.review.rows) {
		row.blocking = false;
		row.validity = DevChangesValidity::Ready;
		row.proposed_memory->evidence.complete = true;
	}
	for (const auto field : {"enabled", "growth_factor", "allow_runtime_growth"}) {
		DevChangesReviewRow row;
		row.key = "memory:policy." + std::string(field);
		row.field_path = field;
		row.target_name = "Shared startup policy";
		row.domain = DevChangesDomain::Memory;
		row.operation = DevChangesOperation::Update;
		row.has_disk = row.has_proposal = true;
		row.on_disk = std::string_view(field) == "growth_factor" ? "1.5" : "true";
		row.proposal = row.next_value =
			std::string_view(field) == "growth_factor" ? "2.0" : "false";
		state.review.rows.emplace_back(std::move(row));
	}
	reset_overview();
	state.view = 1;
	state.domain = 1;
	state.search = "nothing-visible";
	rebuild_fixture_output(state);
	draw();
	draw();
	assert(state.review.candidate.entries.empty());
	assert(state.review.candidate.memoryCapacity->targets.size() == 25);
	assert(state.review.counts.pending() == 28 && state.review.can_commit);
	assert(surface_has_text(FlowElementID{workbench_id.value}, "Memory capacities"));
	assert(surface_has_text(FlowElementID{workbench_id.value}, "Shared memory policy"));
	preview("memory-only-output");
	for (auto& row : state.review.rows) {
		row.operation = DevChangesOperation::Remove;
		row.decision = DevChangesDecision::Remove;
		row.has_proposal = false;
		row.next_value.clear();
		row.proposed_memory.reset();
	}
	rebuild_fixture_output(state);
	draw();
	draw();
	assert(!state.review.candidate.memoryCapacity && state.review.counts.removals == 28);
	assert(surface_has_text(FlowElementID{workbench_id.value}, "28 removals"));
	preview("removal-only-output");

	// Stale/unreadable snapshots preserve review data and cannot queue a bake.
	state = {};
	seed_review(state);
	state.stale = true;
	state.review.valid_disk = false;
	state.review.can_commit = false;
	state.review.error = "The active manifest is unreadable; retained last-valid snapshot.";
	draw();
	draw();
	assert(has_text("Refresh before baking") && has_text("retained last-valid snapshot"));
	const auto bake = local(FlowElementID{controls_id.value}, diagnostics_ui::Control::definitionId,
							LocalElementName{"bake"});
	click(bake);
	assert(state.pending == DevChangesCommand::None && state.review.rows.size() == 80);
	preview("stale-unreadable-review");
	state.stale = false;
	state.review.valid_disk = true;
	state.review.error.clear();
	state.review.rebuild_required = true;
	state.review.rows.reserve(1500);
	for (uint64_t index = 80; index < 1500; ++index) {
		auto row = state.review.rows.front();
		row.key = "large-list:" + std::to_string(index);
		row.field_path = "large_workload.field_" + std::to_string(index);
		row.disk_entry->fieldPath = row.proposed_entry->fieldPath = row.field_path;
		state.review.rows.emplace_back(std::move(row));
	}
	rebuild_fixture_output(state);
	reset_overview();
	draw();
	draw();
	assert(changes_queue_rows(state).size() == 1500 && rendered_queue_count() == 40);
	assert(app.ui().devTreeSnapshot().clay.nodes.size() < 4000);
	changes_select_key(state, state.review.rows.back().key);
	draw();
	draw();
	const auto queue = changes_queue_rows(state);
	const auto selected_index =
		static_cast<std::size_t>(selected_changes_row(state) - state.review.rows.data());
	const auto selected_position = std::ranges::find(queue, selected_index) - queue.begin();
	assert(state.workbench_page == static_cast<std::size_t>(selected_position) / 40);
	assert(expanded_count() == 1);
	check_bounds(app, FlowElementID{selector_id.value});
	check_bounds(app, FlowElementID{workbench_id.value});
	check_bounds(app, FlowElementID{inspector_id.value});
	preview("large-list");
	verify_safe_point_service(app);
	verify_catalogue_panel_scrolling(app);
	std::cout << "Changes persistent review, exact output, Context, independent scrolling and "
				 "responsive workflows verified\n";
}
