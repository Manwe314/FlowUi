#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/bake/DevChangesReview.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"

namespace FlowUi {
class ThemeManager;
}
namespace FlowUi::devSystems::tooling {
/** Runtime intent is independent of the next-manifest decision. */
enum class DevChangesRuntimeAction : uint8_t {
	/** @brief Apply the live/staged proposed value to runtime state. */
	ApplyProposal,
	/** @brief Apply the saved manifest value to runtime state. */
	ApplySaved,
	/** @brief Remove the live override while retaining saved assignments. */
	DiscardLive,
	/** @brief Restore the observed application-authored baseline. */
	ResetAuthored
};
/** Validated, owned command draft; success is reported only after engine acknowledgement. */
struct DevChangesRuntimeEdit {
	/** @var std::vector<DevOverrideCommand> forward
	 * @brief Owned commands implementing the requested runtime action.
	 */
	/** @var std::vector<DevOverrideCommand> inverse
	 * @brief Owned inverse commands used to restore the observed runtime baseline.
	 */
	std::vector<DevOverrideCommand> forward{}, inverse{};
	/** @brief Owned preparation diagnostic when the requested action is unavailable. */
	std::string error{};
	/** @brief Whether a valid command draft was prepared; submission is a separate step. */
	bool available = false;
};
/** Materialize saved reflected JSON and stable actions using a typed observed value.
 * Unobserved baselines and saved resource keys without stable ownership return a diagnostic. */
[[nodiscard]] DevChangesRuntimeEdit
prepare_changes_runtime_edit(const tooling::DevSchemaGeneration& schema,
							 const DevOverrideEngine& engine, const ThemeManager& themes,
							 const DevChangesReviewRow& row, WindowId window,
							 DevChangesRuntimeAction action) noexcept;
} // namespace FlowUi::devSystems::tooling
#endif
