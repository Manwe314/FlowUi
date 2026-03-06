#include "managers/FontManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <artery-font/stdio-serialization.h>
#include <artery-font/std-artery-font.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vk_mem_alloc.h"

namespace {

struct DecodedAtlasImage {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> rgbaPixels;
};

struct StagingBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
};

void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

template <typename T>
const T* listData(const artery_font::StdList<T>& list) {
	return static_cast<const T*>(list);
}

std::string toStdString(const artery_font::StdString& input) {
	const char* value = static_cast<const char*>(input);
	return value ? std::string(value) : std::string();
}

std::string toLowerAscii(std::string text) {
	for (char& c : text) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return text;
}

bool isArfontPath(const std::filesystem::path& path) {
	return toLowerAscii(path.extension().string()) == ".arfont";
}

bool supportsImageEncoding(artery_font::ImageEncoding encoding) {
	return encoding == artery_font::IMAGE_PNG || encoding == artery_font::IMAGE_RAW_BINARY;
}

uint32_t nextLayerCapacity(uint32_t current, uint32_t required) {
	uint32_t capacity = std::max(current, FontManager::kInitialAtlasLayerCapacity);
	while (capacity < required) {
		if (capacity > std::numeric_limits<uint32_t>::max() - FontManager::kAtlasLayerGrowthStep) {
			throw std::runtime_error("Font atlas layer capacity overflow.");
		}
		capacity += FontManager::kAtlasLayerGrowthStep;
	}
	return capacity;
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
		"Failed to allocate font manager command buffer.");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin font manager command buffer.");
	return commandBuffer;
}

void endOneTimeCommands(VulkanContext& vk, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
	vkCheck(vkEndCommandBuffer(commandBuffer), "Failed to end font manager command buffer.");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit font manager command buffer.");
	vkCheck(vkQueueWaitIdle(vk.graphicsQ), "Failed to wait for graphics queue during font upload.");

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
	VkPipelineStageFlags dstStageMask,
	uint32_t baseLayer,
	uint32_t layerCount) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = baseLayer;
	barrier.subresourceRange.layerCount = layerCount;

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

void destroyAtlasImageStorage(VulkanContext& vk, FontManager::AtlasArrayResource& atlas) {
	if (atlas.view != VK_NULL_HANDLE) {
		vkDestroyImageView(vk.device, atlas.view, nullptr);
		atlas.view = VK_NULL_HANDLE;
	}
	if (atlas.image != VK_NULL_HANDLE) {
		vmaDestroyImage(vk.allocator, atlas.image, atlas.allocation);
		atlas.image = VK_NULL_HANDLE;
		atlas.allocation = nullptr;
	}
}

void createAtlasImageStorage(
	VulkanContext& vk,
	uint32_t width,
	uint32_t height,
	uint32_t layers,
	VkImage& outImage,
	VmaAllocation& outAllocation,
	VkImageView& outView) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent = { width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = layers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vkCheck(
		vmaCreateImage(vk.allocator, &imageInfo, &allocationInfo, &outImage, &outAllocation, nullptr),
		"Failed to create font atlas image array.");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.format = imageInfo.format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layers;
	vkCheck(vkCreateImageView(vk.device, &viewInfo, nullptr, &outView), "Failed to create font atlas image view.");
}

void createLinearSampler(VulkanContext& vk, VkSampler& outSampler) {
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
	vkCheck(vkCreateSampler(vk.device, &samplerInfo, nullptr, &outSampler), "Failed to create font atlas sampler.");
}

void transitionImageToShaderRead(VulkanContext& vk, VkCommandPool commandPool, VkImage image, uint32_t layerCount) {
	VkCommandBuffer commandBuffer = beginOneTimeCommands(vk, commandPool);
	cmdTransitionImageLayout(
		commandBuffer,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		layerCount);
	endOneTimeCommands(vk, commandPool, commandBuffer);
}

void growAtlasStorage(
	VulkanContext& vk,
	VkCommandPool commandPool,
	FontManager::AtlasArrayResource& atlas,
	uint32_t newCapacity) {
	VkImage newImage = VK_NULL_HANDLE;
	VmaAllocation newAllocation = nullptr;
	VkImageView newView = VK_NULL_HANDLE;
	createAtlasImageStorage(vk, atlas.width, atlas.height, newCapacity, newImage, newAllocation, newView);

	VkCommandBuffer commandBuffer = beginOneTimeCommands(vk, commandPool);
	cmdTransitionImageLayout(
		commandBuffer,
		newImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		newCapacity);

	if (atlas.layersUsed > 0) {
		cmdTransitionImageLayout(
			commandBuffer,
			atlas.image,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			atlas.layersUsed);

		std::vector<VkImageCopy> regions;
		regions.reserve(atlas.layersUsed);
		for (uint32_t layer = 0; layer < atlas.layersUsed; ++layer) {
			VkImageCopy region{};
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.mipLevel = 0;
			region.srcSubresource.baseArrayLayer = layer;
			region.srcSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.mipLevel = 0;
			region.dstSubresource.baseArrayLayer = layer;
			region.dstSubresource.layerCount = 1;
			region.extent = { atlas.width, atlas.height, 1 };
			regions.push_back(region);
		}

		vkCmdCopyImage(
			commandBuffer,
			atlas.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			newImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(regions.size()),
			regions.data());

		cmdTransitionImageLayout(
			commandBuffer,
			atlas.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			atlas.layersUsed);
	}

	cmdTransitionImageLayout(
		commandBuffer,
		newImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		newCapacity);
	endOneTimeCommands(vk, commandPool, commandBuffer);

	destroyAtlasImageStorage(vk, atlas);
	atlas.image = newImage;
	atlas.allocation = newAllocation;
	atlas.view = newView;
	atlas.layersCapacity = newCapacity;
	atlas.bindingRevision += 1;
}

void ensureAtlasStorageCapacity(
	VulkanContext& vk,
	VkCommandPool commandPool,
	FontManager::AtlasArrayResource& atlas,
	uint32_t width,
	uint32_t height,
	uint32_t requiredLayers) {
	if (requiredLayers == 0) {
		return;
	}

	if (atlas.image == VK_NULL_HANDLE) {
		const uint32_t initialCapacity = nextLayerCapacity(
			std::max(atlas.layersCapacity, FontManager::kInitialAtlasLayerCapacity),
			requiredLayers);

		if (atlas.sampler == VK_NULL_HANDLE) {
			createLinearSampler(vk, atlas.sampler);
		}

		VmaAllocation allocation = nullptr;
		createAtlasImageStorage(vk, width, height, initialCapacity, atlas.image, allocation, atlas.view);
		atlas.allocation = allocation;
		atlas.width = width;
		atlas.height = height;
		atlas.layersCapacity = initialCapacity;
		transitionImageToShaderRead(vk, commandPool, atlas.image, atlas.layersCapacity);
		atlas.bindingRevision += 1;
		return;
	}

	if (atlas.width != width || atlas.height != height) {
		throw std::runtime_error("All registered .arfont files must use the same atlas dimensions.");
	}

	if (requiredLayers <= atlas.layersCapacity) {
		return;
	}

	const uint32_t expandedCapacity = nextLayerCapacity(atlas.layersCapacity, requiredLayers);
	growAtlasStorage(vk, commandPool, atlas, expandedCapacity);
}

StagingBuffer createStagingBuffer(VulkanContext& vk, const uint8_t* bytes, size_t byteCount) {
	StagingBuffer staging{};

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = static_cast<VkDeviceSize>(byteCount);
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo createdAllocationInfo{};
	vkCheck(
		vmaCreateBuffer(
			vk.allocator,
			&bufferInfo,
			&allocationInfo,
			&staging.buffer,
			&staging.allocation,
			&createdAllocationInfo),
		"Failed to create font upload staging buffer.");

	if (!createdAllocationInfo.pMappedData) {
		vmaDestroyBuffer(vk.allocator, staging.buffer, staging.allocation);
		throw std::runtime_error("Failed to map font upload staging buffer.");
	}

	std::memcpy(createdAllocationInfo.pMappedData, bytes, byteCount);
	vkCheck(vmaFlushAllocation(vk.allocator, staging.allocation, 0, byteCount), "Failed to flush font upload staging buffer.");
	return staging;
}

void destroyStagingBuffer(VulkanContext& vk, StagingBuffer& staging) {
	if (staging.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(vk.allocator, staging.buffer, staging.allocation);
		staging.buffer = VK_NULL_HANDLE;
		staging.allocation = nullptr;
	}
}

void uploadLayerPixels(
	VulkanContext& vk,
	VkCommandPool commandPool,
	FontManager::AtlasArrayResource& atlas,
	uint32_t layer,
	const std::vector<uint8_t>& rgbaPixels) {
	const size_t expectedBytes = static_cast<size_t>(atlas.width) * static_cast<size_t>(atlas.height) * 4u;
	if (rgbaPixels.size() != expectedBytes) {
		throw std::runtime_error("Decoded .arfont image size does not match atlas dimensions.");
	}

	StagingBuffer staging = createStagingBuffer(vk, rgbaPixels.data(), rgbaPixels.size());
	try {
		VkCommandBuffer commandBuffer = beginOneTimeCommands(vk, commandPool);
		cmdTransitionImageLayout(
			commandBuffer,
			atlas.image,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			layer,
			1);

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = layer;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = { atlas.width, atlas.height, 1 };

		vkCmdCopyBufferToImage(
			commandBuffer,
			staging.buffer,
			atlas.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		cmdTransitionImageLayout(
			commandBuffer,
			atlas.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			layer,
			1);
		endOneTimeCommands(vk, commandPool, commandBuffer);
	} catch (...) {
		destroyStagingBuffer(vk, staging);
		throw;
	}

	destroyStagingBuffer(vk, staging);
}

int pickAtlasImageIndex(const artery_font::StdArteryFont<float>& font) {
	if (font.images.length() <= 0) {
		return -1;
	}
	const auto* images = listData(font.images);
	const auto* variants = listData(font.variants);

	if (font.variants.length() > 0) {
		const artery_font::ImageType preferredType = variants[0].imageType;
		for (int i = 0; i < font.images.length(); ++i) {
			if (images[i].imageType == preferredType && supportsImageEncoding(images[i].encoding)) {
				return i;
			}
		}
	}

	for (int i = 0; i < font.images.length(); ++i) {
		if (supportsImageEncoding(images[i].encoding)) {
			return i;
		}
	}

	return -1;
}

DecodedAtlasImage decodeImageToRgba8(const artery_font::StdArteryFont<float>::Image& image) {
	DecodedAtlasImage decoded{};
	decoded.width = image.width;
	decoded.height = image.height;

	if (decoded.width == 0 || decoded.height == 0) {
		throw std::runtime_error(".arfont image has zero dimensions.");
	}

	const auto* sourceDataBytes = static_cast<const unsigned char*>(image.data);
	const auto* sourceData = reinterpret_cast<const uint8_t*>(sourceDataBytes);
	const size_t sourceDataSize = static_cast<size_t>(image.data.length());

	switch (image.encoding) {
		case artery_font::IMAGE_PNG: {
			if (!sourceData || sourceDataSize == 0) {
				throw std::runtime_error(".arfont PNG image payload is empty.");
			}
			if (sourceDataSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
				throw std::runtime_error(".arfont PNG payload is too large for stb_image.");
			}

			int decodedWidth = 0;
			int decodedHeight = 0;
			int decodedChannels = 0;
			stbi_uc* rgbaPixels = stbi_load_from_memory(
				sourceData,
				static_cast<int>(sourceDataSize),
				&decodedWidth,
				&decodedHeight,
				&decodedChannels,
				4);
			if (!rgbaPixels) {
				const char* reason = stbi_failure_reason();
				throw std::runtime_error(
					std::string("Failed to decode .arfont PNG image: ") + (reason ? reason : "unknown error"));
			}

			const size_t decodedBytes = static_cast<size_t>(decodedWidth) * static_cast<size_t>(decodedHeight) * 4u;
			decoded.rgbaPixels.resize(decodedBytes);
			std::memcpy(decoded.rgbaPixels.data(), rgbaPixels, decodedBytes);
			stbi_image_free(rgbaPixels);

			decoded.width = static_cast<uint32_t>(decodedWidth);
			decoded.height = static_cast<uint32_t>(decodedHeight);

			if (decoded.width != image.width || decoded.height != image.height) {
				throw std::runtime_error(".arfont PNG decoded dimensions do not match image metadata.");
			}
			break;
		}

		case artery_font::IMAGE_RAW_BINARY: {
			if (image.pixelFormat != artery_font::PIXEL_UNSIGNED8) {
				throw std::runtime_error("Only unsigned 8-bit raw .arfont atlas images are supported.");
			}
			if (image.channels != 1 && image.channels != 3 && image.channels != 4) {
				throw std::runtime_error("Only 1/3/4 channel raw .arfont atlas images are supported.");
			}

			const size_t pixelRowBytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.channels);
			const size_t sourceRowBytes = (image.rawBinaryFormat.rowLength != 0)
				? static_cast<size_t>(image.rawBinaryFormat.rowLength)
				: pixelRowBytes;

			if (sourceRowBytes < pixelRowBytes) {
				throw std::runtime_error(".arfont raw image row length is smaller than required.");
			}

			const size_t requiredBytes = sourceRowBytes * static_cast<size_t>(image.height);
			if (!sourceData || sourceDataSize < requiredBytes) {
				throw std::runtime_error(".arfont raw image payload is too small.");
			}

			decoded.rgbaPixels.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4u);
			const bool topDown = image.rawBinaryFormat.orientation != artery_font::ORIENTATION_BOTTOM_UP;

			for (uint32_t y = 0; y < image.height; ++y) {
				const uint32_t sourceY = topDown ? y : (image.height - 1u - y);
				const uint8_t* srcRow = sourceData + static_cast<size_t>(sourceY) * sourceRowBytes;
				uint8_t* dstRow = decoded.rgbaPixels.data() + static_cast<size_t>(y) * static_cast<size_t>(image.width) * 4u;

				for (uint32_t x = 0; x < image.width; ++x) {
					const uint8_t* src = srcRow + static_cast<size_t>(x) * image.channels;
					uint8_t* dst = dstRow + static_cast<size_t>(x) * 4u;
					if (image.channels == 1) {
						dst[0] = src[0];
						dst[1] = src[0];
						dst[2] = src[0];
						dst[3] = 255;
					} else if (image.channels == 3) {
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = 255;
					} else {
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = src[3];
					}
				}
			}
			break;
		}

		default:
			throw std::runtime_error("Unsupported .arfont image encoding.");
	}

	return decoded;
}

std::string makeUniqueFontName(
	std::string baseName,
	const std::unordered_map<std::string, int>& existingNames) {
	if (baseName.empty()) {
		baseName = "font";
	}

	if (existingNames.find(baseName) == existingNames.end()) {
		return baseName;
	}

	for (uint32_t suffix = 1; suffix < std::numeric_limits<uint32_t>::max(); ++suffix) {
		const std::string candidate = baseName + "_" + std::to_string(suffix);
		if (existingNames.find(candidate) == existingNames.end()) {
			return candidate;
		}
	}
	throw std::runtime_error("Could not generate a unique font name.");
}

} // namespace

void FontManager::init(VulkanContext& vk, uint32_t atlasSize) {
	destroy(vk);
	atlasSizeHint_ = atlasSize;
	atlas_.layersCapacity = kInitialAtlasLayerCapacity;

	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("FontManager init requires a valid Vulkan device + allocator.");
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.graphicsQFamily;
	vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &uploadCommandPool_), "Failed to create font upload command pool.");
}

int FontManager::loadFont(VulkanContext& vk, std::string_view path, float px) {
	(void)px;
	const std::filesystem::path fontPath(path);
	if (fontPath.empty()) {
		return -1;
	}

	if (isArfontPath(fontPath)) {
		return registerOfflineBakedFont(vk, path);
	}

#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	// Runtime TTF->atlas registration is intentionally deferred to a later step.
	return -1;
#else
	return -1;
#endif
}

int FontManager::registerOfflineBakedFont(VulkanContext& vk, std::string_view arfontPath, std::string_view requestedName) {
	if (uploadCommandPool_ == VK_NULL_HANDLE) {
		throw std::runtime_error("FontManager is not initialized.");
	}

	const std::filesystem::path path(arfontPath);
	if (!isArfontPath(path)) {
		return -1;
	}
	if (!std::filesystem::is_regular_file(path)) {
		throw std::runtime_error("Font file does not exist: " + path.string());
	}

	artery_font::StdArteryFont<float> arteryFont{};
	const std::string pathString = path.string();
	if (!artery_font::readFile(arteryFont, pathString.c_str())) {
		throw std::runtime_error("Failed to read .arfont file: " + path.string());
	}
	if (arteryFont.variants.length() <= 0) {
		throw std::runtime_error(".arfont file has no font variants: " + path.string());
	}
	if (arteryFont.images.length() <= 0) {
		throw std::runtime_error(".arfont file has no atlas images: " + path.string());
	}

	const int imageIndex = pickAtlasImageIndex(arteryFont);
	if (imageIndex < 0) {
		throw std::runtime_error(".arfont file has no supported atlas image encoding.");
	}

	const auto* images = listData(arteryFont.images);
	const DecodedAtlasImage decodedImage = decodeImageToRgba8(images[imageIndex]);

	if (atlas_.layersUsed == 0 && atlasSizeHint_ > 0 &&
		(decodedImage.width != atlasSizeHint_ || decodedImage.height != atlasSizeHint_)) {
		std::fprintf(
			stderr,
			"[FlowUi] Warning: .arfont atlas %ux%u does not match configured ui.fontAtlasSize=%u.\n",
			decodedImage.width,
			decodedImage.height,
			atlasSizeHint_);
	}

	ensureAtlasStorageCapacity(vk, uploadCommandPool_, atlas_, decodedImage.width, decodedImage.height, atlas_.layersUsed + 1u);
	const uint32_t assignedLayer = atlas_.layersUsed;
	uploadLayerPixels(vk, uploadCommandPool_, atlas_, assignedLayer, decodedImage.rgbaPixels);
	atlas_.layersUsed += 1u;

	const auto* variants = listData(arteryFont.variants);
	FontFaceData fontFace{};
	fontFace.id = static_cast<int>(nextFontId_++);
	fontFace.sourcePath = path;
	fontFace.atlasLayer = assignedLayer;
	fontFace.atlasWidth = decodedImage.width;
	fontFace.atlasHeight = decodedImage.height;
	fontFace.imageType = static_cast<uint32_t>(images[imageIndex].imageType);
	fontFace.metadata = toStdString(arteryFont.metadata);
	fontFace.defaultVariantIndex = 0;
	fontFace.variants.reserve(static_cast<size_t>(arteryFont.variants.length()));

	for (int variantIndex = 0; variantIndex < arteryFont.variants.length(); ++variantIndex) {
		const auto& sourceVariant = variants[variantIndex];
		FontVariantData variant{};
		variant.flags = sourceVariant.flags;
		variant.weight = sourceVariant.weight;
		variant.fallbackGlyphIndex = sourceVariant.fallbackGlyph;
		variant.fontSizePx = sourceVariant.metrics.fontSize;
		variant.distanceRange = sourceVariant.metrics.distanceRange;
		variant.emSize = sourceVariant.metrics.emSize;
		variant.ascender = sourceVariant.metrics.ascender;
		variant.descender = sourceVariant.metrics.descender;
		variant.lineHeight = sourceVariant.metrics.lineHeight;
		variant.underlineY = sourceVariant.metrics.underlineY;
		variant.underlineThickness = sourceVariant.metrics.underlineThickness;
		variant.distanceRangeMiddle = sourceVariant.metrics.distanceRangeMiddle;
		variant.name = toStdString(sourceVariant.name);
		variant.metadata = toStdString(sourceVariant.metadata);

		const auto* sourceGlyphs = listData(sourceVariant.glyphs);
		variant.glyphs.reserve(static_cast<size_t>(sourceVariant.glyphs.length()));
		for (int glyphIndex = 0; glyphIndex < sourceVariant.glyphs.length(); ++glyphIndex) {
			const auto& sourceGlyph = sourceGlyphs[glyphIndex];
			if (sourceGlyph.image != static_cast<uint32_t>(imageIndex)) {
				throw std::runtime_error(
					".arfont references multiple atlas images per variant, which is not supported yet.");
			}

			GlyphData glyph{};
			glyph.codepoint = sourceGlyph.codepoint;
			glyph.sourceImageIndex = sourceGlyph.image;
			glyph.planeLeft = sourceGlyph.planeBounds.l;
			glyph.planeBottom = sourceGlyph.planeBounds.b;
			glyph.planeRight = sourceGlyph.planeBounds.r;
			glyph.planeTop = sourceGlyph.planeBounds.t;
			glyph.imageLeft = sourceGlyph.imageBounds.l;
			glyph.imageBottom = sourceGlyph.imageBounds.b;
			glyph.imageRight = sourceGlyph.imageBounds.r;
			glyph.imageTop = sourceGlyph.imageBounds.t;
			glyph.advanceX = sourceGlyph.advance.h;
			glyph.advanceY = sourceGlyph.advance.v;

			const uint32_t newGlyphIndex = static_cast<uint32_t>(variant.glyphs.size());
			variant.glyphs.push_back(glyph);

			if (sourceVariant.codepointType == artery_font::CP_UNICODE && glyph.codepoint != 0) {
				variant.unicodeToGlyphIndex.emplace(glyph.codepoint, newGlyphIndex);
			}
		}

		const auto* sourceKernPairs = listData(sourceVariant.kernPairs);
		for (int kernIndex = 0; kernIndex < sourceVariant.kernPairs.length(); ++kernIndex) {
			const auto& pair = sourceKernPairs[kernIndex];
			const uint64_t key = FontVariantData::kerningKey(pair.codepoint1, pair.codepoint2);
			variant.kerningPairs[key] = pair.advance.h;
		}

		if (variant.fallbackGlyphIndex >= variant.glyphs.size()) {
			variant.fallbackGlyphIndex = 0;
		}

		fontFace.variants.push_back(std::move(variant));
	}

	std::string preferredName;
	if (!requestedName.empty()) {
		preferredName = std::string(requestedName);
	} else if (!fontFace.variants.empty() && !fontFace.variants[0].name.empty()) {
		preferredName = fontFace.variants[0].name;
	} else {
		preferredName = path.stem().string();
	}
	fontFace.name = makeUniqueFontName(preferredName, fontIdByName_);

	const size_t newIndex = fonts_.size();
	fonts_.push_back(std::move(fontFace));
	fontIndexById_[fonts_.back().id] = newIndex;
	fontIdByName_[fonts_.back().name] = fonts_.back().id;
	return fonts_.back().id;
}

int FontManager::findFontByName(std::string_view fontName) const {
	const auto it = fontIdByName_.find(std::string(fontName));
	return (it != fontIdByName_.end()) ? it->second : -1;
}

const FontManager::FontFaceData* FontManager::getFontById(int fontId) const {
	const auto it = fontIndexById_.find(fontId);
	if (it == fontIndexById_.end()) {
		return nullptr;
	}
	const size_t index = it->second;
	return (index < fonts_.size()) ? &fonts_[index] : nullptr;
}

const FontManager::FontFaceData* FontManager::getFontByName(std::string_view fontName) const {
	const int fontId = findFontByName(fontName);
	return (fontId >= 0) ? getFontById(fontId) : nullptr;
}

void FontManager::destroy(VulkanContext& vk) {
	fontIdByName_.clear();
	fontIndexById_.clear();
	fonts_.clear();
	nextFontId_ = 0;
	atlasSizeHint_ = 0;

	if (vk.device != VK_NULL_HANDLE) {
		destroyAtlasImageStorage(vk, atlas_);
		if (atlas_.sampler != VK_NULL_HANDLE) {
			vkDestroySampler(vk.device, atlas_.sampler, nullptr);
		}
		if (uploadCommandPool_ != VK_NULL_HANDLE) {
			vkDestroyCommandPool(vk.device, uploadCommandPool_, nullptr);
		}
	}

	atlas_ = AtlasArrayResource{};
	atlas_.layersCapacity = kInitialAtlasLayerCapacity;
	uploadCommandPool_ = VK_NULL_HANDLE;
}
