#pragma once
#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"
#include "FlowUi/Private/utility/EnumFlags.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "FlowUi/Public/managerStructs/PopupTypes.hpp"

namespace FlowUi::detail::manager_storage {

enum class PopupRecordFlag : uint8_t {
	has_pointer_snapshot = 1u << 0,
	measured = 1u << 1,
	has_bounds = 1u << 2,
	dismissed = 1u << 3,
	dismissal_pending = 1u << 4,
	dismiss_on_escape = 1u << 5,
	visible_last_committed = 1u << 6,
};

struct PopupRecord {
	Clay_ElementId rootId{};
	uint64_t submissionOrder = 0;
	Clay_BoundingBox bounds{};
	Clay_Vector2 pointerSnapshot{};
	Clay_Dimensions measuredSize{};
	uint32_t anchorClayId = 0;
	int16_t zIndex = 0;
	PopupOutsidePressPolicy outsidePress = PopupOutsidePressPolicy::DismissAndBlockAnchor;
	EnumFlags<PopupRecordFlag> flags{PopupRecordFlag::dismiss_on_escape};
};

struct PopupSubmission {
	uint64_t key = 0;
	PopupFrame frame{};
};

enum class PopupManagerFlag : uint8_t {
	frame_active = 1u << 0,
	capacity_warning_issued = 1u << 1,
	raw_primary_pointer_down_last_frame = 1u << 2,
	consume_primary_pointer_until_release = 1u << 3,
	suppress_all_primary_pointer_this_frame = 1u << 4,
};

struct PopupManagerState {
	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	std::unordered_map<uint64_t, PopupRecord> committedRecords{};
	std::unordered_map<uint64_t, PopupRecord> workingRecords{};
	std::unordered_map<uint64_t, size_t> currentSubmissionByKey{};
	std::vector<uint64_t> committedStack{};
	std::vector<uint64_t> nextStack{};
	std::vector<PopupSubmission> currentSubmissions{};
	uint64_t nextSubmissionOrder = 1;
	Clay_Vector2 pointerPosition{};
	Clay_Dimensions viewport{};
	uint32_t nextPopupOffset = 0;
	uint32_t suppressedAnchorClayIdThisFrame = 0;
	EnumFlags<PopupManagerFlag> flags{};
};

} // namespace FlowUi::detail::manager_storage
