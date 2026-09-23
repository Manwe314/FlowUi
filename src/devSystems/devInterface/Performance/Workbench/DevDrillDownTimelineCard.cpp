#include "devSystems/devInterface/Performance/Workbench/DevDrillDownTimelineCard.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
void DevDrillDownTimelineCard::buildElement(BuildContext& context) {
	if (!context.params.timeline ||
		context.params.card_index >= context.params.timeline->cards.size())
		return;
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
		exclusive = timeline_end(exclusive, block.exclusive_ns);
	}
	const uint64_t duration = end - start;
	const auto label =
		card.roots.size() == 1 ? target.label : std::to_string(card.roots.size()) + " micro zones";
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	const auto measured = Clay_GetElementData(context.clayID());
	const float width = measured.found ? measured.boundingBox.width : 600;
	CLAY(context.clayID(), root) {
		const auto header = timeline_ui::row(28);
		CLAY(context.clayID("header"), header) {
			timeline_ui::text(context, "Inside: " + label + " · " +
										   timeline_ui::milliseconds(duration) + " · Self " +
										   timeline_ui::milliseconds(exclusive));
			timeline_ui::text(context, "Depth " + std::to_string(card.active_depth));
			timeline_ui::button(context, 1, "+",
								{{}, TimelineAction::Depth, context.params.card_index, 1});
			timeline_ui::button(context, 2, "-",
								{{}, TimelineAction::Depth, context.params.card_index, -1});
			timeline_ui::button(context, 3, "Close",
								{{}, TimelineAction::Close, context.params.card_index});
		}
		timeline_ui::ruler(context.uiManager, context.clayID("ruler"), 0, duration, false);
		auto parent = timeline_ui::row(26);
		parent.backgroundColor = timeline_ui::block_color(target, false);
		CLAY(context.clayID("target"), parent) {
			timeline_ui::text(context,
							  label + " · Frame #" + std::to_string(target.frame.frameNumber) +
								  " · " +
								  std::string(performance_category_names[size_t(target.category)]),
							  interface_theme::kTextCanvas);
		}
		auto lanes = timeline_lanes(state.snapshot, card.active_depth, card.roots);
		if (card.roots.size() > 1)
			lanes.insert(lanes.begin(),
						 TimelineTrackLane{card.roots, target.track, 0, target.domain});
		for (size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
			timeline_ui::lane(context.uiManager, context.clayID(Indexed("lane", lane_index)),
							  context.params, lanes[lane_index].blocks, start, duration, width,
							  context.params.card_index + 1,
							  !(card.roots.size() > 1 && lane_index == 0));
		}
		if (lanes.empty())
			timeline_ui::text(
				context, "No recorded children · leaf, self time, or capture detail unavailable.");
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
