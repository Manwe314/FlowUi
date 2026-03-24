#include "managers/SvgManager.hpp"

#if FLOWUI_INCLUDE_SVG_MANAGER

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>

#include "internal/UiTextureRegistry.hpp"
#include "vk_mem_alloc.h"
#include <plutosvg.h>

namespace FlowUi {

namespace {

void convertArgbPremultipliedToRgbaStraight(
	uint8_t* data,
	uint32_t width,
	uint32_t height,
	uint32_t strideBytes) {
	if (!data || width == 0u || height == 0u || strideBytes < width * 4u) {
		return;
	}

	for (uint32_t y = 0u; y < height; ++y) {
		uint8_t* row = data + static_cast<size_t>(y) * static_cast<size_t>(strideBytes);
		for (uint32_t x = 0u; x < width; ++x) {
			const size_t index = static_cast<size_t>(x) * 4u;
			const uint32_t b = row[index + 0u];
			const uint32_t g = row[index + 1u];
			const uint32_t r = row[index + 2u];
			const uint32_t a = row[index + 3u];
			if (a == 0u) {
				row[index + 0u] = 0u;
				row[index + 1u] = 0u;
				row[index + 2u] = 0u;
				row[index + 3u] = 0u;
				continue;
			}

			uint32_t rr = r;
			uint32_t gg = g;
			uint32_t bb = b;
			if (a != 255u) {
				rr = (rr * 255u) / a;
				gg = (gg * 255u) / a;
				bb = (bb * 255u) / a;
			}

			row[index + 0u] = static_cast<uint8_t>(rr);
			row[index + 1u] = static_cast<uint8_t>(gg);
			row[index + 2u] = static_cast<uint8_t>(bb);
			row[index + 3u] = static_cast<uint8_t>(a);
		}
	}
}

void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

struct StagingBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
};

VkCommandBuffer beginOneTimeCommands(VulkanContext& vk, VkCommandPool commandPool) {
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	vkCheck(
		vkAllocateCommandBuffers(vk.device, &allocateInfo, &commandBuffer),
		"IconManager failed to allocate command buffer.");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "IconManager failed to begin command buffer.");
	return commandBuffer;
}

void endOneTimeCommands(VulkanContext& vk, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
	vkCheck(vkEndCommandBuffer(commandBuffer), "IconManager failed to end command buffer.");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1u;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkCheck(vkQueueSubmit(vk.graphicsQ, 1u, &submitInfo, VK_NULL_HANDLE), "IconManager failed to submit command buffer.");
	vkCheck(vkQueueWaitIdle(vk.graphicsQ), "IconManager failed to wait for queue idle.");

	vkFreeCommandBuffers(vk.device, commandPool, 1u, &commandBuffer);
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
	barrier.subresourceRange.baseMipLevel = 0u;
	barrier.subresourceRange.levelCount = 1u;
	barrier.subresourceRange.baseArrayLayer = 0u;
	barrier.subresourceRange.layerCount = 1u;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStageMask,
		dstStageMask,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
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
		"IconManager failed to create staging buffer.");

	if (!mappedInfo.pMappedData) {
		vmaDestroyBuffer(vk.allocator, staging.buffer, staging.allocation);
		throw std::runtime_error("IconManager failed to map staging buffer.");
	}

	std::memcpy(mappedInfo.pMappedData, data, byteCount);
	vkCheck(vmaFlushAllocation(vk.allocator, staging.allocation, 0, byteCount), "IconManager failed to flush staging buffer.");
	return staging;
}

void destroyStagingBuffer(VulkanContext& vk, StagingBuffer& staging) {
	if (staging.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(vk.allocator, staging.buffer, staging.allocation);
	}
	staging.buffer = VK_NULL_HANDLE;
	staging.allocation = nullptr;
}

void transitionImageLayoutsToShaderRead(
	VulkanContext& vk,
	VkCommandPool commandPool,
	VkImage image) {
	if (vk.device == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
		return;
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	vkCheck(vkAllocateCommandBuffers(vk.device, &allocInfo, &commandBuffer), "IconManager failed to allocate command buffer.");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "IconManager failed to begin command buffer.");

	VkImageMemoryBarrier toTransferDst{};
	toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransferDst.image = image;
	toTransferDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toTransferDst.subresourceRange.baseMipLevel = 0;
	toTransferDst.subresourceRange.levelCount = 1;
	toTransferDst.subresourceRange.baseArrayLayer = 0;
	toTransferDst.subresourceRange.layerCount = 1;
	toTransferDst.srcAccessMask = 0u;
	toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
		&toTransferDst);

	const VkClearColorValue zeroColor = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	VkImageSubresourceRange clearRange{};
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel = 0;
	clearRange.levelCount = 1;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount = 1;
	vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zeroColor, 1u, &clearRange);

	VkImageMemoryBarrier toShaderRead{};
	toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toShaderRead.image = image;
	toShaderRead.subresourceRange = clearRange;
	toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
		&toShaderRead);

	vkCheck(vkEndCommandBuffer(commandBuffer), "IconManager failed to end command buffer.");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1u;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkCheck(vkQueueSubmit(vk.graphicsQ, 1u, &submitInfo, VK_NULL_HANDLE), "IconManager failed to submit command buffer.");
	vkCheck(vkQueueWaitIdle(vk.graphicsQ), "IconManager failed to wait for queue idle.");

	vkFreeCommandBuffers(vk.device, commandPool, 1u, &commandBuffer);
}

} // namespace

IconManager::SurfaceOwner& IconManager::SurfaceOwner::operator=(SurfaceOwner&& other) noexcept {
	if (this == &other) {
		return *this;
	}
	if (surface) {
		plutovg_surface_destroy(surface);
	}
	surface = other.surface;
	other.surface = nullptr;
	return *this;
}

IconManager::SurfaceOwner::~SurfaceOwner() {
	if (surface) {
		plutovg_surface_destroy(surface);
		surface = nullptr;
	}
}

std::size_t IconManager::VariantKeyHash::operator()(const VariantKey& key) const noexcept {
	std::size_t seed = std::hash<std::string>{}(key.nameKey);
	const std::size_t wHash = std::hash<uint32_t>{}(key.requestedWidth);
	const std::size_t hHash = std::hash<uint32_t>{}(key.requestedHeight);
	seed ^= (wHash + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u));
	seed ^= (hHash + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u));
	return seed;
}

uint32_t IconManager::frameAge(uint32_t currentFrame, uint32_t lastUsedFrame) {
	// Unsigned subtraction is wrap-safe in modulo-2^32 arithmetic.
	// Example: lastUsed=UINT32_MAX-5, current=2 => age=8.
	return currentFrame - lastUsedFrame;
}

uint32_t IconManager::bucketRequestedDimension(uint32_t requested) const {
	const uint32_t clamped = std::max<uint32_t>(1u, requested);
	const uint32_t step = std::max<uint32_t>(1u, sizeBucketStep_);
	const uint32_t rounded = ((clamped + step - 1u) / step) * step;
	return rounded;
}

IconManager::VariantKey IconManager::makeVariantKey(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const
{
	VariantKey variantKey{};
	variantKey.nameKey = std::string(key);
	variantKey.requestedWidth = bucketRequestedDimension(requestedWidth);
	variantKey.requestedHeight = bucketRequestedDimension(requestedHeight);
	return variantKey;
}

void IconManager::advanceFrameCounter() {
	++frameCounter_;
	if (frameCounter_ != 0u) {
		return;
	}

	// Overflow policy: renormalize timestamps to maintain stable ordering semantics.
	for (auto& [_, variant] : variantsByKeyAndSize_) {
		variant.lastUsedFrame = 0u;
	}
	for (AtlasPage& page : atlasPages_) {
		page.lastUsedFrame = 0u;
	}
}

void IconManager::touchVariant(VariantEntry& variant, uint32_t frameIndex) {
	variant.lastUsedFrame = frameIndex;
	variant.referencedThisFrame = true;
}

void IconManager::resetVariantFrameMarks() {
	for (auto& [_, variant] : variantsByKeyAndSize_) {
		variant.referencedThisFrame = false;
	}
}

IconManager::AtlasPage IconManager::createAtlasPage(uint32_t pageIndex) const
{
	if (!vk_ || vk_->device == VK_NULL_HANDLE || vk_->allocator == nullptr) {
		throw std::runtime_error("IconManager cannot create atlas pages before Vulkan init.");
	}
	if (!registry_) {
		throw std::runtime_error("IconManager registry backend is not set.");
	}
	if (atlasSampler_ == VK_NULL_HANDLE || commandPool_ == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager atlas resources are not initialized.");
	}

	AtlasPage page{};
	page.namespacedKey = "svg/page/" + std::to_string(pageIndex);
	page.width = std::max<uint32_t>(1u, atlasSize_);
	page.height = std::max<uint32_t>(1u, atlasSize_);
	page.freeRects.emplace_back(AtlasRect{
		.x = 0u,
		.y = 0u,
		.w = page.width,
		.h = page.height,
	});
	page.usedArea = 0u;
	page.lastUsedFrame = frameCounter_;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { page.width, page.height, 1u };
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	vkCheck(
		vmaCreateImage(vk_->allocator, &imageInfo, &allocationInfo, &page.image, &page.allocation, nullptr),
		"IconManager failed to create atlas image.");

	try {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = page.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = imageInfo.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0u;
		viewInfo.subresourceRange.levelCount = 1u;
		viewInfo.subresourceRange.baseArrayLayer = 0u;
		viewInfo.subresourceRange.layerCount = 1u;
		vkCheck(vkCreateImageView(vk_->device, &viewInfo, nullptr, &page.view), "IconManager failed to create atlas image view.");

		transitionImageLayoutsToShaderRead(*vk_, commandPool_, page.image);

		bool inserted = false;
		page.slotId = registry_->registerOrReplaceSlot(*vk_, page.namespacedKey, page.view, atlasSampler_, inserted);
		if (!inserted) {
			throw std::runtime_error("IconManager atlas page namespaced key collision.");
		}
	} catch (...) {
		if (page.view != VK_NULL_HANDLE) {
			vkDestroyImageView(vk_->device, page.view, nullptr);
			page.view = VK_NULL_HANDLE;
		}
		if (page.image != VK_NULL_HANDLE) {
			vmaDestroyImage(vk_->allocator, page.image, page.allocation);
			page.image = VK_NULL_HANDLE;
			page.allocation = nullptr;
		}
		throw;
	}

	return page;
}

void IconManager::destroyAtlasPage(AtlasPage& page) {
	if (registry_ && !page.namespacedKey.empty()) {
		const bool removed = registry_->removeSlot(page.namespacedKey);
		(void)removed;
	}
	if (vk_ && vk_->device != VK_NULL_HANDLE && page.view != VK_NULL_HANDLE) {
		vkDestroyImageView(vk_->device, page.view, nullptr);
	}
	if (vk_ && vk_->allocator != nullptr && page.image != VK_NULL_HANDLE) {
		vmaDestroyImage(vk_->allocator, page.image, page.allocation);
	}

	page.slotId = 0u;
	page.image = VK_NULL_HANDLE;
	page.allocation = nullptr;
	page.view = VK_NULL_HANDLE;
	page.width = 0u;
	page.height = 0u;
	page.usedArea = 0u;
	page.lastUsedFrame = 0u;
	page.namespacedKey.clear();
	page.freeRects.clear();
}

void IconManager::destroyAtlasPages() {
	for (AtlasPage& page : atlasPages_) {
		destroyAtlasPage(page);
	}
	atlasPages_.clear();
}

bool IconManager::tryAllocateInPage(
	AtlasPage& page,
	uint32_t contentWidth,
	uint32_t contentHeight,
	uint32_t padding,
	AtlasAllocation& outAllocation)
{
	if (contentWidth == 0u || contentHeight == 0u || page.freeRects.empty()) {
		return false;
	}

	const uint32_t pad = padding;
	const uint32_t neededWidth = contentWidth + pad * 2u;
	const uint32_t neededHeight = contentHeight + pad * 2u;

	size_t bestIndex = std::numeric_limits<size_t>::max();
	uint64_t bestWaste = std::numeric_limits<uint64_t>::max();

	for (size_t i = 0; i < page.freeRects.size(); ++i) {
		const AtlasRect& rect = page.freeRects[i];
		if (rect.w < neededWidth || rect.h < neededHeight) {
			continue;
		}
		const uint64_t waste =
			static_cast<uint64_t>(rect.w) * static_cast<uint64_t>(rect.h) -
			static_cast<uint64_t>(neededWidth) * static_cast<uint64_t>(neededHeight);
		if (waste < bestWaste) {
			bestWaste = waste;
			bestIndex = i;
			if (waste == 0u) {
				break;
			}
		}
	}

	if (bestIndex == std::numeric_limits<size_t>::max()) {
		return false;
	}

	const AtlasRect selected = page.freeRects[bestIndex];
	page.freeRects.erase(page.freeRects.begin() + static_cast<std::ptrdiff_t>(bestIndex));

	const AtlasRect paddedRect{
		.x = selected.x,
		.y = selected.y,
		.w = neededWidth,
		.h = neededHeight,
	};
	const AtlasRect contentRect{
		.x = paddedRect.x + pad,
		.y = paddedRect.y + pad,
		.w = contentWidth,
		.h = contentHeight,
	};

	// Simple guillotine split into right strip and bottom strip.
	if (selected.w > neededWidth) {
		page.freeRects.emplace_back(AtlasRect{
			.x = selected.x + neededWidth,
			.y = selected.y,
			.w = selected.w - neededWidth,
			.h = neededHeight,
		});
	}
	if (selected.h > neededHeight) {
		page.freeRects.emplace_back(AtlasRect{
			.x = selected.x,
			.y = selected.y + neededHeight,
			.w = selected.w,
			.h = selected.h - neededHeight,
		});
	}

	page.usedArea += static_cast<uint64_t>(paddedRect.w) * static_cast<uint64_t>(paddedRect.h);

	outAllocation.pageIndex = std::numeric_limits<uint32_t>::max();
	outAllocation.paddedRect = paddedRect;
	outAllocation.contentRect = contentRect;
	return true;
}

void IconManager::releasePageRegion(AtlasPage& page, const AtlasRect& paddedRect) {
	if (paddedRect.w == 0u || paddedRect.h == 0u) {
		return;
	}
	page.freeRects.push_back(paddedRect);
	const uint64_t freedArea = static_cast<uint64_t>(paddedRect.w) * static_cast<uint64_t>(paddedRect.h);
	page.usedArea = (freedArea > page.usedArea) ? 0u : (page.usedArea - freedArea);
}

void IconManager::mergeFreeRects(AtlasPage& page)
{
	bool mergedAny = true;
	while (mergedAny) {
		mergedAny = false;
		for (size_t i = 0; i < page.freeRects.size() && !mergedAny; ++i) {
			for (size_t j = i + 1; j < page.freeRects.size(); ++j) {
				AtlasRect& a = page.freeRects[i];
				AtlasRect& b = page.freeRects[j];

				// Vertical merge: same x/w, touching y edge.
				if (a.x == b.x && a.w == b.w && (a.y + a.h == b.y || b.y + b.h == a.y))
				{
					const uint32_t top = std::min(a.y, b.y);
					a = AtlasRect{ .x = a.x, .y = top, .w = a.w, .h = a.h + b.h };
					page.freeRects.erase(page.freeRects.begin() + static_cast<std::ptrdiff_t>(j));
					mergedAny = true;
					break;
				}

				// Horizontal merge: same y/h, touching x edge.
				if (a.y == b.y && a.h == b.h && (a.x + a.w == b.x || b.x + b.w == a.x)) {
					const uint32_t left = std::min(a.x, b.x);
					a = AtlasRect{ .x = left, .y = a.y, .w = a.w + b.w, .h = a.h };
					page.freeRects.erase(page.freeRects.begin() + static_cast<std::ptrdiff_t>(j));
					mergedAny = true;
					break;
				}
			}
		}
	}
}

void IconManager::recalcAtlasUvs(VariantEntry& variant, const AtlasPage& page) const {
	const float invW = page.width > 0u ? (1.0f / static_cast<float>(page.width)) : 0.0f;
	const float invH = page.height > 0u ? (1.0f / static_cast<float>(page.height)) : 0.0f;
	variant.uv0x = static_cast<float>(variant.contentRect.x) * invW;
	variant.uv0y = static_cast<float>(variant.contentRect.y) * invH;
	variant.uv1x = static_cast<float>(variant.contentRect.x + variant.contentRect.w) * invW;
	variant.uv1y = static_cast<float>(variant.contentRect.y + variant.contentRect.h) * invH;
}

const std::string* IconManager::findRequestedKeyByTextureId(uint32_t textureId) const
{
	const auto it = requestedKeyByTextureId_.find(textureId);
	if (it == requestedKeyByTextureId_.end()) {
		return nullptr;
	}
	return &it->second;
}

std::string IconManager::makeRequestNamespacedKey(std::string_view key) const {
	return "svg/request/" + std::string(key);
}

void IconManager::uploadRasterToAtlasPage(
	const AtlasPage& page,
	const TransientRasterResult& raster,
	const AtlasRect& contentRect) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE || vk_->allocator == nullptr) {
		throw std::runtime_error("IconManager is not initialized.");
	}
	if (commandPool_ == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager command pool is not initialized.");
	}
	if (page.image == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager atlas page image is invalid.");
	}
	if (!raster.rgbaPixels || raster.width == 0u || raster.height == 0u || raster.strideBytes < raster.width * 4u) {
		throw std::runtime_error("IconManager raster payload is invalid.");
	}
	if ((raster.strideBytes % 4u) != 0u) {
		throw std::runtime_error("IconManager raster stride is not a multiple of pixel size.");
	}
	if (contentRect.x + raster.width > page.width || contentRect.y + raster.height > page.height) {
		throw std::runtime_error("IconManager atlas upload is out of bounds.");
	}

	const size_t uploadBytes = static_cast<size_t>(raster.strideBytes) * static_cast<size_t>(raster.height);
	StagingBuffer staging = createStagingBuffer(*vk_, raster.rgbaPixels, uploadBytes);

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	try {
		commandBuffer = beginOneTimeCommands(*vk_, commandPool_);

		cmdTransitionImageLayout(
			commandBuffer,
			page.image,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0u;
		copyRegion.bufferRowLength = raster.strideBytes / 4u;
		copyRegion.bufferImageHeight = 0u;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0u;
		copyRegion.imageSubresource.baseArrayLayer = 0u;
		copyRegion.imageSubresource.layerCount = 1u;
		copyRegion.imageOffset = {
			static_cast<int32_t>(contentRect.x),
			static_cast<int32_t>(contentRect.y),
			0,
		};
		copyRegion.imageExtent = {
			raster.width,
			raster.height,
			1u,
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			staging.buffer,
			page.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1u,
			&copyRegion);

		cmdTransitionImageLayout(
			commandBuffer,
			page.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		endOneTimeCommands(*vk_, commandPool_, commandBuffer);
		commandBuffer = VK_NULL_HANDLE;
	} catch (...) {
		if (commandBuffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(vk_->device, commandPool_, 1u, &commandBuffer);
		}
		destroyStagingBuffer(*vk_, staging);
		throw;
	}

	destroyStagingBuffer(*vk_, staging);
}

bool IconManager::tryAllocateAtlasRegion(
	uint32_t contentWidth,
	uint32_t contentHeight,
	AtlasAllocation& outAllocation) {
	outAllocation = AtlasAllocation{};

	auto tryAcrossPages = [&](bool mergeFirst) -> bool {
		for (uint32_t pageIndex = 0u; pageIndex < atlasPages_.size(); ++pageIndex) {
			AtlasPage& page = atlasPages_[pageIndex];
			if (mergeFirst && page.freeRects.size() > 1u) {
				mergeFreeRects(page);
			}

			AtlasAllocation candidate{};
			if (!tryAllocateInPage(page, contentWidth, contentHeight, atlasPadding_, candidate)) {
				continue;
			}
			candidate.pageIndex = pageIndex;
			page.lastUsedFrame = frameCounter_;
			outAllocation = candidate;
			return true;
		}
		return false;
	};

	if (tryAcrossPages(false)) {
		return true;
	}
	if (tryAcrossPages(true)) {
		return true;
	}

	if (atlasPages_.size() < maxAtlasPages_) {
		const uint32_t newPageIndex = static_cast<uint32_t>(atlasPages_.size());
		atlasPages_.push_back(createAtlasPage(newPageIndex));
		AtlasAllocation candidate{};
		if (tryAllocateInPage(atlasPages_.back(), contentWidth, contentHeight, atlasPadding_, candidate)) {
			candidate.pageIndex = newPageIndex;
			atlasPages_.back().lastUsedFrame = frameCounter_;
			outAllocation = candidate;
			return true;
		}
	}

	if (atlasPages_.empty()) {
		return false;
	}

	while (evictLeastRecentlyUsedVariant()) {
		if (tryAcrossPages(false)) {
			return true;
		}
		if (tryAcrossPages(true)) {
			return true;
		}
	}

	return false;
}

bool IconManager::evictLeastRecentlyUsedVariant() {
	auto victimIt = variantsByKeyAndSize_.end();
	uint32_t bestAge = 0u;

	for (auto it = variantsByKeyAndSize_.begin(); it != variantsByKeyAndSize_.end(); ++it) {
		const VariantEntry& candidate = it->second;
		if (candidate.referencedThisFrame) {
			continue;
		}
		const uint32_t age = frameAge(frameCounter_, candidate.lastUsedFrame);
		if (victimIt == variantsByKeyAndSize_.end() || age > bestAge) {
			victimIt = it;
			bestAge = age;
		}
	}

	if (victimIt == variantsByKeyAndSize_.end()) {
		return false;
	}

	const VariantEntry victim = victimIt->second;
	if (victim.pageIndex < atlasPages_.size()) {
		releasePageRegion(atlasPages_[victim.pageIndex], victim.paddedRect);
	}
	variantsByKeyAndSize_.erase(victimIt);
	return true;
}

IconManager::VariantEntry* IconManager::findBestCachedVariant(
	std::string_view nameKey,
	uint32_t bucketedWidth,
	uint32_t bucketedHeight) {
	const uint32_t maxBucketGap = std::max<uint32_t>(1u, sizeBucketStep_);

	VariantEntry* bestAbove = nullptr;
	uint64_t bestAboveGap = std::numeric_limits<uint64_t>::max();
	uint64_t bestAboveArea = std::numeric_limits<uint64_t>::max();

	VariantEntry* bestLower = nullptr;
	uint64_t bestLowerGap = std::numeric_limits<uint64_t>::max();
	uint64_t bestLowerArea = 0u;

	for (auto& [_, variant] : variantsByKeyAndSize_) {
		if (variant.key.nameKey != nameKey) {
			continue;
		}

		const uint32_t variantWidth = variant.key.requestedWidth;
		const uint32_t variantHeight = variant.key.requestedHeight;

		if (variantWidth == bucketedWidth && variantHeight == bucketedHeight) {
			return &variant;
		}

		if (variantWidth >= bucketedWidth && variantHeight >= bucketedHeight) {
			const uint32_t widthGap = variantWidth - bucketedWidth;
			const uint32_t heightGap = variantHeight - bucketedHeight;
			if (widthGap > maxBucketGap || heightGap > maxBucketGap) {
				continue;
			}

			const uint64_t gap =
				static_cast<uint64_t>(widthGap) +
				static_cast<uint64_t>(heightGap);
			const uint64_t area = static_cast<uint64_t>(variantWidth) * static_cast<uint64_t>(variantHeight);
			if (gap < bestAboveGap || (gap == bestAboveGap && area < bestAboveArea)) {
				bestAbove = &variant;
				bestAboveGap = gap;
				bestAboveArea = area;
			}
			continue;
		}

		if (variantWidth <= bucketedWidth && variantHeight <= bucketedHeight) {
			const uint32_t widthGap = bucketedWidth - variantWidth;
			const uint32_t heightGap = bucketedHeight - variantHeight;
			if (widthGap > maxBucketGap || heightGap > maxBucketGap) {
				continue;
			}

			const uint64_t gap =
				static_cast<uint64_t>(widthGap) +
				static_cast<uint64_t>(heightGap);
			const uint64_t area = static_cast<uint64_t>(variantWidth) * static_cast<uint64_t>(variantHeight);
			if (gap < bestLowerGap || (gap == bestLowerGap && area > bestLowerArea)) {
				bestLower = &variant;
				bestLowerGap = gap;
				bestLowerArea = area;
			}
		}
	}

	if (bestAbove) {
		return bestAbove;
	}
	if (bestLower) {
		return bestLower;
	}

	return nullptr;
}

IconManager::VariantEntry& IconManager::ensureVariantForRequest(
	std::string_view nameKey,
	uint32_t requestedWidth,
	uint32_t requestedHeight) {
	const VariantKey bucketedKey = makeVariantKey(nameKey, requestedWidth, requestedHeight);
	if (VariantEntry* cached = findBestCachedVariant(nameKey, bucketedKey.requestedWidth, bucketedKey.requestedHeight)) {
		return *cached;
	}

	TransientRasterResult raster = rasterizeForAtlas(nameKey, bucketedKey.requestedWidth, bucketedKey.requestedHeight);
	AtlasAllocation allocation{};
	if (!tryAllocateAtlasRegion(raster.width, raster.height, allocation)) {
		throw std::runtime_error("IconManager atlas is full and no evictable entries are available.");
	}
	if (allocation.pageIndex >= atlasPages_.size()) {
		throw std::runtime_error("IconManager produced an invalid atlas allocation.");
	}

	AtlasPage& page = atlasPages_[allocation.pageIndex];
	uploadRasterToAtlasPage(page, raster, allocation.contentRect);

	VariantEntry entry{};
	entry.key = bucketedKey;
	entry.pageIndex = allocation.pageIndex;
	entry.slotId = page.slotId;
	entry.paddedRect = allocation.paddedRect;
	entry.contentRect = allocation.contentRect;
	entry.sourceWidth = raster.width;
	entry.sourceHeight = raster.height;
	entry.lastUsedFrame = frameCounter_;
	entry.referencedThisFrame = true;
	recalcAtlasUvs(entry, page);

	auto [it, inserted] = variantsByKeyAndSize_.emplace(entry.key, std::move(entry));
	if (!inserted) {
		return it->second;
	}

	page.lastUsedFrame = frameCounter_;
	return it->second;
}

void IconManager::prepareFrameTextures(
	Clay_RenderCommandArray& renderCommands,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY)
{
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager is not initialized.");
	}

	advanceFrameCounter();
	resetVariantFrameMarks();

	const float clampedScaleX = std::max(uiToFramebufferScaleX, 1.0e-6f);
	const float clampedScaleY = std::max(uiToFramebufferScaleY, 1.0e-6f);

	for (int32_t i = 0; i < renderCommands.length; ++i) {
		Clay_RenderCommand& command = renderCommands.internalArray[i];
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) {
			continue;
		}

		auto* textureRef = reinterpret_cast<TextureRef*>(command.renderData.image.imageData);
		if (!textureRef || textureRef->id == 0u) {
			continue;
		}

		const std::string* requestedKey = findRequestedKeyByTextureId(textureRef->id);
		if (!requestedKey) {
			continue;
		}

		const float scaledWidth = std::max(0.0f, command.boundingBox.width * clampedScaleX);
		const float scaledHeight = std::max(0.0f, command.boundingBox.height * clampedScaleY);
		const uint32_t requestedWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(scaledWidth)));
		const uint32_t requestedHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(scaledHeight)));

		VariantEntry& variant = ensureVariantForRequest(*requestedKey, requestedWidth, requestedHeight);
		touchVariant(variant, frameCounter_);
		if (variant.pageIndex < atlasPages_.size()) {
			atlasPages_[variant.pageIndex].lastUsedFrame = frameCounter_;
		}

		textureRef->id = variant.slotId;
		textureRef->uv0x = variant.uv0x;
		textureRef->uv0y = variant.uv0y;
		textureRef->uv1x = variant.uv1x;
		textureRef->uv1y = variant.uv1y;
		textureRef->sourceWidth = static_cast<int32_t>(variant.sourceWidth);
		textureRef->sourceHeight = static_cast<int32_t>(variant.sourceHeight);
	}
}

void IconManager::setRegistry(detail::IUiTextureRegistry* registry) {
	registry_ = registry;
}

void IconManager::init(VulkanContext& vk, const SvgManagerConfig& config) {
	detail::IUiTextureRegistry* const preservedRegistry = registry_;
	destroy(vk);
	registry_ = preservedRegistry;

	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("IconManager init requires a valid Vulkan device + allocator.");
	}
	if (!registry_) {
		throw std::runtime_error("IconManager registry backend is not set.");
	}
	if (vk.graphicsQFamily == UINT32_MAX || vk.graphicsQ == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager requires a valid graphics queue.");
	}

	vk_ = &vk;
	atlasSize_ = std::max<uint32_t>(1u, config.atlasSize);
	atlasPadding_ = config.atlasPadding;
	sizeBucketStep_ = std::max<uint32_t>(1u, config.sizeBucketStep);
	maxAtlasPages_ = std::max<uint32_t>(1u, config.maxAtlasPages);
	frameCounter_ = 0u;

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
	vkCheck(vkCreateSampler(vk.device, &samplerInfo, nullptr, &atlasSampler_), "IconManager failed to create atlas sampler.");

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.graphicsQFamily;
	vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &commandPool_), "IconManager failed to create command pool.");

	// Seed first page + descriptor slot so shader-facing IDs are available immediately.
	atlasPages_.push_back(createAtlasPage(0u));
}

bool IconManager::registerSvg(std::string_view key, std::string_view svgSource) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager is not initialized.");
	}
	if (key.empty()) {
		throw std::runtime_error("IconManager key must not be empty.");
	}
	if (svgSource.empty()) {
		throw std::runtime_error("IconManager SVG source must not be empty.");
	}
	if (svgSource.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
		throw std::runtime_error("IconManager SVG source is too large.");
	}

	const std::string keyString(key);
	if (documentsByKey_.find(keyString) != documentsByKey_.end()) {
		return false;
	}

	char* ownedSource = static_cast<char*>(std::malloc(svgSource.size() + 1u));
	if (!ownedSource) {
		throw std::runtime_error("IconManager failed to allocate memory for SVG source.");
	}

	std::memcpy(ownedSource, svgSource.data(), svgSource.size());
	ownedSource[svgSource.size()] = '\0';

	plutosvg_document_t* document = plutosvg_document_load_from_data(
		ownedSource,
		static_cast<int>(svgSource.size()),
		-1.0f,
		-1.0f,
		std::free,
		ownedSource);
	if (!document) {
		std::free(ownedSource);
		throw std::runtime_error("IconManager failed to parse SVG source.");
	}

	DocumentRecord record{};
	record.document = document;
	record.intrinsicWidth = std::max(0.0f, plutosvg_document_get_width(document));
	record.intrinsicHeight = std::max(0.0f, plutosvg_document_get_height(document));

	auto [it, inserted] = documentsByKey_.emplace(keyString, record);
	if (!inserted) {
		plutosvg_document_destroy(document);
		return false;
	}

	return true;
}

bool IconManager::registerFromFile(std::string_view key, std::string_view filePath) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager is not initialized.");
	}
	if (key.empty()) {
		throw std::runtime_error("IconManager key must not be empty.");
	}

	const std::string keyString(key);
	if (documentsByKey_.find(keyString) != documentsByKey_.end()) {
		return false;
	}

	const std::filesystem::path path(filePath);
	if (!std::filesystem::is_regular_file(path)) {
		throw std::runtime_error("SVG file does not exist: " + path.string());
	}

	const std::string pathString = path.string();
	plutosvg_document_t* document = plutosvg_document_load_from_file(pathString.c_str(), -1.0f, -1.0f);
	if (!document) {
		throw std::runtime_error("IconManager failed to parse SVG file: " + pathString);
	}

	DocumentRecord record{};
	record.document = document;
	record.intrinsicWidth = std::max(0.0f, plutosvg_document_get_width(document));
	record.intrinsicHeight = std::max(0.0f, plutosvg_document_get_height(document));

	auto [it, inserted] = documentsByKey_.emplace(keyString, record);
	if (!inserted) {
		plutosvg_document_destroy(document);
		return false;
	}

	return true;
}

bool IconManager::remove(std::string_view key)
{
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager is not initialized.");
	}

	const std::string keyString(key);
	auto it = documentsByKey_.find(keyString);
	if (it == documentsByKey_.end()) {
		return false;
	}

	const auto requestIdIt = requestTextureIdByKey_.find(keyString);
	if (requestIdIt != requestTextureIdByKey_.end()) {
		if (registry_) {
			const std::string namespacedKey = makeRequestNamespacedKey(key);
			const bool removed = registry_->removeSlot(namespacedKey);
			(void)removed;
		}
		requestedKeyByTextureId_.erase(requestIdIt->second);
		requestTextureIdByKey_.erase(requestIdIt);
	}

	for (auto variantIt = variantsByKeyAndSize_.begin(); variantIt != variantsByKeyAndSize_.end();) {
		VariantEntry& variant = variantIt->second;
		if (variant.key.nameKey != keyString) {
			++variantIt;
			continue;
		}

		if (variant.pageIndex < atlasPages_.size()) {
			releasePageRegion(atlasPages_[variant.pageIndex], variant.paddedRect);
		}

		variantIt = variantsByKeyAndSize_.erase(variantIt);
	}

	plutosvg_document_destroy(it->second.document);
	documentsByKey_.erase(it);
	return true;
}

bool IconManager::contains(std::string_view key) const {
	return documentsByKey_.find(std::string(key)) != documentsByKey_.end();
}

TextureRef IconManager::textureRef(std::string_view key) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager is not initialized.");
	}
	if (!registry_) {
		throw std::runtime_error("IconManager registry backend is not set.");
	}
	if (atlasPages_.empty() || atlasPages_.front().view == VK_NULL_HANDLE || atlasSampler_ == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager atlas pages are not initialized.");
	}
	if (key.empty()) {
		throw std::runtime_error("IconManager key must not be empty.");
	}

	const std::string keyString(key);
	const auto documentIt = documentsByKey_.find(keyString);
	if (documentIt == documentsByKey_.end()) {
		throw std::runtime_error("IconManager textureRef requested an unknown SVG key: " + keyString);
	}

	auto requestIdIt = requestTextureIdByKey_.find(keyString);
	if (requestIdIt == requestTextureIdByKey_.end()) {
		const std::string namespacedKey = makeRequestNamespacedKey(key);
		bool inserted = false;
		const uint32_t requestTextureId = registry_->registerOrReplaceSlot(
			*vk_,
			namespacedKey,
			atlasPages_.front().view,
			atlasSampler_,
			inserted);
		if (!inserted) {
			throw std::runtime_error("IconManager request namespaced key collision.");
		}
		requestIdIt = requestTextureIdByKey_.emplace(keyString, requestTextureId).first;
		requestedKeyByTextureId_.emplace(requestTextureId, keyString);
	}

	TextureRef texture{};
	texture.id = requestIdIt->second;
	texture.sourceWidth = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicWidth)));
	texture.sourceHeight = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicHeight)));
	return texture;
}

IconManager::TransientRasterResult IconManager::rasterizeForAtlas(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const
{
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("IconManager is not initialized.");
	}
	if (key.empty()) {
		throw std::runtime_error("IconManager raster key must not be empty.");
	}

	const auto recordIt = documentsByKey_.find(std::string(key));
	if (recordIt == documentsByKey_.end() || !recordIt->second.document) {
		throw std::runtime_error("IconManager raster request references an unknown SVG key.");
	}

	const uint32_t targetWidth = std::max<uint32_t>(1u, requestedWidth);
	const uint32_t targetHeight = std::max<uint32_t>(1u, requestedHeight);

	double intrinsicWidth = static_cast<double>(recordIt->second.intrinsicWidth);
	double intrinsicHeight = static_cast<double>(recordIt->second.intrinsicHeight);
	if (intrinsicWidth <= 0.0 || intrinsicHeight <= 0.0) {
		plutovg_rect_t extents{};
		if (plutosvg_document_extents(recordIt->second.document, nullptr, &extents) && extents.w > 0.0f && extents.h > 0.0f) {
			intrinsicWidth = static_cast<double>(extents.w);
			intrinsicHeight = static_cast<double>(extents.h);
		}
	}
	if (intrinsicWidth <= 0.0 || intrinsicHeight <= 0.0) {
		intrinsicWidth = static_cast<double>(targetWidth);
		intrinsicHeight = static_cast<double>(targetHeight);
	}

	const double scaleX = static_cast<double>(targetWidth) / intrinsicWidth;
	const double scaleY = static_cast<double>(targetHeight) / intrinsicHeight;
	const double scale = std::max(0.0, std::min(scaleX, scaleY));
	if (!(scale > 0.0)) {
		throw std::runtime_error("IconManager could not determine a valid SVG raster scale.");
	}

	uint32_t rasterWidth = static_cast<uint32_t>(std::llround(intrinsicWidth * scale));
	uint32_t rasterHeight = static_cast<uint32_t>(std::llround(intrinsicHeight * scale));
	rasterWidth = std::max<uint32_t>(1u, std::min<uint32_t>(targetWidth, rasterWidth));
	rasterHeight = std::max<uint32_t>(1u, std::min<uint32_t>(targetHeight, rasterHeight));

	SurfaceOwner owner(plutosvg_document_render_to_surface(
		recordIt->second.document,
		nullptr,
		static_cast<int>(rasterWidth),
		static_cast<int>(rasterHeight),
		nullptr,
		nullptr,
		nullptr));
	if (!owner.surface) {
		throw std::runtime_error("IconManager failed to rasterize SVG document.");
	}

	const int surfaceWidth = plutovg_surface_get_width(owner.surface);
	const int surfaceHeight = plutovg_surface_get_height(owner.surface);
	const int surfaceStride = plutovg_surface_get_stride(owner.surface);
	unsigned char* surfaceData = plutovg_surface_get_data(owner.surface);
	if (!surfaceData || surfaceWidth <= 0 || surfaceHeight <= 0 || surfaceStride <= 0) {
		throw std::runtime_error("IconManager rasterization returned invalid surface data.");
	}

	convertArgbPremultipliedToRgbaStraight(
		surfaceData,
		static_cast<uint32_t>(surfaceWidth),
		static_cast<uint32_t>(surfaceHeight),
		static_cast<uint32_t>(surfaceStride));

	TransientRasterResult result{};
	result.owner = std::move(owner);
	result.rgbaPixels = surfaceData;
	result.width = static_cast<uint32_t>(surfaceWidth);
	result.height = static_cast<uint32_t>(surfaceHeight);
	result.strideBytes = static_cast<uint32_t>(surfaceStride);
	result.requestedWidth = targetWidth;
	result.requestedHeight = targetHeight;
	return result;
}

void IconManager::destroy(VulkanContext& vk) {
	(void)vk;

	if (registry_) {
		for (const auto& [key, _] : requestTextureIdByKey_) {
			const std::string namespacedKey = makeRequestNamespacedKey(key);
			const bool removed = registry_->removeSlot(namespacedKey);
			(void)removed;
		}
	}

	for (auto& [_, record] : documentsByKey_) {
		plutosvg_document_destroy(record.document);
		record.document = nullptr;
	}
	documentsByKey_.clear();
	requestTextureIdByKey_.clear();
	requestedKeyByTextureId_.clear();
	variantsByKeyAndSize_.clear();
	destroyAtlasPages();

	if (vk_ && vk_->device != VK_NULL_HANDLE && commandPool_ != VK_NULL_HANDLE) {
		vkDestroyCommandPool(vk_->device, commandPool_, nullptr);
	}
	commandPool_ = VK_NULL_HANDLE;

	if (vk_ && vk_->device != VK_NULL_HANDLE && atlasSampler_ != VK_NULL_HANDLE) {
		vkDestroySampler(vk_->device, atlasSampler_, nullptr);
	}
	atlasSampler_ = VK_NULL_HANDLE;

	vk_ = nullptr;
	registry_ = nullptr;
	atlasSize_ = 0u;
	atlasPadding_ = 1u;
	sizeBucketStep_ = 8u;
	maxAtlasPages_ = 10u;
	frameCounter_ = 0u;
}

} // namespace FlowUi

#endif
