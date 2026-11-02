#include "devSystems/devInterface/Changes/Selector/DevChangesSelector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include <array>
#include <cmath>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
void keyboard(DevChangesSelector::BuildContext& context, DevChangesState& state) {
	if (!state.keyboard_active || state.visible_nodes.empty() ||
		context.uiManager.inputFields().hasPrimaryFieldFocus())
		return;
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	auto pressed = [&](int key) { return current.keyDown[key] && !previous.keyDown[key]; };
	auto found = std::ranges::find_if(state.visible_nodes, [&](auto index) {
		return state.nodes[index].identity == state.focused_identity;
	});
	std::size_t position = found == state.visible_nodes.end()
							   ? 0
							   : static_cast<std::size_t>(found - state.visible_nodes.begin());
	bool moved = false;
	if (pressed(GLFW_KEY_UP) && position > 0) {
		--position;
		moved = true;
	}
	if (pressed(GLFW_KEY_DOWN) && position + 1 < state.visible_nodes.size()) {
		++position;
		moved = true;
	}
	auto& node = state.nodes[state.visible_nodes[position]];
	const auto focus_node = [&](uint64_t identity) {
		const auto element = ::FlowUi::detail::element_id::resolveLocal(
			context.id, diagnostics_ui::Control::definitionId, Keyed("node", identity).token);
		context.uiManager.shortcuts().setFocusedElement(context.uiManager.toClayEID(element));
	};
	if (pressed(GLFW_KEY_LEFT)) {
		if (!node.leaf && node.expanded)
			node.expanded = false;
		else if (!node.parent.empty()) {
			const auto parent = std::ranges::find(state.nodes, node.parent, &DevChangesNode::key);
			if (parent != state.nodes.end())
				state.focused_identity = parent->identity;
		}
		focus_node(state.focused_identity);
		rebuild_changes_model(state);
		return;
	}
	if (pressed(GLFW_KEY_RIGHT) && !node.leaf) {
		node.expanded = true;
		focus_node(node.identity);
		rebuild_changes_model(state);
		return;
	}
	if (moved) {
		state.focused_identity = node.identity;
		focus_node(node.identity);
	}
	if (pressed(GLFW_KEY_ENTER)) {
		const auto element = ::FlowUi::detail::element_id::resolveLocal(
			context.id, diagnostics_ui::Control::definitionId, Keyed("node", node.identity).token);
		if (context.uiManager.shortcuts().focusedElement().id != FlowIDToClayID(element))
			return;
		state.selected_key = node.key;
		rebuild_changes_model(state);
		return;
	}
	if (moved) {
		const auto viewport = Clay_GetElementData(context.clayID("scroll"));
		const auto scroll = Clay_GetScrollContainerData(context.clayID("scroll"));
		if (viewport.found && scroll.found && scroll.scrollPosition) {
			const float top = position * 48.0f + scroll.scrollPosition->y;
			if (top < 0)
				scroll.scrollPosition->y -= top;
			if (top + 48 > viewport.boundingBox.height)
				scroll.scrollPosition->y -= top + 48 - viewport.boundingBox.height;
		}
	}
}
} // namespace
void DevChangesSelector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->changes;
	if (state.model_dirty || state.search != state.applied_search ||
		state.domain != state.applied_domain || state.status != state.applied_status)
		rebuild_changes_model(state);
	const auto panel_bounds = Clay_GetElementData(context.clayID());
	const auto parent_bounds = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float panel_width = parent_bounds.found  ? parent_bounds.boundingBox.width
							  : panel_bounds.found ? panel_bounds.boundingBox.width
												   : context.params.interfaceState->selectorWidth;
	auto root = ui::column(0);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.sizing.height = CLAY_SIZING_GROW(0);
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		auto heading = ui::column(7);
		heading.layout.padding = {10, 10, 10, 9};
		CLAY(context.clayID("heading"), heading) {
			ui::text(context, "CHANGES", interface_theme::kTextCanvas, 12);
			CLAY(context.clayID("domains"), selector_choice_track()) {
				static constexpr std::array<std::string_view, 4> labels{"All", "Elements", "Themes",
																		"Memory"};
				static constexpr std::array<std::string_view, 4> compact_labels{"All", "Elem.",
																				"Theme", "Mem."};
				for (uint64_t domain = 0; domain < labels.size(); ++domain)
					ui::choice(context, Keyed("domain", domain),
							   panel_width < 250 ? compact_labels[domain] : labels[domain], domain,
							   state.domain, panel_width < 250 ? 4 : 6);
			}
			const auto& counts = state.review.counts;
			ui::text(context,
					 "Live edits " + std::to_string(counts.live_edits) + " · Prepared memory " +
						 std::to_string(counts.prepared_memory),
					 interface_theme::kTextSecondary, 10);
			ui::text(context,
					 "Ready " +
						 std::to_string(std::ranges::count_if(
							 state.review.rows,
							 [](const auto& row) {
								 return !row.blocking &&
										(row.operation == tooling::DevChangesOperation::Add ||
										 row.operation == tooling::DevChangesOperation::Update ||
										 row.operation == tooling::DevChangesOperation::Remove);
							 })) +
						 " · Conflicts " + std::to_string(counts.conflicts) + " · Errors " +
						 std::to_string(counts.errors) + " · Omitted " +
						 std::to_string(counts.omitted),
					 counts.conflicts || counts.errors ? interface_theme::kStatusAmber
													   : interface_theme::kTextSecondary,
					 10);
			context.uiManager.createElement(kDevSelectorSearch, LocalElementName{"search"})
				.setParameters({.query = &state.search,
								.placeholder = "Search changes...",
								.center_text = true})
				.setDevInternalCapture(true)
				.draw();
			static constexpr std::array<FSEL::ComboBoxOption, 5> statuses{
				{{0, "All changes"},
				 {1, "Pending output"},
				 {2, "Saved entries"},
				 {3, "Issues"},
				 {4, "Omitted proposals"}}};
			FSEL::ComboBoxParameters status{};
			status.options = statuses;
			status.selectedValue = &state.status;
			status.fontSize = 11;
			status.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
			status.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
			context.uiManager.createElement(FSEL::kComboBox, LocalElementName{"status"})
				.setParameters(status)
				.setDevInternalCapture(true)
				.draw();
			CLAY(context.clayID("actions"), ui::row(28)) {
				ui::button(context, LocalElementName{"overview"}, "Overview", state, ui::Overview);
				ui::button(context, LocalElementName{"refresh"}, "Refresh", state, ui::Refresh, 0,
						   true,
						   "Accept a fresh review of disk, live edits and frozen prepared memory.",
						   ui::Icon::Refresh);
			}
		}
		CLAY(context.clayID("scroll"), ui::viewport(context, "scroll", state.selector_scroll)) {
			CLAY(context.clayID("tree"), ui::column(0)) {
				const auto viewport_bounds = Clay_GetElementData(context.clayID("scroll"));
				const float visible_height =
					viewport_bounds.found ? viewport_bounds.boundingBox.height : 500;
				const std::size_t render_begin =
					std::min(state.visible_nodes.size(),
							 static_cast<std::size_t>(
								 std::max(0.0f, std::floor(-state.selector_scroll / 48) - 3)));
				const std::size_t render_end = std::min(
					state.visible_nodes.size(),
					render_begin + static_cast<std::size_t>(std::ceil(visible_height / 48)) + 8);
				if (render_begin) {
					auto spacer = ui::column(0);
					spacer.layout.sizing.height = CLAY_SIZING_FIXED(48.0f * render_begin);
					CLAY(context.clayID("virtual-top"), spacer) {}
				}
				for (std::size_t navigation_index = render_begin; navigation_index < render_end;
					 ++navigation_index) {
					const auto node_index = state.visible_nodes[navigation_index];
					const auto& node = state.nodes[node_index];
					auto indentation = ui::column(0);
					indentation.layout.padding.left =
						static_cast<uint16_t>(std::min<uint16_t>(node.depth, 5) * 9);
					CLAY(context.clayID(Keyed("indent", node.identity)), indentation) {
						const auto& review_row = state.review.rows[node.rows.front()];
						std::string badge =
							node.leaf
								? std::string(tooling::changes_operation_name(review_row.operation))
								: std::to_string(node.rows.size());
						if (node.leaf &&
							review_row.decision == tooling::DevChangesDecision::OmitProposal)
							badge = "OMITTED";
						const auto& entry = review_row.proposed_entry ? review_row.proposed_entry
																	  : review_row.disk_entry;
						const std::string secondary =
							!node.leaf ? "Select to review this group"
							: entry && review_row.domain == tooling::DevChangesDomain::Themes &&
									entry->themeVariant == "*"
								? "All variants · " + std::string(tooling::changes_validity_name(
														  review_row.validity))
								: std::string(tooling::changes_validity_name(review_row.validity));
						context.uiManager.createElement(ui::kControl, Keyed("node", node.identity))
							.setParameters(
								{.label = ui::ellipsize(context, node.label,
														panel_width -
															std::min<uint16_t>(node.depth, 5) * 9 -
															62 - badge.size() * 8.0f,
														12),
								 .secondary = ui::ellipsize(
									 context, secondary,
									 panel_width - std::min<uint16_t>(node.depth, 5) * 9 - 28, 11),
								 .badge = std::move(badge),
								 .tooltip =
									 node.label +
									 (node.leaf ? "\n" + review_row.key + "\n" + review_row.reason
												: "\n" + std::to_string(node.rows.size()) +
													  " destinations"),
								 .icon = node.leaf		 ? ui::Icon::None
										 : node.expanded ? ui::Icon::Expanded
														 : ui::Icon::Collapsed,
								 .activate = ui::activate,
								 .target = &state,
								 .action = node.leaf ? ui::Select : ui::Toggle,
								 .value = node.identity,
								 .marker = ui::validity_color(review_row),
								 .selected = state.selected_key == node.key,
								 .focused = state.keyboard_active &&
											state.focused_identity == node.identity,
								 .issue_row = true,
								 .keyboard_activation = false})
							.setDevInternalCapture(true)
							.draw();
					}
				}
				if (render_end < state.visible_nodes.size()) {
					auto spacer = ui::column(0);
					spacer.layout.sizing.height =
						CLAY_SIZING_FIXED(48.0f * (state.visible_nodes.size() - render_end));
					CLAY(context.clayID("virtual-bottom"), spacer) {}
				}
				if (state.visible_nodes.empty()) {
					auto empty = ui::column(7);
					empty.layout.padding = {12, 12, 16, 16};
					CLAY(context.clayID("empty"), empty) {
						ui::text(context,
								 state.review.rows.empty() ? "No saved or proposed changes"
														   : "No changes match this filter",
								 interface_theme::kTextCanvas);
						ui::text(context,
								 "Live edits and prepared memory appear here. Saved assignments "
								 "remain visible across sessions.",
								 interface_theme::kTextSecondary, 11);
					}
				}
			}
		}
		auto footer = ui::column(4);
		footer.layout.padding = {10, 10, 8, 8};
		footer.backgroundColor = interface_theme::kDepth2Ink;
		CLAY(context.clayID("footer"), footer) {
			const auto visible_leaves = std::ranges::count_if(
				state.visible_nodes, [&](auto index) { return state.nodes[index].leaf; });
			ui::text(context,
					 std::to_string(visible_leaves) + " visible · " +
						 std::to_string(state.review.rows.size()) + " total destinations",
					 interface_theme::kTextSecondary, 10);
			if (state.selected_outside_filter)
				ui::text(context, "Selection outside filter", interface_theme::kStatusAmber, 11);
			if (state.stale)
				ui::text(context, "Review changed · Refresh required",
						 interface_theme::kStatusAmber, 11);
		}
	}
	keyboard(context, state);
}
} // namespace FlowUi::devSystems::interface_elements
#endif
