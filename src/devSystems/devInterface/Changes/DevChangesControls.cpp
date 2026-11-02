#include "devSystems/devInterface/Changes/DevChangesControls.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"
namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = changes_ui;
void popover(DevChangesControls::BuildContext& context, DevChangesState& state) {
	const auto popup_id = context.childID("review-popover");
	const auto dimensions = Clay_GetLayoutDimensions();
	const float width = std::min(380.0f, std::max(200.0f, dimensions.width - 20));
	const float height = std::min(350.0f, std::max(160.0f, dimensions.height - 70));
	PopupRequest request{};
	request.anchor = PopupAnchor::element(::FlowUi::detail::element_id::resolveLocal(
		context.id, diagnostics_ui::Control::definitionId,
		LocalElementName{"review-trigger"}.token));
	request.expectedSize = {width, height};
	request.placement.offset.y = 5;
	const auto popup = context.uiManager.popups().request(popup_id, request);
	if (!popup) {
		state.review_open = false;
		state.message = "Review popup unavailable: " + std::string(popup.error().message());
		state.message_is_error = true;
		return;
	}
	if (!popup->visible && !popup->measureOnly)
		return;
	auto panel = ui::card(8);
	panel.layout.sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height)};
	panel.backgroundColor = interface_theme::kDepth1Panel;
	panel.floating = popup->floating;
	CLAY(context.clayID("review-popover"), panel) {
		CLAY(context.clayID("review-heading"), ui::row(28)) {
			ui::text(context, "Next bake", interface_theme::kTextCanvas, 14);
			ui::button(context, LocalElementName{"close-review"}, "Close", state, ui::CloseReview,
					   0, true, {}, ui::Icon::Close);
		}
		float unused_scroll = 0;
		CLAY(context.clayID("review-scroll"),
			 ui::viewport(context, "review-scroll", unused_scroll)) {
			CLAY(context.clayID("review-content"), ui::column(9)) {
				ui::text(context, state.review.manifest_path.generic_string(),
						 interface_theme::kTextSecondary, 11);
				ui::text(context, changes_summary(state.review.counts),
						 interface_theme::kTextCanvas, 13);
				const auto& counts = state.review.counts;
				ui::text(context,
						 "Elements " + std::to_string(counts.domains[0]) + " · Themes " +
							 std::to_string(counts.domains[1]) + " · Memory " +
							 std::to_string(counts.domains[2]),
						 interface_theme::kTextSecondary, 11);
				ui::text(context,
						 "Conflicts " + std::to_string(counts.conflicts) + " · errors " +
							 std::to_string(counts.errors) + " · warnings " +
							 std::to_string(counts.warnings),
						 counts.conflicts || counts.errors ? interface_theme::kStatusRed
														   : interface_theme::kTextSecondary,
						 12);
				if (state.stale)
					ui::text(context, "Review changed · refresh required",
							 interface_theme::kStatusAmber, 12);
				if (!state.review.error.empty())
					ui::text(context, state.review.error, interface_theme::kStatusRed, 11);
				if (!state.memory_error.empty())
					ui::text(context, state.memory_error, interface_theme::kStatusRed, 11);
				ui::text(context,
						 state.review.rebuild_required
							 ? "Rebuild required · disk differs from this executable"
							 : "Compiled content matches disk",
						 state.review.rebuild_required ? interface_theme::kStatusAmber
													   : interface_theme::kAccentSeaGlass,
						 12);
				ui::text(context,
						 "Bake commits this whole plan. Search, domain filters and selection only "
						 "affect the review display.",
						 interface_theme::kTextMuted, 11);
				ui::button(context, LocalElementName{"review-issues"}, "Review issues", state,
						   ui::Issues, 0, true,
						   "Show unresolved, invalid, conflicting and unavailable destinations in "
						   "the selector.");
				ui::button(context, LocalElementName{"refresh-review"}, "Refresh review", state,
						   ui::Refresh, 0, true, {}, ui::Icon::Refresh);
			}
		}
	}
}
} // namespace
void DevChangesControls::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->changes;
	const auto popup_id = context.childID("review-popover");
	if (context.uiManager.popups().consumeDismissed(popup_id))
		state.review_open = false;
	auto controls = ui::row(28);
	controls.layout.sizing.width = CLAY_SIZING_FIT(0);
	CLAY(context.clayID(), controls) {
		ui::button(context, LocalElementName{"review-trigger"}, "Review", state, ui::Review, 0,
				   true, changes_summary(state.review.counts),
				   state.review_open ? ui::Icon::Expanded : ui::Icon::Collapsed);
		ui::button(context, LocalElementName{"bake"},
				   "Bake (" + std::to_string(state.review.counts.pending()) + ")", state, ui::Bake,
				   0,
				   context.params.app && state.review.can_commit && !state.stale &&
					   state.memory_error.empty(),
				   changes_summary(state.review.counts) +
					   ". Writes the whole reviewed plan; rebuild required after success.");
		if (state.review_open)
			popover(context, state);
		if (!state.review_open && Clay_GetElementData(context.clayID("review-popover")).found) {
			context.uiManager.popups().dismiss(popup_id);
			// Consume our explicit close, so reopening cannot inherit its dismissal edge.
			(void)context.uiManager.popups().consumeDismissed(popup_id);
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
