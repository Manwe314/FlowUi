#include "devSystems/devInterface/Performance/Workbench/DevContiguousTimelineStrip.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
void DevContiguousTimelineStrip::buildElement(BuildContext& context) {
	if (!context.params.timeline || !context.params.selection)
		return;
	auto& state = *context.params.timeline;
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		const auto header = timeline_ui::row(28);
		CLAY(context.clayID("header"), header) {
			const auto& scope = context.params.selection->selected_scope;
			timeline_ui::text(context, scope.kind == DevPerformanceScopeKind::Thread
										   ? "Thread " + std::to_string(scope.id)
									   : scope.id ? "Window " + std::to_string(scope.id)
												  : "All Windows · AppTicks");
			timeline_ui::text(context, "Depth: " + std::to_string(state.active_depth));
			timeline_ui::button(context, 1, "+ Depth",
								{{}, TimelineAction::Depth, timeline_no_parent, 1});
			timeline_ui::button(context, 2, "- Depth",
								{{}, TimelineAction::Depth, timeline_no_parent, -1});
			if (context.params.selection->selector_mode == 1 &&
				context.params.selection->selected_zone) {
				const auto count =
					std::ranges::count_if(state.snapshot.blocks, [&](const auto& block) {
						return block.selected &&
							   block.start_ns < timeline_end(state.visible_start_ns,
															 state.visible_duration_ns) &&
							   timeline_end(block.start_ns, block.duration_ns) >
								   state.visible_start_ns;
					});
				timeline_ui::text(context, std::to_string(count) + " occurrences");
			}
		}
		timeline_viewport(context.uiManager, context.clayID("body"), context.params,
						  TimelineSurfaceKind::Macro);
		if (state.snapshot.blocks.empty())
			timeline_ui::text(
				context, "No timing samples in this scope. Enable timing capture to record zones.");
		if (state.snapshot.uncalibrated_gpu_count)
			timeline_ui::text(
				context,
				std::to_string(state.snapshot.uncalibrated_gpu_count) +
					" GPU samples lack host-clock calibration; unavailable on correlated axis.");
		timeline_ui::text(context,
						  "Wheel: zoom · Middle-button drag: pan · Click a block: inside look");
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
