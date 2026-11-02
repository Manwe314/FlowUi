#include "devSystems/devInterface/Changes/Inspector/DevChangesInspector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include <cmath>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
using Context = DevChangesInspector::BuildContext;
void detail(Context& context, std::string_view label, std::string_view value) {
	const auto identity = ::FlowUi::detail::identity_hash::hashBytes(label);
	const auto panel = Clay_GetElementData(context.clayID());
	const float width =
		panel.found ? panel.boundingBox.width : context.params.interfaceState->inspectorWidth;
	CLAY(context.clayID(Keyed("detail-label", identity)), ui::row(28)) {
		ui::text(context, ui::ellipsize(context, label, width - 110, 10),
				 interface_theme::kTextMuted, 10);
		context.uiManager.createElement(ui::kControl, Keyed("copy-detail", identity))
			.setParameters({.label = "Copy",
							.tooltip = "Copy full " + std::string(label),
							.clipboard_text = std::string(value),
							.icon = ui::Icon::Copy,
							.enabled = !value.empty() && context.uiManager.hasClipboardAccess()})
			.setDevInternalCapture(true)
			.draw();
	}
	ui::text(context, value.empty() ? "Unavailable" : value, interface_theme::kTextSecondary, 12);
}
void policy_editor(Context& context, DevChangesState& state) {
	if (!state.policy_editor_initialized) {
		if (const auto& memory = state.review.candidate.memoryCapacity) {
			state.policy_enabled_choice = memory->enabled;
			state.policy_growth_draft = memory->growth_factor.value_or(1.5f);
			state.policy_runtime_choice = memory->allow_runtime_growth.value_or(true);
		}
		state.policy_editor_initialized = true;
	}
	ui::text(context, "SHARED POLICY DRAFTS", interface_theme::kTextMuted, 10);
	ui::text(context,
			 "Each Stage action creates an explicit proposal. Clear releases the optional setting "
			 "to startup fallback.",
			 interface_theme::kTextSecondary, 11);
	ui::text(context, "Memory section", interface_theme::kTextMuted, 10);
	CLAY(context.clayID("policy-enabled"), selector_choice_track()) {
		ui::choice(context, Keyed("enabled-choice", 0), "Disabled", 0, state.policy_enabled_choice);
		ui::choice(context, Keyed("enabled-choice", 1), "Enabled", 1, state.policy_enabled_choice);
	}
	ui::button(context, LocalElementName{"stage-enabled"}, "Stage section state", state,
			   ui::StagePolicy, 0);
	ui::text(context, "Storage growth factor", interface_theme::kTextMuted, 10);
	static constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.changes.policy.factor")>
		growth_input{};
	FSEL::NumberInputParameters<double> growth{};
	growth.value = &state.policy_growth_draft;
	growth.minimum = 1.001;
	growth.maximum = 64;
	growth.step = .05;
	growth.centerText = true;
	growth.fontSize = 12;
	growth.padding = Clay_Padding{8, 8, 4, 4};
	growth.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
	context.uiManager.createElement(growth_input, LocalElementName{"policy-growth-input"})
		.setParameters(growth)
		.setDevInternalCapture(true)
		.draw();
	const auto growth_id = ::FlowUi::detail::element_id::resolveLocal(
		context.id, std::remove_cvref_t<decltype(growth_input)>::definitionId,
		LocalElementName{"policy-growth-input"}.token);
	const auto* field = context.uiManager.elements().getStatePointerConst(
		growth_input, context.uiManager.windowId(), growth_id);
	state.policy_growth_valid =
		(!field || field->textStatus == FSEL::NumericTextStatus::Complete) &&
		std::isfinite(state.policy_growth_draft) && state.policy_growth_draft > 1;
	if (!state.policy_growth_valid)
		ui::text(context, "Complete a valid factor greater than one before staging.",
				 interface_theme::kStatusAmber, 11);
	CLAY(context.clayID("growth-actions"), ui::row(28)) {
		ui::button(context, LocalElementName{"stage-growth"}, "Stage factor", state,
				   ui::StagePolicy, 1, state.policy_growth_valid);
		ui::button(context, LocalElementName{"clear-growth"}, "Clear factor", state,
				   ui::StagePolicy, 3);
	}
	ui::text(context, "Permit storage runtime growth", interface_theme::kTextMuted, 10);
	CLAY(context.clayID("policy-runtime"), selector_choice_track()) {
		ui::choice(context, Keyed("runtime-choice", 0), "No", 0, state.policy_runtime_choice);
		ui::choice(context, Keyed("runtime-choice", 1), "Yes", 1, state.policy_runtime_choice);
	}
	CLAY(context.clayID("runtime-actions"), ui::row(28)) {
		ui::button(context, LocalElementName{"stage-runtime"}, "Stage permission", state,
				   ui::StagePolicy, 2);
		ui::button(context, LocalElementName{"clear-runtime"}, "Clear", state, ui::StagePolicy, 4);
	}
	const auto saved_memory = std::ranges::count_if(state.review.rows, [](const auto& row) {
		return row.domain == tooling::DevChangesDomain::Memory && row.has_disk;
	});
	ui::button(context, LocalElementName{"clear-section"},
			   "Remove memory section (" + std::to_string(saved_memory) + ")", state,
			   ui::StagePolicy, 5, saved_memory > 0,
			   "Remove all saved capacities and shared controls from the next manifest. Running "
			   "allocations remain unchanged.");
	ui::text(context,
			 "Disabled sections retain their assignments. Growth controls apply to storage "
			 "targets. Caller prefer_baked_capacities=false bypasses compiled capacities.",
			 interface_theme::kTextMuted, 11);
}
void decisions(Context& context, DevChangesState& state, const DevChangesNode* selected) {
	const std::size_t affected = selected ? selected->rows.size() : state.review.rows.size();
	std::size_t proposals = 0, ready = 0, saved = 0;
	auto count = [&](const auto& row) {
		proposals += row.has_proposal;
		ready += row.has_proposal && (!row.blocking || (selected && selected->leaf));
		saved += row.has_disk;
	};
	if (selected)
		for (const auto index : selected->rows)
			count(state.review.rows[index]);
	else
		for (const auto& row : state.review.rows)
			count(row);
	ui::text(context,
			 std::to_string(affected) + " destinations · " + std::to_string(proposals) +
				 " proposals · " + std::to_string(saved) + " saved",
			 interface_theme::kTextSecondary, 11);
	ui::button(context, LocalElementName{"include"},
			   "Include proposal (" + std::to_string(ready) + ")", state, ui::Include, 0, ready > 0,
			   "Include eligible proposals for all selected descendants. Conflicted proposals "
			   "require resolution first.");
	ui::button(context, LocalElementName{"omit"},
			   "Omit proposal (" + std::to_string(proposals) + ")", state, ui::Omit, 0,
			   proposals > 0,
			   "Retain saved assignments; new omitted proposals stay absent. Live/prepared work "
			   "remains active.");
	ui::button(context, LocalElementName{"keep"},
			   "Keep saved value (" + std::to_string(saved) + ")", state, ui::Keep, 0, saved > 0,
			   "Choose the on-disk value without changing current runtime behavior.");
	const auto* leaf = selected_changes_row(state);
	const bool removes_section = leaf && leaf->key == "memory:policy.enabled";
	const auto removal_count =
		removes_section
			? static_cast<std::size_t>(std::ranges::count_if(
				  state.review.rows,
				  [](const auto& row) {
					  return row.domain == tooling::DevChangesDomain::Memory && row.has_disk;
				  }))
			: saved;
	ui::button(context, LocalElementName{"remove"},
			   (removes_section ? "Remove memory section (" : "Remove from next bake (") +
				   std::to_string(removal_count) + ")",
			   state, ui::Remove, 0, saved > 0,
			   "Stage removal, including unresolved saved identities. Compiled behavior remains "
			   "until rebuild.");
	CLAY(context.clayID("decision-history"), ui::row(28)) {
		ui::button(context, LocalElementName{"undo"}, "Undo", state, ui::Undo, 0,
				   !state.undo.empty(), "Undo the last manifest decision, including removal.");
		ui::button(context, LocalElementName{"redo"}, "Redo", state, ui::Redo, 0,
				   !state.redo.empty());
	}
	ui::text(
		context,
		"Manifest decisions do not mutate live values. Undo removal restores its prior decision.",
		interface_theme::kTextMuted, 11);
}
void live_actions(Context& context, DevChangesState& state,
				  const tooling::DevChangesReviewRow& row) {
	if (row.domain == tooling::DevChangesDomain::Memory) {
		if (row.proposed_memory && !row.proposed_memory->evidence.complete)
			ui::button(context, LocalElementName{"acknowledge-memory"},
					   state.acknowledge_incomplete ? "Undo evidence acknowledgement"
													: "Acknowledge incomplete evidence",
					   state, ui::Acknowledge);
		ui::text(context,
				 "Initial capacity hints apply on future startup. Memory proposals cannot resize "
				 "this application.",
				 interface_theme::kTextSecondary, 11);
		return;
	}
	ui::text(context, "RUNTIME ACTIONS", interface_theme::kTextMuted, 10);
	static constexpr std::array<std::string_view, 4> labels{
		"Apply proposal to live", "Apply saved to live", "Discard live edit", "Reset to authored"};
	static constexpr std::array<uint64_t, 4> actions{ui::ApplyProposal, ui::ApplySaved,
													 ui::DiscardLive, ui::ResetAuthored};
	const auto schema = context.params.app ? context.params.app->devTooling().schemas().view()
										   : devMode::DevSchemaView{};
	for (std::size_t action_index = 0; action_index < labels.size(); ++action_index) {
		tooling::DevChangesRuntimeEdit draft{};
		if (schema)
			draft = tooling::prepare_changes_runtime_edit(
				*schema, context.params.app->devTooling().overrides(), context.params.app->themes(),
				row, context.params.interfaceState->selectedWindowId,
				static_cast<tooling::DevChangesRuntimeAction>(action_index));
		if (!schema)
			draft.error = "Runtime schema unavailable.";
		const std::string tooltip =
			draft.available ? "Queue a validated runtime command. Success is reported after "
							  "safe-point acknowledgement; saved assignments are preserved."
							: draft.error;
		ui::button(context, Keyed("live-action", action_index), std::string(labels[action_index]),
				   state, actions[action_index], 0, draft.available && !state.runtime_transaction,
				   tooltip);
	}
	CLAY(context.clayID("live-history"), ui::row(28)) {
		ui::button(context, LocalElementName{"undo-live"}, "Undo live", state, ui::UndoLive, 0,
				   !state.runtime_undo.empty() && !state.runtime_transaction);
		ui::button(context, LocalElementName{"redo-live"}, "Redo live", state, ui::RedoLive, 0,
				   !state.runtime_redo.empty() && !state.runtime_transaction);
	}
	if (state.runtime_transaction)
		ui::text(context, "Waiting for safe-point acknowledgement...",
				 interface_theme::kStatusAmber, 11);
	ui::text(context,
			 "Unsupported/unobserved values are disabled; hover an action for the exact reason. "
			 "For an unresolved identity, author a compatible replacement in Inspect and "
			 "explicitly remove the old saved assignment here.",
			 interface_theme::kTextMuted, 11);
}
} // namespace
void DevChangesInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->changes;
	const auto* selected = selected_changes_node(state);
	const auto* row = selected_changes_row(state);
	const auto* entry = row && row->proposed_entry ? &*row->proposed_entry
						: row && row->disk_entry   ? &*row->disk_entry
												   : nullptr;
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float assigned_width =
		parent.found ? parent.boundingBox.width : context.params.interfaceState->inspectorWidth;
	auto root = ui::column(0);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.sizing.height = CLAY_SIZING_GROW(0);
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		auto heading = ui::column(5);
		heading.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		heading.layout.padding = {12, 12, 12, 10};
		heading.backgroundColor = interface_theme::kDepth2Ink;
		CLAY(context.clayID("heading"), heading) {
			ui::text(context, "CHANGE DETAILS", interface_theme::kTextCanvas, 12);
			ui::text(context,
					 ui::ellipsize(context, selected ? selected->label : "Whole manifest",
								   assigned_width - 24, 12),
					 interface_theme::kTextSecondary, 12);
		}
		auto viewport = ui::viewport(context, "scroll-body", state.inspector_scroll);
		viewport.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		CLAY(context.clayID("scroll-body"), viewport) {
			auto sections_panel = ui::column(0);
			sections_panel.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			CLAY(context.clayID("sections"), sections_panel) {
				static constexpr std::array<std::string_view, 6> sections{
					"Decision", "Target", "Values", "Validation", "Origin", "Output"};
				for (std::size_t section_index = 0; section_index < sections.size();
					 ++section_index) {
					context.uiManager.createElement(ui::kControl, Keyed("section", section_index))
						.setParameters({.label = std::string(sections[section_index]),
										.icon = state.inspector_expanded[section_index]
													? ui::Icon::Expanded
													: ui::Icon::Collapsed,
										.activate = ui::activate,
										.target = &state,
										.action = ui::Section,
										.value = section_index,
										.domain_row = true})
						.setDevInternalCapture(true)
						.draw();
					if (!state.inspector_expanded[section_index])
						continue;
					auto body = ui::column(8);
					body.layout.sizing.width = CLAY_SIZING_PERCENT(1);
					body.layout.padding = {12, 12, 10, 12};
					CLAY(context.clayID(Keyed("section-body", section_index)), body) {
						if (section_index == 0) {
							decisions(context, state, selected);
							if (!selected || selected->key.starts_with("group:Memory") ||
								(row && row->domain == tooling::DevChangesDomain::Memory))
								policy_editor(context, state);
							if (row) {
								ui::button(context, LocalElementName{"edit"},
										   row->domain == tooling::DevChangesDomain::Memory
											   ? "Edit in Memory"
											   : "Edit in Inspect",
										   state, ui::Edit);
								live_actions(context, state, *row);
							}
						}
						if (section_index == 1) {
							if (row) {
								detail(context, "Canonical identity", row->key);
								detail(context, "Full field path", row->field_path);
							}
							if (entry) {
								detail(context, "Owner / value type",
									   entry->ownerCppType + " / " + entry->valueTypeName);
								detail(context, "Source header", entry->sourceHeader);
								detail(context, "Field ID", std::to_string(entry->fieldId));
								if (row->domain == tooling::DevChangesDomain::Themes) {
									detail(context, "Theme type", std::to_string(entry->themeType));
									detail(context, "Variant",
										   entry->themeVariant == "*"
											   ? "All variants (including future registrations)"
											   : entry->themeVariant);
								} else {
									detail(context, "Definition ID",
										   std::to_string(entry->definition.value));
									detail(context, "Scope",
										   entry->targetScope ==
												   tooling::DevOverrideScope::Definition
											   ? "Definition"
											   : "Exact persisted instance; all matching windows");
									if (entry->targetScope ==
										tooling::DevOverrideScope::ExactInstance)
										detail(context, "Stable instance",
											   entry->instanceDebugLabel + " · " +
												   std::to_string(entry->instanceKey.value));
								}
							} else if (!row)
								ui::text(context,
										 "Select a field for exact identity. Group decisions are "
										 "recorded independently for each leaf.");
						}
						if (section_index == 2) {
							if (row) {
								detail(context, "On disk", ui::value(row->on_disk, row->has_disk));
								detail(context, "Proposed",
									   row->proposal_removes
										   ? "Remove assignment"
										   : ui::value(row->proposal, row->has_proposal));
								detail(context, "Next bake",
									   row->operation == tooling::DevChangesOperation::Remove
										   ? "Absent"
										   : ui::value(row->next_value, !row->next_value.empty()));
								detail(context,
									   "Retained live assignment (scope precedence applies)",
									   row->live_value);
								detail(context, "Authored baseline",
									   row->authored_value.empty()
										   ? "Unavailable; no substitute baseline is inferred"
										   : row->authored_value);
								detail(context, "Compiled baseline",
									   row->compiled_value.empty()
										   ? "Typed value unavailable; generated digest identifies "
											 "compiled head"
										   : row->compiled_value);
							} else
								ui::text(context,
										 "Select an individual field to compare exact values. Open "
										 "Final manifest for the full merged candidate.");
						}
						if (section_index == 3) {
							if (state.stale)
								ui::text(
									context,
									"Review revisions changed. Refresh is required before bake.",
									interface_theme::kStatusAmber);
							if (!state.review.valid_disk)
								ui::text(context,
										 state.review.error.empty()
											 ? "The active manifest could not be validated."
											 : state.review.error,
										 interface_theme::kStatusRed);
							if (!state.memory_error.empty())
								ui::text(context, state.memory_error, interface_theme::kStatusRed,
										 11);
							if (row) {
								detail(context, "Validity",
									   tooling::changes_validity_name(row->validity));
								detail(context, "Reason",
									   row->reason.empty()
										   ? "No blocking validation issue for this destination."
										   : row->reason);
								detail(context, "Decision", changes_decision_name(row->decision));
							} else
								ui::text(
									context,
									"Conflicts " + std::to_string(state.review.counts.conflicts) +
										" · errors " + std::to_string(state.review.counts.errors) +
										" · warnings " +
										std::to_string(state.review.counts.warnings));
							ui::text(context,
									 "Missing observation does not prove a target was removed. "
									 "Unresolved saved entries remain visible until explicitly "
									 "resolved or removed.",
									 interface_theme::kTextMuted, 11);
							ui::button(context, LocalElementName{"refresh"}, "Refresh review",
									   state, ui::Refresh);
						}
						if (section_index == 4) {
							if (row)
								detail(context, "Origin", row->origin);
							if (entry) {
								detail(context, "Author",
									   entry->provenance.author.empty() ? "Unknown"
																		: entry->provenance.author);
								detail(context, "Timestamp",
									   entry->provenance.timestamp.empty()
										   ? "Unknown"
										   : entry->provenance.timestamp);
								detail(context, "Note",
									   entry->provenance.note.empty() ? "Unknown"
																	  : entry->provenance.note);
							}
							ui::text(context,
									 "The active manifest is a current head, not an audit log. "
									 "Missing historical metadata is unknown.",
									 interface_theme::kTextMuted, 11);
						}
						if (section_index == 5) {
							detail(context, "Active manifest",
								   state.review.manifest_path.generic_string());
							detail(context, "Reviewed disk revision", state.review.disk_revision);
							detail(context, "Schema / proposal / prepared revisions",
								   std::to_string(state.review.schema_generation) + " / " +
									   std::to_string(state.review.proposal_revision) + " / " +
									   std::to_string(state.review.memory_revision));
							if (entry)
								detail(context, "Initializer metadata (review only)",
									   entry->cppValue);
							ui::text(
								context,
								state.review.rebuild_required
									? "Manifest differs from this executable; rebuild required."
									: "Compiled content matches the reviewed disk head.",
								state.review.rebuild_required ? interface_theme::kStatusAmber
															  : interface_theme::kTextSecondary,
								11);
						}
					}
				}
				if (!state.message.empty()) {
					auto message = ui::column();
					message.layout.padding = {12, 12, 10, 12};
					CLAY(context.clayID("message"), message)
					ui::text(context, state.message,
							 state.message_is_error ? interface_theme::kStatusRed
													: interface_theme::kAccentSeaGlass,
							 11);
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
