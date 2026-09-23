#include "devSystems/devInterface/Performance/Workbench/DevContiguousTimelineStrip.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>
namespace FlowUi::devSystems::interface_elements {
void DevContiguousTimelineStrip::runLogic(InteractionContext& context) {
	if (!context.params.timeline)
		return;
	auto& state = *context.params.timeline;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto bounds = Clay_GetElementData(context.clayID());
	if (!bounds.found || input.mouseX < bounds.boundingBox.x ||
		input.mouseX > bounds.boundingBox.x + bounds.boundingBox.width ||
		input.mouseY < bounds.boundingBox.y ||
		input.mouseY > bounds.boundingBox.y + bounds.boundingBox.height)
		return;
	const double fraction = std::clamp(double(input.mouseX - bounds.boundingBox.x) /
										   std::max(1.0f, bounds.boundingBox.width),
									   0.0, 1.0);
	if (input.scrollY != 0) {
		const auto anchor =
			timeline_end(state.visible_start_ns, uint64_t(fraction * state.visible_duration_ns));
		state.zoom *= std::pow(1.2, input.scrollY);
		clamp_timeline_view(state);
		const auto offset = uint64_t(fraction * state.visible_duration_ns);
		state.visible_start_ns = anchor > offset ? anchor - offset : 0;
		state.paused = true;
	}
	// Middle-button drag or Shift+primary drag preserves ordinary block activation.
	const bool dragging = (input.mouseDown[2] && previous.mouseDown[2]) ||
						  (input.shift && input.mouseDown[0] && previous.mouseDown[0]);
	if (dragging || input.scrollX != 0) {
		const double delta = (dragging ? previous.mouseX - input.mouseX : input.scrollX * 24) /
							 std::max(1.0f, bounds.boundingBox.width) * state.visible_duration_ns;
		if (delta < 0)
			state.visible_start_ns -= std::min(state.visible_start_ns, uint64_t(-delta));
		else
			state.visible_start_ns = timeline_end(state.visible_start_ns, uint64_t(delta));
		state.paused = true;
	}
	clamp_timeline_view(state);
}
void DevContiguousTimelineStrip::buildElement(BuildContext& context) {
	if (!context.params.timeline || !context.params.selection)
		return;
	auto& state = *context.params.timeline;
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	const auto measured = Clay_GetElementData(context.clayID());
	const float width = measured.found ? measured.boundingBox.width : 600;
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
		timeline_ui::ruler(context.uiManager, context.clayID("ruler"),
						   state.visible_start_ns >= state.origin_ns
							   ? state.visible_start_ns - state.origin_ns
							   : 0,
						   state.visible_duration_ns, false);
		auto frames_lane = timeline_ui::row(24);
		frames_lane.layout.childGap = 0;
		frames_lane.clip.horizontal = true;
		frames_lane.clip.scrollInputDisabled = true;
		CLAY(context.clayID("frames"), frames_lane) {
			float cursor = 0;
			for (size_t frame_index = 0; frame_index < state.snapshot.frames.size();
				 ++frame_index) {
				const auto& frame = state.snapshot.frames[frame_index];
				const auto start = std::max(frame.start_ns, state.visible_start_ns);
				const auto end =
					std::min(timeline_end(frame.start_ns, frame.duration_ns),
							 timeline_end(state.visible_start_ns, state.visible_duration_ns));
				if (end <= start)
					continue;
				const float offset = float(double(start - state.visible_start_ns) /
										   state.visible_duration_ns * width);
				const float frame_width =
					float(double(end - start) / state.visible_duration_ns * width);
				if (offset < cursor)
					continue;
				Clay_ElementDeclaration gap{};
				gap.layout.sizing = {.width = CLAY_SIZING_FIXED(offset - cursor),
									 .height = CLAY_SIZING_FIXED(1)};
				CLAY(context.clayID(Indexed("frame-gap", frame_index)), gap);
				timeline_ui::button(
					context, 100 + frame_index,
					frame.label + " · " + timeline_ui::milliseconds(frame.duration_ns) +
						(frame.duration_ns > 33'333'333 ? " !" : ""),
					{{}, TimelineAction::Center, frame_index}, std::max(1.0f, frame_width),
					timeline_ui::frame_color(frame.duration_ns));
				cursor = offset + frame_width;
			}
		}
		const auto lanes = timeline_lanes(state.snapshot, state.active_depth);
		for (size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
			const auto& lane = lanes[lane_index];
			const auto domain = context.params.selection->hardware_domain;
			if ((domain == 1 && lane.domain != TimingSampleDomain::Cpu) ||
				(domain == 2 && lane.domain != TimingSampleDomain::Gpu))
				continue;
			timeline_ui::text(
				context,
				std::string(lane.domain == TimingSampleDomain::Cpu ? "CPU thread " : "GPU queue ") +
					std::to_string(lane.track) + " · Depth " + std::to_string(lane.depth + 1));
			timeline_ui::lane(context.uiManager, context.clayID(Indexed("lane", lane_index)),
							  context.params, lane.blocks, state.visible_start_ns,
							  state.visible_duration_ns, width, 0);
		}
		if (lanes.empty())
			timeline_ui::text(
				context, "No timing samples in this scope. Enable timing capture to record zones.");
		if (state.snapshot.uncalibrated_gpu_count)
			timeline_ui::text(
				context,
				std::to_string(state.snapshot.uncalibrated_gpu_count) +
					" GPU samples lack host-clock calibration; unavailable on correlated axis.");
		timeline_ui::text(
			context, "Wheel: zoom · Middle drag / Shift+drag: pan · Click a block: inside look");
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
