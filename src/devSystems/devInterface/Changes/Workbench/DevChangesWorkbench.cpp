#include "devSystems/devInterface/Changes/Workbench/DevChangesWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "internal/MemoryCapacityTargets.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
using Context = DevChangesWorkbench::BuildContext;
[[nodiscard]] Clay_ElementDeclaration bounded_card(uint16_t gap = 7) noexcept {
	auto card = ui::card(gap);
	card.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	return card;
}
[[nodiscard]] std::string scope_label(const tooling::DevChangesReviewRow& row) {
	const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
	if (!entry)
		return row.domain == tooling::DevChangesDomain::Memory
				   ? changes_memory_owner(row.key.starts_with("memory:policy.") ? row.key
																				: row.field_path)
				   : "Unresolved target";
	if (row.domain == tooling::DevChangesDomain::Themes)
		return entry->themeVariant == "*" ? "All variants (including future variants)"
										  : "Variant: " + entry->themeVariant;
	return entry->targetScope == tooling::DevOverrideScope::Definition
			   ? "Definition scope"
			   : "Instance: " + entry->instanceDebugLabel + " · " +
					 std::to_string(entry->instanceKey.value);
}
void comparison(Context& context, const tooling::DevChangesReviewRow& row, uint64_t identity) {
	auto columns = ui::row();
	columns.layout.sizing.height = CLAY_SIZING_FIT(0);
	columns.layout.childAlignment.y = CLAY_ALIGN_Y_TOP;
	CLAY(context.clayID(Keyed("comparison", identity)), columns) {
		for (uint64_t column_index = 0; column_index < 3; ++column_index) {
			auto cell = ui::column(7);
			cell.layout.padding = {8, 8, 8, 8};
			cell.layout.sizing.width = CLAY_SIZING_GROW(0);
			cell.backgroundColor = interface_theme::kDepth0Keel;
			cell.cornerRadius = CLAY_CORNER_RADIUS(3);
			CLAY(context.clayID(Keyed("value", identity * 3 + column_index)), cell) {
				const auto label = column_index == 0   ? "ON DISK"
								   : column_index == 1 ? "PROPOSED"
													   : "NEXT BAKE";
				const auto value = column_index == 0 ? ui::value(row.on_disk, row.has_disk)
								   : column_index == 1
									   ? row.proposal_removes
											 ? "Remove assignment"
											 : ui::value(row.proposal, row.has_proposal)
								   : row.operation == tooling::DevChangesOperation::Remove
									   ? "Absent"
									   : ui::value(row.next_value, !row.next_value.empty());
				ui::text(context, label, interface_theme::kTextMuted, 10);
				ui::text(context, value,
						 column_index == 2 ? interface_theme::kAccentSeaGlass
										   : interface_theme::kTextCanvas,
						 12);
			}
		}
	}
}
void memory_details(Context& context, const DevChangesState& state,
					const tooling::DevChangesReviewRow& row) {
	const auto& memory = row.proposed_memory ? row.proposed_memory : row.disk_memory;
	if (!memory) {
		ui::text(context, "Shared storage policy. These settings are compiled for future startup; "
						  "they do not alter current allocations.");
		return;
	}
	const auto* definition =
		FlowUi::detail::find_initial_capacity(memory_capacity_target_id(memory->key));
	const std::string units =
		definition && definition->unit == FlowUi::detail::InitialCapacityUnit::Bytes ? "bytes"
																					 : "entries";
	ui::text(context, "Initial capacity in " + units + " · " + memory->policy,
			 interface_theme::kTextCanvas);
	if (definition)
		ui::text(context,
				 "Scope: " + std::string(definition->scope) + " · registered default " +
					 std::to_string(definition->default_value) + " " + units,
				 interface_theme::kTextSecondary, 11);
	const auto observed =
		std::ranges::find(state.memory_observations, row.key, &DevChangesMemoryObservation::key);
	if (observed != state.memory_observations.end()) {
		ui::text(context, "Resolved startup hint: " + observed->startup,
				 interface_theme::kTextCanvas, 12);
		ui::text(context,
				 "Active demand high-water: " + (observed->demand.empty()
													 ? "Unavailable at the current monitoring level"
													 : observed->demand),
				 interface_theme::kTextSecondary, 12);
		ui::text(context,
				 "Observed at tick " + std::to_string(observed->observed_tick) + " · " +
					 std::to_string(observed->observed_ns) + " ns · peak at " +
					 std::to_string(observed->peak_ns) + " ns",
				 interface_theme::kTextMuted, 11);
		if (!observed->allocations.empty()) {
			ui::text(context, "Observed physical capacity by source (not summed)",
					 interface_theme::kTextMuted, 10);
			ui::text(context, observed->allocations, interface_theme::kTextSecondary, 11);
		}
	}
	ui::text(context,
			 "Measured demand " + std::to_string(memory->observed_maximum) + " · headroom " +
				 std::to_string(memory->headroom) + " " + units,
			 interface_theme::kAccentSeaGlass, 12);
	const auto& evidence = memory->evidence;
	ui::text(context,
			 "Evidence: " + evidence.kind + " · " +
				 (evidence.source_name.empty() ? "Unnamed dataset" : evidence.source_name),
			 interface_theme::kTextCanvas, 12);
	ui::text(context,
			 "Dataset " + std::to_string(evidence.dataset_id) + " · ticks [" +
				 std::to_string(evidence.begin_tick) + ", " +
				 std::to_string(evidence.end_tick_exclusive) + ") · sample every " +
				 std::to_string(evidence.sample_every_ticks) + " ticks",
			 interface_theme::kTextSecondary, 11);
	ui::text(context,
			 "Time range " + std::to_string(evidence.begin_ns) + "–" +
				 std::to_string(evidence.end_ns) + " ns",
			 interface_theme::kTextMuted, 11);
	ui::text(context,
			 evidence.complete				   ? "Complete retained evidence"
			 : evidence.acknowledge_incomplete ? "Incomplete evidence acknowledged"
											   : "Incomplete evidence requires acknowledgement",
			 evidence.complete ? interface_theme::kTextSecondary : interface_theme::kStatusAmber,
			 11);
	ui::text(context,
			 "Zero uses the registered default. Removing a saved assignment releases it to the "
			 "startup fallback. Per-window and per-worker hints are not total application memory.",
			 interface_theme::kTextSecondary, 11);
	ui::text(context,
			 "Caller/source startup baseline is unavailable here. Resolved startup hints and "
			 "observed physical allocations are separate measurements; open Memory for recipe "
			 "editing and detailed live evidence.",
			 interface_theme::kTextMuted, 11);
}
void leaf(Context& context, DevChangesState& state, const tooling::DevChangesReviewRow& row) {
	CLAY(context.clayID("selected-change"), bounded_card(10)) {
		ui::text(context, row.target_name, interface_theme::kTextCanvas, 16);
		ui::text(context, row.field_path, interface_theme::kTextSecondary, 12);
		ui::text(context, scope_label(row), interface_theme::kTextSecondary, 11);
		ui::text(context,
				 std::string(tooling::changes_operation_name(row.operation)) + " · " +
					 std::string(tooling::changes_validity_name(row.validity)) + " · " +
					 std::string(changes_decision_name(row.decision)),
				 ui::validity_color(row), 12);
		comparison(context, row, 0);
		if (!row.reason.empty())
			ui::text(context, row.reason, ui::validity_color(row), 12);
		if (row.domain == tooling::DevChangesDomain::Memory) {
			memory_details(context, state, row);
			if (row.proposed_memory && !row.proposed_memory->evidence.complete)
				ui::button(context, LocalElementName{"acknowledge-memory"},
						   state.acknowledge_incomplete ? "Undo evidence acknowledgement"
														: "Acknowledge incomplete evidence",
						   state, ui::Acknowledge);
		} else {
			ui::text(
				context,
				"Retained live assignment: " +
					(row.live_value.empty()
						 ? std::string("Unavailable; no retained typed assignment for this target")
						 : row.live_value),
				interface_theme::kTextCanvas, 12);
			ui::text(context,
					 "Authored: " + (row.authored_value.empty()
										 ? std::string("Authored baseline unavailable")
										 : row.authored_value),
					 interface_theme::kTextSecondary, 12);
			ui::text(context,
					 "Compiled: " + (row.compiled_value.empty()
										 ? std::string("Typed compiled baseline unavailable; see "
													   "generated content identity")
										 : row.compiled_value),
					 interface_theme::kTextMuted, 11);
			const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
			if (entry && row.domain == tooling::DevChangesDomain::Themes)
				ui::text(context,
						 entry->themeVariant == "*"
							 ? "All variants: this assignment also applies to future variants. "
							   "Named variant assignments take precedence."
							 : "Named variant: this assignment takes precedence over a broad '*' "
							   "assignment for this field.",
						 interface_theme::kTextSecondary, 11);
			else if (entry)
				ui::text(context,
						 entry->targetScope == tooling::DevOverrideScope::Definition
							 ? "Definition assignment. Exact instance assignments take precedence; "
							   "source invocation baselines may differ across instances."
							 : "Exact persisted instance assignment; runtime windows are not part "
							   "of its saved identity. Definition assignments run first.",
						 interface_theme::kTextSecondary, 11);
		}
		ui::button(context, LocalElementName{"edit"},
				   row.domain == tooling::DevChangesDomain::Memory ? "Edit in Memory"
																   : "Edit in Inspect",
				   state, ui::Edit);
	}
}
} // namespace
void DevChangesWorkbench::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->changes;
	const auto panel_bounds = Clay_GetElementData(context.clayID());
	const auto parent_bounds = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float panel_width = parent_bounds.found  ? parent_bounds.boundingBox.width
							  : panel_bounds.found ? panel_bounds.boundingBox.width
												   : 440;
	if (state.workbench_page_changed) {
		const auto scroll = Clay_GetScrollContainerData(context.clayID("scroll"));
		if (scroll.found && scroll.scrollPosition)
			scroll.scrollPosition->y = 0;
		state.workbench_scroll = 0;
		state.workbench_page_changed = false;
	}
	auto root = ui::column(0);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.sizing.height = CLAY_SIZING_GROW(0);
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), root) {
		auto heading = ui::column(7);
		heading.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		heading.layout.padding = {14, 14, 12, 10};
		heading.backgroundColor = interface_theme::kDepth1Panel;
		CLAY(context.clayID("heading"), heading) {
			const auto* selected = selected_changes_node(state);
			ui::text(context,
					 ui::ellipsize(context, selected ? selected->label : "Manifest review",
								   panel_width - 28, 16),
					 interface_theme::kTextCanvas, 16);
			ui::text(context, changes_summary(state.review.counts), interface_theme::kTextSecondary,
					 11);
			CLAY(context.clayID("views"), selector_choice_track()) {
				ui::choice(context, Keyed("view", 0), "Pending", 0, state.view);
				ui::choice(context, Keyed("view", 1), "Final manifest", 1, state.view);
			}
		}
		auto viewport = ui::viewport(context, "scroll", state.workbench_scroll);
		viewport.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		CLAY(context.clayID("scroll"), viewport) {
			auto content = ui::column(12);
			content.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			content.layout.padding = {14, 14, 12, 16};
			CLAY(context.clayID("contents"), content) {
				if (state.stale)
					ui::text(context,
							 "Review changed. Refresh to accept current sources before baking.",
							 interface_theme::kStatusAmber, 12);
				if (!state.review.error.empty())
					ui::text(context, state.review.error, interface_theme::kStatusRed);
				if (!state.memory_error.empty()) {
					ui::text(context, state.memory_error, interface_theme::kStatusRed);
					ui::button(context, LocalElementName{"acknowledge"},
							   state.acknowledge_incomplete ? "Acknowledged incomplete evidence"
															: "Acknowledge incomplete evidence",
							   state, ui::Acknowledge);
				}
				if (state.selected_outside_filter)
					ui::text(context, "Selection outside filter · its review remains pinned here.",
							 interface_theme::kStatusAmber, 12);
				if (state.view == 1) {
					CLAY(context.clayID("final-summary"), bounded_card()) {
						ui::text(context, "Exact next manifest", interface_theme::kTextCanvas, 14);
						ui::text(context, state.review.manifest_path.generic_string(),
								 interface_theme::kTextSecondary, 11);
						ui::text(context,
								 "This candidate contains " +
									 std::to_string(state.review.candidate.entries.size()) +
									 " element/theme assignments. Baking commits the entire "
									 "reviewed candidate, independent of selector filters.",
								 interface_theme::kTextSecondary, 12);
						ui::text(context,
								 state.review.can_commit && !state.stale ? "Ready to write"
								 : state.review.counts.pending() == 0 && state.review.valid_disk
									 ? "No output changes"
									 : "Resolve blocking issues / refresh before writing",
								 state.review.can_commit && !state.stale
									 ? interface_theme::kAccentSeaGlass
									 : interface_theme::kStatusAmber);
					}
					std::vector<std::size_t> page_offsets;
					page_offsets.reserve(state.review.candidate_json.size() / 8192 + 2);
					page_offsets.emplace_back(0);
					while (page_offsets.back() < state.review.candidate_json.size()) {
						const auto begin = page_offsets.back();
						auto end = std::min(begin + 8192, state.review.candidate_json.size());
						if (end < state.review.candidate_json.size()) {
							const auto newline = state.review.candidate_json.rfind('\n', end);
							if (newline != std::string::npos && newline > begin)
								end = newline + 1;
							else
								while (end > begin && (static_cast<unsigned char>(
														   state.review.candidate_json[end]) &
													   0xc0u) == 0x80u)
									--end;
						}
						page_offsets.emplace_back(end);
					}
					const auto page_count = std::max<std::size_t>(1, page_offsets.size() - 1);
					state.workbench_page = std::min<uint64_t>(state.workbench_page, page_count - 1);
					if (page_count > 1) {
						CLAY(context.clayID("candidate-pages"), ui::row(28)) {
							ui::button(context, LocalElementName{"previous-candidate"}, "Previous",
									   state, ui::PreviousPage, 0, state.workbench_page > 0);
							ui::text(context,
									 std::to_string(state.workbench_page + 1) + " / " +
										 std::to_string(page_count),
									 interface_theme::kTextMuted, 11);
							ui::button(context, LocalElementName{"next-candidate"}, "Next", state,
									   ui::NextPage, 0, state.workbench_page + 1 < page_count);
						}
					}
					const auto candidate_page =
						state.review.candidate_json.empty()
							? std::string_view{}
							: std::string_view(state.review.candidate_json)
								  .substr(page_offsets[state.workbench_page],
										  page_offsets[state.workbench_page + 1] -
											  page_offsets[state.workbench_page]);
					CLAY(context.clayID("candidate"), bounded_card()) {
						ui::text(context,
								 state.review.candidate_json.empty()
									 ? "Candidate unavailable: resolve the active-file error."
									 : candidate_page,
								 interface_theme::kTextSecondary, 11);
					}
				} else if (const auto* row = selected_changes_row(state))
					leaf(context, state, *row);
				else {
					const auto* selected = selected_changes_node(state);
					CLAY(context.clayID("overview"), bounded_card()) {
						ui::text(context,
								 selected
									 ? "Group review · " + std::to_string(selected->rows.size()) +
										   " destinations"
									 : "Active manifest",
								 interface_theme::kTextCanvas, 14);
						ui::text(context, state.review.manifest_path.generic_string(),
								 interface_theme::kTextSecondary, 11);
						ui::text(context,
								 state.review.rebuild_required
									 ? "Rebuild required · disk differs from this executable"
									 : "Compiled content identity matches disk",
								 state.review.rebuild_required ? interface_theme::kStatusAmber
															   : interface_theme::kAccentSeaGlass,
								 12);
						ui::text(context,
								 "Review the on-disk assignment, session proposal and exact next "
								 "value. Decisions change future builds; live application behavior "
								 "stays active.",
								 interface_theme::kTextSecondary, 12);
						ui::text(context,
								 "Conflicts " + std::to_string(state.review.counts.conflicts) +
									 " · errors " + std::to_string(state.review.counts.errors) +
									 " · warnings " + std::to_string(state.review.counts.warnings),
								 state.review.counts.conflicts || state.review.counts.errors
									 ? interface_theme::kStatusRed
									 : interface_theme::kTextSecondary,
								 11);
					}
					if (selected) {
						CLAY(context.clayID("group-actions"), ui::column()) {
							ui::button(context, LocalElementName{"include-group"},
									   "Include ready proposals", state, ui::Include);
							ui::button(context, LocalElementName{"omit-group"},
									   "Omit group proposals", state, ui::Omit);
							ui::text(context,
									 "Group actions cover all " +
										 std::to_string(selected->rows.size()) +
										 " descendants, including those hidden by display filters. "
										 "Conflicted proposals remain unresolved.",
									 interface_theme::kTextMuted, 11);
						}
					}
					std::vector<std::size_t> review_rows;
					review_rows.reserve(selected ? selected->rows.size()
												 : state.review.rows.size());
					for (std::size_t row_index = 0; row_index < state.review.rows.size();
						 ++row_index) {
						const auto& row = state.review.rows[row_index];
						if (selected &&
							std::ranges::find(selected->rows, row_index) == selected->rows.end())
							continue;
						if (!selected && row.operation == tooling::DevChangesOperation::Keep &&
							row.validity == tooling::DevChangesValidity::Ready)
							continue;
						review_rows.emplace_back(row_index);
					}
					constexpr std::size_t page_size = 40;
					const auto page_count =
						std::max<std::size_t>(1, (review_rows.size() + page_size - 1) / page_size);
					state.workbench_page = std::min<uint64_t>(state.workbench_page, page_count - 1);
					if (page_count > 1) {
						CLAY(context.clayID("pages"), ui::row(28)) {
							ui::button(context, LocalElementName{"previous-page"}, "Previous",
									   state, ui::PreviousPage, 0, state.workbench_page > 0);
							ui::text(context,
									 std::to_string(state.workbench_page + 1) + " / " +
										 std::to_string(page_count),
									 interface_theme::kTextMuted, 11);
							ui::button(context, LocalElementName{"next-page"}, "Next", state,
									   ui::NextPage, 0, state.workbench_page + 1 < page_count);
						}
					}
					std::size_t displayed = 0;
					const auto page_end =
						std::min(review_rows.size(), (state.workbench_page + 1) * page_size);
					for (std::size_t page_index = state.workbench_page * page_size;
						 page_index < page_end; ++page_index) {
						const auto& row = state.review.rows[review_rows[page_index]];
						const auto node =
							std::ranges::find(state.nodes, row.key, &DevChangesNode::key);
						if (node == state.nodes.end())
							continue;
						++displayed;
						CLAY(context.clayID(Keyed("review-card", node->identity)), bounded_card()) {
							context.uiManager
								.createElement(ui::kControl, Keyed("review-row", node->identity))
								.setParameters(
									{.label = ui::ellipsize(
										 context, row.target_name + " · " + row.field_path,
										 panel_width - 112 -
											 std::string(
												 tooling::changes_operation_name(row.operation))
													 .size() *
												 8,
										 12),
									 .secondary = ui::ellipsize(
										 context,
										 std::string(changes_decision_name(row.decision)) + " · " +
											 std::string(
												 tooling::changes_validity_name(row.validity)),
										 panel_width - 76, 11),
									 .badge = std::string(
										 tooling::changes_operation_name(row.operation)),
									 .tooltip = row.target_name + " · " + row.field_path + "\n" +
												row.key + "\n" + row.reason,
									 .activate = ui::activate,
									 .target = &state,
									 .action = ui::Select,
									 .value = node->identity,
									 .marker = ui::validity_color(row),
									 .issue_row = true})
								.setDevInternalCapture(true)
								.draw();
							comparison(context, row, node->identity);
							ui::text(context, scope_label(row), interface_theme::kTextMuted, 11);
							if (row.blocking)
								ui::text(context, row.reason, interface_theme::kStatusRed, 11);
						}
					}
					if (!displayed) {
						ui::text(context, "No pending output changes", interface_theme::kTextCanvas,
								 14);
						ui::text(context,
								 "Saved entries remain in the selector. Final manifest shows the "
								 "exact retained candidate.",
								 interface_theme::kTextSecondary, 12);
					}
				}
				if (!state.message.empty())
					ui::text(context, state.message,
							 state.message_is_error ? interface_theme::kStatusRed
													: interface_theme::kAccentSeaGlass,
							 12);
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
