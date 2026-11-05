#include "devSystems/devInterface/Changes/Selector/DevChangesSelector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include <array>
#include <cmath>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
using Context = DevChangesSelector::BuildContext;
constexpr float node_height = DevChangesSelectorNode::row_height;

using ui::operation_color;
using ui::operation_label;
[[nodiscard]] Clay_ElementDeclaration
section(float height, Clay_Color background = interface_theme::kDepth1Panel) noexcept {
	auto declaration = ui::row(height);
	declaration.layout.padding = {8, 8, 7, 7};
	declaration.backgroundColor = background;
	declaration.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 0, 1, 0}};
	return declaration;
}
template <class BuildContext>
void label(BuildContext& context, std::string_view value, Clay_Color color, uint16_t size = 11) {
	Clay_TextElementConfig configuration{};
	configuration.fontSize = size;
	configuration.textColor = color;
	configuration.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(configuration));
}
void summary(Context& context, DevChangesState& state, float panel_width) {
	auto summary = section(88);
	summary.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	summary.layout.childGap = 8;
	CLAY(context.clayID("summary"), summary) {
		const auto& counts = state.review.counts;
		CLAY(context.clayID("operation-counts"), ui::row(30)) {
			const std::array<std::size_t, 3> values{counts.additions, counts.updates,
													counts.removals};
			static constexpr std::array<tooling::DevChangesOperation, 3> operations{
				tooling::DevChangesOperation::Add, tooling::DevChangesOperation::Update,
				tooling::DevChangesOperation::Remove};
			for (std::size_t index = 0; index < values.size(); ++index) {
				auto metric = ui::column(2);
				metric.layout.sizing.height = CLAY_SIZING_GROW(0);
				CLAY(context.clayID(Keyed("operation-count", index)), metric) {
					label(context, operation_label(operations[index]),
						  operation_color(operations[index]), 10);
					label(context, std::to_string(values[index]), interface_theme::kTextCanvas, 14);
				}
			}
		}
		CLAY(context.clayID("actions"), ui::row(28)) {
			ui::button(context, LocalElementName{"overview"}, "Overview", state, ui::Overview, 0,
					   true,
					   "Review the whole manifest. Display filters do not change bake scope.");
			auto issues = ui::row(28);
			issues.layout.childAlignment.x = CLAY_ALIGN_X_RIGHT;
			CLAY(context.clayID("issue-count"), issues) {
				const auto issue_count = counts.conflicts + counts.errors;
				const bool compact = !FLOWUI_INCLUDE_ICON_MANAGER && panel_width < 250;
				const auto description =
					compact ? issue_count ? "! " + std::to_string(issue_count) : "OK"
							: std::to_string(issue_count) + " issues";
				label(context, description,
					  issue_count ? interface_theme::kStatusRed : interface_theme::kTextMuted, 10);
			}
#if FLOWUI_INCLUDE_ICON_MANAGER
			ui::button(context, LocalElementName{"refresh"}, "", state, ui::Refresh, 0, true,
					   "Refresh review: accept current disk, live edits and prepared memory.",
					   ui::Icon::Refresh);
#else
			ui::button(context, LocalElementName{"refresh"}, "Refresh", state, ui::Refresh, 0, true,
					   "Refresh review: accept current disk, live edits and prepared memory.");
#endif
		}
		if (context.uiManager.getPreviousFramesInteraction().isHovered(
				context.clayID("operation-counts")) ||
			context.uiManager.getPreviousFramesInteraction().isHovered(
				context.clayID("issue-count")))
			diagnostics_ui::pointer_tooltip(
				context, LocalElementName{"summary-tooltip"},
				"Whole manifest: " + std::to_string(counts.live_edits) + " live edits, " +
					std::to_string(counts.prepared_memory) + " prepared memory, " +
					std::to_string(counts.omitted) + " omitted proposals. " +
					std::to_string(counts.conflicts) + " conflicts, " +
					std::to_string(counts.errors) + " errors, " + std::to_string(counts.warnings) +
					" warnings. New adds a missing assignment; "
					"Update replaces a saved value; Removal deletes a saved assignment. "
					"Red markers indicate errors independently of change type.");
	}
}
void keyboard(DevChangesSelector::BuildContext& context, DevChangesState& state) {
	if (!state.keyboard_active || state.visible_nodes.empty() ||
		context.uiManager.inputFields().hasPrimaryFieldFocus())
		return;
	const auto focused = context.uiManager.shortcuts().focusedElement();
	const bool owns_focus = std::ranges::any_of(state.visible_nodes, [&](auto index) {
		return focused.id == FlowIDToClayID(::FlowUi::detail::element_id::resolveLocal(
								 context.id, DevChangesSelectorNode::definitionId,
								 Keyed("node", state.nodes[index].identity).token));
	});
	if (!owns_focus)
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
			context.id, DevChangesSelectorNode::definitionId, Keyed("node", identity).token);
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
			context.id, DevChangesSelectorNode::definitionId, Keyed("node", node.identity).token);
		if (context.uiManager.shortcuts().focusedElement().id != FlowIDToClayID(element))
			return;
		changes_select_key(state, node.key);
		rebuild_changes_model(state);
		return;
	}
	if (moved) {
		const auto viewport = Clay_GetElementData(context.clayID("scroll"));
		const auto scroll = Clay_GetScrollContainerData(context.clayID("scroll"));
		if (viewport.found && scroll.found && scroll.scrollPosition) {
			const float top = position * node_height + scroll.scrollPosition->y;
			if (top < 0)
				scroll.scrollPosition->y -= top;
			if (top + node_height > viewport.boundingBox.height)
				scroll.scrollPosition->y -= top + node_height - viewport.boundingBox.height;
		}
	}
}
} // namespace

void DevChangesSelectorNode::onPressed(InteractionContext& context) {
	FSEL::detail::selectable_surface::onPressed(context, context.params.changes != nullptr);
	context.state().disclosure_armed =
		!context.params.leaf && context.previousInteraction.isPressed(context.clayID("disclosure"));
}
void DevChangesSelectorNode::onReleased(InteractionContext& context) {
	if (!FSEL::detail::selectable_surface::onReleased(context, context.params.changes != nullptr))
		return;
	const bool disclosure = !context.params.leaf &&
							context.previousInteraction.isReleased(context.clayID("disclosure"));
	if (disclosure != context.state().disclosure_armed)
		return;
	context.uiManager.shortcuts().setFocusedElement(context.clayID());
	ui::activate(context.params.changes, disclosure ? ui::Toggle : ui::Select,
				 context.params.identity);
}
void DevChangesSelectorNode::runLogic(InteractionContext& context) {
	FSEL::detail::selectable_surface::runLogic(context, context.params.changes != nullptr);
}
void DevChangesSelectorNode::onHovered(InteractionContext& context) {
	context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
void DevChangesSelectorNode::buildElement(BuildContext& context) {
	const auto& parameters = context.params;
	if (parameters.changes) {
		auto& changes = *parameters.changes;
		const bool focused =
			context.uiManager.shortcuts().focusedElement().id == context.clayID().id;
		if (focused) {
			changes.keyboard_active = true;
			changes.focused_identity = parameters.identity;
		}
		if (focused || parameters.selected || (!changes.focused_identity && parameters.depth == 0))
			ui::register_focus_target(context, changes, context.clayID());
	}
	const bool hovered =
		context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID());
	auto row = ui::row(row_height);
	row.layout.padding = {4, 8, 3, 3};
	row.layout.childGap = 0;
	row.border = {.color = parameters.selected ? interface_theme::kAccentCurrent : Clay_Color{},
				  .width = {3, 0, 0, 0, 0}};
	CLAY(context.clayID(), row) {
		auto indent = ui::column(0);
		indent.layout.sizing = {
			.width = CLAY_SIZING_FIXED(std::min<uint16_t>(parameters.depth, 5) * 14.0f),
			.height = CLAY_SIZING_GROW(0)};
		CLAY(context.clayID("depth"), indent) {}
		auto disclosure = ui::row(19);
		disclosure.layout.sizing.width = CLAY_SIZING_FIXED(18);
		disclosure.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
		CLAY(context.clayID("disclosure"), disclosure) {
			if (!parameters.leaf) {
#if FLOWUI_INCLUDE_ICON_MANAGER
				diagnostics_ui::icon(context, "disclosure-icon",
									 parameters.expanded ? ui::Icon::Expanded : ui::Icon::Collapsed,
									 interface_theme::kTextSecondary, 12);
#else
				label(context, parameters.expanded ? "v" : ">", interface_theme::kTextSecondary,
					  10);
#endif
			}
		}
		auto title = ui::row(19);
		title.clip = {.horizontal = true, .scrollInputDisabled = true};
		CLAY(context.clayID("label"), title)
		label(context, parameters.label,
			  parameters.selected || hovered || parameters.focused
				  ? interface_theme::kTextCanvas
				  : interface_theme::kTextSecondary);
		if (parameters.error) {
			auto error = ui::row(19);
			error.layout.sizing.width = CLAY_SIZING_FIXED(18);
			error.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
			CLAY(context.clayID("error"), error) {
#if FLOWUI_INCLUDE_ICON_MANAGER
				diagnostics_ui::icon(context, "error-icon", ui::Icon::Warning,
									 interface_theme::kStatusRed, 12);
#else
				label(context, "!", interface_theme::kStatusRed, 11);
#endif
			}
		}
		auto trailing = ui::row(19);
		trailing.layout.sizing.width = CLAY_SIZING_FIT(0);
		trailing.layout.padding.left = 6;
		CLAY(context.clayID("operation"), trailing)
		label(context, parameters.trailing, parameters.operation_color, 10);
		if (hovered && !parameters.tooltip.empty())
			diagnostics_ui::pointer_tooltip(context, LocalElementName{"tooltip"},
											parameters.tooltip);
	}
}

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
	root.layout.sizing.height = CLAY_SIZING_PERCENT(1);
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		auto title = section(36, interface_theme::kDepth2Ink);
		title.layout.padding = {12, 12, 0, 0};
		CLAY(context.clayID("title"), title)
		label(context, "Changes", interface_theme::kTextCanvas, 14);
		auto domains = ui::column(0);
		domains.backgroundColor = interface_theme::kDepth0Keel;
		domains.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 0, 1, 0}};
		CLAY(context.clayID("domain-section"), domains) {
			static constexpr std::array<std::string_view, 4> labels{"All", "Elements", "Themes",
																	"Memory"};
			for (uint64_t domain = 0; domain < labels.size(); ++domain)
				ui::bar_choice(context, Keyed("domain", domain), labels[domain], domain,
							   state.domain);
		}
		summary(context, state, panel_width);
		CLAY(context.clayID("filters"), section(44)) {
			context.uiManager.createElement(kDevSelectorSearch, LocalElementName{"search"})
				.setParameters({.query = &state.search,
								.placeholder = "Search...",
								.center_text = true,
								.inline_control = true})
				.setDevInternalCapture(true)
				.draw();
			const auto search = ::FlowUi::detail::element_id::resolveLocal(
				context.id, DevSelectorSearch::definitionId, LocalElementName{"search"}.token);
			const auto search_input = ::FlowUi::detail::element_id::resolveLocal(
				search, FSEL::TextInput::definitionId, LocalElementName{"search-input"}.token);
			ui::register_focus_target(context, state, context.uiManager.toClayEID(search_input),
									  true, search_input);
			static constexpr std::array<FSEL::ComboBoxOption, 5> statuses{
				{{0, "All"}, {1, "Pending"}, {2, "Saved"}, {3, "Issues"}, {4, "Omitted"}}};
			FSEL::ComboBoxParameters status{};
			status.options = statuses;
			status.selectedValue = &state.status;
			status.fontSize = 10;
			status.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
			status.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
			status.sizing = {.width = CLAY_SIZING_FIXED(100), .height = CLAY_SIZING_FIXED(28)};
			context.uiManager.createElement(FSEL::kComboBox, LocalElementName{"status"})
				.setParameters(status)
				.setDevInternalCapture(true)
				.draw();
		}
		auto viewport = ui::viewport(context, "scroll", state.selector_scroll);
		viewport.backgroundColor = interface_theme::kDepth0Keel;
		CLAY(context.clayID("scroll"), viewport) {
			ui::restore_viewport_scroll(context, LocalElementName{"scroll"}, state.selector_scroll);
			ui::FocusViewportScope focus_scope{state, context.clayID("scroll")};
			CLAY(context.clayID("tree"), ui::column(0)) {
				const auto viewport_bounds = Clay_GetElementData(context.clayID("scroll"));
				const float visible_height =
					viewport_bounds.found ? viewport_bounds.boundingBox.height : 500;
				const std::size_t render_begin =
					std::min(state.visible_nodes.size(),
							 static_cast<std::size_t>(std::max(
								 0.0f, std::floor(-state.selector_scroll / node_height) - 3)));
				const std::size_t render_end = std::min(
					state.visible_nodes.size(),
					render_begin +
						static_cast<std::size_t>(std::ceil(visible_height / node_height)) + 8);
				if (render_begin) {
					auto spacer = ui::column(0);
					spacer.layout.sizing.height = CLAY_SIZING_FIXED(node_height * render_begin);
					CLAY(context.clayID("virtual-top"), spacer) {}
				}
				for (std::size_t navigation_index = render_begin; navigation_index < render_end;
					 ++navigation_index) {
					const auto node_index = state.visible_nodes[navigation_index];
					const auto& node = state.nodes[node_index];
					const auto& review_row = state.review.rows[node.rows.front()];
					const bool error = std::ranges::any_of(node.rows, [&](auto row_index) {
						return state.review.rows[row_index].blocking;
					});
					const auto trailing = node.leaf
											  ? std::string(operation_label(review_row.operation))
											  : std::to_string(node.rows.size());
					const auto description =
						node.leaf
							? node.label + "\n" + review_row.key + "\n" +
								  std::string(changes_decision_name(review_row.decision)) + " · " +
								  std::string(tooling::changes_validity_name(review_row.validity)) +
								  "\n" + review_row.reason
							: node.label + "\n" + std::to_string(node.rows.size()) +
								  " destinations" + (error ? " · Contains blocking errors" : "");
					const float label_width = panel_width - std::min<uint16_t>(node.depth, 5) * 14 -
											  42 - trailing.size() * 7.0f - (error ? 18 : 0);
					context.uiManager
						.createElement(kDevChangesSelectorNode, Keyed("node", node.identity))
						.setParameters(
							{.label = ui::ellipsize(context, node.label, label_width, 11),
							 .trailing = trailing,
							 .tooltip = description,
							 .changes = &state,
							 .operation_color = node.leaf ? operation_color(review_row.operation)
														  : interface_theme::kTextMuted,
							 .identity = node.identity,
							 .depth = node.depth,
							 .leaf = node.leaf,
							 .expanded = node.expanded,
							 .selected = state.selected_key == node.key,
							 .focused =
								 state.keyboard_active && state.focused_identity == node.identity,
							 .error = error})
						.setDevInternalCapture(true)
						.draw();
				}
				if (render_end < state.visible_nodes.size()) {
					auto spacer = ui::column(0);
					spacer.layout.sizing.height =
						CLAY_SIZING_FIXED(node_height * (state.visible_nodes.size() - render_end));
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
