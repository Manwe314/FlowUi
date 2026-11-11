#pragma once

#include "FlowUi/Private/renderer/UiRenderTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <clay.h>
#include <vulkan/vulkan.h>

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "FlowUi/Public/configurationStructs/ResourceConfig.hpp"
#include "FlowUi/Public/configurationStructs/UiConfig.hpp"
#include "FlowUi/Public/configurationStructs/VulkanConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Private/inputService/InputFieldRenderOverrides.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/storageSystem/IStorageSystem.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutService.hpp"

namespace FlowUi {
#if FLOW_UI_DEV_MODE
namespace devSystems { namespace tooling { struct DevOverlayCommandBuffer; } }
#endif
#if FLOW_UI_DEV_MODE
namespace devSystems { class DevTimingRecorder; class MemorySampleSink; struct GpuTimingCommandContext; }
#endif
}

struct SharedUiByteResources {
	FlowUi::detail::storage::BufferHandle quadBuffer{};
	FlowUi::detail::storage::ImageHandle placeholderFontImage{};
	FlowUi::detail::storage::ImageViewHandle placeholderFontView{};
	FlowUi::detail::storage::ImageHandle placeholderUiImage{};
	FlowUi::detail::storage::ImageViewHandle placeholderUiView{};
	FlowUi::detail::storage::SamplerHandle linearSampler{};
	FlowUi::detail::storage::SamplerHandle missing_texture_sampler{};

	VkBuffer nativeQuadBuffer = VK_NULL_HANDLE;
	VkImageView nativePlaceholderFontView = VK_NULL_HANDLE;
	VkImageView nativePlaceholderUiView = VK_NULL_HANDLE;
	VkSampler nativeLinearSampler = VK_NULL_HANDLE;
};

struct PreparedUiFrame {
	std::span<const UiRun> runs{};
	FlowUi::detail::storage::FrameEpoch epoch = 0;
	uint32_t instanceCount = 0;
	uint32_t originatingFrameSlot = 0u;
};

namespace FlowUi::detail {

[[nodiscard]] Result<float> resolve_primitive_coverage_width_scale(
	const PrimitiveAntialiasingConfig& config) noexcept;

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
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
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

	FlowUi::ResourceConfig resources_{};
	VkFormat targetFormat_ = VK_FORMAT_UNDEFINED;
	uint32_t maxUiImageDescriptors_ = 256;
	uint32_t frameResourceCount_ = 1u;
	float pointsToPixelsScale_ = 96.0f / 72.0f;
	float primitive_coverage_width_scale_ = 1.0f;
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
		bool allowInstanceGrowth,
		const FlowUi::ResourceConfig& resources = {});
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
		const FlowUi::devSystems::tooling::DevOverlayCommandBuffer* devOverlay = nullptr,
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

#if FLOW_UI_DEV_MODE
	[[nodiscard]] VkFormat devReplayTargetFormat() const noexcept;
	[[nodiscard]] VkDescriptorSetLayout devReplayGlobalsLayout() const noexcept;
	void recordExternalReplay(
		VkCommandBuffer commandBuffer,
		VkExtent2D targetExtent,
		VkDescriptorSet globalsSet,
		uint32_t sourceTextureFrameSlot,
		std::span<const UiRun> replayRuns) const;
#endif

};
