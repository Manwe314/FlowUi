#include "managers/ImageManager.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "internal/UiTextureRegistry.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "Vulkan/Vk_Context.hpp"
#include "stb_image.h"
#include "internal/Vma.hpp"

namespace {

struct StagingBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
};

void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

VkCommandBuffer beginOneTimeCommands(VulkanContext& vk, VkCommandPool commandPool) {
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	vkCheck(
		vkAllocateCommandBuffers(vk.device, &allocateInfo, &commandBuffer),
		"Failed to allocate image manager command buffer.");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin image manager command buffer.");
	return commandBuffer;
}

void endOneTimeCommands(VulkanContext& vk, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
	vkCheck(vkEndCommandBuffer(commandBuffer), "Failed to end image manager command buffer.");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit image manager command buffer.");
	vkCheck(vkQueueWaitIdle(vk.graphicsQ), "Failed to wait for graphics queue during image upload.");

	vkFreeCommandBuffers(vk.device, commandPool, 1, &commandBuffer);
}

void cmdTransitionImageLayout(
	VkCommandBuffer commandBuffer,
	VkImage image,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStageMask,
		dstStageMask,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);
}

StagingBuffer createStagingBuffer(VulkanContext& vk, const uint8_t* data, size_t byteCount) {
	StagingBuffer staging{};

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = byteCount;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo mappedInfo{};
	vkCheck(
		vmaCreateBuffer(
			vk.allocator,
			&bufferInfo,
			&allocationInfo,
			&staging.buffer,
			&staging.allocation,
			&mappedInfo),
		"Failed to create image upload staging buffer.");

	if (!mappedInfo.pMappedData) {
		vmaDestroyBuffer(vk.allocator, staging.buffer, staging.allocation);
		throw std::runtime_error("Failed to map image upload staging buffer.");
	}

	std::memcpy(mappedInfo.pMappedData, data, byteCount);
	vkCheck(vmaFlushAllocation(vk.allocator, staging.allocation, 0, byteCount), "Failed to flush image upload staging buffer.");
	return staging;
}

void destroyStagingBuffer(VulkanContext& vk, StagingBuffer& staging) {
	if (staging.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(vk.allocator, staging.buffer, staging.allocation);
	}
	staging.buffer = VK_NULL_HANDLE;
	staging.allocation = nullptr;
}

} // namespace

namespace FlowUi {

void ImageManager::setRegistry(detail::IUiTextureRegistry* registry) {
	registry_ = registry;
}

void ImageManager::init(VulkanContext& vk, VulkanUiRenderer& renderer, uint32_t framesInFlight) {
	destroy(vk);
	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("ImageManager init requires a valid Vulkan device + allocator.");
	}

	vk_ = &vk;
	renderer_ = &renderer;
	framesInFlight_ = std::max<uint32_t>(1u, framesInFlight);
	currentFrameIndex_ = 0u;
	retiredResourcesByFrame_.assign(framesInFlight_, {});
	imagesByKey_.clear();
	missingTextureWarnings_.clear();

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.graphicsQFamily;
	vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &uploadCommandPool_), "Failed to create image upload command pool.");
}

bool ImageManager::registerImage(std::string_view key, std::string_view filePath) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE || !renderer_) {
		throw std::runtime_error("ImageManager is not initialized.");
	}
	if (!registry_) {
		throw std::runtime_error("ImageManager registry backend is not set.");
	}
	if (key.empty()) {
		throw std::runtime_error("ImageManager key must not be empty.");
	}

	const std::filesystem::path path(filePath);
	if (!std::filesystem::is_regular_file(path)) {
		throw std::runtime_error("Image file does not exist: " + path.string());
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* decodedPixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!decodedPixels || width <= 0 || height <= 0) {
		const std::string reason = stbi_failure_reason() ? stbi_failure_reason() : "unknown decode error";
		if (decodedPixels) {
			stbi_image_free(decodedPixels);
		}
		throw std::runtime_error("Failed to decode image: " + path.string() + " (" + reason + ")");
	}

	ImageResource uploadedResource{};
	try {
		uploadedResource = createImageResource(*vk_, decodedPixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		stbi_image_free(decodedPixels);

		const std::string keyString(key);
		const std::string namespacedKey = makeNamespacedKey(key);
		bool inserted = false;
		const uint32_t assignedSlot = registry_->registerOrReplaceSlot(
			*vk_,
			namespacedKey,
			uploadedResource.view,
			uploadedResource.sampler,
			inserted);

		auto existing = imagesByKey_.find(keyString);
		if (existing != imagesByKey_.end()) {
			enqueueRetiredResource(std::move(existing->second.resource));
			existing->second = ImageRecord{
				.resource = std::move(uploadedResource),
				.slotId = assignedSlot,
				.sourceWidth = width,
				.sourceHeight = height,
				.filePath = path,
			};
			inserted = false;
		} else {
			imagesByKey_.emplace(
				keyString,
				ImageRecord{
					.resource = std::move(uploadedResource),
					.slotId = assignedSlot,
					.sourceWidth = width,
					.sourceHeight = height,
					.filePath = path,
				});
			inserted = true;
		}

		missingTextureWarnings_.erase(keyString);
		return inserted;
	} catch (...) {
		stbi_image_free(decodedPixels);
		destroyImageResource(*vk_, uploadedResource);
		throw;
	}
}

bool ImageManager::removeImage(std::string_view key) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("ImageManager is not initialized.");
	}
	if (!registry_) {
		throw std::runtime_error("ImageManager registry backend is not set.");
	}

	const auto imageIt = imagesByKey_.find(std::string(key));
	if (imageIt == imagesByKey_.end()) {
		return false;
	}

	const std::string namespacedKey = makeNamespacedKey(key);
	const bool removedFromRegistry = registry_->removeSlot(namespacedKey);
	(void)removedFromRegistry;

	enqueueRetiredResource(std::move(imageIt->second.resource));
	imagesByKey_.erase(imageIt);
	return true;
}

bool ImageManager::contains(std::string_view key) const {
	return imagesByKey_.find(std::string(key)) != imagesByKey_.end();
}

TextureRef ImageManager::getTexture(std::string_view key) const {
	TextureRef result{};

	const auto imageIt = imagesByKey_.find(std::string(key));
	if (imageIt == imagesByKey_.end()) {
		const std::string keyString(key);
		if (missingTextureWarnings_.find(keyString) == missingTextureWarnings_.end()) {
			std::fprintf(stderr, "[FlowUi] Warning: texture key '%s' was not found, using fallback texture id 0.\n", keyString.c_str());
			missingTextureWarnings_.insert(keyString);
		}
		result.id = 0u;
		return result;
	}

	result.id = imageIt->second.slotId;
	result.sourceWidth = imageIt->second.sourceWidth;
	result.sourceHeight = imageIt->second.sourceHeight;
	return result;
}

void ImageManager::onFrameStart(VulkanContext& vk, uint32_t frameIndex) {
	if (retiredResourcesByFrame_.empty()) {
		return;
	}
	currentFrameIndex_ = frameIndex % static_cast<uint32_t>(retiredResourcesByFrame_.size());
	std::vector<ImageResource>& bucket = retiredResourcesByFrame_[currentFrameIndex_];
	for (ImageResource& resource : bucket) {
		destroyImageResource(vk, resource);
	}
	bucket.clear();
}

void ImageManager::destroy(VulkanContext& vk) {
	for (auto& [_, imageRecord] : imagesByKey_) {
		destroyImageResource(vk, imageRecord.resource);
	}
	imagesByKey_.clear();

	for (std::vector<ImageResource>& bucket : retiredResourcesByFrame_) {
		for (ImageResource& resource : bucket) {
			destroyImageResource(vk, resource);
		}
		bucket.clear();
	}
	retiredResourcesByFrame_.clear();
	missingTextureWarnings_.clear();

	if (uploadCommandPool_ != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE) {
		vkDestroyCommandPool(vk.device, uploadCommandPool_, nullptr);
	}
	uploadCommandPool_ = VK_NULL_HANDLE;
	vk_ = nullptr;
	renderer_ = nullptr;
}

ImageManager::ImageResource ImageManager::createImageResource(
	VulkanContext& vk,
	const uint8_t* rgbaPixels,
	uint32_t width,
	uint32_t height) {
	ImageResource out{};

	if (!rgbaPixels || width == 0 || height == 0) {
		throw std::runtime_error("ImageManager cannot upload an empty image.");
	}
	if (uploadCommandPool_ == VK_NULL_HANDLE) {
		throw std::runtime_error("ImageManager upload command pool is not initialized.");
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vkCheck(
		vmaCreateImage(
			vk.allocator,
			&imageInfo,
			&allocationInfo,
			&out.image,
			&out.allocation,
			nullptr),
		"Failed to create image texture.");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = out.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = imageInfo.format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	vkCheck(vkCreateImageView(vk.device, &viewInfo, nullptr, &out.view), "Failed to create image texture view.");

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	vkCheck(vkCreateSampler(vk.device, &samplerInfo, nullptr, &out.sampler), "Failed to create image texture sampler.");

	StagingBuffer staging{};
	try {
		const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
		staging = createStagingBuffer(vk, rgbaPixels, byteCount);

		VkCommandBuffer commandBuffer = beginOneTimeCommands(vk, uploadCommandPool_);
		cmdTransitionImageLayout(
			commandBuffer,
			out.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = { 0, 0, 0 };
		copyRegion.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(
			commandBuffer,
			staging.buffer,
			out.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		cmdTransitionImageLayout(
			commandBuffer,
			out.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		endOneTimeCommands(vk, uploadCommandPool_, commandBuffer);
	} catch (...) {
		destroyStagingBuffer(vk, staging);
		destroyImageResource(vk, out);
		throw;
	}

	destroyStagingBuffer(vk, staging);
	return out;
}

void ImageManager::destroyImageResource(VulkanContext& vk, ImageResource& resource) {
	if (resource.sampler != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE) {
		vkDestroySampler(vk.device, resource.sampler, nullptr);
	}
	resource.sampler = VK_NULL_HANDLE;

	if (resource.view != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE) {
		vkDestroyImageView(vk.device, resource.view, nullptr);
	}
	resource.view = VK_NULL_HANDLE;

	if (resource.image != VK_NULL_HANDLE && vk.allocator != nullptr) {
		vmaDestroyImage(vk.allocator, resource.image, resource.allocation);
	}
	resource.image = VK_NULL_HANDLE;
	resource.allocation = nullptr;
}

void ImageManager::enqueueRetiredResource(ImageResource&& resource) {
	if (resource.image == VK_NULL_HANDLE && resource.view == VK_NULL_HANDLE && resource.sampler == VK_NULL_HANDLE) {
		return;
	}
	if (retiredResourcesByFrame_.empty()) {
		retiredResourcesByFrame_.resize(1);
		currentFrameIndex_ = 0u;
	}
	const uint32_t bucketIndex = currentFrameIndex_ % static_cast<uint32_t>(retiredResourcesByFrame_.size());
	retiredResourcesByFrame_[bucketIndex].push_back(std::move(resource));
}

std::string ImageManager::makeNamespacedKey(std::string_view key) const {
	return "img:" + std::string(key);
}

} // namespace FlowUi
