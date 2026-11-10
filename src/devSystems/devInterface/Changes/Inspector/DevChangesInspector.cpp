#include "devSystems/devInterface/Changes/Inspector/DevChangesInspector.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityTargets.hpp"
#include <array>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
using Context = DevChangesInspector::BuildContext;
using LocalState = DevChangesInspector::State;
enum LocalAction : uint64_t {
	ExpandDetail = 1,
	ToggleEvidence,
	ToggleApply,
	ToggleReset,
	Runtime = 10
};

void activate_local(void* target, uint64_t action, uint64_t value) {
	auto& local = *static_cast<LocalState*>(target);
	if (action == ExpandDetail) {
		const auto found = std::ranges::find(local.expanded_details, value);
		if (found == local.expanded_details.end())
			local.expanded_details.emplace_back(value);
		else
			local.expanded_details.erase(found);
	}
	if (action == ToggleEvidence)
		local.evidence_details_open = !local.evidence_details_open;
	if (action == ToggleApply) {
		local.live_menu_open = !local.live_menu_open;
		local.reset_menu_open = false;
	}
	if (action == ToggleReset) {
		local.reset_menu_open = !local.reset_menu_open;
		local.live_menu_open = false;
	}
	static constexpr std::array<uint64_t, 6> runtime_actions{ui::ApplyProposal, ui::ApplySaved,
															 ui::DiscardLive,	ui::ResetAuthored,
															 ui::UndoLive,		ui::RedoLive};
	if (action >= Runtime && action < Runtime + runtime_actions.size() && local.changes) {
		ui::activate(local.changes, runtime_actions[action - Runtime], 0);
		local.restore_apply_focus = action < Runtime + 2;
		local.restore_reset_focus = action >= Runtime + 2 && action < Runtime + 4;
		local.live_menu_open = false;
		local.reset_menu_open = false;
	}
}
template <class Identity>
void local_button(Context& context, const Identity& identity, std::string label, uint64_t action,
				  uint64_t value = 0, bool enabled = true, std::string description = {},
				  ui::Icon icon = ui::Icon::None) {
	diagnostics_ui::button_with_identity(context, identity, std::move(label), &context.state(),
										 activate_local, action, value, enabled,
										 std::move(description), icon, true);
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, diagnostics_ui::Control::definitionId, identity.token);
	ui::register_focus_target(context, context.uiManager.toClayEID(element), enabled);
}
[[nodiscard]] float assigned_width(Context& context) noexcept {
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const auto panel = Clay_GetElementData(context.clayID());
	return parent.found	 ? parent.boundingBox.width
		   : panel.found ? panel.boundingBox.width
						 : context.params.interfaceState->inspectorWidth;
}
[[nodiscard]] bool detail_expanded(const LocalState& local, uint64_t identity) noexcept {
	return std::ranges::find(local.expanded_details, identity) != local.expanded_details.end();
}
void copy_detail(Context& context, uint64_t identity, std::string_view field,
				 std::string_view value) {
	context.uiManager.createElement(ui::kControl, Keyed("copy-detail", identity))
		.setParameters({.label = "Copy",
						.tooltip = "Copy full " + std::string(field),
						.clipboard_text = std::string(value),
						.icon = ui::Icon::Copy,
						.enabled = !value.empty() && context.uiManager.hasClipboardAccess(),
						.icon_only = true})
		.setDevInternalCapture(true)
		.draw();
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, diagnostics_ui::Control::definitionId, Keyed("copy-detail", identity).token);
	ui::register_focus_target(context, context.uiManager.toClayEID(element),
							  !value.empty() && context.uiManager.hasClipboardAccess());
}
void inline_text(Context& context, std::string_view value, Clay_Color color, uint16_t size) {
	Clay_TextElementConfig configuration{};
	configuration.fontSize = size;
	configuration.textColor = color;
	configuration.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(configuration));
}
void notice(Context& context, LocalElementName identity, std::string_view description,
			Clay_Color marker) {
	auto surface = ui::column();
	surface.layout.padding = {8, 8, 8, 8};
	surface.backgroundColor = interface_theme::kDepth0Keel;
	surface.border = {.color = marker, .width = {2, 0, 0, 0, 0}};
	CLAY(context.clayID(identity), surface)
	ui::text(context, description, interface_theme::kTextCanvas, 12);
}
void detail(Context& context, std::string_view field, std::string_view value,
			std::string_view caption = {}) {
	const auto identity = ::FlowUi::detail::identity_hash::hashBytes(field);
	const auto panel = Clay_GetElementData(context.clayID());
	const float width =
		panel.found ? panel.boundingBox.width : context.params.interfaceState->inspectorWidth;
	const bool expanded = detail_expanded(context.state(), identity);
	const auto visible = value.empty() ? std::string("Unavailable") : std::string(value);
	const auto preview = ui::ellipsize(context, visible, width - 60, 12);
	const bool long_value = preview != visible || value.find('\n') != std::string_view::npos;
	const auto field_caption = caption.empty() ? field : caption;
	const bool one_line = !value.empty() && !long_value &&
						  ui::ellipsize(context, value, width - 216, 12) == value &&
						  ui::ellipsize(context, field_caption, 88, 11) == field_caption;
	if (one_line) {
		auto item = ui::row(28);
		item.layout.childGap = 4;
		CLAY(context.clayID(Keyed("detail", identity)), item) {
			auto caption_box = ui::row(28);
			caption_box.layout.sizing.width = CLAY_SIZING_FIXED(90);
			CLAY(context.clayID(Keyed("detail-caption", identity)), caption_box)
			inline_text(context, field_caption, interface_theme::kTextSecondary, 11);
			CLAY(context.clayID(Keyed("detail-value", identity)), ui::row(28))
			inline_text(context, value, interface_theme::kTextCanvas, 12);
			copy_detail(context, identity, field, value);
		}
		return;
	}
	auto item = ui::column(3);
	item.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	CLAY(context.clayID(Keyed("detail", identity)), item) {
		auto heading = ui::row(28);
		heading.layout.childGap = 4;
		CLAY(context.clayID(Keyed("detail-label", identity)), heading) {
			auto label = ui::column(0);
			label.layout.sizing.height = CLAY_SIZING_GROW(0);
			label.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
			CLAY(context.clayID(Keyed("detail-caption", identity)), label)
			ui::text(context, field_caption, interface_theme::kTextSecondary, 11);
			if (long_value)
				local_button(context, Keyed("expand-detail", identity), expanded ? "Less" : "Full",
							 ExpandDetail, identity, true,
							 expanded ? "Collapse full " + std::string(field)
									  : "Read full " + std::string(field),
							 expanded ? ui::Icon::Expanded : ui::Icon::Collapsed);
			copy_detail(context, identity, field, value);
		}
		ui::text(context, expanded ? visible : preview, interface_theme::kTextCanvas, 12);
	}
}
void section(Context& context, std::size_t index, std::string_view title, const auto& draw_body) {
	auto& changes = context.params.interfaceState->changes;
	context.uiManager.createElement(ui::kControl, Keyed("section", index))
		.setParameters(
			{.label = std::string(title),
			 .icon = changes.inspector_expanded[index] ? ui::Icon::Expanded : ui::Icon::Collapsed,
			 .activate = ui::activate,
			 .target = &changes,
			 .action = ui::Section,
			 .value = index,
			 .section_header = true})
		.setDevInternalCapture(true)
		.draw();
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, diagnostics_ui::Control::definitionId, Keyed("section", index).token);
	ui::register_focus_target(context, context.uiManager.toClayEID(element));
	if (!changes.inspector_expanded[index])
		return;
	auto body = ui::column(8);
	body.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	body.layout.padding = {12, 12, 10, 12};
	CLAY(context.clayID(Keyed("section-body", index)), body)
	draw_body();
}
[[nodiscard]] std::string_view domain_label(tooling::DevChangesDomain domain) noexcept {
	return domain == tooling::DevChangesDomain::Elements ? "Element"
		   : domain == tooling::DevChangesDomain::Themes ? "Theme"
														 : "Memory";
}
void scope(Context& context, const tooling::DevChangesReviewRow& row) {
	const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
	if (row.domain == tooling::DevChangesDomain::Memory) {
		if (row.key.starts_with("memory:policy.")) {
			ui::text(context, "Shared startup policy", interface_theme::kTextCanvas, 13);
			ui::text(context, "Applies to storage targets on future startup.",
					 interface_theme::kTextSecondary, 12);
			ui::text(context,
					 row.key == "memory:policy.enabled"
						 ? "Disabling retains already baked assignments. Removing this policy "
						   "removes the "
						   "entire memory section."
						 : "Removing an optional setting releases it to startup fallback.",
					 interface_theme::kTextSecondary, 12);
			ui::text(context, "Stage policy changes in the Workbench.",
					 interface_theme::kTextSecondary, 11);
			return;
		}
		const auto& memory = row.proposed_memory ? row.proposed_memory : row.disk_memory;
		const auto* definition =
			memory ? FlowUi::detail::find_initial_capacity(memory_capacity_target_id(memory->key))
				   : nullptr;
		ui::text(context, changes_memory_owner(row.field_path), interface_theme::kTextCanvas, 13);
		ui::text(context, "Initial capacity hint · future startup", interface_theme::kTextSecondary,
				 12);
		if (definition) {
			const std::string units = definition->unit == FlowUi::detail::InitialCapacityUnit::Bytes
										  ? "bytes"
										  : "entries";
			ui::text(context, std::string(definition->scope) + " · " + units,
					 interface_theme::kTextCanvas, 12);
			ui::text(context,
					 "Registered default: " + std::to_string(definition->default_value) + " " +
						 units,
					 interface_theme::kTextSecondary, 11);
		} else
			ui::text(context, "Registered units and startup scope unavailable.",
					 interface_theme::kTextSecondary, 11);
		if (row.operation == tooling::DevChangesOperation::Remove)
			ui::text(context, "Use startup fallback; caller baseline unavailable.",
					 interface_theme::kStatusAmber, 12);
		return;
	}
	if (!entry) {
		notice(context, LocalElementName{"unresolved-target"},
			   "Already baked target is unresolved.", interface_theme::kStatusRed);
		ui::text(context,
				 "Review replacement or explicit removal in the Workbench. The baked identity "
				 "remains available in Technical details.",
				 interface_theme::kTextSecondary, 12);
		return;
	}
	if (row.domain == tooling::DevChangesDomain::Themes) {
		ui::text(context,
				 entry->themeVariant == "*"	   ? "All theme variants"
				 : entry->themeVariant.empty() ? "Default theme variant"
											   : "Theme variant: " + entry->themeVariant,
				 interface_theme::kTextCanvas, 13);
		ui::text(context,
				 entry->themeVariant == "*"
					 ? "Includes future variants. Named variant assignments take precedence."
					 : "This named variant takes precedence over an all-variants assignment.",
				 interface_theme::kTextSecondary, 12);
	} else if (entry->targetScope == tooling::DevOverrideScope::Definition) {
		ui::text(context, "All instances of this definition", interface_theme::kTextCanvas, 13);
		ui::text(context, "Exact instance overrides take precedence.",
				 interface_theme::kTextSecondary, 12);
	} else {
		ui::text(context,
				 entry->instanceDebugLabel.empty() ? "Exact persisted instance"
												   : entry->instanceDebugLabel,
				 interface_theme::kTextCanvas, 13);
		ui::text(context,
				 "Definition assignments run first. Runtime windows are not part of this baked "
				 "identity.",
				 interface_theme::kTextSecondary, 12);
	}
	if (row.validity == tooling::DevChangesValidity::Unresolved)
		notice(context, LocalElementName{"unresolved-scope"},
			   "The already baked target is unresolved; review its cause in the Workbench.",
			   interface_theme::kStatusRed);
}
void source(Context& context, const tooling::DevBakeEntry& entry) {
	if (!entry.sourceHeader.empty())
		detail(context, "Source header", entry.sourceHeader);
	if (!entry.ownerCppType.empty())
		detail(context, "Owner type", entry.ownerCppType);
	if (!entry.valueTypeName.empty())
		detail(context, "Value type", entry.valueTypeName);
}
void evidence(Context& context, const tooling::DevChangesReviewRow& row) {
	auto& changes = context.params.interfaceState->changes;
	const auto& memory = row.proposed_memory ? row.proposed_memory : row.disk_memory;
	if (!memory)
		return;
	const auto& evidence = memory->evidence;
	ui::text(context, row.proposed_memory ? "Prepared evidence" : "Already baked evidence",
			 interface_theme::kTextSecondary, 11);
	ui::text(context,
			 evidence.source_name.empty() ? "Unnamed evidence dataset" : evidence.source_name,
			 interface_theme::kTextCanvas, 13);
	ui::text(context,
			 evidence.complete				   ? "Complete retained evidence"
			 : evidence.acknowledge_incomplete ? "Incomplete evidence · acknowledged"
											   : "Incomplete evidence",
			 evidence.complete ? interface_theme::kTextSecondary : interface_theme::kStatusAmber,
			 12);
	if (!evidence.complete)
		ui::text(context,
				 "Acknowledgement applies to all prepared memory proposals; review that scope in "
				 "the Workbench.",
				 interface_theme::kTextSecondary, 11);
	ui::text(context,
			 "Ticks [" + std::to_string(evidence.begin_tick) + ", " +
				 std::to_string(evidence.end_tick_exclusive) + ")",
			 interface_theme::kTextSecondary, 12);
	local_button(
		context, LocalElementName{"evidence-details"},
		context.state().evidence_details_open ? "Hide evidence details" : "Evidence details",
		ToggleEvidence, 0, true, "Read exact prepared evidence and the separate review snapshot.",
		context.state().evidence_details_open ? ui::Icon::Expanded : ui::Icon::Collapsed);
	if (!context.state().evidence_details_open)
		return;
	ui::text(context, row.proposed_memory ? "PREPARED EVIDENCE" : "ALREADY BAKED EVIDENCE",
			 interface_theme::kTextSecondary, 11);
	detail(context, "Evidence kind", evidence.kind);
	detail(context, "Dataset", std::to_string(evidence.dataset_id));
	detail(context, "Tick range",
		   "[" + std::to_string(evidence.begin_tick) + ", " +
			   std::to_string(evidence.end_tick_exclusive) + ")");
	detail(context, "Time range",
		   std::to_string(evidence.begin_ns) + "–" + std::to_string(evidence.end_ns) + " ns");
	detail(context, "Sample interval", std::to_string(evidence.sample_every_ticks) + " ticks");
	detail(context, "Selection policy", memory->policy);
	detail(context, "Measured demand", std::to_string(memory->observed_maximum));
	detail(context, "Headroom", std::to_string(memory->headroom));
	if (memory->policy == "percentile")
		detail(context, "Percentile", std::to_string(memory->percentile));
	ui::text(context, "REVIEW SNAPSHOT · LIVE OBSERVATIONS", interface_theme::kTextSecondary, 11);
	const auto observed =
		std::ranges::find(changes.memory_observations, row.key, &DevChangesMemoryObservation::key);
	if (observed == changes.memory_observations.end()) {
		ui::text(context, "No live observation was retained for this review.",
				 interface_theme::kTextSecondary, 12);
		return;
	}
	detail(context, "Observed startup", observed->startup);
	detail(context, "Demand high-water", observed->demand);
	detail(context, "Observation tick", std::to_string(observed->observed_tick));
	detail(context, "Observation time", std::to_string(observed->observed_ns) + " ns");
	detail(context, "Peak time", std::to_string(observed->peak_ns) + " ns");
	if (!observed->allocations.empty())
		detail(context, "Physical allocations", observed->allocations);
	ui::text(context, "Physical allocations are separate sources, not a sum of startup hints.",
			 interface_theme::kTextSecondary, 11);
}
void runtime_option(Context& context, const tooling::DevChangesReviewRow& row,
					std::size_t action_index) {
	static constexpr std::array<std::string_view, 4> labels{
		"Apply proposal", "Apply already baked value", "Discard live edit", "Reset to authored"};
	static constexpr std::array<std::string_view, 4> consequences{
		"Apply the proposal to the running application; already baked assignments are preserved.",
		"Apply the already baked value to the running application; baked assignments are "
		"preserved.",
		"Remove the retained live override. Compiled or authored precedence applies again.",
		"Apply the captured authored baseline to the running application."};
	static constexpr std::array<ui::Icon, 4> icons{ui::Icon::Check, ui::Icon::History,
												   ui::Icon::Close, ui::Icon::Refresh};
	auto& changes = context.params.interfaceState->changes;
	const auto schema = context.params.app ? context.params.app->devTooling().schemas().view()
										   : tooling::DevSchemaView{};
	tooling::DevChangesRuntimeEdit draft{};
	if (schema)
		draft = tooling::prepare_changes_runtime_edit(
			*schema, context.params.app->devTooling().overrides(), context.params.app->themes(),
			row, context.params.interfaceState->selectedWindowId,
			static_cast<tooling::DevChangesRuntimeAction>(action_index));
	const bool enabled = draft.available && !changes.runtime_transaction;
	const std::string description = changes.runtime_transaction
										? "Wait for the pending application acknowledgement."
									: !schema		   ? "Runtime schema unavailable."
									: !draft.available ? draft.error
													   : std::string(consequences[action_index]);
	local_button(context, Keyed("live-action", action_index), std::string(labels[action_index]),
				 Runtime + action_index, 0, enabled,
				 std::string(labels[action_index]) + ": " + description, icons[action_index]);
}
void live_tools(Context& context, const tooling::DevChangesReviewRow& row) {
	auto& changes = context.params.interfaceState->changes;
	auto& local = context.state();
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto menu_trigger = [&](bool apply) {
		return ::FlowUi::detail::element_id::resolveLocal(
			context.id, diagnostics_ui::Control::definitionId,
			apply ? LocalElementName{"apply-live-menu"}.token
				  : LocalElementName{"reset-live-menu"}.token);
	};
	if (local.restore_apply_focus || local.restore_reset_focus) {
		context.uiManager.shortcuts().setFocusedElement(
			context.uiManager.toClayEID(menu_trigger(local.restore_apply_focus)));
		local.restore_apply_focus = local.restore_reset_focus = false;
	}
	const bool next_option = current.keyDown[GLFW_KEY_DOWN] && !previous.keyDown[GLFW_KEY_DOWN];
	const bool previous_option = current.keyDown[GLFW_KEY_UP] && !previous.keyDown[GLFW_KEY_UP];
	if ((local.live_menu_open || local.reset_menu_open) && (next_option || previous_option) &&
		!context.uiManager.inputFields().hasPrimaryFieldFocus()) {
		const uint64_t first_action = local.live_menu_open ? 0 : 2;
		const std::array<FlowElementID, 2> options{
			::FlowUi::detail::element_id::resolveLocal(context.id,
													   diagnostics_ui::Control::definitionId,
													   Keyed("live-action", first_action).token),
			::FlowUi::detail::element_id::resolveLocal(
				context.id, diagnostics_ui::Control::definitionId,
				Keyed("live-action", first_action + 1).token)};
		const auto focused = context.uiManager.shortcuts().focusedElement().id;
		const auto first_id = context.uiManager.toClayEID(options[0]).id;
		const auto last_id = context.uiManager.toClayEID(options[1]).id;
		if (focused == first_id || focused == last_id ||
			focused == context.uiManager.toClayEID(menu_trigger(local.live_menu_open)).id) {
			const std::size_t destination = focused == first_id	 ? 1
											: focused == last_id ? 0
											: next_option		 ? 0
																 : 1;
			context.uiManager.shortcuts().setFocusedElement(
				context.uiManager.toClayEID(options[destination]));
		}
	}
	if ((local.live_menu_open || local.reset_menu_open) && current.keyDown[GLFW_KEY_ESCAPE] &&
		!previous.keyDown[GLFW_KEY_ESCAPE]) {
		const auto trigger = ::FlowUi::detail::element_id::resolveLocal(
			context.id, diagnostics_ui::Control::definitionId,
			local.live_menu_open ? LocalElementName{"apply-live-menu"}.token
								 : LocalElementName{"reset-live-menu"}.token);
		local.live_menu_open = local.reset_menu_open = false;
		context.uiManager.shortcuts().setFocusedElement(context.uiManager.toClayEID(trigger));
	}
	ui::text(
		context,
		"These actions affect the running application. Review the next-bake result separately.",
		interface_theme::kTextSecondary, 12);
	detail(context, "Retained live override", row.live_value, "Live override");
	ui::text(context, "A retained assignment may lose to a more specific override.",
			 interface_theme::kTextSecondary, 11);
	const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
	ui::text(context,
			 "Window for new commands: " +
				 std::to_string(context.params.interfaceState->selectedWindowId),
			 interface_theme::kTextSecondary, 11);
	if (entry)
		ui::text(context,
				 row.domain == tooling::DevChangesDomain::Themes ? "Command scope: theme variant"
				 : entry->targetScope == tooling::DevOverrideScope::Definition
					 ? "Command scope: definition"
					 : "Command scope: exact instance",
				 interface_theme::kTextSecondary, 11);
	auto history = ui::row();
	history.layout.sizing.height = CLAY_SIZING_FIT(0);
	const bool toolbar_icons_available = context.resources().texture(ui::Icon::Resume).handle &&
										 context.resources().texture(ui::Icon::Refresh).handle &&
										 context.resources().texture(ui::Icon::Undo).handle &&
										 context.resources().texture(ui::Icon::Redo).handle;
	if (!toolbar_icons_available)
		history.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	CLAY(context.clayID("live-history"), history) {
		local_button(context, LocalElementName{"apply-live-menu"}, "Apply to live…", ToggleApply, 0,
					 !changes.runtime_transaction,
					 "Apply to live: choose a proposal or already baked value for the running "
					 "application.",
					 ui::Icon::Resume);
		local_button(context, LocalElementName{"reset-live-menu"}, "Reset live edit…", ToggleReset,
					 0, !changes.runtime_transaction,
					 "Reset live edit: discard a live override or restore the captured authored "
					 "baseline.",
					 ui::Icon::Refresh);
		local_button(context, LocalElementName{"undo-live"}, "Undo live", Runtime + 4, 0,
					 !changes.runtime_undo.empty() && !changes.runtime_transaction,
					 "Undo the last acknowledged live command in its original scope and window. "
					 "Manifest decisions are unchanged.",
					 ui::Icon::Undo);
		local_button(context, LocalElementName{"redo-live"}, "Redo live", Runtime + 5, 0,
					 !changes.runtime_redo.empty() && !changes.runtime_transaction,
					 "Redo the last undone live command in its original scope and window. "
					 "Manifest decisions are unchanged.",
					 ui::Icon::Redo);
	}
	if (local.live_menu_open || local.reset_menu_open) {
		auto options = ui::row();
		options.layout.sizing.height = CLAY_SIZING_FIT(0);
		const bool option_icons_available = context.resources().texture(ui::Icon::Check).handle &&
											context.resources().texture(ui::Icon::History).handle &&
											context.resources().texture(ui::Icon::Close).handle &&
											context.resources().texture(ui::Icon::Refresh).handle;
		if (!option_icons_available)
			options.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(context.clayID("live-options"), options) {
			const std::size_t first_action = local.live_menu_open ? 0 : 2;
			runtime_option(context, row, first_action);
			runtime_option(context, row, first_action + 1);
		}
	}
	if (changes.runtime_transaction)
		ui::text(context, "Waiting for application acknowledgement…", interface_theme::kStatusAmber,
				 12);
	if (!changes.runtime_message.empty())
		notice(context, LocalElementName{"runtime-feedback"}, changes.runtime_message,
			   changes.runtime_message_is_error ? interface_theme::kStatusRed
												: interface_theme::kAccentCurrent);
}
void technical(Context& context, const tooling::DevChangesReviewRow& row) {
	detail(context, "Canonical identity", row.key, "Target key");
	detail(context, "Full field path", row.field_path, "Field path");
	if (!row.origin.empty())
		detail(context, "Origin", row.origin);
	const auto& entry = row.proposed_entry ? row.proposed_entry : row.disk_entry;
	if (entry) {
		detail(context, "Field ID", std::to_string(entry->fieldId));
		if (row.domain == tooling::DevChangesDomain::Themes)
			detail(context, "Theme type", std::to_string(entry->themeType));
		else {
			detail(context, "Definition ID", std::to_string(entry->definition.value));
			if (entry->targetScope == tooling::DevOverrideScope::ExactInstance)
				detail(context, "Stable instance", std::to_string(entry->instanceKey.value));
		}
		if (!entry->cppValue.empty())
			detail(context, "Initializer metadata", entry->cppValue, "Initializer");
		if (!entry->provenance.author.empty())
			detail(context, "Author", entry->provenance.author);
		if (!entry->provenance.timestamp.empty())
			detail(context, "Timestamp", entry->provenance.timestamp);
		if (!entry->provenance.note.empty())
			detail(context, "Note", entry->provenance.note);
		if (entry->provenance.author.empty() || entry->provenance.timestamp.empty())
			ui::text(context, "Historical provenance unavailable for some fields.",
					 interface_theme::kTextSecondary, 11);
	}
	if (row.domain != tooling::DevChangesDomain::Memory) {
		detail(context, "Authored baseline", row.authored_value);
		detail(context, "Compiled assignment", row.compiled_value, "Compiled");
	}
	detail(context, "Validation", tooling::changes_validity_name(row.validity));
	if (!row.reason.empty()) {
		detail(context, "Issue", row.reason);
		ui::text(context,
				 row.blocking
					 ? "Bake is blocked until this issue is resolved. Review the proposal, retain "
					   "the already baked value, or explicitly remove the assignment in the "
					   "Workbench when those decisions are available."
					 : "This issue is advisory for the current decision. Review the Already Baked, "
					   "Proposal, and Next Bake stages in the Workbench before baking.",
				 row.blocking ? interface_theme::kStatusAmber : interface_theme::kTextSecondary,
				 12);
	}
}
} // namespace

void DevChangesInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& changes = context.params.interfaceState->changes;
	auto& local = context.state();
	local.changes = &changes;
	if (local.expanded_details.capacity() == 0)
		local.expanded_details.reserve(24);
	if (local.selected_key != changes.selected_key) {
		local.selected_key = changes.selected_key;
		local.expanded_details.clear();
		local.evidence_details_open = local.live_menu_open = local.reset_menu_open = false;
		local.restore_apply_focus = local.restore_reset_focus = false;
	}
	const auto* selected = selected_changes_node(changes);
	const auto* row = selected_changes_row(changes);
	const float width = assigned_width(context);
	auto root = ui::column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		auto heading = ui::row(36);
		heading.layout.padding = {12, 12, 0, 0};
		heading.backgroundColor = interface_theme::kDepth2Ink;
		CLAY(context.clayID("heading"), heading)
		ui::text(context, "Inspector", interface_theme::kTextCanvas, 14);
		auto viewport =
			ui::viewport(context, "scroll-body", changes.inspector_scroll, &local.scroll_restore);
		viewport.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		CLAY(context.clayID("scroll-body"), viewport) {
			ui::FocusViewportScope focus_scope{changes, context.clayID("scroll-body")};
			ui::restore_viewport_scroll(context, LocalElementName{"scroll-body"},
										changes.inspector_scroll);
			auto content = ui::column(0);
			content.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			CLAY(context.clayID("sections"), content) {
				if (selected) {
					auto summary = ui::column(5);
					summary.layout.padding = {12, 12, 10, 12};
					CLAY(context.clayID("context-summary"), summary) {
						ui::text(context,
								 ui::ellipsize(context, row ? row->target_name : selected->label,
											   width - 24, 12),
								 interface_theme::kTextCanvas, 12);
						if (row) {
							auto operation = ui::row(22);
							operation.layout.sizing.width = CLAY_SIZING_FIT(0);
							operation.layout.padding = {6, 6, 0, 0};
							operation.backgroundColor = interface_theme::kDepth0Keel;
							CLAY(context.clayID("operation"), operation)
							inline_text(context,
										std::string(domain_label(row->domain)) + " · " +
											std::string(ui::operation_label(row->operation)),
										ui::operation_color(row->operation), 11);
						}
					}
				}
				if (!selected) {
					auto guidance = ui::column();
					guidance.layout.sizing.width = CLAY_SIZING_PERCENT(1);
					guidance.layout.padding = {12, 12, 16, 16};
					CLAY(context.clayID("empty-context"), guidance)
					ui::text(context, "Select a change to see its scope and evidence.",
							 interface_theme::kTextSecondary, 12);
				} else if (!row) {
					section(context, 0, "Scope", [&] {
						ui::text(context, selected->label, interface_theme::kTextCanvas, 13);
						ui::text(context,
								 std::to_string(selected->rows.size()) + " targets in this group.",
								 interface_theme::kTextSecondary, 12);
						ui::text(
							context,
							selected->key.starts_with("group:Memory")
								? "Future startup capacities and shared policy. Review this group "
								  "in the Workbench."
								: "Review this group's next-bake assignments in the Workbench.",
							interface_theme::kTextSecondary, 12);
					});
				} else {
					section(context, 0, "Scope", [&] { scope(context, *row); });
					const auto& entry = row->proposed_entry ? row->proposed_entry : row->disk_entry;
					if (entry && (!entry->sourceHeader.empty() || !entry->ownerCppType.empty() ||
								  !entry->valueTypeName.empty()))
						section(context, 1, "Source", [&] { source(context, *entry); });
					if (row->domain == tooling::DevChangesDomain::Memory &&
						(row->proposed_memory || row->disk_memory))
						section(context, 2, "Evidence", [&] { evidence(context, *row); });
					if (row->domain != tooling::DevChangesDomain::Memory)
						section(context, 3, "Live tools", [&] { live_tools(context, *row); });
					section(context, 4, "Technical details", [&] { technical(context, *row); });
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
