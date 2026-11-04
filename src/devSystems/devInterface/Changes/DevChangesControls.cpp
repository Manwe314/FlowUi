#include "devSystems/devInterface/Changes/DevChangesControls.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesUi.hpp"

namespace FlowUi::devSystems::interface_elements {
void DevChangesControls::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->changes;
	changes_ui::begin_focus_frame(context, state);
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float assigned_width = parent.found ? parent.boundingBox.width : 0;
	auto controls = changes_ui::row(36);
	CLAY(context.clayID(), controls) {
		changes_ui::button(
			context, LocalElementName{"review-trigger"}, "Review output", state, changes_ui::Review,
			0, true, "Review the complete next output. Filters and selection do not narrow Bake.");
		changes_ui::button(context, LocalElementName{"bake"},
						   "Bake (" + std::to_string(state.review.counts.pending()) + ")", state,
						   changes_ui::Bake, 0,
						   context.params.app && state.review.can_commit && !state.stale &&
							   state.memory_error.empty(),
						   "Writes the reviewed manifest. Rebuild the application to use it. " +
							   changes_summary(state.review.counts));
		if (assigned_width >= 600) {
			CLAY(context.clayID("bake-consequence"), changes_ui::column(0))
			changes_ui::text(context,
							 "Writes the reviewed manifest. Rebuild the application to use it.",
							 interface_theme::kTextSecondary, 11);
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
