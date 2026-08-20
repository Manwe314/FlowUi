#pragma once

#include <cstdint>
#include <optional>

#include <clay.h>

#include "FlowUi/ElementID.hpp"

namespace FlowUi {

enum class PopupAttachmentPoint : uint8_t {
	TopLeft,
	TopMiddle,
	TopRight,
	MiddleLeft,
	Center,
	MiddleRight,
	BottomLeft,
	BottomMiddle,
	BottomRight,
};

[[nodiscard]] constexpr Clay_FloatingAttachPointType toClayAttachment(
	PopupAttachmentPoint point) noexcept {
	switch (point) {
	case PopupAttachmentPoint::TopLeft: return CLAY_ATTACH_POINT_LEFT_TOP;
	case PopupAttachmentPoint::TopMiddle: return CLAY_ATTACH_POINT_CENTER_TOP;
	case PopupAttachmentPoint::TopRight: return CLAY_ATTACH_POINT_RIGHT_TOP;
	case PopupAttachmentPoint::MiddleLeft: return CLAY_ATTACH_POINT_LEFT_CENTER;
	case PopupAttachmentPoint::Center: return CLAY_ATTACH_POINT_CENTER_CENTER;
	case PopupAttachmentPoint::MiddleRight: return CLAY_ATTACH_POINT_RIGHT_CENTER;
	case PopupAttachmentPoint::BottomLeft: return CLAY_ATTACH_POINT_LEFT_BOTTOM;
	case PopupAttachmentPoint::BottomMiddle: return CLAY_ATTACH_POINT_CENTER_BOTTOM;
	case PopupAttachmentPoint::BottomRight: return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
	}
	return CLAY_ATTACH_POINT_CENTER_CENTER;
}

struct PopupPlacement {
	PopupAttachmentPoint anchorPoint = PopupAttachmentPoint::BottomLeft;
	PopupAttachmentPoint popupPoint = PopupAttachmentPoint::TopLeft;
	Clay_Vector2 offset{};
};

enum class PopupAnchorKind : uint8_t {
	Parent,
	Element,
	PointerSnapshot,
	PointerFollow,
	Position,
	Viewport,
};

struct PopupAnchor {
	PopupAnchorKind kind = PopupAnchorKind::Parent;
	uint64_t elementValue = 0;
	Clay_Vector2 coordinate{};

	[[nodiscard]] static constexpr PopupAnchor parent() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::Parent};
	}

	[[nodiscard]] static constexpr PopupAnchor element(FlowElementID id) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Element,
			.elementValue = id.value,
		};
	}

	[[nodiscard]] static constexpr PopupAnchor element(GlobalFlowID id) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Element,
			.elementValue = id.value,
		};
	}

	[[nodiscard]] static constexpr PopupAnchor element(FlowElementPartID id) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Element,
			.elementValue = id.value,
		};
	}

	[[nodiscard]] static constexpr PopupAnchor pointerSnapshot() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::PointerSnapshot};
	}

	[[nodiscard]] static constexpr PopupAnchor pointerFollow() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::PointerFollow};
	}

	[[nodiscard]] static constexpr PopupAnchor position(Clay_Vector2 value) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Position,
			.coordinate = value,
		};
	}

	[[nodiscard]] static constexpr PopupAnchor viewport() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::Viewport};
	}
};

enum class PopupLayer : uint8_t {
	CasualPopup,
	WarningPopup,
	CriticalPopup,
};

enum class PopupFirstFramePolicy : uint8_t {
	DeferUntilMeasured,
	PlaceImmediately,
};

struct PopupOverflowPolicy {
	bool flipX = true;
	bool flipY = true;
	bool shiftToFit = true;
};

/** Controls both outside-press dismissal and ownership of the dismissing press. */
enum class PopupOutsidePressPolicy : uint8_t {
	/** Dismiss and hide the complete primary-pointer gesture from the UI below. */
	DismissAndConsume,
	/** Dismiss and prevent only this popup's anchor/trigger from receiving the press. */
	DismissAndBlockAnchor,
	/** Keep the popup open and leave the press untouched. */
	Ignore,
};

struct PopupRequest {
	PopupAnchor anchor = PopupAnchor::parent();
	PopupPlacement placement{};
	PopupOverflowPolicy overflow{};
	PopupFirstFramePolicy firstFrame = PopupFirstFramePolicy::DeferUntilMeasured;
	std::optional<Clay_Dimensions> expectedSize = std::nullopt;
	PopupLayer layer = PopupLayer::CasualPopup;
	Clay_PointerCaptureMode pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE;
	Clay_FloatingClipToElement clipTo = CLAY_CLIP_TO_NONE;
	PopupOutsidePressPolicy outsidePress = PopupOutsidePressPolicy::DismissAndBlockAnchor;
	bool dismissOnEscape = true;
};

struct PopupFrame {
	bool visible = false;
	bool measureOnly = false;
	Clay_FloatingElementConfig floating{};
};

} // namespace FlowUi
