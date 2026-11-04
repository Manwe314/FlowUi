#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "FlowUi/Error.hpp"
#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace FlowUi::devSystems::agentic {

/** Exact producer identities and immutable RGB bytes transferred after a frame fence completes. */
struct AgenticCompletedImage {
	std::vector<std::uint8_t> rgb;
	std::string run_id;
	std::string capture_id;
	std::string image_id;
	std::string source = "window";
	std::string viewport_key;
	std::string tree_artifact_id;
	std::string theme_artifact_id;
	std::string color_space = "unknown";
	std::string encoding = "native-rgb8";
	std::uint64_t app_tick = 0;
	std::uint64_t window_frame = 0;
	std::uint64_t submission_serial = 0;
	std::uint64_t device_id = 0;
	std::uint64_t queue_id = 0;
	std::uint64_t tree_generation = 0;
	std::uint64_t theme_revision = 0;
	std::uint64_t schema_generation = 0;
	std::uint64_t source_image_revision = 0;
	std::uint64_t producer_app_tick = 0;
	std::uint64_t producer_window_frame = 0;
	std::uint64_t producer_submission_serial = 0;
	std::uint64_t recorded_monotonic_ns = 0;
	std::uint64_t completed_monotonic_ns = 0;
	std::uint64_t copy_duration_ns = 0;
	std::uint32_t window_id = 0;
	std::uint32_t frame_slot = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t source_format = 0;
	std::uint32_t original_width = 0;
	std::uint32_t original_height = 0;
	std::uint32_t crop_x = 0;
	std::uint32_t crop_y = 0;
	float logical_to_physical_scale_x = 1.0f;
	float logical_to_physical_scale_y = 1.0f;
	bool tree_available = false;
	bool theme_available = false;
	bool submission_available = false;
	bool producer_available = false;
	bool producer_current_submission = false;
};

/** Content production is independent of the window frame that samples a viewport image. */
struct AgenticImageProducerStamp {
	std::uint64_t source_image_revision = 0;
	std::uint64_t app_tick = 0;
	std::uint64_t window_frame = 0;
	std::uint64_t submission_serial = 0;
	bool available = false;
	bool current_submission = false;
};

/** A crop in physical source pixels; output retains the original image transform. */
struct AgenticImageCrop {
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

/** Pixel comparison evidence; appearance acceptance remains a separate product decision. */
struct AgenticImageDifference {
	AgenticCompletedImage heatmap;
	double mean_absolute_channel_difference = 0.0;
	double changed_pixel_fraction = 0.0;
	std::uint64_t changed_pixels = 0;
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
