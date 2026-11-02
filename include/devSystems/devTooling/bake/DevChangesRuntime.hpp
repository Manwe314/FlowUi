#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/bake/DevChangesReview.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"

namespace FlowUi {
class ThemeManager;
}
namespace FlowUi::devSystems::tooling {
/** Runtime intent is independent of the next-manifest decision. */
enum class DevChangesRuntimeAction : uint8_t {
	ApplyProposal,
	ApplySaved,
	DiscardLive,
	ResetAuthored
};
/** Validated, owned command draft; success is reported only after engine acknowledgement. */
struct DevChangesRuntimeEdit {
	std::vector<DevOverrideCommand> forward{}, inverse{};
	std::string error{};
	bool available = false;
};
/** Materialize saved reflected JSON and stable actions using a typed observed value.
 * Unobserved baselines and saved resource keys without stable ownership return a diagnostic. */
[[nodiscard]] DevChangesRuntimeEdit
prepare_changes_runtime_edit(const devMode::DevSchemaGeneration& schema,
							 const DevOverrideEngine& engine, const ThemeManager& themes,
							 const DevChangesReviewRow& row, WindowId window,
							 DevChangesRuntimeAction action) noexcept;
} // namespace FlowUi::devSystems::tooling
#endif
