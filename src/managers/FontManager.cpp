#include "managers/FontManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <artery-font/stdio-serialization.h>
#include <artery-font/std-artery-font.h>
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#endif

#include "Vulkan/Vk_Context.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "internal/Vma.hpp"

namespace Font = FlowUi::Font;

namespace {

#if defined(FLOWUI_RUNTIME_FONT_BAKING)
constexpr double kDefaultRuntimeFontPxRange = 2.0;
constexpr double kDefaultRuntimeFontAngleThreshold = 3.0;
constexpr double kDefaultRuntimeFontMiterLimit = 1.0;
constexpr unsigned long long kRuntimeFontLcgMultiplier = 6364136223846793005ull;
constexpr unsigned long long kRuntimeFontLcgIncrement = 1442695040888963407ull;
#endif

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

void destroyAtlasImageStorage(VulkanContext& vk, Font::AtlasArrayResource& atlas) {
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
	Font::AtlasArrayResource& atlas,
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
	Font::AtlasArrayResource& atlas,
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
	Font::AtlasArrayResource& atlas,
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

std::vector<uint8_t> copyAtlasIntoPage(
	const DecodedAtlasImage& source,
	uint32_t pageWidth,
	uint32_t pageHeight,
	const std::filesystem::path& sourcePath) {
	if (pageWidth == 0 || pageHeight == 0) {
		throw std::runtime_error("Font atlas page size must be greater than zero.");
	}
	if (source.width > pageWidth || source.height > pageHeight) {
		throw std::runtime_error(
			".arfont atlas " + std::to_string(source.width) + "x" + std::to_string(source.height) +
			" is larger than configured ui.fontAtlasSize=" + std::to_string(pageWidth) +
			" for " + sourcePath.string());
	}

	const size_t pageRowBytes = static_cast<size_t>(pageWidth) * 4u;
	const size_t sourceRowBytes = static_cast<size_t>(source.width) * 4u;
	std::vector<uint8_t> pagePixels(static_cast<size_t>(pageWidth) * static_cast<size_t>(pageHeight) * 4u, 0u);

	for (uint32_t y = 0; y < source.height; ++y) {
		const size_t sourceOffset = static_cast<size_t>(y) * sourceRowBytes;
		const size_t pageOffset = static_cast<size_t>(y) * pageRowBytes;
		std::memcpy(pagePixels.data() + pageOffset, source.rgbaPixels.data() + sourceOffset, sourceRowBytes);
	}

	return pagePixels;
}

std::string makeUniqueFontName(
	std::string baseName,
	const std::unordered_map<std::string, FontManager::FontId>& existingNames) {
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
	vk_ = &vk;
	atlasSizeHint_ = atlasSize;
	atlas_.layersCapacity = kInitialAtlasLayerCapacity;

	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("FontManager init requires a valid Vulkan device + allocator.");
	}
	if (atlasSizeHint_ == 0) {
		throw std::runtime_error("FontManager init requires a non-zero font atlas size.");
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.graphicsQFamily;
	vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &uploadCommandPool_), "Failed to create font upload command pool.");
}

FontManager::FontFamilyId FontManager::createFamily(const FontFamilyCreateInfo& createInfo) {
	std::string familyName = createInfo.name.empty() ? std::string("Default") : createInfo.name;
	if (familyIdByName_.find(familyName) != familyIdByName_.end()) {
		throw std::runtime_error("Font family already exists: " + familyName);
	}

	const FontFamilyId familyId = static_cast<FontFamilyId>(families_.size());
	FontFamilyData family{};
	family.id = familyId;
	family.name = std::move(familyName);
	families_.push_back(std::move(family));
	familyIdByName_[families_.back().name] = familyId;

	for (const FontFaceCreateInfo& faceInfo : createInfo.faces) {
		addFamilyFace(familyId, faceInfo);
	}

	return familyId;
}

FontManager::FontFamilyId FontManager::getFamilyId(std::string_view familyName) const {
	const auto it = familyIdByName_.find(std::string(familyName));
	return (it != familyIdByName_.end()) ? it->second : std::numeric_limits<FontFamilyId>::max();
}

FontManager::FontId FontManager::addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo) {
	if (familyId >= families_.size()) {
		throw std::runtime_error("Font family id does not exist.");
	}

	const FontId fontId = loadFontFace(createInfo);
	families_[familyId].faces.push_back(FontFamilyFace{
		.fontId = fontId,
		.weight = createInfo.weight,
		.style = createInfo.style,
	});
	return fontId;
}

FontManager::FontId FontManager::addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo) {
	const FontFamilyId familyId = getFamilyId(familyName);
	if (familyId == std::numeric_limits<FontFamilyId>::max()) {
		throw std::runtime_error("Font family does not exist: " + std::string(familyName));
	}
	return addFamilyFace(familyId, createInfo);
}

FontManager::FontId FontManager::resolveFont(FontFamilyId familyId, uint32_t weight, FontStyle style) const {
	if (familyId >= families_.size()) {
		return 0;
	}

	const FontFamilyData& family = families_[familyId];
	const FontFamilyFace* bestFace = nullptr;
	uint32_t bestDistance = std::numeric_limits<uint32_t>::max();

	for (const FontFamilyFace& face : family.faces) {
		if (face.style != style) {
			continue;
		}
		const uint32_t distance = (face.weight > weight) ? (face.weight - weight) : (weight - face.weight);
		if (!bestFace || distance < bestDistance) {
			bestFace = &face;
			bestDistance = distance;
		}
	}

	if (bestFace) {
		return bestFace->fontId;
	}
	if (!family.faces.empty()) {
		return family.faces.front().fontId;
	}
	return 0;
}

FontManager::FontId FontManager::resolveFont(std::string_view familyName, uint32_t weight, FontStyle style) const {
	const FontFamilyId familyId = getFamilyId(familyName);
	return (familyId != std::numeric_limits<FontFamilyId>::max()) ? resolveFont(familyId, weight, style) : 0;
}

FontManager::FontId FontManager::loadFontFace(const FontFaceCreateInfo& createInfo) {
	if (createInfo.path.empty()) {
		throw std::runtime_error("Font face path must not be empty.");
	}
	if (isArfontPath(createInfo.path)) {
		return registerBakedFont(createInfo.path.string(), createInfo.name);
	}
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	return registerRuntimeFont(createInfo);
#else
	return loadFont(createInfo.path.string(), createInfo.pixelSize);
#endif
}

FontManager::FontId FontManager::loadFont(std::string_view path, float px) {
	(void)px;
	if (!vk_ || vk_->device == VK_NULL_HANDLE || vk_->allocator == nullptr) {
		throw std::runtime_error("FontManager is not initialized.");
	}
	const std::filesystem::path fontPath(path);
	if (fontPath.empty()) {
		throw std::runtime_error("Font path must not be empty.");
	}

	if (isArfontPath(fontPath)) {
		return registerBakedFont(path);
	}

#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	FontFaceCreateInfo createInfo{};
	createInfo.path = fontPath;
	createInfo.pixelSize = px;
	return registerRuntimeFont(createInfo);
#else
	throw std::runtime_error("Unsupported font file type: " + fontPath.string());
#endif
}

FontManager::FontId FontManager::registerRuntimeFont(const FontFaceCreateInfo& createInfo) {
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	if (!vk_ || vk_->device == VK_NULL_HANDLE || vk_->allocator == nullptr) {
		throw std::runtime_error("[Flow Ui]: FontManager is not initialized.");
	}
	if (uploadCommandPool_ == VK_NULL_HANDLE) {
		throw std::runtime_error("[Flow Ui]: FontManager is not initialized.");
	}
	if (createInfo.path.empty()) {
		throw std::runtime_error("[Flow Ui]: Runtime font path must not be empty.");
	}
	if (createInfo.pixelSize <= 0.0f) {
		throw std::runtime_error("[Flow Ui]: Runtime font pixel size must be greater than zero.");
	}
	if (!std::filesystem::is_regular_file(createInfo.path)) {
		throw std::runtime_error("[Flow Ui]: Font file does not exist: " + createInfo.path.string());
	}
	if (atlasSizeHint_ == 0 || atlasSizeHint_ > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
		throw std::runtime_error("[Flow Ui]: Runtime font atlas page size is invalid.");
	}

	struct FreetypeGuard {
		msdfgen::FreetypeHandle* handle = nullptr;
		~FreetypeGuard() {
			if (handle) {
				msdfgen::deinitializeFreetype(handle);
			}
		}
	};
	struct FontGuard {
		msdfgen::FontHandle* handle = nullptr;
		~FontGuard() {
			if (handle) {
				msdfgen::destroyFont(handle);
			}
		}
	};

	FreetypeGuard freetype{};
	freetype.handle = msdfgen::initializeFreetype();
	if (!freetype.handle) {
		throw std::runtime_error("Failed to initialize FreeType for runtime font baking.");
	}

	FontGuard font{};
	const std::string pathString = createInfo.path.string();
	font.handle = msdfgen::loadFont(freetype.handle, pathString.c_str());
	if (!font.handle) {
		throw std::runtime_error("Failed to load runtime font file: " + pathString);
	}

	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry fontGeometry(&glyphs);
	const int glyphsLoaded = fontGeometry.loadCharset(
		font.handle,
		1.0,
		msdf_atlas::Charset::ASCII,
		false,
		true);

	if (glyphsLoaded < 0) {
		throw std::runtime_error("Failed to load glyph geometry from runtime font: " + pathString);
	}
	if (glyphsLoaded == 0 || glyphs.empty()) {
		throw std::runtime_error("No glyphs were loaded from runtime font: " + pathString);
	}

	unsigned long long glyphSeed = 0;
	for (msdf_atlas::GlyphGeometry& glyph : glyphs) {
		glyphSeed = glyphSeed * kRuntimeFontLcgMultiplier + kRuntimeFontLcgIncrement;
		glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, kDefaultRuntimeFontAngleThreshold, glyphSeed);
	}

	const uint32_t pageWidth = atlasSizeHint_;
	const uint32_t pageHeight = atlasSizeHint_;
	msdf_atlas::TightAtlasPacker packer;
	packer.setDimensions(static_cast<int>(pageWidth), static_cast<int>(pageHeight));
	packer.setSpacing(0);
	packer.setScale(static_cast<double>(createInfo.pixelSize));
	packer.setPixelRange(kDefaultRuntimeFontPxRange);
	packer.setMiterLimit(kDefaultRuntimeFontMiterLimit);
	packer.setOriginPixelAlignment(false, true);

	const int remaining = packer.pack(glyphs.data(), static_cast<int>(glyphs.size()));
	if (remaining < 0) {
		throw std::runtime_error("Failed to pack runtime font glyphs into atlas: " + pathString);
	}
	if (remaining > 0) {
		throw std::runtime_error(
			"Could not fit " + std::to_string(remaining) +
			" runtime font glyphs into configured ui.fontAtlasSize=" + std::to_string(pageWidth) +
			" for " + pathString);
	}

	msdf_atlas::GeneratorAttributes attributes;
	attributes.config.overlapSupport = true;
	attributes.scanlinePass = true;

	using MtsdfGenerator = msdf_atlas::ImmediateAtlasGenerator<
		float,
		4,
		msdf_atlas::mtsdfGenerator,
		msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;

	MtsdfGenerator generator(static_cast<int>(pageWidth), static_cast<int>(pageHeight));
	generator.setAttributes(attributes);
	generator.setThreadCount(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
	generator.generate(glyphs.data(), glyphs.size());

	msdfgen::BitmapConstSection<msdf_atlas::byte, 4> atlasBitmap =
		static_cast<msdfgen::BitmapConstSection<msdf_atlas::byte, 4>>(generator.atlasStorage());
	atlasBitmap.reorient(msdfgen::Y_DOWNWARD);

	std::vector<uint8_t> pagePixels(static_cast<size_t>(pageWidth) * static_cast<size_t>(pageHeight) * 4u);
	const size_t rowBytes = static_cast<size_t>(pageWidth) * 4u;
	for (uint32_t y = 0; y < pageHeight; ++y) {
		std::memcpy(
			pagePixels.data() + static_cast<size_t>(y) * rowBytes,
			atlasBitmap(0, static_cast<int>(y)),
			rowBytes);
	}

	VulkanContext& vk = *vk_;
	ensureAtlasStorageCapacity(vk, uploadCommandPool_, atlas_, pageWidth, pageHeight, atlas_.layersUsed + 1u);
	const uint32_t assignedLayer = atlas_.layersUsed;
	uploadLayerPixels(vk, uploadCommandPool_, atlas_, assignedLayer, pagePixels);
	atlas_.layersUsed += 1u;

	Font::FontFaceData fontFace{};
	if (nextFontId_ == std::numeric_limits<FontId>::max()) {
		throw std::runtime_error("FlowUi font id limit exceeded.");
	}
	fontFace.id = nextFontId_++;
	fontFace.sourcePath = createInfo.path;
	fontFace.atlasLayer = assignedLayer;
	fontFace.atlasWidth = pageWidth;
	fontFace.atlasHeight = pageHeight;
	fontFace.sourceAtlasX = 0;
	fontFace.sourceAtlasY = 0;
	fontFace.sourceAtlasWidth = pageWidth;
	fontFace.sourceAtlasHeight = pageHeight;
	fontFace.imageType = static_cast<uint32_t>(artery_font::IMAGE_MTSDF);
	fontFace.metadata = "runtime-msdf";
	fontFace.defaultVariantIndex = 0;

	Font::FontVariantData variant{};
	variant.weight = createInfo.weight;
	variant.fontSizePx = static_cast<float>(packer.getScale());
	const msdfgen::Range finalPxRange = packer.getPixelRange();
	variant.distanceRange = static_cast<float>(finalPxRange.upper - finalPxRange.lower);
	variant.distanceRangeMiddle = static_cast<float>(0.5 * (finalPxRange.lower + finalPxRange.upper));

	const msdfgen::FontMetrics& metrics = fontGeometry.getMetrics();
	variant.emSize = static_cast<float>(metrics.emSize);
	variant.ascender = static_cast<float>(metrics.ascenderY);
	variant.descender = static_cast<float>(metrics.descenderY);
	variant.lineHeight = static_cast<float>(metrics.lineHeight);
	variant.underlineY = static_cast<float>(metrics.underlineY);
	variant.underlineThickness = static_cast<float>(metrics.underlineThickness);
	variant.name = createInfo.name.empty() ? createInfo.path.stem().string() : createInfo.name;
	variant.metadata = "runtime-msdf";
	variant.glyphs.reserve(glyphs.size());

	for (const msdf_atlas::GlyphGeometry& glyphGeometry : fontGeometry.getGlyphs()) {
		double planeLeft = 0.0;
		double planeBottom = 0.0;
		double planeRight = 0.0;
		double planeTop = 0.0;
		glyphGeometry.getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);

		double imageLeft = 0.0;
		double imageBottom = 0.0;
		double imageRight = 0.0;
		double imageTop = 0.0;
		glyphGeometry.getQuadAtlasBounds(imageLeft, imageBottom, imageRight, imageTop);

		Font::GlyphData glyph{};
		glyph.codepoint = glyphGeometry.getCodepoint();
		glyph.sourceImageIndex = 0;
		glyph.planeLeft = static_cast<float>(planeLeft);
		glyph.planeBottom = static_cast<float>(planeBottom);
		glyph.planeRight = static_cast<float>(planeRight);
		glyph.planeTop = static_cast<float>(planeTop);
		glyph.imageLeft = static_cast<float>(imageLeft);
		glyph.imageBottom = static_cast<float>(imageBottom);
		glyph.imageRight = static_cast<float>(imageRight);
		glyph.imageTop = static_cast<float>(imageTop);
		glyph.advanceX = static_cast<float>(glyphGeometry.getAdvance());
		glyph.advanceY = 0.0f;

		const uint32_t newGlyphIndex = static_cast<uint32_t>(variant.glyphs.size());
		variant.glyphs.push_back(glyph);
		if (glyph.codepoint != 0) {
			variant.unicodeToGlyphIndex.emplace(glyph.codepoint, newGlyphIndex);
		}
	}

	for (const auto& pair : fontGeometry.getKerning()) {
		const msdf_atlas::GlyphGeometry* leftGlyph = fontGeometry.getGlyph(msdfgen::GlyphIndex(pair.first.first));
		const msdf_atlas::GlyphGeometry* rightGlyph = fontGeometry.getGlyph(msdfgen::GlyphIndex(pair.first.second));
		if (!leftGlyph || !rightGlyph || leftGlyph->getCodepoint() == 0 || rightGlyph->getCodepoint() == 0) {
			continue;
		}
		const uint64_t key = Font::FontVariantData::kerningKey(leftGlyph->getCodepoint(), rightGlyph->getCodepoint());
		variant.kerningPairs[key] = static_cast<float>(pair.second);
	}

	if (const auto questionIt = variant.unicodeToGlyphIndex.find('?'); questionIt != variant.unicodeToGlyphIndex.end()) {
		variant.fallbackGlyphIndex = questionIt->second;
	} else if (variant.fallbackGlyphIndex >= variant.glyphs.size()) {
		variant.fallbackGlyphIndex = 0;
	}

	fontFace.name = makeUniqueFontName(variant.name, fontIdByName_);
	fontFace.variants.push_back(std::move(variant));

	const size_t newIndex = fonts_.size();
	fonts_.push_back(std::move(fontFace));
	fontIndexById_[fonts_.back().id] = newIndex;
	fontIdByName_[fonts_.back().name] = fonts_.back().id;
	return fonts_.back().id;
#else
	(void)createInfo;
	throw std::runtime_error("Runtime font baking is not enabled.");
#endif
}

FontManager::FontId FontManager::registerBakedFont(std::string_view arfontPath, std::string_view requestedName) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE || vk_->allocator == nullptr) {
		throw std::runtime_error("FontManager is not initialized.");
	}
	VulkanContext& vk = *vk_;

	if (uploadCommandPool_ == VK_NULL_HANDLE) {
		throw std::runtime_error("FontManager is not initialized.");
	}

	const std::filesystem::path path(arfontPath);
	if (!isArfontPath(path)) {
		throw std::runtime_error("Unsupported baked font file type: " + path.string());
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
	const uint32_t pageWidth = atlasSizeHint_;
	const uint32_t pageHeight = atlasSizeHint_;
	std::vector<uint8_t> pagePixels = copyAtlasIntoPage(decodedImage, pageWidth, pageHeight, path);

	if (decodedImage.width != pageWidth || decodedImage.height != pageHeight) {
		std::fprintf(
			stderr,
			"[FlowUi] Warning: .arfont atlas %ux%u was copied into configured font atlas page %ux%u.\n",
			decodedImage.width,
			decodedImage.height,
			pageWidth,
			pageHeight);
	}

	ensureAtlasStorageCapacity(vk, uploadCommandPool_, atlas_, pageWidth, pageHeight, atlas_.layersUsed + 1u);
	const uint32_t assignedLayer = atlas_.layersUsed;
	uploadLayerPixels(vk, uploadCommandPool_, atlas_, assignedLayer, pagePixels);
	atlas_.layersUsed += 1u;

	const auto* variants = listData(arteryFont.variants);
	Font::FontFaceData fontFace{};
	if (nextFontId_ == std::numeric_limits<FontId>::max()) {
		throw std::runtime_error("FlowUi font id limit exceeded.");
	}
	fontFace.id = nextFontId_++;
	fontFace.sourcePath = path;
	fontFace.atlasLayer = assignedLayer;
	fontFace.atlasWidth = pageWidth;
	fontFace.atlasHeight = pageHeight;
	fontFace.sourceAtlasX = 0;
	fontFace.sourceAtlasY = 0;
	fontFace.sourceAtlasWidth = decodedImage.width;
	fontFace.sourceAtlasHeight = decodedImage.height;
	fontFace.imageType = static_cast<uint32_t>(images[imageIndex].imageType);
	fontFace.metadata = toStdString(arteryFont.metadata);
	fontFace.defaultVariantIndex = 0;
	fontFace.variants.reserve(static_cast<size_t>(arteryFont.variants.length()));

	for (int variantIndex = 0; variantIndex < arteryFont.variants.length(); ++variantIndex) {
		const auto& sourceVariant = variants[variantIndex];
		Font::FontVariantData variant{};
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

			Font::GlyphData glyph{};
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
			const uint64_t key = Font::FontVariantData::kerningKey(pair.codepoint1, pair.codepoint2);
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

const Font::FontFaceData* FontManager::getFontById(FontId fontId) const {
	const auto it = fontIndexById_.find(fontId);
	if (it == fontIndexById_.end()) {
		return nullptr;
	}
	const size_t index = it->second;
	return (index < fonts_.size()) ? &fonts_[index] : nullptr;
}

void FontManager::destroy(VulkanContext& vk) {
	familyIdByName_.clear();
	families_.clear();
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

	atlas_ = Font::AtlasArrayResource{};
	atlas_.layersCapacity = kInitialAtlasLayerCapacity;
	uploadCommandPool_ = VK_NULL_HANDLE;
	vk_ = nullptr;
}
