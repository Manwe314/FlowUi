#include "managers/PopupManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/PopupManagerState.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "managers/structs/InputStructs.hpp"

namespace {

using FlowUi::PopupAnchorKind;
using FlowUi::PopupAttachmentPoint;
using FlowUi::PopupLayer;
using FlowUi::PopupOutsidePressPolicy;
using FlowUi::PopupOverflowPolicy;
using FlowUi::PopupPlacement;
using FlowUi::detail::manager_storage::PopupManagerState;
using FlowUi::detail::manager_storage::PopupRecord;

constexpr size_t kEscapeKey = 256u;
constexpr uint32_t kPopupLayerCapacity = 1000u;
constexpr std::array<int16_t, 3> kPopupLayerBase{10000, 11000, 12000};

struct ResolvedAnchor {
	Clay_FloatingAttachToElement attachTo = CLAY_ATTACH_TO_NONE;
	Clay_ElementId parentId{};
	Clay_BoundingBox bounds{};
	bool valid = false;
	bool hasGeometry = false;
	bool pointAnchor = false;
};

[[nodiscard]] Clay_ElementId clayElementId(uint64_t value) noexcept {
	const uint32_t id = FlowUi::FlowIDToClayID(FlowUi::FlowElementID{.value = value});
	return Clay_ElementId{
		.id = id,
		.offset = 0,
		.baseId = id,
		.stringId = {},
	};
}

[[nodiscard]] size_t layerIndex(PopupLayer layer) noexcept {
	switch (layer) {
	case PopupLayer::CasualPopup: return 0;
	case PopupLayer::WarningPopup: return 1;
	case PopupLayer::CriticalPopup: return 2;
	}
	return 0;
}

[[nodiscard]] int pointX(PopupAttachmentPoint point) noexcept {
	return static_cast<int>(point) % 3;
}

[[nodiscard]] int pointY(PopupAttachmentPoint point) noexcept {
	return static_cast<int>(point) / 3;
}

[[nodiscard]] PopupAttachmentPoint makePoint(int x, int y) noexcept {
	return static_cast<PopupAttachmentPoint>(y * 3 + x);
}

[[nodiscard]] PopupAttachmentPoint mirrorPointX(PopupAttachmentPoint point) noexcept {
	return makePoint(2 - pointX(point), pointY(point));
}

[[nodiscard]] PopupAttachmentPoint mirrorPointY(PopupAttachmentPoint point) noexcept {
	return makePoint(pointX(point), 2 - pointY(point));
}

[[nodiscard]] PopupPlacement mirrorPlacementX(PopupPlacement placement) noexcept {
	placement.anchorPoint = mirrorPointX(placement.anchorPoint);
	placement.popupPoint = mirrorPointX(placement.popupPoint);
	placement.offset.x = -placement.offset.x;
	return placement;
}

[[nodiscard]] PopupPlacement mirrorPlacementY(PopupPlacement placement) noexcept {
	placement.anchorPoint = mirrorPointY(placement.anchorPoint);
	placement.popupPoint = mirrorPointY(placement.popupPoint);
	placement.offset.y = -placement.offset.y;
	return placement;
}

[[nodiscard]] bool samePlacement(const PopupPlacement& lhs, const PopupPlacement& rhs) noexcept {
	return lhs.anchorPoint == rhs.anchorPoint && lhs.popupPoint == rhs.popupPoint &&
		lhs.offset.x == rhs.offset.x && lhs.offset.y == rhs.offset.y;
}

[[nodiscard]] float axisPoint(float start, float extent, int point) noexcept {
	if (point == 1) return start + extent * 0.5f;
	if (point == 2) return start + extent;
	return start;
}

[[nodiscard]] Clay_BoundingBox popupBounds(
	const Clay_BoundingBox& anchor,
	Clay_Dimensions popupSize,
	const PopupPlacement& placement) noexcept {
	const float anchorX = axisPoint(anchor.x, anchor.width, pointX(placement.anchorPoint));
	const float anchorY = axisPoint(anchor.y, anchor.height, pointY(placement.anchorPoint));
	const float popupOriginX = axisPoint(0.0f, popupSize.width, pointX(placement.popupPoint));
	const float popupOriginY = axisPoint(0.0f, popupSize.height, pointY(placement.popupPoint));
	return Clay_BoundingBox{
		.x = anchorX - popupOriginX + placement.offset.x,
		.y = anchorY - popupOriginY + placement.offset.y,
		.width = popupSize.width,
		.height = popupSize.height,
	};
}

[[nodiscard]] bool fits(const Clay_BoundingBox& bounds, const Clay_BoundingBox& viewport) noexcept {
	return bounds.x >= viewport.x && bounds.y >= viewport.y &&
		bounds.x + bounds.width <= viewport.x + viewport.width &&
		bounds.y + bounds.height <= viewport.y + viewport.height;
}

[[nodiscard]] float overflowScore(
	const Clay_BoundingBox& bounds,
	const Clay_BoundingBox& viewport) noexcept {
	const float left = std::max(viewport.x - bounds.x, 0.0f);
	const float right = std::max(
		bounds.x + bounds.width - (viewport.x + viewport.width), 0.0f);
	const float top = std::max(viewport.y - bounds.y, 0.0f);
	const float bottom = std::max(
		bounds.y + bounds.height - (viewport.y + viewport.height), 0.0f);
	return left + right + top + bottom;
}

[[nodiscard]] PopupPlacement resolveOverflow(
	PopupPlacement requested,
	const PopupOverflowPolicy& policy,
	const Clay_BoundingBox& anchor,
	Clay_Dimensions popupSize,
	const Clay_BoundingBox& viewport) noexcept {
	const Clay_BoundingBox requestedBounds = popupBounds(anchor, popupSize, requested);
	if (fits(requestedBounds, viewport)) return requested;

	std::array<PopupPlacement, 4> candidates{};
	size_t candidateCount = 0;
	auto appendCandidate = [&](PopupPlacement candidate) {
		for (size_t i = 0; i < candidateCount; ++i) {
			if (samePlacement(candidates[i], candidate)) return;
		}
		candidates[candidateCount++] = candidate;
	};

	appendCandidate(requested);
	if (policy.flipY) appendCandidate(mirrorPlacementY(requested));
	if (policy.flipX) appendCandidate(mirrorPlacementX(requested));
	if (policy.flipX && policy.flipY) {
		appendCandidate(mirrorPlacementX(mirrorPlacementY(requested)));
	}

	size_t selectedIndex = 0;
	float selectedScore = overflowScore(requestedBounds, viewport);
	for (size_t i = 1; i < candidateCount; ++i) {
		const Clay_BoundingBox bounds = popupBounds(anchor, popupSize, candidates[i]);
		if (fits(bounds, viewport)) return candidates[i];
		const float score = overflowScore(bounds, viewport);
		if (score < selectedScore) {
			selectedIndex = i;
			selectedScore = score;
		}
	}

	PopupPlacement resolved = candidates[selectedIndex];
	if (!policy.shiftToFit) return resolved;

	const Clay_BoundingBox selectedBounds = popupBounds(anchor, popupSize, resolved);
	const float maximumX = viewport.x + std::max(0.0f, viewport.width - popupSize.width);
	const float maximumY = viewport.y + std::max(0.0f, viewport.height - popupSize.height);
	const float correctedX = std::clamp(selectedBounds.x, viewport.x, maximumX);
	const float correctedY = std::clamp(selectedBounds.y, viewport.y, maximumY);
	resolved.offset.x += correctedX - selectedBounds.x;
	resolved.offset.y += correctedY - selectedBounds.y;
	return resolved;
}

[[nodiscard]] bool validExpectedSize(const std::optional<Clay_Dimensions>& size) noexcept {
	return size.has_value() && std::isfinite(size->width) && std::isfinite(size->height) &&
		size->width >= 0.0f && size->height >= 0.0f;
}

[[nodiscard]] bool containsPoint(
	const Clay_BoundingBox& bounds,
	float x,
	float y) noexcept {
	return x >= bounds.x && x <= bounds.x + bounds.width &&
		y >= bounds.y && y <= bounds.y + bounds.height;
}

bool markDismissed(PopupRecord& record) noexcept {
	if (record.dismissed) return false;
	record.dismissed = true;
	record.dismissalPending = true;
	return true;
}

ResolvedAnchor resolveAnchor(
	const FlowUi::PopupAnchor& anchor,
	PopupRecord& record,
	const PopupManagerState& state) {
	ResolvedAnchor result{};
	switch (anchor.kind) {
	case PopupAnchorKind::Parent: {
		result.attachTo = CLAY_ATTACH_TO_PARENT;
		result.valid = true;
		const uint32_t openParentId = Clay_GetOpenElementId();
		if (openParentId != 0) {
			result.parentId = Clay_ElementId{
				.id = openParentId,
				.offset = 0,
				.baseId = openParentId,
				.stringId = {},
			};
			const Clay_ElementData parentData = Clay_GetElementData(result.parentId);
			if (parentData.found) {
				result.bounds = parentData.boundingBox;
				result.hasGeometry = true;
			}
		}
		break;
	}
	case PopupAnchorKind::Element: {
		if (anchor.elementValue == 0) break;
		result.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
		result.parentId = clayElementId(anchor.elementValue);
		const Clay_ElementData parentData = Clay_GetElementData(result.parentId);
		if (!parentData.found) break;
		result.bounds = parentData.boundingBox;
		result.valid = true;
		result.hasGeometry = true;
		break;
	}
	case PopupAnchorKind::PointerSnapshot:
		if (!record.hasPointerSnapshot) {
			record.pointerSnapshot = state.pointerPosition;
			record.hasPointerSnapshot = true;
		}
		result.attachTo = CLAY_ATTACH_TO_ROOT;
		result.bounds = Clay_BoundingBox{
			.x = record.pointerSnapshot.x,
			.y = record.pointerSnapshot.y,
		};
		result.valid = true;
		result.hasGeometry = true;
		result.pointAnchor = true;
		break;
	case PopupAnchorKind::PointerFollow:
		result.attachTo = CLAY_ATTACH_TO_ROOT;
		result.bounds = Clay_BoundingBox{
			.x = state.pointerPosition.x,
			.y = state.pointerPosition.y,
		};
		result.valid = true;
		result.hasGeometry = true;
		result.pointAnchor = true;
		break;
	case PopupAnchorKind::Position:
		result.attachTo = CLAY_ATTACH_TO_ROOT;
		result.bounds = Clay_BoundingBox{
			.x = anchor.coordinate.x,
			.y = anchor.coordinate.y,
		};
		result.valid = true;
		result.hasGeometry = true;
		result.pointAnchor = true;
		break;
	case PopupAnchorKind::Viewport:
		result.attachTo = CLAY_ATTACH_TO_ROOT;
		result.bounds = Clay_BoundingBox{
			.x = 0.0f,
			.y = 0.0f,
			.width = state.viewport.width,
			.height = state.viewport.height,
		};
		result.valid = true;
		result.hasGeometry = true;
		break;
	}
	return result;
}

Clay_FloatingElementConfig makeFloating(
	const ResolvedAnchor& anchor,
	const PopupPlacement& placement,
	int16_t zIndex,
	Clay_PointerCaptureMode pointerCaptureMode,
	Clay_FloatingClipToElement clipTo) noexcept {
	Clay_FloatingElementConfig floating{};
	floating.offset = placement.offset;
	floating.parentId = anchor.parentId.id;
	floating.zIndex = zIndex;
	floating.attachPoints = Clay_FloatingAttachPoints{
		.element = FlowUi::toClayAttachment(placement.popupPoint),
		.parent = FlowUi::toClayAttachment(placement.anchorPoint),
	};
	floating.pointerCaptureMode = pointerCaptureMode;
	floating.attachTo = anchor.attachTo;
	floating.clipTo = clipTo;

	if (anchor.pointAnchor) {
		floating.offset.x += anchor.bounds.x;
		floating.offset.y += anchor.bounds.y;
		floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP;
	}
	return floating;
}

Clay_FloatingElementConfig makeMeasurementFloating(
	const ResolvedAnchor& anchor,
	const PopupPlacement& placement,
	int16_t zIndex,
	const PopupManagerState& state) noexcept {
	Clay_FloatingElementConfig floating = makeFloating(
		anchor,
		placement,
		zIndex,
		CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
		CLAY_CLIP_TO_NONE);
	floating.offset.x += state.viewport.width + 4096.0f;
	floating.offset.y += state.viewport.height + 4096.0f;
	return floating;
}

} // namespace

namespace FlowUi {

namespace manager_storage = detail::manager_storage;
namespace storage = detail::storage;

void PopupManager::init(storage::IStorageSystem& storageSystem, WindowId window) {
	if (storage_) throw std::logic_error("PopupManager is already initialized.");
	const storage::StringId name = storageSystem.intern("flowui.popup.root");
	const storage::ResourceKey key{storage::ResourceDomain::Layout, name, window};
	const storage::ManagerRecordHandle handle = manager_storage::createState<PopupManagerState>(
		storageSystem, key, storage::ResourceKind::PopupManager, name);
	storage_ = &storageSystem;
	window_ = window;
	stateHandle_ = handle.packed();
}

void PopupManager::destroy() noexcept {
	if (!storage_) return;
	try {
		const storage::StringId name = storage_->intern("flowui.popup.root");
		(void)storage_->removeManagerRecord(
			storage::ResourceKey{storage::ResourceDomain::Layout, name, window_},
			storage::ResourceKind::PopupManager);
	} catch (...) {
	}
	storage_ = nullptr;
	window_ = InvalidWindowId;
	stateHandle_ = 0;
}

PopupManagerState& PopupManager::state() {
	auto* result = manager_storage::state<PopupManagerState>(
		storage_, storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::PopupManager);
	if (!result) throw std::logic_error("PopupManager is not attached to a live window storage scope.");
	return *result;
}

const PopupManagerState& PopupManager::state() const {
	const auto* result = manager_storage::state<PopupManagerState>(
		storage_, storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::PopupManager);
	if (!result) throw std::logic_error("PopupManager is not attached to a live window storage scope.");
	return *result;
}

void PopupManager::beginFrame(
	const FrameInput& currentInput,
	const FrameInput& previousInput,
	float viewportWidth,
	float viewportHeight) {
	auto& current = state();
	if (current.frameActive) {
		throw std::logic_error("PopupManager beginFrame called while a popup frame is already active.");
	}
	current.workingRecords = current.committedRecords;
	current.currentSubmissions.clear();
	current.currentSubmissionByKey.clear();
	current.pointerPosition = Clay_Vector2{currentInput.mouseX, currentInput.mouseY};
	current.viewport = Clay_Dimensions{
		.width = std::max(1.0f, viewportWidth),
		.height = std::max(1.0f, viewportHeight),
	};
	current.nextPopupOffset = 0;
	current.capacityWarningIssued = false;
	current.suppressAllPrimaryPointerThisFrame = false;
	current.suppressedAnchorClayIdThisFrame = 0;
	current.frameActive = true;

	bool dismissedAutomatically = false;
	const bool escapePressed = kEscapeKey < currentInput.keyDown.size() &&
		currentInput.keyDown[kEscapeKey] && !previousInput.keyDown[kEscapeKey];
	if (escapePressed) {
		for (auto it = current.committedStack.rbegin(); it != current.committedStack.rend(); ++it) {
			auto recordIt = current.workingRecords.find(*it);
			if (recordIt == current.workingRecords.end() ||
				!recordIt->second.visibleLastCommitted ||
				!recordIt->second.dismissOnEscape) continue;
			dismissedAutomatically = markDismissed(recordIt->second);
			if (dismissedAutomatically) break;
		}
	}

	const bool pointerPressed = currentInput.mouseDown[0] && !current.rawPrimaryPointerDownLastFrame;
	current.rawPrimaryPointerDownLastFrame = currentInput.mouseDown[0];
	if (current.consumePrimaryPointerUntilRelease) {
		if (currentInput.mouseDown[0]) {
			current.suppressAllPrimaryPointerThisFrame = true;
		} else {
			current.consumePrimaryPointerUntilRelease = false;
		}
	}
	if (!dismissedAutomatically && pointerPressed) {
		for (auto it = current.committedStack.rbegin(); it != current.committedStack.rend(); ++it) {
			auto recordIt = current.workingRecords.find(*it);
			if (recordIt == current.workingRecords.end()) continue;
			PopupRecord& record = recordIt->second;
			if (!record.visibleLastCommitted || !record.hasBounds) continue;
			if (containsPoint(record.bounds, currentInput.mouseX, currentInput.mouseY)) break;
			switch (record.outsidePress) {
			case PopupOutsidePressPolicy::Ignore:
				// The topmost popup owns the outside-press decision; do not reach through it.
				return;
			case PopupOutsidePressPolicy::DismissAndConsume:
				if (markDismissed(record)) {
					current.consumePrimaryPointerUntilRelease = true;
					current.suppressAllPrimaryPointerThisFrame = true;
				}
				return;
			case PopupOutsidePressPolicy::DismissAndBlockAnchor:
				if (markDismissed(record)) {
					current.suppressedAnchorClayIdThisFrame = record.anchorClayId;
				}
				return;
			}
		}
	}
}

bool PopupManager::suppressesAllPrimaryPointerInput() const {
	return state().suppressAllPrimaryPointerThisFrame;
}

uint32_t PopupManager::suppressedAnchorClayId() const {
	return state().suppressedAnchorClayIdThisFrame;
}

PopupFrame PopupManager::request(FlowElementID popupId, const PopupRequest& requestValue) {
	return requestImpl(popupId.value, requestValue);
}

PopupFrame PopupManager::request(GlobalFlowID popupId, const PopupRequest& requestValue) {
	return requestImpl(popupId.value, requestValue);
}

PopupFrame PopupManager::request(FlowElementPartID popupId, const PopupRequest& requestValue) {
	return requestImpl(popupId.value, requestValue);
}

PopupFrame PopupManager::requestImpl(uint64_t popupKey, const PopupRequest& requestValue) {
	auto& current = state();
	if (!current.frameActive) {
		throw std::logic_error("PopupManager::request requires an active UiManager frame.");
	}
	if (popupKey == 0) return {};

	if (const auto duplicate = current.currentSubmissionByKey.find(popupKey);
		duplicate != current.currentSubmissionByKey.end()) {
#if FLOW_UI_DEV_MODE
		std::fprintf(stderr, "[FlowUi] Warning: duplicate popup submission for id %llu.\n",
			static_cast<unsigned long long>(popupKey));
#endif
		return current.currentSubmissions[duplicate->second].frame;
	}

	auto recordIt = current.workingRecords.try_emplace(popupKey).first;
	PopupRecord& record = recordIt->second;
	record.rootId = clayElementId(popupKey);
	record.outsidePress = requestValue.outsidePress;
	record.dismissOnEscape = requestValue.dismissOnEscape;
	record.submissionOrder = current.nextSubmissionOrder++;

	PopupFrame frame{};
	if (!record.dismissed) {
		const ResolvedAnchor anchor = resolveAnchor(requestValue.anchor, record, current);
		if (anchor.valid) {
			record.anchorClayId = anchor.parentId.id;
			const uint32_t offset = std::min(current.nextPopupOffset, kPopupLayerCapacity - 1u);
			if (current.nextPopupOffset < std::numeric_limits<uint32_t>::max()) {
				++current.nextPopupOffset;
			}
			if (offset == kPopupLayerCapacity - 1u &&
				current.nextPopupOffset > kPopupLayerCapacity && !current.capacityWarningIssued) {
#if FLOW_UI_DEV_MODE
				std::fprintf(stderr,
					"[FlowUi] Warning: popup z-index capacity exceeded for this window frame.\n");
#endif
				current.capacityWarningIssued = true;
			}
			record.zIndex = static_cast<int16_t>(
				kPopupLayerBase[layerIndex(requestValue.layer)] + offset);

			const bool hasMeasuredSize = record.measured;
			const bool hasExpectedSize = validExpectedSize(requestValue.expectedSize);
			const bool hasKnownSize = hasMeasuredSize || hasExpectedSize;
			const Clay_Dimensions knownSize = hasMeasuredSize
				? record.measuredSize
				: (hasExpectedSize ? *requestValue.expectedSize : Clay_Dimensions{});

			if (!hasKnownSize &&
				requestValue.firstFrame == PopupFirstFramePolicy::DeferUntilMeasured) {
				frame.measureOnly = true;
				frame.floating = makeMeasurementFloating(
					anchor, requestValue.placement, record.zIndex, current);
			} else {
				PopupPlacement placement = requestValue.placement;
				if (hasKnownSize && anchor.hasGeometry) {
					const Clay_BoundingBox viewport{
						.x = 0.0f,
						.y = 0.0f,
						.width = current.viewport.width,
						.height = current.viewport.height,
					};
					placement = resolveOverflow(
						placement, requestValue.overflow, anchor.bounds, knownSize, viewport);
				}
				frame.visible = true;
				frame.floating = makeFloating(
					anchor,
					placement,
					record.zIndex,
					requestValue.pointerCaptureMode,
					requestValue.clipTo);
			}
		}
#if FLOW_UI_DEV_MODE
		else if (requestValue.anchor.kind == PopupAnchorKind::Element) {
			std::fprintf(stderr, "[FlowUi] Warning: popup anchor %llu was not found.\n",
				static_cast<unsigned long long>(requestValue.anchor.elementValue));
		}
#endif
	}

	const size_t submissionIndex = current.currentSubmissions.size();
	current.currentSubmissions.push_back(manager_storage::PopupSubmission{
		.key = popupKey,
		.frame = frame,
	});
	current.currentSubmissionByKey.emplace(popupKey, submissionIndex);
	return frame;
}

void PopupManager::dismiss(FlowElementID popupId) { dismissImpl(popupId.value); }
void PopupManager::dismiss(GlobalFlowID popupId) { dismissImpl(popupId.value); }
void PopupManager::dismiss(FlowElementPartID popupId) { dismissImpl(popupId.value); }

void PopupManager::dismissImpl(uint64_t popupKey) {
	if (popupKey == 0) return;
	auto& current = state();
	auto& records = current.frameActive ? current.workingRecords : current.committedRecords;
	PopupRecord& record = records[popupKey];
	if (!markDismissed(record)) return;

	if (current.frameActive) {
		if (const auto submission = current.currentSubmissionByKey.find(popupKey);
			submission != current.currentSubmissionByKey.end()) {
			PopupFrame& frame = current.currentSubmissions[submission->second].frame;
			frame.visible = false;
			frame.measureOnly = false;
		}
	} else {
		storage_->noteManagerMutation(window_);
	}
}

bool PopupManager::consumeDismissed(FlowElementID popupId) {
	return consumeDismissedImpl(popupId.value);
}

bool PopupManager::consumeDismissed(GlobalFlowID popupId) {
	return consumeDismissedImpl(popupId.value);
}

bool PopupManager::consumeDismissed(FlowElementPartID popupId) {
	return consumeDismissedImpl(popupId.value);
}

bool PopupManager::consumeDismissedImpl(uint64_t popupKey) {
	if (popupKey == 0) return false;
	auto& current = state();
	auto& records = current.frameActive ? current.workingRecords : current.committedRecords;
	const auto record = records.find(popupKey);
	if (record == records.end() || !record->second.dismissalPending) return false;
	record->second.dismissalPending = false;
	if (!current.frameActive) storage_->noteManagerMutation(window_);
	return true;
}

void PopupManager::endFrame() {
	auto& current = state();
	if (!current.frameActive) return;

	std::vector<uint64_t> nextStack;
	nextStack.reserve(current.currentSubmissions.size());
	for (const manager_storage::PopupSubmission& submission : current.currentSubmissions) {
		auto recordIt = current.workingRecords.find(submission.key);
		if (recordIt == current.workingRecords.end()) continue;
		PopupRecord& record = recordIt->second;
		record.visibleLastCommitted = false;
		record.hasBounds = false;
		if (!submission.frame.visible && !submission.frame.measureOnly) continue;

		const Clay_ElementData elementData = Clay_GetElementData(record.rootId);
		if (!elementData.found) continue;
		record.measuredSize = Clay_Dimensions{
			.width = std::max(0.0f, elementData.boundingBox.width),
			.height = std::max(0.0f, elementData.boundingBox.height),
		};
		record.measured = true;
		if (!submission.frame.visible) continue;
		record.bounds = elementData.boundingBox;
		record.hasBounds = true;
		record.visibleLastCommitted = true;
		nextStack.push_back(submission.key);
	}

	for (auto it = current.workingRecords.begin(); it != current.workingRecords.end();) {
		if (!current.currentSubmissionByKey.contains(it->first)) {
			it = current.workingRecords.erase(it);
		} else {
			++it;
		}
	}

	std::stable_sort(nextStack.begin(), nextStack.end(), [&](uint64_t lhs, uint64_t rhs) {
		const PopupRecord& left = current.workingRecords.at(lhs);
		const PopupRecord& right = current.workingRecords.at(rhs);
		if (left.zIndex != right.zIndex) return left.zIndex < right.zIndex;
		return left.submissionOrder < right.submissionOrder;
	});

	current.committedRecords.swap(current.workingRecords);
	current.committedStack.swap(nextStack);
	current.workingRecords.clear();
	current.currentSubmissions.clear();
	current.currentSubmissionByKey.clear();
	current.frameActive = false;
	storage_->noteManagerMutation(window_);
}

void PopupManager::cancelFrame() noexcept {
	if (!storage_) return;
	try {
		auto& current = state();
		current.workingRecords.clear();
		current.currentSubmissions.clear();
		current.currentSubmissionByKey.clear();
		current.frameActive = false;
		current.nextPopupOffset = 0;
		current.capacityWarningIssued = false;
	} catch (...) {
	}
}

} // namespace FlowUi
