#include "internal/AgenticDebug/FrameCapture.hpp"

#if defined(AgenticDebug) && AgenticDebug

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Vulkan/Vk_Context.hpp"
#include "FlowUi/Resources.hpp"
#include "internal/Vma.hpp"

namespace FlowUi::agentic_debug {
namespace {

struct Capture {
	VmaAllocator allocator{};
	VmaAllocation allocation{};
	VkBuffer buffer{};
	void* mapped{};
	VkDeviceSize bytes{};
	uint64_t sequence{};
	uint64_t frame_number{};
	VkExtent2D extent{};
	VkFormat format{};
	std::string source;
	std::string key;
	~Capture() {
		if (buffer)
			vmaDestroyBuffer(allocator, buffer, allocation);
	}
};

struct Slot {
	std::vector<std::unique_ptr<Capture>> captures;
	uint32_t index{};
	bool submitted{};
};

struct Window {
	std::vector<Slot> slots;
	uint64_t id{};
	uint64_t submitted_frames{};
};

struct Device {
	std::vector<Window> windows;
	std::filesystem::path directory;
	VkDevice handle{};
	uint64_t start_frame{};
	uint64_t frame_count{8u};
	uint64_t sequence{};
};

std::mutex capture_mutex;
std::vector<std::unique_ptr<Device>> devices;

void report_failure() noexcept {
	std::fputs("AgenticDebug: frame capture failed (allocation, Vulkan readback, or output I/O).\n",
			   stderr);
}

[[nodiscard]] uint64_t setting(const char* name, uint64_t fallback) noexcept {
	const char* value = std::getenv(name);
	if (!value || !*value)
		return fallback;
	uint64_t parsed{};
	const auto result = std::from_chars(value, value + std::strlen(value), parsed);
	if (result.ec != std::errc{} || *result.ptr != '\0') {
		std::fprintf(stderr, "AgenticDebug: invalid %s; using default.\n", name);
		return fallback;
	}
	return parsed;
}

[[nodiscard]] Device* find_device(VkDevice handle) noexcept {
	for (const auto& device : devices)
		if (device->handle == handle)
			return device.get();
	return nullptr;
}

Device& acquire_device(VkDevice handle) {
	if (auto* existing = find_device(handle))
		return *existing;
	auto device = std::make_unique<Device>();
	device->handle = handle;
	device->start_frame = setting("FLOWUI_AGENTIC_DEBUG_START_FRAME", 0u);
	device->frame_count = setting("FLOWUI_AGENTIC_DEBUG_FRAME_COUNT", 8u);
#if defined(_WIN32)
	const std::filesystem::path root(_wgetenv(L"FLOWUI_AGENTIC_DEBUG_DIR"));
#else
	const std::filesystem::path root(std::getenv("FLOWUI_AGENTIC_DEBUG_DIR"));
#endif
	std::filesystem::create_directories(root);
	const auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	// Atomic directory creation avoids overwriting captures from concurrent processes.
	for (uint64_t suffix = 0u;; ++suffix) {
		device->directory =
			root / ("capture-" + std::to_string(timestamp) + "-" + std::to_string(suffix));
		if (std::filesystem::create_directory(device->directory))
			break;
	}
	std::fprintf(stderr, "AgenticDebug: captures in %s\n", path_to_utf8(device->directory).c_str());
	devices.emplace_back(std::move(device));
	return *devices.back();
}

Window& acquire_window(Device& device, uint64_t id) {
	for (auto& window : device.windows)
		if (window.id == id)
			return window;
	device.windows.emplace_back();
	device.windows.back().id = id;
	return device.windows.back();
}

Slot& acquire_slot(Window& window, uint32_t index) {
	for (auto& slot : window.slots)
		if (slot.index == index)
			return slot;
	window.slots.emplace_back();
	window.slots.back().index = index;
	return window.slots.back();
}

[[nodiscard]] std::string json_string(std::string_view value) {
	constexpr char digits[] = "0123456789abcdef";
	std::string result;
	result.reserve(value.size() + 2u);
	result += '"';
	for (const unsigned char character : value) {
		if (character == '"' || character == '\\') {
			result += '\\';
			result += static_cast<char>(character);
		} else if (character < 0x20u) {
			result += "\\u00";
			result += digits[character >> 4u];
			result += digits[character & 15u];
		} else
			result += static_cast<char>(character);
	}
	result += '"';
	return result;
}

[[nodiscard]] unsigned char half_to_byte(const unsigned char* data) noexcept {
	uint16_t bits{};
	std::memcpy(&bits, data, sizeof(bits));
	const uint32_t exponent = (bits >> 10u) & 31u;
	const uint32_t mantissa = bits & 1023u;
	float value = exponent == 0u ? std::ldexp(static_cast<float>(mantissa), -24)
								 : std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f,
											  static_cast<int>(exponent) - 15);
	if (bits & 0x8000u)
		value = 0.0f;
	if (exponent == 31u)
		value = mantissa ? 0.0f : ((bits & 0x8000u) ? 0.0f : 1.0f);
	return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void write_capture(const Device& device, const Window& window, const Slot& slot,
				   const Capture& capture) {
	if (vmaInvalidateAllocation(capture.allocator, capture.allocation, 0u, capture.bytes) !=
		VK_SUCCESS) {
		report_failure();
		return;
	}
	const bool half_float = capture.format == VK_FORMAT_R16G16B16A16_SFLOAT;
	const bool bgra =
		capture.format == VK_FORMAT_B8G8R8A8_UNORM || capture.format == VK_FORMAT_B8G8R8A8_SRGB;
	const size_t pixel_count = static_cast<size_t>(capture.extent.width) * capture.extent.height;
	std::vector<unsigned char> rgb(pixel_count * 3u);
	const auto* pixels = static_cast<const unsigned char*>(capture.mapped);
	for (size_t pixel = 0u; pixel < pixel_count; ++pixel) {
		for (size_t channel = 0u; channel < 3u; ++channel) {
			rgb[pixel * 3u + channel] = half_float
											? half_to_byte(pixels + pixel * 8u + channel * 2u)
											: pixels[pixel * 4u + (bgra ? 2u - channel : channel)];
		}
	}
	const std::string filename = "frame-" + std::to_string(capture.sequence) + ".ppm";
	std::ofstream output(device.directory / filename, std::ios::binary);
	output << "P6\n" << capture.extent.width << ' ' << capture.extent.height << "\n255\n";
	output.write(reinterpret_cast<const char*>(rgb.data()),
				 static_cast<std::streamsize>(rgb.size()));
	output.close();
	if (!output) {
		report_failure();
		return;
	}
	std::ofstream manifest(device.directory / "frames.jsonl", std::ios::app);
	manifest << "{\"schema\":1,\"file\":" << json_string(filename)
			 << ",\"sequence\":" << capture.sequence << ",\"window\":" << window.id
			 << ",\"frame\":" << capture.frame_number << ",\"frame_slot\":" << slot.index
			 << ",\"source\":" << json_string(capture.source)
			 << ",\"key\":" << json_string(capture.key) << ",\"width\":" << capture.extent.width
			 << ",\"height\":" << capture.extent.height
			 << ",\"vk_format\":" << static_cast<uint32_t>(capture.format)
			 << ",\"origin\":\"top-left\",\"alpha\":\"discarded\",\"encoding\":"
			 << json_string(half_float ? "linear-clamped-rgb8" : "native-rgb8") << "}\n";
	manifest.close();
	if (!manifest)
		report_failure();
}

void finish_slot(const Device& device, const Window& window, Slot& slot) noexcept {
	if (slot.submitted) {
		for (const auto& capture : slot.captures) {
			try {
				write_capture(device, window, slot, *capture);
			} catch (...) {
				report_failure();
			}
		}
	}
	slot.captures.clear();
	slot.submitted = false;
}

} // namespace

bool requested() noexcept {
#if defined(_WIN32)
	const wchar_t* directory = _wgetenv(L"FLOWUI_AGENTIC_DEBUG_DIR");
#else
	const char* directory = std::getenv("FLOWUI_AGENTIC_DEBUG_DIR");
#endif
	return directory && *directory;
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

void record(VulkanContext& context, VkCommandBuffer command_buffer, VkImage image,
			VkExtent2D extent, VkFormat format, VkImageLayout layout, uint64_t window_id,
			uint32_t frame_slot, std::string_view source, std::string_view key) noexcept {
	if (!requested() || !context.allocator || !command_buffer || !image || !extent.width ||
		!extent.height || !supports_format(context, format))
		return;
	if (layout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL &&
		layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		return;
	try {
		std::scoped_lock lock(capture_mutex);
		Device& device = acquire_device(context.device);
		Window& window = acquire_window(device, window_id);
		if (window.submitted_frames < device.start_frame ||
			window.submitted_frames - device.start_frame >= device.frame_count)
			return;
		Slot& slot = acquire_slot(window, frame_slot);
		if (slot.submitted)
			return;
		auto capture = std::make_unique<Capture>();
		capture->allocator = context.allocator;
		capture->extent = extent;
		capture->format = format;
		capture->source = source;
		capture->key = key;
		capture->sequence = device.sequence++;
		capture->frame_number = window.submitted_frames;
		const uint64_t pixel_count = static_cast<uint64_t>(extent.width) * extent.height;
		const uint64_t stride = format == VK_FORMAT_R16G16B16A16_SFLOAT ? 8u : 4u;
		if (pixel_count > std::numeric_limits<size_t>::max() / stride ||
			pixel_count > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) / 3u)
			return;
		capture->bytes = pixel_count * stride;
		VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		buffer_info.size = capture->bytes;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VmaAllocationCreateInfo allocation_info{};
		allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocation_info.flags =
			VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo native{};
		if (vmaCreateBuffer(context.allocator, &buffer_info, &allocation_info, &capture->buffer,
							&capture->allocation, &native) != VK_SUCCESS) {
			report_failure();
			return;
		}
		capture->mapped = native.pMappedData;
		// Transfer ownership before recording: allocation failures must never free a referenced
		// buffer.
		slot.captures.emplace_back(std::move(capture));
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.image = image;
		barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.oldLayout = layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		// Include the viewport's preceding transition-to-sampling dependency in the chain.
		vkCmdPipelineBarrier(
			command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
		VkBufferImageCopy copy{};
		copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
		copy.imageExtent = {extent.width, extent.height, 1u};
		vkCmdCopyImageToBuffer(command_buffer, image, barrier.newLayout,
							   slot.captures.back()->buffer, 1u, &copy);
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
	} catch (...) {
		report_failure();
	}
}

void mark_submitted(VulkanContext& context, uint64_t window_id, uint32_t frame_slot) noexcept {
	if (!requested())
		return;
	try {
		std::scoped_lock lock(capture_mutex);
		auto& window = acquire_window(acquire_device(context.device), window_id);
		acquire_slot(window, frame_slot).submitted = true;
		++window.submitted_frames;
	} catch (...) {
		report_failure();
	}
}

void complete_slot(VulkanContext& context, uint64_t window_id, uint32_t frame_slot) noexcept {
	try {
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device)) {
			for (auto& window : device->windows)
				if (window.id == window_id) {
					for (auto& slot : window.slots)
						if (slot.index == frame_slot)
							finish_slot(*device, window, slot);
				}
		}
	} catch (...) {
		report_failure();
	}
}

void release_window(VulkanContext& context, uint64_t window_id) noexcept {
	try {
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device)) {
			for (auto& window : device->windows)
				if (window.id == window_id) {
					for (auto& slot : window.slots)
						finish_slot(*device, window, slot);
				}
			std::erase_if(device->windows,
						  [=](const Window& window) { return window.id == window_id; });
		}
	} catch (...) {
		report_failure();
	}
}

void release_device(VulkanContext& context) noexcept {
	try {
		std::scoped_lock lock(capture_mutex);
		if (auto* device = find_device(context.device)) {
			for (auto& window : device->windows) {
				for (auto& slot : window.slots)
					finish_slot(*device, window, slot);
			}
		}
		std::erase_if(devices,
					  [&](const auto& device) { return device->handle == context.device; });
	} catch (...) {
		report_failure();
	}
}

} // namespace FlowUi::agentic_debug

#endif
