#include "devSystems/devInterface/Changes/Workbench/DevChangesWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityTargets.hpp"
#include <cmath>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
using Context = DevChangesWorkbench::BuildContext;
using ReviewRow = tooling::DevChangesReviewRow;
using Decision = tooling::DevChangesDecision;
using Operation = tooling::DevChangesOperation;
constexpr std::size_t review_page_size = 40;

[[nodiscard]] Clay_ElementDeclaration section(uint16_t gap = 8) noexcept {
	auto result = ui::column(gap);
	result.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	return result;
}
[[nodiscard]] Clay_ElementDeclaration action_row(float width) noexcept {
	auto result = width < 400 ? ui::column(8) : ui::row(30);
	result.layout.sizing.height = CLAY_SIZING_FIT(0);
	return result;
}
[[nodiscard]] std::string scope_label(const ReviewRow& row) {
	const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
	if (row.domain == tooling::DevChangesDomain::Memory)
		return row.key.starts_with("memory:policy.") ? "Shared startup policy"
													 : changes_memory_owner(row.field_path);
	if (!entry)
		return "Already Baked target is unresolved";
	if (row.domain == tooling::DevChangesDomain::Themes)
		return entry->themeVariant == "*"
				   ? "All theme variants, including future variants"
				   : "Theme variant: " + (entry->themeVariant.empty() ? std::string("Default")
																	  : entry->themeVariant);
	return entry->targetScope == tooling::DevOverrideScope::Definition
			   ? "All matching instances of this definition"
			   : "Persisted instance: " + (entry->instanceDebugLabel.empty()
											   ? std::string("Unnamed instance")
											   : entry->instanceDebugLabel);
}
[[nodiscard]] std::string next_value(const ReviewRow& row) {
	if (row.blocking)
		return "Unresolved until the blocking issue is addressed";
	if (row.operation == Operation::Remove)
		return "Assignment absent after bake";
	const bool present = row.has_disk || (row.has_proposal && !row.proposal_removes &&
										  row.decision == Decision::IncludeProposal);
	return ui::value(row.next_value, present && row.decision != Decision::Remove);
}
[[nodiscard]] std::string decision_consequence(const ReviewRow& row) {
	switch (row.decision) {
	case Decision::IncludeProposal:
		return row.proposal_removes ? "The proposed removal will be used, subject to validation."
			   : row.has_proposal
				   ? "The proposal will be used for the next bake, subject to validation."
				   : "The already baked assignment is retained; no proposal is available.";
	case Decision::OmitProposal:
		return row.has_disk
				   ? "Already Baked value stays. Live and prepared work remain available."
				   : "This new proposal stays absent. Live and prepared work remain available.";
	case Decision::RetainDisk:
		return "The reviewed already baked value is explicitly retained. Live behavior is "
			   "unchanged.";
	case Decision::Remove:
		return row.key == "memory:policy.enabled" ? "All already baked memory capacities and "
													"shared policy settings will be removed."
												  : "The already baked assignment will be removed. "
													"Compiled behavior remains until "
													"rebuild.";
	}
	return "The next-bake result follows the reviewed decision.";
}
[[nodiscard]] std::string capacity_value(const ReviewRow& row, std::string_view value,
										 bool present) {
	if (!present)
		return "Absent";
	if (value != "0")
		return ui::value(value, true);
	const auto& memory = row.proposed_memory ? row.proposed_memory : row.disk_memory;
	if (!memory)
		return ui::value(value, true);
	const auto* definition =
		memory ? ::FlowUi::detail::find_initial_capacity(memory_capacity_target_id(memory->key))
			   : nullptr;
	return "Use registered default (0)" +
		   (definition ? " · " + std::to_string(definition->default_value) : std::string{});
}
template <class BuildContext, class Identity>
void register_widget(BuildContext& context, DevChangesState& state, FlowDefinitionID definition,
					 const Identity& identity, bool enabled = true, FlowElementID text_input = {}) {
	const auto element =
		::FlowUi::detail::element_id::resolveLocal(context.id, definition, identity.token);
	ui::register_focus_target(context, state, context.uiManager.toClayEID(element), enabled,
							  text_input);
}
[[nodiscard]] const ReviewRow* action_row(const DevChangesWorkbench::State& local,
										  uint64_t identity) noexcept {
	if (!local.changes)
		return nullptr;
	const auto node = std::ranges::find(local.changes->nodes, identity, &DevChangesNode::identity);
	return node != local.changes->nodes.end() && node->leaf && !node->rows.empty()
			   ? &local.changes->review.rows[node->rows.front()]
			   : nullptr;
}
void copy_value(void* target, uint64_t identity, uint64_t column_index) {
	auto& local = *static_cast<DevChangesWorkbench::State*>(target);
	const auto* review_row = action_row(local, identity);
	if (!local.ui_manager || !review_row)
		return;
	local.ui_manager->setClipboardText(column_index == 0   ? review_row->on_disk
									   : column_index == 1 ? review_row->proposal
														   : review_row->next_value);
}
void toggle_review(void* target, uint64_t, uint64_t identity) {
	auto& local = *static_cast<DevChangesWorkbench::State*>(target);
	const auto* review_row = action_row(local, identity);
	if (!review_row)
		return;
	const bool expanded =
		std::ranges::find(local.expanded_reviews, review_row->key) != local.expanded_reviews.end();
	local.expanded_reviews.clear();
	if (!expanded) {
		local.expanded_reviews.reserve(1);
		local.expanded_reviews.emplace_back(review_row->key);
	}
	local.decision_open = local.decision_was_open = false;
}
void batch_decision(void* target, uint64_t, uint64_t decision) {
	auto& local = *static_cast<DevChangesWorkbench::State*>(target);
	if (!local.changes)
		return;
	auto& state = *local.changes;
	queue_changes_scope_decision(state, state.queue_scope_key, static_cast<Decision>(decision));
	state.group_decisions_open = false;
	if (local.ui_manager)
		local.ui_manager->shortcuts().setFocusedElement(local.group_trigger_id);
}
void raw_page_action(void* target, uint64_t action, uint64_t) {
	auto& local = *static_cast<DevChangesWorkbench::State*>(target);
	if (action == 0 && local.raw_page)
		--local.raw_page;
	if (action == 1)
		++local.raw_page;
	local.raw_scroll_x = local.raw_scroll_y = 0;
	const auto scroll = Clay_GetScrollContainerData(local.raw_scroll_id);
	if (scroll.found && scroll.scrollPosition)
		*scroll.scrollPosition = {};
}
void toggle_value(void* target, uint64_t identity, uint64_t column_index) {
	auto& local = *static_cast<DevChangesWorkbench::State*>(target);
	const auto* review_row = action_row(local, identity);
	if (!review_row)
		return;
	const auto key = "value:" + review_row->key + ":" + std::to_string(column_index);
	const auto found = std::ranges::find(local.metadata_expanded, key);
	if (found == local.metadata_expanded.end())
		local.metadata_expanded.emplace_back(key);
	else
		local.metadata_expanded.erase(found);
}
constexpr auto change_decision =
	UiAction("flowui.dev.changes.next-bake-choice",
			 [](DevChangesState& state, DevChangesWorkbench::State& local) {
				 if (local.decision_choice < 4)
					 queue_changes_scope_decision(state, local.decision_target_key,
												  static_cast<Decision>(local.decision_choice));
			 });

/* Flat queue rows retain release-inside behavior without inheriting a card surface. */
struct QueueRow {
	struct Parameters {
		std::string title{}, preview{}, operation{}, tooltip{};
		App* app = nullptr;
		DevChangesState* changes = nullptr;
		DevChangesWorkbench::State* workbench = nullptr;
		FlowElementID queue_parent{};
		Clay_Color operation_color{};
		Clay_ElementId viewport{};
		uint64_t identity = 0;
		float width = 0;
		bool selected = false, blocking = false, excluded = false;
	};
	struct State : FSEL::detail::selectable_surface::State {
		bool pointer_focused = false;
	};
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<QueueRow>;
	using InteractionContext = ElementInteractionContext<QueueRow>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.queue-row");
	static constexpr std::string_view debugName = "Changes Review Queue Row";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context) {
		context.state().pointer_focused = true;
		const auto disclosure =
			context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
				context.id, diagnostics_ui::Control::definitionId,
				LocalElementName{"disclosure"}.token));
		FSEL::detail::selectable_surface::onPressed(
			context, context.params.changes != nullptr &&
						 !context.previousInteraction.isPressed(disclosure));
	}
	static void onReleased(InteractionContext& context) {
		if (FSEL::detail::selectable_surface::onReleased(context,
														 context.params.changes != nullptr) &&
			!context.uiManager.getPreviousFramesInteraction().isHovered(
				context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
					context.id, diagnostics_ui::Control::definitionId,
					LocalElementName{"disclosure"}.token)))) {
			context.uiManager.shortcuts().setFocusedElement(context.clayID());
			toggle_review(context.params.workbench, 0, context.params.identity);
		}
	}
	static void runLogic(InteractionContext& context) {
		FSEL::detail::selectable_surface::runLogic(context, context.params.changes != nullptr);
		const auto& current = context.uiManager.getCurrentFrameInput();
		const auto& previous = context.uiManager.getPreviousFrameInput();
		if ((!context.state().isArmed &&
			 context.uiManager.shortcuts().focusedElement().id != context.clayID().id) ||
			(current.keyDown[GLFW_KEY_TAB] && !previous.keyDown[GLFW_KEY_TAB]))
			context.state().pointer_focused = false;
		if (context.uiManager.shortcuts().focusedElement().id != context.clayID().id ||
			context.uiManager.inputFields().hasPrimaryFieldFocus())
			return;
		if (current.keyDown[GLFW_KEY_ENTER] && !previous.keyDown[GLFW_KEY_ENTER]) {
			context.state().pointer_focused = false;
			toggle_review(context.params.workbench, 0, context.params.identity);
		}
		const bool down = current.keyDown[GLFW_KEY_DOWN] && !previous.keyDown[GLFW_KEY_DOWN];
		const bool up = current.keyDown[GLFW_KEY_UP] && !previous.keyDown[GLFW_KEY_UP];
		if ((!down && !up) || !context.params.changes || !context.params.workbench ||
			context.params.workbench->queue_navigation_handled)
			return;
		context.params.workbench->queue_navigation_handled = true;
		auto& changes = *context.params.changes;
		const auto node =
			std::ranges::find(changes.nodes, context.params.identity, &DevChangesNode::identity);
		if (node == changes.nodes.end() || node->rows.empty())
			return;
		const auto rows = changes_queue_rows(changes);
		const auto current_row = std::ranges::find(rows, node->rows.front());
		if (current_row == rows.end())
			return;
		const auto position = static_cast<std::size_t>(current_row - rows.begin());
		const auto destination = down			? std::min(position + 1, rows.size() - 1)
								 : position > 0 ? position - 1
												: 0;
		if (destination == position)
			return;
		const auto& target_row = changes.review.rows[rows[destination]];
		const auto target_node =
			std::ranges::find(changes.nodes, target_row.key, &DevChangesNode::key);
		if (target_node == changes.nodes.end())
			return;
		const auto target = context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
			context.params.queue_parent, definitionId,
			Keyed("review-row", target_node->identity).token));
		context.uiManager.shortcuts().setFocusedElement(target);
		context.params.workbench->queue_focus_target = target;
		const auto page = static_cast<uint64_t>(destination / review_page_size);
		if (page != changes.workbench_page) {
			changes.workbench_page = page;
			changes.workbench_page_changed = true;
		}
	}
	static void onHovered(InteractionContext& context) {
		context.uiManager.requestCursor(CursorType::PointingHand, 4);
	}
	static void buildElement(BuildContext& context) {
		const auto& parameters = context.params;
		auto row = ui::column(4);
		row.layout.sizing.height = CLAY_SIZING_FIT(58);
		row.layout.padding = {12, 10, 10, 10};
		row.backgroundColor =
			parameters.selected ? interface_theme::kDepth3Elevated : interface_theme::kDepth1Panel;
		const bool focused =
			!context.state().pointer_focused &&
			context.uiManager.shortcuts().focusedElement().id == context.clayID().id;
		row.border = {.color = parameters.selected || focused ? interface_theme::kAccentCurrent
															  : interface_theme::kBorderPrimary,
					  .width = parameters.selected ? Clay_BorderWidth{3, 0, 0, 0, 0}
							   : focused		   ? Clay_BorderWidth{1, 1, 1, 1, 0}
												   : Clay_BorderWidth{0, 0, 0, 0, 0}};
		CLAY(context.clayID(), row) {
			const bool compact = parameters.width < 480;
			auto headline = compact ? ui::column(8) : ui::row(15);
			headline.layout.sizing.height = CLAY_SIZING_FIT(0);
			headline.layout.childGap = 8;
			CLAY(context.clayID("headline"), headline) {
				const auto title_bounds = Clay_GetElementData(context.clayID("title"));
				const float title_width =
					title_bounds.found
						? std::min(title_bounds.boundingBox.width, parameters.width - 24)
					: compact ? parameters.width - 24
							  : parameters.width - 176;
				auto title = ui::column(0);
				title.clip = {.horizontal = true, .scrollInputDisabled = true};
				CLAY(context.clayID("title"), title)
				ui::text(context, ui::ellipsize(context, parameters.title, title_width, 12),
						 interface_theme::kTextCanvas, 12);
				const auto draw_status = [&] {
					if (parameters.blocking)
						ui::text(context, "Issue", interface_theme::kStatusRed, 11);
					ui::text(context, parameters.excluded ? "Excluded" : parameters.operation,
							 parameters.excluded ? interface_theme::kTextSecondary
												 : parameters.operation_color,
							 11);
					if (compact) {
						auto spacer = ui::column(0);
						spacer.layout.sizing.height = CLAY_SIZING_FIXED(0);
						CLAY(context.clayID("status-spacer"), spacer) {}
					}
					context.uiManager.createElement(ui::kControl, LocalElementName{"disclosure"})
						.setParameters(
							{.label = parameters.selected ? "Collapse" : "Expand",
							 .tooltip = parameters.selected ? "Collapse change" : "Expand change",
							 .icon =
								 parameters.selected ? ui::Icon::ChevronUp : ui::Icon::ChevronDown,
							 .activate = toggle_review,
							 .target = parameters.workbench,
							 .value = parameters.identity,
							 .icon_only = true})
						.setDevInternalCapture(true)
						.draw();
				};
				if (compact) {
					auto status = ui::row(28);
					status.layout.sizing.height = CLAY_SIZING_FIT(0);
					CLAY(context.clayID("status"), status)
					draw_status();
				} else
					draw_status();
				const auto disclosure = ::FlowUi::detail::element_id::resolveLocal(
					context.id, diagnostics_ui::Control::definitionId,
					LocalElementName{"disclosure"}.token);
				ui::register_focus_target(context, *parameters.changes,
										  context.uiManager.toClayEID(disclosure));
			}
			ui::text(context, ui::ellipsize(context, parameters.preview, parameters.width - 32, 11),
					 interface_theme::kTextSecondary, 11);
			if (context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
				diagnostics_ui::pointer_tooltip(context, LocalElementName{"tooltip"},
												parameters.tooltip);
		}
		if (parameters.workbench && parameters.workbench->submitted_queue_count <
										parameters.workbench->submitted_queue_ids.size())
			parameters.workbench
				->submitted_queue_ids[parameters.workbench->submitted_queue_count++] =
				context.clayID().id;
		if (parameters.changes)
			ui::register_focus_target(context, *parameters.changes, context.clayID(), true, {},
									  nullptr, 0, parameters.viewport);
	}
};
inline constexpr QueueRow queue_row{};

void comparison(Context& context, const ReviewRow& row, uint64_t identity, float width) {
	const auto previous_bounds = Clay_GetElementData(context.clayID(Keyed("comparison", identity)));
	const float comparison_width =
		previous_bounds.found && previous_bounds.boundingBox.width > 0
			? std::min(previous_bounds.boundingBox.width, std::max(0.0f, width - 56))
			: std::max(0.0f, width - 56);
	const float stage_width = std::max(0.0f, (comparison_width - 16) / 3);
	auto cells = width < 440 ? ui::column(8) : ui::row();
	cells.layout.sizing.height = CLAY_SIZING_FIT(0);
	cells.layout.childAlignment.y = CLAY_ALIGN_Y_TOP;
	if (width >= 440)
		cells.layout.childGap = 8;
	CLAY(context.clayID(Keyed("comparison", identity)), cells) {
		static constexpr std::array<std::string_view, 3> headings{"Already Baked", "Proposal",
																  "Next bake"};
		for (uint64_t column_index = 0; column_index < headings.size(); ++column_index) {
			auto cell = ui::column(6);
			if (width >= 440)
				cell.layout.sizing.width = CLAY_SIZING_FIXED(stage_width);
			cell.layout.padding = {10, 10, 10, 10};
			cell.backgroundColor = column_index == 0   ? interface_theme::kDepth1Panel
								   : column_index == 1 ? interface_theme::kDepth2Ink
													   : interface_theme::kDepth3Elevated;
			cell.border = {.color = column_index == 2 ? ui::operation_color(row.operation)
													  : interface_theme::kBorderVisible,
						   .width = {1, 1, 2, 1, 0}};
			CLAY(context.clayID(Keyed("value", identity * 3 + column_index)), cell) {
				const bool present =
					column_index == 0 ? row.has_disk
					: column_index == 1
						? row.has_proposal
						: row.operation != Operation::Remove && !row.blocking &&
							  (row.has_disk ||
							   (row.has_proposal && row.decision == Decision::IncludeProposal));
				const auto raw = column_index == 0	 ? std::string_view(row.on_disk)
								 : column_index == 1 ? std::string_view(row.proposal)
													 : std::string_view(row.next_value);
				const auto display =
					column_index == 2 ? row.domain == tooling::DevChangesDomain::Memory &&
												!row.blocking && row.operation != Operation::Remove
											? capacity_value(row, raw, present)
											: next_value(row)
					: column_index == 1 && row.proposal_removes ? std::string("Remove assignment")
					: row.domain == tooling::DevChangesDomain::Memory
						? capacity_value(row, raw, present)
						: ui::value(raw, present);
				ui::text(context, headings[column_index], interface_theme::kTextSecondary, 11);
				const auto expanded_key = "value:" + row.key + ":" + std::to_string(column_index);
				const bool expanded =
					std::ranges::find(context.state().metadata_expanded, expanded_key) !=
					context.state().metadata_expanded.end();
				auto compact_display = display;
				std::ranges::replace(compact_display, '\n', ' ');
				std::ranges::replace(compact_display, '\r', ' ');
				const auto collapsed_display =
					ui::ellipsize(context, compact_display,
								  width < 440 ? width - 80 : std::max(0.0f, stage_width - 20), 12);
				const bool expandable = present && collapsed_display != display;
				ui::text(context, expandable && !expanded ? collapsed_display : display,
						 interface_theme::kTextCanvas, 12);
				const auto disclosure_icon = expanded ? ui::Icon::ChevronUp : ui::Icon::ChevronDown;
				const bool action_icons_available =
					context.resources().texture(ui::Icon::Copy).handle &&
					(!expandable || context.resources().texture(disclosure_icon).handle);
				auto actions = action_icons_available ? ui::row(28) : ui::column(6);
				actions.layout.sizing.height = CLAY_SIZING_FIT(0);
				CLAY(context.clayID(Keyed("value-actions", column_index)), actions) {
					context.uiManager.createElement(ui::kControl, Keyed("copy-value", column_index))
						.setParameters(
							{.label = "Copy",
							 .tooltip = "Copy exact " + std::string(headings[column_index]),
							 .icon = ui::Icon::Copy,
							 .activate = copy_value,
							 .target = &context.state(),
							 .action = identity,
							 .value = column_index,
							 .enabled = present && context.uiManager.hasClipboardAccess(),
							 .icon_only = true})
						.setDevInternalCapture(true)
						.draw();
					register_widget(context, context.params.interfaceState->changes,
									diagnostics_ui::Control::definitionId,
									Keyed("copy-value", column_index),
									present && context.uiManager.hasClipboardAccess());
					if (expandable)
						context.uiManager
							.createElement(ui::kControl, Keyed("expand-value", column_index))
							.setParameters(
								{.label = expanded ? "Collapse" : "Expand",
								 .tooltip = expanded ? "Collapse full value" : "Expand full value",
								 .icon = disclosure_icon,
								 .activate = toggle_value,
								 .target = &context.state(),
								 .action = identity,
								 .value = column_index,
								 .icon_only = true})
							.setDevInternalCapture(true)
							.draw();
					if (expandable)
						register_widget(context, context.params.interfaceState->changes,
										diagnostics_ui::Control::definitionId,
										Keyed("expand-value", column_index));
				}
			}
		}
	}
}
void decision_control(Context& context, DevChangesState& state, const ReviewRow& row, float width) {
	auto& local = context.state();
	local.decision_target_key = row.key;
	const bool valid_proposal = row.has_proposal;
	local.decision_labels = {
		!row.has_proposal	   ? "Already Baked retained — no proposal"
		: row.proposal_removes ? "Use proposed removal"
							   : "Use proposal",
		row.has_proposal ? row.has_disk ? "Exclude proposal — already baked stays"
										: "Exclude proposal — stays absent"
						 : "Exclude proposal — no proposal",
		row.has_disk ? "Keep already baked value"
					 : "Keep already baked — no already baked assignment",
		row.has_disk ? row.key == "memory:policy.enabled" ? "Remove entire memory section"
														  : "Remove already baked assignment"
					 : "Remove assignment — nothing already baked",
		""};
	for (std::size_t option_index = 0; option_index < 4; ++option_index)
		local.decision_options[option_index] = {.value = option_index,
												.text = local.decision_labels[option_index],
												.enabled = option_index < 2 ? row.has_proposal
																			: row.has_disk};
	const bool explicit_resolution =
		row.blocking && row.validity == tooling::DevChangesValidity::Conflict;
	local.decision_choice = explicit_resolution ? 4 : static_cast<uint64_t>(row.decision);
	local.decision_labels[4] =
		local.decision_labels[static_cast<std::size_t>(row.decision)] + " (resolve conflict)";
	local.decision_options[4] = {.value = 4, .text = local.decision_labels[4], .enabled = false};
	const auto trigger = context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
		context.id, FSEL::ComboBox::definitionId, LocalElementName{"next-bake-decision"}.token));
	const auto option_id = [&](uint64_t value) {
		const auto combo = ::FlowUi::detail::element_id::resolveLocal(
			context.id, FSEL::ComboBox::definitionId, LocalElementName{"next-bake-decision"}.token);
		return context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
			combo, FSEL::ComboBox::definitionId, Keyed("option", value).token));
	};
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto pressed = [&](int key) { return current.keyDown[key] && !previous.keyDown[key]; };
	const auto initial_highlight = [&]() {
		const auto active = static_cast<uint64_t>(row.decision);
		if (active < 4 && local.decision_options[active].enabled)
			return active;
		for (uint64_t index = 0; index < 4; ++index)
			if (local.decision_options[index].enabled)
				return index;
		return UINT64_MAX;
	};
	const auto focus_highlight = [&]() {
		context.uiManager.shortcuts().setFocusedElement(
			local.decision_highlight < 4 ? option_id(local.decision_highlight) : trigger);
	};
	if (context.uiManager.getPreviousFramesInteraction().isPressed(trigger))
		context.uiManager.shortcuts().setFocusedElement(trigger);
	const bool open_before_keyboard = local.decision_open;
	if (local.decision_open && !local.decision_was_open) {
		local.decision_highlight = initial_highlight();
		focus_highlight();
	}
	auto focused = context.uiManager.shortcuts().focusedElement().id;
	bool owns_focus = focused == trigger.id;
	for (uint64_t index = 0; index < 4; ++index)
		if (focused == option_id(index).id) {
			local.decision_highlight = index;
			owns_focus = true;
		}
	if (!context.uiManager.inputFields().hasPrimaryFieldFocus()) {
		if (!local.decision_open && focused == trigger.id &&
			(pressed(GLFW_KEY_ENTER) || pressed(GLFW_KEY_SPACE))) {
			local.decision_open = true;
			local.decision_highlight = initial_highlight();
			focus_highlight();
		} else if (local.decision_open && owns_focus && pressed(GLFW_KEY_ESCAPE)) {
			local.decision_open = false;
			context.uiManager.shortcuts().setFocusedElement(trigger);
		} else if (local.decision_open && owns_focus &&
				   (pressed(GLFW_KEY_DOWN) || pressed(GLFW_KEY_UP))) {
			const bool down = pressed(GLFW_KEY_DOWN);
			uint64_t destination = local.decision_highlight < 4 ? local.decision_highlight
								   : down						? 3
																: 0;
			for (uint64_t attempt = 0; attempt < 4; ++attempt) {
				destination = down ? (destination + 1) % 4 : (destination + 3) % 4;
				if (local.decision_options[destination].enabled) {
					local.decision_highlight = destination;
					focus_highlight();
					break;
				}
			}
		} else if (local.decision_open && owns_focus && local.decision_highlight < 4 &&
				   local.decision_options[local.decision_highlight].enabled &&
				   (pressed(GLFW_KEY_ENTER) || pressed(GLFW_KEY_SPACE))) {
			queue_changes_scope_decision(state, row.key,
										 static_cast<Decision>(local.decision_highlight));
			local.decision_open = false;
			context.uiManager.shortcuts().setFocusedElement(trigger);
		}
	}
	if (local.decision_open && local.decision_highlight < 4) {
		local.decision_labels[local.decision_highlight] =
			"> " + local.decision_labels[local.decision_highlight];
		local.decision_options[local.decision_highlight].text =
			local.decision_labels[local.decision_highlight];
	}
	ui::text(context, "For next bake", interface_theme::kTextCanvas, 12);
	CLAY(context.clayID("decision-and-edit"), action_row(width)) {
		FSEL::ComboBoxParameters parameters{};
		parameters.options = std::span<const FSEL::ComboBoxOption>(local.decision_options)
								 .first(explicit_resolution ? 5 : 4);
		parameters.selectedValue = &local.decision_choice;
		parameters.open = &local.decision_open;
		parameters.fontSize = 12;
		parameters.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(32)};
		parameters.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
		parameters.popupMaxHeight = 240;
		parameters.optionHeight = 32;
		if (context.params.app)
			parameters.onChanged = ActionCall{
				context.params.app->actions().uiActions().make(change_decision, state, local)};
		auto focus_outline = ui::column(0);
		focus_outline.border = {.color =
									context.uiManager.shortcuts().focusedElement().id == trigger.id
										? interface_theme::kAccentCurrent
										: Clay_Color{},
								.width = {1, 1, 1, 1, 0}};
		CLAY(context.clayID("decision-focus"), focus_outline) {
			context.uiManager.createElement(FSEL::kComboBox, LocalElementName{"next-bake-decision"})
				.setParameters(parameters)
				.setDevInternalCapture(true)
				.draw();
		}
		if (!open_before_keyboard && local.decision_open) {
			local.decision_highlight = initial_highlight();
			focus_highlight();
		}
		if (local.decision_was_open && !local.decision_open) {
			focused = context.uiManager.shortcuts().focusedElement().id;
			bool option_focused = focused == trigger.id;
			for (uint64_t index = 0; index < 4; ++index)
				option_focused = option_focused || focused == option_id(index).id;
			if (option_focused)
				context.uiManager.shortcuts().setFocusedElement(trigger);
		}
		local.decision_was_open = local.decision_open;
		ui::register_focus_target(context, state, trigger);
		if (local.decision_open)
			for (uint64_t index = 0; index < 4; ++index)
				ui::register_focus_target(context, state, option_id(index),
										  local.decision_options[index].enabled, {}, nullptr, 0,
										  context.clayID());
		const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
		const bool policy = row.key.starts_with("memory:policy.");
		const bool can_edit = row.domain == tooling::DevChangesDomain::Memory ||
							  (entry && row.validity != tooling::DevChangesValidity::Unresolved);
		if (selected_changes_row(state))
			ui::button(context, LocalElementName{"edit"},
					   policy											 ? "Edit startup policy"
					   : row.domain == tooling::DevChangesDomain::Memory ? "Edit in Memory"
																		 : "Edit in Inspect",
					   state, policy ? ui::OpenPolicy : ui::Edit, 0, can_edit,
					   can_edit
						   ? "Open compatible authoring tools; the review and filters are retained."
						   : "This already baked target is unresolved. Author a compatible "
							 "replacement and "
							 "explicitly remove the already baked assignment.");
	}
	ui::text(context, decision_consequence(row), interface_theme::kTextSecondary, 12);
	if (row.blocking && valid_proposal)
		ui::text(context,
				 "Use proposal queues this proposal for validation; it cannot bypass the blocker.",
				 interface_theme::kTextCanvas, 12);
	if (row.key == "memory:policy.enabled") {
		const auto capacities = std::ranges::count_if(state.review.rows, [](const auto& candidate) {
			return candidate.domain == tooling::DevChangesDomain::Memory && candidate.has_disk &&
				   !candidate.key.starts_with("memory:policy.");
		});
		const auto policies = std::ranges::count_if(state.review.rows, [](const auto& candidate) {
			return candidate.has_disk && candidate.key.starts_with("memory:policy.");
		});
		ui::text(context,
				 "Removal affects all " + std::to_string(capacities) +
					 " already baked capacity targets and " + std::to_string(policies) +
					 " shared policy settings.",
				 interface_theme::kStatusAmber, 12);
	}
}
void incomplete_evidence(Context& context, DevChangesState& state) {
	const auto affected = std::ranges::count_if(state.review.rows, [](const auto& row) {
		return row.proposed_memory && !row.proposed_memory->evidence.complete;
	});
	if (!affected)
		return;
	const auto hidden = std::ranges::count_if(state.review.rows, [&](const auto& row) {
		if (!row.proposed_memory || row.proposed_memory->evidence.complete)
			return false;
		const auto index = static_cast<std::size_t>(&row - state.review.rows.data());
		return std::ranges::find(state.matching_rows, index) == state.matching_rows.end();
	});
	ui::text(context,
			 "Incomplete evidence affects " + std::to_string(affected) +
				 " prepared memory proposals (" + std::to_string(hidden) + " hidden by filters).",
			 interface_theme::kStatusAmber, 12);
	std::string affected_targets;
	affected_targets.reserve(static_cast<std::size_t>(affected) * 80);
	std::size_t named = 0;
	for (const auto& candidate : state.review.rows) {
		if (!candidate.proposed_memory || candidate.proposed_memory->evidence.complete)
			continue;
		if (!affected_targets.empty())
			affected_targets += '\n';
		affected_targets +=
			candidate.target_name + " / " + candidate.field_path + " [" + candidate.key + "]";
		if (named < 6) {
			ui::text(context, candidate.target_name + " / " + candidate.field_path,
					 interface_theme::kTextSecondary, 11);
			++named;
		}
	}
	if (static_cast<std::size_t>(affected) > named)
		ui::text(context,
				 "Showing " + std::to_string(named) + " of " + std::to_string(affected) +
					 " affected targets; copy the complete set before deciding.",
				 interface_theme::kTextSecondary, 11);
	context.uiManager.createElement(ui::kControl, LocalElementName{"copy-affected-targets"})
		.setParameters({.label = "Copy all affected targets",
						.tooltip = "Copy all targets affected by incomplete prepared evidence",
						.clipboard_text = affected_targets,
						.icon = ui::Icon::Copy,
						.enabled = context.uiManager.hasClipboardAccess(),
						.icon_only = true})
		.setDevInternalCapture(true)
		.draw();
	register_widget(context, state, diagnostics_ui::Control::definitionId,
					LocalElementName{"copy-affected-targets"},
					context.uiManager.hasClipboardAccess());
	ui::button(context, LocalElementName{"acknowledge-memory"},
			   state.acknowledge_incomplete
				   ? "Revoke acknowledgement"
				   : "Acknowledge " + std::to_string(affected) + " prepared proposals",
			   state, ui::Acknowledge, 0, true,
			   "This is a session-wide choice for all prepared memory proposals, including hidden "
			   "targets.");
}
[[nodiscard]] std::string issue_impact(const ReviewRow& row) {
	switch (row.validity) {
	case tooling::DevChangesValidity::Unresolved:
		return "The baked target or field cannot be matched to the current application schema. "
			   "FlowUi cannot safely generate its assignment. Restore a compatible target or "
			   "explicitly remove this assignment.";
	case tooling::DevChangesValidity::Conflict:
		return "Multiple values or an external manifest edit disagree about this destination. "
			   "Choose the proposal, retain the reviewed baked value, or remove the assignment to "
			   "resolve the conflict.";
	case tooling::DevChangesValidity::Invalid:
		return "The value or its build metadata cannot produce a valid assignment. "
			   "Correct the proposal in the authoring tab or remove the invalid assignment.";
	case tooling::DevChangesValidity::Unavailable:
		return "This target or its evidence is unavailable in the current run. "
			   "Refresh after the target is registered and the required evidence is available.";
	default:
		return "Resolve the reported issue before baking. The reviewed values remain unchanged "
			   "until a decision is baked.";
	}
}
void expanded_review(Context& context, DevChangesState& state, const ReviewRow& row,
					 uint64_t identity, float width) {
	const bool dedicated = selected_changes_row(state) != nullptr;
	auto surface = ui::card(12);
	surface.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	surface.layout.padding = {14, 14, 14, 14};
	surface.backgroundColor = interface_theme::kDepth2Ink;
	surface.border.color = interface_theme::kBorderVisible;
	CLAY(context.clayID(Keyed("expanded-review", identity)), surface) {
		CLAY(context.clayID("change-context"), action_row(width)) {
			ui::text(context, scope_label(row), interface_theme::kTextSecondary, 12);
			if (!dedicated) {
				auto spacer = ui::column(0);
				spacer.layout.sizing = {.width = CLAY_SIZING_GROW(0),
										.height = CLAY_SIZING_FIXED(0)};
				if (width >= 400)
					CLAY(context.clayID("change-context-spacer"), spacer) {}
				ui::button(context, Keyed("go-to-change", identity), "Go to change", state,
						   ui::Select, identity, true,
						   "Select this change in the tree and open its dedicated view.",
						   ui::Icon::Link);
			}
		}
		if (row.blocking || row.validity != tooling::DevChangesValidity::Ready) {
			auto issue = ui::card(8);
			issue.backgroundColor = interface_theme::kDepth1Panel;
			issue.border = {.color = row.blocking ? interface_theme::kStatusRed
												  : interface_theme::kStatusAmber,
							.width = {3, 1, 1, 1, 0}};
			CLAY(context.clayID(Keyed("change-issue", identity)), issue) {
				ui::text(context,
						 std::string(row.blocking ? "Issue · Blocks Bake · " : "Warning · ") +
							 std::string(tooling::changes_validity_name(row.validity)),
						 row.blocking ? interface_theme::kStatusRed : interface_theme::kStatusAmber,
						 13);
				ui::text(context, row.reason.empty() ? "This assignment needs review." : row.reason,
						 interface_theme::kTextCanvas, 12);
				ui::text(context, "Affected field: " + row.target_name + " / " + row.field_path,
						 interface_theme::kTextSecondary, 12);
				ui::text(context, "Why it matters", interface_theme::kTextCanvas, 12);
				ui::text(context, issue_impact(row), interface_theme::kTextSecondary, 12);
			}
		}
		comparison(context, row, identity, width);
		if (row.domain == tooling::DevChangesDomain::Memory) {
			if (const auto& memory = row.proposed_memory ? row.proposed_memory : row.disk_memory) {
				const auto* definition =
					::FlowUi::detail::find_initial_capacity(memory_capacity_target_id(memory->key));
				const std::string units =
					definition && definition->unit == ::FlowUi::detail::InitialCapacityUnit::Bytes
						? "bytes"
						: "entries";
				ui::text(context,
						 "Future startup hint · " + units + " · " +
							 (definition ? std::string(definition->scope) : scope_label(row)),
						 interface_theme::kTextCanvas, 12);
				const auto& evidence = memory->evidence;
				const bool retained_demand = evidence.kind == "recording" ||
											 evidence.kind == "live" || evidence.kind == "maximum";
				ui::text(context,
						 retained_demand
							 ? "Recorded demand " + std::to_string(memory->observed_maximum) + " " +
								   units + " · headroom " + std::to_string(memory->headroom) + " " +
								   units
							 : "Retained demand unavailable · configured headroom " +
								   std::to_string(memory->headroom) + " " + units,
						 interface_theme::kTextSecondary, 12);
				if (!memory->evidence.complete)
					incomplete_evidence(context, state);
			}
			if (row.operation == Operation::Remove)
				ui::text(context,
						 "Use startup fallback; caller baseline unavailable. Running allocations "
						 "do not change.",
						 interface_theme::kStatusAmber, 12);
		} else if (row.operation == Operation::Remove)
			ui::text(context,
					 "The assignment becomes absent after bake. Source and remaining scope "
					 "assignments determine fallback after rebuild.",
					 interface_theme::kStatusAmber, 12);
		decision_control(context, state, row, width);
	}
}
void batch_menu(Context& context, DevChangesState& state, std::span<const std::size_t> matching) {
	if (!state.group_decisions_open)
		return;
	const auto scope = std::ranges::find(state.nodes, state.queue_scope_key, &DevChangesNode::key);
	const bool scoped =
		!state.queue_scope_key.empty() && scope != state.nodes.end() && !scope->leaf;
	if (!state.queue_scope_key.empty() && !scoped) {
		state.group_decisions_open = false;
		return;
	}
	const auto scope_name = scoped ? scope->label : std::string("Whole plan");
	std::array<std::size_t, 4> eligible{}, hidden{};
	const auto count = [&](std::size_t index) {
		const auto& row = state.review.rows[index];
		const std::array<bool, 4> applicable{row.has_proposal && !row.blocking, row.has_proposal,
											 row.has_disk, row.has_disk};
		for (std::size_t decision_index = 0; decision_index < applicable.size(); ++decision_index)
			if (applicable[decision_index]) {
				++eligible[decision_index];
				hidden[decision_index] += std::ranges::find(matching, index) == matching.end();
			}
	};
	if (scoped)
		for (const auto index : scope->rows)
			count(index);
	else
		for (std::size_t index = 0; index < state.review.rows.size(); ++index)
			count(index);
	const auto in_scope = [&](std::size_t index) {
		return !scoped || std::ranges::find(scope->rows, index) != scope->rows.end();
	};
	bool removes_memory_section = false;
	for (std::size_t index = 0; index < state.review.rows.size(); ++index)
		removes_memory_section =
			removes_memory_section || (in_scope(index) && state.review.rows[index].has_disk &&
									   state.review.rows[index].key == "memory:policy.enabled");
	std::size_t removed_capacities = 0, removed_policies = 0;
	if (removes_memory_section)
		for (std::size_t index = 0; index < state.review.rows.size(); ++index) {
			const auto& candidate = state.review.rows[index];
			if (!candidate.has_disk || candidate.domain != tooling::DevChangesDomain::Memory)
				continue;
			if (candidate.key.starts_with("memory:policy."))
				++removed_policies;
			else
				++removed_capacities;
			if (!in_scope(index)) {
				++eligible[3];
				hidden[3] += std::ranges::find(matching, index) == matching.end();
			}
		}
	static constexpr std::array<LocalElementName, 5> menu_controls{
		"batch-include", "batch-omit", "batch-keep", "batch-remove", "close-group-decisions"};
	auto& local = context.state();
	local.group_trigger_id = context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
		context.id, diagnostics_ui::Control::definitionId,
		LocalElementName{"group-decisions"}.token));
	std::array<Clay_ElementId, 5> menu_ids{};
	for (std::size_t index = 0; index < menu_ids.size(); ++index)
		menu_ids[index] = context.uiManager.toClayEID(::FlowUi::detail::element_id::resolveLocal(
			context.id, diagnostics_ui::Control::definitionId, menu_controls[index].token));
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const bool down = current.keyDown[GLFW_KEY_DOWN] && !previous.keyDown[GLFW_KEY_DOWN];
	const bool up = current.keyDown[GLFW_KEY_UP] && !previous.keyDown[GLFW_KEY_UP];
	if ((down || up) && !context.uiManager.inputFields().hasPrimaryFieldFocus()) {
		const auto focused = context.uiManager.shortcuts().focusedElement().id;
		const auto selected = std::ranges::find(menu_ids, focused, &Clay_ElementId::id);
		if (focused == local.group_trigger_id.id || selected != menu_ids.end()) {
			std::size_t destination = selected != menu_ids.end()
										  ? static_cast<std::size_t>(selected - menu_ids.begin())
									  : down ? 4
											 : 0;
			for (std::size_t attempt = 0; attempt < menu_ids.size(); ++attempt) {
				destination = down ? (destination + 1) % menu_ids.size()
								   : (destination + menu_ids.size() - 1) % menu_ids.size();
				if (destination == 4 || eligible[destination] > 0) {
					context.uiManager.shortcuts().setFocusedElement(menu_ids[destination]);
					break;
				}
			}
		}
	}
	CLAY(context.clayID("group-decisions-menu"), ui::card(8)) {
		ui::text(context, scope_name + " decisions", interface_theme::kTextCanvas, 14);
		static constexpr std::array<std::string_view, 4> labels{
			"Use eligible proposals", "Exclude proposals", "Keep already baked values",
			"Remove already baked assignments"};
		static constexpr std::array<LocalElementName, 4> identities{"batch-include", "batch-omit",
																	"batch-keep", "batch-remove"};
		for (std::size_t index = 0; index < labels.size(); ++index) {
			if (index == 3) {
				ui::text(context, "Removal area · already baked assignments only",
						 interface_theme::kStatusAmber, 12);
				if (removes_memory_section)
					ui::text(context,
							 "This includes the ENTIRE memory section: " +
								 std::to_string(removed_capacities) +
								 " already baked capacity targets and " +
								 std::to_string(removed_policies) + " shared policy settings." +
								 (scoped ? " This includes targets outside this group." : ""),
							 interface_theme::kTextCanvas, 12);
			}
			ui::text(context,
					 std::to_string(eligible[index]) + " eligible targets " +
						 (index == 3 && removes_memory_section && scoped
							  ? "across this group and the entire memory section"
							  : "in " + scope_name) +
						 " · " + std::to_string(hidden[index]) + " hidden by filters",
					 interface_theme::kTextSecondary, 12);
			context.uiManager.createElement(ui::kControl, identities[index])
				.setParameters({.label = index == 3 && removes_memory_section
											 ? scoped ? "Remove group and memory section"
													  : "Remove all already baked assignments"
											 : std::string(labels[index]),
								.activate = batch_decision,
								.target = &context.state(),
								.value = index,
								.enabled = eligible[index] > 0})
				.setDevInternalCapture(true)
				.draw();
			register_widget(context, state, diagnostics_ui::Control::definitionId,
							identities[index], eligible[index] > 0);
		}
		ui::text(context,
				 "Blocked proposals are skipped when using proposals. Every affected target "
				 "retains its own reversible decision.",
				 interface_theme::kTextSecondary, 11);
		ui::button(context, LocalElementName{"close-group-decisions"}, "Close decisions", state,
				   ui::CloseGroupDecisions);
	}
}
void review_queue(Context& context, DevChangesState& state, float width) {
	const auto rows = changes_queue_rows(state);
	const auto selected = selected_changes_row(state);
	const auto selected_node = selected_changes_node(state);
	if (selected && selected_node) {
		if (state.selected_outside_filter) {
			CLAY(context.clayID("pinned-selection"), section(10)) {
				context.state().submitted_pinned = true;
				ui::text(context, "Selected change outside current filters",
						 interface_theme::kStatusAmber, 13);
				CLAY(context.clayID("pinned-actions"), action_row(width)) {
					ui::button(context, LocalElementName{"reveal-selection"}, "Reveal in list",
							   state, ui::RevealSelection);
					ui::button(context, LocalElementName{"clear-selection"}, "Clear selection",
							   state, ui::ClearSelection);
				}
			}
		}
		auto header = section(8);
		header.layout.padding = {12, 12, 12, 12};
		header.backgroundColor = interface_theme::kDepth1Panel;
		header.border = {.color = ui::operation_color(selected->operation),
						 .width = {3, 0, 0, 0, 0}};
		CLAY(context.clayID("selected-change-header"), header) {
			ui::text(context, selected->target_name + " / " + selected->field_path,
					 interface_theme::kTextCanvas, 15);
			ui::text(context, ui::operation_label(selected->operation),
					 ui::operation_color(selected->operation), 12);
		}
		expanded_review(context, state, *selected, selected_node->identity, width);
		return;
	}
	const auto scope = std::ranges::find(state.nodes, state.queue_scope_key, &DevChangesNode::key);
	const bool scoped =
		!state.queue_scope_key.empty() && scope != state.nodes.end() && !scope->leaf;
	const bool scope_missing = !state.queue_scope_key.empty() && !scoped;
	CLAY(context.clayID("queue-scope"), section()) {
		if (scope_missing) {
			ui::text(context, "Review scope unavailable", interface_theme::kTextCanvas, 14);
			ui::text(context,
					 "Select another group or explicitly return to the whole-plan overview.",
					 interface_theme::kTextSecondary, 12);
			ui::button(context, LocalElementName{"unavailable-scope-overview"}, "Open overview",
					   state, ui::Overview);
		} else {
			ui::text(context,
					 std::to_string(rows.size()) + " matching targets" +
						 (scoped ? " in " + scope->label : " across the whole plan"),
					 interface_theme::kTextSecondary, 12);
			CLAY(context.clayID("queue-actions"), action_row(width)) {
				ui::button(context, LocalElementName{"group-decisions"},
						   scoped ? "Group decisions..." : "Whole-plan decisions...", state,
						   ui::GroupDecisions);
				if (scoped && scope->key.starts_with("group:Memory") &&
					(!selected || !selected->key.starts_with("memory:policy.")))
					ui::button(context, LocalElementName{"edit-policy"},
							   "Edit shared startup policy", state, ui::OpenPolicy);
			}
		}
	}
	batch_menu(context, state, rows);
	const auto page_count =
		std::max<std::size_t>(1, (rows.size() + review_page_size - 1) / review_page_size);
	state.workbench_page = std::min<uint64_t>(state.workbench_page, page_count - 1);
	const auto begin = std::min<std::size_t>(rows.size(), state.workbench_page * review_page_size);
	const auto end = std::min(rows.size(), begin + review_page_size);
	CLAY(context.clayID("review-queue"), section(12)) {
		for (std::size_t position = begin; position < end; ++position) {
			const auto& row = state.review.rows[rows[position]];
			const auto node = std::ranges::find(state.nodes, row.key, &DevChangesNode::key);
			if (node == state.nodes.end())
				continue;
			const auto preview =
				ui::value(row.on_disk, row.has_disk) + " -> " +
				(row.blocking ? std::string("Unresolved")
				 : row.operation == Operation::Remove
					 ? std::string("Absent")
					 : ui::value(row.next_value,
								 row.has_disk || (row.has_proposal &&
												  row.decision == Decision::IncludeProposal &&
												  !row.proposal_removes)));
			const bool expanded = std::ranges::find(context.state().expanded_reviews, row.key) !=
								  context.state().expanded_reviews.end();
			auto group = section(8);
			CLAY(context.clayID(Keyed("review-card", node->identity)), group) {
				context.uiManager.createElement(queue_row, Keyed("review-row", node->identity))
					.setParameters({.title = row.target_name + " / " + row.field_path,
									.preview = preview,
									.operation = std::string(ui::operation_label(row.operation)),
									.tooltip = row.target_name + "\n" + row.field_path + "\n" +
											   scope_label(row) +
											   (row.blocking ? "\n" + row.reason : std::string{}),
									.app = context.params.app,
									.changes = &state,
									.workbench = &context.state(),
									.queue_parent = context.id,
									.operation_color = ui::operation_color(row.operation),
									.viewport = context.clayID("scroll"),
									.identity = node->identity,
									.width = width - 32,
									.selected = expanded,
									.blocking = row.blocking,
									.excluded = row.decision == Decision::OmitProposal})
					.setDevInternalCapture(true)
					.draw();
				if (expanded) {
					auto inset = section(0);
					inset.layout.padding = {10, 10, 0, 0};
					CLAY(context.clayID(Keyed("review-detail-inset", node->identity)), inset)
					expanded_review(context, state, row, node->identity, width - 20);
				}
			}
		}
		if (rows.empty())
			ui::text(context,
					 state.review.rows.empty() ? "No already baked or proposed changes"
											   : "No targets match these filters.",
					 interface_theme::kTextCanvas, 14);
	}
	CLAY(context.clayID("pages"), action_row(width)) {
		ui::text(context,
				 rows.empty() ? "0 matching targets"
							  : std::to_string(begin + 1) + " - " + std::to_string(end) + " of " +
									std::to_string(rows.size()) + " matching targets",
				 interface_theme::kTextSecondary, 12);
		if (page_count > 1) {
			ui::button(context, LocalElementName{"previous-page"}, "Previous", state,
					   ui::PreviousPage, 0, state.workbench_page > 0);
			ui::button(context, LocalElementName{"next-page"}, "Next", state, ui::NextPage, 0,
					   state.workbench_page + 1 < page_count);
		}
	}
}
void copy_text(Context& context, LocalElementName identity, std::string_view value,
			   std::string_view label) {
	context.uiManager.createElement(ui::kControl, identity)
		.setParameters({.label = std::string(label),
						.tooltip = std::string(label),
						.clipboard_text = std::string(value),
						.icon = ui::Icon::Copy,
						.enabled = !value.empty() && context.uiManager.hasClipboardAccess(),
						.icon_only = true})
		.setDevInternalCapture(true)
		.draw();
	register_widget(context, context.params.interfaceState->changes,
					diagnostics_ui::Control::definitionId, identity,
					!value.empty() && context.uiManager.hasClipboardAccess());
}
void raw_manifest(Context& context, DevChangesState& state, float width) {
	context.state().raw_scroll_id = context.clayID("raw-scroll");
	CLAY(context.clayID("raw-actions"), action_row(width)) {
		ui::button(context, LocalElementName{"raw-manifest"},
				   state.raw_manifest_open ? "Hide raw manifest" : "Raw manifest", state,
				   ui::ToggleRawManifest);
		if (width >= 400) {
			auto spacer = ui::column(0);
			spacer.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(0)};
			CLAY(context.clayID("raw-actions-spacer"), spacer) {}
		}
		copy_text(context, "copy-manifest", state.review.candidate_json, "Copy full manifest");
	}
	if (!state.raw_manifest_open)
		return;
	std::vector<std::size_t> offsets;
	offsets.reserve(state.review.candidate_json.size() / 8192 + 2);
	offsets.emplace_back(0);
	while (offsets.back() < state.review.candidate_json.size()) {
		const auto begin = offsets.back();
		auto end = std::min(begin + 8192, state.review.candidate_json.size());
		if (end < state.review.candidate_json.size()) {
			const auto newline = state.review.candidate_json.rfind('\n', end);
			if (newline != std::string::npos && newline > begin)
				end = newline + 1;
			else
				while (end > begin &&
					   (static_cast<unsigned char>(state.review.candidate_json[end]) & 0xc0u) ==
						   0x80u)
					--end;
		}
		offsets.emplace_back(end);
	}
	const auto pages = std::max<std::size_t>(1, offsets.size() - 1);
	context.state().raw_page = std::min<uint64_t>(context.state().raw_page, pages - 1);
	if (pages > 1)
		CLAY(context.clayID("raw-pages"), action_row(width)) {
			context.uiManager.createElement(ui::kControl, LocalElementName{"previous-candidate"})
				.setParameters({.label = "Previous",
								.tooltip = "Previous raw manifest page",
								.icon = ui::Icon::Previous,
								.activate = raw_page_action,
								.target = &context.state(),
								.action = 0,
								.enabled = context.state().raw_page > 0,
								.icon_only = true})
				.setDevInternalCapture(true)
				.draw();
			register_widget(context, state, diagnostics_ui::Control::definitionId,
							LocalElementName{"previous-candidate"}, context.state().raw_page > 0);
			ui::text(context,
					 "Raw page " + std::to_string(context.state().raw_page + 1) + " / " +
						 std::to_string(pages),
					 interface_theme::kTextSecondary, 12);
			context.uiManager.createElement(ui::kControl, LocalElementName{"next-candidate"})
				.setParameters({.label = "Next",
								.tooltip = "Next raw manifest page",
								.icon = ui::Icon::Next,
								.activate = raw_page_action,
								.target = &context.state(),
								.action = 1,
								.enabled = context.state().raw_page + 1 < pages,
								.icon_only = true})
				.setDevInternalCapture(true)
				.draw();
			register_widget(context, state, diagnostics_ui::Control::definitionId,
							LocalElementName{"next-candidate"},
							context.state().raw_page + 1 < pages);
		}
	if (state.review.candidate_json.empty()) {
		ui::text(context, "Candidate unavailable. Resolve the active-file error.",
				 interface_theme::kStatusRed, 12);
		return;
	}
	const auto begin = offsets[context.state().raw_page];
	const auto end = offsets[context.state().raw_page + 1];
	ui::text(context,
			 "Bytes " + std::to_string(begin + 1) + " - " + std::to_string(end) + " of " +
				 std::to_string(state.review.candidate_json.size()) +
				 " · read-only exact serialization",
			 interface_theme::kTextSecondary, 11);
	auto viewport = section(0);
	viewport.layout.sizing.height = CLAY_SIZING_FIXED(360);
	const auto scroll = Clay_GetScrollContainerData(context.clayID("raw-scroll"));
	if (scroll.found && scroll.scrollPosition) {
		context.state().raw_scroll_x = scroll.scrollPosition->x;
		context.state().raw_scroll_y = scroll.scrollPosition->y;
	}
	viewport.clip = {.horizontal = true,
					 .vertical = true,
					 .childOffset = {context.state().raw_scroll_x, context.state().raw_scroll_y}};
	viewport.backgroundColor = interface_theme::kDepth0Keel;
	viewport.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
	CLAY(context.clayID("raw-scroll"), viewport) {
		ui::restore_viewport_scroll(context, LocalElementName{"raw-scroll"},
									context.state().raw_scroll_y);
		if (!scroll.found) {
			const auto opened = Clay_GetScrollContainerData(context.clayID("raw-scroll"));
			if (opened.found && opened.scrollPosition)
				opened.scrollPosition->x = context.state().raw_scroll_x;
		}
		auto code = ui::column(0);
		code.layout.sizing.width = CLAY_SIZING_FIT(0);
		code.layout.padding = {8, 8, 8, 8};
		CLAY(context.clayID("candidate"), code) {
			Clay_TextElementConfig configuration{};
			configuration.fontSize = 11;
			configuration.textColor = interface_theme::kTextCanvas;
			configuration.wrapMode = CLAY_TEXT_WRAP_NONE;
			if (context.params.app)
				context.params.app->fonts().visitDevFonts(
					&configuration, [](void* target, const FontManager::DevFontView& font) {
						if (font.familyName.find("Mono") == std::string_view::npos &&
							font.familyName.find("mono") == std::string_view::npos)
							return true;
						static_cast<Clay_TextElementConfig*>(target)->fontId =
							static_cast<uint16_t>(font.fontId);
						return false;
					});
			CLAY_TEXT(context.uiManager.toClayString(
						  std::string_view(state.review.candidate_json).substr(begin, end - begin)),
					  CLAY_TEXT_CONFIG(configuration));
		}
	}
}
void output(Context& context, DevChangesState& state, float width) {
	const auto& counts = state.review.counts;
	ui::text(context, state.stale ? "Last reviewed output · sources changed" : "Whole-plan output",
			 state.stale ? interface_theme::kStatusAmber : interface_theme::kTextCanvas, 15);
	ui::text(context,
			 std::string(state.review.can_commit && !state.stale ? "Will write "
																 : "Reviewed candidate: ") +
				 std::to_string(counts.additions) + " new, " + std::to_string(counts.updates) +
				 " updates and " + std::to_string(counts.removals) + " removals; " +
				 std::to_string(counts.retained) + " already baked assignments retained.",
			 interface_theme::kTextCanvas, 13);
	CLAY(context.clayID("output-destination"), action_row(width)) {
		auto destination_label = ui::column(4);
		destination_label.layout.sizing.width = CLAY_SIZING_GROW(0);
		CLAY(context.clayID("output-destination-label"), destination_label) {
			ui::text(context, "Manifest destination", interface_theme::kTextSecondary, 11);
			ui::text(context, state.review.manifest_path.filename().generic_string(),
					 interface_theme::kTextCanvas, 13);
		}
		copy_text(context, "copy-destination", state.review.manifest_path.generic_string(),
				  "Copy full path");
	}
	if (state.review.can_commit && !state.stale)
		ui::text(context, "Ready to write the reviewed output.", interface_theme::kAccentSeaGlass,
				 12);
	else if (counts.pending() == 0 && state.review.valid_disk &&
			 counts.errors + counts.conflicts == 0)
		ui::text(context, "No output changes.", interface_theme::kTextCanvas, 12);
	else {
		CLAY(context.clayID("output-readiness"), action_row(width)) {
			ui::text(context, "Resolve blocking issues or refresh before writing.",
					 interface_theme::kStatusRed, 12);
			if (width >= 400) {
				auto spacer = ui::column(0);
				spacer.layout.sizing = {.width = CLAY_SIZING_GROW(0),
										.height = CLAY_SIZING_FIXED(0)};
				CLAY(context.clayID("output-readiness-spacer"), spacer) {}
			}
			ui::button(context, LocalElementName{"output-next-issue"}, "Review next issue", state,
					   ui::NextIssue);
		}
	}
	ui::text(context,
			 state.review.rebuild_required
				 ? "This executable differs from the already baked manifest — rebuild required."
				 : "This executable matches the reviewed already baked manifest.",
			 state.review.rebuild_required ? interface_theme::kStatusAmber
										   : interface_theme::kTextSecondary,
			 12);
	ui::text(context,
			 "Bake writes the reviewed manifest. Rebuild the application to use it. Filters do not "
			 "narrow the write.",
			 interface_theme::kTextSecondary, 12);
	static constexpr std::array<std::string_view, 4> domains{
		"Elements", "Themes", "Memory capacities", "Shared memory policy"};
	for (std::size_t domain_index = 0; domain_index < domains.size(); ++domain_index) {
		std::array<std::size_t, 4> operations{};
		for (const auto& row : state.review.rows) {
			const std::size_t row_domain = row.domain == tooling::DevChangesDomain::Elements ? 0
										   : row.domain == tooling::DevChangesDomain::Themes ? 1
										   : row.key.starts_with("memory:policy.")			 ? 3
																							 : 2;
			if (row_domain != domain_index)
				continue;
			if (row.operation != Operation::Add && row.operation != Operation::Update &&
				row.operation != Operation::Remove && !row.has_disk &&
				!(row.has_proposal && row.decision == Decision::IncludeProposal &&
				  !row.proposal_removes))
				continue;
			++operations[row.operation == Operation::Add	  ? 0
						 : row.operation == Operation::Update ? 1
						 : row.operation == Operation::Remove ? 2
															  : 3];
		}
		auto group = section(6);
		group.layout.padding = {0, 0, 10, 10};
		group.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 0, 1, 0}};
		group.layout.layoutDirection = width < 720 ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT;
		group.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
		CLAY(context.clayID(Keyed("output-domain", domain_index)), group) {
			auto domain_label = ui::column(0);
			domain_label.layout.sizing.width = CLAY_SIZING_GROW(0);
			CLAY(context.clayID(Keyed("output-domain-label", domain_index)), domain_label)
			ui::text(context, domains[domain_index], interface_theme::kTextCanvas, 13);
			auto counts_row = width < 400 ? ui::column(6) : ui::row(28);
			counts_row.layout.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)};
			counts_row.layout.childGap = 14;
			CLAY(context.clayID(Keyed("output-domain-counts", domain_index)), counts_row) {
				ui::text(context, std::to_string(operations[0]) + " New",
						 ui::operation_color(Operation::Add), 12);
				ui::text(context, std::to_string(operations[1]) + " Update",
						 ui::operation_color(Operation::Update), 12);
				ui::text(context, std::to_string(operations[2]) + " Removal",
						 ui::operation_color(Operation::Remove), 12);
				ui::text(context, std::to_string(operations[3]) + " retained",
						 interface_theme::kTextSecondary, 12);
			}
		}
	}
	raw_manifest(context, state, width);
	ui::button(context, LocalElementName{"output-technical"},
			   state.output_technical_open ? "Hide output technical details"
										   : "Output technical details",
			   state, ui::ToggleOutputTechnical);
	if (state.output_technical_open)
		CLAY(context.clayID("output-technical-details"), section()) {
			ui::text(context, state.review.manifest_path.generic_string(),
					 interface_theme::kTextSecondary, 11);
			ui::text(context, "Reviewed disk revision: " + state.review.disk_revision,
					 interface_theme::kTextSecondary, 11);
			ui::text(context,
					 "Schema / proposal / prepared revisions: " +
						 std::to_string(state.review.schema_generation) + " / " +
						 std::to_string(state.review.proposal_revision) + " / " +
						 std::to_string(state.review.memory_revision),
					 interface_theme::kTextSecondary, 11);
		}
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
	ui::button(context, LocalElementName{"close-policy"}, "Return to Review", state,
			   ui::ClosePolicy);
	ui::text(context, "Shared startup policy", interface_theme::kTextCanvas, 16);
	ui::text(context,
			 "Drafts affect output only after Stage. These settings apply at future startup; "
			 "current allocations stay active.",
			 interface_theme::kTextSecondary, 12);
	const auto baked_policy = [&](std::string_view key) {
		const auto found = std::ranges::find(state.review.rows, key, &ReviewRow::key);
		return found == state.review.rows.end() ? std::string("Absent — use startup fallback")
												: ui::value(found->on_disk, found->has_disk);
	};
	CLAY(context.clayID("section-policy"), ui::card(8)) {
		ui::text(context, "Use already baked memory capacities at future startup",
				 interface_theme::kTextCanvas, 13);
		ui::text(context, "Already Baked: " + baked_policy("memory:policy.enabled"),
				 interface_theme::kTextSecondary, 12);
		CLAY(context.clayID("policy-enabled"), selector_choice_track()) {
			ui::choice(context, Keyed("enabled-choice", 0), "No", 0, state.policy_enabled_choice);
			ui::choice(context, Keyed("enabled-choice", 1), "Yes", 1, state.policy_enabled_choice);
		}
		ui::text(context, "No retains already baked assignments while disabling their use.",
				 interface_theme::kTextSecondary, 12);
		ui::button(context, LocalElementName{"stage-enabled"}, "Stage section state", state,
				   ui::StagePolicy, 0);
	}
	CLAY(context.clayID("growth-policy"), ui::card(8)) {
		ui::text(context, "Storage growth factor", interface_theme::kTextCanvas, 13);
		ui::text(context, "Already Baked: " + baked_policy("memory:policy.growth_factor"),
				 interface_theme::kTextSecondary, 12);
		CLAY(context.clayID("growth-intent"), selector_choice_track()) {
			ui::choice(context, Keyed("growth-intent", 0), "Keep already baked", 0,
					   context.state().growth_intent);
			ui::choice(context, Keyed("growth-intent", 1), "Set value", 1,
					   context.state().growth_intent);
			ui::choice(context, Keyed("growth-intent", 2), "Use fallback", 2,
					   context.state().growth_intent);
		}
		static constexpr FSEL::NumberInput<double, DefinitionID("flowui.dev.changes.policy.factor")>
			growth_input{};
		FSEL::NumberInputParameters<double> growth{};
		growth.value = &state.policy_growth_draft;
		growth.minimum = 1.001;
		growth.maximum = 64;
		growth.step = .05;
		growth.fontSize = 12;
		growth.centerText = true;
		growth.padding = Clay_Padding{8, 8, 4, 4};
		growth.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
		context.uiManager.createElement(growth_input, LocalElementName{"policy-growth-input"})
			.setParameters(growth)
			.setDevInternalCapture(true)
			.draw();
		const auto identity = ::FlowUi::detail::element_id::resolveLocal(
			context.id, decltype(growth_input)::definitionId,
			LocalElementName{"policy-growth-input"}.token);
		ui::register_focus_target(context, state, context.uiManager.toClayEID(identity), true,
								  identity);
		const auto* input = context.uiManager.elements().getStatePointerConst(
			growth_input, context.uiManager.windowId(), identity);
		state.policy_growth_valid =
			(!input || input->textStatus == FSEL::NumericTextStatus::Complete) &&
			std::isfinite(state.policy_growth_draft) && state.policy_growth_draft > 1 &&
			state.policy_growth_draft <= 64;
		if (!state.policy_growth_valid)
			ui::text(
				context,
				"Complete a finite factor greater than one before staging. Your draft is retained.",
				interface_theme::kStatusAmber, 12);
		ui::button(context, LocalElementName{"stage-growth"},
				   context.state().growth_intent == 2 ? "Stage startup fallback" : "Stage factor",
				   state, ui::StagePolicy, context.state().growth_intent == 2 ? 3 : 1,
				   context.state().growth_intent != 0 &&
					   (context.state().growth_intent == 2 || state.policy_growth_valid));
	}
	CLAY(context.clayID("runtime-growth-policy"), ui::card(8)) {
		ui::text(context, "Permit storage runtime growth", interface_theme::kTextCanvas, 13);
		ui::text(context, "Already Baked: " + baked_policy("memory:policy.allow_runtime_growth"),
				 interface_theme::kTextSecondary, 12);
		CLAY(context.clayID("runtime-intent"), selector_choice_track()) {
			ui::choice(context, Keyed("runtime-intent", 0), "Keep already baked", 0,
					   context.state().runtime_intent);
			ui::choice(context, Keyed("runtime-intent", 1), "Set value", 1,
					   context.state().runtime_intent);
			ui::choice(context, Keyed("runtime-intent", 2), "Use fallback", 2,
					   context.state().runtime_intent);
		}
		CLAY(context.clayID("policy-runtime"), selector_choice_track()) {
			ui::choice(context, Keyed("runtime-choice", 0), "No", 0, state.policy_runtime_choice);
			ui::choice(context, Keyed("runtime-choice", 1), "Yes", 1, state.policy_runtime_choice);
		}
		ui::button(context, LocalElementName{"stage-runtime"},
				   context.state().runtime_intent == 2 ? "Stage startup fallback"
													   : "Stage permission",
				   state, ui::StagePolicy, context.state().runtime_intent == 2 ? 4 : 2,
				   context.state().runtime_intent != 0);
	}
	const auto capacities = std::ranges::count_if(state.review.rows, [](const auto& row) {
		return row.domain == tooling::DevChangesDomain::Memory && row.has_disk &&
			   !row.key.starts_with("memory:policy.");
	});
	const auto policies = std::ranges::count_if(state.review.rows, [](const auto& row) {
		return row.has_disk && row.key.starts_with("memory:policy.");
	});
	CLAY(context.clayID("policy-removal"), section()) {
		ui::text(context, "Removal area", interface_theme::kStatusAmber, 13);
		ui::text(context,
				 "Remove all " + std::to_string(capacities) +
					 " already baked capacity targets and " + std::to_string(policies) +
					 " shared policy settings. This is reversible before bake.",
				 interface_theme::kTextSecondary, 12);
		ui::button(context, LocalElementName{"clear-section"}, "Remove entire memory section",
				   state, ui::StagePolicy, 5, capacities + policies > 0);
	}
}
void feedback(Context& context, DevChangesState& state, float width) {
	if (state.message.empty() && state.undo.empty() && state.redo.empty())
		return;
	auto panel = section(6);
	panel.layout.padding = {14, 14, 8, 8};
	panel.backgroundColor = interface_theme::kDepth1Panel;
	panel.border = {.color = state.message_is_error ? interface_theme::kStatusRed
													: interface_theme::kBorderPrimary,
					.width = state.message_is_error ? Clay_BorderWidth{3, 0, 1, 0, 0}
													: Clay_BorderWidth{0, 0, 1, 0, 0}};
	CLAY(context.clayID("feedback"), panel) {
		if (!state.message.empty())
			ui::text(context, state.message, interface_theme::kTextCanvas, 12);
		CLAY(context.clayID("decision-history"), action_row(width)) {
			ui::button(context, LocalElementName{"undo"}, "Undo decision", state, ui::Undo, 0,
					   !state.undo.empty());
			ui::button(context, LocalElementName{"redo"}, "Redo decision", state, ui::Redo, 0,
					   !state.redo.empty());
			if (!state.message.empty())
				ui::button(context, LocalElementName{"dismiss-feedback"}, "Dismiss", state,
						   ui::DismissMessage);
		}
	}
}
void blocking_causes(Context& context, const DevChangesState& state) {
	std::array<std::size_t, 5> counts{};
	for (const auto& row : state.review.rows) {
		if (!row.blocking)
			continue;
		const bool memory_evidence = row.domain == tooling::DevChangesDomain::Memory &&
									 (row.reason.find("evidence") != std::string::npos ||
									  row.reason.find("Evidence") != std::string::npos ||
									  row.reason.find("sampling interval") != std::string::npos ||
									  row.reason.find("capture range") != std::string::npos ||
									  row.reason.find("acknowledge") != std::string::npos);
		const std::size_t cause = row.validity == tooling::DevChangesValidity::Conflict		? 0
								  : row.validity == tooling::DevChangesValidity::Unresolved ? 1
								  : memory_evidence											? 2
								  : row.validity == tooling::DevChangesValidity::Invalid	? 3
																							: 4;
		++counts[cause];
	}
	static constexpr std::array<std::string_view, 5> labels{
		"Conflicts", "Unresolved targets", "Memory evidence", "Invalid values", "Other blockers"};
	std::string summary;
	summary.reserve(128);
	for (std::size_t cause = 0; cause < counts.size(); ++cause) {
		if (!counts[cause])
			continue;
		if (!summary.empty())
			summary += " | ";
		summary += std::string(labels[cause]) + " " + std::to_string(counts[cause]);
	}
	if (summary.empty())
		return;
	auto banner = section(4);
	banner.layout.padding.left = 8;
	banner.border = {.color = interface_theme::kStatusRed, .width = {2, 0, 0, 0, 0}};
	CLAY(context.clayID("blocking-causes"), banner)
	ui::text(context, summary, interface_theme::kTextCanvas, 11);
}
} // namespace

void DevChangesWorkbench::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->changes;
	context.state().ui_manager = &context.uiManager;
	context.state().changes = &state;
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const auto previous = Clay_GetElementData(context.clayID());
	const float width = parent.found	 ? parent.boundingBox.width
						: previous.found ? previous.boundingBox.width
										 : 600;
	auto& local = context.state();
	local.queue_navigation_handled = false;
	if (state.view != 0)
		local.queue_focus_target = {};
	if (state.view != 0 || local.previous_selection != state.selected_key) {
		local.decision_open = local.decision_was_open = false;
		local.decision_highlight = UINT64_MAX;
	}
	const auto scroll = Clay_GetScrollContainerData(context.clayID("scroll"));
	if (local.previous_view != state.view) {
		if (local.previous_view < local.view_scroll.size())
			local.view_scroll[local.previous_view] = scroll.found && scroll.scrollPosition
														 ? scroll.scrollPosition->y
														 : state.workbench_scroll;
		if (state.view < local.view_scroll.size()) {
			if (local.previous_view == UINT64_MAX)
				local.view_scroll[state.view] = state.workbench_scroll;
			state.workbench_scroll = local.view_scroll[state.view];
			if (scroll.found && scroll.scrollPosition)
				scroll.scrollPosition->y = state.workbench_scroll;
		}
		local.previous_view = state.view;
	}
	if (local.previous_selection != state.selected_key ||
		local.previous_selection_revision != state.selection_revision ||
		(state.selected_outside_filter && !local.previous_pin)) {
		local.previous_selection = state.selected_key;
		local.previous_selection_revision = state.selection_revision;
		local.reveal_pending = selected_changes_row(state) != nullptr;
	}
	local.previous_pin = state.selected_outside_filter;
	if (state.workbench_page_changed) {
		if (scroll.found && scroll.scrollPosition)
			scroll.scrollPosition->y = 0;
		state.workbench_scroll = 0;
		state.workbench_page_changed = false;
	}
	if (local.reveal_pending && state.view == 0) {
		if (scroll.found && scroll.scrollPosition)
			scroll.scrollPosition->y = 0;
		state.workbench_scroll = 0;
		local.reveal_pending = false;
	}
	if (local.queue_focus_target.id && state.view == 0 && scroll.found && scroll.scrollPosition) {
		const auto bounds = Clay_GetElementData(local.queue_focus_target);
		const auto viewport_bounds = Clay_GetElementData(context.clayID("scroll"));
		const auto submitted_rows =
			std::span<const uint32_t>(local.submitted_queue_ids).first(local.submitted_queue_count);
		const bool submitted =
			std::ranges::find(submitted_rows, local.queue_focus_target.id) != submitted_rows.end();
		if (submitted && bounds.found && viewport_bounds.found) {
			if (bounds.boundingBox.y < viewport_bounds.boundingBox.y)
				scroll.scrollPosition->y += viewport_bounds.boundingBox.y - bounds.boundingBox.y;
			else if (bounds.boundingBox.y + bounds.boundingBox.height >
					 viewport_bounds.boundingBox.y + viewport_bounds.boundingBox.height)
				scroll.scrollPosition->y -= bounds.boundingBox.y + bounds.boundingBox.height -
											viewport_bounds.boundingBox.y -
											viewport_bounds.boundingBox.height;
			state.workbench_scroll = scroll.scrollPosition->y;
			local.queue_focus_target = {};
		}
	}
	local.submitted_queue_count = 0;
	local.submitted_pinned = false;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous_input = context.uiManager.getPreviousFrameInput();
	if (state.group_decisions_open && input.keyDown[GLFW_KEY_ESCAPE] &&
		!previous_input.keyDown[GLFW_KEY_ESCAPE]) {
		state.group_decisions_open = false;
		const auto trigger = ::FlowUi::detail::element_id::resolveLocal(
			context.id, diagnostics_ui::Control::definitionId,
			LocalElementName{"group-decisions"}.token);
		context.uiManager.shortcuts().setFocusedElement(context.uiManager.toClayEID(trigger));
	}
	const auto blocking = std::ranges::count_if(
		state.review.rows, [](const auto& review_row) { return review_row.blocking; });
	auto root = ui::column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), root) {
		auto heading = section(8);
		heading.layout.padding = {14, 14, 12, 10};
		heading.backgroundColor = interface_theme::kDepth1Panel;
		if (!state.review.valid_disk || !state.memory_error.empty())
			heading.border = {.color = interface_theme::kStatusRed, .width = {3, 0, 0, 1, 0}};
		CLAY(context.clayID("heading"), heading) {
			ui::text(context,
					 state.view == 2   ? "Startup policy drafts"
					 : state.view == 1 ? "Review output"
									   : "Review changes",
					 interface_theme::kTextCanvas, 16);
			CLAY(context.clayID("views"), selector_choice_track()) {
				ui::choice(context, Keyed("view", 0), "Review", 0, state.view);
				ui::choice(context, Keyed("view", 1), "Output", 1, state.view);
			}
			ui::text(context,
					 std::to_string(state.review.counts.pending()) +
						 " whole-plan output changes · " + std::to_string(blocking) +
						 " blocking issues",
					 interface_theme::kTextSecondary, 12);
		}
		auto viewport = ui::viewport(context, "scroll", state.workbench_scroll);
		viewport.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		CLAY(context.clayID("scroll"), viewport) {
			ui::FocusViewportScope focus_scope{state, context.clayID("scroll")};
			ui::restore_viewport_scroll(context, LocalElementName{"scroll"},
										state.workbench_scroll);
			auto body = section(12);
			body.layout.padding = {14, 14, 12, 16};
			CLAY(context.clayID("contents"), body) {
				blocking_causes(context, state);
				if (state.stale)
					ui::text(context,
							 "Sources changed. Refresh before baking. Values below are the last "
							 "reviewed snapshot.",
							 interface_theme::kStatusAmber, 12);
				if (!state.review.valid_disk) {
					ui::text(context,
							 state.review.error.empty() ? "The active manifest could not be read."
														: state.review.error,
							 interface_theme::kTextCanvas, 12);
					ui::button(context, LocalElementName{"retry-refresh"}, "Retry refresh", state,
							   ui::Refresh);
				}
				if (!state.memory_error.empty())
					ui::text(context, state.memory_error, interface_theme::kTextCanvas, 12);
				if (blocking || !state.memory_error.empty())
					CLAY(context.clayID("issue-navigation"), action_row(width)) {
						ui::button(context, LocalElementName{"next-issue"}, "Next issue", state,
								   ui::NextIssue);
						ui::button(context, LocalElementName{"show-issues"}, "Show issues", state,
								   ui::Issues);
					}
				if (state.view == 2)
					policy_editor(context, state);
				else if (state.view == 1)
					output(context, state, width);
				else
					review_queue(context, state, width);
			}
		}
		feedback(context, state, width);
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
