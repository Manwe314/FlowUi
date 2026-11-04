#include "devSystems/devAgenticWorkflow/AgenticInteractionTargets.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "FlowUi/App.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] bool valid_region(const AgenticInteractionRegion& region) noexcept {
	return std::isfinite(region.x) && std::isfinite(region.y) && std::isfinite(region.width) &&
		   std::isfinite(region.height) && region.width > 0 && region.height > 0 &&
		   std::isfinite(region.x + region.width) && std::isfinite(region.y + region.height);
}
[[nodiscard]] AgenticInteractionRegion intersection(const AgenticInteractionRegion& first,
													const AgenticInteractionRegion& second,
													bool horizontal = true,
													bool vertical = true) noexcept {
	const double left = horizontal ? std::max(first.x, second.x) : first.x;
	const double top = vertical ? std::max(first.y, second.y) : first.y;
	const double right = horizontal ? std::min(first.x + first.width, second.x + second.width)
									: first.x + first.width;
	const double bottom = vertical ? std::min(first.y + first.height, second.y + second.height)
								   : first.y + first.height;
	return {left, top, right - left, bottom - top};
}
[[nodiscard]] bool contains(const AgenticInteractionRegion& region,
							AgenticInteractionPoint point) noexcept {
	return point.x >= region.x && point.y >= region.y && point.x < region.x + region.width &&
		   point.y < region.y + region.height;
}
[[nodiscard]] bool valid_input_point(const AgenticResolvedInteractionTarget& target) noexcept {
	const auto limit = static_cast<double>(std::numeric_limits<float>::max());
	return std::isfinite(target.layout_point.x) && std::isfinite(target.layout_point.y) &&
		   std::isfinite(target.window_point.x) && std::isfinite(target.window_point.y) &&
		   std::abs(target.window_point.x) <= limit && std::abs(target.window_point.y) <= limit;
}
[[nodiscard]] Json region_json(const AgenticInteractionRegion& region) {
	return Json::make_object({{"x", Json(region.x)},
							  {"y", Json(region.y)},
							  {"width", Json(region.width)},
							  {"height", Json(region.height)}});
}
[[nodiscard]] bool has_id(const std::vector<std::uint32_t>& ids, std::uint64_t clay_id) noexcept {
	return clay_id && std::find(ids.begin(), ids.end(), clay_id) != ids.end();
}
[[nodiscard]] Result<AgenticResolvedInteractionTarget> resolve_error(AgenticError error) noexcept {
	return unexpectedError(agentic_error(error));
}
#if FLOW_UI_DEV_CAPTURE_CLAY
[[nodiscard]] AgenticInteractionRegion bounds_of(const tooling::DevClayNode& node) noexcept {
	return {node.bounds.x, node.bounds.y, node.bounds.width, node.bounds.height};
}
[[nodiscard]] Result<AgenticInteractionRegion>
clipped_region(const tooling::DevTreeSnapshot& tree, const tooling::DevClayNode& node) noexcept {
	auto visible = bounds_of(node);
	if (!valid_region(visible))
		return unexpectedError(agentic_error(AgenticError::invalid_scope));
	std::uint32_t clip_id = node.clipClayId;
	std::array<std::uint32_t, 64> visited{};
	std::size_t visited_count = 0;
	while (clip_id) {
		if (visited_count >= visited.size() ||
			std::find(visited.begin(), visited.begin() + visited_count, clip_id) !=
				visited.begin() + visited_count)
			return unexpectedError(agentic_error(AgenticError::incomplete));
		visited[visited_count++] = clip_id;
		const tooling::DevClayNode* clip = nullptr;
		for (const auto& candidate : tree.clay.nodes)
			if (candidate.clayId == clip_id) {
				if (clip)
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				clip = &candidate;
			}
		if (!clip || !valid_region(bounds_of(*clip)))
			return unexpectedError(agentic_error(AgenticError::incomplete));
		visible = intersection(visible, bounds_of(*clip), clip->declaration.clip.horizontal,
							   clip->declaration.clip.vertical);
		if (!valid_region(visible))
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		clip_id = clip->clipClayId;
	}
	return visible;
}
#endif
} // namespace
AgenticInteractionTargetRegistry::AgenticInteractionTargetRegistry(
	std::size_t maximum_targets) noexcept
	: maximum_targets_(std::min<std::size_t>(maximum_targets, 65536)) {}
Status
AgenticInteractionTargetRegistry::register_target(AgenticCustomInteractionTarget target) noexcept {
	try {
		if (!valid_identifier(target.target_id) || !target.window_id || !target.revision ||
			!valid_region(target.bounds) || target.label.size() > 4096 ||
			target.clips.size() > 64 ||
			(target.expires_after_window_frame &&
			 target.expires_after_window_frame < target.window_frame))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		for (const auto& clip : target.clips)
			if (!valid_region(clip))
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		for (auto& previous : targets_)
			if (previous.target_id == target.target_id) {
				if (target.revision <= previous.revision)
					return unexpectedError(agentic_error(AgenticError::stale_generation));
				previous = std::move(target);
				return {};
			}
		if (targets_.size() >= maximum_targets_)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		targets_.emplace_back(std::move(target));
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Status AgenticInteractionTargetRegistry::unregister_target(std::string_view target_id,
														   std::uint64_t revision) noexcept {
	for (auto target = targets_.begin(); target != targets_.end(); ++target)
		if (target->target_id == target_id) {
			if (target->revision != revision)
				return unexpectedError(agentic_error(AgenticError::stale_generation));
			targets_.erase(target);
			return {};
		}
	return unexpectedError(agentic_error(AgenticError::unavailable));
}
Result<AgenticResolvedInteractionTarget>
AgenticInteractionTargetRegistry::resolve(App& application, WindowId window,
										  const AgenticInteractionTarget& selector,
										  float layout_to_window_scale) const noexcept {
	try {
		if (!application.hasWindow(window) || (selector.window_id && selector.window_id != window))
			return resolve_error(AgenticError::invalid_scope);
		auto scoped = selector;
		scoped.window_id = window;
		const auto& ui = application.ui(window);
		if (selector.kind != AgenticTargetKind::coordinates &&
			selector.kind != AgenticTargetKind::custom_region &&
			(ui.devTreeSnapshot().frameNumber != ui.dev_completed_window_frame() ||
			 ui.devTreeSnapshot().app_tick != ui.dev_completed_app_tick()))
			return resolve_error(AgenticError::unavailable);
		const auto dimensions = ui.dev_layout_dimensions();
		if (!(dimensions.width > 0) || !(dimensions.height > 0))
			return resolve_error(AgenticError::unavailable);
		auto resolved =
			resolve(ui.devTreeSnapshot(), scoped, layout_to_window_scale,
					AgenticInteractionRegion{0, 0, dimensions.width, dimensions.height});
		if (!resolved)
			return resolved;
		if (selector.kind == AgenticTargetKind::custom_region) {
			for (const auto& custom : targets_)
				if (custom.target_id == selector.custom_id) {
					if (custom.window_frame > ui.dev_completed_window_frame() ||
						(custom.expires_after_window_frame &&
						 ui.dev_completed_window_frame() > custom.expires_after_window_frame))
						return resolve_error(AgenticError::stale_generation);
				}
		} else if (selector.kind == AgenticTargetKind::coordinates) {
			resolved->window_frame = ui.dev_completed_window_frame();
			resolved->app_tick = ui.dev_completed_app_tick();
			resolved->layout_revision = resolved->window_frame;
		}
		return resolved;
	} catch (...) {
		return resolve_error(AgenticError::unavailable);
	}
}
Result<AgenticResolvedInteractionTarget> AgenticInteractionTargetRegistry::resolve(
	const tooling::DevTreeSnapshot& tree, const AgenticInteractionTarget& selector,
	float layout_to_window_scale, std::optional<AgenticInteractionRegion> viewport) const noexcept {
	try {
		if ((viewport && !valid_region(*viewport)) || !std::isfinite(layout_to_window_scale) ||
			layout_to_window_scale <= 0 || !std::isfinite(selector.anchor_x) ||
			!std::isfinite(selector.anchor_y) || selector.anchor_x < 0 || selector.anchor_x > 1 ||
			selector.anchor_y < 0 || selector.anchor_y > 1)
			return resolve_error(AgenticError::invalid_request);
		if (selector.window_id && tree.window && selector.window_id != tree.window)
			return resolve_error(AgenticError::invalid_scope);
		AgenticResolvedInteractionTarget result;
		result.selector = selector;
		if (!result.selector.window_id)
			result.selector.window_id = tree.window;
		result.tree_generation = tree.generation;
		result.layout_revision = tree.frameNumber;
		result.app_tick = tree.app_tick;
		result.window_frame = tree.frameNumber;
		result.layout_to_window_x = layout_to_window_scale;
		result.layout_to_window_y = layout_to_window_scale;
		if (selector.tree_generation && *selector.tree_generation != tree.generation)
			return resolve_error(AgenticError::stale_generation);
		if (selector.kind != AgenticTargetKind::custom_region && selector.layout_revision &&
			*selector.layout_revision != tree.frameNumber)
			return resolve_error(AgenticError::stale_generation);
		if (selector.kind == AgenticTargetKind::coordinates) {
			if (!std::isfinite(selector.point.x) || !std::isfinite(selector.point.y))
				return resolve_error(AgenticError::invalid_request);
			result.window_point = selector.point;
			result.layout_point = {selector.point.x / layout_to_window_scale,
								   selector.point.y / layout_to_window_scale};
			result.bounds = {result.layout_point.x, result.layout_point.y, 0, 0};
			result.visible_region = result.bounds;
			if (!valid_input_point(result))
				return resolve_error(AgenticError::invalid_request);
			return result;
		}
		if (selector.kind == AgenticTargetKind::custom_region) {
			for (const auto& custom : targets_)
				if (custom.target_id == selector.custom_id) {
					if (custom.window_id != result.selector.window_id)
						return resolve_error(AgenticError::invalid_scope);
					if ((selector.layout_revision &&
						 *selector.layout_revision != custom.window_frame) ||
						(selector.custom_revision &&
						 *selector.custom_revision != custom.revision) ||
						(custom.expires_after_window_frame &&
						 tree.frameNumber > custom.expires_after_window_frame))
						return resolve_error(AgenticError::stale_generation);
					result.bounds = custom.bounds;
					result.visible_region = custom.bounds;
					result.custom_revision = custom.revision;
					result.label = custom.label;
					result.paint_order = custom.paint_order;
					result.z_index = custom.z_index;
					result.floating = custom.floating;
					for (const auto& clip : custom.clips)
						result.visible_region = intersection(result.visible_region, clip);
					if (viewport)
						result.visible_region = intersection(result.visible_region, *viewport);
					if (!valid_region(result.visible_region))
						return resolve_error(AgenticError::invalid_scope);
					result.hit_test_supported = static_cast<bool>(custom.observe);
					result.app_tick = custom.app_tick;
					result.window_frame = custom.window_frame;
					result.layout_revision = custom.window_frame;
					result.layout_point = {
						result.visible_region.x +
							result.visible_region.width * std::min(selector.anchor_x, 0.999999),
						result.visible_region.y +
							result.visible_region.height * std::min(selector.anchor_y, 0.999999)};
					result.window_point = {result.layout_point.x * layout_to_window_scale,
										   result.layout_point.y * layout_to_window_scale};
					if (!valid_input_point(result))
						return resolve_error(AgenticError::invalid_request);
					return result;
				}
			return resolve_error(AgenticError::unavailable);
		}
#if FLOW_UI_DEV_CAPTURE_CLAY
		if (!tree.stats.complete || tree.stats.truncated || !tree.generation)
			return resolve_error(AgenticError::unavailable);
		const tooling::DevClayNode* selected = nullptr;
		std::uint64_t flow_instance = 0;
		if (selector.kind == AgenticTargetKind::flow_instance ||
			selector.kind == AgenticTargetKind::flow_debug) {
			const tooling::DevFlowNode* flow = nullptr;
			for (const auto& candidate : tree.flow.nodes) {
				const bool match =
					selector.kind == AgenticTargetKind::flow_instance
						? selector.flow_instance_id &&
							  candidate.instance.value == selector.flow_instance_id
						: !selector.flow_debug_name.empty() &&
							  tree.string(candidate.debugName) == selector.flow_debug_name;
				if (match) {
					if (flow)
						return resolve_error(AgenticError::invalid_scope);
					flow = &candidate;
				}
			}
			if (!flow || flow->clayRoot >= tree.clay.nodes.size())
				return resolve_error(AgenticError::unavailable);
			selected = &tree.clay.nodes[flow->clayRoot];
			flow_instance = flow->instance.value;
		} else {
			if (selector.kind != AgenticTargetKind::clay_id &&
				selector.kind != AgenticTargetKind::clay_name)
				return resolve_error(AgenticError::invalid_request);
			for (const auto& candidate : tree.clay.nodes) {
				const bool match = selector.kind == AgenticTargetKind::clay_id
									   ? selector.clay_id && candidate.clayId == selector.clay_id
									   : !selector.clay_name.empty() &&
											 tree.string(candidate.idString) == selector.clay_name;
				if (match) {
					if (selected)
						return resolve_error(AgenticError::invalid_scope);
					selected = &candidate;
				}
			}
		}
		if (!selected || !selected->clayId)
			return resolve_error(AgenticError::unavailable);
		if (std::count_if(
				tree.clay.nodes.begin(), tree.clay.nodes.end(),
				[selected](const auto& node) { return node.clayId == selected->clayId; }) != 1)
			return resolve_error(AgenticError::invalid_scope);
		if (tooling::hasFlag(selected->flags, tooling::DevClayNodeFlag::DuplicateId) ||
			tooling::hasFlag(selected->flags, tooling::DevClayNodeFlag::BoundsUnavailable) ||
			tooling::hasFlag(selected->flags, tooling::DevClayNodeFlag::Exiting))
			return resolve_error(AgenticError::invalid_scope);
		auto visible = clipped_region(tree, *selected);
		if (!visible)
			return unexpectedError(visible.error());
		if (viewport)
			*visible = intersection(*visible, *viewport);
		if (!valid_region(*visible))
			return resolve_error(AgenticError::invalid_scope);
		result.bounds = bounds_of(*selected);
		result.visible_region = *visible;
		result.clay_id = selected->clayId;
		result.flow_instance_id = flow_instance;
		result.label = tree.string(selected->idString);
		result.floating = tooling::hasFlag(selected->flags, tooling::DevClayNodeFlag::Floating);
		if (selected->rootIndex < tree.clay.roots.size()) {
			const auto& root = tree.clay.roots[selected->rootIndex];
			result.z_index = root.zIndex;
			result.paint_order = root.paintOrder;
		}
		result.layout_point = {visible->x + visible->width * std::min(selector.anchor_x, 0.999999),
							   visible->y +
								   visible->height * std::min(selector.anchor_y, 0.999999)};
		result.window_point = {result.layout_point.x * layout_to_window_scale,
							   result.layout_point.y * layout_to_window_scale};
		if (!valid_input_point(result))
			return resolve_error(AgenticError::invalid_request);
		result.hit_test_supported = true;
		result.matching_clay_ids.reserve(std::min<std::size_t>(tree.clay.nodes.size(), 4096));
		for (const auto& candidate : tree.clay.nodes) {
			if (!candidate.clayId || !contains(bounds_of(candidate), result.layout_point))
				continue;
			auto candidate_visible = clipped_region(tree, candidate);
			if (candidate_visible && contains(*candidate_visible, result.layout_point))
				result.matching_clay_ids.emplace_back(candidate.clayId);
			if (result.matching_clay_ids.size() > 4096)
				return resolve_error(AgenticError::budget_exhausted);
		}
		return result;
#else
		return resolve_error(AgenticError::unavailable);
#endif
	} catch (...) {
		return resolve_error(AgenticError::budget_exhausted);
	}
}
Json AgenticInteractionTargetRegistry::describe(const AgenticResolvedInteractionTarget& target) {
	Json candidates = Json::make_array();
	for (const auto clay_id : target.matching_clay_ids)
		candidates.array.emplace_back(decimal(clay_id));
	return Json::make_object(
		{{"window_id", decimal(target.selector.window_id)},
		 {"tree_generation", decimal(target.tree_generation)},
		 {"layout_revision", decimal(target.layout_revision)},
		 {"app_tick", decimal(target.app_tick)},
		 {"window_frame", decimal(target.window_frame)},
		 {"clay_id", decimal(target.clay_id)},
		 {"flow_instance_id", decimal(target.flow_instance_id)},
		 {"custom_id", target.selector.custom_id},
		 {"custom_revision", decimal(target.custom_revision)},
		 {"label", target.label},
		 {"bounds", region_json(target.bounds)},
		 {"visible_region", region_json(target.visible_region)},
		 {"layout_point", Json::make_object({{"x", Json(target.layout_point.x)},
											 {"y", Json(target.layout_point.y)}})},
		 {"window_point", Json::make_object({{"x", Json(target.window_point.x)},
											 {"y", Json(target.window_point.y)}})},
		 {"layout_to_window_x", Json(target.layout_to_window_x)},
		 {"layout_to_window_y", Json(target.layout_to_window_y)},
		 {"z_index", Json(static_cast<double>(target.z_index))},
		 {"paint_order", Json::number(target.paint_order)},
		 {"floating", target.floating},
		 {"hit_test_supported", target.hit_test_supported},
		 {"hit_test_verified", target.hit_test_verified},
		 {"geometry_candidates", std::move(candidates)},
		 {"exclusive_routing_proven", false}});
}
Result<Json> AgenticInteractionTargetRegistry::discover(App& application, WindowId window,
														float scale,
														std::size_t limit) const noexcept {
	try {
		if (!application.hasWindow(window) || !limit || limit > 4096)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		const auto& tree = application.ui(window).devTreeSnapshot();
		Json rows = Json::make_array();
		bool truncated = false;
#if FLOW_UI_DEV_CAPTURE_CLAY
		for (const auto& node : tree.clay.nodes) {
			if (rows.array.size() >= limit) {
				truncated = true;
				break;
			}
			AgenticInteractionTarget selector;
			selector.kind = AgenticTargetKind::clay_id;
			selector.window_id = window;
			selector.clay_id = node.clayId;
			auto target = resolve(application, window, selector, scale);
			if (target)
				rows.array.emplace_back(describe(*target));
		}
#endif
		for (const auto& custom : targets_)
			if (custom.window_id == window) {
				if (rows.array.size() >= limit) {
					truncated = true;
					break;
				}
				AgenticInteractionTarget selector;
				selector.kind = AgenticTargetKind::custom_region;
				selector.window_id = window;
				selector.custom_id = custom.target_id;
				auto target = resolve(application, window, selector, scale);
				if (target)
					rows.array.emplace_back(describe(*target));
			}
		return Json::make_object(
			{{"targets", std::move(rows)},
			 {"tree_generation", decimal(tree.generation)},
			 {"layout_revision", decimal(tree.frameNumber)},
			 {"truncated", truncated},
			 {"geometry_available", tree.stats.complete && !tree.stats.truncated}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<Json>
AgenticInteractionTargetRegistry::observe(const AgenticResolvedInteractionTarget& target,
										  const InteractionSnapshot& interaction,
										  std::size_t maximum_ids) const noexcept {
	try {
		if (interaction.hoveredElementIds.size() > maximum_ids ||
			interaction.pressedElementIds.size() > maximum_ids ||
			interaction.heldElementIds.size() > maximum_ids ||
			interaction.releasedElementIds.size() > maximum_ids)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		bool hovered = has_id(interaction.hoveredElementIds, target.clay_id),
			 pressed = has_id(interaction.pressedElementIds, target.clay_id),
			 held = has_id(interaction.heldElementIds, target.clay_id),
			 released = has_id(interaction.releasedElementIds, target.clay_id);
		Json application_outcome = Json::make_object();
		if (target.selector.kind == AgenticTargetKind::custom_region) {
			const AgenticCustomInteractionTarget* custom = nullptr;
			for (const auto& candidate : targets_)
				if (candidate.target_id == target.selector.custom_id)
					custom = &candidate;
			if (!custom || custom->revision != target.custom_revision)
				return unexpectedError(agentic_error(AgenticError::stale_generation));
			if (!custom->observe)
				return unexpectedError(agentic_error(AgenticError::unavailable));
			const auto callback = custom->observe;
			auto observed = callback(target);
			if (!observed)
				return unexpectedError(observed.error());
			hovered = observed->hovered;
			pressed = observed->pressed;
			held = observed->held;
			released = observed->released;
			application_outcome = std::move(observed->outcome);
		}
		Json other_hovered = Json::make_array();
		for (const auto clay_id : interaction.hoveredElementIds)
			if (clay_id != target.clay_id)
				other_hovered.array.emplace_back(decimal(clay_id));
		auto observed_target = target;
		observed_target.hit_test_verified = hovered;
		return Json::make_object({{"target", describe(observed_target)},
								  {"target_hovered", hovered},
								  {"target_pressed", pressed},
								  {"target_held", held},
								  {"target_released", released},
								  {"other_hovered_clay_ids", std::move(other_hovered)},
								  {"exclusive_routing_proven", false},
								  {"secondary_button_observations_supported", false},
								  {"application_outcome", std::move(application_outcome)}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	}
}
} // namespace FlowUi::devSystems::agentic
#endif
