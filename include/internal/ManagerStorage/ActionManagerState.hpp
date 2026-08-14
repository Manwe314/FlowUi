#pragma once

#include <unordered_map>
#include <vector>

#include "FlowUi/AppActionID.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail::manager_storage {

struct ActionManagerState {
	std::unordered_map<
		AppActionID,
		storage::PersistentRecordHandle,
		AppActionIDHash> bindings{};
	std::vector<storage::PersistentRecordHandle> deferredRemovals{};
};

} // namespace FlowUi::detail::manager_storage
