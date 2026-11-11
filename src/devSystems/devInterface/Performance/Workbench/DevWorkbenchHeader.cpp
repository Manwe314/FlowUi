#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
namespace {
[[nodiscard]] constexpr std::string_view recording_level_name(CpuTimingLevel level) noexcept {
	switch (level) {
	case CpuTimingLevel::OnlyFrameTime: return "Frame Only";
	case CpuTimingLevel::Summary: return "Summary";
	case CpuTimingLevel::Balanced: return "Balanced";
	case CpuTimingLevel::Deep: return "Deep";
	}
	return "Unknown";
}
} // namespace
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
	auto root = timeline_ui::row(176);
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 0;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	CLAY(context.clayID(), root) {
		auto policy_row = timeline_ui::row(22);
		policy_row.clip.horizontal = true;
		policy_row.clip.scrollInputDisabled = true;
		CLAY(context.clayID("recorded-cpu-policy"), policy_row) {
			std::string label = "Recorded CPU: ";
			if (state.snapshot.has_recording_policy) {
				label += recording_level_name(state.snapshot.minimum_recorded_cpu_level);
				if (state.snapshot.minimum_recorded_cpu_level != state.snapshot.maximum_recorded_cpu_level) {
					label += " to ";
					label += recording_level_name(state.snapshot.maximum_recorded_cpu_level);
				}
			} else {
				label += "unavailable";
			}
			label += " · View CPU: ";
			label += recording_level_name(context.params.selection->cpu_detail_level);
			label += " · Depth controls change display only";
			timeline_ui::text(context, label, interface_theme::kAccentSeaGlass);
		}
		auto controls = timeline_ui::row(32);
		controls.clip.horizontal = true;
		CLAY(context.clayID("controls"), controls) {
			timeline_ui::button(context, 2, "Previous frame", {{}, TimelineAction::Previous},
								interface_icons::kPreviousKey);
			timeline_ui::button(context, 3, "Next frame", {{}, TimelineAction::Next},
								interface_icons::kNextKey);
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
			timeline_ui::button(context, 26,
								state.track_controls_open ? "Close tracks" : "Major tracks",
								{{}, TimelineAction::TrackControls}, interface_icons::kSettingsKey);
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
				frames.empty()
					? "No retained frames"
					: std::string(frames[std::min(state.selected_frame, frames.size() - 1)].label),
				{{}, TimelineAction::Breadcrumb, 0});
			timeline_ui::button(context, 32, "Minimap fit", {{}, TimelineAction::MinimapZoom, 0, 1},
								interface_icons::kFitKey);
			timeline_ui::text(context, "Minimap " + std::to_string(int(state.minimap_zoom)) +
										   "x · Wheel to zoom");
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
