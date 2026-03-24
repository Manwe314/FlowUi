#pragma once

#include <cstdint>
#include <vector>

#include <clay.h>
#include <vulkan/vulkan.h>

#include "Vulkan/Vk_Context.hpp"
#include "FlowUi/PublicStructs.hpp"

namespace FlowUi {
struct InputFieldFrameOverrides;
}

enum class UiType : uint8_t {
	Solid = 0,
	Msdf = 1,
	Textured = 2,
};

struct VmaAllocation_T;
struct FontManager;

struct RectF {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
};

struct UiInstance {
	uint32_t type = 0;
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	uint32_t colorRGBA = 0;
	float r0 = 0.0f;
	float r1 = 0.0f;
	float r2 = 0.0f;
	float r3 = 0.0f;
	float borderL = 0.0f;
	float borderT = 0.0f;
	float borderR = 0.0f;
	float borderB = 0.0f;
	uint32_t solidMode = 0;

	float uv0x = 0.0f;
	float uv0y = 0.0f;
	float uv1x = 1.0f;
	float uv1y = 1.0f;
	uint32_t texIndex = 0;
	uint32_t atlasLayer = 0;
	uint32_t _pad0 = 0;
};

struct UiRun {
	UiType type = UiType::Solid;
	RectF scissor{};
	uint32_t firstInstance = 0;
	uint32_t instanceCount = 0;
};

struct VulkanUiRenderer {
	struct AllocatedBuffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation_T* allocation = nullptr;
		void* mapped = nullptr;
		VkDeviceSize size = 0;
	};

	struct AllocatedImage {
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation_T* allocation = nullptr;
		VkImageView view = VK_NULL_HANDLE;
	};

	struct Pipelines {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipeline solid = VK_NULL_HANDLE;
		VkPipeline msdf = VK_NULL_HANDLE;
		VkPipeline textured = VK_NULL_HANDLE;
	};

	struct Descriptors {
		VkDescriptorSetLayout set0 = VK_NULL_HANDLE;
		VkDescriptorSetLayout set1 = VK_NULL_HANDLE;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		VkDescriptorSet globalsSet = VK_NULL_HANDLE;
		VkDescriptorSet texturesSet = VK_NULL_HANDLE;
	};

	Pipelines pipelines{};
	Descriptors descriptors{};

	AllocatedBuffer instanceBuffer{};
	AllocatedBuffer quadVertexBuffer{};
	AllocatedImage placeholderFontAtlas{};
	AllocatedImage placeholderUiTexture{};

	VkSampler linearSampler = VK_NULL_HANDLE;
	VkFormat targetFormat = VK_FORMAT_UNDEFINED;
	uint32_t maxUiImageDescriptors = 256;
	float pointsToPixelsScale = 96.0f / 72.0f;
	const FontManager* fontManager = nullptr;
	uint32_t boundFontAtlasRevision = UINT32_MAX;

	void init(const FlowUi::AppConfig& config, VulkanContext& vk, VkFormat swapFormat);
	void setFontManager(const FontManager* manager);
	void destroy(VulkanContext& vk);
	void onSwapchainFormatChanged(VulkanContext& vk, VkFormat newFormat);
	uint32_t textureSlotCapacity() const;
	void reserveTextureSlots(VulkanContext& vk, uint32_t minCapacity);
	void setTextureSlotBinding(uint32_t slot, VkImageView view, VkSampler sampler);
	void clearTextureSlotBinding(uint32_t slot);
	void rebuildTextureDescriptors(VkDevice device);
	void render(
		VulkanContext& vk,
		VkCommandBuffer cmd,
		const Clay_RenderCommandArray& renderCommands,
		const FlowUi::InputFieldFrameOverrides& inputFieldOverrides,
		VkExtent2D extent,
		VkImageView targetView,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY);

	std::vector<VkDescriptorImageInfo> uiTextureSlotInfos_;
	bool textureDescriptorsDirty_ = false;
	std::vector<UiInstance> instancesScratch_;
	std::vector<UiRun> runsScratch_;
};
