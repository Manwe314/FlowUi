#pragma once

/** @file
 * @brief Public AgenticImage declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "FlowUi/Public/errorManagement/Error.hpp"
#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace FlowUi::devSystems::agentic {

/** Exact producer identities and immutable RGB bytes transferred after a frame fence completes. */
struct AgenticCompletedImage {
	/** @brief Owned tightly packed RGB8 pixels in row-major order, three bytes per pixel. */
	std::vector<std::uint8_t> rgb;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/** @brief Immutable capture identity within its run. */
	std::string capture_id;
	/** @brief Immutable image identity within the capture. */
	std::string image_id;
	/** @brief Named image source, such as window or viewport. */
	std::string source = "window";
	/** @brief Stable viewport resource key for viewport image sources. */
	std::string viewport_key;
	/** @brief Inventory identity of the immutable UI tree associated with this image. */
	std::string tree_artifact_id;
	/**
	 * @brief Inventory identity of the immutable theme evidence associated with this image.
	 */
	std::string theme_artifact_id;
	/** @brief Recorded source color-space name; unknown remains explicit. */
	std::string color_space = "unknown";
	/** @brief Source pixel encoding name; native-rgb8 denotes packed RGB8 bytes. */
	std::string encoding = "native-rgb8";
	/**
	 * @brief Producer-supplied application tick identity; independent of window frame and
	 * resource slot.
	 */
	std::uint64_t app_tick = 0;
	/**
	 * @brief Producer-supplied frame identity within one window; distinct from reusable slots.
	 */
	std::uint64_t window_frame = 0;
	/**
	 * @brief Producer-supplied queue submission serial; independent of renderer frame slots.
	 */
	std::uint64_t submission_serial = 0;
	/** @brief Producer-supplied GPU device identity. */
	std::uint64_t device_id = 0;
	/** @brief Producer-supplied GPU queue identity. */
	std::uint64_t queue_id = 0;
	/** @brief Expected or observed UI tree generation, independent of layout revision. */
	std::uint64_t tree_generation = 0;
	/** @brief Theme revision associated with this selected image. */
	std::uint64_t theme_revision = 0;
	/** @brief Reflection schema generation associated with this selected image. */
	std::uint64_t schema_generation = 0;
	/** @brief Image content revision supplied by the producer. */
	std::uint64_t source_image_revision = 0;
	/** @brief Image producer application tick, independent of the sampling window frame. */
	std::uint64_t producer_app_tick = 0;
	/** @brief Image producer window-frame identity, independent of the sampling frame. */
	std::uint64_t producer_window_frame = 0;
	/** @brief Queue submission serial that produced the sampled image. */
	std::uint64_t producer_submission_serial = 0;
	/** @brief Monotonic image readback recording timestamp, in nanoseconds. */
	std::uint64_t recorded_monotonic_ns = 0;
	/** @brief Monotonic completion timestamp, in nanoseconds; zero means unavailable. */
	std::uint64_t completed_monotonic_ns = 0;
	/** @brief Measured readback copy duration, in nanoseconds. */
	std::uint64_t copy_duration_ns = 0;
	/**
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	std::uint32_t window_id = 0;
	/** @brief Reusable renderer frame slot; it is not a window-frame identity. */
	std::uint32_t frame_slot = 0;
	/** @brief RGB image width in physical pixels. */
	std::uint32_t width = 0;
	/** @brief RGB image height in physical pixels. */
	std::uint32_t height = 0;
	/** @brief Numeric source GPU pixel format recorded before RGB conversion. */
	std::uint32_t source_format = 0;
	/** @brief Original source image width before cropping, in physical pixels. */
	std::uint32_t original_width = 0;
	/** @brief Original source image height before cropping, in physical pixels. */
	std::uint32_t original_height = 0;
	/** @brief Horizontal crop origin in original source physical pixels. */
	std::uint32_t crop_x = 0;
	/** @brief Vertical crop origin in original source physical pixels. */
	std::uint32_t crop_y = 0;
	/** @brief Horizontal scale from logical source coordinates to physical pixels. */
	float logical_to_physical_scale_x = 1.0f;
	/** @brief Vertical scale from logical source coordinates to physical pixels. */
	float logical_to_physical_scale_y = 1.0f;
	/** @brief Whether associated completed UI tree evidence was captured. */
	bool tree_available = false;
	/** @brief Whether associated immutable theme evidence was captured. */
	bool theme_available = false;
	/** @brief Whether exact queue submission metadata is available. */
	bool submission_available = false;
	/** @brief Whether separate image production identity is available. */
	bool producer_available = false;
	/** @brief Whether image production belongs to the sampling frame's submission. */
	bool producer_current_submission = false;
};

/** Content production is independent of the window frame that samples a viewport image. */
struct AgenticImageProducerStamp {
	/** @brief Image content revision supplied by the producer. */
	std::uint64_t source_image_revision = 0;
	/**
	 * @brief Producer-supplied application tick identity; independent of window frame and
	 * resource slot.
	 */
	std::uint64_t app_tick = 0;
	/**
	 * @brief Producer-supplied frame identity within one window; distinct from reusable slots.
	 */
	std::uint64_t window_frame = 0;
	/**
	 * @brief Producer-supplied queue submission serial; independent of renderer frame slots.
	 */
	std::uint64_t submission_serial = 0;
	/** @brief Whether separate image content production identity is known. */
	bool available = false;
	/** @brief Whether the image producer belongs to the current queue submission. */
	bool current_submission = false;
};

/** A crop in physical source pixels; output retains the original image transform. */
struct AgenticImageCrop {
	/** @brief Horizontal crop origin in physical source pixels. */
	std::uint32_t x = 0;
	/** @brief Vertical crop origin in physical source pixels. */
	std::uint32_t y = 0;
	/** @brief Crop width in physical source pixels. */
	std::uint32_t width = 0;
	/** @brief Crop height in physical source pixels. */
	std::uint32_t height = 0;
};

/** Pixel comparison evidence; appearance acceptance remains a separate product decision. */
struct AgenticImageDifference {
	/**
	 * @brief Owned RGB image encoding raw channel differences between the compared images.
	 */
	AgenticCompletedImage heatmap;
	/**
	 * @brief Mean absolute RGB channel difference across the compared images, in [0, 255].
	 */
	double mean_absolute_channel_difference = 0.0;
	/**
	 * @brief Fraction of compared pixels whose channel difference exceeds tolerance, in [0, 1].
	 */
	double changed_pixel_fraction = 0.0;
	/** @brief Count of compared pixels whose channel difference exceeds tolerance. */
	std::uint64_t changed_pixels = 0;
	/**
	 * @brief Largest absolute RGB channel difference in the compared images, in [0, 255].
	 */
	std::uint8_t maximum_channel_difference = 0;
};

/** Serialize exact image/frame/submission identities as lossless decimal-string metadata. */
[[nodiscard]] Json image_metadata(const AgenticCompletedImage& image);
/** Restore validated frame/color/transform metadata onto decoded bytes without losing integer
 * identities. */
[[nodiscard]] Status restore_image_metadata(AgenticCompletedImage& image,
											const Json& metadata) noexcept;
/** Resolve exactly one Flow node by node_index, instance_id or debug_name, rejecting ambiguity. */
[[nodiscard]] Result<std::uint32_t> select_flow_node(const Json& tree,
													 const Json& selector) noexcept;
/** Intersect selected window-node geometry with known clips and convert it to physical image
 * pixels. */
[[nodiscard]] Result<AgenticImageCrop> flow_node_image_crop(const AgenticCompletedImage& image,
															const Json& tree,
															const Json& selector) noexcept;
/** Name the exact immutable theme/schema revision retained for a selected image. */
[[nodiscard]] std::string theme_revision_artifact_id(std::uint64_t theme_revision,
													 std::uint64_t schema_generation);

/** Check dimensions, transform and exact RGB payload length without allocating. */
[[nodiscard]] Status validate_image(const AgenticCompletedImage& image,
									std::uint64_t maximum_bytes = 134217728u) noexcept;
/** Serialize RGB8 source evidence as P6 PPM. Alpha was discarded by readback. */
[[nodiscard]] Result<std::vector<std::uint8_t>>
encode_ppm(const AgenticCompletedImage& image, std::uint64_t maximum_bytes = 134217728u) noexcept;
/** Encode a portable lossless RGB8 PNG using bounded, uncompressed DEFLATE blocks. */
[[nodiscard]] Result<std::vector<std::uint8_t>>
encode_png(const AgenticCompletedImage& image, std::uint64_t maximum_bytes = 134217728u) noexcept;
/** Decode a bounded PNG or P6 PPM, retaining no untrusted source pointers. */
[[nodiscard]] Result<AgenticCompletedImage>
decode_image(std::span<const std::uint8_t> bytes,
			 std::uint64_t maximum_bytes = 134217728u) noexcept;
/** Copy one bounded physical-pixel crop while preserving source/frame identity. */
[[nodiscard]] Result<AgenticCompletedImage>
crop_image(const AgenticCompletedImage& image, const AgenticImageCrop& crop,
		   std::uint64_t maximum_bytes = 134217728u) noexcept;
/** Arrange bounded images in row-major cells, retaining the input order. */
[[nodiscard]] Result<AgenticCompletedImage>
make_contact_sheet(std::span<const AgenticCompletedImage> images, std::uint32_t columns,
				   std::uint32_t padding = 4u, std::uint64_t maximum_bytes = 134217728u) noexcept;
/** Compare equal-size RGB images and return raw differences and an RGB heatmap. */
[[nodiscard]] Result<AgenticImageDifference>
compare_images(const AgenticCompletedImage& baseline, const AgenticCompletedImage& candidate,
			   std::uint8_t tolerance = 0u, std::uint64_t maximum_bytes = 134217728u) noexcept;

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
