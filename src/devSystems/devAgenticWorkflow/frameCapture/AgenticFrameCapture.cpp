#include "devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp"

#if FLOW_UI_AGENTIC_WORKFLOW

#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "FlowUi/Private/storageSystem/vulkan/Vma.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace FlowUi::devSystems::agentic::frame_capture {
namespace {

struct Capture {
	AgenticCompletedImage image;
	VmaAllocator allocator{};
	VmaAllocation allocation{};
	VkBuffer buffer{};
	void* mapped{};
	VkDeviceSize bytes{};
	VkFormat format{};
	~Capture() {
		if (buffer)
			vmaDestroyBuffer(allocator, buffer, allocation);
	}
};

struct Slot {
	std::vector<std::unique_ptr<Capture>> captures;
	AgenticFrameCaptureRequest request;
	std::uint32_t index = 0;
	bool admitted = false;
	bool submitted = false;
};

struct Window {
	std::vector<Slot> slots;
	std::uint64_t id = 0;
};

struct Device {
	std::vector<Window> windows;
	VkDevice handle{};
	std::uint64_t sequence = 0;
	std::uint64_t pending_bytes = 0;
	std::uint64_t failures = 0;
	bool enabled = false;
};

struct Completion {
	AgenticCompletedImage image;
	Status (*sink)(void*, AgenticCompletedImage&&) noexcept = nullptr;
	void* owner = nullptr;
};

std::mutex capture_mutex;
std::vector<std::unique_ptr<Device>> devices;
std::atomic<bool> any_enabled{false};

[[nodiscard]] FlowUiError capture_error(ErrorCode code) noexcept {
	return makeError(code, ErrorSite::AgenticImage);
}

[[nodiscard]] std::uint64_t monotonic_ns() noexcept {
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
										  std::chrono::steady_clock::now().time_since_epoch())
										  .count());
}

[[nodiscard]] Device* find_device(VkDevice handle) noexcept {
	for (const auto& device : devices)
		if (device->handle == handle)
			return device.get();
	return nullptr;
}

[[nodiscard]] Slot* find_slot(Device& device, std::uint64_t window_id,
							  std::uint32_t frame_slot) noexcept {
	for (auto& window : device.windows)
		if (window.id == window_id)
			for (auto& slot : window.slots)
				if (slot.index == frame_slot)
					return &slot;
	return nullptr;
}

[[nodiscard]] std::uint8_t half_to_byte(const std::uint8_t* data) noexcept {
	std::uint16_t bits{};
	std::memcpy(&bits, data, sizeof(bits));
	const std::uint32_t exponent = (bits >> 10u) & 31u;
	const std::uint32_t mantissa = bits & 1023u;
	float value = exponent == 0u ? std::ldexp(static_cast<float>(mantissa), -24)
								 : std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f,
											  static_cast<int>(exponent) - 15);
	if (bits & 0x8000u)
		value = 0.0f;
	if (exponent == 31u)
		value = mantissa ? 0.0f : ((bits & 0x8000u) ? 0.0f : 1.0f);
	return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void finish_slot(Device& device, Slot& slot, std::vector<Completion>& completions) noexcept {
	for (auto& capture : slot.captures) {
		if (slot.submitted) {
			try {
				const std::uint64_t copy_begin = monotonic_ns();
				if (vmaInvalidateAllocation(capture->allocator, capture->allocation, 0u,
											capture->bytes) != VK_SUCCESS) {
					++device.failures;
				} else {
					const bool half_float = capture->format == VK_FORMAT_R16G16B16A16_SFLOAT;
					const bool bgra = capture->format == VK_FORMAT_B8G8R8A8_UNORM ||
									  capture->format == VK_FORMAT_B8G8R8A8_SRGB;
					const std::size_t pixel_count =
						static_cast<std::size_t>(capture->image.width) * capture->image.height;
					capture->image.rgb.resize(pixel_count * 3u);
					const auto* pixels = static_cast<const std::uint8_t*>(capture->mapped);
					for (std::size_t pixel = 0u; pixel < pixel_count; ++pixel)
						for (std::size_t channel = 0u; channel < 3u; ++channel)
							capture->image.rgb[pixel * 3u + channel] =
								half_float ? half_to_byte(pixels + pixel * 8u + channel * 2u)
										   : pixels[pixel * 4u + (bgra ? 2u - channel : channel)];
					capture->image.completed_monotonic_ns = monotonic_ns();
					capture->image.copy_duration_ns =
						capture->image.completed_monotonic_ns - copy_begin;
					completions.emplace_back(Completion{std::move(capture->image),
														slot.request.completed_image,
														slot.request.owner});
				}
			} catch (...) {
				++device.failures;
			}
		}
		device.pending_bytes -= capture->bytes;
	}
	slot.captures.clear();
	slot.submitted = false;
	slot.admitted = false;
	slot.request = {};
}

void deliver(VkDevice handle, std::vector<Completion>& completions) noexcept {
	std::uint64_t failures = 0;
	for (auto& completion : completions)
		if (!completion.sink || !completion.sink(completion.owner, std::move(completion.image)))
			++failures;
	if (failures) {
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(handle))
			device->failures += failures;
	}
}

} // namespace

Status configure_resources(VulkanContext& context, bool enabled) noexcept {
	try {
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device)) {
			if (!enabled && device->pending_bytes)
				return unexpectedError(capture_error(ErrorCode::AgenticBusy));
			device->enabled = enabled;
			any_enabled.store(std::any_of(devices.begin(), devices.end(),
										  [](const auto& candidate) { return candidate->enabled; }),
							  std::memory_order_relaxed);
			return {};
		}
		if (!enabled)
			return {};
		if (!context.device)
			return unexpectedError(capture_error(ErrorCode::AgenticUnavailable));
		auto device = std::make_unique<Device>();
		device->handle = context.device;
		device->enabled = true;
		device->windows.reserve(4u);
		devices.emplace_back(std::move(device));
		any_enabled.store(true, std::memory_order_relaxed);
		return {};
	} catch (...) {
		return unexpectedError(capture_error(ErrorCode::AllocationFailed));
	}
}

bool requested(const VulkanContext& context) noexcept {
	if (!any_enabled.load(std::memory_order_relaxed))
		return false;
	std::scoped_lock lock(capture_mutex);
	const auto* device = find_device(context.device);
	return device && device->enabled;
}

bool supports_surface(const VulkanContext& context, VkSurfaceKHR surface) noexcept {
	VkSurfaceCapabilitiesKHR capabilities{};
	return vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.phys, surface, &capabilities) ==
			   VK_SUCCESS &&
		   (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u;
}

bool supports_format(const VulkanContext& context, VkFormat format) noexcept {
	if (format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R8G8B8A8_SRGB &&
		format != VK_FORMAT_B8G8R8A8_UNORM && format != VK_FORMAT_B8G8R8A8_SRGB &&
		format != VK_FORMAT_R16G16B16A16_SFLOAT)
		return false;
	VkFormatProperties properties{};
	vkGetPhysicalDeviceFormatProperties(context.phys, format, &properties);
	return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0u;
}

Status begin_frame(VulkanContext& context, std::uint64_t window_id, std::uint32_t frame_slot,
				   const AgenticFrameCaptureRequest& request) noexcept {
	try {
		if (!request.completed_image || request.stamp.run_id.empty() ||
			request.stamp.capture_id.empty() || request.stamp.window_id != window_id ||
			request.stamp.frame_slot != frame_slot || !request.maximum_images ||
			request.maximum_images > 1024u || !request.maximum_readback_bytes ||
			!request.stamp.rgb.empty())
			return unexpectedError(capture_error(ErrorCode::AgenticInvalidRequest));
		std::scoped_lock lock(capture_mutex);
		auto* device = find_device(context.device);
		if (!device || !device->enabled)
			return unexpectedError(capture_error(ErrorCode::AgenticUnavailable));
		Slot* selected_slot = find_slot(*device, window_id, frame_slot);
		if (!selected_slot) {
			auto selected_window =
				std::find_if(device->windows.begin(), device->windows.end(),
							 [window_id](const Window& window) { return window.id == window_id; });
			if (selected_window == device->windows.end()) {
				device->windows.emplace_back();
				selected_window = std::prev(device->windows.end());
				selected_window->id = window_id;
				selected_window->slots.reserve(4u);
			}
			selected_window->slots.emplace_back();
			selected_slot = &selected_window->slots.back();
			selected_slot->index = frame_slot;
		}
		if (!selected_slot->captures.empty() || selected_slot->submitted)
			return unexpectedError(capture_error(ErrorCode::AgenticBusy));
		selected_slot->request = request;
		selected_slot->captures.reserve(request.maximum_images);
		selected_slot->admitted = true;
		return {};
	} catch (...) {
		return unexpectedError(capture_error(ErrorCode::AllocationFailed));
	}
}

Status record(VulkanContext& context, VkCommandBuffer command_buffer, VkImage image,
			  VkExtent2D extent, VkFormat format, VkImageLayout layout, std::uint64_t window_id,
			  std::uint32_t frame_slot, std::string_view source, std::string_view viewport_key,
			  VkColorSpaceKHR color_space, const AgenticImageProducerStamp& producer) noexcept {
	if (!any_enabled.load(std::memory_order_relaxed))
		return {};
	try {
		std::scoped_lock lock(capture_mutex);
		auto* device = find_device(context.device);
		if (!device || !device->enabled)
			return {};
		auto* slot = find_slot(*device, window_id, frame_slot);
		if (!slot || !slot->admitted || slot->submitted)
			return {};
		if ((source == "window" && !slot->request.capture_window) ||
			(source == "viewport" && !slot->request.capture_viewports))
			return {};
		if (!context.allocator || !command_buffer || !image || !extent.width || !extent.height ||
			!supports_format(context, format)) {
			++device->failures;
			return unexpectedError(capture_error(ErrorCode::AgenticUnavailable));
		}
		if ((layout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL &&
			 layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ||
			(source != "window" && source != "viewport")) {
			++device->failures;
			return unexpectedError(capture_error(ErrorCode::AgenticInvalidRequest));
		}
		const std::uint64_t pixel_count = static_cast<std::uint64_t>(extent.width) * extent.height;
		const std::uint64_t stride = format == VK_FORMAT_R16G16B16A16_SFLOAT ? 8u : 4u;
		if (pixel_count > std::numeric_limits<std::size_t>::max() / stride ||
			pixel_count > std::numeric_limits<std::size_t>::max() / 3u ||
			slot->captures.size() >= slot->request.maximum_images ||
			pixel_count > slot->request.maximum_readback_bytes / (stride + 3u) ||
			device->pending_bytes >
				slot->request.maximum_readback_bytes - pixel_count * (stride + 3u)) {
			++device->failures;
			return unexpectedError(capture_error(ErrorCode::AgenticBudgetExhausted));
		}
		auto capture = std::make_unique<Capture>();
		capture->image = slot->request.stamp;
		capture->image.image_id =
			slot->request.stamp.image_id.empty()
				? "image_" + std::to_string(device->sequence++)
				: slot->request.stamp.image_id + "_" + std::to_string(device->sequence++);
		capture->image.width = extent.width;
		capture->image.height = extent.height;
		capture->image.original_width = extent.width;
		capture->image.original_height = extent.height;
		capture->image.source_format = static_cast<std::uint32_t>(format);
		capture->image.source = source;
		capture->image.viewport_key = viewport_key;
		capture->image.source_image_revision = producer.source_image_revision;
		capture->image.producer_app_tick = producer.app_tick;
		capture->image.producer_window_frame = producer.window_frame;
		capture->image.producer_submission_serial = producer.submission_serial;
		capture->image.producer_available = producer.available;
		capture->image.producer_current_submission = producer.current_submission;
		if (source == "window") {
			capture->image.source_image_revision = capture->image.window_frame;
			capture->image.producer_app_tick = capture->image.app_tick;
			capture->image.producer_window_frame = capture->image.window_frame;
			capture->image.producer_available = true;
			capture->image.producer_current_submission = true;
		}
		capture->image.color_space =
			color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ? "srgb_nonlinear"
			: color_space == VK_COLOR_SPACE_MAX_ENUM_KHR
				? "unknown"
				: "vulkan:" + std::to_string(static_cast<std::uint32_t>(color_space));
		capture->image.encoding =
			format == VK_FORMAT_R16G16B16A16_SFLOAT ? "linear-clamped-rgb8" : "native-rgb8";
		capture->image.recorded_monotonic_ns = monotonic_ns();
		capture->allocator = context.allocator;
		capture->format = format;
		capture->bytes = pixel_count * stride;
		VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		buffer_info.size = capture->bytes;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VmaAllocationCreateInfo allocation_info{};
		allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocation_info.flags =
			VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo native{};
		const VkResult allocation_result =
			vmaCreateBuffer(context.allocator, &buffer_info, &allocation_info, &capture->buffer,
							&capture->allocation, &native);
		if (allocation_result != VK_SUCCESS || !native.pMappedData) {
			++device->failures;
			return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticImage,
											 0u, capture->bytes,
											 static_cast<std::uint32_t>(allocation_result)));
		}
		capture->mapped = native.pMappedData;
		device->pending_bytes += capture->bytes;
		// Transfer sole buffer ownership before recording commands that reference it.
		slot->captures.emplace_back(std::move(capture));
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.image = image;
		barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.oldLayout = layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(
			command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
		VkBufferImageCopy copy{};
		copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
		copy.imageExtent = {extent.width, extent.height, 1u};
		vkCmdCopyImageToBuffer(command_buffer, image, barrier.newLayout,
							   slot->captures.back()->buffer, 1u, &copy);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = layout;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		const bool attachment = layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		barrier.dstAccessMask =
			attachment ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
					   : VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 attachment ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
										: VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
		VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &host, 0u, nullptr, 0u, nullptr);
		return {};
	} catch (...) {
		return unexpectedError(capture_error(ErrorCode::AllocationFailed));
	}
}

void mark_submitted(VulkanContext& context, std::uint64_t window_id, std::uint32_t frame_slot,
					std::uint64_t submission_serial, std::uint64_t device_id,
					std::uint64_t queue_id) noexcept {
	std::scoped_lock lock(capture_mutex);
	if (auto* device = find_device(context.device))
		if (auto* slot = find_slot(*device, window_id, frame_slot)) {
			slot->submitted = true;
			for (auto& capture : slot->captures) {
				capture->image.submission_serial = submission_serial;
				capture->image.device_id = device_id;
				capture->image.queue_id = queue_id;
				capture->image.submission_available = true;
				if (capture->image.producer_current_submission) {
					capture->image.producer_submission_serial = submission_serial;
					capture->image.producer_current_submission = false;
				}
			}
		}
}

void complete_slot(VulkanContext& context, std::uint64_t window_id,
				   std::uint32_t frame_slot) noexcept {
	std::vector<Completion> completions;
	{
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device))
			if (auto* slot = find_slot(*device, window_id, frame_slot))
				finish_slot(*device, *slot, completions);
	}
	deliver(context.device, completions);
}

void cancel_frame(VulkanContext& context, std::uint64_t window_id,
				  std::uint32_t frame_slot) noexcept {
	std::scoped_lock lock(capture_mutex);
	if (auto* device = find_device(context.device))
		if (auto* slot = find_slot(*device, window_id, frame_slot)) {
			slot->admitted = false;
			if (!slot->submitted) {
				for (const auto& capture : slot->captures)
					device->pending_bytes -= capture->bytes;
				slot->captures.clear();
				slot->request = {};
			}
		}
}

void release_window(VulkanContext& context, std::uint64_t window_id) noexcept {
	std::vector<Completion> completions;
	{
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device)) {
			for (auto& window : device->windows)
				if (window.id == window_id)
					for (auto& slot : window.slots)
						finish_slot(*device, slot, completions);
			std::erase_if(device->windows,
						  [window_id](const Window& window) { return window.id == window_id; });
		}
	}
	deliver(context.device, completions);
}

void release_device(VulkanContext& context) noexcept {
	std::vector<Completion> completions;
	{
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device))
			for (auto& window : device->windows)
				for (auto& slot : window.slots)
					finish_slot(*device, slot, completions);
	}
	deliver(context.device, completions);
	std::scoped_lock lock(capture_mutex);
	std::erase_if(devices, [&](const auto& device) { return device->handle == context.device; });
	any_enabled.store(std::any_of(devices.begin(), devices.end(),
								  [](const auto& device) { return device->enabled; }),
					  std::memory_order_relaxed);
}

void discard_device(VulkanContext& context) noexcept {
	std::scoped_lock lock(capture_mutex);
	// The caller has established device loss or drained execution. No mapped bytes
	// from an unsuccessful submission/drain are promoted into completed evidence.
	std::erase_if(devices, [&](const auto& device) { return device->handle == context.device; });
	any_enabled.store(std::any_of(devices.begin(), devices.end(),
								  [](const auto& device) { return device->enabled; }),
					  std::memory_order_relaxed);
}

std::uint64_t pending_bytes(const VulkanContext& context) noexcept {
	std::scoped_lock lock(capture_mutex);
	const auto* device = find_device(context.device);
	return device ? device->pending_bytes : 0u;
}

std::uint64_t failure_count(const VulkanContext& context) noexcept {
	std::scoped_lock lock(capture_mutex);
	const auto* device = find_device(context.device);
	return device ? device->failures : 0u;
}

} // namespace FlowUi::devSystems::agentic::frame_capture

#endif
