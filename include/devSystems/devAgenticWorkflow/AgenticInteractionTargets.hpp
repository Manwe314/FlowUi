#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "FlowUi/WindowId.hpp"
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
	bool hovered = false, pressed = false, held = false, released = false;
	Json outcome = Json::make_object();
};
/** Owned region published by an application; updating requires a strictly greater revision. */
struct AgenticCustomInteractionTarget {
	std::string target_id, label;
	std::vector<AgenticInteractionRegion> clips;
	std::function<Result<AgenticCustomInteractionObservation>(
		const AgenticResolvedInteractionTarget&)>
		observe;
	AgenticInteractionRegion bounds;
	std::uint64_t window_id = 0, revision = 0, app_tick = 0, window_frame = 0;
	std::uint64_t expires_after_window_frame = 0;
	std::int32_t z_index = 0;
	std::uint32_t paint_order = 0;
	bool floating = false;
};
/** Platform-thread registry for owned custom regions and raw Clay/Flow resolution. */
class AgenticInteractionTargetRegistry {
public:
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
