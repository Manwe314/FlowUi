#pragma once
#include "internal/MemoryDemandPeaks.hpp"

#include <unordered_map>
#include <vector>

#include "FlowUi/AppActionID.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail::manager_storage {

struct ActionManagerState {
	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	std::unordered_map<
		AppActionID,
		storage::PersistentRecordHandle,
		AppActionIDHash> bindings{};
	std::vector<storage::PersistentRecordHandle> deferredRemovals{};
};

} // namespace FlowUi::detail::manager_storage
