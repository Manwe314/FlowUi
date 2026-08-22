#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <clay.h>
#include <vulkan/vulkan.h>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/WindowId.hpp"
#include "Vulkan/Vk_Context.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "internal/InputFieldRenderOverrides.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "internal/Text/TextLayoutService.hpp"

enum class UiType : uint8_t {
	Solid = 0,
	Msdf = 1,
	Textured = 2,
};

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
namespace devSystems { class DevTimingRecorder; class MemorySampleSink; struct GpuTimingCommandContext; }
#endif
}

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

static_assert(std::is_trivially_copyable_v<UiInstance>);
static_assert(sizeof(UiInstance) == 88);
static_assert(alignof(UiInstance) == 4);

struct SharedUiByteResources {
	FlowUi::detail::storage::BufferHandle quadBuffer{};
	FlowUi::detail::storage::ImageHandle placeholderFontImage{};
	FlowUi::detail::storage::ImageViewHandle placeholderFontView{};
	FlowUi::detail::storage::ImageHandle placeholderUiImage{};
	FlowUi::detail::storage::ImageViewHandle placeholderUiView{};
	FlowUi::detail::storage::SamplerHandle linearSampler{};

	VkBuffer nativeQuadBuffer = VK_NULL_HANDLE;
	VkImageView nativePlaceholderFontView = VK_NULL_HANDLE;
	VkImageView nativePlaceholderUiView = VK_NULL_HANDLE;
	VkSampler nativeLinearSampler = VK_NULL_HANDLE;
};

struct PreparedUiFrame {
	std::span<const UiRun> runs{};
	uint32_t instanceCount = 0;
	FlowUi::detail::storage::FrameEpoch epoch = 0;
};

namespace FlowUi::detail {

struct UiConversionCapacity {
	size_t instances = 0;
	size_t runs = 0;
	size_t scissorDepth = 1;
};

struct UiConversionResult {
	uint32_t instanceCount = 0;
	uint32_t runCount = 0;
	uint32_t textGlyphCount = 0;
	uint32_t imageCommandCount = 0;
};

[[nodiscard]] UiConversionCapacity measureUiConversionCapacity(
	const Clay_RenderCommandArray& commands,
	const InputFieldFrameOverrides& overrides);
[[nodiscard]] UiConversionResult buildUiInstancesDirect(
	const Clay_RenderCommandArray& commands,
	const InputFieldFrameOverrides& overrides,
	VkExtent2D extent,
	const manager_storage::FontFrameView* fontView,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::span<UiInstance> instances,
	std::span<UiRun> runs,
	std::span<RectF> scissorStack,
	std::span<const FlowUi::detail::storage::BindingHotRecord> textureBindings = {});
[[nodiscard]] uint64_t growUiInstanceCapacity(
	uint64_t currentBytes,
	uint64_t requiredBytes,
	uint64_t initialBytes);

} // namespace FlowUi::detail

void initSharedUiByteResources(
	FlowUi::detail::storage::IStorageSystem& storage,
	SharedUiByteResources& resources);
void destroySharedUiByteResources(
	FlowUi::detail::storage::IStorageSystem& storage,
	SharedUiByteResources& resources) noexcept;

struct VulkanUiRenderer {
#if FLOW_UI_DEV_MODE
	void appendDevMemorySamples(
		FlowUi::devSystems::MemorySampleSink& sink,
		const PreparedUiFrame* prepared = nullptr) const noexcept;
#endif
	struct UiFrameResources {
		FlowUi::detail::storage::BufferHandle instanceBuffer{};
		FlowUi::detail::storage::NativeBufferView nativeBuffer{};
		uint64_t capacityBytes = 0;
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
		std::vector<VkDescriptorSet> globalsSets{};
		std::vector<VkDescriptorSet> texturesSets{};
	};

	FlowUi::detail::storage::RendererLayoutHandle layoutHandle_{};
	FlowUi::detail::storage::RendererPipelineBundleHandle pipelineBundleHandle_{};
	FlowUi::detail::storage::WindowDescriptorBundleHandle descriptorBundleHandle_{};
	FlowUi::detail::storage::NativeRendererLayout nativeLayout_{};
	FlowUi::detail::storage::NativeRendererPipelineBundle nativePipelineBundle_{};
	FlowUi::detail::storage::NativeWindowDescriptorView nativeDescriptors_{};

	Pipelines pipelines_{};
	Descriptors descriptors_{};

	std::vector<UiFrameResources> frameResources_{};
	const SharedUiByteResources* sharedByteResources_ = nullptr;
	FlowUi::detail::storage::IStorageSystem* storage_ = nullptr;
	FlowUi::WindowId windowId_ = FlowUi::InvalidWindowId;
	uint64_t initialInstanceBytes_ = 1024u * 1024u;
	bool allowInstanceGrowth_ = true;

	VkFormat targetFormat_ = VK_FORMAT_UNDEFINED;
	uint32_t maxUiImageDescriptors_ = 256;
	uint32_t frameResourceCount_ = 1u;
	float pointsToPixelsScale_ = 96.0f / 72.0f;
	std::vector<uint32_t> boundFontAtlasRevisionByFrame_{};
	FlowUi::detail::text::TextLayoutService textLayoutService_{};

	void init(
		const FlowUi::VulkanConfig& vulkanConfig,
		const FlowUi::UiConfig& uiConfig,
		VulkanContext& vk,
		VkFormat swapFormat,
		FlowUi::detail::storage::IStorageSystem& storage,
		FlowUi::WindowId windowId,
		const SharedUiByteResources& sharedResources,
		uint64_t initialInstanceBytes,
		uint32_t textureDescriptorCapacity,
		bool allowInstanceGrowth);
	void destroy(
		VulkanContext& vk,
		FlowUi::detail::storage::IStorageSystem& storage,
		FlowUi::detail::storage::SubmissionSerial lastUse = 0);
	void onSwapchainFormatChanged(
		VulkanContext& vk,
		VkFormat newFormat,
		FlowUi::detail::storage::SubmissionSerial lastUse = 0);
	void applyTextureBindings(
		VkDevice device,
		uint32_t frameSlot,
		const FlowUi::detail::storage::PreparedTextureBindings& prepared);
	[[nodiscard]] PreparedUiFrame prepareFrame(
		VulkanContext& vk,
		FlowUi::detail::storage::IStorageSystem& storage,
		const FlowUi::detail::storage::FrameToken& frame,
		const FlowUi::detail::storage::PreparedTextureBindings& textureBindings,
		const Clay_RenderCommandArray& renderCommands,
		const FlowUi::detail::InputFieldFrameOverrides& inputFieldOverrides,
		const FlowUi::detail::manager_storage::FontFrameView& fontView,
		VkExtent2D extent,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY
#if FLOW_UI_DEV_MODE
		,
		FlowUi::devSystems::DevTimingRecorder* timingRecorder = nullptr
#endif
		);
	void recordPreparedFrame(
		VulkanContext& vk,
		VkCommandBuffer cmd,
		VkExtent2D extent,
		VkImageView targetView,
		uint32_t frameIndex,
		const PreparedUiFrame& prepared
#if FLOW_UI_DEV_MODE
		,
		FlowUi::devSystems::DevTimingRecorder* timingRecorder = nullptr,
		FlowUi::devSystems::GpuTimingCommandContext* gpuTiming = nullptr
#endif
		);

};
