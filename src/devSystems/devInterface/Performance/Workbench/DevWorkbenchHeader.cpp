#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
void DevWorkbenchHeader::runLogic(InteractionContext& context) {
	if (!context.params.timeline)
		return;
	auto& state = *context.params.timeline;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	// Timeline shortcuts remain available without hover, except while editing text.
	if (!input.windowFocused || context.uiManager.inputFields().hasPrimaryFieldFocus())
		return;
	if (!input.ctrl && !input.alt && !input.super) {
		if (input.keyDown[32] && !previous.keyDown[32])
			state.pending.action = TimelineAction::Pause;
		if (input.keyDown[257] && !previous.keyDown[257] &&
			state.inspected_sample < state.snapshot.blocks.size())
			state.pending = {{state.inspected_sample},
							 TimelineAction::Open,
							 state.cards.size(),
							 0,
							 state.snapshot_revision};
		if (input.keyDown[256] && !previous.keyDown[256] && !state.cards.empty())
			state.pending = {{}, TimelineAction::Close, state.cards.size() - 1};
		if (input.keyDown[263] && !previous.keyDown[263])
			state.pending.action = TimelineAction::Previous;
		if (input.keyDown[262] && !previous.keyDown[262])
			state.pending.action = input.shift ? TimelineAction::Spike : TimelineAction::Next;
	}
}
void DevWorkbenchHeader::buildElement(BuildContext& context) {
	if (!context.params.timeline || !context.params.selection)
		return;
	auto& state = *context.params.timeline;
	auto root = timeline_ui::row(154);
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 0;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	CLAY(context.clayID(), root) {
		auto controls = timeline_ui::row(32);
		controls.clip.horizontal = true;
		CLAY(context.clayID("controls"), controls) {
			timeline_ui::button(context, 1, state.paused ? "Follow" : "Freeze",
								{{}, TimelineAction::Pause});
			timeline_ui::button(context, 2, "<", {{}, TimelineAction::Previous});
			timeline_ui::button(context, 3, ">", {{}, TimelineAction::Next});
			timeline_ui::button(context, 4, "Spike", {{}, TimelineAction::Spike});
			for (size_t zoom_index = 0; zoom_index < 4; ++zoom_index) {
				constexpr double zooms[]{1, 2, 5, 10};
				timeline_ui::button(context, 10 + zoom_index,
									std::to_string(int(zooms[zoom_index])) + "x",
									{{}, TimelineAction::Zoom, 0, zooms[zoom_index]});
			}
			timeline_ui::button(
				context, 20,
				context.params.selection->hardware_domain == 0	 ? "CPU+GPU"
				: context.params.selection->hardware_domain == 1 ? "CPU"
																 : "GPU",
				{{}, TimelineAction::Domain, (context.params.selection->hardware_domain + 1) % 3});
			if (!state.snapshot.frames.empty()) {
				const auto duration = state.snapshot.frames.back().duration_ns;
				char delivery[72];
				std::snprintf(delivery, sizeof(delivery), "%.2f ms · %s", duration / 1e6,
							  state.snapshot.window_milestones ? "CPU frame duration"
							  : state.snapshot.tick_cadence	   ? "Tick cadence"
															   : "Recorded CPU envelope");
				timeline_ui::text(context, delivery);
			}
		}
		auto minimap_parameters = context.params;
		minimap_parameters.canvas_clip = context.clayID();
		timeline_viewport(context.uiManager, context.clayID("minimap"), minimap_parameters,
						  TimelineSurfaceKind::Minimap);
		const auto& frames = state.snapshot.frames;
		auto breadcrumbs = timeline_ui::row(26);
		breadcrumbs.clip.horizontal = true;
		CLAY(context.clayID("breadcrumbs"), breadcrumbs) {
			timeline_ui::button(
				context, 30,
				frames.empty() ? "No retained frames"
							   : frames[std::min(state.selected_frame, frames.size() - 1)].label,
				{{}, TimelineAction::Breadcrumb, 0});
			timeline_ui::button(context, 31, "Refresh snapshot", {{}, TimelineAction::Refresh});
			timeline_ui::button(context, 32, "Minimap fit",
								{{}, TimelineAction::MinimapZoom, 0, 1});
			timeline_ui::text(context, "Minimap " + std::to_string(int(state.minimap_zoom)) +
										   "x · Wheel to zoom");
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
