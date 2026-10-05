#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Performance/Workbench/DevContiguousTimelineStrip.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevDrillDownTimelineCard.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
void DevPerformanceWorkbench::buildElement(BuildContext& context) {
	if (!context.params.interfaceState || !context.params.app)
		return;
	auto& state = context.state();
	auto& selection = context.params.interfaceState->performance_selection;
	// Freeze the displayed snapshot before a press can be resolved against a newer ring buffer.
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto workbench_bounds = Clay_GetElementData(context.clayID());
	if ((input.mouseDown[0] || input.mouseDown[2]) && workbench_bounds.found &&
		input.mouseX >= workbench_bounds.boundingBox.x &&
		input.mouseX <= workbench_bounds.boundingBox.x + workbench_bounds.boundingBox.width &&
		input.mouseY >= workbench_bounds.boundingBox.y + 32 &&
		input.mouseY <= workbench_bounds.boundingBox.y + workbench_bounds.boundingBox.height)
		state.paused = true;
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	if ((input.scrollX != 0 || input.scrollY != 0) && context.uiManager.timeline_controller() &&
		context.uiManager.timeline_controller()->owns_scroll(input))
		state.paused = true;
#endif
	apply_timeline_command(state, selection);
	auto& reporting = context.params.app->devMonitoring().timingReporting();
	const auto status = reporting.status();
	const bool scope_changed = state.selection.selected_scope != selection.selected_scope ||
							   state.selection.selected_zone != selection.selected_zone ||
							   state.selection.selector_mode != selection.selector_mode ||
							   state.selection.hardware_domain != selection.hardware_domain ||
							   state.selection.category_mask != selection.category_mask;
	if ((!state.paused && status.mutationSequence != state.mutation_sequence) || scope_changed ||
		state.mutation_sequence == UINT64_MAX) {
		if (!state.paused || state.mutation_sequence == UINT64_MAX) {
			state.retained_reports = reporting.appTickRange(status.oldestRetainedAppTick,
															size_t(status.retainedTickCount));
			state.descriptors = reporting.descriptorSnapshot();
		}
		++state.snapshot_revision;
		state.snapshot = extract_timeline(state.retained_reports, state.descriptors, selection);
		if (state.origin_ns == 0 && state.snapshot.start_ns)
			state.origin_ns = state.snapshot.start_ns;
		state.cards.clear();
		state.mutation_sequence = status.mutationSequence;
		if (!state.snapshot.frames.empty())
			state.selected_frame = state.snapshot.frames.size() - 1;
		if (state.auto_freeze) {
			for (size_t frame_index = 0; frame_index < state.snapshot.frames.size();
				 ++frame_index) {
				if (state.snapshot.frames[frame_index].app_tick > state.last_seen_tick &&
					state.snapshot.frame_metrics[frame_index] > 16'666'667) {
					state.paused = true;
					state.selected_frame = frame_index;
					break;
				}
			}
		}
		state.last_seen_tick = status.newestRetainedAppTick;
		state.visible_start_ns = state.snapshot.end_ns;
		clamp_timeline_view(state);
		if (state.paused && !state.snapshot.frames.empty())
			center_timeline(state, state.snapshot.frames[state.selected_frame].start_ns);
	}
	state.selection = selection;
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth0Keel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	const DevTimelineParameters parameters{&state, &selection, 0, context.clayID("canvas")};
	CLAY(context.clayID(), root) {
		context.uiManager.createElement(kDevWorkbenchHeader, LocalElementName{"header"})
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
		Clay_ElementDeclaration canvas{};
		canvas.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		canvas.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		canvas.layout.childGap = 12;
		canvas.clip.vertical = true;
		const auto scroll = Clay_GetScrollContainerData(context.clayID("canvas"));
		if (scroll.found && scroll.scrollPosition) {
			if (state.reveal_frames) {
				const float target = -std::max(0.0f, scroll.contentDimensions.height -
														 scroll.scrollContainerDimensions.height);
				scroll.scrollPosition->y += (target - scroll.scrollPosition->y) * .45f;
				--state.reveal_frames;
			}
			canvas.clip.childOffset = *scroll.scrollPosition;
		}

		CLAY(context.clayID("canvas"), canvas) {
			context.uiManager.createElement(kDevContiguousTimelineStrip, LocalElementName{"macro"})
				.setParameters(parameters)
				.setDevInternalCapture(true)
				.draw();
			for (size_t card_index = 0; card_index < state.cards.size(); ++card_index) {
				context.uiManager
					.createElement(kDevDrillDownTimelineCard, Keyed("card", card_index))
					.setParameters(DevTimelineParameters{&state, &selection, card_index,
														 context.clayID("canvas")})
					.setDevInternalCapture(true)
					.draw();
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
