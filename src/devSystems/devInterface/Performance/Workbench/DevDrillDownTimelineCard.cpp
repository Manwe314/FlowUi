#include "devSystems/devInterface/Performance/Workbench/DevDrillDownTimelineCard.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
void DevDrillDownTimelineCard::buildElement(BuildContext& context) {
	if (!context.params.timeline ||
		context.params.card_index >= context.params.timeline->cards.size()) {
		if (context.params.timeline)
			timeline_ui::text(context, "Select a major block to inspect its breakdown.");
		return;
	}
	auto& state = *context.params.timeline;
	const auto& card = state.cards[context.params.card_index];
	if (card.roots.empty())
		return;
	const auto& target = state.snapshot.blocks[card.roots.front()];
	uint64_t start = target.start_ns, end = timeline_end(start, target.duration_ns), exclusive = 0;
	for (const auto index : card.roots) {
		const auto& block = state.snapshot.blocks[index];
		start = std::min(start, block.start_ns);
		end = std::max(end, timeline_end(block.start_ns, block.duration_ns));
		exclusive = timeline_end(exclusive, block.recorded_exclusive_ns());
	}
	const uint64_t duration = end - start;
	const auto label = card.roots.size() == 1 ? std::string(target.label)
											  : std::to_string(card.roots.size()) + " micro zones";
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	CLAY(context.clayID(), root) {
		const auto header = timeline_ui::row(28);
		CLAY(context.clayID("header"), header) {
			timeline_ui::text(
				context, std::string(card.roots.size() > 1		? "Choose a member: "
									 : target.cpu_clock_aligned ? "Inside: "
																: "GPU local time: ") +
							 label + " · " + timeline_ui::milliseconds(duration) +
							 (card.roots.size() == 1 && target.domain == TimingSampleDomain::Cpu &&
									  !target.synthetic_tick
								  ? " · Recorded exclusive " + timeline_ui::milliseconds(exclusive)
								  : std::string{}));
			if (card.roots.size() == 1 && !target.synthetic_tick) {
				timeline_ui::text(context, card.active_depth == 256
											   ? "Recorded hierarchy"
											   : "Depth " + std::to_string(card.active_depth));
				timeline_ui::button(context, 1, "Expand all",
									{{}, TimelineAction::Depth, context.params.card_index, 255});
				timeline_ui::button(context, 2, "Collapse",
									{{}, TimelineAction::Depth, context.params.card_index, -255});
			}
			timeline_ui::button(context, 3, "Back",
								{{}, TimelineAction::Close, context.params.card_index});
		}
		const auto breadcrumbs = timeline_ui::row(26);
		CLAY(context.clayID("breadcrumbs"), breadcrumbs) {
			for (size_t history_index = 0; history_index < state.cards.size(); ++history_index) {
				const auto& focus = state.cards[history_index];
				const auto& block = state.snapshot.blocks[focus.roots.front()];
				timeline_ui::button(context, 40 + history_index,
									focus.roots.size() > 1 ? "Aggregate members"
														   : std::string(block.label),
									{{}, TimelineAction::Breadcrumb, history_index + 1});
			}
		}
		Clay_ElementDeclaration plot{};
		plot.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		plot.clip.vertical = true;
		plot.clip.horizontal = true;
		const auto scroll = Clay_GetScrollContainerData(context.clayID("plot-scroll"));
		if (scroll.found && scroll.scrollPosition) {
			if (state.restore_minor_scroll)
				scroll.scrollPosition->y = card.scroll_y;
			else
				state.cards[context.params.card_index].scroll_y = scroll.scrollPosition->y;
			state.restore_minor_scroll = false;
			plot.clip.childOffset = *scroll.scrollPosition;
		}
		auto parameters = context.params;
		parameters.canvas_clip = context.clayID("plot-scroll");
		CLAY(context.clayID("plot-scroll"), plot) {
			timeline_viewport(context.uiManager, context.clayID("body"), parameters,
							  TimelineSurfaceKind::Card);
		}
		if (card.roots.size() == 1 &&
			std::ranges::none_of(state.snapshot.blocks, [&](const auto& block) {
				return block.parent == card.roots.front();
			}))
			timeline_ui::text(
				context, "No recorded children · leaf, self time, or capture detail unavailable.");
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
