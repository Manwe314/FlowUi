#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorEvents.hpp"
#include <exception>

namespace FlowUi {

namespace detail {

/* Route one occurrence through the active singleton App observer. */
#if FLOW_UI_DEV_MODE
void reportErrorEvent(const ErrorEventView& event, const char* file = __builtin_FILE(),
					  const char* function = __builtin_FUNCTION(),
					  std::uint32_t line = __builtin_LINE(),
#if __has_builtin(__builtin_COLUMN)
					  std::uint32_t column = __builtin_COLUMN()) noexcept;
#else
					  std::uint32_t column = 0u) noexcept;
#endif
#else
void reportErrorEvent(const ErrorEventView& event) noexcept;
#endif

/* Report fatal evidence through the common observer, then prohibit continuation. */
[[noreturn]] inline void terminateForFatalError(FlowUiError error) noexcept {
	reportErrorEvent(ErrorEventView{
		.error = error,
		.kind = ErrorEventKind::Fatal,
		.resolution = ErrorResolution::Terminated,
		.inspection = FatalInspectionCapability::TerminalOnly,
	});
	std::terminate();
}

} // namespace detail

} // namespace FlowUi
