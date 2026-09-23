#include "devSystems/devInterface/Performance/Workbench/DevTimelineElements.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cmath>
namespace FlowUi::devSystems::interface_elements {
namespace interaction = FSEL::detail::selectable_surface;
void DevTimelineButton::onPressed(InteractionContext& context) {
	interaction::onPressed(context, true);
}
void DevTimelineButton::runLogic(InteractionContext& context) {
	interaction::runLogic(context, true);
}
void DevTimelineButton::onReleased(InteractionContext& context) {
	if (interaction::onReleased(context, true) && context.params.timeline)
		context.params.timeline->pending = context.params.command;
}
void DevTimelineButton::onHovered(InteractionContext& context) {
	context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
void DevTimelineButton::buildElement(BuildContext& context) {
	auto root = timeline_ui::row(context.params.height);
	root.layout.sizing.width =
		context.params.width > 0 ? CLAY_SIZING_FIXED(context.params.width) : CLAY_SIZING_FIT(0);
	root.layout.padding = context.params.width > 0 && context.params.width < 12
							  ? Clay_Padding{}
							  : Clay_Padding{4, 4, 0, 0};
	root.backgroundColor = context.params.color;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	if (context.params.highlighted ||
		context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
		root.border = {.color = interface_theme::kAccentSeaGlass,
					   .width = Clay_BorderWidth{1, 1, 1, 1, 0}};
	CLAY(context.clayID(), root) {
		timeline_ui::text(context, context.params.label, interface_theme::kTextCanvas);
	}
}
namespace timeline_ui {
Clay_Color frame_color(uint64_t metric) noexcept {
	const float time_ms = float(metric / 1e6);
	Clay_Color low{}, high{};
	float mix = 0;
	if (time_ms <= 16.667f) {
		low = Flow_Color("#0f444c");
		high = Flow_Color("#18B8A6");
		mix = time_ms / 16.667f;
	} else if (time_ms <= 33.333f) {
		low = Flow_Color("#4d3209");
		high = Flow_Color("#F59E0B");
		mix = (time_ms - 16.667f) / 16.666f;
	} else {
		low = Flow_Color("#EF4444");
		high = Flow_Color("#4c1414");
		mix = std::min(1.0f, (time_ms - 33.333f) / 100);
	}
	return {low.r + (high.r - low.r) * mix, low.g + (high.g - low.g) * mix,
			low.b + (high.b - low.b) * mix, 255};
}
Clay_Color block_color(const TimelineBlockSlice& block, bool ghost) noexcept {
	Clay_Color color = block.domain == TimingSampleDomain::Gpu	   ? Flow_Color("#9562B8")
					   : block.category == TimingCategory::Element ? Flow_Color("#6254A2")
					   : block.category == TimingCategory::Wait	   ? Flow_Color("#8F6934")
					   : block.category == TimingCategory::User	   ? Flow_Color("#278354")
																   : Flow_Color("#146E78");
	if (ghost) {
		color.r *= .25f;
		color.g *= .25f;
		color.b *= .25f;
	}
	return color;
}
void ruler(UiManager& manager, Clay_ElementId id, uint64_t start, uint64_t duration, bool seconds) {
	auto root = row(seconds ? 16 : 20);
	root.layout.childGap = 0;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	const auto measured = Clay_GetElementData(id);
	const float width = measured.found ? measured.boundingBox.width : 600;
	const double range = double(std::max(uint64_t{1}, duration));
	const double requested_step = range / std::max(1.0, double(width) / 100.0);
	const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(1.0, requested_step))));
	const double multiple = requested_step / magnitude;
	const double step =
		seconds && duration >= 500'000'000
			? (requested_step <= 500'000'000 ? 500'000'000 : std::ceil(requested_step / 1e9) * 1e9)
			: magnitude * (multiple <= 1   ? 1
						   : multiple <= 2 ? 2
						   : multiple <= 5 ? 5
										   : 10);
	const double first_offset = std::ceil(double(start) / step) * step - double(start);
	CLAY(id, root) {
		const auto draw_tick = [&](uint32_t index, double offset, bool endpoint, bool minor) {
			Clay_ElementDeclaration segment{};
			segment.layout.sizing = {.width = CLAY_SIZING_FIT(0),
									 .height = CLAY_SIZING_FIXED(seconds ? 16.0f : 20.0f)};
			segment.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			segment.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			segment.floating.offset.x = float(offset / range * width);
			if (endpoint) {
				segment.floating.attachPoints.element = CLAY_ATTACH_POINT_RIGHT_TOP;
				segment.backgroundColor = interface_theme::kDepth1Panel;
			}
			CLAY(Clay_GetElementIdWithIndex(CLAY_STRING("timeline.ruler.tick"), id.id + index),
				 segment) {
				char label[72];
				if (minor)
					std::snprintf(label, sizeof(label), ".");
				else if (seconds)
					std::snprintf(label, sizeof(label),
								  duration < 500'000'000 ? (endpoint ? "Elapsed %.3fs" : "| %.3fs")
														 : (endpoint ? "Elapsed %.1fs" : "| %.1fs"),
								  (double(start) + offset) / 1e9);
				else
					std::snprintf(label, sizeof(label), endpoint ? "%.2fms" : "| %.2fms",
								  (double(start) + offset) / 1e6);
				Clay_TextElementConfig config{};
				config.fontSize = 10;
				config.textColor = interface_theme::kTextSecondary;
				config.wrapMode = CLAY_TEXT_WRAP_NONE;
				CLAY_TEXT(manager.toClayString(label), CLAY_TEXT_CONFIG(config));
			}
		};
		uint32_t tick_index = 0;
		for (double offset = first_offset; offset < range && tick_index < 64; offset += step)
			draw_tick(tick_index++, offset, false, false);
		if (seconds && duration >= 500'000'000 && duration <= 20'000'000'000ULL) {
			constexpr double minor_step = 100'000'000;
			for (double offset = std::ceil(double(start) / minor_step) * minor_step - double(start);
				 offset < range && tick_index < 256; offset += minor_step)
				if (std::fmod(double(start) + offset, step) > .5)
					draw_tick(tick_index++, offset, false, true);
		}
		draw_tick(256, range, true, false);
	}
}
void lane(UiManager& manager, Clay_ElementId id, DevTimelineParameters parameters,
		  std::span<const size_t> blocks, uint64_t start, uint64_t duration, float width,
		  size_t chain_index, bool cluster_small) {
	auto root = row(44);
	root.layout.childGap = 0;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	root.backgroundColor = interface_theme::kDepth0Keel;
	auto clusters = cluster_timeline(parameters.timeline->snapshot, blocks, start, duration, width);
	if (!cluster_small) {
		clusters.clear();
		clusters.reserve(blocks.size());
		for (const auto index : blocks) {
			const auto& block = parameters.timeline->snapshot.blocks[index];
			clusters.emplace_back(TimelineCluster{{index}, block.start_ns, block.duration_ns});
		}
	}
	const auto& selection = *parameters.selection;
	const double scale = width / double(std::max(uint64_t{1}, duration));
	float cursor = 0;
	CLAY(id, root) {
		for (size_t cluster_index = 0; cluster_index < clusters.size(); ++cluster_index) {
			auto& cluster = clusters[cluster_index];
			const auto& block = parameters.timeline->snapshot.blocks[cluster.members.front()];
			if (!(selection.category_mask & timingCategoryBit(block.category)))
				continue;
			const uint64_t clipped_start = std::max(start, cluster.start_ns);
			const uint64_t clipped_end = std::min(
				timeline_end(start, duration), timeline_end(cluster.start_ns, cluster.duration_ns));
			const float offset = float((clipped_start - start) * scale);
			const float block_width = std::min(
				width - offset, std::max(1.0f, float((clipped_end - clipped_start) * scale)));
			if (offset + 0.01f < cursor || block_width <= 0)
				continue;
			Clay_ElementDeclaration spacer{};
			spacer.layout.sizing = {.width = CLAY_SIZING_FIXED(std::max(0.0f, offset - cursor)),
									.height = CLAY_SIZING_FIXED(1)};
			CLAY(Clay_GetElementIdWithIndex(CLAY_STRING("timeline.lane.gap"),
											id.id + uint32_t(cluster_index)),
				 spacer);
			std::string label = cluster.members.size() > 1
									? std::to_string(cluster.members.size()) + " micro zones"
									: block.label;
			label += " · " + milliseconds(cluster.duration_ns);
			if (chain_index) {
				char percentage[32];
				std::snprintf(percentage, sizeof(percentage), " · %.1f%%",
							  100.0 * cluster.duration_ns / std::max(uint64_t{1}, duration));
				label += percentage;
			}
			const auto color = block_color(block, selection.selector_mode == 1 &&
													  selection.selected_zone && !block.selected);
			TimelineCommand command{cluster.members, TimelineAction::Open, chain_index};
			manager
				.createElement(kDevTimelineButton,
							   Keyed("sample", (uint64_t(id.id) << 32) | cluster_index))
				.setParameters(DevTimelineButtonParameters{std::move(command), std::move(label),
														   parameters.timeline, color, block_width,
														   38, block.selected})
				.setDevInternalCapture(true)
				.draw();
			cursor = offset + block_width;
		}
		if (cursor < width) {
			Clay_ElementDeclaration gap{};
			gap.layout.sizing = {.width = CLAY_SIZING_FIXED(width - cursor),
								 .height = CLAY_SIZING_FIXED(38)};
			CLAY(Clay_GetElementIdWithIndex(CLAY_STRING("timeline.lane.tail"), id.id), gap) {
				if (width - cursor > 65) {
					Clay_TextElementConfig config{};
					config.fontSize = 10;
					config.textColor = interface_theme::kTextMuted;
					CLAY_TEXT(manager.toClayString("Unrecorded / self"), CLAY_TEXT_CONFIG(config));
				}
			}
		}
	}
}
} // namespace timeline_ui
} // namespace FlowUi::devSystems::interface_elements
#endif
