#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/overlay/DevOverlayTypes.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Hit-test domain used by pointer inspection. */
enum class DevInspectPickDomain : uint8_t {
	/** @brief Select the owning typed Flow invocation. */
	Flow = 0,
	/** @brief Select an individual raw Clay layout node. */
	Clay = 1
};

/** Window-qualified identity retained independently of a captured tree's indices. */
struct DevInspectTarget {
	/** @brief Stable selection key independent of transient snapshot indices. */
	uint64_t selection_key = 0u;
	/** @brief Flow instance address when kind is Flow. */
	::FlowUi::detail::element::ElementInstanceKey instance{};
	/** @brief Flow definition identity when kind is Flow. */
	FlowDefinitionID definition{};
	/** @brief Application window owning the selected target. */
	WindowId window = InvalidWindowId;
	/** @brief Raw Clay element ID for Clay picks. */
	uint32_t clay_id = 0u;
	/** @brief Raw Clay root ID used to disambiguate attachment forests. */
	uint32_t clay_root = 0u;
	/** @brief Last resolved Clay index; refresh against a new snapshot. */
	uint32_t clay_index = UINT32_MAX;
	/** @brief Flow, raw Clay, or empty selection. */
	DevInspectTargetKind kind = DevInspectTargetKind::None;

	/** @brief Construct an empty selection with no window or target identity. */
	constexpr DevInspectTarget() noexcept = default;
	/** @brief Construct a window-qualified typed Flow selection from definition/instance identities. */
	constexpr DevInspectTarget(WindowId win, FlowDefinitionID def,
							   ::FlowUi::detail::element::ElementInstanceKey inst) noexcept
		: selection_key(inst.value), instance(inst), definition(def), window(win),
		  kind(inst ? DevInspectTargetKind::Flow : DevInspectTargetKind::None) {}
	/** @brief Construct a window-qualified raw Clay selection with its stable correlation key. */
	constexpr DevInspectTarget(WindowId win, uint32_t c_id, uint32_t c_root, uint32_t c_index,
							   uint64_t s_key) noexcept
		: selection_key(s_key), window(win), clay_id(c_id), clay_root(c_root), clay_index(c_index),
		  kind(DevInspectTargetKind::Clay) {}

	/** @brief Return whether a nonzero window and a valid Flow/Clay target identity are present. */
	[[nodiscard]] explicit operator bool() const noexcept {
		if (window == InvalidWindowId || kind == DevInspectTargetKind::None) {
			return false;
		}
		if (kind == DevInspectTargetKind::Flow) {
			return bool(instance);
		}
		return clay_id != 0u || clay_index != UINT32_MAX || selection_key != 0u;
	}
	/** @brief Compare every copied target-identity field, including the last resolved Clay index. */
	[[nodiscard]] bool operator==(const DevInspectTarget&) const noexcept = default;
};

/** @brief Pointer-picking state of the inspection controller. */
enum class DevInspectPickMode : uint8_t {
	/** @brief No pointer pick is armed. */
	Idle,
	/** @brief Pick the primary inspected target. */
	Primary,
	/** @brief Pick a comparison target in the primary window. */
	Secondary
};

/** Raw pointer history belongs to each application window, including consumed releases. */
struct DevInspectPointerState {
	/** @brief Last raw pointer X coordinate in layout space. */
	float pointer_x = 0.0f;
	/** @brief Last raw pointer Y coordinate in layout space. */
	float pointer_y = 0.0f;
	/** @brief Whether the pointer is inside this application window. */
	bool pointer_inside = false;
	/** @brief Raw primary-button state from the prior input sample. */
	bool previous_down = false;
	/** @brief Whether a raw button release has been observed. */
	bool observed_release = false;
	/** @brief Whether application pointer input remains suppressed for an active pick. */
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
	/** @brief Return whether window is a nonzero application window other than the excluded developer window. */
	[[nodiscard]] bool eligible_window(WindowId window) const noexcept;
	/** @brief Return whether primary or secondary pointer picking is armed. */
	[[nodiscard]] bool picking() const noexcept { return pick_mode_ != DevInspectPickMode::Idle; }
	/** @brief Return the currently armed pick mode. */
	[[nodiscard]] DevInspectPickMode pick_mode() const noexcept { return pick_mode_; }
	/** @brief Return whether pointer hit testing selects Flow owners or raw Clay nodes. */
	[[nodiscard]] DevInspectPickDomain pick_domain() const noexcept { return pick_domain_; }
	/** @brief Return the currently enabled visual overlay surfaces. */
	[[nodiscard]] DevOverlayModeFlags mode_flags() const noexcept { return mode_flags_; }
	/** @brief Copy the committed primary identity; resolve it against a completed snapshot before using node indices. */
	[[nodiscard]] DevInspectTarget primary_target() const noexcept { return primary_target_; }
	/** @brief Copy the optional comparison identity; resolve it against the matching window snapshot. */
	[[nodiscard]] DevInspectTarget secondary_target() const noexcept { return secondary_target_; }
	/** @brief Return the monotonic inspector-selection revision. */
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
