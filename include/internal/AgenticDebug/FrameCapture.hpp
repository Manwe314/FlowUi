#pragma once

#if defined(AgenticDebug) && AgenticDebug

#include <cstdint>
#include <string_view>
#include <vulkan/vulkan.h>

struct VulkanContext;

namespace FlowUi::agentic_debug {

/** Runtime opt-in, independent of developer mode. Read settings before launching the application.
 */
[[nodiscard]] bool requested() noexcept;
/** Whether this surface permits copying swapchain images. */
[[nodiscard]] bool supports_surface(const VulkanContext& context, VkSurfaceKHR surface) noexcept;
/** Whether the capture writer supports this color format and its transfer usage. */
[[nodiscard]] bool supports_format(const VulkanContext& context, VkFormat format) noexcept;
/** Record a readback after color rendering. The image must have TRANSFER_SRC usage.
 * Restores its layout; performs no queue waits. Call mark_submitted only after successful
 * submission.
 */
void record(VulkanContext& context, VkCommandBuffer command_buffer, VkImage image,
			VkExtent2D extent, VkFormat format, VkImageLayout layout, uint64_t window,
			uint32_t frame_slot, std::string_view source, std::string_view key = {}) noexcept;
/** Associate pending captures with a successfully submitted frame. */
void mark_submitted(VulkanContext& context, uint64_t window, uint32_t frame_slot) noexcept;
/** Write completed captures and discard unsubmitted captures, after the slot's fence signals. */
void complete_slot(VulkanContext& context, uint64_t window, uint32_t frame_slot) noexcept;
/** Release a closed window's captures after all of its graphics work has drained. */
void release_window(VulkanContext& context, uint64_t window) noexcept;
/** Flush and free captures before destroying the allocator, after device work has drained. */
void release_device(VulkanContext& context) noexcept;

} // namespace FlowUi::agentic_debug

#endif
