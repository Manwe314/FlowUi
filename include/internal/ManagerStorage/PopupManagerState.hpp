#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "managers/structs/PopupManagerStructs.hpp"

namespace FlowUi::detail::manager_storage {

struct PopupRecord {
	Clay_ElementId rootId{};
	uint64_t submissionOrder = 0;
	Clay_BoundingBox bounds{};
	Clay_Vector2 pointerSnapshot{};
	Clay_Dimensions measuredSize{};
	int16_t zIndex = 0;
	PopupOutsidePressPolicy outsidePress = PopupOutsidePressPolicy::DismissAndBlockAnchor;
	uint32_t anchorClayId = 0;
	bool hasPointerSnapshot = false;
	bool measured = false;
	bool hasBounds = false;
	bool dismissed = false;
	bool dismissalPending = false;
	bool dismissOnEscape = true;
	bool visibleLastCommitted = false;
};

struct PopupSubmission {
	uint64_t key = 0;
	PopupFrame frame{};
};

struct PopupManagerState {
	std::unordered_map<uint64_t, PopupRecord> committedRecords{};
	std::unordered_map<uint64_t, PopupRecord> workingRecords{};
	std::unordered_map<uint64_t, size_t> currentSubmissionByKey{};
	std::vector<uint64_t> committedStack{};
	std::vector<PopupSubmission> currentSubmissions{};
	uint64_t nextSubmissionOrder = 1;
	Clay_Vector2 pointerPosition{};
	Clay_Dimensions viewport{};
	uint32_t nextPopupOffset = 0;
	uint32_t suppressedAnchorClayIdThisFrame = 0;
	bool frameActive = false;
	bool capacityWarningIssued = false;
	bool rawPrimaryPointerDownLastFrame = false;
	bool consumePrimaryPointerUntilRelease = false;
	bool suppressAllPrimaryPointerThisFrame = false;
};

} // namespace FlowUi::detail::manager_storage
