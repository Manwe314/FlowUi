#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/Private/renderer/ViewportStorageController.hpp"
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#include "FlowUi/Public/managers/ViewPortManager.hpp"

#include <limits>

#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp"

namespace FlowUi::detail::manager_storage {

namespace {

void vkCheck(VkResult result) {
	if (result != VK_SUCCESS) {
		ErrorCode code = result == VK_ERROR_DEVICE_LOST
			? ErrorCode::VulkanDeviceLost : ErrorCode::VulkanNativeCallFailed;
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			code = ErrorCode::AllocationFailed;
		}
		throw FlowUiException(makeError(
			code, ErrorSite::ViewportManagerInitialize, 0u, 0u,
			static_cast<std::uint32_t>(result)));
	}
}

storage::PixelFormat storageFormat(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8G8B8A8_UNORM: return storage::PixelFormat::Rgba8Unorm;
	case VK_FORMAT_R8G8B8A8_SRGB: return storage::PixelFormat::Rgba8Srgb;
	case VK_FORMAT_B8G8R8A8_UNORM: return storage::PixelFormat::Bgra8Unorm;
	case VK_FORMAT_B8G8R8A8_SRGB: return storage::PixelFormat::Bgra8Srgb;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return storage::PixelFormat::Rgba16Float;
	default: throw FlowUiException(makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportCreate));
	}
}

} // namespace

ViewportStorageController::ViewportStorageController(
	storage::IStorageSystem& storageSystem,
	VulkanContext& context,
	WindowId owningWindow,
	uint32_t frameCount)
	: storage(&storageSystem), vk(&context), window(owningWindow),
	  framesInFlight(std::max(1u, frameCount)) {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	capacity_demand.bind(storageSystem.memory_demand_control());
#endif
	::FlowUi::detail::reserve_initial_capacity(
		records, storageSystem.initial_memory_capacity(
					 FlowUi::detail::InitialCapacityTarget::viewports_records));
	::FlowUi::detail::reserve_initial_capacity(
		facades, storageSystem.initial_memory_capacity(
					 FlowUi::detail::InitialCapacityTarget::viewports_facades));
	::FlowUi::detail::reserve_initial_capacity(
		textureOwners, storageSystem.initial_memory_capacity(
						   FlowUi::detail::InitialCapacityTarget::viewports_texture_owners));
	::FlowUi::detail::reserve_initial_capacity(
		retired, storageSystem.initial_memory_capacity(
					 FlowUi::detail::InitialCapacityTarget::viewports_retired));
	const storage::StringId name = storageSystem.intern("flowui.viewport.sampler");
	sampler = storageSystem.acquireSampler(storage::SamplerDesc{ .debugName = name,
		.minFilter = storage::FilterMode::Linear, .magFilter = storage::FilterMode::Linear,
		.addressU = storage::AddressMode::ClampToEdge, .addressV = storage::AddressMode::ClampToEdge,
		.addressW = storage::AddressMode::ClampToEdge,});
	interop = ViewPortVulkanInterop{
		.instance = context.instance, .physicalDevice = context.phys,
		.device = context.device, .allocator = context.allocator,
		.graphicsQueue = context.graphicsQ, .graphicsQueueFamily = context.graphicsQFamily,
		.framesInFlight = framesInFlight,
	};
}

ViewportStorageController::~ViewportStorageController() noexcept {
	if (!storage || !vk) return;
	for (auto& [_, record] : records) {
#if FLOW_UI_DEV_MODE
		discard_dev_capture(record.pending_capture);
		discard_dev_capture(record.published_capture);
#endif
		for (TextureHandle texture : record.active.textures) {
			try { if (texture) storage->releaseAnonymousTexture(texture); } catch (...) {}
		}
		destroyCommands(record.active.commands);
		destroyImages(record.active.images);
	}
	for (RetiredViewportGeneration& generation : retired) {
		destroyCommands(generation.targets.commands);
		destroyImages(generation.targets.images);
	}
#if FLOW_UI_DEV_MODE
	for (auto& capture : retired_dev_captures) {
		capture.texture = {};
		discard_dev_capture(capture);
	}
#endif
	try { if (sampler) storage->releaseSampler(sampler); } catch (...) {}
}

#if FLOW_UI_DEV_MODE
bool ViewportStorageController::supports_dev_capture(VkFormat format) const noexcept {
	if (!vk || !vk->phys ||
		(format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R8G8B8A8_SRGB &&
		 format != VK_FORMAT_B8G8R8A8_UNORM && format != VK_FORMAT_B8G8R8A8_SRGB))
		return false;
	VkFormatProperties properties{};
	vkGetPhysicalDeviceFormatProperties(vk->phys, format, &properties);
	constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
											  VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
											  VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
	return (properties.optimalTilingFeatures & required) == required;
}

ViewportDevCapture ViewportStorageController::create_dev_capture(uint32_t width, uint32_t height,
																 VkFormat format) {
	if (!supports_dev_capture(format) || !width || !height)
		throw FlowUiException(
			makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportPrepare));
	constexpr uint64_t maximum_bytes = 64ull * 1024 * 1024;
	if (width > maximum_bytes / (static_cast<uint64_t>(height) * 4))
		throw FlowUiException(
			makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ViewportPrepare));
	const uint64_t bytes = static_cast<uint64_t>(width) * height * 4;
	if (bytes > maximum_bytes || dev_capture_bytes() > maximum_bytes - bytes)
		throw FlowUiException(
			makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ViewportPrepare));
	ViewportDevCapture capture{};
	const auto name = storage->intern("flowui.dev.viewport.observation");
	const auto pixel_format = storageFormat(format);
	try {
		capture.image.image = storage->createImage(storage::ImageDesc{
			.width = width,
			.height = height,
			.format = pixel_format,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination,
			.memory = storage::MemoryPreference::DeviceLocal,
			.sharing = storage::ResourceSharing::AppShared,
			.access = storage::AccessMode::GpuWrite,
			.debugName = name});
		capture.image.view = storage->createImageView(
			capture.image.image, storage::ImageViewDesc{.format = pixel_format, .debugName = name});
		capture.image.nativeImage = reinterpret_cast<VkImage>(
			static_cast<uintptr_t>(storage->nativeImage(capture.image.image).nativeImage));
		capture.image.width = width;
		capture.image.height = height;
		capture.texture = storage->createAnonymousTexture(
			storage::TextureViewDesc{.imageView = capture.image.view,
									 .sampler = sampler,
									 .sourceWidth = static_cast<int32_t>(width),
									 .sourceHeight = static_cast<int32_t>(height)});
		capture.format = static_cast<uint32_t>(format);
		return capture;
	} catch (...) {
		discard_dev_capture(capture);
		throw;
	}
}

void ViewportStorageController::discard_dev_capture(ViewportDevCapture& capture) noexcept {
	try {
		if (capture.texture)
			storage->releaseAnonymousTexture(capture.texture, capture.submission);
	} catch (...) {}
	try {
		if (capture.image.view)
			storage->releaseImageView(capture.image.view, capture.submission);
	} catch (...) {}
	try {
		if (capture.image.image)
			storage->releaseImage(capture.image.image, capture.submission);
	} catch (...) {}
	capture = {};
}

bool ViewportStorageController::retire_dev_capture(ViewportDevCapture& capture) noexcept {
	if (!capture.texture)
		return true;
	if (retired_dev_captures.size() == retired_dev_captures.capacity())
		return false;
	try {
		storage->releaseAnonymousTexture(capture.texture, capture.submission);
		retired_dev_captures.emplace_back(capture);
		capture = {};
		return true;
	} catch (...) {
		return false;
	}
}

void ViewportStorageController::collect_dev_captures() noexcept {
	for (auto iterator = retired_dev_captures.begin(); iterator != retired_dev_captures.end();) {
		if (!storage->textureRetirementComplete(iterator->texture)) {
			++iterator;
			continue;
		}
		iterator->texture = {};
		discard_dev_capture(*iterator);
		iterator = retired_dev_captures.erase(iterator);
	}
	for (auto& [name, record] : records) {
		if (record.capture_release_requested) {
			const bool pending_retired =
				!record.pending_capture.texture ||
				(record.pending_capture.submission && retire_dev_capture(record.pending_capture));
			const bool published_retired = retire_dev_capture(record.published_capture);
			if (pending_retired && published_retired)
				record.capture_release_requested = false;
			continue;
		}
		auto& pending = record.pending_capture;
		if (pending.submission && pending.submission <= storage->completedSerial()) {
			if (!retire_dev_capture(record.published_capture))
				continue;
			record.published_capture = pending;
			pending = {};
		}
	}
}

uint64_t ViewportStorageController::dev_capture_bytes() const noexcept {
	uint64_t bytes = 0;
	const auto add = [&](const ViewportDevCapture& capture) {
		if (capture.image.image)
			bytes += static_cast<uint64_t>(capture.image.width) * capture.image.height * 4;
	};
	for (const auto& [name, record] : records) {
		add(record.pending_capture);
		add(record.published_capture);
	}
	for (const auto& capture : retired_dev_captures)
		add(capture);
	return bytes;
}
#endif

std::vector<ViewportFrameCommands> ViewportStorageController::createCommands() const {
	std::vector<ViewportFrameCommands> resources(framesInFlight);
	try {
		for (ViewportFrameCommands& entry : resources) {
			VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			info.queueFamilyIndex = vk->graphicsQFamily;
			vkCheck(vkCreateCommandPool(vk->device, &info, nullptr, &entry.pool));
			VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			allocate.commandPool = entry.pool;
			allocate.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			allocate.commandBufferCount = 1;
			vkCheck(vkAllocateCommandBuffers(vk->device, &allocate, &entry.commandBuffer));
		}
		return resources;
	} catch (...) {
		destroyCommands(resources);
		throw;
	}
}

void ViewportStorageController::destroyCommands(std::vector<ViewportFrameCommands>& commands) const noexcept {
	for (ViewportFrameCommands& entry : commands) {
		if (entry.pool && vk && vk->device) vkDestroyCommandPool(vk->device, entry.pool, nullptr);
	}
	commands.clear();
}

ViewportImageResource ViewportStorageController::createImage(
	uint32_t width, uint32_t height, VkFormat format) const {
	if (!width || !height) throw FlowUiException(makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportCreate));
	const storage::PixelFormat pixelFormat = storageFormat(format);
	const storage::StringId name = storage->intern("flowui.viewport.target");
	ViewportImageResource result{};
	storage::ImageUsage image_usage = storage::ImageUsage::Sampled | storage::ImageUsage::ColorAttachment;
#if FLOW_UI_DEV_MODE
	if (supports_dev_capture(format))
		image_usage = image_usage | storage::ImageUsage::TransferSource;
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
	if (devSystems::agentic::frame_capture::requested(*vk) &&
		devSystems::agentic::frame_capture::supports_format(*vk, format)) {
		image_usage = image_usage | storage::ImageUsage::TransferSource;
	}
#endif
	try {
		result.image = storage->createImage(storage::ImageDesc{
			.width = width, .height = height, .format = pixelFormat,
			.usage = image_usage,
			.memory = storage::MemoryPreference::DeviceLocal,
			.sharing = storage::ResourceSharing::WindowLocal,
			.access = storage::AccessMode::GpuWrite, .window = window, .debugName = name,
		});
		result.view = storage->createImageView(result.image,
			storage::ImageViewDesc{.format = pixelFormat, .debugName = name});
		const storage::NativeImageView native = storage->nativeImage(result.image);
		result.nativeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(native.nativeImage));
		result.nativeView = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(
			storage->nativeImageView(result.view).nativeImageView));
		result.width = width;
		result.height = height;
		return result;
	} catch (...) {
		try { if (result.view) storage->releaseImageView(result.view); } catch (...) {}
		try { if (result.image) storage->releaseImage(result.image); } catch (...) {}
		throw;
	}
}

void ViewportStorageController::destroyImages(std::vector<ViewportImageResource>& images) const noexcept {
	for (ViewportImageResource& image : images) {
		try { if (image.view) storage->releaseImageView(image.view); } catch (...) {}
		try { if (image.image) storage->releaseImage(image.image); } catch (...) {}
	}
	images.clear();
}

ViewportTargetGeneration ViewportStorageController::buildTargets(
	uint32_t width, uint32_t height, VkFormat format) {
	(void)storageFormat(format);
	if (nextGeneration == std::numeric_limits<uint64_t>::max()) {
		::FlowUi::detail::terminateForFatalError(makeError(
			ErrorCode::ViewportGenerationExhausted, ErrorSite::ViewportCreate));
	}
	ViewportTargetGeneration result{};
	result.generation = nextGeneration++;
	try {
		result.commands = createCommands();
		result.images.reserve(framesInFlight);
		result.textures.reserve(framesInFlight);
		for (uint32_t slot = 0; slot < framesInFlight; ++slot) {
			result.images.push_back(createImage(width, height, format));
			const ViewportImageResource& image = result.images.back();
			result.textures.push_back(storage->createAnonymousTexture(storage::TextureViewDesc{
				.imageView = image.view, .sampler = sampler,
				.sourceWidth = static_cast<int32_t>(width), .sourceHeight = static_cast<int32_t>(height),
			}));
		}
		return result;
	} catch (...) {
		for (TextureHandle texture : result.textures) {
			try { if (texture) storage->releaseAnonymousTexture(texture); } catch (...) {}
		}
		destroyCommands(result.commands);
		destroyImages(result.images);
		throw;
	}
}

void ViewportStorageController::discardUnpublished(ViewportTargetGeneration&& generation) noexcept {
	for (TextureHandle texture : generation.textures) {
		try { if (texture) storage->releaseAnonymousTexture(texture); } catch (...) {}
	}
	destroyCommands(generation.commands);
	destroyImages(generation.images);
}

void ViewportStorageController::reserveRetirement() { retired.reserve(retired.size() + 1u); }

void ViewportStorageController::retireTargets(ViewportTargetGeneration&& generation) {
	retired.push_back(RetiredViewportGeneration{std::move(generation)});
	for (TextureHandle texture : retired.back().targets.textures) {
		textureOwners.erase(texture.packed());
		storage->releaseAnonymousTexture(texture);
	}
}

void ViewportStorageController::collectRetired() {
	FLOWUI_MEMORY_DEMAND_RECORD(
		capacity_demand, FlowUi::detail::InitialCapacityTarget::viewports_retired, retired.size());
	for (auto it = retired.begin(); it != retired.end();) {
		const bool complete = std::all_of(
			it->targets.textures.begin(), it->targets.textures.end(),
			[this](TextureHandle texture) { return storage->textureRetirementComplete(texture); });
		if (!complete) { ++it; continue; }
		destroyCommands(it->targets.commands);
		destroyImages(it->targets.images);
		it = retired.erase(it);
	}
}

} // namespace FlowUi::detail::manager_storage
