#include "devSystems/devInterface/Performance/Workbench/DevTimelineLayout.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>

namespace FlowUi::devSystems::interface_elements {
std::optional<Clay_BoundingBox>
timeline_project_interval(uint64_t sample_start, uint64_t sample_duration, uint64_t view_start,
						  uint64_t view_duration, float width, float top, float height) noexcept {
	const auto clipped_start = std::max(sample_start, view_start);
	const auto clipped_end = std::min(timeline_end(sample_start, sample_duration),
									  timeline_end(view_start, view_duration));
	if (clipped_end <= clipped_start || width <= 0 || height <= 0 || !std::isfinite(width))
		return std::nullopt;
	const double scale = double(width) / std::max(uint64_t{1}, view_duration);
	const float left = std::min(width, float(double(clipped_start - view_start) * scale));
	const float right =
		std::min(width, std::max(left + 1, float(double(clipped_end - view_start) * scale)));
	return Clay_BoundingBox{std::min(left, std::max(0.0f, width - 1)), top,
							std::max(0.0f, right - std::min(left, std::max(0.0f, width - 1))),
							height};
}
size_t timeline_hit(const TimelineLayout& layout, float horizontal, float vertical) noexcept {
	for (size_t remaining = layout.items.size(); remaining; --remaining) {
		const auto& item = layout.items[remaining - 1];
		const auto& bounds = item.bounds;
		if (item.command.action != TimelineAction::None && horizontal >= bounds.x &&
			horizontal < bounds.x + bounds.width && vertical >= bounds.y &&
			vertical < bounds.y + bounds.height)
			return remaining - 1;
	}
	return timeline_no_parent;
}
namespace {
void decoration(TimelineLayout& layout, Clay_BoundingBox bounds, Clay_Color color,
				std::string label = {}) {
	layout.items.emplace_back(TimelineDisplayItem{std::move(label), {}, {}, bounds, color});
}
void ruler(TimelineLayout& layout, uint64_t origin, bool seconds) {
	const auto relative = layout.start_ns >= origin ? layout.start_ns - origin : 0;
	const double requested = double(layout.duration_ns) / std::max(1.0, double(layout.width) / 100);
	const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(1.0, requested))));
	const double multiple = requested / magnitude;
	const double cadence =
		seconds && layout.duration_ns >= 500'000'000
			? (requested <= 500'000'000 ? 500'000'000 : std::ceil(requested / 1e9) * 1e9)
			: magnitude * (multiple <= 1   ? 1
						   : multiple <= 2 ? 2
						   : multiple <= 5 ? 5
										   : 10);
	const auto step = uint64_t(std::clamp(cadence, 1.0, double(UINT64_MAX / 2)));
	const auto remainder = relative % step;
	uint64_t offset = remainder ? step - remainder : 0;
	for (size_t tick = 0; tick < 64 && offset < layout.duration_ns; ++tick) {
		const float horizontal = float(double(offset) / layout.duration_ns * layout.width);
		if (horizontal > layout.width - 130)
			break;
		char label[64];
		std::snprintf(label, sizeof(label), seconds ? "| %.3fs" : "| %.2fms",
					  double(timeline_end(relative, offset)) / (seconds ? 1e9 : 1e6));
		decoration(
			layout,
			{horizontal, 0, std::min(100.0f, layout.width - horizontal), seconds ? 16.0f : 20.0f},
			interface_theme::kDepth1Panel, label);
		if (UINT64_MAX - offset < step)
			break;
		offset += step;
	}
	if (seconds && layout.duration_ns >= 500'000'000 && layout.duration_ns <= 20'000'000'000ULL) {
		constexpr uint64_t minor_step = 100'000'000;
		const auto minor_remainder = relative % minor_step;
		for (uint64_t minor_offset = minor_remainder ? minor_step - minor_remainder : 0;
			 minor_offset < layout.duration_ns; minor_offset += minor_step) {
			const float horizontal =
				float(double(minor_offset) / layout.duration_ns * layout.width);
			if ((relative % step + minor_offset % step) % step && horizontal < layout.width - 130)
				decoration(layout, {horizontal, 13, 1, 3}, interface_theme::kTextMuted);
		}
	}
	char endpoint[64];
	std::snprintf(endpoint, sizeof(endpoint), seconds ? "Elapsed %.3fs" : "%.2fms",
				  double(timeline_end(relative, layout.duration_ns)) / (seconds ? 1e9 : 1e6));
	decoration(layout,
			   {std::max(0.0f, layout.width - 130), 0, std::min(130.0f, layout.width),
				seconds ? 16.0f : 20.0f},
			   interface_theme::kDepth1Panel, endpoint);
}
std::string details(const TimelineBlockSlice& block, uint64_t origin) {
	return block.label + "\nInclusive " + timeline_ui::milliseconds(block.duration_ns) +
		   (block.domain == TimingSampleDomain::Gpu ? " · Approx. exclusive " : " · Exclusive ") +
		   timeline_ui::milliseconds(block.exclusive_ns) + "\nStart +" +
		   timeline_ui::milliseconds(block.start_ns >= origin ? block.start_ns - origin : 0) +
		   " · " + std::string(performance_category_names[size_t(block.category)]) + "\n" +
		   (block.domain == TimingSampleDomain::Cpu ? "CPU thread " : "GPU queue ") +
		   std::to_string(block.track) + " · Frame " + std::to_string(block.frame.frameNumber) +
		   " · AppTick " + std::to_string(block.app_tick);
}
} // namespace
TimelineLayout build_timeline_layout(const DevTimelineState& state,
									 const DevPerformanceSelection& selection,
									 TimelineSurfaceKind kind, size_t card_index, float width,
									 const std::vector<TimelineTrackLane>* cached_lanes) {
	TimelineLayout layout;
	layout.revision = state.snapshot_revision;
	layout.kind = kind;
	layout.width = std::max(0.0f, width);
	layout.start_ns =
		kind == TimelineSurfaceKind::Minimap ? state.snapshot.start_ns : state.visible_start_ns;
	layout.duration_ns =
		kind == TimelineSurfaceKind::Minimap
			? std::max(uint64_t{1}, state.snapshot.end_ns - state.snapshot.start_ns)
			: state.visible_duration_ns;
	layout.items.reserve((kind == TimelineSurfaceKind::Minimap ? 0 : state.snapshot.blocks.size()) +
						 state.snapshot.frames.size() + 64);
	if (kind == TimelineSurfaceKind::Minimap) {
		layout.height = 48;
		ruler(layout, state.origin_ns, true);
		uint64_t maximum = 33'333'334;
		for (const auto metric : state.snapshot.frame_metrics)
			maximum = std::max(maximum, metric);
		// One bin per logical pixel, preserving every frame for deterministic nearest-start
		// picking.
		const size_t bin_count = size_t(std::clamp(std::ceil(width), 1.0f, 16384.0f));
		std::vector<std::vector<size_t>> bins(bin_count);
		for (size_t index = 0; index < state.snapshot.frames.size(); ++index) {
			const auto& frame = state.snapshot.frames[index];
			if (frame.start_ns < layout.start_ns)
				continue;
			const auto bin =
				std::min(bin_count - 1, size_t(double(frame.start_ns - layout.start_ns) /
											   layout.duration_ns * bin_count));
			bins[bin].emplace_back(index);
		}
		for (size_t bin = 0; bin < bins.size(); ++bin) {
			if (bins[bin].empty())
				continue;
			uint64_t peak = 0;
			bool selected = false;
			for (const auto index : bins[bin]) {
				peak = std::max(peak, index < state.snapshot.frame_metrics.size()
										  ? state.snapshot.frame_metrics[index]
										  : state.snapshot.frames[index].duration_ns);
				selected |= index == state.selected_frame;
			}
			const float height = std::max(2.0f, float(double(peak) / maximum * 30));
			const float left = float(double(bin) / bin_count * width);
			size_t next_bin = bin + 1;
			while (next_bin < bins.size() && bins[next_bin].empty())
				++next_bin;
			const float right = float(double(next_bin) / bin_count * width);
			auto color = timeline_ui::frame_color(peak);
			if (selection.selector_mode == 1 && peak > state.snapshot.percentile_ns)
				color = Flow_Color("#EF4444");
			layout.items.emplace_back(TimelineDisplayItem{
				{},
				std::to_string(bins[bin].size()) + " frames · Peak " +
					timeline_ui::milliseconds(peak) +
					(selection.selector_mode == 1 ? " (selected-zone total)" : " (frame duration)"),
				{std::move(bins[bin]), TimelineAction::Center},
				{left, 48 - height, std::max(1.0f, right - left), height},
				color,
				selected});
		}
		for (const uint64_t target : {16'666'667ULL, 8'333'333ULL}) {
			const float top = 48 - float(double(target) / maximum * 30);
			for (float left = 0; left < width; left += target == 16'666'667 ? 10 : 5)
				decoration(
					layout,
					{left, top, std::min(target == 16'666'667 ? 6.0f : 1.0f, width - left), 1},
					interface_theme::kTextMuted);
		}
		if (auto bounds =
				timeline_project_interval(state.visible_start_ns, state.visible_duration_ns,
										  layout.start_ns, layout.duration_ns, width, 16, 32)) {
			decoration(layout, *bounds, {115, 213, 197, 25});
			layout.items.back().selected = true;
		}
		return layout;
	}
	std::span<const size_t> roots;
	uint32_t depth = state.active_depth;
	if (kind == TimelineSurfaceKind::Card) {
		if (card_index >= state.cards.size() || state.cards[card_index].roots.empty())
			return layout;
		roots = state.cards[card_index].roots;
		depth = state.cards[card_index].active_depth;
		layout.start_ns = UINT64_MAX;
		uint64_t end = 0;
		for (const auto index : roots) {
			const auto& block = state.snapshot.blocks[index];
			layout.start_ns = std::min(layout.start_ns, block.start_ns);
			end = std::max(end, timeline_end(block.start_ns, block.duration_ns));
		}
		layout.duration_ns = std::max(uint64_t{1}, end - layout.start_ns);
	}
	ruler(layout, kind == TimelineSurfaceKind::Card ? layout.start_ns : state.origin_ns, false);
	float top = 20;
	if (kind == TimelineSurfaceKind::Macro) {
		for (size_t index = 0; index < state.snapshot.frames.size(); ++index) {
			const auto& frame = state.snapshot.frames[index];
			if (auto bounds =
					timeline_project_interval(frame.start_ns, frame.duration_ns, layout.start_ns,
											  layout.duration_ns, width, top, 24))
				layout.items.emplace_back(TimelineDisplayItem{
					bounds->width >= 18
						? frame.label + " · " + timeline_ui::milliseconds(frame.duration_ns) +
							  (frame.duration_ns > 33'333'333 ? " !" : "")
						: std::string{},
					{},
					{{}, TimelineAction::Center, index},
					*bounds,
					timeline_ui::frame_color(frame.duration_ns),
					index == state.selected_frame});
		}
		top += 24;
	}
	auto lanes = cached_lanes ? *cached_lanes : timeline_lanes(state.snapshot, depth, roots);
	if (roots.size() > 1)
		lanes.insert(lanes.begin(),
					 TimelineTrackLane{std::vector<size_t>(roots.begin(), roots.end())});
	for (size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
		const auto& lane = lanes[lane_index];
		const bool members = roots.size() > 1 && lane_index == 0;
		if (!members &&
			((selection.hardware_domain == 1 && lane.domain != TimingSampleDomain::Cpu) ||
			 (selection.hardware_domain == 2 && lane.domain != TimingSampleDomain::Gpu)))
			continue;
		decoration(layout, {0, top, width, 18}, interface_theme::kDepth1Panel,
				   members ? "Cluster members"
						   : std::string(lane.domain == TimingSampleDomain::Cpu ? "CPU thread "
																				: "GPU queue ") +
								 std::to_string(lane.track) + " · Depth " +
								 std::to_string(lane.depth + 1));
		top += 18;
		auto clusters = cluster_timeline(state.snapshot, lane.blocks, layout.start_ns,
										 layout.duration_ns, width);
		if (members) {
			clusters.clear();
			for (const auto index : roots)
				clusters.emplace_back(TimelineCluster{{index},
													  state.snapshot.blocks[index].start_ns,
													  state.snapshot.blocks[index].duration_ns});
		}
		std::stable_partition(clusters.begin(), clusters.end(), [&](const auto& cluster) {
			return !state.snapshot.blocks[cluster.members.front()].selected;
		});
		float tail = 0;
		for (auto& cluster : clusters) {
			const auto& block = state.snapshot.blocks[cluster.members.front()];
			if (!(selection.category_mask & timingCategoryBit(block.category)))
				continue;
			auto bounds =
				timeline_project_interval(cluster.start_ns, cluster.duration_ns, layout.start_ns,
										  layout.duration_ns, width, top, 38);
			if (!bounds)
				continue;
			tail = std::max(tail, bounds->x + bounds->width);
			std::string label;
			if (bounds->width >= 18) {
				label = cluster.members.size() > 1
							? std::to_string(cluster.members.size()) + " micro zones"
							: block.label;
				label += " · " + timeline_ui::milliseconds(cluster.duration_ns);
				if (kind == TimelineSurfaceKind::Card) {
					char percent[32];
					std::snprintf(percent, sizeof(percent), " · %.1f%%",
								  100.0 * cluster.duration_ns / layout.duration_ns);
					label += percent;
				}
			}
			layout.items.emplace_back(TimelineDisplayItem{
				std::move(label),
				{},
				{std::move(cluster.members), TimelineAction::Open,
				 kind == TimelineSurfaceKind::Card ? card_index + 1 : 0},
				*bounds,
				timeline_ui::block_color(block, selection.selector_mode == 1 &&
													selection.selected_zone && !block.selected),
				block.selected});
		}
		if (width - tail > 100)
			decoration(layout, {tail, top, width - tail, 38}, interface_theme::kDepth0Keel,
					   "Unrecorded / self");
		top += 44;
	}
	layout.height = top;
	return layout;
}
std::string timeline_item_detail(const DevTimelineState& state, const TimelineLayout& layout,
								 size_t item_index) {
	if (layout.revision != state.snapshot_revision || item_index >= layout.items.size())
		return {};
	const auto& item = layout.items[item_index];
	if (!item.detail.empty())
		return item.detail;
	if (item.command.action == TimelineAction::Center &&
		item.command.index < state.snapshot.frames.size())
		return details(state.snapshot.frames[item.command.index], state.origin_ns);
	if (item.command.action != TimelineAction::Open || item.command.members.empty())
		return {};
	uint64_t start = UINT64_MAX, end = 0, sum = 0;
	for (const auto index : item.command.members) {
		if (index >= state.snapshot.blocks.size())
			return {};
		const auto& block = state.snapshot.blocks[index];
		start = std::min(start, block.start_ns);
		end = std::max(end, timeline_end(block.start_ns, block.duration_ns));
		sum = timeline_end(sum, block.duration_ns);
	}
	std::string detail;
	if (item.command.members.size() == 1)
		detail = details(state.snapshot.blocks[item.command.members.front()], layout.start_ns);
	else {
		detail = std::to_string(item.command.members.size()) + " micro zones\nEnvelope " +
				 timeline_ui::milliseconds(end - start) + " · Member sum " +
				 timeline_ui::milliseconds(sum);
		for (size_t preview = 0; preview < std::min(size_t{4}, item.command.members.size());
			 ++preview)
			detail += "\n" + state.snapshot.blocks[item.command.members[preview]].label;
	}
	if (layout.kind == TimelineSurfaceKind::Card) {
		char percent[48];
		std::snprintf(percent, sizeof(percent), "\n%.1f%% of card interval",
					  100.0 * (end - start) / layout.duration_ns);
		detail += percent;
	}
	size_t overlaps = 0;
	for (const auto& candidate : layout.items)
		if (candidate.command.action == TimelineAction::Open &&
			candidate.bounds.y == item.bounds.y &&
			candidate.bounds.x < item.bounds.x + item.bounds.width &&
			candidate.bounds.x + candidate.bounds.width > item.bounds.x)
			++overlaps;
	if (overlaps > 1)
		detail += "\nOverlapping samples: " + std::to_string(overlaps);
	return detail;
}

} // namespace FlowUi::devSystems::interface_elements
#endif
