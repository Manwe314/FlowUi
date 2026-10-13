#pragma once
#include "FlowUi/Error.hpp"
#include "FlowUi/MemoryCapacityTargets.hpp"
namespace FlowUi::detail {
/** Reserve at owner creation; preserve the container's load factor and growth policy. */
template <class Container>
void reserve_initial_capacity(Container& container, uint64_t value) {
	if (value == 0)
		return;
	if (value > container.max_size())
		throw FlowUiException(
			makeError(ErrorCode::StorageConfigurationInvalid, ErrorSite::StorageInitialize));
	container.reserve(static_cast<size_t>(value));
}
} // namespace FlowUi::detail
