#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Performance/Workbench/DevTimelineLayout.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
int main() {
	assert(!timeline_project_interval(20, 10, 0, 20, 100, 0, 10));
	assert(!timeline_project_interval(0, 10, 10, 20, 100, 0, 10));
	assert(timeline_project_interval(10, 0, 0, 20, 100, 0, 10));
	assert(!timeline_project_interval(0, 10, 0, 20, 0, 0, 10));
	const auto clipped = timeline_project_interval(0, 30, 10, 10, 100, 5, 10);
	assert(clipped && clipped->x == 0 && clipped->width == 100 && clipped->y == 5);
	const auto large =
		timeline_project_interval(UINT64_MAX - 80, 50, UINT64_MAX - 100, 100, 1000, 0, 38);
	assert(large && large->x == 200 && large->width == 500);
	const auto saturated =
		timeline_project_interval(UINT64_MAX - 10, 100, UINT64_MAX - 100, 100, 100, 0, 38);
	assert(saturated && saturated->x == 90 && saturated->width == 10);
	DevTimelineState state;
	DevPerformanceSelection selection;
	selection.category_mask = UINT32_MAX;
	state.snapshot.start_ns = 1000;
	state.snapshot.end_ns = 101000;
	state.visible_start_ns = 1000;
	state.visible_duration_ns = 100000;
	state.snapshot.blocks = {
		{.label = "long", .start_ns = 1000, .duration_ns = 80000, .track = 1},
		{.label = "overlap", .start_ns = 21000, .duration_ns = 20000, .track = 1},
		{.label = "gap after", .start_ns = 91000, .duration_ns = 10000, .track = 1},
	};
	auto layout = build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, 1000);
	const auto overlapping = timeline_hit(layout, 350, 70);
	assert(overlapping != timeline_no_parent &&
		   layout.items[overlapping].command.members.front() == 1);
	const auto long_tail = timeline_hit(layout, 750, 35);
	assert(long_tail != timeline_no_parent && layout.items[long_tail].command.members.front() == 0);
	assert(timeline_hit(layout, 900, 35) == timeline_no_parent);
	const auto ruler_labels = std::ranges::count_if(layout.items, [](const auto& item) {
		return item.screen_space && item.label.starts_with("|");
	});
	assert(ruler_labels >= 3);
	state.snapshot.blocks[0].selected = true;
	layout = build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, 1000);
	assert(layout.items[timeline_hit(layout, 350, 70)].command.members.front() ==
		   1); // overlap remains in its own sibling lane
	state.cards = {{{0}, 1, 1}};
	state.snapshot.blocks[1].parent = 0;
	auto card = build_timeline_layout(state, selection, TimelineSurfaceKind::Card, 0, 1000);
	const auto child = std::ranges::find_if(card.items, [](const auto& item) {
		return item.command.action == TimelineAction::Inspect && item.command.members.front() == 1;
	});
	assert(child != card.items.end() && child->bounds.x == 362.5f && child->bounds.width == 212.5f);

	state.cards[0].visible_start_ns = 21000;
	state.cards[0].visible_duration_ns = 40000;
	const auto zoomed_minor =
		build_timeline_layout(state, selection, TimelineSurfaceKind::Card, 0, 1000);
	assert(zoomed_minor.start_ns == 21000 && zoomed_minor.duration_ns == 40000);
	assert(state.visible_start_ns == 1000 && state.visible_duration_ns == 100000);
	assert(std::ranges::count_if(zoomed_minor.items,
								 [](const auto& item) { return item.screen_space; }) >= 3);
	state.cards[0].visible_duration_ns = 0;
	state.cards[0].roots = {0, 1};
	card = build_timeline_layout(state, selection, TimelineSurfaceKind::Card, 0, 1000);
	assert(std::ranges::count_if(card.items, [](const auto& item) {
			   return item.command.action == TimelineAction::Inspect &&
					  item.command.members.size() == 1;
		   }) >= 2);
	state.pending = {{1}, TimelineAction::Open, 0, 0, state.snapshot_revision + 1};
	apply_timeline_command(state, selection);
	assert(state.cards.size() == 1 && state.cards[0].roots.size() == 2);
	// Labels preserve the original block origin when panning clips the left edge.
	state.cards.clear();
	state.visible_start_ns = 21000;
	state.visible_duration_ns = 80000;
	const auto clipped_layout =
		build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, 1000);
	const auto clipped_block = std::ranges::find_if(clipped_layout.items, [](const auto& item) {
		return item.command.members == std::vector<size_t>{0};
	});
	assert(clipped_block != clipped_layout.items.end() && clipped_block->text_offset_x < 0);
	state.snapshot.blocks[2].duration_ns = 100;
	const auto proxy_layout =
		build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, 1000);
	const auto proxy = std::ranges::find_if(proxy_layout.items, [](const auto& item) {
		return item.command.members == std::vector<size_t>{2};
	});
	assert(proxy != proxy_layout.items.end() && proxy->bounds.width == 2 && proxy->label.empty() &&
		   proxy->color.g > 200);
	selection.category_mask = 0;
	const auto filtered_layout =
		build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, 1000);
	assert(std::ranges::none_of(filtered_layout.items, [](const auto& item) {
		return item.command.action != TimelineAction::None;
	}));
	selection.category_mask = UINT32_MAX;
	state.minimap_zoom = 1;
	for (size_t count : {1000u, 10000u, 100000u}) {
		state.snapshot.frames.clear();
		state.snapshot.frame_metrics.clear();
		state.snapshot.blocks.clear();
		state.snapshot.frames.reserve(count);
		state.snapshot.frame_metrics.reserve(count);
		state.snapshot.blocks.reserve(count);
		state.snapshot.end_ns = state.snapshot.start_ns + count * 100000;
		state.visible_duration_ns = count * 100000;
		for (size_t index = 0; index < count; ++index) {
			TimelineBlockSlice block{.label = "Synthetic zone",
									 .start_ns = 1000 + index * 100000,
									 .duration_ns = 100000,
									 .track = index % 4};
			state.snapshot.frames.emplace_back(block);
			state.snapshot.blocks.emplace_back(block);
			state.snapshot.frame_metrics.emplace_back(index == count / 2 ? 50000000 : 100000);
		}
		const auto start = std::chrono::steady_clock::now();
		layout = build_timeline_layout(state, selection, TimelineSurfaceKind::Macro, 0, 1200);
		const auto minimap =
			build_timeline_layout(state, selection, TimelineSurfaceKind::Minimap, 0, 1200);
		size_t members = 0, bins = 0;
		float peak_height = 0;
		for (const auto& item : minimap.items)
			if (item.command.action == TimelineAction::Center) {
				members += item.command.members.size();
				++bins;
				peak_height = std::max(peak_height, item.bounds.height);
			}
		assert(members == count && bins <= 1200 && peak_height == 78);
		const auto elapsed =
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
				.count();
		std::cout << count << " samples: " << layout.items.size() << " macro items, " << bins
				  << " minimap bins, " << elapsed << " ms projection\n";
	}
	std::cout << "Timeline layout checks passed\n";
}
