#pragma once

/** @file
 * @brief Public AgenticFrameCapture declarations for optional development services.
 */

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"

#include <cstdint>
#include <string_view>
#include <vulkan/vulkan.h>

struct VulkanContext;

namespace FlowUi::devSystems::agentic::frame_capture {

/** Explicit per-frame capture selection; copied before any GPU commands are recorded. */
struct AgenticFrameCaptureRequest {
	/** @brief Copied exact producer/frame metadata to attach to completed image readback. */
	AgenticCompletedImage stamp;
	/** @brief Borrowed completion sink invoked with owned RGB bytes after the frame fence retires. */
	Status (*completed_image)(void* owner, AgenticCompletedImage&& image) noexcept = nullptr;
	/** @brief Borrowed callback owner; it must remain valid until all pending callbacks finish. */
	void* owner = nullptr;
	/** @brief Maximum retained GPU readback buffer bytes for frame capture. */
	std::uint64_t maximum_readback_bytes = 67108864u;
	/** @brief Maximum image count admitted within the configured capture bounds. */
	std::uint32_t maximum_images = 16u;
	/** @brief Whether capture requests include the native window image. */
	bool capture_window = true;
	/** @brief Whether capture requests include viewport image sources. */
	bool capture_viewports = true;
};

/** Reserve transfer-source resource support before windows/viewports are created. No I/O occurs. */
[[nodiscard]] Status configure_resources(VulkanContext& context, bool enabled) noexcept;
/** Return explicit resource enablement; this never reads the process environment. */
[[nodiscard]] bool requested(const VulkanContext& context) noexcept;
/** Whether this surface permits copying swapchain images. */
[[nodiscard]] bool supports_surface(const VulkanContext& context, VkSurfaceKHR surface) noexcept;
/** Whether a supported RGB conversion and transfer-source feature exist. */
[[nodiscard]] bool supports_format(const VulkanContext& context, VkFormat format) noexcept;
/** Select one authoritative App frame after retiring the reused slot's previous fence. */
[[nodiscard]] Status begin_frame(VulkanContext& context, std::uint64_t window_id,
								 std::uint32_t frame_slot,
								 const AgenticFrameCaptureRequest& request) noexcept;
/** Record bounded readback and restore the input layout without submitting or waiting. */
[[nodiscard]] Status record(VulkanContext& context, VkCommandBuffer command_buffer, VkImage image,
							VkExtent2D extent, VkFormat format, VkImageLayout layout,
							std::uint64_t window_id, std::uint32_t frame_slot,
							std::string_view source, std::string_view viewport_key = {},
							VkColorSpaceKHR color_space = VK_COLOR_SPACE_MAX_ENUM_KHR,
							const AgenticImageProducerStamp& producer = {}) noexcept;
/** Attach the actual successful queue submission to every selected producer image. */
void mark_submitted(VulkanContext& context, std::uint64_t window_id, std::uint32_t frame_slot,
					std::uint64_t submission_serial, std::uint64_t device_id,
					std::uint64_t queue_id) noexcept;
/** Convert fence-completed readback to owned CPU bytes; unsubmitted work is discarded. */
void complete_slot(VulkanContext& context, std::uint64_t window_id,
				   std::uint32_t frame_slot) noexcept;
/** Discard unsubmitted records; submitted buffers remain retained until their fence retires. */
void cancel_frame(VulkanContext& context, std::uint64_t window_id,
				  std::uint32_t frame_slot) noexcept;
/** Complete and release a closed window after its GPU work has drained. */
void release_window(VulkanContext& context, std::uint64_t window_id) noexcept;
/** Complete and release every readback before the allocator is destroyed, after GPU drain. */
void release_device(VulkanContext& context) noexcept;
/** Discard untrusted readback after device loss or failed drain; performs no image conversion. */
void discard_device(VulkanContext& context) noexcept;
/** Return retained GPU readback bytes for explicit service memory/quality accounting. */
[[nodiscard]] std::uint64_t pending_bytes(const VulkanContext& context) noexcept;
/** Return failed allocations/conversions/queue admissions, without recursive error reporting. */
[[nodiscard]] std::uint64_t failure_count(const VulkanContext& context) noexcept;

} // namespace FlowUi::devSystems::agentic::frame_capture

#endif
