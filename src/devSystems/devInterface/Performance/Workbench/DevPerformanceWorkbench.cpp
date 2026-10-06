#include "devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/SplitterHandle.hpp"
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
	auto& state = context.params.interfaceState->performance_timeline;
	auto& selection = context.params.interfaceState->performance_selection;
	// Freeze the displayed snapshot before a press can be resolved against a newer ring buffer.
	const auto& input = context.uiManager.getCurrentFrameInput();
	if (input.windowFocused && (input.keyDown[264] || input.keyDown[265]) &&
		!context.uiManager.inputFields().hasPrimaryFieldFocus())
		state.paused = true;
	const auto workbench_bounds = Clay_GetElementData(context.clayID());
	if ((input.mouseDown[0] || input.mouseDown[2]) && workbench_bounds.found &&
		input.mouseX >= workbench_bounds.boundingBox.x &&
		input.mouseX <= workbench_bounds.boundingBox.x + workbench_bounds.boundingBox.width +
							context.params.interfaceState->inspectorWidth &&
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
	const auto snapshot_clock = context.params.app->devMonitoring().timing().nowNs();
	const bool scope_changed = state.selection.selected_scope != selection.selected_scope ||
							   state.selection.selected_zone != selection.selected_zone ||
							   state.selection.selector_mode != selection.selector_mode ||
							   state.selection.hardware_domain != selection.hardware_domain ||
							   state.selection.category_mask != selection.category_mask;
	if ((!state.paused && status.mutationSequence != state.mutation_sequence &&
		 snapshot_clock - state.last_snapshot_refresh_ns >= 100'000'000) ||
		state.refresh_requested || scope_changed || state.mutation_sequence == UINT64_MAX) {
		if (!state.paused || state.refresh_requested || state.mutation_sequence == UINT64_MAX) {
			auto capture = reporting.capture_snapshot();
			state.last_snapshot_refresh_ns = snapshot_clock;
			state.retained_reports = std::move(capture.reports);
			state.descriptors = std::move(capture.descriptors);
			state.mutation_sequence = capture.mutation_sequence;
		}
		auto previous_snapshot = std::move(state.snapshot);
		auto previous_cards = std::move(state.cards);
		const auto previous_inspected = state.inspected_sample;
		++state.snapshot_revision;
		state.snapshot = extract_timeline(state.retained_reports, state.descriptors, selection);
		if (state.origin_ns == 0 && state.snapshot.start_ns)
			state.origin_ns = state.snapshot.start_ns;
		state.cards.clear();
		state.inspected_sample = timeline_no_parent;
		state.refresh_requested = false;
		if (state.paused) {
			const auto remap = [&](size_t old_index) {
				if (old_index >= previous_snapshot.blocks.size())
					return timeline_no_parent;
				const auto& previous = previous_snapshot.blocks[old_index];
				for (size_t candidate_index = 0; candidate_index < state.snapshot.blocks.size();
					 ++candidate_index) {
					const auto& candidate = state.snapshot.blocks[candidate_index];
					if (candidate.scope_visible && candidate.domain == previous.domain &&
						candidate.track == previous.track &&
						candidate.type_id == previous.type_id &&
						candidate.synthetic_tick == previous.synthetic_tick &&
						(previous.domain == TimingSampleDomain::Cpu
							 ? candidate.invocation_id == previous.invocation_id &&
								   (previous.synthetic_tick ||
									candidate.start_ns == previous.start_ns)
							 : candidate.device_identity == previous.device_identity &&
								   candidate.queue_identity == previous.queue_identity &&
								   candidate.frame == previous.frame &&
								   candidate.submission_serial == previous.submission_serial &&
								   candidate.zone_index == previous.zone_index))
						return candidate_index;
				}
				return timeline_no_parent;
			};
			for (auto& card : previous_cards) {
				for (auto& root_index : card.roots)
					root_index = remap(root_index);
				if (std::ranges::find(card.roots, timeline_no_parent) != card.roots.end())
					break;
				state.cards.emplace_back(std::move(card));
			}
			state.inspected_sample = remap(previous_inspected);
		}

		if (!state.snapshot.frames.empty())
			state.selected_frame = state.snapshot.frames.size() - 1;
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
	const DevTimelineParameters parameters{&state, &selection, 0, context.clayID("canvas"),
										   context.params.column_clip};
	CLAY(context.clayID(), root) {
		context.uiManager.createElement(kDevWorkbenchHeader, LocalElementName{"header"})
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
		Clay_ElementDeclaration canvas{};
		canvas.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		canvas.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		const auto canvas_bounds = Clay_GetElementData(context.clayID("canvas"));
		const float available_height = canvas_bounds.found ? canvas_bounds.boundingBox.height : 600;
		state.major_height =
			std::clamp(state.major_height, 80.0f, std::max(80.0f, available_height - 84));

		CLAY(context.clayID("canvas"), canvas) {
			auto major = canvas;
			major.layout.sizing.height = CLAY_SIZING_FIXED(state.major_height);

			CLAY(context.clayID("major-pane"), major) {
				context.uiManager
					.createElement(kDevContiguousTimelineStrip, LocalElementName{"macro"})
					.setParameters(DevTimelineParameters{&state, &selection, 0,
														 context.clayID("major-pane"),
														 context.params.column_clip})
					.setDevInternalCapture(true)
					.draw();
			}
			context.uiManager
				.createElement(FSEL::kSplitterHandle, LocalElementName{"timeline-divider"})
				.setParameters(FSEL::SplitterHandleParameters{
					.axis = FSEL::SplitterAxis::Vertical,
					.targetExtent = &state.major_height,
					.minExtent = 80,
					.maxExtent = std::max(80.0f, available_height - 84),
					.backgroundColor = interface_theme::kTextMuted,
					.hoverColor = interface_theme::kAccentSeaGlass})
				.setDevInternalCapture(true)
				.draw();
			auto minor = canvas;

			CLAY(context.clayID("minor-pane"), minor) {
				context.uiManager
					.createElement(kDevDrillDownTimelineCard, LocalElementName{"minor"})
					.setParameters(DevTimelineParameters{
						&state, &selection, state.cards.empty() ? 0 : state.cards.size() - 1,
						context.clayID("minor-pane"), context.params.column_clip})
					.setDevInternalCapture(true)
					.draw();
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
