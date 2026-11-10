#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE
#include <cstdint>
#include <string>

#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"

namespace FlowUi {

/** Immutable metadata for one window exposed to development tooling. */
struct DevWindowInfo {
	/** Stable native window identity. */
	WindowId id = InvalidWindowId;
	/** Owned native-window title snapshot. */
	std::string title{};
	/** Frame resource slot count for this window. */
	uint32_t framesInFlight = 1u;
	/** Selected backend presentation-mode numeric value. */
	uint32_t actual_present_mode = 0u;
	/** Logical DPI used for point-to-pixel text conversion. */
	float layout_dpi = 96.0f;
	/** Effective text scaling reported for this window. */
	float font_scale = 1.0f;
};
/** Portable copied viewport metadata; discovery never causes viewport rendering. */
struct DevViewportInfo {
	/** Owned viewport resource key. */
	std::string key{};
	/** Opaque texture identity associated with viewport capture. */
	TextureHandle capture_texture{};
	/** Current viewport target generation. */
	uint64_t generation = 0;
	uint64_t captured_frame = 0, /**< Application frame of the most recent capture. */
		 captured_generation = 0, /**< Target generation of the most recent capture. */
		 captured_submission = 0; /**< GPU submission of the most recent capture. */
	uint32_t width = 0, /**< Current target width in pixels. */
		 height = 0, /**< Current target height in pixels. */
		 format = 0; /**< Current backend pixel-format value. */
	uint32_t captured_width = 0, /**< Captured image width in pixels. */
		 captured_height = 0, /**< Captured image height in pixels. */
		 captured_format = 0; /**< Captured backend pixel-format value. */
	bool has_callback = false, /**< Whether a render callback is registered. */
		 clear_every_frame = true; /**< Whether each viewport pass clears its target. */
	bool capture_supported = false, /**< Whether this viewport can produce capture evidence. */
		 capture_pending = false, /**< Whether a capture request is awaiting completion. */
		 capture_failed = false; /**< Whether the most recent capture failed. */
};

} // namespace FlowUi
#endif
