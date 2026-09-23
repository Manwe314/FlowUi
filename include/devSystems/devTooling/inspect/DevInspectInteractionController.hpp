#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/overlay/DevOverlayTypes.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/structs/InputStructs.hpp"

namespace FlowUi::devSystems::tooling {

enum class DevInspectPickDomain : uint8_t { Flow = 0, Clay = 1 };

/** Window-qualified identity retained independently of a captured tree's indices. */
struct DevInspectTarget {
	uint64_t selection_key = 0u;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	uint32_t clay_id = 0u;
	uint32_t clay_root = 0u;
	uint32_t clay_index = UINT32_MAX;
	DevInspectTargetKind kind = DevInspectTargetKind::None;

	constexpr DevInspectTarget() noexcept = default;
	constexpr DevInspectTarget(WindowId win, FlowDefinitionID def,
							   ::FlowUi::detail::element::ElementInstanceKey inst) noexcept
		: selection_key(inst.value), instance(inst), definition(def), window(win),
		  kind(inst ? DevInspectTargetKind::Flow : DevInspectTargetKind::None) {}
	constexpr DevInspectTarget(WindowId win, uint32_t c_id, uint32_t c_root, uint32_t c_index,
							   uint64_t s_key) noexcept
		: selection_key(s_key), window(win), clay_id(c_id), clay_root(c_root), clay_index(c_index),
		  kind(DevInspectTargetKind::Clay) {}

	[[nodiscard]] explicit operator bool() const noexcept {
		if (window == InvalidWindowId || kind == DevInspectTargetKind::None) {
			return false;
		}
		if (kind == DevInspectTargetKind::Flow) {
			return bool(instance);
		}
		return clay_id != 0u || clay_index != UINT32_MAX || selection_key != 0u;
	}
	[[nodiscard]] bool operator==(const DevInspectTarget&) const noexcept = default;
};

enum class DevInspectPickMode : uint8_t { Idle, Primary, Secondary };

/** Raw pointer history belongs to each application window, including consumed releases. */
struct DevInspectPointerState {
	float pointer_x = 0.0f;
	float pointer_y = 0.0f;
	bool pointer_inside = false;
	bool previous_down = false;
	bool observed_release = false;
	bool consume_until_release = false;
};

/** Resolve a stable identity against one completed capture. */
[[nodiscard]] DevOverlayTargetSpec resolve_inspect_target(const DevInspectTarget& target,
														  const DevTreeSnapshot& snapshot) noexcept;

/** Return the topmost visible owner or primitive, respecting Clay paint order and clipping. */
[[nodiscard]] DevInspectTarget hit_test_inspect_target(
	const DevTreeSnapshot& snapshot, float pointer_x, float pointer_y,
	DevInspectPickDomain domain = DevInspectPickDomain::Flow) noexcept;

/** Session-owned inspection state; contains no borrowed UI or snapshot pointers. */
class DevInspectInteractionController {
public:
	/** Set the excluded developer window. Removing it cancels active picking. */
	void set_interface_window(WindowId window) noexcept;
	/** Toggle primary picking, or cancel either currently active pick mode. */
	void toggle_primary_pick() noexcept;
	/** Arm a same-window comparison pick when a primary is assigned. */
	void begin_secondary_pick() noexcept;
	/** Restore committed overlays without changing the inspector selection. */
	void cancel_pick() noexcept;
	/** Commit a selection; inspector synchronization observes the revision. */
	void select_primary(DevInspectTarget target) noexcept;
	/** Clear both targets and notify the inspector, retaining surface choices. */
	void clear_selection() noexcept;
	/** Remove identities belonging to a destroyed window. */
	void remove_window(WindowId window) noexcept;
	/** Toggle one overlay surface without changing targets or pick mode. */
	void toggle_surface(DevOverlayModeFlags flag) noexcept;
	/** Set the active element pick domain (Flow vs Clay). */
	void set_pick_domain(DevInspectPickDomain domain) noexcept;
	/** Toggle between Flow and Clay element picking. */
	void toggle_pick_domain() noexcept;
	/** Filter application input before UI dispatch, preserving raw pointer history. */
	void filter_input(WindowId window, const DevTreeSnapshot& snapshot, FrameInput& input,
					  DevInspectPointerState& pointer) noexcept;
	/** Validate a candidate and update hover from the newly completed layout. */
	void finish_frame(const DevTreeSnapshot& snapshot,
					  const DevInspectPointerState& pointer) noexcept;
	/** Build this controller's frame-local renderer specification. */
	[[nodiscard]] bool overlay_selection(const DevTreeSnapshot& snapshot,
										 DevOverlaySelectionSpec& selection) const noexcept;
	[[nodiscard]] bool eligible_window(WindowId window) const noexcept;
	[[nodiscard]] bool picking() const noexcept { return pick_mode_ != DevInspectPickMode::Idle; }
	[[nodiscard]] DevInspectPickMode pick_mode() const noexcept { return pick_mode_; }
	[[nodiscard]] DevInspectPickDomain pick_domain() const noexcept { return pick_domain_; }
	[[nodiscard]] DevOverlayModeFlags mode_flags() const noexcept { return mode_flags_; }
	[[nodiscard]] DevInspectTarget primary_target() const noexcept { return primary_target_; }
	[[nodiscard]] DevInspectTarget secondary_target() const noexcept { return secondary_target_; }
	[[nodiscard]] uint64_t selection_revision() const noexcept { return selection_revision_; }

private:
	DevInspectTarget primary_target_{};
	DevInspectTarget secondary_target_{};
	DevInspectTarget hover_target_{};
	DevInspectTarget click_candidate_{};
	WindowId interface_window_ = InvalidWindowId;
	uint64_t selection_revision_ = 1u;
	DevOverlayModeFlags mode_flags_ = DevOverlayModeFlags::Default;
	DevInspectPickMode pick_mode_ = DevInspectPickMode::Idle;
	DevInspectPickDomain pick_domain_ = DevInspectPickDomain::Flow;
};

} // namespace FlowUi::devSystems::tooling
#endif
