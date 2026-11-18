#pragma once

#include "FlowUi/Private/storageSystem/StorageSystemValidation.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

namespace FlowUi::detail::storage {

/** Reject growth configuration unsupported by the selected implementation. */
template <StorageSystemConcept Storage>
[[nodiscard]] Status validateStorageConfiguration(const StorageConfig& config) noexcept {
	if constexpr (!storageSupportsCapability<Storage>(StorageCapability::RuntimeGrowth)) {
		if (config.allowRuntimeGrowth || config.allowPersistentGrowth ||
			config.allowTransientGrowth) {
			return unexpectedError(
				makeError(ErrorCode::StorageConfigurationInvalid, ErrorSite::StorageInitialize));
		}
	}
	return {};
}

} // namespace FlowUi::detail::storage
