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
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	root.border.color = interface_theme::kTextMuted;
	root.border.width.top = 1;
	CLAY(context.clayID(), root) {
		const auto header = timeline_ui::row(28);
		CLAY(context.clayID("header"), header) {
			const auto& scope = context.params.selection->selected_scope;
			timeline_ui::text(context, scope.kind == DevPerformanceScopeKind::Thread
										   ? "Thread " + std::to_string(scope.id)
									   : scope.id ? "Window " + std::to_string(scope.id)
												  : "All Windows · App Ticks");
			timeline_ui::button(context, 1, "Fit history", {{}, TimelineAction::Zoom, 0, 1});
			timeline_ui::button(context, 2, "Fit selected", {{}, TimelineAction::FitSelected});
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
		Clay_ElementDeclaration plot{};
		plot.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		plot.clip.vertical = true;
		plot.clip.horizontal = true;
		const auto scroll = Clay_GetScrollContainerData(context.clayID("plot-scroll"));
		if (scroll.found && scroll.scrollPosition)
			plot.clip.childOffset = *scroll.scrollPosition;
		auto parameters = context.params;
		parameters.canvas_clip = context.clayID("plot-scroll");
		CLAY(context.clayID("plot-scroll"), plot) {
			timeline_viewport(context.uiManager, context.clayID("body"), parameters,
							  TimelineSurfaceKind::Macro);
		}
		if (state.snapshot.blocks.empty())
			timeline_ui::text(
				context,
				"No samples match this scope. Adjust filters or capture with more detail.");
		if (state.snapshot.uncalibrated_gpu_count)
			timeline_ui::text(
				context, std::to_string(state.snapshot.uncalibrated_gpu_count) +
							 " GPU samples lack host-clock calibration; use the GPU local picker.");
		auto hint = timeline_ui::row(20);
		const auto column = Clay_GetElementData(context.params.column_clip);
		if (column.found)
			hint.layout.sizing.width = CLAY_SIZING_FIXED(column.boundingBox.width);
		hint.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
		hint.clip.horizontal = true;
		CLAY(context.clayID("hint"), hint) {
			Clay_TextElementConfig config{};
			config.fontSize = 9;
			config.textColor = interface_theme::kTextSecondary;
			config.wrapMode = CLAY_TEXT_WRAP_NONE;
			CLAY_TEXT(context.uiManager.toClayString(
						  "Wheel: zoom · Middle drag: pan · Shift+wheel: scroll · Click: inspect"),
					  CLAY_TEXT_CONFIG(config));
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
