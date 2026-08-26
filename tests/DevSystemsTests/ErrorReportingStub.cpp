#include "FlowUi/Error.hpp"

namespace FlowUi::detail {

#if FLOW_UI_DEV_MODE
void reportErrorEvent(
	const ErrorEventView&,
	const char*,
	const char*,
	std::uint32_t,
	std::uint32_t) noexcept {}
#else
void reportErrorEvent(const ErrorEventView&) noexcept {}
#endif

} // namespace FlowUi::detail
