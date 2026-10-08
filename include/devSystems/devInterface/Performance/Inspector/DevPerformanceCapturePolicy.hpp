#pragma once
#include "devSystems/devInterface/Performance/DevPerformanceContentParameters.hpp"
#if FLOW_UI_DEV_MODE
#include <cmath>
namespace FlowUi::devSystems::interface_elements {
/** Shared UI validation before converting floating capture values to nanoseconds. */
[[nodiscard]] inline std::string_view
performance_capture_policy_error(const DevInterfaceState& state, const App* app) noexcept {
	if (state.capture_start_mode > 1 || state.capture_end_mode > 2)
		return "Choose a valid capture start and stop mode.";
	if (!std::isfinite(state.capture_duration_seconds) || state.capture_duration_seconds < .001f ||
		state.capture_duration_seconds > 86400)
		return "Duration must be between 0.001 and 86400 seconds.";
	if (!std::isfinite(state.capture_budget_ms) || state.capture_budget_ms < .001f ||
		state.capture_budget_ms > 60000)
		return "Threshold must be between 0.001 and 60000 milliseconds.";
	if (!std::isfinite(state.capture_tail_seconds) || state.capture_tail_seconds < 0 ||
		state.capture_tail_seconds > 86400)
		return "After-event duration must be between 0 and 86400 seconds.";
	if (state.capture_key < 32 || state.capture_key >= 349 || state.capture_modifiers > 15)
		return "Choose a valid shortcut key and modifiers.";
	if (state.capture_end_mode == 1 && state.capture_budget_window && app &&
		!app->hasWindow(state.capture_budget_window))
		return "Budget window unavailable. Choose a new source.";
	return {};
}
} // namespace FlowUi::devSystems::interface_elements
#endif
