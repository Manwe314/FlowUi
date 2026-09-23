#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>
namespace FlowUi::devSystems::interface_elements {
void DevWorkbenchHeader::runLogic(InteractionContext& context) {
	if (!context.params.timeline)
		return;
	auto& state = *context.params.timeline;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	// Restrict shortcuts to the Workbench to avoid typing into the zone search field.
	const auto root = Clay_GetElementData(context.clayID());
	const auto strip = Clay_GetElementData(context.clayID("minimap"));
	if (!root.found || input.mouseX < root.boundingBox.x ||
		input.mouseX > root.boundingBox.x + root.boundingBox.width)
		return;
	if (!input.ctrl && !input.alt && !input.super) {
		if (input.keyDown[32] && !previous.keyDown[32])
			state.pending.action = TimelineAction::Pause;
		if (input.keyDown[263] && !previous.keyDown[263])
			state.pending.action = TimelineAction::Previous;
		if (input.keyDown[262] && !previous.keyDown[262])
			state.pending.action = input.shift ? TimelineAction::Spike : TimelineAction::Next;
	}
	if (strip.found && input.mouseDown[0] && input.mouseY >= strip.boundingBox.y &&
		input.mouseY <= strip.boundingBox.y + strip.boundingBox.height) {
		const double fraction = std::clamp(double(input.mouseX - strip.boundingBox.x) /
											   std::max(1.0f, strip.boundingBox.width),
										   0.0, 1.0);
		state.paused = true;
		center_timeline(state, timeline_end(state.snapshot.start_ns,
											uint64_t(fraction * (state.snapshot.end_ns -
																 state.snapshot.start_ns))));
	}
}
void DevWorkbenchHeader::buildElement(BuildContext& context) {
	if (!context.params.timeline || !context.params.selection)
		return;
	auto& state = *context.params.timeline;
	auto root = timeline_ui::row(106);
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 0;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	CLAY(context.clayID(), root) {
		auto controls = timeline_ui::row(32);
		controls.clip.horizontal = true;
		CLAY(context.clayID("controls"), controls) {
			timeline_ui::button(context, 1, state.paused ? "Play" : "Pause",
								{{}, TimelineAction::Pause});
			timeline_ui::button(context, 2, "<", {{}, TimelineAction::Previous});
			timeline_ui::button(context, 3, ">", {{}, TimelineAction::Next});
			timeline_ui::button(context, 4, "Spike", {{}, TimelineAction::Spike});
			timeline_ui::button(context, 5, state.auto_freeze ? "Freeze: on" : "Freeze: off",
								{{}, TimelineAction::Freeze});
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
				std::snprintf(delivery, sizeof(delivery), "%.1f FPS · %.2f ms | 60 target",
							  duration ? 1e9 / duration : 0, duration / 1e6);
				timeline_ui::text(context, delivery);
			}
		}
		const auto range = std::max(uint64_t{1}, state.snapshot.end_ns - state.snapshot.start_ns);
		timeline_ui::ruler(context.uiManager, context.clayID("seconds"),
						   state.snapshot.start_ns >= state.origin_ns
							   ? state.snapshot.start_ns - state.origin_ns
							   : 0,
						   range, true);
		auto minimap = timeline_ui::row(32);
		minimap.layout.childGap = 0;
		minimap.layout.childAlignment.y = CLAY_ALIGN_Y_BOTTOM;
		const auto measured = Clay_GetElementData(context.clayID("minimap"));
		const float width = measured.found ? measured.boundingBox.width : 600;
		const auto& frames = state.snapshot.frames;
		const uint64_t maximum =
			state.snapshot.frame_metrics.empty()
				? 33'333'334
				: std::max(uint64_t{33'333'334},
						   *std::ranges::max_element(state.snapshot.frame_metrics));
		CLAY(context.clayID("minimap"), minimap) {
			float cursor = 0;
			for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
				const auto& frame = frames[frame_index];
				const float offset =
					float(double(frame.start_ns - state.snapshot.start_ns) / range * width);
				const uint64_t slot_end = frame_index + 1 < frames.size()
											  ? frames[frame_index + 1].start_ns
											  : state.snapshot.end_ns;
				const float bar_width =
					std::max(0.5f, float(double(slot_end - frame.start_ns) / range * width));
				if (offset + .01f < cursor)
					continue;
				Clay_ElementDeclaration spacer{};
				spacer.layout.sizing = {.width = CLAY_SIZING_FIXED(std::max(0.0f, offset - cursor)),
										.height = CLAY_SIZING_FIXED(1)};
				CLAY(context.clayID(Indexed("bar-gap", frame_index)), spacer);
				const auto metric = state.snapshot.frame_metrics[frame_index];
				const float height = std::max(2.0f, float(double(metric) / maximum * 30));
				auto color = timeline_ui::frame_color(metric);
				if (context.params.selection->selector_mode == 1 &&
					context.params.selection->selected_zone &&
					metric > state.snapshot.percentile_ns)
					color = interface_theme::kStatusRed;
				timeline_ui::button(context, 100 + frame_index, "",
									{{}, TimelineAction::Center, frame_index}, bar_width, color,
									height, frame_index == state.selected_frame);
				cursor = offset + bar_width;
			}
			// Floating overlays share the bar plot's bounds and never intercept clicks.
			for (uint32_t target = 0; target < 2; ++target) {
				auto line = timeline_ui::row(1);
				line.layout.sizing.width = CLAY_SIZING_FIXED(width);
				line.layout.childGap = target ? 4 : 6;
				line.floating.attachTo = CLAY_ATTACH_TO_PARENT;
				line.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
				line.floating.offset.y =
					32 - float(double(target ? 8'333'333 : 16'666'667) / maximum * 30);
				CLAY(context.clayID(Indexed("target", target)), line) {
					const float dash_width = target ? 1.0f : 6.0f;
					const uint32_t dash_count =
						uint32_t(width / (dash_width + line.layout.childGap));
					for (uint32_t dash_index = 0; dash_index < dash_count; ++dash_index) {
						Clay_ElementDeclaration dash{};
						dash.layout.sizing = {.width = CLAY_SIZING_FIXED(dash_width),
											  .height = CLAY_SIZING_FIXED(1)};
						dash.backgroundColor =
							target ? interface_theme::kTextMuted : interface_theme::kTextSecondary;
						CLAY(context.clayID(Indexed("target-dash", target * 10000 + dash_index)),
							 dash);
					}
				}
			}
			Clay_ElementDeclaration viewport{};
			viewport.layout.sizing = {
				.width = CLAY_SIZING_FIXED(
					std::max(1.0f, float(double(state.visible_duration_ns) / range * width))),
				.height = CLAY_SIZING_FIXED(32)};
			viewport.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			viewport.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			viewport.floating.offset.x =
				float(double(state.visible_start_ns - state.snapshot.start_ns) / range * width);
			viewport.backgroundColor = {115, 213, 197, 25};
			viewport.border = {.color = interface_theme::kAccentSeaGlass,
							   .width = Clay_BorderWidth{1, 1, 1, 1, 0}};
			CLAY(context.clayID("viewport"), viewport);
		}
		const auto breadcrumbs = timeline_ui::row(26);
		CLAY(context.clayID("breadcrumbs"), breadcrumbs) {
			timeline_ui::button(
				context, 30,
				frames.empty() ? "No retained frames"
							   : frames[std::min(state.selected_frame, frames.size() - 1)].label,
				{{}, TimelineAction::Breadcrumb, 0});
			for (size_t card_index = 0; card_index < state.cards.size(); ++card_index) {
				const auto& card = state.cards[card_index];
				const auto& block = state.snapshot.blocks[card.roots.front()];
				timeline_ui::button(context, 40 + card_index,
									"> " + (card.roots.size() > 1 ? "Micro zones" : block.label),
									{{}, TimelineAction::Breadcrumb, card_index + 1});
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
