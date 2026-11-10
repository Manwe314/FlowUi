#pragma once

#include <cstdint>
#include <optional>

#include <clay.h>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"

namespace FlowUi {

/** Nine-point alignment grid used to align an anchor rectangle with a popup rectangle. */
enum class PopupAttachmentPoint : uint8_t {
	/** Top-left corner. */
	TopLeft,
	/** Center of the top edge. */
	TopMiddle,
	/** Top-right corner. */
	TopRight,
	/** Center of the left edge. */
	MiddleLeft,
	/** Rectangle center. */
	Center,
	/** Center of the right edge. */
	MiddleRight,
	/** Bottom-left corner. */
	BottomLeft,
	/** Center of the bottom edge. */
	BottomMiddle,
	/** Bottom-right corner. */
	BottomRight,
};

/** Translate a popup attachment point to its Clay equivalent; an unrecognized value maps to center. */
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

/** Align anchorPoint on the anchor with popupPoint on the popup, then add offset in layout pixels. */
struct PopupPlacement {
	/** Reference point on the anchor rectangle. */
	PopupAttachmentPoint anchorPoint = PopupAttachmentPoint::BottomLeft;
	/** Point on the popup aligned with the anchor reference point. */
	PopupAttachmentPoint popupPoint = PopupAttachmentPoint::TopLeft;
	/** Additional offset in logical layout pixels. */
	Clay_Vector2 offset{};
};

/** Source of the rectangle or coordinate used to place a popup. */
enum class PopupAnchorKind : uint8_t {
	/** Current parent element. */
	Parent,
	/** Explicit Flow element or semantic part. */
	Element,
	/** Pointer coordinate captured for the popup and retained while it remains open. */
	PointerSnapshot,
	/** Current frame pointer coordinate. */
	PointerFollow,
	/** Explicit coordinate in window layout space. */
	Position,
	/** Current window viewport rectangle. */
	Viewport,
};

/**
 * Popup anchor descriptor. Prefer the named factories so only payload fields relevant to kind are
 * populated.
 */
struct PopupAnchor {
	/** Active anchor representation. */
	PopupAnchorKind kind = PopupAnchorKind::Parent;
	/** Numeric Flow identity used only for Element anchors. */
	uint64_t elementValue = 0;
	/** Window layout coordinate used only for Position anchors. */
	Clay_Vector2 coordinate{};

	/** Anchor to the current Clay parent. */
	[[nodiscard]] static constexpr PopupAnchor parent() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::Parent};
	}

	/** Anchor to a local Flow element in the current window. */
	[[nodiscard]] static constexpr PopupAnchor element(FlowElementID id) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Element,
			.elementValue = id.value,
		};
	}

	/** Anchor to a globally named Flow element in the current window. */
	[[nodiscard]] static constexpr PopupAnchor element(GlobalFlowID id) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Element,
			.elementValue = id.value,
		};
	}

	/** Anchor to a semantic part in the current window. */
	[[nodiscard]] static constexpr PopupAnchor element(FlowElementPartID id) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Element,
			.elementValue = id.value,
		};
	}

	/** Anchor to the pointer captured when this popup first resolves its pointer position. */
	[[nodiscard]] static constexpr PopupAnchor pointerSnapshot() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::PointerSnapshot};
	}

	/** Follow the current pointer coordinate on each submitted frame. */
	[[nodiscard]] static constexpr PopupAnchor pointerFollow() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::PointerFollow};
	}

	/** Anchor at value in the current window's logical layout coordinates. */
	[[nodiscard]] static constexpr PopupAnchor position(Clay_Vector2 value) noexcept {
		return PopupAnchor{
			.kind = PopupAnchorKind::Position,
			.coordinate = value,
		};
	}

	/** Anchor to the full current window viewport rectangle. */
	[[nodiscard]] static constexpr PopupAnchor viewport() noexcept {
		return PopupAnchor{.kind = PopupAnchorKind::Viewport};
	}
};

/** Popup stacking band ordered from casual through warning to critical. */
enum class PopupLayer : uint8_t {
	/** Ordinary popup stacking band. */
	CasualPopup,
	/** Warning popup band above casual popups. */
	WarningPopup,
	/** Critical popup band above warning popups. */
	CriticalPopup,
};

/** Visibility before a popup has a measured or expected size. */
enum class PopupFirstFramePolicy : uint8_t {
	/** Use a hidden measurement frame when size is unknown. */
	DeferUntilMeasured,
	/** Place immediately even when no measured size is available. */
	PlaceImmediately,
};

/** Viewport-fit policy applied to candidate popup placements. */
struct PopupOverflowPolicy {
	/** Try horizontally mirrored attachment points when placement overflows. */
	bool flipX = true;
	/** Try vertically mirrored attachment points when placement overflows. */
	bool flipY = true;
	/** Shift the selected placement into the viewport bounds. */
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

/**
 * Per-frame popup anchor, geometry, stacking, and dismissal request. Submit once per popup ID during
 * the active UI build.
 */
struct PopupRequest {
	/** Anchor source; parent by default. */
	PopupAnchor anchor = PopupAnchor::parent();
	/** Attachment grid points and layout-pixel offset. */
	PopupPlacement placement{};
	/** Mirroring and viewport-fit behavior. */
	PopupOverflowPolicy overflow{};
	/** Placement behavior before size measurement. */
	PopupFirstFramePolicy firstFrame = PopupFirstFramePolicy::DeferUntilMeasured;
	/** Optional positive finite size estimate in layout pixels used before measurement. */
	std::optional<Clay_Dimensions> expectedSize = std::nullopt;
	/** Stacking band for this popup. */
	PopupLayer layer = PopupLayer::CasualPopup;
	/** Clay pointer capture mode applied to the floating root. */
	Clay_PointerCaptureMode pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE;
	/** Clay floating clip target applied to the popup root. */
	Clay_FloatingClipToElement clipTo = CLAY_CLIP_TO_NONE;
	/** Outside-pointer dismissal and gesture ownership policy. */
	PopupOutsidePressPolicy outsidePress = PopupOutsidePressPolicy::DismissAndBlockAnchor;
	/** Allow Escape to dismiss the active popup. */
	bool dismissOnEscape = true;
};

/**
 * Resolved popup submission. Build content for visible or measureOnly frames using floating; otherwise
 * emit no popup root.
 */
struct PopupFrame {
	/** Whether the popup is visible in this submission. */
	bool visible = false;
	/** Whether content must be emitted for hidden size measurement. */
	bool measureOnly = false;
	/** Resolved Clay floating configuration for the popup root. */
	Clay_FloatingElementConfig floating{};
};

} // namespace FlowUi
