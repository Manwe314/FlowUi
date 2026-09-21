#include "devSystems/devTooling/inspect/DevInspectInteractionController.hpp"

#if FLOW_UI_DEV_MODE
#include <cmath>

namespace FlowUi::devSystems::tooling {
namespace {
[[nodiscard]] bool eligible_node(const DevFlowNode& node) noexcept {
	return hasFlag(node.flags, DevFlowNodeFlag::Drawn) &&
		   !hasFlag(node.flags, DevFlowNodeFlag::InternalDev) &&
		   !hasFlag(node.flags, DevFlowNodeFlag::CaptureCanceled);
}

#if FLOW_UI_DEV_CAPTURE_CLAY
[[nodiscard]] bool contains(const Clay_BoundingBox& bounds, float pointer_x,
							float pointer_y) noexcept {
	return std::isfinite(bounds.x) && std::isfinite(bounds.y) && std::isfinite(bounds.width) &&
		   std::isfinite(bounds.height) && bounds.width > 0.0f && bounds.height > 0.0f &&
		   pointer_x >= bounds.x && pointer_y >= bounds.y && pointer_x < bounds.x + bounds.width &&
		   pointer_y < bounds.y + bounds.height;
}

[[nodiscard]] bool inside_clips(const DevTreeSnapshot& snapshot, const DevClayNode& node,
								float pointer_x, float pointer_y) noexcept {
	uint32_t clip_id = node.clipClayId;
	if (!clip_id && node.rootIndex < snapshot.clay.roots.size())
		clip_id = snapshot.clay.roots[node.rootIndex].clipClayId;
	for (size_t depth = 0; clip_id && depth < snapshot.clay.nodes.size(); ++depth) {
		const DevClayNode* clip = nullptr;
		for (const auto& candidate : snapshot.clay.nodes) {
			if (candidate.clayId == clip_id) {
				clip = &candidate;
				break;
			}
		}
		if (!clip || hasFlag(clip->flags, DevClayNodeFlag::BoundsUnavailable))
			return false;
		const auto& bounds = clip->bounds;
		if (clip->declaration.clip.horizontal &&
			(pointer_x < bounds.x || pointer_x >= bounds.x + bounds.width))
			return false;
		if (clip->declaration.clip.vertical &&
			(pointer_y < bounds.y || pointer_y >= bounds.y + bounds.height))
			return false;
		if (clip->clipClayId == clip_id)
			return false;
		clip_id = clip->clipClayId;
	}
	return clip_id == 0;
}
#endif
} // namespace

DevOverlayTargetSpec resolve_inspect_target(const DevInspectTarget& target,
											const DevTreeSnapshot& snapshot) noexcept {
	if (!target || target.window != snapshot.window)
		return {};
	for (uint32_t node_index = 0; node_index < snapshot.flow.nodes.size(); ++node_index) {
		const auto& node = snapshot.flow.nodes[node_index];
		if (node.definition == target.definition && node.instance == target.instance &&
			eligible_node(node))
			return {.flowNodeIndex = node_index,
					.definition = node.definition,
					.instanceKey = node.instance};
	}
	return {};
}

DevInspectTarget hit_test_inspect_target(const DevTreeSnapshot& snapshot, float pointer_x,
										 float pointer_y) noexcept {
	if (!std::isfinite(pointer_x) || !std::isfinite(pointer_y) || !snapshot.stats.complete)
		return {};
#if FLOW_UI_DEV_CAPTURE_CLAY
	// Capture is Clay preorder within each root; later siblings/descendants paint last.
	const DevClayNode* topmost = nullptr;
	int16_t top_z = INT16_MIN;
	uint32_t top_order = 0;
	for (const auto& node : snapshot.clay.nodes) {
		if (node.rootIndex >= snapshot.clay.roots.size() ||
			hasFlag(node.flags, DevClayNodeFlag::BoundsUnavailable) ||
			hasFlag(node.flags, DevClayNodeFlag::Exiting) ||
			!contains(node.bounds, pointer_x, pointer_y) ||
			!inside_clips(snapshot, node, pointer_x, pointer_y))
			continue;
		const auto& root = snapshot.clay.roots[node.rootIndex];
		if (topmost &&
			(root.zIndex < top_z || (root.zIndex == top_z && root.paintOrder < top_order)))
			continue;
		topmost = &node;
		top_z = root.zIndex;
		top_order = root.paintOrder;
	}
	for (size_t depth = 0; topmost && depth < snapshot.clay.nodes.size(); ++depth) {
		if (topmost->directFlowOwner < snapshot.flow.nodes.size()) {
			const auto& owner = snapshot.flow.nodes[topmost->directFlowOwner];
			if (!eligible_node(owner))
				return {};
			return {snapshot.window, owner.definition, owner.instance};
		}
		uint32_t parent = topmost->parent;
		if (parent == InvalidClayNode && topmost->rootIndex < snapshot.clay.roots.size())
			parent = snapshot.clay.roots[topmost->rootIndex].attachmentParent;
		topmost = parent < snapshot.clay.nodes.size() ? &snapshot.clay.nodes[parent] : nullptr;
	}
#endif
	return {};
}

void DevInspectInteractionController::set_interface_window(WindowId window) noexcept {
	interface_window_ = window;
	if (window == InvalidWindowId)
		cancel_pick();
}

bool DevInspectInteractionController::eligible_window(WindowId window) const noexcept {
	return window != InvalidWindowId && window != interface_window_;
}

void DevInspectInteractionController::toggle_primary_pick() noexcept {
	if (picking()) {
		cancel_pick();
		return;
	}
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (interface_window_ != InvalidWindowId)
		pick_mode_ = DevInspectPickMode::Primary;
#endif
}

void DevInspectInteractionController::begin_secondary_pick() noexcept {
#if FLOW_UI_DEV_CAPTURE_CLAY
	if (primary_target_ && interface_window_ != InvalidWindowId) {
		cancel_pick();
		pick_mode_ = DevInspectPickMode::Secondary;
	}
#endif
}

void DevInspectInteractionController::cancel_pick() noexcept {
	pick_mode_ = DevInspectPickMode::Idle;
	hover_target_ = {};
	click_candidate_ = {};
}

void DevInspectInteractionController::select_primary(DevInspectTarget target) noexcept {
	if (!eligible_window(target.window) || !target) {
		clear_selection();
		return;
	}
	if (secondary_target_.window != target.window || secondary_target_ == target)
		secondary_target_ = {};
	primary_target_ = target;
	++selection_revision_;
	cancel_pick();
}

void DevInspectInteractionController::clear_selection() noexcept {
	primary_target_ = {};
	secondary_target_ = {};
	++selection_revision_;
	cancel_pick();
}

void DevInspectInteractionController::remove_window(WindowId window) noexcept {
	if (window == interface_window_)
		set_interface_window(InvalidWindowId);
	if (primary_target_.window == window)
		clear_selection();
	if (hover_target_.window == window)
		hover_target_ = {};
	if (click_candidate_.window == window)
		click_candidate_ = {};
}

void DevInspectInteractionController::toggle_surface(DevOverlayModeFlags flag) noexcept {
	mode_flags_ = static_cast<DevOverlayModeFlags>(static_cast<uint32_t>(mode_flags_) ^
												   static_cast<uint32_t>(flag));
}

void DevInspectInteractionController::filter_input(WindowId window, const DevTreeSnapshot& snapshot,
												   FrameInput& input,
												   DevInspectPointerState& pointer) noexcept {
	const bool down = input.mouseDown[0];
	const bool pressed = down && !pointer.previous_down && pointer.observed_release;
	pointer.pointer_x = input.mouseX;
	pointer.pointer_y = input.mouseY;
	pointer.pointer_inside = input.pointerInside;
	if (eligible_window(window)) {
		if (picking() && input.keyDown[256]) {
			cancel_pick();
			input.keyDown[256] = false;
		}
		if (!input.pointerInside && hover_target_.window == window)
			hover_target_ = {};
		if (picking() && pressed && input.pointerInside) {
			pointer.consume_until_release = true;
			if (pick_mode_ != DevInspectPickMode::Secondary || window == primary_target_.window)
				click_candidate_ = hit_test_inspect_target(snapshot, input.mouseX, input.mouseY);
		}
		// An already-held application gesture is allowed to finish before arming.
		if (pointer.consume_until_release)
			input.mouseDown[0] = false;
	}
	if (!down) {
		pointer.observed_release = true;
		pointer.consume_until_release = false;
	}
	pointer.previous_down = down;
}

void DevInspectInteractionController::finish_frame(const DevTreeSnapshot& snapshot,
												   const DevInspectPointerState& pointer) noexcept {
	if (!eligible_window(snapshot.window) || !snapshot.stats.complete || snapshot.stats.truncated)
		return;
	if (primary_target_.window == snapshot.window &&
		!resolve_inspect_target(primary_target_, snapshot).isValid())
		clear_selection();
	if (secondary_target_.window == snapshot.window &&
		!resolve_inspect_target(secondary_target_, snapshot).isValid())
		secondary_target_ = {};
	if (click_candidate_.window == snapshot.window) {
		const auto candidate = click_candidate_;
		click_candidate_ = {};
		if (resolve_inspect_target(candidate, snapshot).isValid()) {
			if (pick_mode_ == DevInspectPickMode::Primary)
				select_primary(candidate);
			else if (pick_mode_ == DevInspectPickMode::Secondary && candidate != primary_target_ &&
					 candidate.window == primary_target_.window) {
				secondary_target_ = candidate;
				cancel_pick();
			}
		}
	}
	if (picking() && pointer.pointer_inside) {
		hover_target_ =
			(pick_mode_ == DevInspectPickMode::Primary || snapshot.window == primary_target_.window)
				? hit_test_inspect_target(snapshot, pointer.pointer_x, pointer.pointer_y)
				: DevInspectTarget{};
	}
}

bool DevInspectInteractionController::overlay_selection(
	const DevTreeSnapshot& snapshot, DevOverlaySelectionSpec& selection) const noexcept {
	selection = {};
	if (!eligible_window(snapshot.window))
		return false;
	auto primary = primary_target_;
	auto secondary = secondary_target_;
	if (pick_mode_ == DevInspectPickMode::Primary && hover_target_)
		primary = hover_target_;
	if (pick_mode_ == DevInspectPickMode::Secondary && hover_target_ && hover_target_ != primary)
		secondary = hover_target_;
	selection.primaryTarget = resolve_inspect_target(primary, snapshot);
	if (secondary != primary)
		selection.secondaryTarget = resolve_inspect_target(secondary, snapshot);
	selection.modeFlags = mode_flags_;
	return selection.primaryTarget.isValid();
}
} // namespace FlowUi::devSystems::tooling
#endif
