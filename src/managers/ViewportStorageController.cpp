#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "managers/ViewPortManager.hpp"
#include "internal/ManagerStorage/ViewportStorageController.hpp"

#include <limits>

#include "Vulkan/Vk_Context.hpp"

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
	const storage::StringId name = storageSystem.intern("flowui.viewport.sampler");
	sampler = storageSystem.acquireSampler(storage::SamplerDesc{
		.minFilter = storage::FilterMode::Linear, .magFilter = storage::FilterMode::Linear,
		.addressU = storage::AddressMode::ClampToEdge, .addressV = storage::AddressMode::ClampToEdge,
		.addressW = storage::AddressMode::ClampToEdge, .debugName = name,
	});
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
	try { if (sampler) storage->releaseSampler(sampler); } catch (...) {}
}

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
	try {
		result.image = storage->createImage(storage::ImageDesc{
			.width = width, .height = height, .format = pixelFormat,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::ColorAttachment,
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
