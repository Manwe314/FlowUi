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
	if ((sample_duration ? clipped_end <= clipped_start
						 : sample_start < view_start ||
							   sample_start >= timeline_end(view_start, view_duration)) ||
		width <= 0 || height <= 0 || !std::isfinite(width))
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
	const float axis_width = std::max(0.0f, layout.width - layout.label_width);
	const auto relative = layout.start_ns >= origin ? layout.start_ns - origin : 0;
	const double requested = double(layout.duration_ns) / std::max(1.0, double(axis_width) / 100);
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
		const float horizontal = float(double(offset) / layout.duration_ns * axis_width);
		if (horizontal >= axis_width - 130)
			break;
		char label[64];
		const auto timestamp = timeline_end(relative, offset);
		if (step >= 1'000'000'000)
			std::snprintf(label, sizeof(label), "| %.3fs", double(timestamp) / 1e9);
		else if (step >= 1'000'000)
			std::snprintf(label, sizeof(label), "| %.2fms", double(timestamp) / 1e6);
		else if (step >= 1'000)
			std::snprintf(label, sizeof(label), "| %.2fus", double(timestamp) / 1e3);
		else
			std::snprintf(label, sizeof(label), "| %llu ns",
						  static_cast<unsigned long long>(timestamp));
		decoration(layout,
				   {layout.label_width + horizontal, 0,
					std::min(float(double(step) / layout.duration_ns * axis_width),
							 axis_width - horizontal - 130),
					seconds ? 16.0f : 20.0f},
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
			const float horizontal = float(double(minor_offset) / layout.duration_ns * axis_width);
			if ((relative % step + minor_offset % step) % step && horizontal < axis_width - 130)
				decoration(layout, {layout.label_width + horizontal, 13, 1, 3},
						   interface_theme::kTextMuted);
		}
	}
	char endpoint[64];
	const auto endpoint_ns = timeline_end(relative, layout.duration_ns);
	if (seconds)
		std::snprintf(endpoint, sizeof(endpoint), "Elapsed %.3fs", double(endpoint_ns) / 1e9);
	else if (step >= 1'000'000'000)
		std::snprintf(endpoint, sizeof(endpoint), "%.3fs", double(endpoint_ns) / 1e9);
	else if (step >= 1'000'000)
		std::snprintf(endpoint, sizeof(endpoint), "%.2fms", double(endpoint_ns) / 1e6);
	else if (step >= 1'000)
		std::snprintf(endpoint, sizeof(endpoint), "%.2fus", double(endpoint_ns) / 1e3);
	else
		std::snprintf(endpoint, sizeof(endpoint), "%llu ns",
					  static_cast<unsigned long long>(endpoint_ns));
	decoration(layout,
			   {layout.label_width + std::max(0.0f, axis_width - 130), 0,
				std::min(130.0f, axis_width), seconds ? 16.0f : 20.0f},
			   interface_theme::kDepth1Panel, endpoint);
}
std::string track_name(const TimelineBlockSlice& block, const TimelineSnapshot& snapshot) {
	if (block.domain == TimingSampleDomain::Cpu)
		return "CPU " + std::to_string(block.track);
	size_t queue_number = 1;
	std::vector<std::pair<uint64_t, uint64_t>> queues;
	for (const auto& sample : snapshot.blocks) {
		if (sample.domain != TimingSampleDomain::Gpu)
			continue;
		const auto identity = std::pair{
			sample.device_identity, sample.queue_identity ? sample.queue_identity : sample.track};
		if (std::ranges::find(queues, identity) != queues.end())
			continue;
		if (sample.device_identity == block.device_identity &&
			identity.second == (block.queue_identity ? block.queue_identity : block.track))
			break;
		queues.emplace_back(identity);
		++queue_number;
	}
	return "GPU queue " + std::to_string(queue_number);
}
std::string details(const TimelineBlockSlice& block, uint64_t origin) {
	if (block.synthetic_tick)
		return block.label + "\n" +
			   (block.measured_cadence ? "Tick cadence " : "Recorded CPU envelope ") +
			   timeline_ui::milliseconds(block.duration_ns) +
			   "\nSynthetic grouping; related activity is associated, not contained";
	return block.label + "\nInclusive " + timeline_ui::milliseconds(block.duration_ns) +
		   (block.domain == TimingSampleDomain::Gpu
				? std::string{}
				: " · Recorded exclusive " + timeline_ui::milliseconds(block.exclusive_ns)) +
		   "\nStart +" +
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
			? std::max(uint64_t{1}, uint64_t((state.snapshot.end_ns - state.snapshot.start_ns) /
											 state.minimap_zoom))
			: state.visible_duration_ns;
	if (kind == TimelineSurfaceKind::Minimap) {
		const auto history = state.snapshot.end_ns - state.snapshot.start_ns;
		const auto center =
			!state.paused ? state.snapshot.end_ns
			: state.minimap_follow_selection && state.selected_frame < state.snapshot.frames.size()
				? timeline_end(state.snapshot.frames[state.selected_frame].start_ns,
							   state.snapshot.frames[state.selected_frame].duration_ns / 2)
				: timeline_end(state.visible_start_ns, state.visible_duration_ns / 2);
		const auto latest = state.snapshot.end_ns - std::min(history, layout.duration_ns);
		layout.start_ns =
			std::clamp(center > layout.duration_ns / 2 ? center - layout.duration_ns / 2 : 0,
					   state.snapshot.start_ns, std::max(state.snapshot.start_ns, latest));
	}
	layout.items.reserve((kind == TimelineSurfaceKind::Minimap ? 0 : state.snapshot.blocks.size()) +
						 state.snapshot.frames.size() + 64);
	if (kind == TimelineSurfaceKind::Minimap) {
		layout.height = 96;
		layout.ruler_height = 16;
		ruler(layout, state.origin_ns, true);
		for (auto& item : layout.items)
			item.screen_space = true;
		uint64_t maximum = 33'333'334;
		for (const auto metric : state.snapshot.frame_metrics)
			maximum = std::max(maximum, metric);
		// One bin per logical pixel, preserving every frame for deterministic nearest-start
		// picking.
		const size_t bin_count = size_t(std::clamp(std::ceil(width), 1.0f, 16384.0f));
		std::vector<std::vector<size_t>> bins(bin_count);
		for (size_t index = 0; index < state.snapshot.frames.size(); ++index) {
			const auto& frame = state.snapshot.frames[index];
			if (frame.start_ns < layout.start_ns ||
				frame.start_ns >= timeline_end(layout.start_ns, layout.duration_ns))
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
			const float height = std::max(2.0f, float(double(peak) / maximum * 78));
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
				std::to_string(bins[bin].size()) +
					(state.snapshot.window_milestones ? " window frames · Peak "
													  : " app ticks · Peak ") +
					timeline_ui::milliseconds(peak) +
					(selection.selector_mode == 1		? " (sum of matching recorded durations)"
					 : state.snapshot.window_milestones ? " (CPU frame duration)"
					 : state.snapshot.tick_cadence		? " (tick cadence)"
														: " (recorded CPU envelope)"),
				{std::move(bins[bin]), TimelineAction::Center},
				{left, 96 - height, std::max(1.0f, right - left), height},
				color,
				selected});
		}
		for (const uint64_t target : {16'666'667ULL, 8'333'333ULL}) {
			const float top = 96 - float(double(target) / maximum * 78);
			for (float left = 0; left < width; left += target == 16'666'667 ? 10 : 5)
				decoration(
					layout,
					{left, top, std::min(target == 16'666'667 ? 6.0f : 1.0f, width - left), 1},
					interface_theme::kTextMuted);
		}
		if (auto bounds =
				timeline_project_interval(state.visible_start_ns, state.visible_duration_ns,
										  layout.start_ns, layout.duration_ns, width, 16, 80)) {
			decoration(layout, *bounds, {115, 213, 197, 25});
			layout.items.back().selected = true;
		}
		return layout;
	}
	std::span<const size_t> roots;
	uint64_t root_origin = 0;
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
		root_origin = layout.start_ns;
		if (state.cards[card_index].visible_duration_ns) {
			layout.start_ns = state.cards[card_index].visible_start_ns;
			layout.duration_ns = state.cards[card_index].visible_duration_ns;
		}
	}
	layout.label_width = std::min(150.0f, width * .25f);
	const float plot_width = std::max(0.0f, width - layout.label_width);
	layout.ruler_height = kind == TimelineSurfaceKind::Card ? 40 : 20;
	decoration(layout, {0, 0, width, layout.ruler_height}, interface_theme::kDepth1Panel);
	ruler(layout,
		  kind == TimelineSurfaceKind::Card &&
				  !state.snapshot.blocks[roots.front()].cpu_clock_aligned
			  ? root_origin
			  : state.origin_ns,
		  false);
	decoration(layout, {0, 0, layout.label_width, 20}, interface_theme::kDepth1Panel,
			   kind == TimelineSurfaceKind::Card &&
					   !state.snapshot.blocks[roots.front()].cpu_clock_aligned
				   ? "GPU local time"
				   : "Capture time");
	for (auto& item : layout.items)
		item.screen_space = true;
	if (kind == TimelineSurfaceKind::Card) {
		TimelineLayout relative = layout;
		relative.items.clear();
		ruler(relative, root_origin, false);
		decoration(relative, {0, 0, layout.label_width, 20}, interface_theme::kDepth1Panel,
				   "Root offset");
		for (auto& item : relative.items) {
			item.bounds.y += 20;
			item.screen_space = true;
			layout.items.emplace_back(std::move(item));
		}
	}
	float top = layout.ruler_height;
	const auto append_block = [&](size_t block_index, float row_top, TimelineAction action) {
		const auto& block = state.snapshot.blocks[block_index];
		if (!(selection.category_mask & timingCategoryBit(block.category)))
			return;
		auto bounds = timeline_project_interval(block.start_ns, block.duration_ns, layout.start_ns,
												layout.duration_ns, plot_width, row_top, 32);
		if (!bounds)
			return;
		bounds->x += layout.label_width;
		layout.items.emplace_back(TimelineDisplayItem{
			double(block.duration_ns) / layout.duration_ns * plot_width >= 80
				? block.label + " · " + timeline_ui::milliseconds(block.duration_ns)
			: double(block.duration_ns) / layout.duration_ns * plot_width >= 32 ? block.label
																				: std::string{},
			{},
			{{block_index}, action, kind == TimelineSurfaceKind::Card ? card_index + 1 : 0},
			*bounds,
			timeline_ui::block_color(
				block,
				!(selection.category_mask & timingCategoryBit(block.category)) ||
					(selection.selector_mode == 1 && selection.selected_zone && !block.selected)),
			block_index == state.inspected_sample});
		auto& item = layout.items.back();
		item.text_offset_x =
			block.start_ns < layout.start_ns
				? -float(double(layout.start_ns - block.start_ns) / layout.duration_ns * plot_width)
				: 0;
		if (double(block.duration_ns) / layout.duration_ns * plot_width <= 3) {
			item.label.clear();
			item.bounds.width = std::min(2.0f, width - item.bounds.x);
			item.color = Flow_Color("#67E8F9");
		}
		if (kind == TimelineSurfaceKind::Macro && !state.cards.empty() &&
			std::ranges::find(state.cards.front().roots, block_index) !=
				state.cards.front().roots.end())
			layout.items.back().focused_root = true;
	};
	if (kind == TimelineSurfaceKind::Card) {
		for (const auto root : roots) {
			append_block(root, top, TimelineAction::Inspect);
			if (!layout.items.empty() &&
				layout.items.back().command.members == std::vector<size_t>{root})
				layout.items.back().focused_root = true;
			decoration(layout, {0, top, layout.label_width, 32}, interface_theme::kDepth1Panel,
					   roots.size() > 1 ? "Aggregate member" : "Selected root");
			top += 33;
		}
	}
	if (kind == TimelineSurfaceKind::Card && roots.size() > 1) {
		layout.height = top;
		return layout;
	}
	const auto generated_lanes =
		cached_lanes ? std::vector<TimelineTrackLane>{}
		: kind == TimelineSurfaceKind::Macro
			? timeline_major_lanes(state.snapshot, selection, state.track_preferences)
			: timeline_lanes(state.snapshot, depth, roots);
	const auto& lanes = cached_lanes ? *cached_lanes : generated_lanes;
	for (const auto& lane : lanes) {
		if ((selection.hardware_domain == 1 && lane.domain != TimingSampleDomain::Cpu) ||
			(selection.hardware_domain == 2 && lane.domain != TimingSampleDomain::Gpu))
			continue;
		if (kind == TimelineSurfaceKind::Macro) {
			std::vector<size_t> visible_blocks;
			visible_blocks.reserve(lane.blocks.size());
			for (const auto block_index : lane.blocks)
				if (selection.category_mask &
					timingCategoryBit(state.snapshot.blocks[block_index].category))
					visible_blocks.emplace_back(block_index);
			auto clusters = cluster_timeline(
				state.snapshot, visible_blocks, layout.start_ns, layout.duration_ns, plot_width,
				state.inspected_sample,
				state.cards.empty() ? std::span<const size_t>{}
									: std::span<const size_t>(state.cards.front().roots));
			for (auto& cluster : clusters) {
				if (cluster.members.size() == 1)
					append_block(cluster.members.front(), top, TimelineAction::Open);
				else if (auto bounds = timeline_project_interval(
							 cluster.start_ns, cluster.duration_ns, layout.start_ns,
							 layout.duration_ns, plot_width, top, 32)) {
					bounds->x += layout.label_width;
					layout.items.emplace_back(
						TimelineDisplayItem{std::to_string(cluster.members.size()) + " roots",
											{},
											{std::move(cluster.members), TimelineAction::Open, 0},
											*bounds,
											Flow_Color("#3B82F6")});
				}
			}
		} else {
			std::vector<size_t> visible_blocks;
			visible_blocks.reserve(lane.blocks.size());
			for (const auto block_index : lane.blocks)
				if (selection.category_mask &
					timingCategoryBit(state.snapshot.blocks[block_index].category))
					visible_blocks.emplace_back(block_index);
			auto clusters = cluster_timeline(
				state.snapshot, visible_blocks, layout.start_ns, layout.duration_ns, plot_width,
				state.inspected_sample,
				state.cards.empty() ? std::span<const size_t>{}
									: std::span<const size_t>(state.cards.front().roots));
			for (auto& cluster : clusters) {
				if (cluster.members.size() == 1) {
					append_block(cluster.members.front(), top, TimelineAction::Inspect);
				} else if (auto bounds = timeline_project_interval(
							   cluster.start_ns, cluster.duration_ns, layout.start_ns,
							   layout.duration_ns, plot_width, top, 32)) {
					bounds->x += layout.label_width;
					layout.items.emplace_back(TimelineDisplayItem{
						std::to_string(cluster.members.size()) + " micro zones",
						{},
						{std::move(cluster.members), TimelineAction::Open, card_index + 1},
						*bounds,
						Flow_Color("#3B82F6")});
				}
			}
		}
		const auto label =
			(lane.window ? "Window " + std::to_string(lane.window) + " / "
			 : lane.domain == TimingSampleDomain::Cpu && kind == TimelineSurfaceKind::Macro
				 ? "Application / "
				 : "") +
			track_name(state.snapshot.blocks[lane.blocks.front()], state.snapshot) +
			(kind == TimelineSurfaceKind::Card ? " / depth "
			 : lane.depth					   ? " / overlap "
											   : "") +
			(kind == TimelineSurfaceKind::Card || lane.depth ? std::to_string(lane.depth + 1) : "");
		decoration(layout, {0, top, layout.label_width, 32}, interface_theme::kDepth1Panel, label);
		decoration(layout, {0, top + 32, width, 1}, interface_theme::kTextMuted);
		top += 33;
	}
	if (kind == TimelineSurfaceKind::Card && roots.size() == 1 &&
		state.snapshot.blocks[roots.front()].synthetic_tick) {
		const auto& tick = state.snapshot.blocks[roots.front()];
		for (size_t sample_index = 0; sample_index < state.snapshot.blocks.size(); ++sample_index) {
			const auto& sample = state.snapshot.blocks[sample_index];
			if (!(selection.category_mask & timingCategoryBit(sample.category)) ||
				sample.synthetic_tick || !sample.scope_visible || !sample.cpu_clock_aligned ||
				(selection.hardware_domain == 1 && sample.domain != TimingSampleDomain::Cpu) ||
				(selection.hardware_domain == 2 && sample.domain != TimingSampleDomain::Gpu) ||
				sample.app_tick != tick.app_tick ||
				(sample.parent != timeline_no_parent &&
				 sample.type_id != timing_zones::kWindowFrameTotal.typeId))
				continue;
			const auto sample_end = timeline_end(sample.start_ns, sample.duration_ns);
			if (sample_end <= layout.start_ns ||
				sample.start_ns >= timeline_end(layout.start_ns, layout.duration_ns))
				layout.items.emplace_back(TimelineDisplayItem{
					(sample_end <= layout.start_ns ? "Earlier: " : "Later: ") + sample.label,
					"Associated AppTick activity; outside cadence interval",
					{{sample_index}, TimelineAction::Inspect},
					{layout.label_width, top, plot_width, 32},
					Flow_Color("#3B82F6")});
			else
				append_block(sample_index, top, TimelineAction::Inspect);
			decoration(layout, {0, top, layout.label_width, 32}, interface_theme::kDepth1Panel,
					   sample.domain == TimingSampleDomain::Cpu ? "Associated CPU"
																: "Associated GPU");
			decoration(layout, {0, top + 32, width, 1}, interface_theme::kTextMuted);
			top += 33;
		}
	}
	if (kind == TimelineSurfaceKind::Card && roots.size() == 1 &&
		state.snapshot.blocks[roots.front()].type_id == timing_zones::kWindowFrameTotal.typeId &&
		selection.hardware_domain != 1) {
		const auto& root = state.snapshot.blocks[roots.front()];
		for (size_t sample_index = 0; sample_index < state.snapshot.blocks.size(); ++sample_index) {
			const auto& sample = state.snapshot.blocks[sample_index];
			if (!(selection.category_mask & timingCategoryBit(sample.category)) ||
				sample.domain != TimingSampleDomain::Gpu || !sample.cpu_clock_aligned ||
				sample.frame != root.frame || sample.parent != timeline_no_parent)
				continue;
			const auto sample_end = timeline_end(sample.start_ns, sample.duration_ns);
			if (sample_end <= layout.start_ns ||
				sample.start_ns >= timeline_end(layout.start_ns, layout.duration_ns)) {
				layout.items.emplace_back(TimelineDisplayItem{
					(sample_end <= layout.start_ns ? "Earlier: " : "Later: ") + sample.label,
					"Associated GPU frame work; outside selected interval",
					{{sample_index}, TimelineAction::Inspect},
					{layout.label_width, top, plot_width, 32},
					Flow_Color("#3B82F6")});
			} else {
				append_block(sample_index, top, TimelineAction::Inspect);
				if (sample.start_ns < layout.start_ns)
					decoration(layout, {layout.label_width, top, 3, 32},
							   interface_theme::kAccentSeaGlass, "<");
				if (sample_end > timeline_end(layout.start_ns, layout.duration_ns))
					decoration(layout, {width - 3, top, 3, 32}, interface_theme::kAccentSeaGlass,
							   ">");
			}
			decoration(layout, {0, top, layout.label_width, 32}, interface_theme::kDepth1Panel,
					   "Associated GPU");
			top += 33;
		}
	}
	if (kind == TimelineSurfaceKind::Macro && selection.hardware_domain != 1) {
		for (size_t sample_index = 0; sample_index < state.snapshot.blocks.size(); ++sample_index) {
			const auto& sample = state.snapshot.blocks[sample_index];
			if (!(selection.category_mask & timingCategoryBit(sample.category)) ||
				sample.cpu_clock_aligned || !sample.scope_visible ||
				sample.parent != timeline_no_parent)
				continue;
			layout.items.emplace_back(TimelineDisplayItem{"Unaligned GPU: " + sample.label,
														  "Open in GPU local time",
														  {{sample_index}, TimelineAction::Open},
														  {layout.label_width, top, plot_width, 32},
														  Flow_Color("#3B82F6")});
			decoration(layout, {0, top, layout.label_width, 32}, interface_theme::kDepth1Panel,
					   "GPU local picker");
			top += 33;
		}
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
	if ((item.command.action != TimelineAction::Open &&
		 item.command.action != TimelineAction::Inspect) ||
		item.command.members.empty())
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
	if (item.command.members.size() == 1 &&
		!(state.selection.category_mask &
		  timingCategoryBit(state.snapshot.blocks[item.command.members.front()].category)))
		detail += "\nCategory-filtered structural context";
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
