#pragma once

/** @file
 * @brief Public AgenticInteractionTargets declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace FlowUi {
class App;
struct InteractionSnapshot;
} // namespace FlowUi
namespace FlowUi::devSystems::tooling {
struct DevTreeSnapshot;
}
namespace FlowUi::devSystems::agentic {
/** Custom target observations are application supplied evidence, never fabricated Clay state. */
struct AgenticCustomInteractionObservation {
	/** @var hovered
	 * @brief Clay IDs actually observed hovered by the primary pointer interaction.
	 */
	/** @var pressed
	 * @brief Clay IDs actually observed pressed by the primary pointer interaction.
	 */
	/** @var held
	 * @brief Clay IDs actually observed held by the primary pointer interaction.
	 */
	/** @var released
	 * @brief Clay IDs actually observed released by the primary pointer interaction.
	 */
	bool hovered = false, pressed = false, held = false, released = false;
	/**
	 * @brief Owned application-supplied target result; persistence requires explicit content
	 * permission.
	 */
	Json outcome = Json::make_object();
};
/** Owned region published by an application; updating requires a strictly greater revision. */
struct AgenticCustomInteractionTarget {
	/** @var target_id
	 * @brief Application-declared stable custom target identity.
	 */
	/** @var label
	 * @brief Application-supplied target label; persistence follows the content policy.
	 */
	std::string target_id, label;
	/** @brief Owned clipping rectangles applied to the custom target's bounds. */
	std::vector<AgenticInteractionRegion> clips;
	/**
	 * @brief Optional platform-thread callback returning genuine application-supplied target
	 * facts.
	 */
	std::function<Result<AgenticCustomInteractionObservation>(
		const AgenticResolvedInteractionTarget&)>
		observe;
	/** @brief Resolved or declared target rectangle in layout coordinates. */
	AgenticInteractionRegion bounds;
	/** @var window_id
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	/** @var revision
	 * @brief Expected or declared monotonically versioned application export identity.
	 */
	/** @var app_tick
	 * @brief Producer-supplied application tick identity; independent of window frame and resource slot.
	 */
	/** @var window_frame
	 * @brief Producer-supplied frame identity within one window; distinct from reusable slots.
	 */
	std::uint64_t window_id = 0, revision = 0, app_tick = 0, window_frame = 0;
	/**
	 * @brief Last window frame for which this custom target is valid; zero leaves it unbounded.
	 */
	std::uint64_t expires_after_window_frame = 0;
	/** @brief Completed-layout stacking index used to distinguish overlapping targets. */
	std::int32_t z_index = 0;
	/** @brief Completed-layout paint ordering used to distinguish overlapping targets. */
	std::uint32_t paint_order = 0;
	/** @brief Whether the resolved target belongs to a floating layout surface. */
	bool floating = false;
};
/** Platform-thread registry for owned custom regions and raw Clay/Flow resolution. */
class AgenticInteractionTargetRegistry {
public:
	/** @brief Construct an empty custom target registry bounded by maximum_targets. */
	explicit AgenticInteractionTargetRegistry(std::size_t maximum_targets = 1024) noexcept;
	/** Register or replace an owned custom region with increasing revision. */
	[[nodiscard]] Status register_target(AgenticCustomInteractionTarget target) noexcept;
	/** Remove only the expected revision, avoiding accidental deletion of a newer region. */
	[[nodiscard]] Status unregister_target(std::string_view target_id,
										   std::uint64_t revision) noexcept;
	/** Resolve one exact selector against a current application's completed layout. */
	[[nodiscard]] Result<AgenticResolvedInteractionTarget>
	resolve(App& application, WindowId window, const AgenticInteractionTarget& selector,
			float layout_to_window_scale = 1.0f) const noexcept;
	/** Pure completed-tree resolver, also supporting explicitly registered custom regions. */
	[[nodiscard]] Result<AgenticResolvedInteractionTarget>
	resolve(const tooling::DevTreeSnapshot& tree, const AgenticInteractionTarget& selector,
			float layout_to_window_scale = 1.0f,
			std::optional<AgenticInteractionRegion> viewport = std::nullopt) const noexcept;
	/** Return bounded current geometry; an empty tree is reported unavailable. */
	[[nodiscard]] Result<Json> discover(App& application, WindowId window,
										float layout_to_window_scale,
										std::size_t limit = 100) const noexcept;
	/** Copy actual membership and all other hovered IDs; membership is not exclusive routing proof.
	 */
	[[nodiscard]] Result<Json> observe(const AgenticResolvedInteractionTarget& target,
									   const InteractionSnapshot& interaction,
									   std::size_t maximum_ids = 4096) const noexcept;
	/** Serialize an owned target without retaining live tree references. */
	[[nodiscard]] static Json describe(const AgenticResolvedInteractionTarget& target);

private:
	std::vector<AgenticCustomInteractionTarget> targets_;
	std::size_t maximum_targets_;
};
} // namespace FlowUi::devSystems::agentic
#endif
