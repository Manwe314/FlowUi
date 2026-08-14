#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "managers/structs/ShortcutManagerStructs.hpp"

namespace FlowUi::detail::manager_storage {

struct ShortcutRegistrationRecord {
	ShortcutScope scope = ShortcutScope::Global;
	int32_t priority = 0;
	ShortcutId id = 0;
	uint64_t registrationOrder = 0;
	uint32_t packedChord = 0;
	ShortcutCallback callback{};
	AppActionCall action{};
	ShortcutHandling handling = ShortcutHandling::Consume;
	bool tombstoned = false;
};

using ShortcutRegistration = std::shared_ptr<ShortcutRegistrationRecord>;
using ShortcutBucket = std::vector<ShortcutRegistration>;
using PublishedShortcutBucket = std::shared_ptr<const ShortcutBucket>;

struct ShortcutManagerState {
	std::unordered_map<uint32_t, PublishedShortcutBucket> chordBuckets{};
	std::unordered_map<ShortcutId, ShortcutRegistration> registrationsById{};
	std::unordered_map<int, uint32_t> registeredKeyRefCount{};
	uint64_t nextShortcutId = 1;
	uint64_t nextRegistrationOrder = 1;
	Clay_ElementId focusedElementId{};
};

} // namespace FlowUi::detail::manager_storage
